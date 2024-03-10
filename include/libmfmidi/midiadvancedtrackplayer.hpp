/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/// \file midiadvancedtrackplayer.hpp
/// \brief Advanced MIDI track player

#pragma once

#include "libmfmidi/abstractmididevice.hpp"
#include "libmfmidi/mfconcepts.hpp"
#include "libmfmidi/midinotifier.hpp"
#include "libmfmidi/midistatus.hpp"
#include "libmfmidi/miditrack.hpp"
#include "libmfmidi/midiutils.hpp"
#include "libmfmidi/platformapi.hpp"
#include <cassert>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>
#include <utility>

namespace libmfmidi {
    using namespace std::literals;

    namespace advtrkplayer {
        template <class Time, class Track>
        struct BasicSnapshot {
            Time sleeptime{}; // time to next event
            // playtime is key // current time
            std::ranges::iterator_t<const Track> nextevent{}; // next event, if sleeptime==0, this is current event
            MIDIStatus                           status{};    // status of played events
        };


        template <class Time, class Track>
        struct MIDIAdvTrkCache {
            using Snapshot = BasicSnapshot<Time, Track>;
            std::map<Time, Snapshot> data;
        };

        template <class Time, class Track>
        inline Time buildPlaybackCache(const Track& trk, MIDIAdvTrkCache<Time, Track>& caches, Time cacheinterval, MIDIDivision division)
        {
            using Snapshot = BasicSnapshot<Time, Track>;

            if (std::ranges::empty(trk)) {
                return {};
            }

            caches.data.clear();
            Time                playtime{};
            auto                nextevent = trk.begin();
            MIDIStatus          status;
            MIDIStatusProcessor proc{status};
            Time                divns         = divisionToDuration(division, status.tempo);
            Time                nextCacheTime = cacheinterval;
            Snapshot            snap{};
            proc.addNotifier([&divns, &status, &division](NotifyType type) {
                if (type == NotifyType::C_Tempo) {
                    divns = divisionToDuration(division, status.tempo);
                }
            });
            while (true) {
                while (playtime + nextevent->deltaTime() * divns < nextCacheTime) { // use < because ignore the event on nextCacheTime (the event will play when tick, but have not played yet when cache)
                    playtime += nextevent->deltaTime() * divns;
                    proc.process(*nextevent);
                    ++nextevent;
                    if (nextevent == trk.end()) {
                        return playtime; // see directGoTo
                    }
                }
                snap.nextevent             = nextevent;
                snap.sleeptime             = playtime + nextevent->deltaTime() * divns - nextCacheTime; // see directGoTo
                snap.status                = status;
                caches.data[nextCacheTime] = std::move(snap);
                nextCacheTime += cacheinterval;
            }
        }

        template <std::ranges::forward_range Track>
        class MIDIAdvancedTrackPlayer; // todo:remove

        /// \brief Internal class for holding status and play status
        ///
        template <std::ranges::forward_range Track>
        class MIDIAdvTrkPlayerCursor : protected NotifyUtils<MIDIAdvTrkPlayerCursor<Track>> {
            friend class MIDIAdvancedTrackPlayer<Track>;
            friend class NotifyUtils<MIDIAdvTrkPlayerCursor>;

        public:
            using Time     = std::chrono::nanoseconds; // must be signed
            using Snapshot = BasicSnapshot<Time, Track>;
            using Cache    = MIDIAdvTrkCache<Time, Track>;

        private:
            // Information
            std::string mname{};
            bool        mactive       = true; // tick-able
            bool        mautorevertStatus = true;

            // Playing status
            Time mdivns{};     // 1 delta-time in nanoseconds
            Time msleeptime{}; // time to sleep
            Time mplaytime{};  // current time, support 5850 centuries long

            // Data
            const Track*                         mtrack{};
            MIDIDivision                             mdivision{};
            std::ranges::iterator_t<const Track> mnextevent{};
            AbstractMIDIDevice*                      mdev{};
            MIDIStatus*                               mstatus;
            Cache*                                   mcache{};
            Time                                     mcacheinterval = 1min;

            // Misc
            std::unique_ptr<MIDIStatusProcessor>   mstproc;
            MIDIProcessorFunction mprocessor;

        public:
            static constexpr bool randomAccessible = std::ranges::random_access_range<Track>;
            using NotifyUtils<MIDIAdvTrkPlayerCursor>::addNotifier;

            explicit MIDIAdvTrkPlayerCursor(std::string name) noexcept
                : mname(std::move(name))
            {
            }

            ~MIDIAdvTrkPlayerCursor() noexcept = default;
            MF_DISABLE_COPY(MIDIAdvTrkPlayerCursor);
            MF_DISABLE_MOVE(MIDIAdvTrkPlayerCursor); // TODO: implement move
            // setMIDIStatus should be called for moving

