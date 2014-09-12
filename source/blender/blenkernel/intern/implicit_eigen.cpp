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

#include "implicit.h"

#ifdef IMPLICIT_SOLVER_EIGEN

//#define USE_EIGEN_CORE
#define USE_EIGEN_CONSTRAINED_CG

#ifndef IMPLICIT_ENABLE_EIGEN_DEBUG
#ifdef NDEBUG
#define IMPLICIT_NDEBUG
#endif
#define NDEBUG
#endif

#include <Eigen/Sparse>
#include <Eigen/src/Core/util/DisableStupidWarnings.h>

#ifdef USE_EIGEN_CONSTRAINED_CG
#include <intern/ConstrainedConjugateGradient.h>
#endif

#ifndef IMPLICIT_ENABLE_EIGEN_DEBUG
#ifndef IMPLICIT_NDEBUG
#undef NDEBUG
#else
#undef IMPLICIT_NDEBUG
#endif
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
		if (i > 0 && i % 3 == 0)
			printf("\n");
		
		printf("%f,\n", v[i]);
	}
}

static void print_lmatrix(const lMatrix &m)
{
	for (int j = 0; j < m.rows(); ++j) {
		if (j > 0 && j % 3 == 0)
			printf("\n");
		
		for (int i = 0; i < m.cols(); ++i) {
			if (i > 0 && i % 3 == 0)
				printf("  ");
			
			implicit_print_matrix_elem(m.coeff(j, i));
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

BLI_INLINE void triplets_m3(TripletList &tlist, float m[3][3], int i, int j)
{
	i *= 3;
	j *= 3;
	for (int l = 0; l < 3; ++l) {
		for (int k = 0; k < 3; ++k) {
			tlist.push_back(Triplet(i + k, j + l, m[k][l]));
		}
	}
}

BLI_INLINE void triplets_m3fl(TripletList &tlist, float m[3][3], int i, int j, float factor)
{
	i *= 3;
	j *= 3;
	for (int l = 0; l < 3; ++l) {
		for (int k = 0; k < 3; ++k) {
			tlist.push_back(Triplet(i + k, j + l, m[k][l] * factor));
		}
	}
}

BLI_INLINE void lMatrix_add_triplets(lMatrix &r, const TripletList &tlist)
{
	lMatrix t(r.rows(), r.cols());
	t.setFromTriplets(tlist.begin(), tlist.end());
	r += t;
}

BLI_INLINE void lMatrix_madd_triplets(lMatrix &r, const TripletList &tlist, float f)
{
	lMatrix t(r.rows(), r.cols());
	t.setFromTriplets(tlist.begin(), tlist.end());
	r += f * t;
}

BLI_INLINE void lMatrix_sub_triplets(lMatrix &r, const TripletList &tlist)
{
	lMatrix t(r.rows(), r.cols());
	t.setFromTriplets(tlist.begin(), tlist.end());
	r -= t;
}

#if 0
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
	lMatrix_copy_m3(tmp, m, i, j);
	r += tmp;
}

BLI_INLINE void lMatrix_sub_m3(lMatrix &r, float m[3][3], int i, int j)
{
	lMatrix tmp(r.cols(), r.cols());
	lMatrix_copy_m3(tmp, m, i, j);
	r -= tmp;
}

BLI_INLINE void lMatrix_madd_m3(lMatrix &r, float m[3][3], float s, int i, int j)
{
	lMatrix tmp(r.cols(), r.cols());
	lMatrix_copy_m3(tmp, m, i, j);
	r += s * tmp;
}
#endif

BLI_INLINE void outerproduct(float r[3][3], const float a[3], const float b[3])
{
	mul_v3_v3fl(r[0], a, b[0]);
	mul_v3_v3fl(r[1], a, b[1]);
	mul_v3_v3fl(r[2], a, b[2]);
}

struct RootTransform {
	float loc[3];
	float rot[3][3];
	
	float vel[3];
	float omega[3];
	
	float acc[3];
	float domega_dt[3];
};

struct Implicit_Data {
	typedef std::vector<RootTransform> RootTransforms;
	
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
		
		root.resize(numverts);
		
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
	lVector F;					/* forces */
	lMatrix dFdV, dFdX;			/* force jacobians */
	
	RootTransforms root;		/* root transforms */
	
	/* motion state data */
	lVector X, Xnew;			/* positions */
	lVector V, Vnew;			/* velocities */
	
	/* internal solver data */
	lVector B;					/* B for A*dV = B */
	lMatrix A;					/* A for A*dV = B */
	
	lVector dV;					/* velocity change (solution of A*dV = B) */
	lVector z;					/* target velocity in constrained directions */
	lMatrix S;					/* filtering matrix for constraints */
};

/* ==== Transformation of Moving Reference Frame ====
 *   x_world, v_world, f_world, a_world, dfdx_world, dfdv_world : state variables in world space
 *   x_root, v_root, f_root, a_root, dfdx_root, dfdv_root       : state variables in root space
 *   
 *   x0 : translation of the root frame (hair root location)
 *   v0 : linear velocity of the root frame
 *   a0 : acceleration of the root frame
 *   R : rotation matrix of the root frame
 *   w : angular velocity of the root frame
 *   dwdt : angular acceleration of the root frame
 */

/* x_root = R^T * x_world */
BLI_INLINE void loc_world_to_root(float r[3], const float v[3], const RootTransform &root)
{
	sub_v3_v3v3(r, v, root.loc);
	mul_transposed_m3_v3((float (*)[3])root.rot, r);
}

/* x_world = R * x_root */
BLI_INLINE void loc_root_to_world(float r[3], const float v[3], const RootTransform &root)
{
	copy_v3_v3(r, v);
	mul_m3_v3((float (*)[3])root.rot, r);
	add_v3_v3(r, root.loc);
}

/* v_root = cross(w, x_root) + R^T*(v_world - v0) */
BLI_INLINE void vel_world_to_root(float r[3], const float x_root[3], const float v[3], const RootTransform &root)
{
	float angvel[3];
	cross_v3_v3v3(angvel, root.omega, x_root);
	
	sub_v3_v3v3(r, v, root.vel);
	mul_transposed_m3_v3((float (*)[3])root.rot, r);
	add_v3_v3(r, angvel);
}

/* v_world = R*(v_root - cross(w, x_root)) + v0 */
BLI_INLINE void vel_root_to_world(float r[3], const float x_root[3], const float v[3], const RootTransform &root)
{
	float angvel[3];
	cross_v3_v3v3(angvel, root.omega, x_root);
	
	sub_v3_v3v3(r, v, angvel);
	mul_m3_v3((float (*)[3])root.rot, r);
	add_v3_v3(r, root.vel);
}

/* a_root = -cross(dwdt, x_root) - 2*cross(w, v_root) - cross(w, cross(w, x_root)) + R^T*(a_world - a0) */
BLI_INLINE void force_world_to_root(float r[3], const float x_root[3], const float v_root[3], const float force[3], float mass, const RootTransform &root)
{
	float euler[3], coriolis[3], centrifugal[3], rotvel[3];
	
	cross_v3_v3v3(euler, root.domega_dt, x_root);
	cross_v3_v3v3(coriolis, root.omega, v_root);
	mul_v3_fl(coriolis, 2.0f);
	cross_v3_v3v3(rotvel, root.omega, x_root);
	cross_v3_v3v3(centrifugal, root.omega, rotvel);
	
	madd_v3_v3v3fl(r, force, root.acc, mass);
	mul_transposed_m3_v3((float (*)[3])root.rot, r);
	madd_v3_v3fl(r, euler, mass);
	madd_v3_v3fl(r, coriolis, mass);
	madd_v3_v3fl(r, centrifugal, mass);
}

/* a_world = R*[ a_root + cross(dwdt, x_root) + 2*cross(w, v_root) + cross(w, cross(w, x_root)) ] + a0 */
BLI_INLINE void force_root_to_world(float r[3], const float x_root[3], const float v_root[3], const float force[3], float mass, const RootTransform &root)
{
	float euler[3], coriolis[3], centrifugal[3], rotvel[3];
	
	cross_v3_v3v3(euler, root.domega_dt, x_root);
	cross_v3_v3v3(coriolis, root.omega, v_root);
	mul_v3_fl(coriolis, 2.0f);
	cross_v3_v3v3(rotvel, root.omega, x_root);
	cross_v3_v3v3(centrifugal, root.omega, rotvel);
	
	madd_v3_v3v3fl(r, force, euler, mass);
	madd_v3_v3fl(r, coriolis, mass);
	madd_v3_v3fl(r, centrifugal, mass);
	mul_m3_v3((float (*)[3])root.rot, r);
	madd_v3_v3fl(r, root.acc, mass);
}

BLI_INLINE void acc_world_to_root(float r[3], const float x_root[3], const float v_root[3], const float acc[3], const RootTransform &root)
{
	force_world_to_root(r, x_root, v_root, acc, 1.0f, root);
}

BLI_INLINE void acc_root_to_world(float r[3], const float x_root[3], const float v_root[3], const float acc[3], const RootTransform &root)
{
	force_root_to_world(r, x_root, v_root, acc, 1.0f, root);
}

BLI_INLINE void cross_m3_v3m3(float r[3][3], const float v[3], float m[3][3])
{
	cross_v3_v3v3(r[0], v, m[0]);
	cross_v3_v3v3(r[1], v, m[1]);
	cross_v3_v3v3(r[2], v, m[2]);
}

BLI_INLINE void cross_v3_identity(float r[3][3], const float v[3])
{
	r[0][0] = 0.0f;		r[1][0] = v[2];		r[2][0] = -v[1];
	r[0][1] = -v[2];	r[1][1] = 0.0f;		r[2][1] = v[0];
	r[0][2] = v[1];		r[1][2] = -v[0];	r[2][2] = 0.0f;
}

/* dfdx_root = m*[ -cross(dwdt, I) - cross(w, cross(w, I)) ] + R^T*(dfdx_world) */
BLI_INLINE void dfdx_world_to_root(float m[3][3], float dfdx[3][3], float mass, const RootTransform &root)
{
	float t[3][3], u[3][3];
	
	copy_m3_m3(t, (float (*)[3])root.rot);
	transpose_m3(t);
	mul_m3_m3m3(m, t, dfdx);
	
	cross_v3_identity(t, root.domega_dt);
	mul_m3_fl(t, mass);
	sub_m3_m3m3(m, m, t);
	
	cross_v3_identity(u, root.omega);
	cross_m3_v3m3(t, root.omega, u);
	mul_m3_fl(t, mass);
	sub_m3_m3m3(m, m, t);
}

/* dfdx_world = R*(dfdx_root + m*[ cross(dwdt, I) + cross(w, cross(w, I)) ]) */
BLI_INLINE void dfdx_root_to_world(float m[3][3], float dfdx[3][3], float mass, const RootTransform &root)
{
	float t[3][3];
	
	cross_v3_identity(t, root.domega_dt);
	mul_m3_fl(t, mass);
	add_m3_m3m3(m, dfdx, t);
	
	cross_v3_identity(u, root.omega);
	cross_m3_v3m3(t, root.omega, u);
	mul_m3_fl(t, mass);
	add_m3_m3m3(m, m, t);
	
	mul_m3_m3m3(m, (float (*)[3])root.rot, m);
}

/* dfdv_root = -2*m*cross(w, I) + R^T*(dfdv_world) */
BLI_INLINE void dfdv_world_to_root(float m[3][3], float dfdv[3][3], float mass, const RootTransform &root)
{
	float t[3][3];
	
	copy_m3_m3(t, (float (*)[3])root.rot);
	transpose_m3(t);
	mul_m3_m3m3(m, t, dfdv);
	
	cross_v3_identity(t, root.omega);
	mul_m3_fl(t, 2.0f*mass);
	sub_m3_m3m3(m, m, t);
}

/* dfdv_world = R*(dfdv_root + 2*m*cross(w, I)) */
BLI_INLINE void dfdv_root_to_world(float m[3][3], float dfdv[3][3], float mass, const RootTransform &root)
{
	float t[3][3];
	
	cross_v3_identity(t, root.omega);
	mul_m3_fl(t, 2.0f*mass);
	add_m3_m3m3(m, dfdv, t);
	
	mul_m3_m3m3(m, (float (*)[3])root.rot, m);
}

/* ================================ */

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
#ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
	printf("==== A ====\n");
	print_lmatrix(id->A);
	printf("==== z ====\n");
	print_lvector(id->z);
	printf("==== B ====\n");
	print_lvector(id->B);
	printf("==== S ====\n");
	print_lmatrix(id->S);
#endif
	id->dV = cg.solveWithGuess(id->B, id->z);
#ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
	printf("==== dV ====\n");
	print_lvector(id->dV);
	printf("========\n");
#endif
	
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
	mul_m3_fl(to, k);
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

BLI_INLINE void dfdv_damp(float to[3][3], const float dir[3], float damping)
{
	// derivative of force wrt velocity
	outerproduct(to, dir, dir);
	mul_m3_fl(to, -damping);
}

BLI_INLINE float fb(float length, float L)
{
	float x = length / L;
	return (-11.541f * powf(x, 4) + 34.193f * powf(x, 3) - 39.083f * powf(x, 2) + 23.116f * x - 9.713f);
}

BLI_INLINE float fbderiv(float length, float L)
{
	float x = length/L;

	return (-46.164f * powf(x, 3) + 102.579f * powf(x, 2) - 78.166f * x + 23.116f);
}

BLI_INLINE float fbstar(float length, float L, float kb, float cb)
{
	float tempfb_fl = kb * fb(length, L);
	float fbstar_fl = cb * (length - L);
	
	if (tempfb_fl < fbstar_fl)
		return fbstar_fl;
	else
		return tempfb_fl;
}

// function to calculae bending spring force (taken from Choi & Co)
BLI_INLINE float fbstar_jacobi(float length, float L, float kb, float cb)
{
	float tempfb_fl = kb * fb(length, L);
	float fbstar_fl = cb * (length - L);

	if (tempfb_fl < fbstar_fl) {
		return cb;
	}
	else {
		return kb * fbderiv(length, L);
	}
}

static void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s, const lVector &X, const lVector &V, float time)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	ClothVertex *v1 = &verts[s->ij]/*, *v2 = &verts[s->kl]*/;
	float extent[3];
	float length = 0, dot = 0;
	float dir[3] = {0, 0, 0};
	float vel[3];
	float k = 0.0f;
	float L = s->restlen;
	float cb; /* = clmd->sim_parms->structural; */ /*UNUSED*/
	
	float scaling = 0.0;
	
	int no_compress = clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
	
	zero_v3(s->f);
	zero_m3(s->dfdx);
	zero_m3(s->dfdv);
	
	s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;
	
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
#ifdef CLOTH_FORCE_SPRING_STRUCTURAL
		if (length > L || no_compress) {
			float stretch_force[3] = {0, 0, 0};
			
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
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_GOAL) {
#ifdef CLOTH_FORCE_SPRING_GOAL
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
#endif
	}
	else {  /* calculate force of bending springs */
#ifdef CLOTH_FORCE_SPRING_BEND
		if (length < L) {
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;
			
			k = clmd->sim_parms->bending;
			
			scaling = k + s->stiffness * fabsf(clmd->sim_parms->max_bend - k);
			cb = k = scaling / (20.0f * (clmd->sim_parms->avg_spring_len + FLT_EPSILON));
			
			madd_v3_v3fl(s->f, dir, fbstar(length, L, k, cb));
			
			outerproduct(s->dfdx, dir, dir);
			mul_m3_fl(s->dfdx, fbstar_jacobi(length, L, k, cb));
		}
#endif
	}
}

