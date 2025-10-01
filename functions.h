#pragma once

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

// ---------------------------
// Public integer aliases
// ---------------------------
using u8  = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

// ---------------------------
// Public constants/utilities used in main.cpp too
// ---------------------------
static const std::string CHARSET =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

// -------- popcount + digest Hamming (unchanged API) --------
static inline unsigned popcount32(uint32_t x) {
#if defined(__GNUG__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_popcount(x));
#elif defined(_MSC_VER)
    return static_cast<unsigned>(__popcnt(x));
#else
    unsigned c = 0; while (x) { x &= (x - 1); ++c; } return c;
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

// ---------------------------
// Forward declare the hash() used by main.cpp
// ---------------------------
std::array<u32,4> hash(const std::string &input);

// ---------------------------
// Test helpers used by main.cpp (unchanged API)
// ---------------------------
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

// =====================================================================
//                ✦✦  HASHING PIPELINE (improved)  ✦✦
//  - Fixed endianness (big-endian word loads, explicit).
//  - Stronger per-block mixing (ARX rounds inspired by Murmur/xx family).
//  - Better word scheduling instead of simple OR/XOR.
//  - Robust finalizer (fmix32-like) on each lane + length folding.
//  The external API and helper names remain the same for your main.cpp.
// =====================================================================

static inline u32 rotl32(u32 x, unsigned r) noexcept { return (x << r) | (x >> (32 - r)); }
static inline u32 rotr32(u32 x, unsigned r) noexcept { return (x >> r) | (x << (32 - r)); }

// Murmur3-style finalizer for 32-bit lanes
static inline u32 fmix32(u32 x) noexcept {
    x ^= x >> 16;
    x *= 0x85EBCA6Bu; // c1'
    x ^= x >> 13;
    x *= 0xC2B2AE35u; // c2'
    x ^= x >> 16;
    return x;
}

// Fixed big-endian 32-bit load (portable & alignment-safe)
static inline u32 load32_be(const u8* p) noexcept {
    return (static_cast<u32>(p[0]) << 24) |
           (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) <<  8) |
           (static_cast<u32>(p[3])      );
}

// ---------------------------
// 1) Bytes from string
// ---------------------------
inline std::vector<u8> string_to_bytes(const std::string &input) {
    return std::vector<u8>(input.begin(), input.end());
}

// ---------------------------
// 2) SHA-like padding to 512-bit blocks (64 bytes per block)
//    - appends 0x80, zeros, then 64-bit big-endian bit length
// ---------------------------
inline void padding(std::vector<u8> &data){
    const size_t block = 64;
    u64 bitlength = static_cast<u64>(data.size()) * 8;

    data.push_back(0x80u);
    while ((data.size() + 8) % block != 0) data.push_back(0x00u);

    // 64-bit big-endian length
    for (int i = 7; i >= 0; --i)
        data.push_back(static_cast<u8>((bitlength >> (i * 8)) & 0xFFu));
}

// ---------------------------
// 3) Split into 64-byte blocks
// ---------------------------
inline std::vector<std::array<u8, 64>> split(const std::vector<u8> &input){
    size_t n = input.size() / 64;
    std::vector<std::array<u8,64>> blocks(n);
    for(size_t i = 0; i < n; ++i){
        std::copy_n(input.data() + i*64, 64, blocks[i].data());
    }
    return blocks;
}

// ---------------------------
// 4) Turn each block into 16 big-endian u32 words
// ---------------------------
inline std::vector<std::array<u32, 16>> blocks_to_words(const std::vector<std::array<u8,64>> &blocks){
    std::vector<std::array<u32, 16>> result;
    result.reserve(blocks.size());
    for(const auto &block : blocks){
        std::array<u32, 16> w{};
        for(size_t i = 0; i < 16; ++i)
            w[i] = load32_be(&block[i*4]);
        result.push_back(w);
    }
    return result;
}

// ---------------------------
// 5) Word scheduling stage A (replace old bit_or):
//    - mix with neighbors, add constants, rotate different amounts.
//    - goal: pre-diffuse before compression.
// ---------------------------
inline void bit_or(std::vector<std::array<u32,16>>& blocks_words) {
    constexpr u32 C1 = 0x9E3779B1u; // golden ratio (32-bit)
    for (auto& w : blocks_words) {
        std::array<u32,16> t = w;
        for (int i = 0; i < 16; ++i) {
            int a = (i + 1) & 15;
            int b = (i + 9) & 15;
            u32 v = t[i] ^ rotl32(t[a], (i % 7) + 5);
            v += (t[b] ^ C1) + static_cast<u32>(i * 0x1000193u);
            w[i] = rotl32(v, (i * 3 + 7) & 31);
        }
    }
}

// ---------------------------
// 6) Word scheduling stage B (replace old bit_xor):
//    - "column" / "diagonal" style XOR-add-rotate (ARX) like ChaCha mix.
// ---------------------------
inline void bit_xor(std::vector<std::array<u32,16>>& blocks_words) {
    for (auto& w : blocks_words) {
        // 2 passes to spread dependencies
        for (int pass = 0; pass < 2; ++pass) {
            // Quarter rounds on 4 lanes
            auto qr = [](u32& a, u32& b, u32& c, u32& d) {
                a += b; d ^= a; d = rotl32(d,16);
                c += d; b ^= c; b = rotl32(b,12);
                a += b; d ^= a; d = rotl32(d, 8);
                c += d; b ^= c; b = rotl32(b, 7);
            };
            qr(w[ 0], w[ 4], w[ 8], w[12]);
            qr(w[ 1], w[ 5], w[ 9], w[13]);
            qr(w[ 2], w[ 6], w[10], w[14]);
            qr(w[ 3], w[ 7], w[11], w[15]);

            qr(w[ 0], w[ 5], w[10], w[15]);
            qr(w[ 1], w[ 6], w[11], w[12]);
            qr(w[ 2], w[ 7], w[ 8], w[13]);
            qr(w[ 3], w[ 4], w[ 9], w[14]);
        }
    }
}

