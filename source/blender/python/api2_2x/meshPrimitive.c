/* 
 * $Id: meshPrimitive.c 10773 2007-05-24 15:00:10Z khughes $
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender, partially based on NMesh.c API.
 *
 * Contributor(s): Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Mesh.h" /*This must come first*/

#include "DNA_scene_types.h"
#include "BDR_editobject.h"
#include "BIF_editmesh.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "blendef.h"

#include "gen_utils.h"

/*
 * local helper procedure which does the dirty work of messing with the
 * edit mesh, active objects, etc.
 */

static PyObject *make_mesh( int type, char *name, short tot, short seg,
		short subdiv, float dia, float height, short ext, short fill )
{
	float cent[3] = {0,0,0};
	float imat[3][3]={{1,0,0},{0,1,0},{0,0,1}};
	Mesh *me;
	BPy_Mesh *obj;
	Object *ob;
	Base *base;

	/* remember active object (if any) for later, so we can re-activate */
	base = BASACT;

	/* make a new object, "copy" to the editMesh structure */
	ob = add_object(OB_MESH);
	me = (Mesh *)ob->data;
	G.obedit = BASACT->object;
	make_editMesh( );

	/* create the primitive in the edit mesh */

	make_prim( type, imat,	/* mesh type, transform matrix */
		tot, seg, 			/* total vertices, segments */
		subdiv, 			/* subdivisions (for Icosphere only) */
		dia, -height,		/* diameter-ish, height */
		ext, fill, 			/* extrude, fill end faces */
		cent );				/* location of center */

	/* copy primitive back to the real mesh */
	load_editMesh( );
	free_editMesh( G.editMesh );
	G.obedit = NULL;

	/* remove object link to the data, then delete the object */
	ob->data = NULL;
	me->id.us = 0;
	free_and_unlink_base(BASACT);

	/* if there was an active object, reactivate it */
	if( base )
		scene_select_base(G.scene, base);

	/* create the BPy_Mesh that wraps this mesh */
	obj = (BPy_Mesh *)PyObject_NEW( BPy_Mesh, &Mesh_Type );

	rename_id( &me->id, name );
	obj->mesh = me;
	obj->object = NULL;
	obj->new = 1;
	return (PyObject *) obj;
}

