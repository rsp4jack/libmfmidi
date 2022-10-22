/// \file midimultitrackcursor.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief A class to iterate \a MIDIMultiTrack.

#pragma once

#include "midimultitrack.hpp"
#include "midistate.hpp"
#include <type_traits>
#include <array>
#include <execution>
#include <limits>

namespace libmfmidi {
    class MIDIMultiTrackCursorTrackState {
    public:
        void reset() noexcept
        {
            absTime = 0;
            curmsg  = {};
        }

        void resetAll() noexcept
        {
            reset();
        }
        MIDIClockTime       absTime{};
        MIDITrack::iterator curmsg;
        MIDITrack::iterator end; // after-tail
    };

    /// \brief Extension of \a MIDIState for \a MIDIMultiTrackCursor
    class MIDIMultiTrackCursorState {
    public:
        void reset() noexcept
        {
            curmsg = {};
            curtrk = {};
        }

        void resetAll() noexcept
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
        explicit MIDIMultiTrackCursor(MIDIMultiTrack* trk) noexcept
            : mtrk(trk)
        {
        }

        void reset() noexcept
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

        bool goNextEvent() noexcept
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
        MIDIMultiTrack*           mtrk;
        MIDIMultiTrackCursorState mst;
    };
}
