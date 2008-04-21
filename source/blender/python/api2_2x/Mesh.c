/* 
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender, partially based on NMesh.c API.
 *
 * Contributor(s): Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Mesh.h" /*This must come first*/

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_oops_types.h"
#include "DNA_space_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"

#include "BDR_editface.h"	/* make_tfaces */
#include "BDR_vpaint.h"
#include "BDR_editobject.h"

#include "BIF_editdeform.h"
#include "BIF_editkey.h"	/* insert_meshkey */
#include "BIF_space.h"		/* REMAKEIPO - insert_meshkey */
#include "BIF_editview.h"
#include "BIF_editmesh.h"
#include "BIF_meshtools.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_multires.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_mball.h"
#include "BKE_utildefines.h"
#include "BKE_depsgraph.h"
#include "BSE_edit.h"		/* for countall(); */
#include "BKE_curve.h"		/* for copy_curve(); */
#include "BKE_modifier.h"	/* for modifier_new(), modifier_copyData(); */
#include "BKE_idprop.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_memarena.h"

#include "blendef.h"
#include "mydevice.h"
#include "butspace.h"		/* for mesh tools */
#include "Object.h"
#include "Key.h"
#include "Image.h"
#include "Material.h"
#include "Mathutils.h"
#include "IDProp.h"
#include "meshPrimitive.h"
#include "constant.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "multires.h"

/* EXPP Mesh defines */

#define MESH_SMOOTHRESH               30
#define MESH_SMOOTHRESH_MIN            1
#define MESH_SMOOTHRESH_MAX           80
#define MESH_SUBDIV                    1
#define MESH_SUBDIV_MIN                0
#define MESH_SUBDIV_MAX                6

#define MESH_HASFACEUV                 0
#define MESH_HASMCOL                   1
#define MESH_HASVERTUV                 2
#define MESH_HASMULTIRES               3

#define MESH_MULTIRES_LEVEL            0
#define MESH_MULTIRES_EDGE             1
#define MESH_MULTIRES_PIN              2
#define MESH_MULTIRES_RENDER           3

#define MESH_TOOL_TOSPHERE             0
#define MESH_TOOL_VERTEXSMOOTH         1
#define MESH_TOOL_FLIPNORM             2
#define MESH_TOOL_SUBDIV               3
#define MESH_TOOL_REMDOUB              4
#define MESH_TOOL_FILL                 5
#define MESH_TOOL_RECALCNORM           6
#define MESH_TOOL_TRI2QUAD             7
#define MESH_TOOL_QUAD2TRI             8

static PyObject *MVertSeq_CreatePyObject( Mesh * mesh );
static PyObject *MFaceSeq_CreatePyObject( Mesh * mesh );
static PyObject *MEdgeSeq_CreatePyObject( Mesh * mesh );
static PyObject *MFace_CreatePyObject( Mesh * mesh, int i );
static PyObject *MEdge_CreatePyObject( Mesh * mesh, int i );

#define MFACE_VERT_BADRANGE_CHECK(me, face) ((int)face->v1 >= me->totvert || (int)face->v2 >= me->totvert || (int)face->v3 >= me->totvert || (int)face->v4 >= me->totvert)
#define MEDGE_VERT_BADRANGE_CHECK(me, edge) ((int)edge->v1 >= me->totvert || (int)edge->v2 >= me->totvert)

/************************************************************************
 *
 * internal utilities
 *
 ************************************************************************/

/*
 * internal structures used for sorting edges and faces
 */

typedef struct SrchEdges {
	unsigned int v[2];		/* indices for verts */
	unsigned char swap;		/* non-zero if verts swapped */
	unsigned int index;		/* index in original param list of this edge */
							/* (will be used by findEdges) */
} SrchEdges;

typedef struct SrchFaces {
	unsigned int v[4];		/* indices for verts */
	unsigned int index;		/* index in original param list of this edge */
	unsigned char order;	/* order of original verts, bitpacked */
} SrchFaces;

typedef struct FaceEdges {
	unsigned int v[2];		/* search key (vert indices) */
	unsigned int index;		/* location in edge list */
	unsigned char sel;		/* selection state */
} FaceEdges;

/*
 * compare edges by vertex indices
 */

int medge_comp( const void *va, const void *vb )
{
	const unsigned int *a = ((SrchEdges *)va)->v;
	const unsigned int *b = ((SrchEdges *)vb)->v;

	/* compare first index for differences */

	if (a[0] < b[0]) return -1;	
	else if (a[0] > b[0]) return 1;

	/* if first indices equal, compare second index for differences */

	else if (a[1] < b[1]) return -1;
	else return (a[1] > b[1]);
}

/*
 * compare edges by insert list indices
 */

int medge_index_comp( const void *va, const void *vb )
{
	const SrchEdges *a = (SrchEdges *)va;
	const SrchEdges *b = (SrchEdges *)vb;

	/* compare list indices for differences */

	if (a->index < b->index) return -1;
	else return (a->index > b->index);
}


/*
 * compare faces by vertex indices
 */

int mface_comp( const void *va, const void *vb )
{
	const SrchFaces *a = va;
	const SrchFaces *b = vb;
	int i;

	/* compare indices, first to last, for differences */
	for( i = 0; i < 4; ++i ) {
		if( a->v[i] < b->v[i] )
			return -1;	
		if( a->v[i] > b->v[i] )
			return 1;
	}

	/*
	 * don't think this needs be done; if order is different then either
	 * (a) the face is good, just reversed or has a different starting
	 * vertex, or (b) face is bad (for 4 verts) and there's a "twist"
	 */

#if 0
	/* if all the same verts, compare their order */
	if( a->order < b->order )
		return -1;	
	if( a->order > b->order )
		return 1;	
#endif

	return 0;
}

/*
 * compare faces by insert list indices
 */

int mface_index_comp( const void *va, const void *vb )
{
	const SrchFaces *a = va;
	const SrchFaces *b = vb;

	/* compare indices, first to last, for differences */
	if( a->index < b->index )
		return -1;	
	if( a->index > b->index )
		return 1;
	return 0;
}

/*
 * compare edges by vertex indices
 */

int faceedge_comp( const void *va, const void *vb )
{
	const unsigned int *a = ((FaceEdges *)va)->v;
	const unsigned int *b = ((FaceEdges *)vb)->v;

	/* compare first index for differences */

	if (a[0] < b[0]) return -1;	
	else if (a[0] > b[0]) return 1;

	/* if first indices equal, compare second index for differences */

	else if (a[1] < b[1]) return -1;
	else return (a[1] > b[1]);
}

/*
 * update the DAG for all objects linked to this mesh
 */

static void mesh_update( Mesh * mesh )
{
	Object_updateDag( (void *) mesh );
}

/*
 * delete vertices from mesh, then delete edges/keys/faces which used those
 * vertices
 *
 * Deletion is done by "smart compaction"; groups of verts/edges/faces which
 * remain in the list are copied to new list instead of one at a time.  Since
 * Blender has no realloc we would have to copy things anyway, so there's no
 * point trying to fill empty entries with data from the end of the lists.
 *
 * vert_table is a lookup table for mapping old verts to new verts (after the
 * vextex list has deleted vertices removed).  Each entry contains the
 * vertex's new index.
 */

static void delete_verts( Mesh *mesh, unsigned int *vert_table, int to_delete )
{
	/*
	 * (1) allocate vertex table (initialize contents to 0)
	 * (2) mark each vertex being deleted in vertex table (= UINT_MAX)
	 * (3) update remaining table entries with "new" vertex index (after
	 *     compaction)
	 * (4) allocate new vertex list
	 * (5) do "smart copy" of vertices from old to new
	 *     * each moved vertex is entered into vertex table: if vertex i is
	 *       moving to index j in new list
	 *       vert_table[i] = j;
	 * (6) if keys, do "smart copy" of keys
	 * (7) process edge list
	 *      update vert index
	 *      delete edges which delete verts
	 * (7) allocate new edge list
	 * (8) do "smart copy" of edges
	 * (9) allocate new face list
	 * (10) do "smart copy" of face
	 */

	unsigned int *tmpvert;
	CustomData vdata;
	int i, count, state, dstindex, totvert;

	totvert = mesh->totvert - to_delete;
	CustomData_copy( &mesh->vdata, &vdata, CD_MASK_MESH, CD_CALLOC, totvert );

	/*
	 * do "smart compaction" of the table; find and copy groups of vertices
	 * which are not being deleted
	 */

	dstindex = 0;
	tmpvert = vert_table;
	count = 0;
	state = 1;
	for( i = 0; i < mesh->totvert; ++i, ++tmpvert ) {
		switch( state ) {
		case 0:		/* skipping verts */
			if( *tmpvert == UINT_MAX ) {
				++count;
			} else {
				count = 1;
				state = 1;
			}
			break;
		case 1:		/* gathering verts */
			if( *tmpvert != UINT_MAX ) {
				++count;
			} else {
				if( count ) {
					CustomData_copy_data( &mesh->vdata, &vdata, i-count,
						dstindex, count );
					dstindex += count;
				}
				count = 1;
				state = 0;
			}
		}
	}

	/* if we were gathering verts at the end of the loop, copy those */
	if( state && count )
		CustomData_copy_data( &mesh->vdata, &vdata, i-count, dstindex, count );

	/* delete old vertex list, install the new one, update vertex count */
	CustomData_free( &mesh->vdata, mesh->totvert );
	mesh->vdata = vdata;
	mesh->totvert = totvert;
	mesh_update_customdata_pointers( mesh );
}

static void delete_edges( Mesh *mesh, unsigned int *vert_table, int to_delete )
{
	int i;
	MEdge *tmpedge;

	/* if not given, then mark and count edges to be deleted */
	if( !to_delete ) {
		tmpedge = mesh->medge;
		for( i = mesh->totedge; i-- ; ++tmpedge )
			if( vert_table[tmpedge->v1] == UINT_MAX ||
					vert_table[tmpedge->v2] == UINT_MAX ) {
				tmpedge->v1 = UINT_MAX;
				++to_delete;
			}
	}

	/* if there are edges to delete, handle it */
	if( to_delete ) {
		CustomData edata;
		int count, state, dstindex, totedge;
		
	/* allocate new edge list and populate */
		totedge = mesh->totedge - to_delete;
		CustomData_copy( &mesh->edata, &edata, CD_MASK_MESH, CD_CALLOC, totedge);

	/*
	 * do "smart compaction" of the edges; find and copy groups of edges
	 * which are not being deleted
	 */

		dstindex = 0;
		tmpedge = mesh->medge;
		count = 0;
		state = 1;
		for( i = 0; i < mesh->totedge; ++i, ++tmpedge ) {
			switch( state ) {
			case 0:		/* skipping edges */
				if( tmpedge->v1 == UINT_MAX ) {
					++count;
				} else {
					count = 1;
					state = 1;
				}
				break;
			case 1:		/* gathering edges */
				if( tmpedge->v1 != UINT_MAX ) {
					++count;
				} else {
					if( count ) {
						CustomData_copy_data( &mesh->edata, &edata, i-count,
							dstindex, count );
						dstindex += count;
					}
					count = 1;
					state = 0;
				}
			}
		/* if edge is good, update vertex indices */
		}

	/* copy any pending good edges */
		if( state && count )
			CustomData_copy_data( &mesh->edata, &edata, i-count, dstindex,
				count );

	/* delete old edge list, install the new one, update vertex count */
		CustomData_free( &mesh->edata, mesh->totedge );
		mesh->edata = edata;
		mesh->totedge = totedge;
		mesh_update_customdata_pointers( mesh );
	}

	/* if vertices were deleted, update edge's vertices */
	if( vert_table ) {
		tmpedge = mesh->medge;
		for( i = mesh->totedge; i--; ++tmpedge ) {
			tmpedge->v1 = vert_table[tmpedge->v1];
			tmpedge->v2 = vert_table[tmpedge->v2];
		}
	}
}

/*	 
* Since all faces must have 3 or 4 verts, we can't have v3 or v4 be zero.	 
* If that happens during the deletion, we have to shuffle the vertices	 
* around; otherwise it can cause an Eeekadoodle or worse.  If there are	 
* texture faces as well, they have to be shuffled as well.	 
*	 
* (code borrowed from test_index_face() in mesh.c, but since we know the	 
* faces already have correct number of vertices, this is a little faster)	 
*/	 
 
static void eeek_fix( MFace *mface, int len4 )
{
	/* if 4 verts, then neither v3 nor v4 can be zero */
	if( len4 ) {
		if( !mface->v3 || !mface->v4 ) {
			SWAP( int, mface->v1, mface->v3 );
			SWAP( int, mface->v2, mface->v4 );
		}
	} else if( !mface->v3 ) {
		/* if 2 verts, then just v3 cannot be zero (v4 MUST be zero) */
		SWAP( int, mface->v1, mface->v2 );
		SWAP( int, mface->v2, mface->v3 );
	}
}

static void delete_faces( Mesh *mesh, unsigned int *vert_table, int to_delete )
{
	int i;
	MFace *tmpface;

		/* if there are faces to delete, handle it */
	if( to_delete ) {
		CustomData fdata;
		int count, state, dstindex, totface;
		
		totface = mesh->totface - to_delete;
		CustomData_copy( &mesh->fdata, &fdata, CD_MASK_MESH, CD_CALLOC, totface );

		/*
		 * do "smart compaction" of the faces; find and copy groups of faces
		 * which are not being deleted
		 */

		dstindex = 0;
		tmpface = mesh->mface;

		count = 0;
		state = 1;
		for( i = 0; i < mesh->totface; ++i ) {
			switch( state ) {
			case 0:		/* skipping faces */
				if( tmpface->v1 == UINT_MAX ) {
					++count;
				} else {
					count = 1;
					state = 1;
				}
				break;
			case 1:		/* gathering faces */
				if( tmpface->v1 != UINT_MAX ) {
					++count;
				} else {
					if( count ) {
						CustomData_copy_data( &mesh->fdata, &fdata, i-count,
							dstindex, count );
						dstindex += count;
					}
					count = 1;
					state = 0;
				}
			}
			++tmpface; 
		}

	/* if we were gathering faces at the end of the loop, copy those */
		if ( state && count )
			CustomData_copy_data( &mesh->fdata, &fdata, i-count, dstindex,
				count );

	/* delete old face list, install the new one, update face count */

		CustomData_free( &mesh->fdata, mesh->totface );
		mesh->fdata = fdata;
		mesh->totface = totface;
		mesh_update_customdata_pointers( mesh );
	}

	/* if vertices were deleted, update face's vertices */
	if( vert_table ) {
		tmpface = mesh->mface;

		for( i = 0; i<mesh->totface; ++i, ++tmpface ) {
			int len4 = tmpface->v4;
			tmpface->v1 = vert_table[tmpface->v1];
			tmpface->v2 = vert_table[tmpface->v2];
			tmpface->v3 = vert_table[tmpface->v3];
			if(len4)
				tmpface->v4 = vert_table[tmpface->v4];
			else
				tmpface->v4 = 0;

			test_index_face( tmpface, &mesh->fdata, i, len4? 4: 3);
		}
	}
}

/*
 * fill up vertex lookup table with old-to-new mappings
 *
 * returns the number of vertices marked for deletion
 */

static unsigned int make_vertex_table( unsigned int *vert_table, int count )
{
	int i;
	unsigned int *tmpvert = vert_table;
	unsigned int to_delete = 0;
	unsigned int new_index = 0;

	/* fill the lookup table with old->new index mappings */
	for( i = count; i; --i, ++tmpvert ) {
		if( *tmpvert == UINT_MAX ) {
			++to_delete;
		} else {
			*tmpvert = new_index;
			++new_index;
		}
	}
	return to_delete;
}


/************************************************************************
 *
 * Color attributes
 *
 ************************************************************************/

/*
 * get a color attribute
 */

static PyObject *MCol_getAttr( BPy_MCol * self, void *type )
{
	unsigned char param;

	switch( (long)type ) {
    case 'R':	/* these are backwards, but that how it works */
		param = self->color->b;
		break;
    case 'G':
		param = self->color->g;
		break;
    case 'B':	/* these are backwards, but that how it works */
		param = self->color->r;
		break;
    case 'A':
		param = self->color->a;
		break;
	default:
		{
			char errstr[1024];
			sprintf( errstr, "undefined type '%d' in MCol_getAttr",
					(int)((long)type & 0xff));
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, errstr );
		}
	}

	return PyInt_FromLong( param ); 
}

/*
 * set a color attribute
 */

static int MCol_setAttr( BPy_MCol * self, PyObject * value, void * type )
{
	unsigned char *param;

	switch( (long)type ) {
    case 'R':	/* these are backwards, but that how it works */
		param = (unsigned char *)&self->color->b;
		break;
    case 'G':
		param = (unsigned char *)&self->color->g;
		break;
    case 'B':	/* these are backwards, but that how it works */
		param = (unsigned char *)&self->color->r;
		break;
    case 'A':
		param = (unsigned char *)&self->color->a;
		break;
	default:
		{
			char errstr[1024];
			sprintf( errstr, "undefined type '%d' in MCol_setAttr",
					(int)((long)type & 0xff));
			return EXPP_ReturnIntError( PyExc_RuntimeError, errstr );
		}
	}

	return EXPP_setIValueClamped( value, param, 0, 255, 'b' );
}

/************************************************************************
 *
 * Python MCol_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MCol_getseters[] = {
	{"r",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "red component",
	 (void *)'R'},
	{"g",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "green component",
	 (void *)'G'},
	{"b",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "blue component",
	 (void *)'B'},
	{"a",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "alpha component",
	 (void *)'A'},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


/*----------------------------object[]---------------------------
  sequence accessor (get)*/
static PyObject *MCol_item(BPy_MCol * self, int i)
{
	unsigned char param;
	switch (i) {
	case 0:
		param = self->color->b;
		break;
	case 1:
		param = self->color->g;
		break;
	case 2:
		param = self->color->r;
		break;
	case 3:
		param = self->color->a;
		break;	
	default:
		return EXPP_ReturnPyObjError(PyExc_IndexError,
			"vector[index] = x: assignment index out of range\n");
	}
	
	return PyInt_FromLong( param );
}

/*----------------------------object[]-------------------------
  sequence accessor (set)*/
static int MCol_ass_item(BPy_MCol * self, int i, PyObject * value)
{
	unsigned char *param;
	
	switch (i) {
	case 0:
		param = (unsigned char *)&self->color->b; /* reversed? why */
		break;
	case 1:
		param = (unsigned char *)&self->color->g;
		break;
	case 2:
		param = (unsigned char *)&self->color->r; /* reversed? why */
		break;
	case 3:
		param = (unsigned char *)&self->color->a;
		break;	
	default:
		{
			return EXPP_ReturnIntError( PyExc_RuntimeError, "Index out of range" );
		}
	}
	return EXPP_setIValueClamped( value, param, 0, 255, 'b' );
}

/************************************************************************
 *
 * Python MCol_Type methods
 *
 ************************************************************************/

static PyObject *MCol_repr( BPy_MCol * self )
{
	return PyString_FromFormat( "[MCol %d %d %d %d]",
			(int)self->color->b, (int)self->color->g, 
			(int)self->color->r, (int)self->color->a ); 
}

/*-----------------PROTCOL DECLARATIONS--------------------------*/
static PySequenceMethods MCol_SeqMethods = {
	(inquiry) NULL,						/* sq_length */
	(binaryfunc) NULL,								/* sq_concat */
	(intargfunc) NULL,								/* sq_repeat */
	(intargfunc) MCol_item,					/* sq_item */
	(intintargfunc) NULL,				/* sq_slice */
	(intobjargproc) MCol_ass_item,			/* sq_ass_item */
	(intintobjargproc) NULL,		/* sq_ass_slice */
};

/************************************************************************
 *
 * Python MCol_Type structure definition
 *
 ************************************************************************/

PyTypeObject MCol_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MCol",           /* char *tp_name; */
	sizeof( BPy_MCol ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) MCol_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MCol_SeqMethods,	        /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MCol_getseters,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

static PyObject *MCol_CreatePyObject( MCol * color )
{
	BPy_MCol *obj = PyObject_NEW( BPy_MCol, &MCol_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->color = color;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * BPy_MVert attributes
 *
 ************************************************************************/

static MVert * MVert_get_pointer( BPy_MVert * self )
{
	if( BPy_MVert_Check( self ) ) {
		if( self->index >= ((Mesh *)self->data)->totvert )
			return (MVert *)EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"MVert is no longer valid" );
		return &((Mesh *)self->data)->mvert[self->index];
	}
	else
		return (MVert *)self->data;
}

/*
 * get a vertex's coordinate
 */

static PyObject *MVert_getCoord( BPy_MVert * self )
{
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return NULL;

	return newVectorObject( v->co, 3, Py_WRAP );
}

/*
 * set a vertex's coordinate
 */

static int MVert_setCoord( BPy_MVert * self, VectorObject * value )
{
	int i;
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return -1;

	if( !VectorObject_Check( value ) || value->size != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected vector argument of size 3" );

	for( i=0; i<3 ; ++i)
		v->co[i] = value->vec[i];

	return 0;
}

/*
 * get a vertex's index
 */

static PyObject *MVert_getIndex( BPy_MVert * self )
{
	if( self->index >= ((Mesh *)self->data)->totvert )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"MVert is no longer valid" );

	return PyInt_FromLong( self->index );
}


/*
 * get a verts's hidden state
 */

static PyObject *MVert_getMFlagBits( BPy_MVert * self, void * type )
{
	MVert *v;

	v = MVert_get_pointer( self );	
	if (!v)
		return NULL; /* error is set */

	return EXPP_getBitfield( &v->flag, (int)((long)type & 0xff), 'b' );
}


/*
 * set a verts's hidden state
 */

static int MVert_setMFlagBits( BPy_MVert * self, PyObject * value,
		void * type )
{
	MVert *v;

	v = MVert_get_pointer( self );

	if (!v)
		return -1; /* error is set */

	return EXPP_setBitfield( value, &v->flag, 
			(int)((long)type & 0xff), 'b' );
}


/*
 * get a vertex's normal
 */

static PyObject *MVert_getNormal( BPy_MVert * self )
{
	float no[3];
	int i;
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return NULL; /* error set */

	for( i = 0; i < 3; ++i )
		no[i] = (float)(v->no[i] / 32767.0);
	return newVectorObject( no, 3, Py_NEW );
}

/*
 * set a vertex's normal
 */

static int MVert_setNormal( BPy_MVert * self, VectorObject * value )
{
	int i;
	MVert *v;
	float normal[3];
	
	v = MVert_get_pointer( self );
	if( !v )
		return -1; /* error set */

	if( !VectorObject_Check( value ) || value->size != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected vector argument of size 3" );
	
	
	for( i=0; i<3 ; ++i)
		normal[i] = value->vec[i];
	
	Normalize(normal);
	
	for( i=0; i<3 ; ++i)
		v->no[i] = (short)(normal[i]*32767.0);
	
	return 0;
}


/*
 * get a vertex's select status
 */

static PyObject *MVert_getSel( BPy_MVert *self )
{
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return NULL; /* error is set */

	return EXPP_getBitfield( &v->flag, SELECT, 'b' );
}

/*
 * set a vertex's select status
 */

static int MVert_setSel( BPy_MVert *self, PyObject *value )
{
	MVert *v = MVert_get_pointer( self );
	Mesh *me = (Mesh *)self->data;
	if (!v)
		return -1; /* error is set */

	/* 
	 * if vertex exists and setting status is OK, delete select storage
	 * of the edges and faces as well
	 */

	if( v && !EXPP_setBitfield( value, &v->flag, SELECT, 'b' ) ) {
		if( me && me->mselect ) {
			MEM_freeN( me->mselect );
			me->mselect = NULL;
		}
		return 0;
	}
	return -1;
}

/*
 * get a vertex's UV coordinates
 */

static PyObject *MVert_getUVco( BPy_MVert *self )
{
	Mesh *me = (Mesh *)self->data;

	if( !me->msticky )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"mesh has no 'sticky' coordinates" );

	if( self->index >= ((Mesh *)self->data)->totvert )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"MVert is no longer valid" );

	return newVectorObject( me->msticky[self->index].co, 2, Py_WRAP );
}

/*
 * set a vertex's UV coordinates
 */

