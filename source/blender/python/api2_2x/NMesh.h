/**
 *
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
 * Contributor(s): Willian P. Germano, Jordi Rovira i Bonnet, Joseph Gilbert.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* Most of this file comes from opy_nmesh.[ch] in the old bpython dir */

#ifndef EXPP_NMESH_H
#define EXPP_NMESH_H

#include "Python.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_armature_types.h"

#include "BDR_editface.h" /* make_tfaces */
#include "BIF_editdeform.h"
#include "BIF_editkey.h" /* insert_meshkey */
#include "BIF_editmesh.h" /* vertexnormals_mesh() */
#include "BIF_space.h"
#include "BKE_mesh.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_screen.h"
#include "BKE_object.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MEM_guardedalloc.h"

#include "blendef.h"
#include "mydevice.h"

#include "Material.h"
#include "Image.h"
#include "vector.h"
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"

/* EXPP PyType Objects */
PyTypeObject NMesh_Type;
PyTypeObject NMFace_Type;
PyTypeObject NMVert_Type;
PyTypeObject NMCol_Type;

extern PyTypeObject Image_Type;

struct BPy_Object;

/* These are from blender/src/editdeform.c, should be declared elsewhere,
 * maybe in BIF_editdeform.h, after proper testing of vgrouping methods XXX */
void create_dverts (Mesh *me);
void add_vert_defnr (Object *ob, int def_nr, int vertnum, float weight,
								int assignmode);
void remove_vert_def_nr (Object *ob, int def_nr, int vertnum);

/* Globals */
static PyObject *g_nmeshmodule = NULL;

/* Type checking for EXPP PyTypes */
#define BPy_NMesh_Check(v)       ((v)->ob_type == &NMesh_Type)
#define BPy_NMFace_Check(v)      ((v)->ob_type == &NMFace_Type)
#define BPy_NMVert_Check(v)      ((v)->ob_type == &NMVert_Type)
#define BPy_NMCol_Check(v)       ((v)->ob_type == &NMCol_Type)


static char NMesh_addVertGroup_doc[] =
"add a named and empty vertex(deform) Group to a mesh that has been linked\n\
to an object. ";

static char NMesh_removeVertGroup_doc[] =
"remove a named vertex(deform) Group from a mesh that has been linked\n\
to an object.  Will remove all verts assigned to group.";

static char NMesh_assignVertsToGroup_doc[] =
"Adds an array (a python list) of vertex points (by index) to a named\n\
vertex group.  The list will have an associated wieght assigned to them.\n\
The weight represents the amount of influence this group has over these\n\
vertex points. Weights should be in the range of 0.0 - 1.0.\n\
The assignmode can be either 'add', 'subtract', or 'replace'.  If this vertex\n\
is not assigned to the group 'add' creates a new association with the weight\n\
specified, otherwise the weight given is added to the current weight of the\n\
vertex.\n\
'subtract' will attempt to subtract the weight passed from a vertex already\n\
associated with a group, else it does nothing. 'replace' attempts to replace\n\
the weight with the new weight value for an already associated vertex/group,\n\
else it does nothing. The mesh must have all it's vertex points set before\n\
attempting to assign any vertex points to a vertex group.";

static char NMesh_removeVertsFromGroup_doc[] =
"Remove an array (a python list) of vertex points from a named group in a\n\
mesh that has been linked to an object. If no list is given this will remove\n\
all vertex point associations with the group passed";

static char NMesh_getVertsFromGroup_doc[] =
"By passing a python list of vertex indices and a named group, this will\n\
return a python list representing the indeces that are a part of this vertex.\n\
group. If no association was found for the index passed nothing will be\n\
return for the index. An optional flag will also return the weights as well";

static char M_NMesh_doc[] =
"The Blender.NMesh submodule";

static char M_NMesh_Col_doc[]=
"([r, g, b, a]) - Get a new mesh color\n\n\
[r=255, g=255, b=255, a=255] Specify the color components";

static char M_NMesh_Face_doc[] =
"(vertexlist = None) - Get a new face, and pass optional vertex list";

static char NMFace_append_doc[] =
"(vert) - appends Vertex 'vert' to face vertex list";

static char M_NMesh_Vert_doc[] =
"([x, y, z]) - Get a new vertice\n\n\
[x, y, z] Specify new coordinates";

static char NMesh_addMaterial_doc[] =
"(material) - add a new Blender Material 'material' to this Mesh's materials\n\
list.";

static char NMesh_insertKey_doc[] =
"(frame = None, type = 'relative') - inserts a Mesh key at the given frame\n\
if called without arguments, it inserts the key at the current Scene frame.\n\
(type) - 'relative' or 'absolute'.  Only relevant on the first call to this\n\
function for each nmesh.";

static char NMesh_removeAllKeys_doc[] =
"() - removes all keys from this mesh\n\
returns True if successful or False if this NMesh wasn't linked to a real\n\
Blender Mesh yet or the Mesh had no keys";

static char NMesh_getSelectedFaces_doc[] =
"(flag = None) - returns list of selected Faces\n\
If flag = 1, return indices instead";

static char NMesh_getActiveFace_doc[] =
"returns the index of the active face ";

