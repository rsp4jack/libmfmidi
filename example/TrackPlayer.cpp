#include <iostream>
#include <fstream>
#include "libmfmidi/miditrackplayer.hpp"
#include "libmfmidi/simpletrackplayer.hpp"
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/samhandlers.hpp"
#include "libmfmidi/details/rtmididevice.hpp"
#include "libmfmidi/details/kdmapidevice.hpp"
#include "libmfmidi/details/win32mmtimer.hpp"
#include "libmfmidi/midimessagefdc.hpp"
#include "libmfmidi/midiprocessor.hpp"
#include <processthreadsapi.h>
#include <timeapi.h>
#include <ranges>

using namespace libmfmidi;
using std::cout;
using std::cin;
using std::endl;

int main(int argc, char** argv)
{
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
    cout << "TrackPlayer: Example of libmfmidi" << endl;
    if (argc == 1) {
        std::cerr << "Error: No input file" << std::endl;
        return -1;
    }

    cout << "Opening file " << argv[1] << endl;
    std::fstream stm;
    char         filebuffer[2048]{};
    stm.rdbuf()->pubsetbuf(filebuffer, 2048);

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

    stm.close();

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
        } else {
            cout << "Unknown Command: " << cmd << endl;
        }
    }
    return 0;
}