static int MVert_setUVco( BPy_MVert *self, PyObject *value )
{
	float uvco[3] = {0.0, 0.0};
	Mesh *me = (Mesh *)self->data;
	struct MSticky *v;
	int i;

	/* 
	 * at least for now, don't allow creation of sticky coordinates if they
	 * don't already exist
	 */

	if( !me->msticky )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"mesh has no 'sticky' coordinates" );

	if( self->index >= ((Mesh *)self->data)->totvert )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"MVert is no longer valid" );

	if( VectorObject_Check( value ) ) {
		VectorObject *vect = (VectorObject *)value;
		if( vect->size != 2 )
			return EXPP_ReturnIntError( PyExc_AttributeError,
					"expected 2D vector" );
		for( i = 0; i < vect->size; ++i )
			uvco[i] = vect->vec[i];
	} else if( !PyArg_ParseTuple( value, "ff",
				&uvco[0], &uvco[1] ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected 2D vector" );

	v = &me->msticky[self->index];

	for( i = 0; i < 2; ++i )
		v->co[i] = uvco[i];

	return 0;
}

/************************************************************************
 *
 * Python MVert_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MVert_getseters[] = {
	{"co",
	 (getter)MVert_getCoord, (setter)MVert_setCoord,
	 "vertex's coordinate",
	 NULL},
	{"index",
	 (getter)MVert_getIndex, (setter)NULL,
	 "vertex's index",
	 NULL},
	{"no",
	 (getter)MVert_getNormal, (setter)MVert_setNormal,
	 "vertex's normal",
	 NULL},
	{"sel",
	 (getter)MVert_getSel, (setter)MVert_setSel,
	 "vertex's select status",
	 NULL},
    {"hide",
     (getter)MVert_getMFlagBits, (setter)MVert_setMFlagBits,
     "vert hidden in edit mode",
     (void *)ME_HIDE},
	{"uvco",
	 (getter)MVert_getUVco, (setter)MVert_setUVco,
	 "vertex's UV coordinates",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyGetSetDef BPy_PVert_getseters[] = {
	{"co",
	 (getter)MVert_getCoord, (setter)MVert_setCoord,
	 "vertex's coordinate",
	 NULL},
	{"no",
	 (getter)MVert_getNormal, (setter)MVert_setNormal,
	 "vertex's normal",
	 NULL},
	{"sel",
	 (getter)MVert_getSel, (setter)MVert_setSel,
	 "vertex's select status",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MVert_Type standard operations
 *
 ************************************************************************/

static void MVert_dealloc( BPy_MVert * self )
{
	if( BPy_PVert_Check( self ) ) /* free memory of thick objects */
		MEM_freeN ( self->data );

	PyObject_DEL( self );
}

static int MVert_compare( BPy_MVert * a, BPy_MVert * b )
{
	return( a->data == b->data && a->index == b->index ) ? 0 : -1;
}

static PyObject *MVert_repr( BPy_MVert * self )
{
	char format[512];
	char index[24];
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return NULL;

	if( BPy_MVert_Check( self ) )
		sprintf( index, "%d", self->index );
	else
		BLI_strncpy( index, "(None)", 24 );

	sprintf( format, "[MVert (%f %f %f) (%f %f %f) %s]",
			v->co[0], v->co[1], v->co[2], (float)(v->no[0] / 32767.0),
			(float)(v->no[1] / 32767.0), (float)(v->no[2] / 32767.0),
			index );

	return PyString_FromString( format );
}

static long MVert_hash( BPy_MVert *self )
{
	return (long)self->index;
}

static PyObject *Mesh_addPropLayer_internal(Mesh *mesh, CustomData *data, int tot, PyObject *args)
{
	char *name=NULL;
	int type = -1;
	
	if( !PyArg_ParseTuple( args, "si", &name, &type) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
							"expected a string and an int" );
	if (strlen(name)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
							"error, maximum name length is 31");
	if((type != CD_PROP_FLT) && (type != CD_PROP_INT) && (type != CD_PROP_STR))
		return EXPP_ReturnPyObjError( PyExc_ValueError,
							"error, unknown layer type");
	if (name)
		CustomData_add_layer_named(data, type, CD_DEFAULT, NULL,tot,name);
	
	mesh_update_customdata_pointers(mesh);
	Py_RETURN_NONE;
}

static PyObject *Mesh_removePropLayer_internal(Mesh *mesh, CustomData *data, int tot,PyObject *value)
{
	CustomDataLayer *layer;
	char *name=PyString_AsString(value);
	int i;
	
	if( !name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );
	
	if (strlen(name)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum name length is 31" );
	
	i = CustomData_get_named_layer_index(data, CD_PROP_FLT, name);
	if(i == -1) i = CustomData_get_named_layer_index(data, CD_PROP_INT, name);
	if(i == -1) i = CustomData_get_named_layer_index(data, CD_PROP_STR, name);
	if (i==-1)
		return EXPP_ReturnPyObjError(PyExc_ValueError,
			"No matching layers to remove" );	
	layer = &data->layers[i];
	CustomData_free_layer(data, layer->type, tot, i);
	mesh_update_customdata_pointers(mesh);

	Py_RETURN_NONE;
}

static PyObject *Mesh_renamePropLayer_internal(Mesh *mesh, CustomData *data, PyObject *args)
{
	CustomDataLayer *layer;
	int i;
	char *name_from, *name_to;
	
	if( !PyArg_ParseTuple( args, "ss", &name_from, &name_to ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected 2 strings" );
	
	if (strlen(name_from)>31 || strlen(name_to)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum name length is 31" );
	
	i = CustomData_get_named_layer_index(data, CD_PROP_FLT, name_from);
	if(i == -1) i = CustomData_get_named_layer_index(data, CD_PROP_INT, name_from);
	if(i == -1) i = CustomData_get_named_layer_index(data, CD_PROP_STR, name_from);
	if(i == -1)
		return EXPP_ReturnPyObjError(PyExc_ValueError,
			"No matching layers to rename" );	

	layer = &data->layers[i];
	
	strcpy(layer->name, name_to); /* we alredy know the string sizes are under 32 */
	CustomData_set_layer_unique_name(data, i);
	Py_RETURN_NONE;
}

static PyObject *Mesh_propList_internal(CustomData *data)
{
	CustomDataLayer *layer;
	PyObject *list = PyList_New( 0 ), *item;
	int i;
	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		if( (layer->type == CD_PROP_FLT) || (layer->type == CD_PROP_INT) || (layer->type == CD_PROP_STR)) {
			item = PyString_FromString(layer->name);
			PyList_Append( list, item );
			Py_DECREF(item);
		}
	}
	return list;
} 

static PyObject *Mesh_getProperty_internal(CustomData *data, int eindex, PyObject *value)
{
	CustomDataLayer *layer;
	char *name=PyString_AsString(value);
	int i;
	MFloatProperty *pf;
	MIntProperty *pi;
	MStringProperty *ps;

	if(!name)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected an string argument" );
	
	if (strlen(name)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum name length is 31" );
	
	i = CustomData_get_named_layer_index(data, CD_PROP_FLT, name);
	if(i == -1) i = CustomData_get_named_layer_index(data, CD_PROP_INT, name);
	if(i == -1) i = CustomData_get_named_layer_index(data, CD_PROP_STR, name);
	if(i == -1)
		return EXPP_ReturnPyObjError(PyExc_ValueError,
			"No matching layers" );	
	
	layer = &data->layers[i];

	if(layer->type == CD_PROP_FLT){ 
		pf = layer->data;
		return PyFloat_FromDouble(pf[eindex].f);
	}
	else if(layer->type == CD_PROP_INT){
		pi = layer->data;
		return PyInt_FromLong(pi[eindex].i);
	
	}
	else if(layer->type == CD_PROP_STR){
		ps = layer->data;
		return PyString_FromString(ps[eindex].s);
	}
	Py_RETURN_NONE;
}

static PyObject *Mesh_setProperty_internal(CustomData *data, int eindex, PyObject *args)
{
	CustomDataLayer *layer;
	int i = 0, index, type = -1;
	float f = 0.0f;
	char *s=NULL, *name=NULL;
	MFloatProperty *pf;
	MIntProperty  *pi;
	MStringProperty *ps;
	PyObject *val;
	
	if(PyArg_ParseTuple(args, "sO", &name, &val)){
		if (strlen(name)>31)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"error, maximum name length is 31" );
		
		if(PyInt_Check(val)){ 
			type = CD_PROP_INT;
			i = (int)PyInt_AS_LONG(val);
		}
		else if(PyFloat_Check(val)){
			type = CD_PROP_FLT;
			f = (float)PyFloat_AsDouble(val);
		}
		else if(PyString_Check(val)){
			type = CD_PROP_STR;
			s = PyString_AsString(val);
		}
		else
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected an name plus either float/int/string" );

	}

	index = CustomData_get_named_layer_index(data, type, name);
	if(index == -1)
		return EXPP_ReturnPyObjError(PyExc_ValueError,
			"No matching layers or type mismatch" );	

	layer = &data->layers[index];
	
	if(type==CD_PROP_STR){
		if (strlen(s)>255){
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum string length is 255");
		}
		else{
			ps =  layer->data;
			strcpy(ps[eindex].s,s);
		}
	}
	else if(type==CD_PROP_FLT){
		pf = layer->data;
		pf[eindex].f = f;
	}
	else{
		pi = layer->data;
		pi[eindex].i = i;
	}
	Py_RETURN_NONE;
}

static PyObject *MVert_getProp( BPy_MVert *self, PyObject *args)
{
	if( BPy_MVert_Check( self ) ){
		Mesh *me = (Mesh *)self->data;
		if(self->index >= me->totvert)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, MVert is no longer valid part of mesh!");
		else
			return Mesh_getProperty_internal(&(me->vdata), self->index, args);
	}
	return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, Vertex not part of a mesh!");
}

static PyObject *MVert_setProp( BPy_MVert *self,  PyObject *args)
{
	if( BPy_MVert_Check( self ) ){
		Mesh *me = (Mesh *)self->data;
		if(self->index >= me->totvert)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, MVert is no longer valid part of mesh!");
		else
			return Mesh_setProperty_internal(&(me->vdata), self->index, args);
	}
	return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, Vertex not part of a mesh!");
}
	
static struct PyMethodDef BPy_MVert_methods[] = {
	{"getProperty", (PyCFunction)MVert_getProp, METH_O,
		"get property indicated by name"},
	{"setProperty", (PyCFunction)MVert_setProp, METH_VARARGS,
		"set property indicated by name"},
	{NULL, NULL, 0, NULL}
};


/************************************************************************
 *
 * Python MVert_Type structure definition
 *
 ************************************************************************/

PyTypeObject MVert_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MVert",            /* char *tp_name; */
	sizeof( BPy_MVert ),        /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MVert_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MVert_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MVert_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) MVert_hash,    /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MVert_methods,          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MVert_getseters,        /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/************************************************************************
 *
 * Python PVert_Type standard operations
 *
 ************************************************************************/

static int PVert_compare( BPy_MVert * a, BPy_MVert * b )
{
	return( a->data == b->data ) ? 0 : -1;
}

/************************************************************************
 *
 * Python PVert_Type structure definition
 *
 ************************************************************************/

PyTypeObject PVert_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender PVert",            /* char *tp_name; */
	sizeof( BPy_MVert ),        /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MVert_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) PVert_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MVert_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) MVert_hash,    /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_PVert_getseters,        /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*
 * create 'thin' or 'thick' MVert objects
 *
 * there are two types of objects; thin (wrappers for mesh vertex) and thick
 * (not contains in mesh).  Thin objects are MVert_Type and thick are
 * PVert_Type.  For thin objects, data is a pointer to a Mesh and index
 * is the vertex's index in mesh->mvert.  For thick objects, data is a
 * pointer to an MVert; index is unused.
 */

/*
 * create a thin MVert object
 */

static PyObject *MVert_CreatePyObject( Mesh *mesh, int i )
{
	BPy_MVert *obj = (BPy_MVert *)PyObject_NEW( BPy_MVert, &MVert_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->index = i;
	obj->data = mesh;
	return (PyObject *)obj;
}

/*
 * create a thick MVert object
 */

static PyObject *PVert_CreatePyObject( MVert *vert )
{
	MVert *newvert;
	BPy_MVert *obj = (BPy_MVert *)PyObject_NEW( BPy_MVert, &PVert_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	newvert = (MVert *)MEM_callocN( sizeof( MVert ), "MVert" );
	if( !newvert )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"MEM_callocN() failed" );

	memcpy( newvert, vert, sizeof( MVert ) );
	obj->data = newvert;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Vertex sequence 
 *
 ************************************************************************/

static int MVertSeq_len( BPy_MVertSeq * self )
{
	return self->mesh->totvert;
}

/*
 * retrive a single MVert from somewhere in the vertex list
 */

static PyObject *MVertSeq_item( BPy_MVertSeq * self, int i )
{
	if( i < 0 || i >= self->mesh->totvert )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return MVert_CreatePyObject( self->mesh, i );
}

/*
 * retrieve a slice of the vertex list (as a Python list)
 *
 * Python is nice enough to handle negative indices for us: if the user
 * specifies -1, Python will pass us len()-1.  So we can just check for
 * indices in the range 0:len()-1.  Of course, we should never actually
 * return the high index, but up to one less.
 */

static PyObject *MVertSeq_slice( BPy_MVertSeq *self, int low, int high )
{
	PyObject *list;
	int i;

	/*
	 * Python list slice operator returns empty list when asked for a slice
	 * outside the list, or if indices are reversed (low > high).  Clamp
	 * our input to do the same.
	 */

	if( low < 0 ) low = 0;
	if( high > self->mesh->totvert ) high = self->mesh->totvert;
	if( low > high ) low = high;

	list = PyList_New( high-low );
	if( !list )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "PyList_New() failed" );

	/*
	 * return Py_NEW copies of requested vertices
	 */

	for( i = low; i < high; ++i )
		PyList_SET_ITEM( list, i-low,
				PVert_CreatePyObject( (void *)&self->mesh->mvert[i] ) );
	return list;
}

/*
 * assign a single MVert to somewhere in the vertex list
 */

static int MVertSeq_assign_item( BPy_MVertSeq * self, int i,
		BPy_MVert *v )
{
	MVert *dst = &self->mesh->mvert[i];
	MVert *src;

	if( !v )
		return EXPP_ReturnIntError( PyExc_IndexError,
					      "del() not supported" );

	if( i < 0 || i >= self->mesh->totvert )
		return EXPP_ReturnIntError( PyExc_IndexError,
					      "array index out of range" );

	if( BPy_MVert_Check( v ) )
		src = &((Mesh *)v->data)->mvert[v->index];
	else
		src = (MVert *)v->data;

	memcpy( dst, src, sizeof(MVert) );
	/* mesh_update( self->mesh );*/
	return 0;
}

static int MVertSeq_assign_slice( BPy_MVertSeq *self, int low, int high,
		   PyObject *args )
{
	int len, i;

	if( !PyList_Check( args ) )
		return EXPP_ReturnIntError( PyExc_IndexError,
					      "can only assign lists of MVerts" );

	len = PyList_Size( args );

	/*
	 * Python list slice assign operator allows for changing the size of the
	 * destination list, by replacement and appending....
	 *
	 * >>> l=[1,2,3,4]
	 * >>> m=[11,12,13,14]
	 * >>> l[5:7]=m
	 * >>> print l
	 * [1, 2, 3, 4, 11, 12, 13, 14]
	 * >>> l=[1,2,3,4]
	 * >>> l[2:3]=m
	 * >>> print l
	 * [1, 2, 11, 12, 13, 14, 4]
	 *
	 * We don't want the size of the list to change (at least not at time
	 * point in development) so we are a little more strict:
	 * - low and high indices must be in range [0:len()]
	 * - high-low == PyList_Size(v)
	 */

	if( low < 0 || high > self->mesh->totvert || low > high )
		return EXPP_ReturnIntError( PyExc_IndexError,
					      "invalid slice range" );

	if( high-low != len )
		return EXPP_ReturnIntError( PyExc_IndexError,
					      "slice range and input list sizes must be equal" );

	for( i = low; i < high; ++i )
	{
		BPy_MVert *v = (BPy_MVert *)PyList_GET_ITEM( args, i-low );
		MVert *dst = &self->mesh->mvert[i];
		MVert *src;

		if( BPy_MVert_Check( v ) )
			src = &((Mesh *)v->data)->mvert[v->index];
		else
			src = (MVert *)v->data;

		memcpy( dst, src, sizeof(MVert) );
	}
	/* mesh_update( self->mesh );*/
	return 0;
}

static PySequenceMethods MVertSeq_as_sequence = {
	( inquiry ) MVertSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) MVertSeq_item,	/* sq_item */
	( intintargfunc ) MVertSeq_slice,	/* sq_slice */
	( intobjargproc ) MVertSeq_assign_item,	/* sq_ass_item */
	( intintobjargproc ) MVertSeq_assign_slice,	/* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MVertSeq_Type iterator (iterates over vertices)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MVertSeq_getIter( BPy_MVertSeq * self )
{
	if (self->iter==-1) { /* iteration for this pyobject is not yet used, just return self */
		self->iter = 0;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		/* were alredy using this as an iterator, make a copy to loop on */
		BPy_MVertSeq *seq = (BPy_MVertSeq *)MVertSeq_CreatePyObject(self->mesh);
		seq->iter = 0;
		return (PyObject *)seq;
	}
}

/*
 * Return next MVert.
 */

static PyObject *MVertSeq_nextIter( BPy_MVertSeq * self )
{
	if( self->iter == self->mesh->totvert ) {
		self->iter= -1; /* allow it to be used as an iterator again without creating a new BPy_MVertSeq */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}

	return MVert_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MVertSeq_Type methods
 *
 ************************************************************************/

static PyObject *MVertSeq_extend( BPy_MVertSeq * self, PyObject *args )
{
	int len, newlen;
	int i,j;
	PyObject *tmp;
	MVert *newvert, *tmpvert;
	Mesh *mesh = self->mesh;
	CustomData vdata;
	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size( args ) ) {
	case 1:		/* better be a list or a tuple */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( !VectorObject_Check ( tmp ) ) {
			if( !PySequence_Check ( tmp ) )
				return EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a sequence of sequence triplets" );
			else if( !PySequence_Size ( tmp ) ) {
				Py_RETURN_NONE;
			}
			args = tmp;
		}
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 3:
		tmp = PyTuple_GET_ITEM( args, 0 );
		/* if first item is not a number, it's wrong */
		if( !PyNumber_Check( tmp ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of sequence triplets" );

		/* otherwise, put into a new tuple */
		args = Py_BuildValue( "((OOO))", tmp,
				PyTuple_GET_ITEM( args, 1 ), PyTuple_GET_ITEM( args, 2 ) );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;

	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of sequence triplets" );
	}

	/* if no verts given, return quietly */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF ( args );
		Py_RETURN_NONE;
	}

	/* create custom vertex data arrays and copy existing vertices into it */

	newlen = mesh->totvert + len;
	CustomData_copy( &mesh->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, newlen );
	CustomData_copy_data( &mesh->vdata, &vdata, 0, 0, mesh->totvert );

	if ( !CustomData_has_layer( &vdata, CD_MVERT ) )
		CustomData_add_layer( &vdata, CD_MVERT, CD_CALLOC, NULL, newlen );

	newvert = CustomData_get_layer( &vdata, CD_MVERT );

	/* scan the input list and insert the new vertices */

	tmpvert = &newvert[mesh->totvert];
	for( i = 0; i < len; ++i ) {
		float co[3];
		tmp = PySequence_GetItem( args, i );
		if( VectorObject_Check( tmp ) ) {
			if( ((VectorObject *)tmp)->size != 3 ) {
				CustomData_free( &vdata, newlen );
				Py_DECREF ( tmp );
				Py_DECREF ( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected vector of size 3" );
			}
			for( j = 0; j < 3; ++j )
				co[j] = ((VectorObject *)tmp)->vec[j];
		} else if( PySequence_Check( tmp ) ) {
			int ok=1;
			PyObject *flt;
			if( PySequence_Size( tmp ) != 3 )
				ok = 0;
			else	
				for( j = 0; ok && j < 3; ++j ) {
					flt = PySequence_ITEM( tmp, j );
					if( !PyNumber_Check ( flt ) )
						ok = 0;
					else
						co[j] = (float)PyFloat_AsDouble( flt );
					Py_DECREF( flt );
				}

			if( !ok ) {
				CustomData_free( &vdata, newlen );
				Py_DECREF ( args );
				Py_DECREF ( tmp );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected sequence triplet of floats" );
			}
		} else {
			CustomData_free( &vdata, newlen );
			Py_DECREF ( args );
			Py_DECREF ( tmp );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected sequence triplet of floats" );
		}

		Py_DECREF ( tmp );

	/* add the coordinate to the new list */
		memcpy( tmpvert->co, co, sizeof(co) );
		
		tmpvert->flag |= SELECT;
	/* TODO: anything else which needs to be done when we add a vert? */
	/* probably not: NMesh's newvert() doesn't */
		++tmpvert;
	}

	CustomData_free( &mesh->vdata, mesh->totvert );
	mesh->vdata = vdata;
	mesh_update_customdata_pointers( mesh );

	/*
	 * if there are keys, have to fix those lists up
	 */

	if( mesh->key ) {
		KeyBlock *currkey = mesh->key->block.first;
		float *fp, *newkey;

		while( currkey ) {

			/* create key list, copy existing data if any */
			newkey = MEM_callocN(mesh->key->elemsize*newlen, "keydata");
			if( currkey->data ) {
				memcpy( newkey, currkey->data,
						mesh->totvert*mesh->key->elemsize );
				MEM_freeN( currkey->data );
				currkey->data = newkey;
			}

			/* add data for new vertices */
			fp = (float *)((char *)currkey->data +
					(mesh->key->elemsize*mesh->totvert));
			tmpvert = mesh->mvert + mesh->totvert;
			for( i = newlen - mesh->totvert; i > 0; --i ) {
				VECCOPY(fp, tmpvert->co);
				fp += 3;
				tmpvert++;
			}
			currkey->totelem = newlen;
			currkey = currkey->next;
		}
	}

	/* set final vertex list size */
	mesh->totvert = newlen;

	mesh_update( mesh );

	Py_DECREF ( args );
	Py_RETURN_NONE;
}

static PyObject *MVertSeq_delete( BPy_MVertSeq * self, PyObject *args )
{
	unsigned int *vert_table;
	int vert_delete, face_count;
	int i;
	Mesh *mesh = self->mesh;
	MFace *tmpface;

	/*
	 * if input tuple contains a single sequence, use it as input instead;
	 * otherwise use the sequence as-is and check later that it contains
	 * one or more integers or MVerts
	 */
	if( PySequence_Size( args ) == 1 ) {
		PyObject *tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check( tmp ) ) 
			args = tmp;
	}

	/* if sequence is empty, do nothing */
	if( PySequence_Size( args ) == 0 ) {
		Py_RETURN_NONE;
	}

	/* allocate vertex lookup table */
	vert_table = (unsigned int *)MEM_callocN( 
			mesh->totvert*sizeof( unsigned int ), "vert_table" );

	/* get the indices of vertices to be removed */
	for( i = PySequence_Size( args ); i--; ) {
		PyObject *tmp = PySequence_GetItem( args, i );
		int index;
		if( BPy_MVert_Check( tmp ) ) {
			if( (void *)self->mesh != ((BPy_MVert*)tmp)->data ) {
				MEM_freeN( vert_table );
				Py_DECREF( tmp );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
						"MVert belongs to a different mesh" );
			}
			index = ((BPy_MVert*)tmp)->index;
		} else if( PyInt_Check( tmp ) ) {
			index = PyInt_AsLong ( tmp );
		} else {
			MEM_freeN( vert_table );
			Py_DECREF( tmp );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of ints or MVerts" );
		}
		Py_DECREF( tmp );
		if( index < 0 || index >= mesh->totvert ) {
			MEM_freeN( vert_table );
			return EXPP_ReturnPyObjError( PyExc_IndexError,
					"array index out of range" );
		}
		vert_table[index] = UINT_MAX;
	}

	/* delete things, then clean up and return */

	vert_delete = make_vertex_table( vert_table, mesh->totvert );
	if( vert_delete )
		delete_verts( mesh, vert_table, vert_delete );

	/* calculate edges to delete, fix vertex indices */
	delete_edges( mesh, vert_table, 0 );

	/*
	 * find number of faces which contain any of the deleted vertices,
	 * and mark them, then delete them
	 */
	tmpface = mesh->mface;
	face_count=0;
	for( i = mesh->totface; i--; ++tmpface ) {
		if( vert_table[tmpface->v1] == UINT_MAX ||
				vert_table[tmpface->v2] == UINT_MAX ||
				vert_table[tmpface->v3] == UINT_MAX ||
				( tmpface->v4 && vert_table[tmpface->v4] == UINT_MAX ) ) {
			tmpface->v1 = UINT_MAX;
			++face_count;
		}
	}
	delete_faces( mesh, vert_table, face_count );

	/* clean up and exit */
	MEM_freeN( vert_table );
	mesh_update ( mesh );
	Py_RETURN_NONE;
}

static PyObject *MVertSeq_selected( BPy_MVertSeq * self )
{
	int i, count;
	Mesh *mesh = self->mesh;
	MVert *tmpvert;
	PyObject *list;

	/* first count selected edges (quicker than appending to PyList?) */
	count = 0;
	tmpvert = mesh->mvert;
	for( i = 0; i < mesh->totvert; ++i, ++tmpvert )
		if( tmpvert->flag & SELECT )
			++count;

	list = PyList_New( count );
	if( !list )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyList_New() failed" );

	/* next, insert selected edges into list */
	count = 0;
	tmpvert = mesh->mvert;
	for( i = 0; i < mesh->totvert; ++i, ++tmpvert ) {
		if( tmpvert->flag & SELECT ) {
			PyObject *tmp = PyInt_FromLong( i );
			if( !tmp ) {
				Py_DECREF( list );
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"PyInt_FromLong() failed" );
			}
			PyList_SET_ITEM( list, count, tmp );
			++count;
		}
	}
	return list;
}
static PyObject *MVertSeq_add_layertype(BPy_MVertSeq *self, PyObject *args)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_addPropLayer_internal(me, &(me->vdata), me->totvert, args);
}
static PyObject *MVertSeq_del_layertype(BPy_MVertSeq *self, PyObject *value)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_removePropLayer_internal(me, &(me->vdata), me->totvert, value);
}
static PyObject *MVertSeq_rename_layertype(BPy_MVertSeq *self, PyObject *args)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_renamePropLayer_internal(me,&(me->vdata),args);
}
static PyObject *MVertSeq_PropertyList(BPy_MVertSeq *self) 
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_propList_internal(&(me->vdata));
}
static PyObject *M_Mesh_PropertiesTypeDict(void)
{
	PyObject *Types = PyConstant_New( );
	if(Types) {
		BPy_constant *d = (BPy_constant *) Types;
		PyConstant_Insert(d, "FLOAT", PyInt_FromLong(CD_PROP_FLT));
		PyConstant_Insert(d, "INT" , PyInt_FromLong(CD_PROP_INT));
		PyConstant_Insert(d, "STRING", PyInt_FromLong(CD_PROP_STR));
	}
	return Types;
}

static struct PyMethodDef BPy_MVertSeq_methods[] = {
	{"extend", (PyCFunction)MVertSeq_extend, METH_VARARGS,
		"add vertices to mesh"},
	{"delete", (PyCFunction)MVertSeq_delete, METH_VARARGS,
		"delete vertices from mesh"},
	{"selected", (PyCFunction)MVertSeq_selected, METH_NOARGS,
		"returns a list containing indices of selected vertices"},
	{"addPropertyLayer",(PyCFunction)MVertSeq_add_layertype, METH_VARARGS,
		"add a new property layer"},
	{"removePropertyLayer",(PyCFunction)MVertSeq_del_layertype, METH_O,
		"removes a property layer"},
	{"renamePropertyLayer",(PyCFunction)MVertSeq_rename_layertype, METH_VARARGS,
		"renames an existing property layer"},
	{NULL, NULL, 0, NULL}
};

static PyGetSetDef BPy_MVertSeq_getseters[] = {
	{"properties",
	(getter)MVertSeq_PropertyList, (setter)NULL,
	"vertex property layers, read only",
	NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};



/************************************************************************
 *
 * Python MVertSeq_Type standard operations
 *
 ************************************************************************/

/*****************************************************************************/
/* Python MVertSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MVertSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MVertSeq",           /* char *tp_name; */
	sizeof( BPy_MVertSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MVertSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) MVertSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MVertSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MVertSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MVertSeq_getseters,     /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/************************************************************************
 *
 * Edge attributes
 *
 ************************************************************************/

static MEdge * MEdge_get_pointer( BPy_MEdge * self )
{
	if( self->index >= self->mesh->totedge )
		return (MEdge *)EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"MEdge is no longer valid" );
	return &self->mesh->medge[self->index];
}

/*
 * get an edge's crease value
 */

static PyObject *MEdge_getCrease( BPy_MEdge * self )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL;

	return PyInt_FromLong( edge->crease );
}

/*
 * set an edge's crease value
 */

static int MEdge_setCrease( BPy_MEdge * self, PyObject * value )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return -1;

	return EXPP_setIValueClamped( value, &edge->crease, 0, 255, 'b' );
}

/*
 * get an edge's flag
 */

static PyObject *MEdge_getFlag( BPy_MEdge * self )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL;

	return PyInt_FromLong( edge->flag );
}

/*
 * set an edge's flag
 */

static int MEdge_setFlag( BPy_MEdge * self, PyObject * value )
{
	short param;
	static short bitmask = SELECT
				| ME_EDGEDRAW
				| ME_SEAM
				| ME_FGON
				| ME_HIDE
				| ME_EDGERENDER
				| ME_LOOSEEDGE
				| ME_SEAM_LAST
				| ME_SHARP;
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return -1;

	if( !PyInt_Check ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = (short)PyInt_AS_LONG ( value );

	if ( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	edge->flag = param;

	return 0;
}

/*
 * get an edge's first vertex
 */

static PyObject *MEdge_getV1( BPy_MEdge * self )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL;

	return MVert_CreatePyObject( self->mesh, edge->v1 );
}

/*
 * set an edge's first vertex
 */

static int MEdge_setV1( BPy_MEdge * self, BPy_MVert * value )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return -1;
	if( !BPy_MVert_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected an MVert" );

	edge->v1 = value->index;
	return 0;
}

/*
 * get an edge's second vertex
 */

static PyObject *MEdge_getV2( BPy_MEdge * self )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL; /* error is set */
	/* if v2 is out of range, the python mvert will complain, no need to check here  */
	return MVert_CreatePyObject( self->mesh, edge->v2 );
}

/*
 * set an edge's second vertex
 */

static int MEdge_setV2( BPy_MEdge * self, BPy_MVert * value )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return -1; /* error is set */
	if( !BPy_MVert_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected an MVert" );

	if ( edge->v1 == value->index )
		return EXPP_ReturnIntError( PyExc_ValueError, "an edge cant use the same vertex for each end" );
	
	edge->v2 = value->index;
	return 0;
}

/*
 * get an edge's index
 */

static PyObject *MEdge_getIndex( BPy_MEdge * self )
{
	if( !MEdge_get_pointer( self ) )
		return NULL; /* error is set */

	return PyInt_FromLong( self->index );
}

/*
 * get an edge's flag
 */

static PyObject *MEdge_getMFlagBits( BPy_MEdge * self, void * type )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL; /* error is set */

	return EXPP_getBitfield( &edge->flag, (int)((long)type & 0xff), 'b' );
}

/*
 * get an edge's length
 */

static PyObject *MEdge_getLength( BPy_MEdge * self )
{
	MEdge *edge = MEdge_get_pointer( self );
	double dot = 0.0f;
	float tmpf;
	int i;
	float *v1, *v2;

	if (!edge)
		return NULL; /* error is set */	
	
	if MEDGE_VERT_BADRANGE_CHECK(self->mesh, edge)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError, "This edge uses removed vert(s)" );
	
	/* get the 2 edges vert locations */
	v1= (&((Mesh *)self->mesh)->mvert[edge->v1])->co;
	v2= (&((Mesh *)self->mesh)->mvert[edge->v2])->co;

	if( !edge )
		return NULL;

	for( i = 0; i < 3; i++ ) {
		tmpf = v1[i] - v2[i];
		dot += tmpf*tmpf;
	}
	return PyFloat_FromDouble( sqrt( dot ) );
}

/*
 * get an key for using in a dictionary or set key
 */

static PyObject *MEdge_getKey( BPy_MEdge * self )
{
	PyObject *attr;
	MEdge *edge = MEdge_get_pointer( self );
	if (!edge)
		return NULL; /* error is set */	
	
	attr = PyTuple_New( 2 );
	if (edge->v1 > edge->v2) {
		PyTuple_SET_ITEM( attr, 0, PyInt_FromLong(edge->v2) );
		PyTuple_SET_ITEM( attr, 1, PyInt_FromLong(edge->v1) );
	} else {
		PyTuple_SET_ITEM( attr, 0, PyInt_FromLong(edge->v1) );
		PyTuple_SET_ITEM( attr, 1, PyInt_FromLong(edge->v2) );
	}
	return attr;
}

