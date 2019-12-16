

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
 * Moving obstacles
 *
 ******************************************************************************/

#include "movingobs.h"
#include "commonkernels.h"
#include "randomstream.h"

using namespace std;
namespace Manta {

//******************************************************************************
// MovingObs class members

int MovingObstacle::sIDcnt = 10;

MovingObstacle::MovingObstacle(FluidSolver *parent, int emptyType)
    : PbClass(parent), mEmptyType(emptyType)
{
  mID = 1 << sIDcnt;
  sIDcnt++;
  if (sIDcnt > 15)
    errMsg(
        "currently only 5 separate moving obstacles supported (are you generating them in a "
        "loop?)");
}

void MovingObstacle::add(Shape *shape)
{
  mShapes.push_back(shape);
}

void MovingObstacle::projectOutside(FlagGrid &flags, BasicParticleSystem &parts)
{
  LevelsetGrid levelset(mParent, false);
  Grid<Vec3> gradient(mParent);

  // rebuild obstacle levelset
  FOR_IDX(levelset)
  {
    levelset[idx] = flags.isObstacle(idx) ? -0.5 : 0.5;
  }
  levelset.reinitMarching(flags, 6.0, 0, true, false, FlagGrid::TypeReserved);

  // build levelset gradient
  GradientOp(gradient, levelset);

  parts.projectOutside(gradient);
}

void MovingObstacle::moveLinear(
    Real t, Real t0, Real t1, Vec3 p0, Vec3 p1, FlagGrid &flags, MACGrid &vel, bool smooth)
{
  Real alpha = (t - t0) / (t1 - t0);
  if (alpha >= 0 && alpha <= 1) {
    Vec3 v = (p1 - p0) / ((t1 - t0) * getParent()->getDt());

    // ease in and out
    if (smooth) {
      v *= 6.0f * (alpha - square(alpha));
      alpha = square(alpha) * (3.0f - 2.0f * alpha);
    }

    Vec3 pos = alpha * p1 + (1.0f - alpha) * p0;
    for (size_t i = 0; i < mShapes.size(); i++)
      mShapes[i]->setCenter(pos);

    // reset flags
    FOR_IDX(flags)
    {
      if ((flags[idx] & mID) != 0)
        flags[idx] = mEmptyType;
    }
    // apply new flags
    for (size_t i = 0; i < mShapes.size(); i++) {
#if NOPYTHON != 1
      mShapes[i]->_args.clear();
      mShapes[i]->_args.add("value", FlagGrid::TypeObstacle | mID);
      mShapes[i]->applyToGrid(&flags, 0);
#else
      errMsg("Not yet supported...");
#endif
    }
    // apply velocities
    FOR_IJK_BND(flags, 1)
    {
      bool cur = (flags(i, j, k) & mID) != 0;
      if (cur || (flags(i - 1, j, k) & mID) != 0)
        vel(i, j, k).x = v.x;
      if (cur || (flags(i, j - 1, k) & mID) != 0)
        vel(i, j, k).y = v.y;
      if (cur || (flags(i, j, k - 1) & mID) != 0)
        vel(i, j, k).z = v.z;
    }
  }
}

}  // namespace Manta
