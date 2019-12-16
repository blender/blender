

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep link).

#include "fluidsolver.h"
namespace Manta {
#ifdef _C_FluidSolver
static const Pb::Register _R_6("FluidSolver", "Solver", "PbClass");
template<> const char *Namify<FluidSolver>::S = "FluidSolver";
static const Pb::Register _R_7("FluidSolver", "FluidSolver", FluidSolver::_W_0);
static const Pb::Register _R_8("FluidSolver", "getGridSize", FluidSolver::_W_1);
static const Pb::Register _R_9("FluidSolver", "printMemInfo", FluidSolver::_W_2);
static const Pb::Register _R_10("FluidSolver", "step", FluidSolver::_W_3);
static const Pb::Register _R_11("FluidSolver", "adaptTimestep", FluidSolver::_W_4);
static const Pb::Register _R_12("FluidSolver", "create", FluidSolver::_W_5);
static const Pb::Register _R_13("FluidSolver",
                                "timestep",
                                FluidSolver::_GET_mDt,
                                FluidSolver::_SET_mDt);
static const Pb::Register _R_14("FluidSolver",
                                "timeTotal",
                                FluidSolver::_GET_mTimeTotal,
                                FluidSolver::_SET_mTimeTotal);
static const Pb::Register _R_15("FluidSolver",
                                "frame",
                                FluidSolver::_GET_mFrame,
                                FluidSolver::_SET_mFrame);
static const Pb::Register _R_16("FluidSolver",
                                "cfl",
                                FluidSolver::_GET_mCflCond,
                                FluidSolver::_SET_mCflCond);
static const Pb::Register _R_17("FluidSolver",
                                "timestepMin",
                                FluidSolver::_GET_mDtMin,
                                FluidSolver::_SET_mDtMin);
static const Pb::Register _R_18("FluidSolver",
                                "timestepMax",
                                FluidSolver::_GET_mDtMax,
                                FluidSolver::_SET_mDtMax);
static const Pb::Register _R_19("FluidSolver",
                                "frameLength",
                                FluidSolver::_GET_mFrameLength,
                                FluidSolver::_SET_mFrameLength);
static const Pb::Register _R_20("FluidSolver",
                                "timePerFrame",
                                FluidSolver::_GET_mTimePerFrame,
                                FluidSolver::_SET_mTimePerFrame);
#endif
extern "C" {
void PbRegister_file_6()
{
  KEEP_UNUSED(_R_6);
  KEEP_UNUSED(_R_7);
  KEEP_UNUSED(_R_8);
  KEEP_UNUSED(_R_9);
  KEEP_UNUSED(_R_10);
  KEEP_UNUSED(_R_11);
  KEEP_UNUSED(_R_12);
  KEEP_UNUSED(_R_13);
  KEEP_UNUSED(_R_14);
  KEEP_UNUSED(_R_15);
  KEEP_UNUSED(_R_16);
  KEEP_UNUSED(_R_17);
  KEEP_UNUSED(_R_18);
  KEEP_UNUSED(_R_19);
  KEEP_UNUSED(_R_20);
}
}
}  // namespace Manta