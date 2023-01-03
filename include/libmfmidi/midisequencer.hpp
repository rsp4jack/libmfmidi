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

/// \file midisequencer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDI Sequencer

#pragma once

#include "libmfmidi/abstractmididevice.hpp"
#include "libmfmidi/mfconcepts.hpp"
#include "libmfmidi/midiutils.hpp"
#include "libmfmidi/midimultitrack.hpp"
#include <span>
#include <thread>
#include <stop_token>
#include <condition_variable>
#include <mutex>
#include "libmfmidi/platformapi.hpp"
#include "libmfmidi/midistatus.hpp"
#include <map>
#include <utility>

namespace libmfmidi {
    namespace sequencer {
        template <std::ranges::forward_range Container>
            requires is_simple_type<Container>
        class MIDISequencer;

        /// \brief Internal class for holding status and play status
        ///
        template <std::ranges::forward_range Container>
        class MIDISequencerCursor {
            template <std::ranges::forward_range T>
                requires is_simple_type<T>
            friend class MIDISequencer;

        public:
            static constexpr bool randomAccessible = std::ranges::random_access_range<Container>;
            using Time                             = std::chrono::nanoseconds; // must be signed
            using id                               = uint16_t;

            explicit MIDISequencerCursor(MIDISequencer<Container>& seq, uint16_t idx) noexcept
                : mid(idx)
                , mseq(seq)
            {
                mstproc.setNotifier([&](NotifyType type) {
                    if (type == NotifyType::C_Tempo) {
                        recalcuateDivns();
                    }
                });
            }

            void recalcuateDivns() noexcept
            {
                mdivns = divisionToSec(mseq.mdivision, mstatus.tempo); // 1 to std::nano, auto cast
            }

            void setNotifier(MIDINotifierFunctionType func)
            {
                mnotifier = std::move(func);
            }

            Time tick(Time slept /*the time that slept*/)
            {
                assert(mactive);                                      // never tick when not active
                mplaytick += slept;                                   // not move it to sequencer thread because of lastSleptTime = 0 in revertSnapshot
                if (msleeptime != mnextevent->deltaTime() * mdivns) { // check if first tick
                    msleeptime = mnextevent->deltaTime() * mdivns;
                }
                assert(slept <= msleeptime);
                msleeptime -= slept;
                if (msleeptime != 0ns) {
                    return msleeptime;
                }

                mstproc.process(*mnextevent);
                if (mdev != nullptr) {
                    mdev->sendMsg(*mnextevent);
                }
                ++mnextevent;
                if (mnextevent == mseq.mdata->cend()) {
                    mactive = false;
                    return {};
                }
                msleeptime = mnextevent.deltaTime() * mdivns;
                return msleeptime;
            }

        private:
            // Information
            id                        mid;            // ID
            bool                      mactive = true; // tick-able
            MIDISequencer<Container>& mseq;

            // Playing status
            std::chrono::duration<double, std::nano> mdivns{};     // 1 delta-time in nanoseconds
            Time                                     msleeptime{}; // time to sleep
            Time                                     mplaytick{};  // current time, support 5850 centuries long

            // Data
            std::ranges::iterator_t<const Container> mnextevent;
            AbstractMIDIDevice*                      mdev{};
            MIDIStatus                               mstatus;

            // Misc
            MIDIStatusProcessor      mstproc{mstatus};
            MIDINotifierFunctionType mnotifier;
        };

        /// \brief A powerful sequencer
        ///
        template <std::ranges::forward_range Container>
            requires is_simple_type<Container>
        class MIDISequencer {
            template <std::ranges::forward_range>
            friend class MIDISequencerCursor;

        public:
            using Cursor                                         = MIDISequencerCursor<Container>;
            using CursorID                                       = Cursor::id;
            using Time                                           = Cursor::Time;
            static constexpr Time                 MAX_SLEEP      = 500ms;
            static constexpr std::chrono::seconds CACHE_INTERVAL = 1min; // seconds

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

            [[nodiscard]] std::jthread::id threadID() const
            {
                return mthread.get_id();
            }

            [[nodiscard]] bool joinable() const
            {
                return mthread.joinable();
            }

