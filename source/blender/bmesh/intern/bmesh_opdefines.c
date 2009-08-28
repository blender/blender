#include "bmesh.h"
#include "bmesh_private.h"
#include <stdio.h>

/*
This file defines (and documents) all bmesh operators (bmops).

Do not rename any operator or slot names! otherwise you must go 
through the code and find all references to them!

A word on slot names:

For geometry input slots, the following are valid names:
* verts
* edges
* faces
* edgefacein
* vertfacein
* vertedgein
* vertfacein
* geom

The basic rules are, for single-type geometry slots, use the plural of the
type name (e.g. edges).  for double-type slots, use the two type names plus
"in" (e.g. edgefacein).  for three-type slots, use geom.

for output slots, for single-type geometry slots, use the type name plus "out",
(e.g. vertout), for double-type slots, use the two type names plus "out",
(e.g. vertfaceout), for three-type slots, use geom.  note that you can also
use more esohteric names (e.g. skirtout) do long as the comment next to the
slot definition tells you what types of elements are in it.

*/

/*
ok, I'm going to write a little docgen script. so all
bmop comments must conform to the following template/rules:

template (py quotes used because nested comments don't work
on all C compilers):

"""
Region Extend.

paragraph1, Extends bleh bleh bleh.
Bleh Bleh bleh.

Another paragraph.

Another paragraph.
"""

so the first line is the "title" of the bmop.
subsequent line blocks seperated by blank lines
are paragraphs.  individual descriptions of slots 
would be extracted from comments
next to them, e.g.

{BMOP_OPSLOT_ELEMENT_BUF, "geomout"}, //output slot, boundary region

the doc generator would automatically detect the presence of "output slot"
and flag the slot as an output.  the same happens for "input slot".  also
note that "edges", "faces", "verts", "loops", and "geometry" are valid 
substitutions for "slot". 

note that slots default to being input slots.
*/

/*
  Vertex Smooth

  Smoothes vertices by using a basic vertex averaging scheme.
*/
BMOpDefine def_vertexsmooth = {
	"vertexsmooth",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {BMOP_OPSLOT_INT, "mirror_clip_x"}, //set vertices close to the x axis before the operation to 0
	 {BMOP_OPSLOT_INT, "mirror_clip_y"}, //set vertices close to the y axis before the operation to 0
	 {BMOP_OPSLOT_INT, "mirror_clip_z"}, //set vertices close to the z axis before the operation to 0
	 {BMOP_OPSLOT_FLT, "clipdist"}, //clipping threshod for the above three slots
	{0} /*null-terminating sentinel*/,
	},
	bmesh_vertexsmooth_exec,
	0
};

/*
  Right-Hand Faces

  Computes an "outside" normal for the specified input faces.
*/

BMOpDefine def_righthandfaces = {
	"righthandfaces",
	{{BMOP_OPSLOT_ELEMENT_BUF, "faces"},
	{0} /*null-terminating sentinel*/,
	},
	bmesh_righthandfaces_exec,
	0
};

/*
  Region Extend
  
  used to implement the select more/less tools.
  this puts some geometry surrounding regions of
  geometry in geom into geomout.
  
  if usefaces is 0 then geomout spits out verts and edges, 
  otherwise it spits out faces.
  */
BMOpDefine def_regionextend = {
	"regionextend",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"}, //input geometry
	 {BMOP_OPSLOT_ELEMENT_BUF, "geomout"}, //output slot, computed boundary geometry.
	 {BMOP_OPSLOT_INT, "constrict"}, //find boundary inside the regions, not outside.
	 {BMOP_OPSLOT_INT, "usefaces"}, //extend from faces instead of edges
	{0} /*null-terminating sentinel*/,
	},
	bmesh_regionextend_exec,
	0
};

