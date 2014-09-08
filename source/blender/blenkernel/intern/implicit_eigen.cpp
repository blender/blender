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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/implicit_eigen.cpp
 *  \ingroup bke
 */

#define IMPLICIT_SOLVER_EIGEN

//#define USE_EIGEN_CORE
#define USE_EIGEN_CONSTRAINED_CG

#ifdef IMPLICIT_SOLVER_EIGEN

#include <Eigen/Sparse>
#include <Eigen/src/Core/util/DisableStupidWarnings.h>

#ifdef USE_EIGEN_CONSTRAINED_CG
#include <intern/ConstrainedConjugateGradient.h>
#endif

#include "MEM_guardedalloc.h"

extern "C" {
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_meshdata_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"
#include "BKE_collision.h"
#include "BKE_effect.h"
#include "BKE_global.h"
}

/* ==== hash functions for debugging ==== */
static unsigned int hash_int_2d(unsigned int kx, unsigned int ky)
{
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	unsigned int a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);

	return c;

#undef rot
}

static int hash_vertex(int type, int vertex)
{
	return hash_int_2d((unsigned int)type, (unsigned int)vertex);
}

static int hash_collpair(int type, CollPair *collpair)
{
	return hash_int_2d((unsigned int)type, hash_int_2d((unsigned int)collpair->face1, (unsigned int)collpair->face2));
}
/* ================ */


typedef float Scalar;

typedef Eigen::SparseMatrix<Scalar> lMatrix;

typedef Eigen::VectorXf lVector;

typedef Eigen::Triplet<Scalar> Triplet;
typedef std::vector<Triplet> TripletList;

#ifdef USE_EIGEN_CORE
typedef Eigen::ConjugateGradient<lMatrix, Eigen::Lower, Eigen::DiagonalPreconditioner<Scalar> > ConjugateGradient;
#endif
#ifdef USE_EIGEN_CONSTRAINED_CG
typedef Eigen::ConstrainedConjugateGradient<lMatrix, Eigen::Lower, lMatrix,
                                            Eigen::DiagonalPreconditioner<Scalar> >
        ConstraintConjGrad;
#endif
using Eigen::ComputationInfo;

static void print_lvector(const lVector &v)
{
	for (int i = 0; i < v.rows(); ++i) {
		printf("%f,\n", v[i]);
	}
}

static void print_lmatrix(const lMatrix &m)
{
	for (int j = 0; j < m.rows(); ++j) {
		for (int i = 0; i < m.cols(); ++i) {
			printf("%-8.3f,", m.coeff(j, i));
		}
		printf("\n");
	}
}

