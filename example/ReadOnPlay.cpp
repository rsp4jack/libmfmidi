/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#if defined(_POSIX_THREADS)
#include <sched.h>
#elif defined(_WIN32)
#include <processthreadsapi.h>
#else
#error No threads
#endif
#include <exception>
#include <filesystem>
#include <ranges>
#include <source_location>
#include <version>

#if defined(_WIN32)
#include <debugapi.h>
#include <fileapi.h>
#include <timeapi.h>
#include <Windows.h>
#elif defined(_POSIX_VERSION)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "mfmidi/mfmidi.hpp"

using namespace mfmidi;
using std::cin;
using namespace std::literals;

#if defined(_WIN32)
void reportError(const std::source_location location = std::source_location::current())
{
    LPVOID lpMsgBuf = nullptr;
    DWORD  dw       = GetLastError();

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf,
        0, nullptr
    );

    std::println(stderr, "Win32 API Error {}: \"{}\" on {}:{}:{} {}", dw, (LPSTR)lpMsgBuf, location.file_name(), location.line(), location.column(), location.function_name());
    LocalFree(lpMsgBuf);
    exit(-1);
}
#endif

struct Helper : event_emitter_util<events::tempo_changed> {
    void operator()(auto&&, const midi_message_span& msg)
    {
        if (msg.isTempo()) {
            emit(events::tempo_changed{msg.tempo()});
        }
    }
};

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#elif defined(_POSIX_VERSION)
    nice(-11);
#endif

    std::println("ROP: Example of mfmidi");
    if (argc == 1) {
        std::println(stderr, "Error: No input file");
        return -1;
    }

    std::println("Opening file {}", argv[1]);

    const uint8_t* mmap_data{};
    size_t         mmap_len{};

#if defined(_WIN32)
    {
        HANDLE file = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == nullptr) {
            reportError();
        }

        size_t filesize{};
        if (GetFileSizeEx(file, reinterpret_cast<LARGE_INTEGER*>(&filesize)) == 0) {
            reportError();
        }

        HANDLE mmap = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mmap == nullptr) {
            reportError();
        }

        void* mmapp = MapViewOfFile(mmap, FILE_MAP_READ, 0, 0, 0);
        if (mmapp == nullptr) {
            reportError();
        }

        MEMORY_BASIC_INFORMATION mmap_info{};
        if (VirtualQuery(mmapp, &mmap_info, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
            reportError();
        }

        mmap_data = reinterpret_cast<const uint8_t*>(mmapp);
        mmap_len  = filesize;
    }
