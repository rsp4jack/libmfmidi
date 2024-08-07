#include "mfmidi/samhandlers.hpp"
#include "mfmidi/smfreader.hpp"
#include <fstream>
#include <iostream>
using namespace mfmidi;
int main(int argc, char** argv)
{
    if (argc == 1) {
        std::cerr << "Error: No input file" << std::endl;
        return -1;
    }
    std::fstream stm;
    stm.open(argv[1], std::ios::in | std::ios::binary);
    HumanReadableSAMHandler hsam(&std::cout);
    SMFReader rd(&hsam, &stm);;
    rd.parse();
    return 0;
}