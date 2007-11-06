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

/*
#define NL_DEBUG
#define NL_PARANOID
*/

#define NL_USE_SUPERLU

#ifndef nlOPENNL_H
#define nlOPENNL_H

#ifdef __cplusplus
extern "C" {
#endif

#define NL_VERSION_0_0 1

/* Datatypes */

typedef unsigned int	NLenum;
typedef unsigned char	NLboolean;
typedef unsigned int	NLbitfield;
typedef void			NLvoid;
typedef signed char		NLbyte;		/* 1-byte signed */
typedef short			NLshort;	/* 2-byte signed */
typedef int				NLint;		/* 4-byte signed */
typedef unsigned char	NLubyte;	/* 1-byte unsigned */
typedef unsigned short	NLushort;	/* 2-byte unsigned */
typedef unsigned int	NLuint;		/* 4-byte unsigned */
typedef int				NLsizei;	/* 4-byte signed */
typedef float			NLfloat;	/* single precision float */
typedef double			NLdouble;	/* double precision float */

typedef void* NLContext;

/* Constants */

#define NL_FALSE   0x0
#define NL_TRUE    0x1

/* Primitives */

#define NL_SYSTEM  0x0
#define NL_MATRIX  0x1

/* Solver Parameters */

#define NL_SOLVER              0x100
#define NL_NB_VARIABLES        0x101
#define NL_LEAST_SQUARES       0x102
#define NL_SYMMETRIC           0x106
#define NL_ERROR               0x108
#define NL_NB_ROWS             0x110
#define NL_NB_RIGHT_HAND_SIDES 0x112 /* 4 max */

/* Contexts */

NLContext nlNewContext(void);
void nlDeleteContext(NLContext context);
void nlMakeCurrent(NLContext context);
NLContext nlGetCurrent(void);

/* State get/set */

void nlSolverParameterf(NLenum pname, NLfloat param);
void nlSolverParameteri(NLenum pname, NLint param);

void nlGetBooleanv(NLenum pname, NLboolean* params);
void nlGetFloatv(NLenum pname, NLfloat* params);
void nlGetIntergerv(NLenum pname, NLint* params);

void nlEnable(NLenum pname);
void nlDisable(NLenum pname);
NLboolean nlIsEnabled(NLenum pname);

/* Variables */

void nlSetVariable(NLuint rhsindex, NLuint index, NLfloat value);
NLfloat nlGetVariable(NLuint rhsindex, NLuint index);
void nlLockVariable(NLuint index);
void nlUnlockVariable(NLuint index);
NLboolean nlVariableIsLocked(NLuint index);

/* Begin/End */

void nlBegin(NLenum primitive);
void nlEnd(NLenum primitive);

/* Setting elements in matrix/vector */

void nlMatrixAdd(NLuint row, NLuint col, NLfloat value);
void nlRightHandSideAdd(NLuint rhsindex, NLuint index, NLfloat value);
void nlRightHandSideSet(NLuint rhsindex, NLuint index, NLfloat value);

/* Multiply */

void nlMatrixMultiply(NLfloat *x, NLfloat *y);

/* Solve */

void nlPrintMatrix(void);
NLboolean nlSolve();
NLboolean nlSolveAdvanced(NLint *permutation, NLboolean solveAgain);

#ifdef __cplusplus
}
#endif

#endif

