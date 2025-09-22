#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <fstream>

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

// void rotate_left(std::array<uint32_t,16>& words) {
//     std::array<uint32_t,16> temp = words;
//     for (int i = 0; i < 16; ++i) {
//         words[i] = temp[(i + 7) % 16];
//     }
// }

// void rotate_right(std::array<uint32_t,16>& words) {
//     std::array<uint32_t,16> temp = words;
//     for (int i = 0; i < 16; ++i) {
//         words[i] = temp[(i - 7 + 16) % 16];
//     }
// }

std::array<u32,4> merge_all_blocks_into_digest(const std::vector<std::array<u32,16>>& blocks_words) {
    auto rotl32 = [] (u32 x, unsigned r) -> u32 {return (x << r) | (x >> (32 - r));};
    auto rotr32 = [] (u32 x, unsigned r) -> u32 {return (x >> r) | (x << (32 - r));};
    // Konstantos
    constexpr u32 c1 = 0x243F6A88u; //pi
    constexpr u32 c2 = 0x2B7E1516u; //e
    constexpr u32 c3 = 0x6A09E667u; //sqrt(2)
    constexpr u32 c4 = 0xBB67AE85u; //sqrt(3)

    std::array<u32,4> S = {0x243F6A88u, 0x85A308D3u, 0x13198A2Eu, 0x03707344u}; //is sha-256

    for (const auto& w : blocks_words) {
        // Atsitiktiniai veiksmai su bitais
        u32 m = ((w[0] & w[1]) | w[2] | w[3]) ^ w[4] ^ w[5];
        m = m * c1;
        m ^= ((w[6] & w[7]) | w[8]);
        m ^= (w[9] & w[10]);
        m = (m + (w[11] ^ (w[12] | w[13]))) * c2;
        m ^= rotl32(w[14], 7) ^ rotr32(w[15], 3);

        m ^= m >> 16;
        m *= c3;
        m ^= m >> 11;
        m *= c4;
        m ^= m >> 16;

        S[0] = (rotl32(S[0] ^ m, 13)) * c1 + w[0];
        S[1] = (rotr32(S[1] + m, 7))  ^ (w[5] | c2);
        S[2] = (S[2] ^ (m * c3)) + rotl32(w[10], 11);
        S[3] = (S[3] + (m ^ c4)) ^ rotr32(w[15], 3);

    }

    for (auto& x : S) {
        x ^= x >> 16; x *= c3;
        x ^= x >> 13; x *= c4;
        x ^= x >> 16;
    }

    return S;
}

std::array<u32,4> hash(const std::string &input) {
    std::vector<u8> bytes = string_to_bytes(input);
    padding(bytes);
    std::vector<std::array<u8, 64>> blocks = split(bytes);
    std::vector<std::array<uint32_t, 16>> words = blocks_to_words(blocks);
    bit_or(words);
    bit_xor(words);
    return merge_all_blocks_into_digest(words);
}

int main(){

    std::string input;

    std::cout << "Options: 1. Enter a string to hash" << std::endl;
    std::cout << "         2. Hash files with singular character" << std::endl;
    std::cout << "         3. Hash files with 3000 characters" << std::endl;
    std::cout << "         4. Hash files that differ in 1 character" << std::endl;
    std::cout << "         5. Test with file konstitucija.txt" << std::endl;
    
    int option;
    std::cin >> option;

    while(true){

        if(option == 1){
            std::cout << "Enter a string to hash: ";
            std::cin.ignore();
            std::getline(std::cin, input);
            auto digest = hash(input);
            std::cout << "Hash: ";
            for (const auto &part : digest) {
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            }
        }

        if(option == 2){
            std::ifstream a("a.txt");
            std::ifstream b("b.txt");

            
        }



    

    return 0;
}