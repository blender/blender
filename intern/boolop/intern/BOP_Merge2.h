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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_Merge2.h
 *  \ingroup boolopintern
 */

 
#ifndef __BOP_MERGE2_H__
#define __BOP_MERGE2_H__

#include "BOP_Misc.h"

#ifdef BOP_NEW_MERGE

#include "BOP_Mesh.h"
#include "BOP_Tag.h"
#include "BOP_MathUtils.h"
#include "MEM_SmartPtr.h"

typedef std::vector< BOP_Faces > BOP_LFaces;
typedef std::vector< BOP_Faces >::iterator BOP_IT_LFaces;

class BOP_Merge2 {
	private:
		BOP_Mesh* m_mesh;
		BOP_Index m_firstVertex;
		static BOP_Merge2 SINGLETON;

		BOP_Merge2() {};
		bool mergeFaces();
		bool mergeFaces(BOP_Indexs &mergeVertices);
		bool mergeFaces(BOP_Faces &oldFaces, BOP_Faces &newFaces, BOP_Indexs &vertices, BOP_Index v);
		bool mergeFaces(BOP_Faces &faces, BOP_Faces &oldFaces, BOP_Faces &newFaces, BOP_Indexs &vertices, BOP_Index v);
		BOP_Face *mergeFaces(BOP_Face *faceI, BOP_Face *faceJ, BOP_Index v);
		BOP_Face *mergeFaces(BOP_Face *faceI, BOP_Face *faceJ, BOP_Indexs &pending, BOP_Index v);
		BOP_Face *mergeFaces(BOP_Face3 *faceI, BOP_Face3 *faceJ, BOP_Index v);
		BOP_Face *mergeFaces(BOP_Face4 *faceI, BOP_Face3 *faceJ, BOP_Index v);
		BOP_Face *mergeFaces(BOP_Face4 *faceI, BOP_Face3 *faceJ, BOP_Indexs &pending, BOP_Index v);
		BOP_Face *mergeFaces(BOP_Face4 *faceI, BOP_Face4 *faceJ, BOP_Indexs &pending, BOP_Index v);
		bool createQuads();
		bool containsIndex(BOP_Indexs indexs, BOP_Index index);
		void getFaces(BOP_LFaces &facesByOriginalFace, BOP_Index v);
		void getFaces(BOP_LFaces &facesByOriginalFace, BOP_Indexs vertices, BOP_Index v);
		BOP_Face *createQuad(BOP_Face3 *faceI, BOP_Face3 *faceJ);
		BOP_Face *createQuad(BOP_Face3 *faceI, BOP_Face4 *faceJ);
		BOP_Face *createQuad(BOP_Face4 *faceI, BOP_Face4 *faceJ);

		bool mergeVertex(BOP_Face *faceI, BOP_Face *faceJ, BOP_Index v,
				BOP_Indexs &mergeVertices);
		bool mergeVertex(BOP_Face *faceI, BOP_Face *faceJ, BOP_Index v,
				BOP_Indexs &pending, BOP_Faces &oldFaces, BOP_Faces &newFaces );
		BOP_Face *find3Neighbor(BOP_Face *faceI, BOP_Face *faceJ,
				BOP_Index X, BOP_Index I, BOP_Index P, BOP_Index N );
		BOP_Face *find4Neighbor(BOP_Face *faceI, BOP_Face *faceJ,
				BOP_Index X, BOP_Index I, BOP_Index P, BOP_Index N,
				BOP_Face **faceL, BOP_Index &O);
		BOP_Face3 *collapse(BOP_Face4 *faceC, BOP_Index X);
		void mergeFaces(BOP_Face *A, BOP_Face *B, BOP_Index X,
			BOP_Index I, BOP_Index N, BOP_Index P, BOP_Faces &newFaces );
		void freeVerts(BOP_Index v, BOP_Vertex *vert);

		void mergeVertex(BOP_Faces&, BOP_Index, BOP_Index);
		void mergeVertex(BOP_Face3 *, BOP_Index, BOP_Index);
		void mergeVertex(BOP_Face4 *, BOP_Index, BOP_Index);
		void cleanup( void );

	public:

		static BOP_Merge2 &getInstance() {
			return SINGLETON;
		}

		void mergeFaces(BOP_Mesh *m, BOP_Index v);
};

void dumpmesh(BOP_Mesh *, bool);

#endif	/* BOP_NEW_MERGE2 */
#endif
