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
 *     levy@loria.fr
 *
 *     ISA Project
 *     LORIA, INRIA Lorraine, 
 *     Campus Scientifique, BP 239
 *     54506 VANDOEUVRE LES NANCY CEDEX 
 *     FRANCE
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
    ) ;
    abort() ;
}

static void __nl_range_assertion_failed(
    float x, float min_val, float max_val, char* file, int line
) {
    fprintf(
        stderr, 
        "OpenNL range assertion failed: %f in [ %f ... %f ], file:%s, line:%d\n",
        x, min_val, max_val, file,line
    ) ;
    abort() ;
}

static void __nl_should_not_have_reached(char* file, int line) {
    fprintf(
        stderr, 
        "OpenNL should not have reached this point: file:%s, line:%d\n",
        file,line
    ) ;
    abort() ;
}


#define __nl_assert(x) {                                        \
    if(!(x)) {                                                  \
        __nl_assertion_failed(#x,__FILE__, __LINE__) ;          \
    }                                                           \
} 

#define __nl_range_assert(x,min_val,max_val) {                  \
    if(((x) < (min_val)) || ((x) > (max_val))) {                \
        __nl_range_assertion_failed(x, min_val, max_val,        \
            __FILE__, __LINE__                                  \
        ) ;                                                     \
    }                                                           \
}

#define __nl_assert_not_reached {                               \
    __nl_should_not_have_reached(__FILE__, __LINE__) ;          \
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
#define __NL_NEW_ARRAY(T,NB)       (T*)(calloc((NB),sizeof(T))) 
#define __NL_RENEW_ARRAY(T,x,NB)   (T*)(realloc(x,(NB)*sizeof(T))) 
#define __NL_DELETE(x)             free(x); x = NULL 
#define __NL_DELETE_ARRAY(x)       free(x); x = NULL

#define __NL_CLEAR(T, x)           memset(x, 0, sizeof(T)) 
#define __NL_CLEAR_ARRAY(T,x,NB)   memset(x, 0, (NB)*sizeof(T)) 

/************************************************************************************/
/* Dynamic arrays for sparse row/columns */

typedef struct {
    NLuint   index ;
    NLfloat value ;
} __NLCoeff ;

typedef struct {
    NLuint size ;
    NLuint capacity ;
    __NLCoeff* coeff ;
} __NLRowColumn ;

static void __nlRowColumnConstruct(__NLRowColumn* c) {
    c->size     = 0 ;
    c->capacity = 0 ;
    c->coeff    = NULL ;
}

static void __nlRowColumnDestroy(__NLRowColumn* c) {
    __NL_DELETE_ARRAY(c->coeff) ;
#ifdef NL_PARANOID
    __NL_CLEAR(__NLRowColumn, c) ; 
#endif
}

static void __nlRowColumnGrow(__NLRowColumn* c) {
    if(c->capacity != 0) {
        c->capacity = 2 * c->capacity ;
        c->coeff = __NL_RENEW_ARRAY(__NLCoeff, c->coeff, c->capacity) ;
    } else {
        c->capacity = 4 ;
        c->coeff = __NL_NEW_ARRAY(__NLCoeff, c->capacity) ;
    }
}

static void __nlRowColumnAdd(__NLRowColumn* c, NLint index, NLfloat value) {
    NLuint i ;
    for(i=0; i<c->size; i++) {
        if(c->coeff[i].index == (NLuint)index) {
            c->coeff[i].value += value ;
            return ;
        }
    }
    if(c->size == c->capacity) {
        __nlRowColumnGrow(c) ;
    }
    c->coeff[c->size].index = index ;
    c->coeff[c->size].value = value ;
    c->size++ ;
}

/* Does not check whether the index already exists */
static void __nlRowColumnAppend(__NLRowColumn* c, NLint index, NLfloat value) {
    if(c->size == c->capacity) {
        __nlRowColumnGrow(c) ;
    }
    c->coeff[c->size].index = index ;
    c->coeff[c->size].value = value ;
    c->size++ ;
}

static void __nlRowColumnZero(__NLRowColumn* c) {
    c->size = 0 ;
}

static void __nlRowColumnClear(__NLRowColumn* c) {
    c->size     = 0 ;
    c->capacity = 0 ;
    __NL_DELETE_ARRAY(c->coeff) ;
}

/************************************************************************************/
/* SparseMatrix data structure */

#define __NL_ROWS      1
#define __NL_COLUMNS   2
#define __NL_SYMMETRIC 4

