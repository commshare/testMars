/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// tensorflow/tensorflow/core/platform/cpu_info.h
// tensorflow/tensorflow/core/platform/cpu_feature_guard.h

#ifndef BUBBLEFS_PLATFORM_CPU_INFO_H_
#define BUBBLEFS_PLATFORM_CPU_INFO_H_

#include <endian.h> // for linux
#include <string>

namespace bubblefs {
namespace port {
  
// TODO(jeff,sanjay): Make portable
constexpr bool kLittleEndian = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;

// Mostly ISA related features that we care about
enum CPUFeature {
  // Do not change numeric assignments.
  MMX = 0,
  SSE = 1,
  SSE2 = 2,
  SSE3 = 3,
  SSSE3 = 4,
  SSE4_1 = 5,
  SSE4_2 = 6,
  CMOV = 7,
  CMPXCHG8B = 8,
  CMPXCHG16B = 9,
  POPCNT = 10,
  AES = 11,
  AVX = 12,
  RDRAND = 13,
  AVX2 = 14,
  FMA = 15,
  F16C = 16,
  PCLMULQDQ = 17,
  RDSEED = 18,
  ADX = 19,
  SMAP = 20,

  // Prefetch Vector Data Into Caches with Intent to Write and T1 Hint
  // http://www.felixcloutier.com/x86/PREFETCHWT1.html.
  // You probably want PREFETCHW instead.
  PREFETCHWT1 = 21,

  BMI1 = 22,
  BMI2 = 23,
  HYPERVISOR = 25,  // 0 when on a real CPU, 1 on (well-behaved) hypervisor.

  // Prefetch Data into Caches in Anticipation of a Write (3D Now!).
  // http://www.felixcloutier.com/x86/PREFETCHW.html
  PREFETCHW = 26,

  // AVX-512: 512-bit vectors (plus masking, etc.) in Knights Landing,
  // Skylake
  // Xeon, etc.; each of these entries is a different subset of
  // instructions,
  // various combinations of which occur on various CPU types.
  AVX512F = 27,        // Foundation
  AVX512CD = 28,       // Conflict detection
  AVX512ER = 29,       // Exponential and reciprocal
  AVX512PF = 30,       // Prefetching
  AVX512VL = 31,       // Shorter vector lengths
  AVX512BW = 32,       // Byte and word
  AVX512DQ = 33,       // Dword and qword
  AVX512VBMI = 34,     // Bit manipulation
  AVX512IFMA = 35,     // Integer multiply-add
  AVX512_4VNNIW = 36,  // Integer neural network
  AVX512_4FMAPS = 37,  // Floating point neural network
};

// Checks whether the current processor supports one of the features above.
// Checks CPU registers to return hardware capabilities.
bool TestCPUFeature(CPUFeature feature);

// Returns CPU Vendor string (i.e. 'GenuineIntel', 'AuthenticAMD', etc.)
std::string CPUVendorIDString();

// Returns CPU family.
int CPUFamily();

// Returns CPU model number.
int CPUModelNum();

// Returns an estimate of the number of schedulable CPUs for this
// process.  Usually, it's constant throughout the lifetime of a
// process, but it might change if the underlying cluster management
// software can change it dynamically.
int NumSchedulableCPUs();

// Called by the framework when we expect heavy CPU computation and we want to
// be sure that the code has been compiled to run optimally on the current
// hardware. The first time it's called it will run lightweight checks of
// available SIMD acceleration features and log warnings about any that aren't
// used.
void InfoAboutUnusedCPUFeatures();

/// @brief 获取当前进程的cpu使用时间
long long GetCurCpuTime();

/// @brief 获取整个系统的cpu使用时间
long long GetTotalCpuTime();

/// @brief 计算进程的cpu使用率
/// @param cur_cpu_time_start 当前进程cpu使用时间段-start
/// @param cur_cpu_time_stop 当前进程cpu使用时间段-stop
/// @param total_cpu_time_start 整个系统cpu使用时间段-start
/// @param total_cpu_time_start 整个系统cpu使用时间段-stop
float CalculateCurCpuUseage(long long cur_cpu_time_start, long long cur_cpu_time_stop,
    long long total_cpu_time_start, long long total_cpu_time_stop);

}  // namespace port
}  // namespace bubblefs

#endif  // BUBBLEFS_PLATFORM_CPU_INFO_H_