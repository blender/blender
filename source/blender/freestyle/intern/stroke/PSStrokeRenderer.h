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

#ifndef __FREESTYLE_PS_STROKE_RENDERER_H__
#define __FREESTYLE_PS_STROKE_RENDERER_H__

/** \file blender/freestyle/intern/stroke/PSStrokeRenderer.h
 *  \ingroup freestyle
 *  \brief Class to define the Postscript rendering of a stroke
 *  \author Stephane Grabli
 *  \date 10/26/2004
 */

#include <fstream>

#include "StrokeRenderer.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*         PSStrokeRenderer       */
/*                                */
/*                                */
/**********************************/

class LIB_STROKE_EXPORT PSStrokeRenderer : public StrokeRenderer
{
public:
	PSStrokeRenderer(const char *iFileName = NULL);
	virtual ~PSStrokeRenderer();

	/*! Renders a stroke rep */
	virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
	virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

	/*! Closes the output PS file */
	void Close();

protected:
	mutable ofstream _ofstream;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_PS_STROKE_RENDERER_H__
