/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

/* Implementation based in part off the paper "B-Mesh: A Fast Modeling
 * System for Base Meshes of 3D Articulated Shapes" (Zhongping Ji,
 * Ligang Liu, Yigang Wang)
 *
 * Note that to avoid confusion with Blender's BMesh data structure,
 * this tool is renamed as the Skin modifier.
 *
 * The B-Mesh paper is current available here:
 * http://www.math.zju.edu.cn/ligangliu/CAGD/Projects/BMesh/
 *
 * The main missing features in this code compared to the paper are:
 *
 * - No mesh evolution. The paper suggests iteratively subdivision-surfacing the
 *   skin output and adapting the output to better conform with the
 *   spheres of influence surrounding each vertex.
 *
 * - No mesh fairing. The paper suggests re-aligning output edges to
 *   follow principal mesh curvatures.
 *
 * - No auxiliary balls. These would serve to influence mesh
 *   evolution, which as noted above is not implemented.
 *
 * The code also adds some features not present in the paper:
 *
 * + Loops in the input edge graph.
 *
 * + Concave surfaces around branch nodes. The paper does not discuss
 *   how to handle non-convex regions; this code adds a number of
 *   cleanup operations to handle many (though not all) of these
 *   cases.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_bitmap.h"
#include "BLI_enum_flags.hh"
#include "BLI_heap_simple.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_stack.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_types.hh" /* For skin mark clear operator UI. */

#include "MOD_ui_common.hh"

#include "bmesh.hh"

/* -------------------------------------------------------------------- */
/** \name Generic BMesh Utilities
 * \{ */

static void vert_face_normal_mark_set(BMVert *v)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
    f->no[0] = FLT_MAX;
  }
}

static void vert_face_normal_mark_update(BMVert *v)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
    if (f->no[0] == FLT_MAX) {
      BM_face_normal_update(f);
    }
  }
}

/**
 * Recalculate the normals of all faces connected to `verts`.
 */
static void vert_array_face_normal_update(BMVert **verts, int verts_len)
{
  for (int i = 0; i < verts_len; i++) {
    vert_face_normal_mark_set(verts[i]);
  }

  for (int i = 0; i < verts_len; i++) {
    vert_face_normal_mark_update(verts[i]);
  }
}

/** \} */

struct EMat {
  float mat[3][3];
  /* Vert that edge is pointing away from, no relation to
   * edge[0] */
  int origin;
};

enum SkinNodeFlag {
  CAP_START = 1,
  CAP_END = 2,
  SEAM_FRAME = 4,
  FLIP_NORMAL = 8,
};
ENUM_OPERATORS(SkinNodeFlag);

struct Frame {
  /* Index in the vertex array */
  BMVert *verts[4];
  /* Location of each corner */
  float co[4][3];
  /* Indicates which corners have been merged with another
   * frame's corner (so they share a vertex index) */
  struct {
    /* Merge to target frame/corner (no merge if frame is null) */
    Frame *frame;
    int corner;
    /* checked to avoid chaining.
     * (merging when we're already been referenced), see #39775 */
    uint is_target : 1;
  } merge[4];

  /* For hull frames, whether each vertex is detached or not */
  bool inside_hull[4];
  /* Whether any part of the frame (corner or edge) is detached */
  bool detached;
};

#define MAX_SKIN_NODE_FRAMES 2
struct SkinNode {
  Frame frames[MAX_SKIN_NODE_FRAMES];
  int totframe;

  SkinNodeFlag flag;

  /* Used for hulling a loop seam */
  int seam_edges[2];
};

struct SkinOutput {
  BMesh *bm;
  SkinModifierData *smd;
  int mat_nr;
};

static void add_poly(SkinOutput *so, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4);

/***************************** Convex Hull ****************************/

static bool is_quad_symmetric(BMVert *quad[4], const SkinModifierData *smd)
{
  const float threshold = 0.0001f;
  const float threshold_squared = threshold * threshold;
  int axis;

  for (axis = 0; axis < 3; axis++) {
    if (smd->symmetry_axes & (1 << axis)) {
      float a[3];

      copy_v3_v3(a, quad[0]->co);
      a[axis] = -a[axis];

      if (len_squared_v3v3(a, quad[1]->co) < threshold_squared) {
        copy_v3_v3(a, quad[2]->co);
        a[axis] = -a[axis];
        if (len_squared_v3v3(a, quad[3]->co) < threshold_squared) {
          return true;
        }
      }
      else if (len_squared_v3v3(a, quad[3]->co) < threshold_squared) {
        copy_v3_v3(a, quad[2]->co);
        a[axis] = -a[axis];
        if (len_squared_v3v3(a, quad[1]->co) < threshold_squared) {
          return true;
        }
      }
    }
  }

  return false;
}

/* Returns true if the quad crosses the plane of symmetry, false otherwise */
static bool quad_crosses_symmetry_plane(BMVert *quad[4], const SkinModifierData *smd)
{
  int axis;

  for (axis = 0; axis < 3; axis++) {
    if (smd->symmetry_axes & (1 << axis)) {
      bool left = false, right = false;
      int i;

      for (i = 0; i < 4; i++) {
        if (quad[i]->co[axis] < 0.0f) {
          left = true;
        }
        else if (quad[i]->co[axis] > 0.0f) {
          right = true;
        }

        if (left && right) {
          return true;
        }
      }
    }
  }

  return false;
}

#ifdef WITH_BULLET

/* Returns true if the frame is filled by precisely two faces (and
 * outputs those faces to fill_faces), otherwise returns false. */
static bool skin_frame_find_contained_faces(const Frame *frame, BMFace *fill_faces[2])
{
  BMEdge *diag;

  /* See if the frame is bisected by a diagonal edge */
  diag = BM_edge_exists(frame->verts[0], frame->verts[2]);
  if (!diag) {
    diag = BM_edge_exists(frame->verts[1], frame->verts[3]);
  }

  if (diag) {
    return BM_edge_face_pair(diag, &fill_faces[0], &fill_faces[1]);
  }

  return false;
}

#endif

/* Returns true if hull is successfully built, false otherwise */
static bool build_hull(SkinOutput *so, Frame **frames, int totframe)
{
#ifdef WITH_BULLET
  BMesh *bm = so->bm;
  BMOperator op;
  BMIter iter;
  BMOIter oiter;
  BMVert *v;
  BMFace *f;
  BMEdge *e;
  int i, j;

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

  for (i = 0; i < totframe; i++) {
    for (j = 0; j < 4; j++) {
      BM_elem_flag_enable(frames[i]->verts[j], BM_ELEM_TAG);
    }
  }

  /* Deselect all faces so that only new hull output faces are
   * selected after the operator is run */
  BM_mesh_elem_hflag_disable_all(bm, BM_ALL_NOLOOP, BM_ELEM_SELECT, false);

  BMO_op_initf(
      bm, &op, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE), "convex_hull input=%hv", BM_ELEM_TAG);
  BMO_op_exec(bm, &op);

  if (BMO_error_occurred_at_level(bm, BMO_ERROR_CANCEL)) {
    BMO_op_finish(bm, &op);
    return false;
  }

  /* Apply face attributes to hull output */
  BMO_ITER (f, &oiter, op.slots_out, "geom.out", BM_FACE) {
    BM_face_normal_update(f);
    if (so->smd->flag & MOD_SKIN_SMOOTH_SHADING) {
      BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
    }
    f->mat_nr = so->mat_nr;
  }

  /* Mark interior frames */
  BMO_ITER (v, &oiter, op.slots_out, "geom_interior.out", BM_VERT) {
    for (i = 0; i < totframe; i++) {
      Frame *frame = frames[i];

      if (!frame->detached) {
        for (j = 0; j < 4; j++) {
          if (frame->verts[j] == v) {
            frame->inside_hull[j] = true;
            frame->detached = true;
            break;
          }
        }
      }
    }
  }

  /* Also mark frames as interior if an edge is not in the hull */
  for (i = 0; i < totframe; i++) {
    Frame *frame = frames[i];

    if (!frame->detached && (!BM_edge_exists(frame->verts[0], frame->verts[1]) ||
                             !BM_edge_exists(frame->verts[1], frame->verts[2]) ||
                             !BM_edge_exists(frame->verts[2], frame->verts[3]) ||
                             !BM_edge_exists(frame->verts[3], frame->verts[0])))
    {
      frame->detached = true;
    }
  }

  /* Remove triangles that would fill the original frames -- skip if
   * frame is partially detached */
  BM_mesh_elem_hflag_disable_all(bm, BM_ALL_NOLOOP, BM_ELEM_TAG, false);
  for (i = 0; i < totframe; i++) {
    Frame *frame = frames[i];
    if (!frame->detached) {
      BMFace *fill_faces[2];

      /* Check if the frame is filled by precisely two
       * triangles. If so, delete the triangles and their shared
       * edge. Otherwise, give up and mark the frame as
       * detached. */
      if (skin_frame_find_contained_faces(frame, fill_faces)) {
        BM_elem_flag_enable(fill_faces[0], BM_ELEM_TAG);
        BM_elem_flag_enable(fill_faces[1], BM_ELEM_TAG);
      }
      else {
        frame->detached = true;
      }
    }
  }

  /* Check if removing triangles above will create wire triangles,
   * mark them too */
  BMO_ITER (e, &oiter, op.slots_out, "geom.out", BM_EDGE) {
    bool is_wire = true;
    BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
      if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
        is_wire = false;
        break;
      }
    }
    if (is_wire) {
      BM_elem_flag_enable(e, BM_ELEM_TAG);
    }
  }

  BMO_op_finish(bm, &op);

  BM_mesh_delete_hflag_tagged(bm, BM_ELEM_TAG, BM_EDGE | BM_FACE);

  return true;
