#include "libmfmidi/kdmapidevice.hpp"
#include "libmfmidi/midimessagefdc.hpp"
#include "libmfmidi/midiprocessor.hpp"
#include "libmfmidi/miditrackplayer.hpp"
#include "libmfmidi/rtmididevice.hpp"
#include "libmfmidi/samhandlers.hpp"
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/win32mmtimer.hpp"

#include <fstream>
#include <iostream>
#include <exception>
#include <filesystem>
#include <ranges>
#include <spanstream>

#if defined(_POSIX_THREADS)
#include <sched.h>
#else
#include <processthreadsapi.h>
#endif

#include <timeapi.h>


using namespace libmfmidi;
using std::cin;
using std::cout;
using std::endl;

int main(int argc, char** argv)
{
    std::set_terminate([]() {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception& e) {
            std::cerr << "[FATAL] " << e.what() << endl;
        }
    });

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
    SMFReader         rd(&hsam, &stm);
    ;

    cout << "Parsing SMF" << endl;
    rd.parse();
    cout << "Parsed" << endl;
    cout << "SMF File: Format " << info.type << "; Division: " << static_cast<uint16_t>(info.division) << ";" << endl;
    cout << "NTrks: " << file.size() << ';' << endl;

    cout << "Merging" << endl;
    MIDITrack trk;
    mergeMultiTrack(std::move(file), trk);
    cout << "Merged" << endl;

    platform::RtMidiMIDIDeviceProvider& prov = platform::RtMidiMIDIDeviceProvider::instance();
    cout << "Dev cnt: " << prov.outputCount() << endl;
    for (unsigned int i = 0; i < prov.outputCount(); ++i) {
        cout << prov.outputName(i) << endl;
    }
    cout << "Choose, " << prov.outputCount() + 1 << " to KDMAPI: ";
    unsigned inp;
    std::cin >> inp;

    AbstractMIDIDevice*                        dev;
    std::unique_ptr<platform::KDMAPIDevice>    kdev;
    std::shared_ptr<platform::LibreMidiOutDevice> sdev;
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

    MIDITrackPlayer player; // init player after everything

    bool useCache;
    cout << "Use cache? 1/0: ";
    cin >> useCache;
    player.setUseCache(useCache);

    auto compfdc = [&](MIDITimedMessage& msg) {
        fdc::MFMarkTempo::process(msg);
        return MIDIMessageF2D::process(msg);
    };
    player.setMsgProcessor(compfdc);
    player.setDivision(info.division);
    player.setDriver(dev);
    player.setTrack(trk);

    player.setNotifier([&](NotifyType type) {
        if (type == NotifyType::T_Mode) {
            sendAllSoundsOff(dev);
        }
    });

    // manual init player thread to set priority
    player.initThread();
#if defined(_POSIX_THREADS)
    sched_param para{};
    para.sched_priority = (sched_get_priority_max(SCHED_FIFO) + sched_get_priority_min(SCHED_FIFO)) / 2;
    pthread_setschedparam(player.nativeHandle(), SCHED_FIFO, &para);
#else
    SetThreadPriority(player.nativeHandle(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    std::vector<std::string> splitedcmd;
    std::getchar();
    while (true) {
        cout << "> ";
        std::string cmd;
        std::getline(cin, cmd);
        splitedcmd.clear();
        for (const auto& i : cmd | std::ranges::views::split(std::string_view(" "))) {
            splitedcmd.emplace_back(i.begin(), i.end());
        }

        if (splitedcmd.empty()) {

        } else if (splitedcmd[0] == "play") {
            player.play();
        } else if (splitedcmd[0] == "pause") {
            player.pause();
        } else if (splitedcmd[0] == "seek") {
            if (splitedcmd.size() < 2) {
                cout << "Current tick time: " << player.tickTime() << endl;
                continue;
            }
            const MIDIClockTime clk = std::stoul(splitedcmd[1]);
            cout << "Seeking to " << clk << endl;
            sendAllSoundsOff(dev);
            player.goTo(clk);
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
