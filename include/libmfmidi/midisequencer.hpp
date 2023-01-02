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
            using id                               = size_t;

            explicit MIDISequencerCursor(MIDISequencer<Container>& seq, size_t id) noexcept
                : mid(id)
                , mseq(seq)
            {
            }

            void recalcuateDivns() noexcept
            {
                mdivns = divisionToSec(mdivision, mstatus.tempo); // 1 to std::nano, auto cast
            }

            void setNotifier(MIDINotifierFunctionType func)
            {
                mnotifier = std::move(func);
            }

            Time tick(Time slept /*the time that slept*/)
            {
                mplaytick += slept; // not move it to sequencer thread because of lastSleptTime = 0 in revertSnapshot
                if (msleeptime != mcurit->deltaTime()) { // check if first tick
                    msleeptime = mcurit->deltaTime();
                }
                assert(slept <= msleeptime);
                msleeptime -= slept;
                if (msleeptime != 0ns) {
                    return msleeptime;
                }

                mstproc.process(*mcurit);
                if (mdev != nullptr) {
                    mdev->sendMsg(*mcurit);
                }
                ++mcurit;
                if (mcurit == mseq.mdata->cend()) {
                    mactive = false;
                    return {};
                }
                msleeptime = mcurit.deltaTime() * mdivns;
                return msleeptime;
            }

        private:
            // Information
            id                        mid;            // ID
            bool                      mactive = true; // tick-able
            MIDISequencer<Container>& mseq;

            // Playing status
            MIDIDivision                             mdivision{};
            std::chrono::duration<double, std::nano> mdivns{};     // 1 delta-time in nanoseconds
            Time                                     msleeptime{}; // time to sleep
            Time                                     mplaytick{};  // current time, support 5850 centuries long

            // Data
            std::ranges::iterator_t<const Container> mcurit;
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
            using Cursor                    = MIDISequencerCursor<Container>;
            using CursorID                  = Cursor::id;
            using Time                      = Cursor::Time;
            static constexpr Time MAX_SLEEP = 500ms;

            [[nodiscard]] inline bool useCache() noexcept
            {
                return museCache;
            }

            inline void setUseCache(bool use) noexcept
            {
                museCache = use;
            }

            void regenerateCache()
            {
                if (!museCache) {
                    return;
                }
            }

            void setData(const Container* data) noexcept
            {
                mdata = data;
            }

            /// \note If you are adding a cursor using a device that does not need to revert MIDIStatus, set \a revertStatus to false.
            CursorID addCursor(AbstractMIDIDevice* device, Time offest, bool revertStatus = true)
            {
                if (mdata == nullptr) {
                    throw std::logic_error("mdata is nullptr");
                }
                Cursor cursor{*this, musableID++};
                cursor.mdev   = device;
                cursor.mcurit = mdata->cbegin();
                mcursors.push_back({std::move(cursor), offest, revertStatus});
            }

        private:
            struct CursorInfo {
                Cursor cursor;
                Time   offest;
                bool   revertStatus;
            };

            struct Snapshot {
                Time sleeptime;
                // playtime is key
                std::ranges::iterator_t<const Container> curit;
                MIDIStatus                               status;
            };

            void playThread(const std::stop_token& token)
            {
                while (!token.stop_requested()) {
                    if (!mplay) {
                        std::unique_lock<std::mutex> lck(mmutex);
                        mcondvar.wait(lck, [&] { return mwakeup; });
                        mwakeup = false;
                    } else {
                        std::chrono::nanoseconds minTime = std::chrono::nanoseconds::max();
                        for (size_t idx = 0; idx < mcursors.size(); ++idx) {
                            minTime = min(minTime, (mcursors[idx].cursor.mactive ? mcursors[idx].cursor.tick(mlastSleptTime) : Time::max()));
                        }
                        minTime = min(minTime, MAX_SLEEP);
                        nanosleep(minTime);
                        mlastSleptTime = minTime;
                    }
                }
            }

            void revertSnapshot(CursorID cursorid, const Snapshot& snapshot, Time playtick)
            {
                if (cursorid >= mcursors.size()) {
                    throw std::invalid_argument("index out of range");
                }
                mlastSleptTime                       = {}; // = 0
                mcursors[cursorid].cursor.msleeptime = snapshot.sleeptime;
                mcursors[cursorid].cursor.mmplaytick = playtick;
                mcursors[cursorid].cursor.mstatus    = snapshot.status;
                mcursors[cursorid].cursor.mcurit     = snapshot.curit;
            }

            // Thread
            std::jthread            mthread;
            bool                    mwakeup = false;
            std::mutex              mmutex;
            std::condition_variable mcondvar;
            bool                    mplay = false;

            // Playback
            const Container*        mdata;
            std::vector<CursorInfo> mcursors;
            CursorID                musableID = 0;
            // std::vector<std::chrono::nanoseconds> msleeptimecache; // use in playThread, cache sleep time of cursors
            Time mlastSleptTime{};

            // Caching
            bool                                     museCache = true;
            std::map<std::chrono::seconds, Snapshot> mcaches;
        };

    }

    using sequencer::MIDISequencer; // NOLINT(misc-unused-using-decls)
    MIDISequencer<MIDITrack> abc;
}
