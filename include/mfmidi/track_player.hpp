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

#pragma once

#include "mfmidi/event.hpp"
#include "mfmidi/midi_device.hpp"
#include "mfmidi/midi_message.hpp"
#include "mfmidi/midi_tempo.hpp"
#include "mfmidi/smf/smf.hpp"
#include "mfmidi/timingapi.hpp"
#include <atomic>
#include <cassert>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>

namespace mfmidi {
    using namespace std::literals;

    namespace details {
        // template <class Track, class Time, playhead_status Status>
        // struct basic_snapshot {
        //     Time sleeptime{}; // time to next event
        //     // playtime is key // current time
        //     std::ranges::iterator_t<const Track> nextevent{}; // next event, if sleeptime==0, this is current event
        //     Status                               status{};    // status of played events
        // };
        //
        // template <class Track, class Time, class R, playhead_status Status, class StatusProc>
        // inline Time buildPlaybackCache(const Track& trk, R& caches, Time cache_interval, MIDIDivision division, bool tempo_change_aware = true, MIDITempo default_tempo = 120_bpm)
        // {
        //     using Snapshot = basic_snapshot<Track, Time, Status>;
        //
        //     if (std::ranges::empty(trk)) {
        //         return {};
        //     }
        //
        //     Time   playtime{};
        //     auto   nextevent = trk.begin();
        //     Status status;
        //     status.tempo = default_tempo;
        //     StatusProc status_proc(status);
        //     Time       divns         = divisionToDuration(division, status.tempo);
        //     Time       nextCacheTime = cache_interval;
        //     Snapshot   snap{};
        //
        //     while (true) {
        //         while (playtime + (*nextevent).deltaTime() * divns < nextCacheTime) { // use < because ignore the event on nextCacheTime (the event will play when tick, but have not played yet when cache)
        //             playtime += (*nextevent).deltaTime() * divns;
        //             status_proc.process(*nextevent);
        //             if (tempo_change_aware && midi_message_owning_view{*nextevent}.isTempo()) {
        //                 divns = divisionToDuration(division, status.tempo);
        //             }
        //             ++nextevent;
        //             if (nextevent == trk.end()) {
        //                 return playtime; // see directGoTo
        //             }
        //         }
        //         snap.nextevent        = nextevent;
        //         snap.sleeptime        = playtime + nextevent->deltaTime() * divns - nextCacheTime; // see directGoTo
        //         snap.status           = status;
        //         caches[nextCacheTime] = std::move(snap);
        //         nextCacheTime += cache_interval;
        //     }
        // }

        struct emulated_message_t {};
        struct realtime_message_t {};
        struct emulated_rewind_message_t {};
        inline constexpr auto emulated_message        = emulated_message_t{};
        inline constexpr auto realtime_message        = realtime_message_t{};
        inline constexpr auto emulated_rewind_message = emulated_rewind_message_t{};

        /// \brief Internal class for holding status and play status
        ///
        template <std::ranges::forward_range Track, class Handler = void>
            requires std::is_void_v<Handler> || (std::invocable<Handler, emulated_message_t, std::ranges::range_value_t<Track>> && std::invocable<Handler, realtime_message_t, std::ranges::range_value_t<Track>>)
        class track_playhead {
        public:
            using Time                                = std::chrono::nanoseconds; // must be signed
            static constexpr bool tempo_changed_aware = !std::is_void_v<Handler> && event_emitter_of<Handler, events::tempo_changed>;

        private:
            static constexpr bool have_handler = !std::is_void_v<Handler>;
            // Information
            std::string _name;

            // Playing status
            Time  _divns{};     // 1 delta-time in nanoseconds
            Time  _sleeptime{}; // sleep period before mnextevent ticks
            Time  _playtime{};  // current time, support 5850 centuries long
            Time  _compensation{};
            tempo _tempo = tempo::from_bpm(120);

            // Data
            midi_device* _dev{};

            const Track*                         _track{};
            std::ranges::iterator_t<const Track> _nextmsg{};

            division _division{};

            std::reference_wrapper<Handler>                                                                                                     _handler{};
            std::conditional_t<tempo_changed_aware, std::optional<decltype(std::declval<Handler>().add_event_handler([](auto&&...) {}))>, char> _handler_event_token;

        public:
            explicit track_playhead(std::string_view name, Handler& handler) noexcept
                : _name(name)
                , _handler(handler)
            {
                if constexpr (tempo_changed_aware) {
                    register_handler(*this);
                }
            }

