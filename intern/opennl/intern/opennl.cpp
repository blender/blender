/** \file opennl/intern/opennl.c
 *  \ingroup opennlintern
 */
/*
 *
 *  OpenNL: Numerical Library
 *  Copyright (C) 2004 Bruno Levy
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  If you modify this software, you should include a notice giving the
 *  name of the person performing the modification, the date of modification,
 *  and the reason for such modification.
 *
 *  Contact: Bruno Levy
 *
 *	 levy@loria.fr
 *
 *	 ISA Project
 *	 LORIA, INRIA Lorraine, 
 *	 Campus Scientifique, BP 239
 *	 54506 VANDOEUVRE LES NANCY CEDEX 
 *	 FRANCE
 *
 *  Note that the GNU General Public License does not permit incorporating
 *  the Software into proprietary programs. 
 */

#include "ONL_opennl.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef NL_PARANOID
#ifndef NL_DEBUG
#define NL_DEBUG
#endif
#endif

#include <Eigen/Sparse>
#include <iostream>

typedef Eigen::SparseMatrix<double, Eigen::ColMajor> EigenSparseMatrix;
typedef Eigen::SparseLU<EigenSparseMatrix> EigenSparseSolver;

/************************************************************************************/
/* Assertions */


static void __nl_assertion_failed(const char* cond, const char* file, int line) {
	fprintf(
		stderr, 
		"OpenNL assertion failed: %s, file:%s, line:%d\n",
		cond,file,line
	);
	abort();
}

static void __nl_range_assertion_failed(
	double x, double min_val, double max_val, const char* file, int line
) {
	fprintf(
		stderr, 
		"OpenNL range assertion failed: %f in [ %f ... %f ], file:%s, line:%d\n",
		x, min_val, max_val, file,line
	);
	abort();
}

static void __nl_should_not_have_reached(const char* file, int line) {
	fprintf(
		stderr, 
		"OpenNL should not have reached this point: file:%s, line:%d\n",
		file,line
	);
	abort();
}


#define __nl_assert(x) {										\
	if(!(x)) {												  \
		__nl_assertion_failed(#x,__FILE__, __LINE__);		  \
	}														   \
} 

#define __nl_range_assert(x,max_val) {						  \
	if(((x) > (max_val))) {										\
		__nl_range_assertion_failed(x, 0.0, max_val,			\
			__FILE__, __LINE__								  \
		);													 \
	}														   \
}

#define __nl_assert_not_reached {							   \
	__nl_should_not_have_reached(__FILE__, __LINE__);		  \
}

#ifdef NL_DEBUG
#define __nl_debug_assert(x) __nl_assert(x)
#define __nl_debug_range_assert(x,min_val,max_val) __nl_range_assert(x,min_val,max_val)
#else
#define __nl_debug_assert(x) 
#define __nl_debug_range_assert(x,min_val,max_val) 
#endif

#ifdef NL_PARANOID
#define __nl_parano_assert(x) __nl_assert(x)
#define __nl_parano_range_assert(x,min_val,max_val) __nl_range_assert(x,min_val,max_val)
#else
#define __nl_parano_assert(x) 
#define __nl_parano_range_assert(x,min_val,max_val) 
#endif

/************************************************************************************/
/* classic macros */

#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y)) 
#endif

#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y)) 
#endif

/************************************************************************************/
/* memory management */

#define __NL_NEW(T)                (T*)(calloc(1, sizeof(T))) 
#define __NL_NEW_ARRAY(T,NB)       (T*)(calloc(MAX(NB, 1),sizeof(T))) 
#define __NL_RENEW_ARRAY(T,x,NB)   (T*)(realloc(x,(NB)*sizeof(T))) 
#define __NL_DELETE(x)             if(x) free(x); x = NULL 
#define __NL_DELETE_ARRAY(x)       if(x) free(x); x = NULL

#define __NL_CLEAR(T, x)           memset(x, 0, sizeof(T)) 
#define __NL_CLEAR_ARRAY(T,x,NB)   if(NB) memset(x, 0, (NB)*sizeof(T)) 

