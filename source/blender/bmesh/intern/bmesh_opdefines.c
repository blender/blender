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
 * (e.g. verts.out), for double-type slots, use the two type names plus "out",
 * (e.g. vertfaces.out), for three-type slots, use geom.  note that you can also
 * use more esoteric names (e.g. geom_skirt.out) so long as the comment next to the
 * slot definition tells you what types of elements are in it.
 *
 */

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h"

/* The formatting of these bmesh operators is parsed by
 * 'doc/python_api/rst_from_bmesh_opdefines.py'
 * for use in python docs, so reStructuredText may be used
 * rather then doxygen syntax.
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
 * are extracted from comments next to them.
 *
 * eg:
 *     {BMO_OP_SLOT_ELEMENT_BUF, "geom.out"},  """ output slot, boundary region """
 *
 * ... or:
 *
 * """ output slot, boundary region """
 *     {BMO_OP_SLOT_ELEMENT_BUF, "geom.out"},
 *
 * Both are acceptable.
 * note that '//' comments are ignored.
 */

/*
 * Vertex Smooth.
 *
 * Smooths vertices by using a basic vertex averaging scheme.
 */
static BMOpDefine bmo_smooth_vert_def = {
	"smooth_vert",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},    /* input vertices */
	 {"factor", BMO_OP_SLOT_FLT},           /* smoothing factor */
	 {"mirror_clip_x", BMO_OP_SLOT_BOOL},   /* set vertices close to the x axis before the operation to 0 */
	 {"mirror_clip_y", BMO_OP_SLOT_BOOL},   /* set vertices close to the y axis before the operation to 0 */
	 {"mirror_clip_z", BMO_OP_SLOT_BOOL},   /* set vertices close to the z axis before the operation to 0 */
	 {"clip_dist",  BMO_OP_SLOT_FLT},       /* clipping threshold for the above three slots */
	 {"use_axis_x", BMO_OP_SLOT_BOOL},      /* smooth vertices along X axis */
	 {"use_axis_y", BMO_OP_SLOT_BOOL},      /* smooth vertices along Y axis */
	 {"use_axis_z", BMO_OP_SLOT_BOOL},      /* smooth vertices along Z axis */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_smooth_vert_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Vertex Smooth Laplacian.
 *
 * Smooths vertices by using Laplacian smoothing propose by.
 * Desbrun, et al. Implicit Fairing of Irregular Meshes using Diffusion and Curvature Flow.
 */
static BMOpDefine bmo_smooth_laplacian_vert_def = {
	"smooth_laplacian_vert",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},    /* input vertices */
	 {"lambda_factor", BMO_OP_SLOT_FLT},           /* lambda param */
	 {"lambda_border", BMO_OP_SLOT_FLT},    /* lambda param in border */
	 {"use_x", BMO_OP_SLOT_BOOL},           /* Smooth object along X axis */
	 {"use_y", BMO_OP_SLOT_BOOL},           /* Smooth object along Y axis */
	 {"use_z", BMO_OP_SLOT_BOOL},           /* Smooth object along Z axis */
	 {"preserve_volume", BMO_OP_SLOT_BOOL}, /* Apply volume preservation after smooth */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_smooth_laplacian_vert_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Right-Hand Faces.
 *
 * Computes an "outside" normal for the specified input faces.
 */
