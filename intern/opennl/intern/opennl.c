/*
 *  $Id$
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

/* SuperLU includes */
#include <ssp_defs.h>
#include <util.h>

/************************************************************************************/
/* Assertions */


static void __nl_assertion_failed(char* cond, char* file, int line) {
	fprintf(
		stderr, 
		"OpenNL assertion failed: %s, file:%s, line:%d\n",
		cond,file,line
	);
	abort();
}

static void __nl_range_assertion_failed(
	float x, float min_val, float max_val, char* file, int line
) {
	fprintf(
		stderr, 
		"OpenNL range assertion failed: %f in [ %f ... %f ], file:%s, line:%d\n",
		x, min_val, max_val, file,line
	);
	abort();
}

static void __nl_should_not_have_reached(char* file, int line) {
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

#define __nl_range_assert(x,min_val,max_val) {				  \
	if(((x) < (min_val)) || ((x) > (max_val))) {				\
		__nl_range_assertion_failed(x, min_val, max_val,		\
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

#define __NL_NEW(T)				(T*)(calloc(1, sizeof(T))) 
#define __NL_NEW_ARRAY(T,NB)	   (T*)(calloc((NB),sizeof(T))) 
#define __NL_RENEW_ARRAY(T,x,NB)   (T*)(realloc(x,(NB)*sizeof(T))) 
#define __NL_DELETE(x)			 free(x); x = NULL 
#define __NL_DELETE_ARRAY(x)	   free(x); x = NULL

#define __NL_CLEAR(T, x)		   memset(x, 0, sizeof(T)) 
#define __NL_CLEAR_ARRAY(T,x,NB)   memset(x, 0, (NB)*sizeof(T)) 

/************************************************************************************/
/* Dynamic arrays for sparse row/columns */

typedef struct {
	NLuint   index;
	NLfloat value;
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

static void __nlRowColumnAdd(__NLRowColumn* c, NLint index, NLfloat value) {
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
static void __nlRowColumnAppend(__NLRowColumn* c, NLint index, NLfloat value) {
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
	NLfloat*	  diag;
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
	M->diag = __NL_NEW_ARRAY(NLfloat, M->diag_size);
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
	__NLSparseMatrix* M, NLuint i, NLuint j, NLfloat value
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
	__NL_CLEAR_ARRAY(NLfloat, M->diag, M->diag_size);	
}

/* Returns the number of non-zero coefficients */
static NLuint __nlSparseMatrixNNZ( __NLSparseMatrix* M) {
	NLuint nnz = 0;
	NLuint i;
	if(M->storage & __NL_ROWS) {
		for(i = 0; i<M->m; i++) {
			nnz += M->row[i].size;
		}
	} else if (M->storage & __NL_COLUMNS) {
		for(i = 0; i<M->n; i++) {
			nnz += M->column[i].size;
		}
	} else {
		__nl_assert_not_reached;
	}
	return nnz;
}

/************************************************************************************/
/* SparseMatrix x Vector routines, internal helper routines */

static void __nlSparseMatrix_mult_rows_symmetric(
	__NLSparseMatrix* A, NLfloat* x, NLfloat* y
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
	__NLSparseMatrix* A, NLfloat* x, NLfloat* y
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
	__NLSparseMatrix* A, NLfloat* x, NLfloat* y
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
	__NLSparseMatrix* A, NLfloat* x, NLfloat* y
) {
	NLuint n = A->n;
	NLuint j,ii; 
	__NLRowColumn* Cj = NULL;
	__NLCoeff* c = NULL;
	__NL_CLEAR_ARRAY(NLfloat, y, A->m);
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

static void __nlSparseMatrixMult(__NLSparseMatrix* A, NLfloat* x, NLfloat* y) {
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
	float value;

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
	__NLSparseMatrix* A, NLfloat* x, NLfloat* y
) {
	NLuint m = A->m;
	NLuint n = A->n;
	NLuint i,ij;
	__NLRowColumn* Ri = NULL;
	__NLCoeff* c = NULL;

	__NL_CLEAR_ARRAY(NLfloat, y, n);

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

typedef void(*__NLMatrixFunc)(float* x, float* y);

typedef struct {
	NLfloat  value[4];
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
	NLfloat*		b;
	NLfloat*		Mtb;
	__NLSparseMatrix M;
	__NLSparseMatrix MtM;
	NLfloat*		x;
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
	NLfloat			error;
	__NLMatrixFunc	matrix_vector_prod;

	struct __NLSuperLUContext {
		NLboolean alloc_slu;
		SuperMatrix L, U;
		NLint *perm_c, *perm_r;
		SuperLUStat_t stat;
	} slu;
} __NLContext;

static __NLContext* __nlCurrentContext = NULL;

static void __nlMatrixVectorProd_default(NLfloat* x, NLfloat* y) {
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

static void __nlFree_SUPERLU(__NLContext *context);

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
	if (context->slu.alloc_slu) {
		__nlFree_SUPERLU(context);
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

void nlSolverParameterf(NLenum pname, NLfloat param) {
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

void nlGetBooleanv(NLenum pname, NLboolean* params) {
	switch(pname) {
	case NL_LEAST_SQUARES: {
		*params = __nlCurrentContext->least_squares;
	} break;
	case NL_SYMMETRIC: {
		*params = __nlCurrentContext->symmetric;
	} break;
	default: {
		__nl_assert_not_reached;
	} break;
	}
}

void nlGetFloatv(NLenum pname, NLfloat* params) {
	switch(pname) {
	case NL_NB_VARIABLES: {
		*params = (NLfloat)(__nlCurrentContext->nb_variables);
	} break;
	case NL_NB_ROWS: {
		*params = (NLfloat)(__nlCurrentContext->nb_rows);
	} break;
	case NL_LEAST_SQUARES: {
		*params = (NLfloat)(__nlCurrentContext->least_squares);
	} break;
	case NL_SYMMETRIC: {
		*params = (NLfloat)(__nlCurrentContext->symmetric);
	} break;
	case NL_ERROR: {
		*params = (NLfloat)(__nlCurrentContext->error);
	} break;
	default: {
		__nl_assert_not_reached;
	} break;
	}
}

void nlGetIntergerv(NLenum pname, NLint* params) {
	switch(pname) {
	case NL_NB_VARIABLES: {
		*params = (NLint)(__nlCurrentContext->nb_variables);
	} break;
	case NL_NB_ROWS: {
		*params = (NLint)(__nlCurrentContext->nb_rows);
	} break;
	case NL_LEAST_SQUARES: {
		*params = (NLint)(__nlCurrentContext->least_squares);
	} break;
	case NL_SYMMETRIC: {
		*params = (NLint)(__nlCurrentContext->symmetric);
	} break;
	default: {
		__nl_assert_not_reached;
	} break;
	}
}

/************************************************************************************/
/* Enable / Disable */

void nlEnable(NLenum pname) {
	switch(pname) {
	default: {
		__nl_assert_not_reached;
	}
	}
}

void nlDisable(NLenum pname) {
	switch(pname) {
	default: {
		__nl_assert_not_reached;
	}
	}
}

NLboolean nlIsEnabled(NLenum pname) {
	switch(pname) {
	default: {
		__nl_assert_not_reached;
	}
	}
	return NL_FALSE;
}

/************************************************************************************/
/* Get/Set Lock/Unlock variables */

void nlSetVariable(NLuint rhsindex, NLuint index, NLfloat value) {
	__nlCheckState(__NL_STATE_SYSTEM);
	__nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1);
	__nlCurrentContext->variable[index].value[rhsindex] = value;	
}

NLfloat nlGetVariable(NLuint rhsindex, NLuint index) {
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

NLboolean nlVariableIsLocked(NLuint index) {
	__nl_assert(__nlCurrentContext->state != __NL_STATE_INITIAL);
	__nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1);
	return __nlCurrentContext->variable[index].locked;
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

		context->b = __NL_NEW_ARRAY(NLfloat, m*context->nb_rhs);
		context->alloc_b = NL_TRUE;

		context->x = __NL_NEW_ARRAY(NLfloat, n*context->nb_rhs);
		context->alloc_x = NL_TRUE;
	}
	else {
		/* need to recompute b only, A is not constructed anymore */
		__NL_CLEAR_ARRAY(NLfloat, context->b, context->m*context->nb_rhs);
	}

	__nlVariablesToVector();
}

static void __nlEndMatrixRHS(NLuint rhs) {
	__NLContext *context = __nlCurrentContext;
	__NLVariable *variable;
	__NLRowColumn *a;
	NLfloat *b, *Mtb;
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
				__NL_NEW_ARRAY(NLfloat, context->n*context->nb_rhs);
			context->alloc_Mtb = NL_TRUE;
		}
	}

	for(i=0; i<context->nb_rhs; i++)
		__nlEndMatrixRHS(i);
}

void nlMatrixAdd(NLuint row, NLuint col, NLfloat value)
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
		
		__nl_range_assert(row, 0, context->m - 1);
		__nl_range_assert(col, 0, context->n - 1);

		__nlSparseMatrixAdd(M, row, col, value);
	}
}

void nlRightHandSideAdd(NLuint rhsindex, NLuint index, NLfloat value)
{
	__NLContext *context = __nlCurrentContext;
	NLfloat* b = context->b;

	__nlCheckState(__NL_STATE_MATRIX);

	if(context->least_squares) {
		__nl_range_assert(index, 0, context->m - 1);
		b[rhsindex*context->m + index] += value;
	}
	else {
		if(!context->variable[index].locked) {
			index = context->variable[index].index;
			__nl_range_assert(index, 0, context->m - 1);

			b[rhsindex*context->m + index] += value;
		}
	}
}

void nlRightHandSideSet(NLuint rhsindex, NLuint index, NLfloat value)
{
	__NLContext *context = __nlCurrentContext;
	NLfloat* b = context->b;

	__nlCheckState(__NL_STATE_MATRIX);

	if(context->least_squares) {
		__nl_range_assert(index, 0, context->m - 1);
		b[rhsindex*context->m + index] = value;
	}
	else {
		if(!context->variable[index].locked) {
			index = context->variable[index].index;
			__nl_range_assert(index, 0, context->m - 1);

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
/* SuperLU wrapper */

/* Note: SuperLU is difficult to call, but it is worth it.	*/
/* Here is a driver inspired by A. Sheffer's "cow flattener". */
static NLboolean __nlFactorize_SUPERLU(__NLContext *context, NLint *permutation) {

	/* OpenNL Context */
	__NLSparseMatrix* M = (context->least_squares)? &context->MtM: &context->M;
	NLuint n = context->n;
	NLuint nnz = __nlSparseMatrixNNZ(M); /* number of non-zero coeffs */

	/* Compressed Row Storage matrix representation */
	NLint	*xa		= __NL_NEW_ARRAY(NLint, n+1);
	NLfloat	*rhs	= __NL_NEW_ARRAY(NLfloat, n);
	NLfloat	*a		= __NL_NEW_ARRAY(NLfloat, nnz);
	NLint	*asub	= __NL_NEW_ARRAY(NLint, nnz);
	NLint	*etree	= __NL_NEW_ARRAY(NLint, n);

	/* SuperLU variables */
	SuperMatrix At, AtP;
	NLint info, panel_size, relax;
	superlu_options_t options;

	/* Temporary variables */
	NLuint i, jj, count;
	
	__nl_assert(!(M->storage & __NL_SYMMETRIC));
	__nl_assert(M->storage & __NL_ROWS);
	__nl_assert(M->m == M->n);
	
	/* Convert M to compressed column format */
	for(i=0, count=0; i<n; i++) {
		__NLRowColumn *Ri = M->row + i;
		xa[i] = count;

		for(jj=0; jj<Ri->size; jj++, count++) {
			a[count] = Ri->coeff[jj].value;
			asub[count] = Ri->coeff[jj].index;
		}
	}
	xa[n] = nnz;

	/* Free M, don't need it anymore at this point */
	__nlSparseMatrixClear(M);

	/* Create superlu A matrix transposed */
	sCreate_CompCol_Matrix(
		&At, n, n, nnz, a, asub, xa, 
		SLU_NC,		/* Colum wise, no supernode */
		SLU_S,		/* floats */ 
		SLU_GE		/* general storage */
	);

	/* Set superlu options */
	set_default_options(&options);
	options.ColPerm = MY_PERMC;
	options.Fact = DOFACT;

	StatInit(&(context->slu.stat));

	panel_size = sp_ienv(1); /* sp_ienv give us the defaults */
	relax = sp_ienv(2);

	/* Compute permutation and permuted matrix */
	context->slu.perm_r = __NL_NEW_ARRAY(NLint, n);
	context->slu.perm_c = __NL_NEW_ARRAY(NLint, n);

	if ((permutation == NULL) || (*permutation == -1)) {
		get_perm_c(3, &At, context->slu.perm_c);

		if (permutation)
			memcpy(permutation, context->slu.perm_c, sizeof(NLint)*n);
	}
	else
		memcpy(context->slu.perm_c, permutation, sizeof(NLint)*n);

	sp_preorder(&options, &At, context->slu.perm_c, etree, &AtP);

	/* Decompose into L and U */
	sgstrf(&options, &AtP, relax, panel_size,
		etree, NULL, 0, context->slu.perm_c, context->slu.perm_r,
		&(context->slu.L), &(context->slu.U), &(context->slu.stat), &info);

	/* Cleanup */

	Destroy_SuperMatrix_Store(&At);
	Destroy_CompCol_Permuted(&AtP);

	__NL_DELETE_ARRAY(etree);
	__NL_DELETE_ARRAY(xa);
	__NL_DELETE_ARRAY(rhs);
	__NL_DELETE_ARRAY(a);
	__NL_DELETE_ARRAY(asub);

	context->slu.alloc_slu = NL_TRUE;

	return (info == 0);
}

static NLboolean __nlInvert_SUPERLU(__NLContext *context) {

	/* OpenNL Context */
	NLfloat* b = (context->least_squares)? context->Mtb: context->b;
	NLfloat* x = context->x;
	NLuint n = context->n, j;

	/* SuperLU variables */
	SuperMatrix B;
	NLint info;

	for(j=0; j<context->nb_rhs; j++, b+=n, x+=n) {
		/* Create superlu array for B */
		sCreate_Dense_Matrix(
			&B, n, 1, b, n, 
			SLU_DN, /* Fortran-type column-wise storage */
			SLU_S,  /* floats						  */
			SLU_GE  /* general						  */
		);

		/* Forward/Back substitution to compute x */
		sgstrs(TRANS, &(context->slu.L), &(context->slu.U),
			context->slu.perm_c, context->slu.perm_r, &B,
			&(context->slu.stat), &info);

		if(info == 0)
			memcpy(x, ((DNformat*)B.Store)->nzval, sizeof(*x)*n);

		Destroy_SuperMatrix_Store(&B);
	}

	return (info == 0);
}

static void __nlFree_SUPERLU(__NLContext *context) {

	Destroy_SuperNode_Matrix(&(context->slu.L));
	Destroy_CompCol_Matrix(&(context->slu.U));

	StatFree(&(context->slu.stat));

	__NL_DELETE_ARRAY(context->slu.perm_r);
	__NL_DELETE_ARRAY(context->slu.perm_c);

	context->slu.alloc_slu = NL_FALSE;
}

void nlPrintMatrix(void) {
	__NLContext *context = __nlCurrentContext;
	__NLSparseMatrix* M  = &(context->M);
	__NLSparseMatrix* MtM  = &(context->MtM);
	float *b = context->b;
	NLuint i, jj, k;
	NLuint m = context->m;
	NLuint n = context->n;
	__NLRowColumn* Ri = NULL;
	float *value = malloc(sizeof(*value)*(n+m));

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
		printf("b (%d):\n", k);
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
			printf("Mtb (%d):\n", k);
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
		result = __nlFactorize_SUPERLU(__nlCurrentContext, permutation);

	if (result) {
		result = __nlInvert_SUPERLU(__nlCurrentContext);

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