/*
 * set an edge's select state
 */

static int MEdge_setSel( BPy_MEdge * self,PyObject * value,
		void * type_unused )
{
	MEdge *edge = MEdge_get_pointer( self );
	int param = PyObject_IsTrue( value );
	Mesh *me = self->mesh;

	if( !edge )
		return -1;
	
	if MEDGE_VERT_BADRANGE_CHECK(me, edge)
		return EXPP_ReturnIntError( PyExc_RuntimeError, "This edge uses removed vert(s)" );
	
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );

	if( param ) {
		edge->flag |= SELECT;
		me->mvert[edge->v1].flag |= SELECT;
		me->mvert[edge->v2].flag |= SELECT;
	}
	else {
		edge->flag &= ~SELECT;
		me->mvert[edge->v1].flag &= ~SELECT;
		me->mvert[edge->v2].flag &= ~SELECT;
	}

	if( self->mesh->mselect ) {
		MEM_freeN( self->mesh->mselect );
		self->mesh->mselect = NULL;
	}

	return 0;
}

/************************************************************************
 *
 * Python MEdge_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MEdge_getseters[] = {
	{"crease",
	 (getter)MEdge_getCrease, (setter)MEdge_setCrease,
	 "edge's crease value",
	 NULL},
	{"flag",
	 (getter)MEdge_getFlag, (setter)MEdge_setFlag,
	 "edge's flags",
	 NULL},
	{"v1",
	 (getter)MEdge_getV1, (setter)MEdge_setV1,
	 "edge's first vertex",
	 NULL},
	{"v2",
	 (getter)MEdge_getV2, (setter)MEdge_setV2,
	 "edge's second vertex",
	 NULL},
	{"index",
	 (getter)MEdge_getIndex, (setter)NULL,
	 "edge's index",
	 NULL},
	{"sel",
	 (getter)MEdge_getMFlagBits, (setter)MEdge_setSel,
     "edge selected in edit mode",
     (void *)SELECT},
	{"length",
	 (getter)MEdge_getLength, (setter)NULL,
     "edge's length, read only",
     NULL},
	{"key",
	 (getter)MEdge_getKey, (setter)NULL,
     "edge's key for using with sets or dictionaries, read only",
     NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MEdge_Type iterator (iterates over vertices)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MEdge_getIter( BPy_MEdge * self )
{
	if (self->iter==-1) { /* not alredy used to iterator on, just use self */
		self->iter = 0;
		return EXPP_incr_ret ( (PyObject *) self );
	} else { /* alredy being iterated on, return a copy */
		BPy_MEdge *seq = (BPy_MEdge *)MEdge_CreatePyObject(self->mesh, self->index);
		seq->iter = 0;
		return (PyObject *)seq;
	}
}

/*
 * Return next MVert.  Throw an exception after the second vertex.
 */

static PyObject *MEdge_nextIter( BPy_MEdge * self )
{
	if( self->iter == 2 ) {
		self->iter = -1;
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	self->iter++;
	if( self->iter == 1 )
		return MEdge_getV1( self );
	else
		return MEdge_getV2( self );
}

/************************************************************************
 *
 * Python MEdge_Type standard operations
 *
 ************************************************************************/

static int MEdge_compare( BPy_MEdge * a, BPy_MEdge * b )
{
	return( a->mesh == b->mesh && a->index == b->index ) ? 0 : -1;
}

static PyObject *MEdge_repr( BPy_MEdge * self )
{
	struct MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL;

	return PyString_FromFormat( "[MEdge (%d %d) %d %d]",
			(int)edge->v1, (int)edge->v2, (int)edge->crease,
			(int)self->index );
}

static long MEdge_hash( BPy_MEdge *self )
{
	return (long)self->index;
}
static PyObject *MEdge_getProp( BPy_MEdge *self, PyObject *args)
{
	Mesh *me = (Mesh *)self->mesh;
	return Mesh_getProperty_internal(&(me->edata), self->index, args);
}

static PyObject *MEdge_setProp( BPy_MEdge *self,  PyObject *args)
{
	Mesh *me = (Mesh *)self->mesh;
	return Mesh_setProperty_internal(&(me->edata), self->index, args);
}

static struct PyMethodDef BPy_MEdge_methods[] = {
	{"getProperty", (PyCFunction)MEdge_getProp, METH_O,
		"get property indicated by name"},
	{"setProperty", (PyCFunction)MEdge_setProp, METH_VARARGS,
		"set property indicated by name"},
	{NULL, NULL, 0, NULL}
};
/************************************************************************
 *
 * Python MEdge_Type structure definition
 *
 ************************************************************************/

PyTypeObject MEdge_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MEdge",           /* char *tp_name; */
	sizeof( BPy_MEdge ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MEdge_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MEdge_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
    NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) MEdge_hash,    /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) MEdge_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MEdge_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MEdge_methods,          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MEdge_getseters,        /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

static PyObject *MEdge_CreatePyObject( Mesh * mesh, int i )
{
	BPy_MEdge *obj = PyObject_NEW( BPy_MEdge, &MEdge_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->mesh = mesh;
	obj->index = i;
	obj->iter = -1;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Edge sequence 
 *
 ************************************************************************/

static int MEdgeSeq_len( BPy_MEdgeSeq * self )
{
	return self->mesh->totedge;
}

static PyObject *MEdgeSeq_item( BPy_MEdgeSeq * self, int i )
{
	if( i < 0 || i >= self->mesh->totedge )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return MEdge_CreatePyObject( self->mesh, i );
}


static PySequenceMethods MEdgeSeq_as_sequence = {
	( inquiry ) MEdgeSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) MEdgeSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MEdgeSeq_Type iterator (iterates over edges)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MEdgeSeq_getIter( BPy_MEdgeSeq * self )
{
	if (self->iter==-1) { /* iteration for this pyobject is not yet used, just return self */
		self->iter = 0;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		BPy_MEdgeSeq *seq = (BPy_MEdgeSeq *)MEdgeSeq_CreatePyObject(self->mesh);
		seq->iter = 0;
		return (PyObject *)seq;
	}
}

/*
 * Return next MEdge.
 */

static PyObject *MEdgeSeq_nextIter( BPy_MEdgeSeq * self )
{
	if( self->iter == self->mesh->totedge ) {
		self->iter= -1;
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}

	return MEdge_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MEdgeSeq_Type methods
 *
 ************************************************************************/

/*
 * Create edges from tuples of vertices.  Duplicate new edges, or
 * edges which already exist,
 */

static PyObject *MEdgeSeq_extend( BPy_MEdgeSeq * self, PyObject *args )
{
	int len, nverts;
	int i, j, ok;
	int new_edge_count, good_edges;
	SrchEdges *oldpair, *newpair, *tmppair, *tmppair2;
	PyObject *tmp;
	BPy_MVert *e[4];
	MEdge *tmpedge;
	Mesh *mesh = self->mesh;

	/* make sure we get a tuple of sequences of something */
	switch( PySequence_Size( args ) ) {
	case 1:
		/* if a sequence... */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check( tmp ) ) {
			PyObject *tmp2;

			/* ignore empty sequences */
			if( !PySequence_Size( tmp ) ) {
				Py_RETURN_NONE;
			}

			/* if another sequence, use it */
			tmp2 = PySequence_ITEM( tmp, 0 );
			if( PySequence_Check( tmp2 ) )
				args = tmp;
			Py_INCREF( args );
			Py_DECREF( tmp2 );
		} else
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of sequence pairs" );
		break;
	case 2:	
	case 3:
	case 4:		/* two to four args may be individual verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		/*
		 * if first item isn't a sequence, then assume it's a bunch of MVerts
		 * and wrap inside a tuple
		 */
		if( !PySequence_Check( tmp ) ) {
			args = Py_BuildValue( "(O)", args );
			if( !args )
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"Py_BuildValue() failed" );
		/*
		 * otherwise, assume it already a bunch of sequences so use as-is
		 */
		} else { 
			Py_INCREF( args );		/* so we can safely DECREF later */
		}
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of sequence pairs" );
	}

	/* make sure there is something to add */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF ( args );
		Py_RETURN_NONE;
	}

	/* verify the param list and get a total count of number of edges */
	new_edge_count = 0;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_GetItem( args, i );

		/* not a tuple of MVerts... error */
		if( !PySequence_Check( tmp ) ) {
			Py_DECREF( tmp );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected sequence of MVert sequences" );
		}

		/* not the right number of MVerts... error */
		nverts = PySequence_Size( tmp );
		if( nverts < 2 || nverts > 4 ) {
			Py_DECREF( tmp );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected 2 to 4 MVerts per sequence" );
		}

		if( EXPP_check_sequence_consistency( tmp, &MVert_Type ) == 1 ) {

			/* get MVerts, check they're from this mesh */
			ok = 1;
			for( j = 0; ok && j < nverts; ++j ) {
				e[0] = (BPy_MVert *)PySequence_GetItem( tmp, j );
				if( (void *)e[0]->data != (void *)self->mesh )
					ok = 0;
				Py_DECREF( e[0] );
			}
			Py_DECREF( tmp );

			/* not MVerts from another mesh ... error */
			if( !ok ) {
				Py_DECREF( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"vertices are from a different mesh" );
			}
		} else {
			ok = 0; 
			for( j = 0; ok == 0 && j < nverts; ++j ) {
				PyObject *item = PySequence_ITEM( tmp, j );
				if( !PyInt_Check( item ) )
					ok = 1;
				else {
					int index = PyInt_AsLong ( item );
					if( index < 0 || index >= self->mesh->totvert )
						ok = 2;
				}
				Py_DECREF( item );
			}
			Py_DECREF( tmp );

			/* not ints or outside of vertex list ... error */
			if( ok ) {
				Py_DECREF( args );
				if( ok == 1 )
					return EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected an integer index" );
				else
					return EXPP_ReturnPyObjError( PyExc_KeyError,
						"index out of range" );
			}
		}

		if( nverts == 2 )
			++new_edge_count;	/* if only two vert, then add only edge */
		else
			new_edge_count += nverts;	/* otherwise, one edge per vert */
	}

	/* OK, commit to allocating the search structures */
	newpair = (SrchEdges *)MEM_callocN( sizeof(SrchEdges)*new_edge_count,
			"MEdgePairs" );

	/* scan the input list and build the new edge pair list */
	len = PySequence_Size( args );
	tmppair = newpair;
	new_edge_count = 0;
	for( i = 0; i < len; ++i ) {
		int edge_count;
		int eedges[4];
		tmp = PySequence_GetItem( args, i );
		nverts = PySequence_Size( tmp );

		/* get new references for the vertices */
		for(j = 0; j < nverts; ++j ) {
			PyObject *item = PySequence_ITEM( tmp, j );
			if( BPy_MVert_Check( item ) ) {
				eedges[j] = ((BPy_MVert *)item)->index;
			} else {
				eedges[j] = PyInt_AsLong ( item );
			}
			Py_DECREF( item );
		}
		Py_DECREF( tmp );

		if( nverts == 2 )
			edge_count = 1;	 /* again, two verts give just one edge */
		else
			edge_count = nverts;	

		/* now add the edges to the search list */
		for( j = 0; j < edge_count; ++j ) {
			int k = j+1;
			if( k == nverts )	/* final edge */ 
				k = 0;

			/* sort verts into search list, skip if two are the same */
			if( eedges[j] != eedges[k] ) {
				if( eedges[j] < eedges[k] ) {
					tmppair->v[0] = eedges[j];
					tmppair->v[1] = eedges[k];
					tmppair->swap = 0;
				} else {
					tmppair->v[0] = eedges[k];
					tmppair->v[1] = eedges[j];
					tmppair->swap = 1;
				} 
				tmppair->index = new_edge_count;
				++new_edge_count;
				tmppair++;
			}
		}

	}

	/* sort the new edge pairs */
	qsort( newpair, new_edge_count, sizeof(SrchEdges), medge_comp );

	/*
	 * find duplicates in the new list and mark.  if it's a duplicate,
	 * then mark by setting second vert index to 0 (a real edge won't have
	 * second vert index of 0 since verts are sorted)
	 */

	good_edges = new_edge_count;	/* all edges are good at this point */

	tmppair = newpair;		/* "last good edge" */
	tmppair2 = &tmppair[1];	/* "current candidate edge" */
	for( i = 0; i < new_edge_count; ++i ) {
		if( tmppair->v[0] != tmppair2->v[0] ||
				tmppair->v[1] != tmppair2->v[1] )
			tmppair = tmppair2;	/* last != current, so current == last */
		else {
			tmppair2->v[1] = 0; /* last == current, so mark as duplicate */
			--good_edges;		/* one less good edge */
		}
		tmppair2++;
	}

	/* if mesh has edges, see if any of the new edges are already in it */
	if( mesh->totedge ) {
		oldpair = (SrchEdges *)MEM_callocN( sizeof(SrchEdges)*mesh->totedge,
				"MEdgePairs" );

		/*
		 * build a search list of new edges (don't need to update "swap"
		 * field, since we're not creating edges here)
		 */
		tmppair = oldpair;
		tmpedge = mesh->medge;
		for( i = 0; i < mesh->totedge; ++i ) {
			if( tmpedge->v1 < tmpedge->v2 ) {
				tmppair->v[0] = tmpedge->v1;
				tmppair->v[1] = tmpedge->v2;
			} else {
				tmppair->v[0] = tmpedge->v2;
				tmppair->v[1] = tmpedge->v1;
			}
			++tmpedge;
			++tmppair;
		}

	/* sort the old edge pairs */
		qsort( oldpair, mesh->totedge, sizeof(SrchEdges), medge_comp );

	/* eliminate new edges already in the mesh */
		tmppair = newpair;
		for( i = new_edge_count; i-- ; ) {
			if( tmppair->v[1] ) {
				if( bsearch( tmppair, oldpair, mesh->totedge,
							sizeof(SrchEdges), medge_comp ) ) {
					tmppair->v[1] = 0;	/* mark as duplicate */
					--good_edges;
				} 
			}
			tmppair++;
		}
		MEM_freeN( oldpair );
	}

	/* if any new edges are left, add to list */
	if( good_edges ) {
		CustomData edata;
		int totedge = mesh->totedge+good_edges;

	/* create custom edge data arrays and copy existing edges into it */
		CustomData_copy( &mesh->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge );
		CustomData_copy_data( &mesh->edata, &edata, 0, 0, mesh->totedge );

		if ( !CustomData_has_layer( &edata, CD_MEDGE ) )
			CustomData_add_layer( &edata, CD_MEDGE, CD_CALLOC, NULL, totedge );

	/* replace old with new data */
		CustomData_free( &mesh->edata, mesh->totedge );
		mesh->edata = edata;
		mesh_update_customdata_pointers( mesh );

	/* resort edges into original order */
		qsort( newpair, new_edge_count, sizeof(SrchEdges), medge_index_comp );

	/* point to the first edge we're going to add */
		tmpedge = &mesh->medge[mesh->totedge];
		tmppair = newpair;

	/* as we find a good edge, add it */
		while( good_edges ) {
			if( tmppair->v[1] ) {	/* not marked as duplicate ! */
				if( !tmppair->swap ) {
					tmpedge->v1 = tmppair->v[0];
					tmpedge->v2 = tmppair->v[1];
				} else {
					tmpedge->v1 = tmppair->v[1];
					tmpedge->v2 = tmppair->v[0];
				}
				tmpedge->flag = ME_EDGEDRAW | ME_EDGERENDER | SELECT;
				mesh->totedge++;
				--good_edges;
				++tmpedge;
			}
			tmppair++;
		}
	}

	/* clean up and leave */
	mesh_update( mesh );
	MEM_freeN( newpair );
	Py_DECREF ( args );
	Py_RETURN_NONE;
}

static PyObject *MEdgeSeq_delete( BPy_MEdgeSeq * self, PyObject *args )
{
	Mesh *mesh = self->mesh;
	MEdge *srcedge;
	MFace *srcface;
	unsigned int *vert_table, *del_table, *edge_table;
	int i, len;
	int face_count, edge_count, vert_count;

	/*
	 * if input tuple contains a single sequence, use it as input instead;
	 * otherwise use the sequence as-is and check later that it contains
	 * one or more integers or MVerts
	 */
	if( PySequence_Size( args ) == 1 ) {
		PyObject *tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check( tmp ) ) 
			args = tmp;
	}

	/* if sequence is empty, do nothing */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_RETURN_NONE;
	}

	edge_table = (unsigned int *)MEM_callocN( len*sizeof( unsigned int ),
			"edge_table" );

	/* get the indices of edges to be removed */
	for( i = len; i--; ) {
		PyObject *tmp = PySequence_GetItem( args, i );
		if( BPy_MEdge_Check( tmp ) )
			edge_table[i] = ((BPy_MEdge *)tmp)->index;
		else if( PyInt_Check( tmp ) )
			edge_table[i] = PyInt_AsLong ( tmp );
		else {
			MEM_freeN( edge_table );
			Py_DECREF( tmp );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of ints or MEdges" );
		}
		Py_DECREF( tmp );

		/* if index out-of-range, throw exception */
		if( edge_table[i] >= (unsigned int)mesh->totedge ) {
			MEM_freeN( edge_table );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"array index out of range" );
		}
	}

	/*
	 * build two tables: first table marks vertices which belong to an edge
	 * which is being deleted
	 */
	del_table = (unsigned int *)MEM_callocN( 
			mesh->totvert*sizeof( unsigned int ), "vert_table" );

	/*
	 * Borrow a trick from editmesh code: for each edge to be deleted, mark
	 * its vertices as well.  Then go through face list and look for two
	 * consecutive marked vertices.
	 */

	/* mark each edge that's to be deleted */
	srcedge = mesh->medge;
	for( i = len; i--; ) {
		unsigned int idx = edge_table[i];
		del_table[srcedge[idx].v1] = UINT_MAX;
		del_table[srcedge[idx].v2] = UINT_MAX;
		srcedge[idx].v1 = UINT_MAX;
	}

	/*
	 * second table is used for vertices which become orphaned (belong to no
	 * edges) and need to be deleted; it's also the normal lookup table for
	 * old->new vertex indices
	 */

	vert_table = (unsigned int *)MEM_mallocN( 
			mesh->totvert*sizeof( unsigned int ), "vert_table" );

	/* assume all edges will be deleted (fills with UINT_MAX) */
	memset( vert_table, UCHAR_MAX, mesh->totvert*sizeof( unsigned int ) );

	/* unmark vertices of each "good" edge; count each "bad" edge */
	edge_count = 0;
	for( i = mesh->totedge; i--; ++srcedge )
		if( srcedge->v1 != UINT_MAX )
			vert_table[srcedge->v1] = vert_table[srcedge->v2] = 0;
		else
			++edge_count;

	/*
	 * find faces which no longer have all edges
	 */

	face_count = 0;
	srcface = mesh->mface;
	for( i = 0; i < mesh->totface; ++i, ++srcface ) {
		int len = srcface->v4 ? 4 : 3;
		unsigned int id[4];
		int del;

		id[0] = del_table[srcface->v1];
		id[1] = del_table[srcface->v2];
		id[2] = del_table[srcface->v3];
		id[3] = del_table[srcface->v4];

		del = ( id[0] == UINT_MAX && id[1] == UINT_MAX ) ||
			( id[1] == UINT_MAX && id[2] == UINT_MAX );
		if( !del ) {
			if( len == 3 )
				del = ( id[2] == UINT_MAX && id[0] == UINT_MAX );
			else
				del = ( id[2] == UINT_MAX && id[3] == UINT_MAX ) ||
					( id[3] == UINT_MAX && id[0] == UINT_MAX );
		}
		if( del ) {
			srcface->v1 = UINT_MAX;
			++face_count;
		} 
	}

	/* fix the vertex lookup table, if any verts to delete, do so now */
	vert_count = make_vertex_table( vert_table, mesh->totvert );
	if( vert_count )
		delete_verts( mesh, vert_table, vert_count );

	/* delete faces which have a deleted edge */
	delete_faces( mesh, vert_table, face_count );

	/* now delete the edges themselves */
	delete_edges( mesh, vert_table, edge_count );

	/* clean up and return */
	MEM_freeN( del_table );
	MEM_freeN( vert_table );
	MEM_freeN( edge_table );
	mesh_update ( mesh );
	Py_RETURN_NONE;
}

static PyObject *MEdgeSeq_collapse( BPy_MEdgeSeq * self, PyObject *args )
{
	MEdge *srcedge;
	unsigned int *edge_table;
	float (*vert_list)[3];
	int i, len;
	Base *base, *basact;
	Mesh *mesh = self->mesh;
	Object *object = NULL; 
	PyObject *tmp;

	/*
	 * when using removedoublesflag(), we need to switch to editmode, so
	 * nobody else can be using it
	 */

	if( G.obedit )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"can't use collapse() while in edit mode" );

	/* make sure we get a tuple of sequences of something */
	switch( PySequence_Size( args ) ) {
	case 1:
		/* if a sequence... */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check( tmp ) ) {
			PyObject *tmp2;

			/* ignore empty sequences */
			if( !PySequence_Size( tmp ) ) {
				Py_RETURN_NONE;
			}

			/* if another sequence, use it */
			tmp2 = PySequence_ITEM( tmp, 0 );
			if( PySequence_Check( tmp2 ) )
				args = tmp;
			Py_INCREF( args );
			Py_DECREF( tmp2 );
		} else
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of sequence pairs" );
		break;
	case 2:	/* two args may be individual edges/verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		/*
		 * if first item isn't a sequence, then assume it's a bunch of MVerts
		 * and wrap inside a tuple
		 */
		if( !PySequence_Check( tmp ) ) {
			args = Py_BuildValue( "(O)", args );
			if( !args )
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"Py_BuildValue() failed" );
		/*
		 * otherwise, assume it already a bunch of sequences so use as-is
		 */
		} else { 
			Py_INCREF( args );		/* so we can safely DECREF later */
		}
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of sequence pairs" );
	}

	/* if sequence is empty, do nothing */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_RETURN_NONE;
	}

	/* allocate table of edge indices and new vertex values */

	edge_table = (unsigned int *)MEM_callocN( len*sizeof( unsigned int ),
			"edge_table" );
	vert_list = (float (*)[3])MEM_callocN( 3*len*sizeof( float ),
			"vert_list" );

	/* get the indices of edges to be collapsed and new vert locations */
	for( i = len; i--; ) {
		PyObject *tmp1;
		PyObject *tmp2;

		tmp = PySequence_GetItem( args, i );

		/* if item isn't sequence of size 2, error */
		if( !PySequence_Check( tmp ) || PySequence_Size( tmp ) != 2 ) {
			MEM_freeN( edge_table );
			MEM_freeN( vert_list );
			Py_DECREF( tmp );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of (MEdges, vector)" );
		}

		/* if items aren't a MEdge/int and vector, error */
		tmp1 = PySequence_GetItem( tmp, 0 );
		tmp2 = PySequence_GetItem( tmp, 1 );
		Py_DECREF( tmp );
		if( !(BPy_MEdge_Check( tmp1 ) || PyInt_Check( tmp1 )) ||
				!VectorObject_Check ( tmp2 ) ) {
			MEM_freeN( edge_table );
			MEM_freeN( vert_list );
			Py_DECREF( tmp1 );
			Py_DECREF( tmp2 );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of (MEdges, vector)" );
		}

		/* store edge index, new vertex location */
		if( PyInt_Check( tmp1 ) )
			edge_table[i] = PyInt_AsLong ( tmp1 );
		else
			edge_table[i] = ((BPy_MEdge *)tmp1)->index;
		memcpy( vert_list[i], ((VectorObject *)tmp2)->vec,
				3*sizeof( float ) );
		Py_DECREF( tmp1 );
		Py_DECREF( tmp2 );

		/* if index out-of-range, throw exception */
		if( edge_table[i] >= (unsigned int)mesh->totedge ) {
			MEM_freeN( edge_table );
			MEM_freeN( vert_list );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"edge index out of range" );
		}
	}

	/*
	 * simple algorithm:
	 * (1) deselect all verts
	 * (2) for each edge
	 *   (2a) replace both verts with the new vert
	 *   (2b) select both verts
	 * (3) call removedoublesflag()
	 */

	/* (1) deselect all verts */
	for( i = mesh->totvert; i--; )
		mesh->mvert[i].flag &= ~SELECT;

	/* (2) replace edge's verts and select them */
	for( i = len; i--; ) {
		srcedge = &mesh->medge[edge_table[i]];
		memcpy( &mesh->mvert[srcedge->v1].co, vert_list[i], 3*sizeof( float ) );
		memcpy( &mesh->mvert[srcedge->v2].co, vert_list[i], 3*sizeof( float ) );
		mesh->mvert[srcedge->v1].flag |= SELECT;
		mesh->mvert[srcedge->v2].flag |= SELECT;
	}

	/* (3) call removedoublesflag() */
	for( base = FIRSTBASE; base; base = base->next ) {
		if( base->object->type == OB_MESH && 
				base->object->data == self->mesh ) {
			object = base->object;
			break;
		}
	}

	basact = BASACT;
	BASACT = base;
	
	removedoublesflag( 1, 0, 0.0 );
	/* make mesh's object active, enter mesh edit mode */
	G.obedit = object;
	
	/* exit edit mode, free edit mesh */
	load_editMesh();
	free_editMesh(G.editMesh);
	
	BASACT = basact;

	/* clean up and exit */
	Py_DECREF( args );
	MEM_freeN( vert_list );
	MEM_freeN( edge_table );
	mesh_update ( mesh );
	Py_RETURN_NONE;
}


static PyObject *MEdgeSeq_selected( BPy_MEdgeSeq * self )
{
	int i, count;
	Mesh *mesh = self->mesh;
	MEdge *tmpedge;
	PyObject *list;

	/* first count selected edges (quicker than appending to PyList?) */
	count = 0;
	tmpedge = mesh->medge;
	for( i = 0; i < mesh->totedge; ++i, ++tmpedge )
		if( (mesh->mvert[tmpedge->v1].flag & SELECT) &&
				(mesh->mvert[tmpedge->v2].flag & SELECT) )
			++count;

	list = PyList_New( count );
	if( !list )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyList_New() failed" );

	/* next, insert selected edges into list */
	count = 0;
	tmpedge = mesh->medge;
	for( i = 0; i < mesh->totedge; ++i, ++tmpedge ) {
		if( (mesh->mvert[tmpedge->v1].flag & SELECT) &&
				(mesh->mvert[tmpedge->v2].flag & SELECT) ) {
			PyObject *tmp = PyInt_FromLong( i );
			if( !tmp ) {
				Py_DECREF( list );
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"PyInt_FromLong() failed" );
			}
			PyList_SET_ITEM( list, count, tmp );
			++count;
		}
	}
	return list;
}

static PyObject *MEdgeSeq_add_layertype(BPy_MEdgeSeq *self, PyObject *args)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_addPropLayer_internal(me, &(me->edata), me->totedge, args);
}
static PyObject *MEdgeSeq_del_layertype(BPy_MEdgeSeq *self, PyObject *value)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_removePropLayer_internal(me, &(me->edata), me->totedge, value);
}
static PyObject *MEdgeSeq_rename_layertype(BPy_MEdgeSeq *self, PyObject *args)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_renamePropLayer_internal(me,&(me->edata),args);
}
static PyObject *MEdgeSeq_PropertyList(BPy_MEdgeSeq *self) 
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_propList_internal(&(me->edata));
}


static struct PyMethodDef BPy_MEdgeSeq_methods[] = {
	{"extend", (PyCFunction)MEdgeSeq_extend, METH_VARARGS,
		"add edges to mesh"},
	{"delete", (PyCFunction)MEdgeSeq_delete, METH_VARARGS,
		"delete edges from mesh"},
	{"selected", (PyCFunction)MEdgeSeq_selected, METH_NOARGS,
		"returns a list containing indices of selected edges"},
	{"collapse", (PyCFunction)MEdgeSeq_collapse, METH_VARARGS,
		"collapse one or more edges to a vertex"},
	{"addPropertyLayer",(PyCFunction)MEdgeSeq_add_layertype, METH_VARARGS,
		"add a new property layer"},
	{"removePropertyLayer",(PyCFunction)MEdgeSeq_del_layertype, METH_O,
		"removes a property layer"},
	{"renamePropertyLayer",(PyCFunction)MEdgeSeq_rename_layertype, METH_VARARGS,
		"renames an existing property layer"},

