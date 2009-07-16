#include "bmesh.h"
#include "bmesh_private.h"

#include <stdio.h>

/*do not rename any operator or slot names! otherwise you must go 
  through the code and find all references to them!*/

BMOpDefine def_finddoubles = {
	"finddoubles",
	/*maps welded vertices to verts they should weld to.*/
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	 //list of verts to keep
	 {BMOP_OPSLOT_ELEMENT_BUF, "keepverts"},
	 {BMOP_OPSLOT_FLT,         "dist"},
	 {BMOP_OPSLOT_MAPPING, "targetmapout"},
	 {0, /*null-terminating sentinel*/}},
	bmesh_finddoubles_exec,
	0,
};

BMOpDefine def_removedoubles = {
	"removedoubles",
	/*maps welded vertices to verts they should weld to.*/
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	 {BMOP_OPSLOT_FLT,         "dist"},
	 {0, /*null-terminating sentinel*/}},
	bmesh_removedoubles_exec,
	0,
};

BMOpDefine def_weldverts = {
	"weldverts",
	/*maps welded vertices to verts they should weld to.*/
	{{BMOP_OPSLOT_MAPPING, "targetmap"},
	 {0, /*null-terminating sentinel*/}},
	bmesh_weldverts_exec,
	0,
};

BMOpDefine def_makevert = {
	"makevert",
	{{BMOP_OPSLOT_VEC, "co"},
	{BMOP_OPSLOT_ELEMENT_BUF, "newvertout"},
	{0, /*null-terminating sentinel*/}},
	bmesh_makevert_exec,
	0,
};

/*contextual_create is fkey, it creates
  new faces, makes stuff from edge nets,
  makes wire edges, etc.  it also dissolves
  faces.*/
BMOpDefine def_contextual_create= {
	"contextual_create",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"},
	 {BMOP_OPSLOT_ELEMENT_BUF, "faceout"},
	 {0, /*null-terminating sentinel*/}},
	bmesh_contextual_create_exec,
	0,
};

BMOpDefine def_edgenet_fill= {
	"edgenet_fill",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"},
	 {BMOP_OPSLOT_ELEMENT_BUF, "faceout"},
	{0, /*null-terminating sentinel*/}},
	bmesh_edgenet_fill_exec,
	0,
};

BMOpDefine def_rotate = {
	"rotate",
	{{BMOP_OPSLOT_VEC, "cent"},
	 {BMOP_OPSLOT_MAT, "mat"},
	{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	{0, /*null-terminating sentinel*/}},
	bmesh_rotate_exec,
	0,
};

BMOpDefine def_translate= {
	"translate",
	{{BMOP_OPSLOT_VEC, "vec"},
	{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	{0, /*null-terminating sentinel*/}},
	bmesh_translate_exec,
	0,
};


/*applies a transform to vertices*/
BMOpDefine def_transform = {
	"transform",
	{{BMOP_OPSLOT_MAT, "mat"},
	{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	{0, /*null-terminating sentinel*/}},
	bmesh_transform_exec,
	0,
};

/*loads a bmesh into an object*/
BMOpDefine def_object_load_bmesh = {
	"object_load_bmesh",
	{{BMOP_OPSLOT_PNT, "scene"},
	{BMOP_OPSLOT_PNT, "object"},
	{0, /*null-terminating sentinel*/}},
	bmesh_to_mesh_exec,
	0,
};


BMOpDefine def_mesh_to_bmesh = {
	"mesh_to_bmesh",
	{{BMOP_OPSLOT_PNT, "mesh"},
	 {0, /*null-terminating sentinel*/}},
	mesh_to_bmesh_exec,
	0
};

BMOpDefine def_extrudeverts_indiv = {
	"extrude_vert_indiv",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	{BMOP_OPSLOT_ELEMENT_BUF, "edgeout"},
	{BMOP_OPSLOT_ELEMENT_BUF, "vertout"},
	{0} /*null-terminating sentinel*/},
	extrude_vert_indiv_exec,
	0
};

#if 0
BMOpDefine def_makeprim = {
	"makeprim",
	{{BMOP_OPSLOT_INT, "type"},
	{BMOP_OPSLOT_INT, "tot", /*rows/cols also applies to spheres*/
	{BMOP_OPSLOT_INT, "seg",
	{BMOP_OPSLOT_INT, "subdiv"},
	{BMOP_OPSLOT_INT, "ext"},
	{BMOP_OPSLOT_INT, "fill"},
	{BMOP_OPSLOT_FLT, "dia"},
	{BMOP_OPSLOT_FLT, "depth"},
	{BMOP_OPSLOT_PNT, "mat"},
	{BMOP_OPSLOT_ELEMENT_BUF, "geomout"}, //won't be implemented right away
	{0}}
	makeprim_exec,
	0
};
#endif

BMOpDefine def_connectverts = {
	"connectverts",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	{BMOP_OPSLOT_ELEMENT_BUF, "edgeout"},
	{0} /*null-terminating sentinel*/},
	connectverts_exec,
	0
};

BMOpDefine def_extrudefaceregion = {
	"extrudefaceregion",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edgefacein"},
	{BMOP_OPSLOT_MAPPING, "exclude"},
	{BMOP_OPSLOT_ELEMENT_BUF, "geomout"},
	{0} /*null-terminating sentinel*/},
	extrude_edge_context_exec,
	0
};

BMOpDefine def_makefgonsop = {
	"makefgon",
	{{BMOP_OPSLOT_INT, "trifan"}, /*use triangle fans instead of 
				        real interpolation*/
	 {0} /*null-terminating sentinel*/},
	bmesh_make_fgons_exec,
	0
};

BMOpDefine def_dissolvevertsop = {
	"dissolveverts",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"},
	{0} /*null-terminating sentinel*/},
	dissolveverts_exec,
	0
};

BMOpDefine def_dissolveedgessop = {
	"dissolveedges",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"},
	{BMOP_OPSLOT_ELEMENT_BUF, "regionout"},
	{0} /*null-terminating sentinel*/},
	dissolveedges_exec,
	0
};

