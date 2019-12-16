

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "shapes.h"
namespace Manta {
#ifdef _C_Box
static const Pb::Register _R_12("Box", "Box", "Shape");
template<> const char *Namify<Box>::S = "Box";
static const Pb::Register _R_13("Box", "Box", Box::_W_9);
#endif
#ifdef _C_Cylinder
static const Pb::Register _R_14("Cylinder", "Cylinder", "Shape");
template<> const char *Namify<Cylinder>::S = "Cylinder";
static const Pb::Register _R_15("Cylinder", "Cylinder", Cylinder::_W_11);
static const Pb::Register _R_16("Cylinder", "setRadius", Cylinder::_W_12);
static const Pb::Register _R_17("Cylinder", "setZ", Cylinder::_W_13);
#endif
#ifdef _C_NullShape
static const Pb::Register _R_18("NullShape", "NullShape", "Shape");
template<> const char *Namify<NullShape>::S = "NullShape";
static const Pb::Register _R_19("NullShape", "NullShape", NullShape::_W_8);
#endif
#ifdef _C_Shape
static const Pb::Register _R_20("Shape", "Shape", "PbClass");
template<> const char *Namify<Shape>::S = "Shape";
static const Pb::Register _R_21("Shape", "Shape", Shape::_W_0);
static const Pb::Register _R_22("Shape", "applyToGrid", Shape::_W_1);
static const Pb::Register _R_23("Shape", "applyToGridSmooth", Shape::_W_2);
static const Pb::Register _R_24("Shape", "computeLevelset", Shape::_W_3);
static const Pb::Register _R_25("Shape", "collideMesh", Shape::_W_4);
static const Pb::Register _R_26("Shape", "getCenter", Shape::_W_5);
static const Pb::Register _R_27("Shape", "setCenter", Shape::_W_6);
static const Pb::Register _R_28("Shape", "getExtent", Shape::_W_7);
#endif
#ifdef _C_Slope
static const Pb::Register _R_29("Slope", "Slope", "Shape");
template<> const char *Namify<Slope>::S = "Slope";
static const Pb::Register _R_30("Slope", "Slope", Slope::_W_14);
#endif
#ifdef _C_Sphere
static const Pb::Register _R_31("Sphere", "Sphere", "Shape");
template<> const char *Namify<Sphere>::S = "Sphere";
static const Pb::Register _R_32("Sphere", "Sphere", Sphere::_W_10);
#endif
extern "C" {
void PbRegister_file_12()
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
}
}
}  // namespace Manta