/*******************************************************************
 * Copyright (C) 2022 Intel Corporation
 * Copyright (C) 2021 Serge Sans Paille
 * SPDX-License-Identifier: BSD-3-Clause
 * Authors: Raghuveer Devulapalli <raghuveer.devulapalli@intel.com>
 *          Serge Sans Paille <serge.guelton@telecom-bretagne.eu>
 * ****************************************************************/
#ifndef AVX512_QSORT_32BIT
#define AVX512_QSORT_32BIT

#include "avx512-common-qsort.h"
#include "xss-network-qsort.hpp"

/*
 * Constants used in sorting 16 elements in a ZMM registers. Based on Bitonic
 * sorting network (see
 * https://en.wikipedia.org/wiki/Bitonic_sorter#/media/File:BitonicSort.svg)
 */
#define NETWORK_32BIT_1 14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1
#define NETWORK_32BIT_2 12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
#define NETWORK_32BIT_3 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7
#define NETWORK_32BIT_4 13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2
#define NETWORK_32BIT_5 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
#define NETWORK_32BIT_6 11, 10, 9, 8, 15, 14, 13, 12, 3, 2, 1, 0, 7, 6, 5, 4
#define NETWORK_32BIT_7 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8

template <typename vtype, typename reg_t>
X86_SIMD_SORT_INLINE reg_t sort_zmm_32bit(reg_t zmm);

template <typename vtype, typename reg_t>
X86_SIMD_SORT_INLINE reg_t bitonic_merge_zmm_32bit(reg_t zmm);

template <>
struct zmm_vector<int32_t> {
    using type_t = int32_t;
    using reg_t = __m512i;
    using halfreg_t = __m256i;
    using opmask_t = __mmask16;
    static const uint8_t numlanes = 16;
    static constexpr int network_sort_threshold = 256;
    static constexpr int partition_unroll_factor = 2;

    static type_t type_max()
    {
        return X86_SIMD_SORT_MAX_INT32;
    }
    static type_t type_min()
    {
        return X86_SIMD_SORT_MIN_INT32;
    }
    static reg_t zmm_max()
    {
        return _mm512_set1_epi32(type_max());
    }

    static opmask_t knot_opmask(opmask_t x)
    {
        return _mm512_knot(x);
    }
    static opmask_t ge(reg_t x, reg_t y)
    {
        return _mm512_cmp_epi32_mask(x, y, _MM_CMPINT_NLT);
    }
    template <int scale>
    static halfreg_t i64gather(__m512i index, void const *base)
    {
        return _mm512_i64gather_epi32(index, base, scale);
    }
    static reg_t merge(halfreg_t y1, halfreg_t y2)
    {
        reg_t z1 = _mm512_castsi256_si512(y1);
        return _mm512_inserti32x8(z1, y2, 1);
    }
    static reg_t loadu(void const *mem)
    {
        return _mm512_loadu_si512(mem);
    }
    static void mask_compressstoreu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_compressstoreu_epi32(mem, mask, x);
    }
    static reg_t mask_loadu(reg_t x, opmask_t mask, void const *mem)
    {
        return _mm512_mask_loadu_epi32(x, mask, mem);
    }
    static reg_t mask_mov(reg_t x, opmask_t mask, reg_t y)
    {
        return _mm512_mask_mov_epi32(x, mask, y);
    }
    static void mask_storeu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_storeu_epi32(mem, mask, x);
    }
    static reg_t min(reg_t x, reg_t y)
    {
        return _mm512_min_epi32(x, y);
    }
    static reg_t max(reg_t x, reg_t y)
    {
        return _mm512_max_epi32(x, y);
    }
    static reg_t permutexvar(__m512i idx, reg_t zmm)
    {
        return _mm512_permutexvar_epi32(idx, zmm);
    }
    static type_t reducemax(reg_t v)
    {
        return _mm512_reduce_max_epi32(v);
    }
    static type_t reducemin(reg_t v)
    {
        return _mm512_reduce_min_epi32(v);
    }
    static reg_t set1(type_t v)
    {
        return _mm512_set1_epi32(v);
    }
    template <uint8_t mask>
    static reg_t shuffle(reg_t zmm)
    {
        return _mm512_shuffle_epi32(zmm, (_MM_PERM_ENUM)mask);
    }
    static void storeu(void *mem, reg_t x)
    {
        return _mm512_storeu_si512(mem, x);
    }

    static halfreg_t max(halfreg_t x, halfreg_t y)
    {
        return _mm256_max_epi32(x, y);
    }
    static halfreg_t min(halfreg_t x, halfreg_t y)
    {
        return _mm256_min_epi32(x, y);
    }
    static reg_t reverse(reg_t zmm)
    {
        const auto rev_index = _mm512_set_epi32(NETWORK_32BIT_5);
        return permutexvar(rev_index, zmm);
    }
    static reg_t bitonic_merge(reg_t x)
    {
        return bitonic_merge_zmm_32bit<zmm_vector<type_t>>(x);
    }
    static reg_t sort_vec(reg_t x)
    {
        return sort_zmm_32bit<zmm_vector<type_t>>(x);
    }
};
template <>
struct zmm_vector<uint32_t> {
    using type_t = uint32_t;
    using reg_t = __m512i;
    using halfreg_t = __m256i;
    using opmask_t = __mmask16;
    static const uint8_t numlanes = 16;
    static constexpr int network_sort_threshold = 256;
    static constexpr int partition_unroll_factor = 2;

