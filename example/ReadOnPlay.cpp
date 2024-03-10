#include "libmfmidi/kdmapidevice.hpp"
#include "libmfmidi/midiadvancedtrackplayer.hpp"
#include "libmfmidi/midimessagefdc.hpp"
#include "libmfmidi/midireadonplay.hpp"
#include "libmfmidi/rtmididevice.hpp"
#include "libmfmidi/samhandlers.hpp"
#include "libmfmidi/smfreader.hpp"
#include <fstream>
#include <iostream>
#if defined(_POSIX_THREADS)
#include <sched.h>
#else
#include <processthreadsapi.h>
#endif
#include <exception>
#include <filesystem>
#include <fmt/chrono.h>
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace libmfmidi;
using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using namespace std::literals;

#if defined(_WIN32)
void reportError(const std::source_location location = std::source_location::current())
{
    LPVOID lpMsgBuf = nullptr;
    DWORD  dw       = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, nullptr
    );

    cerr << "Win32 API Error " << dw << ": \"" << (LPTSTR)lpMsgBuf << "\" on " << location.file_name() << ':' << location.line() << ':' << location.column() << ' ' << location.function_name() << endl;
    LocalFree(lpMsgBuf);
    exit(-1);
}
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#elif defined(_POSIX_VERSION)
    nice(-11);
#endif

    cout << "ROP: Example of libmfmidi" << endl;
    if (argc == 1) {
        std::cerr << "Error: No input file" << std::endl;
        return -1;
    }

    fmt::println("Opening file {}", argv[1]);

    const uint8_t* mmap_data;
    size_t         mmap_len;

#if defined(_WIN32)
    {
        HANDLE file = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == nullptr) {
            reportError();
        }

        size_t filesize;
        if (GetFileSizeEx(file, reinterpret_cast<LARGE_INTEGER*>(&filesize)) == 0) {
            reportError();
        }

        HANDLE mmap = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mmap == nullptr) {
            reportError();
        }

        void* mmapp = MapViewOfFile(mmap, FILE_MAP_READ, 0, 0, 0);
        if (mmap_data == nullptr) {
            reportError();
        }

        MEMORY_BASIC_INFORMATION mmap_info{};
        if (VirtualQuery(mmapp, &mmap_info, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
            reportError();
        }

        mmap_data = reinterpret_cast<const uint8_t*>{mmapp};
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
            fmt::println("Empty file!");
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

    fmt::println("Opened, file {} bytes", mmap_len);

    MIDIReadOnPlayTrack::base_type mmap_span{mmap_data, mmap_len};
    auto                           rop = parseSMFReadOnPlay(mmap_span);

    fmt::println("Parsed as SMF Type {} with {} tracks in division {}", rop.info.type, rop.info.ntrk, static_cast<int16_t>(static_cast<uint16_t>(rop.info.division)));

    auto& prov = platform::RtMidiMIDIDeviceProvider::instance();
    fmt::println("Dev cnt: {}", prov.outputCount());
    for (unsigned int i = 0; i < prov.outputCount(); ++i) {
        cout << prov.outputName(i) << endl;
    }
    fmt::print("Choose, {} to KDMAPI: ", prov.outputCount() + 1);
    unsigned inp;
    std::cin >> inp;

    AbstractMIDIDevice*                        dev;
    std::unique_ptr<platform::RtMidiOutDevice> sdev;
#if defined(_WIN32)
    std::unique_ptr<platform::KDMAPIDevice> kdev;
    if (inp == prov.outputCount() + 1) {
        kdev = std::make_unique<platform::KDMAPIDevice>(true);
        dev  = kdev.get();
    } else
#endif
    {
        sdev = platform::RtMidiMIDIDeviceProvider::buildupOutputDevice(inp);
        dev  = sdev.get();
    }

    if (!dev->open()) {
        std::cerr << "Failed to open device" << endl;
    }

    bool useCache;
    cout << "Use cache? 1/0: ";
    cin >> useCache;

    advtrkplayer::MIDIAdvTrkCache<std::chrono::nanoseconds, MIDIReadOnPlayTrack> cache;
    MIDIStatus                                                                   status;
    MIDIAdvancedTrackPlayer<MIDIReadOnPlayTrack>                                 player; // init player after everything

    auto msgproc = [](MIDIMessage& msg) {
        if (msg.isNoteOn() && (msg.velocity() < 3)) {
            return false;
        }
        return MIDIMessageF2D::process(msg);
    };

    for (const auto& [idx, trk] : std::views::zip(std::views::iota(0U), rop.tracks)) {
        auto* cursor = player.addCursor(fmt::format("Playback_{}", idx));
        cursor->setMIDIStatus(&status);
        cursor->setDevice(dev);
        // TODO: forgive me
        if (useCache) {
            cursor->setCache(new decltype(player)::Cache);
        }
        cursor->setTrack(new MIDIReadOnPlayTrack(trk.subspan(8)), useCache);
        cursor->setProcessor(msgproc);
    }

    player.setDivisionAll(rop.info.division);

    player.cursors().front().cursor->addNotifier([&](NotifyType type) {
        if (type == NotifyType::T_Mode) {
            sendAllSoundsOff(dev);
        }
    });

    // manual init player thread to set priority
    player.initThread();
#if defined(_POSIX_THREADS)
    sched_param para{};
    para.sched_priority = (sched_get_priority_max(SCHED_FIFO) + sched_get_priority_min(SCHED_FIFO)) / 2;
    pthread_setschedparam(player.threadNativeHandle(), SCHED_FIFO, &para);
#else
    SetThreadPriority(player.threadNativeHandle(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    std::vector<std::string> splitedcmd;
    std::getchar();
    while (true) {
        cout << "> ";
        std::string cmd;
        std::getline(cin, cmd);
        splitedcmd.clear();
        for (const auto& i : std::ranges::views::split(cmd, std::string_view(" "))) {
            splitedcmd.emplace_back(i.begin(), i.end());
        }

        if (splitedcmd.empty()) {

        } else if (splitedcmd[0] == "play") {
            if (player.eof()) {
                fmt::println("EOF");
            }
            for (const auto& cinfo : player.cursors()) {
                cinfo.cursor->setActive(true);
            }
            player.play();
        } else if (splitedcmd[0] == "pause") {
            player.pause();
        } else if (splitedcmd[0] == "seek") {
            if (splitedcmd.size() < 2) {
                auto hms = std::chrono::hh_mm_ss<decltype(player)::Time>(player.baseTime());
                fmt::println("Current time: {}:{}:{}", hms.hours(), hms.minutes(), hms.seconds());
                continue;
            }
            cout << "Seeking to " << splitedcmd[1] << endl;
            sendAllSoundsOff(dev);
            std::chrono::nanoseconds target{std::chrono::seconds{std::stoll(splitedcmd[1])}};
            for (const auto& cinfo : player.cursors()) {
                cinfo.cursor->setActive(true);
            }
            player.goTo(target);
        } else if (splitedcmd[0] == "exit") {
            break;
        } else if (splitedcmd[0] == "status") {
            cout << "Is playing: " << player.isPlaying() << endl;
        } else {
            cout << "Unknown Command: " << cmd << endl;
        }
    }
    return 0;
}