/************************************************************************************/
/* Dynamic arrays for sparse row/columns */

typedef struct {
	NLuint   index;
	NLdouble value;
} __NLCoeff;

typedef struct {
	NLuint size;
	NLuint capacity;
	__NLCoeff* coeff;
} __NLRowColumn;

static void __nlRowColumnConstruct(__NLRowColumn* c) {
	c->size	 = 0;
	c->capacity = 0;
	c->coeff	= NULL;
}

static void __nlRowColumnDestroy(__NLRowColumn* c) {
	__NL_DELETE_ARRAY(c->coeff);
#ifdef NL_PARANOID
	__NL_CLEAR(__NLRowColumn, c); 
#endif
}

static void __nlRowColumnGrow(__NLRowColumn* c) {
	if(c->capacity != 0) {
		c->capacity = 2 * c->capacity;
		c->coeff = __NL_RENEW_ARRAY(__NLCoeff, c->coeff, c->capacity);
	} else {
		c->capacity = 4;
		c->coeff = __NL_NEW_ARRAY(__NLCoeff, c->capacity);
	}
}

static void __nlRowColumnAdd(__NLRowColumn* c, NLint index, NLdouble value) {
	NLuint i;
	for(i=0; i<c->size; i++) {
		if(c->coeff[i].index == (NLuint)index) {
			c->coeff[i].value += value;
			return;
		}
	}
	if(c->size == c->capacity) {
		__nlRowColumnGrow(c);
	}
	c->coeff[c->size].index = index;
	c->coeff[c->size].value = value;
	c->size++;
}

/* Does not check whether the index already exists */
static void __nlRowColumnAppend(__NLRowColumn* c, NLint index, NLdouble value) {
	if(c->size == c->capacity) {
		__nlRowColumnGrow(c);
	}
	c->coeff[c->size].index = index;
	c->coeff[c->size].value = value;
	c->size++;
}

static void __nlRowColumnClear(__NLRowColumn* c) {
	c->size	 = 0;
	c->capacity = 0;
	__NL_DELETE_ARRAY(c->coeff);
}

/************************************************************************************/
/* SparseMatrix data structure */

#define __NL_ROWS	  1
#define __NL_COLUMNS   2
#define __NL_SYMMETRIC 4

typedef struct {
	NLuint m;
	NLuint n;
	NLuint diag_size;
	NLenum storage;
	__NLRowColumn* row;
	__NLRowColumn* column;
	NLdouble*	  diag;
} __NLSparseMatrix;


static void __nlSparseMatrixConstruct(
	__NLSparseMatrix* M, NLuint m, NLuint n, NLenum storage
) {
	NLuint i;
	M->m = m;
	M->n = n;
	M->storage = storage;
	if(storage & __NL_ROWS) {
		M->row = __NL_NEW_ARRAY(__NLRowColumn, m);
		for(i=0; i<m; i++) {
			__nlRowColumnConstruct(&(M->row[i]));
		}
	} else {
		M->row = NULL;
	}

	if(storage & __NL_COLUMNS) {
		M->column = __NL_NEW_ARRAY(__NLRowColumn, n);
		for(i=0; i<n; i++) {
			__nlRowColumnConstruct(&(M->column[i]));
		}
	} else {
		M->column = NULL;
	}

	M->diag_size = MIN(m,n);
	M->diag = __NL_NEW_ARRAY(NLdouble, M->diag_size);
}

static void __nlSparseMatrixDestroy(__NLSparseMatrix* M) {
	NLuint i;
	__NL_DELETE_ARRAY(M->diag);
	if(M->storage & __NL_ROWS) {
		for(i=0; i<M->m; i++) {
			__nlRowColumnDestroy(&(M->row[i]));
		}
		__NL_DELETE_ARRAY(M->row);
	}
	if(M->storage & __NL_COLUMNS) {
		for(i=0; i<M->n; i++) {
			__nlRowColumnDestroy(&(M->column[i]));
		}
		__NL_DELETE_ARRAY(M->column);
	}
#ifdef NL_PARANOID
	__NL_CLEAR(__NLSparseMatrix,M);
#endif
}

