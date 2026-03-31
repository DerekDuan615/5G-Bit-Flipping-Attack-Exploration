/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "nr_pdcp_entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nr_pdcp_security_nea2.h"
#include "nr_pdcp_security_nea1.h"
#include "nr_pdcp_integrity_nia2.h"
#include "nr_pdcp_integrity_nia1.h"
#include "nr_pdcp_sdu.h"

#include "LOG/log.h"

// --- Simple deterministic PRNG using XorShift, seeded from keystream --- //
static uint32_t prng_state;
static void prng_seed(const uint8_t *key, size_t keylen) {
    // keylen should be byte length
    // Simple hash of the key for seeding (not cryptographically strong)
    prng_state = 0x12345678UL;    // 32-bit unsigned long constant (hex)
    for (size_t i = 0; i < keylen; ++i) {
        prng_state ^= ((uint32_t)key[i] << ((i % 4) * 8));
        prng_state *= 0x9e3779b1UL;
    }
}
static uint32_t prng_next() {
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;
    return prng_state;
}

// --- Bit-level PRP using Fisher-Yates shuffle --- //
void prp_permute_bits(const char *input, uint8_t *output, size_t bitlen, const uint8_t *keystream, size_t keylen) {
    size_t *perm = malloc(bitlen * sizeof(size_t));
    for (size_t i = 0; i < bitlen; ++i) perm[i] = i;
    prng_seed(keystream, keylen);

    // Fisher-Yates shuffle
    for (size_t i = bitlen - 1; i > 0; --i) {
        size_t j = prng_next() % (i + 1);
        size_t tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    memset(output, 0, (bitlen + 7) / 8);

    // Permute bits according to perm[]
    for (size_t i = 0; i < bitlen; ++i) {
        size_t src_bit = perm[i];
        size_t src_byte = src_bit / 8, src_off = src_bit % 8;
        size_t dst_byte = i / 8, dst_off = i % 8;
        uint8_t bit = (input[src_byte] >> src_off) & 1u;
        output[dst_byte] |= bit << dst_off;
    }

    free(perm);
}

// --- Inverse permutation --- //
void prp_invert_permute_bits(const unsigned char *input, uint8_t *output, size_t bitlen, const uint8_t *keystream, size_t keylen) {
    size_t *perm = malloc(bitlen * sizeof(size_t));
    // Same shuffle as before
    for (size_t i = 0; i < bitlen; ++i) perm[i] = i;
    prng_seed(keystream, keylen);
    for (size_t i = bitlen - 1; i > 0; --i) {
        size_t j = prng_next() % (i + 1);
        size_t tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    // Compute inverse permutation
    size_t *inv_perm = malloc(bitlen * sizeof(size_t));
    for (size_t i = 0; i < bitlen; ++i)
        inv_perm[perm[i]] = i;

    memset(output, 0, (bitlen + 7) / 8);

    for (size_t i = 0; i < bitlen; ++i) {
        size_t src_bit = inv_perm[i];
        size_t src_byte = src_bit / 8, src_off = src_bit % 8;
        size_t dst_byte = i / 8, dst_off = i % 8;
        uint8_t bit = (input[src_byte] >> src_off) & 1u;
        output[dst_byte] |= bit << dst_off;
    }

    free(perm);
    free(inv_perm);
}

/* Enable or Disable Shuffling here: '0' means disable, '1' means enable */
static int shuffle_enable = 1; 

// --- Copy the checksum function in openairinterface5g/openair2/UTIL/OPT/probe.c --- //
// Need to change the name of the function, otherwise we will get an error of "multiple definition of `checksum'" when compilation
unsigned short checksum_pdcp(unsigned short *ptr, int length)
{
    int sum = 0;
    unsigned short answer = 0;
    unsigned short *w = ptr;
    int nleft = length;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}

// --- Helper function which transfer bytes to binary string for checksum XOR result check --- //
void ushort_to_binstr(unsigned short val, char *binstr) {
    for (int i = 15; i >= 0; i--) {
        binstr[15 - i] = ((val >> i) & 1) ? '1' : '0';
    }
    binstr[16] = '\0';
}

// Helper function for checksum verification
bool verify_correction(unsigned char *corrected_buffer, unsigned char *test_data,
                      int pdu_start, int payload_start, int payload_len, int checksum_cal_len) {
    int test_j = 0;
    memcpy(test_data + test_j, corrected_buffer + pdu_start + 12, 4); test_j += 4;
    memcpy(test_data + test_j, corrected_buffer + pdu_start + 16, 4); test_j += 4;
    test_data[test_j++] = 0x00;
    test_data[test_j++] = corrected_buffer[pdu_start + 9];
    memcpy(test_data + test_j, corrected_buffer + pdu_start + 24, 2); test_j += 2;
    memcpy(test_data + test_j, corrected_buffer + pdu_start + 20, 2); test_j += 2;
    memcpy(test_data + test_j, corrected_buffer + pdu_start + 22, 2); test_j += 2;
    memcpy(test_data + test_j, corrected_buffer + pdu_start + 24, 2); test_j += 2;
    test_data[test_j++] = 0x00; test_data[test_j++] = 0x00;
    memcpy(test_data + test_j, corrected_buffer + payload_start, payload_len);
    
    unsigned short test_result = checksum_pdcp((unsigned short*)test_data, checksum_cal_len);
    unsigned short test_checksum = ((test_result & 0xFF) << 8) | (test_result >> 8);
    unsigned short corrected_checksum_field = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
    
    return (test_checksum == corrected_checksum_field);
}

// Helper function for two block single zero pattern recovery
int two_block_single_zero_recovery(unsigned char *buffer, int size, int pdu_start, int payload_start,
                                   int payload_len, int checksum_cal_len, int* zero_bits, int* previous_block_lsbs,
                                   int num_zeros, int num_payload_words, int starting_recovery_count) {
    
    int recovery_count = 0;

    // Allocate working buffers
    unsigned char *corrected_buffer = (unsigned char*)malloc(size);
    unsigned char *test_data = (unsigned char*)calloc(checksum_cal_len, 1);
    
    LOG_W(PDCP, "Two block single zero recovery: %d zeros detected\n", num_zeros);
    for (int i = 0; i < num_zeros; i++) {
        LOG_W(PDCP, "Zero %d: zero_bit=%d, previous_block_lsb=%d\n", i, zero_bits[i], previous_block_lsbs[i]);
    }
    
    int checksum_phase = num_payload_words;
    
    if (num_zeros == 1) {
        // Case 1: One single zero - 2 scenarios
        int zero_bit = zero_bits[0];
        int prev_lsb = previous_block_lsbs[0];
        
        // Scenario 1: zero_bit as checksum error, prev_lsb as payload error
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << zero_bit);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << prev_lsb);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack two 6-bit entries: (word_index1:2bits + bit_index1:4bits) and (word_index2:2bits + bit_index2:4bits)
                    // For this scenario: checksum word (index 3) and payload word (index payload_word)
                    unsigned char first_entry = ((checksum_phase & 0x3) << 4) | (zero_bit & 0xF);  // checksum word index = 3 or checksum_phase
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (prev_lsb & 0xF);  // payload word index
                    
                    // Calculate bit offset for this recovery
                    // NEW - CORRECT
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset = total_bits_used % 8;
                    
                    // Pack first 6-bit entry
                    if (bit_offset <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset));
                        if (bit_offset > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset));
                    }
                    
                    // Update bit position for second entry
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset = total_bits_used % 8;
                    
                    // Pack second 6-bit entry
                    if (bit_offset <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset));
                        if (bit_offset > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset));
                    }
                    
                    recovery_count++;
                    
                    LOG_W(PDCP, "Recovery %d (1-zero s1): checksum_bit=%d, payload_word=%d, payload_bit=%d\n",
                          recovery_count, zero_bit, payload_word, prev_lsb);
                }
            }
        }
        
        // Scenario 2: Both zero_bit and prev_lsb as payload errors
        for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
            for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
                memcpy(corrected_buffer, buffer, size);
                
                // Flip first payload bit (zero_bit)
                int offset1 = payload_start + pw1 * 2;
                if (offset1 < size - 1) {
                    unsigned short val1 = (corrected_buffer[offset1] << 8) | corrected_buffer[offset1 + 1];
                    val1 ^= (1 << zero_bit);
                    corrected_buffer[offset1] = (val1 >> 8) & 0xFF;
                    corrected_buffer[offset1 + 1] = val1 & 0xFF;
                }
                
                // Flip second payload bit (prev_lsb)
                int offset2 = payload_start + pw2 * 2;
                if (offset2 < size - 1) {
                    unsigned short val2 = (corrected_buffer[offset2] << 8) | corrected_buffer[offset2 + 1];
                    val2 ^= (1 << prev_lsb);
                    corrected_buffer[offset2] = (val2 >> 8) & 0xFF;
                    corrected_buffer[offset2 + 1] = val2 & 0xFF;
                    
                    if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                        // Pack two 6-bit entries: (word_index1:2bits + bit_index1:4bits) and (word_index2:2bits + bit_index2:4bits)
                        // For this scenario: both are payload words (indices pw1 and pw2)
                        unsigned char first_entry = ((pw1 & 0x3) << 4) | (zero_bit & 0xF);  // payload word index pw1
                        unsigned char second_entry = ((pw2 & 0x3) << 4) | (prev_lsb & 0xF);  // payload word index pw2
                        
                        // Calculate bit offset for this recovery
                        // NEW - CORRECT
                        int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                        int byte_offset = 6 + (total_bits_used / 8);
                        int bit_offset = total_bits_used % 8;
                        
                        // Pack first 6-bit entry
                        if (bit_offset <= 2) {
                            buffer[size + byte_offset] |= (first_entry << (2 - bit_offset));
                            if (bit_offset > 0) {
                                buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (first_entry >> (bit_offset - 2));
                            buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset));
                        }
                        
                        // Update bit position for second entry
                        total_bits_used += 6;
                        byte_offset = 6 + (total_bits_used / 8);
                        bit_offset = total_bits_used % 8;
                        
                        // Pack second 6-bit entry
                        if (bit_offset <= 2) {
                            buffer[size + byte_offset] |= (second_entry << (2 - bit_offset));
                            if (bit_offset > 0) {
                                buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (second_entry >> (bit_offset - 2));
                            buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset));
                        }
                        
                        recovery_count++;
                        
                        LOG_W(PDCP, "Recovery %d (1-zero s2): payload_word1=%d, payload_bit1=%d, payload_word2=%d, payload_bit2=%d\n",
                              recovery_count, pw1, zero_bit, pw2, prev_lsb);
                    }
                }
            }
        }
        
    } else if (num_zeros == 2) {
        // Case 2: Two single zeros - 4 scenarios
        int zero1 = zero_bits[0];
        int prev1 = previous_block_lsbs[0];
        int zero2 = zero_bits[1];
        int prev2 = previous_block_lsbs[1];
        
        // Scenario 1: zero1 as checksum, prev1 as payload
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit (zero1)
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << zero1);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit (prev1)
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << prev1);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack two 6-bit entries: (word_index1:2bits + bit_index1:4bits) and (word_index2:2bits + bit_index2:4bits)
                    // For this scenario: checksum word (index 3) and payload word (index payload_word)
                    unsigned char first_entry = ((checksum_phase & 0x3) << 4) | (zero1 & 0xF);  // checksum word index = 3
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (prev1 & 0xF);  // payload word index
                    
                    // Calculate bit offset for this recovery
                    // NEW - CORRECT
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset = total_bits_used % 8;
                    
                    // Pack first 6-bit entry
                    if (bit_offset <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset));
                        if (bit_offset > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset));
                    }
                    
                    // Update bit position for second entry
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset = total_bits_used % 8;
                    
                    // Pack second 6-bit entry
                    if (bit_offset <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset));
                        if (bit_offset > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset));
                    }
                    
                    recovery_count++;
                    
                    LOG_W(PDCP, "Recovery %d (2-zero s1): zero1=%d as checksum, prev1=%d as payload\n",
                          recovery_count, zero1, prev1);
                }
            }
        }
        
        // Scenario 2: Both zero1 and prev1 as payload errors
        for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
            for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
                memcpy(corrected_buffer, buffer, size);
                
                // Flip payload bit 1 (zero1)
                int offset1 = payload_start + pw1 * 2;
                if (offset1 < size - 1) {
                    unsigned short val1 = (corrected_buffer[offset1] << 8) | corrected_buffer[offset1 + 1];
                    val1 ^= (1 << zero1);
                    corrected_buffer[offset1] = (val1 >> 8) & 0xFF;
                    corrected_buffer[offset1 + 1] = val1 & 0xFF;
                }
                
                // Flip payload bit 2 (prev1)
                int offset2 = payload_start + pw2 * 2;
                if (offset2 < size - 1) {
                    unsigned short val2 = (corrected_buffer[offset2] << 8) | corrected_buffer[offset2 + 1];
                    val2 ^= (1 << prev1);
                    corrected_buffer[offset2] = (val2 >> 8) & 0xFF;
                    corrected_buffer[offset2 + 1] = val2 & 0xFF;
                    
                    if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                        // Pack two 6-bit entries: (word_index1:2bits + bit_index1:4bits) and (word_index2:2bits + bit_index2:4bits)
                        // For this scenario: both are payload words (indices pw1 and pw2)
                        unsigned char first_entry = ((pw1 & 0x3) << 4) | (zero1 & 0xF);  // payload word index pw1
                        unsigned char second_entry = ((pw2 & 0x3) << 4) | (prev1 & 0xF);  // payload word index pw2
                        
                        // Calculate bit offset for this recovery
                        // NEW - CORRECT
                        int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                        int byte_offset = 6 + (total_bits_used / 8);
                        int bit_offset = total_bits_used % 8;
                        
                        // Pack first 6-bit entry
                        if (bit_offset <= 2) {
                            buffer[size + byte_offset] |= (first_entry << (2 - bit_offset));
                            if (bit_offset > 0) {
                                buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (first_entry >> (bit_offset - 2));
                            buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset));
                        }
                        
                        // Update bit position for second entry
                        total_bits_used += 6;
                        byte_offset = 6 + (total_bits_used / 8);
                        bit_offset = total_bits_used % 8;
                        
                        // Pack second 6-bit entry
                        if (bit_offset <= 2) {
                            buffer[size + byte_offset] |= (second_entry << (2 - bit_offset));
                            if (bit_offset > 0) {
                                buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (second_entry >> (bit_offset - 2));
                            buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset));
                        }
                        
                        recovery_count++;
                        
                        LOG_W(PDCP, "Recovery %d (2-zero s2): zero1=%d and prev1=%d both as payload\n",
                              recovery_count, zero1, prev1);
                    }
                }
            }
        }
        
        // Scenario 3: zero2 as checksum, prev2 as payload
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit (zero2)
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << zero2);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit (prev2)
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << prev2);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack two 6-bit entries: (word_index1:2bits + bit_index1:4bits) and (word_index2:2bits + bit_index2:4bits)
                    // For this scenario: checksum word (index 3) and payload word (index payload_word)
                    unsigned char first_entry = ((checksum_phase & 0x3) << 4) | (zero2 & 0xF);  // checksum word index = 3
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (prev2 & 0xF);  // payload word index
                    
                    // Calculate bit offset for this recovery
                    // NEW - CORRECT
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset = total_bits_used % 8;
                    
                    // Pack first 6-bit entry
                    if (bit_offset <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset));
                        if (bit_offset > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset));
                    }
                    
                    // Update bit position for second entry
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset = total_bits_used % 8;
                    
                    // Pack second 6-bit entry
                    if (bit_offset <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset));
                        if (bit_offset > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset));
                    }
                    
                    recovery_count++;
                    
                    LOG_W(PDCP, "Recovery %d (2-zero s3): zero2=%d as checksum, prev2=%d as payload\n",
                          recovery_count, zero2, prev2);
                }
            }
        }
        
        // Scenario 4: Both zero2 and prev2 as payload errors
        for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
            for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
                memcpy(corrected_buffer, buffer, size);
                
                // Flip payload bit 1 (zero2)
                int offset1 = payload_start + pw1 * 2;
                if (offset1 < size - 1) {
                    unsigned short val1 = (corrected_buffer[offset1] << 8) | corrected_buffer[offset1 + 1];
                    val1 ^= (1 << zero2);
                    corrected_buffer[offset1] = (val1 >> 8) & 0xFF;
                    corrected_buffer[offset1 + 1] = val1 & 0xFF;
                }
                
                // Flip payload bit 2 (prev2)
                int offset2 = payload_start + pw2 * 2;
                if (offset2 < size - 1) {
                    unsigned short val2 = (corrected_buffer[offset2] << 8) | corrected_buffer[offset2 + 1];
                    val2 ^= (1 << prev2);
                    corrected_buffer[offset2] = (val2 >> 8) & 0xFF;
                    corrected_buffer[offset2 + 1] = val2 & 0xFF;
                                        
                    if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                        // Pack two 6-bit entries: (word_index1:2bits + bit_index1:4bits) and (word_index2:2bits + bit_index2:4bits)
                        // For this scenario: both are payload words (indices pw1 and pw2)
                        unsigned char first_entry = ((pw1 & 0x3) << 4) | (zero2 & 0xF);  // payload word index pw1
                        unsigned char second_entry = ((pw2 & 0x3) << 4) | (prev2 & 0xF);  // payload word index pw2
                        
                        // Calculate bit offset for this recovery
                        // NEW - CORRECT
                        int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                        int byte_offset = 6 + (total_bits_used / 8);
                        int bit_offset = total_bits_used % 8;
                        
                        // Pack first 6-bit entry
                        if (bit_offset <= 2) {
                            buffer[size + byte_offset] |= (first_entry << (2 - bit_offset));
                            if (bit_offset > 0) {
                                buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (first_entry >> (bit_offset - 2));
                            buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset));
                        }
                        
                        // Update bit position for second entry
                        total_bits_used += 6;
                        byte_offset = 6 + (total_bits_used / 8);
                        bit_offset = total_bits_used % 8;
                        
                        // Pack second 6-bit entry
                        if (bit_offset <= 2) {
                            buffer[size + byte_offset] |= (second_entry << (2 - bit_offset));
                            if (bit_offset > 0) {
                                buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (second_entry >> (bit_offset - 2));
                            buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset));
                        }
                        
                        recovery_count++;
                        
                        LOG_W(PDCP, "Recovery %d (2-zero s4): zero2=%d and prev2=%d both as payload\n",
                              recovery_count, zero2, prev2);
                    }
                }
            }
        }
    }
    
    // Clean up - safe cleanup with NULL checks and pointer reset
    if (corrected_buffer) {
        free(corrected_buffer);
        corrected_buffer = NULL;
    }
    if (test_data) {
        free(test_data);
        test_data = NULL;
    }
    
    LOG_W(PDCP, "Two block single zero recovery complete: found %d valid recoveries\n", recovery_count);
    return recovery_count;
}

