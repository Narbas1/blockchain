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
#include <random>
#include <limits>

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

std::array<u32,4> hash(const std::string &input);

static const std::string CHARSET =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

static inline unsigned popcount32(uint32_t x) {
#if defined(__GNUG__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_popcount(x));
#elif defined(_MSC_VER)
    return static_cast<unsigned>(__popcnt(x));
#else
    unsigned c = 0;
    while (x) { x &= (x - 1); ++c; }
    return c;
#endif
}
static inline unsigned digest_hamming(const std::array<u32,4>& A,
                                      const std::array<u32,4>& B) {
    unsigned d = 0;
    d += popcount32(A[0] ^ B[0]);
    d += popcount32(A[1] ^ B[1]);
    d += popcount32(A[2] ^ B[2]);
    d += popcount32(A[3] ^ B[3]);
    return d;
}

std::pair<std::string, std::string>
make_pair_one_diff(size_t L, std::mt19937& gen) {
    std::uniform_int_distribution<size_t> dist_pos(0, L - 1);
    std::uniform_int_distribution<size_t> dist_char(0, CHARSET.size() - 1);

    std::string a; a.resize(L);
    for (size_t i = 0; i < L; ++i) a[i] = CHARSET[dist_char(gen)];

    std::string b = a;
    size_t pos = dist_pos(gen);

    char newc;
    do { newc = CHARSET[dist_char(gen)]; } while (newc == a[pos]);
    b[pos] = newc;

    return {a, b};
}

void avalanche_stats_for_length(size_t L, size_t N, std::mt19937& gen) {
    unsigned min_d = std::numeric_limits<unsigned>::max();
    unsigned max_d = 0;
    unsigned long long sum_d = 0ULL;

    for (size_t i = 0; i < N; ++i) {
        auto [s1, s2] = make_pair_one_diff(L, gen);

        auto h1 = hash(s1);
        auto h2 = hash(s2);

        unsigned d = digest_hamming(h1, h2);
        if (d < min_d) min_d = d;
        if (d > max_d) max_d = d;
        sum_d += d;
    }

    double avg = static_cast<double>(sum_d) / static_cast<double>(N);

    std::cout << "Length " << std::dec << L
              << " | Pairs " << N
              << " | Hamming distance (out of 128 bits): "
              << "min=" << min_d
              << ", avg=" << std::fixed << std::setprecision(2) << avg
              << ", max=" << std::dec << max_d
              << " | ideal avg = 64\n";
}

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

inline std::string random_string(size_t length, std::mt19937 &gen, std::uniform_int_distribution<size_t> &dist) {
    std::string s;
    s.resize(length);
    for (size_t i = 0; i < length; ++i) s[i] = CHARSET[dist(gen)];
    return s;
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
        std::cout << "         7. Generate 100 000 random pairs of 10, 100, 500, 1000 characters length" << std::endl;
        std::cout << "         8. Collision search in pair_num files" << std::endl;
        std::cout << "         9. Avalanche test" << std::endl;
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

        if (option == 2) {
            std::ifstream a("files/a.txt", std::ios::binary);
            std::ifstream b("files/b.txt", std::ios::binary);

            std::string content_a((std::istreambuf_iterator<char>(a)), {});
            std::string content_b((std::istreambuf_iterator<char>(b)), {});

            auto digest_a = hash(content_a);
            std::cout << "Hash of a.txt: ";
            for (const auto &part : digest_a)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;

            auto digest_b = hash(content_b);
            std::cout << "Hash of b.txt: ";
            for (const auto &part : digest_b)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
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

    if (option == 7) {
        const size_t pairs = 100000;
        const std::array<size_t, 4> lengths = {10, 100, 500, 1000};

        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, CHARSET.size() - 1);

        for (size_t L : lengths) {
            std::string filename = "pairs_" + std::to_string(L) + ".txt";
            std::ofstream out(filename, std::ios::binary);

            for (size_t i = 0; i < pairs; ++i) {
                std::string a = random_string(L, gen, dist);
                std::string b = random_string(L, gen, dist);
                out << a << '\t' << b << '\n';
            }
            out.close();
            std::cout << "Wrote " << pairs << " pairs to " << filename << "\n";
        }
    }

    if (option == 8) {
        const std::array<size_t, 4> lengths = {10, 100, 500, 1000};

        for (size_t L : lengths) {
            std::string filename = "pairs_" + std::to_string(L) + ".txt";
            std::ifstream in(filename, std::ios::binary);

            size_t collision_count = 0;
            std::string line;
            while (std::getline(in, line)) {
                std::istringstream iss(line);
                std::string a, b;
                if (!(iss >> a >> b)){
                    continue;
                }

                auto hash_a = hash(a);
                auto hash_b = hash(b);
                if (hash_a == hash_b) {
                    ++collision_count;
                }
            }
            in.close();
            std::cout << "Total collisions for length " << L << ": " << collision_count << "\n";
        }
    }
    if (option == 9) {
        const size_t N = 25000;
        const std::array<size_t,4> lengths = {10, 20, 50, 100};

        std::mt19937 gen(std::random_device{}());

        for (size_t L : lengths) {
            avalanche_stats_for_length(L, N, gen);
        }
    }



        }

    return 0;
}