static void __nlSparseMatrixAdd(
	__NLSparseMatrix* M, NLuint i, NLuint j, NLdouble value
) {
	__nl_parano_range_assert(i, 0, M->m - 1);
	__nl_parano_range_assert(j, 0, M->n - 1);
	if((M->storage & __NL_SYMMETRIC) && (j > i)) {
		return;
	}
	if(i == j) {
		M->diag[i] += value;
	}
	if(M->storage & __NL_ROWS) {
		__nlRowColumnAdd(&(M->row[i]), j, value);
	}
	if(M->storage & __NL_COLUMNS) {
		__nlRowColumnAdd(&(M->column[j]), i, value);
	}
}

static void __nlSparseMatrixClear( __NLSparseMatrix* M) {
	NLuint i;
	if(M->storage & __NL_ROWS) {
		for(i=0; i<M->m; i++) {
			__nlRowColumnClear(&(M->row[i]));
		}
	}
	if(M->storage & __NL_COLUMNS) {
		for(i=0; i<M->n; i++) {
			__nlRowColumnClear(&(M->column[i]));
		}
	}
	__NL_CLEAR_ARRAY(NLdouble, M->diag, M->diag_size);	
}

/************************************************************************************/
/* SparseMatrix x Vector routines, internal helper routines */

static void __nlSparseMatrix_mult_rows_symmetric(
	__NLSparseMatrix* A, NLdouble* x, NLdouble* y
) {
	NLuint m = A->m;
	NLuint i,ij;
	__NLRowColumn* Ri = NULL;
	__NLCoeff* c = NULL;
	for(i=0; i<m; i++) {
		y[i] = 0;
		Ri = &(A->row[i]);
		for(ij=0; ij<Ri->size; ij++) {
			c = &(Ri->coeff[ij]);
			y[i] += c->value * x[c->index];
			if(i != c->index) {
				y[c->index] += c->value * x[i];
			}
		}
	}
}

static void __nlSparseMatrix_mult_rows(
	__NLSparseMatrix* A, NLdouble* x, NLdouble* y
) {
	NLuint m = A->m;
	NLuint i,ij;
	__NLRowColumn* Ri = NULL;
	__NLCoeff* c = NULL;
	for(i=0; i<m; i++) {
		y[i] = 0;
		Ri = &(A->row[i]);
		for(ij=0; ij<Ri->size; ij++) {
			c = &(Ri->coeff[ij]);
			y[i] += c->value * x[c->index];
		}
	}
}

static void __nlSparseMatrix_mult_cols_symmetric(
	__NLSparseMatrix* A, NLdouble* x, NLdouble* y
) {
	NLuint n = A->n;
	NLuint j,ii;
	__NLRowColumn* Cj = NULL;
	__NLCoeff* c = NULL;
	for(j=0; j<n; j++) {
		y[j] = 0;
		Cj = &(A->column[j]);
		for(ii=0; ii<Cj->size; ii++) {
			c = &(Cj->coeff[ii]);
			y[c->index] += c->value * x[j];
			if(j != c->index) {
				y[j] += c->value * x[c->index];
			}
		}
	}
}

static void __nlSparseMatrix_mult_cols(
	__NLSparseMatrix* A, NLdouble* x, NLdouble* y
) {
	NLuint n = A->n;
	NLuint j,ii; 
	__NLRowColumn* Cj = NULL;
	__NLCoeff* c = NULL;
	__NL_CLEAR_ARRAY(NLdouble, y, A->m);
	for(j=0; j<n; j++) {
		Cj = &(A->column[j]);
		for(ii=0; ii<Cj->size; ii++) {
			c = &(Cj->coeff[ii]);
			y[c->index] += c->value * x[j];
		}
	}
}

/************************************************************************************/
/* SparseMatrix x Vector routines, main driver routine */

