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

#ifndef __GEOMCLEANER_H__
#define __GEOMCLEANER_H__

/** \file blender/freestyle/intern/geometry/GeomCleaner.h
 *  \ingroup freestyle
 *  \brief Class to define a cleaner of geometry providing a set of useful tools
 *  \author Stephane Grabli
 *  \date 04/03/2002
 */

#include "Geom.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

using namespace Geometry;

class GeomCleaner
{
public:
	inline GeomCleaner() {}
	inline ~GeomCleaner() {}

	/*! Sorts an array of Indexed vertices
	 *    iVertices
	 *      Array of vertices to sort. It is organized as a float series of vertex coordinates: XYZXYZXYZ...
	 *    iVSize
	 *      The size of iVertices array.
	 *    iIndices
	 *      The array containing the vertex indices (used to refer to the vertex coordinates in an indexed face). Each
	 *      element is an unsignedeger multiple of 3.
	 *    iISize
	 *      The size of iIndices array
	 *    oVertices
	 *      Output of sorted vertices. A vertex v1 precedes another one v2 in this array if v1.x<v2.x,
	 *      or v1.x=v2.x && v1.y < v2.y or v1.x=v2.y && v1.y=v2.y && v1.z < v2.z.
	 *      The array is organized as a 3-float serie giving the vertices coordinates: XYZXYZXYZ...
	 *    oIndices
	 *      Output corresponding to the iIndices array but reorganized in order to match the sorted vertex array.
	 */
	static void SortIndexedVertexArray(const float *iVertices, unsigned iVSize, const unsigned *iIndices,
	                                   unsigned iISize, real **oVertices, unsigned **oIndices);

	/*! Compress a SORTED indexed vertex array by eliminating multiple appearing occurences of a single vertex.
	 *    iVertices
	 *      The SORTED vertex array to compress. It is organized as a float series of vertex coordinates: XYZXYZXYZ...
	 *    iVSize
	 *      The size of iVertices array.
	 *    iIndices
	 *      The array containing the vertex indices (used to refer to the vertex coordinates in an indexed face).
	 *      Each element is an unsignedeger multiple of 3.
	 *    iISize
	 *      The size of iIndices array
	 *    oVertices
	 *      The vertex array, result of the compression.
	 *      The array is organized as a 3-float serie giving the vertices coordinates: XYZXYZXYZ...
	 *    oVSize
	 *      The size of oVertices.
	 *    oIndices
	 *      The indices array, reorganized to match the compressed oVertices array.
	 */
	static void CompressIndexedVertexArray(const real *iVertices, unsigned iVSize, const unsigned *iIndices,
	                                       unsigned iISize, real **oVertices, unsigned *oVSize, unsigned **oIndices);

	/*! Sorts and compress an array of indexed vertices.
	 *    iVertices
	 *      The vertex array to sort then compress. It is organized as a float series of
	 *      vertex coordinates: XYZXYZXYZ...
	 *    iVSize
	 *      The size of iVertices array.
	 *    iIndices
	 *      The array containing the vertex indices (used to refer to the vertex coordinates in an indexed face).
	 *      Each element is an unsignedeger multiple of 3.
	 *    iISize
	 *      The size of iIndices array
	 *    oVertices
	 *      The vertex array, result of the sorting-compression.
	 *      The array is organized as a 3-float serie giving the vertices coordinates: XYZXYZXYZ...
	 *    oVSize
	 *      The size of oVertices.
	 *    oIndices
	 *      The indices array, reorganized to match the sorted and compressed oVertices array.
	 */
	static void SortAndCompressIndexedVertexArray(const float *iVertices, unsigned iVSize, const unsigned *iIndices,
	                                              unsigned iISize, real **oVertices, unsigned *oVSize,
	                                              unsigned **oIndices);

	/*! Cleans an indexed vertex array. (Identical to SortAndCompress except that we use here a hash table
	 *  to create the new array.)
	 *    iVertices
	 *      The vertex array to sort then compress. It is organized as a float series of
	 *      vertex coordinates: XYZXYZXYZ...
	 *    iVSize
	 *      The size of iVertices array.
	 *    iIndices
	 *      The array containing the vertex indices (used to refer to the vertex coordinates in an indexed face).
	 *      Each element is an unsignedeger multiple of 3.
	 *    iISize
	 *      The size of iIndices array
	 *    oVertices
	 *      The vertex array, result of the sorting-compression.
	 *      The array is organized as a 3-float serie giving the vertices coordinates: XYZXYZXYZ...
	 *    oVSize
	 *      The size of oVertices.
	 *    oIndices
	 *      The indices array, reorganized to match the sorted and compressed oVertices array.
	 */
	static void CleanIndexedVertexArray(const float *iVertices, unsigned iVSize, const unsigned *iIndices,
	                                    unsigned iISize, real **oVertices, unsigned *oVSize, unsigned **oIndices);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:GeomCleaner")
#endif
};


/*! Binary operators */
//inline bool operator<(const IndexedVertex& iv1, const IndexedVertex& iv2);

/*! Class Indexed Vertex. Used to represent an indexed vertex by storing the vertex coordinates as well as its index */
class IndexedVertex
{
private:
	Vec3r _Vector;
	unsigned _index;

public:
	inline IndexedVertex() {}

	inline IndexedVertex(Vec3r iVector, unsigned iIndex)
	{
		_Vector = iVector;
		_index = iIndex;
	}

	/*! accessors */
	inline const Vec3r& vector() const
	{
		return _Vector;
	}

	inline unsigned index()
	{
		return _index;
	}

	inline real x()
	{
		return _Vector[0];
	}

	inline real y()
	{
		return _Vector[1];
	}

	inline real z()
	{
		return _Vector[2];
	}

	/*! modifiers */
	inline void setVector(const Vec3r& iVector)
	{
		_Vector = iVector;
	}

	inline void setIndex(unsigned iIndex)
	{
		_index = iIndex;
	}

	/*! operators */
	IndexedVertex& operator=(const IndexedVertex& iv)
	{
		_Vector = iv._Vector;
		_index  = iv._index;
		return *this;
	}

	inline real operator[](const unsigned i)
	{
		return _Vector[i];
	}

	//friend inline bool operator<(const IndexedVertex& iv1, const IndexedVertex& iv2);
	inline bool operator<(const IndexedVertex& v) const
	{
		return (_Vector < v._Vector);
	}

	inline bool operator==(const IndexedVertex& v)
	{
		return (_Vector == v._Vector);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:IndexedVertex")
#endif
};

#if 0
bool operator<(const IndexedVertex& iv1, const IndexedVertex& iv2)
{
	return iv1.operator<(iv2);
}
#endif

} /* namespace Freestyle */

#endif // __GEOMCLEANER_H__
