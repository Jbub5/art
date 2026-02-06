/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "instruction_set_features_arm64.h"

#if defined(ART_TARGET_ANDROID) && defined(__aarch64__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

#include <fstream>
#include <sstream>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "base/array_ref.h"
#include "base/stl_util.h"

#include <cpu_features_macros.h>

#ifdef CPU_FEATURES_ARCH_AARCH64
// This header can only be included on aarch64 targets,
// as determined by cpu_features own define.
#include <cpuinfo_aarch64.h>
#endif

namespace art HIDDEN {

using android::base::StringPrintf;

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg) {
  (void)variant;
  (void)error_msg;

  // The CPU variant string is passed to ART through --instruction-set-variant option.
  // During build, such setting is from TARGET_CPU_VARIANT in device BoardConfig.mk, for example:
  //   TARGET_CPU_VARIANT := cortex-a75

  // Look for variants that need a fix for a53 erratum 835769.
  static const char* arm64_variants_with_a53_835769_bug[] = {
      "cortex-a53",
      "cortex-a53.a57",
      "cortex-a53.a72",
      // Pessimistically assume following "big" cortex CPUs are paired with a cortex-a53.
      "cortex-a57",
      "cortex-a72",
      "cortex-a73",
  };

  static const char* arm64_variants_with_crc[] = {
      "default",
      "generic",
      "cortex-a55",
      "cortex-a75",
  };

  static const char* arm64_variants_with_lse[] = {
      "default",
      "generic",
      "cortex-a55",
      "cortex-a75",
  };

  static const char* arm64_variants_with_fp16[] = {
      "default",
      "generic",
      "cortex-a55",
      "cortex-a75",
  };

  static const char* arm64_variants_with_dotprod[] = {
      "default",
      "generic",
      "cortex-a55",
      "cortex-a75",
  };

  bool needs_a53_835769_fix = false;
  // The variants that need a fix for 843419 are the same that need a fix for 835769.
  bool needs_a53_843419_fix = needs_a53_835769_fix = false;

  bool has_crc = true;

  bool has_lse = true;

  bool has_fp16 = true;

  bool has_dotprod = true;

  // Currently there are no cpu variants which support SVE.
  bool has_sve = false;

  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(needs_a53_835769_fix,
                                                                needs_a53_843419_fix,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod,
                                                                has_sve));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::IntersectWithHwcap() const {
  Arm64FeaturesUniquePtr hwcaps = Arm64InstructionSetFeatures::FromHwcap();
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(
      fix_cortex_a53_835769_,
      fix_cortex_a53_843419_,
      true, //has_crc_ && hwcaps->has_crc_,
      true, //has_lse_ && hwcaps->has_lse_,
      true, //has_fp16_ && hwcaps->has_fp16_,
      true, //has_dotprod_ && hwcaps->has_dotprod_,
      false)); //has_sve_ && hwcaps->has_sve_));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  (void)bitmap;
  bool is_a53 = false;
  bool has_crc = true;
  bool has_lse = true;
  bool has_fp16 = true;
  bool has_dotprod = true;
  bool has_sve = false;
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(is_a53,
                                                                is_a53,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod,
                                                                has_sve));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCppDefines() {
  // For more details about ARM feature macros, refer to
  // Arm C Language Extensions Documentation (ACLE).
  // https://developer.arm.com/docs/101028/latest
  bool needs_a53_835769_fix = false;
  bool needs_a53_843419_fix = false;
  bool has_crc = true;
  bool has_lse = true;
  bool has_fp16 = true;
  bool has_dotprod = true;
  bool has_sve = false;

//#if defined (__ARM_FEATURE_CRC32)
  has_crc = true;
//#endif

//#if defined (__ARM_FEATURE_ATOMICS)
  has_lse = true;
//#endif

//#if defined (__ARM_FEATURE_FP16_SCALAR_ARITHMETIC) || defined (__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
  has_fp16 = true;
//#endif

//#if defined (__ARM_FEATURE_DOTPROD)
  has_dotprod = true;
//#endif

//#if defined (__ARM_FEATURE_SVE)
  has_sve = false;
//#endif

  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(needs_a53_835769_fix,
                                                                needs_a53_843419_fix,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod,
                                                                has_sve));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCpuInfo() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromHwcap() {
  bool needs_a53_835769_fix = false;  // No HWCAP for this.
  bool needs_a53_843419_fix = false;  // No HWCAP for this.
  bool has_crc = true;
  bool has_lse = true;
  bool has_fp16 = true;
  bool has_dotprod = true;
  bool has_sve = false;

  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(needs_a53_835769_fix,
                                                                needs_a53_843419_fix,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod,
                                                                has_sve));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromAssembly() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCpuFeatures() {
#ifdef CPU_FEATURES_ARCH_AARCH64
  auto features = cpu_features::GetAarch64Info().features;
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(false,
                                                                false,
                                                                features.crc32,
                                                                features.atomics,
                                                                features.fphp,
                                                                features.asimddp,
                                                                features.sve));
