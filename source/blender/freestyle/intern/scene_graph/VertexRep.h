/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define the representation of a vertex for displaying purpose.
 */

#include "Rep.h"

namespace Freestyle {

class VertexRep : public Rep {
 public:
  inline VertexRep() : Rep()
  {
    _vid = 0;
    _PointSize = 0.0f;
  }

  inline VertexRep(real x, real y, real z, int id = 0) : Rep()
  {
    _coordinates[0] = x;
    _coordinates[1] = y;
    _coordinates[2] = z;

    _vid = id;
    _PointSize = 0.0f;
  }

  inline ~VertexRep() {}

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v)
  {
    Rep::accept(v);
    v.visitVertexRep(*this);
  }

  /** Computes the rep bounding box. */
  virtual void ComputeBBox();

  /** accessors */
  inline const int vid() const
  {
    return _vid;
  }

  inline const real *coordinates() const
  {
    return _coordinates;
  }

  inline real x() const
  {
    return _coordinates[0];
  }

  inline real y() const
  {
    return _coordinates[1];
  }

  inline real z() const
  {
    return _coordinates[2];
  }

  inline float pointSize() const
  {
    return _PointSize;
  }

  /** modifiers */
  inline void setVid(int id)
  {
    _vid = id;
  }

  inline void setX(real x)
  {
    _coordinates[0] = x;
  }

  inline void setY(real y)
  {
    _coordinates[1] = y;
  }

  inline void setZ(real z)
  {
    _coordinates[2] = z;
  }

  inline void setCoordinates(real x, real y, real z)
  {
    _coordinates[0] = x;
    _coordinates[1] = y;
    _coordinates[2] = z;
  }

  inline void setPointSize(float iPointSize)
  {
    _PointSize = iPointSize;
  }

 private:
  int _vid;  // vertex id
  real _coordinates[3];
  float _PointSize;
};

} /* namespace Freestyle */