#elif defined(_POSIX_VERSION)
    {
        namespace fs = std::filesystem;
        auto fd      = open(argv[1], O_RDONLY);
        if (fd < 0) {
            throw fs::filesystem_error("open()", std::error_code{errno, std::system_category()});
        }

        struct stat statbuf {};

        if (fstat(fd, &statbuf) != 0) {
            close(fd);
            throw fs::filesystem_error("fstat()", std::error_code{errno, std::system_category()});
        }
        if (statbuf.st_size == 0) {
            std::println("Empty file!");
            close(fd);
            return 0;
        }
        auto ptr = static_cast<const uint8_t*>(mmap(nullptr, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (ptr == MAP_FAILED) {
            throw fs::filesystem_error("mmap()", std::error_code{errno, std::system_category()});
        }

        mmap_data = ptr;
        mmap_len  = statbuf.st_size;
    }

#endif

    std::println("Opened, file {} bytes", mmap_len);

    span_track::base_type mmap_span{mmap_data, mmap_len};
    auto                  rop = parse_smf_header(mmap_span);

    std::println("Parsed as SMF Type {} with {} tracks in division {}", rop.info.type, rop.info.ntrk, static_cast<int16_t>(static_cast<uint16_t>(rop.info.division)));

    libremidi::observer obs;
    auto                outputs = obs.get_output_ports();
    std::println("Dev cnt: {}", outputs.size());
    for (const auto& [id, dev] : std::views::enumerate(outputs)) {
        std::println("{}: {}", id, dev.display_name);
    }
    std::print("Choose, {} to KDMAPI: ", outputs.size() + 1);
    unsigned inp{};
    std::cin >> inp;

    midi_device*                          dev;
    std::unique_ptr<libremidi_out_device> sdev;
#if defined(_WIN32)
    std::unique_ptr<kdmapi_device> kdev;
    if (inp == outputs.size() + 1) {
        kdev = std::make_unique<kdmapi_device>(true);
        dev  = kdev.get();
    } else
#endif
    {
        sdev = std::make_unique<libremidi_out_device>(outputs.at(inp));
        dev  = sdev.get();
    }

    if (!dev->open()) {
        std::println(stderr, "Failed to open device");
        return 1;
    }

    auto                                     helper = std::make_unique<Helper>();
    track_playhead_group<span_track, Helper> player; // init player after everything
    using Playhead = decltype(player)::Playhead;

    std::mutex                             mutex;
    std::vector<std::unique_ptr<Playhead>> removed_playheads;
    player.set_playhead_removal_handler([&](auto&& head) {
        std::lock_guard guard{mutex};
        removed_playheads.push_back(std::move(head.playhead)); // or std::move(head).playhead
    });

    for (const auto& [idx, trk] : std::views::zip(std::views::iota(0U), rop.tracks)) {
        auto* playhead = player.add_playhead(std::make_unique<Playhead>(std::string_view{std::format("Playback_{}", idx)}, *helper.get()));
        playhead->set_device(dev);
        // TODO: forgive me
        playhead->set_track(new span_track(trk));
    }

    player.set_division(rop.info.division);

    // manual init player thread to set priority
    player.init_thread();
#if defined(_POSIX_THREADS)
    sched_param para{};
    para.sched_priority = (sched_get_priority_max(SCHED_FIFO) + sched_get_priority_min(SCHED_FIFO)) / 2;
    pthread_setschedparam(player.thread_native_handle(), SCHED_FIFO, &para);
#else
    SetThreadPriority(player.thread_native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    std::vector<std::string> splitedcmd;
    std::getchar();
    while (true) {
        std::print("> ");
        std::string cmd;
        std::getline(cin, cmd);
        splitedcmd.clear();
        for (const auto& i : std::ranges::views::split(cmd, std::string_view(" "))) {
            splitedcmd.emplace_back(i.begin(), i.end());
        }

        if (splitedcmd.empty()) {
        } else if (splitedcmd[0] == "play") {
            if (player.empty()) {
                std::println("EOF");
            } else {
                sendAllSoundsOff(dev);
                player.play();
            }
        } else if (splitedcmd[0] == "pause") {
            player.pause();
            sendAllSoundsOff(dev);
        } else if (splitedcmd[0] == "seek") {
            if (splitedcmd.size() < 2) {
                decltype(player)::Time pos;
                if (player.empty()) {
                    auto transformed = player.playheads() | std::views::transform([](auto&& info) {
                                           return info.playhead->playtime();
                                       });
                    pos              = *std::ranges::max_element(transformed);
                } else {
                    pos = player.base_time();
                }
                auto hms = std::chrono::hh_mm_ss{pos};
                std::println("Current time: {}:{}:{}", hms.hours(), hms.minutes(), hms.seconds());
                continue;
            }
            sendAllSoundsOff(dev);
            std::chrono::nanoseconds target{std::chrono::seconds{std::stoll(splitedcmd[1])}};
            std::println("Seeking to {}", target);
            {
                std::lock_guard guard{mutex};
                for (auto& head : removed_playheads) {
                    player.add_playhead(std::move(head));
                }
                removed_playheads.clear();
            }
            player.seek(target);
        } else if (splitedcmd[0] == "exit") {
            break;
        } else if (splitedcmd[0] == "status") {
            std::println("Is playing: {}", player.playing());
        } else {
            std::println("Unknown Command: {}", cmd);
        }
    }
    return 0;
}