#else
  UNUSED_VARS(so, frames, totframe);
  return false;
#endif
}

/* Returns the average frame side length (frames are rectangular, so
 * just the average of two adjacent edge lengths) */
static float frame_len(const Frame *frame)
{
  return (len_v3v3(frame->co[0], frame->co[1]) + len_v3v3(frame->co[1], frame->co[2])) * 0.5f;
}

static void merge_frame_corners(Frame **frames, int totframe)
{
  float dist, side_a, side_b, thresh, mid[3];
  int i, j, k, l;

  for (i = 0; i < totframe; i++) {
    side_a = frame_len(frames[i]);

    /* For each corner of each frame... */
    for (j = 0; j < 4; j++) {

      /* Ensure the merge target is not itself a merge target */
      if (frames[i]->merge[j].frame) {
        continue;
      }

      for (k = i + 1; k < totframe; k++) {
        BLI_assert(frames[i] != frames[k]);

        side_b = frame_len(frames[k]);
        thresh = min_ff(side_a, side_b) / 2.0f;

        /* Compare with each corner of all other frames... */
        for (l = 0; l < 4; l++) {
          if (frames[k]->merge[l].frame || frames[k]->merge[l].is_target) {
            continue;
          }

          /* Some additional concerns that could be checked
           * further:
           *
           * + Vertex coords are being used for the
           *   edge-length test, but are also being
           *   modified, might cause symmetry problems.
           *
           * + A frame could be merged diagonally across
           *   another, would generate a weird (bad) T
           *   junction
           */

          /* Check if corners are near each other, where
           * 'near' is based in the frames' minimum side
           * length */
          dist = len_v3v3(frames[i]->co[j], frames[k]->co[l]);
          if (dist < thresh) {
            mid_v3_v3v3(mid, frames[i]->co[j], frames[k]->co[l]);

            copy_v3_v3(frames[i]->co[j], mid);
            copy_v3_v3(frames[k]->co[l], mid);

            frames[k]->merge[l].frame = frames[i];
            frames[k]->merge[l].corner = j;
            frames[i]->merge[j].is_target = true;

            /* Can't merge another corner into the same
             * frame corner, so move on to frame k+1 */
            break;
          }
        }
      }
    }
  }
}

static Frame **collect_hull_frames(int v,
                                   SkinNode *frames,
                                   blender::GroupedSpan<int> emap,
                                   const blender::Span<blender::int2> edges,
                                   int *tothullframe)
{
  SkinNode *f;
  Frame **hull_frames;
  int hull_frames_num, i;

  (*tothullframe) = emap[v].size();
  hull_frames = MEM_calloc_arrayN<Frame *>(*tothullframe, __func__);
  hull_frames_num = 0;
  for (i = 0; i < emap[v].size(); i++) {
    const blender::int2 &edge = edges[emap[v][i]];
    f = &frames[blender::bke::mesh::edge_other_vert(edge, v)];
    /* Can't have adjacent branch nodes yet */
    if (f->totframe) {
      hull_frames[hull_frames_num++] = &f->frames[0];
    }
    else {
      (*tothullframe)--;
    }
  }

  return hull_frames;
}

/**************************** Create Frames ***************************/

static void node_frames_init(SkinNode *nf, int totframe)
{
  int i;

  nf->totframe = totframe;
  memset(nf->frames, 0, sizeof(nf->frames));

  nf->flag = SkinNodeFlag(0);
  for (i = 0; i < 2; i++) {
    nf->seam_edges[i] = -1;
  }
}

static void create_frame(
    Frame *frame, const float co[3], const float radius[2], const float mat[3][3], float offset)
{
  float rx[3], ry[3], rz[3];
  int i;

  mul_v3_v3fl(ry, mat[1], radius[0]);
  mul_v3_v3fl(rz, mat[2], radius[1]);

  add_v3_v3v3(frame->co[3], co, ry);
  add_v3_v3v3(frame->co[3], frame->co[3], rz);

  sub_v3_v3v3(frame->co[2], co, ry);
  add_v3_v3v3(frame->co[2], frame->co[2], rz);

  sub_v3_v3v3(frame->co[1], co, ry);
  sub_v3_v3v3(frame->co[1], frame->co[1], rz);

  add_v3_v3v3(frame->co[0], co, ry);
  sub_v3_v3v3(frame->co[0], frame->co[0], rz);

  mul_v3_v3fl(rx, mat[0], offset);
  for (i = 0; i < 4; i++) {
    add_v3_v3v3(frame->co[i], frame->co[i], rx);
  }
}

static float half_v2(const float v[2])
{
  return (v[0] + v[1]) * 0.5f;
}

static void end_node_frames(int v,
                            SkinNode *skin_nodes,
                            const blender::Span<blender::float3> vert_positions,
                            const MVertSkin *nodes,
                            blender::GroupedSpan<int> emap,
                            EMat *emat)
{
  const float *rad = nodes[v].radius;
  float mat[3][3];

  if (emap[v].is_empty()) {
    float avg = half_v2(rad);

    /* For solitary nodes, just build a box (two frames) */
    node_frames_init(&skin_nodes[v], 2);
    skin_nodes[v].flag |= (CAP_START | CAP_END);

    /* Hardcoded basis */
    zero_m3(mat);
    mat[0][2] = mat[1][0] = mat[2][1] = 1;

    /* Caps */
    create_frame(&skin_nodes[v].frames[0], vert_positions[v], rad, mat, avg);
    create_frame(&skin_nodes[v].frames[1], vert_positions[v], rad, mat, -avg);
  }
  else {
    /* For nodes with an incoming edge, create a single (capped) frame */
    node_frames_init(&skin_nodes[v], 1);
    skin_nodes[v].flag |= CAP_START;

    /* Use incoming edge for orientation */
    copy_m3_m3(mat, emat[emap[v][0]].mat);
    if (emat[emap[v][0]].origin != v) {
      negate_v3(mat[0]);
    }

    Frame *frame = &skin_nodes[v].frames[0];

    /* End frame */
    create_frame(frame, vert_positions[v], rad, mat, 0);

    /* The caps might need to have their normals inverted. So check if they
     * need to be flipped when creating faces. */
    float normal[3];
    normal_quad_v3(normal, frame->co[0], frame->co[1], frame->co[2], frame->co[3]);
    if (dot_v3v3(mat[0], normal) < 0.0f) {
      skin_nodes[v].flag |= FLIP_NORMAL;
    }
  }
}

