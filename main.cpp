#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>

using u8 = uint8_t;
using u64 = uint64_t;

std::vector<u8> string_to_bytes(const std::string &input) {
    return std::vector<u8>(input.begin(), input.end());
}

void padding(std::vector<u8> &data){
    const size_t block = 64;
    u64 bitlength = static_cast<u64>(data.size()) * 8;

    data.push_back(0x80);
    while ((data.size() + 8) % block != 0) {
        data.push_back(0x00);
    }

    

}

int main(){

    std::string input;

    std::cout << "Enter a message: ";
    std::getline(std::cin, input);

    string_to_bytes(input);

    return 0;
}