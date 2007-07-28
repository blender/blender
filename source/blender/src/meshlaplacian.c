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
 * meshlaplacian.c: Algorithms using the mesh laplacian.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_arithb.h"
#include "BLI_edgehash.h"

#include "BKE_utildefines.h"

#include "BIF_editdeform.h"
#include "BIF_meshlaplacian.h"
#include "BIF_meshtools.h"
#include "BIF_toolbox.h"

#ifdef RIGID_DEFORM
#include "BLI_editVert.h"
#include "BLI_polardecomp.h"
#endif

#include "RE_raytrace.h"

#include "ONL_opennl.h"

/************************** Laplacian System *****************************/

struct LaplacianSystem {
	NLContext context;	/* opennl context */

	int totvert, totface;

	float **verts;			/* vertex coordinates */
	float *varea;			/* vertex weights for laplacian computation */
	char *vpinned;			/* vertex pinning */
	int (*faces)[3];		/* face vertex indices */
	float (*fweights)[3];	/* cotangent weights per face */

	int areaweights;		/* use area in cotangent weights? */
	int storeweights;		/* store cotangent weights in fweights */
	int nlbegun;			/* nlBegin(NL_SYSTEM/NL_MATRIX) done */

	EdgeHash *edgehash;		/* edge hash for construction */

	struct HeatWeighting {
		Mesh *mesh;
		float (*verts)[3];	/* vertex coordinates */
		float (*vnors)[3];	/* vertex normals */

		float (*root)[3];	/* bone root */
		float (*tip)[3];	/* bone tip */
		int numbones;

		float *H;			/* diagonal H matrix */
		float *p;			/* values from all p vectors */
		float *mindist;		/* minimum distance to a bone for all vertices */
		
		RayTree *raytree;	/* ray tracing acceleration structure */
		MFace **vface;		/* a face that the vertex belongs to */
	} heat;

#ifdef RIGID_DEFORM
	struct RigidDeformation {
		EditMesh *mesh;

		float (*R)[3][3];
		float (*rhs)[3];
		float (*origco)[3];
		int thrownerror;
	} rigid;
#endif
};

/* Laplacian matrix construction */

/* Computation of these weights for the laplacian is based on:
   "Discrete Differential-Geometry Operators for Triangulated 2-Manifolds",
   Meyer et al, 2002. Section 3.5, formula (8).
   
   We do it a bit different by going over faces instead of going over each
   vertex and adjacent faces, since we don't store this adjacency. Also, the
   formulas are tweaked a bit to work for non-manifold meshes. */

static void laplacian_increase_edge_count(EdgeHash *edgehash, int v1, int v2)
{
	void **p = BLI_edgehash_lookup_p(edgehash, v1, v2);

	if(p)
		*p = (void*)((long)*p + (long)1);
	else
		BLI_edgehash_insert(edgehash, v1, v2, (void*)(long)1);
}

static int laplacian_edge_count(EdgeHash *edgehash, int v1, int v2)
{
	return (int)(long)BLI_edgehash_lookup(edgehash, v1, v2);
}

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	VecSubf(a, v2, v1);
	VecSubf(b, v3, v1);
	Crossf(c, a, b);

	clen = VecLength(c);

	if (clen == 0.0f)
		return 0.0f;
	
	return Inpf(a, b)/clen;
}

static void laplacian_triangle_area(LaplacianSystem *sys, int i1, int i2, int i3)
{
	float t1, t2, t3, len1, len2, len3, area;
	float *varea= sys->varea, *v1, *v2, *v3;
	int obtuse = 0;

	v1= sys->verts[i1];
	v2= sys->verts[i2];
	v3= sys->verts[i3];

	t1= cotan_weight(v1, v2, v3);
	t2= cotan_weight(v2, v3, v1);
	t3= cotan_weight(v3, v1, v2);

	if(VecAngle3(v2, v1, v3) > 90) obtuse= 1;
	else if(VecAngle3(v1, v2, v3) > 90) obtuse= 2;
	else if(VecAngle3(v1, v3, v2) > 90) obtuse= 3;

	if (obtuse > 0) {
		area= AreaT3Dfl(v1, v2, v3);

		varea[i1] += (obtuse == 1)? area: area*0.5;
		varea[i2] += (obtuse == 2)? area: area*0.5;
		varea[i3] += (obtuse == 3)? area: area*0.5;
	}
	else {
		len1= VecLenf(v2, v3);
		len2= VecLenf(v1, v3);
		len3= VecLenf(v1, v2);

		t1 *= len1*len1;
		t2 *= len2*len2;
		t3 *= len3*len3;

		varea[i1] += (t2 + t3)*0.25f;
		varea[i2] += (t1 + t3)*0.25f;
		varea[i3] += (t1 + t2)*0.25f;
	}
}

