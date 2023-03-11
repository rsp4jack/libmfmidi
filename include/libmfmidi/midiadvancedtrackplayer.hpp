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
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Advanced MIDI track player

#pragma once

#include "libmfmidi/abstractmididevice.hpp"
#include "libmfmidi/mfconcepts.hpp"
#include "libmfmidi/midiutils.hpp"
#include <span>
#include <thread>
#include <stop_token>
#include <condition_variable>
#include <mutex>
#include "libmfmidi/platformapi.hpp"
#include "libmfmidi/midistatus.hpp"
#include <map>
#include <utility>
#include <functional>


namespace libmfmidi {
    using namespace std::literals;

    namespace advtrkplayer {
        template <std::ranges::forward_range Container>
            requires is_simple_type<Container>
        class MIDIAdvancedTrackPlayer;

        /// \brief Internal class for holding status and play status
        ///
        template <std::ranges::forward_range Container>
        class MIDIAdvTrkPlayerCursor : protected NotifyUtils<MIDIAdvTrkPlayerCursor<Container>> {
            template <std::ranges::forward_range T>
                requires is_simple_type<T>
            friend class MIDIAdvancedTrackPlayer;
            friend class NotifyUtils<MIDIAdvTrkPlayerCursor<Container>>;

        public:
            static constexpr bool randomAccessible = std::ranges::random_access_range<Container>;
            using Time                             = std::chrono::nanoseconds; // must be signed
            using id                               = uint16_t;
            using NotifyUtils<MIDIAdvTrkPlayerCursor<Container>>::addNotifier;

            explicit MIDIAdvTrkPlayerCursor(MIDIAdvancedTrackPlayer<Container>& seq, uint16_t idx) noexcept
                : mid(idx)
                , mseq(seq)
            {
                /*
                 * A bug with move semantics and lambda capture
                 * Lambda capture "this" pointer, but when *this moves to another memory location, "this" in capture will NOT be changed!
                 * So move and copy is disabled in this class.
                 * See https://stackoverflow.com/q/44745255/16016815
                 */
                mstproc.addNotifier([&](NotifyType type) {
                    if (type == NotifyType::C_Tempo) {
                        recalcuateDivns();
                    }
                });
            }

            ~MIDIAdvTrkPlayerCursor() noexcept = default;
            MF_DISABLE_COPY(MIDIAdvTrkPlayerCursor);
            MF_DISABLE_MOVE(MIDIAdvTrkPlayerCursor); //! Disable move semantics, see above

            void recalcuateDivns() noexcept
            {
                mdivns = divisionToDuration(mseq.get().mdivision, mstatus.tempo);
            }

            Time tick(Time slept /*the time that slept*/)
            {
                assert(mactive);                                      // never tick when not active
                mplaytick += slept;                                   // not move it to playthread because of lastSleptTime = 0 in revertSnapshot
                if (msleeptime == 0ns) { // check if first tick
                    msleeptime = mnextevent->deltaTime() * mdivns; // usually first tick
                }
                assert(slept <= msleeptime);
                msleeptime -= slept;
                if (msleeptime != 0ns) {
                    return msleeptime;
                }

                if (mnextevent == mseq.get().mdata->cend()) {
                    mactive = false;
                    return {};
                }

                MIDITimedMessage message;
                message.resize(mnextevent->size());
                // std::ranges::copy(*mnextevent, message.begin());
                std::copy(mnextevent->cbegin(), mnextevent->cend(), message.begin());

                mstproc.process(message);
                if (process(message) && mdev != nullptr) {
                    mdev->sendMsg(message);
                }
                ++mnextevent;
                if (mnextevent == mseq.get().mdata->cend()) {
                    mactive = false;
                    return {};
                }
                msleeptime = mnextevent->deltaTime() * mdivns;
                return msleeptime;
            }

            [[nodiscard]] const MIDIStatus& status() const &
            {
                return mstatus;
            }

            [[nodiscard]] bool isActive() const{
                return mactive;
            }

        protected:
            using NotifyUtils<MIDIAdvTrkPlayerCursor<Container>>::notify;

        private:
            // Information
            id                                                         mid;            // ID
            bool                                                       mactive = true; // tick-able
            std::reference_wrapper<MIDIAdvancedTrackPlayer<Container>> mseq;

