/**
 * $$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BKE_DERIVEDMESH_H
#define BKE_DERIVEDMESH_H

struct Object;
struct DispListMesh;

typedef struct DerivedMesh DerivedMesh;
struct DerivedMesh {
	/* Misc. Queries */

	int (*getNumVerts)(DerivedMesh *dm);
	int (*getNumFaces)(DerivedMesh *dm);

			/* Convert to new DispListMesh, should be free'd by caller */
	struct DispListMesh* (*convertToDispListMesh)(DerivedMesh *dm);

	/* Drawing Operations */

			/* Draw all vertices as bgl points (no options) */
	void (*drawVerts)(DerivedMesh *dm);

			/* Draw all edges as lines (no options) */
	void (*drawEdges)(DerivedMesh *dm);

			/* Draw mapped edges as lines (no options) */
	void (*drawMappedEdges)(DerivedMesh *dm);
			
			/* Draw all edges without faces as lines (no options) */
	void (*drawLooseEdges)(DerivedMesh *dm);

			
			/* Draw all faces
			 *  o Set face normal or vertex normal based on inherited face flag
			 *  o Use inherited face material index to call setMaterial
			 */
	void (*drawFacesSolid)(DerivedMesh *dm, void (*setMaterial)(int));

			/* Draw all faces
			 *  o If useTwoSided, draw front and back using col arrays
			 *  o col1,col2 are arrays of length numFace*4 of 4 component colors
			 *    in ABGR format, and should be passed as per-face vertex color.
			 */
	void (*drawFacesColored)(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2);

			/* Draw mapped vertices as bgl points
			 *  o Only if mapped EditVert->h==0
			 */
	void (*drawMappedVertsEM)(DerivedMesh *dm, int sel);

			/* Draw single mapped edge as lines (no options) */
	void (*drawMappedEdgeEM)(DerivedMesh *dm, void *edge);

			/* Draw mapped edges as lines
			 *  o If useColor==0, don't set color
			 *  o If useColor==1, set color based on mapped (EditEdge->f&SELECT)
			 *  o If useColor==2, set color based on mapped (EditVert->f&SELECT)
			 *     - Should interpolate as nicely as possible across edge.
			 *  o If onlySeams, only draw if mapped (EditEdge->seam)
			 */
	void (*drawMappedEdgesEM)(DerivedMesh *dm, int useColor, unsigned char *baseCol, unsigned char *selCol, int onlySeams);

			/* Draw all faces
			 *  o If useColor, set color based on mapped (EditFace->f&SELECT)
			 */
	void (*drawFacesEM)(DerivedMesh *dm, int useColor, unsigned char *baseCol, unsigned char *selCol);

	void (*release)(DerivedMesh *dm);
};

DerivedMesh *mesh_get_derived(struct Object *ob);
DerivedMesh *mesh_get_base_derived(struct Object *ob);

#endif