static float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
static float ZERO[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

BLI_INLINE void lMatrix_reserve_elems(lMatrix &m, int num)
{
	m.reserve(Eigen::VectorXi::Constant(m.cols(), num));
}

BLI_INLINE float *lVector_v3(lVector &v, int vertex)
{
	return v.data() + 3 * vertex;
}

BLI_INLINE const float *lVector_v3(const lVector &v, int vertex)
{
	return v.data() + 3 * vertex;
}

BLI_INLINE void lMatrix_copy_m3(lMatrix &r, float m[3][3], int i, int j)
{
	i *= 3;
	j *= 3;
	for (int l = 0; l < 3; ++l) {
		for (int k = 0; k < 3; ++k) {
			r.coeffRef(i + k, j + l) = m[k][l];
		}
	}
}

BLI_INLINE void lMatrix_add_m3(lMatrix &r, float m[3][3], int i, int j)
{
	lMatrix tmp(r.cols(), r.cols());
	if (i == j) {
		lMatrix_copy_m3(tmp, m, i, i);
	}
	else {
		lMatrix_copy_m3(tmp, m, i, j);
		lMatrix_copy_m3(tmp, m, j, i);
	}
	
	r += tmp;
}

BLI_INLINE void lMatrix_sub_m3(lMatrix &r, float m[3][3], int i, int j)
{
	lMatrix tmp(r.cols(), r.cols());
	if (i == j) {
		lMatrix_copy_m3(tmp, m, i, i);
	}
	else {
		lMatrix_copy_m3(tmp, m, i, j);
		lMatrix_copy_m3(tmp, m, j, i);
	}
	
	r -= tmp;
}

BLI_INLINE void lMatrix_madd_m3(lMatrix &r, float m[3][3], float s, int i, int j)
{
	lMatrix tmp(r.cols(), r.cols());
	if (i == j) {
		lMatrix_copy_m3(tmp, m, i, i);
	}
	else {
		lMatrix_copy_m3(tmp, m, i, j);
		lMatrix_copy_m3(tmp, m, j, i);
	}
	
	r += s * tmp;
}

BLI_INLINE void triplets_m3(TripletList &t, float m[3][3], int i, int j)
{
	i *= 3;
	j *= 3;
	for (int k = 0; k < 3; ++k) {
		for (int l = 0; l < 3; ++l) {
			t.push_back(Triplet(i + k, j + l, m[k][l]));
		}
	}
}

BLI_INLINE void outerproduct(float r[3][3], const float a[3], const float b[3])
{
	mul_v3_v3fl(r[0], a, b[0]);
	mul_v3_v3fl(r[1], a, b[1]);
	mul_v3_v3fl(r[2], a, b[2]);
}

struct Implicit_Data {
	Implicit_Data(int numverts)
	{
		resize(numverts);
	}
	
	void resize(int numverts)
	{
		this->numverts = numverts;
		int tot = 3 * numverts;
		
		M.resize(tot, tot);
		dFdV.resize(tot, tot);
		dFdX.resize(tot, tot);
		
		X.resize(tot);
		Xnew.resize(tot);
		V.resize(tot);
		Vnew.resize(tot);
		F.resize(tot);
		
		B.resize(tot);
		A.resize(tot, tot);
		
		dV.resize(tot);
		z.resize(tot);
		S.resize(tot, tot);
	}
	
	int numverts;
	
	/* inputs */
	lMatrix M;					/* masses */
	lMatrix dFdV, dFdX;			/* force jacobians */
	
	/* motion state data */
	lVector X, Xnew;			/* positions */
	lVector V, Vnew;			/* velocities */
	lVector F;					/* forces */
	
	/* internal solver data */
	lVector B;					/* B for A*dV = B */
	lMatrix A;					/* A for A*dV = B */
	
	lVector dV;					/* velocity change (solution of A*dV = B) */
	lVector z;					/* target velocity in constrained directions */
	lMatrix S;					/* filtering matrix for constraints */
};

static bool simulate_implicit_euler(Implicit_Data *id, float dt)
{
#ifdef USE_EIGEN_CORE
	ConjugateGradient cg;
	cg.setMaxIterations(100);
	cg.setTolerance(0.01f);
	
	id->A = id->M - dt * id->dFdV - dt*dt * id->dFdX;
	cg.compute(id->A);
	
	id->B = dt * id->F + dt*dt * id->dFdX * id->V;
	id->dV = cg.solve(id->B);
	
	id->Vnew = id->V + id->dV;
	
	return cg.info() != Eigen::Success;
#endif

#ifdef USE_EIGEN_CONSTRAINED_CG
	ConstraintConjGrad cg;
	cg.setMaxIterations(100);
	cg.setTolerance(0.01f);
	
	id->A = id->M - dt * id->dFdV - dt*dt * id->dFdX;
	cg.compute(id->A);
	cg.filter() = id->S;
	
	id->B = dt * id->F + dt*dt * id->dFdX * id->V;
	id->dV = cg.solveWithGuess(id->B, id->z);
	
	id->Vnew = id->V + id->dV;
	
	return cg.info() != Eigen::Success;
#endif
}

BLI_INLINE void dfdx_spring(float to[3][3], const float dir[3], float length, float L, float k)
{
	// dir is unit length direction, rest is spring's restlength, k is spring constant.
	//return  ( (I-outerprod(dir, dir))*Min(1.0f, rest/length) - I) * -k;
	outerproduct(to, dir, dir);
	sub_m3_m3m3(to, I, to);
	
	mul_m3_fl(to, (L/length)); 
	sub_m3_m3m3(to, to, I);
	mul_m3_fl(to, -k);
}

/* unused */
#if 0
BLI_INLINE void dfdx_damp(float to[3][3], const float dir[3], float length, const float vel[3], float rest, float damping)
{
	// inner spring damping   vel is the relative velocity  of the endpoints.  
	// 	return (I-outerprod(dir, dir)) * (-damping * -(dot(dir, vel)/Max(length, rest)));
	mul_fvectorT_fvector(to, dir, dir);
	sub_fmatrix_fmatrix(to, I, to);
	mul_fmatrix_S(to,  (-damping * -(dot_v3v3(dir, vel)/MAX2(length, rest))));
}
#endif

DO_INLINE void dfdv_damp(float to[3][3], const float dir[3], float damping)
{
	// derivative of force wrt velocity
	outerproduct(to, dir, dir);
	mul_m3_fl(to, damping);
}

static void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s, const lVector &X, const lVector &V, float time)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	ClothVertex *v1 = &verts[s->ij], *v2 = &verts[s->kl];
	float extent[3];
	float length = 0, dot = 0;
	float dir[3] = {0, 0, 0};
	float vel[3];
	float k = 0.0f;
	float L = s->restlen;
	float cb; /* = clmd->sim_parms->structural; */ /*UNUSED*/
	
	float stretch_force[3] = {0, 0, 0};
	float bending_force[3] = {0, 0, 0};
	float nulldfdx[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	
	float scaling = 0.0;
	
	int no_compress = clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
	
	zero_v3(s->f);
	zero_m3(s->dfdx);
	zero_m3(s->dfdv);
	
	s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;
	/* ignore springs between pinned vertices */
	if (v1->flags & CLOTH_VERT_FLAG_PINNED && v2->flags & CLOTH_VERT_FLAG_PINNED)
		return;
	
	// calculate elonglation
	sub_v3_v3v3(extent, lVector_v3(X, s->kl), lVector_v3(X, s->ij));
	sub_v3_v3v3(vel, lVector_v3(V, s->kl), lVector_v3(V, s->ij));
	dot = dot_v3v3(extent, extent);
	length = sqrt(dot);
	
	if (length > ALMOST_ZERO) {
		/*
		if (length>L) {
			if ((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) &&
			    ( ((length-L)*100.0f/L) > clmd->sim_parms->maxspringlen )) {
				// cut spring!
				s->flags |= CSPRING_FLAG_DEACTIVATE;
				return;
			}
		}
		*/
		mul_v3_v3fl(dir, extent, 1.0f/length);
	}
	else {
		zero_v3(dir);
	}
	
	// calculate force of structural + shear springs
	if (ELEM(s->type, CLOTH_SPRING_TYPE_STRUCTURAL, CLOTH_SPRING_TYPE_SHEAR, CLOTH_SPRING_TYPE_SEWING)) {
		if (length > L || no_compress) {
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;
			
			k = clmd->sim_parms->structural;
			scaling = k + s->stiffness * fabsf(clmd->sim_parms->max_struct - k);

			k = scaling / (clmd->sim_parms->avg_spring_len + FLT_EPSILON);
			// TODO: verify, half verified (couldn't see error)
			if (s->type & CLOTH_SPRING_TYPE_SEWING) {
				// sewing springs usually have a large distance at first so clamp the force so we don't get tunnelling through colission objects
				float force = k*(length-L);
				if (force > clmd->sim_parms->max_sewing) {
					force = clmd->sim_parms->max_sewing;
				}
				mul_v3_v3fl(stretch_force, dir, force);
			}
			else {
				mul_v3_v3fl(stretch_force, dir, k * (length - L));
			}
			
			add_v3_v3(s->f, stretch_force);
			
			// Ascher & Boxman, p.21: Damping only during elonglation
			// something wrong with it...
			madd_v3_v3fl(s->f, dir, clmd->sim_parms->Cdis * dot_v3v3(vel, dir));
			
			/* VERIFIED */
			dfdx_spring(s->dfdx, dir, length, L, k);
			
			/* VERIFIED */
			dfdv_damp(s->dfdv, dir, clmd->sim_parms->Cdis);
			
		}
	}
	else if (s->type & CLOTH_SPRING_TYPE_GOAL) {
		float target[3];
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		// current_position = xold + t * (xnew - xold)
		interp_v3_v3v3(target, v1->xold, v1->xconst, time);
		sub_v3_v3v3(extent, lVector_v3(X, s->ij), target);
		BKE_sim_debug_data_add_line(clmd->debug_data, v1->xconst, v1->xold, 1,0,0, "springs", hash_vertex(7825, s->ij));
		
		// SEE MSG BELOW (these are UNUSED)
		// dot = dot_v3v3(extent, extent);
		// length = sqrt(dot);
		
		k = clmd->sim_parms->goalspring;
		scaling = k + s->stiffness * fabsf(clmd->sim_parms->max_struct - k);
			
		k = v1->goal * scaling / (clmd->sim_parms->avg_spring_len + FLT_EPSILON);
		madd_v3_v3fl(s->f, extent, -k);
		
		/* XXX this has no effect: dir is always null at this point! - lukas_t
		madd_v3_v3fl(s->f, dir, clmd->sim_parms->goalfrict * 0.01f * dot_v3v3(vel, dir));
		*/
		
		// HERE IS THE PROBLEM!!!!
		// dfdx_spring(s->dfdx, dir, length, 0.0, k);
		// dfdv_damp(s->dfdv, dir, MIN2(1.0, (clmd->sim_parms->goalfrict/100.0)));
	}
#if 0
	else {  /* calculate force of bending springs */
		if (length < L) {
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;
			
			k = clmd->sim_parms->bending;
			
			scaling = k + s->stiffness * fabsf(clmd->sim_parms->max_bend - k);
			cb = k = scaling / (20.0f * (clmd->sim_parms->avg_spring_len + FLT_EPSILON));

			mul_fvector_S(bending_force, dir, fbstar(length, L, k, cb));
			VECADD(s->f, s->f, bending_force);

			dfdx_spring_type2(s->dfdx, dir, length, L, k, cb);
		}
	}
#endif
}