// Helper function for two block recovery when one block has length 1, we would conside that block correspond to checksum flip here
int two_block_single_one_checksum_recovery(unsigned char *buffer, int size, int pdu_start, int payload_start,
                                          int payload_len, int checksum_cal_len, int* lsb_indices, int* msb_indices,
                                          int block_count, int min_blocks, int max_blocks, int num_payload_words, int starting_recovery_count) {
    int recovery_count = 0;
    
    // This function handles two-block cases where one block has length 1 (checksum bit)
    // and the other block's LSB is treated as payload bit
    if (!(min_blocks == 2 || (min_blocks == 1 && max_blocks == 2))) {
        LOG_W(PDCP, "Single-one checksum recovery called with unsupported block combination: min=%d, max=%d\n", min_blocks, max_blocks);
        return 0;
    }
    
    // Determine if we should use wraparound interpretation
    bool use_wraparound = (min_blocks == 2 && max_blocks == 3) && (block_count >= 2) &&
                         (lsb_indices[0] == 0) &&
                         (msb_indices[block_count - 1] == 15);
    
    // Determine block parameters for recovery
    int block0_lsb, block0_len, block1_lsb, block1_len;
    
    if (use_wraparound) {
        // Wraparound case: merge first and last blocks
        block0_lsb = lsb_indices[block_count - 1];  // LSB from LAST block
        block0_len = (msb_indices[0] - lsb_indices[0] + 1) +
                     (msb_indices[block_count - 1] - lsb_indices[block_count - 1] + 1);
        block1_lsb = lsb_indices[1];  // Middle block
        block1_len = msb_indices[1] - lsb_indices[1] + 1;
        LOG_W(PDCP, "Wraparound single-one recovery: wrap_len=%d (bits %d-%d wrapping to 0-%d), middle_len=%d (bits %d-%d)\n",
              block0_len, lsb_indices[block_count - 1], msb_indices[block_count - 1], msb_indices[0],
              block1_len, block1_lsb, msb_indices[1]);
    } else {
        // Normal case: use first two blocks (non-wraparound interpretation)
        if (block_count < 2) {
            LOG_W(PDCP, "Need at least 2 blocks for recovery, found %d\n", block_count);
            return 0;
        }
        block0_lsb = lsb_indices[0];
        block0_len = msb_indices[0] - lsb_indices[0] + 1;
        block1_lsb = lsb_indices[1];
        block1_len = msb_indices[1] - lsb_indices[1] + 1;
        LOG_W(PDCP, "Normal single-one recovery: block0_len=%d (bits %d-%d), block1_len=%d (bits %d-%d)\n",
              block0_len, block0_lsb, msb_indices[0], block1_len, block1_lsb, msb_indices[1]);
    }
    
    // Check if we can do recovery (need at least one length-1 block)
    if (block0_len != 1 && block1_len != 1) {
        LOG_W(PDCP, "No length-1 blocks found (block0=%d, block1=%d) - cannot attempt single-one checksum recovery\n",
              block0_len, block1_len);
        return 0;
    }
    
    LOG_W(PDCP, "At least one length-1 block found - proceeding with single-one checksum recovery\n");
    
    // Allocate working buffers
    unsigned char *corrected_buffer = (unsigned char*)malloc(size);
    unsigned char *test_data = (unsigned char*)calloc(checksum_cal_len, 1);
    
    // Try all 4 recovery scenarios
    for (int scenario = 0; scenario < 4 && recovery_count < 16; scenario++) {
        int checksum_bit = -1, payload_bit = -1;
        bool valid_scenario = false;
        
        // Determine which scenario to test
        if (scenario == 0 && block0_len == 1 && block1_len > 1) {
            // Block 0 (len=1) -> checksum, Block 1 LSB -> payload
            checksum_bit = block0_lsb;
            payload_bit = block1_lsb;
            valid_scenario = true;
            LOG_W(PDCP, "Scenario %d: Block 0 (len=1) -> checksum bit %d, Block 1 (len=%d) LSB -> payload bit %d\n",
                  scenario, checksum_bit, block1_len, payload_bit);
        } else if (scenario == 1 && block1_len == 1 && block0_len > 1) {
            // Block 1 (len=1) -> checksum, Block 0 LSB -> payload  
            checksum_bit = block1_lsb;
            payload_bit = block0_lsb;
            valid_scenario = true;
            LOG_W(PDCP, "Scenario %d: Block 1 (len=1) -> checksum bit %d, Block 0 (len=%d) LSB -> payload bit %d\n",
                  scenario, checksum_bit, block0_len, payload_bit);
        } else if (scenario == 2 && block0_len == 1 && block1_len == 1) {
            // Block 0 -> checksum, Block 1 -> payload
            checksum_bit = block0_lsb;
            payload_bit = block1_lsb;
            valid_scenario = true;
            LOG_W(PDCP, "Scenario %d: Block 0 -> checksum bit %d, Block 1 -> payload bit %d\n",
                  scenario, checksum_bit, payload_bit);
        } else if (scenario == 3 && block0_len == 1 && block1_len == 1) {
            // Block 1 -> checksum, Block 0 -> payload
            checksum_bit = block1_lsb;
            payload_bit = block0_lsb;
            valid_scenario = true;
            LOG_W(PDCP, "Scenario %d: Block 1 -> checksum bit %d, Block 0 -> payload bit %d\n",
                  scenario, checksum_bit, payload_bit);
        }
        
        if (!valid_scenario) continue;
        
        // Test this scenario in each payload word
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            // Create test buffer
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << checksum_bit);  // Flip bit
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << payload_bit);  // Flip bit
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                // Verify correction using helper function
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack two 6-bit entries: checksum word (index 3) and payload word
                    unsigned char first_entry = ((3 & 0x3) << 4) | (checksum_bit & 0xF);  // checksum word index = 3
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (payload_bit & 0xF);  // payload word index
                    
                    // Calculate bit offset for this recovery
                    // NEW - CORRECT
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset_pos = total_bits_used % 8;
                    
                    // Pack first 6-bit entry
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                    }
                    
                    // Update bit position for second entry
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset_pos = total_bits_used % 8;
                    
                    // Pack second 6-bit entry
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                    }
                    
                    recovery_count++;
                    LOG_W(PDCP, "Recovery %d: scenario=%d, checksum_bit=%d, payload_word=%d, payload_bit=%d\n",
                          recovery_count, scenario, checksum_bit, payload_word, payload_bit);
                }
            }
        }
    }
    
    free(corrected_buffer);
    free(test_data);
    LOG_W(PDCP, "Single-one checksum recovery complete: found %d valid recoveries\n", recovery_count);
    return recovery_count;
}

// Helper function for two block general recovery (both LSBs as payload bits)
int two_block_general_recovery(unsigned char *buffer, int size, int pdu_start, int payload_start,
                              int payload_len, int checksum_cal_len, int* lsb_indices, int* msb_indices,
                              int block_count, int min_blocks, int max_blocks, int num_payload_words, int starting_recovery_count) {
    int recovery_count = 0;
    
    // This function handles two-block cases where both blocks' LSBs are treated as payload bits
    // It's called after single-one checksum recovery fails or as a fallback
    if (!(min_blocks == 2 || (min_blocks == 1 && max_blocks == 2))) {
        LOG_W(PDCP, "General recovery called with unsupported block combination: min=%d, max=%d\n", min_blocks, max_blocks);
        return 0;
    }
    
    // Determine if we should use wraparound interpretation
    bool use_wraparound = (min_blocks == 2 && max_blocks == 3) && (block_count >= 2) &&
                         (lsb_indices[0] == 0) &&
                         (msb_indices[block_count - 1] == 15);
    
    // Determine block parameters for recovery
    int block0_lsb, block0_len, block1_lsb, block1_len;
    
    if (use_wraparound) {
        // Wraparound case: merge first and last blocks
        block0_lsb = lsb_indices[block_count - 1];  // LSB from LAST block
        block0_len = (msb_indices[0] - lsb_indices[0] + 1) +
                     (msb_indices[block_count - 1] - lsb_indices[block_count - 1] + 1);
        block1_lsb = lsb_indices[1];  // Middle block
        block1_len = msb_indices[1] - lsb_indices[1] + 1;
        LOG_W(PDCP, "Wraparound general recovery: wrap_len=%d (LSB=%d), middle_len=%d (LSB=%d)\n",
              block0_len, block0_lsb, block1_len, block1_lsb);
    } else {
        // Normal case: use first two blocks (non-wraparound interpretation)
        if (block_count < 2) {
            LOG_W(PDCP, "Need at least 2 blocks for recovery, found %d\n", block_count);
            return 0;
        }
        block0_lsb = lsb_indices[0];
        block0_len = msb_indices[0] - lsb_indices[0] + 1;
        block1_lsb = lsb_indices[1];
        block1_len = msb_indices[1] - lsb_indices[1] + 1;
        LOG_W(PDCP, "Normal general recovery: block0_len=%d (LSB=%d), block1_len=%d (LSB=%d)\n",
              block0_len, block0_lsb, block1_len, block1_lsb);
    }
    
    LOG_W(PDCP, "Proceeding with general recovery using payload bits %d and %d\n", block0_lsb, block1_lsb);
    
    // Allocate working buffers
    unsigned char *corrected_buffer = (unsigned char*)malloc(size);
    unsigned char *test_data = (unsigned char*)calloc(checksum_cal_len, 1);
    
    // Try all combinations of payload word positions for both bit flips
    // Since both LSBs are treated as payload bits, we test all payload word combinations
    for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
        for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
            // Create test buffer
            memcpy(corrected_buffer, buffer, size);
            
            // Flip first payload bit (block0_lsb in payload word pw1)
            int offset1 = payload_start + pw1 * 2;
            if (offset1 < size - 1) {
                unsigned short val1 = (corrected_buffer[offset1] << 8) | corrected_buffer[offset1 + 1];
                val1 ^= (1 << block0_lsb);  // Flip bit
                corrected_buffer[offset1] = (val1 >> 8) & 0xFF;
                corrected_buffer[offset1 + 1] = val1 & 0xFF;
            }
            
            // Flip second payload bit (block1_lsb in payload word pw2)
            int offset2 = payload_start + pw2 * 2;
            if (offset2 < size - 1) {
                unsigned short val2 = (corrected_buffer[offset2] << 8) | corrected_buffer[offset2 + 1];
                val2 ^= (1 << block1_lsb);  // Flip bit
                corrected_buffer[offset2] = (val2 >> 8) & 0xFF;
                corrected_buffer[offset2 + 1] = val2 & 0xFF;
                
                // Verify correction using helper function
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack two 6-bit entries: both are payload words
                    unsigned char first_entry = ((pw1 & 0x3) << 4) | (block0_lsb & 0xF);  // payload word index pw1
                    unsigned char second_entry = ((pw2 & 0x3) << 4) | (block1_lsb & 0xF);  // payload word index pw2
                    
                    // Calculate bit offset for this recovery
                    // NEW - CORRECT
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;  // Each recovery uses 12 bits (two 6-bit entries)
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset_pos = total_bits_used % 8;
                    
                    // Pack first 6-bit entry
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                    }
                    
                    // Update bit position for second entry
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset_pos = total_bits_used % 8;
                    
                    // Pack second 6-bit entry
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                    }
                    
                    recovery_count++;
                    LOG_W(PDCP, "Recovery %d: payload_word1=%d, payload_bit1=%d, payload_word2=%d, payload_bit2=%d\n",
                          recovery_count, pw1, block0_lsb, pw2, block1_lsb);
                }
            }
        }
    }
    
    free(corrected_buffer);
    free(test_data);
    LOG_W(PDCP, "General recovery complete: found %d valid recoveries\n", recovery_count);
    return recovery_count;
}

