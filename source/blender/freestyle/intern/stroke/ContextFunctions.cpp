/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/stroke/ContextFunctions.cpp
 *  \ingroup freestyle
 *  \brief Functions related to context queries
 *  \brief Interface to access the context related information.
 *  \author Stephane Grabli
 *  \date 20/12/2003
 */

#include "ContextFunctions.h"

#include "../view_map/SteerableViewMap.h"

#include "../system/TimeStamp.h"

namespace Freestyle {

namespace ContextFunctions {

unsigned GetTimeStampCF()
{
	return TimeStamp::instance()->getTimeStamp();
}

unsigned GetCanvasWidthCF()
{
	return Canvas::getInstance()->width();
}

unsigned GetCanvasHeightCF()
{
	return Canvas::getInstance()->height();
}

BBox<Vec2i> GetBorderCF()
{
	return Canvas::getInstance()->border();
}

void LoadMapCF(const char *iFileName, const char *iMapName, unsigned iNbLevels, float iSigma)
{
	return Canvas::getInstance()->loadMap(iFileName, iMapName, iNbLevels, iSigma);
}

float ReadMapPixelCF(const char *iMapName, int level, unsigned x, unsigned y)
{
	Canvas *canvas = Canvas::getInstance();
	return canvas->readMapPixel(iMapName, level, x, y);
}

float ReadCompleteViewMapPixelCF(int level, unsigned x, unsigned y)
{
	SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
	return svm->readCompleteViewMapPixel(level, x, y);
}

float ReadDirectionalViewMapPixelCF(int iOrientation, int level, unsigned x, unsigned y)
{
	SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
	return svm->readSteerableViewMapPixel(iOrientation, level, x, y);
}

FEdge *GetSelectedFEdgeCF()
{
	return Canvas::getInstance()->selectedFEdge();
}

}  // ContextFunctions namespace

} /* namespace Freestyle */