/*
  Edge Rotate

  Rotates edges topologically.  Also known as "spin edge" to some people.
  Simple example: [/] becomes [|] then [\].
*/
BMOpDefine def_edgerotate = {
	"edgerotate",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"}, //input edges
	 {BMOP_OPSLOT_ELEMENT_BUF, "edgeout"}, //newly spun edges
   	 {BMOP_OPSLOT_INT, "ccw"}, //rotate edge counter-clockwise if true, othewise clockwise
	{0} /*null-terminating sentinel*/,
	},
	bmesh_edgerotate_exec,
	0
};

/*
  Reverse Faces

  Reverses the winding (vertex order) of faces.  This has the effect of
  flipping the normal.
*/
BMOpDefine def_reversefaces = {
	"reversefaces",
	{{BMOP_OPSLOT_ELEMENT_BUF, "faces"}, //input faces
	{0} /*null-terminating sentinel*/,
	},
	bmesh_reversefaces_exec,
	0
};

/*
  Edge Split

  Splits input edges (but doesn't do anything else).
*/
BMOpDefine def_edgesplit = {
	"edgesplit",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"}, //input edges
	{BMOP_OPSLOT_INT, "numcuts"}, //number of cuts
	{BMOP_OPSLOT_ELEMENT_BUF, "outsplit"}, //newly created vertices and edges
	{0} /*null-terminating sentinel*/,
	},
	esplit_exec,
	0
};

/*
  Mirror

  Mirrors geometry along an axis.  The resulting geometry is welded on using
  mergedist.  Pairs of original/mirrored vertices are welded using the mergedist
  parameter (which defines the minimum distance for welding to happen).
*/

BMOpDefine def_mirror = {
	"mirror",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"}, //input geometry
	 {BMOP_OPSLOT_MAT, "mat"}, //matrix defining the mirror transformation
	 {BMOP_OPSLOT_FLT, "mergedist"}, //maximum distance for merging.  does no merging if 0.
	 {BMOP_OPSLOT_ELEMENT_BUF, "newout"}, //output geometry, mirrored
	 {BMOP_OPSLOT_INT,         "axis"}, //the axis to use, 0, 1, or 2 for x, y, z
	 {BMOP_OPSLOT_INT,         "mirror_u"}, //mirror UVs across the u axis
	 {BMOP_OPSLOT_INT,         "mirror_v"}, //mirror UVs across the v axis
	 {0, /*null-terminating sentinel*/}},
	bmesh_mirror_exec,
	0,
};

/*
  Find Doubles

  Takes input verts and find vertices they should weld to.  Outputs a
  mapping slot suitable for use with the weld verts bmop.
*/
BMOpDefine def_finddoubles = {
	"finddoubles",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {BMOP_OPSLOT_ELEMENT_BUF, "keepverts"}, //list of verts to keep
	 {BMOP_OPSLOT_FLT,         "dist"}, //minimum distance
	 {BMOP_OPSLOT_MAPPING, "targetmapout"},
	 {0, /*null-terminating sentinel*/}},
	bmesh_finddoubles_exec,
	0,
};

/*
  Remove Doubles

  Finds groups of vertices closer then dist and merges them together,
  using the weld verts bmop.
*/
BMOpDefine def_removedoubles = {
	"removedoubles",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input verts
	 {BMOP_OPSLOT_FLT,         "dist"}, //minimum distance
	 {0, /*null-terminating sentinel*/}},
	bmesh_removedoubles_exec,
	0,
};

/*
  Collapse Connected

  Collapses connected vertices
*/
BMOpDefine def_collapse = {
	"collapse",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"}, /*input edges*/
	 {0, /*null-terminating sentinel*/}},
	bmesh_collapse_exec,
	0,
};


/*
  Point Merge

  Merge verts together at a point.
*/
BMOpDefine def_pointmerge = {
	"pointmerge",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, /*input vertices*/
	 {BMOP_OPSLOT_VEC,         "mergeco"},
	 {0, /*null-terminating sentinel*/}},
	bmesh_pointmerge_exec,
	0,
};

