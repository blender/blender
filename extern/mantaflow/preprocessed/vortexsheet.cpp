

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Vortex sheets
 * (warning, the vortex methods are currently experimental, and not fully supported!)
 *
 ******************************************************************************/

#include "vortexsheet.h"
#include "solvana.h"

using namespace std;
namespace Manta {

// *****************************************************************************
// VorticityChannel class members

// *****************************************************************************
// VortexSheet Mesh class members

VortexSheetMesh::VortexSheetMesh(FluidSolver *parent) : Mesh(parent), mTexOffset(0.0f)
{
  addTriChannel(&mVorticity);
  addNodeChannel(&mTex1);
  addNodeChannel(&mTex2);
  addNodeChannel(&mTurb);
}

Mesh *VortexSheetMesh::clone()
{
  VortexSheetMesh *nm = new VortexSheetMesh(mParent);
  *nm = *this;
  nm->setName(getName());
  return nm;
}

void VortexSheetMesh::calcVorticity()
{
  for (size_t tri = 0; tri < mTris.size(); tri++) {
    VortexSheetInfo &v = mVorticity.data[tri];
    Vec3 e0 = getEdge(tri, 0), e1 = getEdge(tri, 1), e2 = getEdge(tri, 2);
    Real area = getFaceArea(tri);

    if (area < 1e-10) {
      v.smokeAmount = 0;
      v.vorticity = 0;
    }
    else {
      v.smokeAmount = 0;
      v.vorticity = (v.circulation[0] * e0 + v.circulation[1] * e1 + v.circulation[2] * e2) / area;
    }
  }
}

void VortexSheetMesh::calcCirculation()
{
  for (size_t tri = 0; tri < mTris.size(); tri++) {
    VortexSheetInfo &v = mVorticity.data[tri];
    Vec3 e0 = getEdge(tri, 0), e1 = getEdge(tri, 1), e2 = getEdge(tri, 2);
    Real area = getFaceArea(tri);

    if (area < 1e-10 || normSquare(v.vorticity) < 1e-10) {
      v.circulation = 0;
      continue;
    }

    float cx, cy, cz;
    SolveOverconstraint34(e0.x,
                          e0.y,
                          e0.z,
                          e1.x,
                          e1.y,
                          e1.z,
                          e2.x,
                          e2.y,
                          e2.z,
                          v.vorticity.x,
                          v.vorticity.y,
                          v.vorticity.z,
                          cx,
                          cy,
                          cz);
    v.circulation = Vec3(cx, cy, cz) * area;
  }
}

void VortexSheetMesh::resetTex1()
{
  for (size_t i = 0; i < mNodes.size(); i++)
    mTex1.data[i] = mNodes[i].pos + mTexOffset;
}

void VortexSheetMesh::resetTex2()
{
  for (size_t i = 0; i < mNodes.size(); i++)
    mTex2.data[i] = mNodes[i].pos + mTexOffset;
}

void VortexSheetMesh::reinitTexCoords()
{
  resetTex1();
  resetTex2();
}

};  // namespace Manta