static void __nlSparseMatrixMult(__NLSparseMatrix* A, NLdouble* x, NLdouble* y) {
	if(A->storage & __NL_ROWS) {
		if(A->storage & __NL_SYMMETRIC) {
			__nlSparseMatrix_mult_rows_symmetric(A, x, y);
		} else {
			__nlSparseMatrix_mult_rows(A, x, y);
		}
	} else {
		if(A->storage & __NL_SYMMETRIC) {
			__nlSparseMatrix_mult_cols_symmetric(A, x, y);
		} else {
			__nlSparseMatrix_mult_cols(A, x, y);
		}
	}
}

/* ****************** Routines for least squares ******************* */

static void __nlSparseMatrix_square(
	__NLSparseMatrix* AtA, __NLSparseMatrix *A
) {
	NLuint m = A->m;
	NLuint n = A->n;
	NLuint i, j0, j1;
	__NLRowColumn *Ri = NULL;
	__NLCoeff *c0 = NULL, *c1 = NULL;
	double value;

	__nlSparseMatrixConstruct(AtA, n, n, A->storage);

	for(i=0; i<m; i++) {
		Ri = &(A->row[i]);

		for(j0=0; j0<Ri->size; j0++) {
			c0 = &(Ri->coeff[j0]);
			for(j1=0; j1<Ri->size; j1++) {
				c1 = &(Ri->coeff[j1]);

				value = c0->value*c1->value;
				__nlSparseMatrixAdd(AtA, c0->index, c1->index, value);
			}
		}
	}
}

static void __nlSparseMatrix_transpose_mult_rows(
	__NLSparseMatrix* A, NLdouble* x, NLdouble* y
) {
	NLuint m = A->m;
	NLuint n = A->n;
	NLuint i,ij;
	__NLRowColumn* Ri = NULL;
	__NLCoeff* c = NULL;

	__NL_CLEAR_ARRAY(NLdouble, y, n);

	for(i=0; i<m; i++) {
		Ri = &(A->row[i]);
		for(ij=0; ij<Ri->size; ij++) {
			c = &(Ri->coeff[ij]);
			y[c->index] += c->value * x[i];
		}
	}
}

/************************************************************************************/
/* NLContext data structure */

typedef void(*__NLMatrixFunc)(double* x, double* y);

typedef struct {
	NLdouble  value[4];
	NLboolean locked;
	NLuint	index;
	__NLRowColumn *a;
} __NLVariable;

#define __NL_STATE_INITIAL				0
#define __NL_STATE_SYSTEM				1
#define __NL_STATE_MATRIX				2
#define __NL_STATE_MATRIX_CONSTRUCTED	3
#define __NL_STATE_SYSTEM_CONSTRUCTED	4
#define __NL_STATE_SYSTEM_SOLVED		5

typedef struct {
	NLenum			state;
	NLuint			n;
	NLuint			m;
	__NLVariable*	variable;
	NLdouble*		b;
	NLdouble*		Mtb;
	__NLSparseMatrix M;
	__NLSparseMatrix MtM;
	NLdouble*		x;
	NLuint			nb_variables;
	NLuint			nb_rows;
	NLboolean		least_squares;
	NLboolean		symmetric;
	NLuint			nb_rhs;
	NLboolean		solve_again;
	NLboolean		alloc_M;
	NLboolean		alloc_MtM;
	NLboolean		alloc_variable;
	NLboolean		alloc_x;
	NLboolean		alloc_b;
	NLboolean		alloc_Mtb;
	NLdouble			error;
	__NLMatrixFunc	matrix_vector_prod;

	struct __NLEigenContext {
		EigenSparseSolver *sparse_solver;
	} eigen;
} __NLContext;

static __NLContext* __nlCurrentContext = NULL;

static void __nlMatrixVectorProd_default(NLdouble* x, NLdouble* y) {
	__nlSparseMatrixMult(&(__nlCurrentContext->M), x, y);
}


NLContext nlNewContext(void) {
	__NLContext* result	  = __NL_NEW(__NLContext);
	result->state			= __NL_STATE_INITIAL;
	result->matrix_vector_prod = __nlMatrixVectorProd_default;
	result->nb_rhs = 1;
	nlMakeCurrent(result);
	return result;
}