            // Playing status
            Time mdivns{};     // 1 delta-time in nanoseconds
            Time msleeptime{}; // time to sleep
            Time mplaytick{};  // current time, support 5850 centuries long

            // Data
            std::ranges::iterator_t<const Container> mnextevent;
            AbstractMIDIDevice*                      mdev{};
            MIDIStatus                               mstatus;

            // Misc
            MIDIStatusProcessor   mstproc{mstatus};
            MIDIProcessorFunction mprocessor;

            inline bool process(MIDITimedMessage& msg) const noexcept
            {
                return !mprocessor || mprocessor(msg);
            }
        };

        /// \brief A powerful MIDI track player
        /// Support multi-cursor
        template <std::ranges::forward_range Container>
            requires is_simple_type<Container>
        class MIDIAdvancedTrackPlayer {
            template <std::ranges::forward_range>
            friend class MIDIAdvTrkPlayerCursor;

        public:
            using Cursor                                         = MIDIAdvTrkPlayerCursor<Container>;
            using CursorID                                       = typename Cursor::id;
            using Time                                           = typename Cursor::Time;
            static constexpr Time                 MAX_SLEEP      = 500ms;
            static constexpr std::chrono::minutes CACHE_INTERVAL = 1min; // seconds

            MIDIAdvancedTrackPlayer() noexcept = default;

            ~MIDIAdvancedTrackPlayer() noexcept
            {
                pause();
                mthread.request_stop();
                mwakeup = true;
                mcondvar.notify_all();
            }

            MF_DISABLE_COPY(MIDIAdvancedTrackPlayer);
            MF_DEFAULT_MOVE(MIDIAdvancedTrackPlayer);

            [[nodiscard]] inline bool useCache() const noexcept
            {
                return museCache;
            }

            inline void setUseCache(bool use) noexcept
            {
                museCache = use;
            }

            void initThread()
            {
                if (!mthread.joinable()) {
                    mthread = std::jthread([&](const std::stop_token& stop) {
                        playThread(stop);
                    });
                }
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

            void setDivision(MIDIDivision division)
            {
                mdivision = division;
                for (auto& [id, info] : mcursors) {
                    info.cursor->recalcuateDivns();
                }
            }

            [[nodiscard]] MIDIDivision division() const
            {
                return mdivision;
            }

            [[nodiscard]] Time baseTime() const
            {
                if (mcursors.empty()) {
                    return {};
                }
                auto activeCursors = mcursors | std::views::filter([](const auto& arg) {
                                         return arg.second.cursor->active();
                                     })
                                   | std::views::take(1);
                if (std::ranges::empty(activeCursors)) {
                    if (mcursors.size() == 1) {
                        const CursorInfo& info = mcursors.begin()->second;
                        return info.cursor->mplaytick - info.offest;
                    }
                    return {};
                }
                const CursorInfo& info = (*activeCursors.begin()).second;
                return info.cursor->mplaytick - info.offest;
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

            void generateCache()
            {
                if (!museCache || mdata == nullptr) {
                    return;
                }
                mcaches.clear();
                Time                                     playtick{};
                std::ranges::iterator_t<const Container> nextevent = mdata->cbegin();
                MIDIStatus                               status;
                MIDIStatusProcessor                      proc{status};
                Time                                     divns         = divisionToDuration(mdivision, status.tempo);
                std::chrono::minutes                     nextCacheTime = CACHE_INTERVAL;
                Snapshot                                 snap{};
                proc.addNotifier([&](NotifyType type) {
                    if (type == NotifyType::C_Tempo) {
                        divns = divisionToDuration(mdivision, status.tempo);
                    }
                });
                while (true) {
                    while (playtick + nextevent->deltaTime() * divns < nextCacheTime) { // use < because ignore the event on nextCacheTime (the event will play when tick, but have not played yet when cache)
                        playtick += nextevent->deltaTime() * divns;
                        proc.process(*nextevent);
                        ++nextevent;
                        if (nextevent == mdata->cend()) {
                            return; // see directGoTo
                        }
                    }
                    snap.nextevent         = nextevent;
                    snap.sleeptime         = playtick + nextevent->deltaTime() * divns - nextCacheTime; // see directGoTo
                    snap.status            = status;
                    mcaches[nextCacheTime] = std::move(snap);
                    nextCacheTime += CACHE_INTERVAL;
                }
            }

            void setData(const Container* data)
            {
                Pauser pauser{*this};
                mdata          = data;
                mlastSleptTime = 0ns;
                if (museCache) {
                    generateCache();
                }
                goTo(baseTime());
            }

            /// \note If you are adding a cursor using a device that does not need to revert MIDIStatus, set \a revertStatus to false.
            CursorID addCursor(AbstractMIDIDevice* device, Time offest, bool revertStatus = true)
            {
                if (mdata == nullptr) {
                    throw std::logic_error("mdata is nullptr");
                }
                Pauser pauser{*this};

                // Cursor cursor{*this, musableID};
                auto cursor = std::make_unique<Cursor>(*this, musableID);
                cursor->mdev       = device;
                cursor->mnextevent = mdata->cbegin();
                mcursors.insert_or_assign(musableID, CursorInfo{std::move(cursor), offest, revertStatus});
                cursorGoTo(musableID, baseTime() + offest);
                return musableID++;
            }

            void removeCursor(CursorID cursorid)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Pauser pauser{*this};
                mcursors.erase(cursorid);
            }

            void activeCursor(CursorID cursorid, bool active = true)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Pauser pauser{*this};
                updateCursors();
                mcursors.at(cursorid).cursor->mactive = active;
            }

