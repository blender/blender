/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_opdefines.c
 *  \ingroup bmesh
 *
 * BMesh operator definitions.
 *
 * This file defines (and documents) all bmesh operators (bmops).
 *
 * Do not rename any operator or slot names! otherwise you must go
 * through the code and find all references to them!
 *
 * A word on slot names:
 *
 * For geometry input slots, the following are valid names:
 * - verts
 * - edges
 * - faces
 * - edgefacein
 * - vertfacein
 * - vertedgein
 * - vertfacein
 * - geom
 *
 * The basic rules are, for single-type geometry slots, use the plural of the
 * type name (e.g. edges).  for double-type slots, use the two type names plus
 * "in" (e.g. edgefacein).  for three-type slots, use geom.
 *
 * for output slots, for single-type geometry slots, use the type name plus "out",
 * (e.g. vertout), for double-type slots, use the two type names plus "out",
 * (e.g. vertfaceout), for three-type slots, use geom.  note that you can also
 * use more esohteric names (e.g. skirtout) so long as the comment next to the
 * slot definition tells you what types of elements are in it.
 *
 */

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* ok, I'm going to write a little docgen script. so all
 * bmop comments must conform to the following template/rules:
 *
 * template (py quotes used because nested comments don't work
 * on all C compilers):
 *
 * """
 * Region Extend.
 *
 * paragraph1, Extends bleh bleh bleh.
 * Bleh Bleh bleh.
 *
 * Another paragraph.
 *
 * Another paragraph.
 * """
 *
 * so the first line is the "title" of the bmop.
 * subsequent line blocks separated by blank lines
 * are paragraphs.  individual descriptions of slots
 * would be extracted from comments
 * next to them, e.g.
 *
 * {BMO_OP_SLOT_ELEMENT_BUF, "geomout"}, //output slot, boundary region
 *
 * the doc generator would automatically detect the presence of "output slot"
 * and flag the slot as an output.  the same happens for "input slot".  also
 * note that "edges", "faces", "verts", "loops", and "geometry" are valid
 * substitutions for "slot".
 *
 * note that slots default to being input slots.
 */

/*
 * Vertex Smooth
 *
 * Smoothes vertices by using a basic vertex averaging scheme.
 */
static BMOpDefine bmo_vertexsmooth_def = {
	"vertexsmooth",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {BMO_OP_SLOT_BOOL, "mirror_clip_x"}, //set vertices close to the x axis before the operation to 0
	 {BMO_OP_SLOT_BOOL, "mirror_clip_y"}, //set vertices close to the y axis before the operation to 0
	 {BMO_OP_SLOT_BOOL, "mirror_clip_z"}, //set vertices close to the z axis before the operation to 0
	 {BMO_OP_SLOT_FLT, "clipdist"}, //clipping threshod for the above three slots
	{0} /* null-terminating sentine */,
	},
	bmo_vertexsmooth_exec,
	0
};

/*
 * Right-Hand Faces
 *
 * Computes an "outside" normal for the specified input faces.
 */

static BMOpDefine bmo_righthandfaces_def = {
	"righthandfaces",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"},
	 {BMO_OP_SLOT_BOOL, "do_flip"}, //internal flag, used by bmesh_rationalize_normals
	 {0} /* null-terminating sentine */,
	},
	bmo_righthandfaces_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Region Extend
 *
 * used to implement the select more/less tools.
 * this puts some geometry surrounding regions of
 * geometry in geom into geomout.
 *
 * if usefaces is 0 then geomout spits out verts and edges,
 * otherwise it spits out faces.
 */
static BMOpDefine bmo_regionextend_def = {
	"regionextend",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"}, //input geometry
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"}, //output slot, computed boundary geometry.
	 {BMO_OP_SLOT_BOOL, "constrict"}, //find boundary inside the regions, not outside.
	 {BMO_OP_SLOT_BOOL, "use_faces"}, //extend from faces instead of edges
	 {0} /* null-terminating sentine */,
	},
	bmo_regionextend_exec,
	0
};

/*
 * Edge Rotate
 *
 * Rotates edges topologically.  Also known as "spin edge" to some people.
 * Simple example: [/] becomes [|] then [\].
 */
static BMOpDefine bmo_edgerotate_def = {
	"edgerotate",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, //input edges
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout"}, //newly spun edges
	 {BMO_OP_SLOT_BOOL, "ccw"}, //rotate edge counter-clockwise if true, othewise clockwise
	 {0} /* null-terminating sentine */,
	},
	bmo_edgerotate_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