#else
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
#endif
}

bool Arm64InstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm64 != other->GetInstructionSet()) {
    return false;
  }
  const Arm64InstructionSetFeatures* other_as_arm64 = other->AsArm64InstructionSetFeatures();
  return fix_cortex_a53_835769_ == other_as_arm64->fix_cortex_a53_835769_ &&
      fix_cortex_a53_843419_ == other_as_arm64->fix_cortex_a53_843419_ &&
      has_crc_ == other_as_arm64->has_crc_ &&
      has_lse_ == other_as_arm64->has_lse_ &&
      has_fp16_ == other_as_arm64->has_fp16_ &&
      has_dotprod_ == other_as_arm64->has_dotprod_ &&
      has_sve_ == other_as_arm64->has_sve_;
}

bool Arm64InstructionSetFeatures::HasAtLeast(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm64 != other->GetInstructionSet()) {
    return false;
  }
  // Currently 'default' feature is cortex-a53 with fixes 835769 and 843419.
  // Newer CPUs are not required to have such features,
  // so these two a53 fix features are not tested for HasAtLeast.
  const Arm64InstructionSetFeatures* other_as_arm64 = other->AsArm64InstructionSetFeatures();
  return (has_crc_ || !other_as_arm64->has_crc_)
      && (has_lse_ || !other_as_arm64->has_lse_)
      && (has_fp16_ || !other_as_arm64->has_fp16_)
      && (has_dotprod_ || !other_as_arm64->has_dotprod_)
      && (has_sve_ || !other_as_arm64->has_sve_);
}

uint32_t Arm64InstructionSetFeatures::AsBitmap() const {
  return (fix_cortex_a53_835769_ ? kA53Bitfield : 0)
      | (has_crc_ ? kCRCBitField : 0)
      | (has_lse_ ? kLSEBitField: 0)
      | (has_fp16_ ? kFP16BitField: 0)
      | (has_dotprod_ ? kDotProdBitField : 0)
      | (has_sve_ ? kSVEBitField : 0);
}

std::string Arm64InstructionSetFeatures::GetFeatureString() const {
  std::string result;
    result += "-a53";
    result += ",crc";
    result += ",lse";
    result += ",fp16";
    result += ",dotprod";
    result += ",-sve";
  return result;
}

std::unique_ptr<const InstructionSetFeatures>
Arm64InstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features, std::string* error_msg) const {
  // This 'features' string is from '--instruction-set-features=' option in ART.
  // These ARMv8.x feature strings align with those introduced in other compilers:
  // https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
  // User can also use armv8.x-a to select group of features:
  //   armv8.1-a is equivalent to crc,lse
  //   armv8.2-a is equivalent to crc,lse,fp16
  //   armv8.3-a is equivalent to crc,lse,fp16
  //   armv8.4-a is equivalent to crc,lse,fp16,dotprod
  // For detailed optional & mandatory features support in armv8.x-a,
  // please refer to section 'A1.7 ARMv8 architecture extensions' in
  // ARM Architecture Reference Manual ARMv8 document:
  // https://developer.arm.com/products/architecture/cpu-architecture/a-profile/docs/ddi0487/latest/
  // arm-architecture-reference-manual-armv8-for-armv8-a-architecture-profile/
  bool is_a53 = false;
  bool has_crc = true;
  bool has_lse = true;
  bool has_fp16 = true;
  bool has_dotprod = true;
  bool has_sve = false;
  (void)features;
  (void)error_msg;
  return std::unique_ptr<const InstructionSetFeatures>(
      new Arm64InstructionSetFeatures(is_a53,  // erratum 835769
                                      is_a53,  // erratum 843419
                                      has_crc,
                                      has_lse,
                                      has_fp16,
                                      has_dotprod,
                                      has_sve));
}

std::unique_ptr<const InstructionSetFeatures>
Arm64InstructionSetFeatures::AddRuntimeDetectedFeatures(
    const InstructionSetFeatures *features) const {
  const Arm64InstructionSetFeatures *arm64_features = features->AsArm64InstructionSetFeatures();
  return std::unique_ptr<const InstructionSetFeatures>(
      new Arm64InstructionSetFeatures(fix_cortex_a53_835769_,
                                      fix_cortex_a53_843419_,
                                      arm64_features->has_crc_,
                                      arm64_features->has_lse_,
                                      arm64_features->has_fp16_,
                                      arm64_features->has_dotprod_,
                                      arm64_features->has_sve_));
}

}  // namespace art