/* Returns 1 for seam, 0 otherwise */
static int connection_node_mat(float mat[3][3], int v, blender::GroupedSpan<int> emap, EMat *emat)
{
  float axis[3], angle, ine[3][3], oute[3][3];
  EMat *e1, *e2;

  e1 = &emat[emap[v][0]];
  e2 = &emat[emap[v][1]];

  if (e1->origin != v && e2->origin == v) {
    copy_m3_m3(ine, e1->mat);
    copy_m3_m3(oute, e2->mat);
  }
  else if (e1->origin == v && e2->origin != v) {
    copy_m3_m3(ine, e2->mat);
    copy_m3_m3(oute, e1->mat);
  }
  else {
    return 1;
  }

  /* Get axis and angle to rotate frame by */
  angle = angle_normalized_v3v3(ine[0], oute[0]) / 2.0f;
  cross_v3_v3v3(axis, ine[0], oute[0]);
  normalize_v3(axis);

  /* Build frame matrix (don't care about X axis here) */
  copy_v3_v3(mat[0], ine[0]);
  rotate_normalized_v3_v3v3fl(mat[1], ine[1], axis, angle);
  rotate_normalized_v3_v3v3fl(mat[2], ine[2], axis, angle);

  return 0;
}

static void connection_node_frames(int v,
                                   SkinNode *skin_nodes,
                                   const blender::Span<blender::float3> vert_positions,
                                   const MVertSkin *nodes,
                                   blender::GroupedSpan<int> emap,
                                   EMat *emat)
{
  const float *rad = nodes[v].radius;
  float mat[3][3];
  EMat *e1, *e2;

  if (connection_node_mat(mat, v, emap, emat)) {
    float avg = half_v2(rad);

    /* Get edges */
    e1 = &emat[emap[v][0]];
    e2 = &emat[emap[v][1]];

    /* Handle seam separately to avoid twisting */
    /* Create two frames, will be hulled to neighbors later */
    node_frames_init(&skin_nodes[v], 2);
    skin_nodes[v].flag |= SEAM_FRAME;

    copy_m3_m3(mat, e1->mat);
    if (e1->origin != v) {
      negate_v3(mat[0]);
    }
    create_frame(&skin_nodes[v].frames[0], vert_positions[v], rad, mat, avg);
    skin_nodes[v].seam_edges[0] = emap[v][0];

    copy_m3_m3(mat, e2->mat);
    if (e2->origin != v) {
      negate_v3(mat[0]);
    }
    create_frame(&skin_nodes[v].frames[1], vert_positions[v], rad, mat, avg);
    skin_nodes[v].seam_edges[1] = emap[v][1];

    return;
  }

  /* Build regular frame */
  node_frames_init(&skin_nodes[v], 1);
  create_frame(&skin_nodes[v].frames[0], vert_positions[v], rad, mat, 0);
}

static SkinNode *build_frames(const blender::Span<blender::float3> vert_positions,
                              int verts_num,
                              const MVertSkin *nodes,
                              blender::GroupedSpan<int> emap,
                              EMat *emat)
{
  int v;

  SkinNode *skin_nodes = MEM_calloc_arrayN<SkinNode>(verts_num, __func__);

  for (v = 0; v < verts_num; v++) {
    if (emap[v].size() <= 1) {
      end_node_frames(v, skin_nodes, vert_positions, nodes, emap, emat);
    }
    else if (emap[v].size() == 2) {
      connection_node_frames(v, skin_nodes, vert_positions, nodes, emap, emat);
    }
    else {
      /* Branch node generates no frames */
    }
  }

  return skin_nodes;
}

/**************************** Edge Matrices ***************************/

static void calc_edge_mat(float mat[3][3], const float a[3], const float b[3])
{
  const float z_up[3] = {0, 0, 1};
  float dot;

  /* X = edge direction */
  sub_v3_v3v3(mat[0], b, a);
  normalize_v3(mat[0]);

  dot = dot_v3v3(mat[0], z_up);
  if (dot > -1 + FLT_EPSILON && dot < 1 - FLT_EPSILON) {
    /* Y = Z cross x */
    cross_v3_v3v3(mat[1], z_up, mat[0]);
    normalize_v3(mat[1]);

    /* Z = x cross y */
    cross_v3_v3v3(mat[2], mat[0], mat[1]);
    normalize_v3(mat[2]);
  }
  else {
    mat[1][0] = 1;
    mat[1][1] = 0;
    mat[1][2] = 0;
    mat[2][0] = 0;
    mat[2][1] = 1;
    mat[2][2] = 0;
  }
}

struct EdgeStackElem {
  float mat[3][3];
  int parent_v;
  int e;
};

static void build_emats_stack(BLI_Stack *stack,
                              BLI_bitmap *visited_e,
                              EMat *emat,
                              blender::GroupedSpan<int> emap,
                              const blender::Span<blender::int2> edges,
                              const MVertSkin *vs,
                              const blender::Span<blender::float3> vert_positions)
{
  EdgeStackElem stack_elem;
  float axis[3], angle;
  int i, e, v, parent_v, parent_is_branch;

  BLI_stack_pop(stack, &stack_elem);
  parent_v = stack_elem.parent_v;
  e = stack_elem.e;

  /* Skip if edge already visited */
  if (BLI_BITMAP_TEST(visited_e, e)) {
    return;
  }

  /* Mark edge as visited */
  BLI_BITMAP_ENABLE(visited_e, e);

  /* Process edge */

  parent_is_branch = ((emap[parent_v].size() > 2) || (vs[parent_v].flag & MVERT_SKIN_ROOT));

  v = blender::bke::mesh::edge_other_vert(edges[e], parent_v);
  emat[e].origin = parent_v;

  /* If parent is a branch node, start a new edge chain */
  if (parent_is_branch) {
    calc_edge_mat(emat[e].mat, vert_positions[parent_v], vert_positions[v]);
  }
  else {
    /* Build edge matrix guided by parent matrix */
    sub_v3_v3v3(emat[e].mat[0], vert_positions[v], vert_positions[parent_v]);
    normalize_v3(emat[e].mat[0]);
    angle = angle_normalized_v3v3(stack_elem.mat[0], emat[e].mat[0]);
    cross_v3_v3v3(axis, stack_elem.mat[0], emat[e].mat[0]);
    normalize_v3(axis);
    rotate_normalized_v3_v3v3fl(emat[e].mat[1], stack_elem.mat[1], axis, angle);
    rotate_normalized_v3_v3v3fl(emat[e].mat[2], stack_elem.mat[2], axis, angle);
  }

  /* Add neighbors to stack */
  for (i = 0; i < emap[v].size(); i++) {
    /* Add neighbors to stack */
    copy_m3_m3(stack_elem.mat, emat[e].mat);
    stack_elem.e = emap[v][i];
    stack_elem.parent_v = v;
    BLI_stack_push(stack, &stack_elem);
  }
}

static EMat *build_edge_mats(const MVertSkin *vs,
                             const blender::Span<blender::float3> vert_positions,
                             const int verts_num,
                             const blender::Span<blender::int2> edges,
                             blender::GroupedSpan<int> emap,
                             bool *has_valid_root)
{
  BLI_Stack *stack;
  EMat *emat;
  EdgeStackElem stack_elem;
  BLI_bitmap *visited_e;
  int i, v;

  stack = BLI_stack_new(sizeof(stack_elem), "build_edge_mats.stack");

  visited_e = BLI_BITMAP_NEW(edges.size(), "build_edge_mats.visited_e");
  emat = MEM_calloc_arrayN<EMat>(edges.size(), __func__);

  /* Edge matrices are built from the root nodes, add all roots with
   * children to the stack */
  for (v = 0; v < verts_num; v++) {
    if (vs[v].flag & MVERT_SKIN_ROOT) {
      if (emap[v].size() >= 1) {
        const blender::int2 &edge = edges[emap[v][0]];
        calc_edge_mat(stack_elem.mat,
                      vert_positions[v],
                      vert_positions[blender::bke::mesh::edge_other_vert(edge, v)]);
        stack_elem.parent_v = v;

        /* Add adjacent edges to stack */
        for (i = 0; i < emap[v].size(); i++) {
          stack_elem.e = emap[v][i];
          BLI_stack_push(stack, &stack_elem);
        }

        *has_valid_root = true;
      }
      else if (edges.is_empty()) {
        /* Vertex-only mesh is valid, mark valid root as well (will display error otherwise). */
        *has_valid_root = true;
        break;
      }
    }
  }

  while (!BLI_stack_is_empty(stack)) {
    build_emats_stack(stack, visited_e, emat, emap, edges, vs, vert_positions);
  }

  MEM_freeN(visited_e);
  BLI_stack_free(stack);

  return emat;
}