static void __nlFree_EIGEN(__NLContext *context);

void nlDeleteContext(NLContext context_in) {
	__NLContext* context = (__NLContext*)(context_in);
	int i;

	if(__nlCurrentContext == context) {
		__nlCurrentContext = NULL;
	}
	if(context->alloc_M) {
		__nlSparseMatrixDestroy(&context->M);
	}
	if(context->alloc_MtM) {
		__nlSparseMatrixDestroy(&context->MtM);
	}
	if(context->alloc_variable) {
		for(i=0; i<context->nb_variables; i++) {
			if(context->variable[i].a) {
				__nlRowColumnDestroy(context->variable[i].a);
				__NL_DELETE(context->variable[i].a);
			}
		}

		__NL_DELETE_ARRAY(context->variable);
	}
	if(context->alloc_b) {
		__NL_DELETE_ARRAY(context->b);
	}
	if(context->alloc_Mtb) {
		__NL_DELETE_ARRAY(context->Mtb);
	}
	if(context->alloc_x) {
		__NL_DELETE_ARRAY(context->x);
	}
	if (context->eigen.sparse_solver) {
		__nlFree_EIGEN(context);
	}

#ifdef NL_PARANOID
	__NL_CLEAR(__NLContext, context);
#endif
	__NL_DELETE(context);
}

void nlMakeCurrent(NLContext context) {
	__nlCurrentContext = (__NLContext*)(context);
}

NLContext nlGetCurrent(void) {
	return __nlCurrentContext;
}

static void __nlCheckState(NLenum state) {
	__nl_assert(__nlCurrentContext->state == state);
}

static void __nlTransition(NLenum from_state, NLenum to_state) {
	__nlCheckState(from_state);
	__nlCurrentContext->state = to_state;
}

/************************************************************************************/
/* Get/Set parameters */

void nlSolverParameteri(NLenum pname, NLint param) {
	__nlCheckState(__NL_STATE_INITIAL);
	switch(pname) {
	case NL_NB_VARIABLES: {
		__nl_assert(param > 0);
		__nlCurrentContext->nb_variables = (NLuint)param;
	} break;
	case NL_NB_ROWS: {
		__nl_assert(param > 0);
		__nlCurrentContext->nb_rows = (NLuint)param;
	} break;
	case NL_LEAST_SQUARES: {
		__nlCurrentContext->least_squares = (NLboolean)param;
	} break;
	case NL_SYMMETRIC: {
		__nlCurrentContext->symmetric = (NLboolean)param;		
	} break;
	case NL_NB_RIGHT_HAND_SIDES: {
		__nlCurrentContext->nb_rhs = (NLuint)param;
	} break;
	default: {
		__nl_assert_not_reached;
	} break;
	}
}

/************************************************************************************/
/* Get/Set Lock/Unlock variables */

void nlSetVariable(NLuint rhsindex, NLuint index, NLdouble value) {
	__nlCheckState(__NL_STATE_SYSTEM);
	__nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1);
	__nlCurrentContext->variable[index].value[rhsindex] = value;	
}

NLdouble nlGetVariable(NLuint rhsindex, NLuint index) {
	__nl_assert(__nlCurrentContext->state != __NL_STATE_INITIAL);
	__nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1);
	return __nlCurrentContext->variable[index].value[rhsindex];
}

void nlLockVariable(NLuint index) {
	__nlCheckState(__NL_STATE_SYSTEM);
	__nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1);
	__nlCurrentContext->variable[index].locked = NL_TRUE;
}

void nlUnlockVariable(NLuint index) {
	__nlCheckState(__NL_STATE_SYSTEM);
	__nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1);
	__nlCurrentContext->variable[index].locked = NL_FALSE;
}

/************************************************************************************/
/* System construction */

