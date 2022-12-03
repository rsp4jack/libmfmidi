#include <iostream>
#include <fstream>
#include "libmfmidi/miditrackplayer.hpp"
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/samhandlers.hpp"
#include "libmfmidi/rtmididevice.hpp"
#include "libmfmidi/kdmapidevice.hpp"
#include "libmfmidi/win32mmtimer.hpp"
#include "libmfmidi/midimessagefdc.hpp"
#include "libmfmidi/midiprocessor.hpp"
#include <processthreadsapi.h>
#include <timeapi.h>
#include <ranges>
#include <filesystem>
#include <spanstream>
#include <exception>

using namespace libmfmidi;
using std::cout;
using std::cin;
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
    std::vector<char> memory; // just for own memory, like unique_ptr
    auto filesize = std::filesystem::file_size(argv[1]);
    memory.resize(filesize);
    stm.read(memory.data(), filesize);
    std::span<char> preread{memory.begin(), memory.end()};
    std::basic_spanstream<char, std::char_traits<char>> ss{preread, std::ios::in | std::ios::binary};
    stm.close();
    cout << "Opened" << endl;

    MIDIMultiTrack    file;
    SMFFileInfo       info;
    SMFFileSAMHandler hsam(&file, &info);
    SMFReader         rd(&ss, 0, &hsam);

    cout << "Parsing SMF" << endl;
    rd.parse();
    cout << "Parsed" << endl;
    cout << "SMF File: Format " << info.type << "; Division: " << static_cast<uint16_t>(info.division) << ";" << endl;
    cout << "NTrks: " << file.size() << ';' << endl;

    

    cout << "Merging" << endl;
    MIDITrack trk;
    mergeMultiTrack(std::move(file), trk);
    cout << "Merged" << endl;

    details::RtMidiMIDIDeviceProvider& prov = details::RtMidiMIDIDeviceProvider::instance();
    cout << "Dev cnt: " << prov.outputCount() << endl;
    for (unsigned int i = 0; i < prov.outputCount(); ++i) {
        cout << prov.outputName(i) << endl;
    }
    cout << "Choose, " << prov.outputCount() + 1 << " to KDMAPI: ";
    unsigned inp;
    std::cin >> inp;

    AbstractMIDIDevice*                                  dev;
    std::unique_ptr<libmfmidi::details::KDMAPIDevice>    kdev;
    std::shared_ptr<libmfmidi::details::RtMidiOutDevice> sdev;
    if (inp == prov.outputCount() + 1) {
        kdev = std::make_unique<libmfmidi::details::KDMAPIDevice>(true);
        dev  = kdev.get();
    } else {
        sdev = libmfmidi::details::RtMidiMIDIDeviceProvider::buildupOutputDevice(inp);
        dev  = sdev.get();
    }

    if (!dev->open()) {
        std::cerr << "Failed to open device" << endl;
    }

    MIDITrackPlayer player; // init player after everything

    auto compfdc = [&](MIDITimedMessage& msg) {
        fdc::MFMarkTempo::process(msg);
        return MIDIMessageF2D::process(msg);
    };
    player.setMsgProcessor(compfdc);
    player.setDivision(info.division);
    player.setDriver(dev);
    player.setTrack(trk);

    // manual init player thread to set priority
    player.initThread();
    SetThreadPriority(player.nativeHandle(), THREAD_PRIORITY_TIME_CRITICAL);

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