/*
  Collapse Connected UVs

  Collapses connected UV vertices.
*/
BMOpDefine def_collapse_uvs = {
	"collapse_uvs",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"}, /*input edges*/
	 {0, /*null-terminating sentinel*/}},
	bmesh_collapsecon_exec,
	0,
};

/*
  Weld Verts

  Welds verts together (kindof like remove doubles, merge, etc, all of which
  use or will use this bmop).  You pass in mappings from vertices to the vertices
  they weld with.
*/
BMOpDefine def_weldverts = {
	"weldverts",
	{{BMOP_OPSLOT_MAPPING, "targetmap"}, /*maps welded vertices to verts they should weld to.*/
	 {0, /*null-terminating sentinel*/}},
	bmesh_weldverts_exec,
	0,
};

/*
  Make Vertex

  Creates a single vertex; this bmop was necassary
  for click-create-vertex.
*/
BMOpDefine def_makevert = {
	"makevert",
	{{BMOP_OPSLOT_VEC, "co"}, //the coordinate of the new vert
	{BMOP_OPSLOT_ELEMENT_BUF, "newvertout"}, //the new vert
	{0, /*null-terminating sentinel*/}},
	bmesh_makevert_exec,
	0,
};

/*
  Contextual Create

  This is basically fkey, it creates
  new faces from vertices, makes stuff from edge nets,
  makes wire edges, etc.  It also dissolves
  faces.
  
  Three verts become a triangle, four become a quad.  Two
  become a wire edge.
  */
BMOpDefine def_contextual_create= {
	"contextual_create",
	{{BMOP_OPSLOT_ELEMENT_BUF, "geom"}, //input geometry.
	 {BMOP_OPSLOT_ELEMENT_BUF, "faceout"}, //newly-made face(s)
	 {0, /*null-terminating sentinel*/}},
	bmesh_contextual_create_exec,
	0,
};

/*this may be unimplemented*/
BMOpDefine def_edgenet_fill= {
	"edgenet_fill",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"},
	 {BMOP_OPSLOT_ELEMENT_BUF, "faceout"},
	{0, /*null-terminating sentinel*/}},
	bmesh_edgenet_fill_exec,
	0,
};