// Helper function for one block checksum recovery (handles both normal and wrap-around cases)
int one_block_checksum_recovery(unsigned char *buffer, int size, int pdu_start, int payload_start,
                                int payload_len, int checksum_cal_len, int* lsb_indices, int* msb_indices,
                                int block_count, bool is_wraparound, int num_payload_words, int starting_recovery_count) {
    int recovery_count = 0;
    int lsb_index, msb_index, block_len;
    
    if (is_wraparound && block_count == 2) {
        // Wrap-around case: merge first and last blocks
        // The "block" wraps from high bits (last block) to low bits (first block)
        lsb_index = lsb_indices[1];  // LSB from the high-bit block
        msb_index = msb_indices[0];  // MSB from the low-bit block
        block_len = (msb_indices[0] - lsb_indices[0] + 1) + (msb_indices[1] - lsb_indices[1] + 1);
        LOG_W(PDCP, "Wrap-around one block recovery: LSB=%d (high block), MSB=%d (low block), Total length=%d\n", 
              lsb_index, msb_index, block_len);
    } else {
        // Normal case: single contiguous block
        lsb_index = lsb_indices[0];
        msb_index = msb_indices[0];
        block_len = msb_index - lsb_index + 1;
        LOG_W(PDCP, "Normal one block recovery: LSB=%d, MSB=%d, Length=%d\n", lsb_index, msb_index, block_len);
    }
    
    // Allocate working buffers
    unsigned char *corrected_buffer = (unsigned char*)malloc(size);
    unsigned char *test_data = (unsigned char*)calloc(checksum_cal_len, 1);
    
    // Scenario 1: Block's LSB as checksum, LSB+1 as payload
    if (recovery_count < 16) {
        int checksum_bit = lsb_index;
        int payload_bit = (lsb_index + 1) % 16;  // Handle wrap-around for bit indices
        
        LOG_W(PDCP, "Scenario 1: LSB=%d as checksum, LSB+1=%d as payload\n", checksum_bit, payload_bit);
        
        // Try all payload words
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << checksum_bit);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << payload_bit);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack recovery data
                    unsigned char first_entry = ((3 & 0x3) << 4) | (checksum_bit & 0xF);
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (payload_bit & 0xF);
                    
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset_pos = total_bits_used % 8;
                    
                    // Pack first 6-bit entry
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                    }
                    
                    // Update bit position for second entry
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset_pos = total_bits_used % 8;
                    
                    // Pack second 6-bit entry
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                    }
                    
                    recovery_count++;
                    LOG_W(PDCP, "Recovery %d (scenario 1): checksum_bit=%d, payload_word=%d, payload_bit=%d\n",
                          recovery_count, checksum_bit, payload_word, payload_bit);
                }
            }
        }
    }
    
    // Scenario 2: Block's MSB as checksum, LSB as payload
    if (recovery_count < 16) {
        int checksum_bit = msb_index;
        int payload_bit = lsb_index;
        
        LOG_W(PDCP, "Scenario 2: MSB=%d as checksum, LSB=%d as payload\n", checksum_bit, payload_bit);
        
        // Try all payload words
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << checksum_bit);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << payload_bit);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack recovery data (same as scenario 1)
                    unsigned char first_entry = ((3 & 0x3) << 4) | (checksum_bit & 0xF);
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (payload_bit & 0xF);
                    
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset_pos = total_bits_used % 8;
                    
                    // Pack entries (same bit packing logic as scenario 1)
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                    }
                    
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset_pos = total_bits_used % 8;
                    
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                    }
                    
                    recovery_count++;
                    LOG_W(PDCP, "Recovery %d (scenario 2): checksum_bit=%d, payload_word=%d, payload_bit=%d\n",
                          recovery_count, checksum_bit, payload_word, payload_bit);
                }
            }
        }
    }
    
    // Scenario 3: MSB+1 as checksum, LSB as payload
    if (recovery_count < 16) {
        int checksum_bit = (msb_index + 1) % 16;  // Handle wrap-around
        int payload_bit = lsb_index;
        
        LOG_W(PDCP, "Scenario 3: MSB+1=%d as checksum, LSB=%d as payload\n", checksum_bit, payload_bit);
        
        // Try all payload words (same logic as scenarios 1 and 2)
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << checksum_bit);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << payload_bit);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack recovery data (same packing logic)
                    unsigned char first_entry = ((3 & 0x3) << 4) | (checksum_bit & 0xF);
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (payload_bit & 0xF);
                    
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset_pos = total_bits_used % 8;
                    
                    // Pack entries (same bit packing logic)
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                    }
                    
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset_pos = total_bits_used % 8;
                    
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                    }
                    
                    recovery_count++;
                    LOG_W(PDCP, "Recovery %d (scenario 3): checksum_bit=%d, payload_word=%d, payload_bit=%d\n",
                          recovery_count, checksum_bit, payload_word, payload_bit);
                }
            }
        }
    }
    
    // Scenario 4: LSB-1 as both checksum and payload bit
    if (recovery_count < 16) {
        int bit_index = (lsb_index - 1 + 16) % 16;  // Handle wrap-around (e.g., if lsb_index=0, bit_index=15)
        
        LOG_W(PDCP, "Scenario 4: LSB-1=%d as both checksum and payload bit\n", bit_index);
        
        // Try all payload words (same logic)
        for (int payload_word = 0; payload_word < num_payload_words && recovery_count < 16; payload_word++) {
            memcpy(corrected_buffer, buffer, size);
            
            // Flip checksum bit
            unsigned short checksum_val = (corrected_buffer[pdu_start + 26] << 8) | corrected_buffer[pdu_start + 27];
            checksum_val ^= (1 << bit_index);
            corrected_buffer[pdu_start + 26] = (checksum_val >> 8) & 0xFF;
            corrected_buffer[pdu_start + 27] = checksum_val & 0xFF;
            
            // Flip payload bit (same bit index)
            int payload_offset = payload_start + payload_word * 2;
            if (payload_offset < size - 1) {
                unsigned short payload_val = (corrected_buffer[payload_offset] << 8) | corrected_buffer[payload_offset + 1];
                payload_val ^= (1 << bit_index);
                corrected_buffer[payload_offset] = (payload_val >> 8) & 0xFF;
                corrected_buffer[payload_offset + 1] = payload_val & 0xFF;
                
                if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                    // Pack recovery data (same packing logic)
                    unsigned char first_entry = ((3 & 0x3) << 4) | (bit_index & 0xF);
                    unsigned char second_entry = ((payload_word & 0x3) << 4) | (bit_index & 0xF);
                    
                    int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                    int byte_offset = 6 + (total_bits_used / 8);
                    int bit_offset_pos = total_bits_used % 8;
                    
                    // Pack entries (same bit packing logic)
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                    }
                    
                    total_bits_used += 6;
                    byte_offset = 6 + (total_bits_used / 8);
                    bit_offset_pos = total_bits_used % 8;
                    
                    if (bit_offset_pos <= 2) {
                        buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                        if (bit_offset_pos > 0) {
                            buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                        buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                    }
                    
                    recovery_count++;
                    LOG_W(PDCP, "Recovery %d (scenario 4): checksum_bit=%d, payload_word=%d, payload_bit=%d\n",
                          recovery_count, bit_index, payload_word, bit_index);
                }
            }
        }
    }
    
    free(corrected_buffer);
    free(test_data);
    LOG_W(PDCP, "One block checksum recovery complete: found %d valid recoveries\n", recovery_count);
    return recovery_count;
}

// Helper function for one block payload recovery (both flips in payload)
int one_block_payload_recovery(unsigned char *buffer, int size, int pdu_start, int payload_start,
                               int payload_len, int checksum_cal_len, int* lsb_indices, int* msb_indices,
                               int block_count, bool is_wraparound, int num_payload_words, int starting_recovery_count) {
    int recovery_count = 0;
    int lsb_index, msb_index, block_len;
    
    if (is_wraparound && block_count == 2) {
        // Wrap-around case: merge first and last blocks
        lsb_index = lsb_indices[1];  // LSB from the high-bit block
        msb_index = msb_indices[0];  // MSB from the low-bit block
        block_len = (msb_indices[0] - lsb_indices[0] + 1) + (msb_indices[1] - lsb_indices[1] + 1);
        LOG_W(PDCP, "Wrap-around one block payload recovery: LSB=%d (high block), MSB=%d (low block), Total length=%d\n", 
              lsb_index, msb_index, block_len);
    } else {
        // Normal case: single contiguous block
        lsb_index = lsb_indices[0];
        msb_index = msb_indices[0];
        block_len = msb_index - lsb_index + 1;
        LOG_W(PDCP, "Normal one block payload recovery: LSB=%d, MSB=%d, Length=%d\n", lsb_index, msb_index, block_len);
    }
    
    // Allocate working buffers
    unsigned char *corrected_buffer = (unsigned char*)malloc(size);
    unsigned char *test_data = (unsigned char*)calloc(checksum_cal_len, 1);
    
    // Scenario 1: Block's LSB and MSB+1 as payload bits
    if (recovery_count < 16) {
        int payload_bit1 = lsb_index;
        int payload_bit2 = (msb_index + 1) % 16;  // Handle wrap-around
        
        LOG_W(PDCP, "Scenario 1: LSB=%d and MSB+1=%d as payload bits\n", payload_bit1, payload_bit2);
        
        // Try all combinations of payload words
        for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
            for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
                memcpy(corrected_buffer, buffer, size);
                
                // Flip first payload bit
                int offset1 = payload_start + pw1 * 2;
                if (offset1 < size - 1) {
                    unsigned short val1 = (corrected_buffer[offset1] << 8) | corrected_buffer[offset1 + 1];
                    val1 ^= (1 << payload_bit1);
                    corrected_buffer[offset1] = (val1 >> 8) & 0xFF;
                    corrected_buffer[offset1 + 1] = val1 & 0xFF;
                }
                
                // Flip second payload bit
                int offset2 = payload_start + pw2 * 2;
                if (offset2 < size - 1) {
                    unsigned short val2 = (corrected_buffer[offset2] << 8) | corrected_buffer[offset2 + 1];
                    val2 ^= (1 << payload_bit2);
                    corrected_buffer[offset2] = (val2 >> 8) & 0xFF;
                    corrected_buffer[offset2 + 1] = val2 & 0xFF;
                    
                    if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                        // Pack recovery data
                        unsigned char first_entry = ((pw1 & 0x3) << 4) | (payload_bit1 & 0xF);
                        unsigned char second_entry = ((pw2 & 0x3) << 4) | (payload_bit2 & 0xF);
                        
                        int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                        int byte_offset = 6 + (total_bits_used / 8);
                        int bit_offset_pos = total_bits_used % 8;
                        
                        // Pack first 6-bit entry
                        if (bit_offset_pos <= 2) {
                            buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                            if (bit_offset_pos > 0) {
                                buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                            buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                        }
                        
                        // Update bit position for second entry
                        total_bits_used += 6;
                        byte_offset = 6 + (total_bits_used / 8);
                        bit_offset_pos = total_bits_used % 8;
                        
                        // Pack second 6-bit entry
                        if (bit_offset_pos <= 2) {
                            buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                            if (bit_offset_pos > 0) {
                                buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                            buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                        }
                        
                        recovery_count++;
                        LOG_W(PDCP, "Recovery %d (scenario 1): payload_word1=%d, payload_bit1=%d, payload_word2=%d, payload_bit2=%d\n",
                              recovery_count, pw1, payload_bit1, pw2, payload_bit2);
                    }
                }
            }
        }
    }
    
    // Scenario 2: LSB-1 as both payload bits (same bit in two different payload words)
    if (recovery_count < 16) {
        int payload_bit = (lsb_index - 1 + 16) % 16;  // Handle wrap-around
        
        LOG_W(PDCP, "Scenario 2: LSB-1=%d as both payload bits\n", payload_bit);
        
        // Try all combinations of payload words
        for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
            for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
                if (pw1 == pw2) continue;  // Must be different payload words
                
                memcpy(corrected_buffer, buffer, size);
                
                // Flip first payload bit
                int offset1 = payload_start + pw1 * 2;
                if (offset1 < size - 1) {
                    unsigned short val1 = (corrected_buffer[offset1] << 8) | corrected_buffer[offset1 + 1];
                    val1 ^= (1 << payload_bit);
                    corrected_buffer[offset1] = (val1 >> 8) & 0xFF;
                    corrected_buffer[offset1 + 1] = val1 & 0xFF;
                }
                
                // Flip second payload bit (same bit index, different word)
                int offset2 = payload_start + pw2 * 2;
                if (offset2 < size - 1) {
                    unsigned short val2 = (corrected_buffer[offset2] << 8) | corrected_buffer[offset2 + 1];
                    val2 ^= (1 << payload_bit);
                    corrected_buffer[offset2] = (val2 >> 8) & 0xFF;
                    corrected_buffer[offset2 + 1] = val2 & 0xFF;
                    
                    if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                        // Pack recovery data
                        unsigned char first_entry = ((pw1 & 0x3) << 4) | (payload_bit & 0xF);
                        unsigned char second_entry = ((pw2 & 0x3) << 4) | (payload_bit & 0xF);
                        
                        int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                        int byte_offset = 6 + (total_bits_used / 8);
                        int bit_offset_pos = total_bits_used % 8;
                        
                        // Pack first 6-bit entry
                        if (bit_offset_pos <= 2) {
                            buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                            if (bit_offset_pos > 0) {
                                buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                            buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                        }
                        
                        // Update bit position for second entry
                        total_bits_used += 6;
                        byte_offset = 6 + (total_bits_used / 8);
                        bit_offset_pos = total_bits_used % 8;
                        
                        // Pack second 6-bit entry
                        if (bit_offset_pos <= 2) {
                            buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                            if (bit_offset_pos > 0) {
                                buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                            }
                        } else {
                            buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                            buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                        }
                        
                        recovery_count++;
                        LOG_W(PDCP, "Recovery %d (scenario 2): payload_word1=%d, payload_bit1=%d, payload_word2=%d, payload_bit2=%d\n",
                              recovery_count, pw1, payload_bit, pw2, payload_bit);
                    }
                }
            }
        }
    }
    
    // Scenario 3: Concatenated chains - LSB as one flip, find the other through intermediate XOR
    if (recovery_count < 16) {
        int first_payload_bit = lsb_index;
        
        LOG_W(PDCP, "Scenario 3: LSB=%d as first payload bit, searching for concatenated second bit\n", first_payload_bit);
        
        // Try all payload words for the first bit
        for (int pw1 = 0; pw1 < num_payload_words && recovery_count < 16; pw1++) {
            // Create intermediate buffer with first bit flipped
            unsigned char *intermediate_buffer = (unsigned char*)malloc(size);
            memcpy(intermediate_buffer, buffer, size);
            
            // Flip first payload bit
            int offset1 = payload_start + pw1 * 2;
            if (offset1 < size - 1) {
                unsigned short val1 = (intermediate_buffer[offset1] << 8) | intermediate_buffer[offset1 + 1];
                val1 ^= (1 << first_payload_bit);
                intermediate_buffer[offset1] = (val1 >> 8) & 0xFF;
                intermediate_buffer[offset1 + 1] = val1 & 0xFF;
                
                // Use the same checksum calculation method as verify_correction()
                unsigned char *temp_test_data = (unsigned char*)calloc(checksum_cal_len, 1);
                int test_j = 0;
                
                // Build the same data structure as in verify_correction()
                memcpy(temp_test_data + test_j, intermediate_buffer + pdu_start + 12, 4); test_j += 4;
                memcpy(temp_test_data + test_j, intermediate_buffer + pdu_start + 16, 4); test_j += 4;
                temp_test_data[test_j++] = 0x00;
                temp_test_data[test_j++] = intermediate_buffer[pdu_start + 9];
                memcpy(temp_test_data + test_j, intermediate_buffer + pdu_start + 24, 2); test_j += 2;
                memcpy(temp_test_data + test_j, intermediate_buffer + pdu_start + 20, 2); test_j += 2;
                memcpy(temp_test_data + test_j, intermediate_buffer + pdu_start + 22, 2); test_j += 2;
                memcpy(temp_test_data + test_j, intermediate_buffer + pdu_start + 24, 2); test_j += 2;
                temp_test_data[test_j++] = 0x00; temp_test_data[test_j++] = 0x00;
                memcpy(temp_test_data + test_j, intermediate_buffer + payload_start, payload_len);
                
                // Calculate checksum using the same method
                unsigned short test_result = checksum_pdcp((unsigned short*)temp_test_data, checksum_cal_len);
                unsigned short intermediate_checksum = ((test_result & 0xFF) << 8) | (test_result >> 8);
                
                unsigned short received_checksum = (buffer[pdu_start + 26] << 8) | buffer[pdu_start + 27];
                unsigned short new_xor = intermediate_checksum ^ received_checksum;
                
                LOG_W(PDCP, "Intermediate XOR after flipping bit %d: 0x%04X\n", first_payload_bit, new_xor);
                
                if (new_xor != 0) {
                    // Find contiguous 1s blocks in new XOR
                    int new_lsb_indices[16], new_msb_indices[16];
                    int new_block_count = 0;
                    
                    // Extract blocks from new_xor
                    bool in_block = false;
                    for (int bit = 0; bit < 16; bit++) {
                        bool bit_set = (new_xor >> bit) & 1;
                        if (bit_set && !in_block) {
                            // Start of new block
                            new_lsb_indices[new_block_count] = bit;
                            in_block = true;
                        } else if (!bit_set && in_block) {
                            // End of current block
                            new_msb_indices[new_block_count] = bit - 1;
                            new_block_count++;
                            in_block = false;
                        }
                    }
                    if (in_block) {
                        // Block continues to bit 15
                        new_msb_indices[new_block_count] = 15;
                        new_block_count++;
                    }
                    
                    LOG_W(PDCP, "New XOR has %d blocks\n", new_block_count);
                    for (int i = 0; i < new_block_count; i++) {
                        LOG_W(PDCP, "New block %d: LSB=%d, MSB=%d\n", i, new_lsb_indices[i], new_msb_indices[i]);
                    }
                    
                    // Check for wraparound in new XOR
                    bool new_has_wraparound = (new_block_count >= 2) && 
                                            (new_lsb_indices[0] == 0) && 
                                            (new_msb_indices[new_block_count-1] == 15);
                    
                    int effective_new_blocks = new_has_wraparound ? new_block_count - 1 : new_block_count;
                    
                    LOG_W(PDCP, "New XOR wraparound: %s, effective blocks: %d\n", 
                          new_has_wraparound ? "true" : "false", effective_new_blocks);
                    
                    // Check if new XOR has exactly one contiguous block
                    if (effective_new_blocks == 1) {
                        int new_lsb;
                        if (new_has_wraparound) {
                            // Merge wraparound blocks - use LSB from the high-bit block
                            new_lsb = new_lsb_indices[new_block_count-1];  // LSB from high block
                            LOG_W(PDCP, "New wraparound block: LSB=%d\n", new_lsb);
                        } else {
                            new_lsb = new_lsb_indices[0];
                            LOG_W(PDCP, "New single block: LSB=%d\n", new_lsb);
                        }
                        
                        // Check if new LSB is within the range of original block
                        bool new_lsb_in_range = false;
                        if (is_wraparound) {
                            // Original block wraps around (e.g., bits 15 and 0-6)
                            // Check if new_lsb is in either range: >= lsb_index (high bits) OR <= msb_index (low bits)
                            new_lsb_in_range = (new_lsb >= lsb_index) || (new_lsb <= msb_index);
                            LOG_W(PDCP, "Original wraparound range check: (%d >= %d) || (%d <= %d) = %s\n",
                                  new_lsb, lsb_index, new_lsb, msb_index, new_lsb_in_range ? "true" : "false");
                        } else {
                            // Original block is contiguous
                            new_lsb_in_range = (new_lsb >= lsb_index) && (new_lsb <= msb_index);
                            LOG_W(PDCP, "Original contiguous range check: (%d >= %d) && (%d <= %d) = %s\n",
                                  new_lsb, lsb_index, new_lsb, msb_index, new_lsb_in_range ? "true" : "false");
                        }
                        
                        if (new_lsb_in_range) {
                            int second_payload_bit = new_lsb;
                            
                            LOG_W(PDCP, "Found concatenated pattern: first_bit=%d, second_bit=%d\n", 
                                  first_payload_bit, second_payload_bit);
                            
                            // Try all payload words for the second bit
                            for (int pw2 = 0; pw2 < num_payload_words && recovery_count < 16; pw2++) {
                                memcpy(corrected_buffer, buffer, size);
                                
                                // Flip both payload bits
                                // First bit
                                int offset1_final = payload_start + pw1 * 2;
                                if (offset1_final < size - 1) {
                                    unsigned short val1_final = (corrected_buffer[offset1_final] << 8) | corrected_buffer[offset1_final + 1];
                                    val1_final ^= (1 << first_payload_bit);
                                    corrected_buffer[offset1_final] = (val1_final >> 8) & 0xFF;
                                    corrected_buffer[offset1_final + 1] = val1_final & 0xFF;
                                }
                                
                                // Second bit
                                int offset2_final = payload_start + pw2 * 2;
                                if (offset2_final < size - 1) {
                                    unsigned short val2_final = (corrected_buffer[offset2_final] << 8) | corrected_buffer[offset2_final + 1];
                                    val2_final ^= (1 << second_payload_bit);
                                    corrected_buffer[offset2_final] = (val2_final >> 8) & 0xFF;
                                    corrected_buffer[offset2_final + 1] = val2_final & 0xFF;
                                    
                                    if (verify_correction(corrected_buffer, test_data, pdu_start, payload_start, payload_len, checksum_cal_len)) {
                                        // Pack recovery data
                                        unsigned char first_entry = ((pw1 & 0x3) << 4) | (first_payload_bit & 0xF);
                                        unsigned char second_entry = ((pw2 & 0x3) << 4) | (second_payload_bit & 0xF);
                                        
                                        int total_bits_used = (starting_recovery_count + recovery_count) * 12;
                                        int byte_offset = 6 + (total_bits_used / 8);
                                        int bit_offset_pos = total_bits_used % 8;
                                        
                                        // Pack first 6-bit entry
                                        if (bit_offset_pos <= 2) {
                                            buffer[size + byte_offset] |= (first_entry << (2 - bit_offset_pos));
                                            if (bit_offset_pos > 0) {
                                                buffer[size + byte_offset + 1] |= (first_entry >> (6 - (2 - bit_offset_pos)));
                                            }
                                        } else {
                                            buffer[size + byte_offset] |= (first_entry >> (bit_offset_pos - 2));
                                            buffer[size + byte_offset + 1] |= (first_entry << (10 - bit_offset_pos));
                                        }
                                        
                                        // Update bit position for second entry
                                        total_bits_used += 6;
                                        byte_offset = 6 + (total_bits_used / 8);
                                        bit_offset_pos = total_bits_used % 8;
                                        
                                        // Pack second 6-bit entry
                                        if (bit_offset_pos <= 2) {
                                            buffer[size + byte_offset] |= (second_entry << (2 - bit_offset_pos));
                                            if (bit_offset_pos > 0) {
                                                buffer[size + byte_offset + 1] |= (second_entry >> (6 - (2 - bit_offset_pos)));
                                            }
                                        } else {
                                            buffer[size + byte_offset] |= (second_entry >> (bit_offset_pos - 2));
                                            buffer[size + byte_offset + 1] |= (second_entry << (10 - bit_offset_pos));
                                        }
                                        
                                        recovery_count++;
                                        LOG_W(PDCP, "Recovery %d (scenario 3): payload_word1=%d, payload_bit1=%d, payload_word2=%d, payload_bit2=%d\n",
                                              recovery_count, pw1, first_payload_bit, pw2, second_payload_bit);
                                    }
                                }
                            }
                        }
                    }
                }
                
                free(temp_test_data);
            }
            
            free(intermediate_buffer);
        }
    }
    
    free(corrected_buffer);
    free(test_data);
    LOG_W(PDCP, "One block payload recovery complete: found %d valid recoveries\n", recovery_count);
    return recovery_count;
}


