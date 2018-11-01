/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// caffe2/caffe2/core/context.h
// caffe2/caffe2/core/context.cc

#ifndef BUBBLEFS_UTILS_CAFFE2_CONTEXT_H_
#define BUBBLEFS_UTILS_CAFFE2_CONTEXT_H_

#include <atomic>
#include <cstdlib>
#include <ctime>
#include <random>
#include <unordered_map>

#include "platform/base_error.h"
#include "utils/caffe2_allocator.h"
#include "utils/caffe2_event.h"
#include "utils/caffe2_typeid.h"
#include "utils/caffe2_proto_caffe2.h"

namespace bubblefs {
namespace mycaffe2 {

bool FLAGS_caffe2_report_cpu_memory_usage;  
  
/**
 * A function to generate a random number seed that is unique in a best-effort
 * basis, using an ever-incrementing seed and the current time.
 */
uint32_t RandomNumberSeed();

/**
 * The CPU Context, representing the bare minimum of what a Context class in
 * Caffe2 should implement.
 *
 * See operator.h, especially Operator<Context>, for how Context are used in
 * actual operator implementations that are associated with specific devices.
 * In general, the Context class is passed in as a template argument, and
 * the operator can use the functions defined in the context to execute whatever
 * computation it has.
 *
 * A Context defines all the necessities to run an operator on a specific
 * device. Specific Context classes have the freedom to choose what functions it
 * implements, but there are a few functions that you should consider
 * implementing if you want to write your own context class:
 * - void SwitchToDevice(): any necessary code to switch to the device before
 *     running anything.
 * - void WaitEvent(const Event& ev): make the current context to wait on
 *     an event. For example, for cuda, this is the equivalent of
 *     cudaStreamWaitEvent. For CPU context, it essentially synchronizes the
 *     event.
 * - void Record(Event* ev): record the async activities on the current context
 *     to the event. For example, for cuda, this is the equivalent of
 *     cudaEventRecord on the current stream. For CPU context, it is always
 *     synchronous.
 * - void FinishDeviceComputation(): any wrapping-up work after all the
 *     computation of the operator is done. If there are errors during the
 *     execution, throw exception. For example, in a CUDAContext, this function
 *     carries out a stream synchronization and spots potential errors for
 *     the cuda kernel calls.
 * - static std::pair<void*, MemoryDeleter> New(size_t nbytes): allocates
       memory and returns a deleter.
 * - template <class SrcContext, class DstContext> void CopyBytes(...): does
 *     cross context memory copy.
 * - template <typename T, class SrcContext, class DstContext> void Copy(...):
 *     usually a simple wrapper around the above CopyBytes function.
 *
 * We intentionally did not create a base class for the various possible Context
 * classes there might be, since they are intended to be specified during
 * compile time using templates rather than via polymorphism. You should also
 * not have classes derived from existing context classes.
 */
class CPUContext final {
 public:
  typedef std::mt19937 rand_gen_type;
  CPUContext() : random_seed_(RandomNumberSeed()) {}
  explicit CPUContext(const DeviceOption& option)
      : random_seed_(
            option.random_seed ? option.random_seed
                                     : RandomNumberSeed()) {
    PANIC_ENFORCE_EQ(option.device_type(), CPU);
  }

  ~CPUContext() noexcept {}

  inline void SwitchToDevice(int /*stream_id*/) {}
  inline void SwitchToDevice() {
    SwitchToDevice(0);
  }

  inline void WaitEvent(const Event& ev) {
    ev.Wait(CPU, this);
  }

  inline void Record(Event* ev, const char* err_msg = nullptr) const {
    PANIC_ENFORCE(ev, "Event must not be null.");
    ev->Record(CPU, this, err_msg);
  }