/*
 * Reverse Faces
 *
 * Reverses the winding (vertex order) of faces.  This has the effect of
 * flipping the normal.
 */
static BMOpDefine bmo_reversefaces_def = {
	"reversefaces",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, //input faces
	 {0} /* null-terminating sentine */,
	},
	bmo_reversefaces_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Edge Bisect
 *
 * Splits input edges (but doesn't do anything else).
 * This creates a 2-valence vert.
 */
static BMOpDefine bmo_edgebisect_def = {
	"edgebisect",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, //input edges
	 {BMO_OP_SLOT_INT, "numcuts"}, //number of cuts
	 {BMO_OP_SLOT_ELEMENT_BUF, "outsplit"}, //newly created vertices and edges
	 {0} /* null-terminating sentine */,
	},
	bmo_edgebisect_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

/*
 * Mirror
 *
 * Mirrors geometry along an axis.  The resulting geometry is welded on using
 * mergedist.  Pairs of original/mirrored vertices are welded using the mergedist
 * parameter (which defines the minimum distance for welding to happen).
 */

static BMOpDefine bmo_mirror_def = {
	"mirror",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"}, //input geometry
	 {BMO_OP_SLOT_MAT, "mat"}, //matrix defining the mirror transformation
	 {BMO_OP_SLOT_FLT, "mergedist"}, //maximum distance for merging.  does no merging if 0.
	 {BMO_OP_SLOT_ELEMENT_BUF, "newout"}, //output geometry, mirrored
	 {BMO_OP_SLOT_INT,         "axis"}, //the axis to use, 0, 1, or 2 for x, y, z
	 {BMO_OP_SLOT_BOOL,        "mirror_u"}, //mirror UVs across the u axis
	 {BMO_OP_SLOT_BOOL,        "mirror_v"}, //mirror UVs across the v axis
	 {0, /* null-terminating sentine */}},
	bmo_mirror_exec,
	0,
};

/*
 * Find Doubles
 *
 * Takes input verts and find vertices they should weld to.  Outputs a
 * mapping slot suitable for use with the weld verts bmop.
 */
static BMOpDefine bmo_finddoubles_def = {
	"finddoubles",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {BMO_OP_SLOT_ELEMENT_BUF, "keepverts"}, //list of verts to keep
	 {BMO_OP_SLOT_FLT,         "dist"}, //minimum distance
	 {BMO_OP_SLOT_MAPPING, "targetmapout"},
	 {0, /* null-terminating sentine */}},
	bmo_finddoubles_exec,
	0,
};

/*
 * Remove Doubles
 *
 * Finds groups of vertices closer then dist and merges them together,
 * using the weld verts bmop.
 */
static BMOpDefine bmo_removedoubles_def = {
	"removedoubles",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input verts
	 {BMO_OP_SLOT_FLT,         "dist"}, //minimum distance
	 {0, /* null-terminating sentine */}},
	bmo_removedoubles_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Auto Merge
 *
 * Finds groups of vertices closer then dist and merges them together,
 * using the weld verts bmop.  The merges must go from a vert not in
 * verts to one in verts.
 */
static BMOpDefine bmo_automerge_def = {
	"automerge",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input verts
	 {BMO_OP_SLOT_FLT,         "dist"}, //minimum distance
	 {0, /* null-terminating sentine */}},
	bmo_automerge_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Collapse Connected
 *
 * Collapses connected vertices
 */
static BMOpDefine bmo_collapse_def = {
	"collapse",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edge */
	 {0, /* null-terminating sentine */}},
	bmo_collapse_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};


/*
 * Facedata point Merge
 *
 * Merge uv/vcols at a specific vertex.
 */
static BMOpDefine bmo_pointmerge_facedata_def = {
	"pointmerge_facedata",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, /* input vertice */
	 {BMO_OP_SLOT_ELEMENT_BUF, "snapv"}, /* snap verte */
	 {0, /* null-terminating sentine */}},
	bmo_pointmerge_facedata_exec,
	0,
};

/*
 * Average Vertices Facevert Data
 *
 * Merge uv/vcols associated with the input vertices at
 * the bounding box center. (I know, it's not averaging but
 * the vert_snap_to_bb_center is just too long).
 */
static BMOpDefine bmo_vert_average_facedata_def = {
	"vert_average_facedata",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, /* input vertice */
	 {0, /* null-terminating sentine */}},
	bmo_vert_average_facedata_exec,
	0,
};

/*
 * Point Merge
 *
 * Merge verts together at a point.
 */
