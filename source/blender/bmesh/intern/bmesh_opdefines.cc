/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
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
 * - `verts`
 * - `edges`
 * - `faces`
 * - `edge_face.in`
 * - `vert_face.in`
 * - `vert_edge.in`
 * - `vert_face.in`
 * - `geom`
 *
 * The basic rules are, for single-type geometry slots, use the plural of the
 * type name (e.g. edges). for double-type slots, use the two type names plus
 * "in" (e.g. `edge_face.in`). for three-type slots, use geom.
 *
 * for output slots, for single-type geometry slots, use the type name plus "out",
 * (e.g. `verts.out`), for double-type slots, use the two type names plus "out",
 * (e.g. `vert_faces.out`), for three-type slots, use geom. note that you can also
 * use more esoteric names (e.g. `geom_skirt.out`) so long as the comment next to the
 * slot definition tells you what types of elements are in it.
 */

#include "BLI_utildefines.h"

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh"

#include "DNA_modifier_types.h"

/**
 * The formatting of these bmesh operators is parsed by
 * 'doc/python_api/rst_from_bmesh_opdefines.py'
 * for use in python docs, so reStructuredText may be used
 * rather than DOXYGEN syntax.
 *
 * template (py quotes used because nested comments don't work
 * on all C compilers):
 *
 * """
 * Region Extend.
 *
 * paragraph1, Extends on the title above.
 *
 * Another paragraph.
 *
 * Another paragraph.
 * """
 *
 * so the first line is the "title" of the BMOP.
 * subsequent line blocks separated by blank lines
 * are paragraphs. individual descriptions of slots
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

/* Keep struct definition from wrapping. */

/* Enums shared between multiple operators. */

