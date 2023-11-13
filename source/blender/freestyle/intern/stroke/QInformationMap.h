/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class defining an information map using a QImage
 */

#include <qimage.h>

#include "InformationMap.h"

namespace Freestyle {

class QInformationMap : public InformationMap {
 private:
  QImage _map;  // the image or a piece of image

 public:
  QInformationMap();
  QInformationMap(const QImage &);
  QInformationMap(const QInformationMap &);
  QInformationMap &operator=(const QInformationMap &);

  // float getSmoothedPixel(int x, int y, float sigma = 0.2f);1
  virtual float getMean(int x, int y);
  virtual void retrieveMeanAndVariance(int x, int y, float &oMean, float &oVariance);

  inline const QImage &map() const
  {
    return _map;
  }

  inline void setMap(const QImage &iMap, float iw, float ih)
  {
    _map = iMap.copy();
    _w = iw;
    _h = ih;
  }

 protected:
  virtual float computeGaussian(int x, int y);
};

} /* namespace Freestyle */
