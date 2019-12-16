

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "vortexpart.h"
namespace Manta {
#ifdef _C_ParticleSystem
static const Pb::Register _R_20("ParticleSystem<VortexParticleData>",
                                "ParticleSystem<VortexParticleData>",
                                "ParticleBase");
template<>
const char *Namify<ParticleSystem<VortexParticleData>>::S = "ParticleSystem<VortexParticleData>";
static const Pb::Register _R_21("ParticleSystem<VortexParticleData>",
                                "ParticleSystem",
                                ParticleSystem<VortexParticleData>::_W_2);
static const Pb::Register _R_22("ParticleSystem<VortexParticleData>",
                                "pySize",
                                ParticleSystem<VortexParticleData>::_W_3);
static const Pb::Register _R_23("ParticleSystem<VortexParticleData>",
                                "setPos",
                                ParticleSystem<VortexParticleData>::_W_4);
static const Pb::Register _R_24("ParticleSystem<VortexParticleData>",
                                "getPos",
                                ParticleSystem<VortexParticleData>::_W_5);
static const Pb::Register _R_25("ParticleSystem<VortexParticleData>",
                                "getPosPdata",
                                ParticleSystem<VortexParticleData>::_W_6);
static const Pb::Register _R_26("ParticleSystem<VortexParticleData>",
                                "setPosPdata",
                                ParticleSystem<VortexParticleData>::_W_7);
static const Pb::Register _R_27("ParticleSystem<VortexParticleData>",
                                "clear",
                                ParticleSystem<VortexParticleData>::_W_8);
static const Pb::Register _R_28("ParticleSystem<VortexParticleData>",
                                "advectInGrid",
                                ParticleSystem<VortexParticleData>::_W_9);
static const Pb::Register _R_29("ParticleSystem<VortexParticleData>",
                                "projectOutside",
                                ParticleSystem<VortexParticleData>::_W_10);
static const Pb::Register _R_30("ParticleSystem<VortexParticleData>",
                                "projectOutOfBnd",
                                ParticleSystem<VortexParticleData>::_W_11);
#endif
#ifdef _C_VortexParticleSystem
static const Pb::Register _R_31("VortexParticleSystem",
                                "VortexParticleSystem",
                                "ParticleSystem<VortexParticleData>");
template<> const char *Namify<VortexParticleSystem>::S = "VortexParticleSystem";
static const Pb::Register _R_32("VortexParticleSystem",
                                "VortexParticleSystem",
                                VortexParticleSystem::_W_0);
static const Pb::Register _R_33("VortexParticleSystem", "advectSelf", VortexParticleSystem::_W_1);
static const Pb::Register _R_34("VortexParticleSystem", "applyToMesh", VortexParticleSystem::_W_2);
#endif
extern "C" {
void PbRegister_file_20()
{
  KEEP_UNUSED(_R_20);
  KEEP_UNUSED(_R_21);
  KEEP_UNUSED(_R_22);
  KEEP_UNUSED(_R_23);
  KEEP_UNUSED(_R_24);
  KEEP_UNUSED(_R_25);
  KEEP_UNUSED(_R_26);
  KEEP_UNUSED(_R_27);
  KEEP_UNUSED(_R_28);
  KEEP_UNUSED(_R_29);
  KEEP_UNUSED(_R_30);
  KEEP_UNUSED(_R_31);
  KEEP_UNUSED(_R_32);
  KEEP_UNUSED(_R_33);
  KEEP_UNUSED(_R_34);
}
}
}  // namespace Manta