            void setMIDIStatus(MIDIStatus* mst)
            {
                mstatus = mst;
                mstproc = std::make_unique<MIDIStatusProcessor>(*mst);
                mstproc->addNotifier([this](NotifyType type) {
                    if (type == NotifyType::C_Tempo) {
                        recalcuateDivisionTiming();
                    }
                });
            }

            void recalcuateDivisionTiming() noexcept
            {
                // TODO: how will msleeptime change
                auto newdivns = divisionToDuration(mdivision, mstatus->tempo);
                if ((msleeptime.count() != 0) || (mdivns.count() != 0)) {
                    msleeptime    = Time{msleeptime.count() * newdivns.count() / mdivns.count()};
                }
                
                mdivns = newdivns;
            }

            void syncDeviceStatus(bool force = false) const
            {
                if (!force && !mautorevertStatus) {
                    return;
                }
                for (const auto& msg : reportMIDIStatus(*mstatus, false)) {
                    mdev->sendMsg(msg);
                }
            }

            void setCache(Cache* cache)
            {
                mcache = cache;
            }

            void setProcessor(MIDIProcessorFunction func)
            {
                mprocessor = std::move(func);
            }

            void buildPlaybackCache()
            {
                if (mcache == nullptr || mtrack == nullptr) {
                    return;
                }
                advtrkplayer::buildPlaybackCache(*mtrack, *mcache, mcacheinterval, mdivision);
            }

            void setDevice(AbstractMIDIDevice* dev)
            {
                mdev = dev;
                syncDeviceStatus();
            }

            Time tick(Time slept /*the time that slept*/)
            {
                assert(mactive); // never tick when not active
                if (mnextevent == mtrack->end()) {
                    mactive = false;
                    return {};
                }
                mplaytime += slept;                                  // not move it to playthread because of lastSleptTime = 0 in revertSnapshot
                if (msleeptime == 0ns) {                             // check if first tick
                    msleeptime = mnextevent->deltaTime() * mdivns; // usually first tick
                }
                assert(slept <= msleeptime);
                msleeptime -= slept;
                if (msleeptime != 0ns) {
                    return msleeptime;
                }

                auto sbegintime = hiresticktime();

                static MIDIMessage message{0, 0, 0, 0};
                message.resize(mnextevent->size());
                // std::ranges::copy(*mnextevent, message.begin());
                std::copy(mnextevent->begin(), mnextevent->end(), message.begin());

                if (mstproc) {
                    mstproc->process(message);
                }
                if (process(message) && mdev != nullptr) {
                    mdev->sendMsg(message);
                }
                ++mnextevent;
                if (mnextevent == mtrack->end()) {
                    mactive = false;
                    return {};
                }
                auto sendtimedur = hiresticktime() - sbegintime;
                msleeptime = mnextevent->deltaTime() * mdivns - sendtimedur;
                return msleeptime;
            }

            void setDivision(MIDIDivision div)
            {
                mdivision = div;
                recalcuateDivisionTiming();
            }

            void setCacheInterval(Time interval) noexcept
            {
                mcacheinterval = interval;
            }

            void setTrack(const Track* track, bool updatecache = true)
            {
                mtrack = track;
                if (mcache != nullptr && updatecache) {
                    mcache->data.clear();
                    buildPlaybackCache();
                }
                Time ptime = mplaytime;
                revertSnapshot(defaultSnapshot(), {});
                goTo(ptime);
            }

            [[nodiscard]] const MIDIStatus& status() const&
            {
                return *mstatus;
            }

            [[nodiscard]] bool isActive() const
            {
                return mactive;
            }

            [[nodiscard]] bool eof() const
            {
                return mnextevent == mtrack->end();
            }

            void setActive(bool active)
            {
                mactive = active;
            }

            bool goTo(Time targetTime)
            {
                if (mnextevent == mtrack->end()) {
                    revertSnapshot(defaultSnapshot(), {}); // reset
                }
                recalcuateDivisionTiming();
                if (targetTime < mplaytime) {
                    revertSnapshot(defaultSnapshot(), {}); // reset
                }
                if (mcache != nullptr) {
                    for (auto& [playtime, snap] : mcache->data | std::views::reverse) {
                        if (playtime <= targetTime) {
                            revertSnapshot(snap, playtime);
                        }
                    }
                }
                bool result = directGoTo(targetTime); // for the rest
                if (mdev != nullptr) {
                    syncDeviceStatus();
                }
                recalcuateDivisionTiming();
                return result;
            }

