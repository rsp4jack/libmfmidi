#pragma once

#include "libmfmidi/miditrack.hpp"
#include <unordered_set>
#include "gfx/timsort.hpp"

namespace libmfmidi {
    /// not SMF Type 1
    using MIDIMultiTrack = std::vector<MIDITrack>;

    /// Track 0: Conductor
    /// Track 1: SysEx
    /// Track 2...: Channels (unordered, use \a ensureOrder to make it ordered)
    inline MIDIMultiTrack separateChannelsToTracks(MIDITrack&& src, bool ensureOrder = false)
    {
        MIDIMultiTrack mtrk;
        mtrk.resize(2);
        std::unordered_set<uint8_t> indexS;   // track if a channel is contained in mtrk
        std::array<uint16_t, 16>    indexM{}; // map channels to tracks
        uint16_t                    freeindex = 2; // the index to storage new track
        for (auto& msg : src) {
            if (msg.isChannelMsg()) {
                if (!indexS.contains(msg.channel())) {
                    mtrk.resize(freeindex + 1);
                    indexS.insert(msg.channel());
                    indexM.at(msg.channel() - 1) = freeindex;
                    mtrk.at(freeindex).push_back(std::move(msg));
                    ++freeindex;
                } else {
                    mtrk.at(indexM.at(msg.channel() - 1)).push_back(std::move(msg));
                }
            } else {
                if (msg.isTempo() || msg.isTextEvent() || msg.isTimeSignature() || msg.isKeySignature()) {
                    mtrk.at(0).push_back(msg);
                } else {
                    mtrk.at(1).push_back(msg);
                }
            }
        }
        if (ensureOrder) {
            std::sort(mtrk.begin() + 2, mtrk.end(), [](const auto& lhs, const auto& rhs) {
                return lhs[0].channel() < rhs[0].channel();
            });
        }
        return mtrk;
    }

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
        // assert(std::is_sorted(trk.cbegin(), trk.cend()));
        toRelTimeTrack(trk);
    }
}