static void cloth_apply_spring_force(ClothModifierData *clmd, ClothSpring *s, lVector &F, TripletList &tlist_dFdX, TripletList &tlist_dFdV)
{
	/* XXX reserve elements in tmp? */
	
	/* ignore disabled springs */
	if (!(s->flags & CLOTH_SPRING_FLAG_NEEDED))
		return;
	
	if (!(s->type & CLOTH_SPRING_TYPE_BENDING)) {
		triplets_m3fl(tlist_dFdV, s->dfdv, s->ij, s->ij, 1.0f);
		triplets_m3fl(tlist_dFdV, s->dfdv, s->kl, s->kl, 1.0f);
		triplets_m3fl(tlist_dFdV, s->dfdv, s->ij, s->kl, -1.0f);
		triplets_m3fl(tlist_dFdV, s->dfdv, s->kl, s->ij, -1.0f);
	}
	
	add_v3_v3(lVector_v3(F, s->ij), s->f);
	if (!(s->type & CLOTH_SPRING_TYPE_GOAL)) {
		sub_v3_v3(lVector_v3(F, s->kl), s->f);
	}
	
	triplets_m3fl(tlist_dFdX, s->dfdx, s->ij, s->ij, 1.0f);
	triplets_m3fl(tlist_dFdX, s->dfdx, s->kl, s->kl, 1.0f);
	triplets_m3fl(tlist_dFdX, s->dfdx, s->ij, s->kl, -1.0f);
	triplets_m3fl(tlist_dFdX, s->dfdx, s->kl, s->ij, -1.0f);
}