static void __nlVariablesToVector() {
	__NLContext *context = __nlCurrentContext;
	NLuint i, j, nb_rhs;

	__nl_assert(context->alloc_x);
	__nl_assert(context->alloc_variable);

	nb_rhs= context->nb_rhs;

	for(i=0; i<context->nb_variables; i++) {
		__NLVariable* v = &(context->variable[i]);
		if(!v->locked) {
			__nl_assert(v->index < context->n);

			for(j=0; j<nb_rhs; j++)
				context->x[context->n*j + v->index] = v->value[j];
		}
	}
}

static void __nlVectorToVariables() {
	__NLContext *context = __nlCurrentContext;
	NLuint i, j, nb_rhs;

	__nl_assert(context->alloc_x);
	__nl_assert(context->alloc_variable);

	nb_rhs= context->nb_rhs;

	for(i=0; i<context->nb_variables; i++) {
		__NLVariable* v = &(context->variable[i]);
		if(!v->locked) {
			__nl_assert(v->index < context->n);

			for(j=0; j<nb_rhs; j++)
				v->value[j] = context->x[context->n*j + v->index];
		}
	}
}

static void __nlBeginSystem() {
	__nl_assert(__nlCurrentContext->nb_variables > 0);

	if (__nlCurrentContext->solve_again)
		__nlTransition(__NL_STATE_SYSTEM_SOLVED, __NL_STATE_SYSTEM);
	else {
		__nlTransition(__NL_STATE_INITIAL, __NL_STATE_SYSTEM);

		__nlCurrentContext->variable = __NL_NEW_ARRAY(
			__NLVariable, __nlCurrentContext->nb_variables);
		
		__nlCurrentContext->alloc_variable = NL_TRUE;
	}
}

static void __nlEndSystem() {
	__nlTransition(__NL_STATE_MATRIX_CONSTRUCTED, __NL_STATE_SYSTEM_CONSTRUCTED);	
}

static void __nlBeginMatrix() {
	NLuint i;
	NLuint m = 0, n = 0;
	NLenum storage = __NL_ROWS;
	__NLContext *context = __nlCurrentContext;

	__nlTransition(__NL_STATE_SYSTEM, __NL_STATE_MATRIX);

	if (!context->solve_again) {
		for(i=0; i<context->nb_variables; i++) {
			if(context->variable[i].locked) {
				context->variable[i].index = ~0;
				context->variable[i].a = __NL_NEW(__NLRowColumn);
				__nlRowColumnConstruct(context->variable[i].a);
			}
			else
				context->variable[i].index = n++;
		}

		m = (context->nb_rows == 0)? n: context->nb_rows;

		context->m = m;
		context->n = n;

		__nlSparseMatrixConstruct(&context->M, m, n, storage);
		context->alloc_M = NL_TRUE;

		context->b = __NL_NEW_ARRAY(NLdouble, m*context->nb_rhs);
		context->alloc_b = NL_TRUE;

		context->x = __NL_NEW_ARRAY(NLdouble, n*context->nb_rhs);
		context->alloc_x = NL_TRUE;
	}
	else {
		/* need to recompute b only, A is not constructed anymore */
		__NL_CLEAR_ARRAY(NLdouble, context->b, context->m*context->nb_rhs);
	}

	__nlVariablesToVector();
}

static void __nlEndMatrixRHS(NLuint rhs) {
	__NLContext *context = __nlCurrentContext;
	__NLVariable *variable;
	__NLRowColumn *a;
	NLdouble *b, *Mtb;
	NLuint i, j;

	b = context->b + context->m*rhs;
	Mtb = context->Mtb + context->n*rhs;

	for(i=0; i<__nlCurrentContext->nb_variables; i++) {
		variable = &(context->variable[i]);

		if(variable->locked) {
			a = variable->a;

			for(j=0; j<a->size; j++) {
				b[a->coeff[j].index] -= a->coeff[j].value*variable->value[rhs];
			}
		}
	}

	if(context->least_squares)
		__nlSparseMatrix_transpose_mult_rows(&context->M, b, Mtb);
}