static void cloth_apply_spring_force(ClothModifierData *clmd, ClothSpring *s, lVector &F, lMatrix &dFdX, lMatrix &dFdV)
{
	/* XXX reserve elements in tmp? */
	
	/* ignore disabled springs */
	if (!(s->flags & CLOTH_SPRING_FLAG_NEEDED))
		return;
	
	if (!(s->type & CLOTH_SPRING_TYPE_BENDING)) {
		lMatrix_sub_m3(dFdV, s->dfdv, s->ij, s->kl);
	}
	
	add_v3_v3(lVector_v3(F, s->ij), s->f);
	if (!(s->type & CLOTH_SPRING_TYPE_GOAL)) {
		sub_v3_v3(lVector_v3(F, s->kl), s->f);
		
	}
	
	lMatrix_sub_m3(dFdX, s->dfdx, s->ij, s->kl);
}

static void cloth_calc_force(ClothModifierData *clmd, lVector &F, lMatrix &dFdX, lMatrix &dFdV, const lVector &X, const lVector &V, const lMatrix &M, ListBase *effectors, float time)
{
	Cloth *cloth = clmd->clothObject;
	unsigned int numverts = cloth->numverts;
	ClothVertex *verts = cloth->verts;
	float spring_air 	= clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float gravity[3] = {0,0,0};
//	lVector diagonal(3*numverts);
//	diagonal.setZero
	TripletList coeff_dFdX, coeff_dFdV;
	
	F.setZero();
	dFdX.setZero();
	dFdV.setZero();
	
	/* set dFdV jacobi matrix diagonal entries to -spring_air */
//	dFdV.setIdentity();
//	initdiag_bfmatrix(dFdV, tm2);
	
	/* global acceleration (gravitation) */
	if (clmd->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		/* scale gravity force
		 * XXX 0.001 factor looks totally arbitrary ... what is this? lukas_t
		 */
		mul_v3_v3fl(gravity, clmd->scene->physics_settings.gravity, 0.001f * clmd->sim_parms->effector_weights->global_gravity);
	}
	
	/* initialize force with gravity */
	for (int i = 0; i < numverts; ++i) {
		/* gravitational mass same as inertial mass */
		mul_v3_v3fl(lVector_v3(F, i), gravity, verts[i].mass);
	}

#if 0
	/* Collect forces and derivatives:  F, dFdX, dFdV */
	Cloth 		*cloth 		= clmd->clothObject;
	unsigned int i	= 0;
	float 		spring_air 	= clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float 		gravity[3] = {0.0f, 0.0f, 0.0f};
	float 		tm2[3][3] 	= {{0}};
	MFace 		*mfaces 	= cloth->mfaces;
	unsigned int numverts = cloth->numverts;
	LinkNode *search;
	lfVector *winvec;
	EffectedPoint epoint;

	tm2[0][0] = tm2[1][1] = tm2[2][2] = -spring_air;
	
	/* global acceleration (gravitation) */
	if (clmd->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(gravity, clmd->scene->physics_settings.gravity);
		mul_fvector_S(gravity, gravity, 0.001f * clmd->sim_parms->effector_weights->global_gravity); /* scale gravity force */
	}

	/* set dFdX jacobi matrix to zero */
	init_bfmatrix(dFdX, ZERO);
	/* set dFdX jacobi matrix diagonal entries to -spring_air */ 
	initdiag_bfmatrix(dFdV, tm2);

	init_lfvector(lF, gravity, numverts);
	
	hair_volume_forces(clmd, lF, lX, lV, numverts);

	/* multiply lF with mass matrix
	 * force = mass * acceleration (in this case: gravity)
	 */
	for (i = 0; i < numverts; i++) {
		float temp[3];
		copy_v3_v3(temp, lF[i]);
		mul_fmatrix_fvector(lF[i], M[i].m, temp);
	}

	submul_lfvectorS(lF, lV, spring_air, numverts);
	
	/* handle external forces like wind */
	if (effectors) {
		// 0 = force, 1 = normalized force
		winvec = create_lfvector(cloth->numverts);
		
		if (!winvec)
			printf("winvec: out of memory in implicit.c\n");
		
		// precalculate wind forces
		for (i = 0; i < cloth->numverts; i++) {
			pd_point_from_loc(clmd->scene, (float*)lX[i], (float*)lV[i], i, &epoint);
			pdDoEffectors(effectors, NULL, clmd->sim_parms->effector_weights, &epoint, winvec[i], NULL);
		}
		
		for (i = 0; i < cloth->numfaces; i++) {
			float trinormal[3] = {0, 0, 0}; // normalized triangle normal
			float triunnormal[3] = {0, 0, 0}; // not-normalized-triangle normal
			float tmp[3] = {0, 0, 0};
			float factor = (mfaces[i].v4) ? 0.25 : 1.0 / 3.0;
			factor *= 0.02f;
			
			// calculate face normal
			if (mfaces[i].v4)
				CalcFloat4(lX[mfaces[i].v1], lX[mfaces[i].v2], lX[mfaces[i].v3], lX[mfaces[i].v4], triunnormal);
			else
				CalcFloat(lX[mfaces[i].v1], lX[mfaces[i].v2], lX[mfaces[i].v3], triunnormal);

			normalize_v3_v3(trinormal, triunnormal);
			
			// add wind from v1
			copy_v3_v3(tmp, trinormal);
			mul_v3_fl(tmp, calculateVertexWindForce(winvec[mfaces[i].v1], triunnormal));
			VECADDS(lF[mfaces[i].v1], lF[mfaces[i].v1], tmp, factor);
			
			// add wind from v2
			copy_v3_v3(tmp, trinormal);
			mul_v3_fl(tmp, calculateVertexWindForce(winvec[mfaces[i].v2], triunnormal));
			VECADDS(lF[mfaces[i].v2], lF[mfaces[i].v2], tmp, factor);
			
			// add wind from v3
			copy_v3_v3(tmp, trinormal);
			mul_v3_fl(tmp, calculateVertexWindForce(winvec[mfaces[i].v3], triunnormal));
			VECADDS(lF[mfaces[i].v3], lF[mfaces[i].v3], tmp, factor);
			
			// add wind from v4
			if (mfaces[i].v4) {
				copy_v3_v3(tmp, trinormal);
				mul_v3_fl(tmp, calculateVertexWindForce(winvec[mfaces[i].v4], triunnormal));
				VECADDS(lF[mfaces[i].v4], lF[mfaces[i].v4], tmp, factor);
			}
		}

		/* Hair has only edges */
		if (cloth->numfaces == 0) {
			ClothSpring *spring;
			float edgevec[3] = {0, 0, 0}; //edge vector
			float edgeunnormal[3] = {0, 0, 0}; // not-normalized-edge normal
			float tmp[3] = {0, 0, 0};
			float factor = 0.01;

			search = cloth->springs;
			while (search) {
				spring = search->link;
				
				if (spring->type == CLOTH_SPRING_TYPE_STRUCTURAL) {
					sub_v3_v3v3(edgevec, (float*)lX[spring->ij], (float*)lX[spring->kl]);

					project_v3_v3v3(tmp, winvec[spring->ij], edgevec);
					sub_v3_v3v3(edgeunnormal, winvec[spring->ij], tmp);
					/* hair doesn't stretch too much so we can use restlen pretty safely */
					VECADDS(lF[spring->ij], lF[spring->ij], edgeunnormal, spring->restlen * factor);

					project_v3_v3v3(tmp, winvec[spring->kl], edgevec);
					sub_v3_v3v3(edgeunnormal, winvec[spring->kl], tmp);
					VECADDS(lF[spring->kl], lF[spring->kl], edgeunnormal, spring->restlen * factor);
				}

				search = search->next;
			}
		}

		del_lfvector(winvec);
	}
#endif
		
	// calculate spring forces
	for (LinkNode *link = cloth->springs; link; link = link->next) {
		// only handle active springs
		ClothSpring *spring = (ClothSpring *)link->link;
		if (!(spring->flags & CLOTH_SPRING_FLAG_DEACTIVATE))
			cloth_calc_spring_force(clmd, spring, X, V, time);
	}
	
	// apply spring forces
	for (LinkNode *link = cloth->springs; link; link = link->next) {
		// only handle active springs
		ClothSpring *spring = (ClothSpring *)link->link;
		if (!(spring->flags & CLOTH_SPRING_FLAG_DEACTIVATE))
			cloth_apply_spring_force(clmd, spring, F, dFdX, dFdV);
	}
}