            bool directGoTo(Time targetTime)
            {
                assert(mplaytime <= targetTime);
                auto currentevent = mnextevent;
                while (mplaytime + mnextevent->deltaTime() * mdivns < targetTime) { // use <, see cur.msleeptime under
                    currentevent = mnextevent;
                    mplaytime += currentevent->deltaTime() * mdivns;
                    if (mstproc) {
                        mstproc->process(*currentevent);
                    }
                    ++mnextevent;
                    if (mnextevent == mtrack->end()) { // should not happen if targetTime is not out of range, because uses < above (so nextevent never point to the end iterator (sentinal))
                        return false;
                    }
                }
                msleeptime = mplaytime + mnextevent->deltaTime() * mdivns - targetTime; // if there is a event on targetTime, msleeptime will be 0 and mnextevent will play immediately when tick
                mplaytime  = targetTime;                                                  // this must be after cur.msleeptime, see above
                return true;
            }

        protected:
            using NotifyUtils<MIDIAdvTrkPlayerCursor>::notify;

            std::pair<Snapshot, Time> captureSnapshot()
            {
                return {
                    Snapshot{.sleeptime = msleeptime, .nextevent = mnextevent, .status = *mstatus},
                    mplaytime
                };
            }

            void revertSnapshot(const Snapshot& snapshot, Time playtime)
            {
                msleeptime = snapshot.sleeptime;
                mplaytime  = playtime;
                *mstatus    = snapshot.status;
                mnextevent = snapshot.nextevent;
            }

            Snapshot defaultSnapshot() const
            {
                return {0ns, mtrack->begin(), {}};
            }

        private:
            inline bool process(MIDIMessage& msg) const noexcept
            {
                return !mprocessor || mprocessor(msg);
            }
        };

        // template <std::ranges::forward_range Container, class Time>

        /// \brief A powerful MIDI track player
        /// Support multi-cursor
        template <std::ranges::forward_range Track>
        class MIDIAdvancedTrackPlayer {

        public:
            using Cursor                                         = MIDIAdvTrkPlayerCursor<Track>;
            using Cache                                          = Cursor::Cache;
            using Time                                           = typename Cursor::Time;
            using Snapshot                                       = BasicSnapshot<Time, Track>;
            static constexpr Time                 MAX_SLEEP      = 500ms;
            static constexpr std::chrono::minutes CACHE_INTERVAL = 2min; // seconds

            MIDIAdvancedTrackPlayer() noexcept = default;

            ~MIDIAdvancedTrackPlayer() noexcept
            {
                pause();
                mthread.request_stop();
                mwakeup = true;
                mcondvar.notify_all();

                if (mcachemanaged) {
                    delete mmanagedcache;
                }
            }

#pragma region lessimportant

            MF_DISABLE_COPY(MIDIAdvancedTrackPlayer);
            MF_DEFAULT_MOVE(MIDIAdvancedTrackPlayer);

            inline void setCacheAll(Cache* cache, bool managed) noexcept
            {
                for (auto& cinfo : mcursors) {
                    cinfo.cursor->setCache(cache);
                }
                mcachemanaged = managed;
                mmanagedcache = cache;
            }

            [[nodiscard]] bool isPlaying() const noexcept
            {
                return mplay;
            }

            [[nodiscard]] std::jthread::id threadID() const
            {
                return mthread.get_id();
            }

            [[nodiscard]] std::jthread::native_handle_type threadNativeHandle()
            {
                return mthread.native_handle();
            }

            [[nodiscard]] bool joinable() const
            {
                return mthread.joinable();
            }

#pragma endregion

            void initThread()
            {
                if (!mthread.joinable()) {
                    mthread = std::jthread([this](const std::stop_token& stop) {
                        playThread(stop);
                    });
                }
            }

            void setDivisionAll(MIDIDivision division)
            {
                for (auto& info : mcursors) {
                    info.cursor->setDivision(division);
                    info.cursor->recalcuateDivisionTiming();
                }
            }

            [[nodiscard]] Time baseTime() const
            {
                if (mcursors.empty()) {
                    return {};
                }
                auto activeCursors = mcursors | std::views::filter([](const auto& arg) {
                                         return arg.cursor->isActive();
                                     })
                                   | std::views::take(1);
                if (std::ranges::empty(activeCursors)) {
                    Time mxtime{};
                    for (const auto& cinfo : mcursors) {
                        mxtime = std::max(mxtime, cinfo.cursor->mplaytime - cinfo.offest);
                    }
                    return mxtime;
                }
                const CursorInfo& info = (*activeCursors.begin());
                return info.cursor->mplaytime - info.offest;
            }

            bool play()
            {
                if (mcursors.empty()) {
                    return false;
                }
                if (!mthread.joinable()) {
                    initThread();
                }
                mplay   = true;
                mwakeup = true;
                mcondvar.notify_all();
                return true;
            }

            /// \return Return if the player is playing before pause
            bool pause()
            {
                bool play = mplay;
                mplay     = false;
                return play;
            }

