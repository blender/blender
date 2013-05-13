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

#ifndef __FREESTYLE_STROKE_LAYER_H__
#define __FREESTYLE_STROKE_LAYER_H__

/** \file blender/freestyle/intern/stroke/StrokeLayer.h
 *  \ingroup freestyle
 *  \brief Class to define a layer of strokes.
 *  \author Stephane Grabli
 *  \date 18/12/2002
 */

#include <deque>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class Stroke;
class StrokeRenderer;
class StrokeLayer
{
public:
	typedef std::deque<Stroke*> stroke_container;

protected:
	stroke_container _strokes;

public:
	StrokeLayer() {}

	StrokeLayer(const stroke_container& iStrokes)
	{
		_strokes = iStrokes;
	}

	StrokeLayer(const StrokeLayer& iBrother)
	{
		_strokes = iBrother._strokes;
	}

	virtual ~StrokeLayer();

	/*! Render method */
	void ScaleThickness(float iFactor);
	void Render(const StrokeRenderer *iRenderer);
	void RenderBasic(const StrokeRenderer *iRenderer);

	/*! clears the layer */
	void clear();

	/*! accessors */
	inline stroke_container::iterator strokes_begin()
	{
		return _strokes.begin();
	}

	inline stroke_container::iterator strokes_end()
	{
		return _strokes.end();
	}

	inline int strokes_size() const
	{
		return _strokes.size();
	}

	inline bool empty() const
	{
		return _strokes.empty();
	}

	/*! modifiers */
	inline void setStrokes(stroke_container& iStrokes)
	{
		_strokes = iStrokes;
	}

	inline void AddStroke(Stroke *iStroke)
	{
		_strokes.push_back(iStroke);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeLayer")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_STROKE_LAYER_H__