static void laplacian_triangle_weights(LaplacianSystem *sys, int f, int i1, int i2, int i3)
{
	float t1, t2, t3;
	float *varea= sys->varea, *v1, *v2, *v3;

	v1= sys->verts[i1];
	v2= sys->verts[i2];
	v3= sys->verts[i3];

	/* instead of *0.5 we divided by the number of faces of the edge, it still
	   needs to be varified that this is indeed the correct thing to do! */
	t1= cotan_weight(v1, v2, v3)/laplacian_edge_count(sys->edgehash, i2, i3);
	t2= cotan_weight(v2, v3, v1)/laplacian_edge_count(sys->edgehash, i3, i1);
	t3= cotan_weight(v3, v1, v2)/laplacian_edge_count(sys->edgehash, i1, i2);

	nlMatrixAdd(i1, i1, (t2+t3)*varea[i1]);
	nlMatrixAdd(i2, i2, (t1+t3)*varea[i2]);
	nlMatrixAdd(i3, i3, (t1+t2)*varea[i3]);

	nlMatrixAdd(i1, i2, -t3*varea[i1]);
	nlMatrixAdd(i2, i1, -t3*varea[i2]);

	nlMatrixAdd(i2, i3, -t1*varea[i2]);
	nlMatrixAdd(i3, i2, -t1*varea[i3]);

	nlMatrixAdd(i3, i1, -t2*varea[i3]);
	nlMatrixAdd(i1, i3, -t2*varea[i1]);

	if(sys->storeweights) {
		sys->fweights[f][0]= t1*varea[i1];
		sys->fweights[f][1]= t2*varea[i2];
		sys->fweights[f][2]= t3*varea[i3];
	}
}

LaplacianSystem *laplacian_system_construct_begin(int totvert, int totface)
{
	LaplacianSystem *sys;

	sys= MEM_callocN(sizeof(LaplacianSystem), "LaplacianSystem");

	sys->verts= MEM_callocN(sizeof(float*)*totvert, "LaplacianSystemVerts");
	sys->vpinned= MEM_callocN(sizeof(char)*totvert, "LaplacianSystemVpinned");
	sys->faces= MEM_callocN(sizeof(int)*3*totface, "LaplacianSystemFaces");

	sys->totvert= 0;
	sys->totface= 0;

	sys->areaweights= 1;
	sys->storeweights= 0;

	/* create opennl context */
	nlNewContext();
	nlSolverParameteri(NL_NB_VARIABLES, totvert);

	sys->context= nlGetCurrent();

	return sys;
}

void laplacian_add_vertex(LaplacianSystem *sys, float *co, int pinned)
{
	sys->verts[sys->totvert]= co;
	sys->vpinned[sys->totvert]= pinned;
	sys->totvert++;
}

void laplacian_add_triangle(LaplacianSystem *sys, int v1, int v2, int v3)
{
	sys->faces[sys->totface][0]= v1;
	sys->faces[sys->totface][1]= v2;
	sys->faces[sys->totface][2]= v3;
	sys->totface++;
}

