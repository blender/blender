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

#ifndef __FREESTYLE_NATURE_H__
#define __FREESTYLE_NATURE_H__

/** \file blender/freestyle/intern/winged_edge/Nature.h
 *  \ingroup freestyle
 *  \brief Different natures for both vertices and edges
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

namespace Freestyle {

/*! Namespace gathering the different possible natures of 0D and 1D elements of the ViewMap */
namespace Nature {

/* XXX Why not using enums??? */

typedef unsigned short VertexNature;
/*! true for any 0D element */
static const VertexNature POINT        = 0;        // 0
/*! true for SVertex */
static const VertexNature S_VERTEX     = (1 << 0); // 1
/*! true for ViewVertex */
static const VertexNature VIEW_VERTEX  = (1 << 1); // 2
/*! true for NonTVertex */
static const VertexNature NON_T_VERTEX = (1 << 2); // 4
/*! true for TVertex */
static const VertexNature T_VERTEX     = (1 << 3); // 8
/*! true for CUSP */
static const VertexNature CUSP         = (1 << 4); // 16

typedef unsigned short EdgeNature;
/*! true for non feature edges (always false for 1D elements of the ViewMap) */
static const EdgeNature NO_FEATURE         = 0;        // 0
/*! true for silhouettes */
static const EdgeNature SILHOUETTE         = (1 << 0); // 1
/*! true for borders */
static const EdgeNature BORDER             = (1 << 1); // 2
/*! true for creases */
static const EdgeNature CREASE             = (1 << 2); // 4
/*! true for ridges */
static const EdgeNature RIDGE              = (1 << 3); // 8
/*! true for valleys */
static const EdgeNature VALLEY             = (1 << 4); // 16
/*! true for suggestive contours */
static const EdgeNature SUGGESTIVE_CONTOUR = (1 << 5); // 32
/*! true for material boundaries */
static const EdgeNature MATERIAL_BOUNDARY  = (1 << 6); // 64
/*! true for user-defined edge marks */
static const EdgeNature EDGE_MARK          = (1 << 7); // 128

} // end of namespace Nature

} /* namespace Freestyle */

#endif // __FREESTYLE_NATURE_H__
