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
 * Contributor(s): Tao Ju
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __MODELREADER_H__
#define __MODELREADER_H__

#include "GeoCommon.h"

/*
 * Virtual class for input file readers
 *
 * @author Tao Ju
 */
class ModelReader
{
public:
/// Constructor
ModelReader(){
};

/// Get next triangle
virtual Triangle *getNextTriangle( ) = 0;
virtual int getNextTriangle(int t[3]) = 0;

/// Get bounding box
virtual float getBoundingBox(float origin[3]) = 0;

/// Get number of triangles
virtual int getNumTriangles( ) = 0;

/// Get storage size
virtual int getMemory( ) = 0;

/// Reset file reading location
virtual void reset( ) = 0;

/// For explicit vertex models
virtual int getNumVertices( ) = 0;

virtual void getNextVertex(float v[3]) = 0;

virtual void printInfo( ) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:ModelReader")
#endif

};

#endif  /* __MODELREADER_H__ */