void laplacian_system_construct_end(LaplacianSystem *sys)
{
	int (*face)[3];
	int a, totvert=sys->totvert, totface=sys->totface;

	laplacian_begin_solve(sys, 0);

	sys->varea= MEM_callocN(sizeof(float)*totvert, "LaplacianSystemVarea");

	sys->edgehash= BLI_edgehash_new();
	for(a=0, face=sys->faces; a<sys->totface; a++, face++) {
		laplacian_increase_edge_count(sys->edgehash, (*face)[0], (*face)[1]);
		laplacian_increase_edge_count(sys->edgehash, (*face)[1], (*face)[2]);
		laplacian_increase_edge_count(sys->edgehash, (*face)[2], (*face)[0]);
	}

	if(sys->areaweights)
		for(a=0, face=sys->faces; a<sys->totface; a++, face++)
			laplacian_triangle_area(sys, (*face)[0], (*face)[1], (*face)[2]);
	
	for(a=0; a<totvert; a++) {
		if(sys->areaweights) {
			if(sys->varea[a] != 0.0f)
				sys->varea[a]= 0.5f/sys->varea[a];
		}
		else
			sys->varea[a]= 1.0f;

		/* for heat weighting */
		if(sys->heat.H)
			nlMatrixAdd(a, a, sys->heat.H[a]);
	}

	if(sys->storeweights)
		sys->fweights= MEM_callocN(sizeof(float)*3*totface, "LaplacianFWeight");
	
	for(a=0, face=sys->faces; a<totface; a++, face++)
		laplacian_triangle_weights(sys, a, (*face)[0], (*face)[1], (*face)[2]);

	MEM_freeN(sys->faces);
	sys->faces= NULL;

	if(sys->varea) {
		MEM_freeN(sys->varea);
		sys->varea= NULL;
	}

	BLI_edgehash_free(sys->edgehash, NULL);
	sys->edgehash= NULL;
}

void laplacian_system_delete(LaplacianSystem *sys)
{
	if(sys->verts) MEM_freeN(sys->verts);
	if(sys->varea) MEM_freeN(sys->varea);
	if(sys->vpinned) MEM_freeN(sys->vpinned);
	if(sys->faces) MEM_freeN(sys->faces);
	if(sys->fweights) MEM_freeN(sys->fweights);

	nlDeleteContext(sys->context);
	MEM_freeN(sys);
}

void laplacian_begin_solve(LaplacianSystem *sys, int index)
{
	int a;

	if (!sys->nlbegun) {
		nlBegin(NL_SYSTEM);

		if(index >= 0) {
			for(a=0; a<sys->totvert; a++) {
				if(sys->vpinned[a]) {
					nlSetVariable(a, sys->verts[a][index]);
					nlLockVariable(a);
				}
			}
		}

		nlBegin(NL_MATRIX);
		sys->nlbegun = 1;
	}
}

void laplacian_add_right_hand_side(LaplacianSystem *sys, int v, float value)
{
	nlRightHandSideAdd(v, value);
}

int laplacian_system_solve(LaplacianSystem *sys)
{
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);
	sys->nlbegun = 0;

	//nlPrintMatrix();

	return nlSolveAdvanced(NULL, NL_TRUE);
}

float laplacian_system_get_solution(int v)
{
	return nlGetVariable(v);
}

/************************* Heat Bone Weighting ******************************/
/* From "Automatic Rigging and Animation of 3D Characters"
         Ilya Baran and Jovan Popovic, SIGGRAPH 2007 */

#define C_WEIGHT			1.0f
#define WEIGHT_LIMIT		0.05f
#define DISTANCE_EPSILON	1e-4f

/* Raytracing for vertex to bone visibility */

static LaplacianSystem *HeatSys = NULL;

static void heat_ray_coords_func(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	MFace *mface= (MFace*)face;
	float (*verts)[3]= HeatSys->heat.verts;

	*v1= verts[mface->v1];
	*v2= verts[mface->v2];
	*v3= verts[mface->v3];
	*v4= (mface->v4)? verts[mface->v4]: NULL;
}

static int heat_ray_check_func(Isect *is, RayFace *face)
{
	float *v1, *v2, *v3, *v4, nor[3];

	/* don't intersect if the ray faces along the face normal */
	heat_ray_coords_func(face, &v1, &v2, &v3, &v4);

	if(v4) CalcNormFloat4(v1, v2, v3, v4, nor);
	else CalcNormFloat(v1, v2, v3, nor);
	
	return (INPR(nor, is->vec) < 0);
}