/* Init constraint matrix
 * This is part of the modified CG method suggested by Baraff/Witkin in
 * "Large Steps in Cloth Simulation" (Siggraph 1998)
 */
static void setup_constraint_matrix(ClothModifierData *clmd, ColliderContacts *contacts, int totcolliders, const lVector &V, lMatrix &S, lVector &z, float dt)
{
	ClothVertex *verts = clmd->clothObject->verts;
	int numverts = clmd->clothObject->numverts;
	int i, j, v;

	for (v = 0; v < numverts; v++) {
		if (verts[v].flags & CLOTH_VERT_FLAG_PINNED) {
			/* pinned vertex constraints */
			zero_v3(lVector_v3(z, v)); /* velocity is defined externally */
			lMatrix_copy_m3(S, ZERO, v, v);
		}
		else {
			/* free vertex */
			zero_v3(lVector_v3(z, v));
			lMatrix_copy_m3(S, I, v, v);
		}
	}

#if 0 // TODO
	for (i = 0; i < totcolliders; ++i) {
		ColliderContacts *ct = &contacts[i];
		for (j = 0; j < ct->totcollisions; ++j) {
			CollPair *collpair = &ct->collisions[j];
			int v = collpair->face1;
			float cmat[3][3];
			float impulse[3];
			
			/* pinned verts handled separately */
			if (verts[v].flags & CLOTH_VERT_FLAG_PINNED)
				continue;
			
			/* calculate collision response */
			if (!cloth_points_collpair_response(clmd, ct->collmd, ct->ob->pd, collpair, dt, impulse))
				continue;
			
			add_v3_v3(z[v], impulse);
			
			/* modify S to enforce velocity constraint in normal direction */
			mul_fvectorT_fvector(cmat, collpair->normal, collpair->normal);
			sub_m3_m3m3(S[v].m, I, cmat);
			
			BKE_sim_debug_data_add_dot(clmd->debug_data, collpair->pa, 0, 1, 0, "collision", hash_collpair(936, collpair));
			BKE_sim_debug_data_add_dot(clmd->debug_data, collpair->pb, 1, 0, 0, "collision", hash_collpair(937, collpair));
			BKE_sim_debug_data_add_line(clmd->debug_data, collpair->pa, collpair->pb, 0.7, 0.7, 0.7, "collision", hash_collpair(938, collpair));
			
			{ /* DEBUG */
//				float nor[3];
//				mul_v3_v3fl(nor, collpair->normal, collpair->distance);
//				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, nor, 1, 1, 0, "collision", hash_collpair(939, collpair));
				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, impulse, 1, 1, 0, "collision", hash_collpair(940, collpair));
//				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, collpair->normal, 1, 1, 0, "collision", hash_collpair(941, collpair));
			}
		}
	}