static char NMesh_hasVertexUV_doc[] =
"(flag = None) - returns 1 if Mesh has per vertex UVs ('Sticky')\n\
The optional argument sets the Sticky flag";

static char NMesh_hasFaceUV_doc[] =
"(flag = None) - returns 1 if Mesh has textured faces\n\
The optional argument sets the textured faces flag";

static char NMesh_hasVertexColours_doc[] =
"(flag = None) - returns 1 if Mesh has vertex colours.\n\
The optional argument sets the vertex colour flag";

static char NMesh_getVertexInfluences_doc[] =
"Return a list of the influences of bones in the vertex \n\
specified by index. The list contains pairs with the \n\
bone name and the weight.";


static char NMesh_update_doc[] = "(recalc_normals = 0) - updates the Mesh.\n\
if recalc_normals is given and is equal to 1, normal vectors are recalculated.";

static char NMesh_getMode_doc[] =
"() - get the mode flags of this nmesh as an or'ed int value.";

static char NMesh_setMode_doc[] =
"(none to 5 strings) - set the mode flags of this nmesh.\n\
() - unset all flags.";

static char NMesh_getMaxSmoothAngle_doc[] =
"() - get the max smooth angle for mesh auto smoothing.";

static char NMesh_setMaxSmoothAngle_doc[] =
"(int) - set the max smooth angle for mesh auto smoothing in the range\n\
[1,80] in degrees.";

static char NMesh_getSubDivLevels_doc[] =
"() - get the subdivision levels for display and rendering: [display, render]";

static char NMesh_setSubDivLevels_doc[] =
"([int, int]) - set the subdivision levels for [display, render] -- they are\n\
clamped to the range [1,6].";

static char M_NMesh_New_doc[] =
"() - returns a new, empty NMesh mesh object\n";

static char M_NMesh_GetRaw_doc[] =
"([name]) - Get a raw mesh from Blender\n\n\
[name] Name of the mesh to be returned\n\n\
If name is not specified a new empty mesh is\n\
returned, otherwise Blender returns an existing\n\
mesh.";

static char M_NMesh_GetRawFromObject_doc[] =
"(name) - Get the raw mesh used by a Blender object\n\n\
(name) Name of the object to get the mesh from\n\n\
This returns the mesh as used by the object, which\n\
means it contains all deformations and modifications.";

static char M_NMesh_PutRaw_doc[] =
"(mesh, [name, renormal]) - Return a raw mesh to Blender\n\n\
(mesh) The NMesh object to store\n\
[name] The mesh to replace\n\
[renormal=1] Flag to control vertex normal recalculation\n\n\
If the name of a mesh to replace is not given a new\n\
object is created and returned.";

/* Typedefs for the new types */

typedef struct {
	PyObject_HEAD
	unsigned char r, g, b, a;

} BPy_NMCol; /* an NMesh color: [r,g,b,a] */

typedef struct {
	PyObject_VAR_HEAD
	float co[3];
	float no[3];
	float uvco[3];
	int index;

} BPy_NMVert; /* an NMesh vertex */

typedef struct {
	PyObject_HEAD
	PyObject *v;
	PyObject *uv;
	PyObject *col;
	short mode;
	short flag;
	unsigned char transp;
	Image *image;
	char mat_nr, smooth;

} BPy_NMFace; /* an NMesh face */

typedef struct {
	PyObject_HEAD
	Mesh *mesh;
  Object *object; /* for vertex grouping info, since it's stored on the object */
	PyObject *name;
	PyObject *materials;
	PyObject *verts;
	PyObject *faces;
  int sel_face; /*@ XXX remove */
	short smoothresh; /* max AutoSmooth angle */
	short subdiv[2]; /* SubDiv Levels: display and rendering */
	short mode; /* see the EXPP_NMESH_* defines in the beginning of this file */
	char flags;

#define NMESH_HASMCOL	1<<0
#define NMESH_HASVERTUV	1<<1
#define NMESH_HASFACEUV	1<<2

} BPy_NMesh;

/* PROTOS */
extern void test_object_materials(ID *id); /* declared in BKE_material.h */
static int unlink_existingMeshData(Mesh *mesh);
static int convert_NMeshToMesh(Mesh *mesh, BPy_NMesh *nmesh);
void mesh_update(Mesh *mesh);
PyObject *new_NMesh(Mesh *oldmesh);
Mesh *Mesh_fromNMesh(BPy_NMesh *nmesh);
PyObject *NMesh_assignMaterials_toObject(BPy_NMesh *nmesh, Object *ob);
Material **nmesh_updateMaterials(BPy_NMesh *nmesh);
Material **newMaterialList_fromPyList (PyObject *list);
void mesh_update(Mesh *mesh);
static PyObject *NMesh_addVertGroup(PyObject *self, PyObject *args);
static PyObject *NMesh_removeVertGroup(PyObject *self, PyObject *args);
static PyObject *NMesh_assignVertsToGroup(PyObject *self, PyObject *args);
static PyObject *NMesh_removeVertsFromGroup(PyObject *self, PyObject *args);
static PyObject *NMesh_getVertsFromGroup(PyObject *self, PyObject *args);


#endif /* EXPP_NMESH_H */
