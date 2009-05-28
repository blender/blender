#include "bmesh.h"
#include "bmesh_private.h"

#include <stdio.h>

/*loads a bmesh into an object*/
BMOpDefine def_object_load_bmesh = {
	"object_load_bmesh",
	/*pointer to a mesh struct*/
	{{BMOP_OPSLOT_PNT, "scene"},
	{BMOP_OPSLOT_PNT, "object"},
	{0, /*null-terminating sentinel*/}},
	bmesh_to_mesh_exec,
	0
};


BMOpDefine def_mesh_to_bmesh = {
	"mesh_to_bmesh",
	{{BMOP_OPSLOT_PNT, "me"},
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
	{BMOP_OPSLOT_INT, "flag"},
	{BMOP_OPSLOT_FLT, "radius"},
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
	&def_dissolvevertsop,
	&def_makefgonsop,
	&def_extrudefaceregion,
	&def_connectverts,
	//&def_makeprim,
	&def_extrudeverts_indiv,
	&def_mesh_to_bmesh,
	&def_object_load_bmesh,
};

int bmesh_total_ops = (sizeof(opdefines) / sizeof(void*));