	{NULL, NULL, 0, NULL}
};
static PyGetSetDef BPy_MEdgeSeq_getseters[] = {
	{"properties",
	(getter)MEdgeSeq_PropertyList, (setter)NULL,
	"edge property layers, read only",
	NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


/************************************************************************
 *
 * Python MEdgeSeq_Type standard operators
 *
 ************************************************************************/

/*****************************************************************************/
/* Python MEdgeSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MEdgeSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MEdgeSeq",           /* char *tp_name; */
	sizeof( BPy_MEdgeSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MEdgeSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) MEdgeSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MEdgeSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MEdgeSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MEdgeSeq_getseters,     /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/************************************************************************
 *
 * Face attributes
 *
 ************************************************************************/

static MFace * MFace_get_pointer( BPy_MFace * self )
{
	if( self->index >= self->mesh->totface )
		return (MFace *)EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"MFace is no longer valid" );
	return &self->mesh->mface[self->index];
}

/*
 * get a face's vertices
 */

static PyObject *MFace_getVerts( BPy_MFace * self )
{
	PyObject *attr;
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

	attr = PyTuple_New( face->v4 ? 4 : 3 );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	PyTuple_SetItem( attr, 0, MVert_CreatePyObject( self->mesh, face->v1 ) );
	PyTuple_SetItem( attr, 1, MVert_CreatePyObject( self->mesh, face->v2 ) );
	PyTuple_SetItem( attr, 2, MVert_CreatePyObject( self->mesh, face->v3 ) );
	if( face->v4 )
		PyTuple_SetItem( attr, 3, MVert_CreatePyObject( self->mesh,
					face->v4 ) );

	return attr;
}

/*
 * set a face's vertices
 */

static int MFace_setVerts( BPy_MFace * self, PyObject * args )
{
	BPy_MVert *v1, *v2, *v3, *v4 = NULL;
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return -1;

	if( !PyArg_ParseTuple ( args, "O!O!O!|O!", &MVert_Type, &v1,
				&MVert_Type, &v2, &MVert_Type, &v3, &MVert_Type, &v4 ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected tuple of 3 or 4 MVerts" );
	
	if(	v1->index == v2->index || 
		v1->index == v3->index || 
		v2->index == v3->index  ) 
		return EXPP_ReturnIntError( PyExc_ValueError,
			"cannot assign 2 or move verts that are the same" );
	
	if(v4 && (	v1->index == v4->index ||
				v2->index == v4->index ||
				v3->index == v4->index ))
		return EXPP_ReturnIntError( PyExc_ValueError,
			"cannot assign 2 or move verts that are the same" );
	
	if(		v1->index >= self->mesh->totvert || 
			v2->index >= self->mesh->totvert || 
			v3->index >= self->mesh->totvert ||
	(v4 &&(	v4->index >= self->mesh->totvert)))
		return EXPP_ReturnIntError( PyExc_ValueError,
			"cannot assign verts that have been removed" );
	
	face->v1 = v1->index;
	face->v2 = v2->index;
	face->v3 = v3->index;
	if( v4 )
		face->v4 = v4->index;
	return 0;
}

/*
 * get face's material index
 */

static PyObject *MFace_getMat( BPy_MFace * self )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

	return PyInt_FromLong( face->mat_nr );
}

/*
 * set face's material index
 */

static int MFace_setMat( BPy_MFace * self, PyObject * value )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return -1; /* error is set */

	return EXPP_setIValueRange( value, &face->mat_nr, 0, 15, 'b' );
}

/*
 * get a face's index
 */

static PyObject *MFace_getIndex( BPy_MFace * self )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL; /* error is set */

	return PyInt_FromLong( self->index );
}

/*
 * get face's normal index
 */

static PyObject *MFace_getNormal( BPy_MFace * self )
{
	float *vert[4];
	float no[3];
	MFace *face = MFace_get_pointer( self );

	Mesh *me = self->mesh;
	
	if( !face )
	return NULL; /* error is set */
	
	if MFACE_VERT_BADRANGE_CHECK(me, face)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"one or more MFace vertices are no longer valid" );

	vert[0] = me->mvert[face->v1].co;
	vert[1] = me->mvert[face->v2].co;
	vert[2] = me->mvert[face->v3].co;
	if( face->v4 ) {
		vert[3] = me->mvert[face->v4].co;
		CalcNormFloat4( vert[0], vert[1], vert[2], vert[3], no );
	} else
		CalcNormFloat( vert[0], vert[1], vert[2], no );

	return newVectorObject( no, 3, Py_NEW );
}

/*
 * get face's center location
 */

static PyObject *MFace_getCent( BPy_MFace * self )
{
	float *vert[4];
	float cent[3]= {0,0,0};
	int i=3, j, k;
	Mesh *me = self->mesh;
	MFace *face = MFace_get_pointer( self );
	
	if( !face )
		return NULL; /* error is set */
	

	if MFACE_VERT_BADRANGE_CHECK(me, face)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"one or more MFace vertices are no longer valid" );

	vert[0] = me->mvert[face->v1].co;
	vert[1] = me->mvert[face->v2].co;
	vert[2] = me->mvert[face->v3].co;
	if( face->v4 ) {
		vert[3] = me->mvert[face->v4].co;
		i=4;
	} 
	
	for (j=0;j<i;j++) {
		for (k=0;k<3;k++) {
			cent[k]+=vert[j][k];
		}
	}
	
	for (j=0;j<3;j++) {
		cent[j]=cent[j]/i;
	}
	return newVectorObject( cent, 3, Py_NEW );
}

/*
 * get face's area
 */
static PyObject *MFace_getArea( BPy_MFace * self )
{
	float *v1,*v2,*v3,*v4;
	MFace *face = MFace_get_pointer( self );
	Mesh *me = self->mesh;
	
	if( !face )
		return NULL; /* error is set */

	if MFACE_VERT_BADRANGE_CHECK(me, face)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"one or more MFace vertices are no longer valid" );

	v1 = me->mvert[face->v1].co;
	v2 = me->mvert[face->v2].co;
	v3 = me->mvert[face->v3].co;
	
	if( face->v4 ) {
		v4 = me->mvert[face->v4].co;
		return PyFloat_FromDouble( AreaQ3Dfl(v1, v2, v3, v4));
	} else
		return PyFloat_FromDouble( AreaT3Dfl(v1, v2, v3));
}

/*
 * get one of a face's mface flag bits
 */

static PyObject *MFace_getMFlagBits( BPy_MFace * self, void * type )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL; /* error is set */

	return EXPP_getBitfield( &face->flag, (int)((long)type & 0xff), 'b' );
}

/*
 * set one of a face's mface flag bits
 */

static int MFace_setMFlagBits( BPy_MFace * self, PyObject * value,
		void * type )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return -1; /* error is set */

	return EXPP_setBitfield( value, &face->flag, 
			(int)((long)type & 0xff), 'b' );
}

static int MFace_setSelect( BPy_MFace * self, PyObject * value,
		void * type_unused )
{
	MFace *face = MFace_get_pointer( self );
	int param = PyObject_IsTrue( value );
	Mesh *me;

	if( !face )
		return -1; /* error is set */

	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );

	me = self->mesh;
	if( param ) {
		face->flag |= ME_FACE_SEL;
		me->mvert[face->v1].flag |= SELECT;
		me->mvert[face->v2].flag |= SELECT;
		me->mvert[face->v3].flag |= SELECT;
		if( face->v4 )
			me->mvert[face->v4].flag |= SELECT;
	}
	else {
		face->flag &= ~ME_FACE_SEL;
		me->mvert[face->v1].flag &= ~SELECT;
		me->mvert[face->v2].flag &= ~SELECT;
		me->mvert[face->v3].flag &= ~SELECT;
		if( face->v4 )
			me->mvert[face->v4].flag &= ~SELECT;
	}

	if( self->mesh->mselect ) {
		MEM_freeN( self->mesh->mselect );
		self->mesh->mselect = NULL;
	}

	return 0;
}

/*
 * get face's texture image
 */

static PyObject *MFace_getImage( BPy_MFace *self )
{
	MTFace *face;
	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	face = &self->mesh->mtface[self->index];

	if( face->tpage )
		return Image_CreatePyObject( face->tpage );
	else
		Py_RETURN_NONE;
}

/*
 * change or clear face's texture image
 */

static int MFace_setImage( BPy_MFace *self, PyObject *value )
{
	MTFace *face;

	if( !MFace_get_pointer( self ) )
		return -1;

	if( value && value != Py_None && !BPy_Image_Check( value ) )
	    return EXPP_ReturnIntError( PyExc_TypeError,
		    "expected image object or None" );

	if( !self->mesh->mtface )
#if 0
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );
#else
		make_tfaces( self->mesh );
#endif

	face = &self->mesh->mtface[self->index];

	if( value == NULL || value == Py_None )
        	face->tpage = NULL;		/* should memory be freed? */
	else {
		face->tpage = ( ( BPy_Image * ) value )->image;
		face->mode |= TF_TEX;
	}

	return 0;
}

#define MFACE_FLAG_BITMASK ( TF_SELECT | TF_SEL1 | \
		TF_SEL2 | TF_SEL3 | TF_SEL4 | TF_HIDE )

/*
* get face's texture flag
*/

static PyObject *MFace_getFlag( BPy_MFace *self )
{
	int flag;
	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	flag = self->mesh->mtface[self->index].flag & MFACE_FLAG_BITMASK;
	
	/* so old scripts still work */
	if (self->index == self->mesh->act_face)
		flag |= TF_ACTIVE;
		
	return PyInt_FromLong( (long)( flag ) );
}

/*
 * set face's texture flag
 */

static int MFace_setFlag( BPy_MFace *self, PyObject *value )
{
	int param;

	if( !self->mesh->mtface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PyInt_Check ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", MFACE_FLAG_BITMASK );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );

	/* only one face can be active, so don't allow that here */
	if( param & TF_ACTIVE )
		param &= ~TF_ACTIVE;
	
	if( ( param & MFACE_FLAG_BITMASK ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	/* merge active setting with other new params */
	param |= (self->mesh->mtface[self->index].flag);
	self->mesh->mtface[self->index].flag = (char)param;

	return 0;
}

/*
 * get face's texture mode
 */

static PyObject *MFace_getMode( BPy_MFace *self )
{
	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	return PyInt_FromLong( self->mesh->mtface[self->index].mode );
}

/*
 * set face's texture mode
 */

static int MFace_setMode( BPy_MFace *self, PyObject *value )
{
	int param;
	static short bitmask = TF_DYNAMIC
				| TF_TEX
				| TF_SHAREDVERT
				| TF_LIGHT
				| TF_SHAREDCOL
				| TF_TILES
				| TF_BILLBOARD
				| TF_TWOSIDE
				| TF_INVISIBLE
				| TF_OBCOL
				| TF_BILLBOARD2
				| TF_SHADOW
				| TF_BMFONT;

	if( !self->mesh->mtface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PyInt_Check ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );
	
	if( param == 0xffff )		/* if param is ALL, set everything but HALO */
		param = bitmask ^ TF_BILLBOARD;
	else if( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	/* Blender UI doesn't allow these on at the same time */

	if( ( param & (TF_BILLBOARD | TF_BILLBOARD2) ) == 
			(TF_BILLBOARD | TF_BILLBOARD2) )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"HALO and BILLBOARD cannot be enabled simultaneously" );

	self->mesh->mtface[self->index].mode = (short)param;

	return 0;
}

/*
 * get face's texture transparency setting
 */

static PyObject *MFace_getTransp( BPy_MFace *self )
{
	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	return PyInt_FromLong( self->mesh->mtface[self->index].transp );
}

/*
 * set face's texture transparency setting
 */

static int MFace_setTransp( BPy_MFace *self, PyObject *value )
{
	if( !self->mesh->mtface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	return EXPP_setIValueRange( value,
			&self->mesh->mtface[self->index].transp, TF_SOLID, TF_SUB, 'b' );
}

/*
 * get a face's texture UV coord values
 */

static PyObject *MFace_getUV( BPy_MFace * self )
{
	MTFace *face;
	PyObject *attr;
	int length, i;

	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	face = &self->mesh->mtface[self->index];
	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	attr = PyTuple_New( length );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	for( i=0; i<length; ++i ) {
		PyObject *vector = newVectorObject( face->uv[i], 2, Py_WRAP );
		if( !vector )
			return NULL;
		PyTuple_SetItem( attr, i, vector );
	}

	return attr;
}

/*
 * set a face's texture UV coord values
 */

static int MFace_setUV( BPy_MFace * self, PyObject * value )
{
	MTFace *face;
	int length, i;

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PySequence_Check( value ) ||
			EXPP_check_sequence_consistency( value, &vector_Type ) != 1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "expected sequence of vectors" );

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	if( length != PySequence_Size( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "size of vertex and UV sequences differ" );

	if( !self->mesh->mtface )
#if 0
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );
#else
		make_tfaces( self->mesh );
#endif

	face = &self->mesh->mtface[self->index];
	for( i=0; i<length; ++i ) {
		VectorObject *vector = (VectorObject *)PySequence_ITEM( value, i );
		face->uv[i][0] = vector->vec[0];
		face->uv[i][1] = vector->vec[1];
		Py_DECREF( vector );
	}
	return 0;
}

/*
 * get a face's texture UV coord select state
 */

static PyObject *MFace_getUVSel( BPy_MFace * self )
{
	MTFace *face;
	PyObject *attr;
	int length, i, mask;

	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	face = &self->mesh->mtface[self->index];
	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	attr = PyTuple_New( length );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	/* get coord select state, one bit at a time */
	mask = TF_SEL1;
	for( i=0; i<length; ++i, mask <<= 1 ) {
		PyObject *value = PyInt_FromLong( face->flag & mask ? 1 : 0 );
		if( !value ) {
			Py_DECREF( attr );
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyInt_FromLong() failed" );
		}
		PyTuple_SetItem( attr, i, value );
	}

	return attr;
}

/*
 * set a face's texture UV coord select state
 */

static int MFace_setUVSel( BPy_MFace * self, PyObject * value )
{
	MTFace *face;
	int length, i, mask;

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PySequence_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a tuple of integers" );

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	if( length != PySequence_Size( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "size of vertex and UV lists differ" );

	if( !self->mesh->mtface )
#if 0
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );
#else
		make_tfaces( self->mesh );
#endif

	/* set coord select state, one bit at a time */
	face = &self->mesh->mtface[self->index];
	mask = TF_SEL1;
	for( i=0; i<length; ++i, mask <<= 1 ) {
		PyObject *tmp = PySequence_GetItem( value, i ); /* adds a reference, remove below */
		if( !PyInt_Check( tmp ) ) {
			Py_DECREF(tmp);
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected a tuple of integers" );
		}
		if( PyInt_AsLong( tmp ) )
			face->flag |= mask;
		else
			face->flag &= ~mask;
		Py_DECREF(tmp);
	}
	return 0;
}

/*
 * get a face's vertex colors. note that if mesh->mtfaces is defined, then 
 * it takes precedent over mesh->mcol
 */

static PyObject *MFace_getCol( BPy_MFace * self )
{
	PyObject *attr;
	int length, i;
	MCol * mcol;

	/* if there's no mesh color vectors or texture faces, nothing to do */

	if( !self->mesh->mcol )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no vertex colors" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	mcol = &self->mesh->mcol[self->index*4];

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	attr = PyTuple_New( length );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	for( i=0; i<length; ++i ) {
		PyObject *color = MCol_CreatePyObject( &mcol[i] );
		if( !color )
			return NULL;
		PyTuple_SetItem( attr, i, color );
	}

	return attr;
}

/*
 * set a face's vertex colors
 */

static int MFace_setCol( BPy_MFace * self, PyObject *value )
{
	int length, i;
	MCol * mcol;

	/* if there's no mesh color vectors or texture faces, nothing to do */

	if( !self->mesh->mcol )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no vertex colors" );

	if( !MFace_get_pointer( self ) )
		return -1;

	mcol = &self->mesh->mcol[self->index*4];

	length = self->mesh->mface[self->index].v4 ? 4 : 3;

	if( !PyList_Check( value ) && !PyTuple_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a sequence of MCols" );

	if( EXPP_check_sequence_consistency( value, &MCol_Type ) != 1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a sequence of MCols" );

	if( PySequence_Size( value ) != length )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"incorrect number of colors for this face" );

	for( i=0; i<length; ++i ) {
		BPy_MCol *obj = (BPy_MCol *)PySequence_ITEM( value, i );
		mcol[i].r = obj->color->r;
		mcol[i].g = obj->color->g;
		mcol[i].b = obj->color->b;
		mcol[i].a = obj->color->a;
		Py_DECREF( obj );
	}
	return 0;
}


/*
 * get edge keys for using in a dictionary or set key
 */

static PyObject *MFace_getEdgeKeys( BPy_MFace * self )
{
	MFace *face = MFace_get_pointer( self );
	PyObject *attr, *edpair;
	
	if (!face)
		return NULL; /* error set */
	
	if (face->v4) {
		attr = PyTuple_New( 4 );
		edpair = PyTuple_New( 2 );
		if (face->v1 > face->v2) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v2) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v1) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v1) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v2) );
		}
		PyTuple_SET_ITEM( attr, 0, edpair );
		
		edpair = PyTuple_New( 2 );
		if (face->v2 > face->v3) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v3) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v2) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v2) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v3) );
		}
		PyTuple_SET_ITEM( attr, 1, edpair );

		edpair = PyTuple_New( 2 );
		if (face->v3 > face->v4) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v4) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v3) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v3) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v4) );
		}
		PyTuple_SET_ITEM( attr, 2, edpair );
		
		edpair = PyTuple_New( 2 );
		if (face->v4 > face->v1) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v1) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v4) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v4) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v1) );
		}
		PyTuple_SET_ITEM( attr, 3, edpair );
		
	} else {
		
		attr = PyTuple_New( 3 );
		edpair = PyTuple_New( 2 );
		if (face->v1 > face->v2) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v2) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v1) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v1) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v2) );
		}
		PyTuple_SET_ITEM( attr, 0, edpair );
		
		edpair = PyTuple_New( 2 );
		if (face->v2 > face->v3) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v3) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v2) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v2) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v3) );
		}
		PyTuple_SET_ITEM( attr, 1, edpair );

		edpair = PyTuple_New( 2 );
		if (face->v3 > face->v1) {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v1) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v3) );
		} else {
			PyTuple_SET_ITEM( edpair, 0, PyInt_FromLong(face->v3) );
			PyTuple_SET_ITEM( edpair, 1, PyInt_FromLong(face->v1) );
		}
		PyTuple_SET_ITEM( attr, 2, edpair );
	}
	
	return attr;
}


/************************************************************************
 *
 * Python MFace_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MFace_getseters[] = {
    {"verts",
     (getter)MFace_getVerts, (setter)MFace_setVerts,
     "face's vertices",
     NULL},
    {"v",
     (getter)MFace_getVerts, (setter)MFace_setVerts,
     "deprecated: see 'verts'",
     NULL},
    {"mat",
     (getter)MFace_getMat, (setter)MFace_setMat,
     "face's material index",
     NULL},
    {"index",
     (getter)MFace_getIndex, (setter)NULL,
     "face's index",
     NULL},
    {"no",
     (getter)MFace_getNormal, (setter)NULL,
     "face's normal",
     NULL},
    {"cent",
     (getter)MFace_getCent, (setter)NULL,
     "face's center",
     NULL},
    {"area",
     (getter)MFace_getArea, (setter)NULL,
     "face's 3D area",
     NULL},

    {"hide",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face hidden in edit mode",
     (void *)ME_HIDE},
    {"sel",
     (getter)MFace_getMFlagBits, (setter)MFace_setSelect,
     "face selected in edit mode",
     (void *)ME_FACE_SEL},
    {"smooth",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face smooth enabled",
     (void *)ME_SMOOTH},

	/* attributes for texture faces (mostly, I think) */

    {"col",
     (getter)MFace_getCol, (setter)MFace_setCol,
     "face's vertex colors",
     NULL},
    {"flag",
     (getter)MFace_getFlag, (setter)MFace_setFlag,
     "flags associated with texture faces",
     NULL},
    {"image",
     (getter)MFace_getImage, (setter)MFace_setImage,
     "image associated with texture faces",
     NULL},
    {"mode",
     (getter)MFace_getMode, (setter)MFace_setMode,
     "modes associated with texture faces",
     NULL},
    {"transp",
     (getter)MFace_getTransp, (setter)MFace_setTransp,
     "transparency of texture faces",
     NULL},
    {"uv",
     (getter)MFace_getUV, (setter)MFace_setUV,
     "face's UV coordinates",
     NULL},
    {"uvSel",
     (getter)MFace_getUVSel, (setter)MFace_setUVSel,
     "face's UV coordinates select status",
     NULL},
    {"edge_keys",
     (getter)MFace_getEdgeKeys, (setter)NULL,
     "for each edge this face uses return an ordered tuple edge pair that can be used as a key in a dictionary or set",
     NULL},

	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MFace_Type iterator (iterates over vertices)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MFace_getIter( BPy_MFace * self )
{
	if (self->iter==-1) {
		self->iter = 0;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		BPy_MFace *seq= (BPy_MFace *)MFace_CreatePyObject(self->mesh, self->index);
		seq->iter = 0;
		return (PyObject *) seq;
	}
}

/*
 * Return next MVert.  Throw an exception after the final vertex.
 */

static PyObject *MFace_nextIter( BPy_MFace * self )
{
	struct MFace *face = &self->mesh->mface[self->index];
	int len = self->mesh->mface[self->index].v4 ? 4 : 3;

	if( self->iter == len ) {
		self->iter = -1;
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	++self->iter;
	switch ( self->iter ) {
	case 1:
		return MVert_CreatePyObject( self->mesh, face->v1 );
	case 2:
		return MVert_CreatePyObject( self->mesh, face->v2 );
	case 3:
		return MVert_CreatePyObject( self->mesh, face->v3 );
	default :
		return MVert_CreatePyObject( self->mesh, face->v4 );
	}
}

/************************************************************************
 *
 * Python MFace_Type methods
 *
 ************************************************************************/

/************************************************************************
 *
 * Python MFace_Type standard operations
 *
 ************************************************************************/
static int MFace_compare( BPy_MFace * a, BPy_MFace * b )
{
	return( a->mesh == b->mesh && a->index == b->index ) ? 0 : -1;
}

static PyObject *MFace_repr( BPy_MFace* self )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

	if( face->v4 )
		return PyString_FromFormat( "[MFace (%d %d %d %d) %d]",
				(int)face->v1, (int)face->v2, 
				(int)face->v3, (int)face->v4, (int)self->index ); 
	else
		return PyString_FromFormat( "[MFace (%d %d %d) %d]",
				(int)face->v1, (int)face->v2,
				(int)face->v3, (int)self->index ); 
}

static long MFace_hash( BPy_MFace *self )
{
	return (long)self->index;
}

static int MFace_len( BPy_MFace * self )
{
	if( self->index >= self->mesh->totface )
		return 0;
	return self->mesh->mface[self->index].v4 ? 4 : 3;
}

static PySequenceMethods MFace_as_sequence = {
	( inquiry ) MFace_len,         /* sq_length */
	( binaryfunc ) 0,	           /* sq_concat */
	( intargfunc ) 0,	           /* sq_repeat */
	( intargfunc ) 0,              /* sq_item */
	( intintargfunc ) 0,           /* sq_slice */
	( intobjargproc ) 0,           /* sq_ass_item */
	( intintobjargproc ) 0,        /* sq_ass_slice */
	0,0,0,
};

static PyObject *MFace_getProp( BPy_MFace *self, PyObject *args)
{
	Mesh *me = (Mesh *)self->mesh;
	MFace *face = MFace_get_pointer( self );
	if( !face )
		return NULL;
	mesh_update_customdata_pointers(me); //!
	return Mesh_getProperty_internal(&(me->fdata), self->index, args);
}

static PyObject *MFace_setProp( BPy_MFace *self,  PyObject *args)
{
	Mesh *me = (Mesh *)self->mesh;
	PyObject *obj;
	MFace *face = MFace_get_pointer( self );
	if( !face )
		return NULL; /* error set */
	
	obj = Mesh_setProperty_internal(&(me->fdata), self->index, args);
	mesh_update_customdata_pointers(me); //!
	return obj;
}

static struct PyMethodDef BPy_MFace_methods[] = {
	{"getProperty", (PyCFunction)MFace_getProp, METH_O,
		"get property indicated by name"},
	{"setProperty", (PyCFunction)MFace_setProp, METH_VARARGS,
		"set property indicated by name"},
	{NULL, NULL, 0, NULL}
};
/************************************************************************
 *
 * Python MFace_Type structure definition
 *
 ************************************************************************/

PyTypeObject MFace_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MFace",            /* char *tp_name; */
	sizeof( BPy_MFace ),        /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MFace_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MFace_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MFace_as_sequence,         /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) MFace_hash,    /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc ) MFace_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MFace_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MFace_methods,          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MFace_getseters,        /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