/**
 * @brief returns the maximum PDCP PDU size
 *        which corresponds to data PDU for DRBs with 18 bits PDCP SN
 *        and integrity enabled
*/
int nr_max_pdcp_pdu_size(sdu_size_t sdu_size)
{
  return (sdu_size + LONG_PDCP_HEADER_SIZE + PDCP_INTEGRITY_SIZE);
}

static void nr_pdcp_entity_recv_pdu(nr_pdcp_entity_t *entity,
                                    char *_buffer, int size)
{
  unsigned char    *buffer = (unsigned char *)_buffer;
  nr_pdcp_sdu_t    *sdu;
  int              rcvd_sn;
  uint32_t         rcvd_hfn;
  uint32_t         rcvd_count;
  int              header_size;
  int              integrity_size;
  int              sdap_header_size = 0;
  int              rx_deliv_sn;
  uint32_t         rx_deliv_hfn;

  if (entity->entity_suspended) {
    LOG_W(PDCP,
          "PDCP entity (%s) %d is suspended. Quit RX procedure.\n",
          entity->type > NR_PDCP_DRB_UM ? "SRB" : "DRB",
          entity->rb_id);
    return;
  }

  if (size < 1) {
    LOG_E(PDCP, "bad PDU received (size = %d)\n", size);
    return;
  }

  if (entity->type != NR_PDCP_SRB && !(buffer[0] & 0x80)) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    /* TODO: This is something of a hack. The most significant bit
       in buffer[0] should be 1 if the packet is a data packet. We are
       processing malformed data packets if the most significant bit
       is 0. Rather than exit(1), this hack allows us to continue for now.
       We need to investigate why this hack is neccessary. */
    buffer[0] |= 128;
  }
  entity->stats.rxpdu_pkts++;
  entity->stats.rxpdu_bytes += size;

  if (entity->has_sdap_rx) sdap_header_size = 1; // SDAP Header is one byte

  if (entity->sn_size == SHORT_SN_SIZE) {
    rcvd_sn = ((buffer[0] & 0xf) <<  8) |
                buffer[1];
    header_size = SHORT_PDCP_HEADER_SIZE;
  } else {
    rcvd_sn = ((buffer[0] & 0x3) << 16) |
               (buffer[1]        <<  8) |
                buffer[2];
    header_size = LONG_PDCP_HEADER_SIZE;
  }
  entity->stats.rxpdu_sn = rcvd_sn;

  /* SRBs always have MAC-I, even if integrity is not active */
  if (entity->has_integrity || entity->type == NR_PDCP_SRB) {
    integrity_size = PDCP_INTEGRITY_SIZE;
  } else {
    integrity_size = 0;
  }

  if (size < header_size + sdap_header_size + integrity_size + 1) {
    LOG_E(PDCP, "bad PDU received (size = %d)\n", size);

    entity->stats.rxpdu_dd_pkts++;
    entity->stats.rxpdu_dd_bytes += size;

    return;
  }

  rx_deliv_sn  = entity->rx_deliv & entity->sn_max;
  rx_deliv_hfn = entity->rx_deliv >> entity->sn_size;

  if (rcvd_sn < rx_deliv_sn - entity->window_size) {
    rcvd_hfn = rx_deliv_hfn + 1;
  } else if (rcvd_sn >= rx_deliv_sn + entity->window_size) {
    rcvd_hfn = rx_deliv_hfn - 1;
  } else {
    rcvd_hfn = rx_deliv_hfn;
  }

  rcvd_count = (rcvd_hfn << entity->sn_size) | rcvd_sn;

  nr_pdcp_integrity_data_t msg_integrity = { 0 };

  /* the MAC-I/header/rcvd_count is needed by some RRC procedures, store it */
  if (entity->has_integrity || entity->type == NR_PDCP_SRB) {
    msg_integrity.count = rcvd_count;
    memcpy(msg_integrity.mac, &buffer[size-4], 4);
    msg_integrity.header_size = header_size;
    memcpy(msg_integrity.header, buffer, header_size);
  }

  if (entity->has_ciphering) {

    /* DeShuffling is added here: Start */   

    if (shuffle_enable == 0){
        // Ciphertext
        if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
            LOG_W(PDCP, "(Receiver side before deciphering) %s():\n", __func__);
            for (int i = 0; i < size - header_size - sdap_header_size; ++i) {
                LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + i]);
            }
            LOG_W(PDCP, "\n");
        }  
        
        // Deciphering
        entity->cipher(entity->security_context,
                    buffer + header_size + sdap_header_size,
                    size - (header_size + sdap_header_size),
                    entity->rb_id, rcvd_count, entity->is_gnb ? 0 : 1);
        
        // Plaintext
        if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
            LOG_W(PDCP, "(Receiver side after deciphering) %s():\n", __func__);
            for (int k = 0; k < size + integrity_size - sdap_header_size - 3; ++k) {
                LOG_W(PDCP, "%02x ", ((unsigned char *)buffer)[header_size + sdap_header_size + k]);
            }
            LOG_W(PDCP, "\n");
        }
    } else if (shuffle_enable == 1){
        //JOON: this is the shuffled ciphertext
        if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
            LOG_W(PDCP, "(Receiver side before deciphering - Shuffled Ciphertext) %s():\n", __func__);
            for (int i = 0; i < size - header_size - sdap_header_size; ++i) {
                LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + i]);
            }
            LOG_W(PDCP, "\n");
        }  
        
        //checksum starts from 26th byte, payload extends to the end
        // not sure why data length extends 3 more bytes in this function
        uint8_t data_length = size + integrity_size - sdap_header_size - 26 - 3;
        LOG_W(PDCP, "(Recv) data_length: %d, size: %d, integrity_size: %d, sdap_header_size: %d\n\n", data_length, size, integrity_size, sdap_header_size);
        
        //copy the checksum and payload part of the shuffled ciphertext
        unsigned char *shuffled_ctx_data = malloc(sizeof(char) * data_length);
        if (shuffled_ctx_data) {
            memcpy(shuffled_ctx_data, &buffer[header_size + sdap_header_size + 26], data_length);
            
            entity->cipher(entity->security_context,
                        buffer + header_size + sdap_header_size,
                        size - (header_size + sdap_header_size),
                        entity->rb_id, rcvd_count, entity->is_gnb ? 0 : 1);
            
            //JOON: this is the shuffled plaintext
            if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
                LOG_W(PDCP, "(Receiver side after deciphering - Shuffled Plaintext) %s():\n", __func__);
                for (int i = 0; i < size - header_size - sdap_header_size; ++i) {
                    LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + i]);
                }
                LOG_W(PDCP, "\n");
                
                //copy the checksum and payload part of the shuffled plaintext
                unsigned char *shuffled_ptx_data = malloc(sizeof(char) * data_length);
                if (shuffled_ptx_data) {
                    memcpy(shuffled_ptx_data, &buffer[header_size + sdap_header_size + 26], data_length);
                    
                    //extract the keystream by XORing shuffled ptx and ctx
                    unsigned char *keystream = malloc(sizeof(char) * data_length);
                    if (keystream) {
                        for (int i = 0; i < data_length; i++) {
                            keystream[i] = shuffled_ptx_data[i] ^ shuffled_ctx_data[i];
                        }
                        
                        LOG_W(PDCP, "(data part of keystream) %s():\n", __func__);
                        for (int k = 0; k < data_length; ++k) {
                            LOG_W(PDCP, "%02x ", keystream[k]);
                        }
                        LOG_W(PDCP, "\n");
                        
                        //unshuffle ctx based on the keystream
                        unsigned char *temp = malloc(sizeof(char) * data_length);
                        if (temp) {
                            prp_invert_permute_bits(shuffled_ctx_data, temp, data_length * 8, keystream, data_length);
                            
                            LOG_W(PDCP, "Unshuffled Ciphertext (checksum and data payload) %s():\n", __func__);
                            for (int k = 0; k < data_length; ++k) {
                                LOG_W(PDCP, "%02x ", temp[k]);
                            }
                            LOG_W(PDCP, "\n");
                            
                            //reinitialize buffer with the unshuffled ciphertext
                            memcpy(&buffer[header_size + sdap_header_size + 26], temp, data_length);
                            
                            LOG_W(PDCP, "Unshuffled Ciphertext (all) %s():\n", __func__);
                            for (int k = 0; k < size + integrity_size - sdap_header_size - 3; ++k) {
                                LOG_W(PDCP, "%02x ", ((unsigned char *)buffer)[header_size + sdap_header_size + k]);
                            }
                            LOG_W(PDCP, "\n");
                            
                            //copy the deciphered header
                            unsigned char *deciphered_header = malloc(sizeof(char) * 26);
                            if (deciphered_header) {
                                memcpy(deciphered_header, &buffer[header_size + sdap_header_size], 26);
                                
                                //decipher the true ctx
                                entity->cipher(entity->security_context,
                                            buffer + header_size + sdap_header_size,
                                            size - (header_size + sdap_header_size),
                                            entity->rb_id, rcvd_count, entity->is_gnb ? 0 : 1);
                                
                                memcpy(&buffer[header_size + sdap_header_size], deciphered_header, 26);
                                
                                LOG_W(PDCP, "Unshuffled Plaintext (all) %s():\n", __func__);
                                for (int k = 0; k < size + integrity_size - sdap_header_size - 3; ++k) {
                                    LOG_W(PDCP, "%02x ", ((unsigned char *)buffer)[header_size + sdap_header_size + k]);
                                }
                                LOG_W(PDCP, "\n");
                                
                                // Free deciphered_header
                                free(deciphered_header);
                            } else {
                                LOG_E(PDCP, "Failed to allocate deciphered_header\n");
                            }
                            
                            // Free temp
                            free(temp);
                        } else {
                            LOG_E(PDCP, "Failed to allocate temp\n");
                        }
                        
                        // Free keystream
                        free(keystream);
                    } else {
                        LOG_E(PDCP, "Failed to allocate keystream\n");
                    }
                    
                    // Free shuffled_ptx_data
                    free(shuffled_ptx_data);
                } else {
                    LOG_E(PDCP, "Failed to allocate shuffled_ptx_data\n");
                }
            }
            
            // Free shuffled_ctx_data
            free(shuffled_ctx_data);
        } else {
            LOG_E(PDCP, "Failed to allocate shuffled_ctx_data\n");
        }
    } else {
        LOG_W(PDCP, "Error Value of shuffle_enable \n");
    }

    /* DeShuffling is added here: End */   
  }

  if (entity->has_integrity) {
    unsigned char integrity[PDCP_INTEGRITY_SIZE] = {0};
    entity->integrity(entity->integrity_context, integrity,
                      buffer, size - integrity_size,
                      entity->rb_id, rcvd_count, entity->is_gnb ? 0 : 1);
    if (memcmp(integrity, buffer + size - integrity_size, PDCP_INTEGRITY_SIZE) != 0) {
      LOG_E(PDCP, "discard NR PDU, integrity failed\n");
      entity->stats.rxpdu_dd_pkts++;
      entity->stats.rxpdu_dd_bytes += size;

      return;
    }
  }

  /* --- The Bit-Flipping Identification starts here --- */

  // Define a variable to enable/disable bit-flipping identification
  int bitflipIden_enable = 1;  

  // now 'buffer' should be deciphered PDCP SDU. We consider 'buffer[0]' is the position after 'header' and 'sdap_header'
  // 'Transport Layer Protocol' should be buffer[9]
  // 'source IP address' should be 'buffer[12, 13, 14, 15]'
  // 'destination IP address' should be 'buffer[16, 17, 18, 19]'
  // 'source port' should be 'buffer[20, 21]'
  // 'destination port' should be 'buffer[22, 23]'
  // 'UDP length' should be 'buffer[24, 25]'
  // 'UDP checksum' should be 'buffer[26, 27]'
  // 'Actual payload starts at 'buffer[28]'

  // So, we need to build a 'unsigned char data[]':
  // (1) Its first 4 bytes (source IP address) should be same as: 
  //                                        'buffer[header_size + sdap_header_size +12], buffer[header_size + sdap_header_size +13],
  //                                         buffer[header_size + sdap_header_size +14], buffer[header_size + sdap_header_size +15]
  // (2) Its next 4 bytes (destination IP address) should be same as:
  //                                        'buffer[header_size + sdap_header_size +16], buffer[header_size + sdap_header_size +17],
  //                                         buffer[header_size + sdap_header_size +18], buffer[header_size + sdap_header_size +19]'
  // (3) Its next byte should be '0x00'
  // (4) Its next byte (transport layer protocol) should be same as 'buffer[header_size + sdap_header_size +9]'
  // (5) Its next 2 bytes (udp length) should be same as 'buffer[header_size + sdap_header_size +24], buffer[header_size + sdap_header_size +25]'
  // (6) Its next 2 bytes (source port) should be same as 'buffer[header_size + sdap_header_size +20], buffer[header_size + sdap_header_size +21]'
  // (7) Its next 2 bytes (destination port) should be same as 'buffer[header_size + sdap_header_size +22], buffer[header_size + sdap_header_size +23]'
  // (8) Its next 2 bytes (UDP length) should be same as 'buffer[header_size + sdap_header_size +24], buffer[header_size + sdap_header_size +25]'
  // (9) Its next 2 bytes should be same as '0x00, 0x00', which is reserved for checksum
  // (10) Its next bytes should be same as the bytes start from 'buffer[header_size + sdap_header_size +28]', till the end of the 'buffer'.
  // (11) We will check the length of the data, if its byte number is odd, we need to add a zero byte.

  if ((entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM) && (entity->is_gnb == 1) && (bitflipIden_enable == 1)){

    /* 1. Recalculate the checksum manually (received PDCP PDU might be attacked by bit-flipping on both checksum and data payload part) */

    // 1.1 Extract the start byte index of the PDCP SDU inside the buffer
    int pdu_start = header_size + sdap_header_size;

    // 1.2 Extract the start byte index, and the length of actual payload
    int payload_start = pdu_start + 28;
    int payload_len = size - payload_start; // bytes from (header + sdap + 28) to end of buffer

    // 1.3 Calculate the total length of the UDP pseudo-header for checksum calculation (correspond to the point (1) to (11) above)
    int checksum_cal_len = 4 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + payload_len; // unit: bytes

    // 1.4 Zero-padding if odd
    if (checksum_cal_len % 2 != 0) checksum_cal_len++;

    // 1.5 Allocate the memory for the UDP pseudo-header for checksum calculation 
    unsigned char *data1 = (unsigned char *)calloc(checksum_cal_len, 1); // calloc makes extra byte 0 if needed

    // 1.6 Fill the UDP pseudo-header based on the PDCP SDU received

    int j = 0;  

    // (1.6.1) First 4 bytes of 'source IP address'
    memcpy(data1 + j, buffer + pdu_start + 12, 4);  
    j += 4;    

    // (1.6.2) Next 4 bytes of 'destination IP address'
    memcpy(data1 + j, buffer + pdu_start + 16, 4);
    j += 4;   

    // (1.6.3) Next byte 0x00
    data1[j++] = 0x00;

    // (1.6.4) Next byte of 'Transport Layer Protocol'
    data1[j++] = buffer[pdu_start + 9];

    // (1.6.5) Next 2 bytes of 'UDP length'
    memcpy(data1 + j, buffer + pdu_start + 24, 2);
    j += 2;

    // (1.6.6) Next 2 bytes of 'Source Port'
    memcpy(data1 + j, buffer + pdu_start + 20, 2);
    j += 2;

    // (1.6.7) Next 2 bytes of 'Destination Port'
    memcpy(data1 + j, buffer + pdu_start + 22, 2);
    j += 2;

    // (1.6.8) Next 2 bytes of 'UDP length' (again)
    memcpy(data1 + j, buffer + pdu_start + 24, 2);
    j += 2;

    // (1.6.9) Next 2 bytes 0x00,0x00 (reserved for checksum)
    data1[j++] = 0x00;
    data1[j++] = 0x00;

    // (1.6.10) Fill the data payload
    memcpy(data1 + j, buffer + payload_start, payload_len);
    j += payload_len;

    // (1.6.11) Zero byte already set if padding was needed, due to calloc, so UDP pseudo-header for checksum calculation is ready.

    // 1.7 Extract the received checksum
    int offset = header_size + sdap_header_size + 26;
    unsigned char *ptr = (unsigned char *)buffer;
    unsigned short checksum_rec = (ptr[offset] << 8) | ptr[offset + 1];

    // 1.8 Print the received checksum
    LOG_W(PDCP, "(Received checksum): 0x%04x\n", checksum_rec);
    LOG_W(PDCP, "\n");

    // 1.9 Calculate the checksum based on the received PDCP SDU
    unsigned short result = checksum_pdcp((unsigned short *)data1, checksum_cal_len);
    unsigned short checksum_cal = ((result & 0xFF) << 8) | (result >> 8);

    // 1.10 Print the calculated checksum
    LOG_W(PDCP, "(Calculated checksum): 0x%04x\n", checksum_cal);
    LOG_W(PDCP, "\n");

    // 1.11 Do XOR on the received checksum and calculated checksum
    unsigned short xor_cksum = checksum_rec ^ checksum_cal;

    // 1.12 Print the XOR result as hex, for benign case it should always be zero
    LOG_W(PDCP, "(XOR between two checksum): 0x%04x\n", xor_cksum);
    LOG_W(PDCP, "\n");

    // 1.13 Transfer the received checksum, calculated checksum, and their XOR values to binary string
    char bin_rec[17], bin_cal[17], bin_xor[17];
    ushort_to_binstr(checksum_cal, bin_cal);
    ushort_to_binstr(checksum_rec, bin_rec);
    ushort_to_binstr(xor_cksum, bin_xor);

    // 1.14 Count contiguous 1s blocks in xor_cksum
    int block_count = 0;
    int lsb_indices[8], msb_indices[8];
    bool in_block = false;
    int current_lsb = -1;
    
    // Linear scan for blocks
    for (int i = 0; i < 16; i++) {
        bool bit_set = (xor_cksum >> i) & 1;
        if (bit_set && !in_block) {
            current_lsb = i;
            in_block = true;
        } else if (!bit_set && in_block) {
            lsb_indices[block_count] = current_lsb;
            msb_indices[block_count] = i - 1;
            block_count++;
            in_block = false;
        }
    }
    if (in_block) {
        lsb_indices[block_count] = current_lsb;
        msb_indices[block_count] = 15;
        block_count++;
    }
    
    // Check for wraparound ambiguity
    int min_blocks = block_count, max_blocks = block_count;
    if (block_count >= 2) {
        bool msb_set = (xor_cksum >> 15) & 1;
        bool lsb_set = xor_cksum & 1;
        if (msb_set && lsb_set && lsb_indices[0] == 0 && msb_indices[block_count - 1] == 15) {
            min_blocks = block_count - 1;  // Potential wraparound
        }
    }
    
    LOG_W(PDCP, "XOR result: 0x%04x, Binary: %s\n", xor_cksum, bin_xor);
    if (min_blocks == max_blocks) {
        LOG_W(PDCP, "Definitive count: %d contiguous 1s blocks\n", min_blocks);
    } else {
        LOG_W(PDCP, "Possible count: %d or %d contiguous 1s blocks\n", min_blocks, max_blocks);
    }

    // 1.15 Write the 6 strings to a .csv file "checksum_rec, checksum_cal, bin_cal, bin_rec, bin_xor, contiguous_blocks"
    FILE *fp = fopen("/home/derek/openairinterface5g2024w43/openair2/LAYER2/nr_pdcp/checksums.csv", "a"); // "a" for append mode
    if (fp) {
        if (min_blocks == max_blocks) {
            // Definitive count
            fprintf(fp, "0x%04x,0x%04x,%s,%s,%s,%d\n", checksum_rec, checksum_cal, bin_rec, bin_cal, bin_xor, min_blocks);
        } else {
            // Ambiguous count - store as "min|max"
            fprintf(fp, "0x%04x,0x%04x,%s,%s,%s,%d|%d\n", checksum_rec, checksum_cal, bin_rec, bin_cal, bin_xor, min_blocks, max_blocks);
        }
        fclose(fp);
    } else {
        LOG_E(PDCP, "Could not open checksums.csv for writing\n");
    }
    
    // Free memory of 'data1'
    free(data1);

    /* 2. Extend the PDCP PDU. Replace the UDP length field to the new value, and recalculate the checksum */

    // You can extend any number of bytes, best be an even number for 2-byte units.
    // For our experiment we need: 2 bytes (received checksum) + 2 bytes (calculated checksum) + 2 bytes (XOR) + 30 bytes (store 20 pairs of possible flipped bits, each pair takes 12 bits) = 36 bytes total
    int num_ext_byte = 30;  // 15 2-byte words total, 12 words to store at most 16 pairs of bit flips 

    // 2.1 Extend the *buffer pointer for 'num_ext_byte' bytes
    unsigned char *temp = realloc(buffer, size + num_ext_byte);
    if (!temp) {
        LOG_E(PDCP, "Could not reallocate buffer for extension. It is not dynamically allocated.\n");
    } else {
        // Updates the buffer pointer to the new memory location and initializes the extended portion with zeros.
        buffer = temp;
        memset(buffer + size, 0x00, num_ext_byte);
        
        // Fill the first 2 bytes of the extended bytes: received checksum
        buffer[size] = (checksum_rec >> 8) & 0xFF;
        buffer[size + 1] = checksum_rec & 0xFF;
        
        // Fill the next 2 bytes of the extended bytes: calculated checksum  
        buffer[size + 2] = (checksum_cal >> 8) & 0xFF;
        buffer[size + 3] = checksum_cal & 0xFF;
        
        // Fill the next 2 bytes: XOR value
        buffer[size + 4] = (xor_cksum >> 8) & 0xFF;
        buffer[size + 5] = xor_cksum & 0xFF;
        
        // Calculate number of payload words
        int num_payload_words = (payload_len + 1) / 2;  // Number of 2-byte payload word
        int recovery_count = 0;  // Count of valid recoveries found

        LOG_W(PDCP, "Found %d contiguous 1s blocks (min=%d, max=%d)\n", block_count, min_blocks, max_blocks);
        for (int i = 0; i < block_count; i++) {
            int block_len = msb_indices[i] - lsb_indices[i] + 1;
            LOG_W(PDCP, "Block %d: LSB=%d, MSB=%d, Length=%d\n", i, lsb_indices[i], msb_indices[i], block_len);
        }
        
        // Handle different patterns
        if (min_blocks == 0 && max_blocks == 0) {
            // No bit flips - checksums match perfectly
            LOG_W(PDCP, "No bit flips detected - checksums match\n");
            LOG_W(PDCP, "No flips - stored pattern 0x0000\n");
        } else if (min_blocks == 1 && max_blocks == 1) {
            // Definitive single block
            LOG_W(PDCP, "Definitive single 1s block case\n");
            
            // Method 1: Try one-block checksum recovery
            int one_block_recoveries = one_block_checksum_recovery(buffer, size, pdu_start, payload_start,
                                                                  payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                  block_count, false, num_payload_words, recovery_count);
            recovery_count += one_block_recoveries;
            LOG_W(PDCP, "One block checksum recovery found %d recoveries, total so far: %d\n", one_block_recoveries, recovery_count);

            // Method 2: Try one-block payload recovery (continue even if previous method found recoveries)
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying one-block payload recovery (current count: %d)\n", recovery_count);
                int one_block_payload_recoveries = one_block_payload_recovery(buffer, size, pdu_start, payload_start,
                                                                            payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                            block_count, false, num_payload_words, recovery_count);
                recovery_count += one_block_payload_recoveries;
                LOG_W(PDCP, "One block payload recovery found %d recoveries, total so far: %d\n", one_block_payload_recoveries, recovery_count);
            }

            // If no recoveries found, store the pattern
            if (recovery_count == 0) {
                // Create pattern [1,1,1,1] using proper 6-bit entries
                // Each entry: (word_index:2bits << 4) | bit_index:4bits
                // Pattern: word_index=1, bit_index=1 for all entries
                unsigned char pattern_entry = ((1 & 0x3) << 4) | (1 & 0xF);  // 0x11
                
                // Fill the recovery data area with the pattern
                // 24 bytes = 192 bits = 32 six-bit entries = 16 pairs
                for (int bit_pos = 0; bit_pos < 192; bit_pos += 6) {
                    int byte_offset = 6 + (bit_pos / 8);
                    int bit_offset_in_byte = bit_pos % 8;
                    
                    // Pack 6-bit entry
                    if (bit_offset_in_byte <= 2) {
                        buffer[size + byte_offset] |= (pattern_entry << (2 - bit_offset_in_byte));
                        if (bit_offset_in_byte > 0) {
                            buffer[size + byte_offset + 1] |= (pattern_entry >> (6 - (2 - bit_offset_in_byte)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (pattern_entry >> (bit_offset_in_byte - 2));
                        buffer[size + byte_offset + 1] |= (pattern_entry << (10 - bit_offset_in_byte));
                    }
                }
                LOG_W(PDCP, "Single block - stored pattern as packed [1,1] entries\n");
              }
    
        } else if (min_blocks == 2 && max_blocks == 2) {
            // Definitive two blocks - check for single zero pattern first
            LOG_W(PDCP, "Definitive two 1s blocks case\n");
            
            // Pattern 1:Check for single zeros between the two blocks
            bool is_single_zero_pattern = false;
            int zero_bits[2];
            int previous_block_lsbs[2];
            int num_single_zeros = 0;
            
            // Check gaps between consecutive blocks
            for (int i = 0; i < block_count - 1; i++) {
                int current_end = msb_indices[i];
                int next_start = lsb_indices[i + 1];
                
                if (next_start > current_end + 1) {
                    int gap_size = next_start - current_end - 1;
                    if (gap_size == 1 && num_single_zeros < 2) {
                        zero_bits[num_single_zeros] = current_end + 1;
                        previous_block_lsbs[num_single_zeros] = lsb_indices[i];
                        num_single_zeros++;
                        is_single_zero_pattern = true;
                        LOG_W(PDCP, "Single zero found: zero_bit=%d, previous_block_lsb=%d\n", 
                              zero_bits[num_single_zeros-1], previous_block_lsbs[num_single_zeros-1]);
                    }
                }
            }
            
            // Check circular gap (from last block to first block)
            int last_end = msb_indices[block_count - 1];
            int first_start = lsb_indices[0];
            int circular_gap_size = (16 + first_start - last_end - 1) % 16;
            
            if (circular_gap_size == 1 && num_single_zeros < 2) {
                zero_bits[num_single_zeros] = (last_end + 1) % 16;
                previous_block_lsbs[num_single_zeros] = lsb_indices[block_count - 1];
                num_single_zeros++;
                is_single_zero_pattern = true;
                LOG_W(PDCP, "Circular single zero found: zero_bit=%d, previous_block_lsb=%d\n", 
                      zero_bits[num_single_zeros-1], previous_block_lsbs[num_single_zeros-1]);
            }
            
            if (is_single_zero_pattern) {
                LOG_W(PDCP, "Single zero pattern detected - attempting recovery\n");
                int single_zero_recoveries = two_block_single_zero_recovery(buffer, size, pdu_start, payload_start,
                                                                          payload_len, checksum_cal_len, zero_bits, previous_block_lsbs,
                                                                          num_single_zeros, num_payload_words, recovery_count);
                recovery_count += single_zero_recoveries;
                LOG_W(PDCP, "Single zero recovery found %d recoveries, total so far: %d\n", single_zero_recoveries, recovery_count);
            }

            // Method 2: Try checksum+payload recovery (continue even if previous method found recoveries)
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying checksum+payload recovery (current count: %d)\n", recovery_count);
                int checksum_payload_recoveries = two_block_single_one_checksum_recovery(buffer, size, pdu_start, payload_start,
                                                                                        payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                                        block_count, min_blocks, max_blocks, num_payload_words, recovery_count);
                recovery_count += checksum_payload_recoveries;
                LOG_W(PDCP, "Checksum+payload recovery found %d recoveries, total so far: %d\n", checksum_payload_recoveries, recovery_count);
            }

            // Method 3: Try general payload recovery (continue even if previous methods found recoveries)
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying general payload recovery (current count: %d)\n", recovery_count);
                int general_recoveries = two_block_general_recovery(buffer, size, pdu_start, payload_start,
                                                                  payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                  block_count, min_blocks, max_blocks, num_payload_words, recovery_count);
                recovery_count += general_recoveries;
                LOG_W(PDCP, "General payload recovery found %d recoveries, total so far: %d\n", general_recoveries, recovery_count);
            }
            
            if (recovery_count == 0) {
                
                // Create pattern [2,2,2,2] using proper 6-bit entries
                // Each entry: (word_index:2bits << 4) | bit_index:4bits
                // Pattern: word_index=2, bit_index=2 for all entries
                unsigned char pattern_entry = ((2 & 0x3) << 4) | (2 & 0xF);  // 0x22
                
                // Fill the recovery data area with the pattern
                // 24 bytes = 192 bits = 32 six-bit entries = 16 pairs
                for (int bit_pos = 0; bit_pos < 192; bit_pos += 6) {
                    int byte_offset = 6 + (bit_pos / 8);
                    int bit_offset_in_byte = bit_pos % 8;
                    
                    // Pack 6-bit entry
                    if (bit_offset_in_byte <= 2) {
                        buffer[size + byte_offset] |= (pattern_entry << (2 - bit_offset_in_byte));
                        if (bit_offset_in_byte > 0) {
                            buffer[size + byte_offset + 1] |= (pattern_entry >> (6 - (2 - bit_offset_in_byte)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (pattern_entry >> (bit_offset_in_byte - 2));
                        buffer[size + byte_offset + 1] |= (pattern_entry << (10 - bit_offset_in_byte));
                    }
                }

                LOG_W(PDCP, "No recoveries found in definitive two-block case - stored pattern 0x2222\n");
            }
        } else if (min_blocks == 2 && max_blocks == 3) {
            // Ambiguous 2|3 blocks - treat as 2 blocks (wraparound) and check for single zero pattern
            LOG_W(PDCP, "Ambiguous 2|3 blocks case (wraparound) - treating as 2 blocks\n");
            
            // Pattern1: Check for single zeros in wraparound interpretation
            bool is_single_zero_pattern = false;
            int zero_bits[2];
            int previous_block_lsbs[2];
            int num_single_zeros = 0;
            
            // Handle wraparound case for 3 blocks
            bool can_merge = ((xor_cksum >> 15) & 1) && (xor_cksum & 1) && 
                             (lsb_indices[0] == 0) && (msb_indices[2] == 15);
            
            if (can_merge) {
                int merged_end = msb_indices[0];
                int middle_start = lsb_indices[1];
                int middle_end = msb_indices[1];
                int merged_lsb = lsb_indices[2];
                
                if (middle_start > merged_end + 1) {
                    int gap_size = middle_start - merged_end - 1;
                    if (gap_size == 1 && num_single_zeros < 2) {
                        zero_bits[num_single_zeros] = merged_end + 1;
                        previous_block_lsbs[num_single_zeros] = merged_lsb;
                        num_single_zeros++;
                        is_single_zero_pattern = true;
                        LOG_W(PDCP, "Wraparound single zero found: zero_bit=%d, previous_block_lsb=%d\n", 
                              zero_bits[num_single_zeros-1], previous_block_lsbs[num_single_zeros-1]);
                    }
                }
                
                int circular_gap = (16 + merged_lsb - middle_end - 1) % 16;
                if (circular_gap == 1 && num_single_zeros < 2) {
                    zero_bits[num_single_zeros] = (middle_end + 1) % 16;
                    previous_block_lsbs[num_single_zeros] = lsb_indices[1];
                    num_single_zeros++;
                    is_single_zero_pattern = true;
                    LOG_W(PDCP, "Wraparound circular single zero found: zero_bit=%d, previous_block_lsb=%d\n", 
                          zero_bits[num_single_zeros-1], previous_block_lsbs[num_single_zeros-1]);
                }
            }
            
            if (is_single_zero_pattern) {
                LOG_W(PDCP, "Single zero pattern detected - attempting recovery\n");
                int single_zero_recoveries = two_block_single_zero_recovery(buffer, size, pdu_start, payload_start,
                                                                          payload_len, checksum_cal_len, zero_bits, previous_block_lsbs,
                                                                          num_single_zeros, num_payload_words, recovery_count);
                recovery_count += single_zero_recoveries;
                LOG_W(PDCP, "Single zero recovery found %d recoveries, total so far: %d\n", single_zero_recoveries, recovery_count);
            }

            // Method 2: Try checksum+payload recovery (continue even if previous method found recoveries)
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying checksum+payload recovery (current count: %d)\n", recovery_count);
                int checksum_payload_recoveries = two_block_single_one_checksum_recovery(buffer, size, pdu_start, payload_start,
                                                                                        payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                                        block_count, min_blocks, max_blocks, num_payload_words, recovery_count);
                recovery_count += checksum_payload_recoveries;
                LOG_W(PDCP, "Checksum+payload recovery found %d recoveries, total so far: %d\n", checksum_payload_recoveries, recovery_count);
            }

            // Method 3: Try general payload recovery (continue even if previous methods found recoveries)
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying general payload recovery (current count: %d)\n", recovery_count);
                int general_recoveries = two_block_general_recovery(buffer, size, pdu_start, payload_start,
                                                                  payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                  block_count, min_blocks, max_blocks, num_payload_words, recovery_count);
                recovery_count += general_recoveries;
                LOG_W(PDCP, "General payload recovery found %d recoveries, total so far: %d\n", general_recoveries, recovery_count);
            }
            
            if (recovery_count == 0) {
                // No recoveries found - store pattern 0x2223
                // Create pattern [2,3,2,3] using proper 6-bit entries
                // All entries: word_index=2, bit_index=3 -> [2,3]
                unsigned char pattern_entry = ((2 & 0x3) << 4) | (3 & 0xF);  // 0x23 for [2,3]
                
                // Fill the recovery data area with the pattern
                // 24 bytes = 192 bits = 32 six-bit entries = 16 pairs
                for (int entry_num = 0; entry_num < 32; entry_num++) {
                    // Use the same pattern for all entries: [2,3]
                    unsigned char current_entry = pattern_entry;  // Always [2,3]
                    
                    int bit_pos = entry_num * 6;
                    int byte_offset = 6 + (bit_pos / 8);
                    int bit_offset_in_byte = bit_pos % 8;
                    
                    // Pack 6-bit entry
                    if (bit_offset_in_byte <= 2) {
                        buffer[size + byte_offset] |= (current_entry << (2 - bit_offset_in_byte));
                        if (bit_offset_in_byte > 0) {
                            buffer[size + byte_offset + 1] |= (current_entry >> (6 - (2 - bit_offset_in_byte)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (current_entry >> (bit_offset_in_byte - 2));
                        buffer[size + byte_offset + 1] |= (current_entry << (10 - bit_offset_in_byte));
                    }
                }
                LOG_W(PDCP, "No recoveries found in 2|3 wraparound case - stored pattern as packed [2,3] entries\n");
            }
        } else if (min_blocks == 1 && max_blocks == 2) {          
            // Ambiguous 1|2 blocks case - either wraparound single block OR true two blocks
            LOG_W(PDCP, "Ambiguous 1|2 blocks case\n");
            
            // Check if this is a wraparound single block
            bool is_wraparound_single = (block_count == 2) && (lsb_indices[0] == 0) && (msb_indices[1] == 15);
            
            // Method 1: Try one-block recovery methods ONLY if wraparound detected
            if (is_wraparound_single) {
                LOG_W(PDCP, "Detected wraparound single block - attempting one-block recoveries\n");
                
                // Try one-block checksum recovery (wraparound)
                int one_block_checksum_recoveries = one_block_checksum_recovery(buffer, size, pdu_start, payload_start,
                                                                              payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                              block_count, true, num_payload_words, recovery_count);
                recovery_count += one_block_checksum_recoveries;
                LOG_W(PDCP, "Wraparound one block checksum recovery found %d recoveries, total so far: %d\n", one_block_checksum_recoveries, recovery_count);
                
                // Try one-block payload recovery (wraparound)
                if (recovery_count < 16) {
                    LOG_W(PDCP, "Trying wraparound one-block payload recovery (current count: %d)\n", recovery_count);
                    int one_block_payload_recoveries = one_block_payload_recovery(buffer, size, pdu_start, payload_start,
                                                                                payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                                block_count, true, num_payload_words, recovery_count);
                    recovery_count += one_block_payload_recoveries;
                    LOG_W(PDCP, "Wraparound one block payload recovery found %d recoveries, total so far: %d\n", one_block_payload_recoveries, recovery_count);
                }
            } else {
                LOG_W(PDCP, "Not a wraparound single block - treating as true two blocks\n");
            }
            
            // Always try two-block recovery methods (whether wraparound or true two blocks)
            // Method 1: Check for single zeros between the two blocks
            if (recovery_count < 16) {
                bool is_single_zero_pattern = false;
                int zero_bits[2];
                int previous_block_lsbs[2];
                int num_single_zeros = 0;
                
                // Check gaps between consecutive blocks
                for (int i = 0; i < block_count - 1; i++) {
                    int current_end = msb_indices[i];
                    int next_start = lsb_indices[i + 1];
                    if (next_start > current_end + 1) {
                        int gap_size = next_start - current_end - 1;
                        if (gap_size == 1 && num_single_zeros < 2) {
                            zero_bits[num_single_zeros] = current_end + 1;
                            previous_block_lsbs[num_single_zeros] = lsb_indices[i];
                            num_single_zeros++;
                            is_single_zero_pattern = true;
                            LOG_W(PDCP, "Single zero found: zero_bit=%d, previous_block_lsb=%d\n",
                                  zero_bits[num_single_zeros-1], previous_block_lsbs[num_single_zeros-1]);
                        }
                    }
                }
                
                // Check circular gap (from last block to first block) - but only if not wraparound
                bool has_wraparound = (lsb_indices[0] == 0) && (msb_indices[block_count - 1] == 15);
                if (!has_wraparound) {
                    int last_end = msb_indices[block_count - 1];
                    int first_start = lsb_indices[0];
                    int circular_gap_size = (16 + first_start - last_end - 1) % 16;
                    if (circular_gap_size == 1 && num_single_zeros < 2) {
                        zero_bits[num_single_zeros] = (last_end + 1) % 16;
                        previous_block_lsbs[num_single_zeros] = lsb_indices[block_count - 1];
                        num_single_zeros++;
                        is_single_zero_pattern = true;
                        LOG_W(PDCP, "Circular single zero found: zero_bit=%d, previous_block_lsb=%d\n",
                              zero_bits[num_single_zeros-1], previous_block_lsbs[num_single_zeros-1]);
                    }
                }
                
                if (is_single_zero_pattern) {
                    LOG_W(PDCP, "Single zero pattern detected in 1|2 case - attempting recovery\n");
                    int single_zero_recoveries = two_block_single_zero_recovery(buffer, size, pdu_start, payload_start,
                                                                              payload_len, checksum_cal_len, zero_bits, previous_block_lsbs,
                                                                              num_single_zeros, num_payload_words, recovery_count);
                    recovery_count += single_zero_recoveries;
                    LOG_W(PDCP, "Single zero recovery found %d recoveries, total so far: %d\n", single_zero_recoveries, recovery_count);
                }
            }
            
            // Method 2: Try checksum+payload recovery
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying checksum+payload recovery (current count: %d)\n", recovery_count);
                int checksum_payload_recoveries = two_block_single_one_checksum_recovery(buffer, size, pdu_start, payload_start,
                                                                                        payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                                        block_count, min_blocks, max_blocks, num_payload_words, recovery_count);
                recovery_count += checksum_payload_recoveries;
                LOG_W(PDCP, "Checksum+payload recovery found %d recoveries, total so far: %d\n", checksum_payload_recoveries, recovery_count);
            }
            
            // Method 3: Try general payload recovery
            if (recovery_count < 16) {
                LOG_W(PDCP, "Trying general payload recovery (current count: %d)\n", recovery_count);
                int general_recoveries = two_block_general_recovery(buffer, size, pdu_start, payload_start,
                                                                  payload_len, checksum_cal_len, lsb_indices, msb_indices,
                                                                  block_count, min_blocks, max_blocks, num_payload_words, recovery_count);
                recovery_count += general_recoveries;
                LOG_W(PDCP, "General payload recovery found %d recoveries, total so far: %d\n", general_recoveries, recovery_count);
            }
            
            if (recovery_count == 0) {
                // No recoveries found - store pattern [1,2,1,2]               
                unsigned char pattern_entry = ((1 & 0x3) << 4) | (2 & 0xF);  // 0x12 for [1,2]
                
                for (int entry_num = 0; entry_num < 32; entry_num++) {
                    unsigned char current_entry = pattern_entry;  // Always [1,2]
                    
                    int bit_pos = entry_num * 6;
                    int byte_offset = 6 + (bit_pos / 8);
                    int bit_offset_in_byte = bit_pos % 8;
                    
                    // Pack 6-bit entry
                    if (bit_offset_in_byte <= 2) {
                        buffer[size + byte_offset] |= (current_entry << (2 - bit_offset_in_byte));
                        if (bit_offset_in_byte > 0) {
                            buffer[size + byte_offset + 1] |= (current_entry >> (6 - (2 - bit_offset_in_byte)));
                        }
                    } else {
                        buffer[size + byte_offset] |= (current_entry >> (bit_offset_in_byte - 2));
                        buffer[size + byte_offset + 1] |= (current_entry << (10 - bit_offset_in_byte));
                    }
                }
                
                LOG_W(PDCP, "No recoveries found in 1|2 case - stored pattern as packed [1,2] entries\n");
            } else {
                LOG_W(PDCP, "Total recoveries found in 1|2 case: %d\n", recovery_count);
            }
        }
        
        size += num_ext_byte;
    }
    
    LOG_W(PDCP, "%s(): Extended %d bytes, (gNB:1, UE:0 %d)\n", __func__, num_ext_byte, entity->is_gnb);
    LOG_W(PDCP, "Extended data layout: [RX_CKSUM][CALC_CKSUM][XOR][RECOVERY_DATA...]\n");
    for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
        LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
    }
    LOG_W(PDCP, "\n");

    // 2.2 Add the value 'num_ext_byte' to the UDP length field of the PDCP PDU
    unsigned short udp_len = (buffer[pdu_start + 24] << 8) | buffer[pdu_start + 25];    
    udp_len += num_ext_byte;    
    buffer[pdu_start + 24] = (udp_len >> 8) & 0xFF;
    buffer[pdu_start + 25] = udp_len & 0xFF;

    LOG_W(PDCP, "%s(): (Extended PDU with changed UDP length), (gNB:1, UE:0 %d)\n", __func__, entity->is_gnb);
    for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
        LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
    }
    LOG_W(PDCP, "\n");

    // 2.3 Recalculate the UDP checksum after extending the payload to make sure PDU can go to app layer (data is malicious)
    // Calculate the new payload length including the extended bytes
    int new_payload_len = payload_len + num_ext_byte;

    // Calculate the total length of the UDP pseudo-header for new checksum calculation
    int new_checksum_cal_len = 4 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + new_payload_len; // unit: bytes

    // Zero-padding if odd
    if (new_checksum_cal_len % 2 != 0) new_checksum_cal_len++;

    // Allocate memory for the new UDP pseudo-header for checksum calculation
    unsigned char *data2 = (unsigned char*)calloc(new_checksum_cal_len, 1);

    // Fill the new UDP pseudo-header based on the extended PDCP SDU
    int g = 0;

    // (1) First 4 bytes of 'source IP address'
    memcpy(data2 + g, buffer + pdu_start + 12, 4);
    g += 4;

    // (2) Next 4 bytes of 'destination IP address'
    memcpy(data2 + g, buffer + pdu_start + 16, 4);
    g += 4;

    // (3) Next byte 0x00
    data2[g++] = 0x00;

    // (4) Next byte of 'Transport Layer Protocol'
    data2[g++] = buffer[pdu_start + 9];

    // (5) Next 2 bytes of 'UDP length' (updated length)
    memcpy(data2 + g, buffer + pdu_start + 24, 2);
    g += 2;

    // (6) Next 2 bytes of 'Source Port'
    memcpy(data2 + g, buffer + pdu_start + 20, 2);
    g += 2;

    // (7) Next 2 bytes of 'Destination Port'
    memcpy(data2 + g, buffer + pdu_start + 22, 2);
    g += 2;

    // (8) Next 2 bytes of 'UDP length' (again, updated length)
    memcpy(data2 + g, buffer + pdu_start + 24, 2);
    g += 2;

    // (9) Next 2 bytes 0x00,0x00 (reserved for checksum)
    data2[g++] = 0x00;
    data2[g++] = 0x00;

    // (10) Fill the original data payload + extended data
    memcpy(data2 + g, buffer + payload_start, new_payload_len);
    g += new_payload_len;

    // Calculate the new UDP checksum
    unsigned short new_udp_result = checksum_pdcp((unsigned short *)data2, new_checksum_cal_len);
    unsigned short new_udp_checksum = ((new_udp_result & 0xFF) << 8) | (new_udp_result >> 8);

    // Update the UDP checksum field in the buffer
    buffer[pdu_start + 26] = (new_udp_checksum >> 8) & 0xFF;
    buffer[pdu_start + 27] = new_udp_checksum & 0xFF;

    LOG_W(PDCP, "New UDP checksum calculated: 0x%04x\n", new_udp_checksum);
    LOG_W(PDCP, "%s(): (Extended PDU with changed UDP length and recalculated UDP checksum), (gNB:1, UE:0 %d)\n", __func__, entity->is_gnb);
    for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
        LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
    }
    LOG_W(PDCP, "\n");

    // Free memory of 'data2'
    free(data2);

    // 2.4 Add the value 'num_ext_byte' to the 'PDCP SDU length' field of the PDCP PDU
    unsigned short pdcp_sdu_len = (buffer[pdu_start + 2] << 8) | buffer[pdu_start + 3];    
    pdcp_sdu_len += num_ext_byte;    
    buffer[pdu_start + 2] = (pdcp_sdu_len >> 8) & 0xFF;
    buffer[pdu_start + 3] = pdcp_sdu_len & 0xFF;

    LOG_W(PDCP, "%s(): (Extended PDU with changed UDP length and PDCP SDU length), (gNB:1, UE:0 %d)\n", __func__, entity->is_gnb);
    for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
        LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
    }
    LOG_W(PDCP, "\n");

    // 2.5 Recalculate the IP checksum and replace corresponding field
    // We need to build another 'unsigned char data[]', which should be the same as buffer[0] to buffer [19], where buffer [10,11] should be zero
    int ip_header_len = 20; // length of IP header is fixed no matter how many bytes we added after the data payload

    // (2.5.1) Allocate the memory for the IP pseudo-header for checksum recalculation
    unsigned char *data3 = (unsigned char*)calloc(ip_header_len, 1); // calloc makes extra byte 0 if needed

    // (2.5.2) Copy the IP header from buffer to data3
    memcpy(data3, buffer + pdu_start, ip_header_len);

    // (2.5.3) Zero out the IP header checksum field (bytes 10 and 11) for recalculation
    data3[10] = 0x00;
    data3[11] = 0x00;

    // Debug print data3 before checksum calculation
    LOG_W(PDCP, "data3 before checksum calculation: ");
    for (int k = 0; k < ip_header_len; ++k) {
        LOG_W(PDCP, "%02x ", data3[k]);
    }
    LOG_W(PDCP, "\n");

    // (2.5.4) Recalculate the IP header checksum based on the extended PDCP PDU
    unsigned short result_ip = checksum_pdcp((unsigned short *)data3, ip_header_len);
    unsigned short checksum_cal_ip = ((result_ip & 0xFF) << 8) | (result_ip >> 8);

    // Debug print result_ip and checksum_cal_ip after calculation
    LOG_W(PDCP, "result_ip: %04x\n", result_ip);
    LOG_W(PDCP, "checksum_cal_ip: %04x\n", checksum_cal_ip);

    // (2.5.5) Use the recalculated checksum to replace the one in the PDCP PDU
    buffer[pdu_start + 10] = (checksum_cal_ip >> 8) & 0xFF; // high byte
    buffer[pdu_start + 11] = checksum_cal_ip & 0xFF;        // low byte

    LOG_W(PDCP, "%s(): (Extended PDU with changed IP header checksum), (gNB:1, UE:0 %d)\n", __func__, entity->is_gnb);
    for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
        LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
    }
    LOG_W(PDCP, "\n");

    // Free memory of 'data3'
    free(data3);

  }


  /* --- The Bit-Flipping Identification ends here --- */


  if (rcvd_count < entity->rx_deliv
      || nr_pdcp_sdu_in_list(entity->rx_list, rcvd_count)) {
    LOG_W(PDCP, "discard NR PDU rcvd_count=%d, entity->rx_deliv %d,sdu_in_list %d\n", rcvd_count,entity->rx_deliv,nr_pdcp_sdu_in_list(entity->rx_list,rcvd_count));
    entity->stats.rxpdu_dd_pkts++;
    entity->stats.rxpdu_dd_bytes += size;

    return;
  }

  // LOG_W(PDCP, "%s(): (Check the buffer before SDU is built), (gNB:1, UE:0 %d)\n", __func__, entity->is_gnb);
  // for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
  //   LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
  // }
  // LOG_W(PDCP, "\n");

  sdu = nr_pdcp_new_sdu(rcvd_count,
                        (char *)buffer + header_size,
                        size - header_size - integrity_size,
                        &msg_integrity);
  entity->rx_list = nr_pdcp_sdu_list_add(entity->rx_list, sdu);
  entity->rx_size += size-header_size;

  if (rcvd_count >= entity->rx_next) {
    entity->rx_next = rcvd_count + 1;
  }

  // LOG_W(PDCP, "%s(): (Check the buffer after SDU is built), (gNB:1, UE:0 %d)\n", __func__, entity->is_gnb);
  // for (int k = 0; k < size - header_size - sdap_header_size; ++k) {
  //   LOG_W(PDCP, "%02x ", buffer[header_size + sdap_header_size + k]);
  // }
  // LOG_W(PDCP, "\n");

  /* TODO(?): out of order delivery */

  if (rcvd_count == entity->rx_deliv) {
    /* deliver all SDUs starting from rx_deliv up to discontinuity or end of list */
    uint32_t count = entity->rx_deliv;
    while (entity->rx_list != NULL && count == entity->rx_list->count) {
      nr_pdcp_sdu_t *cur = entity->rx_list;
      entity->deliver_sdu(entity->deliver_sdu_data, entity,
                          cur->buffer, cur->size,
                          &cur->msg_integrity);
      entity->rx_list = cur->next;
      entity->rx_size -= cur->size;
      entity->stats.txsdu_pkts++;
      entity->stats.txsdu_bytes += cur->size;

      nr_pdcp_free_sdu(cur);
      count++;
    }
    entity->rx_deliv = count;
    LOG_D(PDCP,
          "%s: entity (%s) %d - rx_deliv = %d, rcvd_sn = %d \n",
          __func__,
          entity->type == NR_PDCP_DRB_AM ? "DRB" : "SRB",
          entity->rb_id,
          entity->rx_deliv,
          rcvd_sn);
  }

  if (entity->t_reordering_start != 0 && entity->rx_deliv >= entity->rx_reord) {
    /* stop and reset t-Reordering */
    entity->t_reordering_start = 0;
  }

  if (entity->t_reordering_start == 0 && entity->rx_deliv < entity->rx_next) {
    entity->rx_reord = entity->rx_next;
    entity->t_reordering_start = entity->t_current;
  }
}