static float calc_nor_area_tri(float nor[3], const float v1[3], const float v2[3], const float v3[3])
{
	float n1[3], n2[3];
	
	sub_v3_v3v3(n1, v1, v2);
	sub_v3_v3v3(n2, v2, v3);
	
	cross_v3_v3v3(nor, n1, n2);
	return normalize_v3(nor);
}

static float calc_nor_area_quad(float nor[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float n1[3], n2[3];
	
	sub_v3_v3v3(n1, v1, v3);
	sub_v3_v3v3(n2, v2, v4);
	
	cross_v3_v3v3(nor, n1, n2);
	return normalize_v3(nor);
}

static void cloth_calc_force(ClothModifierData *clmd, lVector &F, lMatrix &dFdX, lMatrix &dFdV, const lVector &X, const lVector &V, const lMatrix &M, ListBase *effectors, float time)
{
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *id = cloth->implicit;
	unsigned int numverts = cloth->numverts;
	ClothVertex *verts = cloth->verts;
	float drag = clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float gravity[3] = {0,0,0};
	float f[3], dfdx[3][3], dfdv[3][3];
	
	F.setZero();
	dFdX.setZero();
	dFdV.setZero();
	
	TripletList tlist_dFdV, tlist_dFdX;
	
#ifdef CLOTH_FORCE_GRAVITY
	/* global acceleration (gravitation) */
	if (clmd->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		/* scale gravity force
		 * XXX 0.001 factor looks totally arbitrary ... what is this? lukas_t
		 */
		mul_v3_v3fl(gravity, clmd->scene->physics_settings.gravity, 0.001f * clmd->sim_parms->effector_weights->global_gravity);
	}
	for (int i = 0; i < numverts; ++i) {
		float acc[3];
		/* gravitational mass same as inertial mass */
		acc_world_to_root(acc, lVector_v3(X, i), lVector_v3(V, i), gravity, id->root[i]);
		madd_v3_v3fl(lVector_v3(F, i), acc, verts[i].mass);
	}
#endif
	
#ifdef CLOTH_FORCE_DRAG
	/* air drag */
	for (int i = 0; i < numverts; ++i) {
#if 1
		/* NB: uses root space velocity, no need to transform */
		mul_v3_v3fl(f, lVector_v3(V, i), -drag);
		add_v3_v3(lVector_v3(F, i), f);
		
		triplets_m3fl(tlist_dFdV, I, i, i, -drag);
#else
		float drag_dfdv[3][3], t[3];
		
		mul_v3_v3fl(f, lVector_v3(V, i), -drag);
		force_world_to_root(t, lVector_v3(X, i), lVector_v3(V, i), f, verts[i].mass, id->root[i]);
		add_v3_v3(lVector_v3(F, i), t);
		
		copy_m3_m3(drag_dfdv, I);
		mul_m3_fl(drag_dfdv, -drag);
		dfdv_world_to_root(dfdv, drag_dfdv, verts[i].mass, id->root[i]);
		triplets_m3(tlist_dFdV, dfdv, i, i);
#endif
	}
#endif

//	hair_volume_forces(clmd, lF, lX, lV, numverts);

#ifdef CLOTH_FORCE_EFFECTORS
	/* handle external forces like wind */
	if (effectors) {
		const float effector_scale = 0.02f;
		MFace *mfaces = cloth->mfaces;
		EffectedPoint epoint;
		lVector winvec(F.rows());
		winvec.setZero();
		
		// precalculate wind forces
		for (int i = 0; i < cloth->numverts; i++) {
			pd_point_from_loc(clmd->scene, (float*)lVector_v3(X, i), (float*)lVector_v3(V, i), i, &epoint);
			pdDoEffectors(effectors, NULL, clmd->sim_parms->effector_weights, &epoint, lVector_v3(winvec, i), NULL);
		}
		
		for (int i = 0; i < cloth->numfaces; i++) {
			float nor[3], area;
			float factor;
			MFace *mf = &mfaces[i];
			
			// calculate face normal and area
			if (mf->v4) {
				area = calc_nor_area_quad(nor, lVector_v3(X, mf->v1), lVector_v3(X, mf->v2), lVector_v3(X, mf->v3), lVector_v3(X, mf->v4));
				factor = effector_scale * area * 0.25f;
			}
			else {
				area = calc_nor_area_tri(nor, lVector_v3(X, mf->v1), lVector_v3(X, mf->v2), lVector_v3(X, mf->v3));
				factor = effector_scale * area / 3.0f;
			}
			
			madd_v3_v3fl(lVector_v3(F, mf->v1), nor, factor * dot_v3v3(lVector_v3(winvec, mf->v1), nor));
			madd_v3_v3fl(lVector_v3(F, mf->v2), nor, factor * dot_v3v3(lVector_v3(winvec, mf->v2), nor));
			madd_v3_v3fl(lVector_v3(F, mf->v3), nor, factor * dot_v3v3(lVector_v3(winvec, mf->v3), nor));
			if (mf->v4)
				madd_v3_v3fl(lVector_v3(F, mf->v4), nor, factor * dot_v3v3(lVector_v3(winvec, mf->v4), nor));
		}

		/* Hair has only edges */
		if (cloth->numfaces == 0) {
			ClothSpring *spring;
			float dir[3], length;
			float factor = 0.01;
			
			for (LinkNode *link = cloth->springs; link; link = link->next) {
				spring = (ClothSpring *)link->link;
				
				/* structural springs represent hair strands,
				 * their length signifies surface area and mass
				 */
				if (spring->type != CLOTH_SPRING_TYPE_STRUCTURAL)
					continue;
				
				float *win_ij = lVector_v3(winvec, spring->ij);
				float *win_kl = lVector_v3(winvec, spring->kl);
				float win_ortho[3];
				
				sub_v3_v3v3(dir, (float*)lVector_v3(X, spring->ij), (float*)lVector_v3(X, spring->kl));
				length = normalize_v3(dir);
				
				madd_v3_v3v3fl(win_ortho, win_ij, dir, -dot_v3v3(win_ij, dir));
				madd_v3_v3fl(lVector_v3(F, spring->ij), win_ortho, factor * length);
				
				madd_v3_v3v3fl(win_ortho, win_kl, dir, -dot_v3v3(win_kl, dir));
				madd_v3_v3fl(lVector_v3(F, spring->kl), win_ortho, factor * length);
			}
		}
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
			cloth_apply_spring_force(clmd, spring, F, tlist_dFdX, tlist_dFdV);
	}
	
	lMatrix_add_triplets(dFdV, tlist_dFdV);
	lMatrix_add_triplets(dFdX, tlist_dFdX);
}

/* Init constraint matrix
 * This is part of the modified CG method suggested by Baraff/Witkin in
 * "Large Steps in Cloth Simulation" (Siggraph 1998)
 */
static void setup_constraint_matrix(ClothModifierData *clmd, ColliderContacts *contacts, int totcolliders, const lVector &V, lMatrix &S, lVector &z, float dt)
{
	ClothVertex *verts = clmd->clothObject->verts;
	int numverts = clmd->clothObject->numverts;
	TripletList tlist_sub;
	int i, j, v;
	
	S.setIdentity();
	z.setZero();
	
	for (v = 0; v < numverts; v++) {
		if (verts[v].flags & CLOTH_VERT_FLAG_PINNED) {
			/* pinned vertex constraints */
			zero_v3(lVector_v3(z, v)); /* velocity is defined externally */
			triplets_m3(tlist_sub, I, v, v);
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
	
	lMatrix_sub_triplets(S, tlist_sub);
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
				float v[3];
				sub_v3_v3v3(v, verts[i].xconst, verts[i].xold);
				// mul_v3_fl(id->V[i], clmd->sim_parms->stepsPerFrame);
				/* note: should be zero for root vertices, but other verts could be pinned as well */
				vel_world_to_root(lVector_v3(id->V, i), lVector_v3(id->X, i), v, id->root[i]);
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
			vel_root_to_world(verts[i].tv, lVector_v3(id->X, i), lVector_v3(id->V, i), id->root[i]);
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
				if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
					float x[3];
					interp_v3_v3v3(x, verts[i].xold, verts[i].xconst, step + dt);
					loc_world_to_root(lVector_v3(id->Xnew, i), x, id->root[i]);
				}
			}
			
			loc_root_to_world(verts[i].txold, lVector_v3(id->X, i), id->root[i]);
			
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
			
			vel_root_to_world(verts[i].v, lVector_v3(id->X, i), lVector_v3(id->V, i), id->root[i]);
		}
		else {
			loc_root_to_world(verts[i].x, lVector_v3(id->X, i), id->root[i]);
			copy_v3_v3(verts[i].txold, verts[i].x);
			
			vel_root_to_world(verts[i].v, lVector_v3(id->X, i), lVector_v3(id->V, i), id->root[i]);
		}
	}
	
	return 1;
}

void implicit_set_positions(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	ClothHairRoot *cloth_roots = clmd->roots;
	unsigned int numverts = cloth->numverts, i;
	
	Implicit_Data::RootTransforms &root = cloth->implicit->root;
	lVector &X = cloth->implicit->X;
	lVector &V = cloth->implicit->V;
	
	for (i = 0; i < numverts; i++) {
		copy_v3_v3(root[i].loc, cloth_roots[i].loc);
		copy_m3_m3(root[i].rot, cloth_roots[i].rot);
		
		loc_world_to_root(lVector_v3(X, i), verts[i].x, root[i]);
		vel_world_to_root(lVector_v3(V, i), lVector_v3(X, i), verts[i].v, root[i]);
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
