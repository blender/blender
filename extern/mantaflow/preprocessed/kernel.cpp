

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

#include "kernel.h"
#include "grid.h"
#include "grid4d.h"
#include "particle.h"

namespace Manta {

KernelBase::KernelBase(const GridBase *base, int bnd)
    : maxX(base->getSizeX() - bnd),
      maxY(base->getSizeY() - bnd),
      maxZ(base->is3D() ? (base->getSizeZ() - bnd) : 1),
      minZ(base->is3D() ? bnd : 0),
      maxT(1),
      minT(0),
      X(base->getStrideX()),
      Y(base->getStrideY()),
      Z(base->getStrideZ()),
      dimT(0),
      size(base->getSizeX() * base->getSizeY() * (IndexInt)base->getSizeZ())
{
}

KernelBase::KernelBase(IndexInt num)
    : maxX(0), maxY(0), maxZ(0), minZ(0), maxT(0), X(0), Y(0), Z(0), dimT(0), size(num)
{
}

KernelBase::KernelBase(const Grid4dBase *base, int bnd)
    : maxX(base->getSizeX() - bnd),
      maxY(base->getSizeY() - bnd),
      maxZ(base->getSizeZ() - bnd),
      minZ(bnd),
      maxT(base->getSizeT() - bnd),
      minT(bnd),
      X(base->getStrideX()),
      Y(base->getStrideY()),
      Z(base->getStrideZ()),
      dimT(base->getStrideT()),
      size(base->getSizeX() * base->getSizeY() * base->getSizeZ() * (IndexInt)base->getSizeT())
{
}

}  // namespace Manta