            ~track_playhead() noexcept
            {
                if constexpr (tempo_changed_aware) {
                    unregister_handler(*this);
                }
            }

            track_playhead(const track_playhead&)            = delete;
            track_playhead& operator=(const track_playhead&) = delete;

            track_playhead(track_playhead&&) noexcept            = delete; // todo: implement todo
            track_playhead& operator=(track_playhead&&) noexcept = delete;
            // re-register should be done for moving

            void retiming() noexcept
            {
                auto newdivns = division_to_duration(_division, _tempo);
                if ((_sleeptime.count() != 0) || (_divns.count() != 0)) {
                    _sleeptime = Time{_sleeptime.count() * newdivns.count() / _divns.count()};
                }
                _divns = newdivns;
            }

            Time tick(Time slept /*the time that slept*/)
            {
            TICK_BEGIN:
                if (eof()) {
                    return Time::max();
                }
                _playtime += slept; // not move it to playthread because of lastSleptTime = 0 in revertSnapshot

                auto&& msg = *_nextmsg;
                // if (_sleeptime == 0ns) {
                //     _sleeptime = event.deltaTime() * _divns; // usually first tick
                // }
                if (slept > _sleeptime) {
                    _compensation += slept - _sleeptime;
                    slept = _sleeptime;
                }
                _sleeptime -= slept;
                if (_sleeptime != 0ns) {
                    return _sleeptime;
                }
                if constexpr (have_handler) {
                    _handler(realtime_message, msg);
                }
                if (_dev != nullptr) {
                    _dev->send_msg(msg);
                }
                ++_nextmsg;
                if (eof()) {
                    return Time::max();
                }
                _sleeptime = (*_nextmsg).deltaTime() * _divns;
                if (_sleeptime <= _compensation) {
                    _compensation -= _sleeptime;
                    slept = _sleeptime;
                    goto TICK_BEGIN;
                }
                return _sleeptime - std::exchange(_compensation, 0ns);
            }

            bool seek(Time target)
            {
                if (_playtime == target) {
                    return true;
                }
                if (target > _playtime) {
                    return go_forward(target);
                }
                if constexpr (std::ranges::random_access_range<Track> && !have_handler) {
                    go_rewind(target);
                    return true;
                }
                reset_playhead_to_begin();
                return go_forward(target);
            }

            bool go_forward(Time target)
            {
                assert(_playtime <= target);
                if (eof()) {
                    return false;
                }
                while (true) {
                    auto&& msg = *_nextmsg;
                    if (_playtime + _sleeptime >= target) { // time point before it happen
                        _sleeptime = _playtime + _sleeptime - target;
                        _playtime  = target;
                        return true;
                    }
                    _playtime += _sleeptime;
                    if constexpr (have_handler) {
                        _handler(emulated_message, msg);
                    }
                    ++_nextmsg;
                    if (eof()) {
                        _sleeptime = 0ns; // optional
                        return false;
                    }
                    _sleeptime = (*_nextmsg).deltaTime() * _divns;
                }
            }

            // actually negative playtime should be okay
            void go_rewind(Time target)
                // requires std::ranges::bidirectional_range<Track>
                requires(!have_handler) // i have no idea about how to implement that with a handler
            {
                assert(_playtime >= target);
                const auto& begin = std::ranges::cbegin(_track);
                if (eof()) {
                    // rewind to the time point before the last event happens
                    --_nextmsg;
                    _sleeptime = 0ns;
                }
                while (true) {
                    auto&& event = _nextmsg;
                    auto   last  = _playtime - (event.deltaTime() - _sleeptime);
                    if (last <= target) {
                        _sleeptime = _playtime + _sleeptime - target;
                        _playtime  = target;
                        return;
                    }
                    _playtime  = last;
                    _sleeptime = 0ns;
                    if (_nextmsg == begin) {
                        assert(target <= 0ns);
                        _sleeptime = -target;
                        _playtime  = target;
                        return;
                    }
                    --_nextmsg;
                }
            }

            [[nodiscard]] division     division() const { return _division; }
            [[nodiscard]] tempo        tempo() const { return _tempo; }
            [[nodiscard]] midi_device* device() const { return _dev; }
            [[nodiscard]] const Track* track() const { return _track; }
            [[nodiscard]] bool         eof() const { return _nextmsg == _track->end(); }
            [[nodiscard]] Handler&     handler() const { return _handler.get(); }
            [[nodiscard]] Time         playtime() const { return _playtime; }