static PyObject *MFace_CreatePyObject( Mesh * mesh, int i )
{
	BPy_MFace *obj = PyObject_NEW( BPy_MFace, &MFace_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->mesh = mesh;
	obj->index = i;
	obj->iter= -1;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Face sequence 
 *
 ************************************************************************/

static int MFaceSeq_len( BPy_MFaceSeq * self )
{
	return self->mesh->totface;
}

static PyObject *MFaceSeq_item( BPy_MFaceSeq * self, int i )
{
	if( i < 0 || i >= self->mesh->totface )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return MFace_CreatePyObject( self->mesh, i );
}

static PySequenceMethods MFaceSeq_as_sequence = {
	( inquiry ) MFaceSeq_len,      /* sq_length */
	( binaryfunc ) 0,	           /* sq_concat */
	( intargfunc ) 0,	           /* sq_repeat */
	( intargfunc ) MFaceSeq_item,  /* sq_item */
	( intintargfunc ) 0,           /* sq_slice */
	( intobjargproc ) 0,           /* sq_ass_item */
	( intintobjargproc ) 0,        /* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MFaceSeq_Type iterator (iterates over faces)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MFaceSeq_getIter( BPy_MFaceSeq * self )
{
	if (self->iter==-1) {
		self->iter = 0;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		BPy_MFaceSeq *seq = (BPy_MFaceSeq *)MFaceSeq_CreatePyObject(self->mesh);
		seq->iter = 0;
		return (PyObject *)seq;
	}
}

/*
 * Return next MFace.
 */

static PyObject *MFaceSeq_nextIter( BPy_MFaceSeq * self )
{
	if( self->iter == self->mesh->totface ) {
		self->iter= -1; /* not being used in a seq */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	return MFace_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MFaceSeq_Type methods
 *
 ************************************************************************/

static PyObject *MFaceSeq_extend( BPy_MEdgeSeq * self, PyObject *args,
	  PyObject *keywds )
{
	/*
	 * (a) check input for valid edge objects, faces which consist of
	 *     only three or four edges
	 * (b) check input to be sure edges form a closed face (each edge
	 *     contains verts in two other different edges?)
	 *
	 * (1) build list of new faces; remove duplicates
	 *   * use existing "v4=0 rule" for 3-vert faces
	 * (2) build list of existing faces for searching
	 * (3) from new face list, remove existing faces:
	 */

	int len, nverts;
	int i, j, k, new_face_count;
	int good_faces;
	SrchFaces *oldpair, *newpair, *tmppair, *tmppair2;
	PyObject *tmp;
	MFace *tmpface;
	Mesh *mesh = self->mesh;
	int ignore_dups = 0;
	PyObject *return_list = NULL;
	char flag = ME_FACE_SEL;
	
	/* before we try to add faces, add edges; if it fails; exit */

	tmp = MEdgeSeq_extend( self, args );
	if( !tmp )
		return NULL;
	Py_DECREF( tmp );

	/* process any keyword arguments */
	if( keywds ) {
		PyObject *res = PyDict_GetItemString( keywds, "ignoreDups" );
		if( res ) {
			ignore_dups = PyObject_IsTrue( res );
			if (ignore_dups==-1) {
				return EXPP_ReturnPyObjError( PyExc_TypeError,
						"keyword argument \"ignoreDups\" expected True/False or 0/1" );
			}
		}
		res = PyDict_GetItemString( keywds, "indexList" );
		if (res) {
			switch( PyObject_IsTrue( res ) ) {
			case  0:
				break;
			case -1:
				return EXPP_ReturnPyObjError( PyExc_TypeError,
						"keyword argument \"indexList\" expected True/False or 0/1" );
			default:
				return_list = PyList_New( 0 );
			}
		}
		
		res = PyDict_GetItemString( keywds, "smooth" );
		if (res) {
			switch( PyObject_IsTrue( res ) ) {
				case  0:
					break;
				case -1:
					return EXPP_ReturnPyObjError( PyExc_TypeError,
							"keyword argument \"smooth\" expected True/False or 0/1" );
				default:
					flag |= ME_SMOOTH;
			
			}
		}
	}

	/* make sure we get a tuple of sequences of something */

	switch( PySequence_Size( args ) ) {
	case 1:		/* better be a sequence or a tuple */
		/* if a sequence... */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check( tmp ) ) {
			PyObject *tmp2;

			/* ignore empty sequences */
			if( !PySequence_Size( tmp ) ) {
				Py_RETURN_NONE;
			}

			/* if another sequence, use it */
			tmp2 = PySequence_ITEM( tmp, 0 );
			if( PySequence_Check( tmp2 ) )
				args = tmp;
			Py_INCREF( args );
			Py_DECREF( tmp2 );
		} else
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of sequence pairs" );
		break;
	case 2:	
	case 3:
	case 4:		/* two to four args may be individual verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		/*
		 * if first item isn't a sequence, then assume it's a bunch of MVerts
		 * and wrap inside a tuple
		 */
		if( !PySequence_Check( tmp ) ) {
			args = Py_BuildValue( "(O)", args );
			if( !args )
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"Py_BuildValue() failed" );
		/*
		 * otherwise, assume it already a bunch of sequences so use as-is
		 */
		} else { 
			Py_INCREF( args );		/* so we can safely DECREF later */
		}
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of sequence pairs" );
	}

	/* if nothing to add, just exit */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF( args );
		Py_RETURN_NONE;
	}

	/* 
	 * Since we call MEdgeSeq_extend first, we already know the input list
	 * is valid.  Here we just need to count the total number of faces.
	 */

	new_face_count = 0;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_ITEM( args, i );
		nverts = PySequence_Size( tmp );
		if( return_list || nverts != 2 )		
			++new_face_count; /* new faces must have 3 or 4 verts */
		Py_DECREF( tmp );
	}

	/* OK, commit to allocating the search structures */
	newpair = (SrchFaces *)MEM_callocN( sizeof(SrchFaces)*new_face_count,
			"MFacePairs" );

	/* scan the input list and build the new face pair list */
	len = PySequence_Size( args );
	tmppair = newpair;

	for( i = 0; i < len; ++i ) {
		MFace tmpface;
		unsigned int vert[4]={0,0,0,0};
		unsigned char order[4]={0,1,2,3};
		tmp = PySequence_GetItem( args, i );
		nverts = PySequence_Size( tmp );

		if( nverts == 2 ) {	/* again, ignore 2-vert tuples */
			if( return_list )	/* if returning indices, mark as deleted */
				tmppair->v[1] = 0;
			Py_DECREF( tmp );
			continue;
		}
	
		/*
		 * get the face's vertices' indexes
		 */
	
		for( j = 0; j < nverts; ++j ) {
			PyObject *item = PySequence_ITEM( tmp, j );
			if( BPy_MVert_Check( item ) )
				vert[j] = ((BPy_MVert *)item)->index;
			else
				vert[j] = PyInt_AsLong( item );
			Py_DECREF( item );
		}
		Py_DECREF( tmp );
		tmpface.v1 = vert[0];
		tmpface.v2 = vert[1];
		tmpface.v3 = vert[2];
		tmpface.v4 = vert[3];

		/*
		 * go through some contortions to guarantee the third and fourth
		 * vertices are not index 0
		 */
		eeek_fix( &tmpface, nverts == 4 );
		vert[0] = tmpface.v1;
		vert[1] = tmpface.v2;
		vert[2] = tmpface.v3;
		if( nverts == 3 )
			vert[3] = tmppair->v[3] = 0;
		else
			vert[3] = tmpface.v4;

		/*
		 * sort the verts before placing in pair list.  the order of
		 * vertices in the face is very important, so keep track of
		 * the original order
		 */

		for( j = nverts-1; j >= 0; --j ) {
			for( k = 0; k < j; ++k ) {
				if( vert[k] > vert[k+1] ) {
					SWAP( int, vert[k], vert[k+1] );
					SWAP( char, order[k], order[k+1] );
				} else if( vert[k] == vert[k+1] ) {
					break;
				}
			}
			if( k < j )
				break;
			tmppair->v[j] = vert[j];
		}
		if( j >= 0 ) {				/* a duplicate vertex found */
			if( return_list ) {		/* if returning index list */
				tmppair->v[1] = 0;	/*    mark as deleted */
			} else {
				--new_face_count;	/* otherwise skip */
				continue;
			}
		}
		tmppair->index = i;

		/* pack order into a byte */
		tmppair->order = order[0]|(order[1]<<2)|(order[2]<<4)|(order[3]<<6);
		++tmppair;
	}

	/*
	 * find duplicates in the new list and mark.  if it's a duplicate,
	 * then mark by setting second vert index to 0 (a real edge won't have
	 * second vert index of 0 since verts are sorted)
	 */

	good_faces = new_face_count;	/* assume all faces good to start */

	tmppair = newpair;	/* "last good edge" */
	tmppair2 = &tmppair[1];	/* "current candidate edge" */
	if( !ignore_dups ) {

	/* sort the new face pairs */
		qsort( newpair, new_face_count, sizeof(SrchFaces), mface_comp );

		for( i = 0; i < new_face_count; ++i ) {
			if( mface_comp( tmppair, tmppair2 ) )
				tmppair = tmppair2;	/* last != current, so current == last */
			else {
				tmppair2->v[1] = 0; /* last == current, so mark as duplicate */
				--good_faces;		/* one less good face */
			}
			tmppair2++;
		}
	}

	/* if mesh has faces, see if any of the new faces are already in it */
	if( mesh->totface && !ignore_dups ) {
		oldpair = (SrchFaces *)MEM_callocN( sizeof(SrchFaces)*mesh->totface,
				"MFacePairs" );

		tmppair = oldpair;
		tmpface = mesh->mface;
		for( i = 0; i < mesh->totface; ++i ) {
			unsigned char order[4]={0,1,2,3};
			int verts[4];
			verts[0]=tmpface->v1;
			verts[1]=tmpface->v2;
			verts[2]=tmpface->v3;
			verts[3]=tmpface->v4;
	
			len = ( tmpface->v4 ) ? 3 : 2;
			tmppair->v[3] = 0;	/* for triangular faces */

		/* sort the verts before placing in pair list here too */
			for( j = len; j >= 0; --j ) {
				for( k = 0; k < j; ++k )
					if( verts[k] > verts[k+1] ) {
						SWAP( int, verts[k], verts[k+1] );
						SWAP( unsigned char, order[k], order[k+1] );
					}
				tmppair->v[j] = verts[j];
			}

		/* pack order into a byte */
			tmppair->order = order[0]|(order[1]<<2)|(order[2]<<4)|(order[3]<<6);
			++tmppair;
			++tmpface;
		}

	/* sort the old face pairs */
		qsort( oldpair, mesh->totface, sizeof(SrchFaces), mface_comp );

	/* eliminate new faces already in the mesh */
		tmppair = newpair;
		for( i = good_faces; i ; ) {
			if( tmppair->v[1] ) {
				if( bsearch( tmppair, oldpair, mesh->totface, 
						sizeof(SrchFaces), mface_comp ) ) {
					tmppair->v[1] = 0;	/* mark as duplicate */
					--good_faces;
				}
				--i;
			}
			tmppair++;
		}
		MEM_freeN( oldpair );
	}

	/* if any new faces are left, add to list */
	if( good_faces || return_list ) {
		int totface = mesh->totface+good_faces;	/* new face count */
		CustomData fdata;

		CustomData_copy( &mesh->fdata, &fdata, CD_MASK_MESH, CD_DEFAULT, totface );
		CustomData_copy_data( &mesh->fdata, &fdata, 0, 0, mesh->totface );

		if ( !CustomData_has_layer( &fdata, CD_MFACE ) )
			CustomData_add_layer( &fdata, CD_MFACE, CD_CALLOC, NULL, totface );

		CustomData_free( &mesh->fdata, mesh->totface );
		mesh->fdata = fdata;
		mesh_update_customdata_pointers( mesh );

	/* sort the faces back into their original input list order */
		if( !ignore_dups )
			qsort( newpair, new_face_count, sizeof(SrchFaces),
					mface_index_comp );


	/* point to the first face we're going to add */
		tmpface = &mesh->mface[mesh->totface];
		tmppair = newpair;

		if( return_list )
			good_faces = new_face_count;	/* assume all faces good to start */
			
	/* as we find a good face, add it */
		while ( good_faces ) {
			if( tmppair->v[1] ) {
				int i;
				unsigned int index[4];
				unsigned char order = tmppair->order;

		/* unpack the order of the vertices */
				for( i = 0; i < 4; ++i ) {
					index[(order & 0x03)] = i;
					order >>= 2;
				}

		/* now place vertices in the proper order */
				tmpface->v1 = tmppair->v[index[0]];
				tmpface->v2 = tmppair->v[index[1]];
				tmpface->v3 = tmppair->v[index[2]];
				tmpface->v4 = tmppair->v[index[3]];

				tmpface->flag = flag;

				if( return_list ) {
					tmp = PyInt_FromLong( mesh->totface );
					PyList_Append( return_list, tmp );
					Py_DECREF(tmp);
				}
				mesh->totface++;
				++tmpface;
				--good_faces;
			} else if( return_list ) {
				PyList_Append( return_list, Py_None );
				--good_faces;
			}
			tmppair++;
		}
	}

	/* clean up and leave */
	mesh_update( mesh );
	Py_DECREF ( args );
	MEM_freeN( newpair );

	if( return_list )
		return return_list;
	else
		Py_RETURN_NONE;
}

struct fourEdges
{
	FaceEdges *v[4];
};

static PyObject *MFaceSeq_delete( BPy_MFaceSeq * self, PyObject *args )
{
	unsigned int *face_table;
	int i, len;
	Mesh *mesh = self->mesh;
	MFace *tmpface;
	int face_count;
	int edge_also = 0;

	/* check for valid inputs */

	if( PySequence_Size( args ) != 2 ||
			!PyArg_ParseTuple( args, "iO", &edge_also, &args ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected and int and a sequence of ints or MFaces" );

	if( !PyList_Check( args ) && !PyTuple_Check( args ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected and int and a sequence of ints or MFaces" );

	/* see how many args we need to parse */
	len = PySequence_Size( args );
	if( len < 1 )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"sequence must contain at least one int or MFace" );

	face_table = MEM_callocN( len*sizeof( unsigned int ),
			"face_table" );

	/* get the indices of faces to be removed */
	for( i = len; i--; ) {
		PyObject *tmp = PySequence_GetItem( args, i );
		if( BPy_MFace_Check( tmp ) )
			face_table[i] = ((BPy_MFace *)tmp)->index;
		else if( PyInt_Check( tmp ) )
			face_table[i] = PyInt_AsLong( tmp );
		else {
			MEM_freeN( face_table );
			Py_DECREF( tmp );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of ints or MFaces" );
		}
		Py_DECREF( tmp );

		/* if index out-of-range, throw exception */
		if( face_table[i] >= (unsigned int)mesh->totface ) {
			MEM_freeN( face_table );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"array index out of range" );
		}
	}

	if( edge_also ) {
	/*
	 * long version
	 *
	 * (1) build sorted table of all edges
	 * (2) construct face->edge lookup table for all faces
	 * 	   face->e1 = mesh->medge[i]
	 * (3) (delete sorted table)
	 * (4) mark all edges as live
	 * (5) mark all edges for deleted faces as dead
	 * (6) mark all edges for remaining faces as live
	 * (7) delete all dead edges
	 * (8) (delete face lookup table)
	 *
	 */

		FaceEdges *edge_table, *tmp_et;
		MEdge *tmpedge;
		FaceEdges **face_edges;
		FaceEdges **tmp_fe;
		struct fourEdges *fface;
		int edge_count;

		edge_table = MEM_mallocN( mesh->totedge*sizeof( FaceEdges ),
			"edge_table" );

		tmpedge = mesh->medge;
		tmp_et = edge_table;

		for( i = 0; i < mesh->totedge; ++i ) {
			if( tmpedge->v1 < tmpedge->v2 ) { 
				tmp_et->v[0] = tmpedge->v1;
				tmp_et->v[1] = tmpedge->v2;
			} else {
				tmp_et->v[0] = tmpedge->v2;
				tmp_et->v[1] = tmpedge->v1;
			}
			tmp_et->index = i;
			tmp_et->sel = 1;		/* select each edge */
			++tmpedge; 
			++tmp_et;
		}

		/* sort the edge pairs */
		qsort( edge_table, mesh->totedge, sizeof(FaceEdges), faceedge_comp );

		/* build face translation table, lookup edges */
		face_edges = MEM_callocN( 4*sizeof(FaceEdges*)*mesh->totface,
			"face_edges" );	

		tmp_fe = face_edges;
		tmpface = mesh->mface;
		for( i = mesh->totface; i--; ++tmpface ) {
			FaceEdges *ptrs[4];
			unsigned int verts[4];
			int j,k;
			FaceEdges target;
			int len=tmpface->v4 ? 4 : 3;

			ptrs[3] = NULL;
			verts[0] = tmpface->v1;
			verts[1] = tmpface->v2;
			verts[2] = tmpface->v3;
			if( len == 4 )
				verts[3] = tmpface->v4;
			for( j = 0; j < len; ++j ) {
				k = (j+1) % len;
				if( verts[j] < verts[k] ) { 
					target.v[0] = verts[j];
					target.v[1] = verts[k];
				} else {
					target.v[0] = verts[k];
					target.v[1] = verts[j];
				}
				ptrs[j] = bsearch( &target, edge_table, mesh->totedge,
							sizeof(FaceEdges), faceedge_comp );
			}
			for( j = 0; j < 4; ++j, ++tmp_fe )
				*tmp_fe = ptrs[j];
		}

		/* for each face, deselect each edge */
		tmpface = mesh->mface;
		face_count = 0;
		for( i = len; i--; ) {
			if( tmpface[face_table[i]].v1 != UINT_MAX ) {
				fface = (void *)face_edges;
				fface += face_table[i];
				fface->v[0]->sel = 0;
				fface->v[1]->sel = 0;
				fface->v[2]->sel = 0;
				if( fface->v[3] )
					fface->v[3]->sel = 0;
				tmpface[face_table[i]].v1 = UINT_MAX;
				++face_count;
			}
		}
		
		/* for each remaining face, select all edges */
		tmpface = mesh->mface;
		fface = (struct fourEdges *)face_edges;
		for( i = mesh->totface; i--; ++tmpface, ++fface ) {
			if( tmpface->v1 != UINT_MAX ) {
				fface->v[0]->sel = 1;
				fface->v[1]->sel = 1;
				fface->v[2]->sel = 1;
				if( fface->v[3] )
					fface->v[3]->sel = 1;
			}
		}
		/* now mark the selected edges for deletion */

		edge_count = 0;
		for( i = 0; i < mesh->totedge; ++i ) {
			if( !edge_table[i].sel ) {
				mesh->medge[edge_table[i].index].v1 = UINT_MAX;
				++edge_count;
			}
		}

		if( edge_count )
			delete_edges( mesh, NULL, edge_count );

		MEM_freeN( face_edges );
		MEM_freeN( edge_table );
	} else {
	/* mark faces to delete */
		tmpface = mesh->mface;
		face_count = 0;
		for( i = len; i--; )
			if( tmpface[face_table[i]].v1 != UINT_MAX ) {
				tmpface[face_table[i]].v1 = UINT_MAX;
				++face_count;
			}
	}

	/* delete faces which have a deleted edge */
	delete_faces( mesh, NULL, face_count );

	/* clean up and return */
	MEM_freeN( face_table );
	mesh_update ( mesh );
	Py_RETURN_NONE;
}

/* copied from meshtools.c - should make generic? */
static void permutate(void *list, int num, int size, int *index)
{
	void *buf;
	int len;
	int i;

	len = num * size;

	buf = MEM_mallocN(len, "permutate");
	memcpy(buf, list, len);
	
	for (i = 0; i < num; i++) {
		memcpy((char *)list + (i * size), (char *)buf + (index[i] * size), size);
	}
	MEM_freeN(buf);
}

/* this wrapps list sorting then applies back to the mesh */
static PyObject *MFaceSeq_sort( BPy_MEdgeSeq * self, PyObject *args,
	  PyObject *keywds )
{
	PyObject *ret, *sort_func, *newargs;
	
	Mesh *mesh = self->mesh;
	PyObject *sorting_list;
	CustomDataLayer *layer;
	int i, *index;
	
	/* get a list for internal use */
	sorting_list = PySequence_List( (PyObject *)self );
	if( !sorting_list )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyList_New() failed" );
	
	/* create index list */
	index = (int *) MEM_mallocN(sizeof(int) * mesh->totface, "sort faces");
	if (!index)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"faces.sort(...) failed to allocate memory" );
	
	newargs = EXPP_PyTuple_New_Prepend(args, sorting_list);
	sort_func = PyObject_GetAttrString( ((PyObject *)&PyList_Type), "sort");
	
	ret = PyObject_Call(sort_func, newargs, keywds);
	
	Py_DECREF(newargs);
	Py_DECREF(sort_func);
	
	if (ret) {
		/* copy the faces indicies to index */
		for (i = 0; i < mesh->totface; i++)
			index[i] = ((BPy_MFace *)PyList_GET_ITEM(sorting_list, i))->index;
		
		for(i = 0; i < mesh->fdata.totlayer; i++) {
			layer = &mesh->fdata.layers[i];
			permutate(layer->data, mesh->totface, CustomData_sizeof(layer->type), index);
		}
	}
	Py_DECREF(sorting_list);
	MEM_freeN(index);
	return ret;
}

static PyObject *MFaceSeq_selected( BPy_MFaceSeq * self )
{
	int i, count;
	Mesh *mesh = self->mesh;
	MFace *tmpface;
	PyObject *list;

	/* first count selected faces (quicker than appending to PyList?) */
	count = 0;
	tmpface = mesh->mface;
	for( i = 0; i < mesh->totface; ++i, ++tmpface )
		if( tmpface->flag & ME_FACE_SEL )
			++count;

	list = PyList_New( count );
	if( !list )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyList_New() failed" );

	/* next, insert selected faces into list */
	count = 0;
	tmpface = mesh->mface;
	for( i = 0; i < mesh->totface; ++i, ++tmpface ) {
		if( tmpface->flag & ME_FACE_SEL ) {
			PyObject *tmp = PyInt_FromLong( i );
			if( !tmp ) {
				Py_DECREF( list );
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"PyInt_FromLong() failed" );
			}
			PyList_SET_ITEM( list, count, tmp );
			++count;
		}
	}
	return list;
}

static PyObject *MFaceSeq_add_layertype(BPy_MFaceSeq *self, PyObject *args)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_addPropLayer_internal(me, &(me->fdata), me->totface, args);
}
static PyObject *MFaceSeq_del_layertype(BPy_MFaceSeq *self, PyObject *value)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_removePropLayer_internal(me, &(me->fdata), me->totface, value);
}
static PyObject *MFaceSeq_rename_layertype(BPy_MFaceSeq *self, PyObject *args)
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_renamePropLayer_internal(me,&(me->fdata),args);
}
static PyObject *MFaceSeq_PropertyList(BPy_MFaceSeq *self) 
{
	Mesh *me = (Mesh*)self->mesh;
	return Mesh_propList_internal(&(me->fdata));
}

static struct PyMethodDef BPy_MFaceSeq_methods[] = {
	{"extend", (PyCFunction)MFaceSeq_extend, METH_VARARGS|METH_KEYWORDS,
		"add faces to mesh"},
	{"delete", (PyCFunction)MFaceSeq_delete, METH_VARARGS,
		"delete faces from mesh"},
	{"sort", (PyCFunction)MFaceSeq_sort, METH_VARARGS|METH_KEYWORDS,
		"sort the faces using list sorts syntax"},
	{"selected", (PyCFunction)MFaceSeq_selected, METH_NOARGS,
		"returns a list containing indices of selected faces"},
	{"addPropertyLayer",(PyCFunction)MFaceSeq_add_layertype, METH_VARARGS,
		"add a new property layer"},
	{"removePropertyLayer",(PyCFunction)MFaceSeq_del_layertype, METH_O,
		"removes a property layer"},
	{"renamePropertyLayer",(PyCFunction)MFaceSeq_rename_layertype, METH_VARARGS,
		"renames an existing property layer"},		
	{NULL, NULL, 0, NULL}
};
static PyGetSetDef BPy_MFaceSeq_getseters[] = {
	{"properties",
	(getter)MFaceSeq_PropertyList, (setter)NULL,
	"vertex property layers, read only",
	NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


/************************************************************************
 *
 * Python MFaceSeq_Type standard operations
 *
 ************************************************************************/

/*****************************************************************************/
/* Python MFaceSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MFaceSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MFaceSeq",           /* char *tp_name; */
	sizeof( BPy_MFaceSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MFaceSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc )MFaceSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc )MFaceSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MFaceSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MFaceSeq_getseters,     /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/************************************************************************
 *
 * Python BPy_Mesh methods
 *
 ************************************************************************/

static PyObject *Mesh_calcNormals( BPy_Mesh * self )
{
	Mesh *mesh = self->mesh;

	mesh_calc_normals( mesh->mvert, mesh->totvert, mesh->mface,
			mesh->totface, NULL );
	Py_RETURN_NONE;
}

static PyObject *Mesh_vertexShade( BPy_Mesh * self )
{
	Base *base = FIRSTBASE;

	if( G.obedit )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"can't shade vertices while in edit mode" );

	while( base ) {
		if( base->object->type == OB_MESH && 
				base->object->data == self->mesh ) {
			base->flag |= SELECT;
			set_active_base( base );
			make_vertexcol(1);
			countall();
			Py_RETURN_NONE;
		}
		base = base->next;
	}
	return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"object not found in baselist!" );
}

/*
 * force display list update
 */

static PyObject *Mesh_Update( BPy_Mesh * self, PyObject *args, PyObject *kwd )
{

	char *blockname= NULL;
	static char *kwlist[] = {"key", NULL};
	
	if( !PyArg_ParseTupleAndKeywords(args, kwd, "|s", kwlist, &blockname) )
	return EXPP_ReturnPyObjError( PyExc_TypeError,
			"Expected nothing or the name of a shapeKey");
	
	if (blockname) {
		Mesh *me = self->mesh;
		MVert *mv = me->mvert;  
		Key *key= me->key;
		KeyBlock *kb;
		float (*co)[3];
		int i;
		
		if (!key)
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"Cannot update the key for this mesh, it has no shape keys");
		
		for (kb = key->block.first; kb; kb=kb->next)
			if (strcmp(blockname, kb->name)==0)
				break;
		
		if (!kb)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"This requested key to update does not exist");
		
		for(i=0, co= kb->data; i<me->totvert; i++, mv++, co++)
			VECCOPY(*co, mv->co);
	} else {
		/* Normal operation */
		mesh_update( self->mesh );
	}
	Py_RETURN_NONE;
}

/*
 * search for a single edge in mesh's edge list
 */

static PyObject *Mesh_findEdge( BPy_Mesh * self, PyObject *args )
{
	int i;
	unsigned int v1, v2;
	PyObject *tmp;
	MEdge *edge = self->mesh->medge;

	if( EXPP_check_sequence_consistency( args, &MVert_Type ) == 1 &&
			PySequence_Size( args ) == 2 ) {
		tmp = PyTuple_GET_ITEM( args, 0 );
		v1 = ((BPy_MVert *)tmp)->index;
		tmp = PyTuple_GET_ITEM( args, 1 );
		v2 = ((BPy_MVert *)tmp)->index;
	} else if( PyArg_ParseTuple( args, "ii", &v1, &v2 ) ) {
		if( (int)v1 >= self->mesh->totvert || (int)v2 >= self->mesh->totvert )
			return EXPP_ReturnPyObjError( PyExc_IndexError,
					"index out of range" );
	} else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"expected tuple of two ints or MVerts" );

	for( i = 0; i < self->mesh->totedge; ++i ) {
		if( ( edge->v1 == v1 && edge->v2 == v2 )
				|| ( edge->v1 == v2 && edge->v2 == v1 ) ) {
			tmp = PyInt_FromLong( i );
			if( tmp )
				return tmp;
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"PyInt_FromLong() failed" );
		}
		++edge;
	}
	Py_RETURN_NONE;
}

/*
 * search for a group of edges in mesh's edge list
 */

static PyObject *Mesh_findEdges( PyObject * self, PyObject *args )
{
	int len;
	int i;
	SrchEdges *oldpair, *tmppair, target, *result;
	PyObject *list, *tmp;
	BPy_MVert *v1, *v2;
	unsigned int index1, index2;
	MEdge *tmpedge;
	Mesh *mesh = ((BPy_Mesh *)self)->mesh;

	/* if no edges, nothing to do */

	if( !mesh->totedge )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"mesh has no edges" );

	/* make sure we get a sequence of tuples of something */

	tmp = PyTuple_GET_ITEM( args, 0 );
	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		if( !PySequence_Check ( tmp ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of tuple int or MVert pairs" );
		args = tmp;
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 2:		/* take any two args and put into a tuple */
		if( PyTuple_Check( tmp ) )
			Py_INCREF( args );	/* if first arg is a tuple, assume both are */
		else {
			args = Py_BuildValue( "((OO))", tmp, PyTuple_GET_ITEM( args, 1 ) );
			if( !args )
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"Py_BuildValue() failed" );
		}
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple pairs" );
	}

	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	/* if a single edge, handle the simpler way */
	if( len == 1 ) {
		PyObject *result;
		tmp = PySequence_GetItem( args, 0 );
		result = Mesh_findEdge( (BPy_Mesh *)self, tmp );
		Py_DECREF( tmp );
		Py_DECREF( args );
		return result;
	}

	/* build a list of all edges so we can search */
	oldpair = (SrchEdges *)MEM_callocN( sizeof(SrchEdges)*mesh->totedge,
			"MEdgePairs" );

	tmppair = oldpair;
	tmpedge = mesh->medge;
	for( i = 0; i < mesh->totedge; ++i ) {
		if( tmpedge->v1 < tmpedge->v2 ) {
			tmppair->v[0] = tmpedge->v1;
			tmppair->v[1] = tmpedge->v2;
		} else {
			tmppair->v[0] = tmpedge->v2;
			tmppair->v[1] = tmpedge->v1;
		}
		tmppair->index = i;
		++tmpedge;
		++tmppair;
	}

	/* sort the old edge pairs */
	qsort( oldpair, mesh->totedge, sizeof(SrchEdges), medge_comp );

	list = PyList_New( len );
	if( !len )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyList_New() failed" );

	/* scan the input list, find vert pairs, then search the edge list */

	for( i = 0; i < len; ++i ) {
		tmp = PySequence_GetItem( args, i );
		if( !PyTuple_Check( tmp ) || PyTuple_Size( tmp ) != 2 ) {
			MEM_freeN( oldpair );
			Py_DECREF( tmp );
			Py_DECREF( args );
			Py_DECREF( list );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected tuple pair" );
		}

		/* get objects, check that they are both MVerts of this mesh */
		v1 = (BPy_MVert *)PyTuple_GET_ITEM( tmp, 0 );
		v2 = (BPy_MVert *)PyTuple_GET_ITEM( tmp, 1 );
		Py_DECREF ( tmp );
		if( BPy_MVert_Check( v1 ) && BPy_MVert_Check( v2 ) ) {
			if( v1->data != (void *)mesh || v2->data != (void *)mesh ) {
				MEM_freeN( oldpair );
				Py_DECREF( args );
				Py_DECREF( list );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"one or both MVerts do not belong to this mesh" );
			}
			index1 = v1->index;
			index2 = v2->index;
		} else if( PyInt_Check( v1 ) && PyInt_Check( v2 ) ) {
			index1 = PyInt_AsLong( (PyObject *)v1 );
			index2 = PyInt_AsLong( (PyObject *)v2 );
			if( (int)index1 >= mesh->totvert
					|| (int)index2 >= mesh->totvert ) {
				MEM_freeN( oldpair );
				Py_DECREF( args );
				Py_DECREF( list );
				return EXPP_ReturnPyObjError( PyExc_IndexError,
						"index out of range" );
			}
		} else {
			MEM_freeN( oldpair );
			Py_DECREF( args );
			Py_DECREF( list );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected tuple to contain MVerts" );
		}

		/* sort verts into order */
		if( index1 < index2 ) {
			target.v[0] = index1;
			target.v[1] = index2;
		} else {
			target.v[0] = index2;
			target.v[1] = index1;
		}

		/* search edge list for a match; result is index or None */
		result = bsearch( &target, oldpair, mesh->totedge,
				sizeof(SrchEdges), medge_comp );
		if( result )
			tmp = PyInt_FromLong( result->index );
		else
			tmp = EXPP_incr_ret( Py_None );
		if( !tmp ) {
			MEM_freeN( oldpair );
			Py_DECREF( args );
			Py_DECREF( list );
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyInt_FromLong() failed" );
		}
		PyList_SET_ITEM( list, i, tmp );
	}

	MEM_freeN( oldpair );
	Py_DECREF ( args );
	return list;
}

