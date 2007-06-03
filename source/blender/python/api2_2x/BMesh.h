/* 
 * $Id: BMesh.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_BMESH_H
#define EXPP_BMESH_H

#include <Python.h>
#include "BKE_bmesh.h"

/* The BMesh PyTypeObject defined in BMesh.c */
extern PyTypeObject BMesh_Type;
extern PyTypeObject BMesh_Vert_Type;
extern PyTypeObject BMesh_Edge_Type;
extern PyTypeObject BMesh_Loop_Type;
extern PyTypeObject BMesh_Poly_Type;
extern PyTypeObject BMesh_VertSeq_Type;
extern PyTypeObject BMesh_EdgeSeq_Type;
extern PyTypeObject BMesh_LoopSeq_Type;
extern PyTypeObject BMesh_PolySeq_Type;


#define BPy_BMesh_Check(v)       ((v)->ob_type == &BMesh_Type)

#define BPy_BMesh_Vert_Check(v)       ((v)->ob_type == &BMesh_Vert_Type)
#define BPy_BMesh_Edge_Check(v)       ((v)->ob_type == &BMesh_Edge_Type)
#define BPy_BMesh_Loop_Check(v)       ((v)->ob_type == &BMesh_Loop_Type)
#define BPy_BMesh_Poly_Check(v)       ((v)->ob_type == &BMesh_Poly_Type)

#define BPy_BMesh_VertSeq_Check(v)    ((v)->ob_type == &BMesh_VertSeq_Type)
#define BPy_BMesh_EdgeSeq_Check(v)    ((v)->ob_type == &BMesh_EdgeSeq_Type)
#define BPy_BMesh_LoopSeq_Check(v)    ((v)->ob_type == &BMesh_LoopSeq_Type)
#define BPy_BMesh_PolySeq_Check(v)    ((v)->ob_type == &BMesh_PolySeq_Type)

/*****************************************************************************/
/* Python BPy_Group structure definition.                                  */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD
	struct BME_Mesh *bmesh; 
} BPy_BMesh;

/* vert/egde/poly */
typedef struct {
	PyObject_HEAD
	struct BME_Vert *bvert; 
} BPy_BMesh_Vert;

typedef struct {
	PyObject_HEAD
	struct BME_Edge *bedge; 
} BPy_BMesh_Edge;

typedef struct {
	PyObject_HEAD
	struct BME_Loop *bloop; 
} BPy_BMesh_Loop;

typedef struct {
	PyObject_HEAD
	struct BME_Mesh *bmesh;
	struct BME_Poly *bpoly; 
} BPy_BMesh_Poly;

/* vert/egde/poly - sequence types
 * 
 * Make sure the bmesh is always the first element
 * so they can be cast to BPy_BMesh
 */
typedef struct {
	PyObject_HEAD
	struct BME_Mesh *bmesh;
	struct BME_Vert *iter;
	long mode;
} BPy_BMesh_VertSeq;

typedef struct {
	PyObject_HEAD
	struct BME_Mesh *bmesh;
	struct BME_Edge *iter;
	long mode;
} BPy_BMesh_EdgeSeq;

typedef struct { /* This dosnt refer back to its bmesh*/
	PyObject_HEAD
	struct BME_Loop *iter_init;
	struct BME_Loop *iter;
	long mode;
} BPy_BMesh_LoopSeq;

typedef struct {
	PyObject_HEAD
	struct BME_Mesh *bmesh;
	struct BME_Poly *iter;
	long mode;
} BPy_BMesh_PolySeq;

PyObject *BMesh_Init( void );
PyObject *BMesh_CreatePyObject( struct BME_Mesh *bmesh );
BME_Mesh *BMesh_FromPyObject( PyObject * py_obj );

#endif				/* EXPP_BMESH_H */