static void heat_ray_tree_create(LaplacianSystem *sys)
{
	Mesh *me = sys->heat.mesh;
	RayTree *tree;
	MFace *mface;
	float min[3], max[3];
	int a;

	/* create a raytrace tree from the mesh */
	INIT_MINMAX(min, max);

	for(a=0; a<me->totvert; a++)
		DO_MINMAX(sys->heat.verts[a], min, max);

	tree= RE_ray_tree_create(64, me->totface, min, max,
		heat_ray_coords_func, heat_ray_check_func);
	
	sys->heat.vface= MEM_callocN(sizeof(MFace*)*me->totvert, "HeatVFaces");

	HeatSys= sys;

	for(a=0, mface=me->mface; a<me->totface; a++, mface++) {
		RE_ray_tree_add_face(tree, mface);

		sys->heat.vface[mface->v1]= mface;
		sys->heat.vface[mface->v2]= mface;
		sys->heat.vface[mface->v3]= mface;
		if(mface->v4) sys->heat.vface[mface->v4]= mface;
	}

	HeatSys= NULL;
	
	RE_ray_tree_done(tree);

	sys->heat.raytree= tree;
}

static int heat_ray_bone_visible(LaplacianSystem *sys, int vertex, int bone)
{
	Isect isec;
	MFace *mface;
	float dir[3];
	int visible;

	mface= sys->heat.vface[vertex];
	if(!mface)
		return 1;

	/* setup isec */
	isec.mode= RE_RAY_SHADOW;
	isec.lay= -1;
	isec.face_last= NULL;
	isec.faceorig= mface;

	VECCOPY(isec.start, sys->heat.verts[vertex]);
	PclosestVL3Dfl(isec.end, isec.start,
		sys->heat.root[bone], sys->heat.tip[bone]);

	/* add an extra offset to the start position to avoid self intersection */
	VECSUB(dir, isec.end, isec.start);
	Normalize(dir);
	VecMulf(dir, 1e-5);
	VecAddf(isec.start, isec.start, dir);
	
	HeatSys= sys;
	visible= !RE_ray_tree_intersect(sys->heat.raytree, &isec);
	HeatSys= NULL;

	return visible;
}

static float heat_bone_distance(LaplacianSystem *sys, int vertex, int bone)
{
	float closest[3], d[3], dist, cosine;
	
	/* compute euclidian distance */
	PclosestVL3Dfl(closest, sys->heat.verts[vertex],
		sys->heat.root[bone], sys->heat.tip[bone]);

	VecSubf(d, sys->heat.verts[vertex], closest);
	dist= Normalize(d);

	/* if the vertex normal does not point along the bone, increase distance */
	cosine= INPR(d, sys->heat.vnors[vertex]);

	return dist/(0.5f*(cosine + 1.001f));
}

static int heat_bone_closest(LaplacianSystem *sys, int vertex, int bone)
{
	float dist;
	
	dist= heat_bone_distance(sys, vertex, bone);

	if(dist <= sys->heat.mindist[vertex]*(1.0f + DISTANCE_EPSILON))
		if(heat_ray_bone_visible(sys, vertex, bone))
			return 1;
	
	return 0;
}

static void heat_set_H(LaplacianSystem *sys, int vertex)
{
	float dist, mindist, h;
	int j, numclosest = 0;

	mindist= 1e10;

	/* compute minimum distance */
	for(j=0; j<sys->heat.numbones; j++) {
		dist= heat_bone_distance(sys, vertex, j);

		if(dist < mindist)
			mindist= dist;
	}

	sys->heat.mindist[vertex]= mindist;

	/* count number of bones with approximately this minimum distance */
	for(j=0; j<sys->heat.numbones; j++)
		if(heat_bone_closest(sys, vertex, j))
			numclosest++;

	sys->heat.p[vertex]= (numclosest > 0)? 1.0f/numclosest: 0.0f;

	/* compute H entry */
	if(numclosest > 0) {
		if(mindist > 1e-5)
			h= numclosest*C_WEIGHT/(mindist*mindist);
		else
			h= 1e10f;
	}
	else
		h= 0.0f;
	
	sys->heat.H[vertex]= h;
}

void heat_calc_vnormals(LaplacianSystem *sys)
{
	float fnor[3];
	int a, v1, v2, v3, (*face)[3];

	sys->heat.vnors= MEM_callocN(sizeof(float)*3*sys->totvert, "HeatVNors");

	for(a=0, face=sys->faces; a<sys->totface; a++, face++) {
		v1= (*face)[0];
		v2= (*face)[1];
		v3= (*face)[2];

		CalcNormFloat(sys->verts[v1], sys->verts[v2], sys->verts[v3], fnor);
		
		VecAddf(sys->heat.vnors[v1], sys->heat.vnors[v1], fnor);
		VecAddf(sys->heat.vnors[v2], sys->heat.vnors[v2], fnor);
		VecAddf(sys->heat.vnors[v3], sys->heat.vnors[v3], fnor);
	}

	for(a=0; a<sys->totvert; a++)
		Normalize(sys->heat.vnors[a]);
}

