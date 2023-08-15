/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Functions taking 1D input
 */

#include "AdvancedFunctions1D.h"
#include "Canvas.h"

#include "../view_map/SteerableViewMap.h"

#include "BLI_sys_types.h"

namespace Freestyle::Functions1D {

int GetSteerableViewMapDensityF1D::operator()(Interface1D &inter)
{
  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
  Interface0DIterator it = inter.pointsBegin(_sampling);
  Interface0DIterator itnext = it;
  ++itnext;
  FEdge *fe;
  uint nSVM;
  vector<float> values;

  while (!itnext.isEnd()) {
    Interface0D &i0D = (*it);
    Interface0D &i0Dnext = (*itnext);
    fe = i0D.getFEdge(i0Dnext);
    if (fe == nullptr) {
      cerr << "GetSteerableViewMapDensityF1D warning: no FEdge between " << i0D.getId() << " and "
           << i0Dnext.getId() << endl;
      // compute the direction between these two ???
      Vec2f dir = i0Dnext.getPoint2D() - i0D.getPoint2D();
      nSVM = svm->getSVMNumber(dir);
    }
    else {
      nSVM = svm->getSVMNumber(fe->getId().getFirst());
    }
    Vec2r m((i0D.getProjectedX() + i0Dnext.getProjectedX()) / 2.0,
            (i0D.getProjectedY() + i0Dnext.getProjectedY()) / 2.0);
    values.push_back(svm->readSteerableViewMapPixel(nSVM, _level, int(m[0]), int(m[1])));
    ++it;
    ++itnext;
  }

  float res, res_tmp;
  vector<float>::iterator v = values.begin(), vend = values.end();
  uint size = 1;
  switch (_integration) {
    case MIN:
      res = *v;
      ++v;
      for (; v != vend; ++v) {
        res_tmp = *v;
        if (res_tmp < res) {
          res = res_tmp;
        }
      }
      break;
    case MAX:
      res = *v;
      ++v;
      for (; v != vend; ++v) {
        res_tmp = *v;
        if (res_tmp > res) {
          res = res_tmp;
        }
      }
      break;
    case FIRST:
      res = *v;
      break;
    case LAST:
      --vend;
      res = *vend;
      break;
    case MEAN:
    default:
      res = *v;
      ++v;
      for (; v != vend; ++v, ++size) {
        res += *v;
      }
      res /= (size ? size : 1);
      break;
  }
  result = res;
  return 0;
}

int GetDirectionalViewMapDensityF1D::operator()(Interface1D &inter)
{
  // soc uint size;
  result = integrate(_fun, inter.pointsBegin(_sampling), inter.pointsEnd(_sampling), _integration);
  return 0;
}

int GetCompleteViewMapDensityF1D::operator()(Interface1D &inter)
{
  // soc uint size;
  // Id id = inter.getId(); /* UNUSED */
  result = integrate(_fun, inter.pointsBegin(_sampling), inter.pointsEnd(_sampling), _integration);
  return 0;
}

int GetViewMapGradientNormF1D::operator()(Interface1D &inter)
{
  result = integrate(
      _func, inter.pointsBegin(_sampling), inter.pointsEnd(_sampling), _integration);
  return 0;
}

}  // namespace Freestyle::Functions1D