typedef struct {
    NLuint m ;
    NLuint n ;
    NLuint diag_size ;
    NLenum storage ;
    __NLRowColumn* row ;
    __NLRowColumn* column ;
    NLfloat*      diag ;
} __NLSparseMatrix ;


static void __nlSparseMatrixConstruct(
    __NLSparseMatrix* M, NLuint m, NLuint n, NLenum storage
) {
    NLuint i ;
    M->m = m ;
    M->n = n ;
    M->storage = storage ;
    if(storage & __NL_ROWS) {
        M->row = __NL_NEW_ARRAY(__NLRowColumn, m) ;
        for(i=0; i<n; i++) {
            __nlRowColumnConstruct(&(M->row[i])) ;
        }
    } else {
        M->row = NULL ;
    }

    if(storage & __NL_COLUMNS) {
        M->column = __NL_NEW_ARRAY(__NLRowColumn, n) ;
        for(i=0; i<n; i++) {
            __nlRowColumnConstruct(&(M->column[i])) ;
        }
    } else {
        M->column = NULL ;
    }

    M->diag_size = MIN(m,n) ;
    M->diag = __NL_NEW_ARRAY(NLfloat, M->diag_size) ;
}

static void __nlSparseMatrixDestroy(__NLSparseMatrix* M) {
    NLuint i ;
    __NL_DELETE_ARRAY(M->diag) ;
    if(M->storage & __NL_ROWS) {
        for(i=0; i<M->m; i++) {
            __nlRowColumnDestroy(&(M->row[i])) ;
        }
        __NL_DELETE_ARRAY(M->row) ;
    }
    if(M->storage & __NL_COLUMNS) {
        for(i=0; i<M->n; i++) {
            __nlRowColumnDestroy(&(M->column[i])) ;
        }
        __NL_DELETE_ARRAY(M->column) ;
    }
#ifdef NL_PARANOID
    __NL_CLEAR(__NLSparseMatrix,M) ;
#endif
}

static void __nlSparseMatrixAdd(
    __NLSparseMatrix* M, NLuint i, NLuint j, NLfloat value
) {
    __nl_parano_range_assert(i, 0, M->m - 1) ;
    __nl_parano_range_assert(j, 0, M->n - 1) ;
    if((M->storage & __NL_SYMMETRIC) && (j > i)) {
        return ;
    }
    if(i == j) {
        M->diag[i] += value ;
    }
    if(M->storage & __NL_ROWS) {
        __nlRowColumnAdd(&(M->row[i]), j, value) ;
    }
    if(M->storage & __NL_COLUMNS) {
        __nlRowColumnAdd(&(M->column[j]), i, value) ;
    }
}

static void __nlSparseMatrixClear( __NLSparseMatrix* M) {
    NLuint i ;
    if(M->storage & __NL_ROWS) {
        for(i=0; i<M->m; i++) {
            __nlRowColumnClear(&(M->row[i])) ;
        }
    }
    if(M->storage & __NL_COLUMNS) {
        for(i=0; i<M->n; i++) {
            __nlRowColumnClear(&(M->column[i])) ;
        }
    }
    __NL_CLEAR_ARRAY(NLfloat, M->diag, M->diag_size) ;    
}

/* Returns the number of non-zero coefficients */
static NLuint __nlSparseMatrixNNZ( __NLSparseMatrix* M) {
    NLuint nnz = 0 ;
    NLuint i ;
    if(M->storage & __NL_ROWS) {
        for(i = 0; i<M->m; i++) {
            nnz += M->row[i].size ;
        }
    } else if (M->storage & __NL_COLUMNS) {
        for(i = 0; i<M->n; i++) {
            nnz += M->column[i].size ;
        }
    } else {
        __nl_assert_not_reached ;
    }
    return nnz ;
}

/************************************************************************************/
/* SparseMatrix x Vector routines, internal helper routines */

static void __nlSparseMatrix_mult_rows_symmetric(
    __NLSparseMatrix* A, NLfloat* x, NLfloat* y
) {
    NLuint m = A->m ;
    NLuint i,ij ;
    __NLRowColumn* Ri = NULL ;
    __NLCoeff* c = NULL ;
    for(i=0; i<m; i++) {
        y[i] = 0 ;
        Ri = &(A->row[i]) ;
        for(ij=0; ij<Ri->size; ij++) {
            c = &(Ri->coeff[ij]) ;
            y[i] += c->value * x[c->index] ;
            if(i != c->index) {
                y[c->index] += c->value * x[i] ;
            }
        }
    }
}