static void __nlEndMatrix() {
	__NLContext *context = __nlCurrentContext;
	NLuint i;

	__nlTransition(__NL_STATE_MATRIX, __NL_STATE_MATRIX_CONSTRUCTED);	
	
	if(context->least_squares) {
		if(!__nlCurrentContext->solve_again) {
			__nlSparseMatrix_square(&context->MtM, &context->M);
			context->alloc_MtM = NL_TRUE;

			context->Mtb =
				__NL_NEW_ARRAY(NLdouble, context->n*context->nb_rhs);
			context->alloc_Mtb = NL_TRUE;
		}
	}

	for(i=0; i<context->nb_rhs; i++)
		__nlEndMatrixRHS(i);
}

void nlMatrixAdd(NLuint row, NLuint col, NLdouble value)
{
	__NLContext *context = __nlCurrentContext;

	__nlCheckState(__NL_STATE_MATRIX);

	if(context->solve_again)
		return;

	if (!context->least_squares && context->variable[row].locked);
	else if (context->variable[col].locked) {
		if(!context->least_squares)
			row = context->variable[row].index;
		__nlRowColumnAppend(context->variable[col].a, row, value);
	}
	else {
		__NLSparseMatrix* M  = &context->M;
		
		if(!context->least_squares)
			row = context->variable[row].index;
		col = context->variable[col].index;
		
		__nl_range_assert(row, context->m - 1);
		__nl_range_assert(col, context->n - 1);

		__nlSparseMatrixAdd(M, row, col, value);
	}
}

void nlRightHandSideAdd(NLuint rhsindex, NLuint index, NLdouble value)
{
	__NLContext *context = __nlCurrentContext;
	NLdouble* b = context->b;

	__nlCheckState(__NL_STATE_MATRIX);

	if(context->least_squares) {
		__nl_range_assert(index, context->m - 1);
		b[rhsindex*context->m + index] += value;
	}
	else {
		if(!context->variable[index].locked) {
			index = context->variable[index].index;
			__nl_range_assert(index, context->m - 1);

			b[rhsindex*context->m + index] += value;
		}
	}
}

void nlRightHandSideSet(NLuint rhsindex, NLuint index, NLdouble value)
{
	__NLContext *context = __nlCurrentContext;
	NLdouble* b = context->b;

	__nlCheckState(__NL_STATE_MATRIX);

	if(context->least_squares) {
		__nl_range_assert(index, context->m - 1);
		b[rhsindex*context->m + index] = value;
	}
	else {
		if(!context->variable[index].locked) {
			index = context->variable[index].index;
			__nl_range_assert(index, context->m - 1);

			b[rhsindex*context->m + index] = value;
		}
	}
}

void nlBegin(NLenum prim) {
	switch(prim) {
	case NL_SYSTEM: {
		__nlBeginSystem();
	} break;
	case NL_MATRIX: {
		__nlBeginMatrix();
	} break;
	default: {
		__nl_assert_not_reached;
	}
	}
}

void nlEnd(NLenum prim) {
	switch(prim) {
	case NL_SYSTEM: {
		__nlEndSystem();
	} break;
	case NL_MATRIX: {
		__nlEndMatrix();
	} break;
	default: {
		__nl_assert_not_reached;
	}
	}
}

/************************************************************************/
/* Eigen wrapper */

/* Note: Eigen is difficult to call, but it is worth it.	*/
/* Here is a driver inspired by A. Sheffer's "cow flattener". */
static NLboolean __nlFactorize_EIGEN(__NLContext *context, NLint *permutation) {

	/* OpenNL Context */
	__NLSparseMatrix* M = (context->least_squares)? &context->MtM: &context->M;
	NLuint n = context->n;

	/* Temporary variables */
	NLuint i, jj, count;
	
	__nl_assert(!(M->storage & __NL_SYMMETRIC));
	__nl_assert(M->storage & __NL_ROWS);
	__nl_assert(M->m == M->n);

	/* Convert M to compressed column format */
	EigenSparseMatrix A(M->m, M->n);

	for(i=0, count=0; i<n; i++) {
		__NLRowColumn *Ri = M->row + i;

		for(jj=0; jj<Ri->size; jj++, count++)
			A.insert(i, Ri->coeff[jj].index) = Ri->coeff[jj].value;
	}

	A.makeCompressed();

	/* Free M, don't need it anymore at this point */
	__nlSparseMatrixClear(M);

	/* Performance Sparse LU factorization */
	EigenSparseSolver *sparse_solver = new EigenSparseSolver();
	context->eigen.sparse_solver = sparse_solver;

	sparse_solver->analyzePattern(A);
	sparse_solver->factorize(A);

	return (sparse_solver->info() == Eigen::Success);
}

