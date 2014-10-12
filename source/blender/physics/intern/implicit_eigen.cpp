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
 *  \ingroup bph
 */

#include "implicit.h"

#ifdef IMPLICIT_SOLVER_EIGEN

//#define USE_EIGEN_CORE
#define USE_EIGEN_CONSTRAINED_CG

#ifdef __GNUC__
#  pragma GCC diagnostic push
/* XXX suppress verbose warnings in eigen */
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

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

#ifdef __GNUC__
#  pragma GCC diagnostic pop
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

#include "BPH_mass_spring.h"
}

typedef float Scalar;

static float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
static float ZERO[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

/* slightly extended Eigen vector class
 * with conversion to/from plain C float array
 */
class fVector : public Eigen::Vector3f {
public:
	typedef float *ctype;
	
	fVector()
	{
	}
	
	fVector(const ctype &v)
	{
		for (int k = 0; k < 3; ++k)
			coeffRef(k) = v[k];
	}
	
	fVector &operator = (const ctype &v)
	{
		for (int k = 0; k < 3; ++k)
			coeffRef(k) = v[k];
		return *this;
	}
	
	operator ctype()
	{
		return data();
	}
};

/* slightly extended Eigen matrix class
 * with conversion to/from plain C float array
 */
class fMatrix : public Eigen::Matrix3f {
public:
	typedef float (*ctype)[3];
	
	fMatrix()
	{
	}
	
	fMatrix(const ctype &v)
	{
		for (int k = 0; k < 3; ++k)
			for (int l = 0; l < 3; ++l)
				coeffRef(l, k) = v[k][l];
	}
	
	fMatrix &operator = (const ctype &v)
	{
		for (int k = 0; k < 3; ++k)
			for (int l = 0; l < 3; ++l)
				coeffRef(l, k) = v[k][l];
		return *this;
	}
	
	operator ctype()
	{
		return (ctype)data();
	}
};

typedef Eigen::VectorXf lVector;

typedef Eigen::Triplet<Scalar> Triplet;
typedef std::vector<Triplet> TripletList;

typedef Eigen::SparseMatrix<Scalar> lMatrix;

struct lMatrixCtor {
	lMatrixCtor(int numverts) :
	    m_numverts(numverts)
	{
		/* reserve for diagonal entries */
		m_trips.reserve(numverts * 9);
	}
	
	int numverts() const { return m_numverts; }
	
	void set(int i, int j, const fMatrix &m)
	{
		BLI_assert(i >= 0 && i < m_numverts);
		BLI_assert(j >= 0 && j < m_numverts);
		i *= 3;
		j *= 3;
		for (int k = 0; k < 3; ++k)
			for (int l = 0; l < 3; ++l)
				m_trips.push_back(Triplet(i + k, j + l, m.coeff(l, k)));
	}
	
	inline lMatrix construct() const
	{
		lMatrix m(m_numverts, m_numverts);
		m.setFromTriplets(m_trips.begin(), m_trips.end());
		return m;
	}
	
private:
	const int m_numverts;
	TripletList m_trips;
};

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

BLI_INLINE void outerproduct(float r[3][3], const float a[3], const float b[3])
{
	mul_v3_v3fl(r[0], a, b[0]);
	mul_v3_v3fl(r[1], a, b[1]);
	mul_v3_v3fl(r[2], a, b[2]);
}

struct Implicit_Data {
	typedef std::vector<fMatrix> fMatrixVector;
	
	Implicit_Data(int numverts) :
	    M(numverts)
	{
		resize(numverts);
	}
	
	void resize(int numverts)
	{
		this->numverts = numverts;
		int tot = 3 * numverts;
		
		dFdV.resize(tot, tot);
		dFdX.resize(tot, tot);
		
		tfm.resize(numverts, I);
		
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
	lMatrixCtor M;				/* masses */
	lVector F;					/* forces */
	lMatrix dFdV, dFdX;			/* force jacobians */
	
	fMatrixVector tfm;			/* local coordinate transform */
	
	/* motion state data */
	lVector X, Xnew;			/* positions */
	lVector V, Vnew;			/* velocities */
	
	/* internal solver data */
	lVector B;					/* B for A*dV = B */
	lMatrix A;					/* A for A*dV = B */
	
	lVector dV;					/* velocity change (solution of A*dV = B) */
	lVector z;					/* target velocity in constrained directions */
	lMatrix S;					/* filtering matrix for constraints */
	
	struct SimDebugData *debug_data;
};

/* ==== Transformation from/to root reference frames ==== */

BLI_INLINE void world_to_root_v3(Implicit_Data *data, int index, float r[3], const float v[3])
{
	copy_v3_v3(r, v);
	mul_transposed_m3_v3(data->tfm[index], r);
}

BLI_INLINE void root_to_world_v3(Implicit_Data *data, int index, float r[3], const float v[3])
{
	mul_v3_m3v3(r, data->tfm[index], v);
}

BLI_INLINE void world_to_root_m3(Implicit_Data *data, int index, float r[3][3], float m[3][3])
{
	float trot[3][3];
	copy_m3_m3(trot, data->tfm[index]);
	transpose_m3(trot);
	mul_m3_m3m3(r, trot, m);
}

BLI_INLINE void root_to_world_m3(Implicit_Data *data, int index, float r[3][3], float m[3][3])
{
	mul_m3_m3m3(r, data->tfm[index], m);
}

/* ================================ */

bool BPH_mass_spring_solve(Implicit_Data *data, float dt, ImplicitSolverResult *result)
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
	
	lMatrix M = data->M.construct();
	data->A = M - dt * data->dFdV - dt*dt * data->dFdX;
	cg.compute(data->A);
	cg.filter() = data->S;
	
	data->B = dt * data->F + dt*dt * data->dFdX * data->V;
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
	data->dV = cg.solveWithGuess(data->B, data->z);
#ifdef IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT
	printf("==== dV ====\n");
	print_lvector(id->dV);
	printf("========\n");
#endif
	
	data->Vnew = data->V + data->dV;
	data->Xnew = data->X + data->Vnew * dt;
	
	switch (cg.info()) {
		case Eigen::Success:        result->status = BPH_SOLVER_SUCCESS;         break;
		case Eigen::NoConvergence:  result->status = BPH_SOLVER_NO_CONVERGENCE;  break;
		case Eigen::InvalidInput:   result->status = BPH_SOLVER_INVALID_INPUT;   break;
		case Eigen::NumericalIssue: result->status = BPH_SOLVER_NUMERICAL_ISSUE; break;
	}

	result->iterations = cg.iterations();
	result->error = cg.error();
	
	return cg.info() != Eigen::Success;
#endif
}

/* ================================ */

void BPH_mass_spring_apply_result(Implicit_Data *data)
{
	data->X = data->Xnew;
	data->V = data->Vnew;
}

void BPH_mass_spring_set_rest_transform(Implicit_Data *data, int index, float tfm[3][3])
{
#ifdef CLOTH_ROOT_FRAME
	copy_m3_m3(data->tfm[index], tfm);
#else
	unit_m3(data->tfm[index]);
	(void)tfm;
#endif
}

void BPH_mass_spring_set_motion_state(Implicit_Data *data, int index, const float x[3], const float v[3])
{
	world_to_root_v3(data, index, lVector_v3(data->X, index), x);
	world_to_root_v3(data, index, lVector_v3(data->V, index), v);
}

void BPH_mass_spring_set_position(Implicit_Data *data, int index, const float x[3])
{
	world_to_root_v3(data, index, lVector_v3(data->X, index), x);
}

void BPH_mass_spring_set_velocity(Implicit_Data *data, int index, const float v[3])
{
	world_to_root_v3(data, index, lVector_v3(data->V, index), v);
}

void BPH_mass_spring_get_motion_state(struct Implicit_Data *data, int index, float x[3], float v[3])
{
	if (x) root_to_world_v3(data, index, x, lVector_v3(data->X, index));
	if (v) root_to_world_v3(data, index, v, lVector_v3(data->V, index));
}

void BPH_mass_spring_set_vertex_mass(Implicit_Data *data, int index, float mass)
{
	float m[3][3];
	copy_m3_m3(m, I);
	mul_m3_fl(m, mass);
	data->M.set(index, index, m);
}

#endif /* IMPLICIT_SOLVER_EIGEN */