static BMOpDefine bmo_pointmerge_def = {
	"pointmerge",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, /* input vertice */
	 {BMO_OP_SLOT_VEC,         "mergeco"},
	 {0, /* null-terminating sentine */}},
	bmo_pointmerge_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Collapse Connected UVs
 *
 * Collapses connected UV vertices.
 */
static BMOpDefine bmo_collapse_uvs_def = {
	"collapse_uvs",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edge */
	 {0, /* null-terminating sentine */}},
	bmo_collapse_uvs_exec,
	0,
};

/*
 * Weld Verts
 *
 * Welds verts together (kindof like remove doubles, merge, etc, all of which
 * use or will use this bmop).  You pass in mappings from vertices to the vertices
 * they weld with.
 */
static BMOpDefine bmo_weldverts_def = {
	"weldverts",
	{{BMO_OP_SLOT_MAPPING, "targetmap"}, /* maps welded vertices to verts they should weld to */
	 {0, /* null-terminating sentine */}},
	bmo_weldverts_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Make Vertex
 *
 * Creates a single vertex; this bmop was necessary
 * for click-create-vertex.
 */
static BMOpDefine bmo_makevert_def = {
	"makevert",
	{{BMO_OP_SLOT_VEC, "co"}, //the coordinate of the new vert
	 {BMO_OP_SLOT_ELEMENT_BUF, "newvertout"}, //the new vert
	 {0, /* null-terminating sentine */}},
	bmo_makevert_exec,
	0,
};

/*
 * Join Triangles
 *
 * Tries to intelligently join triangles according
 * to various settings and stuff.
 */
static BMOpDefine bmo_join_triangles_def = {
	"join_triangles",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, //input geometry.
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"}, //joined faces
	 {BMO_OP_SLOT_BOOL, "cmp_sharp"},
	 {BMO_OP_SLOT_BOOL, "cmp_uvs"},
	 {BMO_OP_SLOT_BOOL, "cmp_vcols"},
	 {BMO_OP_SLOT_BOOL, "cmp_materials"},
	 {BMO_OP_SLOT_FLT, "limit"},
	 {0, /* null-terminating sentine */}},
	bmo_join_triangles_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Contextual Create
 *
 * This is basically fkey, it creates
 * new faces from vertices, makes stuff from edge nets,
 * makes wire edges, etc.  It also dissolves
 * faces.
 *
 * Three verts become a triangle, four become a quad.  Two
 * become a wire edge.
 */
static BMOpDefine bmo_contextual_create_def = {
	"contextual_create",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"}, //input geometry.
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"}, //newly-made face(s)
	 {0, /* null-terminating sentine */}},
	bmo_contextual_create_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES,
};

/*
 * Bridge edge loops with faces
 */
static BMOpDefine bmo_bridge_loops_def = {
	"bridge_loops",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edge */
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"}, /* new face */
	 {0, /* null-terminating sentine */}},
	bmo_bridge_loops_exec,
	0,
};

static BMOpDefine bmo_edgenet_fill_def = {
	"edgenet_fill",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edge */
	 {BMO_OP_SLOT_MAPPING,     "restrict"}, /* restricts edges to groups.  maps edges to integer */
	 {BMO_OP_SLOT_BOOL,        "use_restrict"},
	 {BMO_OP_SLOT_BOOL,        "use_fill_check"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "excludefaces"}, /* list of faces to ignore for manifold check */
	 {BMO_OP_SLOT_MAPPING,     "faceout_groupmap"}, /* maps new faces to the group numbers they came fro */
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"}, /* new face */
	 {0, /* null-terminating sentine */}},
	bmo_edgenet_fill_exec,
	0,
};

/*
 * Edgenet Prepare
 *
 * Identifies several useful edge loop cases and modifies them so
 * they'll become a face when edgenet_fill is called.  The cases covered are:
 *
 * - One single loop; an edge is added to connect the ends
 * - Two loops; two edges are added to connect the endpoints (based on the
 *   shortest distance between each endpont).
 */
static BMOpDefine bmo_edgenet_prepare_def = {
	"edgenet_prepare",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, //input edges
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout"}, //new edges
	 {0, /* null-terminating sentine */}},
	bmo_edgenet_prepare,
	0,
};

/*
 * Rotate
 *
 * Rotate vertices around a center, using a 3x3 rotation
 * matrix.  Equivalent of the old rotateflag function.
 */