static void __nlSparseMatrix_mult_rows(
    __NLSparseMatrix* A, NLfloat* x, NLfloat* y
) {
    NLuint m = A->m ;
    NLuint i,ij ;
    __NLRowColumn* Ri = NULL ;
    __NLCoeff* c = NULL ;
    for(i=0; i<m; i++) {
        y[i] = 0 ;
        Ri = &(A->row[i]) ;
        for(ij=0; ij<Ri->size; ij++) {
            c = &(Ri->coeff[ij]) ;
            y[i] += c->value * x[c->index] ;
        }
    }
}

static void __nlSparseMatrix_mult_cols_symmetric(
    __NLSparseMatrix* A, NLfloat* x, NLfloat* y
) {
    NLuint n = A->n ;
    NLuint j,ii ;
    __NLRowColumn* Cj = NULL ;
    __NLCoeff* c = NULL ;
    for(j=0; j<n; j++) {
        y[j] = 0 ;
        Cj = &(A->column[j]) ;
        for(ii=0; ii<Cj->size; ii++) {
            c = &(Cj->coeff[ii]) ;
            y[c->index] += c->value * x[j] ;
            if(j != c->index) {
                y[j] += c->value * x[c->index] ;
            }
        }
    }
}

static void __nlSparseMatrix_mult_cols(
    __NLSparseMatrix* A, NLfloat* x, NLfloat* y
) {
    NLuint n = A->n ;
    NLuint j,ii ; 
    __NLRowColumn* Cj = NULL ;
    __NLCoeff* c = NULL ;
    __NL_CLEAR_ARRAY(NLfloat, y, A->m) ;
    for(j=0; j<n; j++) {
        Cj = &(A->column[j]) ;
        for(ii=0; ii<Cj->size; ii++) {
            c = &(Cj->coeff[ii]) ;
            y[c->index] += c->value * x[j] ;
        }
    }
}

/************************************************************************************/
/* SparseMatrix x Vector routines, main driver routine */

void __nlSparseMatrixMult(__NLSparseMatrix* A, NLfloat* x, NLfloat* y) {
    if(A->storage & __NL_ROWS) {
        if(A->storage & __NL_SYMMETRIC) {
            __nlSparseMatrix_mult_rows_symmetric(A, x, y) ;
        } else {
            __nlSparseMatrix_mult_rows(A, x, y) ;
        }
    } else {
        if(A->storage & __NL_SYMMETRIC) {
            __nlSparseMatrix_mult_cols_symmetric(A, x, y) ;
        } else {
            __nlSparseMatrix_mult_cols(A, x, y) ;
        }
    }
}

/************************************************************************************/
/* NLContext data structure */

typedef void(*__NLMatrixFunc)(float* x, float* y) ;

typedef struct {
    NLfloat  value ;
    NLboolean locked ;
    NLuint    index ;
} __NLVariable ;

#define __NL_STATE_INITIAL            0
#define __NL_STATE_SYSTEM             1
#define __NL_STATE_MATRIX             2
#define __NL_STATE_ROW                3
#define __NL_STATE_MATRIX_CONSTRUCTED 4
#define __NL_STATE_SYSTEM_CONSTRUCTED 5
#define __NL_STATE_SOLVED             6

typedef struct {
    NLenum           state ;
    __NLVariable*    variable ;
    NLuint           n ;
    __NLSparseMatrix M ;
    __NLRowColumn    af ;
    __NLRowColumn    al ;
    __NLRowColumn    xl ;
    NLfloat*        x ;
    NLfloat*        b ;
    NLfloat         right_hand_side ;
    NLfloat         row_scaling ;
    NLuint           nb_variables ;
    NLuint           current_row ;
    NLboolean        least_squares ;
    NLboolean        symmetric ;
    NLboolean        normalize_rows ;
    NLboolean        alloc_M ;
    NLboolean        alloc_af ;
    NLboolean        alloc_al ;
    NLboolean        alloc_xl ;
    NLboolean        alloc_variable ;
    NLboolean        alloc_x ;
    NLboolean        alloc_b ;
    NLfloat         error ;
    __NLMatrixFunc   matrix_vector_prod ;
} __NLContext ;

static __NLContext* __nlCurrentContext = NULL ;

void __nlMatrixVectorProd_default(NLfloat* x, NLfloat* y) {
    __nlSparseMatrixMult(&(__nlCurrentContext->M), x, y) ;
}


