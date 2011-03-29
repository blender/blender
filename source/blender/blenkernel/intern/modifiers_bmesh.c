/*
* $Id: modifier_bmesh.c 20831 2009-06-12 14:02:37Z joeedh $
*
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Joseph Eagar
*
* ***** END GPL LICENSE BLOCK *****
*
* Modifier stack implementation.
*
* BKE_modifier.h contains the function prototypes for this file.
*
*/

#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"
#include "ctype.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"
#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_edgehash.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_cellalloc.h"

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_editVert.h"
#include "BLI_array.h"

#include "BKE_main.h"
#include "BKE_anim.h"
#include "BKE_bmesh.h"
// XXX #include "BKE_booleanops.h"
#include "BKE_cloth.h"
#include "BKE_collision.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "CCGSubSurf.h"
#include "RE_shader_ext.h"
#include "LOD_decimation.h"

/*converts a cddm to a BMEditMesh.  if existing is non-NULL, the
  new geometry will be put in there.*/
BMEditMesh *CDDM_To_BMesh(Object *ob, DerivedMesh *dm, BMEditMesh *existing)
{
	int allocsize[4] = {512, 512, 2048, 512};
	BMesh *bm, bmold; /*bmold is for storing old customdata layout*/
	BMEditMesh *em = existing;
	MVert *mv, *mvert;
	MEdge *me, *medge;
	DMFaceIter *dfiter;
	DMLoopIter *dliter;
	BMVert *v, **vtable, **verts=NULL;
	BMEdge *e, **etable, **edges=NULL;
	BMFace *f;
	BMIter liter;
	BLI_array_declare(verts);
	BLI_array_declare(edges);
	int numTex, numCol;
	int i, j, k, totvert, totedge, totface;
	
	if (em) bm = em->bm;
	else bm = BM_Make_Mesh(ob, allocsize);

	bmold = *bm;

	/*merge custom data layout*/
	CustomData_bmesh_merge(&dm->vertData, &bm->vdata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_VERT);
	CustomData_bmesh_merge(&dm->edgeData, &bm->edata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_EDGE);
	CustomData_bmesh_merge(&dm->loopData, &bm->ldata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_LOOP);
	CustomData_bmesh_merge(&dm->polyData, &bm->pdata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_FACE);

	/*needed later*/
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumFaces(dm);

	vtable = MEM_callocN(sizeof(void**)*totvert, "vert table in BMDM_Copy");
	etable = MEM_callocN(sizeof(void**)*totedge, "edge table in BMDM_Copy");

	/*do verts*/
	mv = mvert = dm->dupVertArray(dm);
	for (i=0; i<totvert; i++, mv++) {
		v = BM_Make_Vert(bm, mv->co, NULL);
		
		VECCOPY(v->no, mv->no);
		v->head.flag = MEFlags_To_BMFlags(mv->flag, BM_VERT);

		CustomData_to_bmesh_block(&dm->vertData, &bm->vdata, i, &v->head.data);
		vtable[i] = v;
	}
	MEM_freeN(mvert);

	/*do edges*/
	me = medge = dm->dupEdgeArray(dm);
	for (i=0; i<totedge; i++, me++) {
		e = BM_Make_Edge(bm, vtable[me->v1], vtable[me->v2], NULL, 0);

		e->head.flag = MEFlags_To_BMFlags(me->flag, BM_EDGE);

		CustomData_to_bmesh_block(&dm->edgeData, &bm->edata, i, &e->head.data);
		etable[i] = e;
	}
	MEM_freeN(medge);
	
	/*do faces*/
	k = 0;
	dfiter = dm->newFaceIter(dm);
	for (; !dfiter->done; dfiter->step(dfiter)) {
		BMLoop *l;

		BLI_array_empty(verts);
		BLI_array_empty(edges);

		dliter = dfiter->getLoopsIter(dfiter);
		for (j=0; !dliter->done; dliter->step(dliter), j++) {
			BLI_array_growone(verts);
			BLI_array_growone(edges);

			verts[j] = vtable[dliter->vindex];
			edges[j] = etable[dliter->eindex];
		}

		if (j < 2)
			break;
		
		f = BM_Make_Ngon(bm, verts[0], verts[1], edges, dfiter->len, 0);

		if (!f) 
			continue;

		f->head.flag = MEFlags_To_BMFlags(dfiter->flags, BM_FACE);
		f->mat_nr = dfiter->mat_nr;

		dliter = dfiter->getLoopsIter(dfiter);
		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		for (j=0; l; l=BMIter_Step(&liter)) {
			CustomData_to_bmesh_block(&dm->loopData, &bm->ldata, k, &l->head.data);
			k += 1;
		}

		CustomData_to_bmesh_block(&dm->polyData, &bm->pdata, 
			dfiter->index, &f->head.data);
	}
	dfiter->free(dfiter);

	MEM_freeN(vtable);
	MEM_freeN(etable);
	
	BLI_array_free(verts);
	BLI_array_free(edges);

	if (!em) em = BMEdit_Create(bm);
	else BMEdit_RecalcTesselation(em);

	return em;
}