static int nr_pdcp_entity_process_sdu(nr_pdcp_entity_t *entity,
                                      char *buffer,
                                      int size,
                                      int sdu_id,
                                      char *pdu_buffer,
                                      int pdu_max_size)
{
  uint32_t count;
  int      sn;
  int      header_size;
  int      integrity_size;
  int      sdap_header_size = 0;
  char    *buf = pdu_buffer;
  DevAssert(nr_max_pdcp_pdu_size(size) <= pdu_max_size);
  int      dc_bit;

  if (entity->entity_suspended) {
    LOG_W(PDCP,
          "PDCP entity (%s) %d is suspended. Quit SDU processing.\n",
          entity->type > NR_PDCP_DRB_UM ? "SRB" : "DRB",
          entity->rb_id);
    return -1;
  }

  //JOON: Print the buffer content as hexadecimal
  LOG_W(PDCP, "%s(): (Plaintext), (gNB:1, UE:0 %d), (pdusessionID %d), (sdu_id %d)\n", __func__, entity->is_gnb, entity->pdusession_id, sdu_id);
  for (int i = 0; i < size; ++i) {
    LOG_W(PDCP, "%02x ", (unsigned char)buffer[i]);
  }
  LOG_W(PDCP, "\n");

  entity->stats.rxsdu_pkts++;
  entity->stats.rxsdu_bytes += size;


  count = entity->tx_next;
  sn = entity->tx_next & entity->sn_max;

  if (entity->has_sdap_tx) sdap_header_size = 1; // SDAP header is one byte

  /* D/C bit is only to be set for DRBs */
  if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM) {
    dc_bit = 0x80;
  } else {
    dc_bit = 0;
  }

  if (entity->sn_size == SHORT_SN_SIZE) {
    buf[0] = dc_bit | ((sn >> 8) & 0xf);
    buf[1] = sn & 0xff;
    header_size = SHORT_PDCP_HEADER_SIZE;
  } else {
    buf[0] = dc_bit | ((sn >> 16) & 0x3);
    buf[1] = (sn >> 8) & 0xff;
    buf[2] = sn & 0xff;
    header_size = LONG_PDCP_HEADER_SIZE;
  }

  /* SRBs always have MAC-I, even if integrity is not active */
  if (entity->has_integrity || entity->type == NR_PDCP_SRB) {
    integrity_size = PDCP_INTEGRITY_SIZE;
  } else {
    integrity_size = 0;
  }

  memcpy(buf + header_size, buffer, size);

  if (entity->has_integrity) {
    uint8_t integrity[PDCP_INTEGRITY_SIZE] = {0};
    entity->integrity(entity->integrity_context,
                      integrity,
                      (unsigned char *)buf, header_size + size,
                      entity->rb_id, count, entity->is_gnb ? 1 : 0);

    memcpy((unsigned char *)buf + header_size + size, integrity, PDCP_INTEGRITY_SIZE);
  } else if (integrity_size == PDCP_INTEGRITY_SIZE) {
    // set MAC-I to 0 for SRBs with integrity not active
    memset(buf + header_size + size, 0, PDCP_INTEGRITY_SIZE);
  }

  if (entity->has_ciphering) {

    /* Shuffling is added here: Start */    

    if (shuffle_enable == 0){

      //JOON: this is the plaintext
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "(Sender side before ciphering) %s():\n", __func__);
        for (int i = 0; i < size + integrity_size - sdap_header_size; ++i) {
          LOG_W(PDCP, "%02x ", ((unsigned char *)buf)[header_size + sdap_header_size + i]);
        }
        LOG_W(PDCP, "\n");
      }

      // Ciphering
      entity->cipher(entity->security_context,
                   (unsigned char *)buf + header_size + sdap_header_size,
                   size + integrity_size - sdap_header_size,
                   entity->rb_id, count, entity->is_gnb ? 1 : 0);

      //JOON: this is the ciphertext
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "(Sender side after ciphering) %s():\n", __func__);
        for (int k = 0; k < size + integrity_size - sdap_header_size; ++k) {
          LOG_W(PDCP, "%02x ", ((unsigned char *)buf)[header_size + sdap_header_size + k]);
        }
        LOG_W(PDCP, "\n");
      }

    }
    else if (shuffle_enable == 1){

      //JOON: this is the plaintext
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "(Sender side before ciphering) %s():\n", __func__);
        for (int i = 0; i < size + integrity_size - sdap_header_size; ++i) {
          LOG_W(PDCP, "%02x ", ((unsigned char *)buf)[header_size + sdap_header_size + i]);
        }
        LOG_W(PDCP, "\n");
      }

      //checksum starts from 26th byte, payload extends to the end
      uint8_t data_length = size + integrity_size - sdap_header_size - 26;

      LOG_W(PDCP, "(Process) data_length: %d, size: %d, integrity_size: %d, sdap_header_size: %d\n\n", data_length, size, integrity_size, sdap_header_size);

      //copy the checksum and payload part of the plaintext
      unsigned char *ptx_data = malloc(sizeof(char) * data_length);
      memcpy(ptx_data, &buf[header_size + sdap_header_size + 26], data_length);
      
      // if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
      //   LOG_W(PDCP, "(ptx) %s():\n", __func__);
      //   for (int k = 0; k < data_length; ++k) {
      //     LOG_W(PDCP, "%02x ", ptx_data[k]);
      //   }
      //   LOG_W(PDCP, "\n");
      // }

      entity->cipher(entity->security_context,
                    (unsigned char *)buf + header_size + sdap_header_size,
                    size + integrity_size - sdap_header_size,
                    entity->rb_id, count, entity->is_gnb ? 1 : 0);
    
      //JOON: this is the ciphertext
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "(Sender side after ciphering) %s():\n", __func__);
        for (int k = 0; k < size + integrity_size - sdap_header_size; ++k) {
          LOG_W(PDCP, "%02x ", ((unsigned char *)buf)[header_size + sdap_header_size + k]);
        }
        LOG_W(PDCP, "\n");
      }

      //copy the checksum and payload part of the ciphertext
      unsigned char *ctx_data = malloc(sizeof(char) * data_length);
      memcpy(ctx_data, &buf[header_size + sdap_header_size + 26], data_length);
    
      // if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
      //   LOG_W(PDCP, "(ctx) %s():\n", __func__);
      //   for (int k = 0; k < data_length; ++k) {
      //     LOG_W(PDCP, "%02x ", ctx_data[k]);
      //   }
      //   LOG_W(PDCP, "\n");
      // }


      //extract the keystream by XORing ptx and ctx
      unsigned char *keystream = malloc(sizeof(char) * data_length);
      for (int i = 0; i < data_length; i++) {
        keystream[i] = ptx_data[i] ^ ctx_data[i];
      }
      
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "(data part of keystream) %s():\n", __func__);
        for (int k = 0; k < data_length; ++k) {
          LOG_W(PDCP, "%02x ", keystream[k]);
        }
        LOG_W(PDCP, "\n");
      }

      //shuffle ctx based on the keystream
      unsigned char *temp = malloc(sizeof(char) * data_length);
      prp_permute_bits(&buf[header_size + sdap_header_size + 26], temp, data_length * 8, keystream, data_length);
      
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "Shuffled Ciphertext (Only checksum and data payload) %s():\n", __func__);
        for (int k = 0; k < data_length; ++k) {
          LOG_W(PDCP, "%02x ", temp[k]);
        }
        LOG_W(PDCP, "\n");
      }

      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        memcpy(&buf[header_size + sdap_header_size + 26], temp, data_length);
      }
        
      if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM){
        LOG_W(PDCP, "Shuffled Ciphertext (all) %s():\n", __func__);
        for (int k = 0; k < size + integrity_size - sdap_header_size; ++k) {
          LOG_W(PDCP, "%02x ", ((unsigned char *)buf)[header_size + sdap_header_size + k]);
        }
        LOG_W(PDCP, "\n");
      }

      // Clean up all allocated memory
      free(ptx_data);
      ptx_data = NULL;
      free(ctx_data);
      ctx_data = NULL;
      free(keystream);
      keystream = NULL;
      free(temp);
      temp = NULL;
    }


    /* Shuffling is added here: End */  

  }

  entity->tx_next++;

  entity->stats.txpdu_pkts++;
  entity->stats.txpdu_bytes += header_size + size + integrity_size;
  entity->stats.txpdu_sn = sn;

  return header_size + size + integrity_size;
}