static NLboolean __nlInvert_EIGEN(__NLContext *context) {

	/* OpenNL Context */
	NLdouble* b = (context->least_squares)? context->Mtb: context->b;
	NLdouble* x = context->x;
	NLuint n = context->n, j;

	/* Solve each right hand side */
	for(j=0; j<context->nb_rhs; j++, b+=n, x+=n) {
		Eigen::Map<Eigen::VectorXd> eigen_b(b, n);

		Eigen::VectorXd eigen_x = context->eigen.sparse_solver->solve(eigen_b);
		for (NLuint i = 0; i < n; i++)
			x[i] = eigen_x[i];

		if (context->eigen.sparse_solver->info() != Eigen::Success)
			return false;
	}

	return true;
}

static void __nlFree_EIGEN(__NLContext *context) {
	delete context->eigen.sparse_solver;
	context->eigen.sparse_solver = NULL;
}

void nlPrintMatrix(void) {
	__NLContext *context = __nlCurrentContext;
	__NLSparseMatrix* M  = &(context->M);
	__NLSparseMatrix* MtM  = &(context->MtM);
	double *b = context->b;
	NLuint i, jj, k;
	NLuint m = context->m;
	NLuint n = context->n;
	__NLRowColumn* Ri = NULL;
	double *value = (double*)malloc(sizeof(*value)*(n+m));

	printf("A:\n");
	for(i=0; i<m; i++) {
		Ri = &(M->row[i]);

		memset(value, 0.0, sizeof(*value)*n);
		for(jj=0; jj<Ri->size; jj++)
			value[Ri->coeff[jj].index] = Ri->coeff[jj].value;

		for (k = 0; k<n; k++)
			printf("%.3f ", value[k]);
		printf("\n");
	}

	for(k=0; k<context->nb_rhs; k++) {
		printf("b (%u):\n", k);
		for(i=0; i<n; i++)
			printf("%f ", b[context->n*k + i]);
		printf("\n");
	}

	if(context->alloc_MtM) {
		printf("AtA:\n");
		for(i=0; i<n; i++) {
			Ri = &(MtM->row[i]);

			memset(value, 0.0, sizeof(*value)*m);
			for(jj=0; jj<Ri->size; jj++)
				value[Ri->coeff[jj].index] = Ri->coeff[jj].value;

			for (k = 0; k<n; k++)
				printf("%.3f ", value[k]);
			printf("\n");
		}

		for(k=0; k<context->nb_rhs; k++) {
			printf("Mtb (%u):\n", k);
			for(i=0; i<n; i++)
				printf("%f ", context->Mtb[context->n*k + i]);
			printf("\n");
		}
		printf("\n");
	}

	free(value);
}

/************************************************************************/
/* nlSolve() driver routine */

NLboolean nlSolveAdvanced(NLint *permutation, NLboolean solveAgain) {
	NLboolean result = NL_TRUE;

	__nlCheckState(__NL_STATE_SYSTEM_CONSTRUCTED);

	if (!__nlCurrentContext->solve_again)
		result = __nlFactorize_EIGEN(__nlCurrentContext, permutation);

	if (result) {
		result = __nlInvert_EIGEN(__nlCurrentContext);

		if (result) {
			__nlVectorToVariables();

			if (solveAgain)
				__nlCurrentContext->solve_again = NL_TRUE;

			__nlTransition(__NL_STATE_SYSTEM_CONSTRUCTED, __NL_STATE_SYSTEM_SOLVED);
		}
	}

	return result;
}

NLboolean nlSolve() {
	return nlSolveAdvanced(NULL, NL_FALSE);
}

