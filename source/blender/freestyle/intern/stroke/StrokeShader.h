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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_STROKE_SHADERS_H__
#define __FREESTYLE_STROKE_SHADERS_H__

/** \file blender/freestyle/intern/stroke/StrokeShader.h
 *  \ingroup freestyle
 *  \brief Class defining StrokeShader
 *  \author Stephane Grabli
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include <iostream>
#include <vector>

#include "../python/Director.h"

//
//  StrokeShader base class
//
//////////////////////////////////////////////////////

class Stroke;

/*! Base class for Stroke Shaders.
 *  Any Stroke Shader must inherit from this class and overload the shade() method.
 *  A StrokeShader is designed to modify any Stroke's attribute such as Thickness, Color,
 *  Geometry, Texture, Blending mode...
 *  The basic way to achieve this operation consists in iterating over the StrokeVertices of the Stroke
 *  and to modify each one's StrokeAttribute.
 *  Here is a python code example of such an iteration:
 *  \code
 *  it = ioStroke.strokeVerticesBegin()
 *  while not it.isEnd():
 *      att = it.getObject().attribute()
 *      ## perform here any attribute modification
 *      it.increment()
 *  \endcode
 *  Here is a C++ code example of such an iteration:
 *  \code
 *  for(StrokeInternal::StrokeVertexIterator v = ioStroke.strokeVerticesBegin(), vend = ioStroke.strokeVerticesEnd();
 *      v != vend;
 *      ++v)
 *  {
 *  	StrokeAttribute& att = v->attribute();
 *  	// perform any attribute modification here...
 *  }
 *  \endcode
 */
class LIB_STROKE_EXPORT StrokeShader
{
public:
	PyObject *py_ss;

	/*! Default constructor. */
	StrokeShader()
	{
		py_ss = 0;
	}

	/*! Destructor. */
	virtual ~StrokeShader() {}

	/*! Returns the string corresponding to the shader's name. */
	virtual string getName() const
	{
		return "StrokeShader";
	}

	/*! The shading method. This method must be overloaded by inherited classes.
	 *  \param ioStroke
	 *    The stroke we wish to shade. this Stroke is modified by the Shader (which typically
	 *    modifies the Stroke's attribute's values such as Color, Thickness, Geometry...)
	 */
	virtual int shade(Stroke& ioStroke) const
	{
		return Director_BPy_StrokeShader_shade( const_cast<StrokeShader *>(this), ioStroke);
	}
};

#endif // __FREESTYLE_STROKE_SHADERS_H__