/*
 * replace mesh data with mesh data from another object
 */


static PyObject *Mesh_getFromObject( BPy_Mesh * self, PyObject * args )
{
	Object *ob = NULL;
	PyObject *object_arg;
	ID tmpid;
	Mesh *tmpmesh;
	Curve *tmpcu = NULL;
	DerivedMesh *dm;
	Object *tmpobj = NULL;
	int cage = 0, render = 0, i;

	if( !PyArg_ParseTuple( args, "O|ii", &object_arg, &cage, &render ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected object or string and optional integer arguments" );
	
	if ( PyString_Check( object_arg ) ) {
		char *name;
		name = PyString_AsString ( object_arg );
		ob = ( Object * ) GetIdFromList( &( G.main->object ), name );
		if( !ob )
			return EXPP_ReturnPyObjError( PyExc_AttributeError, name );
	} else if ( BPy_Object_Check(object_arg) ) {
		ob = (( BPy_Object * ) object_arg)->object;
	} else {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected object or string and optional integer arguments" );
	}
	
	if( cage != 0 && cage != 1 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"cage value must be 0 or 1" );

	

	/* perform the mesh extraction based on type */
 	switch (ob->type) {
 	case OB_FONT:
 	case OB_CURVE:
 	case OB_SURF:
		/* copies object and modifiers (but not the data) */
		tmpobj= copy_object( ob );
		tmpcu = (Curve *)tmpobj->data;
		tmpcu->id.us--;

		/* if getting the original caged mesh, delete object modifiers */
		if( cage )
			object_free_modifiers(tmpobj);

		/* copies the data */
		tmpobj->data = copy_curve( (Curve *) ob->data );

#if 0
		/* copy_curve() sets disp.first null, so currently not need */
		{
			Curve *cu;
			cu = (Curve *)tmpobj->data;
			if( cu->disp.first )
				MEM_freeN( cu->disp.first );
			cu->disp.first = NULL;
		}
	
#endif

		/* get updated display list, and convert to a mesh */
		makeDispListCurveTypes( tmpobj, 0 );
		nurbs_to_mesh( tmpobj );
		
		/* nurbs_to_mesh changes the type tp a mesh, check it worked */
		if (tmpobj->type != OB_MESH) {
			free_libblock_us( &G.main->object, tmpobj );
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cant convert curve to mesh. Does the curve have any segments?" );
		}
		tmpmesh = tmpobj->data;
		free_libblock_us( &G.main->object, tmpobj );
		break;
 	case OB_MBALL:
		/* metaballs don't have modifiers, so just convert to mesh */
		ob = find_basis_mball( ob );
		tmpmesh = add_mesh("Mesh");
		mball_to_mesh( &ob->disp, tmpmesh );

 		break;
 	case OB_MESH:
		/* copies object and modifiers (but not the data) */
		if (cage) {
			/* copies the data */
			tmpmesh = copy_mesh( ob->data );
		/* if not getting the original caged mesh, get final derived mesh */
		} else {
			/* Make a dummy mesh, saves copying */
			
			/* Write the display mesh into the dummy mesh */
			if (render)
				dm = mesh_create_derived_render( ob, CD_MASK_MESH );
			else
				dm = mesh_create_derived_view( ob, CD_MASK_MESH );
			
			tmpmesh = add_mesh( "Mesh" );
			DM_to_mesh( dm, tmpmesh );
			dm->release( dm );
		}
		
		break;
 	default:
 		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"Object does not have geometry data" );
  	}

	/* free mesh data in the original */
	free_mesh( self->mesh );
	/* save a copy of our ID, dup the temporary mesh, restore the ID */
	tmpid = self->mesh->id;
	memcpy( self->mesh, tmpmesh, sizeof( Mesh ) );
	self->mesh->id = tmpid;
	
	/* if mesh has keys, make sure they point back to this mesh */
	if( self->mesh->key )
		self->mesh->key->from = (ID *)self->mesh;
	
	
	
	/* Copy materials to new object */
	switch (ob->type) {
	case OB_SURF:
		self->mesh->totcol = tmpcu->totcol;		
		
		/* free old material list (if it exists) and adjust user counts */
		if( tmpcu->mat ) {
			for( i = tmpcu->totcol; i-- > 0; ) {
				
				/* are we an object material or data based? */
				if (ob->colbits & 1<<i) {
					self->mesh->mat[i] = ob->mat[i];
					ob->mat[i]->id.us++;
					tmpmesh->mat[i]->id.us--;
				} else {
					self->mesh->mat[i] = tmpcu->mat[i];
					if (self->mesh->mat[i]) {
						tmpmesh->mat[i]->id.us++;
					}
				}
			}
		}
		break;

#if 0
	/* Crashes when assigning the new material, not sure why */
	case OB_MBALL:
		tmpmb = (MetaBall *)ob->data;
		self->mesh->totcol = tmpmb->totcol;
		
		/* free old material list (if it exists) and adjust user counts */
		if( tmpmb->mat ) {
			for( i = tmpmb->totcol; i-- > 0; ) {
				self->mesh->mat[i] = tmpmb->mat[i]; /* CRASH HERE ??? */
				if (self->mesh->mat[i]) {
					tmpmb->mat[i]->id.us++;
				}
			}
		}
		break;
#endif

	case OB_MESH:
		if (!cage) {
			Mesh *origmesh= ob->data;
			self->mesh->flag= origmesh->flag;
			self->mesh->mat = MEM_dupallocN(origmesh->mat);
			self->mesh->totcol = origmesh->totcol;
			self->mesh->smoothresh= origmesh->smoothresh;
			if( origmesh->mat ) {
				for( i = origmesh->totcol; i-- > 0; ) {
					/* are we an object material or data based? */
					if (ob->colbits & 1<<i) {
						self->mesh->mat[i] = ob->mat[i];
						
						if (ob->mat[i])
							ob->mat[i]->id.us++;
						if (origmesh->mat[i])
							origmesh->mat[i]->id.us--;
					} else {
						self->mesh->mat[i] = origmesh->mat[i];
						
						if (origmesh->mat[i])
							origmesh->mat[i]->id.us++;
					}
				}
			}
		}
		break;
	} /* end copy materials */
	
	
	
	/* remove the temporary mesh */
	BLI_remlink( &G.main->mesh, tmpmesh );
	MEM_freeN( tmpmesh );

	/* make sure materials get updated in objects */
	test_object_materials( ( ID * ) self->mesh );

	mesh_update( self->mesh );
	Py_RETURN_NONE;
}

/*
 * apply a transform to the mesh's vertices
 *
 * WARNING: unlike NMesh, this method ALWAYS changes the original mesh
 */

static PyObject *Mesh_transform( BPy_Mesh *self, PyObject *args, PyObject *kwd )
{
	Mesh *mesh = self->mesh;
	MVert *mvert;
	/*PyObject *pymat = NULL;*/
	MatrixObject *bpymat=NULL;
	int i, recalc_normals = 0, selected_only = 0;

	static char *kwlist[] = {"matrix", "recalc_normals", "selected_only", NULL};
	
	if( !PyArg_ParseTupleAndKeywords(args, kwd, "|O!ii", kwlist,
				 &matrix_Type, &bpymat, &recalc_normals, &selected_only) ) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"matrix must be a 4x4 transformation matrix\n"
				"for example as returned by object.matrixWorld\n"
				"and optionaly keyword bools, recalc_normals and selected_only\n");
	}
	
	if (!bpymat)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"the first argument must be a matrix or\n"
				"matrix passed as a keyword argument\n");
	
	
	/*bpymat = ( MatrixObject * ) pymat;*/

	if( bpymat->colSize != 4 || bpymat->rowSize != 4 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"matrix must be a 4x4 transformation matrix\n"
				"for example as returned by object.getMatrix()" );
	
	/* loop through all the verts and transform by the supplied matrix */
	mvert = mesh->mvert;
	if (selected_only) {
		for( i = 0; i < mesh->totvert; i++, mvert++ ) {
			if (mvert->flag & SELECT) { 
				Mat4MulVecfl( (float(*)[4])*bpymat->matrix, mvert->co );
			}
		}
	} else {
		for( i = 0; i < mesh->totvert; i++, mvert++ ) {
			Mat4MulVecfl( (float(*)[4])*bpymat->matrix, mvert->co );
		}
	}
	
	if( recalc_normals ) {
		/* loop through all the verts and transform normals by the inverse
		 * of the transpose of the supplied matrix */
		float invmat[4][4], vec[3], nx, ny, nz;
		
		/*
		 * we only need to invert a 3x3 submatrix, because the 4th component of
		 * affine vectors is 0, but Mat4Invert reports non invertible matrices
		 */

		if (!Mat4Invert((float(*)[4])*invmat, (float(*)[4])*bpymat->matrix))
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"given matrix is not invertible");

		/*
		 * since normal is stored as shorts, convert to float 
		 */

		mvert = mesh->mvert;
		for( i = 0; i < mesh->totvert; i++, mvert++ ) {
			nx= vec[0] = (float)(mvert->no[0] / 32767.0);
			ny= vec[1] = (float)(mvert->no[1] / 32767.0);
			nz= vec[2] = (float)(mvert->no[2] / 32767.0);
			vec[0] = nx*invmat[0][0] + ny*invmat[0][1] + nz*invmat[0][2];
			vec[1] = nx*invmat[1][0] + ny*invmat[1][1] + nz*invmat[1][2]; 
			vec[2] = nx*invmat[2][0] + ny*invmat[2][1] + nz*invmat[2][2];
			Normalize( vec );
			mvert->no[0] = (short)(vec[0] * 32767.0);
			mvert->no[1] = (short)(vec[1] * 32767.0);
			mvert->no[2] = (short)(vec[2] * 32767.0);
		}
	}

	Py_RETURN_NONE;
}

static PyObject *Mesh_addVertGroup( PyObject * self, PyObject * value )
{
	char *groupStr = PyString_AsString(value);
	struct Object *object;

	if( !groupStr )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	object = ( ( BPy_Mesh * ) self )->object;
	
	if( object == NULL )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh not linked to an object" );

	/* add_defgroup_name clamps the name to 32, make sure that dosnt change  */
	add_defgroup_name( object, groupStr );

	EXPP_allqueue( REDRAWBUTSALL, 1 );

	Py_RETURN_NONE;
}

static PyObject *Mesh_removeVertGroup( PyObject * self, PyObject * value )
{
	char *groupStr = PyString_AsString(value);
	struct Object *object;
	int nIndex;
	bDeformGroup *pGroup;

	if( G.obedit )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"can't use removeVertGroup() while in edit mode" );
	
	if( !groupStr )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	if( ( ( BPy_Mesh * ) self )->object == NULL )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh must be linked to an object first..." );

	object = ( ( BPy_Mesh * ) self )->object;

	pGroup = get_named_vertexgroup( object, groupStr );
	if( pGroup == NULL )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "group does not exist!" );

	nIndex = get_defgroup_num( object, pGroup );
	if( nIndex == -1 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "no deform groups assigned to mesh" );
	nIndex++;
	object->actdef = (unsigned short)nIndex;

	del_defgroup_in_object_mode( object );
	
	EXPP_allqueue( REDRAWBUTSALL, 1 );

	Py_RETURN_NONE;
}

extern void add_vert_defnr( Object * ob, int def_nr, int vertnum, float weight,
		             int assignmode );
extern void remove_vert_def_nr (Object *ob, int def_nr, int vertnum);

static PyObject *Mesh_assignVertsToGroup( BPy_Mesh * self, PyObject * args )
{
	char *groupStr;
	int nIndex;
	bDeformGroup *pGroup;
	PyObject *listObject;
	int tempInt;
	int x;
	int assignmode = WEIGHT_REPLACE;
	float weight = 1.0;
	Object *object = self->object;
	Mesh *mesh = self->mesh;

	if( !object )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh must be linked to an object first" );

	if( ((Mesh *)object->data) != mesh )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "object no longer linked to this mesh" );

	if( !PyArg_ParseTuple ( args, "sO!fi", &groupStr, &PyList_Type,
			&listObject, &weight, &assignmode) ) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string, list, float, int arguments" );
	}

	pGroup = get_named_vertexgroup( object, groupStr );
	if( pGroup == NULL )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "group does not exist!" );

	nIndex = get_defgroup_num( object, pGroup );
	if( nIndex == -1 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "no deform groups assigned to mesh" );


	if( assignmode != WEIGHT_REPLACE && assignmode != WEIGHT_ADD &&
			assignmode != WEIGHT_SUBTRACT )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "bad assignment mode" );

	/* makes a set of dVerts corresponding to the mVerts */
	if( !mesh->dvert ) 
		create_dverts( &mesh->id );

	/* loop list adding verts to group */
	for( x = 0; x < PyList_Size( listObject ); x++ ) {
		if( !PyArg_Parse ( PyList_GetItem( listObject, x ), "i", &tempInt ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
						      "python list integer not parseable" );

		if( tempInt < 0 || tempInt >= mesh->totvert )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
						      "bad vertex index in list" );

		add_vert_defnr( object, nIndex, tempInt, weight, assignmode );
	}

	Py_RETURN_NONE;
}

static PyObject *Mesh_removeVertsFromGroup( BPy_Mesh * self, PyObject * args )
{
	/* not passing a list will remove all verts from group */

	char *groupStr;
	int nIndex;
	Object *object;
	Mesh *mesh;
	bDeformGroup *pGroup;
	PyObject *listObject = NULL;
	int tempInt;
	int i;

	object = self->object;
	mesh = self->mesh;
	if( !object )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh must be linked to an object first" );

	if( ((Mesh *)object->data) != mesh )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "object no longer linked to this mesh" );

	if( !PyArg_ParseTuple
	    ( args, "s|O!", &groupStr, &PyList_Type, &listObject ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string and optional list argument" );

	if( !mesh->dvert )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "this mesh contains no deform vertices" );

	pGroup = get_named_vertexgroup( object, groupStr );
	if( pGroup == NULL )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "group does not exist!" );

	nIndex = get_defgroup_num( object, pGroup );
	if( nIndex == -1 )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "no deform groups assigned to mesh" );

	/* get out of edit mode */

	if( G.obedit ) {
		load_editMesh();
		free_editMesh(G.editMesh);
		G.obedit = NULL;
	}

	if( !listObject ) /* no list given */
		for( i = 0; i < mesh->totvert; i++ )
			remove_vert_def_nr( object, nIndex, i );
	else		 /* loop list removing verts to group */
		for( i = 0; i < PyList_Size( listObject ); i++ ) {
			if( !PyArg_Parse( PyList_GetItem( listObject, i ), "i", &tempInt ) )
				return EXPP_ReturnPyObjError( PyExc_TypeError,
							      "python list integer not parseable" );

			if( tempInt < 0 || tempInt >= mesh->totvert )
				return EXPP_ReturnPyObjError( PyExc_ValueError,
							      "bad vertex index in list" );

			remove_vert_def_nr( object, nIndex, tempInt );
		}

	Py_RETURN_NONE;
}

static PyObject *Mesh_getVertsFromGroup( BPy_Mesh* self, PyObject * args )
{
	/*
	 * not passing a list will return all verts from group
	 * passing indecies not part of the group will not return data in pyList
	 * can be used as a index/group check for a vertex
	 */

	char *groupStr;
	int nIndex;
	bDeformGroup *pGroup;
	MDeformVert *dvert;
	int i, k, count;
	PyObject *vertexList;
	Object *object;
	Mesh *mesh;

	int num = 0;
	int weightRet = 0;
	PyObject *listObject = NULL;

	object = self->object;
	mesh = self->mesh;
	if( !object )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh must be linked to an object first" );

	if( ((Mesh *)object->data) != mesh )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "object no longer linked to this mesh" );

	if( !PyArg_ParseTuple( args, "s|iO!", &groupStr, &weightRet,
			       &PyList_Type, &listObject ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string and optional int and list arguments" );

	if( weightRet < 0 || weightRet > 1 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "return weights flag must be 0 or 1" );

	if( !mesh->dvert )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "this mesh contains no deform vertices" );

	pGroup = get_named_vertexgroup( object, groupStr );
	if( !pGroup )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "group does not exist!" );

	nIndex = get_defgroup_num( object, pGroup );
	if( nIndex == -1 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "no deform groups assigned to mesh" );

	count = 0;

	if( !listObject ) {	/* do entire group */
		vertexList = PyList_New( mesh->totvert );
		if( !vertexList )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
							  "getVertsFromGroup: can't create pylist!" );
		
		dvert = mesh->dvert;
		for( num = 0; num < mesh->totvert; num++, ++dvert ) {
			for( i = 0; i < dvert->totweight; i++ ) {
				if( dvert->dw[i].def_nr == nIndex ) {
					PyObject *attr;
					if( weightRet )
						attr = Py_BuildValue( "(i,f)", num,
								dvert->dw[i].weight );
					else
						attr = PyInt_FromLong ( num );
					PyList_SetItem( vertexList, count, attr );
					count++;
				}
			}
		}
		
		if (count < mesh->totvert)
			PyList_SetSlice(vertexList, count, mesh->totvert, NULL);
		
	} else {			/* do individual vertices */
		int listObjectLen = PyList_Size( listObject );
		
		vertexList = PyList_New( listObjectLen );
		for( i = 0; i < listObjectLen; i++ ) {
			PyObject *attr = NULL;

			num = PyInt_AsLong( PyList_GetItem( listObject, i ) );
			if (num == -1) {/* -1 is an error AND an invalid range, we dont care which */
				Py_DECREF(vertexList);
				return EXPP_ReturnPyObjError( PyExc_TypeError,
							      "python list integer not parseable" );
			}

			if( num < 0 || num >= mesh->totvert ) {
				Py_DECREF(vertexList);
				return EXPP_ReturnPyObjError( PyExc_ValueError,
							      "bad vertex index in list" );
			}
			dvert = mesh->dvert + num;
			for( k = 0; k < dvert->totweight; k++ ) {
				if( dvert->dw[k].def_nr == nIndex ) {
					if( weightRet )
						attr = Py_BuildValue( "(i,f)", num,
								dvert->dw[k].weight );
					else
						attr = PyInt_FromLong ( num );
					PyList_SetItem( vertexList, count, attr );
					count++;
				}
			}
		}
		if (count < listObjectLen)
			PyList_SetSlice(vertexList, count, listObjectLen, NULL);
	}
	
	return vertexList;
}

static PyObject *Mesh_renameVertGroup( BPy_Mesh * self, PyObject * args )
{
	char *oldGr = NULL;
	char *newGr = NULL;
	bDeformGroup *defGroup;
	Object *object;
	Mesh *mesh;

	object = self->object;
	mesh = self->mesh;
	if( !object )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh must be linked to an object first" );

	if( ((Mesh *)object->data) != mesh )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "object no longer linked to this mesh" );

	if( !PyArg_ParseTuple( args, "ss", &oldGr, &newGr ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected two string arguments" );

	defGroup = get_named_vertexgroup( object, oldGr );
	if( !defGroup )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't find the vertex group" );

	PyOS_snprintf( defGroup->name, 32, newGr );
	unique_vertexgroup_name( defGroup, object );

	Py_RETURN_NONE;
}




static PyObject *Mesh_getVertGroupNames( BPy_Mesh * self )
{
	bDeformGroup *defGroup;
	PyObject *list;
	Object *obj = self->object;
	Mesh *mesh = self->mesh;
	int count;

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh must be linked to an object first" );

	if( ((Mesh *)obj->data) != mesh )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "object no longer linked to this mesh" );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "This mesh must be linked to an object" );

	count = 0;
	for( defGroup = obj->defbase.first; defGroup; defGroup = defGroup->next )
		++count;

	list = PyList_New( count );
	count = 0;
	for( defGroup = obj->defbase.first; defGroup; defGroup = defGroup->next )
		PyList_SET_ITEM( list, count++,
				PyString_FromString( defGroup->name ) );

	return list;
}

static PyObject *Mesh_getVertexInfluences( BPy_Mesh * self, PyObject * args )
{
	int index;
	PyObject *influence_list = NULL;
	Object *object = self->object;
	Mesh *me = self->mesh;

	/* Get a reference to the mesh object wrapped in here. */
	if( !object )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This mesh must be linked to an object" ); 

	/* Parse the parameters: only on integer (vertex index) */
	if( !PyArg_ParseTuple( args, "i", &index ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int argument (index of the vertex)" );

	/* check for valid index */
	if( index < 0 || index >= me->totvert )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"vertex index out of range" );

	influence_list = PyList_New( 0 );

	/* Proceed only if we have vertex deformation information */
	if( me->dvert ) {
		int i;
		MDeformWeight *sweight = NULL;

		/* Number of bones influencing the vertex */
		int totinfluences = me->dvert[index].totweight;

		/* Get the reference of the first weight structure */
		sweight = me->dvert[index].dw;

		/* Build the list only with weights and names of the influent bones */
		for( i = 0; i < totinfluences; i++, sweight++ ) {
			bDeformGroup *defgroup = BLI_findlink( &object->defbase,
					sweight->def_nr );
			if( defgroup )
				PyList_Append( influence_list, Py_BuildValue( "[sf]",
						defgroup->name, sweight->weight ) ); 
		}
	}

	return influence_list;
}

static PyObject *Mesh_removeAllKeys( BPy_Mesh * self )
{
	Mesh *mesh = self->mesh;
	
	if( !mesh || !mesh->key )
		Py_RETURN_FALSE;

	mesh->key->id.us--;
	mesh->key = NULL;
	
	Py_RETURN_TRUE;
}


static PyObject *Mesh_insertKey( BPy_Mesh * self, PyObject * args )
{
	Mesh *mesh = self->mesh;
	int fra = -1, oldfra = -1;
	char *type = NULL;
	short typenum;
	
	if (mesh->mr)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Shape Keys cannot be added to meshes with multires" );
	
	if( !PyArg_ParseTuple( args, "|is", &fra, &type ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or an int and optionally a string as arguments" );
	
	if( !type || !strcmp( type, "relative" ) )
		typenum = 1;
	else if( !strcmp( type, "absolute" ) )
		typenum = 2;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "if given, type should be 'relative' or 'absolute'" );
	
	if( fra > 0 ) {
		fra = EXPP_ClampInt( fra, 1, MAXFRAME );
		oldfra = G.scene->r.cfra;
		G.scene->r.cfra = fra;
	}

	insert_meshkey( mesh, typenum );
	allspace(REMAKEIPO, 0);
	
	if( fra > 0 )
		G.scene->r.cfra = oldfra;
	
	Py_RETURN_NONE;
}




/* Custom Data Layers */

static PyObject * Mesh_addCustomLayer_internal(Mesh *me, PyObject * args, int type)
{
	char *name = NULL;
	CustomData *data = &me->fdata;
	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string or nothing" );
	
	if (strlen(name)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum name length is 31" );
	
	if (name)
		CustomData_add_layer_named(data, type, CD_DEFAULT,
											NULL, me->totface, name);
	else
		CustomData_add_layer(data, type, CD_DEFAULT,
											NULL, me->totface);
	mesh_update_customdata_pointers(me);
	Py_RETURN_NONE;
}

static PyObject *Mesh_addUVLayer( BPy_Mesh * self, PyObject * args )
{
	return Mesh_addCustomLayer_internal(self->mesh, args, CD_MTFACE);
}

static PyObject *Mesh_addColorLayer( BPy_Mesh * self, PyObject * args )
{
	return Mesh_addCustomLayer_internal(self->mesh, args, CD_MCOL);
}

static PyObject *Mesh_removeLayer_internal( BPy_Mesh * self, PyObject * value, int type )
{
	Mesh *me = self->mesh;
	CustomData *data = &me->fdata;
	char *name = PyString_AsString(value);
	int i;
	
	if( !name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );
	
	if (strlen(name)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum name length is 31" );
	
	i = CustomData_get_named_layer_index(data, type, name);
	
	if (i==-1)
		return EXPP_ReturnPyObjError(PyExc_ValueError,
			"No matching layers to remove" );	
				
	CustomData_free_layer(data, type, me->totface, i);
	mesh_update_customdata_pointers(me);
	
	/*	No more Color or UV layers left ?
		switch modes if this is the active object */	
	if (!CustomData_has_layer(data, type)) {
		if (me == get_mesh(OBACT)) {
			if(type == CD_MCOL && (G.f & G_VERTEXPAINT))
				G.f &= ~G_VERTEXPAINT; /* get out of vertexpaint mode */
			if(type == CD_MTFACE && (G.f & G_FACESELECT))
				G.f |= ~G_FACESELECT; /* get out of faceselect mode */
		}
	}
	
	Py_RETURN_NONE;
}


static PyObject *Mesh_removeUVLayer( BPy_Mesh * self, PyObject * value )
{
	return Mesh_removeLayer_internal(self, value, CD_MTFACE);
}

static PyObject *Mesh_removeColorLayer( BPy_Mesh * self, PyObject * value )
{
	return Mesh_removeLayer_internal(self, value, CD_MCOL);
}


static PyObject *Mesh_renameLayer_internal( BPy_Mesh * self, PyObject * args, int type )
{
	CustomData *data;
	CustomDataLayer *layer;
	Mesh *mesh = self->mesh;
	int i;
	char *name_from, *name_to;
	
	data = &mesh->fdata;
	
	if( !PyArg_ParseTuple( args, "ss", &name_from, &name_to ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected 2 strings" );
	
	if (strlen(name_from)>31 || strlen(name_to)>31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"error, maximum name length is 31" );
	
	i = CustomData_get_named_layer_index(data, type, name_from);
	
	if (i==-1)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"layer name was not found" );	
	
	layer = &data->layers[i];
	strcpy(layer->name, name_to); /* we alredy know the string sizes are under 32 */
	CustomData_set_layer_unique_name(data, i);
	Py_RETURN_NONE;
}

static PyObject *Mesh_renameUVLayer( BPy_Mesh * self, PyObject * args )
{
	return Mesh_renameLayer_internal( self, args, CD_MTFACE );
}

static PyObject *Mesh_renameColorLayer( BPy_Mesh * self, PyObject * args )
{
	return Mesh_renameLayer_internal( self, args, CD_MCOL );
}


static PyObject *Mesh_getLayerNames_internal( BPy_Mesh * self, int type )
{
	CustomData *data;
	CustomDataLayer *layer;
	PyObject *str, *list = PyList_New( 0 );
	Mesh *mesh = self->mesh;
	int i;
	data = &mesh->fdata;
	
	/* see if there is a duplicate */
	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		if(layer->type == type) {
			str = PyString_FromString(layer->name);
			PyList_Append( list, str );
			Py_DECREF(str);
		}
	}
	return list;
}

static PyObject *Mesh_getUVLayerNames( BPy_Mesh * self )
{
	return Mesh_getLayerNames_internal(self, CD_MTFACE);
}

static PyObject *Mesh_getColorLayerNames( BPy_Mesh * self )
{
	return Mesh_getLayerNames_internal(self, CD_MCOL);
}
/* used by activeUVLayer and activeColorLayer attrs */
static PyObject *Mesh_getActiveLayer( BPy_Mesh * self, void *type )
{
	CustomData *data = &self->mesh->fdata;
	int layer_type = GET_INT_FROM_POINTER(type);
	int i;
	if (layer_type < 0) { /* hack, if negative, its the renderlayer.*/
		layer_type = -layer_type;
		i = CustomData_get_render_layer_index(data, layer_type);
	} else {
		i = CustomData_get_active_layer_index(data, layer_type);
	}
	if (i == -1) /* so -1 is for no active layer 0+ for an active layer */
		Py_RETURN_NONE;
	else {
		return PyString_FromString( data->layers[i].name);
	}
}

static int Mesh_setActiveLayer( BPy_Mesh * self, PyObject * value, void *type )
{
	CustomData *data = &self->mesh->fdata;
	char *name;
	int i,ok,n,layer_type = GET_INT_FROM_POINTER(type), render=0;
	
	if( !PyString_Check( value ) )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected a string argument" );

	if (layer_type<0) {
		layer_type = -layer_type;
		render = 1;
	}

	name = PyString_AsString( value );
	ok = 0;
	n = 0;
	for(i=0; i < data->totlayer; ++i) {
		if(data->layers[i].type == layer_type) {
			if (strcmp(data->layers[i].name, name)==0) {
				ok = 1;
				break;
			}
			n++;
		}
	}
	
	if (!ok)
		return EXPP_ReturnIntError( PyExc_ValueError,
				"layer name does not exist" );
	if (render) {
		CustomData_set_layer_render(data, layer_type, n);
	} else {
		CustomData_set_layer_active(data, layer_type, n);
		mesh_update_customdata_pointers(self->mesh);
	}
	return 0;
}


/* multires */
static PyObject *Mesh_getMultiresLevelCount( BPy_Mesh * self )
{
	int i;
	if (!self->mesh->mr)
		i=0;
	else
		i= self->mesh->mr->level_count;
	
	return PyInt_FromLong(i);
}


static PyObject *Mesh_getMultires( BPy_Mesh * self, void *type )
{	
	int i=0;
	if (self->mesh->mr) {
		switch (GET_INT_FROM_POINTER(type)) {
		case MESH_MULTIRES_LEVEL:
			i = self->mesh->mr->newlvl;
			break;
		case MESH_MULTIRES_EDGE:
			i = self->mesh->mr->edgelvl;
			break;
		case MESH_MULTIRES_PIN:
			i = self->mesh->mr->pinlvl;
			break;
		case MESH_MULTIRES_RENDER:
			i = self->mesh->mr->renderlvl;
			break;
		}
	}
	
	return PyInt_FromLong(i);
}

static int Mesh_setMultires( BPy_Mesh * self, PyObject *value, void *type )
{
	int i;
	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					"expected integer argument" );
	
	if (!self->object)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			"This mesh must be linked to an object" ); 
	
	if (!self->mesh->mr)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					"the mesh has no multires data" );
	
	if (!self->mesh->mr->level_count)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					"multires data has no levels added" );
	
	i = PyInt_AsLong(value);
	
	if (i<1||i>self->mesh->mr->level_count)
		return EXPP_ReturnIntError( PyExc_TypeError,
					"value out of range" );
	
	switch (GET_INT_FROM_POINTER(type)) {
	case MESH_MULTIRES_LEVEL:
		self->mesh->mr->newlvl = i;
		multires_set_level_cb(self->object, self->mesh);
		break;
	case MESH_MULTIRES_EDGE:
		self->mesh->mr->edgelvl = i;
		multires_edge_level_update(self->object, self->mesh);
		break;
	case MESH_MULTIRES_PIN:
		self->mesh->mr->pinlvl = i;
		break;
	case MESH_MULTIRES_RENDER:
		self->mesh->mr->renderlvl = i;
		break;
	}
	
	return 0;
}