/************************** Input Subdivision *************************/

/* Returns number of edge subdivisions, taking into account the radius
 * of the endpoints and the edge length. If both endpoints are branch
 * nodes, at least two intermediate frames are required. (This avoids
 * having any special cases for dealing with sharing a frame between
 * two hulls.) */
static int calc_edge_subdivisions(const blender::Span<blender::float3> vert_positions,
                                  const MVertSkin *nodes,
                                  const blender::int2 &edge,
                                  const blender::Span<int> degree)
{
  /* prevent memory errors #38003. */
#define NUM_SUBDIVISIONS_MAX 128

  const MVertSkin *evs[2] = {&nodes[edge[0]], &nodes[edge[1]]};
  float avg_radius;
  const bool v1_branch = degree[edge[0]] > 2;
  const bool v2_branch = degree[edge[1]] > 2;
  int subdivisions_num;

  /* If either end is a branch node marked 'loose', don't subdivide
   * the edge (or subdivide just twice if both are branches) */
  if ((v1_branch && (evs[0]->flag & MVERT_SKIN_LOOSE)) ||
      (v2_branch && (evs[1]->flag & MVERT_SKIN_LOOSE)))
  {
    if (v1_branch && v2_branch) {
      return 2;
    }

    return 0;
  }

  avg_radius = half_v2(evs[0]->radius) + half_v2(evs[1]->radius);

  if (avg_radius != 0.0f) {
    /* possible (but unlikely) that we overflow INT_MAX */
    float subdivisions_num_fl;
    const float edge_len = len_v3v3(vert_positions[edge[0]], vert_positions[edge[1]]);
    subdivisions_num_fl = (edge_len / avg_radius);
    if (subdivisions_num_fl < NUM_SUBDIVISIONS_MAX) {
      subdivisions_num = int(subdivisions_num_fl);
    }
    else {
      subdivisions_num = NUM_SUBDIVISIONS_MAX;
    }
  }
  else {
    subdivisions_num = 0;
  }

  /* If both ends are branch nodes, two intermediate nodes are
   * required */
  if (subdivisions_num < 2 && v1_branch && v2_branch) {
    subdivisions_num = 2;
  }

  return subdivisions_num;

#undef NUM_SUBDIVISIONS_MAX
}

/* Take a Mesh and subdivide its edges to keep skin nodes
 * reasonably close. */
static Mesh *subdivide_base(const Mesh *orig)
{
  int subd_num;
  int i, j, k, u, v;
  float radrat;

  const MVertSkin *orignode = static_cast<const MVertSkin *>(
      CustomData_get_layer(&orig->vert_data, CD_MVERT_SKIN));
  const blender::Span<blender::float3> orig_vert_positions = orig->vert_positions();
  const blender::Span<blender::int2> orig_edges = orig->edges();
  const MDeformVert *origdvert = orig->deform_verts().data();
  int orig_vert_num = orig->verts_num;
  int orig_edge_num = orig->edges_num;

  /* Get degree of all vertices */
  blender::Array<int> degree(orig_vert_num, 0);
  blender::array_utils::count_indices(orig_edges.cast<int>(), degree);

  /* Per edge, store how many subdivisions are needed */
  blender::Array<int> edge_subd(orig_edge_num, 0);
  for (i = 0, subd_num = 0; i < orig_edge_num; i++) {
    edge_subd[i] += calc_edge_subdivisions(orig_vert_positions, orignode, orig_edges[i], degree);
    BLI_assert(edge_subd[i] >= 0);
    subd_num += edge_subd[i];
  }

  /* Allocate output mesh */
  Mesh *result = BKE_mesh_new_nomain_from_template(
      orig, orig_vert_num + subd_num, orig_edge_num + subd_num, 0, 0);

  blender::MutableSpan<blender::float3> out_vert_positions = result->vert_positions_for_write();
  blender::MutableSpan<blender::int2> result_edges = result->edges_for_write();
  MVertSkin *outnode = static_cast<MVertSkin *>(
      CustomData_get_layer_for_write(&result->vert_data, CD_MVERT_SKIN, result->verts_num));
  MDeformVert *outdvert = nullptr;
  if (origdvert) {
    outdvert = result->deform_verts_for_write().data();
  }

  /* Copy original vertex data */
  CustomData_copy_data(&orig->vert_data, &result->vert_data, 0, 0, orig_vert_num);

  /* Subdivide edges */
  int result_edge_i = 0;
  for (i = 0, v = orig_vert_num; i < orig_edge_num; i++) {
    struct VGroupData {
      /* Vertex group number */
      int def_nr;
      float w1, w2;
    };
    VGroupData *vgroups = nullptr, *vg;
    int vgroups_num = 0;

    const blender::int2 &edge = orig_edges[i];

    if (origdvert) {
      const MDeformVert *dv1 = &origdvert[edge[0]];
      const MDeformVert *dv2 = &origdvert[edge[1]];
      vgroups = MEM_calloc_arrayN<VGroupData>(dv1->totweight, __func__);

      /* Only want vertex groups used by both vertices */
      for (j = 0; j < dv1->totweight; j++) {
        vg = nullptr;
        for (k = 0; k < dv2->totweight; k++) {
          if (dv1->dw[j].def_nr == dv2->dw[k].def_nr) {
            vg = &vgroups[vgroups_num];
            vgroups_num++;
            break;
          }
        }

        if (vg) {
          vg->def_nr = dv1->dw[j].def_nr;
          vg->w1 = dv1->dw[j].weight;
          vg->w2 = dv2->dw[k].weight;
        }
      }
    }

    u = edge[0];
    radrat = (half_v2(outnode[edge[1]].radius) / half_v2(outnode[edge[0]].radius));
    if (isfinite(radrat)) {
      radrat = (radrat + 1) / 2;
    }
    else {
      /* Happens when skin is scaled to zero. */
      radrat = 1.0f;
    }

    /* Add vertices and edge segments */
    for (j = 0; j < edge_subd[i]; j++, v++) {
      float r = (j + 1) / float(edge_subd[i] + 1);
      float t = powf(r, radrat);

      /* Interpolate vertex coord */
      interp_v3_v3v3(
          out_vert_positions[v], out_vert_positions[edge[0]], out_vert_positions[edge[1]], t);

      /* Interpolate skin radii */
      interp_v3_v3v3(outnode[v].radius, orignode[edge[0]].radius, orignode[edge[1]].radius, t);

      /* Interpolate vertex group weights */
      for (k = 0; k < vgroups_num; k++) {
        float weight;

        vg = &vgroups[k];
        weight = interpf(vg->w2, vg->w1, t);

        if (weight > 0) {
          BKE_defvert_add_index_notest(&outdvert[v], vg->def_nr, weight);
        }
      }

      result_edges[result_edge_i][0] = u;
      result_edges[result_edge_i][1] = v;
      result_edge_i++;
      u = v;
    }

    if (vgroups) {
      MEM_freeN(vgroups);
    }

    /* Link up to final vertex */
    result_edges[result_edge_i][0] = u;
    result_edges[result_edge_i][1] = edge[1];
    result_edge_i++;
  }

  return result;
}

/******************************* Output *******************************/

/* Can be either quad or triangle */
static void add_poly(SkinOutput *so, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4)
{
  BMVert *verts[4] = {v1, v2, v3, v4};
  BMFace *f;

  BLI_assert(!ELEM(v1, v2, v3, v4));
  BLI_assert(!ELEM(v2, v3, v4));
  BLI_assert(v3 != v4);
  BLI_assert(v1 && v2 && v3);

  f = BM_face_create_verts(so->bm, verts, v4 ? 4 : 3, nullptr, BM_CREATE_NO_DOUBLE, true);
  BM_face_normal_update(f);
  if (so->smd->flag & MOD_SKIN_SMOOTH_SHADING) {
    BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
  }
  f->mat_nr = so->mat_nr;
}