static BMOpDefine bmo_rotate_def = {
	"rotate",
	{{BMO_OP_SLOT_VEC, "cent"}, //center of rotation
	 {BMO_OP_SLOT_MAT, "mat"}, //matrix defining rotation
	 {BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {0, /* null-terminating sentine */}},
	bmo_rotate_exec,
	0,
};

/*
 * Translate
 *
 * Translate vertices by an offset.  Equivalent of the
 * old translateflag function.
 */
static BMOpDefine bmo_translate_def = {
	"translate",
	{{BMO_OP_SLOT_VEC, "vec"}, //translation offset
	 {BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {0, /* null-terminating sentine */}},
	bmo_translate_exec,
	0,
};

/*
 * Scale
 *
 * Scales vertices by an offset.
 */
static BMOpDefine bmo_scale_def = {
	"scale",
	{{BMO_OP_SLOT_VEC, "vec"}, //scale factor
	 {BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {0, /* null-terminating sentine */}},
	bmo_scale_exec,
	0,
};


/*
 * Transform
 *
 * Transforms a set of vertices by a matrix.  Multiplies
 * the vertex coordinates with the matrix.
 */
static BMOpDefine bmo_transform_def = {
	"transform",
	{{BMO_OP_SLOT_MAT, "mat"}, //transform matrix
	 {BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {0, /* null-terminating sentine */}},
	bmo_transform_exec,
	0,
};

/*
 * Object Load BMesh
 *
 * Loads a bmesh into an object/mesh.  This is a "private"
 * bmop.
 */
static BMOpDefine bmo_object_load_bmesh_def = {
	"object_load_bmesh",
	{{BMO_OP_SLOT_PNT, "scene"},
	 {BMO_OP_SLOT_PNT, "object"},
	 {0, /* null-terminating sentine */}},
	bmo_object_load_bmesh_exec,
	0,
};


/*
 * BMesh to Mesh
 *
 * Converts a bmesh to a Mesh.  This is reserved for exiting editmode.
 */
static BMOpDefine bmo_bmesh_to_mesh_def = {
	"bmesh_to_mesh",
	{{BMO_OP_SLOT_PNT, "mesh"}, //pointer to a mesh structure to fill in
	 {BMO_OP_SLOT_PNT, "object"}, //pointer to an object structure
	 {BMO_OP_SLOT_BOOL, "notessellation"}, //don't calculate mfaces
	 {0, /* null-terminating sentine */}},
	bmo_bmesh_to_mesh_exec,
	0,
};

/*
 * Mesh to BMesh
 *
 * Load the contents of a mesh into the bmesh.  this bmop is private, it's
 * reserved exclusively for entering editmode.
 */
static BMOpDefine bmo_mesh_to_bmesh_def = {
	"mesh_to_bmesh",
	{{BMO_OP_SLOT_PNT, "mesh"}, //pointer to a Mesh structure
	 {BMO_OP_SLOT_PNT, "object"}, //pointer to an Object structure
	 {BMO_OP_SLOT_BOOL, "set_shapekey"}, //load active shapekey coordinates into verts
	 {0, /* null-terminating sentine */}},
	bmo_mesh_to_bmesh_exec,
	0
};

/*
 * Individual Face Extrude
 *
 * Extrudes faces individually.
 */
static BMOpDefine bmo_extrude_indivface_def = {
	"extrude_face_indiv",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, //input faces
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"}, //output faces
	 {BMO_OP_SLOT_ELEMENT_BUF, "skirtout"}, //output skirt geometry, faces and edges
	 {0} /* null-terminating sentine */},
	bmo_extrude_face_indiv_exec,
	0
};

/*
 * Extrude Only Edges
 *
 * Extrudes Edges into faces, note that this is very simple, there's no fancy
 * winged extrusion.
 */
static BMOpDefine bmo_extrude_edge_only_def = {
	"extrude_edge_only",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, //input vertices
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"}, //output geometry
	 {0} /* null-terminating sentine */},
	bmo_extrude_edge_only_exec,
	0
};

/*
 * Individual Vertex Extrude
 *
 * Extrudes wire edges from vertices.
 */
static BMOpDefine bmo_extrude_vert_indiv_def = {
	"extrude_vert_indiv",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, //input vertices
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout"}, //output wire edges
	 {BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output vertices
	 {0} /* null-terminating sentine */},
	bmo_extrude_vert_indiv_exec,
	0
};

static BMOpDefine bmo_connectverts_def = {
	"connectverts",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout"},
	 {0} /* null-terminating sentine */},
	bmo_connectverts_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_extrude_face_region_def = {
	"extrude_face_region",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edgefacein"},
	 {BMO_OP_SLOT_MAPPING, "exclude"},
	 {BMO_OP_SLOT_BOOL, "alwayskeeporig"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"},
	 {0} /* null-terminating sentine */},
	bmo_extrude_face_region_exec,
	0
};

static BMOpDefine bmo_dissolve_verts_def = {
	"dissolve_verts",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"},
	 {0} /* null-terminating sentine */},
	bmo_dissolve_verts_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_dissolve_edges_def = {
	"dissolve_edges",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "regionout"},
	 {BMO_OP_SLOT_BOOL, "use_verts"}, // dissolve verts left between only 2 edges.
	 {0} /* null-terminating sentine */},
	bmo_dissolve_edges_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_dissolve_edge_loop_def = {
	"dissolve_edge_loop",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "regionout"},
	 {0} /* null-terminating sentine */},
	bmo_dissolve_edgeloop_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_dissolve_faces_def = {
	"dissolve_faces",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "regionout"},
	 {BMO_OP_SLOT_BOOL, "use_verts"}, // dissolve verts left between only 2 edges.
	 {0} /* null-terminating sentine */},
	bmo_dissolve_faces_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_dissolve_limit_def = {
	"dissolve_limit",
	{{BMO_OP_SLOT_FLT, "angle_limit"}, /* total rotation angle (degrees) */
	 {BMO_OP_SLOT_ELEMENT_BUF, "verts"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "edges"},
	 {0} /* null-terminating sentine */},
	bmo_dissolve_limit_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_triangulate_def = {
	"triangulate",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"},
	 {BMO_OP_SLOT_MAPPING, "facemap"},
	 {BMO_OP_SLOT_BOOL, "use_beauty"},
	 {0} /* null-terminating sentine */},
	bmo_triangulate_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_esubd_def = {
	"esubd",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"},
	 {BMO_OP_SLOT_INT, "numcuts"},
	 {BMO_OP_SLOT_FLT, "smooth"},
	 {BMO_OP_SLOT_FLT, "fractal"},
	 {BMO_OP_SLOT_INT, "beauty"},
	 {BMO_OP_SLOT_INT, "seed"},
	 {BMO_OP_SLOT_MAPPING, "custompatterns"},
	 {BMO_OP_SLOT_MAPPING, "edgepercents"},

	/* these next three can have multiple types of elements in them */
	 {BMO_OP_SLOT_ELEMENT_BUF, "outinner"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "outsplit"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"}, /* contains all output geometr */

	 {BMO_OP_SLOT_INT,  "quadcornertype"}, //quad corner type, see bmesh_operators.h
	 {BMO_OP_SLOT_BOOL, "gridfill"}, //fill in fully-selected faces with a grid
	 {BMO_OP_SLOT_BOOL, "singleedge"}, //tesselate the case of one edge selected in a quad or triangle

	 {0} /* null-terminating sentine */,
	},
	bmo_esubd_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

static BMOpDefine bmo_del_def = {
	"del",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"},
	 {BMO_OP_SLOT_INT, "context"},
	 {0} /* null-terminating sentine */},
	bmo_del_exec,
	0
};

static BMOpDefine bmo_dupe_def = {
	"dupe",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "origout"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "newout"},
	/* facemap maps from source faces to dupe
	 * faces, and from dupe faces to source faces */
	 {BMO_OP_SLOT_MAPPING, "facemap"},
	 {BMO_OP_SLOT_MAPPING, "boundarymap"},
	 {BMO_OP_SLOT_MAPPING, "isovertmap"},
	 {BMO_OP_SLOT_PNT, "dest"}, /* destination bmesh, if NULL will use current on */
	 {0} /* null-terminating sentine */},
	bmo_dupe_exec,
	0
};