#endif
}

int implicit_solver(Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors)
{
	float step=0.0f, tf=clmd->sim_parms->timescale;
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts/*, *cv*/;
	unsigned int numverts = cloth->numverts;
	float dt = clmd->sim_parms->timescale / clmd->sim_parms->stepsPerFrame;
	float spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;
	Implicit_Data *id = cloth->implicit;
	ColliderContacts *contacts = NULL;
	int totcolliders = 0;
	
	BKE_sim_debug_data_clear_category(clmd->debug_data, "collision");
	
	if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) { /* do goal stuff */
		for (int i = 0; i < numverts; i++) {
			// update velocities with constrained velocities from pinned verts
			if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
				sub_v3_v3v3(lVector_v3(id->V, i), verts[i].xconst, verts[i].xold);
				// mul_v3_fl(id->V[i], clmd->sim_parms->stepsPerFrame);
			}
		}
	}
	
	if (clmd->debug_data) {
		for (int i = 0; i < numverts; i++) {
			BKE_sim_debug_data_add_dot(clmd->debug_data, verts[i].x, 1.0f, 0.1f, 1.0f, "points", hash_vertex(583, i));
		}
	}
	
	while (step < tf) {
		
		/* copy velocities for collision */
		for (int i = 0; i < numverts; i++) {
			copy_v3_v3(verts[i].tv, lVector_v3(id->V, i));
			copy_v3_v3(verts[i].v, verts[i].tv);
		}
		
		/* determine contact points */
		if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
			if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_POINTS) {
				cloth_find_point_contacts(ob, clmd, 0.0f, tf, &contacts, &totcolliders);
			}
		}
		
		/* setup vertex constraints for pinned vertices and contacts */
		setup_constraint_matrix(clmd, contacts, totcolliders, id->V, id->S, id->z, dt);
		
		// damping velocity for artistic reasons
//		mul_lfvectorS(id->V, id->V, clmd->sim_parms->vel_damping, numverts);

		// calculate forces
		cloth_calc_force(clmd, id->F, id->dFdX, id->dFdV, id->X, id->V, id->M, effectors, step);
		
		// calculate new velocity
		simulate_implicit_euler(id, dt);
		
		// advance positions
		id->Xnew = id->X + id->Vnew * dt;
		
		for (int i = 0; i < numverts; i++) {
			/* move pinned verts to correct position */
			if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) {
				if (verts[i].flags & CLOTH_VERT_FLAG_PINNED)
					interp_v3_v3v3(lVector_v3(id->Xnew, i), verts[i].xold, verts[i].xconst, step + dt);
			}
			
			copy_v3_v3(verts[i].txold, lVector_v3(id->X, i));
			
			if (!(verts[i].flags & CLOTH_VERT_FLAG_PINNED) && i > 0) {
				BKE_sim_debug_data_add_line(clmd->debug_data, lVector_v3(id->X, i), lVector_v3(id->X, i-1), 0.6, 0.3, 0.3, "hair", hash_vertex(4892, i));
				BKE_sim_debug_data_add_line(clmd->debug_data, lVector_v3(id->Xnew, i), lVector_v3(id->Xnew, i-1), 1, 0.5, 0.5, "hair", hash_vertex(4893, i));
				BKE_sim_debug_data_add_line(clmd->debug_data, verts[i].xconst, verts[i-1].xconst, 0.25, 0.4, 0.25, "hair", hash_vertex(4873, i));
			}
//			BKE_sim_debug_data_add_vector(clmd->debug_data, id->X[i], id->V[i], 0, 0, 1, "velocity", hash_vertex(3158, i));
		}
		
		/* free contact points */
		if (contacts) {
			cloth_free_contacts(contacts, totcolliders);
		}

		id->X = id->Xnew;
		id->V = id->Vnew;
		
		step += dt;
	}
	
	for (int i = 0; i < numverts; i++) {
		if ((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (verts [i].flags & CLOTH_VERT_FLAG_PINNED)) {
			copy_v3_v3(verts[i].x, verts[i].xconst);
			copy_v3_v3(verts[i].txold, verts[i].x);
			
			copy_v3_v3(verts[i].v, lVector_v3(id->V, i));
		}
		else {
			copy_v3_v3(verts[i].x, lVector_v3(id->X, i));
			copy_v3_v3(verts[i].txold, verts[i].x);
			
			copy_v3_v3(verts[i].v, lVector_v3(id->V, i));
		}
	}
	
	return 1;
}

