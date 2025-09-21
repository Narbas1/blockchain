#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <algorithm>
#include <iomanip>

using u8 = uint8_t;
using u32 = uint32_t;
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

    for (int i = 7; i >= 0; --i) {
        data.push_back((static_cast<u8>((bitlength >> (i * 8)) & 0xFF)));
    }

}

std::vector<std::array<u8, 64>> split(const std::vector<u8> &input){
    size_t n = input.size() / 64;
    std::vector<std::array<u8,64>> blocks(n);

    for(size_t i = 0; i < n; ++i){
        std::copy_n(input.data() + i*64, 64, blocks[i].data());
    }

    return blocks;
}

std::vector<std::array<uint32_t, 16>> blocks_to_words(const std::vector<std::array<u8,64>> &blocks){

    std::vector<std::array<uint32_t, 16>> result;

    for(const auto &block : blocks){
        std::array<uint32_t, 16> words;
        for(size_t i = 0; i < 16; ++i){
            words[i] = (static_cast<u32>(block[i*4]) << 24) |
                       (static_cast<u32>(block[i*4 + 1]) << 16) |
                       (static_cast<u32>(block[i*4 + 2]) << 8) |
                       (static_cast<u32>(block[i*4 + 3]));
        }
        result.push_back(words);
    }
    return result;
}

void bit_or(std::vector<std::array<uint32_t,16>>& blocks_words) {
    for (auto& w : blocks_words) {
        auto original = w;
        for (int i = 0; i < 16; ++i) {
            int j = (i + 1) & 15;
            w[i] = original[i] | original[j];
        }
    }
}

void bit_xor(std::vector<std::array<uint32_t,16>>& blocks_words) {

    for (auto& w : blocks_words) {

        for (int i = 0; i < 4; ++i) w[i] ^= w[i + 4];
        for (int i = 0; i < 8; ++i) w[i] ^= w[i + 8];
    }
}

void rotate_left(std::array<uint32_t,16>& words) {
    std::array<uint32_t,16> temp = words;
    for (int i = 0; i < 16; ++i) {
        words[i] = temp[(i + 7) % 16];
    }
}

void rotate_right(std::array<uint32_t,16>& words) {
    std::array<uint32_t,16> temp = words;
    for (int i = 0; i < 16; ++i) {
        words[i] = temp[(i - 7 + 16) % 16];
    }
}

void not_second_half(std::array<u32,16>& words) {
    for (size_t i = 8; i < 16; i+=2) {
        words[i] = ~words[i];
    }
}

int main(){

    std::string input;

    std::cout << "Enter a message: ";
    std::getline(std::cin, input);

    std::vector<u8> bytes = string_to_bytes(input);
    padding(bytes);
    std::vector<std::array<u8, 64>> blocks = split(bytes);

    std::vector<std::array<uint32_t, 16>> words = blocks_to_words(blocks);

    bit_or(words);
    bit_xor(words);

    return 0;
}