static BMOpDefine bmo_split_def = {
	"split",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"},
	 {BMO_OP_SLOT_MAPPING, "boundarymap"},
	 {BMO_OP_SLOT_MAPPING, "isovertmap"},
	 {BMO_OP_SLOT_PNT, "dest"}, /* destination bmesh, if NULL will use current on */
	 {BMO_OP_SLOT_BOOL, "use_only_faces"}, /* when enabled. dont duplicate loose verts/edges */
	 {0} /* null-terminating sentine */},
	bmo_split_exec,
	0
};

/*
 * Spin
 *
 * Extrude or duplicate geometry a number of times,
 * rotating and possibly translating after each step
 */
static BMOpDefine bmo_spin_def = {
	"spin",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "lastout"}, /* result of last step */
	 {BMO_OP_SLOT_VEC, "cent"}, /* rotation center */
	 {BMO_OP_SLOT_VEC, "axis"}, /* rotation axis */
	 {BMO_OP_SLOT_VEC, "dvec"}, /* translation delta per step */
	 {BMO_OP_SLOT_FLT, "ang"}, /* total rotation angle (degrees) */
	 {BMO_OP_SLOT_INT, "steps"}, /* number of steps */
	 {BMO_OP_SLOT_BOOL, "do_dupli"}, /* duplicate or extrude? */
	 {0} /* null-terminating sentine */},
	bmo_spin_exec,
	0
};


