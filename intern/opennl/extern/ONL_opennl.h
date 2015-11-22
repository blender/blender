/** \file opennl/extern/ONL_opennl.h
 *  \ingroup opennlextern
 */
/*
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

#ifndef nlOPENNL_H
#define nlOPENNL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Datatypes */

typedef unsigned int	NLenum;
typedef unsigned char	NLboolean;
typedef int				NLint;		/* 4-byte signed */
typedef unsigned int	NLuint;		/* 4-byte unsigned */
typedef double			NLdouble;	/* double precision float */

typedef struct NLContext NLContext;

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
#define NL_ERROR               0x108
#define NL_NB_ROWS             0x110
#define NL_NB_RIGHT_HAND_SIDES 0x112 /* 4 max */

/* Contexts */

NLContext *nlNewContext(void);
void nlDeleteContext(NLContext *context);

/* State get/set */

void nlSolverParameteri(NLContext *context, NLenum pname, NLint param);

/* Variables */

void nlSetVariable(NLContext *context, NLuint rhsindex, NLuint index, NLdouble value);
NLdouble nlGetVariable(NLContext *context, NLuint rhsindex, NLuint index);
void nlLockVariable(NLContext *context, NLuint index);
void nlUnlockVariable(NLContext *context, NLuint index);

/* Begin/End */

void nlBegin(NLContext *context, NLenum primitive);
void nlEnd(NLContext *context, NLenum primitive);

/* Setting elements in matrix/vector */

void nlMatrixAdd(NLContext *context, NLuint row, NLuint col, NLdouble value);
void nlRightHandSideAdd(NLContext *context, NLuint rhsindex, NLuint index, NLdouble value);
void nlRightHandSideSet(NLContext *context, NLuint rhsindex, NLuint index, NLdouble value);

/* Solve */

void nlPrintMatrix(NLContext *context);
NLboolean nlSolve(NLContext *context, NLboolean solveAgain);

#ifdef __cplusplus
}
#endif

#endif

