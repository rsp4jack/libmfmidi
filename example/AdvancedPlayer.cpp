#include <iostream>
#include <fstream>
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
#include <spanstream>
#include <exception>
#include <fmt/chrono.h>

using namespace libmfmidi;
using std::cin;
using std::cout;
using std::endl;
using namespace std::literals;

int main(int argc, char** argv)
{
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    cout << "TrackPlayer: Example of libmfmidi" << endl;
    if (argc == 1) {
        std::cerr << "Error: No input file" << std::endl;
        return -1;
    }

    // spanstream for speed
    cout << "Opening file " << argv[1] << endl;
    std::fstream stm;
    stm.open(argv[1], std::ios::in | std::ios::binary);
    cout << "Opened" << endl;

    MIDIMultiTrack    file;
    SMFFileInfo       info;
    SMFFileSAMHandler hsam(&file, &info);
    SMFReader         rd(&stm, 0, &hsam);

    cout << "Parsing SMF" << endl;
    rd.parse();
    cout << "Parsed" << endl;
    cout << "SMF File: Format " << info.type << "; Division: " << static_cast<uint16_t>(info.division) << ";" << endl;
    cout << "NTrks: " << file.size() << ';' << endl;

    cout << "Merging" << endl;
    MIDITrack trk;
    mergeMultiTrack(std::move(file), trk);
    cout << "Merged" << endl;

    auto& prov = platform::RtMidiMIDIDeviceProvider::instance();
    cout << "Dev cnt: " << prov.outputCount() << endl;
    for (unsigned int i = 0; i < prov.outputCount(); ++i) {
        cout << prov.outputName(i) << endl;
    }
    cout << "Choose, " << prov.outputCount() + 1 << " to KDMAPI: ";
    unsigned inp;
    std::cin >> inp;

    AbstractMIDIDevice*                                  dev;
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

    MIDIAdvancedTrackPlayer<MIDITrack> player; // init player after everything
    player.setData(&trk); // before cursors
    auto                               cursor = player.addCursor(dev, 0ns);

    bool useCache;
    cout << "Use cache? 1/0: ";
    cin >> useCache;
    player.setUseCache(useCache);

    auto compfdc = [&](MIDITimedMessage& msg) {
        return MIDIMessageF2D::process(msg);
    };
    player.setCursorProcessor(cursor, compfdc);
    player.setDivision(info.division);

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
            std::chrono::nanoseconds target{std::stoll(splitedcmd[1])};
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
