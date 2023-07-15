#include <iostream>
#include <fstream>
#include "libmfmidi/midireadonplay.hpp"
#include "libmfmidi/midiadvancedtrackplayer.hpp"
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/samhandlers.hpp"
#include "libmfmidi/rtmididevice.hpp"
#include "libmfmidi/kdmapidevice.hpp"
#include "libmfmidi/midimessagefdc.hpp"
#if defined(_POSIX_THREADS)
#include <sched.h>
#else
#include <processthreadsapi.h>
#endif
#include <timeapi.h>
#include <ranges>
#include <filesystem>
#include <exception>
#include <fmt/chrono.h>
#include <version>
#include <source_location>

#include <fileapi.h>

using namespace libmfmidi;
using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using namespace std::literals;

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

    cerr << "Win32 API Error " << dw << ": \"" << lpMsgBuf << "\" on " << location.file_name() << ':' << location.line() << ':' << location.column() << ' ' << location.function_name() << endl;
    LocalFree(lpMsgBuf);
    exit(-1);
}

int main(int argc, char** argv)
{
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    cout << "ROP: Example of libmfmidi" << endl;
    if (argc == 1) {
        std::cerr << "Error: No input file" << std::endl;
        return -1;
    }

    cout << "Opening file " << argv[1] << endl;

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

    void* mmap_data = MapViewOfFile(mmap, FILE_MAP_READ, 0, 0, 0);
    if (mmap_data == nullptr) {
        reportError();
    }

    MEMORY_BASIC_INFORMATION mmap_info{};
    if (VirtualQuery(mmap_data, &mmap_info, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
        reportError();
    }

    size_t mmap_len = filesize;

    cout << "Opened, mapped " << mmap_len << " bytes, file " << filesize << " bytes\n";

    MIDIReadOnPlayTrack trk{
        MIDIReadOnPlayTrack::base_type{reinterpret_cast<const uint8_t*>(mmap_data), mmap_len}
    };

    auto& prov = platform::RtMidiMIDIDeviceProvider::instance();
    cout << "Dev cnt: " << prov.outputCount() << endl;
    for (unsigned int i = 0; i < prov.outputCount(); ++i) {
        cout << prov.outputName(i) << endl;
    }
    cout << "Choose, " << prov.outputCount() + 1 << " to KDMAPI: ";
    unsigned inp;
    std::cin >> inp;

    AbstractMIDIDevice*                        dev;
    std::unique_ptr<platform::KDMAPIDevice>    kdev;
    std::unique_ptr<platform::RtMidiOutDevice> sdev;
    if (inp == prov.outputCount() + 1) {
        kdev = std::make_unique<platform::KDMAPIDevice>(true);
        dev  = kdev.get();
    } else {
        sdev = platform::RtMidiMIDIDeviceProvider::buildupOutputDevice(inp);
        dev  = sdev.get();
    }

    if (!dev->open()) {
        std::cerr << "Failed to open device" << endl;
    }

    MIDIAdvancedTrackPlayer<MIDIReadOnPlayTrack> player; // init player after everything
    player.setData(&trk); // before cursors
    auto cursor = player.addCursor(dev, 0ns);

    bool useCache;
    cout << "Use cache? 1/0: ";
    cin >> useCache;
    player.setUseCache(useCache);

    auto compfdc = [&](MIDITimedMessage& msg) {
        return MIDIMessageF2D::process(msg);
    };
    player.setCursorProcessor(cursor, compfdc);

    cout << "Division: ";
    uint16_t div;
    cin >> div;
    player.setDivision(MIDIDivision{div});

    player.addCursorNotifier(cursor, [&](NotifyType type) {
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
            if (!player.isCursorActive(cursor)) {
                player.activeCursor(cursor);
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
            player.goTo(target);
        } else if (splitedcmd[0] == "exit") {
            break;
        } else if (splitedcmd[0] == "status") {
            cout << "Is playing: " << player.isPlaying() << endl;
        } else {
            cout << "Unknown Command: " << cmd << endl;
        }
    }

    UnmapViewOfFile(mmap_data);
    CloseHandle(mmap);
    CloseHandle(file);
    return 0;
}