static PyObject *Mesh_addMultiresLevel( BPy_Mesh * self, PyObject * args )
{
	char typenum;
	int i, levels = 1;
	char *type = NULL;
	if( G.obedit )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
					"can't add multires level while in edit mode" );
	if( !PyArg_ParseTuple( args, "|is", &levels, &type ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or an int and optionally a string as arguments" );
	if( !type || !strcmp( type, "catmull-clark" ) )
		typenum = 0;
	else if( !strcmp( type, "simple" ) )
		typenum = 1;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "if given, type should be 'catmull-clark' or 'simple'" );
	if (!self->mesh->mr)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"the mesh has no multires data" );
	for( i = 0; i < levels; i++ ) {
		multires_add_level(self->object, self->mesh, typenum);
	};
	multires_update_levels(self->mesh, 0);
	multires_level_to_editmesh(self->object, self->mesh, 0);	
	multires_finish_mesh_update(self->object);
	Py_RETURN_NONE;
}

/* end multires */


static PyObject *Mesh_Tools( BPy_Mesh * self, int type, void **args )
{
	Base *base;
	int result;
	Object *object = NULL; 
	PyObject *attr = NULL;

	/* if already in edit mode, exit */

	if( G.obedit )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"can't use mesh tools while in edit mode" );

	for( base = FIRSTBASE; base; base = base->next ) {
		if( base->object->type == OB_MESH && 
				base->object->data == self->mesh ) {
			object = base->object;
			break;
		}
	}
	if( !object )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"can't find an object for the mesh" );

	if( object->type != OB_MESH )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
						"Object specified is not a mesh." );

	/* make mesh's object active, enter mesh edit mode */
	G.obedit = object;
	make_editMesh();

	/* apply operation, then exit edit mode */
	switch( type ) {
	case MESH_TOOL_TOSPHERE:
		vertices_to_sphere();
		break;
	case MESH_TOOL_VERTEXSMOOTH:
		vertexsmooth();
		break;
	case MESH_TOOL_FLIPNORM:
		/* would be simple to rewrite this to not use edit mesh */
		/* see flipface() */
		flip_editnormals();
		break;
	case MESH_TOOL_SUBDIV:
		esubdivideflag( 1, 0.0, *((int *)args[0]), 1, 0 );
		break;
	case MESH_TOOL_REMDOUB:
		result = removedoublesflag( 1, 0, *((float *)args[0]) );

		attr = PyInt_FromLong( result );
		if( !attr )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"PyInt_FromLong() failed" );
		break;
	case MESH_TOOL_FILL:
		fill_mesh();
		break;
	case MESH_TOOL_RECALCNORM:
		righthandfaces( *((int *)args[0]) );
		break;
	case MESH_TOOL_TRI2QUAD:
		join_triangles();
		break;
	case MESH_TOOL_QUAD2TRI:
		convert_to_triface( *((int *)args[0]) );
		break;
	}

	/* exit edit mode, free edit mesh */
	load_editMesh();
	free_editMesh(G.editMesh);

	if(G.f & G_FACESELECT)
		EXPP_allqueue( REDRAWIMAGE, 0 );
	if(G.f & G_WEIGHTPAINT)
		mesh_octree_table(G.obedit, NULL, 'e');
	G.obedit = NULL;

	DAG_object_flush_update(G.scene, object, OB_RECALC_DATA);

	if( attr )
		return attr;

	Py_RETURN_NONE;
}

/*
 * "Subdivide" function
 */