static bool nr_pdcp_entity_check_integrity(struct nr_pdcp_entity_t *entity,
                                           const uint8_t *buffer,
                                           int buffer_size,
                                           const nr_pdcp_integrity_data_t *msg_integrity)
{
  if (!entity->has_integrity)
    return false;

  int header_size = msg_integrity->header_size;

  uint8_t b[buffer_size + header_size];

  for (int i = 0; i < header_size; i++)
    b[i] = msg_integrity->header[i];

  memcpy(b + header_size, buffer, buffer_size);

  unsigned char mac[4];
  entity->integrity(entity->integrity_context, mac,
                    b, buffer_size + header_size,
                    entity->rb_id, msg_integrity->count, entity->is_gnb ? 0 : 1);

  return memcmp(mac, msg_integrity->mac, 4) == 0;
}

/* may be called several times, take care to clean previous settings */
static void nr_pdcp_entity_set_security(struct nr_pdcp_entity_t *entity,
                                        const nr_pdcp_entity_security_keys_and_algos_t *parameters)
{
  if (parameters->integrity_algorithm != -1) {
    entity->security_keys_and_algos.integrity_algorithm = parameters->integrity_algorithm;
    memcpy(entity->security_keys_and_algos.integrity_key, parameters->integrity_key, NR_K_KEY_SIZE);
  }
  if (parameters->ciphering_algorithm != -1) {
    entity->security_keys_and_algos.ciphering_algorithm = parameters->ciphering_algorithm;
    memcpy(entity->security_keys_and_algos.ciphering_key, parameters->ciphering_key, NR_K_KEY_SIZE);
  }

  if (parameters->integrity_algorithm == 0) {
    entity->has_integrity = 0;
    if (entity->free_integrity != NULL)
      entity->free_integrity(entity->integrity_context);
    entity->free_integrity = NULL;
  }

  if (parameters->integrity_algorithm != 0 && parameters->integrity_algorithm != -1) {
    entity->has_integrity = 1;
    if (entity->free_integrity != NULL)
      entity->free_integrity(entity->integrity_context);
    if (parameters->integrity_algorithm == 2) {
      entity->integrity_context = nr_pdcp_integrity_nia2_init(entity->security_keys_and_algos.integrity_key);
      entity->integrity = nr_pdcp_integrity_nia2_integrity;
      entity->free_integrity = nr_pdcp_integrity_nia2_free_integrity;
    } else if (parameters->integrity_algorithm == 1) {
      entity->integrity_context = nr_pdcp_integrity_nia1_init(entity->security_keys_and_algos.integrity_key);
      entity->integrity = nr_pdcp_integrity_nia1_integrity;
      entity->free_integrity = nr_pdcp_integrity_nia1_free_integrity;
    } else {
      LOG_E(PDCP, "FATAL: only nia1 and nia2 supported for the moment\n");
      exit(1);
    }
  }

  if (parameters->ciphering_algorithm == 0) {
    entity->has_ciphering = 0;
    if (entity->free_security != NULL)
      entity->free_security(entity->security_context);
    entity->free_security = NULL;
  }

  if (parameters->ciphering_algorithm != 0 && parameters->ciphering_algorithm != -1) {
    entity->has_ciphering = 1;
    if (entity->free_security != NULL)
      entity->free_security(entity->security_context);
    if (parameters->ciphering_algorithm == 2) {
      entity->security_context = nr_pdcp_security_nea2_init(entity->security_keys_and_algos.ciphering_key);
      entity->cipher = nr_pdcp_security_nea2_cipher;
      entity->free_security = nr_pdcp_security_nea2_free_security;
    } else if (parameters->ciphering_algorithm == 1) {
      entity->security_context = nr_pdcp_security_nea1_init(entity->security_keys_and_algos.ciphering_key);
      entity->cipher = nr_pdcp_security_nea1_cipher;
      entity->free_security = nr_pdcp_security_nea1_free_security;
    } else {
      LOG_E(PDCP, "FATAL: only nea1 and nea2 supported for the moment\n");
      exit(1);
    }
  }
}

