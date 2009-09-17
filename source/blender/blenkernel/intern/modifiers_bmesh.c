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

#include "BLI_arithb.h"
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
#include "BKE_simple_deform.h"

#include "CCGSubSurf.h"
#include "RE_shader_ext.h"
#include "LOD_decimation.h"

/*converts a cddm to a BMEditMesh.  if existing is non-NULL, the
  new geometry will be put in there.*/
BMEditMesh *CDDM_To_BMesh(DerivedMesh *dm, BMEditMesh *existing)
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
	else bm = BM_Make_Mesh(allocsize);

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
		
		v->bweight = mv->bweight;
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

		e->bweight = me->bweight;
		e->crease = me->crease;
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

static float vertarray_size(MVert *mvert, int numVerts, int axis)
{
	int i;
	float min_co, max_co;

	/* if there are no vertices, width is 0 */
	if(numVerts == 0) return 0;

	/* find the minimum and maximum coordinates on the desired axis */
	min_co = max_co = mvert->co[axis];
	++mvert;
	for(i = 1; i < numVerts; ++i, ++mvert) {
		if(mvert->co[axis] < min_co) min_co = mvert->co[axis];
		if(mvert->co[axis] > max_co) max_co = mvert->co[axis];
	}

	return max_co - min_co;
}

/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions */
static void vertgroup_flip_name (char *name, int strip_number)
{
	int     len;
	char    prefix[128]={""};   /* The part before the facing */
	char    suffix[128]={""};   /* The part after the facing */
	char    replace[128]={""};  /* The replacement string */
	char    number[128]={""};   /* The number extension string */
	char    *index=NULL;

	len= strlen(name);
	if(len<3) return; // we don't do names like .R or .L

	/* We first check the case with a .### extension, let's find the last period */
	if(isdigit(name[len-1])) {
		index= strrchr(name, '.'); // last occurrance
		if (index && isdigit(index[1]) ) { // doesnt handle case bone.1abc2 correct..., whatever!
			if(strip_number==0) 
				strcpy(number, index);
			*index= 0;
			len= strlen(name);
		}
	}

	strcpy (prefix, name);

#define IS_SEPARATOR(a) ((a)=='.' || (a)==' ' || (a)=='-' || (a)=='_')

	/* first case; separator . - _ with extensions r R l L  */
	if( IS_SEPARATOR(name[len-2]) ) {
		switch(name[len-1]) {
			case 'l':
				prefix[len-1]= 0;
				strcpy(replace, "r");
				break;
			case 'r':
				prefix[len-1]= 0;
				strcpy(replace, "l");
				break;
			case 'L':
				prefix[len-1]= 0;
				strcpy(replace, "R");
				break;
			case 'R':
				prefix[len-1]= 0;
				strcpy(replace, "L");
				break;
		}
	}
	/* case; beginning with r R l L , with separator after it */
	else if( IS_SEPARATOR(name[1]) ) {
		switch(name[0]) {
			case 'l':
				strcpy(replace, "r");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'r':
				strcpy(replace, "l");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'L':
				strcpy(replace, "R");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'R':
				strcpy(replace, "L");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
		}
	}
	else if(len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		index = BLI_strcasestr(prefix, "right");
		if (index==prefix || index==prefix+len-5) {
			if(index[0]=='r') 
				strcpy (replace, "left");
			else {
				if(index[1]=='I') 
					strcpy (replace, "LEFT");
				else
					strcpy (replace, "Left");
			}
			*index= 0;
			strcpy (suffix, index+5);
		}
		else {
			index = BLI_strcasestr(prefix, "left");
			if (index==prefix || index==prefix+len-4) {
				if(index[0]=='l') 
					strcpy (replace, "right");
				else {
					if(index[1]=='E') 
						strcpy (replace, "RIGHT");
					else
						strcpy (replace, "Right");
				}
				*index= 0;
				strcpy (suffix, index+4);
			}
		}
	}

#undef IS_SEPARATOR

	sprintf (name, "%s%s%s%s", prefix, replace, suffix, number);
}