NLContext nlNewContext() {
    __NLContext* result      = __NL_NEW(__NLContext) ;
    result->state            = __NL_STATE_INITIAL ;
    result->row_scaling      = 1.0 ;
    result->right_hand_side  = 0.0 ;
    result->matrix_vector_prod = __nlMatrixVectorProd_default ;
    nlMakeCurrent(result) ;
    return result ;
}

void nlDeleteContext(NLContext context_in) {
    __NLContext* context = (__NLContext*)(context_in) ;
    if(__nlCurrentContext == context) {
        __nlCurrentContext = NULL ;
    }
    if(context->alloc_M) {
        __nlSparseMatrixDestroy(&context->M) ;
    }
    if(context->alloc_af) {
        __nlRowColumnDestroy(&context->af) ;
    }
    if(context->alloc_al) {
        __nlRowColumnDestroy(&context->al) ;
    }
    if(context->alloc_xl) {
        __nlRowColumnDestroy(&context->xl) ;
    }
    if(context->alloc_variable) {
        __NL_DELETE_ARRAY(context->variable) ;
    }
    if(context->alloc_x) {
        __NL_DELETE_ARRAY(context->x) ;
    }
    if(context->alloc_b) {
        __NL_DELETE_ARRAY(context->b) ;
    }

#ifdef NL_PARANOID
    __NL_CLEAR(__NLContext, context) ;
#endif
    __NL_DELETE(context) ;
}

void nlMakeCurrent(NLContext context) {
    __nlCurrentContext = (__NLContext*)(context) ;
}

NLContext nlGetCurrent() {
    return __nlCurrentContext ;
}

void __nlCheckState(NLenum state) {
    __nl_assert(__nlCurrentContext->state == state) ;
}

void __nlTransition(NLenum from_state, NLenum to_state) {
    __nlCheckState(from_state) ;
    __nlCurrentContext->state = to_state ;
}

/************************************************************************************/
/* Get/Set parameters */

void nlSolverParameterf(NLenum pname, NLfloat param) {
    __nlCheckState(__NL_STATE_INITIAL) ;
    switch(pname) {
    case NL_NB_VARIABLES: {
        __nl_assert(param > 0) ;
        __nlCurrentContext->nb_variables = (NLuint)param ;
    } break ;
    case NL_LEAST_SQUARES: {
        __nlCurrentContext->least_squares = (NLboolean)param ;
    } break ;
    case NL_SYMMETRIC: {
        __nlCurrentContext->symmetric = (NLboolean)param ;        
    }
    default: {
        __nl_assert_not_reached ;
    } break ;
    }
}

void nlSolverParameteri(NLenum pname, NLint param) {
    __nlCheckState(__NL_STATE_INITIAL) ;
    switch(pname) {
    case NL_NB_VARIABLES: {
        __nl_assert(param > 0) ;
        __nlCurrentContext->nb_variables = (NLuint)param ;
    } break ;
    case NL_LEAST_SQUARES: {
        __nlCurrentContext->least_squares = (NLboolean)param ;
    } break ;
    case NL_SYMMETRIC: {
        __nlCurrentContext->symmetric = (NLboolean)param ;        
    }
    default: {
        __nl_assert_not_reached ;
    } break ;
    }
}

void nlRowParameterf(NLenum pname, NLfloat param) {
    __nlCheckState(__NL_STATE_MATRIX) ;
    switch(pname) {
    case NL_RIGHT_HAND_SIDE: {
        __nlCurrentContext->right_hand_side = param ;
    } break ;
    case NL_ROW_SCALING: {
        __nlCurrentContext->row_scaling = param ;
    } break ;
    }
}

void nlRowParameteri(NLenum pname, NLint param) {
    __nlCheckState(__NL_STATE_MATRIX) ;
    switch(pname) {
    case NL_RIGHT_HAND_SIDE: {
        __nlCurrentContext->right_hand_side = (NLfloat)param ;
    } break ;
    case NL_ROW_SCALING: {
        __nlCurrentContext->row_scaling = (NLfloat)param ;
    } break ;
    }
}

void nlGetBooleanv(NLenum pname, NLboolean* params) {
    switch(pname) {
    case NL_LEAST_SQUARES: {
        *params = __nlCurrentContext->least_squares ;
    } break ;
    case NL_SYMMETRIC: {
        *params = __nlCurrentContext->symmetric ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    } break ;
    }
}

