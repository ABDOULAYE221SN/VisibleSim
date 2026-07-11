/**
 * @file spongent160.h
 * @brief SPONGENT-160 lightweight hash function implementation
 * 
 * SPONGENT-160 is a lightweight cryptographic hash function based on
 * the sponge construction with a PRESENT-type permutation.
 * 
 * Parameters for SPONGENT-160:
 * - Hash size (n): 160 bits
 * - State width (b): 176 bits
 * - Capacity (c): 160 bits
 * - Bitrate (r): 16 bits
 * - Rounds (R): 90
 * 
 * Reference: "SPONGENT: A Lightweight Hash Function" (CHES 2011)
 * Authors: Andrey Bogdanov, Miroslav Knežević, Gregor Leander, et al.
 */

#ifndef SPONGENT160_H
#define SPONGENT160_H

#include <cstdint>
#include <vector>
#include <cstring>

namespace Spongent {

/**
 * @class Spongent160
 * @brief Implementation of SPONGENT-160 hash function
 */
class Spongent160 {
public:
    // Constants for SPONGENT-160
    static constexpr int HASH_SIZE_BITS = 160;
    static constexpr int HASH_SIZE_BYTES = 20;
    static constexpr int STATE_SIZE_BITS = 176;
    static constexpr int STATE_SIZE_BYTES = 22;
    static constexpr int CAPACITY_BITS = 160;
    static constexpr int RATE_BITS = 16;
    static constexpr int RATE_BYTES = 2;
    static constexpr int NUM_ROUNDS = 90;

    /**
     * @brief Compute SPONGENT-160 hash of input data
     * @param data Input data to hash
     * @param length Length of input data in bytes
     * @return 160-bit hash value (20 bytes)
     */
    static std::vector<uint8_t> hash(const uint8_t* data, size_t length);
    
    /**
     * @brief Compute SPONGENT-160 hash of input vector
     * @param data Input data to hash
     * @return 160-bit hash value (20 bytes)
     */
    static std::vector<uint8_t> hash(const std::vector<uint8_t>& data);

private:
    // PRESENT S-box (4-bit substitution) - declared here, defined in .cpp
    static const uint8_t SBOX[16];
    
    // Round constants for SPONGENT-160 (90 rounds)
    static const uint8_t ROUND_CONSTANTS[90];
    
    /**
     * @brief Apply PRESENT-type permutation to state
     * @param state State array (176 bits = 22 bytes)
     * @param round Round number (for round constant)
     */
    static void permutation(uint8_t* state, int round);
    
    /**
     * @brief Apply S-box layer (4-bit substitution)
     * @param state State array
     */
    static void sboxLayer(uint8_t* state);
    
    /**
     * @brief Apply P-layer (bit permutation)
     * @param state State array
     */
    static void pLayer(uint8_t* state);
    
    /**
     * @brief Add round counter to state
     * @param state State array
     * @param round Round number
     */
    static void addCounter(uint8_t* state, int round);
    
    /**
     * @brief Pad input data according to SPONGENT padding rule
     * @param data Input data
     * @param length Length of input data
     * @return Padded data
     */
    static std::vector<uint8_t> pad(const uint8_t* data, size_t length);
    
    /**
     * @brief XOR data into state (absorbing phase)
     * @param state State array
     * @param data Data to absorb
     * @param offset Offset in data
     */
    static void absorbBlock(uint8_t* state, const uint8_t* data, size_t offset);
    
    /**
     * @brief Extract hash from state (squeezing phase)
     * @param state State array
     * @param output Output buffer
     */
    static void squeeze(const uint8_t* state, uint8_t* output);
};

} // namespace Spongent

#endif // SPONGENT160_H
