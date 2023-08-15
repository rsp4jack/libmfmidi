/// \file midimultitrackcursor.hpp
/// \brief A class to iterate \a MIDIMultiTrack.

#pragma once

#include "libmfmidi/midimultitrack.hpp"
#include "libmfmidi/midistatus.hpp"
#include <type_traits>
#include <array>
#include <execution>
#include <limits>

namespace libmfmidi {
    class MIDIMultiTrackCursorTrackState {
    public:
        constexpr void reset() noexcept
        {
            absTime = 0;
            curmsg  = {};
        }

        constexpr void resetAll() noexcept
        {
            reset();
        }
        MIDIClockTime       absTime{};
        MIDITrack::const_iterator curmsg;
        MIDITrack::const_iterator end; // after-tail
    };

    /// \brief Extension of \a MIDIStatus for \a MIDIMultiTrackCursor
    class MIDIMultiTrackCursorState {
    public:
        constexpr void reset() noexcept
        {
            curmsg = {};
            curtrk = {};
        }

        constexpr void resetAll() noexcept
        {
            reset();          
            for (auto& track : trks) {
                track.reset();
            }
        }

        std::array<MIDIMultiTrackCursorTrackState, NUM_TRACKS> trks;
        MIDITrack::const_iterator                                    curmsg;
        decltype(trks)::iterator                               curtrk;
    };

    /// \brief A class to iterate all tracks of \a MIDIMultiTrack together.
    /// Calling it \a iterator is ambiguous.
    class MIDIMultiTrackCursor {
    public:
        constexpr explicit MIDIMultiTrackCursor(const MIDIMultiTrack* trk) noexcept
            : mtrk(trk)
        {
        }

        constexpr void reset() noexcept
        {
            mst.resetAll();
            for (uint16_t idx = 0; idx < mtrk->size(); ++idx) {
                if(!mtrk->at(idx).empty()){
                    mst.trks.at(idx).curmsg = mtrk->at(idx).begin()+idx;
                    mst.trks.at(idx).end     = mtrk->at(idx).end();
                    mst.trks.at(idx).absTime = mst.trks.at(idx).curmsg->deltaTime();
                }
            }
            findEarliestEvent();
        }

        constexpr bool goNextEvent() noexcept
        {
            ++mst.curtrk->curmsg;
            if (mst.curtrk->curmsg >= mst.curtrk->end) {
                mst.curtrk->curmsg = {};
                mst.curtrk->absTime = MIDICLKTM_MAX;
            } else {
                mst.curtrk->absTime += mst.curtrk->curmsg->deltaTime();
            }
            findEarliestEvent();
            return !(mst.curtrk->absTime == MIDICLKTM_MAX);
        }

        MIDITrack::const_iterator curEvent() const noexcept
        {
            return mst.curmsg;
        }

        MIDIClockTime curEventAbsTime() const noexcept
        {
            return mst.curtrk->absTime;
        }

        /// \brief Find the track that have earliest absTime to get the earliest event
        void findEarliestEvent() noexcept
        {
            auto trkst = std::min_element(std::execution::par_unseq, mst.trks.begin(), mst.trks.end(), [](const MIDIMultiTrackCursorTrackState& lhs, const MIDIMultiTrackCursorTrackState& rhs) {
                return lhs.absTime < rhs.absTime;
            });
            mst.curmsg = trkst->curmsg;
            mst.curtrk = trkst;
        }

    private:
        const MIDIMultiTrack*           mtrk;
        MIDIMultiTrackCursorState mst;
    };
}

