#include <iostream>
#include <fstream>
#include "libmfmidi/smfreader.hpp"
#include "libmfmidi/samhandlers.hpp"
using namespace libmfmidi;
int main(int argc, char** argv)
{
    if (argc == 1) {
        std::cerr << "Error: No input file" << std::endl;
        return -1;
    }
    std::fstream stm;
    stm.open(argv[1], std::ios::in | std::ios::binary);
    HumanReadableSAMHandler hsam(&std::cout);
    SMFReader rd (&stm, 0, &hsam);
    rd.parse();
    return 0;
}