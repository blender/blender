/*
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
 */

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
 * + No mesh evolution. The paper suggests iteratively subsurfing the
 *   skin output and adapting the output to better conform with the
 *   spheres of influence surrounding each vertex.
 *
 * + No mesh fairing. The paper suggests re-aligning output edges to
 *   follow principal mesh curvatures.
 *
 * + No auxiliary balls. These would serve to influence mesh
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

#include "BLI_utildefines.h"

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_heap_simple.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"
#include "BLI_stack.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_types.h" /* For skin mark clear operator UI. */

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "bmesh.h"

typedef struct {
  float mat[3][3];
  /* Vert that edge is pointing away from, no relation to
   * MEdge.v1 */
  int origin;
} EMat;

typedef enum {
  CAP_START = 1,
  CAP_END = 2,
  SEAM_FRAME = 4,
  FLIP_NORMAL = 8,
} SkinNodeFlag;

typedef struct Frame {
  /* Index in the MVert array */
  BMVert *verts[4];
  /* Location of each corner */
  float co[4][3];
  /* Indicates which corners have been merged with another
   * frame's corner (so they share an MVert index) */
  struct {
    /* Merge to target frame/corner (no merge if frame is null) */
    struct Frame *frame;
    int corner;
    /* checked to avoid chaining.
     * (merging when we're already been referenced), see T39775 */
    uint is_target : 1;
  } merge[4];

  /* For hull frames, whether each vertex is detached or not */
  bool inside_hull[4];
  /* Whether any part of the frame (corner or edge) is detached */
  bool detached;
} Frame;

#define MAX_SKIN_NODE_FRAMES 2
typedef struct {
  Frame frames[MAX_SKIN_NODE_FRAMES];
  int totframe;

  SkinNodeFlag flag;

  /* Used for hulling a loop seam */
  int seam_edges[2];
} SkinNode;