/*
 * Similar faces search
 *
 * Find similar faces (area/material/perimeter, ...).
 */
static BMOpDefine bmo_similarfaces_def = {
	"similarfaces",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {BMO_OP_SLOT_ELEMENT_BUF, "faceout"}, /* output faces */
	 {BMO_OP_SLOT_INT, "type"},			/* type of selection */
	 {BMO_OP_SLOT_FLT, "thresh"},		/* threshold of selection */
	 {0} /* null-terminating sentine */},
	bmo_similarfaces_exec,
	0
};

/*
 * Similar edges search
 *
 *  Find similar edges (length, direction, edge, seam, ...).
 */
static BMOpDefine bmo_similaredges_def = {
	"similaredges",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edges */
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout"}, /* output edges */
	 {BMO_OP_SLOT_INT, "type"},			/* type of selection */
	 {BMO_OP_SLOT_FLT, "thresh"},		/* threshold of selection */
	 {0} /* null-terminating sentine */},
	bmo_similaredges_exec,
	0
};

/*
 * Similar vertices search
 *
 * Find similar vertices (normal, face, vertex group, ...).
 */
static BMOpDefine bmo_similarverts_def = {
	"similarverts",
	{{BMO_OP_SLOT_ELEMENT_BUF, "verts"}, /* input vertices */
	 {BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, /* output vertices */
	 {BMO_OP_SLOT_INT, "type"},			/* type of selection */
	 {BMO_OP_SLOT_FLT, "thresh"},		/* threshold of selection */
	 {0} /* null-terminating sentine */},
	bmo_similarverts_exec,
	0
};

/*
 * uv rotation
 * cycle the uvs
 */
static BMOpDefine bmo_face_rotateuvs_def = {
	"face_rotateuvs",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {BMO_OP_SLOT_INT, "dir"},			/* direction */
	 {0} /* null-terminating sentine */},
	bmo_face_rotateuvs_exec,
	0
};

/*
 * uv reverse
 * reverse the uvs
 */
static BMOpDefine bmo_face_reverseuvs_def = {
	"face_reverseuvs",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {0} /* null-terminating sentine */},
	bmo_face_reverseuvs_exec,
	0
};

/*
 * color rotation
 * cycle the colors
 */
static BMOpDefine bmo_face_rotatecolors_def = {
	"face_rotatecolors",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {BMO_OP_SLOT_INT, "dir"},			/* direction */
	 {0} /* null-terminating sentine */},
	bmo_rotatecolors_exec,
	0
};

/*
 * color reverse
 * reverse the colors
 */
static BMOpDefine bmo_face_reversecolors_def = {
	"face_reversecolors",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {0} /* null-terminating sentine */},
	bmo_face_reversecolors_exec,
	0
};

/*
 * Similar vertices search
 *
 * Find similar vertices (normal, face, vertex group, ...).
 */
static BMOpDefine bmo_vertexshortestpath_def = {
	"vertexshortestpath",
	{{BMO_OP_SLOT_ELEMENT_BUF, "startv"}, /* start vertex */
	 {BMO_OP_SLOT_ELEMENT_BUF, "endv"}, /* end vertex */
	 {BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, /* output vertices */
	 {BMO_OP_SLOT_INT, "type"},			/* type of selection */
	 {0} /* null-terminating sentine */},
	bmo_vertexshortestpath_exec,
	0
};

/*
 * Edge Split
 *
 * Disconnects faces along input edges.
 */
static BMOpDefine bmo_edgesplit_def = {
	"edgesplit",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edges */
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout1"}, /* old output disconnected edges */
	 {BMO_OP_SLOT_ELEMENT_BUF, "edgeout2"}, /* new output disconnected edges */
	 {0} /* null-terminating sentine */},
	bmo_edgesplit_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

/*
 * Create Grid
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_grid_def = {
	"create_grid",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_INT,         "xsegments"}, //number of x segments
	 {BMO_OP_SLOT_INT,         "ysegments"}, //number of y segments
	 {BMO_OP_SLOT_FLT,         "size"}, //size of the grid
	 {BMO_OP_SLOT_MAT,         "mat"}, //matrix to multiply the new geometry with
	 {0, /* null-terminating sentine */}},
	bmo_create_grid_exec,
	0,
};