  /**
   * A struct to host thread-local cuda objects.
   *
   * In Caffe2, each thread has its own non-default cuda stream as well as
   * related objects such as cublas and curand handles. This is achieved by
   * having the ThreadLocalCUDAObjects wrapper that takes care of allocating
   * and deallocating these objects at the thread scope. This class is solely
   * used inside CUDAContext and should not be used externally.s
   * ThreadLocalCUDAObjects:
   *   vector<cudaStream_t> cuda_streams_[CAFFE2_COMPILE_TIME_MAX_GPUS];
   *   vector<cublasHandle_t> cublas_handles_[CAFFE2_COMPILE_TIME_MAX_GPUS];
   *   vector<cudnnHandle_t> cudnn_handles_[CAFFE2_COMPILE_TIME_MAX_GPUS];
   * 
   * static thread_local ThreadLocalCUDAObjects cuda_objects_;
   * cublasHandle_t cblast = cuda_objects_.GetHandle(gpu_id_, stream_id_);
   * cudnnHandle_t cdnnt = cuda_objects_.GetCudnnHandle(gpu_id_, stream_id_);
   * cudaStream_t cs = cuda_objects_.GetStream(gpu_id, stream_id);
   * cudaStreamSynchronize(cs);
   * cudaError_t error = cudaGetLastError();
   */
  inline void FinishDeviceComputation() {}

  inline rand_gen_type& RandGenerator() {
    if (!random_generator_.get()) {
      random_generator_.reset(new rand_gen_type(random_seed_));
    }
    return *random_generator_.get();
  }

  static std::pair<void*, MemoryDeleter> New(size_t nbytes) {
    auto data_and_deleter = GetCPUAllocator()->New(nbytes);
    if (FLAGS_caffe2_report_cpu_memory_usage) {
      reporter_.New(data_and_deleter.first, nbytes);
      data_and_deleter.second = ReportAndDelete;
    }
    return data_and_deleter;
  }

  // Two copy functions that deals with cross-device copies.
  template <class SrcContext, class DstContext>
  inline void CopyBytes(size_t nbytes, const void* src, void* dst);

  template <typename T, class SrcContext, class DstContext>
  inline void Copy(size_t n, const T* src, T* dst) {
    if (std::is_fundamental<T>::value) {
      CopyBytes<SrcContext, DstContext>(
          n * sizeof(T),
          static_cast<const void*>(src),
          static_cast<void*>(dst));
    } else {
      for (int i = 0; i < n; ++i) {
        dst[i] = src[i];
      }
    }
  }

  template <class SrcContext, class DstContext>
  inline void
  CopyItems(const TypeMeta& meta, size_t n, const void* src, void* dst) {
    if (meta.copy()) {
      meta.copy()(src, dst, n);
    } else {
      CopyBytes<SrcContext, DstContext>(n * meta.itemsize(), src, dst);
    }
  }

  // By default CPU operators don't have async device parts
  static bool HasAsyncPartDefault() {
    return false;
  }

  static bool SupportsAsyncScheduling() {
    return false;
  }

  // CPU streams are not implemented and are silently ignored by CPU ops,
  // return true to signal executor to schedule a CPU op
  static bool IsStreamFree(const DeviceOption& /* unused */, int /* unused */) {
    return true;
  }

 protected:
  // TODO(jiayq): instead of hard-coding a generator, make it more flexible.
  int random_seed_{1701};
  std::unique_ptr<rand_gen_type> random_generator_;
  static MemoryAllocationReporter reporter_;

 private:
  static void ReportAndDelete(void* ptr) {
    reporter_.Delete(ptr);
    GetCPUAllocator()->GetDeleter()(ptr);
  }
};

template<>
inline void CPUContext::CopyBytes<CPUContext, CPUContext>(
    size_t nbytes, const void* src, void* dst) {
  if (nbytes == 0) {
    return;
  }
  PANIC_ENFORCE(src, "src is NULL");
  PANIC_ENFORCE(dst, "dst is NULL");
  memcpy(dst, src, nbytes);
}

uint32_t RandomNumberSeed() {
  // Originally copied from folly::randomNumberSeed (at 418ad4)
  // modified to use chrono instead of sys/time.h
  static std::atomic<uint32_t> seedInput(0);
  auto tv = std::chrono::system_clock::now().time_since_epoch();
  uint64_t usec = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(tv).count());
  uint32_t tv_sec = usec / 1000000;
  uint32_t tv_usec = usec % 1000000;
  const uint32_t kPrime0 = 51551;
  const uint32_t kPrime1 = 61631;
  const uint32_t kPrime2 = 64997;
  const uint32_t kPrime3 = 111857;
  return kPrime0 * (seedInput++) + kPrime1 * static_cast<uint32_t>(getpid()) +
      kPrime2 * tv_sec + kPrime3 * tv_usec;
}

}  // namespace mycaffe2
}  // namespace bubblefs

#endif  // BUBBLEFS_UTILS_CAFFE2_CONTEXT_H_