static PyObject *Mesh_subdivide( BPy_Mesh * self, PyObject * args )
{
	int beauty = 0;
	void *params = &beauty;

	if( !PyArg_ParseTuple( args, "|i", &beauty ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected nothing or an int argument" );

	return Mesh_Tools( self, MESH_TOOL_SUBDIV, &params );
}

/*
 * "Smooth" function
 */

static PyObject *Mesh_smooth( BPy_Mesh * self )
{
	return Mesh_Tools( self, MESH_TOOL_VERTEXSMOOTH, NULL );
}

/*
 * "Remove doubles" function
 */

static PyObject *Mesh_removeDoubles( BPy_Mesh * self, PyObject *args )
{
	float limit;
	void *params = &limit;

	if( !PyArg_ParseTuple( args, "f", &limit ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected float argument" );

	limit = EXPP_ClampFloat( limit, 0.0f, 1.0f );

	return Mesh_Tools( self, MESH_TOOL_REMDOUB, &params );
}

/*
 * "recalc normals" function
 */

static PyObject *Mesh_recalcNormals( BPy_Mesh * self, PyObject *args )
{
	int direction = 0;
	void *params = &direction;

	if( !PyArg_ParseTuple( args, "|i", &direction ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected nothing or an int in range [0,1]" );

	if( direction < 0 || direction > 1 )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected int in range [0,1]" );

	/* righthandfaces(1) = outward, righthandfaces(2) = inward */
	++direction;

	return Mesh_Tools( self, MESH_TOOL_RECALCNORM, &params );
}

/*
 * "Quads to Triangles"  function
 */

static PyObject *Mesh_quad2tri( BPy_Mesh * self, PyObject *args )
{
	int kind = 0;
	void *params = &kind;

	if( !PyArg_ParseTuple( args, "|i", &kind ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected nothing or an int in range [0,1]" );

	if( kind < 0 || kind > 1 )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected int in range [0,1]" );

	return Mesh_Tools( self, MESH_TOOL_QUAD2TRI, &params );
}

/*
 * "Triangles to Quads"  function
 */

static PyObject *Mesh_tri2quad( BPy_Mesh * self )
{
	return Mesh_Tools( self, MESH_TOOL_TRI2QUAD, NULL );
}

/*
 * "Flip normals" function
 */

static PyObject *Mesh_flipNormals( BPy_Mesh * self )
{
	return Mesh_Tools( self, MESH_TOOL_FLIPNORM, NULL );
}

/*
 * "To sphere" function
 */

static PyObject *Mesh_toSphere( BPy_Mesh * self )
{
	return Mesh_Tools( self, MESH_TOOL_TOSPHERE, NULL );
}

/*
 * "Fill" (scan fill) function
 */

static PyObject *Mesh_fill( BPy_Mesh * self )
{
	return Mesh_Tools( self, MESH_TOOL_FILL, NULL );
}


/*
 * "pointInside" function
 */
/* Warning - this is ordered - need to test both orders to be sure */
#define SIDE_OF_LINE(pa,pb,pp)	((pa[0]-pp[0])*(pb[1]-pp[1]))-((pb[0]-pp[0])*(pa[1]-pp[1]))
#define POINT_IN_TRI(p0,p1,p2,p3)	((SIDE_OF_LINE(p1,p2,p0)>=0) && (SIDE_OF_LINE(p2,p3,p0)>=0) && (SIDE_OF_LINE(p3,p1,p0)>=0))
static short pointInside_internal(float *vec, float *v1, float *v2, float  *v3 )
{	
	float z,w1,w2,w3,wtot;
	
	
	if (vec[2] > MAX3(v1[2], v2[2], v3[2]))
		return 0;
	
	/* need to test both orders */
	if (!POINT_IN_TRI(vec, v1,v2,v3) && !POINT_IN_TRI(vec, v3,v2,v1))
		return 0;
	
	w1= AreaF2Dfl(vec, v2, v3);
	w2=	AreaF2Dfl(v1, vec, v3);
	w3=	AreaF2Dfl(v1, v2, vec);
	wtot = w1+w2+w3;
	w1/=wtot; w2/=wtot; w3/=wtot;
	z =((v1[2] * w1) +
		(v2[2] * w2) +
		(v3[2] * w3));
	
	/* only return true if the face is above vec*/
	if (vec[2] < z )
		return 1;

	return 0;
}

static PyObject *Mesh_pointInside( BPy_Mesh * self, PyObject * args, PyObject *kwd )
{
	Mesh *mesh = self->mesh;
	MFace *mf = mesh->mface;
	MVert *mvert = mesh->mvert;
	int i;
	int isect_count=0;
	int selected_only = 0;
	VectorObject *vec;
	static char *kwlist[] = {"point", "selected_only", NULL};
	
	if( !PyArg_ParseTupleAndKeywords(args, kwd, "|O!i", kwlist,
		 &vector_Type, &vec, &selected_only) ) {
			 return EXPP_ReturnPyObjError( PyExc_TypeError, "expected a vector and an optional bool argument");
	}
	
	if(vec->size < 3)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mesh.pointInside(vec) expects a 3D vector object\n");
	
	for( i = 0; i < mesh->totface; mf++, i++ ) {
		if (!selected_only || mf->flag & ME_FACE_SEL) {
			if (pointInside_internal(vec->vec, mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co)) {
				isect_count++;
			} else if (mf->v4 && pointInside_internal(vec->vec,mvert[mf->v1].co, mvert[mf->v3].co, mvert[mf->v4].co)) {
				
				isect_count++;
			}
		}
	}
	
	if (isect_count % 2)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}


/* This is a bit nasty, Blenders tangents are computed for rendering, and this isnt compatible with a normal Mesh
 * so we have to rewrite parts of it here, make sure these stay in sync */

static PyObject *Mesh_getTangents( BPy_Mesh * self )
{
	/* python stuff */
	PyObject *py_tanlist;
	PyObject *py_tuple;
	
	
	PyObject *py_vector;
#if 0	/* BI-TANGENT */
	PyObject *py_bivector;
	PyObject *py_pair;
	
	float no[3];
#endif
	/* mesh vars */
	Mesh *mesh = self->mesh;
	MTFace *tf = mesh->mtface;
	MFace *mf = mesh->mface;
	MVert *v1, *v2, *v3, *v4;
	int mf_vi[4];
	
	/* See convertblender.c */
	float *uv1, *uv2, *uv3, *uv4;
	float fno[3];
	float tang[3];
	float uv[4][2];
	float *vtang;
	
	float (*orco)[3] = NULL;
	
	MemArena *arena= NULL;
	VertexTangent **vtangents= NULL;
	int i, j, len;
	
	
	if(!mesh->mtface) {
		if (!self->object)
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, "cannot get tangents when there are not UV's, or the mesh has no link to an object");
		
		orco = (float(*)[3])get_mesh_orco_verts(self->object);
		
		if (!orco)
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, "cannot get orco's for this objects tangents");
	}
	
	/* vertex normals */
	arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_calloc(arena);
	vtangents= MEM_callocN(sizeof(VertexTangent*)*mesh->totvert, "VertexTangent");
	
	for( i = 0, tf = mesh->mtface, mf = mesh->mface; i < mesh->totface; mf++, tf++, i++ ) {
		v1 = &mesh->mvert[mf->v1];
		v2 = &mesh->mvert[mf->v2];
		v3 = &mesh->mvert[mf->v3];
		if (mf->v4) {
			v4 = &mesh->mvert[mf->v4];
			
			CalcNormFloat4( v1->co, v2->co, v3->co, v4->co, fno );
		} else {
			v4 = NULL;
			CalcNormFloat( v1->co, v2->co, v3->co, fno );
		}
		
		if(mesh->mtface) {
			uv1= tf->uv[0];
			uv2= tf->uv[1];
			uv3= tf->uv[2];
			uv4= tf->uv[3];
		} else {
			uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
			spheremap(orco[mf->v1][0], orco[mf->v1][1], orco[mf->v1][2], &uv[0][0], &uv[0][1]);
			spheremap(orco[mf->v2][0], orco[mf->v2][1], orco[mf->v2][2], &uv[1][0], &uv[1][1]);
			spheremap(orco[mf->v3][0], orco[mf->v3][1], orco[mf->v3][2], &uv[2][0], &uv[2][1]);
			if(v4)
				spheremap(orco[mf->v4][0], orco[mf->v4][1], orco[mf->v4][2], &uv[3][0], &uv[3][1]);
		}
		
		tangent_from_uv(uv1, uv2, uv3, v1->co, v2->co, v3->co, fno, tang);
		sum_or_add_vertex_tangent(arena, &vtangents[mf->v1], tang, uv1);
		sum_or_add_vertex_tangent(arena, &vtangents[mf->v2], tang, uv2);
		sum_or_add_vertex_tangent(arena, &vtangents[mf->v3], tang, uv3);
		
		if (mf->v4) {
			v4 = &mesh->mvert[mf->v4];
			
			tangent_from_uv(uv1, uv3, uv4, v1->co, v3->co, v4->co, fno, tang);
			sum_or_add_vertex_tangent(arena, &vtangents[mf->v1], tang, uv1);
			sum_or_add_vertex_tangent(arena, &vtangents[mf->v3], tang, uv3);
			sum_or_add_vertex_tangent(arena, &vtangents[mf->v4], tang, uv4);
		}
	}
	
	
	py_tanlist = PyList_New(mesh->totface);
	
	for( i = 0, tf = mesh->mtface, mf = mesh->mface; i < mesh->totface; mf++, tf++, i++ ) {
		 
		len = mf->v4 ? 4 : 3; 
		
		if(mesh->mtface) {
			uv1= tf->uv[0];
			uv2= tf->uv[1];
			uv3= tf->uv[2];
			uv4= tf->uv[3];
		} else {
			uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
			spheremap(orco[mf->v1][0], orco[mf->v1][1], orco[mf->v1][2], &uv[0][0], &uv[0][1]);
			spheremap(orco[mf->v2][0], orco[mf->v2][1], orco[mf->v2][2], &uv[1][0], &uv[1][1]);
			spheremap(orco[mf->v3][0], orco[mf->v3][1], orco[mf->v3][2], &uv[2][0], &uv[2][1]);
			if(len==4)
				spheremap(orco[mf->v4][0], orco[mf->v4][1], orco[mf->v4][2], &uv[3][0], &uv[3][1]);
		}
		
		mf_vi[0] = mf->v1;
		mf_vi[1] = mf->v2;
		mf_vi[2] = mf->v3;
		mf_vi[3] = mf->v4;
		
#if 0	/* BI-TANGENT */
		/* now calculate the bitangent */
		if (mf->flag & ME_SMOOTH) {
			no[0] = (float)(mesh->mvert[mf_vi[j]]->no[0] / 32767.0);
			no[1] = (float)(mesh->mvert[mf_vi[j]]->no[1] / 32767.0);
			no[2] = (float)(mesh->mvert[mf_vi[j]]->no[2] / 32767.0);
		} else {
			/* calc face normal */
			if (len==4)		CalcNormFloat4( mesh->mvert[0]->co, mesh->mvert[1]->co, mesh->mvert[2]->co, mesh->mvert[3]->co, no );
			else			CalcNormFloat4( mesh->mvert[0]->co, mesh->mvert[1]->co, mesh->mvert[2]->co, no );
		}
#endif
		
		py_tuple = PyTuple_New( len );
		
		for (j=0; j<len; j++) {
			vtang= find_vertex_tangent(vtangents[mf_vi[j]], mesh->mtface ? tf->uv[j] : uv[j]);	/* mf_vi[j] == mf->v1,   uv[j] == tf->uv[0] */
			
			py_vector = newVectorObject( vtang, 3, Py_NEW );
			Normalize(((VectorObject *)py_vector)->vec);
			
#if 0		/* BI-TANGENT */
			py_pair = PyTuple_New( 2 );
			PyTuple_SetItem( py_pair, 0, py_vector );
			PyTuple_SetItem( py_pair, 1, py_bivector );
			
			/* qdn: tangent space */
			/* copied from texture.c */
			float B[3], tv[3];
			Crossf(B, shi->vn, shi->nmaptang);	/* bitangent */
			/* transform norvec from tangent space to object surface in camera space */
			tv[0] = texres.nor[0]*shi->nmaptang[0] + texres.nor[1]*B[0] + texres.nor[2]*shi->vn[0];
			tv[1] = texres.nor[0]*shi->nmaptang[1] + texres.nor[1]*B[1] + texres.nor[2]*shi->vn[1];
			tv[2] = texres.nor[0]*shi->nmaptang[2] + texres.nor[1]*B[2] + texres.nor[2]*shi->vn[2];
			shi->vn[0]= facm*shi->vn[0] + fact*tv[0];
			shi->vn[1]= facm*shi->vn[1] + fact*tv[1];
			shi->vn[2]= facm*shi->vn[2] + fact*tv[2];				
			PyTuple_SetItem( py_tuple, j, py_pair );
#else
			PyTuple_SetItem( py_tuple, j, py_vector );
#endif
		}

		PyList_SetItem( py_tanlist, i, py_tuple );
	}
	
	BLI_memarena_free(arena);
	if (orco) MEM_freeN( orco );
	MEM_freeN( vtangents );
	
	return py_tanlist;
}

/*
 * "__copy__" return a copy of the mesh
 */

static PyObject *Mesh_copy( BPy_Mesh * self )
{
	BPy_Mesh *obj;

	obj = (BPy_Mesh *)PyObject_NEW( BPy_Mesh, &Mesh_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "PyObject_New() failed" );
	
	obj->mesh = copy_mesh( self->mesh );
	obj->mesh->id.us= 0;
	obj->object = NULL;
	obj->new = 1;
	return (PyObject *)obj;
}


static struct PyMethodDef BPy_Mesh_methods[] = {
	{"calcNormals", (PyCFunction)Mesh_calcNormals, METH_NOARGS,
		"all recalculate vertex normals"},
	{"vertexShade", (PyCFunction)Mesh_vertexShade, METH_VARARGS,
		"color vertices based on the current lighting setup"},
	{"findEdges", (PyCFunction)Mesh_findEdges, METH_VARARGS,
		"find indices of an multiple edges in the mesh"},
	{"getFromObject", (PyCFunction)Mesh_getFromObject, METH_VARARGS,
		"Get a mesh by name"},
	{"update", (PyCFunction)Mesh_Update, METH_VARARGS | METH_KEYWORDS,
		"Update display lists after changes to mesh"},
	{"transform", (PyCFunction)Mesh_transform, METH_VARARGS | METH_KEYWORDS,
		"Applies a transformation matrix to mesh's vertices"},
	{"addVertGroup", (PyCFunction)Mesh_addVertGroup, METH_O,
		"Assign vertex group name to the object linked to the mesh"},
	{"removeVertGroup", (PyCFunction)Mesh_removeVertGroup, METH_O,
		"Delete vertex group name from the object linked to the mesh"},
	{"assignVertsToGroup", (PyCFunction)Mesh_assignVertsToGroup, METH_VARARGS,
		"Assigns vertices to a vertex group"},
	{"removeVertsFromGroup", (PyCFunction)Mesh_removeVertsFromGroup, METH_VARARGS,
		"Removes vertices from a vertex group"},
	{"getVertsFromGroup", (PyCFunction)Mesh_getVertsFromGroup, METH_VARARGS,
		"Get index and optional weight for vertices in vertex group"},
	{"renameVertGroup", (PyCFunction)Mesh_renameVertGroup, METH_VARARGS,
		"Rename an existing vertex group"},
	{"getVertGroupNames", (PyCFunction)Mesh_getVertGroupNames, METH_NOARGS,
		"Get names of vertex groups"},
	{"getVertexInfluences", (PyCFunction)Mesh_getVertexInfluences, METH_VARARGS,
		"Get list of the influences of bones for a given mesh vertex"},
	/* Shape Keys */
	{"removeAllKeys", (PyCFunction)Mesh_removeAllKeys, METH_NOARGS,
		"Remove all the shape keys from a mesh"},
	{"insertKey", (PyCFunction)Mesh_insertKey, METH_VARARGS,
		"(frame = None, type = 'relative') - inserts a Mesh key at the given frame"},
	/* Mesh tools */
	{"smooth", (PyCFunction)Mesh_smooth, METH_NOARGS,
		"Flattens angle of selected faces (experimental)"},
	{"flipNormals", (PyCFunction)Mesh_flipNormals, METH_NOARGS,
		"Toggles the direction of selected face's normals (experimental)"},
	{"toSphere", (PyCFunction)Mesh_toSphere, METH_NOARGS,
		"Moves selected vertices outward in a spherical shape (experimental)"},
	{"fill", (PyCFunction)Mesh_fill, METH_NOARGS,
		"Scan fill a closed edge loop (experimental)"},
	{"triangleToQuad", (PyCFunction)Mesh_tri2quad, METH_VARARGS,
		"Convert selected triangles to quads (experimental)"},
	{"quadToTriangle", (PyCFunction)Mesh_quad2tri, METH_VARARGS,
		"Convert selected quads to triangles (experimental)"},
	{"subdivide", (PyCFunction)Mesh_subdivide, METH_VARARGS,
		"Subdivide selected edges in a mesh (experimental)"},
	{"remDoubles", (PyCFunction)Mesh_removeDoubles, METH_VARARGS,
		"Removes duplicates from selected vertices (experimental)"},
	{"recalcNormals", (PyCFunction)Mesh_recalcNormals, METH_VARARGS,
		"Recalculates inside or outside normals (experimental)"},
	{"pointInside", (PyCFunction)Mesh_pointInside, METH_VARARGS|METH_KEYWORDS,
		"Recalculates inside or outside normals (experimental)"},
	{"getTangents", (PyCFunction)Mesh_getTangents, METH_VARARGS|METH_KEYWORDS,
		"Return a list of face tangents"},
		
	/* mesh custom data layers */
	{"addUVLayer", (PyCFunction)Mesh_addUVLayer, METH_VARARGS,
		"adds a UV layer to this mesh"},
	{"addColorLayer", (PyCFunction)Mesh_addColorLayer, METH_VARARGS,
		"adds a color layer to this mesh "},
	{"removeUVLayer", (PyCFunction)Mesh_removeUVLayer, METH_O,
		"removes a UV layer to this mesh"},
	{"removeColorLayer", (PyCFunction)Mesh_removeColorLayer, METH_O,
		"removes a color layer to this mesh"},
	{"getUVLayerNames", (PyCFunction)Mesh_getUVLayerNames, METH_NOARGS,
		"Get names of UV layers"},
	{"getColorLayerNames", (PyCFunction)Mesh_getColorLayerNames, METH_NOARGS,
		"Get names of Color layers"},
	{"renameUVLayer", (PyCFunction)Mesh_renameUVLayer, METH_VARARGS,
		"Rename a UV Layer"},
	{"renameColorLayer", (PyCFunction)Mesh_renameColorLayer, METH_VARARGS,
		"Rename a Color Layer"},
	/* mesh multires */
	{"addMultiresLevel", (PyCFunction)Mesh_addMultiresLevel, METH_VARARGS,
		"(levels=1, type='catmull-clark') - adds multires levels of given type"},
		
	/* python standard class functions */
	{"__copy__", (PyCFunction)Mesh_copy, METH_NOARGS,
		"Return a copy of the mesh"},
	{"copy", (PyCFunction)Mesh_copy, METH_NOARGS,
		"Return a copy of the mesh"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python BPy_Mesh attributes
 *
 ************************************************************************/

static PyObject *MVertSeq_CreatePyObject( Mesh * mesh )
{
	
	BPy_MVertSeq *obj = PyObject_NEW( BPy_MVertSeq, &MVertSeq_Type);
	obj->mesh = mesh;
	
	/*
	an iter of -1 means this seq has not been used as an iterator yet
	once it is, then any other calls on getIter will return a new copy of BPy_MVertSeq
	This means you can loop do nested loops with the same iterator without worrying about
	the iter variable being used twice and messing up the loops.
	*/
	obj->iter = -1;
	return (PyObject *)obj;
}

static PyObject *Mesh_getVerts( BPy_Mesh * self )
{
	return MVertSeq_CreatePyObject(self->mesh);
}

static int Mesh_setVerts( BPy_Mesh * self, PyObject * args )
{
	MVert *dst;
	MVert *src;
	int i;
	
	/* special case if None: delete the mesh */
	if( args == NULL || args == Py_None ) {
		Mesh *me = self->mesh;
		free_mesh( me );
        me->mvert = NULL; me->medge = NULL; me->mface = NULL;
		me->mtface = NULL; me->dvert = NULL; me->mcol = NULL;
		me->msticky = NULL; me->mat = NULL; me->bb = NULL; me->mselect = NULL;
		me->totvert = me->totedge = me->totface = me->totcol = 0;
		mesh_update( me );
		return 0;
	}

	if( PyList_Check( args ) ) {
		if( EXPP_check_sequence_consistency( args, &MVert_Type ) != 1 &&
			  EXPP_check_sequence_consistency( args, &PVert_Type ) != 1 )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected a list of MVerts" );

		if( PyList_Size( args ) != self->mesh->totvert )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"list must have the same number of vertices as the mesh" );

		dst = self->mesh->mvert;
		for( i = 0; i < PyList_Size( args ); ++i ) {
			BPy_MVert *v = (BPy_MVert *)PyList_GET_ITEM( args, i );

			if( BPy_MVert_Check( v ) )
				src = &((Mesh *)v->data)->mvert[v->index];
			else
				src = (MVert *)v->data;

			memcpy( dst, src, sizeof(MVert) );
			++dst;
		}
	} else if( args->ob_type == &MVertSeq_Type ) {
		Mesh *mesh = ( (BPy_MVertSeq *) args)->mesh;

		if( mesh->totvert != self->mesh->totvert )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"vertex sequences must have the same number of vertices" );

		memcpy( self->mesh->mvert, mesh->mvert, mesh->totvert*sizeof(MVert) );
	} else
		return EXPP_ReturnIntError( PyExc_TypeError, 
				"expected a list or sequence of MVerts" );
	return 0;
}

static PyObject *MEdgeSeq_CreatePyObject( Mesh *mesh )
{
	BPy_MEdgeSeq *obj = PyObject_NEW( BPy_MEdgeSeq, &MEdgeSeq_Type);
	obj->mesh = mesh;
	obj->iter = -1; /* iterator not yet used */
	return (PyObject *)obj;
}

static PyObject *Mesh_getEdges( BPy_Mesh * self )
{
	return MEdgeSeq_CreatePyObject(self->mesh);
}

static PyObject *MFaceSeq_CreatePyObject( Mesh * mesh )
{
	BPy_MFaceSeq *obj= PyObject_NEW( BPy_MFaceSeq, &MFaceSeq_Type);
	obj->mesh = mesh;
	obj->iter = -1; /* iterator not yet used */
	return (PyObject *)obj;
}

static PyObject *Mesh_getFaces( BPy_Mesh * self )
{
	return MFaceSeq_CreatePyObject( self->mesh );
}

static PyObject *Mesh_getMaterials( BPy_Mesh *self )
{
	return EXPP_PyList_fromMaterialList( self->mesh->mat,
			self->mesh->totcol, 1 );
}

static int Mesh_setMaterials( BPy_Mesh *self, PyObject * value )
{
    Material **matlist;
	int len;

    if( !PySequence_Check( value ) ||
			!EXPP_check_sequence_consistency( value, &Material_Type ) )
        return EXPP_ReturnIntError( PyExc_TypeError,
                  "list should only contain materials or None)" );

    len = PyList_Size( value );
    if( len > 16 )
        return EXPP_ReturnIntError( PyExc_TypeError,
                          "list can't have more than 16 materials" );

	/* free old material list (if it exists) and adjust user counts */
	if( self->mesh->mat ) {
		Mesh *me = self->mesh;
		int i;
		for( i = me->totcol; i-- > 0; )
			if( me->mat[i] )
           		me->mat[i]->id.us--;
		MEM_freeN( me->mat );
	}

	/* build the new material list, increment user count, store it */

	matlist = EXPP_newMaterialList_fromPyList( value );
	EXPP_incr_mats_us( matlist, len );
	self->mesh->mat = matlist;
    	self->mesh->totcol = (short)len;

/**@ This is another ugly fix due to the weird material handling of blender.
    * it makes sure that object material lists get updated (by their length)
    * according to their data material lists, otherwise blender crashes.
    * It just stupidly runs through all objects...BAD BAD BAD.
    */

    	test_object_materials( ( ID * ) self->mesh );

	return 0;
}

static PyObject *Mesh_getMaxSmoothAngle( BPy_Mesh * self )
{
    return PyInt_FromLong( self->mesh->smoothresh );
}

static int Mesh_setMaxSmoothAngle( BPy_Mesh *self, PyObject *value )
{
    return EXPP_setIValueClamped( value, &self->mesh->smoothresh,
                            MESH_SMOOTHRESH_MIN,
                            MESH_SMOOTHRESH_MAX, 'h' );
}

static PyObject *Mesh_getSubDivLevels( BPy_Mesh * self )
{
	return  Py_BuildValue( "(h,h)",
			self->mesh->subdiv, self->mesh->subdivr );
}

static int Mesh_setSubDivLevels( BPy_Mesh *self, PyObject *value )
{
	int subdiv[2];
	int i;
	PyObject *tmp;

	if( !PyTuple_Check( value ) || PyTuple_Size( value ) != 2 )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected (int, int) as argument" );

	for( i = 0; i < 2; i++ ) {
		tmp = PyTuple_GET_ITEM( value, i );
		if( !PyInt_Check( tmp ) )
			return EXPP_ReturnIntError ( PyExc_TypeError,
				  "expected a list [int, int] as argument" );
		subdiv[i] = EXPP_ClampInt( PyInt_AsLong( tmp ),
						 MESH_SUBDIV_MIN,
						 MESH_SUBDIV_MAX );
	}

	self->mesh->subdiv = (short)subdiv[0];
	self->mesh->subdivr = (short)subdiv[1];
	return 0;
}

static PyObject *Mesh_getFlag( BPy_Mesh * self, void *type )
{
	switch( (long)type ) {
	case MESH_HASFACEUV:
		return self->mesh->mtface ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
	case MESH_HASMCOL:
		return self->mesh->mcol ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
	case MESH_HASVERTUV:
		return self->mesh->msticky ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
	case MESH_HASMULTIRES:
		return self->mesh->mr ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get attribute" );
	}
}

static int Mesh_setFlag( BPy_Mesh * self, PyObject *value, void *type )
{
	int param;
	Mesh *mesh = self->mesh;

	param = PyObject_IsTrue( value );

	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );

	/* sticky is independent of faceUV and vertUV */

	switch( (long)type ) {
	case MESH_HASFACEUV:
		if( !param ) {
			if( mesh->mtface ) {
				CustomData_free_layers( &mesh->fdata, CD_MTFACE, mesh->totface );
				mesh->mtface = NULL;
			}
		} else if( !mesh->mtface ) {
			if( !mesh->totface )
				return EXPP_ReturnIntError( PyExc_RuntimeError,
					"mesh has no faces" );
			make_tfaces( mesh );
		}
		return 0;
	case MESH_HASMCOL:
		if( !param ) {
			if( mesh->mcol ) {
				CustomData_free_layers( &mesh->fdata, CD_MCOL, mesh->totface );
				mesh->mcol = NULL;
			}
		} else if( !mesh->mcol ) {
				/* TODO: mesh_create_shadedColors */
			mesh->mcol = CustomData_add_layer( &mesh->fdata, CD_MCOL,
				CD_DEFAULT, NULL, mesh->totface );
		}
		return 0;
	case MESH_HASVERTUV:
		if( !param ) {
			if( mesh->msticky ) {
				CustomData_free_layer_active( &mesh->vdata, CD_MSTICKY, mesh->totvert );
				mesh->msticky = NULL;
			}
		} else {
			if( !mesh->msticky ) {
				mesh->msticky = CustomData_add_layer( &mesh->vdata, CD_MSTICKY,
					CD_CALLOC, NULL, mesh->totvert );
				memset( mesh->msticky, 255, mesh->totvert*sizeof( MSticky ) );
				/* TODO: rework RE_make_sticky() so we can calculate */
			}
		}
		return 0;
	case MESH_HASMULTIRES:
		if (!self->object)
			return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This mesh must be linked to an object" ); 
		
		if( !param ) {
			if ( mesh->mr ) {
				multires_delete(self->object, mesh);
			}
		} else {
			if ( !mesh->mr ) {
				if (mesh->key)
					return EXPP_ReturnIntError( PyExc_RuntimeError,
						"Cannot enable multires for a mesh with shape keys" ); 
				multires_make(self->object, mesh);
			}
		}
		return 0;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					"couldn't get attribute" );
	}
}

static PyObject *Mesh_getMode( BPy_Mesh * self )
{
	return PyInt_FromLong( self->mesh->flag );
}

static int Mesh_setMode( BPy_Mesh *self, PyObject *value )
{
	short param;
	static short bitmask = ME_ISDONE | ME_NOPUNOFLIP | ME_TWOSIDED |
		ME_UVEFFECT | ME_VCOLEFFECT | ME_AUTOSMOOTH | ME_SMESH |
		ME_SUBSURF | ME_OPT_EDGES;

	if( !PyInt_Check ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = (short)PyInt_AS_LONG ( value );

	if( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	self->mesh->flag = param; 

	return 0;
}

static PyObject *Mesh_getKey( BPy_Mesh * self )
{
	if( self->mesh->key )
		return Key_CreatePyObject(self->mesh->key);
	else
		Py_RETURN_NONE;
}


static PyObject *Mesh_getActiveFace( BPy_Mesh * self )
{
	/* not needed but keep incase exceptions make use of it */
	if( !self->mesh->mtface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if (self->mesh->act_face != -1 && self->mesh->act_face <= self->mesh->totface)
		return PyInt_FromLong( self->mesh->act_face );
	
	Py_RETURN_NONE;
}

static int Mesh_setActiveFace( BPy_Mesh * self, PyObject * value )
{
	int param;

	/* if no texture faces, error */

	if( !self->mesh->mtface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	/* if param isn't an int, error */

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int argument" );

	/* check for a valid index */

	param = PyInt_AsLong( value );
	if( param < 0 || param > self->mesh->totface )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"face index out of range" );
	
	self->mesh->act_face = param;
	return 0;
}

static PyObject *Mesh_getActiveGroup( BPy_Mesh * self )
{
	bDeformGroup *defGroup;
	Object *object = self->object;

	if( !object )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This mesh must be linked to an object" ); 

	if( object->actdef ) {
		defGroup = BLI_findlink( &object->defbase, object->actdef-1 );
		return PyString_FromString( defGroup->name );
	}

	Py_RETURN_NONE;
}

static int Mesh_setActiveGroup( BPy_Mesh * self, PyObject * arg )
{
	char *name;
	int tmp;
	Object *object = self->object;

	if( !object )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This mesh must be linked to an object" ); 

	if( !PyString_Check( arg ) )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"expected a string argument" );

	name = PyString_AsString( arg );
	tmp = object->actdef;
	vertexgroup_select_by_name( object, name );
	if( !object->actdef ) {
		object->actdef = tmp;
		return EXPP_ReturnIntError( PyExc_ValueError,
				"vertex group not found" );
	}

	return 0;
}

static PyObject *Mesh_getTexMesh( BPy_Mesh * self )
{
	Mesh *texme= self->mesh->texcomesh;
	
	if (texme)
		return Mesh_CreatePyObject( texme, NULL );
	else
		Py_RETURN_NONE;
}

static int Mesh_setTexMesh( BPy_Mesh * self, PyObject * value )
{	
	int ret = GenericLib_assignData(value, (void **) &self->mesh->texcomesh, 0, 1, ID_ME, 0);
	
	if (ret==0 && value!=Py_None) /*This must be a mesh type*/
		(( BPy_Mesh * ) value)->new= 0;
	
	return ret;
}

static int Mesh_setSel( BPy_Mesh * self, PyObject * value )
{
	int i, param = PyObject_IsTrue( value );
	Mesh *me = self->mesh;
	MVert *mvert = me->mvert;
	MEdge *medge = me->medge;
	MFace *mface = me->mface;

	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if( param ) {
		for( i = 0; i < me->totvert; ++mvert, ++i )
			mvert->flag |= SELECT;
		for( i = 0; i < me->totedge; ++medge, ++i )
			medge->flag |= SELECT;
		for( i = 0; i < me->totface; ++mface, ++i )
			mface->flag |= ME_FACE_SEL;
	} else {
		for( i = 0; i < me->totvert; ++mvert, ++i )
			mvert->flag &= ~SELECT;
		for( i = 0; i < me->totedge; ++medge, ++i )
			medge->flag &= ~SELECT;
		for( i = 0; i < me->totface; ++mface, ++i )
			mface->flag &= ~ME_FACE_SEL;
	}

	return 0;
}

static int Mesh_setHide( BPy_Mesh * self, PyObject * value )
{
	int i, param = PyObject_IsTrue( value );
	Mesh *me = self->mesh;
	MVert *mvert = me->mvert;
	MEdge *medge = me->medge;
	MFace *mface = me->mface;

	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if( param ) {
		for( i = 0; i < me->totvert; ++mvert, ++i )
			mvert->flag |= ME_HIDE;
		for( i = 0; i < me->totedge; ++medge, ++i )
			medge->flag |= ME_HIDE;
		for( i = 0; i < me->totface; ++mface, ++i )
			mface->flag |= ME_HIDE;
	} else {
		for( i = 0; i < me->totvert; ++mvert, ++i )
			mvert->flag &= ~ME_HIDE;
		for( i = 0; i < me->totedge; ++medge, ++i )
			medge->flag &= ~ME_HIDE;
		for( i = 0; i < me->totface; ++mface, ++i )
			mface->flag &= ~ME_HIDE;
	}

	return 0;
}

/************************************************************************
 *
 * Python Mesh_Type standard operations
 *
 ************************************************************************/

static void Mesh_dealloc( BPy_Mesh * self )
{
	Mesh *mesh = self->mesh;

	/* if the mesh is new and has no users, delete it */
	if( self->new && !mesh->id.us )
	    free_libblock( &G.main->mesh, mesh );

	PyObject_DEL( self );
}

static int Mesh_compare( BPy_Mesh * a, BPy_Mesh * b )
{
	return ( a->mesh == b->mesh ) ? 0 : -1;
}

static PyObject *Mesh_repr( BPy_Mesh * self )
{
	return PyString_FromFormat( "[Mesh \"%s\"]",
				    self->mesh->id.name + 2 );
}

/*****************************************************************************/
/* Python Mesh_Type attributes get/set structure:                           */
/*****************************************************************************/
static PyGetSetDef BPy_Mesh_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"verts",
	 (getter)Mesh_getVerts, (setter)Mesh_setVerts,
	 "The mesh's vertices (MVert)",
	 NULL},
	{"edges",
	 (getter)Mesh_getEdges, (setter)NULL,
	 "The mesh's edge data (MEdge)",
	 NULL},
	{"faces",
	 (getter)Mesh_getFaces, (setter)NULL,
	 "The mesh's face data (MFace)",
	 NULL},
	{"materials",
	 (getter)Mesh_getMaterials, (setter)Mesh_setMaterials,
	 "List of the mesh's materials",
	 NULL},
	{"degr",
	 (getter)Mesh_getMaxSmoothAngle, (setter)Mesh_setMaxSmoothAngle,
	 "The max angle for auto smoothing",
	 NULL},
	{"maxSmoothAngle",
	 (getter)Mesh_getMaxSmoothAngle, (setter)Mesh_setMaxSmoothAngle,
	 "deprecated: see 'degr'",
	 NULL},
	{"subDivLevels",
	 (getter)Mesh_getSubDivLevels, (setter)Mesh_setSubDivLevels,
	 "The display and rendering subdivision levels",
	 NULL},
	{"mode",
	 (getter)Mesh_getMode, (setter)Mesh_setMode,
	 "The mesh's mode bitfield",
	 NULL},
	{"key",
	 (getter)Mesh_getKey, (setter)NULL,
	 "The mesh's key",
	 NULL},
	{"faceUV",
	 (getter)Mesh_getFlag, (setter)Mesh_setFlag,
	 "UV-mapped textured faces enabled",
 	 (void *)MESH_HASFACEUV},
	{"vertexColors",
	 (getter)Mesh_getFlag, (setter)Mesh_setFlag,
	 "Vertex colors for the mesh enabled",
	 (void *)MESH_HASMCOL},
	{"vertexUV",
	 (getter)Mesh_getFlag, (setter)Mesh_setFlag,
	 "'Sticky' flag for per vertex UV coordinates enabled",
	 (void *)MESH_HASVERTUV},
	{"multires",
	 (getter)Mesh_getFlag, (setter)Mesh_setFlag,
	 "'Sticky' flag for per vertex UV coordinates enabled",
	 (void *)MESH_HASMULTIRES},
	{"activeFace",
	 (getter)Mesh_getActiveFace, (setter)Mesh_setActiveFace,
	 "Index of the mesh's active texture face (in UV editor)",
	 NULL},
	{"activeGroup",
	 (getter)Mesh_getActiveGroup, (setter)Mesh_setActiveGroup,
	 "Active group for the mesh",
	 NULL},

	/* uv layers */
	{"activeColorLayer",
	 (getter)Mesh_getActiveLayer, (setter)Mesh_setActiveLayer,
	 "Name of the active UV layer",
	 (void *)CD_MCOL},
	{"activeUVLayer",
	 (getter)Mesh_getActiveLayer, (setter)Mesh_setActiveLayer,
	 "Name of the active vertex color layer",
	 (void *)CD_MTFACE},
	/* hack flip CD_MCOL so it uses the render setting */
	{"renderColorLayer",
	 (getter)Mesh_getActiveLayer, (setter)Mesh_setActiveLayer,
	 "Name of the render UV layer",
	 (void *)-CD_MCOL},
	{"renderUVLayer",
	 (getter)Mesh_getActiveLayer, (setter)Mesh_setActiveLayer,
	 "Name of the render vertex color layer",
	 (void *)-CD_MTFACE},
	 
	 

	/* Multires */
	{"multiresLevelCount",
	 (getter)Mesh_getMultiresLevelCount, (setter)NULL,
	 "The total number of multires levels",
	 NULL},
	{"multiresDrawLevel",
	 (getter)Mesh_getMultires, (setter)Mesh_setMultires,
	 "The current multires display level",
	 (void *)MESH_MULTIRES_LEVEL},
	{"multiresEdgeLevel",
	 (getter)Mesh_getMultires, (setter)Mesh_setMultires,
	 "The current multires edge level",
	 (void *)MESH_MULTIRES_EDGE},
	{"multiresPinLevel",
	 (getter)Mesh_getMultires, (setter)Mesh_setMultires,
	 "The current multires pin level",
	 (void *)MESH_MULTIRES_PIN},
	{"multiresRenderLevel",
	 (getter)Mesh_getMultires, (setter)Mesh_setMultires,
	 "The current multires render level",
	 (void *)MESH_MULTIRES_RENDER},

	{"texMesh",
	 (getter)Mesh_getTexMesh, (setter)Mesh_setTexMesh,
	 "The meshes tex mesh proxy texture coord mesh",
	 NULL},
	{"sel",
	 (getter)NULL, (setter)Mesh_setSel,
	 "Select/deselect all verts, edges, faces in the mesh",
	 NULL},
	{"hide",
	 (getter)NULL, (setter)Mesh_setHide,
	 "Hide/unhide all verts, edges, faces in the mesh",
	 NULL},

	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Mesh_Type structure definition:                                   */
/*****************************************************************************/
PyTypeObject Mesh_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Mesh",             /* char *tp_name; */
	sizeof( BPy_Mesh ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) Mesh_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Mesh_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) Mesh_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Mesh_methods,          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Mesh_getseters,        /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*
 * get one or all mesh data objects
 */

static PyObject *M_Mesh_Get( PyObject * self_unused, PyObject * args )
{
	char *name = NULL;
	Mesh *mesh = NULL;
	BPy_Mesh* obj;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected zero or one string arguments" );

	if( name ) {
		mesh = ( Mesh * ) GetIdFromList( &( G.main->mesh ), name );

		if( !mesh ) {
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Mesh \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}
		return Mesh_CreatePyObject( mesh, NULL );
	} else {			/* () - return a list with all meshes in the scene */
		PyObject *meshlist;
		Link *link;
		int index = 0;

		meshlist = PyList_New( BLI_countlist( &( G.main->mesh ) ) );

		if( !meshlist )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
					"couldn't create PyList" );

		link = G.main->mesh.first;
		index = 0;
		while( link ) {
			obj = ( BPy_Mesh * ) Mesh_CreatePyObject( ( Mesh * )link, NULL );
			PyList_SetItem( meshlist, index, ( PyObject * ) obj );
			index++;
			link = link->next;
		}
		return meshlist;
	}
}

/*
 * create a new mesh data object
 */

static PyObject *M_Mesh_New( PyObject * self_unused, PyObject * args )
{
	char *name = "Mesh";
	Mesh *mesh;
	BPy_Mesh *obj;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or a string as argument" );

	obj = (BPy_Mesh *)PyObject_NEW( BPy_Mesh, &Mesh_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "PyObject_New() failed" );

	mesh = add_mesh(name); /* doesn't return NULL now, but might someday */
	
	if( !mesh ) {
		Py_DECREF ( obj );
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "FATAL: could not create mesh object" );
	}
	
	/* Bound box set to null needed because a new mesh is initialized
	with a bounding box of -1 -1 -1 -1 -1 -1
	if its not set to null the bounding box is not re-calculated
	when ob.getBoundBox() is called.*/
	MEM_freeN(mesh->bb);
	mesh->bb= NULL;
	
	mesh->id.us = 0;

	obj->mesh = mesh;
	obj->object = NULL;
	obj->new = 1;
	return (PyObject *)obj;
}

/*
 * creates a new MVert for users to manipulate
 */

static PyObject *M_Mesh_MVert( PyObject * self_unused, PyObject * args )
{
	int i;
	MVert vert;

	/* initialize the new vert's data */
	memset( &vert, 0, sizeof( MVert ) );

	/*
	 * accept either a 3D vector or tuple of three floats
	 */

	if( PyTuple_Size ( args ) == 1 ) {
		PyObject *tmp = PyTuple_GET_ITEM( args, 0 );
		if( !VectorObject_Check( tmp ) || ((VectorObject *)tmp)->size != 3 )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected three floats or vector of size 3" );
		for( i = 0; i < 3; ++i )
			vert.co[i] = ((VectorObject *)tmp)->vec[i];
	} else if( !PyArg_ParseTuple ( args, "fff",
				&vert.co[0], &vert.co[1], &vert.co[2] ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
			"expected three floats or vector of size 3" );

	/* make a new MVert from the data */
	return PVert_CreatePyObject( &vert );
}

static PyObject *M_Mesh_Modes( PyObject * self_unused, PyObject * args )
{
	int modes = 0;

	if( !G.scene ) {
		Py_RETURN_NONE;
	}

	if( !PyArg_ParseTuple( args, "|i", &modes ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected optional int as argument" );

	if( modes > ( SCE_SELECT_VERTEX | SCE_SELECT_EDGE | SCE_SELECT_FACE ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "value out of range" );

	if( modes > 0 )
		G.scene->selectmode = (short)modes;
	
	return PyInt_FromLong( G.scene->selectmode );
}

static struct PyMethodDef M_Mesh_methods[] = {
	{"New", (PyCFunction)M_Mesh_New, METH_VARARGS,
		"Create a new mesh"},
	{"Get", (PyCFunction)M_Mesh_Get, METH_VARARGS,
		"Get a mesh by name"},
	{"MVert", (PyCFunction)M_Mesh_MVert, METH_VARARGS,
		"Create a new MVert"},
	{"Mode", (PyCFunction)M_Mesh_Modes, METH_VARARGS,
		"Get/set edit selection mode(s)"},
	{NULL, NULL, 0, NULL},
};

static PyObject *M_Mesh_ModesDict( void )
{
	PyObject *Modes = PyConstant_New(  );

	if( Modes ) {
		BPy_constant *d = ( BPy_constant * ) Modes;

		PyConstant_Insert( d, "NOVNORMALSFLIP",
				PyInt_FromLong( ME_NOPUNOFLIP ) );
		PyConstant_Insert( d, "TWOSIDED", PyInt_FromLong( ME_TWOSIDED ) );
		PyConstant_Insert( d, "AUTOSMOOTH", 
				PyInt_FromLong( ME_AUTOSMOOTH ) );
	}

	return Modes;
}

/* Set constants for face drawing mode -- see drawmesh.c */

static PyObject *M_Mesh_FaceModesDict( void )
{
	PyObject *FM = PyConstant_New(  );

	if( FM ) {
		BPy_constant *d = ( BPy_constant * ) FM;

		PyConstant_Insert( d, "BILLBOARD",
				 PyInt_FromLong( TF_BILLBOARD2 ) );
		PyConstant_Insert( d, "ALL", PyInt_FromLong( 0xffff ) );
		PyConstant_Insert( d, "HALO", PyInt_FromLong( TF_BILLBOARD ) );
		PyConstant_Insert( d, "DYNAMIC", PyInt_FromLong( TF_DYNAMIC ) );
		PyConstant_Insert( d, "INVISIBLE", PyInt_FromLong( TF_INVISIBLE ) );
		PyConstant_Insert( d, "LIGHT", PyInt_FromLong( TF_LIGHT ) );
		PyConstant_Insert( d, "OBCOL", PyInt_FromLong( TF_OBCOL ) );
		PyConstant_Insert( d, "SHADOW", PyInt_FromLong( TF_SHADOW ) );
		PyConstant_Insert( d, "TEXT", PyInt_FromLong( TF_BMFONT ) );
		PyConstant_Insert( d, "SHAREDVERT", PyInt_FromLong( TF_SHAREDVERT ) );
		PyConstant_Insert( d, "SHAREDCOL", PyInt_FromLong( TF_SHAREDCOL ) );
		PyConstant_Insert( d, "TEX", PyInt_FromLong( TF_TEX ) );
		PyConstant_Insert( d, "TILES", PyInt_FromLong( TF_TILES ) );
		PyConstant_Insert( d, "TWOSIDE", PyInt_FromLong( TF_TWOSIDE ) );
	}

	return FM;
}

static PyObject *M_Mesh_FaceFlagsDict( void )
{
	PyObject *FF = PyConstant_New(  );

	if( FF ) {
		BPy_constant *d = ( BPy_constant * ) FF;

		PyConstant_Insert( d, "SELECT", PyInt_FromLong( TF_SELECT ) );
		PyConstant_Insert( d, "HIDE", PyInt_FromLong( TF_HIDE ) );
		PyConstant_Insert( d, "ACTIVE", PyInt_FromLong( TF_ACTIVE ) ); /* deprecated */
	}

	return FF;
}

static PyObject *M_Mesh_FaceTranspModesDict( void )
{
	PyObject *FTM = PyConstant_New(  );

	if( FTM ) {
		BPy_constant *d = ( BPy_constant * ) FTM;

		PyConstant_Insert( d, "SOLID", PyInt_FromLong( TF_SOLID ) );
		PyConstant_Insert( d, "ADD", PyInt_FromLong( TF_ADD ) );
		PyConstant_Insert( d, "ALPHA", PyInt_FromLong( TF_ALPHA ) );
		PyConstant_Insert( d, "SUB", PyInt_FromLong( TF_SUB ) );
	}

	return FTM;
}

static PyObject *M_Mesh_EdgeFlagsDict( void )
{
	PyObject *EF = PyConstant_New(  );

	if( EF ) {
		BPy_constant *d = ( BPy_constant * ) EF;

		PyConstant_Insert(d, "SELECT", PyInt_FromLong( SELECT ) );
		PyConstant_Insert(d, "EDGEDRAW", PyInt_FromLong( ME_EDGEDRAW ) );
		PyConstant_Insert(d, "EDGERENDER", PyInt_FromLong( ME_EDGERENDER ) );
		PyConstant_Insert(d, "SEAM", PyInt_FromLong( ME_SEAM ) );
		PyConstant_Insert(d, "FGON", PyInt_FromLong( ME_FGON ) );
		PyConstant_Insert(d, "LOOSE", PyInt_FromLong( ME_LOOSEEDGE ) );
		PyConstant_Insert(d, "SHARP", PyInt_FromLong( ME_SHARP ) );
	}

	return EF;
}

static PyObject *M_Mesh_VertAssignDict( void )
{
	PyObject *Vert = PyConstant_New(  );
	if( Vert ) {
		BPy_constant *d = ( BPy_constant * ) Vert;
		PyConstant_Insert(d, "ADD", PyInt_FromLong(WEIGHT_ADD));
		PyConstant_Insert(d, "REPLACE", PyInt_FromLong(WEIGHT_REPLACE));
		PyConstant_Insert(d, "SUBTRACT", PyInt_FromLong(WEIGHT_SUBTRACT));
	}
	return Vert;
}


static PyObject *M_Mesh_SelectModeDict( void )
{
	PyObject *Mode = PyConstant_New(  );
	if( Mode ) {
		BPy_constant *d = ( BPy_constant * ) Mode;
		PyConstant_Insert(d, "VERTEX", PyInt_FromLong(SCE_SELECT_VERTEX));
		PyConstant_Insert(d, "EDGE", PyInt_FromLong(SCE_SELECT_EDGE));
		PyConstant_Insert(d, "FACE", PyInt_FromLong(SCE_SELECT_FACE));
	}
	return Mode;
}

static char M_Mesh_doc[] = "The Blender.Mesh submodule";

PyObject *Mesh_Init( void )
{
	PyObject *submodule;

	PyObject *Modes = M_Mesh_ModesDict( );
	PyObject *FaceFlags = M_Mesh_FaceFlagsDict( );
	PyObject *FaceModes = M_Mesh_FaceModesDict( );
	PyObject *FaceTranspModes = M_Mesh_FaceTranspModesDict( );
	PyObject *EdgeFlags = M_Mesh_EdgeFlagsDict(  );
	PyObject *AssignModes = M_Mesh_VertAssignDict( );
	PyObject *SelectModes = M_Mesh_SelectModeDict( );
	PyObject *PropertyTypes = M_Mesh_PropertiesTypeDict( );
	
	if( PyType_Ready( &MCol_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MVert_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &PVert_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MVertSeq_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MEdge_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MEdgeSeq_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MFace_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MFaceSeq_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &Mesh_Type ) < 0 )
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Mesh", M_Mesh_methods, M_Mesh_doc );
	PyDict_SetItemString( PyModule_GetDict( submodule ),
			"Primitives", MeshPrimitives_Init( ) );

	if( Modes )
		PyModule_AddObject( submodule, "Modes", Modes );
	if( FaceFlags )
		PyModule_AddObject( submodule, "FaceFlags", FaceFlags );
	if( FaceModes )
		PyModule_AddObject( submodule, "FaceModes", FaceModes );
	if( FaceTranspModes )
		PyModule_AddObject( submodule, "FaceTranspModes",
				    FaceTranspModes );
	if( EdgeFlags )
		PyModule_AddObject( submodule, "EdgeFlags", EdgeFlags );
	if( AssignModes )
		PyModule_AddObject( submodule, "AssignModes", AssignModes );
	if( SelectModes )
		PyModule_AddObject( submodule, "SelectModes", SelectModes );
	if( PropertyTypes )
		PyModule_AddObject( submodule, "PropertyTypes", PropertyTypes );



	return submodule;
}

/* These are needed by Object.c */

PyObject *Mesh_CreatePyObject( Mesh * me, Object *obj )
{
	BPy_Mesh *nmesh = PyObject_NEW( BPy_Mesh, &Mesh_Type );

	if( !nmesh )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"couldn't create BPy_Mesh object" );

	nmesh->mesh = me;
	nmesh->object = obj;
	nmesh->new = 0;
	G.totmesh++;

	return ( PyObject * ) nmesh;
}


Mesh *Mesh_FromPyObject( PyObject * pyobj, Object *obj )
{
	BPy_Mesh *blen_obj;

	blen_obj = ( BPy_Mesh * ) pyobj;
	if (obj)
		blen_obj->object = obj;

	return blen_obj->mesh;

}
