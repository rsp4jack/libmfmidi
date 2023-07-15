#include <iostream>
#include <fstream>
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/samhandlers.hpp"
#include <format>
#include <filesystem>

using std::cout;
using std::endl;
using namespace libmfmidi;

int main(int argc, char** argv)
{
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

    std::fstream out;
    auto         name = std::format("{}.{}.trk", std::filesystem::path(argv[1]).filename().string(), static_cast<uint16_t>(info.division));
    out.open(name, std::ios::out | std::ios::binary);
    for (const auto& msg : trk) {
        writeVarNum(msg.deltaTime(), &out);
        out.write(reinterpret_cast<const char*>(msg.base().data()), msg.size());
    }
    out.flush();
    out.close();
    cout << "Done, saved as " << name << endl;
    return 0;
}