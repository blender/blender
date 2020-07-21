

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "mesh.h"
namespace Manta {
#ifdef _C_Mesh
static const Pb::Register _R_12("Mesh", "Mesh", "PbClass");
template<> const char *Namify<Mesh>::S = "Mesh";
static const Pb::Register _R_13("Mesh", "Mesh", Mesh::_W_0);
static const Pb::Register _R_14("Mesh", "clear", Mesh::_W_1);
static const Pb::Register _R_15("Mesh", "fromShape", Mesh::_W_2);
static const Pb::Register _R_16("Mesh", "advectInGrid", Mesh::_W_3);
static const Pb::Register _R_17("Mesh", "scale", Mesh::_W_4);
static const Pb::Register _R_18("Mesh", "offset", Mesh::_W_5);
static const Pb::Register _R_19("Mesh", "rotate", Mesh::_W_6);
static const Pb::Register _R_20("Mesh", "computeVelocity", Mesh::_W_7);
static const Pb::Register _R_21("Mesh", "load", Mesh::_W_8);
static const Pb::Register _R_22("Mesh", "save", Mesh::_W_9);
static const Pb::Register _R_23("Mesh", "computeLevelset", Mesh::_W_10);
static const Pb::Register _R_24("Mesh", "getLevelset", Mesh::_W_11);
static const Pb::Register _R_25("Mesh", "applyMeshToGrid", Mesh::_W_12);
static const Pb::Register _R_26("Mesh", "getNodesDataPointer", Mesh::_W_13);
static const Pb::Register _R_27("Mesh", "getTrisDataPointer", Mesh::_W_14);
static const Pb::Register _R_28("Mesh", "create", Mesh::_W_15);
#endif
#ifdef _C_MeshDataBase
static const Pb::Register _R_29("MeshDataBase", "MeshDataBase", "PbClass");
template<> const char *Namify<MeshDataBase>::S = "MeshDataBase";
static const Pb::Register _R_30("MeshDataBase", "MeshDataBase", MeshDataBase::_W_16);
#endif
#ifdef _C_MeshDataImpl
static const Pb::Register _R_31("MeshDataImpl<int>", "MeshDataImpl<int>", "MeshDataBase");
template<> const char *Namify<MeshDataImpl<int>>::S = "MeshDataImpl<int>";
static const Pb::Register _R_32("MeshDataImpl<int>", "MeshDataImpl", MeshDataImpl<int>::_W_17);
static const Pb::Register _R_33("MeshDataImpl<int>", "clear", MeshDataImpl<int>::_W_18);
static const Pb::Register _R_34("MeshDataImpl<int>", "setSource", MeshDataImpl<int>::_W_19);
static const Pb::Register _R_35("MeshDataImpl<int>", "setConst", MeshDataImpl<int>::_W_20);
static const Pb::Register _R_36("MeshDataImpl<int>", "setConstRange", MeshDataImpl<int>::_W_21);
static const Pb::Register _R_37("MeshDataImpl<int>", "copyFrom", MeshDataImpl<int>::_W_22);
static const Pb::Register _R_38("MeshDataImpl<int>", "add", MeshDataImpl<int>::_W_23);
static const Pb::Register _R_39("MeshDataImpl<int>", "sub", MeshDataImpl<int>::_W_24);
static const Pb::Register _R_40("MeshDataImpl<int>", "addConst", MeshDataImpl<int>::_W_25);
static const Pb::Register _R_41("MeshDataImpl<int>", "addScaled", MeshDataImpl<int>::_W_26);
static const Pb::Register _R_42("MeshDataImpl<int>", "mult", MeshDataImpl<int>::_W_27);
static const Pb::Register _R_43("MeshDataImpl<int>", "multConst", MeshDataImpl<int>::_W_28);
static const Pb::Register _R_44("MeshDataImpl<int>", "safeDiv", MeshDataImpl<int>::_W_29);
static const Pb::Register _R_45("MeshDataImpl<int>", "clamp", MeshDataImpl<int>::_W_30);
static const Pb::Register _R_46("MeshDataImpl<int>", "clampMin", MeshDataImpl<int>::_W_31);
static const Pb::Register _R_47("MeshDataImpl<int>", "clampMax", MeshDataImpl<int>::_W_32);
static const Pb::Register _R_48("MeshDataImpl<int>", "getMaxAbs", MeshDataImpl<int>::_W_33);
static const Pb::Register _R_49("MeshDataImpl<int>", "getMax", MeshDataImpl<int>::_W_34);
static const Pb::Register _R_50("MeshDataImpl<int>", "getMin", MeshDataImpl<int>::_W_35);
static const Pb::Register _R_51("MeshDataImpl<int>", "sum", MeshDataImpl<int>::_W_36);
static const Pb::Register _R_52("MeshDataImpl<int>", "sumSquare", MeshDataImpl<int>::_W_37);
static const Pb::Register _R_53("MeshDataImpl<int>", "sumMagnitude", MeshDataImpl<int>::_W_38);
static const Pb::Register _R_54("MeshDataImpl<int>", "setConstIntFlag", MeshDataImpl<int>::_W_39);
static const Pb::Register _R_55("MeshDataImpl<int>", "printMdata", MeshDataImpl<int>::_W_40);
static const Pb::Register _R_56("MeshDataImpl<int>", "save", MeshDataImpl<int>::_W_41);
static const Pb::Register _R_57("MeshDataImpl<int>", "load", MeshDataImpl<int>::_W_42);
static const Pb::Register _R_58("MeshDataImpl<int>", "getDataPointer", MeshDataImpl<int>::_W_43);
static const Pb::Register _R_59("MeshDataImpl<Real>", "MeshDataImpl<Real>", "MeshDataBase");
template<> const char *Namify<MeshDataImpl<Real>>::S = "MeshDataImpl<Real>";
static const Pb::Register _R_60("MeshDataImpl<Real>", "MeshDataImpl", MeshDataImpl<Real>::_W_17);
static const Pb::Register _R_61("MeshDataImpl<Real>", "clear", MeshDataImpl<Real>::_W_18);
static const Pb::Register _R_62("MeshDataImpl<Real>", "setSource", MeshDataImpl<Real>::_W_19);
static const Pb::Register _R_63("MeshDataImpl<Real>", "setConst", MeshDataImpl<Real>::_W_20);
static const Pb::Register _R_64("MeshDataImpl<Real>", "setConstRange", MeshDataImpl<Real>::_W_21);
static const Pb::Register _R_65("MeshDataImpl<Real>", "copyFrom", MeshDataImpl<Real>::_W_22);
static const Pb::Register _R_66("MeshDataImpl<Real>", "add", MeshDataImpl<Real>::_W_23);
static const Pb::Register _R_67("MeshDataImpl<Real>", "sub", MeshDataImpl<Real>::_W_24);
static const Pb::Register _R_68("MeshDataImpl<Real>", "addConst", MeshDataImpl<Real>::_W_25);
static const Pb::Register _R_69("MeshDataImpl<Real>", "addScaled", MeshDataImpl<Real>::_W_26);
static const Pb::Register _R_70("MeshDataImpl<Real>", "mult", MeshDataImpl<Real>::_W_27);
static const Pb::Register _R_71("MeshDataImpl<Real>", "multConst", MeshDataImpl<Real>::_W_28);
static const Pb::Register _R_72("MeshDataImpl<Real>", "safeDiv", MeshDataImpl<Real>::_W_29);
static const Pb::Register _R_73("MeshDataImpl<Real>", "clamp", MeshDataImpl<Real>::_W_30);
static const Pb::Register _R_74("MeshDataImpl<Real>", "clampMin", MeshDataImpl<Real>::_W_31);
static const Pb::Register _R_75("MeshDataImpl<Real>", "clampMax", MeshDataImpl<Real>::_W_32);
static const Pb::Register _R_76("MeshDataImpl<Real>", "getMaxAbs", MeshDataImpl<Real>::_W_33);
static const Pb::Register _R_77("MeshDataImpl<Real>", "getMax", MeshDataImpl<Real>::_W_34);
static const Pb::Register _R_78("MeshDataImpl<Real>", "getMin", MeshDataImpl<Real>::_W_35);
static const Pb::Register _R_79("MeshDataImpl<Real>", "sum", MeshDataImpl<Real>::_W_36);
static const Pb::Register _R_80("MeshDataImpl<Real>", "sumSquare", MeshDataImpl<Real>::_W_37);
static const Pb::Register _R_81("MeshDataImpl<Real>", "sumMagnitude", MeshDataImpl<Real>::_W_38);
static const Pb::Register _R_82("MeshDataImpl<Real>",
                                "setConstIntFlag",
                                MeshDataImpl<Real>::_W_39);
static const Pb::Register _R_83("MeshDataImpl<Real>", "printMdata", MeshDataImpl<Real>::_W_40);
static const Pb::Register _R_84("MeshDataImpl<Real>", "save", MeshDataImpl<Real>::_W_41);
static const Pb::Register _R_85("MeshDataImpl<Real>", "load", MeshDataImpl<Real>::_W_42);
static const Pb::Register _R_86("MeshDataImpl<Real>", "getDataPointer", MeshDataImpl<Real>::_W_43);
static const Pb::Register _R_87("MeshDataImpl<Vec3>", "MeshDataImpl<Vec3>", "MeshDataBase");
template<> const char *Namify<MeshDataImpl<Vec3>>::S = "MeshDataImpl<Vec3>";
static const Pb::Register _R_88("MeshDataImpl<Vec3>", "MeshDataImpl", MeshDataImpl<Vec3>::_W_17);
static const Pb::Register _R_89("MeshDataImpl<Vec3>", "clear", MeshDataImpl<Vec3>::_W_18);
static const Pb::Register _R_90("MeshDataImpl<Vec3>", "setSource", MeshDataImpl<Vec3>::_W_19);
static const Pb::Register _R_91("MeshDataImpl<Vec3>", "setConst", MeshDataImpl<Vec3>::_W_20);
static const Pb::Register _R_92("MeshDataImpl<Vec3>", "setConstRange", MeshDataImpl<Vec3>::_W_21);
static const Pb::Register _R_93("MeshDataImpl<Vec3>", "copyFrom", MeshDataImpl<Vec3>::_W_22);
static const Pb::Register _R_94("MeshDataImpl<Vec3>", "add", MeshDataImpl<Vec3>::_W_23);
static const Pb::Register _R_95("MeshDataImpl<Vec3>", "sub", MeshDataImpl<Vec3>::_W_24);
static const Pb::Register _R_96("MeshDataImpl<Vec3>", "addConst", MeshDataImpl<Vec3>::_W_25);
static const Pb::Register _R_97("MeshDataImpl<Vec3>", "addScaled", MeshDataImpl<Vec3>::_W_26);
static const Pb::Register _R_98("MeshDataImpl<Vec3>", "mult", MeshDataImpl<Vec3>::_W_27);
static const Pb::Register _R_99("MeshDataImpl<Vec3>", "multConst", MeshDataImpl<Vec3>::_W_28);
static const Pb::Register _R_100("MeshDataImpl<Vec3>", "safeDiv", MeshDataImpl<Vec3>::_W_29);
static const Pb::Register _R_101("MeshDataImpl<Vec3>", "clamp", MeshDataImpl<Vec3>::_W_30);
static const Pb::Register _R_102("MeshDataImpl<Vec3>", "clampMin", MeshDataImpl<Vec3>::_W_31);
static const Pb::Register _R_103("MeshDataImpl<Vec3>", "clampMax", MeshDataImpl<Vec3>::_W_32);
static const Pb::Register _R_104("MeshDataImpl<Vec3>", "getMaxAbs", MeshDataImpl<Vec3>::_W_33);
static const Pb::Register _R_105("MeshDataImpl<Vec3>", "getMax", MeshDataImpl<Vec3>::_W_34);
static const Pb::Register _R_106("MeshDataImpl<Vec3>", "getMin", MeshDataImpl<Vec3>::_W_35);
static const Pb::Register _R_107("MeshDataImpl<Vec3>", "sum", MeshDataImpl<Vec3>::_W_36);
static const Pb::Register _R_108("MeshDataImpl<Vec3>", "sumSquare", MeshDataImpl<Vec3>::_W_37);
static const Pb::Register _R_109("MeshDataImpl<Vec3>", "sumMagnitude", MeshDataImpl<Vec3>::_W_38);
static const Pb::Register _R_110("MeshDataImpl<Vec3>",
                                 "setConstIntFlag",
                                 MeshDataImpl<Vec3>::_W_39);
static const Pb::Register _R_111("MeshDataImpl<Vec3>", "printMdata", MeshDataImpl<Vec3>::_W_40);
static const Pb::Register _R_112("MeshDataImpl<Vec3>", "save", MeshDataImpl<Vec3>::_W_41);
static const Pb::Register _R_113("MeshDataImpl<Vec3>", "load", MeshDataImpl<Vec3>::_W_42);
static const Pb::Register _R_114("MeshDataImpl<Vec3>",
                                 "getDataPointer",
                                 MeshDataImpl<Vec3>::_W_43);
#endif
static const Pb::Register _R_9("MeshDataImpl<int>", "MdataInt", "");
static const Pb::Register _R_10("MeshDataImpl<Real>", "MdataReal", "");
static const Pb::Register _R_11("MeshDataImpl<Vec3>", "MdataVec3", "");
extern "C" {
void PbRegister_file_9()
{
  KEEP_UNUSED(_R_12);
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
}
}
}  // namespace Manta