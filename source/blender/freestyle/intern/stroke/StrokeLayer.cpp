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

/** \file blender/freestyle/intern/stroke/StrokeLayer.cpp
 *  \ingroup freestyle
 *  \brief Class to define a layer of strokes.
 *  \author Stephane Grabli
 *  \date 18/12/2002
 */

#include "Canvas.h"
#include "Stroke.h"
#include "StrokeLayer.h"

namespace Freestyle {

StrokeLayer::~StrokeLayer()
{
	clear();
}

void StrokeLayer::ScaleThickness(float iFactor)
{
	for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end(); s != send; ++s) {
		(*s)->ScaleThickness(iFactor);
	}
}

void StrokeLayer::SetLineStyle(struct FreestyleLineStyle *iLineStyle)
{
	for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end(); s != send; ++s) {
		(*s)->SetLineStyle(iLineStyle);
	}
}

void StrokeLayer::Render(const StrokeRenderer *iRenderer)
{
	for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end(); s != send; ++s) {
		(*s)->Render(iRenderer);
	}
}

void StrokeLayer::RenderBasic(const StrokeRenderer *iRenderer)
{
	for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end(); s != send; ++s) {
		(*s)->RenderBasic(iRenderer);
	}
}

void StrokeLayer::clear()
{
	for (stroke_container::iterator s = _strokes.begin(), send = _strokes.end(); s != send; ++s)
		delete *s;
	_strokes.clear();
}

} /* namespace Freestyle */
