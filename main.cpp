#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>

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
        m ^= m >> 7;
        m *= c4;
        m ^= m >> 13;

        S[0] = (rotl32(S[0] ^ m, 13)) * c1 + w[0];
        S[1] = (rotr32(S[1] + m, 7))  ^ (w[5] | c2);
        S[2] = (S[2] ^ (m * c3)) + rotl32(w[10], 11);
        S[3] = (S[3] + (m ^ c4)) ^ rotr32(w[15], 3);

    }
    for (auto& x : S) {
        x ^= x >> 16;
        x *= c3;
        x ^= x >> 13;
        x *= c4;
        x ^= x >> 16;
        x = rotl32(x ^ c1, 5) + rotr32(x ^ c2, 11);
        x ^= (x * c3) + (x >> 7);
        x = (x ^ (x << 3)) * c4;
        u32 r = ((x >> 27) | 1);
        x ^= rotl32(x, r);
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

    while(true){
        std::string input;

        std::cout << "Options: 1. Enter a string to hash" << std::endl;
        std::cout << "         2. Hash files with singular character" << std::endl;
        std::cout << "         3. Hash files with 3000 random characters" << std::endl;
        std::cout << "         4. Hash files that differ in 1 character" << std::endl;
        std::cout << "         5. Hash empty file" << std::endl;
        std::cout << "         6. Test with file konstitucija.txt" << std::endl;
        int option;
        std::cin >> option;

        if(option == 1){
            std::cout << "Enter a string to hash: ";
            std::cin.ignore();
            std::getline(std::cin, input);

            auto digest = hash(input);
            std::cout << "Hash: ";
            for (const auto &part : digest) {
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            }
            std::cout << std::endl;
        }

        if(option == 2){
            std::ifstream a("files/a.txt", std::ios::binary);
            std::ifstream b("files/b.txt", std::ios::binary);

            std::ostringstream buffer;
            buffer << a.rdbuf();
            std::string content = buffer.str();

            auto digest_a = hash(content);
            std::cout << "Hash of a.txt: ";
            for (const auto &part : digest_a) {
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            }
            std::cout << std::endl;

            buffer.clear();
            buffer << b.rdbuf();
            content = buffer.str();

            auto digest_b = hash(content);
            std::cout << "Hash of b.txt: ";
            for (const auto &part : digest_b) {
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            }
            std::cout << std::endl;


        }

        if (option == 3) {
            std::ifstream file1("files/random3000_1.txt", std::ios::binary);
            std::ifstream file2("files/random3000_2.txt", std::ios::binary);

            std::string content1((std::istreambuf_iterator<char>(file1)), {});
            std::string content2((std::istreambuf_iterator<char>(file2)), {});

            auto digest1 = hash(content1);
            std::cout << "Hash of random3000_1.txt: ";
            for (const auto &part : digest1)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;

            auto digest2 = hash(content2);
            std::cout << "Hash of random3000_2.txt: ";
            for (const auto &part : digest2)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
}

        if(option == 4){
            std::ifstream file1("files/random1.txt", std::ios::binary);
            std::ifstream file2("files/random2.txt", std::ios::binary);

            std::string content1((std::istreambuf_iterator<char>(file1)), {});
            std::string content2((std::istreambuf_iterator<char>(file2)), {});

            auto digest1 = hash(content1);
            std::cout << "Hash of random1.txt: ";
            for (const auto &part : digest1)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;

            auto digest2 = hash(content2);
            std::cout << "Hash of random2.txt: ";
            for (const auto &part : digest2)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
        }

        if(option == 5){
            std::ifstream file("files/empty.txt", std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(file)), {});

            auto digest = hash(content);
            std::cout << "Hash of empty.txt: ";
            for (const auto &part : digest)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
        }

        if(option == 6){

            std::ifstream file("files/konstitucija.txt", std::ios::binary);

            std::vector<std::string> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(line + "\n");
            }
            std::vector<size_t> line_counts = {1,2,4,8,16,32,64,128, 256, 512, 789};

            for (size_t n : line_counts) {

                if (n > lines.size()) break;

                std::string combined;
                for (size_t i = 0; i < n; ++i) {
                    combined += lines[i];
                }

                auto start = std::chrono::high_resolution_clock::now();
                auto digest = hash(combined);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> duration = end - start;

                std::cout << "Hash of first " << std::dec << n << " lines: ";
                for (const auto& part : digest) {
                    std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
                }
                std::cout << " (Time: " << duration.count() << " ms)";
                std::cout << std::endl;
            }
        }

    }

    return 0;
}