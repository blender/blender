

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2014 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Function and macros for defining compution kernels over grids
 *
 ******************************************************************************/

#ifndef _KERNEL_H
#define _KERNEL_H

#if TBB == 1
#  include <tbb/blocked_range3d.h>
#  include <tbb/blocked_range.h>
#  include <tbb/parallel_for.h>
#  include <tbb/parallel_reduce.h>
#endif

#if OPENMP == 1
#  include <omp.h>
#endif

#include "general.h"

namespace Manta {

// fwd decl
class GridBase;
class Grid4dBase;
class ParticleBase;

// simple iteration
#define FOR_IJK_BND(grid, bnd) \
  for (int k = ((grid).is3D() ? bnd : 0), \
           __kmax = ((grid).is3D() ? ((grid).getSizeZ() - bnd) : 1); \
       k < __kmax; \
       k++) \
    for (int j = bnd; j < (grid).getSizeY() - bnd; j++) \
      for (int i = bnd; i < (grid).getSizeX() - bnd; i++)

#define FOR_IJK_REVERSE(grid) \
  for (int k = (grid).getSizeZ() - 1; k >= 0; k--) \
    for (int j = (grid).getSizeY() - 1; j >= 0; j--) \
      for (int i = (grid).getSizeX() - 1; i >= 0; i--)

#define FOR_IDX(grid) \
  for (IndexInt idx = 0, total = (grid).getSizeX() * (grid).getSizeY() * (grid).getSizeZ(); \
       idx < total; \
       idx++)

#define FOR_IJK(grid) FOR_IJK_BND(grid, 0)

#define FOR_PARTS(parts) for (IndexInt idx = 0, total = (parts).size(); idx < total; idx++)

// simple loop over 4d grids
#define FOR_IJKT_BND(grid, bnd) \
  for (int t = ((grid).is4D() ? bnd : 0); t < ((grid).is4D() ? ((grid).getSizeT() - bnd) : 1); \
       ++t) \
    for (int k = ((grid).is3D() ? bnd : 0); k < ((grid).is3D() ? ((grid).getSizeZ() - bnd) : 1); \
         ++k) \
      for (int j = bnd; j < (grid).getSizeY() - bnd; ++j) \
        for (int i = bnd; i < (grid).getSizeX() - bnd; ++i)

//! Basic data structure for kernel data, initialized based on kernel type (e.g. single, idx, etc).
struct KernelBase {
  int maxX, maxY, maxZ, minZ, maxT, minT;
  int X, Y, Z, dimT;
  IndexInt size;

  KernelBase(IndexInt num);
  KernelBase(const GridBase *base, int bnd);
  KernelBase(const Grid4dBase *base, int bnd);

  // specify in your derived classes:

  // kernel operators
  // ijk mode: void operator() (int i, int j, int k)
  // idx mode: void operator() (IndexInt idx)

  // reduce mode:
  // void join(classname& other)
  // void setup()
};

}  // namespace Manta

// all kernels will automatically be added to the "Kernels" group in doxygen

#endif