            void setDivision(MIDIDivision division)
            {
                mdivision = division;
            }

            [[nodiscard]] MIDIDivision division() const
            {
                return mdivision;
            }

            [[nodiscard]] Time baseTime() const
            {
                if (!mcursors.empty()) {
                    return {};
                }
                CursorInfo& info = (mcursors | std::views::filter([](const auto& arg) {
                                        return arg.second.cursor.mactive;
                                    })
                                    | std::views::take(1))[0]
                                       .second;
                return info.cursor.mplaytick - info.offest;
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

            /// \return Return if the sequencer is playing before pause
            bool pause()
            {
                bool play = mplay;
                mplay     = false;
                return play;
            }

            void generateCache()
            {
                if (!museCache) {
                    return;
                }
                mcaches.clear();
                Time                                     playtick{};
                std::ranges::iterator_t<const Container> nextevent = mdata->cbegin();
                MIDIStatus                               status;
                MIDIStatusProcessor                      proc{status};
                Time                                     divns         = divisionToSec(mdivision, status.tempo);
                Time                                     nextCacheTime = CACHE_INTERVAL;
                Snapshot                                 snap{};
                proc.setNotifier([&](NotifyType type) {
                    if (type == NotifyType::C_Tempo) {
                        divns = divisionToSec(mdivision, status.tempo);
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

            void setData(const Container* data) noexcept
            {
                Pauser pauser;
                mdata          = data;
                mlastSleptTime = 0;
                if (museCache) {
                    generateCache();
                }
            }

            /// \note If you are adding a cursor using a device that does not need to revert MIDIStatus, set \a revertStatus to false.
            CursorID addCursor(AbstractMIDIDevice* device, Time offest, bool revertStatus = true)
            {
                if (mdata == nullptr) {
                    throw std::logic_error("mdata is nullptr");
                }
                Pauser pauser;

                Cursor cursor{*this, musableID};
                cursor.mdev         = device;
                cursor.mnextevent   = mdata->cbegin();
                mcursors[musableID] = {std::move(cursor), offest, revertStatus};
                goTo(musableID, baseTime() + offest);
                return musableID++;
            }

            void removeCursor(CursorID cursorid)
            {
                if (!mcursors.contains(cursorid)) {
                    throw std::invalid_argument("index out of range");
                }
                Pauser pauser;
                mcursors.erase(cursorid);
            }

            void activeCursor(CursorID cursorid, bool active = true)
            {
                if (!mcursors.contains(cursorid)) {
                    throw std::invalid_argument("index out of range");
                }
                Pauser pauser;
                updateCursors();
                mcursors[cursorid].cursor.mactive = active;
            }

            void goTo(Time targetTime)
            {
                Pauser pauser;
                for (auto& [cursorid, cursorinfo] : mcursors) {
                    goTo(targetTime + cursorinfo.offest, cursorid);
                }
            }

        private:
            // RAII class for auto pause and play
            class Pauser {
            public:
                explicit Pauser(MIDISequencer& seq)
                    : sequencer(seq)
                    , toplay(seq.pause())
                {
                }

                ~Pauser()
                {
                    if (toplay) {
                        sequencer.play();
                    }
                }

                Pauser(const Pauser&)            = delete;
                Pauser(Pauser&&)                 = delete;
                Pauser& operator=(const Pauser&) = delete;
                Pauser& operator=(Pauser&&)      = delete;

            private:
                MIDISequencer& sequencer;
                bool           toplay;
            };

            struct CursorInfo {
                Cursor cursor;
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
                        std::unique_lock<std::mutex> lck(mmutex);
                        mcondvar.wait(lck, [&] { return mwakeup; });
                        mwakeup = false;
                    } else {
                        nanosleep(mlastSleptTime); // because mlastSleptTime may be changed (such as when revert snapshot)
                        std::chrono::nanoseconds minTime = std::chrono::nanoseconds::max();
                        for (auto& [cursorid, cursorinfo] : mcursors | std::views::filter([](const auto& element) {
                                                                return element.second.cursor.mactive;
                                                            })) {
                            minTime = min(minTime, cursorinfo.cursor.tick(mlastSleptTime));
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
                if (!mcursors.contains(cursorid)) {
                    throw std::invalid_argument("index out of range");
                }
                Snapshot snap;
                snap.sleeptime = mcursors[cursorid].cursor.msleeptime;
                snap.nextevent = mcursors[cursorid].cursor.mnextevent;
                snap.status    = mcursors[cursorid].cursor.mstatus;
                return {std::move(snap), mcursors[cursorid].cursor.mplaytick};
            }

            void revertSnapshot(CursorID cursorid, const Snapshot& snapshot, Time playtick)
            {
                if (!mcursors.contains(cursorid)) {
                    throw std::invalid_argument("index out of range");
                }
                mlastSleptTime                       = {}; // = 0
                mcursors[cursorid].cursor.msleeptime = snapshot.sleeptime;
                mcursors[cursorid].cursor.mmplaytick = playtick;
                mcursors[cursorid].cursor.mstatus    = snapshot.status;
                mcursors[cursorid].cursor.mnextevent = snapshot.nextevent;
            }

            bool goTo(Time targetTime, CursorID cursorid)
            {
                if (!mcursors.contains(cursorid)) {
                    throw std::invalid_argument("index out of range");
                }
                if (targetTime == mcursors[cursorid].cursor.mplaytick) {
                    return true;
                }
                mlastSleptTime = 0;
                if (targetTime < mcursors[cursorid].cursor.mplaytick) {
                    revertSnapshot(cursorid, {}, {}); // reset
                }
                if (museCache) {
                    for (auto& [playtick, snap] : mcaches | std::views::reverse) {
                        if (playtick <= targetTime) {
                            revertSnapshot(cursorid, snap, playtick);
                        }
                    }
                }
                return directGoTo(targetTime, cursorid); // for the rest
            }

            bool directGoTo(Time targetTime, CursorID cursorid)
            {
                if (!mcursors.contains(cursorid)) {
                    throw std::invalid_argument("index out of range");
                }
                Cursor& cur = mcursors[cursorid];
                assert(cur.mplaytick < targetTime);
                std::ranges::iterator_t<const Container> currentevent = cur.mnextevent;
                while (cur.mplaytick + cur.mnextevent->deltaTime() * cur.mdivns < targetTime) { // use <, see cur.msleeptime under
                    currentevent = cur.mnextevent;
                    cur.mplaytick += currentevent->deltaTime() * cur.mdivns;
                    cur.mproc.process(*currentevent);
                    ++cur.mnextevent;
                    if (cur.mnextevent == mdata->cend()) { // should not happen if targetTime is not out of range, because uses < above (so nextevent never point to the end iterator (sentinal))
                        return false;
                    }
                }
                cur.msleeptime = cur.mplaytick + cur.mnextevent->deltaTime() * cur.mdivns - targetTime; // if there is a event on targetTime, msleeptime will be 0 and mnextevent will play immediately when tick
                cur.mplaytick  = targetTime;                                                            // this must be after cur.msleeptime, see above
            }

            // set position
            void updateCursors()
            {
                for (auto& [cursorr, cursorinfo] : mcursors) {
                    cursorinfo.cursor.goTo(baseTime() + cursorinfo.offest);
                }
            }

            // Thread
            std::jthread            mthread;
            bool                    mwakeup = false;
            std::mutex              mmutex;
            std::condition_variable mcondvar;
            bool                    mplay = false;

            // Playback
            MIDIDivision                             mdivision{};
            const Container*                         mdata{};
            std::unordered_map<uint16_t, CursorInfo> mcursors;
            CursorID                                 musableID = 0;
            // std::vector<std::chrono::nanoseconds> msleeptimecache; // use in playThread, cache sleep time of cursors
            Time mlastSleptTime{}; // if you changed playback data, set this to 0 and it will be recalcuated

            // Caching
            bool                                     museCache = true;
            std::map<std::chrono::seconds, Snapshot> mcaches;
        };

    }

    using sequencer::MIDISequencer; // NOLINT(misc-unused-using-decls)
    MIDISequencer<MIDITrack> abc;
}