            void set_division(mfmidi::division div)
            {
                _division = div;
                retiming();
            }

            void set_tempo(mfmidi::tempo tempo)
            {
                _tempo = tempo;
                retiming();
            }

            void set_device(midi_device* dev)
            {
                _dev = dev;
            }

            void set_track(const Track* track)
            {
                _track = track;
                reset_playhead_to_begin();
            }

            void set_handler(Handler& handler)
            {
                _handler = handler;
            }

        protected:
            void reset_playhead_to_begin()
            {
                using namespace literals;

                assert(_track != nullptr);
                _nextmsg   = std::ranges::begin(*_track);
                _sleeptime = (*_nextmsg).deltaTime() * _divns;
                _playtime  = 0ns;
                _tempo     = 120_bpm;
                retiming();
            }

            friend void unregister_handler(track_playhead& self)
                requires track_playhead::tempo_changed_aware
            {
                if (self._handler_event_token) {
                    self._handler.get().remove_event_handler(self._handler_event_token.value());
                }
            }

            friend void register_handler(track_playhead& self)
                requires track_playhead::tempo_changed_aware
            {
                self._handler_event_token = self._handler.get().add_event_handler([&self](const events::tempo_changed& event) {
                    self.set_tempo(event.tempo);
                });
            }
        };

        // template <std::ranges::forward_range Container, class Time>

        template <class Track, class Handler>
            requires requires {
                typename track_playhead<Track, Handler>;
            }
        class track_playhead_group {
        public:
            using Playhead = track_playhead<Track, Handler>;
            using Time     = typename Playhead::Time;

            static constexpr Time MAX_SLEEP = 500ms;

            struct playhead_info {
                std::unique_ptr<Playhead> playhead;
                Time                      offest;
            };

            using PlayheadRemovalHandler = std::function<void(playhead_info&&)>;

        private:
            // RAII class for auto pause and play
            class Pauser {
            public:
                explicit Pauser(track_playhead_group& seq)
                    : player(seq)
                    , toplay(seq.pause())
                {}

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
                track_playhead_group& player;
                bool                  toplay;
            };

            // Playback
            std::vector<playhead_info> _playheads;
            PlayheadRemovalHandler     _rhandler;
            // std::vector<std::chrono::nanoseconds> msleeptimecache; // use in playThread, cache sleep time of cursors
            Time _lastSleptTime{}; // if you changed playback data, set this to 0 and it will be recalcuated
            Time _compensation{};

            // Thread
            std::jthread            _thread;
            bool                    _wakeup = false;
            std::mutex              _mutex;
            std::condition_variable _condvar;
            std::atomic_flag        _play = false; // since C++20

        public:
            track_playhead_group() noexcept = default;

            ~track_playhead_group() noexcept
            {
                _thread.request_stop();
                {
                    std::lock_guard<std::mutex> guard{_mutex};
                    _wakeup = true;
                    _condvar.notify_all();
                }
            }

            track_playhead_group(const track_playhead_group&)            = delete;
            track_playhead_group& operator=(const track_playhead_group&) = delete;
            track_playhead_group(track_playhead_group&&)                 = delete;
            track_playhead_group& operator=(track_playhead_group&&)      = delete;

            [[nodiscard]] bool playing() const noexcept
            {
                return _play.test();
            }
            [[nodiscard]] bool empty() const { return _playheads.empty(); }

            [[nodiscard]] auto thread_id() const { return _thread.get_id(); }
            [[nodiscard]] auto thread_native_handle() { return _thread.native_handle(); }
            [[nodiscard]] bool joinable() const { return _thread.joinable(); }

            [[nodiscard]] PlayheadRemovalHandler playhead_removal_handler() const { return _rhandler; }

            void set_playhead_removal_handler(PlayheadRemovalHandler handler)
            {
                _rhandler = std::move(handler);
            }

            void set_division(division division)
            {
                for (auto& info : _playheads) {
                    info.playhead->set_division(division);
                    info.playhead->retiming();
                }
            }

            void set_handler(Handler& handler)
            {
                for (auto& info : _playheads) {
                    info.playhead->set_handler(handler);
                }
            }