/*
 * Create UV Sphere
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_uvsphere_def = {
	"create_uvsphere",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_INT,         "segments"}, //number of u segments
	 {BMO_OP_SLOT_INT,         "revolutions"}, //number of v segment
	 {BMO_OP_SLOT_FLT,         "diameter"}, //diameter
	 {BMO_OP_SLOT_MAT,         "mat"}, //matrix to multiply the new geometry with--
	 {0, /* null-terminating sentine */}},
	bmo_create_uvsphere_exec,
	0,
};

/*
 * Create Ico Sphere
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_icosphere_def = {
	"create_icosphere",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_INT,         "subdivisions"}, //how many times to recursively subdivide the sphere
	 {BMO_OP_SLOT_FLT,         "diameter"}, //diameter
	 {BMO_OP_SLOT_MAT,         "mat"}, //matrix to multiply the new geometry with
	 {0, /* null-terminating sentine */}},
	bmo_create_icosphere_exec,
	0,
};

/*
 * Create Suzanne
 *
 * Creates a monkey.  Be wary.
 */
static BMOpDefine bmo_create_monkey_def = {
	"create_monkey",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_MAT, "mat"}, //matrix to multiply the new geometry with--
	 {0, /* null-terminating sentine */}},
	bmo_create_monkey_exec,
	0,
};

/*
 * Create Cone
 *
 * Creates a cone with variable depth at both ends
 */
static BMOpDefine bmo_create_cone_def = {
	"create_cone",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_BOOL, "cap_ends"}, //wheter or not to fill in the ends with faces
	 {BMO_OP_SLOT_BOOL, "cap_tris"}, //fill ends with triangles instead of ngons
	 {BMO_OP_SLOT_INT, "segments"},
	 {BMO_OP_SLOT_FLT, "diameter1"}, //diameter of one end
	 {BMO_OP_SLOT_FLT, "diameter2"}, //diameter of the opposite
	 {BMO_OP_SLOT_FLT, "depth"}, //distance between ends
	 {BMO_OP_SLOT_MAT, "mat"}, //matrix to multiply the new geometry with--
	 {0, /* null-terminating sentine */}},
	bmo_create_cone_exec,
	0,
};

/*
 * Creates a circle
 */
static BMOpDefine bmo_create_circle_def = {
	"create_circle",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_BOOL, "cap_ends"}, //wheter or not to fill in the ends with faces
	 {BMO_OP_SLOT_BOOL, "cap_tris"}, //fill ends with triangles instead of ngons
	 {BMO_OP_SLOT_INT, "segments"},
	 {BMO_OP_SLOT_FLT, "diameter"}, //diameter of one end
	 {BMO_OP_SLOT_MAT, "mat"}, //matrix to multiply the new geometry with--
	 {0, /* null-terminating sentine */}},
	bmo_create_circle_exec,
	0,
};

/*
 * Create Cone
 *
 * Creates a cone with variable depth at both ends
 */
static BMOpDefine bmo_create_cube_def = {
	"create_cube",
	{{BMO_OP_SLOT_ELEMENT_BUF, "vertout"}, //output verts
	 {BMO_OP_SLOT_FLT, "size"}, //size of the cube
	 {BMO_OP_SLOT_MAT, "mat"}, //matrix to multiply the new geometry with--
	 {0, /* null-terminating sentine */}},
	bmo_create_cube_exec,
	0,
};

/*
 * Bevel
 *
 * Bevels edges and vertices
 */
static BMOpDefine bmo_bevel_def = {
	"bevel",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"}, /* input edges and vertices */
	 {BMO_OP_SLOT_ELEMENT_BUF, "face_spans"}, /* new geometry */
	 {BMO_OP_SLOT_ELEMENT_BUF, "face_holes"}, /* new geometry */
	 {BMO_OP_SLOT_BOOL, "use_lengths"}, /* grab edge lengths from a PROP_FLT customdata laye */
	 {BMO_OP_SLOT_BOOL, "use_even"}, /* corner vert placement: use shell/angle calculations  */
	 {BMO_OP_SLOT_BOOL, "use_dist"}, /* corner vert placement: evaluate percent as a distance,
	                                  * modifier uses this. We could do this as another float setting */
	 {BMO_OP_SLOT_INT, "lengthlayer"}, /* which PROP_FLT layer to us */
	 {BMO_OP_SLOT_FLT, "percent"}, /* percentage to expand bevelled edge */
	 {0} /* null-terminating sentine */},
	bmo_bevel_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

