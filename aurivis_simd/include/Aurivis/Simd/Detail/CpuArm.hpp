#pragma once

#include "Aurivis/Simd/Config.hpp"
#include "Aurivis/Simd/CpuFeatures.hpp"

#include <cstddef>

#if AURIVIS_ARCH_ARM64

#if AURIVIS_OS_LINUX
#include <sys/auxv.h>
#elif AURIVIS_OS_APPLE
#include <sys/sysctl.h>
#elif AURIVIS_OS_WINDOWS
// Declared by hand to avoid including <windows.h>. Identical to the declaration in <processthreadsapi.h>.
extern "C" __declspec(dllimport) int __stdcall IsProcessorFeaturePresent(unsigned long processorFeature);
#endif

namespace Aurivis::Simd::Detail {

#if AURIVIS_OS_LINUX

// --- Linux: auxiliary vector entries and bits from <asm/hwcap.h> ---
// Defined here because older C libraries ship headers without the newer bits.
inline constexpr unsigned long LINUX_AT_HWCAP = 16;
inline constexpr unsigned long LINUX_AT_HWCAP2 = 26;
inline constexpr unsigned long LINUX_HWCAP_FPHP = 1ul << 9;
inline constexpr unsigned long LINUX_HWCAP_ASIMDHP = 1ul << 10;
inline constexpr unsigned long LINUX_HWCAP_ASIMDDP = 1ul << 20;
inline constexpr unsigned long LINUX_HWCAP_SVE = 1ul << 22;
inline constexpr unsigned long LINUX_HWCAP2_SVE2 = 1ul << 1;
inline constexpr unsigned long LINUX_HWCAP2_I8MM = 1ul << 13;
inline constexpr unsigned long LINUX_HWCAP2_BF16 = 1ul << 14;

/**
 * @brief Reads the ARM features that the Linux kernel reports in the auxiliary vector.
 * @details The kernel only sets a bit when it also saves the matching registers, so SVE is only reported when the
 *          kernel supports it.
 * @param features Receives the detected features.
 */
inline void DetectArmLinux(CpuFeatures& features) noexcept {
  const unsigned long hwcap = getauxval(LINUX_AT_HWCAP);
  const unsigned long hwcap2 = getauxval(LINUX_AT_HWCAP2);
  features.neonFp16 = (hwcap & LINUX_HWCAP_FPHP) != 0 && (hwcap & LINUX_HWCAP_ASIMDHP) != 0;
  features.dotProd = (hwcap & LINUX_HWCAP_ASIMDDP) != 0;
  features.sve = (hwcap & LINUX_HWCAP_SVE) != 0;
  features.sve2 = features.sve && (hwcap2 & LINUX_HWCAP2_SVE2) != 0;
  features.i8mm = (hwcap2 & LINUX_HWCAP2_I8MM) != 0;
  features.neonBf16 = (hwcap2 & LINUX_HWCAP2_BF16) != 0;
}

#elif AURIVIS_OS_APPLE

/**
 * @brief Checks one of the hw.optional.arm.FEAT_* entries that macOS 12 and newer provide.
 * @param name The full sysctl name.
 * @return True if the entry exists and is set. Older systems without the entry report false.
 */
inline bool HasAppleFeature(const char* name) noexcept {
  int value = 0;
  size_t size = sizeof(value);
  return sysctlbyname(name, &value, &size, nullptr, 0) == 0 && value != 0;
}

/**
 * @brief Reads the ARM features that macOS reports.
 * @details Apple chips have no SVE outside of streaming mode, so sve and sve2 stay false.
 * @param features Receives the detected features.
 */
inline void DetectArmApple(CpuFeatures& features) noexcept {
  features.neonFp16 = HasAppleFeature("hw.optional.arm.FEAT_FP16");
  features.neonBf16 = HasAppleFeature("hw.optional.arm.FEAT_BF16");
  features.dotProd = HasAppleFeature("hw.optional.arm.FEAT_DotProd");
  features.i8mm = HasAppleFeature("hw.optional.arm.FEAT_I8MM");
}

#elif AURIVIS_OS_WINDOWS

// --- Windows: processor feature numbers from <winnt.h> ---
// Defined here because older Windows SDKs do not have them. Older Windows versions report unknown numbers as absent.
inline constexpr unsigned long WINDOWS_PF_ARM_V82_DP = 43;
inline constexpr unsigned long WINDOWS_PF_ARM_SVE = 46;
inline constexpr unsigned long WINDOWS_PF_ARM_SVE2 = 47;
inline constexpr unsigned long WINDOWS_PF_ARM_V82_I8MM = 66;
inline constexpr unsigned long WINDOWS_PF_ARM_V82_FP16 = 67;
inline constexpr unsigned long WINDOWS_PF_ARM_V86_BF16 = 68;

inline bool HasWindowsFeature(unsigned long feature) noexcept { return IsProcessorFeaturePresent(feature) != 0; }

/**
 * @brief Reads the ARM features that Windows reports.
 * @details SVE is only reported when Windows also saves the SVE registers.
 * @param features Receives the detected features.
 */
inline void DetectArmWindows(CpuFeatures& features) noexcept {
  features.neonFp16 = HasWindowsFeature(WINDOWS_PF_ARM_V82_FP16);
  features.neonBf16 = HasWindowsFeature(WINDOWS_PF_ARM_V86_BF16);
  features.dotProd = HasWindowsFeature(WINDOWS_PF_ARM_V82_DP);
  features.i8mm = HasWindowsFeature(WINDOWS_PF_ARM_V82_I8MM);
  features.sve = HasWindowsFeature(WINDOWS_PF_ARM_SVE);
  features.sve2 = features.sve && HasWindowsFeature(WINDOWS_PF_ARM_SVE2);
}

#endif

/**
 * @brief Detects the ARM SIMD features that both the CPU and the operating system support.
 * @details ARM CPUs do not let user code read their feature registers on every system, so the operating system is
 *          asked instead. On systems without a known interface only NEON is reported.
 * @return The detected features. x86 fields stay false.
 */
inline CpuFeatures DetectArm() noexcept {
  CpuFeatures features;
  features.neon = true;  // NEON is mandatory on every AArch64 CPU
#if AURIVIS_OS_LINUX
  DetectArmLinux(features);
#elif AURIVIS_OS_APPLE
  DetectArmApple(features);
#elif AURIVIS_OS_WINDOWS
  DetectArmWindows(features);
#endif
  return features;
}

}  // namespace Aurivis::Simd::Detail

#endif  // AURIVIS_ARCH_ARM64