            bool isCursorActive(CursorID cursorid) const
            {
                return mcursors.at(cursorid).cursor->active();
            }

            void setCursorProcessor(CursorID cursorid, const MIDIProcessorFunction& func)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Pauser pauser{*this};
                mcursors.at(cursorid).cursor->mprocessor = func;
            }

            [[nodiscard]] bool checkCursorID(CursorID cursorid) const noexcept
            {
                return mcursors.contains(cursorid);
            }

            [[nodiscard]] auto& cursors() noexcept
            {
            	return mcursors;
            }

            [[nodiscard]] const auto& cursors() const noexcept
            {
            	return mcursors;
            }

            void syncDeviceStatus(CursorID cursorid) const
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Pauser pauser{*this};
                for (const auto& msg : reportMIDIStatus(mcursors.at(cursorid).cursor->mstatus, false)) {
                    mcursors.at(cursorid).cursor->mdev.sendMsg(msg);
                }
            }

            void setCursorDevice(CursorID cursorid, AbstractMIDIDevice* device) const
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Pauser pauser{*this};
                mcursors.at(cursorid).cursor->mdev = device;
                if (mcursors.at(cursorid).revertStatus) {
                    syncDeviceStatus(cursorid);
                }
            }

            void addCursorNotifier(CursorID cursorid, const MIDINotifierFunctionType& func)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Pauser pauser{*this};
                mcursors.at(cursorid).cursor->addNotifier(func);
            }

            void goTo(Time targetTime)
            {
                Pauser pauser{*this};
                for (auto& [cursorid, cursorinfo] : mcursors) {
                    if(!cursorGoTo(cursorid, targetTime + cursorinfo.offest)){
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
                Time   offest;
                bool   revertStatus;
            };

            struct Snapshot {
                Time sleeptime{}; // time to next event
                // playtime is key // current time
                std::ranges::iterator_t<const Container> nextevent{}; // next event, if sleeptime==0, this is current event
                MIDIStatus                               status{};    // status of played events
            };

            void playThread(const std::stop_token& token)
            {
                while (!token.stop_requested()) {
                    if (!mplay) {
                        for (auto& [cid, cursor] : mcursors) {
                            cursor.cursor->notify(NotifyType::T_Mode);
                        }
                        std::unique_lock<std::mutex> lck(mmutex);
                        mcondvar.wait(lck, [&] { return mwakeup; });
                        mwakeup = false;
                        if (mplay) {
                            for (auto& [cid, cursor] : mcursors) {
                                cursor.cursor->notify(NotifyType::T_Mode);
                            }
                        }
                    } else {
                        nanosleep(mlastSleptTime); // because mlastSleptTime may be changed (such as when revert snapshot)
                        std::chrono::nanoseconds minTime = std::chrono::nanoseconds::max();
                        for (auto& [cursorid, cursorinfo] : mcursors | std::views::filter([](const auto& element) {
                                                                return element.second.cursor->active();
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

            // return {Snapshot, playtick}
            std::pair<Snapshot, Time> captureSnapshot(CursorID cursorid)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Snapshot snap;
                snap.sleeptime = mcursors.at(cursorid).cursor->msleeptime;
                snap.nextevent = mcursors.at(cursorid).cursor->mnextevent;
                snap.status    = mcursors.at(cursorid).cursor->mstatus;
                return {std::move(snap), mcursors.at(cursorid).cursor->mplaytick};
            }

            void revertSnapshot(CursorID cursorid, const Snapshot& snapshot, Time playtick)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                mlastSleptTime                          = {}; // = 0
                mcursors.at(cursorid).cursor->msleeptime = snapshot.sleeptime;
                mcursors.at(cursorid).cursor->mplaytick  = playtick;
                mcursors.at(cursorid).cursor->mstatus    = snapshot.status;
                mcursors.at(cursorid).cursor->mnextevent = snapshot.nextevent;
            }

            Snapshot defaultSnapshot() const
            {
                return {0ns, mdata->cbegin(), {}};
            }

            bool cursorGoTo(CursorID cursorid, Time targetTime)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                if (targetTime < 0ns) {
                    throw std::out_of_range("targetTime < 0");
                }
                if (mcursors.at(cursorid).cursor->mnextevent == mdata->cend()) {
                    revertSnapshot(cursorid, defaultSnapshot(), {}); // reset
                }
                mcursors.at(cursorid).cursor->recalcuateDivns();
                if (targetTime == mcursors.at(cursorid).cursor->mplaytick) {
                    return true;
                }
                mlastSleptTime = 0ns;
                if (targetTime < mcursors.at(cursorid).cursor->mplaytick) {
                    revertSnapshot(cursorid, defaultSnapshot(), {}); // reset
                }
                if (museCache) {
                    for (auto& [playtick, snap] : mcaches | std::views::reverse) {
                        if (playtick <= targetTime) {
                            revertSnapshot(cursorid, snap, playtick);
                        }
                    }
                }
                bool result = directGoTo(targetTime, cursorid); // for the rest
                if (mcursors.at(cursorid).revertStatus && mcursors.at(cursorid).cursor->mdev != nullptr) {
                    for (const auto& msg : reportMIDIStatus(mcursors.at(cursorid).cursor->mstatus, false)) {
                        mcursors.at(cursorid).cursor->mdev->sendMsg(msg);
                    }
                }
                mcursors.at(cursorid).cursor->recalcuateDivns();
                return result;
            }

            bool directGoTo(Time targetTime, CursorID cursorid)
            {
                if (!checkCursorID(cursorid)) {
                    throw std::out_of_range("index out of range");
                }
                Cursor& cur = *(mcursors.at(cursorid).cursor);
                assert(cur.mplaytick <= targetTime);
                std::ranges::iterator_t<const Container> currentevent = cur.mnextevent;
                while (cur.mplaytick + cur.mnextevent->deltaTime() * cur.mdivns < targetTime) { // use <, see cur.msleeptime under
                    currentevent = cur.mnextevent;
                    cur.mplaytick += currentevent->deltaTime() * cur.mdivns;
                    cur.mstproc.process(*currentevent);
                    ++cur.mnextevent;
                    if (cur.mnextevent == mdata->cend()) { // should not happen if targetTime is not out of range, because uses < above (so nextevent never point to the end iterator (sentinal))
                        return false;
                    }
                }
                cur.msleeptime = cur.mplaytick + cur.mnextevent->deltaTime() * cur.mdivns - targetTime; // if there is a event on targetTime, msleeptime will be 0 and mnextevent will play immediately when tick
                cur.mplaytick  = targetTime;                                                            // this must be after cur.msleeptime, see above
                return true;
            }

            // set position
            void updateCursors()
            {
                auto base = baseTime();
                for (auto& [cursor, cursorinfo] : mcursors) {
                    cursorGoTo(cursor, base + cursorinfo.offest);
                }
            }

            // Playback
            MIDIDivision                             mdivision{};
            const Container*                         mdata{};
            std::unordered_map<uint16_t, CursorInfo> mcursors;
            CursorID                                 musableID = 0;
            // std::vector<std::chrono::nanoseconds> msleeptimecache; // use in playThread, cache sleep time of cursors
            Time mlastSleptTime{}; // if you changed playback data, set this to 0 and it will be recalcuated

            // Caching
            bool                                     museCache = true;
            std::map<std::chrono::minutes, Snapshot> mcaches;

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
