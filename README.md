# blockchain

---

This hashing function takes a string of any length, and produces 128 bit digest. It implements padding and splitting strategy from SHA-256. It uses various bit manipulation techniques, such as XOR'ing, OR'ing, bit shifting and rotating, multiplying by constants that are derived from PI, e, sqrt(2), sqrt(3).

---

With this program you can:
  1. Enter any string to hash,
  2. Hash files with singular character,
  3. Hash files with 3000 random characters,
  4. Hash files that differ in 1 character,
  5. Hash an empty file,
  6. Do tests with konstitucija.txt,
  7. Generate string pairs for later collision test,
  8. Collision test,
  9. Avalanche test.

---

# Pseudocode for padding function:
```
function padding(data: vector<byte>)
    block_size <- 64
    bit_len <- (len(data) * 8) as u64
    append 0x80 to data
    while ( (len(data) + 8) mod 64 ≠ 0 )
        append 0x00 to data
    append 64-bit big-endian length
    for i from 7 downto 0
        b <- (bit_len >> (i*8)) & 0xFF
        append byte(b) to data
```

  #Pseudocode for splitting:

  ```
  function split_into_blocks(data: vector<byte>) -> vector<array[64]byte>
    n <- len(data) / 64
    blocks <- vector of n arrays[64]byte
    for i in 0..n-1
        copy data[i*64 .. i*64+63] into blocks[i]
    return blocks
```

  #Pseudocode for splitting into words:

  ```
function BLOCKS_TO_16_WORDS_BE(blocks) -> vector<array[16]u32>
    result <- empty vector
    for each block in blocks
        words <- array[16]u32
        for i in 0..15
            b0 <- block[4*i + 0]
            b1 <- block[4*i + 1]
            b2 <- block[4*i + 2]
            b3 <- block[4*i + 3]
            words[i] <- (u32)b0<<24 | (u32)b1<<16 | (u32)b2<<8 | (u32)b3
        push words into result
    return result
```

#Pseudocode for bit_or mixing

```
function BIT_OR_MIX(words_vec: vector<array[16]u32>)
    for each w in words_vec
        original <- copy of w
        for i in 0..15
            j <- (i + 1) & 15
            w[i] <- original[i] OR original[j]
```

#Pseudocode for bit_xor mixing:

```
function BIT_XOR_MIX(words_vec: vector[array[16]u32])
    for each w in words_vec
        for i in 0..3
            w[i] <- w[i] XOR w[i+4]
        for i in 0..7
            w[i] <- w[i] XOR w[i+8]
```

#Pseudocode for hashing:

```
function COMPRESS_ALL_BLOCKS(all_blocks_words) -> array[4]u32
    S ← IV (as above)

    for each w[0..15] in all_blocks_words:

        m ← ((w0 & w1) | w2 | w3) XOR w4 XOR w5
        m ← m * c1
        m ← m XOR ( (w6 & w7) | w8 )
        m ← m XOR ( w9 & w10 )
        m ← ( m + ( w11 XOR (w12 | w13) ) ) * c2
        m ← m XOR ( ROTL32(w14, 7) XOR ROTR32(w15, 3) )

        m ← m XOR (m >> 16)
        m ← m * c3
        m ← m XOR (m >> 7)
        m ← m * c4
        m ← m XOR (m >> 13)

        S0 ← ( ROTL32(S0 XOR m, 13) ) * c1 + w0
        S1 ← ( ROTR32(S1 + m, 7) ) XOR ( w5 | c2 )
        S2 ← ( S2 XOR (m * c3) ) + ROTL32(w10, 11)
        S3 ← ( S3 + (m XOR c4) ) XOR ROTR32(w15, 3)

    for each x in S:
        x ← x XOR (x >> 16)
        x ← x * c3
        x ← x XOR (x >> 13)
        x ← x * c4
        x ← x XOR (x >> 16)

        x ← ROTL32(x XOR c1, 5) + ROTR32(x XOR c2, 11)
        x ← x XOR ( x * c3 ) + ( x >> 7 )
        x ← ( x XOR (x << 3) ) * c4
        x ← x XOR ROTL32(x, (x & 13))

    return S
```
















    