void implicit_set_positions(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts, i;
	
	lVector &X = cloth->implicit->X;
	lVector &V = cloth->implicit->V;
	
	for (i = 0; i < numverts; i++) {
		copy_v3_v3(lVector_v3(X, i), verts[i].x);
		copy_v3_v3(lVector_v3(V, i), verts[i].v);
	}
}

static void implicit_set_mass(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts;
	
	lMatrix &M = cloth->implicit->M;
	
	lMatrix_reserve_elems(M, 1);
	for (int i = 0; i < numverts; ++i) {
		M.insert(3*i+0, 3*i+0) = verts[i].mass;
		M.insert(3*i+1, 3*i+1) = verts[i].mass;
		M.insert(3*i+2, 3*i+2) = verts[i].mass;
	}
}

int implicit_init(Object *UNUSED(ob), ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	
	cloth->implicit = new Implicit_Data(cloth->numverts);
	
	implicit_set_mass(clmd);
	implicit_set_positions(clmd);
	
#if 0
	// init springs 
	search = cloth->springs;
	for (i = 0; i < cloth->numsprings; i++) {
		spring = search->link;
		
		// dFdV_start[i].r = big_I[i].r = big_zero[i].r = 
		id->A[i+cloth->numverts].r = id->dFdV[i+cloth->numverts].r = id->dFdX[i+cloth->numverts].r = 
				id->P[i+cloth->numverts].r = id->Pinv[i+cloth->numverts].r = id->bigI[i+cloth->numverts].r = id->M[i+cloth->numverts].r = spring->ij;

		// dFdV_start[i].c = big_I[i].c = big_zero[i].c = 
		id->A[i+cloth->numverts].c = id->dFdV[i+cloth->numverts].c = id->dFdX[i+cloth->numverts].c = 
				id->P[i+cloth->numverts].c = id->Pinv[i+cloth->numverts].c = id->bigI[i+cloth->numverts].c = id->M[i+cloth->numverts].c = spring->kl;

		spring->matrix_index = i + cloth->numverts;
		
		search = search->next;
	}
#endif

	return 1;
}

int	implicit_free(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	
	if (cloth && cloth->implicit) {
		delete cloth->implicit;
	}
	
	return 1;
}

/* ================ Volumetric Hair Interaction ================
 * adapted from
 *      Volumetric Methods for Simulation and Rendering of Hair
 *      by Lena Petrovic, Mark Henne and John Anderson
 *      Pixar Technical Memo #06-08, Pixar Animation Studios
 */

/* Note about array indexing:
 * Generally the arrays here are one-dimensional.
 * The relation between 3D indices and the array offset is
 *   offset = x + res_x * y + res_y * z
 */

/* TODO: This is an initial implementation and should be made much better in due time.
 * What should at least be implemented is a grid size parameter and a smoothing kernel
 * for bigger grids.
 */

#if 0
/* 10x10x10 grid gives nice initial results */
static const int hair_grid_res = 10;

static int hair_grid_size(int res)
{
	return res * res * res;
}

BLI_INLINE void hair_grid_get_scale(int res, const float gmin[3], const float gmax[3], float scale[3])
{
	sub_v3_v3v3(scale, gmax, gmin);
	mul_v3_fl(scale, 1.0f / (res-1));
}

typedef struct HairGridVert {
	float velocity[3];
	float density;
} HairGridVert;

#define HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, axis) ( min_ii( max_ii( (int)((vec[axis] - gmin[axis]) / scale[axis]), 0), res-2 ) )

BLI_INLINE int hair_grid_offset(const float vec[3], int res, const float gmin[3], const float scale[3])
{
	int i, j, k;
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	return i + (j + k*res)*res;
}

BLI_INLINE int hair_grid_interp_weights(int res, const float gmin[3], const float scale[3], const float vec[3], float uvw[3])
{
	int i, j, k, offset;
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res)*res;
	
	uvw[0] = (vec[0] - gmin[0]) / scale[0] - (float)i;
	uvw[1] = (vec[1] - gmin[1]) / scale[1] - (float)j;
	uvw[2] = (vec[2] - gmin[2]) / scale[2] - (float)k;
	
	return offset;
}

BLI_INLINE void hair_grid_interpolate(const HairGridVert *grid, int res, const float gmin[3], const float scale[3], const float vec[3],
                                      float *density, float velocity[3], float density_gradient[3])
{
	HairGridVert data[8];
	float uvw[3], muvw[3];
	int res2 = res * res;
	int offset;
	
	offset = hair_grid_interp_weights(res, gmin, scale, vec, uvw);
	muvw[0] = 1.0f - uvw[0];
	muvw[1] = 1.0f - uvw[1];
	muvw[2] = 1.0f - uvw[2];
	
	data[0] = grid[offset           ];
	data[1] = grid[offset         +1];
	data[2] = grid[offset     +res  ];
	data[3] = grid[offset     +res+1];
	data[4] = grid[offset+res2      ];
	data[5] = grid[offset+res2    +1];
	data[6] = grid[offset+res2+res  ];
	data[7] = grid[offset+res2+res+1];
	
	if (density) {
		*density = muvw[2]*( muvw[1]*( muvw[0]*data[0].density + uvw[0]*data[1].density )   +
		                      uvw[1]*( muvw[0]*data[2].density + uvw[0]*data[3].density ) ) +
		            uvw[2]*( muvw[1]*( muvw[0]*data[4].density + uvw[0]*data[5].density )   +
		                      uvw[1]*( muvw[0]*data[6].density + uvw[0]*data[7].density ) );
	}
	if (velocity) {
		int k;
		for (k = 0; k < 3; ++k) {
			velocity[k] = muvw[2]*( muvw[1]*( muvw[0]*data[0].velocity[k] + uvw[0]*data[1].velocity[k] )   +
			                         uvw[1]*( muvw[0]*data[2].velocity[k] + uvw[0]*data[3].velocity[k] ) ) +
			               uvw[2]*( muvw[1]*( muvw[0]*data[4].velocity[k] + uvw[0]*data[5].velocity[k] )   +
			                         uvw[1]*( muvw[0]*data[6].velocity[k] + uvw[0]*data[7].velocity[k] ) );
		}
	}
	if (density_gradient) {
		density_gradient[0] = muvw[1] * muvw[2] * ( data[0].density - data[1].density ) +
		                       uvw[1] * muvw[2] * ( data[2].density - data[3].density ) +
		                      muvw[1] *  uvw[2] * ( data[4].density - data[5].density ) +
		                       uvw[1] *  uvw[2] * ( data[6].density - data[7].density );
		
		density_gradient[1] = muvw[2] * muvw[0] * ( data[0].density - data[2].density ) +
		                       uvw[2] * muvw[0] * ( data[4].density - data[6].density ) +
		                      muvw[2] *  uvw[0] * ( data[1].density - data[3].density ) +
		                       uvw[2] *  uvw[0] * ( data[5].density - data[7].density );
		
		density_gradient[2] = muvw[2] * muvw[0] * ( data[0].density - data[4].density ) +
		                       uvw[2] * muvw[0] * ( data[1].density - data[5].density ) +
		                      muvw[2] *  uvw[0] * ( data[2].density - data[6].density ) +
		                       uvw[2] *  uvw[0] * ( data[3].density - data[7].density );
	}
}