static void connect_frames(SkinOutput *so, BMVert *frame1[4], BMVert *frame2[4])
{
  BMVert *q[4][4] = {
      {frame2[0], frame2[1], frame1[1], frame1[0]},
      {frame2[1], frame2[2], frame1[2], frame1[1]},
      {frame2[2], frame2[3], frame1[3], frame1[2]},
      {frame2[3], frame2[0], frame1[0], frame1[3]},
  };
  int i;
  bool swap;

  /* Check if frame normals need swap */
#if 0
  {
    /* simple method, works mostly */
    float p[3], no[3];
    sub_v3_v3v3(p, q[3][0]->co, q[0][0]->co);
    normal_quad_v3(no, q[0][0]->co, q[0][1]->co, q[0][2]->co, q[0][3]->co);
    swap = dot_v3v3(no, p) > 0;
  }
#else
  {
    /* comprehensive method, accumulate flipping of all faces */
    float cent_sides[4][3];
    float cent[3];
    float dot = 0.0f;

    for (i = 0; i < 4; i++) {
      mid_v3_v3v3v3v3(cent_sides[i], UNPACK4_EX(, q[i], ->co));
    }
    mid_v3_v3v3v3v3(cent, UNPACK4(cent_sides));

    for (i = 0; i < 4; i++) {
      float p[3], no[3];
      normal_quad_v3(no, UNPACK4_EX(, q[i], ->co));
      sub_v3_v3v3(p, cent, cent_sides[i]);
      dot += dot_v3v3(no, p);
    }

    swap = dot > 0;
  }
#endif

  for (i = 0; i < 4; i++) {
    if (swap) {
      add_poly(so, q[i][3], q[i][2], q[i][1], q[i][0]);
    }
    else {
      add_poly(so, q[i][0], q[i][1], q[i][2], q[i][3]);
    }
  }
}

static void output_frames(BMesh *bm, SkinNode *sn, const MDeformVert *input_dvert)
{
  Frame *f;
  int i, j;

  /* Output all frame verts */
  for (i = 0; i < sn->totframe; i++) {
    f = &sn->frames[i];
    for (j = 0; j < 4; j++) {
      if (!f->merge[j].frame) {
        BMVert *v = f->verts[j] = BM_vert_create(bm, f->co[j], nullptr, BM_CREATE_NOP);

        if (input_dvert) {
          MDeformVert *dv = static_cast<MDeformVert *>(
              CustomData_bmesh_get(&bm->vdata, v->head.data, CD_MDEFORMVERT));

          BLI_assert(dv->totweight == 0);
          BKE_defvert_copy(dv, input_dvert);
        }
      }
    }
  }
}

#define PRINT_HOLE_INFO 0

static void calc_frame_center(float center[3], const Frame *frame)
{
  add_v3_v3v3(center, frame->verts[0]->co, frame->verts[1]->co);
  add_v3_v3(center, frame->verts[2]->co);
  add_v3_v3(center, frame->verts[3]->co);
  mul_v3_fl(center, 0.25f);
}

/* Does crappy fan triangulation of face, may not be so accurate for
 * concave faces */
static int isect_ray_poly(const float ray_start[3],
                          const float ray_dir[3],
                          BMFace *f,
                          float *r_lambda)
{
  BMVert *v, *v_first = nullptr, *v_prev = nullptr;
  BMIter iter;
  float best_dist = FLT_MAX;
  bool hit = false;

  BM_ITER_ELEM (v, &iter, f, BM_VERTS_OF_FACE) {
    if (!v_first) {
      v_first = v;
    }
    else if (v_prev != v_first) {
      float dist;
      bool curhit;

      curhit = isect_ray_tri_v3(
          ray_start, ray_dir, v_first->co, v_prev->co, v->co, &dist, nullptr);
      if (curhit && dist < best_dist) {
        hit = true;
        best_dist = dist;
      }
    }

    v_prev = v;
  }

  *r_lambda = best_dist;
  return hit;
}

/* Reduce the face down to 'n' corners by collapsing the edges;
 * returns the new face.
 *
 * The orig_verts should contain the vertices of 'f'
 */
static BMFace *collapse_face_corners(BMesh *bm, BMFace *f, int n, BMVert **orig_verts)
{
  int orig_len = f->len;

  BLI_assert(n >= 3);
  BLI_assert(f->len > n);
  if (f->len <= n) {
    return f;
  }

  /* Collapse shortest edge for now */
  while (f->len > n) {
    BMFace *vf;
    BMEdge *shortest_edge;
    BMVert *v_safe, *v_merge;
    BMOperator op;
    BMIter iter;
    int i;
    BMOpSlot *slot_targetmap;

    shortest_edge = BM_face_find_shortest_loop(f)->e;
    BMO_op_initf(bm, &op, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE), "weld_verts");

    slot_targetmap = BMO_slot_get(op.slots_in, "targetmap");

    /* NOTE: could probably calculate merges in one go to be
     * faster */

    v_safe = shortest_edge->v1;
    v_merge = shortest_edge->v2;
    mid_v3_v3v3(v_safe->co, v_safe->co, v_merge->co);
    BMO_slot_map_elem_insert(&op, slot_targetmap, v_merge, v_safe);
    BMO_op_exec(bm, &op);
    BMO_op_finish(bm, &op);

    /* Find the new face */
    f = nullptr;
    BM_ITER_ELEM (vf, &iter, v_safe, BM_FACES_OF_VERT) {
      bool wrong_face = false;

      for (i = 0; i < orig_len; i++) {
        if (orig_verts[i] == v_merge) {
          orig_verts[i] = nullptr;
        }
        else if (orig_verts[i] && !BM_vert_in_face(orig_verts[i], vf)) {
          wrong_face = true;
          break;
        }
      }

      if (!wrong_face) {
        f = vf;
        break;
      }
    }

    BLI_assert(f);
  }

  return f;
}

/* Choose a good face to merge the frame with, used in case the frame
 * is completely inside the hull. */
static BMFace *skin_hole_target_face(BMesh *bm, Frame *frame)
{
  BMFace *f, *isect_target_face, *center_target_face;
  BMIter iter;
  float frame_center[3];
  float frame_normal[3];
  float best_isect_dist = FLT_MAX;
  float best_center_dist = FLT_MAX;

  calc_frame_center(frame_center, frame);
  normal_quad_v3(frame_normal,
                 frame->verts[3]->co,
                 frame->verts[2]->co,
                 frame->verts[1]->co,
                 frame->verts[0]->co);

  /* Use a line intersection test and nearest center test against
   * all faces */
  isect_target_face = center_target_face = nullptr;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    float dist, poly_center[3];
    int hit;

    /* Intersection test */
    hit = isect_ray_poly(frame_center, frame_normal, f, &dist);
    if (hit && dist < best_isect_dist) {
      isect_target_face = f;
      best_isect_dist = dist;
    }

    /* Nearest test */
    BM_face_calc_center_median(f, poly_center);
    dist = len_v3v3(frame_center, poly_center);
    if (dist < best_center_dist) {
      center_target_face = f;
      best_center_dist = dist;
    }
  }

  f = isect_target_face;
  if (!f || best_center_dist < best_isect_dist / 2) {
    f = center_target_face;
  }

  /* This case is unlikely now, but could still happen. Should look
   * into splitting edges to make new faces. */
#if PRINT_HOLE_INFO
  if (!f) {
    printf("no good face found\n");
  }
#endif

  return f;
}

/* Use edge-length heuristic to choose from eight possible face bridges */
static void skin_choose_quad_bridge_order(BMVert *a[4], BMVert *b[4], int best_order[4])
{
  int orders[8][4];
  float shortest_len;
  int i, j;

  /* Enumerate all valid orderings */
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      orders[i][j] = (j + i) % 4;
      orders[i + 4][j] = 3 - ((j + i) % 4);
    }
  }

  shortest_len = FLT_MAX;
  for (i = 0; i < 8; i++) {
    float len = 0;

    /* Get total edge length for this configuration */
    for (j = 0; j < 4; j++) {
      len += len_squared_v3v3(a[j]->co, b[orders[i][j]]->co);
    }

    if (len < shortest_len) {
      shortest_len = len;
      memcpy(best_order, orders[i], sizeof(int[4]));
    }
  }
}