    static type_t type_max()
    {
        return X86_SIMD_SORT_MAX_UINT32;
    }
    static type_t type_min()
    {
        return 0;
    }
    static reg_t zmm_max()
    {
        return _mm512_set1_epi32(type_max());
    } // TODO: this should broadcast bits as is?

    template <int scale>
    static halfreg_t i64gather(__m512i index, void const *base)
    {
        return _mm512_i64gather_epi32(index, base, scale);
    }
    static reg_t merge(halfreg_t y1, halfreg_t y2)
    {
        reg_t z1 = _mm512_castsi256_si512(y1);
        return _mm512_inserti32x8(z1, y2, 1);
    }
    static opmask_t knot_opmask(opmask_t x)
    {
        return _mm512_knot(x);
    }
    static opmask_t ge(reg_t x, reg_t y)
    {
        return _mm512_cmp_epu32_mask(x, y, _MM_CMPINT_NLT);
    }
    static reg_t loadu(void const *mem)
    {
        return _mm512_loadu_si512(mem);
    }
    static reg_t max(reg_t x, reg_t y)
    {
        return _mm512_max_epu32(x, y);
    }
    static void mask_compressstoreu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_compressstoreu_epi32(mem, mask, x);
    }
    static reg_t mask_loadu(reg_t x, opmask_t mask, void const *mem)
    {
        return _mm512_mask_loadu_epi32(x, mask, mem);
    }
    static reg_t mask_mov(reg_t x, opmask_t mask, reg_t y)
    {
        return _mm512_mask_mov_epi32(x, mask, y);
    }
    static void mask_storeu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_storeu_epi32(mem, mask, x);
    }
    static reg_t min(reg_t x, reg_t y)
    {
        return _mm512_min_epu32(x, y);
    }
    static reg_t permutexvar(__m512i idx, reg_t zmm)
    {
        return _mm512_permutexvar_epi32(idx, zmm);
    }
    static type_t reducemax(reg_t v)
    {
        return _mm512_reduce_max_epu32(v);
    }
    static type_t reducemin(reg_t v)
    {
        return _mm512_reduce_min_epu32(v);
    }
    static reg_t set1(type_t v)
    {
        return _mm512_set1_epi32(v);
    }
    template <uint8_t mask>
    static reg_t shuffle(reg_t zmm)
    {
        return _mm512_shuffle_epi32(zmm, (_MM_PERM_ENUM)mask);
    }
    static void storeu(void *mem, reg_t x)
    {
        return _mm512_storeu_si512(mem, x);
    }

    static halfreg_t max(halfreg_t x, halfreg_t y)
    {
        return _mm256_max_epu32(x, y);
    }
    static halfreg_t min(halfreg_t x, halfreg_t y)
    {
        return _mm256_min_epu32(x, y);
    }
    static reg_t reverse(reg_t zmm)
    {
        const auto rev_index = _mm512_set_epi32(NETWORK_32BIT_5);
        return permutexvar(rev_index, zmm);
    }
    static reg_t bitonic_merge(reg_t x)
    {
        return bitonic_merge_zmm_32bit<zmm_vector<type_t>>(x);
    }
    static reg_t sort_vec(reg_t x)
    {
        return sort_zmm_32bit<zmm_vector<type_t>>(x);
    }
};
template <>
struct zmm_vector<float> {
    using type_t = float;
    using reg_t = __m512;
    using halfreg_t = __m256;
    using opmask_t = __mmask16;
    static const uint8_t numlanes = 16;
    static constexpr int network_sort_threshold = 256;
    static constexpr int partition_unroll_factor = 2;

    static type_t type_max()
    {
        return X86_SIMD_SORT_INFINITYF;
    }
    static type_t type_min()
    {
        return -X86_SIMD_SORT_INFINITYF;
    }
    static reg_t zmm_max()
    {
        return _mm512_set1_ps(type_max());
    }