typedef struct IndexMapEntry {
	/* the new vert index that this old vert index maps to */
	int new;
	/* -1 if this vert isn't merged, otherwise the old vert index it
	* should be replaced with
	*/
	int merge;
	/* 1 if this vert's first copy is merged with the last copy of its
	* merge target, otherwise 0
	*/
	short merge_final;
} IndexMapEntry;

/* indexMap - an array of IndexMap entries
 * oldIndex - the old index to map
 * copyNum - the copy number to map to (original = 0, first copy = 1, etc.)
 */
static int calc_mapping(IndexMapEntry *indexMap, int oldIndex, int copyNum)
{
	if(indexMap[oldIndex].merge < 0) {
		/* vert wasn't merged, so use copy of this vert */
		return indexMap[oldIndex].new + copyNum;
	} else if(indexMap[oldIndex].merge == oldIndex) {
		/* vert was merged with itself */
		return indexMap[oldIndex].new;
	} else {
		/* vert was merged with another vert */
		/* follow the chain of merges to the end, or until we've passed
		* a number of vertices equal to the copy number
		*/
		if(copyNum <= 0)
			return indexMap[oldIndex].new;
		else
			return calc_mapping(indexMap, indexMap[oldIndex].merge,
					    copyNum - 1);
	}
}

static DerivedMesh *arrayModifier_doArray(ArrayModifierData *amd,
					  Scene *scene, Object *ob, DerivedMesh *dm,
                                          int initFlags)
{
	DerivedMesh *cddm = dm; //copying shouldn't be necassary here, as all modifiers return CDDM's
	BMEditMesh *em = CDDM_To_BMesh(cddm, NULL);
	BMOperator op, oldop, weldop;
	int i, j, indexLen;
	/* offset matrix */
	float offset[4][4];
	float final_offset[4][4];
	float tmp_mat[4][4];
	float length = amd->length;
	int count = amd->count, maxVerts;
	int finalVerts, finalEdges, finalFaces;
	int *indexMap = NULL;
	DerivedMesh *start_cap = NULL, *end_cap = NULL;
	MVert *src_mvert;

	/* need to avoid infinite recursion here */
	if(amd->start_cap && amd->start_cap != ob)
		start_cap = mesh_get_derived_final(scene, amd->start_cap, CD_MASK_MESH);
	if(amd->end_cap && amd->end_cap != ob)
		end_cap = mesh_get_derived_final(scene, amd->end_cap, CD_MASK_MESH);

	Mat4One(offset);

	src_mvert = cddm->getVertArray(dm);
	maxVerts = cddm->getNumVerts(dm);

	if(amd->offset_type & MOD_ARR_OFF_CONST)
		VecAddf(offset[3], offset[3], amd->offset);
	if(amd->offset_type & MOD_ARR_OFF_RELATIVE) {
		for(j = 0; j < 3; j++)
			offset[3][j] += amd->scale[j] * vertarray_size(src_mvert,
					maxVerts, j);
	}

	if((amd->offset_type & MOD_ARR_OFF_OBJ) && (amd->offset_ob)) {
		float obinv[4][4];
		float result_mat[4][4];

		if(ob)
			Mat4Invert(obinv, ob->obmat);
		else
			Mat4One(obinv);

		Mat4MulSerie(result_mat, offset,
				 obinv, amd->offset_ob->obmat,
                                 NULL, NULL, NULL, NULL, NULL);
		Mat4CpyMat4(offset, result_mat);
	}

	if(amd->fit_type == MOD_ARR_FITCURVE && amd->curve_ob) {
		Curve *cu = amd->curve_ob->data;
		if(cu) {
			float tmp_mat[3][3];
			float scale;
			
			object_to_mat3(amd->curve_ob, tmp_mat);
			scale = Mat3ToScalef(tmp_mat);
				
			if(!cu->path) {
				cu->flag |= CU_PATH; // needed for path & bevlist
				makeDispListCurveTypes(scene, amd->curve_ob, 0);
			}
			if(cu->path)
				length = scale*cu->path->totdist;
		}
	}

	/* calculate the maximum number of copies which will fit within the
	prescribed length */
	if(amd->fit_type == MOD_ARR_FITLENGTH
		  || amd->fit_type == MOD_ARR_FITCURVE)
	{
		float dist = sqrt(INPR(offset[3], offset[3]));

		if(dist > 1e-6f)
			/* this gives length = first copy start to last copy end
			add a tiny offset for floating point rounding errors */
			count = (length + 1e-6f) / dist;
		else
			/* if the offset has no translation, just make one copy */
			count = 1;
	}

	if(count < 1)
		count = 1;

	/* allocate memory for count duplicates (including original) plus
		  * start and end caps
	*/
	finalVerts = dm->getNumVerts(dm) * count;
	finalEdges = dm->getNumEdges(dm) * count;
	finalFaces = dm->getNumFaces(dm) * count;
	if(start_cap) {
		finalVerts += start_cap->getNumVerts(start_cap);
		finalEdges += start_cap->getNumEdges(start_cap);
		finalFaces += start_cap->getNumFaces(start_cap);
	}
	if(end_cap) {
		finalVerts += end_cap->getNumVerts(end_cap);
		finalEdges += end_cap->getNumEdges(end_cap);
		finalFaces += end_cap->getNumFaces(end_cap);
	}

	/* calculate the offset matrix of the final copy (for merging) */ 
	Mat4One(final_offset);

	for(j=0; j < count - 1; j++) {
		Mat4MulMat4(tmp_mat, final_offset, offset);
		Mat4CpyMat4(final_offset, tmp_mat);
	}

	BMO_Init_Op(&weldop, "weldverts");
	BMO_InitOpf(em->bm, &op, "dupe geom=%avef");
	oldop = op;
	for (j=0; j < count; j++) {
		BMVert *v, *v2;
		BMOpSlot *s1;
		BMOpSlot *s2;

		BMO_InitOpf(em->bm, &op, "dupe geom=%s", &oldop, j==0 ? "geom" : "newout");
		BMO_Exec_Op(em->bm, &op);

		s1 = BMO_GetSlot(&op, "geom");
		s2 = BMO_GetSlot(&op, "newout");

		BMO_CallOpf(em->bm, "transform mat=%m4 verts=%s", offset, &op, "newout");

		#define _E(s, i) ((BMVert**)(s)->data.buf)[i]

		/*calculate merge mapping*/
		if (j == 0) {
			BMOperator findop;
			BMOIter oiter;
			BMVert *v, *v2;
			BMHeader *h;

			BMO_InitOpf(em->bm, &findop, 
				"finddoubles verts=%av dist=%f keepverts=%s", 
				amd->merge_dist, &op, "geom");

			i = 0;
			BMO_ITER(h, &oiter, em->bm, &op, "geom", BM_ALL) {
				BMINDEX_SET(h, i);
				i++;
			}

			BMO_ITER(h, &oiter, em->bm, &op, "newout", BM_ALL) {
				BMINDEX_SET(h, i);
				i++;
			}

			BMO_Exec_Op(em->bm, &findop);

			indexLen = i;
			indexMap = MEM_callocN(sizeof(int)*indexLen, "indexMap");

			/*element type argument doesn't do anything here*/
			BMO_ITER(v, &oiter, em->bm, &findop, "targetmapout", 0) {
				v2 = BMO_IterMapValp(&oiter);

				indexMap[BMINDEX_GET(v)] = BMINDEX_GET(v2)+1;
			}

			BMO_Finish_Op(em->bm, &findop);
		} 

		/*generate merge mappping using index map.  we do this by using the
		  operator slots as lookup arrays.*/
		#define E(i) (i) < s1->len ? _E(s1, i) : _E(s2, (i)-s1->len)

		for (i=0; i<indexLen; i++) {
			if (!indexMap[i]) continue;

			v = E(i);
			v2 = E(indexMap[i]-1);

			BMO_Insert_MapPointer(em->bm, &weldop, "targetmap", v, v2);
		}

		#undef E
		#undef _E

		BMO_Finish_Op(em->bm, &oldop);
		oldop = op;
	}

	if (j > 0) BMO_Finish_Op(em->bm, &op);

	if (amd->flags & MOD_ARR_MERGE)
		BMO_Exec_Op(em->bm, &weldop);

	BMO_Finish_Op(em->bm, &weldop);

	BMEdit_RecalcTesselation(em);
	cddm = CDDM_from_BMEditMesh(em, NULL);

	BMEdit_Free(em);
	MEM_freeN(indexMap);

	return cddm;
}