void nlGetFloatv(NLenum pname, NLfloat* params) {
    switch(pname) {
    case NL_NB_VARIABLES: {
        *params = (NLfloat)(__nlCurrentContext->nb_variables) ;
    } break ;
    case NL_LEAST_SQUARES: {
        *params = (NLfloat)(__nlCurrentContext->least_squares) ;
    } break ;
    case NL_SYMMETRIC: {
        *params = (NLfloat)(__nlCurrentContext->symmetric) ;
    } break ;
    case NL_ERROR: {
        *params = (NLfloat)(__nlCurrentContext->error) ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    } break ;
    }
}

void nlGetIntergerv(NLenum pname, NLint* params) {
    switch(pname) {
    case NL_NB_VARIABLES: {
        *params = (NLint)(__nlCurrentContext->nb_variables) ;
    } break ;
    case NL_LEAST_SQUARES: {
        *params = (NLint)(__nlCurrentContext->least_squares) ;
    } break ;
    case NL_SYMMETRIC: {
        *params = (NLint)(__nlCurrentContext->symmetric) ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    } break ;
    }
}

/************************************************************************************/
/* Enable / Disable */

void nlEnable(NLenum pname) {
    switch(pname) {
    case NL_NORMALIZE_ROWS: {
        __nl_assert(__nlCurrentContext->state != __NL_STATE_ROW) ;
        __nlCurrentContext->normalize_rows = NL_TRUE ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    }
    }
}

void nlDisable(NLenum pname) {
    switch(pname) {
    case NL_NORMALIZE_ROWS: {
        __nl_assert(__nlCurrentContext->state != __NL_STATE_ROW) ;
        __nlCurrentContext->normalize_rows = NL_FALSE ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    }
    }
}

NLboolean nlIsEnabled(NLenum pname) {
    switch(pname) {
    case NL_NORMALIZE_ROWS: {
        return __nlCurrentContext->normalize_rows ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    }
    }
    return NL_FALSE ;
}

/************************************************************************************/
/* Get/Set Lock/Unlock variables */

void nlSetVariable(NLuint index, NLfloat value) {
    __nlCheckState(__NL_STATE_SYSTEM) ;
    __nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1) ;
    __nlCurrentContext->variable[index].value = value ;    
}

NLfloat nlGetVariable(NLuint index) {
    __nl_assert(__nlCurrentContext->state != __NL_STATE_INITIAL) ;
    __nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1) ;
    return __nlCurrentContext->variable[index].value ;
}

void nlLockVariable(NLuint index) {
    __nlCheckState(__NL_STATE_SYSTEM) ;
    __nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1) ;
    __nlCurrentContext->variable[index].locked = NL_TRUE ;
}

void nlUnlockVariable(NLuint index) {
    __nlCheckState(__NL_STATE_SYSTEM) ;
    __nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1) ;
    __nlCurrentContext->variable[index].locked = NL_FALSE ;
}

NLboolean nlVariableIsLocked(NLuint index) {
    __nl_assert(__nlCurrentContext->state != __NL_STATE_INITIAL) ;
    __nl_parano_range_assert(index, 0, __nlCurrentContext->nb_variables - 1) ;
    return __nlCurrentContext->variable[index].locked  ;
}

/************************************************************************************/
/* System construction */

void __nlVariablesToVector() {
    NLuint i ;
    __nl_assert(__nlCurrentContext->alloc_x) ;
    __nl_assert(__nlCurrentContext->alloc_variable) ;
    for(i=0; i<__nlCurrentContext->nb_variables; i++) {
        __NLVariable* v = &(__nlCurrentContext->variable[i]) ;
        if(!v->locked) {
            __nl_assert(v->index < __nlCurrentContext->n) ;
            __nlCurrentContext->x[v->index] = v->value ;
        }
    }
}

void __nlVectorToVariables() {
    NLuint i ;
    __nl_assert(__nlCurrentContext->alloc_x) ;
    __nl_assert(__nlCurrentContext->alloc_variable) ;
    for(i=0; i<__nlCurrentContext->nb_variables; i++) {
        __NLVariable* v = &(__nlCurrentContext->variable[i]) ;
        if(!v->locked) {
            __nl_assert(v->index < __nlCurrentContext->n) ;
            v->value = __nlCurrentContext->x[v->index] ;
        }
    }
}


void __nlBeginSystem() {
    __nlTransition(__NL_STATE_INITIAL, __NL_STATE_SYSTEM) ;
    __nl_assert(__nlCurrentContext->nb_variables > 0) ;
    __nlCurrentContext->variable = __NL_NEW_ARRAY(
        __NLVariable, __nlCurrentContext->nb_variables
    ) ;
    __nlCurrentContext->alloc_variable = NL_TRUE ;
}