static void hair_velocity_smoothing(const HairGridVert *hairgrid, const float gmin[3], const float scale[3], float smoothfac,
                                    lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int v;
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		float density, velocity[3];
		
		hair_grid_interpolate(hairgrid, hair_grid_res, gmin, scale, lX[v], &density, velocity, NULL);
		
		sub_v3_v3(velocity, lV[v]);
		madd_v3_v3fl(lF[v], velocity, smoothfac);
	}
}

static void hair_velocity_collision(const HairGridVert *collgrid, const float gmin[3], const float scale[3], float collfac,
                                    lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int v;
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		int offset = hair_grid_offset(lX[v], hair_grid_res, gmin, scale);
		
		if (collgrid[offset].density > 0.0f) {
			lF[v][0] += collfac * (collgrid[offset].velocity[0] - lV[v][0]);
			lF[v][1] += collfac * (collgrid[offset].velocity[1] - lV[v][1]);
			lF[v][2] += collfac * (collgrid[offset].velocity[2] - lV[v][2]);
		}
	}
}

static void hair_pressure_force(const HairGridVert *hairgrid, const float gmin[3], const float scale[3], float pressurefac, float minpressure,
                                lfVector *lF, lfVector *lX, unsigned int numverts)
{
	int v;
	
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		float density, gradient[3], gradlen;
		
		hair_grid_interpolate(hairgrid, hair_grid_res, gmin, scale, lX[v], &density, NULL, gradient);
		
		gradlen = normalize_v3(gradient) - minpressure;
		if (gradlen < 0.0f)
			continue;
		mul_v3_fl(gradient, gradlen);
		
		madd_v3_v3fl(lF[v], gradient, pressurefac);
	}
}

static void hair_volume_get_boundbox(lfVector *lX, unsigned int numverts, float gmin[3], float gmax[3])
{
	int i;
	
	INIT_MINMAX(gmin, gmax);
	for (i = 0; i < numverts; i++)
		DO_MINMAX(lX[i], gmin, gmax);
}

BLI_INLINE bool hair_grid_point_valid(const float vec[3], float gmin[3], float gmax[3])
{
	return !(vec[0] < gmin[0] || vec[1] < gmin[1] || vec[2] < gmin[2] ||
	         vec[0] > gmax[0] || vec[1] > gmax[1] || vec[2] > gmax[2]);
}

BLI_INLINE float dist_tent_v3f3(const float a[3], float x, float y, float z)
{
	float w = (1.0f - fabsf(a[0] - x)) * (1.0f - fabsf(a[1] - y)) * (1.0f - fabsf(a[2] - z));
	return w;
}

/* returns the grid array offset as well to avoid redundant calculation */
static int hair_grid_weights(int res, const float gmin[3], const float scale[3], const float vec[3], float weights[8])
{
	int i, j, k, offset;
	float uvw[3];
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res)*res;
	
	uvw[0] = (vec[0] - gmin[0]) / scale[0];
	uvw[1] = (vec[1] - gmin[1]) / scale[1];
	uvw[2] = (vec[2] - gmin[2]) / scale[2];
	
	weights[0] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)k    );
	weights[1] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)k    );
	weights[2] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)k    );
	weights[3] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)k    );
	weights[4] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)(k+1));
	weights[5] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)(k+1));
	weights[6] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)(k+1));
	weights[7] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)(k+1));
	
	return offset;
}

static HairGridVert *hair_volume_create_hair_grid(ClothModifierData *clmd, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int res = hair_grid_res;
	int size = hair_grid_size(res);
	HairGridVert *hairgrid;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float smoothfac = 2.0f * clmd->sim_parms->velocity_smooth;
	unsigned int	v = 0;
	int	            i = 0;

	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(res, gmin, gmax, scale);

	hairgrid = MEM_mallocN(sizeof(HairGridVert) * size, "hair voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(hairgrid[i].velocity);
		hairgrid[i].density = 0.0f;
	}

	/* gather velocities & density */
	if (smoothfac > 0.0f) {
		for (v = 0; v < numverts; v++) {
			float *V = lV[v];
			float weights[8];
			int di, dj, dk;
			int offset;
			
			if (!hair_grid_point_valid(lX[v], gmin, gmax))
				continue;
			
			offset = hair_grid_weights(res, gmin, scale, lX[v], weights);
			
			for (di = 0; di < 2; ++di) {
				for (dj = 0; dj < 2; ++dj) {
					for (dk = 0; dk < 2; ++dk) {
						int voffset = offset + di + (dj + dk*res)*res;
						int iw = di + dj*2 + dk*4;
						
						hairgrid[voffset].density += weights[iw];
						madd_v3_v3fl(hairgrid[voffset].velocity, V, weights[iw]);
					}
				}
			}
		}
	}

	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = hairgrid[i].density;
		if (density > 0.0f)
			mul_v3_fl(hairgrid[i].velocity, 1.0f/density);
	}
	
	return hairgrid;
}


