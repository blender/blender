

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "levelset.h"
namespace Manta {
#ifdef _C_LevelsetGrid
static const Pb::Register _R_11("LevelsetGrid", "LevelsetGrid", "Grid<Real>");
template<> const char *Namify<LevelsetGrid>::S = "LevelsetGrid";
static const Pb::Register _R_12("LevelsetGrid", "LevelsetGrid", LevelsetGrid::_W_0);
static const Pb::Register _R_13("LevelsetGrid", "reinitMarching", LevelsetGrid::_W_1);
static const Pb::Register _R_14("LevelsetGrid", "createMesh", LevelsetGrid::_W_2);
static const Pb::Register _R_15("LevelsetGrid", "join", LevelsetGrid::_W_3);
static const Pb::Register _R_16("LevelsetGrid", "subtract", LevelsetGrid::_W_4);
static const Pb::Register _R_17("LevelsetGrid", "initFromFlags", LevelsetGrid::_W_5);
static const Pb::Register _R_18("LevelsetGrid", "fillHoles", LevelsetGrid::_W_6);
static const Pb::Register _R_19("LevelsetGrid", "floodFill", LevelsetGrid::_W_7);
#endif
extern "C" {
void PbRegister_file_11()
{
  KEEP_UNUSED(_R_11);
  KEEP_UNUSED(_R_12);
  KEEP_UNUSED(_R_13);
  KEEP_UNUSED(_R_14);
  KEEP_UNUSED(_R_15);
  KEEP_UNUSED(_R_16);
  KEEP_UNUSED(_R_17);
  KEEP_UNUSED(_R_18);
  KEEP_UNUSED(_R_19);
}
}
}  // namespace Manta