static PyObject *M_MeshPrim_Plane( PyObject *self_unused, PyObject *args )
{
	float size = 2.0;

	if( !PyArg_ParseTuple( args, "|f", &size ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected optional float arg" );

	size *= (float)(sqrt(2.0)/2.0);
	return make_mesh( 0, "Plane", 4, 0, 0, size, -size, 0, 1 );
}

static PyObject *M_MeshPrim_Cube( PyObject *self_unused, PyObject *args )
{
	float height = 2.0;
	float dia;

	if( !PyArg_ParseTuple( args, "|f", &height ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected optional float arg" );

	height /= 2.0;
	dia = height * (float)sqrt(2.0);
	return make_mesh( 1, "Cube", 4, 32, 2, dia, -height, 1, 1 );
}

static PyObject *M_MeshPrim_Circle( PyObject *self_unused, PyObject *args )
{
	int tot = 32;
	float size = 2;

	if( !PyArg_ParseTuple( args, "|if", &tot, &size ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int and optional float arg" );
	if( tot < 3 || tot > 100 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"number of vertices must be in the range [3:100]" );

	size /= 2.0;
	return make_mesh( 4, "Circle", tot, 0, 0, size, -size, 0, 0 );
}

static PyObject *M_MeshPrim_Cylinder( PyObject *self_unused, PyObject *args )
{
	int tot = 32;
	float size = 2.0;
	float len = 2.0;

	if( !PyArg_ParseTuple( args, "|iff", &tot, &size, &len ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int and optional float arg" );
	if( tot < 3 || tot > 100 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"number of vertices must be in the range [3:100]" );

	return make_mesh( 5, "Cylinder", tot, 0, 0, size/2.0, -len/2.0, 1, 1 );
}

static PyObject *M_MeshPrim_Tube( PyObject *self_unused, PyObject *args )
{
	int tot = 32;
	float size = 2.0;
	float len = 2.0;

	if( !PyArg_ParseTuple( args, "|iff", &tot, &size, &len ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int and optional float arg" );
	if( tot < 3 || tot > 100 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"number of vertices must be in the range [3:100]" );

	return make_mesh( 6, "Tube", tot, 0, 0, size/2.0, -len/2.0, 1, 0 );
}

static PyObject *M_MeshPrim_Cone( PyObject *self_unused, PyObject *args )
{
	int tot = 32;
	float size = 2.0;
	float len = 2.0;

	if( !PyArg_ParseTuple( args, "|iff", &tot, &size, &len ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int and optional float arg" );
	if( tot < 3 || tot > 100 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"number of vertices must be in the range [3:100]" );

	return make_mesh( 7, "Cone", tot, 0, 0, size/2.0, -len/2.0, 0, 1 );
}

static PyObject *M_MeshPrim_Grid( PyObject *self_unused, PyObject *args )
{
	int xres = 32;
	int yres = 32;
	float size = 2.0;

	if( !PyArg_ParseTuple( args, "|iif", &xres, &yres, &size ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two ints and an optional float arg" );
	if( xres < 2 || xres > 100 || yres < 2 || yres > 100 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"resolution must be in the range [2:100]" );

	size /= 2.0;
	return make_mesh( 10, "Grid", xres, yres, 0, size, -size, 0, 0 );
}

static PyObject *M_MeshPrim_UVsphere( PyObject *self_unused, PyObject *args )
{
	int segs = 32;
	int rings = 32;
	float size = 2.0;

	if( !PyArg_ParseTuple( args, "|iif", &segs, &rings, &size ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two ints and an optional float arg" );
	if( segs < 3 || segs > 100 || rings < 3 || rings > 100 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"segments and rings must be in the range [3:100]" );

	size /= 2.0;
	return make_mesh( 11, "UVsphere", segs, rings, 0, size, -size, 0, 0 );
}

static PyObject *M_MeshPrim_Icosphere( PyObject *self_unused, PyObject *args )
{
	int subdiv = 2;
	float size = 2.0;

	if( !PyArg_ParseTuple( args, "|if", &subdiv, &size ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int and an optional float arg" );
	if( subdiv < 1 || subdiv > 5 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"subdivisions must be in the range [1:5]" );

	size /= 2.0;
	return make_mesh( 12, "Icosphere", 0, 0, subdiv, size, -size, 0, 0 );
}

static PyObject *M_MeshPrim_Suzanne( PyObject *self_unused, PyObject *args )
{
	return make_mesh( 13, "Monkey", 0, 0, 0, 0, 0, 0, 0 );
}

static struct PyMethodDef M_MeshPrim_methods[] = {
	{"Plane", (PyCFunction)M_MeshPrim_Plane, METH_VARARGS,
		"Create a plane mesh"},
	{"Cube", (PyCFunction)M_MeshPrim_Cube, METH_VARARGS,
		"Create a cube mesh"},
	{"Circle", (PyCFunction)M_MeshPrim_Circle, METH_VARARGS,
		"Create a circle mesh"},
	{"Cylinder", (PyCFunction)M_MeshPrim_Cylinder, METH_VARARGS,
		"Create a cylindrical mesh"},
	{"Tube", (PyCFunction)M_MeshPrim_Tube, METH_VARARGS,
		"Create a tube mesh"},
	{"Cone", (PyCFunction)M_MeshPrim_Cone, METH_VARARGS,
		"Create a conic mesh"},
	{"Grid", (PyCFunction)M_MeshPrim_Grid, METH_VARARGS,
		"Create a 2D grid mesh"},
	{"UVsphere", (PyCFunction)M_MeshPrim_UVsphere, METH_VARARGS,
		"Create a UV sphere mesh"},
	{"Icosphere", (PyCFunction)M_MeshPrim_Icosphere, METH_VARARGS,
		"Create a Ico sphere mesh"},
	{"Monkey", (PyCFunction)M_MeshPrim_Suzanne, METH_NOARGS,
		"Create a Suzanne mesh"},
	{NULL, NULL, 0, NULL},
};

static char M_MeshPrim_doc[] = "The Blender.Mesh.Primitives submodule";

PyObject *MeshPrimitives_Init( void )
{
	return Py_InitModule3( "Blender.Mesh.Primitives",
				M_MeshPrim_methods, M_MeshPrim_doc );
}

