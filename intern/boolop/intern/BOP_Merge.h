/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
 
#ifndef BOP_MERGE_H
#define BOP_MERGE_H

#include "BOP_Misc.h"

#ifdef BOP_ORIG_MERGE
#include "BOP_Mesh.h"
#include "BOP_Tag.h"
#include "BOP_MathUtils.h"
#include "MEM_SmartPtr.h"

typedef vector< BOP_Faces > BOP_LFaces;
typedef vector< BOP_Faces >::iterator BOP_IT_LFaces;

class BOP_Merge {
	private:
		BOP_Mesh* m_mesh;
		BOP_Index m_firstVertex;
		static BOP_Merge SINGLETON;

		BOP_Merge() {};
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
		BOP_Face *createQuad(BOP_Face3 *faceI, BOP_Face3 *faceJ, BOP_Index v);
		bool containsIndex(BOP_Indexs indexs, BOP_Index index);
		void getFaces(BOP_LFaces &facesByOriginalFace, BOP_Index v);
		void getFaces(BOP_LFaces &facesByOriginalFace, BOP_Indexs vertices, BOP_Index v);

	public:

		static BOP_Merge &getInstance() {
			return SINGLETON;
		}

		void mergeFaces(BOP_Mesh *m, BOP_Index v);
};

#endif	/* BOP_ORIG_MERGE */

#endif
