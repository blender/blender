#include "registry.h"
static const Pb::Register _reg(
    "python/defines.py",
    "################################################################################\n#\n# "
    "MantaFlow fluid solver framework\n# Copyright 2011 Tobias Pfaff, Nils Thuerey \n#\n# This "
    "program is free software, distributed under the terms of the\n# Apache License, Version 2.0 "
    "\n# http://www.apache.org/licenses/LICENSE-2.0\n#\n# Defines some constants for use in "
    "python "
    "subprograms\n#\n#############################################################################"
    "####\n\n# mantaflow conventions\nReal = float\n\n# some defines to make C code and scripts "
    "more alike...\nfalse = False\ntrue  = True\nVec3  = vec3\nVec4  = vec4\nVec3Grid = "
    "VecGrid\n\n# grid flags\nFlagFluid    = 1\nFlagObstacle = 2\nFlagEmpty    = 4\nFlagInflow   "
    "= 8\nFlagOutflow  = 16\nFlagStick    = 64\nFlagReserved = 256\n# and same for "
    "FlagGrid::CellType enum names:\nTypeFluid    = 1\nTypeObstacle = 2\nTypeEmpty    = "
    "4\nTypeInflow   = 8\nTypeOutflow  = 16\nTypeStick    = 64\nTypeReserved = 256\n\n# "
    "integration mode\nIntEuler = 0\nIntRK2   = 1\nIntRK4   = 2\n\n# CG preconditioner\nPcNone    "
    "  = 0\nPcMIC       = 1\nPcMGDynamic = 2\nPcMGStatic  = 3\n\n# particles\nPtypeSpray   = "
    "2\nPtypeBubble  = 4\nPtypeFoam    = 8\nPtypeTracer  = 16\n\n# OpenVDB export "
    "flags\nCompression_None = 0\nCompression_Zip = 1\nCompression_Blosc = 2\n\n\n\n\n");
extern "C" {
void PbRegister_file_0()
{
  KEEP_UNUSED(_reg);
}
}