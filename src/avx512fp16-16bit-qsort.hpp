/*******************************************************************
 * Copyright (C) 2022 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 * Authors: Raghuveer Devulapalli <raghuveer.devulapalli@intel.com>
 * ****************************************************************/

#ifndef AVX512FP16_QSORT_16BIT
#define AVX512FP16_QSORT_16BIT

#include "avx512-16bit-common.h"
#include "xss-network-qsort.hpp"

typedef union {
    _Float16 f_;
    uint16_t i_;
} Fp16Bits;

template <>
struct zmm_vector<_Float16> {
    using type_t = _Float16;
    using reg_t = __m512h;
    using halfreg_t = __m256h;
    using opmask_t = __mmask32;
    static const uint8_t numlanes = 32;
    static constexpr int network_sort_threshold = 128;
    static constexpr int partition_unroll_factor = 0;

    static __m512i get_network(int index)
    {
        return _mm512_loadu_si512(&network[index - 1][0]);
    }
    static type_t type_max()
    {
        Fp16Bits val;
        val.i_ = X86_SIMD_SORT_INFINITYH;
        return val.f_;
    }
    static type_t type_min()
    {
        Fp16Bits val;
        val.i_ = X86_SIMD_SORT_NEGINFINITYH;
        return val.f_;
    }
    static reg_t zmm_max()
    {
        return _mm512_set1_ph(type_max());
    }
    static opmask_t knot_opmask(opmask_t x)
    {
        return _knot_mask32(x);
    }
    static opmask_t ge(reg_t x, reg_t y)
    {
        return _mm512_cmp_ph_mask(x, y, _CMP_GE_OQ);
    }
    static opmask_t get_partial_loadmask(int size)
    {
        return (0x00000001 << size) - 0x00000001;
    }
    template <int type>
    static opmask_t fpclass(reg_t x)
    {
        return _mm512_fpclass_ph_mask(x, type);
    }
    static reg_t loadu(void const *mem)
    {
        return _mm512_loadu_ph(mem);
    }
    static reg_t max(reg_t x, reg_t y)
    {
        return _mm512_max_ph(x, y);
    }
    static void mask_compressstoreu(void *mem, opmask_t mask, reg_t x)
    {
        __m512i temp = _mm512_castph_si512(x);
        // AVX512_VBMI2
        return _mm512_mask_compressstoreu_epi16(mem, mask, temp);
    }
    static reg_t maskz_loadu(opmask_t mask, void const *mem)
    {
        return _mm512_castsi512_ph(_mm512_maskz_loadu_epi16(mask, mem));
    }
    static reg_t mask_loadu(reg_t x, opmask_t mask, void const *mem)
    {
        // AVX512BW
        return _mm512_castsi512_ph(
                _mm512_mask_loadu_epi16(_mm512_castph_si512(x), mask, mem));
    }
    static reg_t mask_mov(reg_t x, opmask_t mask, reg_t y)
    {
        return _mm512_castsi512_ph(_mm512_mask_mov_epi16(
                _mm512_castph_si512(x), mask, _mm512_castph_si512(y)));
    }
    static void mask_storeu(void *mem, opmask_t mask, reg_t x)
    {
        return _mm512_mask_storeu_epi16(mem, mask, _mm512_castph_si512(x));
    }
    static reg_t min(reg_t x, reg_t y)
    {
        return _mm512_min_ph(x, y);
    }
    static reg_t permutexvar(__m512i idx, reg_t zmm)
    {
        return _mm512_permutexvar_ph(idx, zmm);
    }
    static type_t reducemax(reg_t v)
    {
        return _mm512_reduce_max_ph(v);
    }
    static type_t reducemin(reg_t v)
    {
        return _mm512_reduce_min_ph(v);
    }
    static reg_t set1(type_t v)
    {
        return _mm512_set1_ph(v);
    }
    template <uint8_t mask>
    static reg_t shuffle(reg_t zmm)
    {
        __m512i temp = _mm512_shufflehi_epi16(_mm512_castph_si512(zmm),
                                              (_MM_PERM_ENUM)mask);
        return _mm512_castsi512_ph(
                _mm512_shufflelo_epi16(temp, (_MM_PERM_ENUM)mask));
    }
    static void storeu(void *mem, reg_t x)
    {
        return _mm512_storeu_ph(mem, x);
    }
    static reg_t reverse(reg_t zmm)
    {
        const auto rev_index = get_network(4);
        return permutexvar(rev_index, zmm);
    }
    static reg_t bitonic_merge(reg_t x)
    {
        return bitonic_merge_zmm_16bit<zmm_vector<type_t>>(x);
    }
    static reg_t sort_vec(reg_t x)
    {
        return sort_zmm_16bit<zmm_vector<type_t>>(x);
    }
};

template <>
bool is_a_nan<_Float16>(_Float16 elem)
{
    Fp16Bits temp;
    temp.f_ = elem;
    return (temp.i_ & 0x7c00) == 0x7c00;
}

template <>
void replace_inf_with_nan(_Float16 *arr, int64_t arrsize, int64_t nan_count)
{
    memset(arr + arrsize - nan_count, 0xFF, nan_count * 2);
}

/* Specialized template function for _Float16 qsort_*/
template <>
void avx512_qsort(_Float16 *arr, int64_t arrsize)
{
    if (arrsize > 1) {
        int64_t nan_count
                = replace_nan_with_inf<zmm_vector<_Float16>, _Float16>(arr,
                                                                       arrsize);
        qsort_<zmm_vector<_Float16>, _Float16>(
                arr, 0, arrsize - 1, 2 * (int64_t)log2(arrsize));
        replace_inf_with_nan(arr, arrsize, nan_count);
    }
}
#endif // AVX512FP16_QSORT_16BIT
