/* 
 * $Id$
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
#include "BIF_editview.h"
#include "BIF_editmesh.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_mball.h"
#include "BKE_utildefines.h"
#include "BKE_depsgraph.h"
#include "BSE_edit.h"		/* for countall(); */
#include "BKE_curve.h"		/* for copy_curve(); */
#include "BKE_modifier.h"	/* for modifier_new(), modifier_copyData(); */

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "blendef.h"
#include "mydevice.h"
#include "butspace.h"		/* for mesh tools */
#include "Object.h"
#include "Key.h"
#include "Image.h"
#include "Material.h"
#include "Mathutils.h"
#include "constant.h"
#include "gen_utils.h"

#define MESH_TOOLS			/* add access to mesh tools */

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

#define MESH_TOOL_TOSPHERE             0
#define MESH_TOOL_VERTEXSMOOTH         1
#define MESH_TOOL_FLIPNORM             2
#define MESH_TOOL_SUBDIV               3
#define MESH_TOOL_REMDOUB              4
#define MESH_TOOL_FILL                 5
#define MESH_TOOL_RECALCNORM           6
#define MESH_TOOL_TRI2QUAD             7
#define MESH_TOOL_QUAD2TRI             8

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

static void delete_dverts( Mesh *mesh, unsigned int *vert_table, int to_delete )
{
	unsigned int *tmpvert;
	int i;
	char state;
	MDeformVert *newvert, *srcvert, *dstvert;
	int count;

	newvert = (MDeformVert *)MEM_mallocN(
			sizeof( MDeformVert )*( mesh->totvert-to_delete ), "MDeformVerts" );

	/*
	 * do "smart compaction" of the table; find and copy groups of vertices
	 * which are not being deleted
	 */

	dstvert = newvert;
	srcvert = mesh->dvert;
	tmpvert = vert_table;
	count = 0;
	state = 1;
	for( i = 0; i < mesh->totvert; ++i, ++tmpvert ) {
		switch( state ) {
		case 0:		/* skipping verts */
			if( *tmpvert == UINT_MAX )
				++count;
			else {
				srcvert = mesh->dvert + i;
				count = 1;
				state = 1;
			}
			break;
		case 1:		/* gathering verts */
			if( *tmpvert != UINT_MAX ) {
				++count;
			} else {
				if( count ) {
					memcpy( dstvert, srcvert, sizeof( MDeformVert ) * count );
					dstvert += count;
				}
				count = 1;
				state = 0;
			}
		}
		if( !state && mesh->dvert[i].dw )
			MEM_freeN( mesh->dvert[i].dw );
	}

	/* if we were gathering verts at the end of the loop, copy those */
	if( state && count )
		memcpy( dstvert, srcvert, sizeof( MDeformVert ) * count );

	/* delete old vertex list, install the new one */

	MEM_freeN( mesh->dvert );
	mesh->dvert = newvert;
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
	int i;
	char state;
	MVert *newvert, *srcvert, *dstvert;
	int count;


	/* is there are deformed verts also, delete them first */
	if( mesh->dvert )
		delete_dverts( mesh, vert_table, to_delete );

	newvert = (MVert *)MEM_mallocN(
			sizeof( MVert )*( mesh->totvert-to_delete ), "MVerts" );

	/*
	 * do "smart compaction" of the table; find and copy groups of vertices
	 * which are not being deleted
	 */

	dstvert = newvert;
	srcvert = mesh->mvert;
	tmpvert = vert_table;
	count = 0;
	state = 1;
	for( i = 0; i < mesh->totvert; ++i, ++tmpvert ) {
		switch( state ) {
		case 0:		/* skipping verts */
			if( *tmpvert == UINT_MAX ) {
				++count;
			} else {
				srcvert = mesh->mvert + i;
				count = 1;
				state = 1;
			}
			break;
		case 1:		/* gathering verts */
			if( *tmpvert != UINT_MAX ) {
				++count;
			} else {
				if( count ) {
					memcpy( dstvert, srcvert, sizeof( MVert ) * count );
					dstvert += count;
				}
				count = 1;
				state = 0;
			}
		}
	}

	/* if we were gathering verts at the end of the loop, copy those */
	if( state && count )
		memcpy( dstvert, srcvert, sizeof( MVert ) * count );

	/* delete old vertex list, install the new one, update vertex count */

	MEM_freeN( mesh->mvert );
	mesh->mvert = newvert;
	mesh->totvert -= to_delete;
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
		MEdge *newedge, *srcedge, *dstedge;
		int count, state;

	/* allocate new edge list and populate */
		newedge = (MEdge *)MEM_mallocN(
				sizeof( MEdge )*( mesh->totedge-to_delete ), "MEdges" );

	/*
	 * do "smart compaction" of the edges; find and copy groups of edges
	 * which are not being deleted
	 */

		dstedge = newedge;
		srcedge = mesh->medge;
		tmpedge = srcedge;
		count = 0;
		state = 1;
		for( i = 0; i < mesh->totedge; ++i, ++tmpedge ) {
			switch( state ) {
			case 0:		/* skipping edges */
				if( tmpedge->v1 == UINT_MAX ) {
					++count;
				} else {
					srcedge = tmpedge;
					count = 1;
					state = 1;
				}
				break;
			case 1:		/* gathering edges */
				if( tmpedge->v1 != UINT_MAX ) {
					++count;
				} else {
					if( count ) {
						memcpy( dstedge, srcedge, sizeof( MEdge ) * count );
						dstedge += count;
					}
					count = 1;
					state = 0;
				}
			}
		/* if edge is good, update vertex indices */
		}

	/* copy any pending good edges */
		if( state && count )
			memcpy( dstedge, srcedge, sizeof( MEdge ) * count );

	/* delete old vertex list, install the new one, update vertex count */
		MEM_freeN( mesh->medge );
		mesh->medge = newedge;
		mesh->totedge -= to_delete;
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

static void eeek_fix( MFace *mface, TFace *tface, int len4 )
{
	/* if 4 verts, then neither v3 nor v4 can be zero */
	if( len4 ) {
		if( !mface->v3 || !mface->v4 ) {
			SWAP( int, mface->v1, mface->v3 );
			SWAP( int, mface->v2, mface->v4 );
			if( tface ) {
				SWAP( float, tface->uv[0][0], tface->uv[2][0] );
				SWAP( float, tface->uv[0][1], tface->uv[2][1] );
				SWAP( float, tface->uv[1][0], tface->uv[3][0] );
				SWAP( float, tface->uv[1][1], tface->uv[3][1] );
				SWAP( unsigned int, tface->col[0], tface->col[2] );
				SWAP( unsigned int, tface->col[1], tface->col[3] );
			}
		}
	} else if( !mface->v3 ) {
	/* if 2 verts, then just v3 cannot be zero (v4 MUST be zero) */
		SWAP( int, mface->v1, mface->v2 );
		SWAP( int, mface->v2, mface->v3 );
		if( tface ) {
			SWAP( float, tface->uv[0][0], tface->uv[1][0] );
			SWAP( float, tface->uv[0][1], tface->uv[1][1] );
			SWAP( float, tface->uv[2][0], tface->uv[1][0] );
			SWAP( float, tface->uv[2][1], tface->uv[1][1] );
			SWAP( unsigned int, tface->col[0], tface->col[1] );
			SWAP( unsigned int, tface->col[1], tface->col[2] );
		}
	}
}

static void delete_faces( Mesh *mesh, unsigned int *vert_table, int to_delete )
{
	int i;
	MFace *tmpface;
	TFace *tmptface;

		/* if there are faces to delete, handle it */
	if( to_delete ) {
		MFace *newface, *srcface, *dstface;
		TFace *newtface = NULL, *srctface, *dsttface;
		char state;
		int count;

		newface = (MFace *)MEM_mallocN( ( mesh->totface-to_delete )
				* sizeof( MFace ), "MFace" );
		if( mesh->tface )
			newtface = (TFace *)MEM_mallocN( ( mesh->totface-to_delete )
					* sizeof( TFace ), "TFace" );

		/*
		 * do "smart compaction" of the faces; find and copy groups of faces
		 * which are not being deleted
		 */

		dstface = newface;
		srcface = mesh->mface;
		tmpface = srcface;
		dsttface = newtface;
		srctface = mesh->tface;
		tmptface = srctface;

		count = 0;
		state = 1;
		for( i = 0; i < mesh->totface; ++i ) {
			switch( state ) {
			case 0:		/* skipping faces */
				if( tmpface->v1 == UINT_MAX ) {
					++count;
				} else {
					srcface = tmpface;
					srctface = tmptface;
					count = 1;
					state = 1;
				}
				break;
			case 1:		/* gathering faces */
				if( tmpface->v1 != UINT_MAX ) {
					++count;
				} else {
					if( count ) {
						memcpy( dstface, srcface, sizeof( MFace ) * count );
						dstface += count;
						if( newtface ) {
							memcpy( dsttface, srctface, sizeof( TFace )
									* count );
							dsttface += count;
						}
					}
					count = 1;
					state = 0;
				}
			}
			++tmpface; 
			++tmptface; 
		}

	/* if we were gathering faces at the end of the loop, copy those */
		if ( state && count ) {
			memcpy( dstface, srcface, sizeof( MFace ) * count );
			if( newtface )
				memcpy( dsttface, srctface, sizeof( TFace ) * count );
		}

	/* delete old face list, install the new one, update face count */

		MEM_freeN( mesh->mface );
		mesh->mface = newface;
		mesh->totface -= to_delete;
		if( newtface ) {
			MEM_freeN( mesh->tface );
			mesh->tface = newtface;
		}
	}

	/* if vertices were deleted, update face's vertices */
	if( vert_table ) {
		tmpface = mesh->mface;
		tmptface = mesh->tface;
		for( i = mesh->totface; i--; ) {
			int len4 = tmpface->v4;
			tmpface->v1 = vert_table[tmpface->v1];
			tmpface->v2 = vert_table[tmpface->v2];
			tmpface->v3 = vert_table[tmpface->v3];
			tmpface->v4 = vert_table[tmpface->v4];

			eeek_fix( tmpface, tmptface, len4 );

			++tmpface;
			if( mesh->tface )
				++tmptface;
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
	PyObject *attr;

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

	attr = PyInt_FromLong( param );
	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed"); 
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

/************************************************************************
 *
 * Python MCol_Type methods
 *
 ************************************************************************/

static void MCol_dealloc( BPy_MCol * self )
{
	PyObject_DEL( self );
}

static PyObject *MCol_repr( BPy_MCol * self )
{
	return PyString_FromFormat( "[MCol %d %d %d %d]",
			(int)self->color->r, (int)self->color->g, 
			(int)self->color->b, (int)self->color->a ); 
}

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

	( destructor ) MCol_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) MCol_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
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
	PyObject *attr;

	if( self->index >= ((Mesh *)self->data)->totvert )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"MVert is no longer valid" );

	attr = PyInt_FromLong( self->index );
	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
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
		return NULL;

	for( i = 0; i < 3; ++i )
		no[i] = (float)(v->no[i] / 32767.0);
	return newVectorObject( no, 3, Py_NEW );
}

/*
 * get a vertex's select status
 */

static PyObject *MVert_getSel( BPy_MVert *self )
{
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return NULL;

	return EXPP_getBitfield( &v->flag, SELECT, 'b' );
}

/*
 * set a vertex's select status
 */

static int MVert_setSel( BPy_MVert *self, PyObject *value )
{
	MVert *v;

	v = MVert_get_pointer( self );
	if( !v )
		return -1;

	return EXPP_setBitfield( value, &v->flag, SELECT, 'b' );
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
	 (getter)MVert_getNormal, (setter)NULL,
	 "vertex's normal",
	 NULL},
	{"sel",
	 (getter)MVert_getSel, (setter)MVert_setSel,
	 "vertex's select status",
	 NULL},
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
	 (getter)MVert_getNormal, (setter)NULL,
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
	NULL,                       /* struct PyMethodDef *tp_methods; */
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
};

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
	// mesh_update( self->mesh );
	return 0;
};

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
	// mesh_update( self->mesh );
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
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MVert.
 */

