

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "noisefield.h"
namespace Manta {
#ifdef _C_WaveletNoiseField
static const Pb::Register _R_13("WaveletNoiseField", "NoiseField", "PbClass");
template<> const char *Namify<WaveletNoiseField>::S = "WaveletNoiseField";
static const Pb::Register _R_14("WaveletNoiseField", "WaveletNoiseField", WaveletNoiseField::_W_0);
static const Pb::Register _R_15("WaveletNoiseField",
                                "posOffset",
                                WaveletNoiseField::_GET_mPosOffset,
                                WaveletNoiseField::_SET_mPosOffset);
static const Pb::Register _R_16("WaveletNoiseField",
                                "posScale",
                                WaveletNoiseField::_GET_mPosScale,
                                WaveletNoiseField::_SET_mPosScale);
static const Pb::Register _R_17("WaveletNoiseField",
                                "valOffset",
                                WaveletNoiseField::_GET_mValOffset,
                                WaveletNoiseField::_SET_mValOffset);
static const Pb::Register _R_18("WaveletNoiseField",
                                "valScale",
                                WaveletNoiseField::_GET_mValScale,
                                WaveletNoiseField::_SET_mValScale);
static const Pb::Register _R_19("WaveletNoiseField",
                                "clamp",
                                WaveletNoiseField::_GET_mClamp,
                                WaveletNoiseField::_SET_mClamp);
static const Pb::Register _R_20("WaveletNoiseField",
                                "clampNeg",
                                WaveletNoiseField::_GET_mClampNeg,
                                WaveletNoiseField::_SET_mClampNeg);
static const Pb::Register _R_21("WaveletNoiseField",
                                "clampPos",
                                WaveletNoiseField::_GET_mClampPos,
                                WaveletNoiseField::_SET_mClampPos);
static const Pb::Register _R_22("WaveletNoiseField",
                                "timeAnim",
                                WaveletNoiseField::_GET_mTimeAnim,
                                WaveletNoiseField::_SET_mTimeAnim);
#endif
extern "C" {
void PbRegister_file_13()
{
  KEEP_UNUSED(_R_13);
  KEEP_UNUSED(_R_14);
  KEEP_UNUSED(_R_15);
  KEEP_UNUSED(_R_16);
  KEEP_UNUSED(_R_17);
  KEEP_UNUSED(_R_18);
  KEEP_UNUSED(_R_19);
  KEEP_UNUSED(_R_20);
  KEEP_UNUSED(_R_21);
  KEEP_UNUSED(_R_22);
}
}
}  // namespace Manta