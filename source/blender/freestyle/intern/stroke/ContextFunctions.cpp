/* SPDX-FileCopyrightText: 2012-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Functions related to context queries
 * \brief Interface to access the context related information.
 */

#include "ContextFunctions.h"

#include "../view_map/SteerableViewMap.h"

#include "../system/TimeStamp.h"

#include "BLI_sys_types.h"

namespace Freestyle::ContextFunctions {

uint GetTimeStampCF()
{
  return TimeStamp::instance()->getTimeStamp();
}

uint GetCanvasWidthCF()
{
  return Canvas::getInstance()->width();
}

uint GetCanvasHeightCF()
{
  return Canvas::getInstance()->height();
}

BBox<Vec2i> GetBorderCF()
{
  return Canvas::getInstance()->border();
}

void LoadMapCF(const char *iFileName, const char *iMapName, uint iNbLevels, float iSigma)
{
  return Canvas::getInstance()->loadMap(iFileName, iMapName, iNbLevels, iSigma);
}

float ReadMapPixelCF(const char *iMapName, int level, uint x, uint y)
{
  Canvas *canvas = Canvas::getInstance();
  return canvas->readMapPixel(iMapName, level, x, y);
}

float ReadCompleteViewMapPixelCF(int level, uint x, uint y)
{
  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
  return svm->readCompleteViewMapPixel(level, x, y);
}

float ReadDirectionalViewMapPixelCF(int iOrientation, int level, uint x, uint y)
{
  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
  return svm->readSteerableViewMapPixel(iOrientation, level, x, y);
}

FEdge *GetSelectedFEdgeCF()
{
  return Canvas::getInstance()->selectedFEdge();
}

}  // namespace Freestyle::ContextFunctions