void __nlEndSystem() {
    __nlTransition(__NL_STATE_MATRIX_CONSTRUCTED, __NL_STATE_SYSTEM_CONSTRUCTED) ;    
}

void __nlBeginMatrix() {
    NLuint i ;
    NLuint n = 0 ;
    NLenum storage = __NL_ROWS ;

    __nlTransition(__NL_STATE_SYSTEM, __NL_STATE_MATRIX) ;

    for(i=0; i<__nlCurrentContext->nb_variables; i++) {
        if(!__nlCurrentContext->variable[i].locked) {
            __nlCurrentContext->variable[i].index = n ;
            n++ ;
        } else {
            __nlCurrentContext->variable[i].index = ~0 ;
        }
    }

    __nlCurrentContext->n = n ;

    /* a least squares problem results in a symmetric matrix */
    if(__nlCurrentContext->least_squares) {
        __nlCurrentContext->symmetric = NL_TRUE ;
    }

    if(__nlCurrentContext->symmetric) {
        storage = (storage | __NL_SYMMETRIC) ;
    }

    /* SuperLU storage does not support symmetric storage */
    storage = (storage & ~__NL_SYMMETRIC) ;

    __nlSparseMatrixConstruct(&__nlCurrentContext->M, n, n, storage) ;
    __nlCurrentContext->alloc_M = NL_TRUE ;

    __nlCurrentContext->x = __NL_NEW_ARRAY(NLfloat, n) ;
    __nlCurrentContext->alloc_x = NL_TRUE ;
    
    __nlCurrentContext->b = __NL_NEW_ARRAY(NLfloat, n) ;
    __nlCurrentContext->alloc_b = NL_TRUE ;

    __nlVariablesToVector() ;

    __nlRowColumnConstruct(&__nlCurrentContext->af) ;
    __nlCurrentContext->alloc_af = NL_TRUE ;
    __nlRowColumnConstruct(&__nlCurrentContext->al) ;
    __nlCurrentContext->alloc_al = NL_TRUE ;
    __nlRowColumnConstruct(&__nlCurrentContext->xl) ;
    __nlCurrentContext->alloc_xl = NL_TRUE ;

    __nlCurrentContext->current_row = 0 ;
}

void __nlEndMatrix() {
    __nlTransition(__NL_STATE_MATRIX, __NL_STATE_MATRIX_CONSTRUCTED) ;    
    
    __nlRowColumnDestroy(&__nlCurrentContext->af) ;
    __nlCurrentContext->alloc_af = NL_FALSE ;
    __nlRowColumnDestroy(&__nlCurrentContext->al) ;
    __nlCurrentContext->alloc_al = NL_FALSE ;
    __nlRowColumnDestroy(&__nlCurrentContext->xl) ;
    __nlCurrentContext->alloc_al = NL_FALSE ;
    
    if(!__nlCurrentContext->least_squares) {
        __nl_assert(
            __nlCurrentContext->current_row == 
            __nlCurrentContext->n
        ) ;
    }
}

void __nlBeginRow() {
    __nlTransition(__NL_STATE_MATRIX, __NL_STATE_ROW) ;
    __nlRowColumnZero(&__nlCurrentContext->af) ;
    __nlRowColumnZero(&__nlCurrentContext->al) ;
    __nlRowColumnZero(&__nlCurrentContext->xl) ;
}

void __nlScaleRow(NLfloat s) {
    __NLRowColumn*    af = &__nlCurrentContext->af ;
    __NLRowColumn*    al = &__nlCurrentContext->al ;
    NLuint nf            = af->size ;
    NLuint nl            = al->size ;
    NLuint i ;
    for(i=0; i<nf; i++) {
        af->coeff[i].value *= s ;
    }
    for(i=0; i<nl; i++) {
        al->coeff[i].value *= s ;
    }
    __nlCurrentContext->right_hand_side *= s ;
}

void __nlNormalizeRow(NLfloat weight) {
    __NLRowColumn*    af = &__nlCurrentContext->af ;
    __NLRowColumn*    al = &__nlCurrentContext->al ;
    NLuint nf            = af->size ;
    NLuint nl            = al->size ;
    NLuint i ;
    NLfloat norm = 0.0 ;
    for(i=0; i<nf; i++) {
        norm += af->coeff[i].value * af->coeff[i].value ;
    }
    for(i=0; i<nl; i++) {
        norm += al->coeff[i].value * al->coeff[i].value ;
    }
    norm = sqrt(norm) ;
    __nlScaleRow(weight / norm) ;
}

