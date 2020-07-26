

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "particle.h"
namespace Manta {
#ifdef _C_BasicParticleSystem
static const Pb::Register _R_13("BasicParticleSystem",
                                "BasicParticleSystem",
                                "ParticleSystem<BasicParticleData>");
template<> const char *Namify<BasicParticleSystem>::S = "BasicParticleSystem";
static const Pb::Register _R_14("BasicParticleSystem",
                                "BasicParticleSystem",
                                BasicParticleSystem::_W_12);
static const Pb::Register _R_15("BasicParticleSystem", "save", BasicParticleSystem::_W_13);
static const Pb::Register _R_16("BasicParticleSystem", "load", BasicParticleSystem::_W_14);
static const Pb::Register _R_17("BasicParticleSystem",
                                "readParticles",
                                BasicParticleSystem::_W_15);
static const Pb::Register _R_18("BasicParticleSystem", "addParticle", BasicParticleSystem::_W_16);
static const Pb::Register _R_19("BasicParticleSystem", "printParts", BasicParticleSystem::_W_17);
static const Pb::Register _R_20("BasicParticleSystem",
                                "getDataPointer",
                                BasicParticleSystem::_W_18);
#endif
#ifdef _C_ParticleBase
static const Pb::Register _R_21("ParticleBase", "ParticleBase", "PbClass");
template<> const char *Namify<ParticleBase>::S = "ParticleBase";
static const Pb::Register _R_22("ParticleBase", "ParticleBase", ParticleBase::_W_0);
static const Pb::Register _R_23("ParticleBase", "create", ParticleBase::_W_1);
static const Pb::Register _R_24("ParticleBase",
                                "maxParticles",
                                ParticleBase::_GET_mMaxParticles,
                                ParticleBase::_SET_mMaxParticles);
#endif
#ifdef _C_ParticleDataBase
static const Pb::Register _R_25("ParticleDataBase", "ParticleDataBase", "PbClass");
template<> const char *Namify<ParticleDataBase>::S = "ParticleDataBase";
static const Pb::Register _R_26("ParticleDataBase", "ParticleDataBase", ParticleDataBase::_W_21);
#endif
#ifdef _C_ParticleDataImpl
static const Pb::Register _R_27("ParticleDataImpl<int>",
                                "ParticleDataImpl<int>",
                                "ParticleDataBase");
template<> const char *Namify<ParticleDataImpl<int>>::S = "ParticleDataImpl<int>";
static const Pb::Register _R_28("ParticleDataImpl<int>",
                                "ParticleDataImpl",
                                ParticleDataImpl<int>::_W_22);
static const Pb::Register _R_29("ParticleDataImpl<int>", "clear", ParticleDataImpl<int>::_W_23);
static const Pb::Register _R_30("ParticleDataImpl<int>",
                                "setSource",
                                ParticleDataImpl<int>::_W_24);
static const Pb::Register _R_31("ParticleDataImpl<int>", "copyFrom", ParticleDataImpl<int>::_W_25);
static const Pb::Register _R_32("ParticleDataImpl<int>", "setConst", ParticleDataImpl<int>::_W_26);
static const Pb::Register _R_33("ParticleDataImpl<int>",
                                "setConstRange",
                                ParticleDataImpl<int>::_W_27);
static const Pb::Register _R_34("ParticleDataImpl<int>", "add", ParticleDataImpl<int>::_W_28);
static const Pb::Register _R_35("ParticleDataImpl<int>", "sub", ParticleDataImpl<int>::_W_29);
static const Pb::Register _R_36("ParticleDataImpl<int>", "addConst", ParticleDataImpl<int>::_W_30);
static const Pb::Register _R_37("ParticleDataImpl<int>",
                                "addScaled",
                                ParticleDataImpl<int>::_W_31);
static const Pb::Register _R_38("ParticleDataImpl<int>", "mult", ParticleDataImpl<int>::_W_32);
static const Pb::Register _R_39("ParticleDataImpl<int>",
                                "multConst",
                                ParticleDataImpl<int>::_W_33);
static const Pb::Register _R_40("ParticleDataImpl<int>", "safeDiv", ParticleDataImpl<int>::_W_34);
static const Pb::Register _R_41("ParticleDataImpl<int>", "clamp", ParticleDataImpl<int>::_W_35);
static const Pb::Register _R_42("ParticleDataImpl<int>", "clampMin", ParticleDataImpl<int>::_W_36);
static const Pb::Register _R_43("ParticleDataImpl<int>", "clampMax", ParticleDataImpl<int>::_W_37);
static const Pb::Register _R_44("ParticleDataImpl<int>",
                                "getMaxAbs",
                                ParticleDataImpl<int>::_W_38);
static const Pb::Register _R_45("ParticleDataImpl<int>", "getMax", ParticleDataImpl<int>::_W_39);
static const Pb::Register _R_46("ParticleDataImpl<int>", "getMin", ParticleDataImpl<int>::_W_40);
static const Pb::Register _R_47("ParticleDataImpl<int>", "sum", ParticleDataImpl<int>::_W_41);
static const Pb::Register _R_48("ParticleDataImpl<int>",
                                "sumSquare",
                                ParticleDataImpl<int>::_W_42);
static const Pb::Register _R_49("ParticleDataImpl<int>",
                                "sumMagnitude",
                                ParticleDataImpl<int>::_W_43);
static const Pb::Register _R_50("ParticleDataImpl<int>",
                                "setConstIntFlag",
                                ParticleDataImpl<int>::_W_44);
static const Pb::Register _R_51("ParticleDataImpl<int>",
                                "printPdata",
                                ParticleDataImpl<int>::_W_45);
static const Pb::Register _R_52("ParticleDataImpl<int>", "save", ParticleDataImpl<int>::_W_46);
static const Pb::Register _R_53("ParticleDataImpl<int>", "load", ParticleDataImpl<int>::_W_47);
static const Pb::Register _R_54("ParticleDataImpl<int>",
                                "getDataPointer",
                                ParticleDataImpl<int>::_W_48);
static const Pb::Register _R_55("ParticleDataImpl<Real>",
                                "ParticleDataImpl<Real>",
                                "ParticleDataBase");
template<> const char *Namify<ParticleDataImpl<Real>>::S = "ParticleDataImpl<Real>";
static const Pb::Register _R_56("ParticleDataImpl<Real>",
                                "ParticleDataImpl",
                                ParticleDataImpl<Real>::_W_22);
static const Pb::Register _R_57("ParticleDataImpl<Real>", "clear", ParticleDataImpl<Real>::_W_23);
static const Pb::Register _R_58("ParticleDataImpl<Real>",
                                "setSource",
                                ParticleDataImpl<Real>::_W_24);
static const Pb::Register _R_59("ParticleDataImpl<Real>",
                                "copyFrom",
                                ParticleDataImpl<Real>::_W_25);
static const Pb::Register _R_60("ParticleDataImpl<Real>",
                                "setConst",
                                ParticleDataImpl<Real>::_W_26);
static const Pb::Register _R_61("ParticleDataImpl<Real>",
                                "setConstRange",
                                ParticleDataImpl<Real>::_W_27);
static const Pb::Register _R_62("ParticleDataImpl<Real>", "add", ParticleDataImpl<Real>::_W_28);
static const Pb::Register _R_63("ParticleDataImpl<Real>", "sub", ParticleDataImpl<Real>::_W_29);
static const Pb::Register _R_64("ParticleDataImpl<Real>",
                                "addConst",
                                ParticleDataImpl<Real>::_W_30);
static const Pb::Register _R_65("ParticleDataImpl<Real>",
                                "addScaled",
                                ParticleDataImpl<Real>::_W_31);
static const Pb::Register _R_66("ParticleDataImpl<Real>", "mult", ParticleDataImpl<Real>::_W_32);
static const Pb::Register _R_67("ParticleDataImpl<Real>",
                                "multConst",
                                ParticleDataImpl<Real>::_W_33);
static const Pb::Register _R_68("ParticleDataImpl<Real>",
                                "safeDiv",
                                ParticleDataImpl<Real>::_W_34);
static const Pb::Register _R_69("ParticleDataImpl<Real>", "clamp", ParticleDataImpl<Real>::_W_35);
static const Pb::Register _R_70("ParticleDataImpl<Real>",
                                "clampMin",
                                ParticleDataImpl<Real>::_W_36);
static const Pb::Register _R_71("ParticleDataImpl<Real>",
                                "clampMax",
                                ParticleDataImpl<Real>::_W_37);
static const Pb::Register _R_72("ParticleDataImpl<Real>",
                                "getMaxAbs",
                                ParticleDataImpl<Real>::_W_38);
static const Pb::Register _R_73("ParticleDataImpl<Real>", "getMax", ParticleDataImpl<Real>::_W_39);
static const Pb::Register _R_74("ParticleDataImpl<Real>", "getMin", ParticleDataImpl<Real>::_W_40);
static const Pb::Register _R_75("ParticleDataImpl<Real>", "sum", ParticleDataImpl<Real>::_W_41);
static const Pb::Register _R_76("ParticleDataImpl<Real>",
                                "sumSquare",
                                ParticleDataImpl<Real>::_W_42);
static const Pb::Register _R_77("ParticleDataImpl<Real>",
                                "sumMagnitude",
                                ParticleDataImpl<Real>::_W_43);
static const Pb::Register _R_78("ParticleDataImpl<Real>",
                                "setConstIntFlag",
                                ParticleDataImpl<Real>::_W_44);
static const Pb::Register _R_79("ParticleDataImpl<Real>",
                                "printPdata",
                                ParticleDataImpl<Real>::_W_45);
static const Pb::Register _R_80("ParticleDataImpl<Real>", "save", ParticleDataImpl<Real>::_W_46);
static const Pb::Register _R_81("ParticleDataImpl<Real>", "load", ParticleDataImpl<Real>::_W_47);
static const Pb::Register _R_82("ParticleDataImpl<Real>",
                                "getDataPointer",
                                ParticleDataImpl<Real>::_W_48);
static const Pb::Register _R_83("ParticleDataImpl<Vec3>",
                                "ParticleDataImpl<Vec3>",
                                "ParticleDataBase");
template<> const char *Namify<ParticleDataImpl<Vec3>>::S = "ParticleDataImpl<Vec3>";
static const Pb::Register _R_84("ParticleDataImpl<Vec3>",
                                "ParticleDataImpl",
                                ParticleDataImpl<Vec3>::_W_22);
static const Pb::Register _R_85("ParticleDataImpl<Vec3>", "clear", ParticleDataImpl<Vec3>::_W_23);
static const Pb::Register _R_86("ParticleDataImpl<Vec3>",
                                "setSource",
                                ParticleDataImpl<Vec3>::_W_24);
static const Pb::Register _R_87("ParticleDataImpl<Vec3>",
                                "copyFrom",
                                ParticleDataImpl<Vec3>::_W_25);
static const Pb::Register _R_88("ParticleDataImpl<Vec3>",
                                "setConst",
                                ParticleDataImpl<Vec3>::_W_26);
static const Pb::Register _R_89("ParticleDataImpl<Vec3>",
                                "setConstRange",
                                ParticleDataImpl<Vec3>::_W_27);
static const Pb::Register _R_90("ParticleDataImpl<Vec3>", "add", ParticleDataImpl<Vec3>::_W_28);
static const Pb::Register _R_91("ParticleDataImpl<Vec3>", "sub", ParticleDataImpl<Vec3>::_W_29);
static const Pb::Register _R_92("ParticleDataImpl<Vec3>",
                                "addConst",
                                ParticleDataImpl<Vec3>::_W_30);
static const Pb::Register _R_93("ParticleDataImpl<Vec3>",
                                "addScaled",
                                ParticleDataImpl<Vec3>::_W_31);
static const Pb::Register _R_94("ParticleDataImpl<Vec3>", "mult", ParticleDataImpl<Vec3>::_W_32);
static const Pb::Register _R_95("ParticleDataImpl<Vec3>",
                                "multConst",
                                ParticleDataImpl<Vec3>::_W_33);
static const Pb::Register _R_96("ParticleDataImpl<Vec3>",
                                "safeDiv",
                                ParticleDataImpl<Vec3>::_W_34);
static const Pb::Register _R_97("ParticleDataImpl<Vec3>", "clamp", ParticleDataImpl<Vec3>::_W_35);
static const Pb::Register _R_98("ParticleDataImpl<Vec3>",
                                "clampMin",
                                ParticleDataImpl<Vec3>::_W_36);
static const Pb::Register _R_99("ParticleDataImpl<Vec3>",
                                "clampMax",
                                ParticleDataImpl<Vec3>::_W_37);
static const Pb::Register _R_100("ParticleDataImpl<Vec3>",
                                 "getMaxAbs",
                                 ParticleDataImpl<Vec3>::_W_38);
static const Pb::Register _R_101("ParticleDataImpl<Vec3>",
                                 "getMax",
                                 ParticleDataImpl<Vec3>::_W_39);
static const Pb::Register _R_102("ParticleDataImpl<Vec3>",
                                 "getMin",
                                 ParticleDataImpl<Vec3>::_W_40);
static const Pb::Register _R_103("ParticleDataImpl<Vec3>", "sum", ParticleDataImpl<Vec3>::_W_41);
static const Pb::Register _R_104("ParticleDataImpl<Vec3>",
                                 "sumSquare",
                                 ParticleDataImpl<Vec3>::_W_42);
static const Pb::Register _R_105("ParticleDataImpl<Vec3>",
                                 "sumMagnitude",
                                 ParticleDataImpl<Vec3>::_W_43);
static const Pb::Register _R_106("ParticleDataImpl<Vec3>",
                                 "setConstIntFlag",
                                 ParticleDataImpl<Vec3>::_W_44);
static const Pb::Register _R_107("ParticleDataImpl<Vec3>",
                                 "printPdata",
                                 ParticleDataImpl<Vec3>::_W_45);
static const Pb::Register _R_108("ParticleDataImpl<Vec3>", "save", ParticleDataImpl<Vec3>::_W_46);
static const Pb::Register _R_109("ParticleDataImpl<Vec3>", "load", ParticleDataImpl<Vec3>::_W_47);
static const Pb::Register _R_110("ParticleDataImpl<Vec3>",
                                 "getDataPointer",
                                 ParticleDataImpl<Vec3>::_W_48);
#endif
#ifdef _C_ParticleIndexSystem
static const Pb::Register _R_111("ParticleIndexSystem",
                                 "ParticleIndexSystem",
                                 "ParticleSystem<ParticleIndexData>");
template<> const char *Namify<ParticleIndexSystem>::S = "ParticleIndexSystem";
static const Pb::Register _R_112("ParticleIndexSystem",
                                 "ParticleIndexSystem",
                                 ParticleIndexSystem::_W_19);
#endif
#ifdef _C_ParticleSystem
static const Pb::Register _R_113("ParticleSystem<BasicParticleData>",
                                 "ParticleSystem<BasicParticleData>",
                                 "ParticleBase");
template<>
const char *Namify<ParticleSystem<BasicParticleData>>::S = "ParticleSystem<BasicParticleData>";
static const Pb::Register _R_114("ParticleSystem<BasicParticleData>",
                                 "ParticleSystem",
                                 ParticleSystem<BasicParticleData>::_W_2);
static const Pb::Register _R_115("ParticleSystem<BasicParticleData>",
                                 "pySize",
                                 ParticleSystem<BasicParticleData>::_W_3);
static const Pb::Register _R_116("ParticleSystem<BasicParticleData>",
                                 "setPos",
                                 ParticleSystem<BasicParticleData>::_W_4);
static const Pb::Register _R_117("ParticleSystem<BasicParticleData>",
                                 "getPos",
                                 ParticleSystem<BasicParticleData>::_W_5);
static const Pb::Register _R_118("ParticleSystem<BasicParticleData>",
                                 "getPosPdata",
                                 ParticleSystem<BasicParticleData>::_W_6);
static const Pb::Register _R_119("ParticleSystem<BasicParticleData>",
                                 "setPosPdata",
                                 ParticleSystem<BasicParticleData>::_W_7);
static const Pb::Register _R_120("ParticleSystem<BasicParticleData>",
                                 "clear",
                                 ParticleSystem<BasicParticleData>::_W_8);
static const Pb::Register _R_121("ParticleSystem<BasicParticleData>",
                                 "advectInGrid",
                                 ParticleSystem<BasicParticleData>::_W_9);
static const Pb::Register _R_122("ParticleSystem<BasicParticleData>",
                                 "projectOutside",
                                 ParticleSystem<BasicParticleData>::_W_10);
static const Pb::Register _R_123("ParticleSystem<BasicParticleData>",
                                 "projectOutOfBnd",
                                 ParticleSystem<BasicParticleData>::_W_11);
static const Pb::Register _R_124("ParticleSystem<ParticleIndexData>",
                                 "ParticleSystem<ParticleIndexData>",
                                 "ParticleBase");
template<>
const char *Namify<ParticleSystem<ParticleIndexData>>::S = "ParticleSystem<ParticleIndexData>";
static const Pb::Register _R_125("ParticleSystem<ParticleIndexData>",
                                 "ParticleSystem",
                                 ParticleSystem<ParticleIndexData>::_W_2);
static const Pb::Register _R_126("ParticleSystem<ParticleIndexData>",
                                 "pySize",
                                 ParticleSystem<ParticleIndexData>::_W_3);
static const Pb::Register _R_127("ParticleSystem<ParticleIndexData>",
                                 "setPos",
                                 ParticleSystem<ParticleIndexData>::_W_4);
static const Pb::Register _R_128("ParticleSystem<ParticleIndexData>",
                                 "getPos",
                                 ParticleSystem<ParticleIndexData>::_W_5);
static const Pb::Register _R_129("ParticleSystem<ParticleIndexData>",
                                 "getPosPdata",
                                 ParticleSystem<ParticleIndexData>::_W_6);
static const Pb::Register _R_130("ParticleSystem<ParticleIndexData>",
                                 "setPosPdata",
                                 ParticleSystem<ParticleIndexData>::_W_7);
static const Pb::Register _R_131("ParticleSystem<ParticleIndexData>",
                                 "clear",
                                 ParticleSystem<ParticleIndexData>::_W_8);
static const Pb::Register _R_132("ParticleSystem<ParticleIndexData>",
                                 "advectInGrid",
                                 ParticleSystem<ParticleIndexData>::_W_9);
static const Pb::Register _R_133("ParticleSystem<ParticleIndexData>",
                                 "projectOutside",
                                 ParticleSystem<ParticleIndexData>::_W_10);
static const Pb::Register _R_134("ParticleSystem<ParticleIndexData>",
                                 "projectOutOfBnd",
                                 ParticleSystem<ParticleIndexData>::_W_11);
#endif
static const Pb::Register _R_10("ParticleDataImpl<int>", "PdataInt", "");
static const Pb::Register _R_11("ParticleDataImpl<Real>", "PdataReal", "");
static const Pb::Register _R_12("ParticleDataImpl<Vec3>", "PdataVec3", "");
extern "C" {
void PbRegister_file_10()
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
  KEEP_UNUSED(_R_38);
  KEEP_UNUSED(_R_39);
  KEEP_UNUSED(_R_40);
  KEEP_UNUSED(_R_41);
  KEEP_UNUSED(_R_42);
  KEEP_UNUSED(_R_43);
  KEEP_UNUSED(_R_44);
  KEEP_UNUSED(_R_45);
  KEEP_UNUSED(_R_46);
  KEEP_UNUSED(_R_47);
  KEEP_UNUSED(_R_48);
  KEEP_UNUSED(_R_49);
  KEEP_UNUSED(_R_50);
  KEEP_UNUSED(_R_51);
  KEEP_UNUSED(_R_52);
  KEEP_UNUSED(_R_53);
  KEEP_UNUSED(_R_54);
  KEEP_UNUSED(_R_55);
  KEEP_UNUSED(_R_56);
  KEEP_UNUSED(_R_57);
  KEEP_UNUSED(_R_58);
  KEEP_UNUSED(_R_59);
  KEEP_UNUSED(_R_60);
  KEEP_UNUSED(_R_61);
  KEEP_UNUSED(_R_62);
  KEEP_UNUSED(_R_63);
  KEEP_UNUSED(_R_64);
  KEEP_UNUSED(_R_65);
  KEEP_UNUSED(_R_66);
  KEEP_UNUSED(_R_67);
  KEEP_UNUSED(_R_68);
  KEEP_UNUSED(_R_69);
  KEEP_UNUSED(_R_70);
  KEEP_UNUSED(_R_71);
  KEEP_UNUSED(_R_72);
  KEEP_UNUSED(_R_73);
  KEEP_UNUSED(_R_74);
  KEEP_UNUSED(_R_75);
  KEEP_UNUSED(_R_76);
  KEEP_UNUSED(_R_77);
  KEEP_UNUSED(_R_78);
  KEEP_UNUSED(_R_79);
  KEEP_UNUSED(_R_80);
  KEEP_UNUSED(_R_81);
  KEEP_UNUSED(_R_82);
  KEEP_UNUSED(_R_83);
  KEEP_UNUSED(_R_84);
  KEEP_UNUSED(_R_85);
  KEEP_UNUSED(_R_86);
  KEEP_UNUSED(_R_87);
  KEEP_UNUSED(_R_88);
  KEEP_UNUSED(_R_89);
  KEEP_UNUSED(_R_90);
  KEEP_UNUSED(_R_91);
  KEEP_UNUSED(_R_92);
  KEEP_UNUSED(_R_93);
  KEEP_UNUSED(_R_94);
  KEEP_UNUSED(_R_95);
  KEEP_UNUSED(_R_96);
  KEEP_UNUSED(_R_97);
  KEEP_UNUSED(_R_98);
  KEEP_UNUSED(_R_99);
  KEEP_UNUSED(_R_100);
  KEEP_UNUSED(_R_101);
  KEEP_UNUSED(_R_102);
  KEEP_UNUSED(_R_103);
  KEEP_UNUSED(_R_104);
  KEEP_UNUSED(_R_105);
  KEEP_UNUSED(_R_106);
  KEEP_UNUSED(_R_107);
  KEEP_UNUSED(_R_108);
  KEEP_UNUSED(_R_109);
  KEEP_UNUSED(_R_110);
  KEEP_UNUSED(_R_111);
  KEEP_UNUSED(_R_112);
  KEEP_UNUSED(_R_113);
  KEEP_UNUSED(_R_114);
  KEEP_UNUSED(_R_115);
  KEEP_UNUSED(_R_116);
  KEEP_UNUSED(_R_117);
  KEEP_UNUSED(_R_118);
  KEEP_UNUSED(_R_119);
  KEEP_UNUSED(_R_120);
  KEEP_UNUSED(_R_121);
  KEEP_UNUSED(_R_122);
  KEEP_UNUSED(_R_123);
  KEEP_UNUSED(_R_124);
  KEEP_UNUSED(_R_125);
  KEEP_UNUSED(_R_126);
  KEEP_UNUSED(_R_127);
  KEEP_UNUSED(_R_128);
  KEEP_UNUSED(_R_129);
  KEEP_UNUSED(_R_130);
  KEEP_UNUSED(_R_131);
  KEEP_UNUSED(_R_132);
  KEEP_UNUSED(_R_133);
  KEEP_UNUSED(_R_134);
}
}
}  // namespace Manta