typedef struct {
  BMesh *bm;
  SkinModifierData *smd;
  int mat_nr;
} SkinOutput;

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
          return 1;
        }
      }
      else if (len_squared_v3v3(a, quad[3]->co) < threshold_squared) {
        copy_v3_v3(a, quad[2]->co);
        a[axis] = -a[axis];
        if (len_squared_v3v3(a, quad[1]->co) < threshold_squared) {
          return 1;
        }
      }
    }
  }

  return 0;
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
  else {
    return false;
  }
}

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

  if (BMO_error_occurred(bm)) {
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
                             !BM_edge_exists(frame->verts[3], frame->verts[0]))) {
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
  UNUSED_VARS(so, frames, totframe, skin_frame_find_contained_faces);
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

static Frame **collect_hull_frames(
    int v, SkinNode *frames, const MeshElemMap *emap, const MEdge *medge, int *tothullframe)
{
  SkinNode *f;
  Frame **hull_frames;
  int nbr, i;

  (*tothullframe) = emap[v].count;
  hull_frames = MEM_calloc_arrayN(
      (*tothullframe), sizeof(Frame *), "hull_from_frames.hull_frames");
  i = 0;
  for (nbr = 0; nbr < emap[v].count; nbr++) {
    const MEdge *e = &medge[emap[v].indices[nbr]];
    f = &frames[BKE_mesh_edge_other_vert(e, v)];
    /* Can't have adjacent branch nodes yet */
    if (f->totframe) {
      hull_frames[i++] = &f->frames[0];
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

  nf->flag = 0;
  for (i = 0; i < 2; i++) {
    nf->seam_edges[i] = -1;
  }
}

static void create_frame(
    Frame *frame, const float co[3], const float radius[2], float mat[3][3], float offset)
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
                            const MVert *mvert,
                            const MVertSkin *nodes,
                            const MeshElemMap *emap,
                            EMat *emat)
{
  const float *rad = nodes[v].radius;
  float mat[3][3];

  if (emap[v].count == 0) {
    float avg = half_v2(rad);

    /* For solitary nodes, just build a box (two frames) */
    node_frames_init(&skin_nodes[v], 2);
    skin_nodes[v].flag |= (CAP_START | CAP_END);

    /* Hardcoded basis */
    zero_m3(mat);
    mat[0][2] = mat[1][0] = mat[2][1] = 1;

    /* Caps */
    create_frame(&skin_nodes[v].frames[0], mvert[v].co, rad, mat, avg);
    create_frame(&skin_nodes[v].frames[1], mvert[v].co, rad, mat, -avg);
  }
  else {
    /* For nodes with an incoming edge, create a single (capped) frame */
    node_frames_init(&skin_nodes[v], 1);
    skin_nodes[v].flag |= CAP_START;

    /* Use incoming edge for orientation */
    copy_m3_m3(mat, emat[emap[v].indices[0]].mat);
    if (emat[emap[v].indices[0]].origin != v) {
      negate_v3(mat[0]);
    }

    Frame *frame = &skin_nodes[v].frames[0];

    /* End frame */
    create_frame(frame, mvert[v].co, rad, mat, 0);

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
static int connection_node_mat(float mat[3][3], int v, const MeshElemMap *emap, EMat *emat)
{
  float axis[3], angle, ine[3][3], oute[3][3];
  EMat *e1, *e2;

  e1 = &emat[emap[v].indices[0]];
  e2 = &emat[emap[v].indices[1]];

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
                                   const MVert *mvert,
                                   const MVertSkin *nodes,
                                   const MeshElemMap *emap,
                                   EMat *emat)
{
  const float *rad = nodes[v].radius;
  float mat[3][3];
  EMat *e1, *e2;

  if (connection_node_mat(mat, v, emap, emat)) {
    float avg = half_v2(rad);

    /* Get edges */
    e1 = &emat[emap[v].indices[0]];
    e2 = &emat[emap[v].indices[1]];

    /* Handle seam separately to avoid twisting */
    /* Create two frames, will be hulled to neighbors later */
    node_frames_init(&skin_nodes[v], 2);
    skin_nodes[v].flag |= SEAM_FRAME;

    copy_m3_m3(mat, e1->mat);
    if (e1->origin != v) {
      negate_v3(mat[0]);
    }
    create_frame(&skin_nodes[v].frames[0], mvert[v].co, rad, mat, avg);
    skin_nodes[v].seam_edges[0] = emap[v].indices[0];

    copy_m3_m3(mat, e2->mat);
    if (e2->origin != v) {
      negate_v3(mat[0]);
    }
    create_frame(&skin_nodes[v].frames[1], mvert[v].co, rad, mat, avg);
    skin_nodes[v].seam_edges[1] = emap[v].indices[1];

    return;
  }

  /* Build regular frame */
  node_frames_init(&skin_nodes[v], 1);
  create_frame(&skin_nodes[v].frames[0], mvert[v].co, rad, mat, 0);
}

static SkinNode *build_frames(
    const MVert *mvert, int totvert, const MVertSkin *nodes, const MeshElemMap *emap, EMat *emat)
{
  SkinNode *skin_nodes;
  int v;

  skin_nodes = MEM_calloc_arrayN(totvert, sizeof(SkinNode), "build_frames.skin_nodes");

  for (v = 0; v < totvert; v++) {
    if (emap[v].count <= 1) {
      end_node_frames(v, skin_nodes, mvert, nodes, emap, emat);
    }
    else if (emap[v].count == 2) {
      connection_node_frames(v, skin_nodes, mvert, nodes, emap, emat);
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

typedef struct {
  float mat[3][3];
  int parent_v;
  int e;
} EdgeStackElem;

static void build_emats_stack(BLI_Stack *stack,
                              BLI_bitmap *visited_e,
                              EMat *emat,
                              const MeshElemMap *emap,
                              const MEdge *medge,
                              const MVertSkin *vs,
                              const MVert *mvert)
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

  parent_is_branch = ((emap[parent_v].count > 2) || (vs[parent_v].flag & MVERT_SKIN_ROOT));

  v = BKE_mesh_edge_other_vert(&medge[e], parent_v);
  emat[e].origin = parent_v;

  /* If parent is a branch node, start a new edge chain */
  if (parent_is_branch) {
    calc_edge_mat(emat[e].mat, mvert[parent_v].co, mvert[v].co);
  }
  else {
    /* Build edge matrix guided by parent matrix */
    sub_v3_v3v3(emat[e].mat[0], mvert[v].co, mvert[parent_v].co);
    normalize_v3(emat[e].mat[0]);
    angle = angle_normalized_v3v3(stack_elem.mat[0], emat[e].mat[0]);
    cross_v3_v3v3(axis, stack_elem.mat[0], emat[e].mat[0]);
    normalize_v3(axis);
    rotate_normalized_v3_v3v3fl(emat[e].mat[1], stack_elem.mat[1], axis, angle);
    rotate_normalized_v3_v3v3fl(emat[e].mat[2], stack_elem.mat[2], axis, angle);
  }

  /* Add neighbors to stack */
  for (i = 0; i < emap[v].count; i++) {
    /* Add neighbors to stack */
    copy_m3_m3(stack_elem.mat, emat[e].mat);
    stack_elem.e = emap[v].indices[i];
    stack_elem.parent_v = v;
    BLI_stack_push(stack, &stack_elem);
  }
}

static EMat *build_edge_mats(const MVertSkin *vs,
                             const MVert *mvert,
                             int totvert,
                             const MEdge *medge,
                             const MeshElemMap *emap,
                             int totedge,
                             bool *has_valid_root)
{
  BLI_Stack *stack;
  EMat *emat;
  EdgeStackElem stack_elem;
  BLI_bitmap *visited_e;
  int i, v;

  stack = BLI_stack_new(sizeof(stack_elem), "build_edge_mats.stack");

  visited_e = BLI_BITMAP_NEW(totedge, "build_edge_mats.visited_e");
  emat = MEM_calloc_arrayN(totedge, sizeof(EMat), "build_edge_mats.emat");

  /* Edge matrices are built from the root nodes, add all roots with
   * children to the stack */
  for (v = 0; v < totvert; v++) {
    if (vs[v].flag & MVERT_SKIN_ROOT) {
      if (emap[v].count >= 1) {
        const MEdge *e = &medge[emap[v].indices[0]];
        calc_edge_mat(stack_elem.mat, mvert[v].co, mvert[BKE_mesh_edge_other_vert(e, v)].co);
        stack_elem.parent_v = v;

        /* Add adjacent edges to stack */
        for (i = 0; i < emap[v].count; i++) {
          stack_elem.e = emap[v].indices[i];
          BLI_stack_push(stack, &stack_elem);
        }

        *has_valid_root = true;
      }
    }
  }

  while (!BLI_stack_is_empty(stack)) {
    build_emats_stack(stack, visited_e, emat, emap, medge, vs, mvert);
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
static int calc_edge_subdivisions(const MVert *mvert,
                                  const MVertSkin *nodes,
                                  const MEdge *e,
                                  const int *degree)
{
  /* prevent memory errors [#38003] */
#define NUM_SUBDIVISIONS_MAX 128

  const MVertSkin *evs[2] = {&nodes[e->v1], &nodes[e->v2]};
  float avg_radius;
  const bool v1_branch = degree[e->v1] > 2;
  const bool v2_branch = degree[e->v2] > 2;
  int num_subdivisions;

  /* If either end is a branch node marked 'loose', don't subdivide
   * the edge (or subdivide just twice if both are branches) */
  if ((v1_branch && (evs[0]->flag & MVERT_SKIN_LOOSE)) ||
      (v2_branch && (evs[1]->flag & MVERT_SKIN_LOOSE))) {
    if (v1_branch && v2_branch) {
      return 2;
    }
    else {
      return 0;
    }
  }

  avg_radius = half_v2(evs[0]->radius) + half_v2(evs[1]->radius);

  if (avg_radius != 0.0f) {
    /* possible (but unlikely) that we overflow INT_MAX */
    float num_subdivisions_fl;
    const float edge_len = len_v3v3(mvert[e->v1].co, mvert[e->v2].co);
    num_subdivisions_fl = (edge_len / avg_radius);
    if (num_subdivisions_fl < NUM_SUBDIVISIONS_MAX) {
      num_subdivisions = (int)num_subdivisions_fl;
    }
    else {
      num_subdivisions = NUM_SUBDIVISIONS_MAX;
    }
  }
  else {
    num_subdivisions = 0;
  }

  /* If both ends are branch nodes, two intermediate nodes are
   * required */
  if (num_subdivisions < 2 && v1_branch && v2_branch) {
    num_subdivisions = 2;
  }

  return num_subdivisions;

#undef NUM_SUBDIVISIONS_MAX
}

/* Take a Mesh and subdivide its edges to keep skin nodes
 * reasonably close. */
static Mesh *subdivide_base(Mesh *orig)
{
  Mesh *result;
  MVertSkin *orignode, *outnode;
  MVert *origvert, *outvert;
  MEdge *origedge, *outedge, *e;
  MDeformVert *origdvert, *outdvert;
  int totorigvert, totorigedge;
  int totsubd, *degree, *edge_subd;
  int i, j, k, u, v;
  float radrat;

  orignode = CustomData_get_layer(&orig->vdata, CD_MVERT_SKIN);
  origvert = orig->mvert;
  origedge = orig->medge;
  origdvert = orig->dvert;
  totorigvert = orig->totvert;
  totorigedge = orig->totedge;

  /* Get degree of all vertices */
  degree = MEM_calloc_arrayN(totorigvert, sizeof(int), "degree");
  for (i = 0; i < totorigedge; i++) {
    degree[origedge[i].v1]++;
    degree[origedge[i].v2]++;
  }

  /* Per edge, store how many subdivisions are needed */
  edge_subd = MEM_calloc_arrayN((uint)totorigedge, sizeof(int), "edge_subd");
  for (i = 0, totsubd = 0; i < totorigedge; i++) {
    edge_subd[i] += calc_edge_subdivisions(origvert, orignode, &origedge[i], degree);
    BLI_assert(edge_subd[i] >= 0);
    totsubd += edge_subd[i];
  }

  MEM_freeN(degree);

  /* Allocate output mesh */
  result = BKE_mesh_new_nomain_from_template(
      orig, totorigvert + totsubd, totorigedge + totsubd, 0, 0, 0);

  outvert = result->mvert;
  outedge = result->medge;
  outnode = CustomData_get_layer(&result->vdata, CD_MVERT_SKIN);
  outdvert = result->dvert;

  /* Copy original vertex data */
  CustomData_copy_data(&orig->vdata, &result->vdata, 0, 0, totorigvert);

  /* Subdivide edges */
  for (i = 0, v = totorigvert; i < totorigedge; i++) {
    struct {
      /* Vertex group number */
      int def_nr;
      float w1, w2;
    } *vgroups = NULL, *vg;
    int totvgroup = 0;

    e = &origedge[i];

    if (origdvert) {
      const MDeformVert *dv1 = &origdvert[e->v1];
      const MDeformVert *dv2 = &origdvert[e->v2];
      vgroups = MEM_calloc_arrayN(dv1->totweight, sizeof(*vgroups), "vgroup");

      /* Only want vertex groups used by both vertices */
      for (j = 0; j < dv1->totweight; j++) {
        vg = NULL;
        for (k = 0; k < dv2->totweight; k++) {
          if (dv1->dw[j].def_nr == dv2->dw[k].def_nr) {
            vg = &vgroups[totvgroup];
            totvgroup++;
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

    u = e->v1;
    radrat = (half_v2(outnode[e->v2].radius) / half_v2(outnode[e->v1].radius));
    if (isfinite(radrat)) {
      radrat = (radrat + 1) / 2;
    }
    else {
      /* Happens when skin is scaled to zero. */
      radrat = 1.0f;
    }

    /* Add vertices and edge segments */
    for (j = 0; j < edge_subd[i]; j++, v++, outedge++) {
      float r = (j + 1) / (float)(edge_subd[i] + 1);
      float t = powf(r, radrat);

      /* Interpolate vertex coord */
      interp_v3_v3v3(outvert[v].co, outvert[e->v1].co, outvert[e->v2].co, t);

      /* Interpolate skin radii */
      interp_v3_v3v3(outnode[v].radius, orignode[e->v1].radius, orignode[e->v2].radius, t);

      /* Interpolate vertex group weights */
      for (k = 0; k < totvgroup; k++) {
        float weight;

        vg = &vgroups[k];
        weight = interpf(vg->w2, vg->w1, t);

        if (weight > 0) {
          BKE_defvert_add_index_notest(&outdvert[v], vg->def_nr, weight);
        }
      }

      outedge->v1 = u;
      outedge->v2 = v;
      u = v;
    }

    if (vgroups) {
      MEM_freeN(vgroups);
    }

    /* Link up to final vertex */
    outedge->v1 = u;
    outedge->v2 = e->v2;
    outedge++;
  }

  MEM_freeN(edge_subd);

  return result;
}

/******************************* Output *******************************/

/* Can be either quad or triangle */
static void add_poly(SkinOutput *so, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4)
{
  BMVert *verts[4] = {v1, v2, v3, v4};
  BMFace *f;

  BLI_assert(v1 != v2 && v1 != v3 && v1 != v4);
  BLI_assert(v2 != v3 && v2 != v4);
  BLI_assert(v3 != v4);
  BLI_assert(v1 && v2 && v3);

  f = BM_face_create_verts(so->bm, verts, v4 ? 4 : 3, NULL, BM_CREATE_NO_DOUBLE, true);
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
        BMVert *v = f->verts[j] = BM_vert_create(bm, f->co[j], NULL, BM_CREATE_NOP);

        if (input_dvert) {
          MDeformVert *dv;
          dv = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_MDEFORMVERT);

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

/* Does crappy fan triangulation of poly, may not be so accurate for
 * concave faces */
static int isect_ray_poly(const float ray_start[3],
                          const float ray_dir[3],
                          BMFace *f,
                          float *r_lambda)
{
  BMVert *v, *v_first = NULL, *v_prev = NULL;
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

      curhit = isect_ray_tri_v3(ray_start, ray_dir, v_first->co, v_prev->co, v->co, &dist, NULL);
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

    /* Note: could probably calculate merges in one go to be
     * faster */

    v_safe = shortest_edge->v1;
    v_merge = shortest_edge->v2;
    mid_v3_v3v3(v_safe->co, v_safe->co, v_merge->co);
    BMO_slot_map_elem_insert(&op, slot_targetmap, v_merge, v_safe);
    BMO_op_exec(bm, &op);
    BMO_op_finish(bm, &op);

    /* Find the new face */
    f = NULL;
    BM_ITER_ELEM (vf, &iter, v_safe, BM_FACES_OF_VERT) {
      bool wrong_face = false;

      for (i = 0; i < orig_len; i++) {
        if (orig_verts[i] == v_merge) {
          orig_verts[i] = NULL;
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
  isect_target_face = center_target_face = NULL;
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

/* Use edge-length heuristic to choose from eight possible polygon bridges */
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
      memcpy(best_order, orders[i], sizeof(int) * 4);
    }
  }
}

static void skin_fix_hole_no_good_verts(BMesh *bm, Frame *frame, BMFace *split_face)
{
  BMFace *f;
  BMVert *verts[4];
  BMVert **vert_buf = NULL;
  BLI_array_declare(vert_buf);
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
  split_face = NULL;
  BMO_ITER (f, &oiter, op.slots_out, "faces.out", BM_FACE) {
    BLI_assert(!split_face);
    split_face = f;
  }

  BMO_op_finish(bm, &op);

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
     * vertices, avoids frequent allocs in collapse_face_corners() */
    if (BLI_array_len(vert_buf) < split_face->len) {
      BLI_array_grow_items(vert_buf, (split_face->len - BLI_array_len(vert_buf)));
    }

    /* Get split face's verts */
    BM_iter_as_array(bm, BM_VERTS_OF_FACE, split_face, (void **)vert_buf, split_face->len);

    /* Earlier edge split operations may have turned some quads
     * into higher-degree faces */
    split_face = collapse_face_corners(bm, split_face, 4, vert_buf);
  }

  /* Done with dynamic array, split_face must now be a quad */
  BLI_array_free(vert_buf);
  BLI_assert(split_face->len == 4);
  if (split_face->len != 4) {
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
  BMVert *opp = NULL;
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
    if (tri[1][i] != tri[0][0] && tri[1][i] != tri[0][1] && tri[1][i] != tri[0][2]) {
      opp = tri[1][i];
      break;
    }
  }
  BLI_assert(opp);

  for (i = 0, j = 0; i < 3; i++, j++) {
    ndx[j] = tri[0][i];
    /* When the triangle edge cuts across our quad-to-be,
     * throw in the second triangle's vertex */
    if ((tri[0][i] == e->v1 || tri[0][i] == e->v2) &&
        (tri[0][(i + 1) % 3] == e->v1 || tri[0][(i + 1) % 3] == e->v2)) {
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

    e = BLI_heapsimple_pop_min(heap);

    if (BM_edge_face_pair(e, &adj[0], &adj[1])) {
      /* If both triangles still free, and if they don't already
       * share a border with another face, output as a quad */
      if (!BM_elem_flag_test(adj[0], BM_ELEM_TAG) && !BM_elem_flag_test(adj[1], BM_ELEM_TAG) &&
          !BM_face_share_face_check(adj[0], adj[1])) {
        add_quad_from_tris(so, e, adj);
        BM_elem_flag_enable(adj[0], BM_ELEM_TAG);
        BM_elem_flag_enable(adj[1], BM_ELEM_TAG);
        BM_elem_flag_enable(e, BM_ELEM_TAG);
      }
    }
  }

  BLI_heapsimple_free(heap, NULL);

  BM_mesh_delete_hflag_tagged(so->bm, BM_ELEM_TAG, BM_EDGE | BM_FACE);
}

static void skin_merge_close_frame_verts(SkinNode *skin_nodes,
                                         int totvert,
                                         const MeshElemMap *emap,
                                         const MEdge *medge)
{
  Frame **hull_frames;
  int v, tothullframe;

  for (v = 0; v < totvert; v++) {
    /* Only check branch nodes */
    if (!skin_nodes[v].totframe) {
      hull_frames = collect_hull_frames(v, skin_nodes, emap, medge, &tothullframe);
      merge_frame_corners(hull_frames, tothullframe);
      MEM_freeN(hull_frames);
    }
  }
}

static void skin_update_merged_vertices(SkinNode *skin_nodes, int totvert)
{
  int v;

  for (v = 0; v < totvert; v++) {
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

static void skin_fix_hull_topology(BMesh *bm, SkinNode *skin_nodes, int totvert)
{
  int v;

  for (v = 0; v < totvert; v++) {
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

static void skin_output_end_nodes(SkinOutput *so, SkinNode *skin_nodes, int totvert)
{
  int v;

  for (v = 0; v < totvert; v++) {
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
                                    const MEdge *medge,
                                    int totedge)
{
  int e;

  for (e = 0; e < totedge; e++) {
    SkinNode *a, *b;
    a = &skin_nodes[medge[e].v1];
    b = &skin_nodes[medge[e].v2];

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
                              int totvert,
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
  for (i = 0; i < totvert; i++) {
    for (j = 0; j < skin_nodes[i].totframe; j++) {
      Frame *frame = &skin_nodes[i].frames[j];

      for (k = 0; k < 4; k++) {
        BM_elem_flag_enable(frame->verts[k], BM_ELEM_TAG);
      }
    }
  }

  /* Add temporary shapekey layer to store original coordinates */
  BM_data_layer_add(bm, &bm->vdata, CD_SHAPEKEY);
  skey = CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY) - 1;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    copy_v3_v3(CustomData_bmesh_get_n(&bm->vdata, v->head.data, CD_SHAPEKEY, skey), v->co);
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

      add_v3_v3(avg, CustomData_bmesh_get_n(&bm->vdata, other->head.data, CD_SHAPEKEY, skey));
      totv++;
    }

    if (totv > 1) {
      mul_v3_fl(avg, 1.0f / (float)totv);
      interp_v3_v3v3(v->co, v->co, avg, weight);
    }
  }

  /* Done with original coordinates */
  BM_data_layer_free_n(bm, &bm->vdata, CD_SHAPEKEY, skey);
}

/* Returns true if all hulls are successfully built, false otherwise */
static bool skin_output_branch_hulls(
    SkinOutput *so, SkinNode *skin_nodes, int totvert, const MeshElemMap *emap, const MEdge *medge)
{
  bool result = true;
  int v;

  for (v = 0; v < totvert; v++) {
    SkinNode *sn = &skin_nodes[v];

    /* Branch node hulls */
    if (!sn->totframe) {
      Frame **hull_frames;
      int tothullframe;

      hull_frames = collect_hull_frames(v, skin_nodes, emap, medge, &tothullframe);
      if (!build_hull(so, hull_frames, tothullframe)) {
        result = false;
      }

      MEM_freeN(hull_frames);
    }
  }

  return result;
}

static BMesh *build_skin(SkinNode *skin_nodes,
                         int totvert,
                         const MeshElemMap *emap,
                         const MEdge *medge,
                         int totedge,
                         const MDeformVert *input_dvert,
                         SkinModifierData *smd)
{
  SkinOutput so;
  int v;

  so.smd = smd;
  so.bm = BM_mesh_create(&bm_mesh_allocsize_default,
                         &((struct BMeshCreateParams){
                             .use_toolflags = true,
                         }));
  so.mat_nr = 0;

  /* BMESH_TODO: bumping up the stack level (see MOD_array.c) */
  BM_mesh_elem_toolflags_ensure(so.bm);
  BMO_push(so.bm, NULL);
  bmesh_edit_begin(so.bm, 0);

  if (input_dvert) {
    BM_data_layer_add(so.bm, &so.bm->vdata, CD_MDEFORMVERT);
  }

  /* Check for mergeable frame corners around hulls before
   * outputting vertices */
  skin_merge_close_frame_verts(skin_nodes, totvert, emap, medge);

  /* Write out all frame vertices to the mesh */
  for (v = 0; v < totvert; v++) {
    if (skin_nodes[v].totframe) {
      output_frames(so.bm, &skin_nodes[v], input_dvert ? &input_dvert[v] : NULL);
    }
  }

  /* Update vertex pointers for merged frame corners */
  skin_update_merged_vertices(skin_nodes, totvert);

  if (!skin_output_branch_hulls(&so, skin_nodes, totvert, emap, medge)) {
    BKE_modifier_set_error(&smd->modifier, "Hull error");
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
  skin_fix_hull_topology(so.bm, skin_nodes, totvert);

  skin_smooth_hulls(so.bm, skin_nodes, totvert, smd);

  skin_output_end_nodes(&so, skin_nodes, totvert);
  skin_output_connections(&so, skin_nodes, medge, totedge);
  hull_merge_triangles(&so, smd);

  bmesh_edit_end(so.bm, 0);
  BMO_pop(so.bm);

  return so.bm;
}

static void skin_set_orig_indices(Mesh *mesh)
{
  int *orig, totpoly;

  totpoly = mesh->totpoly;
  orig = CustomData_add_layer(&mesh->pdata, CD_ORIGINDEX, CD_CALLOC, NULL, totpoly);
  copy_vn_i(orig, totpoly, ORIGINDEX_NONE);
}

/*
 * 0) Subdivide edges (in caller)
 * 1) Generate good edge matrices (uses root nodes)
 * 2) Generate node frames
 * 3) Output vertices and polygons from frames, connections, and hulls
 */
static Mesh *base_skin(Mesh *origmesh, SkinModifierData *smd)
{
  Mesh *result;
  MVertSkin *nodes;
  BMesh *bm;
  EMat *emat;
  SkinNode *skin_nodes;
  MeshElemMap *emap;
  int *emapmem;
  MVert *mvert;
  MEdge *medge;
  MDeformVert *dvert;
  int totvert, totedge;
  bool has_valid_root = false;

  nodes = CustomData_get_layer(&origmesh->vdata, CD_MVERT_SKIN);

  mvert = origmesh->mvert;
  dvert = origmesh->dvert;
  medge = origmesh->medge;
  totvert = origmesh->totvert;
  totedge = origmesh->totedge;

  BKE_mesh_vert_edge_map_create(&emap, &emapmem, medge, totvert, totedge);

  emat = build_edge_mats(nodes, mvert, totvert, medge, emap, totedge, &has_valid_root);
  skin_nodes = build_frames(mvert, totvert, nodes, emap, emat);
  MEM_freeN(emat);
  emat = NULL;

  bm = build_skin(skin_nodes, totvert, emap, medge, totedge, dvert, smd);

  MEM_freeN(skin_nodes);
  MEM_freeN(emap);
  MEM_freeN(emapmem);

  if (!has_valid_root) {
    BKE_modifier_set_error(
        &smd->modifier,
        "No valid root vertex found (you need one per mesh island you want to skin)");
  }

  if (!bm) {
    return NULL;
  }

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, origmesh);
  BM_mesh_free(bm);

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  skin_set_orig_indices(result);

  return result;
}

static Mesh *final_skin(SkinModifierData *smd, Mesh *mesh)
{
  Mesh *result;

  /* Skin node layer is required */
  if (!CustomData_get_layer(&mesh->vdata, CD_MVERT_SKIN)) {
    return mesh;
  }

  mesh = subdivide_base(mesh);
  result = base_skin(mesh, smd);

  BKE_id_free(NULL, mesh);
  return result;
}

/**************************** Skin Modifier ***************************/

static void initData(ModifierData *md)
{
  SkinModifierData *smd = (SkinModifierData *)md;

  /* Enable in editmode by default */
  md->mode |= eModifierMode_Editmode;

  smd->branch_smoothing = 0;
  smd->flag = 0;
  smd->symmetry_axes = MOD_SKIN_SYMM_X;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *UNUSED(ctx), Mesh *mesh)
{
  Mesh *result;

  if (!(result = final_skin((SkinModifierData *)md, mesh))) {
    return mesh;
  }
  return result;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MVERT_SKIN | CD_MASK_MDEFORMVERT;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  PointerRNA op_ptr;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "branch_smoothing", 0, NULL, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Symmetry"));
  uiItemR(row, &ptr, "use_x_symmetry", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "use_y_symmetry", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "use_z_symmetry", toggles_flag, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "use_smooth_shade", 0, NULL, ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemO(row, IFACE_("Create Armature"), ICON_NONE, "OBJECT_OT_skin_armature_create");
  uiItemO(row, NULL, ICON_NONE, "MESH_OT_customdata_skin_add");

  row = uiLayoutRow(layout, true);
  uiItemFullO(row,
              "OBJECT_OT_skin_loose_mark_clear",
              IFACE_("Mark Loose"),
              ICON_NONE,
              NULL,
              WM_OP_EXEC_DEFAULT,
              0,
              &op_ptr);
  RNA_enum_set(&op_ptr, "action", 0); /* SKIN_LOOSE_MARK */
  uiItemFullO(row,
              "OBJECT_OT_skin_loose_mark_clear",
              IFACE_("Clear Loose"),
              ICON_NONE,
              NULL,
              WM_OP_EXEC_DEFAULT,
              0,
              &op_ptr);
  RNA_enum_set(&op_ptr, "action", 1); /* SKIN_LOOSE_CLEAR */

  uiItemO(layout, IFACE_("Mark Root"), ICON_NONE, "OBJECT_OT_skin_root_mark");
  uiItemO(layout, IFACE_("Equalize Radii"), ICON_NONE, "OBJECT_OT_skin_radii_equalize");

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Skin, panel_draw);
}

ModifierTypeInfo modifierType_Skin = {
    /* name */ "Skin",
    /* structName */ "SkinModifierData",
    /* structSize */ sizeof(SkinModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