static BMO_FlagSet bmo_enum_axis_xyz[] = {
    {0, "X"},
    {1, "Y"},
    {2, "Z"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_axis_neg_xyz_and_xyz[] = {
    {0, "-X"},
    {1, "-Y"},
    {2, "-Z"},
    {3, "X"},
    {4, "Y"},
    {5, "Z"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_falloff_type[] = {
    {SUBD_FALLOFF_SMOOTH, "SMOOTH"},
    {SUBD_FALLOFF_SPHERE, "SPHERE"},
    {SUBD_FALLOFF_ROOT, "ROOT"},
    {SUBD_FALLOFF_SHARP, "SHARP"},
    {SUBD_FALLOFF_LIN, "LINEAR"},
    {SUBD_FALLOFF_INVSQUARE, "INVERSE_SQUARE"},
    {0, nullptr},
};

static eBMOpSlotSubType_Union to_subtype_union(const eBMOpSlotSubType_Ptr ptr)
{
  eBMOpSlotSubType_Union value;
  value.ptr = ptr;
  return value;
}

static eBMOpSlotSubType_Union to_subtype_union(const eBMOpSlotSubType_Map map)
{
  eBMOpSlotSubType_Union value;
  value.map = map;
  return value;
}

static eBMOpSlotSubType_Union to_subtype_union(const eBMOpSlotSubType_Int intg)
{
  eBMOpSlotSubType_Union value;
  value.intg = intg;
  return value;
}

/* Quiet 'enum-conversion' warning. */
#define BM_FACE ((eBMOpSlotSubType_Elem)BM_FACE)
#define BM_EDGE ((eBMOpSlotSubType_Elem)BM_EDGE)
#define BM_VERT ((eBMOpSlotSubType_Elem)BM_VERT)

/*
 * Vertex Smooth.
 *
 * Smooths vertices by using a basic vertex averaging scheme.
 */
static BMOpDefine bmo_smooth_vert_def = {
    /*opname*/ "smooth_vert",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Smoothing factor. */
        {"factor", BMO_OP_SLOT_FLT},
        /* Set vertices close to the x axis before the operation to 0. */
        {"mirror_clip_x", BMO_OP_SLOT_BOOL},
        /* Set vertices close to the y axis before the operation to 0. */
        {"mirror_clip_y", BMO_OP_SLOT_BOOL},
        /* Set vertices close to the z axis before the operation to 0. */
        {"mirror_clip_z", BMO_OP_SLOT_BOOL},
        /* Clipping threshold for the above three slots. */
        {"clip_dist", BMO_OP_SLOT_FLT},
        /* Smooth vertices along X axis. */
        {"use_axis_x", BMO_OP_SLOT_BOOL},
        /* Smooth vertices along Y axis. */
        {"use_axis_y", BMO_OP_SLOT_BOOL},
        /* Smooth vertices along Z axis. */
        {"use_axis_z", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_smooth_vert_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Vertex Smooth Laplacian.
 *
 * Smooths vertices by using Laplacian smoothing propose by.
 * Desbrun, et al. Implicit Fairing of Irregular Meshes using Diffusion and Curvature Flow.
 */
static BMOpDefine bmo_smooth_laplacian_vert_def = {
    /*opname*/ "smooth_laplacian_vert",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Lambda parameter. */
        {"lambda_factor", BMO_OP_SLOT_FLT},
        /* Lambda param in border. */
        {"lambda_border", BMO_OP_SLOT_FLT},
        /* Smooth object along X axis. */
        {"use_x", BMO_OP_SLOT_BOOL},
        /* Smooth object along Y axis. */
        {"use_y", BMO_OP_SLOT_BOOL},
        /* Smooth object along Z axis. */
        {"use_z", BMO_OP_SLOT_BOOL},
        /* Apply volume preservation after smooth. */
        {"preserve_volume", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_smooth_laplacian_vert_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Right-Hand Faces.
 *
 * Computes an "outside" normal for the specified input faces.
 */
static BMOpDefine bmo_recalc_face_normals_def = {
    /*opname*/ "recalc_face_normals",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_recalc_face_normals_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Planar Faces.
 *
 * Iteratively flatten faces.
 */
static BMOpDefine bmo_planar_faces_def = {
    /*opname*/ "planar_faces",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Number of times to flatten faces (for when connected faces are used) */
        {"iterations", BMO_OP_SLOT_INT},
        /* Influence for making planar each iteration */
        {"factor", BMO_OP_SLOT_FLT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output slot, computed boundary geometry. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_planar_faces_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_SELECT_FLUSH | BMO_OPTYPE_FLAG_SELECT_VALIDATE),
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
    /*opname*/ "region_extend",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Find boundary inside the regions, not outside. */
        {"use_contract", BMO_OP_SLOT_BOOL},
        /* Extend from faces instead of edges. */
        {"use_faces", BMO_OP_SLOT_BOOL},
        /* Step over connected faces. */
        {"use_face_step", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output slot, computed boundary geometry. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_region_extend_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_SELECT_FLUSH | BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Edge Rotate.
 *
 * Rotates edges topologically. Also known as "spin edge" to some people.
 * Simple example: `[/] becomes [|] then [\]`.
 */
static BMOpDefine bmo_rotate_edges_def = {
    /*opname*/ "rotate_edges",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Rotate edge counter-clockwise if true, otherwise clockwise. */
        {"use_ccw", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Newly spun edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_rotate_edges_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Reverse Faces.
 *
 * Reverses the winding (vertex order) of faces.
 * This has the effect of flipping the normal.
 */
static BMOpDefine bmo_reverse_faces_def = {
    /*opname*/ "reverse_faces",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Maintain multi-res offset. */
        {"flip_multires", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_reverse_faces_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Flip Quad Tessellation
 *
 * Flip the tessellation direction of the selected quads.
 */
static BMOpDefine bmo_flip_quad_tessellation_def = {
    /*opname*/ "flip_quad_tessellation",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_flip_quad_tessellation_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Edge Bisect.
 *
 * Splits input edges (but doesn't do anything else).
 * This creates a 2-valence vert.
 */
static BMOpDefine bmo_bisect_edges_def = {
    /*opname*/ "bisect_edges",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Number of cuts. */
        {"cuts", BMO_OP_SLOT_INT},
        {"edge_percents",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_FLT)}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Newly created vertices and edges. */
        {"geom_split.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_bisect_edges_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Mirror.
 *
 * Mirrors geometry along an axis. The resulting geometry is welded on using
 * merge_dist. Pairs of original/mirrored vertices are welded using the merge_dist
 * parameter (which defines the minimum distance for welding to happen).
 */
static BMOpDefine bmo_mirror_def = {
    /*opname*/ "mirror",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Matrix defining the mirror transformation. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Maximum distance for merging. does no merging if 0. */
        {"merge_dist", BMO_OP_SLOT_FLT},
        /* The axis to use. */
        {"axis",
         BMO_OP_SLOT_INT,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_INT_ENUM)},
         bmo_enum_axis_xyz},
        /* Mirror UVs across the u axis. */
        {"mirror_u", BMO_OP_SLOT_BOOL},
        /* Mirror UVs across the v axis. */
        {"mirror_v", BMO_OP_SLOT_BOOL},
        /* Mirror UVs in each tile. */
        {"mirror_udim", BMO_OP_SLOT_BOOL},
        /* Transform shape keys too. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output geometry, mirrored. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_mirror_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Find Doubles.
 *
 * Takes input verts and find vertices they should weld to.
 * Outputs a mapping slot suitable for use with the weld verts BMOP.
 *
 * If keep_verts is used, vertices outside that set can only be merged
 * with vertices in that set.
 */
static BMOpDefine bmo_find_doubles_def = {
    /*opname*/ "find_doubles",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* List of verts to keep. */
        {"keep_verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Limit the search for doubles by connected geometry. */
        {"use_connected", BMO_OP_SLOT_BOOL},
        /* Maximum distance. */
        {"dist", BMO_OP_SLOT_FLT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"targetmap.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_find_doubles_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Remove Doubles.
 *
 * Finds groups of vertices closer than dist and merges them together,
 * using the weld verts BMOP.
 */
static BMOpDefine bmo_remove_doubles_def = {
    /*opname*/ "remove_doubles",
    /*slot_types_in*/
    {
        /* Input verts. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Limit the search for doubles by connected geometry. */
        {"use_connected", BMO_OP_SLOT_BOOL},
        /* Minimum distance. */
        {"dist", BMO_OP_SLOT_FLT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_remove_doubles_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Collapse Connected.
 *
 * Collapses connected vertices
 */
static BMOpDefine bmo_collapse_def = {
    /*opname*/ "collapse",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Also collapse UVs and such. */
        {"uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_collapse_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Face-Data Point Merge.
 *
 * Merge uv/vcols at a specific vertex.
 */
static BMOpDefine bmo_pointmerge_facedata_def = {
    /*opname*/ "pointmerge_facedata",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Snap vertex. */
        {"vert_snap", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_pointmerge_facedata_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Average Vertices Face-vert Data.
 *
 * Merge uv/vcols associated with the input vertices at
 * the bounding box center. (I know, it's not averaging but
 * the vert_snap_to_bb_center is just too long).
 */
static BMOpDefine bmo_average_vert_facedata_def = {
    /*opname*/ "average_vert_facedata",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_average_vert_facedata_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Point Merge.
 *
 * Merge verts together at a point.
 */
static BMOpDefine bmo_pointmerge_def = {
    /*opname*/ "pointmerge",
    /*slot_types_in*/
    {
        /* Input vertices (all verts will be merged into the first). */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Position to merge at. */
        {"merge_co", BMO_OP_SLOT_VEC},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_pointmerge_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Collapse Connected UVs.
 *
 * Collapses connected UV vertices.
 */
static BMOpDefine bmo_collapse_uvs_def = {
    /*opname*/ "collapse_uvs",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_collapse_uvs_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Weld Verts.
 *
 * Welds verts together (kind-of like remove doubles, merge, etc, all of which
 * use or will use this BMOP). You pass in mappings from vertices to the vertices
 * they weld with.
 */
static BMOpDefine bmo_weld_verts_def = {
    /*opname*/ "weld_verts",
    /*slot_types_in*/
    {
        /* Maps welded vertices to verts they should weld to. */
        {"targetmap", BMO_OP_SLOT_MAPPING, {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        /* Merged vertices to their centroid position,
         * otherwise the position of the target vertex is used. */
        {"use_centroid", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_weld_verts_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Make Vertex.
 *
 * Creates a single vertex; this BMOP was necessary
 * for click-create-vertex.
 */
static BMOpDefine bmo_create_vert_def = {
    /*opname*/ "create_vert",
    /*slot_types_in*/
    {
        /* The coordinate of the new vert. */
        {"co", BMO_OP_SLOT_VEC},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* The new vert. */
        {"vert.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_vert_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Join Triangles.
 *
 * Tries to intelligently join triangles according
 * to angle threshold and delimiters.
 */

#if 0 /* See comments at top of bmo_join_triangles.cc */
#  define USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
#endif

static BMOpDefine bmo_join_triangles_def = {
    /*opname*/ "join_triangles",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Compare seam */
        {"cmp_seam", BMO_OP_SLOT_BOOL},
        /* Compare sharp */
        {"cmp_sharp", BMO_OP_SLOT_BOOL},
        /* Compare UVs */
        {"cmp_uvs", BMO_OP_SLOT_BOOL},
        /* Compare VCols. */
        {"cmp_vcols", BMO_OP_SLOT_BOOL},
        /* Compare materials. */
        {"cmp_materials", BMO_OP_SLOT_BOOL},
        {"angle_face_threshold", BMO_OP_SLOT_FLT},
        {"angle_shape_threshold", BMO_OP_SLOT_FLT},
        {"topology_influence", BMO_OP_SLOT_FLT},
        {"deselect_joined", BMO_OP_SLOT_BOOL},
#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
        {"merge_limit", BMO_OP_SLOT_INT},
        {"neighbor_debug", BMO_OP_SLOT_INT},
#endif
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Joined faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_join_triangles_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Contextual Create.
 *
 * This is basically F-key, it creates
 * new faces from vertices, makes stuff from edge nets,
 * makes wire edges, etc. It also dissolves faces.
 *
 * Three verts become a triangle, four become a quad. Two
 * become a wire edge.
 */
static BMOpDefine bmo_contextual_create_def = {
    /*opname*/ "contextual_create",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Material to use. */
        {"mat_nr", BMO_OP_SLOT_INT},
        /* Smooth to use. */
        {"use_smooth", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Newly-made face(s). */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},

        /* NOTE: this is for stand-alone edges only,
         * not edges which are a part of newly created faces. */

        /* Newly-made edge(s). */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_contextual_create_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Bridge edge loops with faces.
 */
static BMOpDefine bmo_bridge_loops_def = {
    /*opname*/ "bridge_loops",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {"use_pairs", BMO_OP_SLOT_BOOL},
        {"use_cyclic", BMO_OP_SLOT_BOOL},
        /* Merge rather than creating faces. */
        {"use_merge", BMO_OP_SLOT_BOOL},
        /* Merge factor. */
        {"merge_factor", BMO_OP_SLOT_FLT},
        /* Twist offset for closed loops. */
        {"twist_offset", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* New edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_bridge_loops_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Grid Fill.
 *
 * Create faces defined by 2 disconnected edge loops (which share edges).
 */
static BMOpDefine bmo_grid_fill_def = {
    /*opname*/ "grid_fill",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},

        /* Restricts edges to groups. maps edges to integer. */

        /* Material to use. */
        {"mat_nr", BMO_OP_SLOT_INT},
        /* Smooth state to use. */
        {"use_smooth", BMO_OP_SLOT_BOOL},
        /* Use simple interpolation. */
        {"use_interp_simple", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },

    /*slot_types_out*/
    {
        /* New faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_grid_fill_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Fill Holes.
 *
 * Fill boundary edges with faces, copying surrounding custom-data.
 */
static BMOpDefine bmo_holes_fill_def = {
    /*opname*/ "holes_fill",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Number of face sides to fill. */
        {"sides", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_holes_fill_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Face Attribute Fill.
 *
 * Fill in faces with data from adjacent faces.
 */
static BMOpDefine bmo_face_attribute_fill_def = {
    /*opname*/ "face_attribute_fill",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Copy face winding. */
        {"use_normals", BMO_OP_SLOT_BOOL},
        /* Copy face data. */
        {"use_data", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Faces that could not be handled. */
        {"faces_fail.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_face_attribute_fill_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Edge Loop Fill.
 *
 * Create faces defined by one or more non overlapping edge loops.
 */
static BMOpDefine bmo_edgeloop_fill_def = {
    /*opname*/ "edgeloop_fill",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},

        /* Restricts edges to groups. maps edges to integer. */

        /* Material to use. */
        {"mat_nr", BMO_OP_SLOT_INT},
        /* Smooth state to use. */
        {"use_smooth", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_edgeloop_fill_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Edge Net Fill.
 *
 * Create faces defined by enclosed edges.
 */
static BMOpDefine bmo_edgenet_fill_def = {
    /*opname*/ "edgenet_fill",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Material to use. */
        {"mat_nr", BMO_OP_SLOT_INT},
        /* Smooth state to use. */
        {"use_smooth", BMO_OP_SLOT_BOOL},
        /* Number of sides. */
        {"sides", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_edgenet_fill_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Edge-net Prepare.
 *
 * Identifies several useful edge loop cases and modifies them so
 * they'll become a face when edgenet_fill is called. The cases covered are:
 *
 * - One single loop; an edge is added to connect the ends
 * - Two loops; two edges are added to connect the endpoints (based on the
 *   shortest distance between each endpoint).
 */
static BMOpDefine bmo_edgenet_prepare_def = {
    /*opname*/ "edgenet_prepare",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_edgenet_prepare_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Rotate.
 *
 * Rotate vertices around a center, using a 3x3 rotation matrix.
 */
static BMOpDefine bmo_rotate_def = {
    /*opname*/ "rotate",
    /*slot_types_in*/
    {
        /* Center of rotation. */
        {"cent", BMO_OP_SLOT_VEC},
        /* Matrix defining rotation. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Matrix to define the space (typically object matrix). */
        {"space", BMO_OP_SLOT_MAT},
        /* Transform shape keys too. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_rotate_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Translate.
 *
 * Translate vertices by an offset.
 */
static BMOpDefine bmo_translate_def = {
    /*opname*/ "translate",
    /*slot_types_in*/
    {
        /* Translation offset. */
        {"vec", BMO_OP_SLOT_VEC},
        /* Matrix to define the space (typically object matrix). */
        {"space", BMO_OP_SLOT_MAT},
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Transform shape keys too. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_translate_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Scale.
 *
 * Scales vertices by an offset.
 */
static BMOpDefine bmo_scale_def = {
    /*opname*/ "scale",
    /*slot_types_in*/
    {
        /* Scale factor. */
        {"vec", BMO_OP_SLOT_VEC},
        /* Matrix to define the space (typically object matrix). */
        {"space", BMO_OP_SLOT_MAT},
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Transform shape keys too. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_scale_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Transform.
 *
 * Transforms a set of vertices by a matrix. Multiplies
 * the vertex coordinates with the matrix.
 */
static BMOpDefine bmo_transform_def = {
    /*opname*/ "transform",
    /*slot_types_in*/
    {
        /* Transform matrix. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Matrix to define the space (typically object matrix). */
        {"space", BMO_OP_SLOT_MAT},
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Transform shape keys too. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_transform_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Object Load BMesh.
 *
 * Loads a bmesh into an object/mesh. This is a "private"
 * BMOP.
 */
static BMOpDefine bmo_object_load_bmesh_def = {
    /*opname*/ "object_load_bmesh",
    /*slot_types_in*/
    {
        /* Pointer to an scene structure. */
        {"scene", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_SCENE)},

        /* Pointer to an object structure. */
        {"object", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_OBJECT)},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_object_load_bmesh_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * BMesh to Mesh.
 *
 * Converts a bmesh to a Mesh. This is reserved for exiting edit-mode.
 */
static BMOpDefine bmo_bmesh_to_mesh_def = {
    /*opname*/ "bmesh_to_mesh",
    /*slot_types_in*/
    {

        /* Pointer to a mesh structure to fill in. */
        {"mesh", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_MESH)},
        /* Pointer to an object structure. */
        {"object", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_OBJECT)},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_bmesh_to_mesh_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Mesh to BMesh.
 *
 * Load the contents of a mesh into the bmesh. this BMOP is private, it's
 * reserved exclusively for entering edit-mode.
 */
static BMOpDefine bmo_mesh_to_bmesh_def = {
    /*opname*/ "mesh_to_bmesh",
    /*slot_types_in*/
    {
        /* Pointer to a Mesh structure. */
        {"mesh", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_MESH)},
        /* Pointer to an Object structure. */
        {"object", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_OBJECT)},
        /* Load active shapekey coordinates into verts. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_mesh_to_bmesh_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Individual Face Extrude.
 *
 * Extrudes faces individually.
 */
static BMOpDefine bmo_extrude_discrete_faces_def = {
    /*opname*/ "extrude_discrete_faces",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Create faces with reversed direction. */
        {"use_normal_flip", BMO_OP_SLOT_BOOL},
        /* Pass to duplicate. */
        {"use_select_history", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_extrude_discrete_faces_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Extrude Only Edges.
 *
 * Extrudes Edges into faces, note that this is very simple, there's no fancy
 * winged extrusion.
 */
static BMOpDefine bmo_extrude_edge_only_def = {
    /*opname*/ "extrude_edge_only",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Create faces with reversed direction. */
        {"use_normal_flip", BMO_OP_SLOT_BOOL},
        /* Pass to duplicate. */
        {"use_select_history", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output geometry. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_extrude_edge_only_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Individual Vertex Extrude.
 *
 * Extrudes wire edges from vertices.
 */
static BMOpDefine bmo_extrude_vert_indiv_def = {
    /*opname*/ "extrude_vert_indiv",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Pass to duplicate. */
        {"use_select_history", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output wire edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Output vertices. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_extrude_vert_indiv_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts.
 *
 * Split faces by adding edges that connect **verts**.
 */
static BMOpDefine bmo_connect_verts_def = {
    /*opname*/ "connect_verts",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Input faces to explicitly exclude from connecting. */
        {"faces_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Prevent splits with overlaps & intersections. */
        {"check_degenerate", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_connect_verts_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts to form Convex Faces.
 *
 * Ensures all faces are convex **faces**.
 */
static BMOpDefine bmo_connect_verts_concave_def = {
    /*opname*/ "connect_verts_concave",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_connect_verts_concave_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts Across non Planer Faces.
 *
 * Split faces by connecting edges along non planer **faces**.
 */
static BMOpDefine bmo_connect_verts_nonplanar_def = {
    /*opname*/ "connect_verts_nonplanar",
    /*slot_types_in*/
    {
        /* Total rotation angle (radians). */
        {"angle_limit", BMO_OP_SLOT_FLT},
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_connect_verts_nonplanar_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Connect Verts.
 *
 * Split faces by adding edges that connect **verts**.
 */
static BMOpDefine bmo_connect_vert_pair_def = {
    /*opname*/ "connect_vert_pair",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Input vertices to explicitly exclude from connecting. */
        {"verts_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Input faces to explicitly exclude from connecting. */
        {"faces_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_connect_vert_pair_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Extrude Faces.
 *
 * Extrude operator (does not transform)
 */
static BMOpDefine bmo_extrude_face_region_def = {
    /*opname*/ "extrude_face_region",
    /*slot_types_in*/
    {
        /* Edges and faces. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Input edges to explicitly exclude from extrusion. */
        {"edges_exclude", BMO_OP_SLOT_MAPPING, to_subtype_union(BMO_OP_SLOT_SUBTYPE_MAP_EMPTY)},
        /* Keep original geometry (requires ``geom`` to include edges). */
        {"use_keep_orig", BMO_OP_SLOT_BOOL},
        /* Create faces with reversed direction. */
        {"use_normal_flip", BMO_OP_SLOT_BOOL},
        /* Use winding from surrounding faces instead of this region. */
        {"use_normal_from_adjacent", BMO_OP_SLOT_BOOL},
        /* Dissolve edges whose faces form a flat surface. */
        {"use_dissolve_ortho_edges", BMO_OP_SLOT_BOOL},
        /* Pass to duplicate. */
        {"use_select_history", BMO_OP_SLOT_BOOL},
        /* Skip flipping of input faces to preserve original orientation. */
        {"skip_input_flip", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_extrude_face_region_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Dissolve Verts.
 */
static BMOpDefine bmo_dissolve_verts_def = {
    /*opname*/ "dissolve_verts",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Split off face corners to maintain surrounding geometry. */
        {"use_face_split", BMO_OP_SLOT_BOOL},
        /* Split off face corners instead of merging faces. */
        {"use_boundary_tear", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_dissolve_verts_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Dissolve Edges.
 */
static BMOpDefine bmo_dissolve_edges_def = {
    /*opname*/ "dissolve_edges",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Dissolve verts left between only 2 edges. */
        {"use_verts", BMO_OP_SLOT_BOOL},
        /* Split off face corners to maintain surrounding geometry. */
        {"use_face_split", BMO_OP_SLOT_BOOL},
        /* Do not dissolve verts between 2 edges when their angle exceeds this threshold.
         * Disabled by default. */
        {"angle_threshold", BMO_OP_SLOT_FLT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"region.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ bmo_dissolve_edges_init,
    /*exec*/ bmo_dissolve_edges_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Dissolve Faces.
 */
static BMOpDefine bmo_dissolve_faces_def = {
    /*opname*/ "dissolve_faces",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Dissolve verts left between only 2 edges. */
        {"use_verts", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"region.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_dissolve_faces_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

static BMO_FlagSet bmo_enum_dissolve_limit_flags[] = {
    {BMO_DELIM_NORMAL, "NORMAL"},
    {BMO_DELIM_MATERIAL, "MATERIAL"},
    {BMO_DELIM_SEAM, "SEAM"},
    {BMO_DELIM_SHARP, "SHARP"},
    {BMO_DELIM_UV, "UV"},
    {0, nullptr},
};

/*
 * Limited Dissolve.
 *
 * Dissolve planar faces and co-linear edges.
 */
static BMOpDefine bmo_dissolve_limit_def = {
    /*opname*/ "dissolve_limit",
    /*slot_types_in*/
    {
        /* Total rotation angle (radians). */
        {"angle_limit", BMO_OP_SLOT_FLT},
        /* Dissolve all vertices in between face boundaries. */
        {"use_dissolve_boundaries", BMO_OP_SLOT_BOOL},
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Delimit dissolve operation. */
        {"delimit",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_FLAG),
         bmo_enum_dissolve_limit_flags},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"region.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_dissolve_limit_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Degenerate Dissolve.
 *
 * Dissolve edges with no length, faces with no area.
 */
static BMOpDefine bmo_dissolve_degenerate_def = {
    /*opname*/ "dissolve_degenerate",
    /*slot_types_in*/
    {
        /* Maximum distance to consider degenerate. */
        {"dist", BMO_OP_SLOT_FLT},
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_dissolve_degenerate_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

static BMO_FlagSet bmo_enum_triangulate_quad_method[] = {
    {MOD_TRIANGULATE_QUAD_BEAUTY, "BEAUTY"},
    {MOD_TRIANGULATE_QUAD_FIXED, "FIXED"},
    {MOD_TRIANGULATE_QUAD_ALTERNATE, "ALTERNATE"},
    {MOD_TRIANGULATE_QUAD_SHORTEDGE, "SHORT_EDGE"},
    {MOD_TRIANGULATE_QUAD_LONGEDGE, "LONG_EDGE"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_triangulate_ngon_method[] = {
    {MOD_TRIANGULATE_NGON_BEAUTY, "BEAUTY"},
    {MOD_TRIANGULATE_NGON_EARCLIP, "EAR_CLIP"},
    {0, nullptr},
};

/*
 * Triangulate.
 */
static BMOpDefine bmo_triangulate_def = {
    /*opname*/ "triangulate",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Method for splitting the quads into triangles. */
        {"quad_method",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_triangulate_quad_method},
        /* Method for splitting the polygons into triangles. */
        {"ngon_method",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_triangulate_ngon_method},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {"face_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        /* Duplicate faces. */
        {"face_map_double.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_triangulate_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Un-Subdivide.
 *
 * Reduce detail in geometry containing grids.
 */
static BMOpDefine bmo_unsubdivide_def = {
    /*opname*/ "unsubdivide",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Number of times to unsubdivide. */
        {"iterations", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_unsubdivide_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

static BMO_FlagSet bmo_enum_subdivide_edges_quad_corner_type[] = {
    {SUBD_CORNER_STRAIGHT_CUT, "STRAIGHT_CUT"},
    {SUBD_CORNER_INNERVERT, "INNER_VERT"},
    {SUBD_CORNER_PATH, "PATH"},
    {SUBD_CORNER_FAN, "FAN"},
    {0, nullptr},
};

/*
 * Subdivide Edges.
 *
 * Advanced operator for subdividing edges
 * with options for face patterns, smoothing and randomization.
 */
static BMOpDefine bmo_subdivide_edges_def = {
    /*opname*/ "subdivide_edges",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Smoothness factor. */
        {"smooth", BMO_OP_SLOT_FLT},
        /* Smooth falloff type. */
        {"smooth_falloff",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_falloff_type},
        /* Fractal randomness factor. */
        {"fractal", BMO_OP_SLOT_FLT},
        /* Apply fractal displacement along normal only. */
        {"along_normal", BMO_OP_SLOT_FLT},
        /* Number of cuts. */
        {"cuts", BMO_OP_SLOT_INT},
        /* Seed for the random number generator. */
        {"seed", BMO_OP_SLOT_INT},
        /* Uses custom pointers. */
        {"custom_patterns",
         BMO_OP_SLOT_MAPPING,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL)},
        {"edge_percents", BMO_OP_SLOT_MAPPING, to_subtype_union(BMO_OP_SLOT_SUBTYPE_MAP_FLT)},
        /* Quad corner type. */
        {"quad_corner_type",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_subdivide_edges_quad_corner_type},
        /* Fill in fully-selected faces with a grid. */
        {"use_grid_fill", BMO_OP_SLOT_BOOL},
        /* Tessellate the case of one edge selected in a quad or triangle. */
        {"use_single_edge", BMO_OP_SLOT_BOOL},
        /* Only subdivide quads (for loop-cut). */
        {"use_only_quads", BMO_OP_SLOT_BOOL},
        /* For making new primitives only. */
        {"use_sphere", BMO_OP_SLOT_BOOL},
        /* Maintain even offset when smoothing. */
        {"use_smooth_even", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* NOTE: these next three can have multiple types of elements in them. */
        {"geom_inner.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {"geom_split.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Contains all output geometry. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_subdivide_edges_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

static BMO_FlagSet bmo_enum_subdivide_edgering_interp_mode[] = {
    {SUBD_RING_INTERP_LINEAR, "LINEAR"},
    {SUBD_RING_INTERP_PATH, "PATH"},
    {SUBD_RING_INTERP_SURF, "SURFACE"},
    {0, nullptr},
};

/*
 * Subdivide Edge-Ring.
 *
 * Take an edge-ring, and subdivide with interpolation options.
 */
static BMOpDefine bmo_subdivide_edgering_def = {
    /*opname*/ "subdivide_edgering",
    /*slot_types_in*/
    {
        /* Input vertices. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Interpolation method. */
        {"interp_mode",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_subdivide_edgering_interp_mode},
        /* Smoothness factor. */
        {"smooth", BMO_OP_SLOT_FLT},
        /* Number of cuts. */
        {"cuts", BMO_OP_SLOT_INT},
        /* Profile shape type. */
        {"profile_shape",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_falloff_type},
        /* How much intermediary new edges are shrunk/expanded. */
        {"profile_shape_factor", BMO_OP_SLOT_FLT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_subdivide_edgering_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Bisect Plane.
 *
 * Bisects the mesh by a plane (cut the mesh in half).
 */
static BMOpDefine bmo_bisect_plane_def = {
    /*opname*/ "bisect_plane",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Minimum distance when testing if a vert is exactly on the plane. */
        {"dist", BMO_OP_SLOT_FLT},
        /* Point on the plane. */
        {"plane_co", BMO_OP_SLOT_VEC},
        /* Direction of the plane. */
        {"plane_no", BMO_OP_SLOT_VEC},
        /* Snap axis aligned verts to the center. */
        {"use_snap_center", BMO_OP_SLOT_BOOL},
        /* When enabled. remove all geometry on the positive side of the plane. */
        {"clear_outer", BMO_OP_SLOT_BOOL},
        /* When enabled. remove all geometry on the negative side of the plane. */
        {"clear_inner", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output geometry aligned with the plane (new and existing). */
        {"geom_cut.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE}},
        /* Input and output geometry (result of cut). */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_bisect_plane_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

static BMO_FlagSet bmo_enum_delete_context[] = {
    {DEL_VERTS, "VERTS"},
    {DEL_EDGES, "EDGES"},
    {DEL_ONLYFACES, "FACES_ONLY"},
    {DEL_EDGESFACES, "EDGES_FACES"},
    {DEL_FACES, "FACES"},
    {DEL_FACES_KEEP_BOUNDARY, "FACES_KEEP_BOUNDARY"},
    {DEL_ONLYTAGGED, "TAGGED_ONLY"},
    {0, nullptr},
};

/*
 * Delete Geometry.
 *
 * Utility operator to delete geometry.
 */
static BMOpDefine bmo_delete_def = {
    /*opname*/ "delete",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Geometry types to delete. */
        {"context",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_delete_context},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_delete_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Duplicate Geometry.
 *
 * Utility operator to duplicate geometry,
 * optionally into a destination mesh.
 */
static BMOpDefine bmo_duplicate_def = {
    /*opname*/ "duplicate",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Destination bmesh, if None will use current on. */
        {"dest", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_BMESH)},
        {"use_select_history", BMO_OP_SLOT_BOOL},
        {"use_edge_flip_from_face", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"geom_orig.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},

        /* NOTE: face_map maps from source faces to dupe faces,
         * and from dupe faces to source faces. */

        {"vert_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {"edge_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {"face_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        /* Boundary edges from the split geometry that maps edges from the original geometry
         * to the destination edges. */
        {"boundary_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {"isovert_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_duplicate_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Split Off Geometry.
 *
 * Disconnect geometry from adjacent edges and faces,
 * optionally into a destination mesh.
 */
static BMOpDefine bmo_split_def = {
    /*opname*/ "split",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Destination bmesh, if None will use current one. */
        {"dest", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_BMESH)},
        /* When enabled. don't duplicate loose verts/edges. */
        {"use_only_faces", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Boundary edges from the split geometry that maps edges from the original geometry
         * to the destination edges.
         *
         * When the source edges have been deleted, the destination edge will be used
         * for both the key and the value. */
        {"boundary_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {"isovert_map.out",
         BMO_OP_SLOT_MAPPING,
         {eBMOpSlotSubType_Elem(BMO_OP_SLOT_SUBTYPE_MAP_ELEM)}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_split_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Spin.
 *
 * Extrude or duplicate geometry a number of times,
 * rotating and possibly translating after each step
 */
static BMOpDefine bmo_spin_def = {
    /*opname*/ "spin",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Rotation center. */
        {"cent", BMO_OP_SLOT_VEC},
        /* Rotation axis. */
        {"axis", BMO_OP_SLOT_VEC},
        /* Translation delta per step. */
        {"dvec", BMO_OP_SLOT_VEC},
        /* Total rotation angle (radians). */
        {"angle", BMO_OP_SLOT_FLT},
        /* Matrix to define the space (typically object matrix). */
        {"space", BMO_OP_SLOT_MAT},
        /* Number of steps. */
        {"steps", BMO_OP_SLOT_INT},
        /* Merge first/last when the angle is a full revolution. */
        {"use_merge", BMO_OP_SLOT_BOOL},
        /* Create faces with reversed direction. */
        {"use_normal_flip", BMO_OP_SLOT_BOOL},
        /* Duplicate or extrude?. */
        {"use_duplicate", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Result of last step. */
        {"geom_last.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_spin_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * UV Rotation.
 *
 * Cycle the loop UVs
 */
static BMOpDefine bmo_rotate_uvs_def = {
    /*opname*/ "rotate_uvs",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Rotate counter-clockwise if true, otherwise clockwise. */
        {"use_ccw", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_rotate_uvs_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * UV Reverse.
 *
 * Reverse the UVs
 */
static BMOpDefine bmo_reverse_uvs_def = {
    /*opname*/ "reverse_uvs",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_reverse_uvs_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Color Rotation.
 *
 * Cycle the loop colors
 */
static BMOpDefine bmo_rotate_colors_def = {
    /*opname*/ "rotate_colors",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Rotate counter-clockwise if true, otherwise clockwise. */
        {"use_ccw", BMO_OP_SLOT_BOOL},
        /* Index into color attribute list. */
        {"color_index", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_rotate_colors_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Color Reverse
 *
 * Reverse the loop colors.
 */
static BMOpDefine bmo_reverse_colors_def = {
    /*opname*/ "reverse_colors",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Index into color attribute list. */
        {"color_index", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {{{'\0'}}},
    /*init*/ nullptr,
    /*exec*/ bmo_reverse_colors_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NOP),
};

/*
 * Edge Split.
 *
 * Disconnects faces along input edges.
 */
static BMOpDefine bmo_split_edges_def = {
    /*opname*/ "split_edges",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},

        /* NOTE: needed for vertex rip so we can rip only half an edge
         * at a boundary which would otherwise split off. */

        /* Optional tag verts, use to have greater control of splits. */
        {"verts", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Use 'verts' for splitting, else just find verts to split from edges. */
        {"use_verts", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Old output disconnected edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_split_edges_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Grid.
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_grid_def = {
    /*opname*/ "create_grid",
    /*slot_types_in*/
    {
        /* Number of x segments. */
        {"x_segments", BMO_OP_SLOT_INT},
        /* Number of y segments. */
        {"y_segments", BMO_OP_SLOT_INT},
        /* Size of the grid. */
        {"size", BMO_OP_SLOT_FLT},
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_grid_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create UV Sphere.
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_uvsphere_def = {
    /*opname*/ "create_uvsphere",
    /*slot_types_in*/
    {
        /* Number of u segments. */
        {"u_segments", BMO_OP_SLOT_INT},
        /* Number of v segment. */
        {"v_segments", BMO_OP_SLOT_INT},
        /* Radius. */
        {"radius", BMO_OP_SLOT_FLT},
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_uvsphere_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Ico-Sphere.
 *
 * Creates a grid with a variable number of subdivisions
 */
static BMOpDefine bmo_create_icosphere_def = {
    /*opname*/ "create_icosphere",
    /*slot_types_in*/
    {
        /* How many times to recursively subdivide the sphere. */
        {"subdivisions", BMO_OP_SLOT_INT},
        /* Radius. */
        {"radius", BMO_OP_SLOT_FLT},
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_icosphere_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Suzanne.
 *
 * Creates a monkey (standard blender primitive).
 */
static BMOpDefine bmo_create_monkey_def = {
    /*opname*/ "create_monkey",
    /*slot_types_in*/
    {
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_monkey_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Cone.
 *
 * Creates a cone with variable depth at both ends
 */
static BMOpDefine bmo_create_cone_def = {
    /*opname*/ "create_cone",
    /*slot_types_in*/
    {
        /* Whether or not to fill in the ends with faces. */
        {"cap_ends", BMO_OP_SLOT_BOOL},
        /* Fill ends with triangles instead of ngons. */
        {"cap_tris", BMO_OP_SLOT_BOOL},
        /* Number of vertices in the base circle. */
        {"segments", BMO_OP_SLOT_INT},
        /* Radius of one end. */
        {"radius1", BMO_OP_SLOT_FLT},
        /* Radius of the opposite. */
        {"radius2", BMO_OP_SLOT_FLT},
        /* Distance between ends. */
        {"depth", BMO_OP_SLOT_FLT},
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_cone_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Creates a Circle.
 */
static BMOpDefine bmo_create_circle_def = {
    /*opname*/ "create_circle",
    /*slot_types_in*/
    {
        /* Whether or not to fill in the ends with faces. */
        {"cap_ends", BMO_OP_SLOT_BOOL},
        /* Fill ends with triangles instead of ngons. */
        {"cap_tris", BMO_OP_SLOT_BOOL},
        /* Number of vertices in the circle. */
        {"segments", BMO_OP_SLOT_INT},
        /* Radius of the circle. */
        {"radius", BMO_OP_SLOT_FLT},
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_circle_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Create Cube
 *
 * Creates a cube.
 */
static BMOpDefine bmo_create_cube_def = {
    /*opname*/ "create_cube",
    /*slot_types_in*/
    {
        /* Size of the cube. */
        {"size", BMO_OP_SLOT_FLT},
        /* Matrix to multiply the new geometry with. */
        {"matrix", BMO_OP_SLOT_MAT},
        /* Calculate default UVs. */
        {"calc_uvs", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_create_cube_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

static BMO_FlagSet bmo_enum_bevel_offset_type[] = {
    {BEVEL_AMT_OFFSET, "OFFSET"},
    {BEVEL_AMT_WIDTH, "WIDTH"},
    {BEVEL_AMT_DEPTH, "DEPTH"},
    {BEVEL_AMT_PERCENT, "PERCENT"},
    {BEVEL_AMT_ABSOLUTE, "ABSOLUTE"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_bevel_profile_type[] = {
    {BEVEL_PROFILE_SUPERELLIPSE, "SUPERELLIPSE"},
    {BEVEL_PROFILE_CUSTOM, "CUSTOM"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_bevel_face_strength_type[] = {
    {BEVEL_FACE_STRENGTH_NONE, "NONE"},
    {BEVEL_FACE_STRENGTH_NEW, "NEW"},
    {BEVEL_FACE_STRENGTH_AFFECTED, "AFFECTED"},
    {BEVEL_FACE_STRENGTH_ALL, "ALL"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_bevel_miter_type[] = {
    {BEVEL_MITER_SHARP, "SHARP"},
    {BEVEL_MITER_PATCH, "PATCH"},
    {BEVEL_MITER_ARC, "ARC"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_bevel_vmesh_method[] = {
    {BEVEL_VMESH_ADJ, "ADJ"},
    {BEVEL_VMESH_CUTOFF, "CUTOFF"},
    {0, nullptr},
};

static BMO_FlagSet bmo_enum_bevel_affect_type[] = {
    {BEVEL_AFFECT_VERTICES, "VERTICES"},
    {BEVEL_AFFECT_EDGES, "EDGES"},
    {0, nullptr},
};

/*
 * Bevel.
 *
 * Bevels edges and vertices
 */
static BMOpDefine bmo_bevel_def = {
    /*opname*/ "bevel",
    /*slot_types_in*/
    {
        /* Input edges and vertices. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Amount to offset beveled edge. */
        {"offset", BMO_OP_SLOT_FLT},
        /* How to measure the offset. */
        {"offset_type",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_offset_type},
        /* The profile type to use for bevel. */
        {"profile_type",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_profile_type},
        /* Number of segments in bevel. */
        {"segments", BMO_OP_SLOT_INT},
        /* Profile shape, 0->1 (.5=>round). */
        {"profile", BMO_OP_SLOT_FLT},
        /* Whether to bevel vertices or edges. */
        {"affect",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_affect_type},
        /* Do not allow beveled edges/vertices to overlap each other. */
        {"clamp_overlap", BMO_OP_SLOT_BOOL},
        /* Material for bevel faces, -1 means get from adjacent faces. */
        {"material", BMO_OP_SLOT_INT},
        /* Prefer to slide along edges to having even widths. */
        {"loop_slide", BMO_OP_SLOT_BOOL},
        /* Extend edge data to allow seams to run across bevels. */
        {"mark_seam", BMO_OP_SLOT_BOOL},
        /* Extend edge data to allow sharp edges to run across bevels. */
        {"mark_sharp", BMO_OP_SLOT_BOOL},
        /* Harden normals. */
        {"harden_normals", BMO_OP_SLOT_BOOL},
        /* Whether to set face strength, and which faces to set if so. */
        {"face_strength_mode",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_face_strength_type},
        /* Outer miter kind. */
        {"miter_outer",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_miter_type},
        /* Outer miter kind. */
        {"miter_inner",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_miter_type},
        /* Amount to offset beveled edge. */
        {"spread", BMO_OP_SLOT_FLT},
        /* CurveProfile, if None ignored */
        {"custom_profile", BMO_OP_SLOT_PTR, to_subtype_union(BMO_OP_SLOT_SUBTYPE_PTR_STRUCT)},
        /* The method to use to create meshes at intersections. */
        {"vmesh_method",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_bevel_vmesh_method},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Output edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        {{'\0'}},
    },

    /*init*/ nullptr,
    /*exec*/ bmo_bevel_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/* No enum is defined for this. */
static BMO_FlagSet bmo_enum_beautify_fill_method[] = {
    {0, "AREA"},
    {1, "ANGLE"},
    {0, nullptr},
};

/*
 * Beautify Fill.
 *
 * Rotate edges to create more evenly spaced triangles.
 */
static BMOpDefine bmo_beautify_fill_def = {
    /*opname*/ "beautify_fill",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Edges that can be flipped. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Restrict edge rotation to mixed tagged vertices. */
        {"use_restrict_tag", BMO_OP_SLOT_BOOL},
        /* Method to define what is beautiful. */
        {"method",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_beautify_fill_method},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New flipped faces and edges. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_beautify_fill_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

/*
 * Triangle Fill.
 *
 * Fill edges with triangles
 */
static BMOpDefine bmo_triangle_fill_def = {
    /*opname*/ "triangle_fill",
    /*slot_types_in*/
    {
        /* Use best triangulation division. */
        {"use_beauty", BMO_OP_SLOT_BOOL},
        /* Dissolve resulting faces. */
        {"use_dissolve", BMO_OP_SLOT_BOOL},
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Optionally pass the fill normal to use. */
        {"normal", BMO_OP_SLOT_VEC},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* New faces and edges. */
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_triangle_fill_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_UNTAN_MULTIRES | BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Solidify.
 *
 * Turns a mesh into a shell with thickness
 */
static BMOpDefine bmo_solidify_def = {
    /*opname*/ "solidify",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"geom", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Thickness. */
        {"thickness", BMO_OP_SLOT_FLT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_solidify_face_region_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Face Inset (Individual).
 *
 * Insets individual faces.
 */
static BMOpDefine bmo_inset_individual_def = {
    /*opname*/ "inset_individual",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Thickness. */
        {"thickness", BMO_OP_SLOT_FLT},
        /* Depth. */
        {"depth", BMO_OP_SLOT_FLT},
        /* Scale the offset to give more even thickness. */
        {"use_even_offset", BMO_OP_SLOT_BOOL},
        /* Blend face data across the inset. */
        {"use_interpolate", BMO_OP_SLOT_BOOL},
        /* Scale the offset by surrounding geometry. */
        {"use_relative_offset", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_inset_individual_exec,
    /* Caller needs to handle BMO_OPTYPE_FLAG_SELECT_FLUSH. */
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC),
};

/*
 * Face Inset (Regions).
 *
 * Inset or outset face regions.
 */
static BMOpDefine bmo_inset_region_def = {
    /*opname*/ "inset_region",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Input faces to explicitly exclude from inset. */
        {"faces_exclude", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Inset face boundaries. */
        {"use_boundary", BMO_OP_SLOT_BOOL},
        /* Scale the offset to give more even thickness. */
        {"use_even_offset", BMO_OP_SLOT_BOOL},
        /* Blend face data across the inset. */
        {"use_interpolate", BMO_OP_SLOT_BOOL},
        /* Scale the offset by surrounding geometry. */
        {"use_relative_offset", BMO_OP_SLOT_BOOL},
        /* Inset the region along existing edges. */
        {"use_edge_rail", BMO_OP_SLOT_BOOL},
        /* Thickness. */
        {"thickness", BMO_OP_SLOT_FLT},
        /* Depth. */
        {"depth", BMO_OP_SLOT_FLT},
        /* Outset rather than inset. */
        {"use_outset", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_inset_region_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Edge-loop Offset.
 *
 * Creates edge loops based on simple edge-outset method.
 */
static BMOpDefine bmo_offset_edgeloops_def = {
    /*opname*/ "offset_edgeloops",
    /*slot_types_in*/
    {
        /* Input edges. */
        {"edges", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        /* Extend loop around end-points. */
        {"use_cap_endpoint", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output edges. */
        {"edges.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_EDGE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_offset_edgeloops_exec,
    /*type_flag*/ (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH),
};

/*
 * Wire Frame.
 *
 * Makes a wire-frame copy of faces.
 */
static BMOpDefine bmo_wireframe_def = {
    /*opname*/ "wireframe",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Thickness. */
        {"thickness", BMO_OP_SLOT_FLT},
        /* Offset the thickness from the center. */
        {"offset", BMO_OP_SLOT_FLT},
        /* Remove original geometry. */
        {"use_replace", BMO_OP_SLOT_BOOL},
        /* Inset face boundaries. */
        {"use_boundary", BMO_OP_SLOT_BOOL},
        /* Scale the offset to give more even thickness. */
        {"use_even_offset", BMO_OP_SLOT_BOOL},
        /* Crease hub edges for improved subdivision surface. */
        {"use_crease", BMO_OP_SLOT_BOOL},
        /* The mean crease weight for resulting edges. */
        {"crease_weight", BMO_OP_SLOT_FLT},
        /* Scale the offset by surrounding geometry. */
        {"use_relative_offset", BMO_OP_SLOT_BOOL},
        /* Offset material index of generated faces. */
        {"material_offset", BMO_OP_SLOT_INT},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_wireframe_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

static BMO_FlagSet bmo_enum_poke_center_mode[] = {
    {BMOP_POKE_MEDIAN_WEIGHTED, "MEAN_WEIGHTED"},
    {BMOP_POKE_MEDIAN, "MEAN"},
    {BMOP_POKE_BOUNDS, "BOUNDS"},
    {0, nullptr},
};

/*
 * Pokes a face.
 *
 * Splits a face into a triangle fan.
 */
static BMOpDefine bmo_poke_def = {
    /*opname*/ "poke",
    /*slot_types_in*/
    {
        /* Input faces. */
        {"faces", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        /* Center vertex offset along normal. */
        {"offset", BMO_OP_SLOT_FLT},
        /* Calculation mode for center vertex. */
        {"center_mode",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_poke_center_mode},
        /* Apply offset. */
        {"use_relative_offset", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        /* Output verts. */
        {"verts.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT}},
        /* Output faces. */
        {"faces.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_poke_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
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
    /*opname*/ "convex_hull",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"input", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Skip hull triangles that are covered by a pre-existing face. */
        {"use_existing_faces", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {"geom_interior.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {"geom_unused.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {"geom_holes.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_convex_hull_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
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
    /*opname*/ "symmetrize",
    /*slot_types_in*/
    {
        /* Input geometry. */
        {"input", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        /* Axis to use. */
        {"direction",
         BMO_OP_SLOT_INT,
         to_subtype_union(BMO_OP_SLOT_SUBTYPE_INT_ENUM),
         bmo_enum_axis_neg_xyz_and_xyz},
        /* Minimum distance. */
        {"dist", BMO_OP_SLOT_FLT},
        /* Transform shape keys too. */
        {"use_shapekey", BMO_OP_SLOT_BOOL},
        {{'\0'}},
    },
    /*slot_types_out*/
    {
        {"geom.out", BMO_OP_SLOT_ELEMENT_BUF, {BM_VERT | BM_EDGE | BM_FACE}},
        {{'\0'}},
    },
    /*init*/ nullptr,
    /*exec*/ bmo_symmetrize_exec,
    /*type_flag*/
    (BMO_OPTYPE_FLAG_NORMALS_CALC | BMO_OPTYPE_FLAG_SELECT_FLUSH |
     BMO_OPTYPE_FLAG_SELECT_VALIDATE),
};

#undef BM_FACE
#undef BM_EDGE
#undef BM_VERT

const BMOpDefine *bmo_opdefines[] = {
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
    &bmo_flip_quad_tessellation_def,
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
