/**
 * $Id$
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

/* TODO (Probably)
 *
 *  o Make drawMapped* functions take a predicate function that
 *    determines whether to draw the edge (this predicate can
 *    also set color, etc). This will be slightly more general 
 *    and allow some of the functions to be collapsed.
 *  o Once accessor functions are added then single element draw
 *    functions can be implemented using primitive accessors.
 *  o Add function to dispatch to renderer instead of using
 *    conversion to DLM.
 */

struct Object;
struct EditMesh;
struct EditVert;
struct EditEdge;
struct EditFace;
struct DispListMesh;

typedef struct DerivedMesh DerivedMesh;
struct DerivedMesh {
	/* Misc. Queries */

	int (*getNumVerts)(DerivedMesh *dm);
	int (*getNumFaces)(DerivedMesh *dm);

	void (*getMappedVertCoEM)(DerivedMesh *dm, void *vert, float co_r[3]);

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
			 *  o Only if setMaterial returns true
			 */
	void (*drawFacesSolid)(DerivedMesh *dm, int (*setMaterial)(int));

			/* Draw all faces
			 *  o If useTwoSided, draw front and back using col arrays
			 *  o col1,col2 are arrays of length numFace*4 of 4 component colors
			 *    in ABGR format, and should be passed as per-face vertex color.
			 */
	void (*drawFacesColored)(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2);

			/* Draw all faces uses TFace 
			 *  o Drawing options too complicated to enumerate, look at code.
			 */
	void (*drawFacesTex)(DerivedMesh *dm, int (*setDrawParams)(TFace *tf, int matnr));

			/* Draw mapped vertices as bgl points
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-vert) returns true
			 */
	void (*drawMappedVertsEM)(DerivedMesh *dm, int (*setDrawOptions)(void *userData, struct EditVert *eve), void *userData);

			/* Draw single mapped edge as lines (no options) */
	void (*drawMappedEdgeEM)(DerivedMesh *dm, void *edge);

			/* Draw mapped edges as lines
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-edge) returns true
			 */
	void (*drawMappedEdgesEM)(DerivedMesh *dm, int (*setDrawOptions)(void *userData, struct EditEdge *eed), void *userData);

			/* Draw mapped edges as lines with interpolation values
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-edge, mapped-v0, mapped-v1, t) returns true
			 *
			 * NOTE: This routine is optional!
			 */
	void (*drawMappedEdgesInterpEM)(DerivedMesh *dm, 
									int (*setDrawOptions)(void *userData, struct EditEdge *eed), 
									void (*setDrawInterpOptions)(void *userData, struct EditEdge *eed, float t),
									void *userData);

			/* Draw all faces
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-face) returns true
			 */
	void (*drawMappedFacesEM)(DerivedMesh *dm, int (*setDrawOptions)(void *userData, struct EditFace *efa), void *userData);

	void (*release)(DerivedMesh *dm);
};

	/* Internal function, just temporarily exposed */
DerivedMesh *derivedmesh_from_displistmesh(struct DispListMesh *dlm);

DerivedMesh *mesh_get_derived(struct Object *ob);
DerivedMesh *mesh_get_derived_final(struct Object *ob, int *needsFree_r);
DerivedMesh *mesh_get_derived_render(struct Object *ob, int *needsFree_r);

	/* IMPORTANT: The functions below do not return "true" DerivedMesh
	 * objects, rather they are just proxies for the mesh or editmesh
	 * objects and are used to keep the drawing code consistent. They
	 * should not be used as general purpose objects (access the Mesh
	 * or EditMesh directly).
	 */

	/* Utility function, just chooses appropriate DerivedMesh based
	 * on mesh flags. Release result if *needsFree_r is true.
	 */
DerivedMesh *mesh_get_cage_derived(struct Object *ob, int *needsFree_r);
DerivedMesh *mesh_get_base_derived(struct Object *ob);

#endif