static void check_t_reordering(nr_pdcp_entity_t *entity)
{
  uint32_t count;

  /* if t_reordering is set to "infinity" (seen as -1) then do nothing */
  if (entity->t_reordering == -1)
    return;

  if (entity->t_reordering_start == 0
      || entity->t_current <= entity->t_reordering_start + entity->t_reordering)
    return;
  LOG_D(PDCP,
        "%s: entity (%s) %d: t_reordering_start = %ld, t_current = %ld, t_reordering = %d \n",
        __func__,
        entity->type > NR_PDCP_DRB_UM ? "SRB" : "DRB",
        entity->rb_id,
        entity->t_reordering_start,
        entity->t_current,
        entity->t_reordering);

  /* stop timer */
  entity->t_reordering_start = 0;

  /* deliver all SDUs with count < rx_reord */
  while (entity->rx_list != NULL && entity->rx_list->count < entity->rx_reord) {
    nr_pdcp_sdu_t *cur = entity->rx_list;
    entity->deliver_sdu(entity->deliver_sdu_data, entity,
                        cur->buffer, cur->size,
                        &cur->msg_integrity);
    entity->rx_list = cur->next;
    entity->rx_size -= cur->size;
    entity->stats.txsdu_pkts++;
    entity->stats.txsdu_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
  }

  /* deliver all SDUs starting from rx_reord up to discontinuity or end of list */
  count = entity->rx_reord;
  while (entity->rx_list != NULL && count == entity->rx_list->count) {
    nr_pdcp_sdu_t *cur = entity->rx_list;
    entity->deliver_sdu(entity->deliver_sdu_data, entity,
                        cur->buffer, cur->size,
                        &cur->msg_integrity);
    entity->rx_list = cur->next;
    entity->rx_size -= cur->size;
    entity->stats.txsdu_pkts++;
    entity->stats.txsdu_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
    count++;
  }

  entity->rx_deliv = count;

  if (entity->rx_deliv < entity->rx_next) {
    entity->rx_reord = entity->rx_next;
    entity->t_reordering_start = entity->t_current;
  }
}

