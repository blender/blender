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

#ifndef __FREESTYLE_MODIFIERS_H__
#define __FREESTYLE_MODIFIERS_H__

/** \file blender/freestyle/intern/stroke/Modifiers.h
 *  \ingroup freestyle
 *  \brief modifiers...
 *  \author Stephane Grabli
 *  \date 05/01/2003
 */

#include "TimeStamp.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/* ----------------------------------------- *
 *                                           *
 *              modifiers                    *
 *                                           *
 * ----------------------------------------- */

/*! Base class for modifiers.
 *  Modifiers are used in the Operators in order to "mark" the processed Interface1D.
 */
template<class Edge>
struct EdgeModifier : public unary_function<Edge, void>
{
	/*! Default construction */
	EdgeModifier() : unary_function<Edge, void>() {}

	/*! the () operator */
	virtual void operator()(Edge& iEdge) {}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:EdgeModifier")
#endif
};

/*! Modifier that sets the time stamp of an Interface1D to the time stamp of the system. */
template<class Edge>
struct TimestampModifier : public EdgeModifier<Edge>
{
	/*! Default constructor */
	TimestampModifier() : EdgeModifier<Edge>() {}

	/*! The () operator. */
	virtual void operator()(Edge& iEdge)
	{
		TimeStamp *timestamp = TimeStamp::instance();
		iEdge.setTimeStamp(timestamp->getTimeStamp());
	}
};

} /* namespace Freestyle */

#endif // MODIFIERS_H
