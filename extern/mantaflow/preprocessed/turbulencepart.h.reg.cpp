

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "turbulencepart.h"
namespace Manta {
#ifdef _C_ParticleSystem
static const Pb::Register _R_21("ParticleSystem<TurbulenceParticleData>",
                                "ParticleSystem<TurbulenceParticleData>",
                                "ParticleBase");
template<>
const char *Namify<ParticleSystem<TurbulenceParticleData>>::S =
    "ParticleSystem<TurbulenceParticleData>";
static const Pb::Register _R_22("ParticleSystem<TurbulenceParticleData>",
                                "ParticleSystem",
                                ParticleSystem<TurbulenceParticleData>::_W_2);
static const Pb::Register _R_23("ParticleSystem<TurbulenceParticleData>",
                                "pySize",
                                ParticleSystem<TurbulenceParticleData>::_W_3);
static const Pb::Register _R_24("ParticleSystem<TurbulenceParticleData>",
                                "setPos",
                                ParticleSystem<TurbulenceParticleData>::_W_4);
static const Pb::Register _R_25("ParticleSystem<TurbulenceParticleData>",
                                "getPos",
                                ParticleSystem<TurbulenceParticleData>::_W_5);
static const Pb::Register _R_26("ParticleSystem<TurbulenceParticleData>",
                                "getPosPdata",
                                ParticleSystem<TurbulenceParticleData>::_W_6);
static const Pb::Register _R_27("ParticleSystem<TurbulenceParticleData>",
                                "setPosPdata",
                                ParticleSystem<TurbulenceParticleData>::_W_7);
static const Pb::Register _R_28("ParticleSystem<TurbulenceParticleData>",
                                "clear",
                                ParticleSystem<TurbulenceParticleData>::_W_8);
static const Pb::Register _R_29("ParticleSystem<TurbulenceParticleData>",
                                "advectInGrid",
                                ParticleSystem<TurbulenceParticleData>::_W_9);
static const Pb::Register _R_30("ParticleSystem<TurbulenceParticleData>",
                                "projectOutside",
                                ParticleSystem<TurbulenceParticleData>::_W_10);
static const Pb::Register _R_31("ParticleSystem<TurbulenceParticleData>",
                                "projectOutOfBnd",
                                ParticleSystem<TurbulenceParticleData>::_W_11);
#endif
#ifdef _C_TurbulenceParticleSystem
static const Pb::Register _R_32("TurbulenceParticleSystem",
                                "TurbulenceParticleSystem",
                                "ParticleSystem<TurbulenceParticleData>");
template<> const char *Namify<TurbulenceParticleSystem>::S = "TurbulenceParticleSystem";
static const Pb::Register _R_33("TurbulenceParticleSystem",
                                "TurbulenceParticleSystem",
                                TurbulenceParticleSystem::_W_0);
static const Pb::Register _R_34("TurbulenceParticleSystem",
                                "resetTexCoords",
                                TurbulenceParticleSystem::_W_1);
static const Pb::Register _R_35("TurbulenceParticleSystem",
                                "seed",
                                TurbulenceParticleSystem::_W_2);
static const Pb::Register _R_36("TurbulenceParticleSystem",
                                "synthesize",
                                TurbulenceParticleSystem::_W_3);
static const Pb::Register _R_37("TurbulenceParticleSystem",
                                "deleteInObstacle",
                                TurbulenceParticleSystem::_W_4);
#endif
extern "C" {
void PbRegister_file_21()
{
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
  KEEP_UNUSED(_R_35);
  KEEP_UNUSED(_R_36);
  KEEP_UNUSED(_R_37);
}
}
}  // namespace Manta