static void skin_fix_hole_no_good_verts(BMesh *bm, Frame *frame, BMFace *split_face)
{
  BMFace *f;
  BMVert *verts[4];
  BMOIter oiter;
  BMOperator op;
  int i, best_order[4];
  BMOpSlot *slot_targetmap;

  BLI_assert(split_face->len >= 3);

  /* Extrude the split face */
  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);
  BM_elem_flag_enable(split_face, BM_ELEM_TAG);
  BMO_op_initf(bm,
               &op,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "extrude_discrete_faces faces=%hf",
               BM_ELEM_TAG);
  BMO_op_exec(bm, &op);

  /* Update split face (should only be one new face created
   * during extrusion) */
  split_face = nullptr;
  BMO_ITER (f, &oiter, op.slots_out, "faces.out", BM_FACE) {
    BLI_assert(!split_face);
    split_face = f;
  }

  BMO_op_finish(bm, &op);

  blender::Vector<BMVert *> vert_buf;

  if (split_face->len == 3) {
    BMEdge *longest_edge;

    /* Need at least four ring edges, so subdivide longest edge if
     * face is a triangle */
    longest_edge = BM_face_find_longest_loop(split_face)->e;

    BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
    BM_elem_flag_enable(longest_edge, BM_ELEM_TAG);

    BMO_op_callf(bm,
                 BMO_FLAG_DEFAULTS,
                 "subdivide_edges edges=%he cuts=%i quad_corner_type=%i",
                 BM_ELEM_TAG,
                 1,
                 SUBD_CORNER_STRAIGHT_CUT);
  }
  else if (split_face->len > 4) {
    /* Maintain a dynamic vert array containing the split_face's
     * vertices, avoids frequent allocations in #collapse_face_corners(). */
    vert_buf.reinitialize(split_face->len);

    /* Get split face's verts */
    BM_iter_as_array(bm, BM_VERTS_OF_FACE, split_face, (void **)vert_buf.data(), split_face->len);

    /* Earlier edge split operations may have turned some quads
     * into higher-degree faces */
    split_face = collapse_face_corners(bm, split_face, 4, vert_buf.data());
  }

  /* `split_face` should now be a quad. */
  BLI_assert(split_face->len == 4);

  /* Account for the highly unlikely case that it's not a quad. */
  if (split_face->len != 4) {
    /* Reuse `vert_buf` for updating normals. */
    vert_buf.reinitialize(split_face->len);
    BM_iter_as_array(bm, BM_FACES_OF_VERT, split_face, (void **)vert_buf.data(), split_face->len);

    vert_array_face_normal_update(vert_buf.data(), split_face->len);
    return;
  }

  /* Get split face's verts */
  // BM_iter_as_array(bm, BM_VERTS_OF_FACE, split_face, (void **)verts, 4);
  BM_face_as_array_vert_quad(split_face, verts);
  skin_choose_quad_bridge_order(verts, frame->verts, best_order);

  /* Delete split face and merge */
  BM_face_kill(bm, split_face);
  BMO_op_init(bm, &op, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE), "weld_verts");
  slot_targetmap = BMO_slot_get(op.slots_in, "targetmap");
  for (i = 0; i < 4; i++) {
    BMO_slot_map_elem_insert(&op, slot_targetmap, verts[i], frame->verts[best_order[i]]);
  }
  BMO_op_exec(bm, &op);
  BMO_op_finish(bm, &op);

  vert_array_face_normal_update(frame->verts, 4);
}

/* If the frame has some vertices that are inside the hull (detached)
 * and some attached, duplicate the attached vertices and take the
 * whole frame off the hull. */
static void skin_hole_detach_partially_attached_frame(BMesh *bm, Frame *frame)
{
  int i, attached[4], totattached = 0;

  /* Get/count attached frame corners */
  for (i = 0; i < 4; i++) {
    if (!frame->inside_hull[i]) {
      attached[totattached++] = i;
    }
  }

  /* Detach everything */
  for (i = 0; i < totattached; i++) {
    BMVert **av = &frame->verts[attached[i]];
    (*av) = BM_vert_create(bm, (*av)->co, *av, BM_CREATE_NOP);
  }
}

static void quad_from_tris(BMEdge *e, BMFace *adj[2], BMVert *ndx[4])
{
  BMVert *tri[2][3];
  BMVert *opp = nullptr;
  int i, j;

  BLI_assert(adj[0]->len == 3 && adj[1]->len == 3);

#if 0
  BM_iter_as_array(bm, BM_VERTS_OF_FACE, adj[0], (void **)tri[0], 3);
  BM_iter_as_array(bm, BM_VERTS_OF_FACE, adj[1], (void **)tri[1], 3);
#else
  BM_face_as_array_vert_tri(adj[0], tri[0]);
  BM_face_as_array_vert_tri(adj[1], tri[1]);
#endif

  /* Find what the second tri has that the first doesn't */
  for (i = 0; i < 3; i++) {
    if (!ELEM(tri[1][i], tri[0][0], tri[0][1], tri[0][2])) {
      opp = tri[1][i];
      break;
    }
  }
  BLI_assert(opp);

  for (i = 0, j = 0; i < 3; i++, j++) {
    ndx[j] = tri[0][i];
    /* When the triangle edge cuts across our quad-to-be,
     * throw in the second triangle's vertex */
    if (ELEM(tri[0][i], e->v1, e->v2) &&
        (tri[0][(i + 1) % 3] == e->v1 || tri[0][(i + 1) % 3] == e->v2))
    {
      j++;
      ndx[j] = opp;
    }
  }
}

static void add_quad_from_tris(SkinOutput *so, BMEdge *e, BMFace *adj[2])
{
  BMVert *quad[4];

  quad_from_tris(e, adj, quad);

  add_poly(so, quad[0], quad[1], quad[2], quad[3]);
}

static void hull_merge_triangles(SkinOutput *so, const SkinModifierData *smd)
{
  BMIter iter;
  BMEdge *e;
  HeapSimple *heap;
  float score;

  heap = BLI_heapsimple_new();

  BM_mesh_elem_hflag_disable_all(so->bm, BM_FACE, BM_ELEM_TAG, false);

  /* Build heap */
  BM_ITER_MESH (e, &iter, so->bm, BM_EDGES_OF_MESH) {
    BMFace *adj[2];

    /* Only care if the edge is used by exactly two triangles */
    if (BM_edge_face_pair(e, &adj[0], &adj[1])) {
      if (adj[0]->len == 3 && adj[1]->len == 3) {
        BMVert *quad[4];

        BLI_assert(BM_face_is_normal_valid(adj[0]));
        BLI_assert(BM_face_is_normal_valid(adj[1]));

        /* Construct quad using the two triangles adjacent to
         * the edge */
        quad_from_tris(e, adj, quad);

        /* Calculate a score for the quad, higher score for
         * triangles being closer to coplanar */
        score = ((BM_face_calc_area(adj[0]) + BM_face_calc_area(adj[1])) *
                 dot_v3v3(adj[0]->no, adj[1]->no));

        /* Check if quad crosses the axis of symmetry */
        if (quad_crosses_symmetry_plane(quad, smd)) {
          /* Increase score if the triangles form a
           * symmetric quad, otherwise don't use it */
          if (is_quad_symmetric(quad, smd)) {
            score *= 10;
          }
          else {
            continue;
          }
        }

        /* Don't use the quad if it's concave */
        if (!is_quad_convex_v3(quad[0]->co, quad[1]->co, quad[2]->co, quad[3]->co)) {
          continue;
        }

        BLI_heapsimple_insert(heap, -score, e);
      }
    }
  }

  while (!BLI_heapsimple_is_empty(heap)) {
    BMFace *adj[2];

    e = static_cast<BMEdge *>(BLI_heapsimple_pop_min(heap));

    if (BM_edge_face_pair(e, &adj[0], &adj[1])) {
      /* If both triangles still free, and if they don't already
       * share a border with another face, output as a quad */
      if (!BM_elem_flag_test(adj[0], BM_ELEM_TAG) && !BM_elem_flag_test(adj[1], BM_ELEM_TAG) &&
          !BM_face_share_face_check(adj[0], adj[1]))
      {
        add_quad_from_tris(so, e, adj);
        BM_elem_flag_enable(adj[0], BM_ELEM_TAG);
        BM_elem_flag_enable(adj[1], BM_ELEM_TAG);
        BM_elem_flag_enable(e, BM_ELEM_TAG);
      }
    }
  }

  BLI_heapsimple_free(heap, nullptr);

  BM_mesh_delete_hflag_tagged(so->bm, BM_ELEM_TAG, BM_EDGE | BM_FACE);
}