static PyObject *MVertSeq_nextIter( BPy_MVertSeq * self )
{
	if( self->iter == self->mesh->totvert )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

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
	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( !VectorObject_Check ( tmp ) ) {
			if( !PySequence_Check ( tmp ) )
				return EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a sequence of tuple triplets" );
			args = tmp;
		}
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 3:		/* take any three args and put into a tuple */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PyTuple_Check( tmp ) ) {
			Py_INCREF( args );
			break;
		}
		args = Py_BuildValue( "((OOO))", tmp,
				PyTuple_GET_ITEM( args, 1 ), PyTuple_GET_ITEM( args, 2 ) );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple triplets" );
	}

	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF ( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	newlen = mesh->totvert + len;
	newvert = MEM_callocN( sizeof( MVert )*newlen, "MVerts" );

	/* scan the input list and insert the new vertices */

	tmpvert = &newvert[mesh->totvert];
	for( i = 0; i < len; ++i ) {
		float co[3];
		tmp = PySequence_GetItem( args, i );
		if( VectorObject_Check( tmp ) ) {
			if( ((VectorObject *)tmp)->size != 3 ) {
				MEM_freeN( newvert );
				Py_DECREF ( tmp );
				Py_DECREF ( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected vector of size 3" );
			}
			for( j = 0; j < 3; ++j )
				co[j] = ((VectorObject *)tmp)->vec[j];
		} else if( PyTuple_Check( tmp ) ) {
			int ok=1;
			PyObject *flt;
			if( PyTuple_Size( tmp ) != 3 )
				ok = 0;
			else	
				for( j = 0; ok && j < 3; ++j ) {
					flt = PyTuple_GET_ITEM( tmp, j );
					if( !PyNumber_Check ( flt ) )
						ok = 0;
					else
						co[j] = (float)PyFloat_AsDouble( flt );
				}

			if( !ok ) {
				MEM_freeN( newvert );
				Py_DECREF ( args );
				Py_DECREF ( tmp );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected tuple triplet of floats" );
			}
		}
		Py_DECREF ( tmp );

	/* add the coordinate to the new list */
		memcpy( tmpvert->co, co, sizeof(co) );

	/* TODO: anything else which needs to be done when we add a vert? */
	/* probably not: NMesh's newvert() doesn't */
		++tmpvert;
	}

	/*
	 * if we got here we've added all the new verts, so just copy the old
	 * verts over and we're done
	 */

	if( mesh->mvert ) {
		memcpy( newvert, mesh->mvert, mesh->totvert*sizeof(MVert) );
		MEM_freeN( mesh->mvert );
	}
	mesh->mvert = newvert;

	/*
	 * maybe not quite done; if there are keys, have to fix those lists up
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

	/*
	 * if there are vertex groups, also have to fix them
	 */

	if( mesh->dvert ) {
		MDeformVert *newdvert;
		newdvert = MEM_callocN( sizeof(MDeformVert)*newlen , "mesh defVert" );
		memcpy( newdvert, mesh->dvert, sizeof(MDeformVert)*mesh->totvert );
		MEM_freeN( mesh->dvert );
		mesh->dvert = newdvert;
	}

	/* set final vertex list size */
	mesh->totvert = newlen;

	mesh_update( mesh );

	Py_DECREF ( args );
	return EXPP_incr_ret( Py_None );
}

static PyObject *MVertSeq_delete( BPy_MVertSeq * self, PyObject *args )
{
	unsigned int *vert_table;
	int vert_delete, face_count;
	int i;
	Mesh *mesh = self->mesh;
	MFace *tmpface;

	Py_INCREF( args );		/* so we can safely DECREF later */

	/* accept a sequence (lists or tuples) also */
	if( PySequence_Size( args ) == 1 ) {
		PyObject *tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check ( tmp ) ) {
			Py_DECREF( args );		/* release previous reference */
			args = tmp;				/* PyTuple_GET_ITEM returns new ref */
		}
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
				Py_DECREF( args );
				Py_DECREF( tmp );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
						"MVert belongs to a different mesh" );
			}
			index = ((BPy_MVert*)tmp)->index;
		}
		else if( PyInt_CheckExact( tmp ) )
			index = PyInt_AsLong ( tmp );
		else {
			MEM_freeN( vert_table );
			Py_DECREF( args );
			Py_DECREF( tmp );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of ints or MVerts" );
		}
		Py_DECREF( tmp );
		if( index < 0 || index >= mesh->totvert ) {
			MEM_freeN( vert_table );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
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
				vert_table[tmpface->v4] == UINT_MAX ) {
			tmpface->v1 = UINT_MAX;
			++face_count;
		}
	}
	delete_faces( mesh, vert_table, face_count );

	/* clean up and exit */
	MEM_freeN( vert_table );
	mesh_update ( mesh );
	Py_DECREF( args );
	return EXPP_incr_ret( Py_None );
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