static void heat_laplacian_create(LaplacianSystem *sys)
{
	Mesh *me = sys->heat.mesh;
	MFace *mface;
	int a;

	/* heat specific definitions */
	sys->heat.mindist= MEM_callocN(sizeof(float)*me->totvert, "HeatMinDist");
	sys->heat.H= MEM_callocN(sizeof(float)*me->totvert, "HeatH");
	sys->heat.p= MEM_callocN(sizeof(float)*me->totvert, "HeatP");

	/* add verts and faces to laplacian */
	for(a=0; a<me->totvert; a++)
		laplacian_add_vertex(sys, sys->heat.verts[a], 0);

	for(a=0, mface=me->mface; a<me->totface; a++, mface++) {
		laplacian_add_triangle(sys, mface->v1, mface->v2, mface->v3);
		if(mface->v4)
			laplacian_add_triangle(sys, mface->v1, mface->v3, mface->v4);
	}

	/* for distance computation in set_H */
	heat_calc_vnormals(sys);

	for(a=0; a<me->totvert; a++)
		heat_set_H(sys, a);
}

void heat_bone_weighting(Object *ob, Mesh *me, float (*verts)[3], int numbones, bDeformGroup **dgrouplist, bDeformGroup **dgroupflip, float (*root)[3], float (*tip)[3], int *selected)
{
	LaplacianSystem *sys;
	MFace *mface;
	float solution;
	int a, aflip, totface, j, thrownerror = 0;

	/* count triangles */
	for(totface=0, a=0, mface=me->mface; a<me->totface; a++, mface++) {
		totface++;
		if(mface->v4) totface++;
	}

	/* create laplacian */
	sys = laplacian_system_construct_begin(me->totvert, totface);

	sys->heat.mesh= me;
	sys->heat.verts= verts;
	sys->heat.root= root;
	sys->heat.tip= tip;
	sys->heat.numbones= numbones;

	heat_ray_tree_create(sys);
	heat_laplacian_create(sys);

	laplacian_system_construct_end(sys);

	/* compute weights per bone */
	for(j=0; j<numbones; j++) {
		if(!selected[j])
			continue;

		laplacian_begin_solve(sys, -1);

		for(a=0; a<me->totvert; a++)
			if(heat_bone_closest(sys, a, j))
				laplacian_add_right_hand_side(sys, a,
					sys->heat.H[a]*sys->heat.p[a]);

		if(laplacian_system_solve(sys)) {
			for(a=0; a<me->totvert; a++) {
				solution= laplacian_system_get_solution(a);

				if(solution > WEIGHT_LIMIT)
					add_vert_to_defgroup(ob, dgrouplist[j], a, solution,
						WEIGHT_REPLACE);
				else
					remove_vert_defgroup(ob, dgrouplist[j], a);

				/* do same for mirror */
				aflip = (dgroupflip)? mesh_get_x_mirror_vert(ob, a): 0;
				if (dgroupflip && dgroupflip[j] && aflip >= 0) {
					if(solution > WEIGHT_LIMIT)
						add_vert_to_defgroup(ob, dgroupflip[j], aflip,
							solution, WEIGHT_REPLACE);
					else
						remove_vert_defgroup(ob, dgroupflip[j], aflip);
				}
			}
		}
		else if(!thrownerror) {
			error("Bone Heat Weighting:"
				" failed to find solution for one or more bones");
			thrownerror= 1;
			break;
		}
	}

	/* free */
	RE_ray_tree_free(sys->heat.raytree);
	MEM_freeN(sys->heat.vface);

	MEM_freeN(sys->heat.mindist);
	MEM_freeN(sys->heat.H);
	MEM_freeN(sys->heat.p);
	MEM_freeN(sys->heat.vnors);

	laplacian_system_delete(sys);
}

#ifdef RIGID_DEFORM
/********************** As-Rigid-As-Possible Deformation ******************/
/* From "As-Rigid-As-Possible Surface Modeling",
        Olga Sorkine and Marc Alexa, ESGP 2007. */

