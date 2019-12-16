/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Helper functions for simple integration
 *
 ******************************************************************************/

#ifndef _INTEGRATE_H
#define _INTEGRATE_H

#include <vector>
#include "vectorbase.h"
#include "kernel.h"

namespace Manta {

enum IntegrationMode { IntEuler = 0, IntRK2, IntRK4 };

//! Integrate a particle set with a given velocity kernel
template<class VelKernel> void integratePointSet(VelKernel &k, int mode)
{
  typedef typename VelKernel::type0 PosType;
  PosType &x = k.getArg0();
  const std::vector<Vec3> &u = k.getRet();
  const int N = x.size();

  if (mode == IntEuler) {
    for (int i = 0; i < N; i++)
      x[i].pos += u[i];
  }
  else if (mode == IntRK2) {
    PosType x0(x);

    for (int i = 0; i < N; i++)
      x[i].pos = x0[i].pos + 0.5 * u[i];

    k.run();
    for (int i = 0; i < N; i++)
      x[i].pos = x0[i].pos + u[i];
  }
  else if (mode == IntRK4) {
    PosType x0(x);
    std::vector<Vec3> uTotal(u);

    for (int i = 0; i < N; i++)
      x[i].pos = x0[i].pos + 0.5 * u[i];

    k.run();
    for (int i = 0; i < N; i++) {
      x[i].pos = x0[i].pos + 0.5 * u[i];
      uTotal[i] += 2 * u[i];
    }

    k.run();
    for (int i = 0; i < N; i++) {
      x[i].pos = x0[i].pos + u[i];
      uTotal[i] += 2 * u[i];
    }

    k.run();
    for (int i = 0; i < N; i++)
      x[i].pos = x0[i].pos + (Real)(1. / 6.) * (uTotal[i] + u[i]);
  }
  else
    errMsg("unknown integration type");

  // for(int i=0; i<N; i++) std::cout << x[i].pos.y-x[0].pos.y << std::endl;
  // std::cout << "<><><>" << std::endl;
}

}  // namespace Manta

#endif