static struct PyMethodDef BPy_MVertSeq_methods[] = {
	{"extend", (PyCFunction)MVertSeq_extend, METH_VARARGS,
		"add vertices to mesh"},
	{"delete", (PyCFunction)MVertSeq_delete, METH_VARARGS,
		"delete vertices from mesh"},
	{"selected", (PyCFunction)MVertSeq_selected, METH_NOARGS,
		"returns a list containing indices of selected vertices"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MVertSeq_Type standard operations
 *
 ************************************************************************/

static void MVertSeq_dealloc( BPy_MVertSeq * self )
{
	PyObject_DEL( self );
}

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

	( destructor ) MVertSeq_dealloc,/* destructor tp_dealloc; */
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
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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
	PyObject *attr;
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL;

	attr = PyInt_FromLong( edge->crease );
	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
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
	PyObject *attr;
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return NULL;

	attr = PyInt_FromLong( edge->flag );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set an edge's flag
 */

static int MEdge_setFlag( BPy_MEdge * self, PyObject * value )
{
	short param;
	static short bitmask = 1 		/* 1=select */
				| ME_EDGEDRAW
				| ME_EDGERENDER
				| ME_SEAM
				| ME_FGON;
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return -1;

	if( !PyInt_CheckExact ( value ) ) {
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
		return NULL;

	return MVert_CreatePyObject( self->mesh, edge->v2 );
}

/*
 * set an edge's second vertex
 */

static int MEdge_setV2( BPy_MEdge * self, BPy_MVert * value )
{
	MEdge *edge = MEdge_get_pointer( self );

	if( !edge )
		return -1;
	if( !BPy_MVert_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected an MVert" );

	edge->v2 = value->index;
	return 0;
}

/*
 * get an edges's index
 */

static PyObject *MEdge_getIndex( BPy_MEdge * self )
{
	PyObject *attr;

	if( !MEdge_get_pointer( self ) )
		return NULL;

	attr = PyInt_FromLong( self->index );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
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
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MVert.  Throw an exception after the second vertex.
 */

static PyObject *MEdge_nextIter( BPy_MEdge * self )
{
	if( self->iter == 2 )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

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

static void MEdge_dealloc( BPy_MEdge * self )
{
	PyObject_DEL( self );
}

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

	( destructor ) MEdge_dealloc,/* destructor tp_dealloc; */
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
	NULL,                       /* struct PyMethodDef *tp_methods; */
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
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MEdge.
 */

static PyObject *MEdgeSeq_nextIter( BPy_MEdgeSeq * self )
{
	if( self->iter == self->mesh->totedge )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

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
	int i, j;
	int new_edge_count, good_edges;
	SrchEdges *oldpair, *newpair, *tmppair, *tmppair2;
	PyObject *tmp;
	BPy_MVert *e[4];
	MEdge *tmpedge;
	Mesh *mesh = self->mesh;

	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		args = PyTuple_GET_ITEM( args, 0 );
		if( !PySequence_Check ( args ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of tuple pairs" );
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 2:	
	case 3:
	case 4:		/* two to four args may be individual verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PyTuple_Check( tmp ) ) {/* maybe just tuples, so use args as-is */
			Py_INCREF( args );		/* so we can safely DECREF later */
			break;
		}
		args = Py_BuildValue( "(O)", args );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple pairs" );
	}

	/* make sure there is something to add */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	/* verify the param list and get a total count of number of edges */
	new_edge_count = 0;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_GetItem( args, i );

		/* not a tuple of MVerts... error */
		if( !PyTuple_Check( tmp ) ||
				EXPP_check_sequence_consistency( tmp, &MVert_Type ) != 1 ) {
			Py_DECREF( tmp );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected sequence of MVert tuples" );
		}

		/* not the right number of MVerts... error */
		nverts = PyTuple_Size( tmp );
		if( nverts < 2 || nverts > 4 ) {
			Py_DECREF( tmp );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected 2 to 4 MVerts per tuple" );
		}
		Py_DECREF( tmp );

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
	for( i = 0; i < len; ++i ) {
		int edge_count;
		tmp = PySequence_GetItem( args, i );
		nverts = PyTuple_Size( tmp );

		/* get copies of vertices */
		for(j = 0; j < nverts; ++j )
			e[j] = (BPy_MVert *)PyTuple_GET_ITEM( tmp, j );
		Py_DECREF( tmp );

		if( nverts == 2 )
			edge_count = 1;	 /* again, two verts give just one edge */
		else
			edge_count = nverts;	

		/* now add the edges to the search list */
		for(j = 0; j < edge_count; ++j ) {
			int k = j+1;
			if( k == nverts )	/* final edge */ 
				k = 0;

			/* sort verts into search list, abort if two are the same */
			if( e[j]->index < e[k]->index ) {
				tmppair->v[0] = e[j]->index;
				tmppair->v[1] = e[k]->index;
				tmppair->swap = 0;
			} else if( e[j]->index > e[k]->index ) {
				tmppair->v[0] = e[k]->index;
				tmppair->v[1] = e[j]->index;
				tmppair->swap = 1;
			} else {
				MEM_freeN( newpair );
				Py_DECREF( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
						"tuple contains duplicate vertices" );
			}
			tmppair++;
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
		int totedge = mesh->totedge+good_edges;	/* new edge count */

	/* allocate new edge list */
		tmpedge = MEM_callocN(totedge*sizeof(MEdge), "Mesh_addEdges");

	/* if we're appending, copy the old edge list and delete it */
		if( mesh->medge ) {
			memcpy( tmpedge, mesh->medge, mesh->totedge*sizeof(MEdge));
			MEM_freeN( mesh->medge );
		}
		mesh->medge = tmpedge;		/* point to the new edge list */

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
				tmpedge->flag = ME_EDGEDRAW | ME_EDGERENDER;
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
	return EXPP_incr_ret( Py_None );
}

static PyObject *MEdgeSeq_delete( BPy_MEdgeSeq * self, PyObject *args )
{
	Mesh *mesh = self->mesh;
	MEdge *srcedge;
	MFace *srcface;
	unsigned int *vert_table, *del_table, *edge_table;
	int i, len;
	int face_count, edge_count, vert_count;

	Py_INCREF( args );		/* so we can safely DECREF later */

	/* accept a sequence (lists or tuples) also */
	if( PySequence_Size( args ) == 1 ) {
		PyObject *tmp = PyTuple_GET_ITEM( args, 0 );
		if( PySequence_Check ( tmp ) ) {
			Py_DECREF( args );		/* release previous reference */
			args = tmp;				/* PyTuple_GET_ITEM returns new ref */
		}
	}

	/* see how many args we need to parse */
	len = PySequence_Size( args );
	edge_table = (unsigned int *)MEM_callocN( len*sizeof( unsigned int ),
			"edge_table" );

	/* get the indices of edges to be removed */
	for( i = len; i--; ) {
		PyObject *tmp = PySequence_GetItem( args, i );
		if( BPy_MEdge_Check( tmp ) )
			edge_table[i] = ((BPy_MEdge *)tmp)->index;
		else if( PyInt_CheckExact( tmp ) )
			edge_table[i] = PyInt_AsLong ( tmp );
		else {
			MEM_freeN( edge_table );
			Py_DECREF( tmp );
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of ints or MEdges" );
		}
		Py_DECREF( tmp );

		/* if index out-of-range, throw exception */
		if( edge_table[i] >= (unsigned int)mesh->totedge ) {
			MEM_freeN( edge_table );
			Py_DECREF( args );
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
	Py_DECREF( args );
	mesh_update ( mesh );
	return EXPP_incr_ret( Py_None );
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

static struct PyMethodDef BPy_MEdgeSeq_methods[] = {
	{"extend", (PyCFunction)MEdgeSeq_extend, METH_VARARGS,
		"add edges to mesh"},
	{"delete", (PyCFunction)MEdgeSeq_delete, METH_VARARGS,
		"delete edges from mesh"},
	{"selected", (PyCFunction)MEdgeSeq_selected, METH_NOARGS,
		"returns a list containing indices of selected edges"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MEdgeSeq_Type standard operators
 *
 ************************************************************************/

static void MEdgeSeq_dealloc( BPy_MEdgeSeq * self )
{
	PyObject_DEL( self );
}

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

	( destructor ) MEdgeSeq_dealloc,/* destructor tp_dealloc; */
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
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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
	PyObject *attr;
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

	attr = PyInt_FromLong( face->mat_nr );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's material index
 */

static int MFace_setMat( BPy_MFace * self, PyObject * value )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return -1;

	return EXPP_setIValueRange( value, &face->mat_nr, 0, 15, 'b' );
}

/*
 * get a face's index
 */

static PyObject *MFace_getIndex( BPy_MFace * self )
{
	PyObject *attr;
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

	attr = PyInt_FromLong( self->index );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * get face's normal index
 */

static PyObject *MFace_getNormal( BPy_MFace * self )
{
	float *vert[4];
	float no[3];
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

	if( (int)face->v1 >= self->mesh->totvert ||
			(int)face->v2 >= self->mesh->totvert ||
			(int)face->v3 >= self->mesh->totvert ||
			(int)face->v4 >= self->mesh->totvert )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"one or more MFace vertices are no longer valid" );

	vert[0] = self->mesh->mvert[face->v1].co;
	vert[1] = self->mesh->mvert[face->v2].co;
	vert[2] = self->mesh->mvert[face->v3].co;
	vert[3] = self->mesh->mvert[face->v4].co;
	if( face->v4 )
		CalcNormFloat4( vert[0], vert[1], vert[2], vert[3], no );
	else
		CalcNormFloat( vert[0], vert[1], vert[2], no );

	return newVectorObject( no, 3, Py_NEW );
}

/*
 * get one of a face's mface flag bits
 */

static PyObject *MFace_getMFlagBits( BPy_MFace * self, void * type )
{
	MFace *face = MFace_get_pointer( self );

	if( !face )
		return NULL;

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
		return -1;

	return EXPP_setBitfield( value, &face->flag, (int)((long)type & 0xff), 'b' );
}

/*
 * get face's texture image
 */

static PyObject *MFace_getImage( BPy_MFace *self )
{
	TFace *face;
	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	face = &self->mesh->tface[self->index];

	if( face->tpage )
		return Image_CreatePyObject( face->tpage );
	else
		return EXPP_incr_ret( Py_None );
}

/*
 * change or clear face's texture image
 */

static int MFace_setImage( BPy_MFace *self, PyObject *value )
{
	TFace *face;
	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	face = &self->mesh->tface[self->index];
    if( value == Py_None )
        face->tpage = NULL;		/* should memory be freed? */
    else {
        if( !BPy_Image_Check( value ) )
            return EXPP_ReturnIntError( PyExc_TypeError,
					"expected image object" );
        face->tpage = ( ( BPy_Image * ) value )->image;
    }

    return 0;
}

/*
 * get face's texture flag
 */

static PyObject *MFace_getFlag( BPy_MFace *self )
{
	PyObject *attr;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	attr = PyInt_FromLong( self->mesh->tface[self->index].flag );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's texture flag
 */

static int MFace_setFlag( BPy_MFace *self, PyObject *value )
{
	int param;
	static short bitmask = TF_SELECT | TF_HIDE;

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PyInt_CheckExact ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );

	/* only one face can be active, so don't allow that here */

	if( ( param & bitmask ) == TF_ACTIVE )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"cannot make a face active; use 'activeFace' attribute" );
	
	if( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	/* merge active setting with other new params */
	param |= (self->mesh->tface[self->index].flag & TF_ACTIVE);
	self->mesh->tface[self->index].flag = (char)param;

	return 0;
}

/*
 * get face's texture mode
 */

static PyObject *MFace_getMode( BPy_MFace *self )
{
	PyObject *attr;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	attr = PyInt_FromLong( self->mesh->tface[self->index].mode );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
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

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PyInt_CheckExact ( value ) ) {
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

	self->mesh->tface[self->index].mode = (short)param;

	return 0;
}

/*
 * get face's texture transparency setting
 */

static PyObject *MFace_getTransp( BPy_MFace *self )
{
	PyObject *attr;
	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	attr = PyInt_FromLong( self->mesh->tface[self->index].transp );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's texture transparency setting
 */

static int MFace_setTransp( BPy_MFace *self, PyObject *value )
{
	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	return EXPP_setIValueRange( value,
			&self->mesh->tface[self->index].transp, TF_SOLID, TF_SUB, 'b' );
}

/*
 * get a face's texture UV coord values
 */

static PyObject *MFace_getUV( BPy_MFace * self )
{
	TFace *face;
	PyObject *attr;
	int length, i;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	face = &self->mesh->tface[self->index];
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
	TFace *face;
	int length, i;

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PyTuple_Check( value ) ||
			EXPP_check_sequence_consistency( value, &vector_Type ) != 1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "expected tuple of vectors" );

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	if( length != PyTuple_Size( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "size of vertex and UV lists differ" );

	face = &self->mesh->tface[self->index];
	for( i=0; i<length; ++i ) {
		VectorObject *vector = (VectorObject *)PyTuple_GET_ITEM( value, i );
		face->uv[i][0] = vector->vec[0];
		face->uv[i][1] = vector->vec[1];
	}
	return 0;
}

/*
 * get a face's texture UV coord select state
 */

static PyObject *MFace_getUVSel( BPy_MFace * self )
{
	TFace *face;
	PyObject *attr;
	int length, i, mask;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	face = &self->mesh->tface[self->index];
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
	TFace *face;
	int length, i, mask;

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !MFace_get_pointer( self ) )
		return -1;

	if( !PySequence_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a tuple of integers" );

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	if( length != PyTuple_Size( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "size of vertex and UV lists differ" );

	/* set coord select state, one bit at a time */
	face = &self->mesh->tface[self->index];
	mask = TF_SEL1;
	for( i=0; i<length; ++i, mask <<= 1 ) {
		PyObject *tmp = PyTuple_GET_ITEM( value, i );
		if( !PyInt_CheckExact( tmp ) ) {
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected a tuple of integers" );
		}
		if( PyInt_AsLong( tmp ) )
			face->flag |= mask;
		else
			face->flag &= ~mask;
	}
	return 0;
}

/*
 * get a face's vertex colors. note that if mesh->tfaces is defined, then 
 * it takes precedent over mesh->mcol
 */

static PyObject *MFace_getCol( BPy_MFace * self )
{
	PyObject *attr;
	int length, i;
	MCol * mcol;

	/* if there's no mesh color vectors or texture faces, nothing to do */

	if( !self->mesh->mcol && !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no vertex colors" );

	if( !MFace_get_pointer( self ) )
		return NULL;

	if( self->mesh->tface )
		mcol = (MCol *) self->mesh->tface[self->index].col;
	else
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

    {"hide",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face hidden in edit mode",
     (void *)ME_HIDE},
    {"sel",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face selected in edit mode",
     (void *)ME_FACE_SEL},
    {"smooth",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face smooth enabled",
     (void *)ME_SMOOTH},

	/* attributes for texture faces (mostly, I think) */

    {"col",
     (getter)MFace_getCol, (setter)NULL,
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
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MVert.  Throw an exception after the final vertex.
 */

static PyObject *MFace_nextIter( BPy_MFace * self )
{
	struct MFace *face = &self->mesh->mface[self->index];
	int len = self->mesh->mface[self->index].v4 ? 4 : 3;

	if( self->iter == len )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

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

static void MFace_dealloc( BPy_MFace * self )
{
	PyObject_DEL( self );
}

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

	( destructor ) MFace_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MFace_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MFace_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
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
	NULL,                       /* struct PyMethodDef *tp_methods; */
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
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MFace.
 */

static PyObject *MFaceSeq_nextIter( BPy_MFaceSeq * self )
{
	if( self->iter == self->mesh->totface )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

	return MFace_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MFaceSeq_Type methods
 *
 ************************************************************************/

static PyObject *MFaceSeq_extend( BPy_MEdgeSeq * self, PyObject *args )
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

	/*
	 * before we try to add faces, add edges; if it fails; exit
	 */

	tmp = MEdgeSeq_extend( self, args );
	if( !tmp )
		return NULL;
	
	Py_DECREF( tmp );

	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		args = PyTuple_GET_ITEM( args, 0 );
		if( !PySequence_Check ( args ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of tuple pairs" );
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 2:	
	case 3:
	case 4:		/* two to four args may be individual verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PyTuple_Check( tmp ) ) {/* maybe just tuples, so use args as-is */
			Py_INCREF( args );		/* so we can safely DECREF later */
			break;
		}
		args = Py_BuildValue( "(O)", args );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple pairs" );
	}

	/* make sure there is something to add */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	/* verify the param list and get a total count of number of edges */
	new_face_count = 0;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_GetItem( args, i );

		/* not a tuple of MVerts... error */
		if( !PyTuple_Check( tmp ) ||
				EXPP_check_sequence_consistency( tmp, &MVert_Type ) != 1 ) {
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected sequence of MVert tuples" );
		}

		/* not the right number of MVerts... error */
		nverts = PyTuple_Size( tmp );
		if( nverts < 2 || nverts > 4 ) {
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected 2 to 4 MVerts per tuple" );
		}

		if( nverts != 2 )		/* new faces cannot have only 2 verts */
			++new_face_count;
	}

	/* OK, commit to allocating the search structures */
	newpair = (SrchFaces *)MEM_callocN( sizeof(SrchFaces)*new_face_count,
			"MFacePairs" );

	/* scan the input list and build the new face pair list */
	len = PySequence_Size( args );
	tmppair = newpair;
	for( i = 0; i < len; ++i ) {
		BPy_MVert *e;
		MFace tmpface;
		unsigned int vert[4]={0,0,0,0};
		unsigned char order[4]={0,1,2,3};
		tmp = PySequence_GetItem( args, i );
		nverts = PyTuple_Size( tmp );

		if( nverts == 2 )	/* again, ignore 2-vert tuples */
			break;

		/*
		 * go through some contortions to guarantee the third and fourth
		 * vertices are not index 0
		 */

		e = (BPy_MVert *)PyTuple_GET_ITEM( tmp, 0 );
		tmpface.v1 = e->index;
		e = (BPy_MVert *)PyTuple_GET_ITEM( tmp, 1 );
		tmpface.v2 = e->index;
		e = (BPy_MVert *)PyTuple_GET_ITEM( tmp, 2 );
		tmpface.v3 = e->index;
		if( nverts == 4 ) {
			e = (BPy_MVert *)PyTuple_GET_ITEM( tmp, 3 );
			tmpface.v4 = e->index;
		}
		Py_DECREF( tmp );

		eeek_fix( &tmpface, NULL, nverts==4 );
		vert[0] = tmpface.v1;
		vert[1] = tmpface.v2;
		vert[2] = tmpface.v3;
		if( nverts == 3 )
			tmppair->v[3] = 0;
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
					MEM_freeN( newpair );
					Py_DECREF( args );
					return EXPP_ReturnPyObjError( PyExc_ValueError,
						"tuple contains duplicate vertices" );
				}
			}
			tmppair->v[j] = vert[j];
		}

		/* pack order into a byte */
		tmppair->order = order[0]|(order[1]<<2)|(order[2]<<4)|(order[3]<<6);
		++tmppair;
	}

	/* sort the new face pairs */
	qsort( newpair, new_face_count, sizeof(SrchFaces), mface_comp );

	/*
	 * find duplicates in the new list and mark.  if it's a duplicate,
	 * then mark by setting second vert index to 0 (a real edge won't have
	 * second vert index of 0 since verts are sorted)
	 */

	good_faces = new_face_count;	/* assume all faces good to start */

	tmppair = newpair;	/* "last good edge" */
	tmppair2 = &tmppair[1];	/* "current candidate edge" */
	for( i = 0; i < new_face_count; ++i ) {
		if( mface_comp( tmppair, tmppair2 ) )
			tmppair = tmppair2;	/* last != current, so current == last */
		else {
			tmppair2->v[1] = 0; /* last == current, so mark as duplicate */
			--good_faces;		/* one less good face */
		}
		tmppair2++;
	}

	/* if mesh has faces, see if any of the new faces are already in it */
	if( mesh->totface ) {
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
		for( i = good_faces; i-- ; ) {
			if( tmppair->v[1] ) {
				if( bsearch( tmppair, oldpair, mesh->totface, 
						sizeof(SrchFaces), mface_comp ) ) {
					tmppair->v[1] = 0;	/* mark as duplicate */
					--good_faces;
				} 
			}
			tmppair++;
		}
		MEM_freeN( oldpair );
	}

	/* if any new faces are left, add to list */
	if( good_faces ) {
		int totface = mesh->totface+good_faces;	/* new face count */

	/* if mesh has tfaces, reallocate them first */
		if( mesh->tface ) {
			TFace *tmptface;

			tmptface = MEM_callocN(totface*sizeof(TFace), "Mesh_addFaces");
			memcpy( tmptface, mesh->tface, mesh->totface*sizeof(TFace));
			MEM_freeN( mesh->tface );
			mesh->tface = tmptface;
		}

	/* allocate new face list */
		tmpface = MEM_callocN(totface*sizeof(MFace), "Mesh_addFaces");

	/* if we're appending, copy the old face list and delete it */
		if( mesh->mface ) {
			memcpy( tmpface, mesh->mface, mesh->totface*sizeof(MFace));
			MEM_freeN( mesh->mface );
		}
		mesh->mface = tmpface;		/* point to the new face list */

	/* point to the first face we're going to add */
		tmpface = &mesh->mface[mesh->totface];
		tmppair = newpair;

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

				tmpface->flag = 0;
				mesh->totface++;
				++tmpface;
				--good_faces;
			}
			tmppair++;
		}
	}

	/* clean up and leave */
	mesh_update( mesh );
	Py_DECREF ( args );
	MEM_freeN( newpair );
	return EXPP_incr_ret( Py_None );
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
   
	/* see how many args we need to parse */
	len = PySequence_Size( args );
	if( len < 1 )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"sequence must contain at least one int or MFace" );

	face_table = (unsigned int *)MEM_callocN( len*sizeof( unsigned int ),
			"face_table" );

	/* get the indices of faces to be removed */
	for( i = len; i--; ) {
		PyObject *tmp = PySequence_GetItem( args, i );
		if( BPy_MFace_Check( tmp ) )
			face_table[i] = ((BPy_MFace *)tmp)->index;
		else if( PyInt_CheckExact( tmp ) )
			face_table[i] = PyInt_AsLong ( tmp );
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
			if(len == 4)
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

		/* for each face, deselect each edge */
		tmpface = mesh->mface;
		fface = (struct fourEdges *)face_edges;
		for( i = mesh->totface; i--; ++tmpface, ++fface ) {
			if( tmpface->v1 != UINT_MAX ) {
				FaceEdges (*face)[4];
				face = (void *)face_edges;
				face += face_table[i];
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
	return EXPP_incr_ret( Py_None );
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

static struct PyMethodDef BPy_MFaceSeq_methods[] = {
	{"extend", (PyCFunction)MFaceSeq_extend, METH_VARARGS,
		"add faces to mesh"},
	{"delete", (PyCFunction)MFaceSeq_delete, METH_VARARGS,
		"delete faces from mesh"},
	{"selected", (PyCFunction)MFaceSeq_selected, METH_NOARGS,
		"returns a list containing indices of selected faces"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MFaceSeq_Type standard operations
 *
 ************************************************************************/

static void MFaceSeq_dealloc( BPy_MFaceSeq * self )
{
	PyObject_DEL( self );
}

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

	( destructor ) MFaceSeq_dealloc,/* destructor tp_dealloc; */
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
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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
	return EXPP_incr_ret( Py_None );
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
			make_vertexcol();
			countall();
			return EXPP_incr_ret( Py_None );
		}
		base = base->next;
	}
	return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"object not found in baselist!" );
}

/*
 * force display list update
 */

static PyObject *Mesh_Update( BPy_Mesh * self )
{
	mesh_update( self->mesh );
	return EXPP_incr_ret( Py_None );
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
	return EXPP_incr_ret( Py_None );
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
		} else if( PyInt_CheckExact( v1 ) && PyInt_CheckExact( v2 ) ) {
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
	Object *ob;
	char *name;
	ID tmpid;
	Mesh *tmpmesh;
	Curve *tmpcu = NULL;
	DispListMesh *dlm;
	DerivedMesh *dm;
	Object *tmpobj = NULL;
	int cage = 0, i;

	if( !PyArg_ParseTuple( args, "s|i", &name, &cage ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected string and optional integer arguments" );

	if( cage != 0 && cage != 1 )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"cage value must be 0 or 1" );

	/* find the specified object */
	ob = ( Object * ) GetIdFromList( &( G.main->object ), name );
	if( !ob )
		return EXPP_ReturnPyObjError( PyExc_AttributeError, name );

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
		tmpmesh = tmpobj->data;
		free_libblock_us( &G.main->object, tmpobj );
 		break;
 	case OB_MBALL:
		/* metaballs don't have modifiers, so just convert to mesh */
		ob = find_basis_mball( ob );
		tmpmesh = add_mesh();
		mball_to_mesh( &ob->disp, tmpmesh );
 		break;
 	case OB_MESH:
		/* copies object and modifiers (but not the data) */
		tmpobj= copy_object( ob );
		tmpmesh = tmpobj->data;
		tmpmesh->id.us--;

		/* copies the data */
		tmpobj->data = copy_mesh( tmpmesh );
		G.totmesh++;
		tmpmesh = tmpobj->data;

		/* if not getting the original caged mesh, get final derived mesh */
		if( !cage ) {
			dm = mesh_create_derived_render( tmpobj );
			dlm = dm->convertToDispListMesh( dm, 0 );
			displistmesh_to_mesh( dlm, tmpmesh );
			dm->release( dm );
		}
		
		
		/* take control of mesh before object is freed */
		tmpobj->data = NULL;
		free_libblock_us( &G.main->object, tmpobj );
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
				self->mesh->mat[i] = tmpcu->mat[i];
				if (self->mesh->mat[i]) {
					tmpmesh->mat[i]->id.us++;
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
		self->mesh->totcol = tmpmesh->totcol;		
		if( tmpmesh->mat ) {
			for( i = tmpmesh->totcol; i-- > 0; ) {
				self->mesh->mat[i] = tmpmesh->mat[i];
				/* user count dosent need to change */
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
	return EXPP_incr_ret( Py_None );
}

/*
 * apply a transform to the mesh's vertices
 *
 * WARNING: unlike NMesh, this method ALWAYS changes the original mesh
 */

static PyObject *Mesh_transform( BPy_Mesh *self, PyObject *args )
{
	Mesh *mesh = self->mesh;
	MVert *mv;
	PyObject *ob1 = NULL;
	MatrixObject *mat;
	int i, recalc_normals = 0;

	if( !PyArg_ParseTuple( args, "O!|i", &matrix_Type, &ob1, &recalc_normals ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected matrix and optionally an int as arguments" ) );

	mat = ( MatrixObject * ) ob1;

	if( mat->colSize != 4 || mat->rowSize != 4 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"matrix must be a 4x4 transformation matrix\n"
				"for example as returned by object.getMatrix()" );
	
	/* loop through all the verts and transform by the supplied matrix */
	mv = mesh->mvert;
	for( i = 0; i < mesh->totvert; i++, mv++ )
		Mat4MulVecfl( (float(*)[4])*mat->matrix, mv->co );

	if( recalc_normals ) {
		/* loop through all the verts and transform normals by the inverse
		 * of the transpose of the supplied matrix */
		float invmat[4][4];

		/*
		 * we only need to invert a 3x3 submatrix, because the 4th component of
		 * affine vectors is 0, but Mat4Invert reports non invertible matrices
		 */

		if (!Mat4Invert((float(*)[4])*invmat, (float(*)[4])*mat->matrix))
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"given matrix is not invertible");

		/*
		 * since normal is stored as shorts, convert to float 
		 */

		mv = mesh->mvert;
		for( i = 0; i < mesh->totvert; i++, mv++ ) {
			float vec[3];
			vec[0] = (float)(mv->no[0] / 32767.0);
			vec[1] = (float)(mv->no[1] / 32767.0);
			vec[2] = (float)(mv->no[2] / 32767.0);
			Mat4MulVecfl( (float(*)[4])*invmat, vec );
			Normalise( vec );
			mv->no[0] = (short)(vec[0] * 32767.0);
			mv->no[1] = (short)(vec[1] * 32767.0);
			mv->no[2] = (short)(vec[2] * 32767.0);
		}
	}

	return EXPP_incr_ret( Py_None );
}

static PyObject *Mesh_addVertGroup( PyObject * self, PyObject * args )
{
	char *groupStr;
	struct Object *object;
	PyObject *tempStr;

	if( !PyArg_ParseTuple( args, "s", &groupStr ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	if( ( ( BPy_Mesh * ) self )->object == NULL )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "mesh not linked to an object" );

	object = ( ( BPy_Mesh * ) self )->object;

	//get clamped name
	tempStr = PyString_FromStringAndSize( groupStr, 32 );
	groupStr = PyString_AsString( tempStr );

	add_defgroup_name( object, groupStr );

	EXPP_allqueue( REDRAWBUTSALL, 1 );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Mesh_removeVertGroup( PyObject * self, PyObject * args )
{
	char *groupStr;
	struct Object *object;
	int nIndex;
	bDeformGroup *pGroup;

	if( !PyArg_ParseTuple( args, "s", &groupStr ) )
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

	del_defgroup( object );

	EXPP_allqueue( REDRAWBUTSALL, 1 );

	return EXPP_incr_ret( Py_None );
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
					      "expected string, list,	float, string arguments" );
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
		create_dverts( mesh );

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

	return EXPP_incr_ret( Py_None );
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
	G.obedit = 0;
	exit_editmode( 1 );

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

	return EXPP_incr_ret( Py_None );
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
	PyObject *tempVertexList;

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

	/* temporary list */
	tempVertexList = PyList_New( mesh->totvert );
	if( !tempVertexList )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "getVertsFromGroup: can't create pylist!" );

	count = 0;

	if( !listObject ) {	/* do entire group */
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
					PyList_SetItem( tempVertexList, count, attr );
					count++;
				}
			}
		}
	} else {			/* do individual vertices */
		for( i = 0; i < PyList_Size( listObject ); i++ ) {
			PyObject *attr = NULL;

			if( !PyArg_Parse( PyList_GetItem( listObject, i ), "i", &num ) )
				return EXPP_ReturnPyObjError( PyExc_TypeError,
							      "python list integer not parseable" );

			if( num < 0 || num >= mesh->totvert )
				return EXPP_ReturnPyObjError( PyExc_ValueError,
							      "bad vertex index in list" );

			dvert = mesh->dvert + num;
			for( k = 0; k < dvert->totweight; k++ ) {
				if( dvert->dw[k].def_nr == nIndex ) {
					if( weightRet )
						attr = Py_BuildValue( "(i,f)", num,
								dvert->dw[k].weight );
					else
						attr = PyInt_FromLong ( num );
					PyList_SetItem( tempVertexList, count, attr );
					count++;
				}
			}
			if( !attr )
				return EXPP_ReturnPyObjError( PyExc_ValueError,
							      "specified index not in vertex group" );
		}
	}
	/* only return what we need */
	vertexList = PyList_GetSlice( tempVertexList, 0, count );

	Py_DECREF( tempVertexList );

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

	return EXPP_incr_ret( Py_None );
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

#ifdef MESH_TOOLS

static PyObject *Mesh_Tools( BPy_Mesh * self, int type, void **args )
{
	Base *base, *basact;
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

	/* save active object for later, make mesh's object active */
	basact = BASACT;
	BASACT = base;

	/* enter mesh edit mode, apply subdivide, then exit edit mode */
	enter_editmode( );
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
		result = removedoublesflag( 1, *((float *)args[0]) );

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
	exit_editmode( 1 );
	BASACT = basact;
	if( attr )
		return attr;

	return EXPP_incr_ret( Py_None );
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

#endif

static struct PyMethodDef BPy_Mesh_methods[] = {
	{"calcNormals", (PyCFunction)Mesh_calcNormals, METH_NOARGS,
		"all recalculate vertex normals"},
	{"vertexShade", (PyCFunction)Mesh_vertexShade, METH_VARARGS,
		"color vertices based on the current lighting setup"},
	{"findEdges", (PyCFunction)Mesh_findEdges, METH_VARARGS,
		"find indices of an multiple edges in the mesh"},
	{"getFromObject", (PyCFunction)Mesh_getFromObject, METH_VARARGS,
		"Get a mesh by name"},
	{"update", (PyCFunction)Mesh_Update, METH_NOARGS,
		"Update display lists after changes to mesh"},
	{"transform", (PyCFunction)Mesh_transform, METH_VARARGS,
		"Applies a transformation matrix to mesh's vertices"},
	{"addVertGroup", (PyCFunction)Mesh_addVertGroup, METH_VARARGS,
		"Assign vertex group name to the object linked to the mesh"},
	{"removeVertGroup", (PyCFunction)Mesh_removeVertGroup, METH_VARARGS,
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



#ifdef MESH_TOOLS
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
#endif
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python BPy_Mesh attributes
 *
 ************************************************************************/

static PyObject *Mesh_getVerts( BPy_Mesh * self )
{
	BPy_MVertSeq *seq = PyObject_NEW( BPy_MVertSeq, &MVertSeq_Type);
	seq->mesh = self->mesh;
	return (PyObject *)seq;
}

static int Mesh_setVerts( BPy_Mesh * self, PyObject * args )
{
	MVert *dst;
	MVert *src;
	int i;
	
	/* special case if None: delete the mesh */
	if( args == Py_None ) {
		Mesh *me = self->mesh;
		free_mesh( me );
        me->mvert = NULL; me->medge = NULL; me->mface = NULL;
		me->tface = NULL; me->dvert = NULL; me->mcol = NULL;
		me->msticky = NULL; me->mat = NULL; me->bb = NULL;
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

static PyObject *Mesh_getEdges( BPy_Mesh * self )
{
	BPy_MEdgeSeq *seq = PyObject_NEW( BPy_MEdgeSeq, &MEdgeSeq_Type);
	seq->mesh = self->mesh;
	return (PyObject *)seq;
}

static PyObject *Mesh_getFaces( BPy_Mesh * self )
{
	BPy_MFaceSeq *seq = PyObject_NEW( BPy_MFaceSeq, &MFaceSeq_Type);
	seq->mesh = self->mesh;
	return (PyObject *)seq;
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
    PyObject *attr = PyInt_FromLong( self->mesh->smoothresh );

    if( attr )
        return attr;

    return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

static int Mesh_setMaxSmoothAngle( BPy_Mesh *self, PyObject *value )
{
    return EXPP_setIValueClamped( value, &self->mesh->smoothresh,
                            MESH_SMOOTHRESH_MIN,
                            MESH_SMOOTHRESH_MAX, 'h' );
}

static PyObject *Mesh_getSubDivLevels( BPy_Mesh * self )
{
	PyObject *attr = Py_BuildValue( "(h,h)",
			self->mesh->subdiv, self->mesh->subdivr );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"Py_BuildValue() failed" );
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

static PyObject *Mesh_getName( BPy_Mesh * self )
{
	PyObject *attr = PyString_FromString( self->mesh->id.name + 2 );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Mesh.name attribute" );
}

static int Mesh_setName( BPy_Mesh * self, PyObject * value )
{
	char *name;
	char buf[21];

	name = PyString_AsString ( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->mesh->id, buf );

	return 0;
}

static PyObject *Mesh_getUsers( BPy_Mesh * self )
{
	PyObject *attr = PyInt_FromLong( self->mesh->id.us );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Mesh.users attribute" );
}

static PyObject *Mesh_getFlag( BPy_Mesh * self, void *type )
{
	PyObject *attr;

	switch( (long)type ) {
	case MESH_HASFACEUV:
		attr = self->mesh->tface ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
		break;
	case MESH_HASMCOL:
		attr = self->mesh->mcol ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
		break;
	case MESH_HASVERTUV:
		attr = self->mesh->msticky ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
		break;
	default:
		attr = NULL;
	}

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get attribute" );
}

static int Mesh_setFlag( BPy_Mesh * self, PyObject *value, void *type )
{
	int param, i;
	Mesh *mesh = self->mesh;

	if( !PyInt_CheckExact( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument in range [0,1]" );

	param = PyInt_AsLong( value );
	if( param != 0 && param != 1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument in range [0,1]" );

	/* sticky is independent of faceUV and vertUV */
	/* faceUV (tface) has priority over vertUV (mcol) */

	switch( (long)type ) {
	case MESH_HASFACEUV:
		if( !param ) {
			if( mesh->tface ) {
				MEM_freeN( mesh->tface );
				mesh->tface = NULL;
			}
		} else if( !mesh->tface )
			make_tfaces( mesh );
		return 0;
	case MESH_HASMCOL:
		if( !param ) {
			if( mesh->mcol ) {
				MEM_freeN( mesh->mcol );
				mesh->mcol = NULL;
			}
		} else if( !mesh->mcol ) {
				/* TODO: mesh_create_shadedColors */
			mesh->mcol = MEM_callocN( sizeof(unsigned int)*mesh->totface*4,
						"mcol" );
			for( i = 0; i < mesh->totface*4; i++ )
				mesh->mcol[i].a = 255;
			if( mesh->tface )
				mcol_to_tface( mesh, 1 );
		}
		return 0;
	case MESH_HASVERTUV:
		if( !param ) {
			if( mesh->msticky ) {
				MEM_freeN( mesh->msticky );
				mesh->msticky = NULL;
			}
		} else {
			if( !mesh->msticky ) {
				mesh->msticky= MEM_callocN( mesh->totvert*sizeof( MSticky ),
						"sticky" );
				/* TODO: rework RE_make_sticky() so we can calculate */
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
	PyObject *attr = PyInt_FromLong( self->mesh->flag );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Mesh.mode attribute" );
}

static int Mesh_setMode( BPy_Mesh *self, PyObject *value )
{
	short param;
	static short bitmask = ME_NOPUNOFLIP | ME_TWOSIDED | ME_AUTOSMOOTH;

	if( !PyInt_CheckExact ( value ) ) {
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

static PyObject *Mesh_getActiveFace( BPy_Mesh * self )
{
	TFace *face;
	int i, totface;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	face = self->mesh->tface;
	totface = self->mesh->totface;

	for( i = 0; i < totface; ++face, ++i )
		if( face->flag & TF_ACTIVE ) {
			PyObject *attr = PyInt_FromLong( i );

			if( attr )
				return attr;

			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"PyInt_FromLong() failed" );
		}

	return EXPP_incr_ret( Py_None );
}

static int Mesh_setActiveFace( BPy_Mesh * self, PyObject * value )
{
	TFace *face;
	int param;

	/* if no texture faces, error */

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	/* if param isn't an int, error */

	if( !PyInt_CheckExact( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int argument" );

	/* check for a valid index */

	param = PyInt_AsLong( value );
	if( param < 0 || param > self->mesh->totface )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"face index out of range" );

	face = self->mesh->tface;

	/* if requested face isn't already active, then inactivate all
	 * faces and activate the requested one */

	if( !( face[param].flag & TF_ACTIVE ) ) {
		int i;
		for( i = self->mesh->totface; i > 0; ++face, --i )
			face->flag &= ~TF_ACTIVE;
		self->mesh->tface[param].flag |= TF_ACTIVE;
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
	PyObject_DEL( self );
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
	{"name",
	 (getter)Mesh_getName, (setter)Mesh_setName,
	 "The mesh's data name",
	 NULL},
	{"mode",
	 (getter)Mesh_getMode, (setter)Mesh_setMode,
	 "The mesh's mode bitfield",
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
	{"activeFace",
	 (getter)Mesh_getActiveFace, (setter)Mesh_setActiveFace,
	 "Index of the mesh's active texture face (in UV editor)",
	 NULL},
	{"users",
	 (getter)Mesh_getUsers, (setter)NULL,
	 "Number of users of the mesh",
	 NULL},

	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Mesh_Type callback function prototypes:                           */
/*****************************************************************************/
static void Mesh_dealloc( BPy_Mesh * object );

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
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) Mesh_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
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

static PyObject *M_Mesh_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Mesh *mesh = NULL;
	BPy_Mesh* obj;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected zero or one string arguments" );

	if( name ) {
		mesh = ( Mesh * ) GetIdFromList( &( G.main->mesh ), name );

		if( !mesh )
			return EXPP_incr_ret( Py_None );

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

static PyObject *M_Mesh_New( PyObject * self, PyObject * args )
{
	char *name = "Mesh";
	Mesh *mesh;
	BPy_Mesh *obj;
	char buf[21];

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or a string as argument" );

	obj = (BPy_Mesh *)PyObject_NEW( BPy_Mesh, &Mesh_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "PyObject_New() failed" );

	mesh = add_mesh(); /* doesn't return NULL now, but might someday */

	if( !mesh ) {
		Py_DECREF ( obj );
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "FATAL: could not create mesh object" );
	}
	mesh->id.us = 0;
	G.totmesh++;

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );
	rename_id( &mesh->id, buf );

	obj->mesh = mesh;
	obj->object = NULL;
	return (PyObject *)obj;
}

/*
 * creates a new MVert for users to manipulate
 */

static PyObject *M_Mesh_MVert( PyObject * self, PyObject * args )
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

static struct PyMethodDef M_Mesh_methods[] = {
	{"New", (PyCFunction)M_Mesh_New, METH_VARARGS,
		"Create a new mesh"},
	{"Get", (PyCFunction)M_Mesh_Get, METH_VARARGS,
		"Get a mesh by name"},
	{"MVert", (PyCFunction)M_Mesh_MVert, METH_VARARGS,
		"Create a new MVert"},
	{NULL, NULL, 0, NULL},
};

static PyObject *M_Mesh_Modes( void )
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
		PyConstant_Insert( d, "ACTIVE", PyInt_FromLong( TF_ACTIVE ) );
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

static char M_Mesh_doc[] = "The Blender.Mesh submodule";

PyObject *Mesh_Init( void )
{
	PyObject *submodule;

	PyObject *Modes = M_Mesh_Modes(  );
	PyObject *FaceFlags = M_Mesh_FaceFlagsDict(  );
	PyObject *FaceModes = M_Mesh_FaceModesDict(  );
	PyObject *FaceTranspModes = M_Mesh_FaceTranspModesDict(  );
	PyObject *EdgeFlags = M_Mesh_EdgeFlagsDict(  );
	PyObject *AssignModes = M_Mesh_VertAssignDict(  );

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

	return ( PyObject * ) nmesh;
}

int Mesh_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Mesh_Type );
}

Mesh *Mesh_FromPyObject( PyObject * pyobj, Object *obj )
{
	BPy_Mesh *blen_obj;

	blen_obj = ( BPy_Mesh * ) pyobj;
	blen_obj->object = obj;
	return blen_obj->mesh;

}