/* investigate:
   - transpose R in orthogonal
   - flipped normals and per face adding
   - move cancelling to transform, make origco pointer
*/

static LaplacianSystem *RigidDeformSystem = NULL;

static void rigid_add_half_edge_to_R(LaplacianSystem *sys, EditVert *v1, EditVert *v2, float w)
{
	float e[3], e_[3];
	int i;

	VecSubf(e, sys->rigid.origco[v1->tmp.l], sys->rigid.origco[v2->tmp.l]);
	VecSubf(e_, v1->co, v2->co);

	/* formula (5) */
	for (i=0; i<3; i++) {
		sys->rigid.R[v1->tmp.l][i][0] += w*e[0]*e_[i];
		sys->rigid.R[v1->tmp.l][i][1] += w*e[1]*e_[i];
		sys->rigid.R[v1->tmp.l][i][2] += w*e[2]*e_[i];
	}
}

static void rigid_add_edge_to_R(LaplacianSystem *sys, EditVert *v1, EditVert *v2, float w)
{
	rigid_add_half_edge_to_R(sys, v1, v2, w);
	rigid_add_half_edge_to_R(sys, v2, v1, w);
}

static void rigid_orthogonalize_R(float R[][3])
{
	HMatrix M, Q, S;

	Mat4CpyMat3(M, R);
	polar_decomp(M, Q, S);
	Mat3CpyMat4(R, Q);
}

static void rigid_add_half_edge_to_rhs(LaplacianSystem *sys, EditVert *v1, EditVert *v2, float w)
{
	/* formula (8) */
	float Rsum[3][3], rhs[3];

	if (sys->vpinned[v1->tmp.l])
		return;

	Mat3AddMat3(Rsum, sys->rigid.R[v1->tmp.l], sys->rigid.R[v2->tmp.l]);
	Mat3Transp(Rsum);

	VecSubf(rhs, sys->rigid.origco[v1->tmp.l], sys->rigid.origco[v2->tmp.l]);
	Mat3MulVecfl(Rsum, rhs);
	VecMulf(rhs, 0.5f);
	VecMulf(rhs, w);

	VecAddf(sys->rigid.rhs[v1->tmp.l], sys->rigid.rhs[v1->tmp.l], rhs);
}

static void rigid_add_edge_to_rhs(LaplacianSystem *sys, EditVert *v1, EditVert *v2, float w)
{
	rigid_add_half_edge_to_rhs(sys, v1, v2, w);
	rigid_add_half_edge_to_rhs(sys, v2, v1, w);
}

void rigid_deform_iteration()
{
	LaplacianSystem *sys= RigidDeformSystem;
	EditMesh *em;
	EditVert *eve;
	EditFace *efa;
	int a, i;

	if(!sys)
		return;
	
	nlMakeCurrent(sys->context);
	em= sys->rigid.mesh;

	/* compute R */
	memset(sys->rigid.R, 0, sizeof(float)*3*3*sys->totvert);
	memset(sys->rigid.rhs, 0, sizeof(float)*3*sys->totvert);

	for(a=0, efa=em->faces.first; efa; efa=efa->next, a++) {
		rigid_add_edge_to_R(sys, efa->v1, efa->v2, sys->fweights[a][2]);
		rigid_add_edge_to_R(sys, efa->v2, efa->v3, sys->fweights[a][0]);
		rigid_add_edge_to_R(sys, efa->v3, efa->v1, sys->fweights[a][1]);

		if(efa->v4) {
			a++;
			rigid_add_edge_to_R(sys, efa->v1, efa->v3, sys->fweights[a][2]);
			rigid_add_edge_to_R(sys, efa->v3, efa->v4, sys->fweights[a][0]);
			rigid_add_edge_to_R(sys, efa->v4, efa->v1, sys->fweights[a][1]);
		}
	}

	for(a=0, eve=em->verts.first; eve; eve=eve->next, a++) {
		rigid_orthogonalize_R(sys->rigid.R[a]);
		eve->tmp.l= a;
	}
	
	/* compute right hand sides for solving */
	for(a=0, efa=em->faces.first; efa; efa=efa->next, a++) {
		rigid_add_edge_to_rhs(sys, efa->v1, efa->v2, sys->fweights[a][2]);
		rigid_add_edge_to_rhs(sys, efa->v2, efa->v3, sys->fweights[a][0]);
		rigid_add_edge_to_rhs(sys, efa->v3, efa->v1, sys->fweights[a][1]);

		if(efa->v4) {
			a++;
			rigid_add_edge_to_rhs(sys, efa->v1, efa->v3, sys->fweights[a][2]);
			rigid_add_edge_to_rhs(sys, efa->v3, efa->v4, sys->fweights[a][0]);
			rigid_add_edge_to_rhs(sys, efa->v4, efa->v1, sys->fweights[a][1]);
		}
	}

	/* solve for positions, for X,Y and Z separately */
	for(i=0; i<3; i++) {
		laplacian_begin_solve(sys, i);

		for(a=0; a<sys->totvert; a++)
			if(!sys->vpinned[a]) {
				/*if (i==0)
					printf("rhs %f\n", sys->rigid.rhs[a][0]);*/
				laplacian_add_right_hand_side(sys, a, sys->rigid.rhs[a][i]);
			}

		if(laplacian_system_solve(sys)) {
			for(a=0, eve=em->verts.first; eve; eve=eve->next, a++)
				eve->co[i]= laplacian_system_get_solution(a);
		}
		else {
			if(!sys->rigid.thrownerror) {
				error("RigidDeform: failed to find solution.");
				sys->rigid.thrownerror= 1;
			}
			break;
		}
	}

	/*printf("\n--------------------------------------------\n\n");*/
}

