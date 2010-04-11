/*
* $Id$
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
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Daniel Dunbar
*                 Ton Roosendaal,
*                 Ben Batt,
*                 Brecht Van Lommel,
*                 Campbell Barton
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"

/* Screw modifier: revolves the edges about an axis
*/

/* used for gathering edge connectivity */
typedef struct ScrewVertConnect {
	float dist;  /* distance from the center axis */
	float co[3]; /* loaction relative to the transformed axis */
	float no[3]; /* calc normal of the vertex */
	int v[2]; /* 2  verts on either side of this one */
	MEdge *e[2]; /* edges on either side, a bit of a waste since each edge ref's 2 edges */
	char flag;
} ScrewVertConnect;

typedef struct ScrewVertIter {
	ScrewVertConnect * v_array;
	ScrewVertConnect * v_poin;
	int v;
	int v_other;
	MEdge *e;
} ScrewVertIter;

#define ScrewVertIter_INIT(iter, array, v_init, dir)\
	iter.v_array = array;\
	iter.v = v_init;\
	if (v_init>=0) {\
		iter.v_poin = &array[v_init];\
		iter.v_other = iter.v_poin->v[dir];\
		if (dir)\
			iter.e = iter.v_poin->e[0];\
		else\
			iter.e = iter.v_poin->e[1];\
	} else {\
		iter.v_poin= NULL;\
		iter.e= NULL;\
	}


#define ScrewVertIter_NEXT(iter)\
	if (iter.v_poin->v[0] == iter.v_other) {\
		iter.v_other= iter.v;\
		iter.v= iter.v_poin->v[1];\
	} else if (iter.v_poin->v[1] == iter.v_other) {\
		iter.v_other= iter.v;\
		iter.v= iter.v_poin->v[0];\
	}\
	if (iter.v >=0)	{\
		iter.v_poin= &iter.v_array[iter.v];\
		if ( iter.v_poin->e[0] != iter.e )	iter.e= iter.v_poin->e[0];\
		else								iter.e= iter.v_poin->e[1];\
	} else {\
		iter.e= NULL;\
		iter.v_poin= NULL;\
	}
	
