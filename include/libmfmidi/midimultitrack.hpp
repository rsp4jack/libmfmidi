/// \file midimultitrack.cpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDIMultiTrack

#pragma once

#include "libmfmidi/miditrack.hpp"
#include <unordered_set>
#include "gfx/timsort.hpp"

namespace libmfmidi {
    /// \brief A class to storage MIDITracks.
    /// inherit from std::vector<MIDITrack>
    /// \warning This is not a MIDI 1 file. See \a MIDIFile .
    /// \sa MIDIFile, MIDITrack
    class MIDIMultiTrack : public std::vector<MIDITrack> {
    public:
        [[nodiscard]] constexpr auto numTracksWithEvents() const
        {
            return std::count_if(cbegin(), cend(), [](const MIDITrack& trk) {
                return !trk.empty();
            });
        }

        // MIDI Division is moved to MIDIFile.

        /// Track 0: Condstruer: Tempo, time sign, key sign, text...
        /// Track 1: System reset: SysEx, other things...
        /// Track 2...: Channels
        /// \warning Track 2 to end may be not sorted. Please use \a ensureSort .
        void separateChannelsToTracks(const MIDITrack& src, bool ensureSort = false)
        {
            clear();
            resize(2);
            std::unordered_set<uint8_t> indexS;   // index set for *contains*
            std::array<uint16_t, 16>    indexM{}; // map
            uint16_t                    freeindex = 2;
            for (const auto& msg : src) {
                if (msg.isChannelMsg()) {
                    if (!indexS.contains(msg.channel())) {
                        resize(freeindex + 1);
                        indexS.insert(msg.channel());
                        indexM.at(msg.channel() - 1) = freeindex;
                        at(freeindex).push_back(msg);
                        ++freeindex;
                    } else {
                        at(indexM.at(msg.channel() - 1)).push_back(msg);
                    }
                } else {
                    if (msg.isTempo() || msg.isTextEvent() || msg.isTimeSignature() || msg.isKeySignature()) {
                        at(0).push_back(msg);
                    } else {
                        at(1).push_back(msg);
                    }
                }
            }
            if (ensureSort) {
                std::sort(begin() + 2, end(), [](const auto& lhs, const auto& rhs) {
                    return lhs[0].channel() < rhs[0].channel();
                });
            }
        }
    };

    constexpr void toAbsTimeMultiTrack(MIDIMultiTrack& mtrk)
    {
        for (auto& trk : mtrk) {
            toAbsTimeTrack(trk);
        }
    }

    constexpr void toRelTimeMultiTrack(MIDIMultiTrack& mtrk)
    {
        for (auto& trk : mtrk) {
            toRelTimeTrack(trk);
        }
    }

    inline void mergeMultiTrack(MIDIMultiTrack&& mtrk, MIDITrack& trk)
    {
        toAbsTimeMultiTrack(mtrk);
        trk.clear();
        size_t middle;
        size_t size = 0;
        for (const auto& tr : mtrk) {
            size += tr.size();
        }
        trk.reserve(trk.size() + size);
        for (auto& tr : mtrk) {
            middle = trk.size();
            for (auto& ev : tr) {
                if (!ev.isEndOfTrack()) {
                    trk.push_back(std::move(ev));
                }
            }
            if (middle < trk.size()) {
                gfx::timmerge(trk.begin(), trk.begin() + middle, trk.end());
            }
        }
        // TODO: Remove it
        //assert(std::is_sorted(trk.cbegin(), trk.cend()));
        toRelTimeTrack(trk);
    }
}