void __nlEndRow() {
    __NLRowColumn*    af = &__nlCurrentContext->af ;
    __NLRowColumn*    al = &__nlCurrentContext->al ;
    __NLRowColumn*    xl = &__nlCurrentContext->xl ;
    __NLSparseMatrix* M  = &__nlCurrentContext->M  ;
    NLfloat* b        = __nlCurrentContext->b ;
    NLuint nf          = af->size ;
    NLuint nl          = al->size ;
    NLuint current_row = __nlCurrentContext->current_row ;
    NLuint i ;
    NLuint j ;
    NLfloat S ;
    __nlTransition(__NL_STATE_ROW, __NL_STATE_MATRIX) ;

    if(__nlCurrentContext->normalize_rows) {
        __nlNormalizeRow(__nlCurrentContext->row_scaling) ;
    } else {
        __nlScaleRow(__nlCurrentContext->row_scaling) ;
    }

    if(__nlCurrentContext->least_squares) {
        for(i=0; i<nf; i++) {
            for(j=0; j<nf; j++) {
                __nlSparseMatrixAdd(
                    M, af->coeff[i].index, af->coeff[j].index,
                    af->coeff[i].value * af->coeff[j].value
                ) ;
            }
        }
        S = -__nlCurrentContext->right_hand_side ;
        for(j=0; j<nl; j++) {
            S += al->coeff[j].value * xl->coeff[j].value ;
        }
        for(i=0; i<nf; i++) {
            b[ af->coeff[i].index ] -= af->coeff[i].value * S ;
        }
    } else {
        for(i=0; i<nf; i++) {
            __nlSparseMatrixAdd(
                M, current_row, af->coeff[i].index, af->coeff[i].value
            ) ;
        }
        b[current_row] = -__nlCurrentContext->right_hand_side ;
        for(i=0; i<nl; i++) {
            b[current_row] -= al->coeff[i].value * xl->coeff[i].value ;
        }
    }
    __nlCurrentContext->current_row++ ;
    __nlCurrentContext->right_hand_side = 0.0 ;    
    __nlCurrentContext->row_scaling     = 1.0 ;
}

void nlCoefficient(NLuint index, NLfloat value) {
    __NLVariable* v;
	unsigned int zero= 0;
    __nlCheckState(__NL_STATE_ROW) ;
    __nl_range_assert(index, zero, __nlCurrentContext->nb_variables - 1) ;
    v = &(__nlCurrentContext->variable[index]) ;
    if(v->locked) {
        __nlRowColumnAppend(&(__nlCurrentContext->al), 0, value) ;
        __nlRowColumnAppend(&(__nlCurrentContext->xl), 0, v->value) ;
    } else {
        __nlRowColumnAppend(&(__nlCurrentContext->af), v->index, value) ;
    }
}

void nlBegin(NLenum prim) {
    switch(prim) {
    case NL_SYSTEM: {
        __nlBeginSystem() ;
    } break ;
    case NL_MATRIX: {
        __nlBeginMatrix() ;
    } break ;
    case NL_ROW: {
        __nlBeginRow() ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    }
    }
}

void nlEnd(NLenum prim) {
    switch(prim) {
    case NL_SYSTEM: {
        __nlEndSystem() ;
    } break ;
    case NL_MATRIX: {
        __nlEndMatrix() ;
    } break ;
    case NL_ROW: {
        __nlEndRow() ;
    } break ;
    default: {
        __nl_assert_not_reached ;
    }
    }
}

/************************************************************************/
/* SuperLU wrapper */