static void skin_merge_close_frame_verts(SkinNode *skin_nodes,
                                         int verts_num,
                                         blender::GroupedSpan<int> emap,
                                         const blender::Span<blender::int2> edges)
{
  Frame **hull_frames;
  int v, tothullframe;

  for (v = 0; v < verts_num; v++) {
    /* Only check branch nodes */
    if (!skin_nodes[v].totframe) {
      hull_frames = collect_hull_frames(v, skin_nodes, emap, edges, &tothullframe);
      merge_frame_corners(hull_frames, tothullframe);
      MEM_freeN(hull_frames);
    }
  }
}

static void skin_update_merged_vertices(SkinNode *skin_nodes, int verts_num)
{
  int v;

  for (v = 0; v < verts_num; v++) {
    SkinNode *sn = &skin_nodes[v];
    int i, j;

    for (i = 0; i < sn->totframe; i++) {
      Frame *f = &sn->frames[i];

      for (j = 0; j < 4; j++) {
        if (f->merge[j].frame) {
          /* Merge chaining not allowed */
          BLI_assert(!f->merge[j].frame->merge[f->merge[j].corner].frame);

          f->verts[j] = f->merge[j].frame->verts[f->merge[j].corner];
        }
      }
    }
  }
}

static void skin_fix_hull_topology(BMesh *bm, SkinNode *skin_nodes, int verts_num)
{
  int v;

  for (v = 0; v < verts_num; v++) {
    SkinNode *sn = &skin_nodes[v];
    int j;

    for (j = 0; j < sn->totframe; j++) {
      Frame *f = &sn->frames[j];

      if (f->detached) {
        BMFace *target_face;

        skin_hole_detach_partially_attached_frame(bm, f);

        target_face = skin_hole_target_face(bm, f);
        if (target_face) {
          skin_fix_hole_no_good_verts(bm, f, target_face);
        }
      }
    }
  }
}

static void skin_output_end_nodes(SkinOutput *so, SkinNode *skin_nodes, int verts_num)
{
  int v;

  for (v = 0; v < verts_num; v++) {
    SkinNode *sn = &skin_nodes[v];
    /* Assuming here just two frames */
    if (sn->flag & SEAM_FRAME) {
      BMVert *v_order[4];
      int i, order[4];

      skin_choose_quad_bridge_order(sn->frames[0].verts, sn->frames[1].verts, order);
      for (i = 0; i < 4; i++) {
        v_order[i] = sn->frames[1].verts[order[i]];
      }
      connect_frames(so, sn->frames[0].verts, v_order);
    }
    else if (sn->totframe == 2) {
      connect_frames(so, sn->frames[0].verts, sn->frames[1].verts);
    }

    if (sn->flag & CAP_START) {
      if (sn->flag & FLIP_NORMAL) {
        add_poly(so,
                 sn->frames[0].verts[0],
                 sn->frames[0].verts[1],
                 sn->frames[0].verts[2],
                 sn->frames[0].verts[3]);
      }
      else {
        add_poly(so,
                 sn->frames[0].verts[3],
                 sn->frames[0].verts[2],
                 sn->frames[0].verts[1],
                 sn->frames[0].verts[0]);
      }
    }
    if (sn->flag & CAP_END) {
      add_poly(so,
               sn->frames[1].verts[0],
               sn->frames[1].verts[1],
               sn->frames[1].verts[2],
               sn->frames[1].verts[3]);
    }
  }
}

static void skin_output_connections(SkinOutput *so,
                                    SkinNode *skin_nodes,
                                    const blender::Span<blender::int2> edges)
{
  for (const int e : edges.index_range()) {
    SkinNode *a, *b;
    a = &skin_nodes[edges[e][0]];
    b = &skin_nodes[edges[e][1]];

    if (a->totframe && b->totframe) {
      if ((a->flag & SEAM_FRAME) || (b->flag & SEAM_FRAME)) {
        Frame *fr[2] = {&a->frames[0], &b->frames[0]};
        BMVert *v_order[4];
        int i, order[4];

        if ((a->flag & SEAM_FRAME) && (e != a->seam_edges[0])) {
          fr[0]++;
        }
        if ((b->flag & SEAM_FRAME) && (e != b->seam_edges[0])) {
          fr[1]++;
        }

        skin_choose_quad_bridge_order(fr[0]->verts, fr[1]->verts, order);
        for (i = 0; i < 4; i++) {
          v_order[i] = fr[1]->verts[order[i]];
        }
        connect_frames(so, fr[0]->verts, v_order);
      }
      else {
        connect_frames(so, a->frames[0].verts, b->frames[0].verts);
      }
    }
  }
}

static void skin_smooth_hulls(BMesh *bm,
                              SkinNode *skin_nodes,
                              int verts_num,
                              const SkinModifierData *smd)
{
  BMIter iter, eiter;
  BMVert *v;
  int i, j, k, skey;

  if (smd->branch_smoothing == 0) {
    return;
  }

  /* Mark all frame vertices */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);
  for (i = 0; i < verts_num; i++) {
    for (j = 0; j < skin_nodes[i].totframe; j++) {
      Frame *frame = &skin_nodes[i].frames[j];

      for (k = 0; k < 4; k++) {
        BM_elem_flag_enable(frame->verts[k], BM_ELEM_TAG);
      }
    }
  }

  /* Add temporary shape-key layer to store original coordinates. */
  BM_data_layer_add(bm, &bm->vdata, CD_SHAPEKEY);
  skey = CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY) - 1;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    copy_v3_v3(
        static_cast<float *>(CustomData_bmesh_get_n(&bm->vdata, v->head.data, CD_SHAPEKEY, skey)),
        v->co);
  }

  /* Smooth vertices, weight unmarked vertices more strongly (helps
   * to smooth frame vertices, but don't want to alter them too
   * much) */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BMEdge *e;
    float avg[3];
    float weight = smd->branch_smoothing;
    int totv = 1;

    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      weight *= 0.5f;
    }

    copy_v3_v3(avg, v->co);
    BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
      BMVert *other = BM_edge_other_vert(e, v);

      add_v3_v3(avg,
                static_cast<float *>(
                    CustomData_bmesh_get_n(&bm->vdata, other->head.data, CD_SHAPEKEY, skey)));
      totv++;
    }

    if (totv > 1) {
      mul_v3_fl(avg, 1.0f / float(totv));
      interp_v3_v3v3(v->co, v->co, avg, weight);
    }
  }

  /* Done with original coordinates */
  BM_data_layer_free_n(bm, &bm->vdata, CD_SHAPEKEY, skey);

  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BM_face_normal_update(f);
  }
}

/* Returns true if all hulls are successfully built, false otherwise */
static bool skin_output_branch_hulls(SkinOutput *so,
                                     SkinNode *skin_nodes,
                                     int verts_num,
                                     blender::GroupedSpan<int> emap,
                                     const blender::Span<blender::int2> edges)
{
  bool result = true;
  int v;

  for (v = 0; v < verts_num; v++) {
    SkinNode *sn = &skin_nodes[v];

    /* Branch node hulls */
    if (!sn->totframe) {
      Frame **hull_frames;
      int tothullframe;

      hull_frames = collect_hull_frames(v, skin_nodes, emap, edges, &tothullframe);
      if (!build_hull(so, hull_frames, tothullframe)) {
        result = false;
      }

      MEM_freeN(hull_frames);
    }
  }

  return result;
}

enum eSkinErrorFlag {
  SKIN_ERROR_NO_VALID_ROOT = (1 << 0),
  SKIN_ERROR_HULL = (1 << 1),
};
ENUM_OPERATORS(eSkinErrorFlag);