            [[nodiscard]] bool eof()
            {
                for (const auto& cinfo : mcursors) {
                    if (!cinfo.cursor->isActive()){
                        continue;
                    }
                    if (cinfo.cursor->eof()) {
                        return true;
                    }
                }
                return false;
            }

            void setTrack(const Track* data, bool updateCache = true)
            {
                Pauser pauser{*this};
                mlastSleptTime = 0ns;
                for (auto& cinfo : mcursors) {
                    cinfo.cursor->setTrack(data, updateCache);
                }
            }

            Cursor* addCursor(std::unique_ptr<Cursor> cursor, Time setoffest = {})
            {
                auto result = cursor.get();
                mcursors.emplace_back(std::move(cursor), setoffest);
                return result;
            }

            Cursor* addCursor(std::string name, Time setoffest = {})
            {
                return addCursor(std::make_unique<MIDIAdvTrkPlayerCursor<Track>>(std::move(name)), setoffest);
            }

            [[nodiscard]] auto& cursors() noexcept
            {
                return mcursors;
            }

            [[nodiscard]] const auto& cursors() const noexcept
            {
                return mcursors;
            }

            void syncDeviceStatusAll(bool force = false)
            {
                Pauser pauser{*this};
                for (auto& cinfo : mcursors) {
                    cinfo.cursor->syncDeviceStatus(force);
                }
            }

            void setCursorDeviceAll(AbstractMIDIDevice* device)
            {
                Pauser pauser{*this};
                for (auto& cinfo : mcursors) {
                    cinfo.cursor->setDevice(device);
                }
            }

            void goTo(Time targetTime)
            {
                Pauser pauser{*this};
                mlastSleptTime = {};
                for (auto& cursorinfo : mcursors) {
                    if (!cursorinfo.cursor->goTo(targetTime + cursorinfo.offest)) {
                        throw std::out_of_range("targetTime out of range");
                    }
                }
            }

        private:
            // RAII class for auto pause and play
            class Pauser {
            public:
                explicit Pauser(MIDIAdvancedTrackPlayer& seq)
                    : player(seq)
                    , toplay(seq.pause())
                {
                }

                ~Pauser()
                {
                    if (toplay) {
                        player.play();
                    }
                }

                Pauser(const Pauser&)            = delete;
                Pauser(Pauser&&)                 = delete;
                Pauser& operator=(const Pauser&) = delete;
                Pauser& operator=(Pauser&&)      = delete;

            private:
                MIDIAdvancedTrackPlayer& player;
                bool                     toplay;
            };

            struct CursorInfo {
                std::unique_ptr<Cursor> cursor;
                Time                    offest;
            };

            void playThread(const std::stop_token& token)
            {
                while (!token.stop_requested()) {
                    if (!mplay) {
                        for (auto& cursor : mcursors) {
                            cursor.cursor->notify(NotifyType::T_Mode);
                        }
                        std::unique_lock<std::mutex> lck(mmutex);
                        mcondvar.wait(lck, [this] { return mwakeup; });
                        mwakeup = false;
                        if (mplay) {
                            for (auto& cursor : mcursors) {
                                cursor.cursor->notify(NotifyType::T_Mode);
                            }
                        }
                    } else {
                        nanosleep(mlastSleptTime); // because mlastSleptTime may be changed (such as when revert snapshot)
                        std::chrono::nanoseconds minTime = std::chrono::nanoseconds::max();
                        for (auto& cursorinfo : mcursors | std::views::filter([](const auto& element) {
                                                                return element.cursor->isActive();
                                                            })) {
                            minTime = min(minTime, cursorinfo.cursor->tick(mlastSleptTime));
                        }
                        if (minTime == Time::max()) {
                            mplay = false;
                            continue;
                        }
                        minTime        = min(minTime, MAX_SLEEP);
                        mlastSleptTime = minTime;
                    }
                }
            }

            // return {Snapshot, playtime}

            // set position
            void updateCursors()
            {
                auto base = baseTime();
                for (auto&  cursorinfo : mcursors) {
                    cursorinfo.cursor->goTo(base + cursorinfo.offest);
                }
            }

            // Playback
            std::vector<CursorInfo> mcursors;
            // std::vector<std::chrono::nanoseconds> msleeptimecache; // use in playThread, cache sleep time of cursors
            Time mlastSleptTime{}; // if you changed playback data, set this to 0 and it will be recalcuated

            // Caching
            bool mcachemanaged = false;
            Cache* mmanagedcache{};

            // Thread
            std::jthread            mthread;
            bool                    mwakeup = false;
            std::mutex              mmutex;
            std::condition_variable mcondvar;
            bool                    mplay = false;
        };

    }

    using advtrkplayer::MIDIAdvancedTrackPlayer; // NOLINT(misc-unused-using-decls)
}
