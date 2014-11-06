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

#ifndef __EIGEN_UTILS_H__
#define __EIGEN_UTILS_H__

/** \file eigen_utils.h
 *  \ingroup bph
 */

#ifdef __GNUC__
#  pragma GCC diagnostic push
/* XXX suppress verbose warnings in eigen */
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#include <Eigen/Sparse>
#include <Eigen/src/Core/util/DisableStupidWarnings.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#include "BLI_utildefines.h"


typedef float Scalar;

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
	
	fVector& operator = (const ctype &v)
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
	
	fMatrix& operator = (const ctype &v)
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

/* Extension of dense Eigen vectors,
 * providing 3-float block access for blenlib math functions
 */
class lVector : public Eigen::VectorXf {
public:
	typedef Eigen::VectorXf base_t;
	
	lVector()
	{
	}
	
	template <typename T>
	lVector& operator = (T rhs)
	{
		base_t::operator=(rhs);
		return *this;
	}
	
	float* v3(int vertex)
	{
		return &coeffRef(3 * vertex);
	}
	
	const float* v3(int vertex) const
	{
		return &coeffRef(3 * vertex);
	}
};

typedef Eigen::Triplet<Scalar> Triplet;
typedef std::vector<Triplet> TripletList;

typedef Eigen::SparseMatrix<Scalar> lMatrix;

/* Constructor type that provides more convenient handling of Eigen triplets
 * for efficient construction of sparse 3x3 block matrices.
 * This should be used for building lMatrix instead of writing to such lMatrix directly (which is very inefficient).
 * After all elements have been defined using the set() method, the actual matrix can be filled using construct().
 */
struct lMatrixCtor {
	lMatrixCtor()
	{
	}
	
	void reset()
	{
		m_trips.clear();
	}
	
	void reserve(int numverts)
	{
		/* reserve for diagonal entries */
		m_trips.reserve(numverts * 9);
	}
	
	void add(int i, int j, const fMatrix &m)
	{
		i *= 3;
		j *= 3;
		for (int k = 0; k < 3; ++k)
			for (int l = 0; l < 3; ++l)
				m_trips.push_back(Triplet(i + k, j + l, m.coeff(l, k)));
	}
	
	void sub(int i, int j, const fMatrix &m)
	{
		i *= 3;
		j *= 3;
		for (int k = 0; k < 3; ++k)
			for (int l = 0; l < 3; ++l)
				m_trips.push_back(Triplet(i + k, j + l, -m.coeff(l, k)));
	}
	
	inline void construct(lMatrix &m)
	{
		m.setFromTriplets(m_trips.begin(), m_trips.end());
		m_trips.clear();
	}
	
private:
	TripletList m_trips;
};

typedef Eigen::ConjugateGradient<lMatrix, Eigen::Lower, Eigen::DiagonalPreconditioner<Scalar> > ConjugateGradient;

using Eigen::ComputationInfo;

BLI_INLINE void print_lvector(const lVector &v)
{
	for (int i = 0; i < v.rows(); ++i) {
		if (i > 0 && i % 3 == 0)
			printf("\n");
		
		printf("%f,\n", v[i]);
	}
}

BLI_INLINE void print_lmatrix(const lMatrix &m)
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

#endif