static HairGridVert *hair_volume_create_collision_grid(ClothModifierData *clmd, lfVector *lX, unsigned int numverts)
{
	int res = hair_grid_res;
	int size = hair_grid_size(res);
	HairGridVert *collgrid;
	ListBase *colliders;
	ColliderCache *col = NULL;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float collfac = 2.0f * clmd->sim_parms->collider_friction;
	unsigned int	v = 0;
	int	            i = 0;

	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(res, gmin, gmax, scale);

	collgrid = MEM_mallocN(sizeof(HairGridVert) * size, "hair collider voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(collgrid[i].velocity);
		collgrid[i].density = 0.0f;
	}

	/* gather colliders */
	colliders = get_collider_cache(clmd->scene, NULL, NULL);
	if (colliders && collfac > 0.0f) {
		for (col = colliders->first; col; col = col->next) {
			MVert *loc0 = col->collmd->x;
			MVert *loc1 = col->collmd->xnew;
			float vel[3];
			float weights[8];
			int di, dj, dk;
			
			for (v=0; v < col->collmd->numverts; v++, loc0++, loc1++) {
				int offset;
				
				if (!hair_grid_point_valid(loc1->co, gmin, gmax))
					continue;
				
				offset = hair_grid_weights(res, gmin, scale, lX[v], weights);
				
				sub_v3_v3v3(vel, loc1->co, loc0->co);
				
				for (di = 0; di < 2; ++di) {
					for (dj = 0; dj < 2; ++dj) {
						for (dk = 0; dk < 2; ++dk) {
							int voffset = offset + di + (dj + dk*res)*res;
							int iw = di + dj*2 + dk*4;
							
							collgrid[voffset].density += weights[iw];
							madd_v3_v3fl(collgrid[voffset].velocity, vel, weights[iw]);
						}
					}
				}
			}
		}
	}
	free_collider_cache(&colliders);

	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = collgrid[i].density;
		if (density > 0.0f)
			mul_v3_fl(collgrid[i].velocity, 1.0f/density);
	}
	
	return collgrid;
}

static void hair_volume_forces(ClothModifierData *clmd, lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	HairGridVert *hairgrid, *collgrid;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float smoothfac = 2.0f * clmd->sim_parms->velocity_smooth;
	float collfac = 2.0f * clmd->sim_parms->collider_friction;
	float pressfac = clmd->sim_parms->pressure;
	float minpress = clmd->sim_parms->pressure_threshold;
	
	if (smoothfac <= 0.0f && collfac <= 0.0f && pressfac <= 0.0f)
		return;
	
	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(hair_grid_res, gmin, gmax, scale);
	
	hairgrid = hair_volume_create_hair_grid(clmd, lX, lV, numverts);
	collgrid = hair_volume_create_collision_grid(clmd, lX, numverts);
	
	hair_velocity_smoothing(hairgrid, gmin, scale, smoothfac, lF, lX, lV, numverts);
	hair_velocity_collision(collgrid, gmin, scale, collfac, lF, lX, lV, numverts);
	hair_pressure_force(hairgrid, gmin, scale, pressfac, minpress, lF, lX, numverts);
	
	MEM_freeN(hairgrid);
	MEM_freeN(collgrid);
}
#endif

bool implicit_hair_volume_get_texture_data(Object *UNUSED(ob), ClothModifierData *clmd, ListBase *UNUSED(effectors), VoxelData *vd)
{
#if 0
	lfVector *lX, *lV;
	HairGridVert *hairgrid/*, *collgrid*/;
	int numverts;
	int totres, i;
	int depth;

	if (!clmd->clothObject || !clmd->clothObject->implicit)
		return false;

	lX = clmd->clothObject->implicit->X;
	lV = clmd->clothObject->implicit->V;
	numverts = clmd->clothObject->numverts;

	hairgrid = hair_volume_create_hair_grid(clmd, lX, lV, numverts);
//	collgrid = hair_volume_create_collision_grid(clmd, lX, numverts);

	vd->resol[0] = hair_grid_res;
	vd->resol[1] = hair_grid_res;
	vd->resol[2] = hair_grid_res;
	
	totres = hair_grid_size(hair_grid_res);
	
	if (vd->hair_type == TEX_VD_HAIRVELOCITY) {
		depth = 4;
		vd->data_type = TEX_VD_RGBA_PREMUL;
	}
	else {
		depth = 1;
		vd->data_type = TEX_VD_INTENSITY;
	}
	
	if (totres > 0) {
		vd->dataset = (float *)MEM_mapallocN(sizeof(float) * depth * (totres), "hair volume texture data");
		
		for (i = 0; i < totres; ++i) {
			switch (vd->hair_type) {
				case TEX_VD_HAIRDENSITY:
					vd->dataset[i] = hairgrid[i].density;
					break;
				
				case TEX_VD_HAIRRESTDENSITY:
					vd->dataset[i] = 0.0f; // TODO
					break;
				
				case TEX_VD_HAIRVELOCITY:
					vd->dataset[i + 0*totres] = hairgrid[i].velocity[0];
					vd->dataset[i + 1*totres] = hairgrid[i].velocity[1];
					vd->dataset[i + 2*totres] = hairgrid[i].velocity[2];
					vd->dataset[i + 3*totres] = len_v3(hairgrid[i].velocity);
					break;
				
				case TEX_VD_HAIRENERGY:
					vd->dataset[i] = 0.0f; // TODO
					break;
			}
		}
	}
	else {
		vd->dataset = NULL;
	}
	
	MEM_freeN(hairgrid);
//	MEM_freeN(collgrid);
	
	return true;
#else
	return false; // XXX TODO
#endif
}

/* ================================ */

#endif /* IMPLICIT_SOLVER_EIGEN */