static void nr_pdcp_entity_set_time(struct nr_pdcp_entity_t *entity, uint64_t now)
{
  entity->t_current = now;

  check_t_reordering(entity);
}

static void deliver_all_sdus(nr_pdcp_entity_t *entity)
{
  // deliver the PDCP SDUs stored in the receiving PDCP entity to upper layers
  while (entity->rx_list != NULL) {
    nr_pdcp_sdu_t *cur = entity->rx_list;
    entity->deliver_sdu(entity->deliver_sdu_data, entity,
                        cur->buffer, cur->size,
                        &cur->msg_integrity);
    entity->rx_list = cur->next;
    entity->rx_size -= cur->size;
    entity->stats.txsdu_pkts++;
    entity->stats.txsdu_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
  }
}

/**
 * @brief PDCP Entity Suspend according to 5.1.4 of 3GPP TS 38.323
 * Transmitting PDCP entity shall:
 * - set TX_NEXT to the initial value;
 * - discard all stored PDCP PDUs (NOTE: PDUs are stored in RLC)
 * Receiving PDCP entity shall:
 * - if t-Reordering is running:
 *   a) stop and reset t-Reordering;
 *   b) deliver all stored PDCP SDUs
 * - set RX_NEXT and RX_DELIV to the initial value.
 */
static void nr_pdcp_entity_suspend(nr_pdcp_entity_t *entity)
{
  /* Transmitting PDCP entity */
  entity->tx_next = 0;
  /* Receiving PDCP entity */
  if (entity->t_reordering_start != 0) {
    entity->t_reordering_start = 0;
    deliver_all_sdus(entity);
  }
  entity->rx_next = 0;
  entity->rx_deliv = 0;
  /* Flag to keep track of PDCP entity status */
  entity->entity_suspended = true;
}

static void free_rx_list(nr_pdcp_entity_t *entity)
{
  nr_pdcp_sdu_t *cur = entity->rx_list;
  while (cur != NULL) {
    nr_pdcp_sdu_t *next = cur->next;
    entity->stats.rxpdu_dd_pkts++;
    entity->stats.rxpdu_dd_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
    cur = next;
  }
  entity->rx_list = NULL;
  entity->rx_size = 0;
}

/**
 * @brief PDCP entity re-establishment according to 5.1.2 of 3GPP TS 38.323
 * @todo  deal with ciphering/integrity algos and keys for transmitting/receiving entity procedures
*/
static void nr_pdcp_entity_reestablish_drb_am(nr_pdcp_entity_t *entity,
                                              const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  /* transmitting entity procedures */
  /* do nothing */

  /* receiving entity procedures */
  /* do nothing */

  /* ciphering and integrity: common for both tx and rx entities */
  entity->set_security(entity, security_parameters);

  /* Flag PDCP entity as re-established */
  entity->entity_suspended = false;
}

static void nr_pdcp_entity_reestablish_drb_um(nr_pdcp_entity_t *entity,
                                              const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  /* transmitting entity procedures */
  entity->tx_next = 0;

  /* receiving entity procedures */
  /* deliver all SDUs if t_reordering is running */
  if (entity->t_reordering_start != 0)
    deliver_all_sdus(entity);
  /* stop t_reordering */
  entity->t_reordering_start = 0;
  /* set rx_next and rx_deliv to the initial value */
  entity->rx_next = 0;
  entity->rx_deliv = 0;

  /* ciphering and integrity: common for both tx and rx entities */
  entity->set_security(entity, security_parameters);

  /* Flag PDCP entity as re-established */
  entity->entity_suspended = false;
}

static void nr_pdcp_entity_reestablish_srb(nr_pdcp_entity_t *entity,
                                           const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  /* transmitting entity procedures */
  entity->tx_next = 0;

  /* receiving entity procedures */
  free_rx_list(entity);
  /* stop t_reordering */
  entity->t_reordering_start = 0;
  /* set rx_next and rx_deliv to the initial value */
  entity->rx_next = 0;
  entity->rx_deliv = 0;

  /* ciphering and integrity: common for both tx and rx entities */
  entity->set_security(entity, security_parameters);

  /* Flag PDCP entity as re-established */
  entity->entity_suspended = false;
}

static void nr_pdcp_entity_release(nr_pdcp_entity_t *entity)
{
  deliver_all_sdus(entity);
}

static void nr_pdcp_entity_delete(nr_pdcp_entity_t *entity)
{
  free_rx_list(entity);
  if (entity->free_security != NULL)
    entity->free_security(entity->security_context);
  if (entity->free_integrity != NULL)
    entity->free_integrity(entity->integrity_context);
  free(entity);
}

static void nr_pdcp_entity_get_stats(nr_pdcp_entity_t *entity,
                                     nr_pdcp_statistics_t *out)
{
  *out = entity->stats;
}


nr_pdcp_entity_t *new_nr_pdcp_entity(
    nr_pdcp_entity_type_t type,
    int is_gnb,
    int rb_id,
    int pdusession_id,
    bool has_sdap_rx,
    bool has_sdap_tx,
    void (*deliver_sdu)(void *deliver_sdu_data, struct nr_pdcp_entity_t *entity,
                        char *buf, int size,
                        const nr_pdcp_integrity_data_t *msg_integrity),
    void *deliver_sdu_data,
    void (*deliver_pdu)(void *deliver_pdu_data, ue_id_t ue_id, int rb_id,
                        char *buf, int size, int sdu_id),
    void *deliver_pdu_data,
    int sn_size,
    int t_reordering,
    int discard_timer,
    const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_entity_t *ret;

  ret = calloc(1, sizeof(nr_pdcp_entity_t));
  if (ret == NULL) {
    LOG_E(PDCP, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  ret->type = type;

  ret->recv_pdu        = nr_pdcp_entity_recv_pdu;
  ret->process_sdu     = nr_pdcp_entity_process_sdu;
  ret->set_security    = nr_pdcp_entity_set_security;
  ret->check_integrity = nr_pdcp_entity_check_integrity;
  ret->set_time        = nr_pdcp_entity_set_time;

  ret->delete_entity = nr_pdcp_entity_delete;
  ret->release_entity = nr_pdcp_entity_release;
  ret->suspend_entity = nr_pdcp_entity_suspend;

  switch (type) {
    case NR_PDCP_DRB_AM:
      ret->reestablish_entity = nr_pdcp_entity_reestablish_drb_am;
      break;
    case NR_PDCP_DRB_UM:
      ret->reestablish_entity = nr_pdcp_entity_reestablish_drb_um;
      break;
    case NR_PDCP_SRB:
      ret->reestablish_entity = nr_pdcp_entity_reestablish_srb;
      break;
  }
  
  ret->get_stats = nr_pdcp_entity_get_stats;
  ret->deliver_sdu = deliver_sdu;
  ret->deliver_sdu_data = deliver_sdu_data;

  ret->deliver_pdu = deliver_pdu;
  ret->deliver_pdu_data = deliver_pdu_data;

  ret->rb_id         = rb_id;
  ret->pdusession_id = pdusession_id;
  ret->has_sdap_rx   = has_sdap_rx;
  ret->has_sdap_tx   = has_sdap_tx;
  ret->sn_size       = sn_size;
  ret->t_reordering  = t_reordering;
  ret->discard_timer = discard_timer;

  ret->sn_max        = (1 << sn_size) - 1;
  ret->window_size   = 1 << (sn_size - 1);

  ret->is_gnb = is_gnb;

  nr_pdcp_entity_set_security(ret, security_parameters);

  return ret;
}