// ---------------------------
// 7) Per-block compression (replace old merge_all_blocks_into_digest):
//    - 4-lane state S
//    - 2 sweeps over 16 words with different constants
//    - ARX rounds; block index and message length folded into state
//    - final fmix on each lane
// ---------------------------
inline std::array<u32,4> merge_all_blocks_into_digest(
        const std::vector<std::array<u32,16>>& blocks_words,
        u64 total_bits) {

    // ChaCha/Blake/Murmur-inspired constants
    constexpr u32 K1 = 0x243F6A88u; // pi
    constexpr u32 K2 = 0x85A308D3u;
    constexpr u32 K3 = 0x13198A2Eu;
    constexpr u32 K4 = 0x03707344u;

    constexpr u32 C1 = 0x9E3779B1u; // golden ratio
    constexpr u32 C2 = 0x85EBCA77u; // Murmur-ish
    constexpr u32 C3 = 0xC2B2AE3Du;
    constexpr u32 C4 = 0x27D4EB2Fu;

    std::array<u32,4> S = {K1, K2, K3, K4};

    auto round_mix = [](std::array<u32,4>& s, u32 m0, u32 m1, u32 m2, u32 m3, u32 cA, u32 cB) {
        s[0] += m0 + cA; s[1] ^= s[0]; s[1] = rotl32(s[1], 13);
        s[2] += s[1] + cB; s[3] ^= s[2]; s[3] = rotr32(s[3], 16);

        s[0] += m1; s[2] ^= s[0]; s[2] = rotl32(s[2], 7);
        s[1] += m2; s[3] ^= s[1]; s[3] = rotl32(s[3], 17);

        s[0] += m3; s[1] ^= s[0]; s[1] = rotl32(s[1], 11);
        // shuffle lanes to break symmetry
        std::swap(s[0], s[2]);
        s[1] = rotl32(s[1], 3);
        s[3] = rotr32(s[3], 5);
    };

    for (size_t b = 0; b < blocks_words.size(); ++b) {
        const auto& w = blocks_words[b];

        // Sweep 0: forward
        for (int i = 0; i < 16; i += 4) {
            round_mix(S, w[i], w[i+1], w[i+2], w[i+3], C1, C2);
        }
        // Fold block index and size (prevents extension/simple-prefix issues)
        S[0] ^= static_cast<u32>(b) ^ static_cast<u32>(total_bits);
        S[1] ^= static_cast<u32>(b >> 1) ^ static_cast<u32>(total_bits >> 32);
        S[2] += rotl32(S[0], 5) + C3;
        S[3] += rotr32(S[1], 7) + C4;

        // Sweep 1: reversed, different constants
        for (int i = 12; i >= 0; i -= 4) {
            round_mix(S, w[i], w[i+1], w[i+2], w[i+3], C3, C4);
        }

        // Mild per-block final scramble
        S[0] ^= rotl32(S[3], 9);
        S[1] ^= rotr32(S[0], 7);
        S[2] ^= rotl32(S[1], 13);
        S[3] ^= rotr32(S[2], 11);
    }

    // Incorporate total length (bits) one more time
    S[0] ^= static_cast<u32>(total_bits);
    S[1] ^= static_cast<u32>(total_bits >> 32);
    S[2] += 0xDEADBEEFu;
    S[3] += 0xBADC0FFEu;

    // Per-lane fmix
    S[0] = fmix32(S[0]);
    S[1] = fmix32(S[1]);
    S[2] = fmix32(S[2]);
    S[3] = fmix32(S[3]);

    // Cross-lane avalanche
    u32 all = S[0] ^ S[1] ^ S[2] ^ S[3];
    S[0] = fmix32(S[0] ^ rotl32(all,  3));
    S[1] = fmix32(S[1] ^ rotl32(all, 11));
    S[2] = fmix32(S[2] ^ rotl32(all, 17));
    S[3] = fmix32(S[3] ^ rotl32(all, 23));

    return S;
}

// ---------------------------
// 8) Top-level hash() used by main.cpp
// ---------------------------
inline std::array<u32,4> hash(const std::string &input) {
    std::vector<u8> bytes = string_to_bytes(input);
    const u64 total_bits_before_pad = static_cast<u64>(bytes.size()) * 8ULL;

    padding(bytes);
    std::vector<std::array<u8, 64>> blocks = split(bytes);
    std::vector<std::array<u32, 16>> words  = blocks_to_words(blocks);

    // schedule/mix words before compression
    bit_or(words);
    bit_xor(words);

    return merge_all_blocks_into_digest(words, total_bits_before_pad);
}

// ---------------------------
// 9) random_string helper (unchanged API)
// ---------------------------
inline std::string random_string(size_t length, std::mt19937 &gen, std::uniform_int_distribution<size_t> &dist) {
    std::string s; s.resize(length);
    for (size_t i = 0; i < length; ++i) s[i] = CHARSET[dist(gen)];
    return s;
}
