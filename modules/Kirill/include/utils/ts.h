//
// Created by kirill on 17.09.17.
//

#ifndef HEAT_EQUATION_TS_H_H
#define HEAT_EQUATION_TS_H_H

#include <mpi.h>
#include <math.h>

#define ROOT 0
#define NR 7
#define ALIGNMENT 32

//  Load settings
#define ENABLE_PARALLEL 0
#define AVX2_RUN        0
#define MKL_RUN         0
#define CPUGPU_RUN      0
#define FPGA_RUN        1

#define DOUBLE_TYPE

// Load paths
#define INPUT_EULER_SETTING_PATH    "/home/kirill/projects/heat-equation/initial/setting3.ini"
#define INPUT_EULER_FUNCTION_PATH   "/home/kirill/projects/heat-equation/initial/function3.txt"
#define RESULT_EULER_PATH           "/home/kirill/projects/heat-equation/result/Kirill/euler3D_double_fpga.txt"
#define KERNEL_CL_PATH              "/home/kirill/projects/heat-equation/build_clion_release/modules/Kirill/src/kernel.aocx"


#if AVX2_RUN
#include <immintrin.h>
#endif

#if MKL_RUN
#include <mkl.h>
#endif

#if FPGA_RUN || CPUGPU_RUN
#include "opencl.h"
#endif

#if defined(COMPLEX_TYPE)

#include <complex.h>
#define DEF_TYPE COMPLEX
typedef complex TYPE;


#elif defined(FLOAT_TYPE)

#define MPI_TYPE MPI_FLOAT
#define PRI_TYPE "f"
#define PRIE_TYPE ".7e"
#define ABS(x) fabsf(x)

#define LENVEC 8
#define m_real __m256
#define m_ind __m256i

#define mm_set1(a)                 _mm256_set1_ps(a)
#define mm_load(vec)               _mm256_load_ps(vec)
#define mm_load_si(vec)            _mm256_load_si256((const __m256i *)(vec))
#define mm_set_epi32(j)            _mm256_set_epi32(j+NR*7, j+NR*6, j+NR*5, j+NR*4, j+NR*3, j+NR*2, j+NR*1, j+NR*0)
#define mm_i32gather(val, vindex)  _mm256_i32gather_ps(val, vindex, 4)
#define mm_fmadd(a, b, c)          _mm256_fmadd_ps(a, b, c)
#define mm_hadd(a,b)               _mm256_hadd_ps(a,b)
#define mm_stream(out, in)         _mm256_store_ps(out, in)
#define mm_mul(a,b)                _mm256_mul_ps(a,b)
typedef float TYPE;

#elif defined(DOUBLE_TYPE)

#define MPI_TYPE MPI_DOUBLE
#define PRI_TYPE "lf"
#define PRIE_TYPE ".15le"
#define ABS(x) fabs(x)

#define LENVEC 4
#define m_real __m256d
#define m_ind __m128i

#define mm_set1(a)                _mm256_set1_pd(a)
#define mm_load(vec)              _mm256_loadu_pd(vec)
#define mm_load_si(vec)           _mm_load_si128((const __m128i*)(vec))
#define mm_set_epi32(j)           _mm_setr_epi32(j+NR*0, j+NR*1, j+NR*2, j+NR*3)
#define mm_i32gather(val, vindex) _mm256_i32gather_pd(val, vindex, 8)
#define mm_fmadd(a, b, c)         _mm256_fmadd_pd(a, b, c)
#define mm_hadd(a,b)              _mm256_hadd_pd(a,b)
#define mm_stream(out, in)        _mm256_storeu_pd(out, in)
#define mm_mul(a,b)               _mm256_mul_pd(a,b)

typedef double TYPE;

#endif

typedef struct {
  double start;
  double finish;
} t_timer;

static t_timer timer;

#endif //HEAT_EQUATION_TS_H_H
