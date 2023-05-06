/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Functions taking 0D input
 */

#include "AdvancedFunctions0D.h"
#include "Canvas.h"

#include "../view_map/Functions0D.h"
#include "../view_map/SteerableViewMap.h"

namespace Freestyle::Functions0D {

int DensityF0D::operator()(Interface0DIterator &iter)
{
  Canvas *canvas = Canvas::getInstance();
  int bound = _filter.getBound();

  if ((iter->getProjectedX() - bound < 0) || (iter->getProjectedX() + bound > canvas->width()) ||
      (iter->getProjectedY() - bound < 0) || (iter->getProjectedY() + bound > canvas->height()))
  {
    result = 0.0;
    return 0;
  }

  RGBImage image;
  canvas->readColorPixels(int(iter->getProjectedX()) - bound,
                          int(iter->getProjectedY()) - bound,
                          _filter.maskSize(),
                          _filter.maskSize(),
                          image);
  result = _filter.getSmoothedPixel<RGBImage>(
      &image, int(iter->getProjectedX()), int(iter->getProjectedY()));

  return 0;
}

int LocalAverageDepthF0D::operator()(Interface0DIterator &iter)
{
  Canvas *iViewer = Canvas::getInstance();
  int bound = _filter.getBound();

  if ((iter->getProjectedX() - bound < 0) || (iter->getProjectedX() + bound > iViewer->width()) ||
      (iter->getProjectedY() - bound < 0) || (iter->getProjectedY() + bound > iViewer->height()))
  {
    result = 0.0;
    return 0;
  }

  GrayImage image;
  iViewer->readDepthPixels(int(iter->getProjectedX()) - bound,
                           int(iter->getProjectedY()) - bound,
                           _filter.maskSize(),
                           _filter.maskSize(),
                           image);
  result = _filter.getSmoothedPixel(
      &image, int(iter->getProjectedX()), int(iter->getProjectedY()));

  return 0;
}

int ReadMapPixelF0D::operator()(Interface0DIterator &iter)
{
  Canvas *canvas = Canvas::getInstance();
  result = canvas->readMapPixel(
      _mapName, _level, int(iter->getProjectedX()), int(iter->getProjectedY()));
  return 0;
}

int ReadSteerableViewMapPixelF0D::operator()(Interface0DIterator &iter)
{
  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
  result = svm->readSteerableViewMapPixel(
      _orientation, _level, int(iter->getProjectedX()), int(iter->getProjectedY()));
  return 0;
}

int ReadCompleteViewMapPixelF0D::operator()(Interface0DIterator &iter)
{
  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
  result = svm->readCompleteViewMapPixel(
      _level, int(iter->getProjectedX()), int(iter->getProjectedY()));
  return 0;
}

int GetViewMapGradientNormF0D::operator()(Interface0DIterator &iter)
{
  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
  float pxy = svm->readCompleteViewMapPixel(
      _level, int(iter->getProjectedX()), int(iter->getProjectedY()));
  float gx = svm->readCompleteViewMapPixel(
                 _level, int(iter->getProjectedX()) + _step, int(iter->getProjectedY())) -
             pxy;
  float gy = svm->readCompleteViewMapPixel(
                 _level, int(iter->getProjectedX()), int(iter->getProjectedY()) + _step) -
             pxy;
  result = Vec2f(gx, gy).norm();
  return 0;
}

}  // namespace Freestyle::Functions0D
