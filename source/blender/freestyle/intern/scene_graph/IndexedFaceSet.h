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

#ifndef __FREESTYLE_INDEXED_FACE_SET_H__
#define __FREESTYLE_INDEXED_FACE_SET_H__

/** \file blender/freestyle/intern/scene_graph/IndexedFaceSet.h
 *  \ingroup freestyle
 *  \brief A Set of indexed faces to represent a surfacic object
 *  \author Stephane Grabli
 *  \date 22/01/2002
 */

#include <memory.h>
#include <stdio.h>

//! inherits from class Rep
#include "Rep.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

class IndexedFaceSet : public Rep
{
public:
	/*! Triangles description style:*/
	enum TRIANGLES_STYLE {
		TRIANGLE_STRIP,
		TRIANGLE_FAN,
		TRIANGLES,
	};

	/*! User-specified face and edge marks for feature edge detection */
	/* XXX Why in hel not use an enum here too? */
	typedef unsigned char FaceEdgeMark;
	static const FaceEdgeMark FACE_MARK =      1 << 0;
	static const FaceEdgeMark EDGE_MARK_V1V2 = 1 << 1;
	static const FaceEdgeMark EDGE_MARK_V2V3 = 1 << 2;
	static const FaceEdgeMark EDGE_MARK_V3V1 = 1 << 3;

	/*! Builds an empty indexed face set  */
	IndexedFaceSet();

	/*! Builds an indexed face set
	 *    iVertices
	 *      The array of object vertices 3D coordinates (for all faces).
	 *      If iCopy != 0, the array is copied; you must desallocate iVertices. Else you must not.
	 *    iVSize
	 *      The size of iVertices (must be a multiple of 3)
	 *    iNormals
	 *      The array of object normals 3D coordinates.
	 *      If iCopy != 0, the array is copied; you must desallocate iNormals. Else you must not.
	 *    iNSize
	 *      The size of iNormals
	 *    iMaterials
	 *      The array of materials
	 *    iMSize
	 *      The size of iMaterials
	 *    iTexCoords
	 *      The array of texture coordinates.
	 *    iTSize
	 *      The size of iTexCoords (must be multiple of 2)
	 *    iNumFaces
	 *      The number of faces
	 *    iNumVertexPerFace
	 *      Array containing the number of vertices per face.
	 *    iFaceStyle
	 *      Array containing the description style of each faces.
	 *      The style belongs to:
	 *        - TRIANGLE_STRIP: the face indices describe a triangle strip
	 *        - TRIANGLE_FAN  : the face indices describe a triangle fan
	 *        - TRIANGLES     : the face indices describe single triangles
	 *      If iCopy != 0, the array is copied; you must desallocate iFaceStyle. Else you must not.
	 *    iVIndices,
	 *      Array of vertices indices.
	 *      The integers contained in this array must be multiple of 3.
	 *      If iCopy != 0, the array is copied; you must desallocate iVIndices. Else you must not.
	 *    iVISize
	 *      The size of iVIndices.
	 *    iNIndices
	 *      Array of normals indices.
	 *      The integers contained in this array must be multiple of 3.
	 *      If iCopy != 0, the array is copied; you must desallocate iNIndices. Else you must not.
	 *    iNISize
	 *      The size of iNIndices
	 *    iMIndices
	 *      The Material indices (per vertex)
	 *    iMISize
	 *      The size of iMIndices
	 *    iTIndices
	 *      The Texture coordinates indices (per vertex). The integers contained in this array must be multiple of 2.
	 *    iTISize
	 *      The size of iMIndices
	 *    iCopy
	 *      0 : the arrays are not copied. The pointers passed as arguments are used. IndexedFaceSet takes these
	 *          arrays desallocation in charge.
	 *      1 : the arrays are copied. The caller is in charge of the arrays, passed as arguments desallocation.
	 */
	IndexedFaceSet(float *iVertices, unsigned iVSize, float *iNormals, unsigned iNSize, FrsMaterial **iMaterials,
	               unsigned iMSize, float *iTexCoords, unsigned iTSize, unsigned iNumFaces, unsigned *iNumVertexPerFace,
	               TRIANGLES_STYLE *iFaceStyle, FaceEdgeMark *iFaceEdgeMarks, unsigned *iVIndices, unsigned iVISize,
	               unsigned *iNIndices, unsigned iNISize, unsigned *iMIndices, unsigned iMISize, unsigned *iTIndices,
	               unsigned iTISize, unsigned iCopy = 1);