    static opmask_t knot_opmask(opmask_t x)
    {
        return _mm512_knot(x);
    }
    static opmask_t ge(reg_t x, reg_t y)
    {
        return _mm512_cmp_ps_mask(x, y, _CMP_GE_OQ);
    }
    static opmask_t get_partial_loadmask(int size)
    {
        return (0x0001 << size) - 0x0001;
    }
    template <int type>
    static opmask_t fpclass(reg_t x)
    {
        return _mm512_fpclass_ps_mask(x, type);
    }
    template <int scale>
    static halfreg_t i64gather(__m512i index, void const *base)
    {
        return _mm512_i64gather_ps(index, base, scale);
    }
    static reg_t merge(halfreg_t y1, halfreg_t y2)
    {
        reg_t z1 = _mm512_castsi512_ps(
                _mm512_castsi256_si512(_mm256_castps_si256(y1)));
        return _mm512_insertf32x8(z1, y2, 1);
    }
    static reg_t loadu(void const *mem)
    {
        return _mm512_loadu_ps(mem);
    }
    static reg_t max(reg_t x, reg_t y)
    {
        return _mm512_max_ps(x, y);
    }
    static void mask_compressstoreu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_compressstoreu_ps(mem, mask, x);
    }
    static reg_t maskz_loadu(opmask_t mask, void const *mem)
    {
        return _mm512_maskz_loadu_ps(mask, mem);
    }
    static reg_t mask_loadu(reg_t x, opmask_t mask, void const *mem)
    {
        return _mm512_mask_loadu_ps(x, mask, mem);
    }
    static reg_t mask_mov(reg_t x, opmask_t mask, reg_t y)
    {
        return _mm512_mask_mov_ps(x, mask, y);
    }
    static void mask_storeu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_storeu_ps(mem, mask, x);
    }
    static reg_t min(reg_t x, reg_t y)
    {
        return _mm512_min_ps(x, y);
    }
    static reg_t permutexvar(__m512i idx, reg_t zmm)
    {
        return _mm512_permutexvar_ps(idx, zmm);
    }
    static type_t reducemax(reg_t v)
    {
        return _mm512_reduce_max_ps(v);
    }
    static type_t reducemin(reg_t v)
    {
        return _mm512_reduce_min_ps(v);
    }
    static reg_t set1(type_t v)
    {
        return _mm512_set1_ps(v);
    }
    template <uint8_t mask>
    static reg_t shuffle(reg_t zmm)
    {
        return _mm512_shuffle_ps(zmm, zmm, (_MM_PERM_ENUM)mask);
    }
    static void storeu(void *mem, reg_t x)
    {
        return _mm512_storeu_ps(mem, x);
    }

    static halfreg_t max(halfreg_t x, halfreg_t y)
    {
        return _mm256_max_ps(x, y);
    }
    static halfreg_t min(halfreg_t x, halfreg_t y)
    {
        return _mm256_min_ps(x, y);
    }
    static reg_t reverse(reg_t zmm)
    {
        const auto rev_index = _mm512_set_epi32(NETWORK_32BIT_5);
        return permutexvar(rev_index, zmm);
    }
    static reg_t bitonic_merge(reg_t x)
    {
        return bitonic_merge_zmm_32bit<zmm_vector<type_t>>(x);
    }
    static reg_t sort_vec(reg_t x)
    {
        return sort_zmm_32bit<zmm_vector<type_t>>(x);
    }
};

/*
 * Assumes zmm is random and performs a full sorting network defined in
 * https://en.wikipedia.org/wiki/Bitonic_sorter#/media/File:BitonicSort.svg
 */
template <typename vtype, typename reg_t = typename vtype::reg_t>
X86_SIMD_SORT_INLINE reg_t sort_zmm_32bit(reg_t zmm)
{
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(2, 3, 0, 1)>(zmm),
            0xAAAA);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(0, 1, 2, 3)>(zmm),
            0xCCCC);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(2, 3, 0, 1)>(zmm),
            0xAAAA);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::permutexvar(_mm512_set_epi32(NETWORK_32BIT_3), zmm),
            0xF0F0);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(1, 0, 3, 2)>(zmm),
            0xCCCC);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(2, 3, 0, 1)>(zmm),
            0xAAAA);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::permutexvar(_mm512_set_epi32(NETWORK_32BIT_5), zmm),
            0xFF00);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::permutexvar(_mm512_set_epi32(NETWORK_32BIT_6), zmm),
            0xF0F0);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(1, 0, 3, 2)>(zmm),
            0xCCCC);
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(2, 3, 0, 1)>(zmm),
            0xAAAA);
    return zmm;
}

// Assumes zmm is bitonic and performs a recursive half cleaner
template <typename vtype, typename reg_t = typename vtype::reg_t>
X86_SIMD_SORT_INLINE reg_t bitonic_merge_zmm_32bit(reg_t zmm)
{
    // 1) half_cleaner[16]: compare 1-9, 2-10, 3-11 etc ..
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::permutexvar(_mm512_set_epi32(NETWORK_32BIT_7), zmm),
            0xFF00);
    // 2) half_cleaner[8]: compare 1-5, 2-6, 3-7 etc ..
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::permutexvar(_mm512_set_epi32(NETWORK_32BIT_6), zmm),
            0xF0F0);
    // 3) half_cleaner[4]
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(1, 0, 3, 2)>(zmm),
            0xCCCC);
    // 3) half_cleaner[1]
    zmm = cmp_merge<vtype>(
            zmm,
            vtype::template shuffle<SHUFFLE_MASK(2, 3, 0, 1)>(zmm),
            0xAAAA);
    return zmm;
}

#endif //AVX512_QSORT_32BIT