static void initData(ModifierData *md)
{
	ScrewModifierData *ltmd= (ScrewModifierData*) md;
	ltmd->ob_axis= NULL;
	ltmd->angle= M_PI * 2.0;
	ltmd->axis= 2;
	ltmd->flag= 0;
	ltmd->steps= 16;
	ltmd->render_steps= 16;
	ltmd->iter= 1;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	ScrewModifierData *sltmd= (ScrewModifierData*) md;
	ScrewModifierData *tltmd= (ScrewModifierData*) target;
	
	tltmd->ob_axis= sltmd->ob_axis;
	tltmd->angle= sltmd->angle;
	tltmd->axis= sltmd->axis;
	tltmd->flag= sltmd->flag;
	tltmd->steps= sltmd->steps;
	tltmd->render_steps= sltmd->render_steps;
	tltmd->screw_ofs= sltmd->screw_ofs;
	tltmd->iter= sltmd->iter;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
										DerivedMesh *derivedData,
										int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= derivedData;
	DerivedMesh *result;
	ScrewModifierData *ltmd= (ScrewModifierData*) md;
	
	int *origindex;
	int mface_index=0;
	int i, j;
	int i1,i2;
	int steps= ltmd->steps;
	int maxVerts=0, maxEdges=0, maxFaces=0;
	int totvert= dm->getNumVerts(dm);
	int totedge= dm->getNumEdges(dm);

	char axis_char, close;
	float angle= ltmd->angle;
	float screw_ofs= ltmd->screw_ofs;
	float axis_vec[3]= {0.0f, 0.0f, 0.0f};
	float tmp_vec1[3], tmp_vec2[3]; 
	float mat3[3][3];
	float mtx_tx[4][4]; /* transform the coords by an object relative to this objects transformation */
	float mtx_tx_inv[4][4]; /* inverted */
	float mtx_tmp_a[4][4];
	
	int vc_tot_linked= 0;
	short other_axis_1, other_axis_2;
	float *tmpf1, *tmpf2;
	
	MFace *mface_new, *mf_new;
	MEdge *medge_orig, *med_orig, *med_new, *med_new_firstloop, *medge_new;
	MVert *mvert_new, *mvert_orig, *mv_orig, *mv_new, *mv_new_base;

	ScrewVertConnect *vc, *vc_tmp, *vert_connect= NULL;

	float mat[4][4] =	{{0.0f, 0.0f, 0.0f, 0.0f},
						 {0.0f, 0.0f, 0.0f, 0.0f},
						 {0.0f, 0.0f, 0.0f, 0.0f},
						 {0.0f, 0.0f, 0.0f, 1.0f}};

	/* dont do anything? */
	if (!totvert)
		return CDDM_from_template(dm, 0, 0, 0);

	steps= useRenderParams ? ltmd->render_steps : ltmd->steps;

	switch(ltmd->axis) {
	case 0:
		other_axis_1=1;
		other_axis_2=2;
		break;
	case 1:
		other_axis_1=0;
		other_axis_2=2;
		break;
	case 2:
		other_axis_1=0;
		other_axis_2=1;
		break;
	}

	axis_vec[ltmd->axis]= 1.0f;

	if (ltmd->ob_axis) {
		float mtx3_tx[3][3];
		/* calc the matrix relative to the axis object */
		invert_m4_m4(mtx_tmp_a, ob->obmat);
		copy_m4_m4(mtx_tx_inv, ltmd->ob_axis->obmat);
		mul_m4_m4m4(mtx_tx, mtx_tx_inv, mtx_tmp_a);

		copy_m3_m4(mtx3_tx, mtx_tx);

		/* calc the axis vec */
		mul_m3_v3(mtx3_tx, axis_vec);
		normalize_v3(axis_vec);

		/* screw */
		if(ltmd->flag & MOD_SCREW_OBJECT_OFFSET) {
			/* find the offset along this axis relative to this objects matrix */
			float totlen = len_v3(mtx_tx[3]);

			if(totlen != 0.0f) {
				float zero[3]={0.0f, 0.0f, 0.0f};
				float cp[3];				
				screw_ofs= closest_to_line_v3(cp, mtx_tx[3], zero, axis_vec);
			}
			else {
				screw_ofs= 0.0f;
			}
		}

		/* angle */

#if 0	// cant incluide this, not pradictable enough, though quite fun,.
		if(ltmd->flag & MOD_SCREW_OBJECT_ANGLE) {


			float vec[3] = {0,1,0};
			float cross1[3];
			float cross2[3];
			cross_v3_v3v3(cross1, vec, axis_vec);

			mul_v3_m3v3(cross2, mtx3_tx, cross1);
			{
				float c1[3];
				float c2[3];
				float axis_tmp[3];

				cross_v3_v3v3(c1, cross2, axis_vec);
				cross_v3_v3v3(c2, axis_vec, c1);


				angle= angle_v3v3(cross1, c2);

				cross_v3_v3v3(axis_tmp, cross1, c2);
				normalize_v3(axis_tmp);

				if(len_v3v3(axis_tmp, axis_vec) > 1.0f)
					angle= -angle;

			}
		}
#endif
	}
	else {
		/* exis char is used by i_rotate*/
		axis_char= 'X' + ltmd->axis;

		/* useful to be able to use the axis vec in some cases still */
		zero_v3(axis_vec);
		axis_vec[ltmd->axis]= 1.0f;
	}

	/* apply the multiplier */
	angle *= ltmd->iter;
	screw_ofs *= ltmd->iter;

	/* multiplying the steps is a bit tricky, this works best */
	steps = ((steps + 1) * ltmd->iter) - (ltmd->iter - 1);

	/* will the screw be closed?
	 * Note! smaller then FLT_EPSILON*100 gives problems with float precission so its never closed. */
	if (fabs(screw_ofs) <= (FLT_EPSILON*100) && fabs(fabs(angle) - (M_PI * 2)) <= (FLT_EPSILON*100)) {
		close= 1;
		steps--;
		if(steps < 2) steps= 2;
	
		maxVerts =	totvert  * steps; /* -1 because we're joining back up */
		maxEdges =	(totvert * steps) + /* these are the edges between new verts */
					(totedge * steps); /* -1 because vert edges join */
		maxFaces =	totedge * steps;

		screw_ofs= 0.0f;
	}
	else {
		close= 0;
		if(steps < 2) steps= 2;

		maxVerts =	totvert  * steps; /* -1 because we're joining back up */
		maxEdges =	(totvert * (steps-1)) + /* these are the edges between new verts */
					(totedge * steps); /* -1 because vert edges join */
		maxFaces =	totedge * (steps-1);
	}
	
	result= CDDM_from_template(dm, maxVerts, maxEdges, maxFaces);
	
	/* copy verts from mesh */
	mvert_orig =	dm->getVertArray(dm);
	medge_orig =	dm->getEdgeArray(dm);
	
	mvert_new =		result->getVertArray(result);
	mface_new =		result->getFaceArray(result);
	medge_new =		result->getEdgeArray(result);
	
	origindex= result->getFaceDataArray(result, CD_ORIGINDEX);
	
	/* Set the locations of the first set of verts */
	
	mv_new= mvert_new;
	mv_orig= mvert_orig;
	
	/* Copy the first set of edges */
	med_orig= medge_orig;
	med_new= medge_new;
	for (i=0; i < totedge; i++, med_orig++, med_new++) {
		med_new->v1= med_orig->v1;
		med_new->v2= med_orig->v2;
		med_new->crease= med_orig->crease;
		med_new->flag= med_orig->flag &  ~ME_LOOSEEDGE;
	}
	
	if(ltmd->flag & MOD_SCREW_NORMAL_CALC) {
		/*
		 * Normal Calculation (for face flipping)
		 * Sort edge verts for correct face flipping
		 * NOT REALLY NEEDED but face flipping is nice.
		 *
		 * */


		/* Notice!
		 *
		 * Since we are only ordering the edges here it can avoid mallocing the
		 * extra space by abusing the vert array berfore its filled with new verts.
		 * The new array for vert_connect must be at least sizeof(ScrewVertConnect) * totvert
		 * and the size of our resulting meshes array is sizeof(MVert) * totvert * 3
		 * so its safe to use the second 2 thrids of MVert the array for vert_connect,
		 * just make sure ScrewVertConnect struct is no more then twice as big as MVert,
		 * at the moment there is no chance of that being a problem,
		 * unless MVert becomes half its current size.
		 *
		 * once the edges are ordered, vert_connect is not needed and it can be used for verts
		 *
		 * This makes the modifier faster with one less alloc.
		 */

		vert_connect= MEM_mallocN(sizeof(ScrewVertConnect) * totvert, "ScrewVertConnect");
		//vert_connect= (ScrewVertConnect *) &medge_new[totvert]; /* skip the first slice of verts */
		vc= vert_connect;

		/* Copy Vert Locations */
		/* - We can do this in a later loop - only do here if no normal calc */
		if (!totedge) {
			for (i=0; i < totvert; i++, mv_orig++, mv_new++) {
				copy_v3_v3(mv_new->co, mv_orig->co);
				normalize_v3_v3(vc->no, mv_new->co); /* no edges- this is realy a dummy normal */
			}
		}
		else {
			/*printf("\n\n\n\n\nStarting Modifier\n");*/
			/* set edge users */
			med_new= medge_new;
			mv_new= mvert_new;

			if (ltmd->ob_axis) {
				/*mtx_tx is initialized early on */
				for (i=0; i < totvert; i++, mv_new++, mv_orig++, vc++) {
					vc->co[0]= mv_new->co[0]= mv_orig->co[0];
					vc->co[1]= mv_new->co[1]= mv_orig->co[1];
					vc->co[2]= mv_new->co[2]= mv_orig->co[2];

					vc->flag= 0;
					vc->e[0]= vc->e[1]= NULL;
					vc->v[0]= vc->v[1]= -1;

					mul_m4_v3(mtx_tx, vc->co);
					/* length in 2d, dont sqrt because this is only for comparison */
					vc->dist =	vc->co[other_axis_1]*vc->co[other_axis_1] +
								vc->co[other_axis_2]*vc->co[other_axis_2];

					/* printf("location %f %f %f -- %f\n", vc->co[0], vc->co[1], vc->co[2], vc->dist);*/
				}
			}
			else {
				for (i=0; i < totvert; i++, mv_new++, mv_orig++, vc++) {
					vc->co[0]= mv_new->co[0]= mv_orig->co[0];
					vc->co[1]= mv_new->co[1]= mv_orig->co[1];
					vc->co[2]= mv_new->co[2]= mv_orig->co[2];

					vc->flag= 0;
					vc->e[0]= vc->e[1]= NULL;
					vc->v[0]= vc->v[1]= -1;

					/* length in 2d, dont sqrt because this is only for comparison */
					vc->dist =	vc->co[other_axis_1]*vc->co[other_axis_1] +
								vc->co[other_axis_2]*vc->co[other_axis_2];

					/* printf("location %f %f %f -- %f\n", vc->co[0], vc->co[1], vc->co[2], vc->dist);*/
				}
			}

			/* this loop builds connectivity info for verts */
			for (i=0; i<totedge; i++, med_new++) {
				vc= &vert_connect[med_new->v1];

				if (vc->v[0]==-1) { /* unused */
					vc->v[0]= med_new->v2;
					vc->e[0]= med_new;
				}
				else if (vc->v[1]==-1) {
					vc->v[1]= med_new->v2;
					vc->e[1]= med_new;
				}
				else {
					vc->v[0]= vc->v[1]= -2; /* erro value  - dont use, 3 edges on vert */
				}

				vc= &vert_connect[med_new->v2];

				/* same as above but swap v1/2 */
				if (vc->v[0]==-1) { /* unused */
					vc->v[0]= med_new->v1;
					vc->e[0]= med_new;
				}
				else if (vc->v[1]==-1) {
					vc->v[1]= med_new->v1;
					vc->e[1]= med_new;
				}
				else {
					vc->v[0]= vc->v[1]= -2; /* erro value  - dont use, 3 edges on vert */
				}
			}

			/* find the first vert */
			vc= vert_connect;
			for (i=0; i < totvert; i++, vc++) {
				int VBEST=-1, ed_loop_closed=0; /* vert and vert new */
				int ed_loop_flip;
				float fl= -1.0f;
				ScrewVertIter lt_iter;

				/* Now do search for connected verts, order all edges and flip them
				 * so resulting faces are flipped the right way */
				vc_tot_linked= 0; /* count the number of linked verts for this loop */
				if (vc->flag==0) {
					/*printf("Loop on connected vert: %i\n", i);*/

					for(j=0; j<2; j++) {
						/*printf("\tSide: %i\n", j);*/
						ScrewVertIter_INIT(lt_iter, vert_connect, i, j);
						if (j==1) {
							ScrewVertIter_NEXT(lt_iter);
						}
						while (lt_iter.v_poin) {
							/*printf("\t\tVERT: %i\n", lt_iter.v);*/
							if (lt_iter.v_poin->flag) {
								/*printf("\t\t\tBreaking Found end\n");*/
								//endpoints[0]= endpoints[1]= -1;
								ed_loop_closed= 1; /* circle */
								break;
							}
							lt_iter.v_poin->flag= 1;
							vc_tot_linked++;
							/*printf("Testing 2 floats %f : %f\n", fl, lt_iter.v_poin->dist);*/
							if (fl <= lt_iter.v_poin->dist) {
								fl= lt_iter.v_poin->dist;
								VBEST= lt_iter.v;
								/*printf("\t\t\tVERT BEST: %i\n", VBEST);*/
							}
							ScrewVertIter_NEXT(lt_iter);
							if (!lt_iter.v_poin) {
								/*printf("\t\t\tFound End Also Num %i\n", j);*/
								/*endpoints[j]= lt_iter.v_other;*/ /* other is still valid */
								break;
							}
						}
					}

					/* now we have a collection of used edges. flip their edges the right way*/
					/*if (VBEST !=-1) - */

					/*printf("Done Looking - vc_tot_linked: %i\n", vc_tot_linked);*/

					if (vc_tot_linked>1) {
						float vf_1, vf_2, vf_best;

						vc_tmp= &vert_connect[VBEST];

						tmpf1= vert_connect[vc_tmp->v[0]].co;
						tmpf2= vert_connect[vc_tmp->v[1]].co;


						/* edge connects on each side! */
						if ((vc_tmp->v[0] > -1) && (vc_tmp->v[1] > -1)) {
							/*printf("Verts on each side (%i %i)\n", vc_tmp->v[0], vc_tmp->v[1]);*/
							/* find out which is higher */

							vf_1= tmpf1[ltmd->axis];
							vf_2= tmpf2[ltmd->axis];
							vf_best= vc_tmp->co[ltmd->axis];

							if (vf_1 < vf_best && vf_best < vf_2) {
								ed_loop_flip= 0;
							}
							else if (vf_1 > vf_best && vf_best > vf_2) {
								ed_loop_flip= 1;
							}
							else {
								/* not so simple to work out which edge is higher */
								sub_v3_v3v3(tmp_vec1, tmpf1, vc_tmp->co);
								sub_v3_v3v3(tmp_vec1, tmpf2, vc_tmp->co);
								normalize_v3(tmp_vec1);
								normalize_v3(tmp_vec2);

								if (tmp_vec1[ltmd->axis] < tmp_vec2[ltmd->axis]) {
									ed_loop_flip= 1;
								}
								else {
									ed_loop_flip= 0;
								}
							}
						}
						else if (vc_tmp->v[0] >= 0) { /*vertex only connected on 1 side */
							/*printf("Verts on ONE side (%i %i)\n", vc_tmp->v[0], vc_tmp->v[1]);*/
							if (tmpf1[ltmd->axis] < vc_tmp->co[ltmd->axis]) { /* best is above */
								ed_loop_flip= 1;
							}
							else { /* best is below or even... in even case we cant know whet  to do. */
								ed_loop_flip= 0;
							}

						}/* else {
							printf("No Connected ___\n");
						}*/

						/*printf("flip direction %i\n", ed_loop_flip);*/


						/* switch the flip option if set */
						if (ltmd->flag & MOD_SCREW_NORMAL_FLIP)
							ed_loop_flip= !ed_loop_flip;

						if (angle < 0.0f)
							ed_loop_flip= !ed_loop_flip;

						/* if its closed, we only need 1 loop */
						for(j=ed_loop_closed; j<2; j++) {
							/*printf("Ordering Side J %i\n", j);*/

							ScrewVertIter_INIT(lt_iter, vert_connect, VBEST, j);
							/*printf("\n\nStarting - Loop\n");*/
							lt_iter.v_poin->flag= 1; /* so a non loop will traverse the other side */


							/* If this is the vert off the best vert and
							 * the best vert has 2 edges connected too it
							 * then swap the flip direction */
							if (j==1 && (vc_tmp->v[0] > -1) && (vc_tmp->v[1] > -1))
								ed_loop_flip= !ed_loop_flip;

							while (lt_iter.v_poin && lt_iter.v_poin->flag != 2) {
								/*printf("\tOrdering Vert V %i\n", lt_iter.v);*/

								lt_iter.v_poin->flag= 2;
								if (lt_iter.e) {
									if (lt_iter.v == lt_iter.e->v1) {
										if (ed_loop_flip==0) {
											/*printf("\t\t\tFlipping 0\n");*/
											SWAP(int, lt_iter.e->v1, lt_iter.e->v2);
										}/* else {
											printf("\t\t\tFlipping Not 0\n");
										}*/
									}
									else if (lt_iter.v == lt_iter.e->v2) {
										if (ed_loop_flip==1) {
											/*printf("\t\t\tFlipping 1\n");*/
											SWAP(int, lt_iter.e->v1, lt_iter.e->v2);
										}/* else {
											printf("\t\t\tFlipping Not 1\n");
										}*/
									}/* else {
										printf("\t\tIncorrect edge topology");
									}*/
								}/* else {
									printf("\t\tNo Edge at this point\n");
								}*/
								ScrewVertIter_NEXT(lt_iter);
							}
						}
					}
				}

				/* *VERTEX NORMALS*
				 * we know the surrounding edges are ordered correctly now
				 * so its safe to create vertex normals.
				 *
				 * calculate vertex normals that can be propodated on lathing
				 * use edge connectivity work this out */
				if (vc->v[0]>=0) {
					if (vc->v[1]>=0) {
						/* 2 edges connedted */
						/* make 2 connecting vert locations relative to the middle vert */
						sub_v3_v3v3(tmp_vec1, mvert_new[vc->v[0]].co, mvert_new[i].co);
						sub_v3_v3v3(tmp_vec2, mvert_new[vc->v[1]].co, mvert_new[i].co);
						/* normalize so both edges have the same influence, no matter their length */
						normalize_v3(tmp_vec1);
						normalize_v3(tmp_vec2);

						/* vc_no_tmp1 - this line is the average direction of both connecting edges
						 *
						 * Use the edge order to make the subtraction, flip the normal the right way
						 * edge should be there but check just in case... */
						if (vc->e && vc->e[0]->v1 == i) {
							sub_v3_v3v3(tmp_vec1, tmp_vec1, tmp_vec2);
						}
						else {
							sub_v3_v3v3(tmp_vec1, tmp_vec2, tmp_vec1);
						}
					}
					else {
						/* only 1 edge connected - same as above except
						 * dont need to average edge direction */
						if (vc->e && vc->e[0]->v2 == i) {
							sub_v3_v3v3(tmp_vec1, mvert_new[i].co, mvert_new[vc->v[0]].co);
						}
						else {
							sub_v3_v3v3(tmp_vec1, mvert_new[vc->v[0]].co, mvert_new[i].co);
						}
					}

					/* vc_no_tmp2 - is a line 90d from the pivot to the vec
					 * This is used so the resulting normal points directly away from the middle */
					cross_v3_v3v3(tmp_vec2, axis_vec, vc->co);

					/* edge average vector and right angle to the pivot make the normal */
					cross_v3_v3v3(vc->no, tmp_vec1, tmp_vec2);

				}
				else {
					copy_v3_v3(vc->no, vc->co);
				}

				/* we wont be looping on this data again so copy normals here */
				if (angle < 0.0f)
					negate_v3(vc->no);

				normalize_v3(vc->no);
				normal_float_to_short_v3(mvert_new[i].no, vc->no);

				/* Done with normals */
			}
		}
	}
	else {

		if (ltmd->flag & MOD_SCREW_NORMAL_FLIP) {
			mv_orig= mvert_orig;
			mv_new= mvert_new + (totvert-1);

			for (i=0; i < totvert; i++, mv_new--, mv_orig++) {
				copy_v3_v3(mv_new->co, mv_orig->co);
			}
		}
		else {
			mv_orig= mvert_orig;
			mv_new= mvert_new;

			for (i=0; i < totvert; i++, mv_new++, mv_orig++) {
				copy_v3_v3(mv_new->co, mv_orig->co);
			}
		}
	}
	/* done with edge connectivity based normal flipping */
	
	
	/* Add Faces */
	for (i=1; i < steps; i++) {
		float step_angle;
		float no_tx[3];
		/* Rotation Matrix */
		if (close)		step_angle= (angle / steps) * i;
		else			step_angle= (angle / (steps-1)) * i;

		if (ltmd->ob_axis) {
			axis_angle_to_mat3(mat3, axis_vec, step_angle);
			copy_m4_m3(mat, mat3);
		}
		else {
			unit_m4(mat);
			rotate_m4(mat, axis_char, step_angle);
			copy_m3_m4(mat3, mat);
		}

		if(screw_ofs)
			madd_v3_v3fl(mat[3], axis_vec, screw_ofs * ((float)i / (float)(steps-1)));

		mv_new_base= mvert_new;
		mv_new= &mvert_new[totvert*i]; /* advance to the next slice */
		
		for (j=0; j<totvert; j++, mv_new_base++, mv_new++) {
			/* set normal */
			if(vert_connect) {
				mul_v3_m3v3(no_tx, mat3, vert_connect[j].no);

				/* set the normal now its transformed */
				normal_float_to_short_v3(mv_new->no, no_tx);
			}
			
			/* set location */
			copy_v3_v3(mv_new->co, mv_new_base->co);
			
			/* only need to set these if using non cleared memory */
			/*mv_new->mat_nr= mv_new->flag= 0;*/
				
			if (ltmd->ob_axis) {
				sub_v3_v3(mv_new->co, mtx_tx[3]);

				mul_m4_v3(mat, mv_new->co);

				add_v3_v3(mv_new->co, mtx_tx[3]);
			}
			else {
				mul_m4_v3(mat, mv_new->co);
			}
			
			/* add the new edge */
			med_new->v1= j+(i*totvert);
			med_new->v2= med_new->v1 - totvert;
			med_new->flag= ME_EDGEDRAW|ME_EDGERENDER;
			med_new++;
		}
	}

	/* we can avoid if using vert alloc trick */
	if(vert_connect) {
		MEM_freeN(vert_connect);
		vert_connect= NULL;
	}

	if (close) {
		/* last loop of edges, previous loop dosnt account for the last set of edges */
		for (i=0; i<totvert; i++) {
			med_new->v1= i;
			med_new->v2= i+((steps-1)*totvert);
			med_new->flag= ME_EDGEDRAW|ME_EDGERENDER;
			med_new++;
		}
	}
	
	mf_new= mface_new;
	med_new_firstloop= medge_new;
	
	for (i=0; i < totedge; i++, med_new_firstloop++) {
		/* for each edge, make a cylinder of quads */
		i1= med_new_firstloop->v1;
		i2= med_new_firstloop->v2;
		
		for (j=0; j < steps-1; j++) {
			
			/* new face */
			mf_new->v1= i1;
			mf_new->v2= i2;
			mf_new->v3= i2 + totvert;
			mf_new->v4= i1 + totvert;
			
			if( !mf_new->v3 || !mf_new->v4 ) {
				SWAP(int, mf_new->v1, mf_new->v3);
				SWAP(int, mf_new->v2, mf_new->v4);
			}
			mf_new->flag= ME_SMOOTH;
			origindex[mface_index]= ORIGINDEX_NONE;
			mf_new++;
			mface_index++;
			
			/* new vertical edge */
			if (j) { /* The first set is alredy dome */
				med_new->v1= i1;
				med_new->v2= i2;
				med_new->flag= med_new_firstloop->flag;
				med_new->crease= med_new_firstloop->crease;
				med_new++;
			}
			i1 += totvert;
			i2 += totvert;
		}
		
		/* close the loop*/
		if (close) { 
			mf_new->v1= i1;
			mf_new->v2= i2;
			mf_new->v3= med_new_firstloop->v2;
			mf_new->v4= med_new_firstloop->v1;

			if( !mf_new->v3 || !mf_new->v4 ) {
				SWAP(int, mf_new->v1, mf_new->v3);
				SWAP(int, mf_new->v2, mf_new->v4);
			}
			mf_new->flag= ME_SMOOTH;
			origindex[mface_index]= ORIGINDEX_NONE;
			mf_new++;
			mface_index++;
		}
		
		/* new vertical edge */
		med_new->v1= i1;
		med_new->v2= i2;
		med_new->flag= med_new_firstloop->flag & ~ME_LOOSEEDGE;
		med_new->crease= med_new_firstloop->crease;
		med_new++;
	}
	
	if((ltmd->flag & MOD_SCREW_NORMAL_CALC)==0) {
		CDDM_calc_normals(result);
	}

	return result;
}


static void updateDepgraph(
									ModifierData *md, DagForest *forest,
									Scene *scene, Object *ob, DagNode *obNode)
{
	ScrewModifierData *ltmd= (ScrewModifierData*) md;

	if(ltmd->ob_axis) {
		DagNode *curNode= dag_get_node(forest, ltmd->ob_axis);

		dag_add_relation(forest, curNode, obNode,
						 DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
						 "Screw Modifier");
	}
}

static void foreachObjectLink(
				ModifierData *md, Object *ob,
				void (*walk)(void *userData, Object *ob, Object **obpoin),
				void *userData)
{
	ScrewModifierData *ltmd= (ScrewModifierData*) md;

	walk(userData, ob, &ltmd->ob_axis);
}

/* This dosnt work with material*/
static DerivedMesh *applyModifierEM(
						ModifierData *md, Object *ob, EditMesh *editData,
						DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}

static int dependsOnTime(ModifierData *md)
{
	return 0;
}


ModifierTypeInfo modifierType_Screw = {
	/* name */              "Screw",
	/* structName */        "ScrewModifierData",
	/* structSize */        sizeof(ScrewModifierData),
	/* type */              eModifierTypeType_Constructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
