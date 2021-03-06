// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for arm independent of OS goes here.

#include "v8.h"

#if V8_TARGET_ARCH_ARM64

#include "arm64/cpu-arm64.h"
#include "arm64/utils-arm64.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool CpuFeatures::initialized_ = false;
#endif
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::found_by_runtime_probing_only_ = 0;
unsigned CpuFeatures::cross_compile_ = 0;


class CacheLineSizes {
 public:
  CacheLineSizes() {
#ifdef USE_SIMULATOR
    cache_type_register_ = 0;
#else
    // Copy the content of the cache type register to a core register.
    __asm__ __volatile__ ("mrs %[ctr], ctr_el0"  // NOLINT
                          : [ctr] "=r" (cache_type_register_));
#endif
  }

  uint32_t icache_line_size() const { return ExtractCacheLineSize(0); }
  uint32_t dcache_line_size() const { return ExtractCacheLineSize(16); }

 private:
  uint32_t ExtractCacheLineSize(int cache_line_size_shift) const {
    // The cache type register holds the size of the caches as a power of two.
    return 1 << ((cache_type_register_ >> cache_line_size_shift) & 0xf);
  }

  uint32_t cache_type_register_;
};


void CPU::FlushICache(void* address, size_t length) {
  if (length == 0) return;

#ifdef USE_SIMULATOR
  // TODO(all): consider doing some cache simulation to ensure every address
  // run has been synced.
  USE(address);
  USE(length);
#else
  // The code below assumes user space cache operations are allowed. The goal
  // of this routine is to make sure the code generated is visible to the I
  // side of the CPU.

  uintptr_t start = reinterpret_cast<uintptr_t>(address);
  // Sizes will be used to generate a mask big enough to cover a pointer.
  CacheLineSizes sizes;
  uintptr_t dsize = sizes.dcache_line_size();
  uintptr_t isize = sizes.icache_line_size();
  // Cache line sizes are always a power of 2.
  ASSERT(CountSetBits(dsize, 64) == 1);
  ASSERT(CountSetBits(isize, 64) == 1);
  uintptr_t dstart = start & ~(dsize - 1);
  uintptr_t istart = start & ~(isize - 1);
  uintptr_t end = start + length;

  __asm__ __volatile__ (  // NOLINT
    // Clean every line of the D cache containing the target data.
    "0:                                \n\t"
    // dc      : Data Cache maintenance
    //    c    : Clean
    //     va  : by (Virtual) Address
    //       u : to the point of Unification
    // The point of unification for a processor is the point by which the
    // instruction and data caches are guaranteed to see the same copy of a
    // memory location. See ARM DDI 0406B page B2-12 for more information.
    "dc   cvau, %[dline]                \n\t"
    "add  %[dline], %[dline], %[dsize]  \n\t"
    "cmp  %[dline], %[end]              \n\t"
    "b.lt 0b                            \n\t"
    // Barrier to make sure the effect of the code above is visible to the rest
    // of the world.
    // dsb    : Data Synchronisation Barrier
    //    ish : Inner SHareable domain
    // The point of unification for an Inner Shareable shareability domain is
    // the point by which the instruction and data caches of all the processors
    // in that Inner Shareable shareability domain are guaranteed to see the
    // same copy of a memory location.  See ARM DDI 0406B page B2-12 for more
    // information.
    "dsb  ish                           \n\t"
    // Invalidate every line of the I cache containing the target data.
    "1:                                 \n\t"
    // ic      : instruction cache maintenance
    //    i    : invalidate
    //     va  : by address
    //       u : to the point of unification
    "ic   ivau, %[iline]                \n\t"
    "add  %[iline], %[iline], %[isize]  \n\t"
    "cmp  %[iline], %[end]              \n\t"
    "b.lt 1b                            \n\t"
    // Barrier to make sure the effect of the code above is visible to the rest
    // of the world.
    "dsb  ish                           \n\t"
    // Barrier to ensure any prefetching which happened before this code is
    // discarded.
    // isb : Instruction Synchronisation Barrier
    "isb                                \n\t"
    : [dline] "+r" (dstart),
      [iline] "+r" (istart)
    : [dsize] "r"  (dsize),
      [isize] "r"  (isize),
      [end]   "r"  (end)
    // This code does not write to memory but without the dependency gcc might
    // move this code before the code is generated.
    : "cc", "memory"
  );  // NOLINT
#endif
}


void CpuFeatures::Probe(bool serializer_enabled) {
  ASSERT(supported_ == 0);

  if (serializer_enabled && FLAG_enable_always_align_csp) {
    // Always align csp in snapshot code - this is safe and ensures that csp
    // will always be aligned if it is enabled by probing at runtime.
    supported_ |= static_cast<uint64_t>(1) << ALWAYS_ALIGN_CSP;
  }

  if (!serializer_enabled) {
    CPU cpu;
    // Always align csp on Nvidia cores or when debug_code is enabled.
    if (FLAG_enable_always_align_csp &&
        (cpu.implementer() == CPU::NVIDIA || FLAG_debug_code)) {
      found_by_runtime_probing_only_ |=
          static_cast<uint64_t>(1) << ALWAYS_ALIGN_CSP;
    }

    supported_ |= found_by_runtime_probing_only_;
  }

#ifdef DEBUG
  initialized_ = true;
#endif
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