DerivedMesh *arrayModifier_applyModifier(ModifierData *md, Object *ob, 
					 DerivedMesh *derivedData,
                                         int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	ArrayModifierData *amd = (ArrayModifierData*) md;

	result = arrayModifier_doArray(amd, md->scene, ob, derivedData, 0);

	//if(result != derivedData)
	//	CDDM_calc_normals(result);

	return result;
}

DerivedMesh *arrayModifier_applyModifierEM(ModifierData *md, Object *ob,
                                           BMEditMesh *editData, 
                                           DerivedMesh *derivedData)
{
	return arrayModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Mirror */
#define VERT_NEW	1

DerivedMesh *doMirrorOnAxis(MirrorModifierData *mmd,
		Object *ob,
		DerivedMesh *dm,
		int initFlags,
		int axis)
{
	float tolerance = mmd->tolerance;
	DerivedMesh *result, *cddm;
	BMEditMesh *em;
	BMesh *bm;
	BMOIter siter1;
	BMOperator op;
	BMVert *v1;
	int vector_size=0, a, b;
	bDeformGroup *def, *defb;
	bDeformGroup **vector_def = NULL;
	float mtx[4][4], imtx[4][4];
	int j;

	cddm = dm; //copying shouldn't be necassary here, as all modifiers return CDDM's
	em = CDDM_To_BMesh(dm, NULL);

	/*convienence variable*/
	bm = em->bm;

	if (mmd->flag & MOD_MIR_VGROUP) {
		/* calculate the number of deformedGroups */
		for(vector_size = 0, def = ob->defbase.first; def;
		    def = def->next, vector_size++);

		/* load the deformedGroups for fast access */
		vector_def =
		    (bDeformGroup **)MEM_mallocN(sizeof(bDeformGroup*) * vector_size,
		                                 "group_index");
		for(a = 0, def = ob->defbase.first; def; def = def->next, a++) {
			vector_def[a] = def;
		}
	}

	if (mmd->mirror_ob) {
		float mtx2[4][4], vec[3];
		
		Mat4Invert(mtx2, mmd->mirror_ob->obmat);
		Mat4MulMat4(mtx, ob->obmat, mtx2);
	} else {
		Mat4One(mtx);
	}

	BMO_InitOpf(bm, &op, "mirror geom=%avef mat=%m4 mergedist=%f axis=%d", 
	            mtx, mmd->tolerance, axis);
	
	BMO_Exec_Op(bm, &op);

	BMO_CallOpf(bm, "reversefaces faces=%s", &op, "newout");
	
	/*handle vgroup stuff*/
	if (mmd->flag & MOD_MIR_VGROUP) {
		BMO_ITER(v1, &siter1, bm, &op, "newout", BM_VERT) {
			MDeformVert *dvert = CustomData_bmesh_get(&bm->vdata, v1->head.data, CD_MDEFORMVERT);
			
			if (dvert) {
				for(j = 0; j < dvert[0].totweight; ++j) {
					char tmpname[32];
					
					if(dvert->dw[j].def_nr < 0 ||
					   dvert->dw[j].def_nr >= vector_size)
						continue;
					
					def = vector_def[dvert->dw[j].def_nr];
					strcpy(tmpname, def->name);
					vertgroup_flip_name(tmpname,0);
					
					for(b = 0, defb = ob->defbase.first; defb;
					    defb = defb->next, b++)
					{
						if(!strcmp(defb->name, tmpname))
						{
							dvert->dw[j].def_nr = b;
							break;
						}
					}
				}
			}
		}
	}
	
	BMO_Finish_Op(bm, &op);

	BMEdit_RecalcTesselation(em);
	result = CDDM_from_BMEditMesh(em, NULL); //CDDM_copy(getEditDerivedBMesh(em, ob, NULL), 0);

	BMEdit_Free(em);
	MEM_freeN(em);

	if (vector_def) MEM_freeN(vector_def);

	return result;
}
