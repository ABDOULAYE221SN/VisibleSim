/**
 * @file spongent160.cpp
 * @brief SPONGENT-160 lightweight hash function implementation
 */

#include "spongent160.h"
#include <cstring>
#include <algorithm>

namespace Spongent {

// PRESENT S-box (4-bit substitution)
const uint8_t Spongent160::SBOX[16] = {
    0xC, 0x5, 0x6, 0xB, 0x9, 0x0, 0xA, 0xD,
    0x3, 0xE, 0xF, 0x8, 0x4, 0x7, 0x1, 0x2
};

// Round constants for SPONGENT-160 (90 rounds, 7 bits each)
const uint8_t Spongent160::ROUND_CONSTANTS[90] = {
    0x75, 0x3A, 0x1D, 0x0E, 0x07, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
    0x00, 0x00
};

// P-layer permutation table for SPONGENT-160 (176 bits)
static const int PERM_TABLE[176] = {
    0, 44, 88, 132, 1, 45, 89, 133, 2, 46, 90, 134, 3, 47, 91, 135,
    4, 48, 92, 136, 5, 49, 93, 137, 6, 50, 94, 138, 7, 51, 95, 139,
    8, 52, 96, 140, 9, 53, 97, 141, 10, 54, 98, 142, 11, 55, 99, 143,
    12, 56, 100, 144, 13, 57, 101, 145, 14, 58, 102, 146, 15, 59, 103, 147,
    16, 60, 104, 148, 17, 61, 105, 149, 18, 62, 106, 150, 19, 63, 107, 151,
    20, 64, 108, 152, 21, 65, 109, 153, 22, 66, 110, 154, 23, 67, 111, 155,
    24, 68, 112, 156, 25, 69, 113, 157, 26, 70, 114, 158, 27, 71, 115, 159,
    28, 72, 116, 160, 29, 73, 117, 161, 30, 74, 118, 162, 31, 75, 119, 163,
    32, 76, 120, 164, 33, 77, 121, 165, 34, 78, 122, 166, 35, 79, 123, 167,
    36, 80, 124, 168, 37, 81, 125, 169, 38, 82, 126, 170, 39, 83, 127, 171,
    40, 84, 128, 172, 41, 85, 129, 173, 42, 86, 130, 174, 43, 87, 131, 175
};

std::vector<uint8_t> Spongent160::hash(const uint8_t* data, size_t length) {
    // Initialize state to all zeros
    uint8_t state[STATE_SIZE_BYTES];
    std::memset(state, 0, STATE_SIZE_BYTES);
    
    // Pad the input
    std::vector<uint8_t> padded = pad(data, length);
    
    // Absorbing phase
    size_t numBlocks = padded.size() / RATE_BYTES;
    for (size_t i = 0; i < numBlocks; i++) {
        // XOR block into state (first r bits)
        absorbBlock(state, padded.data(), i * RATE_BYTES);
        
        // Apply permutation for all rounds
        for (int round = 0; round < NUM_ROUNDS; round++) {
            permutation(state, round);
        }
    }
    
    // Squeezing phase
    std::vector<uint8_t> output(HASH_SIZE_BYTES);
    squeeze(state, output.data());
    
    return output;
}

std::vector<uint8_t> Spongent160::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

void Spongent160::permutation(uint8_t* state, int round) {
    // Apply S-box layer
    sboxLayer(state);
    
    // Apply P-layer (bit permutation)
    pLayer(state);
    
    // Add round counter
    addCounter(state, round);
}

void Spongent160::sboxLayer(uint8_t* state) {
    // Apply 4-bit S-box to each nibble
    for (int i = 0; i < STATE_SIZE_BYTES; i++) {
        uint8_t high = (state[i] >> 4) & 0x0F;
        uint8_t low = state[i] & 0x0F;
        state[i] = (SBOX[high] << 4) | SBOX[low];
    }
}

void Spongent160::pLayer(uint8_t* state) {
    // Create temporary state for permutation
    uint8_t temp[STATE_SIZE_BYTES];
    std::memset(temp, 0, STATE_SIZE_BYTES);
    
    // Apply bit permutation according to P-layer table
    for (int i = 0; i < STATE_SIZE_BITS; i++) {
        int srcByte = i / 8;
        int srcBit = i % 8;
        int dstPos = PERM_TABLE[i];
        int dstByte = dstPos / 8;
        int dstBit = dstPos % 8;
        
        // Get bit from source
        uint8_t bit = (state[srcByte] >> srcBit) & 1;
        
        // Set bit in destination
        temp[dstByte] |= (bit << dstBit);
    }
    
    // Copy back to state
    std::memcpy(state, temp, STATE_SIZE_BYTES);
}

void Spongent160::addCounter(uint8_t* state, int round) {
    // XOR round constant into the last byte of state
    // Round constants are 7 bits
    if (round < 90) {
        state[STATE_SIZE_BYTES - 1] ^= ROUND_CONSTANTS[round];
    }
}

std::vector<uint8_t> Spongent160::pad(const uint8_t* data, size_t length) {
    // SPONGENT padding: append 1 bit, then zeros until length is multiple of rate
    // Minimum padding is 1 bit
    
    size_t bitLength = length * 8;
    size_t paddedBitLength = ((bitLength / RATE_BITS) + 1) * RATE_BITS;
    size_t paddedByteLength = paddedBitLength / 8;
    
    std::vector<uint8_t> padded(paddedByteLength, 0);
    
    // Copy original data
    std::memcpy(padded.data(), data, length);
    
    // Add padding bit (1 followed by zeros)
    // Set the first bit after the message to 1
    size_t paddingBytePos = length;
    size_t paddingBitPos = 0;
    
    if (paddingBytePos < paddedByteLength) {
        padded[paddingBytePos] |= (0x80 >> paddingBitPos);
    }
    
    return padded;
}

void Spongent160::absorbBlock(uint8_t* state, const uint8_t* data, size_t offset) {
    // XOR rate bytes (r/8 = 2 bytes) into the first part of state
    for (int i = 0; i < RATE_BYTES; i++) {
        state[i] ^= data[offset + i];
    }
}

void Spongent160::squeeze(const uint8_t* state, uint8_t* output) {
    // Extract first n bits (160 bits = 20 bytes) from state
    std::memcpy(output, state, HASH_SIZE_BYTES);
}

} // namespace Spongent