static BMOpDefine bmo_recalc_face_normals_def = {
	"recalc_face_normals",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_recalc_face_normals_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Planar Faces.
 *
 * Iteratively flatten faces.
 */
static BMOpDefine bmo_planar_faces_def = {
	"planar_faces",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input geometry. */
	 {"iterations", BMO_OP_SLOT_INT},  /* Number of times to flatten faces (for when connected faces are used) */
	 {"factor", BMO_OP_SLOT_FLT},  /* Influence for making planar each iteration */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* output slot, computed boundary geometry. */
	 {{'\0'}},
	},
	bmo_planar_faces_exec,
	(BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Region Extend.
 *
 * used to implement the select more/less tools.
 * this puts some geometry surrounding regions of
 * geometry in geom into geom.out.
 *
 * if use_faces is 0 then geom.out spits out verts and edges,
 * otherwise it spits out faces.
 */
static BMOpDefine bmo_region_extend_def = {
	"region_extend",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},     /* input geometry */
	 {"use_contract", BMO_OP_SLOT_BOOL},    /* find boundary inside the regions, not outside. */
	 {"use_faces", BMO_OP_SLOT_BOOL},       /* extend from faces instead of edges */
	 {"use_face_step", BMO_OP_SLOT_BOOL},   /* step over connected faces */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* output slot, computed boundary geometry. */
	 {{'\0'}},
	},
	bmo_region_extend_exec,
	(BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Edge Rotate.
 *
 * Rotates edges topologically.  Also known as "spin edge" to some people.
 * Simple example: ``[/] becomes [|] then [\]``.
 */
static BMOpDefine bmo_rotate_edges_def = {
	"rotate_edges",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input edges */
	 {"use_ccw", BMO_OP_SLOT_BOOL},         /* rotate edge counter-clockwise if true, otherwise clockwise */
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* newly spun edges */
	 {{'\0'}},
	},
	bmo_rotate_edges_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Reverse Faces.
 *
 * Reverses the winding (vertex order) of faces.
 * This has the effect of flipping the normal.
 */
static BMOpDefine bmo_reverse_faces_def = {
	"reverse_faces",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {"flip_multires", BMO_OP_SLOT_BOOL},  /* maintain multi-res offset */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_reverse_faces_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Edge Bisect.
 *
 * Splits input edges (but doesn't do anything else).
 * This creates a 2-valence vert.
 */
static BMOpDefine bmo_bisect_edges_def = {
	"bisect_edges",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	 {"cuts", BMO_OP_SLOT_INT}, /* number of cuts */
	 {"edge_percents", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_FLT}},
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom_split.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* newly created vertices and edges */
	 {{'\0'}},
	},
	bmo_bisect_edges_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Mirror.
 *
 * Mirrors geometry along an axis.  The resulting geometry is welded on using
 * merge_dist.  Pairs of original/mirrored vertices are welded using the merge_dist
 * parameter (which defines the minimum distance for welding to happen).
 */
static BMOpDefine bmo_mirror_def = {
	"mirror",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},     /* input geometry */
	 {"matrix",          BMO_OP_SLOT_MAT},   /* matrix defining the mirror transformation */
	 {"merge_dist",      BMO_OP_SLOT_FLT},   /* maximum distance for merging.  does no merging if 0. */
	 {"axis",            BMO_OP_SLOT_INT},   /* the axis to use, 0, 1, or 2 for x, y, z */
	 {"mirror_u",        BMO_OP_SLOT_BOOL},  /* mirror UVs across the u axis */
	 {"mirror_v",        BMO_OP_SLOT_BOOL},  /* mirror UVs across the v axis */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* output geometry, mirrored */
	 {{'\0'}},
	},
	bmo_mirror_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Find Doubles.
 *
 * Takes input verts and find vertices they should weld to.
 * Outputs a mapping slot suitable for use with the weld verts bmop.
 *
 * If keep_verts is used, vertices outside that set can only be merged
 * with vertices in that set.
 */
static BMOpDefine bmo_find_doubles_def = {
	"find_doubles",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* input vertices */
	 {"keep_verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* list of verts to keep */
	 {"dist",         BMO_OP_SLOT_FLT}, /* minimum distance */
	 {{'\0'}},
	},
	/* slots_out */
	{{"targetmap.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {{'\0'}},
	},
	bmo_find_doubles_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Remove Doubles.
 *
 * Finds groups of vertices closer then dist and merges them together,
 * using the weld verts bmop.
 */
static BMOpDefine bmo_remove_doubles_def = {
	"remove_doubles",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* input verts */
	 {"dist",         BMO_OP_SLOT_FLT}, /* minimum distance */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_remove_doubles_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Auto Merge.
 *
 * Finds groups of vertices closer then **dist** and merges them together,
 * using the weld verts bmop.  The merges must go from a vert not in
 * **verts** to one in **verts**.
 */
static BMOpDefine bmo_automerge_def = {
	"automerge",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* input verts */
	 {"dist",         BMO_OP_SLOT_FLT}, /* minimum distance */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_automerge_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Collapse Connected.
 *
 * Collapses connected vertices
 */
static BMOpDefine bmo_collapse_def = {
	"collapse",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	 {"uvs", BMO_OP_SLOT_BOOL}, /* also collapse UVs and such */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_collapse_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Face-Data Point Merge.
 *
 * Merge uv/vcols at a specific vertex.
 */
static BMOpDefine bmo_pointmerge_facedata_def = {
	"pointmerge_facedata",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},    /* input vertices */
	 {"vert_snap", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | (int)BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE}},    /* snap vertex */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_pointmerge_facedata_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Average Vertices Facevert Data.
 *
 * Merge uv/vcols associated with the input vertices at
 * the bounding box center. (I know, it's not averaging but
 * the vert_snap_to_bb_center is just too long).
 */
static BMOpDefine bmo_average_vert_facedata_def = {
	"average_vert_facedata",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* input vertices */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_average_vert_facedata_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Point Merge.
 *
 * Merge verts together at a point.
 */
static BMOpDefine bmo_pointmerge_def = {
	"pointmerge",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* input vertices (all verts will be merged into the first). */
	 {"merge_co", BMO_OP_SLOT_VEC},  /* Position to merge at. */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_pointmerge_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Collapse Connected UV's.
 *
 * Collapses connected UV vertices.
 */
static BMOpDefine bmo_collapse_uvs_def = {
	"collapse_uvs",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_collapse_uvs_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Weld Verts.
 *
 * Welds verts together (kind-of like remove doubles, merge, etc, all of which
 * use or will use this bmop).  You pass in mappings from vertices to the vertices
 * they weld with.
 */
static BMOpDefine bmo_weld_verts_def = {
	"weld_verts",
	/* slots_in */
	/* maps welded vertices to verts they should weld to */
	{{"targetmap", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_weld_verts_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Make Vertex.
 *
 * Creates a single vertex; this bmop was necessary
 * for click-create-vertex.
 */
static BMOpDefine bmo_create_vert_def = {
	"create_vert",
	/* slots_in */
	{{"co", BMO_OP_SLOT_VEC},  /* the coordinate of the new vert */
	 {{'\0'}},
	},
	/* slots_out */
	{{"vert.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* the new vert */
	 {{'\0'}},
	},
	bmo_create_vert_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Join Triangles.
 *
 * Tries to intelligently join triangles according
 * to angle threshold and delimiters.
 */
static BMOpDefine bmo_join_triangles_def = {
	"join_triangles",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input geometry. */
	 {"cmp_seam", BMO_OP_SLOT_BOOL},
	 {"cmp_sharp", BMO_OP_SLOT_BOOL},
	 {"cmp_uvs", BMO_OP_SLOT_BOOL},
	 {"cmp_vcols", BMO_OP_SLOT_BOOL},
	 {"cmp_materials", BMO_OP_SLOT_BOOL},
	 {"angle_face_threshold", BMO_OP_SLOT_FLT},
	 {"angle_shape_threshold", BMO_OP_SLOT_FLT},
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},  /* joined faces */
	 {{'\0'}},
	},
	bmo_join_triangles_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Contextual Create.
 *
 * This is basically F-key, it creates
 * new faces from vertices, makes stuff from edge nets,
 * makes wire edges, etc.  It also dissolves faces.
 *
 * Three verts become a triangle, four become a quad.  Two
 * become a wire edge.
 */
static BMOpDefine bmo_contextual_create_def = {
	"contextual_create",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},     /* input geometry. */
	 {"mat_nr",         BMO_OP_SLOT_INT},   /* material to use */
	 {"use_smooth",        BMO_OP_SLOT_BOOL}, /* smooth to use */
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* newly-made face(s) */
	/* note, this is for stand-alone edges only, not edges which are apart of newly created faces */
	 {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* newly-made edge(s) */
	 {{'\0'}},
	},
	bmo_contextual_create_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Bridge edge loops with faces.
 */
static BMOpDefine bmo_bridge_loops_def = {
	"bridge_loops",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	 {"use_pairs",          BMO_OP_SLOT_BOOL},
	 {"use_cyclic",         BMO_OP_SLOT_BOOL},
	 {"use_merge",          BMO_OP_SLOT_BOOL},
	 {"merge_factor",       BMO_OP_SLOT_FLT},
	 {"twist_offset",       BMO_OP_SLOT_INT},
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* new faces */
	 {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* new edges */
	 {{'\0'}},
	},
	bmo_bridge_loops_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Grid Fill.
 *
 * Create faces defined by 2 disconnected edge loops (which share edges).
 */
static BMOpDefine bmo_grid_fill_def = {
	"grid_fill",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	/* restricts edges to groups.  maps edges to integer */
	 {"mat_nr",         BMO_OP_SLOT_INT},      /* material to use */
	 {"use_smooth",     BMO_OP_SLOT_BOOL},     /* smooth state to use */
	 {"use_interp_simple", BMO_OP_SLOT_BOOL},  /* use simple interpolation */
	 {{'\0'}},
	},
	/* slots_out */
	/* maps new faces to the group numbers they came from */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},     /* new faces */
	 {{'\0'}},
	},
	bmo_grid_fill_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};


/*
 * Fill Holes.
 *
 * Fill boundary edges with faces, copying surrounding customdata.
 */
static BMOpDefine bmo_holes_fill_def = {
	"holes_fill",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	 {"sides",          BMO_OP_SLOT_INT},   /* number of face sides to fill */
	 {{'\0'}},
	},
	/* slots_out */
	/* maps new faces to the group numbers they came from */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},     /* new faces */
	 {{'\0'}},
	},
	bmo_holes_fill_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};


/*
 * Face Attribute Fill.
 *
 * Fill in faces with data from adjacent faces.
 */
static BMOpDefine bmo_face_attribute_fill_def = {
	"face_attribute_fill",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},     /* input faces */
	 {"use_normals",        BMO_OP_SLOT_BOOL},  /* copy face winding */
	 {"use_data",           BMO_OP_SLOT_BOOL},  /* copy face data */
	 {{'\0'}},
	},
	/* slots_out */
	/* maps new faces to the group numbers they came from */
	{{"faces_fail.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},     /* faces that could not be handled */
	 {{'\0'}},
	},
	bmo_face_attribute_fill_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Edge Loop Fill.
 *
 * Create faces defined by one or more non overlapping edge loops.
 */
static BMOpDefine bmo_edgeloop_fill_def = {
	"edgeloop_fill",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	/* restricts edges to groups.  maps edges to integer */
	 {"mat_nr",         BMO_OP_SLOT_INT},      /* material to use */
	 {"use_smooth",        BMO_OP_SLOT_BOOL},  /* smooth state to use */
	 {{'\0'}},
	},
	/* slots_out */
	/* maps new faces to the group numbers they came from */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},     /* new faces */
	 {{'\0'}},
	},
	bmo_edgeloop_fill_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};


/*
 * Edge Net Fill.
 *
 * Create faces defined by enclosed edges.
 */
static BMOpDefine bmo_edgenet_fill_def = {
	"edgenet_fill",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input edges */
	 {"mat_nr",          BMO_OP_SLOT_INT},  /* material to use */
	 {"use_smooth",      BMO_OP_SLOT_BOOL}, /* smooth state to use */
	 {"sides",           BMO_OP_SLOT_INT},  /* number of sides */
	 {{'\0'}},
	},
	/* slots_out */
	/* maps new faces to the group numbers they came from */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},  /* new faces */
	 {{'\0'}},
	},
	bmo_edgenet_fill_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Edgenet Prepare.
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
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input edges */
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},  /* new edges */
	 {{'\0'}},
	},
	bmo_edgenet_prepare_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Rotate.
 *
 * Rotate vertices around a center, using a 3x3 rotation matrix.
 */
static BMOpDefine bmo_rotate_def = {
	"rotate",
	/* slots_in */
	{{"cent",            BMO_OP_SLOT_VEC},  /* center of rotation */
	 {"matrix",          BMO_OP_SLOT_MAT},  /* matrix defining rotation */
	 {"verts",           BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* input vertices */
	 {"space",           BMO_OP_SLOT_MAT},  /* matrix to define the space (typically object matrix) */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_rotate_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Translate.
 *
 * Translate vertices by an offset.
 */
static BMOpDefine bmo_translate_def = {
	"translate",
	/* slots_in */
	{{"vec", BMO_OP_SLOT_VEC},  /* translation offset */
	 {"space", BMO_OP_SLOT_MAT},  /* matrix to define the space (typically object matrix) */
	 {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* input vertices */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_translate_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Scale.
 *
 * Scales vertices by an offset.
 */
static BMOpDefine bmo_scale_def = {
	"scale",
	/* slots_in */
	{{"vec", BMO_OP_SLOT_VEC},  /* scale factor */
	 {"space", BMO_OP_SLOT_MAT},  /* matrix to define the space (typically object matrix) */
	 {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* input vertices */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_scale_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};


/*
 * Transform.
 *
 * Transforms a set of vertices by a matrix.  Multiplies
 * the vertex coordinates with the matrix.
 */
static BMOpDefine bmo_transform_def = {
	"transform",
	/* slots_in */
	{{"matrix",          BMO_OP_SLOT_MAT},  /* transform matrix */
	 {"space",           BMO_OP_SLOT_MAT},  /* matrix to define the space (typically object matrix) */
	 {"verts",           BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* input vertices */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_transform_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Object Load BMesh.
 *
 * Loads a bmesh into an object/mesh.  This is a "private"
 * bmop.
 */
static BMOpDefine bmo_object_load_bmesh_def = {
	"object_load_bmesh",
	/* slots_in */
	{{"scene", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_SCENE}},
	 {"object", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_OBJECT}},
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_object_load_bmesh_exec,
	(BMO_OPTYPE_FLAG_NOP),
};


/*
 * BMesh to Mesh.
 *
 * Converts a bmesh to a Mesh.  This is reserved for exiting editmode.
 */
static BMOpDefine bmo_bmesh_to_mesh_def = {
	"bmesh_to_mesh",
	/* slots_in */
	{
	 /* pointer to a mesh structure to fill in */
	 {"mesh", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_MESH}},
	/* pointer to an object structure */
	 {"object", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_OBJECT}},
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_bmesh_to_mesh_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Mesh to BMesh.
 *
 * Load the contents of a mesh into the bmesh.  this bmop is private, it's
 * reserved exclusively for entering editmode.
 */
static BMOpDefine bmo_mesh_to_bmesh_def = {
	"mesh_to_bmesh",
	/* slots_in */
	{
	 /* pointer to a Mesh structure */
	 {"mesh", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_MESH}},
	/* pointer to an Object structure */
	 {"object", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_OBJECT}},
	 {"use_shapekey", BMO_OP_SLOT_BOOL},  /* load active shapekey coordinates into verts */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_mesh_to_bmesh_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Individual Face Extrude.
 *
 * Extrudes faces individually.
 */
static BMOpDefine bmo_extrude_discrete_faces_def = {
	"extrude_discrete_faces",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},     /* input faces */
	 {"use_normal_flip", BMO_OP_SLOT_BOOL},  /* Create faces with reversed direction. */
	 {"use_select_history", BMO_OP_SLOT_BOOL},  /* pass to duplicate */
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},   /* output faces */
	 {{'\0'}},
	},
	bmo_extrude_discrete_faces_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Extrude Only Edges.
 *
 * Extrudes Edges into faces, note that this is very simple, there's no fancy
 * winged extrusion.
 */
static BMOpDefine bmo_extrude_edge_only_def = {
	"extrude_edge_only",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input vertices */
	 {"use_normal_flip", BMO_OP_SLOT_BOOL},  /* Create faces with reversed direction. */
	 {"use_select_history", BMO_OP_SLOT_BOOL},  /* pass to duplicate */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},  /* output geometry */
	 {{'\0'}},
	},
	bmo_extrude_edge_only_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Individual Vertex Extrude.
 *
 * Extrudes wire edges from vertices.
 */
static BMOpDefine bmo_extrude_vert_indiv_def = {
	"extrude_vert_indiv",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},    /* input vertices */
	 {"use_select_history", BMO_OP_SLOT_BOOL},  /* pass to duplicate */
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},  /* output wire edges */
	 {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* output vertices */
	 {{'\0'}},
	},
	bmo_extrude_vert_indiv_exec,
	(BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts.
 *
 * Split faces by adding edges that connect **verts**.
 */
static BMOpDefine bmo_connect_verts_def = {
	"connect_verts",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
	 {"faces_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {"check_degenerate", BMO_OP_SLOT_BOOL},  /* prevent splits with overlaps & intersections */
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {{'\0'}},
	},
	bmo_connect_verts_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts to form Convex Faces.
 *
 * Ensures all faces are convex **faces**.
 */
static BMOpDefine bmo_connect_verts_concave_def = {
	"connect_verts_concave",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	bmo_connect_verts_concave_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts Across non Planer Faces.
 *
 * Split faces by connecting edges along non planer **faces**.
 */
static BMOpDefine bmo_connect_verts_nonplanar_def = {
	"connect_verts_nonplanar",
	/* slots_in */
	{{"angle_limit", BMO_OP_SLOT_FLT}, /* total rotation angle (radians) */
	 {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	bmo_connect_verts_nonplanar_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts.
 *
 * Split faces by adding edges that connect **verts**.
 */
static BMOpDefine bmo_connect_vert_pair_def = {
	"connect_vert_pair",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
	 {"verts_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
	 {"faces_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {{'\0'}},
	},
	bmo_connect_vert_pair_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};


/*
 * Extrude Faces.
 *
 * Extrude operator (does not transform)
 */
static BMOpDefine bmo_extrude_face_region_def = {
	"extrude_face_region",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},     /* edges and faces */
	 {"edges_exclude", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_EMPTY}},
	 {"use_keep_orig", BMO_OP_SLOT_BOOL},   /* keep original geometry (requires ``geom`` to include edges). */
	 {"use_normal_flip", BMO_OP_SLOT_BOOL},  /* Create faces with reversed direction. */
	 {"use_select_history", BMO_OP_SLOT_BOOL},  /* pass to duplicate */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {{'\0'}},
	},
	bmo_extrude_face_region_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Dissolve Verts.
 */
static BMOpDefine bmo_dissolve_verts_def = {
	"dissolve_verts",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
	 {"use_face_split", BMO_OP_SLOT_BOOL},
	 {"use_boundary_tear", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_dissolve_verts_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Dissolve Edges.
 */
static BMOpDefine bmo_dissolve_edges_def = {
	"dissolve_edges",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {"use_verts", BMO_OP_SLOT_BOOL},  /* dissolve verts left between only 2 edges. */
	 {"use_face_split", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	/* slots_out */
	{{"region.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	bmo_dissolve_edges_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Dissolve Faces.
 */
static BMOpDefine bmo_dissolve_faces_def = {
	"dissolve_faces",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {"use_verts", BMO_OP_SLOT_BOOL},  /* dissolve verts left between only 2 edges. */
	 {{'\0'}},
	},
	/* slots_out */
	{{"region.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}},
	},
	bmo_dissolve_faces_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Limited Dissolve.
 *
 * Dissolve planar faces and co-linear edges.
 */
static BMOpDefine bmo_dissolve_limit_def = {
	"dissolve_limit",
	/* slots_in */
	{{"angle_limit", BMO_OP_SLOT_FLT}, /* total rotation angle (radians) */
	 {"use_dissolve_boundaries", BMO_OP_SLOT_BOOL},
	 {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
	 {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {"delimit", BMO_OP_SLOT_INT},
	 {{'\0'}},
	},
	/* slots_out */
	{{"region.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {{'\0'}}},
	bmo_dissolve_limit_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Degenerate Dissolve.
 *
 * Dissolve edges with no length, faces with no area.
 */
static BMOpDefine bmo_dissolve_degenerate_def = {
	"dissolve_degenerate",
	/* slots_in */
	{{"dist", BMO_OP_SLOT_FLT}, /* minimum distance to consider degenerate */
	 {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {{'\0'}},
	},
	/* slots_out */
	{{{'\0'}}},
	bmo_dissolve_degenerate_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Triangulate.
 */
static BMOpDefine bmo_triangulate_def = {
	"triangulate",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {"quad_method", BMO_OP_SLOT_INT},
	 {"ngon_method", BMO_OP_SLOT_INT},
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {"face_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {"face_map_double.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},  /* duplicate faces */
	 {{'\0'}},
	},
	bmo_triangulate_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Un-Subdivide.
 *
 * Reduce detail in geometry containing grids.
 */
static BMOpDefine bmo_unsubdivide_def = {
	"unsubdivide",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* input vertices */
	 {"iterations", BMO_OP_SLOT_INT},
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_unsubdivide_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Subdivide Edges.
 *
 * Advanced operator for subdividing edges
 * with options for face patterns, smoothing and randomization.
 */
static BMOpDefine bmo_subdivide_edges_def = {
	"subdivide_edges",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
	 {"smooth", BMO_OP_SLOT_FLT},
	 {"smooth_falloff", BMO_OP_SLOT_INT}, /* SUBD_FALLOFF_ROOT and friends */
	 {"fractal", BMO_OP_SLOT_FLT},
	 {"along_normal", BMO_OP_SLOT_FLT},
	 {"cuts", BMO_OP_SLOT_INT},
	 {"seed", BMO_OP_SLOT_INT},
	 {"custom_patterns", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL}},  /* uses custom pointers */
	 {"edge_percents", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_FLT}},

	 {"quad_corner_type",  BMO_OP_SLOT_INT}, /* quad corner type, see bmesh_operators.h */
	 {"use_grid_fill", BMO_OP_SLOT_BOOL},   /* fill in fully-selected faces with a grid */
	 {"use_single_edge", BMO_OP_SLOT_BOOL}, /* tessellate the case of one edge selected in a quad or triangle */
	 {"use_only_quads", BMO_OP_SLOT_BOOL},  /* only subdivide quads (for loopcut) */
	 {"use_sphere", BMO_OP_SLOT_BOOL},     /* for making new primitives only */
	 {"use_smooth_even", BMO_OP_SLOT_BOOL},  /* maintain even offset when smoothing */
	 {{'\0'}},
	},
	/* slots_out */
	{/* these next three can have multiple types of elements in them */
	 {"geom_inner.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"geom_split.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* contains all output geometry */
	 {{'\0'}},
	},
	bmo_subdivide_edges_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Subdivide Edge-Ring.
 *
 * Take an edge-ring, and subdivide with interpolation options.
 */
static BMOpDefine bmo_subdivide_edgering_def = {
	"subdivide_edgering",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* input vertices */
	 {"interp_mode", BMO_OP_SLOT_INT},
	 {"smooth", BMO_OP_SLOT_FLT},
	 {"cuts", BMO_OP_SLOT_INT},
	 {"profile_shape", BMO_OP_SLOT_INT},
	 {"profile_shape_factor", BMO_OP_SLOT_FLT},
	 {{'\0'}},
	},
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* output faces */
	 {{'\0'}}},
	bmo_subdivide_edgering_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Bisect Plane.
 *
 * Bisects the mesh by a plane (cut the mesh in half).
 */
static BMOpDefine bmo_bisect_plane_def = {
	"bisect_plane",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"dist",         BMO_OP_SLOT_FLT},     /* minimum distance when testing if a vert is exactly on the plane */
	 {"plane_co", BMO_OP_SLOT_VEC},         /* point on the plane */
	 {"plane_no", BMO_OP_SLOT_VEC},         /* direction of the plane */
	 {"use_snap_center", BMO_OP_SLOT_BOOL},  /* snap axis aligned verts to the center */
	 {"clear_outer",   BMO_OP_SLOT_BOOL},    /* when enabled. remove all geometry on the positive side of the plane */
	 {"clear_inner",   BMO_OP_SLOT_BOOL},    /* when enabled. remove all geometry on the negative side of the plane */
	 {{'\0'}},
	},
	{{"geom_cut.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE}},  /* output geometry aligned with the plane (new and existing) */
	 {"geom.out",     BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},  /* input and output geometry (result of cut)  */
	 {{'\0'}}},
	bmo_bisect_plane_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Delete Geometry.
 *
 * Utility operator to delete geometry.
 */
static BMOpDefine bmo_delete_def = {
	"delete",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"context", BMO_OP_SLOT_INT},  /* enum DEL_VERTS ... */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_delete_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Duplicate Geometry.
 *
 * Utility operator to duplicate geometry,
 * optionally into a destination mesh.
 */
static BMOpDefine bmo_duplicate_def = {
	"duplicate",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	/* destination bmesh, if NULL will use current on */
	 {"dest", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_BMESH}},
	 {"use_select_history", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom_orig.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	/* facemap maps from source faces to dupe
	 * faces, and from dupe faces to source faces */
	 {"vert_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {"edge_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {"face_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {"boundary_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {"isovert_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {{'\0'}},
	},
	bmo_duplicate_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Split Off Geometry.
 *
 * Disconnect geometry from adjacent edges and faces,
 * optionally into a destination mesh.
 */
static BMOpDefine bmo_split_def = {
	"split",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	/* destination bmesh, if NULL will use current one */
	 {"dest", BMO_OP_SLOT_PTR, {(int)BMO_OP_SLOT_SUBTYPE_PTR_BMESH}},
	 {"use_only_faces", BMO_OP_SLOT_BOOL},  /* when enabled. don't duplicate loose verts/edges */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"boundary_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {"isovert_map.out", BMO_OP_SLOT_MAPPING, {(int)BMO_OP_SLOT_SUBTYPE_MAP_ELEM}},
	 {{'\0'}},
	},
	bmo_split_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Spin.
 *
 * Extrude or duplicate geometry a number of times,
 * rotating and possibly translating after each step
 */
static BMOpDefine bmo_spin_def = {
	"spin",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"cent", BMO_OP_SLOT_VEC},             /* rotation center */
	 {"axis", BMO_OP_SLOT_VEC},             /* rotation axis */
	 {"dvec", BMO_OP_SLOT_VEC},             /* translation delta per step */
	 {"angle", BMO_OP_SLOT_FLT},            /* total rotation angle (radians) */
	 {"space", BMO_OP_SLOT_MAT},            /* matrix to define the space (typically object matrix) */
	 {"steps", BMO_OP_SLOT_INT},            /* number of steps */
	 {"use_merge", BMO_OP_SLOT_BOOL},       /* Merge first/last when the angle is a full revolution. */
	 {"use_normal_flip", BMO_OP_SLOT_BOOL}, /* Create faces with reversed direction. */
	 {"use_duplicate", BMO_OP_SLOT_BOOL},   /* duplicate or extrude? */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom_last.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* result of last step */
	 {{'\0'}},
	},
	bmo_spin_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};


/*
 * Similar Faces Search.
 *
 * Find similar faces (area/material/perimeter, ...).
 */
static BMOpDefine bmo_similar_faces_def = {
	"similar_faces",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {"type", BMO_OP_SLOT_INT},             /* type of selection */
	 {"thresh", BMO_OP_SLOT_FLT},           /* threshold of selection */
	 {"compare", BMO_OP_SLOT_INT},          /* comparison method */
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},  /* output faces */
	 {{'\0'}},
	},
	bmo_similar_faces_exec,
	(BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Similar Edges Search.
 *
 * Find similar edges (length, direction, edge, seam, ...).
 */
static BMOpDefine bmo_similar_edges_def = {
	"similar_edges",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input edges */
	 {"type", BMO_OP_SLOT_INT},             /* type of selection */
	 {"thresh", BMO_OP_SLOT_FLT},           /* threshold of selection */
	 {"compare", BMO_OP_SLOT_INT},          /* comparison method */
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},  /* output edges */
	 {{'\0'}},
	},
	bmo_similar_edges_exec,
	(BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Similar Verts Search.
 *
 * Find similar vertices (normal, face, vertex group, ...).
 */
static BMOpDefine bmo_similar_verts_def = {
	"similar_verts",
	/* slots_in */
	{{"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},    /* input vertices */
	 {"type", BMO_OP_SLOT_INT},             /* type of selection */
	 {"thresh", BMO_OP_SLOT_FLT},           /* threshold of selection */
	 {"compare", BMO_OP_SLOT_INT},          /* comparison method */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},  /* output vertices */
	 {{'\0'}},
	},
	bmo_similar_verts_exec,
	(BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * UV Rotation.
 *
 * Cycle the loop UV's
 */
static BMOpDefine bmo_rotate_uvs_def = {
	"rotate_uvs",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {"use_ccw", BMO_OP_SLOT_BOOL},         /* rotate counter-clockwise if true, otherwise clockwise */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_rotate_uvs_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * UV Reverse.
 *
 * Reverse the UV's
 */
static BMOpDefine bmo_reverse_uvs_def = {
	"reverse_uvs",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_reverse_uvs_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Color Rotation.
 *
 * Cycle the loop colors
 */
static BMOpDefine bmo_rotate_colors_def = {
	"rotate_colors",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {"use_ccw", BMO_OP_SLOT_BOOL},         /* rotate counter-clockwise if true, otherwise clockwise */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_rotate_colors_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Color Reverse
 *
 * Reverse the loop colors.
 */
static BMOpDefine bmo_reverse_colors_def = {
	"reverse_colors",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {{'\0'}},
	},
	{{{'\0'}}},  /* no output */
	bmo_reverse_colors_exec,
	(BMO_OPTYPE_FLAG_NOP),
};

/*
 * Edge Split.
 *
 * Disconnects faces along input edges.
 */
static BMOpDefine bmo_split_edges_def = {
	"split_edges",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input edges */
	 /* needed for vertex rip so we can rip only half an edge at a boundary wich would otherwise split off */
	 {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},    /* optional tag verts, use to have greater control of splits */
	 {"use_verts",        BMO_OP_SLOT_BOOL}, /* use 'verts' for splitting, else just find verts to split from edges */
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* old output disconnected edges */
	 {{'\0'}},
	},
	bmo_split_edges_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Grid.
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_grid_def = {
	"create_grid",
	/* slots_in */
	{{"x_segments",      BMO_OP_SLOT_INT},  /* number of x segments */
	 {"y_segments",      BMO_OP_SLOT_INT},  /* number of y segments */
	 {"size",            BMO_OP_SLOT_FLT},  /* size of the grid */
	 {"matrix",          BMO_OP_SLOT_MAT},  /* matrix to multiply the new geometry with */
	 {"calc_uvs",        BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_grid_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create UV Sphere.
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_uvsphere_def = {
	"create_uvsphere",
	/* slots_in */
	{{"u_segments",      BMO_OP_SLOT_INT}, /* number of u segments */
	 {"v_segments",      BMO_OP_SLOT_INT}, /* number of v segment */
	 {"diameter",        BMO_OP_SLOT_FLT}, /* diameter */
	 {"matrix",          BMO_OP_SLOT_MAT}, /* matrix to multiply the new geometry with */
	 {"calc_uvs",        BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_uvsphere_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Ico-Sphere.
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_icosphere_def = {
	"create_icosphere",
	/* slots_in */
	{{"subdivisions",    BMO_OP_SLOT_INT}, /* how many times to recursively subdivide the sphere */
	 {"diameter",        BMO_OP_SLOT_FLT}, /* diameter */
	 {"matrix",          BMO_OP_SLOT_MAT}, /* matrix to multiply the new geometry with */
	 {"calc_uvs",        BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_icosphere_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Suzanne.
 *
 * Creates a monkey (standard blender primitive).
 */
static BMOpDefine bmo_create_monkey_def = {
	"create_monkey",
	/* slots_in */
	{{"matrix",    BMO_OP_SLOT_MAT},  /* matrix to multiply the new geometry with */
	 {"calc_uvs",  BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_monkey_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Cone.
 *
 * Creates a cone with variable depth at both ends
 */
static BMOpDefine bmo_create_cone_def = {
	"create_cone",
	/* slots_in */
	{{"cap_ends",        BMO_OP_SLOT_BOOL},  /* whether or not to fill in the ends with faces */
	 {"cap_tris",        BMO_OP_SLOT_BOOL},  /* fill ends with triangles instead of ngons */
	 {"segments",        BMO_OP_SLOT_INT},
	 {"diameter1",       BMO_OP_SLOT_FLT},  /* diameter of one end */
	 {"diameter2",       BMO_OP_SLOT_FLT},  /* diameter of the opposite */
	 {"depth",           BMO_OP_SLOT_FLT},  /* distance between ends */
	 {"matrix",          BMO_OP_SLOT_MAT},  /* matrix to multiply the new geometry with */
	 {"calc_uvs",        BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_cone_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Creates a Circle.
 */
static BMOpDefine bmo_create_circle_def = {
	"create_circle",
	/* slots_in */
	{{"cap_ends",        BMO_OP_SLOT_BOOL},  /* whether or not to fill in the ends with faces */
	 {"cap_tris",        BMO_OP_SLOT_BOOL},  /* fill ends with triangles instead of ngons */
	 {"segments",        BMO_OP_SLOT_INT},
	 {"radius",          BMO_OP_SLOT_FLT},  /* Radius of the circle. */
	 {"matrix",          BMO_OP_SLOT_MAT},  /* matrix to multiply the new geometry with */
	 {"calc_uvs",        BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_circle_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Cube
 *
 * Creates a cube.
 */
static BMOpDefine bmo_create_cube_def = {
	"create_cube",
	/* slots_in */
	{{"size",            BMO_OP_SLOT_FLT},  /* size of the cube */
	 {"matrix",          BMO_OP_SLOT_MAT},  /* matrix to multiply the new geometry with */
	 {"calc_uvs",        BMO_OP_SLOT_BOOL}, /* calculate default UVs */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},
	bmo_create_cube_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Bevel.
 *
 * Bevels edges and vertices
 */
static BMOpDefine bmo_bevel_def = {
	"bevel",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},     /* input edges and vertices */
	 {"offset", BMO_OP_SLOT_FLT},           /* amount to offset beveled edge */
	 {"offset_type", BMO_OP_SLOT_INT},      /* how to measure offset (enum) */
	 {"segments", BMO_OP_SLOT_INT},         /* number of segments in bevel */
	 {"profile", BMO_OP_SLOT_FLT},          /* profile shape, 0->1 (.5=>round) */
	 {"vertex_only", BMO_OP_SLOT_BOOL},     /* only bevel vertices, not edges */
	 {"clamp_overlap", BMO_OP_SLOT_BOOL},   /* do not allow beveled edges/vertices to overlap each other */
	 {"material", BMO_OP_SLOT_INT},         /* material for bevel faces, -1 means get from adjacent faces */
	 {"loop_slide", BMO_OP_SLOT_BOOL},      /* prefer to slide along edges to having even widths */
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* output faces */
	 {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* output edges */
	 {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {{'\0'}},
	},

	bmo_bevel_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Beautify Fill.
 *
 * Rotate edges to create more evenly spaced triangles.
 */
static BMOpDefine bmo_beautify_fill_def = {
	"beautify_fill",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* input faces */
	 {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* edges that can be flipped */
	 {"use_restrict_tag", BMO_OP_SLOT_BOOL}, /* restrict edge rotation to mixed tagged vertices */
	 {"method", BMO_OP_SLOT_INT}, /* method to define what is beautiful */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* new flipped faces and edges */
	 {{'\0'}},
	},
	bmo_beautify_fill_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Triangle Fill.
 *
 * Fill edges with triangles
 */
static BMOpDefine bmo_triangle_fill_def = {
	"triangle_fill",
	/* slots_in */
	{{"use_beauty", BMO_OP_SLOT_BOOL},
	 {"use_dissolve", BMO_OP_SLOT_BOOL},  /* dissolve resulting faces */
	 {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input edges */
	 {"normal", BMO_OP_SLOT_VEC},  /* optionally pass the fill normal to use */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}}, /* new faces and edges */
	 {{'\0'}},
	},
	bmo_triangle_fill_exec,
	(BMO_OPTYPE_FLAG_UNTAN_MULTIRES |
	 BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Solidify.
 *
 * Turns a mesh into a shell with thickness
 */
static BMOpDefine bmo_solidify_def = {
	"solidify",
	/* slots_in */
	{{"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"thickness", BMO_OP_SLOT_FLT},
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {{'\0'}},
	},
	bmo_solidify_face_region_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Face Inset (Individual).
 *
 * Insets individual faces.
 */
static BMOpDefine bmo_inset_individual_def = {
	"inset_individual",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {"thickness", BMO_OP_SLOT_FLT},
	 {"depth", BMO_OP_SLOT_FLT},
	 {"use_even_offset", BMO_OP_SLOT_BOOL},
	 {"use_interpolate", BMO_OP_SLOT_BOOL},
	 {"use_relative_offset", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* output faces */
	 {{'\0'}},
	},
	bmo_inset_individual_exec,
	/* caller needs to handle BMO_OPTYPE_FLAG_SELECT_FLUSH */
	(BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Face Inset (Regions).
 *
 * Inset or outset face regions.
 */
static BMOpDefine bmo_inset_region_def = {
	"inset_region",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},    /* input faces */
	 {"faces_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
	 {"use_boundary", BMO_OP_SLOT_BOOL},
	 {"use_even_offset", BMO_OP_SLOT_BOOL},
	 {"use_interpolate", BMO_OP_SLOT_BOOL},
	 {"use_relative_offset", BMO_OP_SLOT_BOOL},
	 {"use_edge_rail", BMO_OP_SLOT_BOOL},
	 {"thickness", BMO_OP_SLOT_FLT},
	 {"depth", BMO_OP_SLOT_FLT},
	 {"use_outset", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* output faces */
	 {{'\0'}},
	},
	bmo_inset_region_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Edgeloop Offset.
 *
 * Creates edge loops based on simple edge-outset method.
 */
static BMOpDefine bmo_offset_edgeloops_def = {
	"offset_edgeloops",
	/* slots_in */
	{{"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},    /* input faces */
	 {"use_cap_endpoint", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	/* slots_out */
	{{"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}}, /* output faces */
	 {{'\0'}},
	},
	bmo_offset_edgeloops_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Wire Frame.
 *
 * Makes a wire-frame copy of faces.
 */
static BMOpDefine bmo_wireframe_def = {
	"wireframe",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},   /* input faces */
	 {"thickness", BMO_OP_SLOT_FLT},
	 {"offset", BMO_OP_SLOT_FLT},
	 {"use_replace", BMO_OP_SLOT_BOOL},
	 {"use_boundary", BMO_OP_SLOT_BOOL},
	 {"use_even_offset", BMO_OP_SLOT_BOOL},
	 {"use_crease", BMO_OP_SLOT_BOOL},
	 {"crease_weight", BMO_OP_SLOT_FLT},
	 {"use_relative_offset", BMO_OP_SLOT_BOOL},
	 {"material_offset", BMO_OP_SLOT_INT},
	 {{'\0'}},
	},
	/* slots_out */
	{{"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* output faces */
	 {{'\0'}},
	},
	bmo_wireframe_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Pokes a face.
 *
 * Splits a face into a triangle fan.
 */
static BMOpDefine bmo_poke_def = {
	"poke",
	/* slots_in */
	{{"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},   /* input faces */
	 {"offset", BMO_OP_SLOT_FLT}, /* center vertex offset along normal */
	 {"center_mode", BMO_OP_SLOT_INT}, /* calculation mode for center vertex */
	 {"use_relative_offset", BMO_OP_SLOT_BOOL}, /* apply offset */
	 {{'\0'}},
	},
	/* slots_out */
	{{"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}}, /* output verts */
	 {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}}, /* output faces */
	 {{'\0'}},
	},
	bmo_poke_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

#ifdef WITH_BULLET
/*
 * Convex Hull
 *
 * Builds a convex hull from the vertices in 'input'.
 *
 * If 'use_existing_faces' is true, the hull will not output triangles
 * that are covered by a pre-existing face.
 *
 * All hull vertices, faces, and edges are added to 'geom.out'. Any
 * input elements that end up inside the hull (i.e. are not used by an
 * output face) are added to the 'interior_geom' slot. The
 * 'unused_geom' slot will contain all interior geometry that is
 * completely unused. Lastly, 'holes_geom' contains edges and faces
 * that were in the input and are part of the hull.
 */
static BMOpDefine bmo_convex_hull_def = {
	"convex_hull",
	/* slots_in */
	{{"input", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"use_existing_faces", BMO_OP_SLOT_BOOL},
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"geom_interior.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"geom_unused.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"geom_holes.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {{'\0'}},
	},
	bmo_convex_hull_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};
#endif

/*
 * Symmetrize.
 *
 * Makes the mesh elements in the "input" slot symmetrical. Unlike
 * normal mirroring, it only copies in one direction, as specified by
 * the "direction" slot. The edges and faces that cross the plane of
 * symmetry are split as needed to enforce symmetry.
 *
 * All new vertices, edges, and faces are added to the "geom.out" slot.
 */
static BMOpDefine bmo_symmetrize_def = {
	"symmetrize",
	/* slots_in */
	{{"input", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {"direction", BMO_OP_SLOT_INT},
	 {"dist", BMO_OP_SLOT_FLT}, /* minimum distance */
	 {{'\0'}},
	},
	/* slots_out */
	{{"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
	 {{'\0'}},
	},
	bmo_symmetrize_exec,
	(BMO_OPTYPE_FLAG_NORMALS_CALC |
	 BMO_OPTYPE_FLAG_SELECT_FLUSH |
	 BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

const BMOpDefine *bmo_opdefines[] = {
	&bmo_automerge_def,
	&bmo_average_vert_facedata_def,
	&bmo_beautify_fill_def,
	&bmo_bevel_def,
	&bmo_bisect_edges_def,
	&bmo_bmesh_to_mesh_def,
	&bmo_bridge_loops_def,
	&bmo_collapse_def,
	&bmo_collapse_uvs_def,
	&bmo_connect_verts_def,
	&bmo_connect_verts_concave_def,
	&bmo_connect_verts_nonplanar_def,
	&bmo_connect_vert_pair_def,
	&bmo_contextual_create_def,
#ifdef WITH_BULLET
	&bmo_convex_hull_def,
#endif
	&bmo_create_circle_def,
	&bmo_create_cone_def,
	&bmo_create_cube_def,
	&bmo_create_grid_def,
	&bmo_create_icosphere_def,
	&bmo_create_monkey_def,
	&bmo_create_uvsphere_def,
	&bmo_create_vert_def,
	&bmo_delete_def,
	&bmo_dissolve_edges_def,
	&bmo_dissolve_faces_def,
	&bmo_dissolve_verts_def,
	&bmo_dissolve_limit_def,
	&bmo_dissolve_degenerate_def,
	&bmo_duplicate_def,
	&bmo_holes_fill_def,
	&bmo_face_attribute_fill_def,
	&bmo_offset_edgeloops_def,
	&bmo_edgeloop_fill_def,
	&bmo_edgenet_fill_def,
	&bmo_edgenet_prepare_def,
	&bmo_extrude_discrete_faces_def,
	&bmo_extrude_edge_only_def,
	&bmo_extrude_face_region_def,
	&bmo_extrude_vert_indiv_def,
	&bmo_find_doubles_def,
	&bmo_grid_fill_def,
	&bmo_inset_individual_def,
	&bmo_inset_region_def,
	&bmo_join_triangles_def,
	&bmo_mesh_to_bmesh_def,
	&bmo_mirror_def,
	&bmo_object_load_bmesh_def,
	&bmo_pointmerge_def,
	&bmo_pointmerge_facedata_def,
	&bmo_poke_def,
	&bmo_recalc_face_normals_def,
	&bmo_planar_faces_def,
	&bmo_region_extend_def,
	&bmo_remove_doubles_def,
	&bmo_reverse_colors_def,
	&bmo_reverse_faces_def,
	&bmo_reverse_uvs_def,
	&bmo_rotate_colors_def,
	&bmo_rotate_def,
	&bmo_rotate_edges_def,
	&bmo_rotate_uvs_def,
	&bmo_scale_def,
	&bmo_similar_edges_def,
	&bmo_similar_faces_def,
	&bmo_similar_verts_def,
	&bmo_smooth_vert_def,
	&bmo_smooth_laplacian_vert_def,
	&bmo_solidify_def,
	&bmo_spin_def,
	&bmo_split_def,
	&bmo_split_edges_def,
	&bmo_subdivide_edges_def,
	&bmo_subdivide_edgering_def,
	&bmo_bisect_plane_def,
	&bmo_symmetrize_def,
	&bmo_transform_def,
	&bmo_translate_def,
	&bmo_triangle_fill_def,
	&bmo_triangulate_def,
	&bmo_unsubdivide_def,
	&bmo_weld_verts_def,
	&bmo_wireframe_def,
};

const int bmo_opdefines_total = ARRAY_SIZE(bmo_opdefines);
