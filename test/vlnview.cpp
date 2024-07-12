#include "mfmidi/mfranges.hpp"

#include <iostream>

int main(){
    for (auto val: libmfmidi::smf_variable_length_number_view<std::uint32_t, std::byte>{0x12345678}){
        std::cout << static_cast<unsigned char>(val) << ' ';
    }
    std::cout << std::endl;
}