	/*! Builds an indexed face set from an other indexed face set */
	IndexedFaceSet(const IndexedFaceSet& iBrother);

	void swap(IndexedFaceSet& ioOther)
	{
		std::swap(_Vertices, ioOther._Vertices);
		std::swap(_Normals, ioOther._Normals);
		std::swap(_FrsMaterials, ioOther._FrsMaterials);
		std::swap(_TexCoords, ioOther._TexCoords);
		std::swap(_FaceEdgeMarks, ioOther._FaceEdgeMarks);

		std::swap(_VSize, ioOther._VSize);
		std::swap(_NSize, ioOther._NSize);
		std::swap(_MSize, ioOther._MSize);
		std::swap(_TSize, ioOther._TSize);

		std::swap(_NumFaces, ioOther._NumFaces);
		std::swap(_NumVertexPerFace, ioOther._NumVertexPerFace);
		std::swap(_FaceStyle, ioOther._FaceStyle);

		std::swap(_VIndices, ioOther._VIndices);
		std::swap(_NIndices, ioOther._NIndices);
		std::swap(_MIndices, ioOther._MIndices); // Material Indices
		std::swap(_TIndices, ioOther._TIndices);

		std::swap(_VISize, ioOther._VISize);
		std::swap(_NISize, ioOther._NISize);
		std::swap(_MISize, ioOther._MISize);
		std::swap(_TISize, ioOther._TISize);

		std::swap(_displayList, ioOther._displayList);

		Rep::swap(ioOther);
	}

	IndexedFaceSet& operator=(const IndexedFaceSet& iBrother)
	{
		IndexedFaceSet tmp(iBrother);
		swap(tmp);
		return *this;
	}

	/*! Desctructor
	 *  desallocates all the ressources
	 */
	virtual ~IndexedFaceSet();

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v);

	/*! Compute the Bounding Box */
	virtual void ComputeBBox();

	/*! modifiers */
	inline void setDisplayList(unsigned int index)
	{
		_displayList = index;
	}

	/*! Accessors */
	virtual const float *vertices() const
	{
		return _Vertices;
	}

	virtual const float *normals() const
	{
		return _Normals;
	}

	virtual const FrsMaterial*const* frs_materials() const
	{
		return _FrsMaterials;
	}

	virtual const float *texCoords() const
	{
		return _TexCoords;
	}

	virtual const unsigned vsize() const
	{
		return _VSize;
	}

	virtual const unsigned nsize() const
	{
		return _NSize;
	}

	virtual const unsigned msize() const
	{
		return _MSize;
	}

	virtual const unsigned tsize() const
	{
		return _TSize;
	}

	virtual const unsigned numFaces() const
	{
		return _NumFaces;
	}

	virtual const unsigned *numVertexPerFaces() const
	{
		return _NumVertexPerFace;
	}

	virtual const TRIANGLES_STYLE *trianglesStyle() const
	{
		return _FaceStyle;
	}

	virtual const unsigned char *faceEdgeMarks() const
	{
		return _FaceEdgeMarks;
	}

	virtual const unsigned *vindices() const
	{
		return _VIndices;
	}

	virtual const unsigned *nindices() const
	{
		return _NIndices;
	}

	virtual const unsigned *mindices() const
	{
		return _MIndices;
	}

	virtual const unsigned *tindices() const
	{
		return _TIndices;
	}

	virtual const unsigned visize() const
	{
		return _VISize;
	}

	virtual const unsigned nisize() const
	{
		return _NISize;
	}

	virtual const unsigned misize() const
	{
		return _MISize;
	}

	virtual const unsigned tisize() const
	{
		return _TISize;
	}

	inline unsigned int displayList() const
	{
		return _displayList;
	}

protected:
	float *_Vertices;
	float *_Normals;
	FrsMaterial **_FrsMaterials;
	float *_TexCoords;

	unsigned _VSize;
	unsigned _NSize;
	unsigned _MSize;
	unsigned _TSize;

	unsigned _NumFaces;
	unsigned *_NumVertexPerFace;
	TRIANGLES_STYLE *_FaceStyle;
	FaceEdgeMark *_FaceEdgeMarks;

	unsigned *_VIndices;
	unsigned *_NIndices;
	unsigned *_MIndices; // Material Indices
	unsigned *_TIndices; // Texture coordinates Indices

	unsigned _VISize;
	unsigned _NISize;
	unsigned _MISize;
	unsigned _TISize;

	unsigned int _displayList;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:IndexedFaceSet")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_INDEXED_FACE_SET_H__