/*
 * Beautify Fill
 *
 * Makes triangle a bit nicer
 */
static BMOpDefine bmo_beautify_fill_def = {
	"beautify_fill",
	{{BMO_OP_SLOT_ELEMENT_BUF, "faces"}, /* input faces */
	 {BMO_OP_SLOT_ELEMENT_BUF, "constrain_edges"}, /* edges that can't be flipped */
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"}, /* new flipped faces and edges */
	 {0} /* null-terminating sentine */},
	bmo_beautify_fill_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

/*
 * Triangle Fill
 *
 * Fill edges with triangles
 */
static BMOpDefine bmo_triangle_fill_def = {
	"triangle_fill",
	{{BMO_OP_SLOT_ELEMENT_BUF, "edges"}, /* input edges */
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"}, /* new faces and edges */
	 {0} /* null-terminating sentine */},
	bmo_triangle_fill_exec,
	BMO_OP_FLAG_UNTAN_MULTIRES
};

/*
 * Solidify
 *
 * Turns a mesh into a shell with thickness
 */
static BMOpDefine bmo_solidify_def = {
	"solidify",
	{{BMO_OP_SLOT_ELEMENT_BUF, "geom"},
	 {BMO_OP_SLOT_FLT, "thickness"},
	 {BMO_OP_SLOT_ELEMENT_BUF, "geomout"},
	 {0}},
	bmo_solidify_face_region_exec,
	0
};

BMOpDefine *opdefines[] = {
	&bmo_split_def,
	&bmo_spin_def,
	&bmo_dupe_def,
	&bmo_del_def,
	&bmo_esubd_def,
	&bmo_triangulate_def,
	&bmo_dissolve_faces_def,
	&bmo_dissolve_edges_def,
	&bmo_dissolve_edge_loop_def,
	&bmo_dissolve_verts_def,
	&bmo_dissolve_limit_def,
	&bmo_extrude_face_region_def,
	&bmo_connectverts_def,
	&bmo_extrude_vert_indiv_def,
	&bmo_mesh_to_bmesh_def,
	&bmo_object_load_bmesh_def,
	&bmo_transform_def,
	&bmo_translate_def,
	&bmo_rotate_def,
	&bmo_edgenet_fill_def,
	&bmo_contextual_create_def,
	&bmo_makevert_def,
	&bmo_weldverts_def,
	&bmo_removedoubles_def,
	&bmo_finddoubles_def,
	&bmo_mirror_def,
	&bmo_edgebisect_def,
	&bmo_reversefaces_def,
	&bmo_edgerotate_def,
	&bmo_regionextend_def,
	&bmo_righthandfaces_def,
	&bmo_vertexsmooth_def,
	&bmo_extrude_edge_only_def,
	&bmo_extrude_indivface_def,
	&bmo_collapse_uvs_def,
	&bmo_pointmerge_def,
	&bmo_collapse_def,
	&bmo_similarfaces_def,
	&bmo_similaredges_def,
	&bmo_similarverts_def,
	&bmo_pointmerge_facedata_def,
	&bmo_vert_average_facedata_def,
	&bmo_face_rotateuvs_def,
	&bmo_bmesh_to_mesh_def,
	&bmo_face_reverseuvs_def,
	&bmo_edgenet_prepare_def,
	&bmo_face_rotatecolors_def,
	&bmo_face_reversecolors_def,
	&bmo_vertexshortestpath_def,
	&bmo_scale_def,
	&bmo_edgesplit_def,
	&bmo_automerge_def,
	&bmo_create_uvsphere_def,
	&bmo_create_grid_def,
	&bmo_create_icosphere_def,
	&bmo_create_monkey_def,
	&bmo_create_cube_def,
	&bmo_create_circle_def,
	&bmo_create_cone_def,
	&bmo_join_triangles_def,
	&bmo_bevel_def,
	&bmo_beautify_fill_def,
	&bmo_triangle_fill_def,
	&bmo_bridge_loops_def,
	&bmo_solidify_def,
};

int bmesh_total_ops = (sizeof(opdefines) / sizeof(void *));