/*
  Rotate

  Rotate vertices around a center, using a 3x3 rotation
  matrix.  Equivilent of the old rotateflag function.
*/
BMOpDefine def_rotate = {
	"rotate",
	{{BMOP_OPSLOT_VEC, "cent"}, //center of rotation
	 {BMOP_OPSLOT_MAT, "mat"}, //matrix defining rotation
	{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input vertices
	{0, /*null-terminating sentinel*/}},
	bmesh_rotate_exec,
	0,
};

/*
  Translate

  Translate vertices by an offset.  Equivelent of the
  old translateflag function.
*/
BMOpDefine def_translate= {
	"translate",
	{{BMOP_OPSLOT_VEC, "vec"}, //translation offset
	{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input vertices
	{0, /*null-terminating sentinel*/}},
	bmesh_translate_exec,
	0,
};


/*
  Transform

  Transforms a set of vertices by a matrix.  Multiplies
  the vertex coordinates with the matrix.
*/
BMOpDefine def_transform = {
	"transform",
	{{BMOP_OPSLOT_MAT, "mat"}, //transform matrix
	{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input vertices
	{0, /*null-terminating sentinel*/}},
	bmesh_transform_exec,
	0,
};

/*
  Object Load BMesh

  Loads a bmesh into an object/mesh.  This is a "private"
  bmop.
*/
BMOpDefine def_object_load_bmesh = {
	"object_load_bmesh",
	{{BMOP_OPSLOT_PNT, "scene"},
	{BMOP_OPSLOT_PNT, "object"},
	{0, /*null-terminating sentinel*/}},
	bmesh_to_mesh_exec,
	0,
};


/*
  Mesh to BMesh

  Load the contents of a mesh into the bmesh.
*/
BMOpDefine def_mesh_to_bmesh = {
	"mesh_to_bmesh",
	{{BMOP_OPSLOT_PNT, "mesh"}, //pointer to a Mesh structure
	 {0, /*null-terminating sentinel*/}},
	mesh_to_bmesh_exec,
	0
};

/*
  Individual Face Extrude

  Extrudes faces individually.
*/
BMOpDefine def_extrude_indivface = {
	"extrude_face_indiv",
	{{BMOP_OPSLOT_ELEMENT_BUF, "faces"}, //input faces
	{BMOP_OPSLOT_ELEMENT_BUF, "faceout"}, //output faces
	{BMOP_OPSLOT_ELEMENT_BUF, "skirtout"}, //output skirt geometry, faces and edges
	{0} /*null-terminating sentinel*/},
	bmesh_extrude_face_indiv_exec,
	0
};

/*
  Extrude Only Edges

  Extrudes Edges into faces, note that this is very simple, there's no fancy
  winged extrusion.
*/
BMOpDefine def_extrude_onlyedge = {
	"extrude_edge_only",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"}, //input vertices
	{BMOP_OPSLOT_ELEMENT_BUF, "geomout"}, //output geometry
	{0} /*null-terminating sentinel*/},
	bmesh_extrude_onlyedge_exec,
	0
};

/*
  Individual Vertex Extrude

  Extrudes wire edges from vertices.
*/
BMOpDefine def_extrudeverts_indiv = {
	"extrude_vert_indiv",
	{{BMOP_OPSLOT_ELEMENT_BUF, "verts"}, //input vertices
	{BMOP_OPSLOT_ELEMENT_BUF, "edgeout"}, //output wire edges
	{BMOP_OPSLOT_ELEMENT_BUF, "vertout"}, //output vertices
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

	{BMOP_OPSLOT_INT, "quadcornertype"}, //quad corner type, see bmesh_operators.h
	{BMOP_OPSLOT_INT, "gridfill"}, //fill in fully-selected faces with a grid
	{BMOP_OPSLOT_INT, "singleedge"}, //tesselate the case of one edge selected in a quad or triangle

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

/*
  Similar faces select

  Select similar faces (area/material/perimeter....).
*/
BMOpDefine def_similarfaces = {
	"similarfaces",
	{{BMOP_OPSLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {BMOP_OPSLOT_ELEMENT_BUF, "faceout"}, /* output faces */
	 {BMOP_OPSLOT_INT, "type"},			/* type of selection */
	 {BMOP_OPSLOT_FLT, "thresh"},		/* threshold of selection */
	 {0} /*null-terminating sentinel*/},
	bmesh_similarfaces_exec,
	0
};

/*
  Similar edges select

  Select similar edges (length, direction, edge, seam,....).
*/
BMOpDefine def_similaredges = {
	"similaredges",
	{{BMOP_OPSLOT_ELEMENT_BUF, "edges"}, /* input edges */
	 {BMOP_OPSLOT_ELEMENT_BUF, "edgeout"}, /* output edges */
	 {BMOP_OPSLOT_INT, "type"},			/* type of selection */
	 {BMOP_OPSLOT_FLT, "thresh"},		/* threshold of selection */
	 {0} /*null-terminating sentinel*/},
	bmesh_similaredges_exec,
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
	&def_mirror,
	&def_edgesplit,
	&def_reversefaces,
	&def_edgerotate,
	&def_regionextend,
	&def_righthandfaces,
	&def_vertexsmooth,
	&def_extrude_onlyedge,
	&def_extrude_indivface,
	&def_collapse_uvs,
	&def_pointmerge,
	&def_collapse,
	&def_similarfaces,
	&def_similaredges,
};

int bmesh_total_ops = (sizeof(opdefines) / sizeof(void*));