            [[nodiscard]] Time base_time() const
            {
                if (_playheads.empty()) {
                    throw std::out_of_range{"No playheads in group"};
                }
                const playhead_info& info = _playheads.front();
                return info.playhead->playtime() - info.offest;
            }

            bool play()
            {
                if (_playheads.empty()) {
                    return false;
                }
                if (!_thread.joinable()) {
                    init_thread();
                }
                _play.test_and_set();
                {
                    std::lock_guard<std::mutex> guard{_mutex};
                    _wakeup = true;
                    _condvar.notify_all();
                }
                return true;
            }

            /// \return Return if the player is playing before pause
            bool pause()
            {
                bool play = _play.test();
                _play.clear();
                return play;
            }

            void set_track(const Track* data)
            {
                Pauser pauser{*this};
                _lastSleptTime = 0ns;
                for (auto& info : _playheads) {
                    info.playhead->set_track(data);
                }
            }

            Playhead* add_playhead(std::unique_ptr<Playhead>&& playhead, Time setoffest = {})
            {
                auto result = playhead.get();
                _playheads.emplace_back(std::move(playhead), setoffest);
                return result;
            }

            [[nodiscard]] auto playheads() noexcept
            {
                return std::ranges::ref_view<decltype(_playheads)>{_playheads};
            }

            void set_device(midi_device* device)
            {
                Pauser pauser{*this};
                for (auto& cinfo : _playheads) {
                    cinfo.playhead->set_device(device);
                }
            }

            void seek_throw(Time targetTime)
            {
                Pauser pauser{*this};
                _lastSleptTime = {};
                for (auto& info : _playheads) {
                    if (!info.playhead->seek(targetTime + info.offest)) {
                        throw std::out_of_range("targetTime out of range");
                    }
                }
            }

            void seek(Time targetTime)
            {
                Pauser pauser{*this};
                _lastSleptTime = {};
                for (auto& info : _playheads) {
                    info.playhead->seek(targetTime + info.offest);
                }
            }

            void init_thread()
            {
                if (!_thread.joinable()) {
                    _thread = std::jthread([this](const std::stop_token& stop) {
                        playThread(stop);
                    });
                }
            }

        private:
            void playThread(const std::stop_token& token)
            {
                while (!token.stop_requested()) {
                    if (!_play.test()) {
                        for ([[maybe_unused]] auto& info : _playheads) {
                            // info.playhead->notify(NotifyType::T_Mode);
                            // todo: emit something
                        }
                        {
                            std::unique_lock<std::mutex> lck(_mutex);
                            _condvar.wait(lck, [this] { return _wakeup; });
                            _wakeup = false;
                        }
                        if (_play.test()) {
                        }
                    } else {
                        auto begin = hiresticktime();
                        Time sleep = 0ns;
                        if (_compensation < _lastSleptTime) {
                            sleep = _lastSleptTime - _compensation;
                            nanosleep(sleep); // because _lastSleptTime may be changed (such as when revert snapshot)
                            _compensation = 0ns;
                        } else {
                            _compensation -= _lastSleptTime;
                        }
                        std::chrono::nanoseconds minTime = std::chrono::nanoseconds::max();

                    STARTLOOP_13:

                        auto end = _playheads.end();
                        for (auto it = _playheads.begin(); it != end; ++it) {
                            Time interval = (*it).playhead->tick(_lastSleptTime);
                            if (interval == Time::max()) {
                                if (it == _playheads.begin()) {
                                    if (_rhandler) {
                                        _rhandler(std::move(*it));
                                    }
                                    _playheads.erase(it);
                                    goto STARTLOOP_13;
                                }
                                auto oit = it;
                                --it;
                                if (_rhandler) {
                                    _rhandler(std::move(*oit));
                                }
                                _playheads.erase(oit);
                                end = _playheads.end();
                                continue;
                            }
                            minTime = std::min(minTime, interval);
                        }
                        if (minTime == Time::max()) {
                            _play.clear();
                            continue;
                        }
                        minTime          = std::min(minTime, MAX_SLEEP);
                        _lastSleptTime   = minTime;
                        auto now         = hiresticktime();
                        auto elapsedtime = now - begin - sleep;
                        _compensation += elapsedtime;
                    }
                }
            }
        };

    }

    using details::track_playhead;       // NOLINT(misc-unused-using-decls)
    using details::track_playhead_group; // NOLINT(misc-unused-using-decls)
}