static void rigid_laplacian_create(LaplacianSystem *sys)
{
	EditMesh *em = sys->rigid.mesh;
	EditVert *eve;
	EditFace *efa;
	int a;

	/* add verts and faces to laplacian */
	for(a=0, eve=em->verts.first; eve; eve=eve->next, a++) {
		laplacian_add_vertex(sys, eve->co, eve->pinned);
		eve->tmp.l= a;
	}

	for(efa=em->faces.first; efa; efa=efa->next) {
		laplacian_add_triangle(sys,
			efa->v1->tmp.l, efa->v2->tmp.l, efa->v3->tmp.l);
		if(efa->v4)
			laplacian_add_triangle(sys,
				efa->v1->tmp.l, efa->v3->tmp.l, efa->v4->tmp.l);
	}
}

void rigid_deform_begin(EditMesh *em)
{
	LaplacianSystem *sys;
	EditVert *eve;
	EditFace *efa;
	int a, totvert, totface;

	/* count vertices, triangles */
	for(totvert=0, eve=em->verts.first; eve; eve=eve->next)
		totvert++;

	for(totface=0, efa=em->faces.first; efa; efa=efa->next) {
		totface++;
		if(efa->v4) totface++;
	}

	/* create laplacian */
	sys = laplacian_system_construct_begin(totvert, totface);

	sys->rigid.mesh= em;
	sys->rigid.R = MEM_callocN(sizeof(float)*3*3*totvert, "RigidDeformR");
	sys->rigid.rhs = MEM_callocN(sizeof(float)*3*totvert, "RigidDeformRHS");
	sys->rigid.origco = MEM_callocN(sizeof(float)*3*totvert, "RigidDeformCo");

	for(a=0, eve=em->verts.first; eve; eve=eve->next, a++)
		VecCopyf(sys->rigid.origco[a], eve->co);

	sys->areaweights= 0;
	sys->storeweights= 1;

	rigid_laplacian_create(sys);

	laplacian_system_construct_end(sys);

	RigidDeformSystem = sys;
}

void rigid_deform_end(int cancel)
{
	LaplacianSystem *sys = RigidDeformSystem;

	if(sys) {
		EditMesh *em = sys->rigid.mesh;
		EditVert *eve;
		int a;

		if(cancel)
			for(a=0, eve=em->verts.first; eve; eve=eve->next, a++)
				if(!eve->pinned)
					VecCopyf(eve->co, sys->rigid.origco[a]);

		if(sys->rigid.R) MEM_freeN(sys->rigid.R);
		if(sys->rigid.rhs) MEM_freeN(sys->rigid.rhs);
		if(sys->rigid.origco) MEM_freeN(sys->rigid.origco);

		/* free */
		laplacian_system_delete(sys);
	}

	RigidDeformSystem = NULL;
}
#endif