/* Note: SuperLU is difficult to call, but it is worth it.    */
/* Here is a driver inspired by A. Sheffer's "cow flattener". */
static NLboolean __nlSolve_SUPERLU( NLboolean do_perm) {

    /* OpenNL Context */
    __NLSparseMatrix* M  = &(__nlCurrentContext->M) ;
    NLfloat* b          = __nlCurrentContext->b ;
    NLfloat* x          = __nlCurrentContext->x ;

    /* Compressed Row Storage matrix representation */
    NLuint    n      = __nlCurrentContext->n ;
    NLuint    nnz    = __nlSparseMatrixNNZ(M) ; /* Number of Non-Zero coeffs */
    NLint*    xa     = __NL_NEW_ARRAY(NLint, n+1) ;
    NLfloat* rhs    = __NL_NEW_ARRAY(NLfloat, n) ;
    NLfloat* a      = __NL_NEW_ARRAY(NLfloat, nnz) ;
    NLint*    asub   = __NL_NEW_ARRAY(NLint, nnz) ;

    /* Permutation vector */
    NLint*    perm_r  = __NL_NEW_ARRAY(NLint, n) ;
    NLint*    perm    = __NL_NEW_ARRAY(NLint, n) ;

    /* SuperLU variables */
    SuperMatrix A, B ; /* System       */
    SuperMatrix L, U ; /* Inverse of A */
    NLint info ;       /* status code  */
    DNformat *vals = NULL ; /* access to result */
    float *rvals  = NULL ; /* access to result */

    /* SuperLU options and stats */
    superlu_options_t options ;
    SuperLUStat_t     stat ;


    /* Temporary variables */
    __NLRowColumn* Ri = NULL ;
    NLuint         i,jj,count ;
    
    __nl_assert(!(M->storage & __NL_SYMMETRIC)) ;
    __nl_assert(M->storage & __NL_ROWS) ;
    __nl_assert(M->m == M->n) ;
    
    
    /*
     * Step 1: convert matrix M into SuperLU compressed column 
     *   representation.
     * -------------------------------------------------------
     */

    count = 0 ;
    for(i=0; i<n; i++) {
        Ri = &(M->row[i]) ;
        xa[i] = count ;
        for(jj=0; jj<Ri->size; jj++) {
            a[count]    = Ri->coeff[jj].value ;
            asub[count] = Ri->coeff[jj].index ;
            count++ ;
        }
    }
    xa[n] = nnz ;

    /* Save memory for SuperLU */
    __nlSparseMatrixClear(M) ;


    /*
     * Rem: symmetric storage does not seem to work with
     * SuperLU ... (->deactivated in main SLS::Solver driver)
     */
    sCreate_CompCol_Matrix(
        &A, n, n, nnz, a, asub, xa, 
        SLU_NR,              /* Row_wise, no supernode */
        SLU_S,               /* floats                */ 
        SLU_GE               /* general storage        */
    );

    /* Step 2: create vector */
    sCreate_Dense_Matrix(
        &B, n, 1, b, n, 
        SLU_DN, /* Fortran-type column-wise storage */
        SLU_S,  /* floats                          */
        SLU_GE  /* general                          */
    );
            

    /* Step 3: get permutation matrix 
     * ------------------------------
     * com_perm: 0 -> no re-ordering
     *           1 -> re-ordering for A^t.A
     *           2 -> re-ordering for A^t+A
     *           3 -> approximate minimum degree ordering
     */
    get_perm_c(do_perm ? 3 : 0, &A, perm) ;

    /* Step 4: call SuperLU main routine
     * ---------------------------------
     */

    set_default_options(&options) ;
    options.ColPerm = MY_PERMC ;
    StatInit(&stat) ;

    sgssv(&options, &A, perm, perm_r, &L, &U, &B, &stat, &info);

    /* Step 5: get the solution
     * ------------------------
     * Fortran-type column-wise storage
     */
    vals = (DNformat*)B.Store;
    rvals = (float*)(vals->nzval);
    if(info == 0) {
        for(i = 0; i <  n; i++){
            x[i] = rvals[i];
        }
    }

    /* Step 6: cleanup
     * ---------------
     */

    /*
     *  For these two ones, only the "store" structure
     * needs to be deallocated (the arrays have been allocated
     * by us).
     */
    Destroy_SuperMatrix_Store(&A) ;
    Destroy_SuperMatrix_Store(&B) ;

    
    /*
     *   These ones need to be fully deallocated (they have been
     * allocated by SuperLU).
     */
    Destroy_SuperNode_Matrix(&L);
    Destroy_CompCol_Matrix(&U);

    __NL_DELETE_ARRAY(xa) ;
    __NL_DELETE_ARRAY(rhs) ;
    __NL_DELETE_ARRAY(a) ;
    __NL_DELETE_ARRAY(asub) ;
    __NL_DELETE_ARRAY(perm_r) ;
    __NL_DELETE_ARRAY(perm) ;

    return (info == 0) ;
}


/************************************************************************/
/* nlSolve() driver routine */

NLboolean nlSolve() {
    NLboolean result = NL_TRUE ;

    __nlCheckState(__NL_STATE_SYSTEM_CONSTRUCTED) ;
    result = __nlSolve_SUPERLU(NL_TRUE) ;

    __nlVectorToVariables() ;
    __nlTransition(__NL_STATE_SYSTEM_CONSTRUCTED, __NL_STATE_SOLVED) ;

    return result ;
}

