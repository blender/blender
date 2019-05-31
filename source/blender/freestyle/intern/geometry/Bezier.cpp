/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 * \brief Class to define a Bezier curve of order 4.
 */

#include "Bezier.h"
#include "FitCurve.h"

using namespace std;

namespace Freestyle {

BezierCurveSegment::BezierCurveSegment()
{
}

BezierCurveSegment::~BezierCurveSegment()
{
}

void BezierCurveSegment::AddControlPoint(const Vec2d &iPoint)
{
  _ControlPolygon.push_back(iPoint);
  if (_ControlPolygon.size() == 4) {
    Build();
  }
}

void BezierCurveSegment::Build()
{
  if (_ControlPolygon.size() != 4) {
    return;
  }

  // Compute the rightmost part of the matrix:
  vector<Vec2d>::const_iterator p0, p1, p2, p3;
  p0 = _ControlPolygon.begin();
  p1 = p0;
  ++p1;
  p2 = p1;
  ++p2;
  p3 = p2;
  ++p3;
  float x[4], y[4];

  x[0] = -p0->x() + 3 * p1->x() - 3 * p2->x() + p3->x();
  x[1] = 3 * p0->x() - 6 * p1->x() + 3 * p2->x();
  x[2] = -3 * p0->x() + 3 * p1->x();
  x[3] = p0->x();

  y[0] = -p0->y() + 3 * p1->y() - 3 * p2->y() + p3->y();
  y[1] = 3 * p0->y() - 6 * p1->y() + 3 * p2->y();
  y[2] = -3 * p0->y() + 3 * p1->y();
  y[3] = p0->y();

  int nvertices = 12;
  float increment = 1.0 / (float)nvertices;
  float t = 0.0f;
  for (int i = 0; i <= nvertices; ++i) {
    _Vertices.push_back(Vec2d((x[3] + t * (x[2] + t * (x[1] + t * x[0]))),
                              (y[3] + t * (y[2] + t * (y[1] + t * y[0])))));
    t += increment;
  }
}

BezierCurve::BezierCurve()
{
  _currentSegment = new BezierCurveSegment;
}

BezierCurve::BezierCurve(vector<Vec2d> &iPoints, double error)
{
  FitCurveWrapper fitcurve;
  _currentSegment = new BezierCurveSegment;
  vector<Vec2d> curve;

  fitcurve.FitCurve(iPoints, curve, error);
  int i = 0;
  vector<Vec2d>::iterator v, vend;
  for (v = curve.begin(), vend = curve.end(); v != vend; ++v) {
    if ((i == 0) || (i % 4 != 0)) {
      AddControlPoint(*v);
    }
    ++i;
  }
}

BezierCurve::~BezierCurve()
{
  if (!_Segments.empty()) {
    vector<BezierCurveSegment *>::iterator v, vend;
    for (v = _Segments.begin(), vend = _Segments.end(); v != vend; ++v) {
      delete *v;
    }
  }
  if (_currentSegment) {
    delete _currentSegment;
  }
}

void BezierCurve::AddControlPoint(const Vec2d &iPoint)
{
  _ControlPolygon.push_back(iPoint);
  _currentSegment->AddControlPoint(iPoint);
  if (_currentSegment->size() == 4) {
    _Segments.push_back(_currentSegment);
    _currentSegment = new BezierCurveSegment;
    _currentSegment->AddControlPoint(iPoint);
  }
}

} /* namespace Freestyle */
