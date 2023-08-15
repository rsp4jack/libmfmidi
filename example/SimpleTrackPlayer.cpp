/// \file SimpleTrackPlayer.cpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \deprecated Deprecated because \c SimpleMIDIPlayer

#include "libmfmidi/simpletrackplayer.hpp"
#include "libmfmidi/kdmapidevice.hpp"
#include "libmfmidi/midimessagefdc.hpp"
#include "libmfmidi/midiprocessor.hpp"
#include "libmfmidi/rtmididevice.hpp"
#include "libmfmidi/samhandlers.hpp"
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/win32mmtimer.hpp"
#include <fstream>
#include <iostream>
#include <ranges>


using namespace libmfmidi;
using std::cout;
using std::endl;

[[deprecated]] int main(int argc, char** argv)
{
    std::cerr << "This example is deprecated." << endl;
    cout << "SimpleTrackPlayer: Example of libmfmidi" << endl;
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
    SMFReader         rd(&hsam, &stm);

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

    details::Win32MMTimer timer;
    SimpleTrackPlayer     player; // init player after everything

    auto compfdc = [&](MIDITimedMessage& msg) {
        fdc::MFMarkTempo::process(msg);
        return MIDIMessageF2D::process(msg);
    };
    player.setMsgProcessor(compfdc);
    player.setDivision(info.division);
    player.setDriver(dev);
    player.setTimer(&timer);

    cout << "Preprocessing" << endl;
    player.setTrack(std::move(trk));
    cout << "Preprocessed" << endl;

    std::vector<std::string> splitedcmd;
    std::getchar();
    while (true) {
        cout << "> ";
        std::string cmd;
        std::getline(std::cin, cmd);
        splitedcmd.clear();
        for (const auto& i : std::ranges::views::split(cmd, std::string_view(" "))) {
            splitedcmd.emplace_back(i.begin(), i.end());
        }

        if (splitedcmd.empty()) {

        } else if (splitedcmd[0] == "play") {
            player.play();
        } else if (splitedcmd[0] == "pause") {
            player.pause();
        } else if (splitedcmd[0] == "pos") {
            cout << "Current tick time: " << player.tickTime() << endl;
            continue;
        } else if (splitedcmd[0] == "seek") {
            sendAllSoundsOff(dev);
            player.goZero();
        } else if (splitedcmd[0] == "exit") {
            break;
        } else if (splitedcmd[0] == "status") {
            cout << "Is playing: " << player.isPlaying() << endl;
        } else {
            cout << "Unknown Command: " << cmd << endl;
        }
    }
    player.pause();
    return 0;
}