static BMesh *build_skin(SkinNode *skin_nodes,
                         int verts_num,
                         blender::GroupedSpan<int> emap,
                         const blender::Span<blender::int2> edges,
                         const MDeformVert *input_dvert,
                         SkinModifierData *smd,
                         eSkinErrorFlag *r_error)
{
  SkinOutput so;
  int v;

  so.smd = smd;
  BMeshCreateParams create_params{};
  create_params.use_toolflags = true;
  so.bm = BM_mesh_create(&bm_mesh_allocsize_default, &create_params);
  so.mat_nr = 0;

  /* BMESH_TODO: bumping up the stack level (see MOD_array.cc) */
  BM_mesh_elem_toolflags_ensure(so.bm);
  BMO_push(so.bm, nullptr);
  bmesh_edit_begin(so.bm, BMOpTypeFlag(0));

  if (input_dvert) {
    BM_data_layer_add(so.bm, &so.bm->vdata, CD_MDEFORMVERT);
  }

  /* Check for mergeable frame corners around hulls before
   * outputting vertices */
  skin_merge_close_frame_verts(skin_nodes, verts_num, emap, edges);

  /* Write out all frame vertices to the mesh */
  for (v = 0; v < verts_num; v++) {
    if (skin_nodes[v].totframe) {
      output_frames(so.bm, &skin_nodes[v], input_dvert ? &input_dvert[v] : nullptr);
    }
  }

  /* Update vertex pointers for merged frame corners */
  skin_update_merged_vertices(skin_nodes, verts_num);

  if (!skin_output_branch_hulls(&so, skin_nodes, verts_num, emap, edges)) {
    *r_error |= SKIN_ERROR_HULL;
  }

  /* Merge triangles here in the hope of providing better target
   * faces for skin_fix_hull_topology() to connect to */
  hull_merge_triangles(&so, smd);

  /* Using convex hulls may not generate a nice manifold mesh. Two
   * problems can occur: an input frame's edges may be inside the
   * hull, and/or an input frame's vertices may be inside the hull.
   *
   * General fix to produce manifold mesh: for any frame that is
   * partially detached, first detach it fully, then find a suitable
   * existing face to merge with. (Note that we do this after
   * creating all hull faces, but before creating any other
   * faces.
   */
  skin_fix_hull_topology(so.bm, skin_nodes, verts_num);

  skin_smooth_hulls(so.bm, skin_nodes, verts_num, smd);

  skin_output_end_nodes(&so, skin_nodes, verts_num);
  skin_output_connections(&so, skin_nodes, edges);
  hull_merge_triangles(&so, smd);

  bmesh_edit_end(so.bm, BMOpTypeFlag(0));
  BMO_pop(so.bm);

  return so.bm;
}

static void skin_set_orig_indices(Mesh *mesh)
{
  int *orig = static_cast<int *>(
      CustomData_add_layer(&mesh->face_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->faces_num));
  copy_vn_i(orig, mesh->faces_num, ORIGINDEX_NONE);
}

/*
 * 0) Subdivide edges (in caller)
 * 1) Generate good edge matrices (uses root nodes)
 * 2) Generate node frames
 * 3) Output vertices and polygons from frames, connections, and hulls
 */
static Mesh *base_skin(Mesh *origmesh, SkinModifierData *smd, eSkinErrorFlag *r_error)
{
  Mesh *result;
  BMesh *bm;
  EMat *emat;
  SkinNode *skin_nodes;
  bool has_valid_root = false;

  const MVertSkin *nodes = static_cast<const MVertSkin *>(
      CustomData_get_layer(&origmesh->vert_data, CD_MVERT_SKIN));

  const blender::Span<blender::float3> vert_positions = origmesh->vert_positions();
  const blender::Span<blender::int2> edges = origmesh->edges();
  const MDeformVert *dvert = origmesh->deform_verts().data();
  const int verts_num = origmesh->verts_num;

  blender::Array<int> vert_to_edge_offsets;
  blender::Array<int> vert_to_edge_indices;
  const blender::GroupedSpan<int> vert_to_edge = blender::bke::mesh::build_vert_to_edge_map(
      edges, verts_num, vert_to_edge_offsets, vert_to_edge_indices);

  emat = build_edge_mats(nodes, vert_positions, verts_num, edges, vert_to_edge, &has_valid_root);
  skin_nodes = build_frames(vert_positions, verts_num, nodes, vert_to_edge, emat);
  MEM_freeN(emat);
  emat = nullptr;

  bm = build_skin(skin_nodes, verts_num, vert_to_edge, edges, dvert, smd, r_error);

  MEM_freeN(skin_nodes);

  if (!has_valid_root) {
    *r_error |= SKIN_ERROR_NO_VALID_ROOT;
  }

  if (!bm) {
    return nullptr;
  }

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, origmesh);
  BM_mesh_free(bm);

  skin_set_orig_indices(result);

  return result;
}

static Mesh *final_skin(SkinModifierData *smd, Mesh *mesh, eSkinErrorFlag *r_error)
{
  Mesh *result;

  /* Skin node layer is required */
  if (!CustomData_get_layer(&mesh->vert_data, CD_MVERT_SKIN)) {
    return mesh;
  }

  mesh = subdivide_base(mesh);
  result = base_skin(mesh, smd, r_error);

  BKE_id_free(nullptr, mesh);
  return result;
}

/**************************** Skin Modifier ***************************/

static void init_data(ModifierData *md)
{
  SkinModifierData *smd = (SkinModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SkinModifierData), modifier);

  /* Enable in editmode by default. */
  md->mode |= eModifierMode_Editmode;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  eSkinErrorFlag error = eSkinErrorFlag(0);
  Mesh *result = final_skin((SkinModifierData *)md, mesh, &error);

  if (error & SKIN_ERROR_NO_VALID_ROOT) {
    error &= ~SKIN_ERROR_NO_VALID_ROOT;
    BKE_modifier_set_error(
        ctx->object,
        md,
        "No valid root vertex found (you need one per mesh island you want to skin)");
  }
  if (error & SKIN_ERROR_HULL) {
    error &= ~SKIN_ERROR_HULL;
    BKE_modifier_set_error(ctx->object, md, "Hull error");
  }
  BLI_assert(error == 0);

  if (result == nullptr) {
    return mesh;
  }
  return result;
}

static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MVERT_SKIN | CD_MASK_MDEFORMVERT;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA op_ptr;

  layout->use_property_split_set(true);

  layout->prop(ptr, "branch_smoothing", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true, IFACE_("Symmetry"));
  row->prop(ptr, "use_x_symmetry", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_y_symmetry", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_z_symmetry", toggles_flag, std::nullopt, ICON_NONE);

  layout->prop(ptr, "use_smooth_shade", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(false);
  row->op("OBJECT_OT_skin_armature_create", IFACE_("Create Armature"), ICON_NONE);
  row->op("MESH_OT_customdata_skin_add", std::nullopt, ICON_NONE);

  row = &layout->row(false);
  op_ptr = row->op("OBJECT_OT_skin_loose_mark_clear",
                   IFACE_("Mark Loose"),
                   ICON_NONE,
                   blender::wm::OpCallContext::ExecDefault,
                   UI_ITEM_NONE);
  RNA_enum_set(&op_ptr, "action", 0); /* SKIN_LOOSE_MARK */
  op_ptr = row->op("OBJECT_OT_skin_loose_mark_clear",
                   IFACE_("Clear Loose"),
                   ICON_NONE,
                   blender::wm::OpCallContext::ExecDefault,
                   UI_ITEM_NONE);
  RNA_enum_set(&op_ptr, "action", 1); /* SKIN_LOOSE_CLEAR */

  layout->op("OBJECT_OT_skin_root_mark", IFACE_("Mark Root"), ICON_NONE);
  layout->op("OBJECT_OT_skin_radii_equalize", IFACE_("Equalize Radii"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Skin, panel_draw);
}

ModifierTypeInfo modifierType_Skin = {
    /*idname*/ "Skin",
    /*name*/ N_("Skin"),
    /*struct_name*/ "SkinModifierData",
    /*struct_size*/ sizeof(SkinModifierData),
    /*srna*/ &RNA_SkinModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_SKIN,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
