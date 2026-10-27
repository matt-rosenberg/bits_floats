#include "assignment.h"

#include <math.h>
#include <stddef.h>

unsigned bitset_symmetric_difference_count(uint32_t a, uint32_t b) {
    unsigned int symmetric_difference = a ^ b;
    unsigned int count = 0;
    for (int i = 0; i < 32; i++){
        if (symmetric_difference & 1){
            count++;
        }
        symmetric_difference = symmetric_difference >> 1;
    }
    return count;
}

uint32_t bit_range_mask(unsigned low, unsigned high) {
    uint32_t value = ~0u;
    value = value >> (32 - (high - low + 1));
    value = value << (low);
    return value;
}

uint32_t splice_bits(
    uint32_t destination,
    uint32_t source,
    unsigned low,
    unsigned high
) {
    uint32_t mask = bit_range_mask(low, high);
    destination = destination & ~mask;
    uint32_t low_source_bits = (source << low) & mask;
    return destination | low_source_bits;
}

int32_t decode_twos_complement(uint32_t bits, unsigned width) {
    uint32_t mask = bit_range_mask(0, width - 1);
    bits = bits & mask; //bits becomes the low width bits
    uint32_t signBit = (bit_range_mask(width-1, width - 1)); //all 0s except the signbit
    bits = bits ^ signBit;
    return (bits - signBit);
}

bool add_signed_width(
    uint32_t a_bits,
    uint32_t b_bits,
    unsigned width,
    uint32_t *sum_bits
) {
    uint32_t mask = bit_range_mask(0, width - 1);
    uint32_t sign_mask = bit_range_mask(width - 1, width - 1);
    uint32_t a = a_bits & mask;
    uint32_t b = b_bits & mask;
    uint32_t sum = (a + b) & mask;
    *sum_bits = sum;

    uint32_t sign_a = a & sign_mask;
    uint32_t sign_b = b & sign_mask;
    uint32_t sign_sum = sum & sign_mask;
    return (sign_a == sign_b) && (sign_sum != sign_a);
}

int compare_signed_unsigned_math(
    int32_t signed_value,
    uint32_t unsigned_value
) {
    if (signed_value < 0){ //this does signed int comparison
        return -1;
    }
    uint32_t s = (uint32_t) signed_value;
    if (s < unsigned_value){
        return -1;
    }
    if (s > unsigned_value){
        return 1;
    }
    return 0;
}

uint32_t multiply_by_24(uint32_t x) {
    uint32_t times_32 = x << 5;
    uint32_t times_8 = x << 3;
    return times_32 - times_8;
}

fp32_info_t fp32_decode(uint32_t bits) {
    uint32_t sign_bit_mask = bit_range_mask(31, 31);
    uint32_t sign_bit = bits & sign_bit_mask;
    uint32_t exponent_bits_mask = bit_range_mask(23, 30);
    uint8_t exponent_bits = ((bits & exponent_bits_mask) >> 23);
    uint32_t fraction_bits_mask = bit_range_mask(0,22);
    uint32_t fraction_bits = bits & fraction_bits_mask;
    fp32_kind_t kind = FP32_NORMAL;
    int unbiased_exponent = exponent_bits - 127;
    if (exponent_bits == 0 && fraction_bits == 0){
        kind = FP32_ZERO;
        unbiased_exponent = -126;
    } else if (exponent_bits == 0){
        kind = FP32_SUBNORMAL;
        unbiased_exponent = -126;
    } else if (exponent_bits == 255 && fraction_bits == 0){
        kind = FP32_INFINITY;
        unbiased_exponent = 0;
    } else if (exponent_bits == 255){
        kind = FP32_NAN;
        unbiased_exponent = 0;
    }
    

    fp32_info_t info = {
        .negative = sign_bit,
        .kind = kind,
        .exponent_bits = exponent_bits,
        .fraction_bits = fraction_bits,
        .unbiased_exponent = unbiased_exponent,
    };
    return info;
}

double fp32_exact_value(uint32_t bits) {
    fp32_info_t info = fp32_decode(bits);
    if (info.kind == FP32_NAN) {
        return NAN;
    }
    if (info.kind == FP32_INFINITY) {
        return info.negative ? -INFINITY : INFINITY;
    }
    double value;
    if (info.kind == FP32_NORMAL) {
        uint32_t significand = bit_range_mask(23, 23) | info.fraction_bits;
        value = ldexp((double)significand, info.unbiased_exponent - 23);
    } else {
        value = ldexp((double)info.fraction_bits, -126 - 23);
    }
    if (info.negative) {
        value = -value;
    }
    return value;
}

uint32_t fp32_next_up_bits(uint32_t bits) {
    uint32_t sign_mask = bit_range_mask(31, 31);
    uint32_t exponent_mask = bit_range_mask(23, 30);
    uint32_t fraction_mask = bit_range_mask(0, 22);
    uint32_t sign = bits & sign_mask;
    uint32_t exponent = bits & exponent_mask;
    uint32_t fraction = bits & fraction_mask;

    if (exponent == exponent_mask){
        if (fraction != 0) {
            return bits;
        }
        if (sign == 0) {
            return bits;
        }
    }
    if ((bits & ~sign_mask) == 0) {
        return 1;
    }
    if (sign == 0) {
        return bits + 1;
    } else {
        return bits - 1;
    }
}

bool fp32_truncate_to_int(uint32_t bits, int32_t *out) {
    uint32_t sign = bits & bit_range_mask(31, 31);
    uint32_t exponent = (bits >> 23) & bit_range_mask(0, 7);
    uint32_t fraction = bits & bit_range_mask(0, 22);
    if (exponent == 255) {
        return false;
    }
    if (exponent == 0) {
        *out = 0;
        return true;
    }
    int unbiased_exponent = (int)exponent - 127;
    if (unbiased_exponent < 0) {
        *out = 0;
        return true;
    }
    if (unbiased_exponent >= 31) {
        if (unbiased_exponent == 31 && fraction == 0 && sign != 0) {
            *out = INT32_MIN;
            return true;
        }
        return false;
    }
    uint32_t significand = bit_range_mask(23, 23) | fraction;
    uint32_t magnitude;
    if (unbiased_exponent >= 23) {
        magnitude = significand << (unbiased_exponent - 23);
    } else {
        magnitude = significand >> (23 - unbiased_exponent);
    }
    if (sign != 0) {
        *out = -(int32_t)magnitude;
    } else {
        *out = (int32_t)magnitude;
    }
    return true;
}