BMOpDefine def_dissolveedgeloopsop = {
	"dissolveedgeloop",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"},
	{BMOP_OPSLOT_ELEMENT_BUF, "regionout"},
	{0} /*null-terminating sentinel*/},
	dissolve_edgeloop_exec,
	0
};

BMOpDefine def_dissolvefacesop = {
	"dissolvefaces",
	{{BMOP_OPSLOT_ELEMENT_BUF, "faces"},
	{BMOP_OPSLOT_ELEMENT_BUF, "regionout"},
	{0} /*null-terminating sentinel*/},
	dissolvefaces_exec,
	0
};


BMOpDefine def_triangop = {
	"triangulate",
	{{BMOP_OPSLOT_ELEMENT_BUF, "faces"},
	{BMOP_OPSLOT_ELEMENT_BUF, "edgeout"},
	{BMOP_OPSLOT_ELEMENT_BUF, "faceout"},
	{BMOP_OPSLOT_MAPPING, "facemap"},
	{0} /*null-terminating sentinel*/},
	triangulate_exec,
	0
};

BMOpDefine def_subdop = {
	"esubd",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"},
	{BMOP_OPSLOT_INT, "numcuts"},
	{BMOP_OPSLOT_FLT, "smooth"},
	{BMOP_OPSLOT_FLT, "fractal"},
	{BMOP_OPSLOT_INT, "beauty"},
	{BMOP_OPSLOT_MAPPING, "custompatterns"},
	{BMOP_OPSLOT_MAPPING, "edgepercents"},
	
	/*these next two can have multiple types of elements in them.*/
	{BMOP_OPSLOT_ELEMENT_BUF, "outinner"},
	{BMOP_OPSLOT_ELEMENT_BUF, "outsplit"},
	{0} /*null-terminating sentinel*/,
	},
	esubdivide_exec,
	0
};

BMOpDefine def_edit2bmesh = {
	"editmesh_to_bmesh",
	{{BMOP_OPSLOT_PNT, "em"}, {BMOP_OPSLOT_MAPPING, "map"},
	{0} /*null-terminating sentinel*/},
	edit2bmesh_exec,
	0
};

BMOpDefine def_bmesh2edit = {
	"bmesh_to_editmesh",
	{{BMOP_OPSLOT_PNT, "emout"},
	{0} /*null-terminating sentinel*/},
	bmesh2edit_exec,
	0
};

BMOpDefine def_delop = {
	"del",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"}, {BMOP_OPSLOT_INT, "context"},
	{0} /*null-terminating sentinel*/},
	delop_exec,
	0
};

BMOpDefine def_dupeop = {
	"dupe",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"},
	{BMOP_OPSLOT_ELEMENT_BUF, "origout"},
	{BMOP_OPSLOT_ELEMENT_BUF, "newout"},
	/*facemap maps from source faces to dupe
	  faces, and from dupe faces to source faces.*/
	{BMOP_OPSLOT_MAPPING, "facemap"},
	{BMOP_OPSLOT_MAPPING, "boundarymap"},
	{BMOP_OPSLOT_MAPPING, "isovertmap"},
	{0} /*null-terminating sentinel*/},
	dupeop_exec,
	0
};

BMOpDefine def_splitop = {
	"split",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"},
	{BMOP_OPSLOT_ELEMENT_BUF, "geomout"},
	{BMOP_OPSLOT_MAPPING, "boundarymap"},
	{BMOP_OPSLOT_MAPPING, "isovertmap"},
	{0} /*null-terminating sentinel*/},
	splitop_exec,
	0
};

BMOpDefine *opdefines[] = {
	&def_splitop,
	&def_dupeop,
	&def_delop,
	&def_edit2bmesh,
	&def_bmesh2edit,
	&def_subdop,
	&def_triangop,
	&def_dissolvefacesop,
	&def_dissolveedgessop,
	&def_dissolveedgeloopsop,
	&def_dissolvevertsop,
	&def_makefgonsop,
	&def_extrudefaceregion,
	&def_connectverts,
	//&def_makeprim,
	&def_extrudeverts_indiv,
	&def_mesh_to_bmesh,
	&def_object_load_bmesh,
	&def_transform,
	&def_translate,
	&def_rotate,
	&def_edgenet_fill,
	&def_contextual_create,
	&def_makevert,
	&def_weldverts,
	&def_removedoubles,
	&def_finddoubles,
};

int bmesh_total_ops = (sizeof(opdefines) / sizeof(void*));
