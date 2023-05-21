/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation */

/** \file
 * \ingroup edmesh
 *
 * Interactive editmesh knife tool.
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_smallhash.h"
#include "BLI_stack.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_layer.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_unit.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_mesh.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_object_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h" /* Own include. */

/* Detect isolated holes and fill them. */
#define USE_NET_ISLAND_CONNECT

#define KMAXDIST (10 * UI_SCALE_FAC) /* Max mouse distance from edge before not detecting it. */

/* WARNING: Knife float precision is fragile:
 * Be careful before making changes here see: (#43229, #42864, #42459, #41164).
 */
#define KNIFE_FLT_EPS 0.00001f
#define KNIFE_FLT_EPS_SQUARED (KNIFE_FLT_EPS * KNIFE_FLT_EPS)
#define KNIFE_FLT_EPSBIG 0.0005f

#define KNIFE_FLT_EPS_PX_VERT 0.5f
#define KNIFE_FLT_EPS_PX_EDGE 0.05f
#define KNIFE_FLT_EPS_PX_FACE 0.05f

#define KNIFE_DEFAULT_ANGLE_SNAPPING_INCREMENT 30.0f
#define KNIFE_MIN_ANGLE_SNAPPING_INCREMENT 0.0f
#define KNIFE_MAX_ANGLE_SNAPPING_INCREMENT 180.0f

typedef struct KnifeColors {
  uchar line[3];
  uchar edge[3];
  uchar edge_extra[3];
  uchar curpoint[3];
  uchar curpoint_a[4];
  uchar point[3];
  uchar point_a[4];
  uchar xaxis[3];
  uchar yaxis[3];
  uchar zaxis[3];
  uchar axis_extra[3];
} KnifeColors;

/* Knife-tool Operator. */
typedef struct KnifeVert {
  Object *ob;
  uint ob_index;
  BMVert *v; /* Non-NULL if this is an original vert. */
  ListBase edges;
  ListBase faces;

  float co[3], cageco[3];
  bool is_face, in_space;
  bool is_cut; /* Along a cut created by user input (will draw too). */
  bool is_invalid;
  bool is_splitting; /* Created when an edge was split. */
} KnifeVert;

typedef struct Ref {
  struct Ref *next, *prev;
  void *ref;
} Ref;

typedef struct KnifeEdge {
  KnifeVert *v1, *v2;
  BMFace *basef; /* Face to restrict face fill to. */
  ListBase faces;

  BMEdge *e;   /* Non-NULL if this is an original edge. */
  bool is_cut; /* Along a cut created by user input (will draw too). */
  bool is_invalid;
  int splits; /* Number of times this edge has been split. */
} KnifeEdge;

typedef struct KnifeLineHit {
  float hit[3], cagehit[3];
  float schit[2]; /* Screen coordinates for cagehit. */
  float l;        /* Lambda along cut line. */
  float perc;     /* Lambda along hit line. */
  float m;        /* Depth front-to-back. */

  /* Exactly one of kfe, v, or f should be non-NULL,
   * saying whether cut line crosses and edge,
   * is snapped to a vert, or is in the middle of some face. */
  KnifeEdge *kfe;
  KnifeVert *v;
  BMFace *f;
  Object *ob;
  uint ob_index;
} KnifeLineHit;

typedef struct KnifePosData {
  float co[3];
  float cage[3];

  /* At most one of vert, edge, or bmface should be non-NULL,
   * saying whether the point is snapped to a vertex, edge, or in a face.
   * If none are set, this point is in space and is_space should be true. */
  KnifeVert *vert;
  KnifeEdge *edge;
  BMFace *bmface;
  Object *ob; /* Object of the vert, edge or bmface. */
  uint ob_index;

  /* When true, the cursor isn't over a face. */
  bool is_space;

  float mval[2]; /* Mouse screen position (may be non-integral if snapped to something). */
} KnifePosData;

typedef struct KnifeMeasureData {
  float cage[3];
  float mval[2];
  bool is_stored;
} KnifeMeasureData;

typedef struct KnifeUndoFrame {
  int cuts;         /* Line hits cause multiple edges/cuts to be created at once. */
  int splits;       /* Number of edges split. */
  KnifePosData pos; /* Store previous KnifePosData. */
  KnifeMeasureData mdata;

} KnifeUndoFrame;

typedef struct KnifeBVH {
  BVHTree *tree;          /* Knife Custom BVH Tree. */
  BMLoop *(*looptris)[3]; /* Used by #knife_bvh_raycast_cb to store the intersecting looptri. */
  float uv[2];            /* Used by #knife_bvh_raycast_cb to store the intersecting uv. */
  uint ob_index;

  /* Use #bm_ray_cast_cb_elem_not_in_face_check. */
  bool (*filter_cb)(BMFace *f, void *userdata);
  void *filter_data;

} KnifeBVH;

/** Additional per-object data. */
typedef struct KnifeObjectInfo {
  const float (*cagecos)[3];

  /**
   * Optionally allocate triangle indices, these are needed for non-interactive knife
   * projection as multiple cuts are made without the BVH being updated.
   * Using these indices the it's possible to access `cagecos` even if the face has been cut
   * and the loops in `em->looptris` no longer refer to the original triangles, see: #97153.
   */
  const int (*tri_indices)[3];

  /** Only assigned for convenient access. */
  BMEditMesh *em;
} KnifeObjectInfo;

/* struct for properties used while drawing */
typedef struct KnifeTool_OpData {
  ARegion *region;   /* Region that knifetool was activated in. */
  void *draw_handle; /* For drawing preview loop. */
  ViewContext vc;    /* NOTE: _don't_ use 'mval', instead use the one we define below. */
  float mval[2];     /* Mouse value with snapping applied. */

  Scene *scene;

  /* Used for swapping current object when in multi-object edit mode. */
  Object **objects;
  uint objects_len;
  bool objects_free;

  /** Array `objects_len` length of additional per-object data. */
  KnifeObjectInfo *objects_info;

  MemArena *arena;

  /* Reused for edge-net filling. */
  struct {
    /* Cleared each use. */
    GSet *edge_visit;
#ifdef USE_NET_ISLAND_CONNECT
    MemArena *arena;
#endif
  } edgenet;

  GHash *origvertmap;
  GHash *origedgemap;
  GHash *kedgefacemap;
  GHash *facetrimap;

  KnifeBVH bvh;

  BLI_mempool *kverts;
  BLI_mempool *kedges;
  bool no_cuts; /* A cut has not been made yet. */

  BLI_Stack *undostack;
  BLI_Stack *splitstack; /* Store edge splits by #knife_split_edge. */

  float vthresh;
  float ethresh;

  /* Used for drag-cutting. */
  KnifeLineHit *linehits;
  int totlinehit;

  /* Data for mouse-position-derived data. */
  KnifePosData curr; /* Current point under the cursor. */
  KnifePosData prev; /* Last added cut (a line draws from the cursor to this). */
  KnifePosData init; /* The first point in the cut-list, used for closing the loop. */

  /* Number of knife edges `kedges`. */
  int totkedge;
  /* Number of knife vertices, `kverts`. */
  int totkvert;

  BLI_mempool *refs;

  KnifeColors colors;

  /* Run by the UI or not. */
  bool is_interactive;

  /* Operator options. */
  bool cut_through;   /* Preference, can be modified at runtime (that feature may go). */
  bool only_select;   /* Set on initialization. */
  bool select_result; /* Set on initialization. */

  bool is_ortho;
  float ortho_extent;
  float ortho_extent_center[3];

  float clipsta, clipend;

  enum { MODE_IDLE, MODE_DRAGGING, MODE_CONNECT, MODE_PANNING } mode;
  bool is_drag_hold;

  int prevmode;
  bool snap_midpoints;
  bool ignore_edge_snapping;
  bool ignore_vert_snapping;

  NumInput num;
  float angle_snapping_increment; /* Degrees */

  /* Use to check if we're currently dragging an angle snapped line. */
  short angle_snapping_mode;
  bool is_angle_snapping;
  bool angle_snapping;
  float angle;
  /* Relative angle snapping reference edge. */
  KnifeEdge *snap_ref_edge;
  int snap_ref_edges_count;
  int snap_edge; /* Used by #KNF_MODAL_CYCLE_ANGLE_SNAP_EDGE to choose an edge for snapping. */

  short constrain_axis;
  short constrain_axis_mode;
  bool axis_constrained;
  char axis_string[2];

  short dist_angle_mode;
  bool show_dist_angle;
  KnifeMeasureData mdata; /* Data for distance and angle drawing calculations. */

  KnifeUndoFrame *undo; /* Current undo frame. */
  bool is_drag_undo;

  bool depth_test;
} KnifeTool_OpData;

enum {
  KNF_MODAL_CANCEL = 1,
  KNF_MODAL_CONFIRM,
  KNF_MODAL_UNDO,
  KNF_MODAL_MIDPOINT_ON,
  KNF_MODAL_MIDPOINT_OFF,
  KNF_MODAL_NEW_CUT,
  KNF_MODAL_IGNORE_SNAP_ON,
  KNF_MODAL_IGNORE_SNAP_OFF,
  KNF_MODAL_ADD_CUT,
  KNF_MODAL_ANGLE_SNAP_TOGGLE,
  KNF_MODAL_CYCLE_ANGLE_SNAP_EDGE,
  KNF_MODAL_CUT_THROUGH_TOGGLE,
  KNF_MODAL_SHOW_DISTANCE_ANGLE_TOGGLE,
  KNF_MODAL_DEPTH_TEST_TOGGLE,
  KNF_MODAL_PANNING,
  KNF_MODAL_X_AXIS,
  KNF_MODAL_Y_AXIS,
  KNF_MODAL_Z_AXIS,
  KNF_MODAL_ADD_CUT_CLOSED,
};

enum {
  KNF_CONSTRAIN_ANGLE_MODE_NONE = 0,
  KNF_CONSTRAIN_ANGLE_MODE_SCREEN = 1,
  KNF_CONSTRAIN_ANGLE_MODE_RELATIVE = 2
};

enum {
  KNF_CONSTRAIN_AXIS_NONE = 0,
  KNF_CONSTRAIN_AXIS_X = 1,
  KNF_CONSTRAIN_AXIS_Y = 2,
  KNF_CONSTRAIN_AXIS_Z = 3
};

enum {
  KNF_CONSTRAIN_AXIS_MODE_NONE = 0,
  KNF_CONSTRAIN_AXIS_MODE_GLOBAL = 1,
  KNF_CONSTRAIN_AXIS_MODE_LOCAL = 2
};

enum {
  KNF_MEASUREMENT_NONE = 0,
  KNF_MEASUREMENT_BOTH = 1,
  KNF_MEASUREMENT_DISTANCE = 2,
  KNF_MEASUREMENT_ANGLE = 3
};

/* -------------------------------------------------------------------- */
/** \name Drawing
 * \{ */

static void knifetool_raycast_planes(const KnifeTool_OpData *kcd, float r_v1[3], float r_v2[3])
{
  float planes[4][4];
  planes_from_projmat(
      kcd->vc.rv3d->persmat, planes[2], planes[0], planes[1], planes[3], NULL, NULL);

  /* Ray-cast all planes. */
  float ray_dir[3];
  float ray_hit_best[2][3] = {{UNPACK3(kcd->prev.cage)}, {UNPACK3(kcd->curr.cage)}};
  float lambda_best[2] = {-FLT_MAX, FLT_MAX};
  int i;

  {
    float curr_cage_adjust[3];
    float co_depth[3];

    copy_v3_v3(co_depth, kcd->prev.cage);
    ED_view3d_win_to_3d(kcd->vc.v3d, kcd->region, co_depth, kcd->curr.mval, curr_cage_adjust);

    sub_v3_v3v3(ray_dir, curr_cage_adjust, kcd->prev.cage);
  }

  for (i = 0; i < 4; i++) {
    float ray_hit[3];
    float lambda_test;
    if (!isect_ray_plane_v3(kcd->prev.cage, ray_dir, planes[i], &lambda_test, false)) {
      continue;
    }

    madd_v3_v3v3fl(ray_hit, kcd->prev.cage, ray_dir, lambda_test);
    if (lambda_test < 0.0f) {
      if (lambda_test > lambda_best[0]) {
        copy_v3_v3(ray_hit_best[0], ray_hit);
        lambda_best[0] = lambda_test;
      }
    }
    else {
      if (lambda_test < lambda_best[1]) {
        copy_v3_v3(ray_hit_best[1], ray_hit);
        lambda_best[1] = lambda_test;
      }
    }
  }

  copy_v3_v3(r_v1, ray_hit_best[0]);
  copy_v3_v3(r_v2, ray_hit_best[1]);
}

static void knifetool_draw_angle_snapping(const KnifeTool_OpData *kcd)
{
  float v1[3], v2[3];

  knifetool_raycast_planes(kcd, v1, v2);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor3(TH_TRANSFORM);
  GPU_line_width(2.0);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex3fv(pos, v1);
  immVertex3fv(pos, v2);
  immEnd();

  immUnbindProgram();
}

static void knifetool_draw_orientation_locking(const KnifeTool_OpData *kcd)
{
  if (compare_v3v3(kcd->prev.cage, kcd->curr.cage, KNIFE_FLT_EPSBIG)) {
    return;
  }

  float v1[3], v2[3];

  /* This is causing buggy behavior when `prev.cage` and `curr.cage` are too close together. */
  knifetool_raycast_planes(kcd, v1, v2);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  switch (kcd->constrain_axis) {
    case KNF_CONSTRAIN_AXIS_X: {
      immUniformColor3ubv(kcd->colors.xaxis);
      break;
    }
    case KNF_CONSTRAIN_AXIS_Y: {
      immUniformColor3ubv(kcd->colors.yaxis);
      break;
    }
    case KNF_CONSTRAIN_AXIS_Z: {
      immUniformColor3ubv(kcd->colors.zaxis);
      break;
    }
    default: {
      immUniformColor3ubv(kcd->colors.axis_extra);
      break;
    }
  }

  GPU_line_width(2.0);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex3fv(pos, v1);
  immVertex3fv(pos, v2);
  immEnd();

  immUnbindProgram();
}

static void knifetool_draw_visible_distances(const KnifeTool_OpData *kcd)
{
  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_identity_set();
  wmOrtho2_region_pixelspace(kcd->region);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  char numstr[256];
  float numstr_size[2];
  float posit[2];
  const float bg_margin = 4.0f * UI_SCALE_FAC;
  const float font_size = 14.0f;
  const int distance_precision = 4;

  /* Calculate distance and convert to string. */
  const float cut_len = len_v3v3(kcd->prev.cage, kcd->curr.cage);

  UnitSettings *unit = &kcd->scene->unit;
  if (unit->system == USER_UNIT_NONE) {
    SNPRINTF(numstr, "%.*f", distance_precision, cut_len);
  }
  else {
    BKE_unit_value_as_string(numstr,
                             sizeof(numstr),
                             (double)(cut_len * unit->scale_length),
                             distance_precision,
                             B_UNIT_LENGTH,
                             unit,
                             false);
  }

  BLF_enable(blf_mono_font, BLF_ROTATION);
  BLF_size(blf_mono_font, font_size * UI_SCALE_FAC);
  BLF_rotation(blf_mono_font, 0.0f);
  BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

  /* Center text. */
  mid_v2_v2v2(posit, kcd->prev.mval, kcd->curr.mval);
  posit[0] -= numstr_size[0] / 2.0f;
  posit[1] -= numstr_size[1] / 2.0f;

  /* Draw text background. */
  float color_back[4] = {0.0f, 0.0f, 0.0f, 0.5f}; /* TODO: Replace with theme color. */
  immUniformColor4fv(color_back);

  GPU_blend(GPU_BLEND_ALPHA);
  immRectf(pos,
           posit[0] - bg_margin,
           posit[1] - bg_margin,
           posit[0] + bg_margin + numstr_size[0],
           posit[1] + bg_margin + numstr_size[1]);
  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();

  /* Draw text. */
  uchar color_text[3];
  UI_GetThemeColor3ubv(TH_TEXT, color_text);

  BLF_color3ubv(blf_mono_font, color_text);
  BLF_position(blf_mono_font, posit[0], posit[1], 0.0f);
  BLF_draw(blf_mono_font, numstr, sizeof(numstr));
  BLF_disable(blf_mono_font, BLF_ROTATION);

  GPU_matrix_pop();
  GPU_matrix_pop_projection();
}

static void knifetool_draw_angle(const KnifeTool_OpData *kcd,
                                 const float start[3],
                                 const float mid[3],
                                 const float end[3],
                                 const float start_ss[2],
                                 const float mid_ss[2],
                                 const float end_ss[2],
                                 const float angle)
{
  const RegionView3D *rv3d = kcd->region->regiondata;
  const int arc_steps = 24;
  const float arc_size = 64.0f * UI_SCALE_FAC;
  const float bg_margin = 4.0f * UI_SCALE_FAC;
  const float cap_size = 4.0f * UI_SCALE_FAC;
  const float font_size = 14.0f;
  const int angle_precision = 3;

  /* Angle arc in 3d space. */
  GPU_blend(GPU_BLEND_ALPHA);

  const uint pos_3d = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  {
    float dir_tmp[3];
    float ar_coord[3];

    float dir_a[3];
    float dir_b[3];
    float quat[4];
    float axis[3];
    float arc_angle;

    const float inverse_average_scale = 1 / (kcd->curr.ob->object_to_world[0][0] +
                                             kcd->curr.ob->object_to_world[1][1] +
                                             kcd->curr.ob->object_to_world[2][2]);

    const float px_scale =
        3.0f * inverse_average_scale *
        (ED_view3d_pixel_size_no_ui_scale(rv3d, mid) *
         min_fff(arc_size, len_v2v2(start_ss, mid_ss) / 2.0f, len_v2v2(end_ss, mid_ss) / 2.0f));

    sub_v3_v3v3(dir_a, start, mid);
    sub_v3_v3v3(dir_b, end, mid);
    normalize_v3(dir_a);
    normalize_v3(dir_b);

    cross_v3_v3v3(axis, dir_a, dir_b);
    arc_angle = angle_normalized_v3v3(dir_a, dir_b);

    axis_angle_to_quat(quat, axis, arc_angle / arc_steps);

    copy_v3_v3(dir_tmp, dir_a);

    immUniformThemeColor3(TH_WIRE);
    GPU_line_width(1.0);

    immBegin(GPU_PRIM_LINE_STRIP, arc_steps + 1);
    for (int j = 0; j <= arc_steps; j++) {
      madd_v3_v3v3fl(ar_coord, mid, dir_tmp, px_scale);
      mul_qt_v3(quat, dir_tmp);

      immVertex3fv(pos_3d, ar_coord);
    }
    immEnd();
  }

  immUnbindProgram();

  /* Angle text and background in 2d space. */
  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_identity_set();
  wmOrtho2_region_pixelspace(kcd->region);

  uint pos_2d = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Angle as string. */
  char numstr[256];
  float numstr_size[2];
  float posit[2];

  UnitSettings *unit = &kcd->scene->unit;
  if (unit->system == USER_UNIT_NONE) {
    SNPRINTF(numstr, "%.*f°", angle_precision, RAD2DEGF(angle));
  }
  else {
    BKE_unit_value_as_string(
        numstr, sizeof(numstr), (double)angle, angle_precision, B_UNIT_ROTATION, unit, false);
  }

  BLF_enable(blf_mono_font, BLF_ROTATION);
  BLF_size(blf_mono_font, font_size * UI_SCALE_FAC);
  BLF_rotation(blf_mono_font, 0.0f);
  BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

  posit[0] = mid_ss[0] + (cap_size * 2.0f);
  posit[1] = mid_ss[1] - (numstr_size[1] / 2.0f);

  /* Draw text background. */
  float color_back[4] = {0.0f, 0.0f, 0.0f, 0.5f}; /* TODO: Replace with theme color. */
  immUniformColor4fv(color_back);

  GPU_blend(GPU_BLEND_ALPHA);
  immRectf(pos_2d,
           posit[0] - bg_margin,
           posit[1] - bg_margin,
           posit[0] + bg_margin + numstr_size[0],
           posit[1] + bg_margin + numstr_size[1]);
  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();

  /* Draw text. */
  uchar color_text[3];
  UI_GetThemeColor3ubv(TH_TEXT, color_text);

  BLF_color3ubv(blf_mono_font, color_text);
  BLF_position(blf_mono_font, posit[0], posit[1], 0.0f);
  BLF_rotation(blf_mono_font, 0.0f);
  BLF_draw(blf_mono_font, numstr, sizeof(numstr));
  BLF_disable(blf_mono_font, BLF_ROTATION);

  GPU_matrix_pop();
  GPU_matrix_pop_projection();

  GPU_blend(GPU_BLEND_NONE);
}

static void knifetool_draw_visible_angles(const KnifeTool_OpData *kcd)
{
  Ref *ref;
  KnifeVert *kfv;
  KnifeVert *tempkfv;
  KnifeEdge *kfe;
  KnifeEdge *tempkfe;

  if (kcd->curr.vert) {
    kfv = kcd->curr.vert;

    float min_angle = FLT_MAX;
    float angle = 0.0f;
    float *end;

    kfe = ((Ref *)kfv->edges.first)->ref;
    for (ref = kfv->edges.first; ref; ref = ref->next) {
      tempkfe = ref->ref;
      if (tempkfe->v1 != kfv) {
        tempkfv = tempkfe->v1;
      }
      else {
        tempkfv = tempkfe->v2;
      }
      angle = angle_v3v3v3(kcd->prev.cage, kcd->curr.cage, tempkfv->cageco);
      if (angle < min_angle) {
        min_angle = angle;
        kfe = tempkfe;
        end = tempkfv->cageco;
      }
    }

    if (min_angle > KNIFE_FLT_EPSBIG) {
      /* Last vertex in screen space. */
      float end_ss[2];
      ED_view3d_project_float_global(kcd->region, end, end_ss, V3D_PROJ_TEST_NOP);

      knifetool_draw_angle(kcd,
                           kcd->prev.cage,
                           kcd->curr.cage,
                           end,
                           kcd->prev.mval,
                           kcd->curr.mval,
                           end_ss,
                           min_angle);
    }
  }
  else if (kcd->curr.edge) {
    kfe = kcd->curr.edge;

    /* Check for most recent cut (if cage is part of previous cut). */
    if (!compare_v3v3(kfe->v1->cageco, kcd->prev.cage, KNIFE_FLT_EPSBIG) &&
        !compare_v3v3(kfe->v2->cageco, kcd->prev.cage, KNIFE_FLT_EPSBIG))
    {
      /* Determine acute angle. */
      float angle1 = angle_v3v3v3(kcd->prev.cage, kcd->curr.cage, kfe->v1->cageco);
      float angle2 = angle_v3v3v3(kcd->prev.cage, kcd->curr.cage, kfe->v2->cageco);

      float angle;
      float *end;
      if (angle1 < angle2) {
        angle = angle1;
        end = kfe->v1->cageco;
      }
      else {
        angle = angle2;
        end = kfe->v2->cageco;
      }

      /* Last vertex in screen space. */
      float end_ss[2];
      ED_view3d_project_float_global(kcd->region, end, end_ss, V3D_PROJ_TEST_NOP);

      knifetool_draw_angle(
          kcd, kcd->prev.cage, kcd->curr.cage, end, kcd->prev.mval, kcd->curr.mval, end_ss, angle);
    }
  }

  if (kcd->prev.vert) {
    kfv = kcd->prev.vert;
    float min_angle = FLT_MAX;
    float angle = 0.0f;
    float *end;

    /* If using relative angle snapping, always draw angle to reference edge. */
    if (kcd->is_angle_snapping && kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) {
      kfe = kcd->snap_ref_edge;
      if (kfe->v1 != kfv) {
        tempkfv = kfe->v1;
      }
      else {
        tempkfv = kfe->v2;
      }
      min_angle = angle_v3v3v3(kcd->curr.cage, kcd->prev.cage, tempkfv->cageco);
      end = tempkfv->cageco;
    }
    else {
      /* Choose minimum angle edge. */
      kfe = ((Ref *)kfv->edges.first)->ref;
      for (ref = kfv->edges.first; ref; ref = ref->next) {
        tempkfe = ref->ref;
        if (tempkfe->v1 != kfv) {
          tempkfv = tempkfe->v1;
        }
        else {
          tempkfv = tempkfe->v2;
        }
        angle = angle_v3v3v3(kcd->curr.cage, kcd->prev.cage, tempkfv->cageco);
        if (angle < min_angle) {
          min_angle = angle;
          kfe = tempkfe;
          end = tempkfv->cageco;
        }
      }
    }

    if (min_angle > KNIFE_FLT_EPSBIG) {
      /* Last vertex in screen space. */
      float end_ss[2];
      ED_view3d_project_float_global(kcd->region, end, end_ss, V3D_PROJ_TEST_NOP);

      knifetool_draw_angle(kcd,
                           kcd->curr.cage,
                           kcd->prev.cage,
                           end,
                           kcd->curr.mval,
                           kcd->prev.mval,
                           end_ss,
                           min_angle);
    }
  }
  else if (kcd->prev.edge) {
    /* Determine acute angle. */
    kfe = kcd->prev.edge;
    float angle1 = angle_v3v3v3(kcd->curr.cage, kcd->prev.cage, kfe->v1->cageco);
    float angle2 = angle_v3v3v3(kcd->curr.cage, kcd->prev.cage, kfe->v2->cageco);

    float angle;
    float *end;
    /* kcd->prev.edge can have one vertex part of cut and one part of mesh? */
    /* This never seems to happen for kcd->curr.edge. */
    if ((!kcd->prev.vert || kcd->prev.vert->v == kfe->v1->v) || kfe->v1->is_cut) {
      angle = angle2;
      end = kfe->v2->cageco;
    }
    else if ((!kcd->prev.vert || kcd->prev.vert->v == kfe->v2->v) || kfe->v2->is_cut) {
      angle = angle1;
      end = kfe->v1->cageco;
    }
    else {
      if (angle1 < angle2) {
        angle = angle1;
        end = kfe->v1->cageco;
      }
      else {
        angle = angle2;
        end = kfe->v2->cageco;
      }
    }

    /* Last vertex in screen space. */
    float end_ss[2];
    ED_view3d_project_float_global(kcd->region, end, end_ss, V3D_PROJ_TEST_NOP);

    knifetool_draw_angle(
        kcd, kcd->curr.cage, kcd->prev.cage, end, kcd->curr.mval, kcd->prev.mval, end_ss, angle);
  }
  else if (kcd->mdata.is_stored && !kcd->prev.is_space) {
    float angle = angle_v3v3v3(kcd->curr.cage, kcd->prev.cage, kcd->mdata.cage);
    knifetool_draw_angle(kcd,
                         kcd->curr.cage,
                         kcd->prev.cage,
                         kcd->mdata.cage,
                         kcd->curr.mval,
                         kcd->prev.mval,
                         kcd->mdata.mval,
                         angle);
  }
}

static void knifetool_draw_dist_angle(const KnifeTool_OpData *kcd)
{
  switch (kcd->dist_angle_mode) {
    case KNF_MEASUREMENT_BOTH: {
      knifetool_draw_visible_distances(kcd);
      knifetool_draw_visible_angles(kcd);
      break;
    }
    case KNF_MEASUREMENT_DISTANCE: {
      knifetool_draw_visible_distances(kcd);
      break;
    }
    case KNF_MEASUREMENT_ANGLE: {
      knifetool_draw_visible_angles(kcd);
      break;
    }
  }
}

/* Modal loop selection drawing callback. */
static void knifetool_draw(const bContext *UNUSED(C), ARegion *UNUSED(region), void *arg)
{
  const KnifeTool_OpData *kcd = arg;
  GPU_depth_test(GPU_DEPTH_NONE);

  GPU_matrix_push_projection();
  GPU_polygon_offset(1.0f, 1.0f);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (kcd->mode == MODE_DRAGGING) {
    immUniformColor3ubv(kcd->colors.line);
    GPU_line_width(2.0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, kcd->prev.cage);
    immVertex3fv(pos, kcd->curr.cage);
    immEnd();
  }

  if (kcd->prev.vert) {
    immUniformColor3ubv(kcd->colors.point);
    GPU_point_size(11 * UI_SCALE_FAC);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->prev.cage);
    immEnd();
  }

  if (kcd->prev.bmface || kcd->prev.edge) {
    immUniformColor3ubv(kcd->colors.curpoint);
    GPU_point_size(9 * UI_SCALE_FAC);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->prev.cage);
    immEnd();
  }

  if (kcd->curr.vert) {
    immUniformColor3ubv(kcd->colors.point);
    GPU_point_size(11 * UI_SCALE_FAC);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->curr.cage);
    immEnd();
  }
  else if (kcd->curr.edge) {
    immUniformColor3ubv(kcd->colors.edge);
    GPU_line_width(2.0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, kcd->curr.edge->v1->cageco);
    immVertex3fv(pos, kcd->curr.edge->v2->cageco);
    immEnd();
  }

  if (kcd->curr.bmface || kcd->curr.edge) {
    immUniformColor3ubv(kcd->colors.curpoint);
    GPU_point_size(9 * UI_SCALE_FAC);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->curr.cage);
    immEnd();
  }

  if (kcd->depth_test) {
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }

  if (kcd->totkedge > 0) {
    BLI_mempool_iter iter;
    KnifeEdge *kfe;

    immUniformColor3ubv(kcd->colors.line);
    GPU_line_width(1.0);

    GPUBatch *batch = immBeginBatchAtMost(GPU_PRIM_LINES, BLI_mempool_len(kcd->kedges) * 2);

    BLI_mempool_iternew(kcd->kedges, &iter);
    for (kfe = BLI_mempool_iterstep(&iter); kfe; kfe = BLI_mempool_iterstep(&iter)) {
      if (!kfe->is_cut || kfe->is_invalid) {
        continue;
      }

      immVertex3fv(pos, kfe->v1->cageco);
      immVertex3fv(pos, kfe->v2->cageco);
    }

    immEnd();

    GPU_batch_draw(batch);
    GPU_batch_discard(batch);
  }

  if (kcd->totkvert > 0) {
    BLI_mempool_iter iter;
    KnifeVert *kfv;

    immUniformColor3ubv(kcd->colors.point);
    GPU_point_size(5.0 * UI_SCALE_FAC);

    GPUBatch *batch = immBeginBatchAtMost(GPU_PRIM_POINTS, BLI_mempool_len(kcd->kverts));

    BLI_mempool_iternew(kcd->kverts, &iter);
    for (kfv = BLI_mempool_iterstep(&iter); kfv; kfv = BLI_mempool_iterstep(&iter)) {
      if (!kfv->is_cut || kfv->is_invalid) {
        continue;
      }

      immVertex3fv(pos, kfv->cageco);
    }

    immEnd();

    GPU_batch_draw(batch);
    GPU_batch_discard(batch);
  }

  /* Draw relative angle snapping reference edge. */
  if (kcd->is_angle_snapping && kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) {
    immUniformColor3ubv(kcd->colors.edge_extra);
    GPU_line_width(2.0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, kcd->snap_ref_edge->v1->cageco);
    immVertex3fv(pos, kcd->snap_ref_edge->v2->cageco);
    immEnd();
  }

  if (kcd->totlinehit > 0) {
    KnifeLineHit *lh;
    int i, snapped_verts_count, other_verts_count;
    float fcol[4];

    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertBuf *vert = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(vert, kcd->totlinehit);

    lh = kcd->linehits;
    for (i = 0, snapped_verts_count = 0, other_verts_count = 0; i < kcd->totlinehit; i++, lh++) {
      if (lh->v) {
        GPU_vertbuf_attr_set(vert, pos, snapped_verts_count++, lh->cagehit);
      }
      else {
        GPU_vertbuf_attr_set(vert, pos, kcd->totlinehit - 1 - other_verts_count++, lh->cagehit);
      }
    }

    GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_POINTS, vert, NULL, GPU_BATCH_OWNS_VBO);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);

    /* Draw any snapped verts first. */
    rgba_uchar_to_float(fcol, kcd->colors.point_a);
    GPU_batch_uniform_4fv(batch, "color", fcol);
    GPU_point_size(11 * UI_SCALE_FAC);
    if (snapped_verts_count > 0) {
      GPU_batch_draw_range(batch, 0, snapped_verts_count);
    }

    /* Now draw the rest. */
    rgba_uchar_to_float(fcol, kcd->colors.curpoint_a);
    GPU_batch_uniform_4fv(batch, "color", fcol);
    GPU_point_size(7 * UI_SCALE_FAC);
    if (other_verts_count > 0) {
      GPU_batch_draw_range(batch, snapped_verts_count, other_verts_count);
    }

    GPU_batch_discard(batch);

    GPU_blend(GPU_BLEND_NONE);
  }

  immUnbindProgram();

  GPU_depth_test(GPU_DEPTH_NONE);

  if (kcd->mode == MODE_DRAGGING) {
    if (kcd->is_angle_snapping) {
      knifetool_draw_angle_snapping(kcd);
    }
    else if (kcd->axis_constrained) {
      knifetool_draw_orientation_locking(kcd);
    }

    if (kcd->show_dist_angle) {
      knifetool_draw_dist_angle(kcd);
    }
  }

  GPU_matrix_pop_projection();

  /* Reset default. */
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header
 * \{ */

static void knife_update_header(bContext *C, wmOperator *op, KnifeTool_OpData *kcd)
{
  char header[UI_MAX_DRAW_STR];
  char buf[UI_MAX_DRAW_STR];

  char *p = buf;
  int available_len = sizeof(buf);

#define WM_MODALKEY(_id) \
  WM_modalkeymap_operator_items_to_string_buf( \
      op->type, (_id), true, UI_MAX_SHORTCUT_STR, &available_len, &p)

  SNPRINTF(
      header,
      TIP_("%s: confirm, %s: cancel, %s: undo, "
           "%s: start/define cut, %s: close cut, %s: new cut, "
           "%s: midpoint snap (%s), %s: ignore snap (%s), "
           "%s: angle constraint %.2f(%.2f) (%s%s%s%s), %s: cut through (%s), "
           "%s: panning, %s%s%s: orientation lock (%s), "
           "%s: distance/angle measurements (%s), "
           "%s: x-ray (%s)"),
      WM_MODALKEY(KNF_MODAL_CONFIRM),
      WM_MODALKEY(KNF_MODAL_CANCEL),
      WM_MODALKEY(KNF_MODAL_UNDO),
      WM_MODALKEY(KNF_MODAL_ADD_CUT),
      WM_MODALKEY(KNF_MODAL_ADD_CUT_CLOSED),
      WM_MODALKEY(KNF_MODAL_NEW_CUT),
      WM_MODALKEY(KNF_MODAL_MIDPOINT_ON),
      WM_bool_as_string(kcd->snap_midpoints),
      WM_MODALKEY(KNF_MODAL_IGNORE_SNAP_ON),
      WM_bool_as_string(kcd->ignore_edge_snapping),
      WM_MODALKEY(KNF_MODAL_ANGLE_SNAP_TOGGLE),
      (kcd->angle >= 0.0f) ? RAD2DEGF(kcd->angle) : 360.0f + RAD2DEGF(kcd->angle),
      (kcd->angle_snapping_increment > KNIFE_MIN_ANGLE_SNAPPING_INCREMENT &&
       kcd->angle_snapping_increment <= KNIFE_MAX_ANGLE_SNAPPING_INCREMENT) ?
          kcd->angle_snapping_increment :
          KNIFE_DEFAULT_ANGLE_SNAPPING_INCREMENT,
      kcd->angle_snapping ?
          ((kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_SCREEN) ? "Screen" : "Relative") :
          "OFF",
      /* TODO: Can this be simplified? */
      (kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) ? " - " : "",
      (kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) ?
          WM_MODALKEY(KNF_MODAL_CYCLE_ANGLE_SNAP_EDGE) :
          "",
      (kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) ? ": cycle edge" : "",
      /**/
      WM_MODALKEY(KNF_MODAL_CUT_THROUGH_TOGGLE),
      WM_bool_as_string(kcd->cut_through),
      WM_MODALKEY(KNF_MODAL_PANNING),
      WM_MODALKEY(KNF_MODAL_X_AXIS),
      WM_MODALKEY(KNF_MODAL_Y_AXIS),
      WM_MODALKEY(KNF_MODAL_Z_AXIS),
      (kcd->axis_constrained ? kcd->axis_string : WM_bool_as_string(kcd->axis_constrained)),
      WM_MODALKEY(KNF_MODAL_SHOW_DISTANCE_ANGLE_TOGGLE),
      WM_bool_as_string(kcd->show_dist_angle),
      WM_MODALKEY(KNF_MODAL_DEPTH_TEST_TOGGLE),
      WM_bool_as_string(!kcd->depth_test));

#undef WM_MODALKEY

  ED_workspace_status_text(C, header);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knife Object Info Accessors (#KnifeObjectInfo)
 * \{ */

static const int *knife_bm_tri_index_get(const KnifeTool_OpData *kcd,
                                         int ob_index,
                                         int tri_index,
                                         int tri_index_buf[3])
{
  const KnifeObjectInfo *obinfo = &kcd->objects_info[ob_index];
  if (obinfo->tri_indices) {
    return obinfo->tri_indices[tri_index];
  }
  for (int i = 0; i < 3; i++) {
    tri_index_buf[i] = BM_elem_index_get(obinfo->em->looptris[tri_index][i]->v);
  }
  return tri_index_buf;
}

static void knife_bm_tri_cagecos_get(const KnifeTool_OpData *kcd,
                                     int ob_index,
                                     int tri_index,
                                     float cos[3][3])
{
  const KnifeObjectInfo *obinfo = &kcd->objects_info[ob_index];
  int tri_ind_buf[3];
  const int *tri_ind = knife_bm_tri_index_get(kcd, ob_index, tri_index, tri_ind_buf);
  for (int i = 0; i < 3; i++) {
    copy_v3_v3(cos[i], obinfo->cagecos[tri_ind[i]]);
  }
}

static void knife_bm_tri_cagecos_get_worldspace(const KnifeTool_OpData *kcd,
                                                int ob_index,
                                                int tri_index,
                                                float cos[3][3])
{
  knife_bm_tri_cagecos_get(kcd, ob_index, tri_index, cos);
  const Object *ob = kcd->objects[ob_index];
  for (int i = 0; i < 3; i++) {
    mul_m4_v3(ob->object_to_world, cos[i]);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knife BVH Utils
 * \{ */

static bool knife_bm_face_is_select(BMFace *f)
{
  return (BM_elem_flag_test(f, BM_ELEM_SELECT) != 0);
}

static bool knife_bm_face_is_not_hidden(BMFace *f)
{
  return (BM_elem_flag_test(f, BM_ELEM_HIDDEN) == 0);
}

static void knife_bvh_init(KnifeTool_OpData *kcd)
{
  Object *ob;
  BMEditMesh *em;

  /* Test Function. */
  bool (*test_fn)(BMFace *);
  if (kcd->only_select && kcd->cut_through) {
    test_fn = knife_bm_face_is_select;
  }
  else {
    test_fn = knife_bm_face_is_not_hidden;
  }

  /* Construct BVH Tree. */
  const float epsilon = FLT_EPSILON * 2.0f;
  int tottri = 0;
  int ob_tottri = 0;
  BMLoop *(*looptris)[3];
  BMFace *f_test = NULL, *f_test_prev = NULL;
  bool test_fn_ret = false;

  /* Calculate tottri. */
  for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
    ob_tottri = 0;
    ob = kcd->objects[ob_index];
    em = BKE_editmesh_from_object(ob);

    for (int i = 0; i < em->tottri; i++) {
      f_test = em->looptris[i][0]->f;
      if (f_test != f_test_prev) {
        test_fn_ret = test_fn(f_test);
        f_test_prev = f_test;
      }

      if (test_fn_ret) {
        ob_tottri++;
      }
    }

    tottri += ob_tottri;
  }

  kcd->bvh.tree = BLI_bvhtree_new(tottri, epsilon, 8, 8);

  f_test_prev = NULL;
  test_fn_ret = false;

  /* Add tri's for each object.
   * TODO:
   * test_fn can leave large gaps between bvh tree indices.
   * Compacting bvh tree indices may be possible.
   * Don't forget to update #knife_bvh_intersect_plane!
   */
  tottri = 0;
  for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
    ob = kcd->objects[ob_index];
    em = BKE_editmesh_from_object(ob);
    looptris = em->looptris;

    for (int i = 0; i < em->tottri; i++) {

      f_test = looptris[i][0]->f;
      if (f_test != f_test_prev) {
        test_fn_ret = test_fn(f_test);
        f_test_prev = f_test;
      }

      if (!test_fn_ret) {
        continue;
      }

      float tri_cos[3][3];
      knife_bm_tri_cagecos_get_worldspace(kcd, ob_index, i, tri_cos);
      BLI_bvhtree_insert(kcd->bvh.tree, i + tottri, &tri_cos[0][0], 3);
    }

    tottri += em->tottri;
  }

  BLI_bvhtree_balance(kcd->bvh.tree);
}

/* Wrapper for #BLI_bvhtree_free. */
static void knife_bvh_free(KnifeTool_OpData *kcd)
{
  if (kcd->bvh.tree) {
    BLI_bvhtree_free(kcd->bvh.tree);
    kcd->bvh.tree = NULL;
  }
}

static void knife_bvh_raycast_cb(void *userdata,
                                 int index,
                                 const BVHTreeRay *ray,
                                 BVHTreeRayHit *hit)
{
  if (index == -1) {
    return;
  }

  KnifeTool_OpData *kcd = userdata;
  BMLoop **ltri;
  Object *ob;
  BMEditMesh *em;

  float dist, uv[2];
  bool isect;
  int tottri;

  tottri = 0;
  uint ob_index = 0;
  for (; ob_index < kcd->objects_len; ob_index++) {
    index -= tottri;
    ob = kcd->objects[ob_index];
    em = BKE_editmesh_from_object(ob);
    tottri = em->tottri;
    if (index < tottri) {
      ltri = em->looptris[index];
      break;
    }
  }

  if (kcd->bvh.filter_cb) {
    if (!kcd->bvh.filter_cb(ltri[0]->f, kcd->bvh.filter_data)) {
      return;
    }
  }

  float tri_cos[3][3];
  knife_bm_tri_cagecos_get_worldspace(kcd, ob_index, index, tri_cos);
  isect = (ray->radius > 0.0f ?
               isect_ray_tri_epsilon_v3(
                   ray->origin, ray->direction, UNPACK3(tri_cos), &dist, uv, ray->radius) :
#ifdef USE_KDOPBVH_WATERTIGHT
               isect_ray_tri_watertight_v3(
                   ray->origin, ray->isect_precalc, UNPACK3(tri_cos), &dist, uv));
#else
                 isect_ray_tri_v3(ray->origin, ray->direction, UNPACK3(tri_cos), &dist, uv);
#endif

  if (isect && dist < hit->dist) {
    madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

    /* Discard clipped points. */
    if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d) &&
        ED_view3d_clipping_test(kcd->vc.rv3d, hit->co, false))
    {
      return;
    }

    hit->dist = dist;
    hit->index = index;

    copy_v3_v3(hit->no, ltri[0]->f->no);

    kcd->bvh.looptris = em->looptris;
    copy_v2_v2(kcd->bvh.uv, uv);
    kcd->bvh.ob_index = ob_index;
  }
}

/* `co` is expected to be in world space. */
static BMFace *knife_bvh_raycast(KnifeTool_OpData *kcd,
                                 const float co[3],
                                 const float dir[3],
                                 const float radius,
                                 float *r_dist,
                                 float r_hitout[3],
                                 float r_cagehit[3],
                                 uint *r_ob_index)
{
  BMFace *face;
  BVHTreeRayHit hit;
  const float dist = r_dist ? *r_dist : FLT_MAX;
  hit.dist = dist;
  hit.index = -1;

  BLI_bvhtree_ray_cast(kcd->bvh.tree, co, dir, radius, &hit, knife_bvh_raycast_cb, kcd);

  /* Handle Hit */
  if (hit.index != -1 && hit.dist != dist) {
    face = kcd->bvh.looptris[hit.index][0]->f;

    /* Hits returned in world space. */
    if (r_hitout) {
      float tri_cos[3][3];
      knife_bm_tri_cagecos_get_worldspace(kcd, kcd->bvh.ob_index, hit.index, tri_cos);
      interp_v3_v3v3v3_uv(r_hitout, UNPACK3(tri_cos), kcd->bvh.uv);

      if (r_cagehit) {
        copy_v3_v3(r_cagehit, hit.co);
      }
    }

    if (r_dist) {
      *r_dist = hit.dist;
    }

    if (r_ob_index) {
      *r_ob_index = kcd->bvh.ob_index;
    }

    return face;
  }
  return NULL;
}

/* `co` is expected to be in world space. */
static BMFace *knife_bvh_raycast_filter(KnifeTool_OpData *kcd,
                                        const float co[3],
                                        const float dir[3],
                                        const float radius,
                                        float *r_dist,
                                        float r_hitout[3],
                                        float r_cagehit[3],
                                        uint *r_ob_index,
                                        bool (*filter_cb)(BMFace *f, void *userdata),
                                        void *filter_userdata)
{
  kcd->bvh.filter_cb = filter_cb;
  kcd->bvh.filter_data = filter_userdata;

  BMFace *face;
  BVHTreeRayHit hit;
  const float dist = r_dist ? *r_dist : FLT_MAX;
  hit.dist = dist;
  hit.index = -1;

  BLI_bvhtree_ray_cast(kcd->bvh.tree, co, dir, radius, &hit, knife_bvh_raycast_cb, kcd);

  kcd->bvh.filter_cb = NULL;
  kcd->bvh.filter_data = NULL;

  /* Handle Hit */
  if (hit.index != -1 && hit.dist != dist) {
    face = kcd->bvh.looptris[hit.index][0]->f;

    /* Hits returned in world space. */
    if (r_hitout) {
      float tri_cos[3][3];
      knife_bm_tri_cagecos_get_worldspace(kcd, kcd->bvh.ob_index, hit.index, tri_cos);
      interp_v3_v3v3v3_uv(r_hitout, UNPACK3(tri_cos), kcd->bvh.uv);

      if (r_cagehit) {
        copy_v3_v3(r_cagehit, hit.co);
      }
    }

    if (r_dist) {
      *r_dist = hit.dist;
    }

    if (r_ob_index) {
      *r_ob_index = kcd->bvh.ob_index;
    }

    return face;
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Utils
 * \{ */

static void knife_project_v2(const KnifeTool_OpData *kcd, const float co[3], float sco[2])
{
  ED_view3d_project_float_global(kcd->region, co, sco, V3D_PROJ_TEST_NOP);
}

/* Ray is returned in world space. */
static void knife_input_ray_segment(KnifeTool_OpData *kcd,
                                    const float mval[2],
                                    const float ofs,
                                    float r_origin[3],
                                    float r_origin_ofs[3])
{
  /* Unproject to find view ray. */
  ED_view3d_unproject_v3(kcd->vc.region, mval[0], mval[1], 0.0f, r_origin);
  ED_view3d_unproject_v3(kcd->vc.region, mval[0], mval[1], ofs, r_origin_ofs);
}

/* No longer used, but may be useful in the future. */
static void UNUSED_FUNCTION(knifetool_recast_cageco)(KnifeTool_OpData *kcd,
                                                     float mval[3],
                                                     float r_cage[3])
{
  float origin[3];
  float origin_ofs[3];
  float ray[3], ray_normal[3];
  float co[3]; /* Unused. */

  knife_input_ray_segment(kcd, mval, 1.0f, origin, origin_ofs);

  sub_v3_v3v3(ray, origin_ofs, origin);
  normalize_v3_v3(ray_normal, ray);

  knife_bvh_raycast(kcd, origin, ray_normal, 0.0f, NULL, co, r_cage, NULL);
}

static bool knife_verts_edge_in_face(KnifeVert *v1, KnifeVert *v2, BMFace *f)
{
  bool v1_inside, v2_inside;
  bool v1_inface, v2_inface;
  BMLoop *l1, *l2;

  if (!f || !v1 || !v2) {
    return false;
  }

  l1 = v1->v ? BM_face_vert_share_loop(f, v1->v) : NULL;
  l2 = v2->v ? BM_face_vert_share_loop(f, v2->v) : NULL;

  if ((l1 && l2) && BM_loop_is_adjacent(l1, l2)) {
    /* Boundary-case, always false to avoid edge-in-face checks below. */
    return false;
  }

  /* Find out if v1 and v2, if set, are part of the face. */
  v1_inface = (l1 != NULL);
  v2_inface = (l2 != NULL);

  /* BM_face_point_inside_test uses best-axis projection so this isn't most accurate test... */
  v1_inside = v1_inface ? false : BM_face_point_inside_test(f, v1->co);
  v2_inside = v2_inface ? false : BM_face_point_inside_test(f, v2->co);
  if ((v1_inface && v2_inside) || (v2_inface && v1_inside) || (v1_inside && v2_inside)) {
    return true;
  }

  if (v1_inface && v2_inface) {
    float mid[3];
    /* Can have case where v1 and v2 are on shared chain between two faces.
     * BM_face_splits_check_legal does visibility and self-intersection tests,
     * but it is expensive and maybe a bit buggy, so use a simple
     * "is the midpoint in the face" test. */
    mid_v3_v3v3(mid, v1->co, v2->co);
    return BM_face_point_inside_test(f, mid);
  }
  return false;
}

static void knife_recalc_ortho(KnifeTool_OpData *kcd)
{
  kcd->is_ortho = ED_view3d_clip_range_get(
      kcd->vc.depsgraph, kcd->vc.v3d, kcd->vc.rv3d, &kcd->clipsta, &kcd->clipend, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knife Element Utils
 *
 * Currently only used in #knife_find_line_hits.
 * \{ */

static BMElem *bm_elem_from_knife_vert(KnifeVert *kfv, KnifeEdge **r_kfe)
{
  BMElem *ele_test;
  KnifeEdge *kfe = NULL;

  /* vert? */
  ele_test = (BMElem *)kfv->v;

  if (r_kfe || ele_test == NULL) {
    if (kfv->v == NULL) {
      Ref *ref;
      for (ref = kfv->edges.first; ref; ref = ref->next) {
        kfe = ref->ref;
        if (kfe->e) {
          if (r_kfe) {
            *r_kfe = kfe;
          }
          break;
        }
      }
    }
  }

  /* edge? */
  if (ele_test == NULL) {
    if (kfe) {
      ele_test = (BMElem *)kfe->e;
    }
  }

  /* face? */
  if (ele_test == NULL) {
    if (BLI_listbase_is_single(&kfe->faces)) {
      ele_test = ((Ref *)kfe->faces.first)->ref;
    }
  }

  return ele_test;
}

static BMElem *bm_elem_from_knife_edge(KnifeEdge *kfe)
{
  BMElem *ele_test;

  ele_test = (BMElem *)kfe->e;

  if (ele_test == NULL) {
    ele_test = (BMElem *)kfe->basef;
  }

  return ele_test;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knife Element List Utils
 * \{ */

static ListBase *knife_empty_list(KnifeTool_OpData *kcd)
{
  ListBase *list;

  list = BLI_memarena_alloc(kcd->arena, sizeof(ListBase));
  BLI_listbase_clear(list);
  return list;
}

static void knife_append_list(KnifeTool_OpData *kcd, ListBase *lst, void *elem)
{
  Ref *ref;

  ref = BLI_mempool_calloc(kcd->refs);
  ref->ref = elem;
  BLI_addtail(lst, ref);
}

static Ref *find_ref(ListBase *lb, void *ref)
{
  Ref *ref1;

  for (ref1 = lb->first; ref1; ref1 = ref1->next) {
    if (ref1->ref == ref) {
      return ref1;
    }
  }

  return NULL;
}

static void knife_append_list_no_dup(KnifeTool_OpData *kcd, ListBase *lst, void *elem)
{
  if (!find_ref(lst, elem)) {
    knife_append_list(kcd, lst, elem);
  }
}

static void knife_add_to_vert_edges(KnifeTool_OpData *kcd, KnifeEdge *kfe)
{
  knife_append_list(kcd, &kfe->v1->edges, kfe);
  knife_append_list(kcd, &kfe->v2->edges, kfe);
}

/* Add faces of an edge to a KnifeVert's faces list.  No checks for dups. */
static void knife_add_edge_faces_to_vert(KnifeTool_OpData *kcd, KnifeVert *kfv, BMEdge *e)
{
  BMIter bmiter;
  BMFace *f;

  BM_ITER_ELEM (f, &bmiter, e, BM_FACES_OF_EDGE) {
    knife_append_list(kcd, &kfv->faces, f);
  }
}

/* Find a face in common in the two faces lists.
 * If more than one, return the first; if none, return NULL. */
static BMFace *knife_find_common_face(ListBase *faces1, ListBase *faces2)
{
  Ref *ref1, *ref2;

  for (ref1 = faces1->first; ref1; ref1 = ref1->next) {
    for (ref2 = faces2->first; ref2; ref2 = ref2->next) {
      if (ref1->ref == ref2->ref) {
        return (BMFace *)(ref1->ref);
      }
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knife Element Creation
 * \{ */

static KnifeVert *new_knife_vert(KnifeTool_OpData *kcd, const float co[3], const float cageco[3])
{
  KnifeVert *kfv = BLI_mempool_calloc(kcd->kverts);

  kcd->totkvert++;

  copy_v3_v3(kfv->co, co);
  copy_v3_v3(kfv->cageco, cageco);

  return kfv;
}

static KnifeEdge *new_knife_edge(KnifeTool_OpData *kcd)
{
  KnifeEdge *kfe = BLI_mempool_calloc(kcd->kedges);
  kcd->totkedge++;
  return kfe;
}

/* Get a KnifeVert wrapper for an existing BMVert. */
static KnifeVert *get_bm_knife_vert(KnifeTool_OpData *kcd, BMVert *v, Object *ob, uint ob_index)
{
  KnifeVert *kfv = BLI_ghash_lookup(kcd->origvertmap, v);
  const float *cageco;

  if (!kfv) {
    BMIter bmiter;
    BMFace *f;

    if (BM_elem_index_get(v) >= 0) {
      cageco = kcd->objects_info[ob_index].cagecos[BM_elem_index_get(v)];
    }
    else {
      cageco = v->co;
    }

    float cageco_ws[3];
    mul_v3_m4v3(cageco_ws, ob->object_to_world, cageco);

    kfv = new_knife_vert(kcd, v->co, cageco_ws);
    kfv->v = v;
    kfv->ob = ob;
    kfv->ob_index = ob_index;

    BLI_ghash_insert(kcd->origvertmap, v, kfv);
    BM_ITER_ELEM (f, &bmiter, v, BM_FACES_OF_VERT) {
      knife_append_list(kcd, &kfv->faces, f);
    }
  }

  return kfv;
}

/* Get a KnifeEdge wrapper for an existing BMEdge. */
static KnifeEdge *get_bm_knife_edge(KnifeTool_OpData *kcd, BMEdge *e, Object *ob, uint ob_index)
{
  KnifeEdge *kfe = BLI_ghash_lookup(kcd->origedgemap, e);
  if (!kfe) {
    BMIter bmiter;
    BMFace *f;

    kfe = new_knife_edge(kcd);
    kfe->e = e;
    kfe->v1 = get_bm_knife_vert(kcd, e->v1, ob, ob_index);
    kfe->v2 = get_bm_knife_vert(kcd, e->v2, ob, ob_index);

    knife_add_to_vert_edges(kcd, kfe);

    BLI_ghash_insert(kcd->origedgemap, e, kfe);

    BM_ITER_ELEM (f, &bmiter, e, BM_FACES_OF_EDGE) {
      knife_append_list(kcd, &kfe->faces, f);
    }
  }

  return kfe;
}

static ListBase *knife_get_face_kedges(KnifeTool_OpData *kcd, Object *ob, uint ob_index, BMFace *f)
{
  ListBase *list = BLI_ghash_lookup(kcd->kedgefacemap, f);

  if (!list) {
    BMIter bmiter;
    BMEdge *e;

    list = knife_empty_list(kcd);

    BM_ITER_ELEM (e, &bmiter, f, BM_EDGES_OF_FACE) {
      knife_append_list(kcd, list, get_bm_knife_edge(kcd, e, ob, ob_index));
    }

    BLI_ghash_insert(kcd->kedgefacemap, f, list);
  }

  return list;
}

static void knife_edge_append_face(KnifeTool_OpData *kcd, KnifeEdge *kfe, BMFace *f)
{
  knife_append_list(kcd, knife_get_face_kedges(kcd, kfe->v1->ob, kfe->v1->ob_index, f), kfe);
  knife_append_list(kcd, &kfe->faces, f);
}

static KnifeVert *knife_split_edge(KnifeTool_OpData *kcd,
                                   KnifeEdge *kfe,
                                   const float co[3],
                                   const float cageco[3],
                                   KnifeEdge **r_kfe)
{
  KnifeEdge *newkfe = new_knife_edge(kcd);
  Ref *ref;
  BMFace *f;

  newkfe->v1 = kfe->v1;
  newkfe->v2 = new_knife_vert(kcd, co, cageco);
  newkfe->v2->ob = kfe->v1->ob;
  newkfe->v2->ob_index = kfe->v1->ob_index;
  newkfe->v2->is_cut = true;
  if (kfe->e) {
    knife_add_edge_faces_to_vert(kcd, newkfe->v2, kfe->e);
  }
  else {
    /* kfe cuts across an existing face.
     * If v1 and v2 are in multiple faces together (e.g., if they
     * are in doubled polys) then this arbitrarily chooses one of them. */
    f = knife_find_common_face(&kfe->v1->faces, &kfe->v2->faces);
    if (f) {
      knife_append_list(kcd, &newkfe->v2->faces, f);
    }
  }
  newkfe->basef = kfe->basef;

  ref = find_ref(&kfe->v1->edges, kfe);
  BLI_remlink(&kfe->v1->edges, ref);

  kfe->v1 = newkfe->v2;
  kfe->v1->is_splitting = true;
  BLI_addtail(&kfe->v1->edges, ref);

  for (ref = kfe->faces.first; ref; ref = ref->next) {
    knife_edge_append_face(kcd, newkfe, ref->ref);
  }

  knife_add_to_vert_edges(kcd, newkfe);

  newkfe->is_cut = kfe->is_cut;
  newkfe->e = kfe->e;

  newkfe->splits++;
  kfe->splits++;

  kcd->undo->splits++;

  BLI_stack_push(kcd->splitstack, (void *)&kfe);
  BLI_stack_push(kcd->splitstack, (void *)&newkfe);

  *r_kfe = newkfe;

  return newkfe->v2;
}

/* Rejoin two edges split by #knife_split_edge. */
static void knife_join_edge(KnifeEdge *newkfe, KnifeEdge *kfe)
{
  newkfe->is_invalid = true;
  newkfe->v2->is_invalid = true;

  kfe->v1 = newkfe->v1;

  kfe->splits--;
  kfe->v1->is_splitting = false;
  kfe->v2->is_splitting = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cut/Hit Utils
 * \{ */

/* User has just clicked for first time or first time after a restart (E key).
 * Copy the current position data into prev. */
static void knife_start_cut(KnifeTool_OpData *kcd)
{
  kcd->prev = kcd->curr;
  kcd->curr.is_space = 0; /* TODO: Why do we do this? */
  kcd->mdata.is_stored = false;

  if (kcd->prev.vert == NULL && kcd->prev.edge == NULL) {
    float ofs_local[3];
    negate_v3_v3(ofs_local, kcd->vc.rv3d->ofs);
    ED_view3d_win_to_3d(kcd->vc.v3d, kcd->region, ofs_local, kcd->curr.mval, kcd->prev.cage);

    copy_v3_v3(kcd->prev.co, kcd->prev.cage); /* TODO: do we need this? */
    copy_v3_v3(kcd->curr.cage, kcd->prev.cage);
    copy_v3_v3(kcd->curr.co, kcd->prev.co);
  }
}

static void linehit_to_knifepos(KnifePosData *kpos, KnifeLineHit *lh)
{
  kpos->bmface = lh->f;
  kpos->vert = lh->v;
  kpos->edge = lh->kfe;
  copy_v3_v3(kpos->cage, lh->cagehit);
  copy_v3_v3(kpos->co, lh->hit);
  copy_v2_v2(kpos->mval, lh->schit);
}

/* Primary key: lambda along cut
 * Secondary key: lambda along depth
 * Tertiary key: pointer comparisons of verts if both snapped to verts
 */
static int linehit_compare(const void *vlh1, const void *vlh2)
{
  const KnifeLineHit *lh1 = vlh1;
  const KnifeLineHit *lh2 = vlh2;

  if (lh1->l < lh2->l) {
    return -1;
  }
  if (lh1->l > lh2->l) {
    return 1;
  }
  if (lh1->m < lh2->m) {
    return -1;
  }
  if (lh1->m > lh2->m) {
    return 1;
  }
  if (lh1->v < lh2->v) {
    return -1;
  }
  if (lh1->v > lh2->v) {
    return 1;
  }
  return 0;
}

/*
 * Sort linehits by distance along cut line, and secondarily from
 * front to back (from eye), and tertiarily by snap vertex,
 * and remove any duplicates.
 */
static void prepare_linehits_for_cut(KnifeTool_OpData *kcd)
{
  bool is_double = false;

  int n = kcd->totlinehit;
  KnifeLineHit *linehits = kcd->linehits;
  if (n == 0) {
    return;
  }

  qsort(linehits, n, sizeof(KnifeLineHit), linehit_compare);

  /* Remove any edge hits that are preceded or followed
   * by a vertex hit that is very near. Mark such edge hits using
   * l == -1 and then do another pass to actually remove.
   * Also remove all but one of a series of vertex hits for the same vertex. */
  for (int i = 0; i < n; i++) {
    KnifeLineHit *lhi = &linehits[i];
    if (lhi->v == NULL) {
      continue;
    }

    for (int j = i - 1; j >= 0; j--) {
      KnifeLineHit *lhj = &linehits[j];
      if (!lhj->kfe || fabsf(lhi->l - lhj->l) > KNIFE_FLT_EPSBIG ||
          fabsf(lhi->m - lhj->m) > KNIFE_FLT_EPSBIG)
      {
        break;
      }

      if (lhi->kfe == lhj->kfe) {
        lhj->l = -1.0f;
        is_double = true;
      }
    }
    for (int j = i + 1; j < n; j++) {
      KnifeLineHit *lhj = &linehits[j];
      if (fabsf(lhi->l - lhj->l) > KNIFE_FLT_EPSBIG || fabsf(lhi->m - lhj->m) > KNIFE_FLT_EPSBIG) {
        break;
      }
      if ((lhj->kfe && (lhi->kfe == lhj->kfe)) || (lhi->v == lhj->v)) {
        lhj->l = -1.0f;
        is_double = true;
      }
    }
  }

  if (is_double) {
    /* Delete-in-place loop: copying from pos j to pos i+1. */
    int i = 0;
    int j = 1;
    while (j < n) {
      KnifeLineHit *lhi = &linehits[i];
      KnifeLineHit *lhj = &linehits[j];
      if (lhj->l == -1.0f) {
        j++; /* Skip copying this one. */
      }
      else {
        /* Copy unless a no-op. */
        if (lhi->l == -1.0f) {
          /* Could happen if linehits[0] is being deleted. */
          memcpy(&linehits[i], &linehits[j], sizeof(KnifeLineHit));
        }
        else {
          if (i + 1 != j) {
            memcpy(&linehits[i + 1], &linehits[j], sizeof(KnifeLineHit));
          }
          i++;
        }
        j++;
      }
    }
    kcd->totlinehit = i + 1;
  }
}

/* Add hit to list of hits in facehits[f], where facehits is a map, if not already there. */
static void add_hit_to_facehits(KnifeTool_OpData *kcd,
                                GHash *facehits,
                                BMFace *f,
                                KnifeLineHit *hit)
{
  ListBase *list = BLI_ghash_lookup(facehits, f);

  if (!list) {
    list = knife_empty_list(kcd);
    BLI_ghash_insert(facehits, f, list);
  }
  knife_append_list_no_dup(kcd, list, hit);
}

/**
 * Special purpose function, if the linehit is connected to a real edge/vert.
 * Return true if \a co is outside the face.
 */
static bool knife_add_single_cut__is_linehit_outside_face(BMFace *f,
                                                          const KnifeLineHit *lh,
                                                          const float co[3])
{

  if (lh->v && lh->v->v) {
    BMLoop *l; /* side-of-loop */
    if ((l = BM_face_vert_share_loop(f, lh->v->v)) &&
        (BM_loop_point_side_of_loop_test(l, co) < 0.0f)) {
      return true;
    }
  }
  else if (lh->kfe && lh->kfe->e) {
    BMLoop *l; /* side-of-edge */
    if ((l = BM_face_edge_share_loop(f, lh->kfe->e)) &&
        (BM_loop_point_side_of_edge_test(l, co) < 0.0f))
    {
      return true;
    }
  }

  return false;
}

static void knife_add_single_cut(KnifeTool_OpData *kcd,
                                 KnifeLineHit *lh1,
                                 KnifeLineHit *lh2,
                                 BMFace *f)
{
  KnifeEdge *kfe, *kfe2;
  BMEdge *e_base;

  if ((lh1->v && lh1->v == lh2->v) || (lh1->kfe && lh1->kfe == lh2->kfe)) {
    return;
  }

  /* If the cut is on an edge. */
  if ((lh1->v && lh2->v) && (lh1->v->v && lh2->v && lh2->v->v) &&
      (e_base = BM_edge_exists(lh1->v->v, lh2->v->v)))
  {
    return;
  }
  if (knife_add_single_cut__is_linehit_outside_face(f, lh1, lh2->hit) ||
      knife_add_single_cut__is_linehit_outside_face(f, lh2, lh1->hit))
  {
    return;
  }

  /* Check if edge actually lies within face (might not, if this face is concave). */
  if ((lh1->v && !lh1->kfe) && (lh2->v && !lh2->kfe)) {
    if (!knife_verts_edge_in_face(lh1->v, lh2->v, f)) {
      return;
    }
  }

  kfe = new_knife_edge(kcd);
  kfe->is_cut = true;
  kfe->basef = f;

  if (lh1->v) {
    kfe->v1 = lh1->v;
  }
  else if (lh1->kfe) {
    kfe->v1 = knife_split_edge(kcd, lh1->kfe, lh1->hit, lh1->cagehit, &kfe2);
    lh1->v = kfe->v1; /* Record the #KnifeVert for this hit. */
  }
  else {
    BLI_assert(lh1->f);
    kfe->v1 = new_knife_vert(kcd, lh1->hit, lh1->cagehit);
    kfe->v1->ob = lh1->ob;
    kfe->v1->ob_index = lh1->ob_index;
    kfe->v1->is_cut = true;
    kfe->v1->is_face = true;
    knife_append_list(kcd, &kfe->v1->faces, lh1->f);
    lh1->v = kfe->v1; /* Record the #KnifeVert for this hit. */
  }

  if (lh2->v) {
    kfe->v2 = lh2->v;
  }
  else if (lh2->kfe) {
    kfe->v2 = knife_split_edge(kcd, lh2->kfe, lh2->hit, lh2->cagehit, &kfe2);
    lh2->v = kfe->v2; /* Future uses of lh2 won't split again. */
  }
  else {
    BLI_assert(lh2->f);
    kfe->v2 = new_knife_vert(kcd, lh2->hit, lh2->cagehit);
    kfe->v2->ob = lh2->ob;
    kfe->v2->ob_index = lh2->ob_index;
    kfe->v2->is_cut = true;
    kfe->v2->is_face = true;
    knife_append_list(kcd, &kfe->v2->faces, lh2->f);
    lh2->v = kfe->v2; /* Record the KnifeVert for this hit. */
  }

  knife_add_to_vert_edges(kcd, kfe);

  if (kfe->basef && !find_ref(&kfe->faces, kfe->basef)) {
    knife_edge_append_face(kcd, kfe, kfe->basef);
  }

  /* Update current undo frame cut count. */
  kcd->undo->cuts++;
}

/* Given a list of KnifeLineHits for one face, sorted by l
 * and then by m, make the required KnifeVerts and
 * KnifeEdges.
 */
static void knife_cut_face(KnifeTool_OpData *kcd, BMFace *f, ListBase *hits)
{
  Ref *r;

  if (BLI_listbase_count_at_most(hits, 2) != 2) {
    return;
  }

  for (r = hits->first; r->next; r = r->next) {
    knife_add_single_cut(kcd, r->ref, r->next->ref, f);
  }
}

static void knife_make_face_cuts(KnifeTool_OpData *kcd, BMesh *bm, BMFace *f, ListBase *kfedges)
{
  KnifeEdge *kfe;
  Ref *ref;
  int edge_array_len = BLI_listbase_count(kfedges);
  int i;

  BMEdge **edge_array = BLI_array_alloca(edge_array, edge_array_len);

  /* Point to knife edges we've created edges in, edge_array aligned. */
  KnifeEdge **kfe_array = BLI_array_alloca(kfe_array, edge_array_len);

  BLI_assert(BLI_gset_len(kcd->edgenet.edge_visit) == 0);

  i = 0;
  for (ref = kfedges->first; ref; ref = ref->next) {
    bool is_new_edge = false;
    kfe = ref->ref;

    if (kfe->is_invalid) {
      continue;
    }

    if (kfe->e == NULL) {
      if (kfe->v1->v && kfe->v2->v) {
        kfe->e = BM_edge_exists(kfe->v1->v, kfe->v2->v);
      }
    }

    if (kfe->e) {
      if (BM_edge_in_face(kfe->e, f)) {
        /* Shouldn't happen, but in this case just ignore. */
        continue;
      }
    }
    else {
      if (kfe->v1->v == NULL) {
        kfe->v1->v = BM_vert_create(bm, kfe->v1->co, NULL, 0);
      }
      if (kfe->v2->v == NULL) {
        kfe->v2->v = BM_vert_create(bm, kfe->v2->co, NULL, 0);
      }
      BLI_assert(kfe->e == NULL);
      kfe->e = BM_edge_create(bm, kfe->v1->v, kfe->v2->v, NULL, 0);
      if (kfe->e) {
        if (kcd->select_result || BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          BM_edge_select_set(bm, kfe->e, true);
        }
        is_new_edge = true;
      }
    }

    BLI_assert(kfe->e);

    if (BLI_gset_add(kcd->edgenet.edge_visit, kfe->e)) {
      kfe_array[i] = is_new_edge ? kfe : 0;
      edge_array[i] = kfe->e;
      i += 1;
    }
  }

  if (i) {
    const int edge_array_len_orig = i;
    edge_array_len = i;

#ifdef USE_NET_ISLAND_CONNECT
    uint edge_array_holes_len;
    BMEdge **edge_array_holes;
    if (BM_face_split_edgenet_connect_islands(bm,
                                              f,
                                              edge_array,
                                              edge_array_len,
                                              true,
                                              kcd->edgenet.arena,
                                              &edge_array_holes,
                                              &edge_array_holes_len))
    {
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        for (i = edge_array_len; i < edge_array_holes_len; i++) {
          BM_edge_select_set(bm, edge_array_holes[i], true);
        }
      }

      edge_array_len = edge_array_holes_len;
      edge_array = edge_array_holes; /* Owned by the arena. */
    }
#endif

    {
      BMFace **face_arr = NULL;
      int face_arr_len;

      BM_face_split_edgenet(bm, f, edge_array, edge_array_len, &face_arr, &face_arr_len);

      if (face_arr) {
        MEM_freeN(face_arr);
      }
    }

    /* Remove dangling edges, not essential - but nice for users. */
    for (i = 0; i < edge_array_len_orig; i++) {
      if (kfe_array[i] == NULL) {
        continue;
      }
      if (BM_edge_is_wire(kfe_array[i]->e)) {
        BM_edge_kill(bm, kfe_array[i]->e);
        kfe_array[i]->e = NULL;
      }
    }

#ifdef USE_NET_ISLAND_CONNECT
    BLI_memarena_clear(kcd->edgenet.arena);
#endif
  }

  BLI_gset_clear(kcd->edgenet.edge_visit, NULL);
}

static int sort_verts_by_dist_cb(void *co_p, const void *cur_a_p, const void *cur_b_p)
{
  const KnifeVert *cur_a = ((const Ref *)cur_a_p)->ref;
  const KnifeVert *cur_b = ((const Ref *)cur_b_p)->ref;
  const float *co = co_p;
  const float a_sq = len_squared_v3v3(co, cur_a->co);
  const float b_sq = len_squared_v3v3(co, cur_b->co);

  if (a_sq < b_sq) {
    return -1;
  }
  if (a_sq > b_sq) {
    return 1;
  }
  return 0;
}

/* Use the network of KnifeEdges and KnifeVerts accumulated to make real BMVerts and BMEdedges. */
static void knife_make_cuts(KnifeTool_OpData *kcd, Object *ob)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BMesh *bm = em->bm;
  KnifeEdge *kfe;
  KnifeVert *kfv;
  BMFace *f;
  BMEdge *e, *enew;
  ListBase *list;
  Ref *ref;
  float pct;
  SmallHashIter hiter;
  BLI_mempool_iter iter;
  SmallHash fhash_, *fhash = &fhash_;
  SmallHash ehash_, *ehash = &ehash_;

  BLI_smallhash_init(fhash);
  BLI_smallhash_init(ehash);

  /* Put list of cutting edges for a face into fhash, keyed by face. */
  BLI_mempool_iternew(kcd->kedges, &iter);
  for (kfe = BLI_mempool_iterstep(&iter); kfe; kfe = BLI_mempool_iterstep(&iter)) {
    if (kfe->is_invalid || kfe->v1->ob != ob) {
      continue;
    }

    /* Select edges that lie directly on the cut. */
    if (kcd->select_result) {
      if (kfe->e && kfe->is_cut) {
        BM_edge_select_set(bm, kfe->e, true);
      }
    }

    f = kfe->basef;
    if (!f || kfe->e) {
      continue;
    }
    list = BLI_smallhash_lookup(fhash, (uintptr_t)f);
    if (!list) {
      list = knife_empty_list(kcd);
      BLI_smallhash_insert(fhash, (uintptr_t)f, list);
    }
    knife_append_list(kcd, list, kfe);
  }

  /* Put list of splitting vertices for an edge into ehash, keyed by edge. */
  BLI_mempool_iternew(kcd->kverts, &iter);
  for (kfv = BLI_mempool_iterstep(&iter); kfv; kfv = BLI_mempool_iterstep(&iter)) {
    if (kfv->v || kfv->is_invalid || kfv->ob != ob) {
      continue; /* Already have a BMVert. */
    }
    for (ref = kfv->edges.first; ref; ref = ref->next) {
      kfe = ref->ref;
      e = kfe->e;
      if (!e) {
        continue;
      }
      list = BLI_smallhash_lookup(ehash, (uintptr_t)e);
      if (!list) {
        list = knife_empty_list(kcd);
        BLI_smallhash_insert(ehash, (uintptr_t)e, list);
      }
      /* There can be more than one kfe in kfv's list with same e. */
      if (!find_ref(list, kfv)) {
        knife_append_list(kcd, list, kfv);
      }
    }
  }

  /* Split bmesh edges where needed. */
  for (list = BLI_smallhash_iternew(ehash, &hiter, (uintptr_t *)&e); list;
       list = BLI_smallhash_iternext(&hiter, (uintptr_t *)&e))
  {
    BLI_listbase_sort_r(list, sort_verts_by_dist_cb, e->v1->co);

    for (ref = list->first; ref; ref = ref->next) {
      kfv = ref->ref;
      pct = line_point_factor_v3(kfv->co, e->v1->co, e->v2->co);
      kfv->v = BM_edge_split(bm, e, e->v1, &enew, pct);
    }
  }

  if (kcd->only_select) {
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  }

  /* Do cuts for each face. */
  for (list = BLI_smallhash_iternew(fhash, &hiter, (uintptr_t *)&f); list;
       list = BLI_smallhash_iternext(&hiter, (uintptr_t *)&f))
  {
    knife_make_face_cuts(kcd, bm, f, list);
  }

  BLI_smallhash_release(fhash);
  BLI_smallhash_release(ehash);
}

/* User has just left-clicked after the first time.
 * Add all knife cuts implied by line from prev to curr.
 * If that line crossed edges then kcd->linehits will be non-NULL.
 * Make all of the KnifeVerts and KnifeEdges implied by this cut.
 */
static void knife_add_cut(KnifeTool_OpData *kcd)
{
  int i;
  GHash *facehits;
  BMFace *f;
  Ref *r;
  GHashIterator giter;
  ListBase *list;

  /* Allocate new undo frame on stack, unless cut is being dragged. */
  if (!kcd->is_drag_undo) {
    kcd->undo = BLI_stack_push_r(kcd->undostack);
    kcd->undo->pos = kcd->prev;
    kcd->undo->cuts = 0;
    kcd->undo->splits = 0;
    kcd->undo->mdata = kcd->mdata;
    kcd->is_drag_undo = true;
  }

  /* Save values for angle drawing calculations. */
  copy_v3_v3(kcd->mdata.cage, kcd->prev.cage);
  copy_v2_v2(kcd->mdata.mval, kcd->prev.mval);
  kcd->mdata.is_stored = true;

  prepare_linehits_for_cut(kcd);
  if (kcd->totlinehit == 0) {
    if (kcd->is_drag_hold == false) {
      kcd->prev = kcd->curr;
    }
    return;
  }

  /* Consider most recent linehit in angle drawing calculations. */
  if (kcd->totlinehit >= 2) {
    copy_v3_v3(kcd->mdata.cage, kcd->linehits[kcd->totlinehit - 2].cagehit);
  }

  /* Make facehits: map face -> list of linehits touching it. */
  facehits = BLI_ghash_ptr_new("knife facehits");
  for (i = 0; i < kcd->totlinehit; i++) {
    KnifeLineHit *lh = &kcd->linehits[i];
    if (lh->f) {
      add_hit_to_facehits(kcd, facehits, lh->f, lh);
    }
    if (lh->v) {
      for (r = lh->v->faces.first; r; r = r->next) {
        add_hit_to_facehits(kcd, facehits, r->ref, lh);
      }
    }
    if (lh->kfe) {
      for (r = lh->kfe->faces.first; r; r = r->next) {
        add_hit_to_facehits(kcd, facehits, r->ref, lh);
      }
    }
  }

  /* NOTE: as following loop progresses, the 'v' fields of
   * the linehits will be filled in (as edges are split or
   * in-face verts are made), so it may be true that both
   * the v and the kfe or f fields will be non-NULL. */
  GHASH_ITER (giter, facehits) {
    f = (BMFace *)BLI_ghashIterator_getKey(&giter);
    list = (ListBase *)BLI_ghashIterator_getValue(&giter);
    knife_cut_face(kcd, f, list);
  }

  /* Set up for next cut. */
  kcd->prev = kcd->curr;

  if (kcd->prev.bmface) {
    /* Was "in face" but now we have a KnifeVert it is snapped to. */
    KnifeLineHit *lh = &kcd->linehits[kcd->totlinehit - 1];
    kcd->prev.vert = lh->v;
    kcd->prev.bmface = NULL;
  }

  if (kcd->is_drag_hold) {
    KnifeLineHit *lh = &kcd->linehits[kcd->totlinehit - 1];
    linehit_to_knifepos(&kcd->prev, lh);
  }

  BLI_ghash_free(facehits, NULL, NULL);
  MEM_freeN(kcd->linehits);
  kcd->linehits = NULL;
  kcd->totlinehit = 0;
}

static void knife_finish_cut(KnifeTool_OpData *kcd)
{
  if (kcd->linehits) {
    MEM_freeN(kcd->linehits);
    kcd->linehits = NULL;
    kcd->totlinehit = 0;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen Line Hits (#knife_find_line_hits)
 * \{ */

/* Record the index in kcd->em->looptris of first looptri triple for a given face,
 * given an index for some triple in that array.
 * This assumes that all of the triangles for a given face are contiguous
 * in that array (as they are by the current tessellation routines).
 * Actually store index + 1 in the hash, because 0 looks like "no entry"
 * to hash lookup routine; will reverse this in the get routine.
 * Doing this lazily rather than all at once for all faces.
 */
static void set_lowest_face_tri(KnifeTool_OpData *kcd, BMEditMesh *em, BMFace *f, int index)
{
  int i;

  if (BLI_ghash_lookup(kcd->facetrimap, f)) {
    return;
  }

  BLI_assert(index >= 0 && index < em->tottri);
  BLI_assert(em->looptris[index][0]->f == f);
  for (i = index - 1; i >= 0; i--) {
    if (em->looptris[i][0]->f != f) {
      i++;
      break;
    }
  }
  if (i == -1) {
    i++;
  }

  BLI_ghash_insert(kcd->facetrimap, f, POINTER_FROM_INT(i + 1));
}

/* This should only be called for faces that have had a lowest face tri set by previous function.
 */
static int get_lowest_face_tri(KnifeTool_OpData *kcd, BMFace *f)
{
  int ans;

  ans = POINTER_AS_INT(BLI_ghash_lookup(kcd->facetrimap, f));
  BLI_assert(ans != 0);
  return ans - 1;
}

/**
 * Find intersection of v1-v2 with face f.
 * Only take intersections that are at least \a face_tol_sq (in screen space) away
 * from other intersection elements.
 * If v1-v2 is coplanar with f, call that "no intersection though
 * it really means "infinite number of intersections".
 * In such a case we should have gotten hits on edges or verts of the face.
 */
static bool knife_ray_intersect_face(KnifeTool_OpData *kcd,
                                     const float s[2],
                                     const float v1[3],
                                     const float v2[3],
                                     Object *ob,
                                     uint ob_index,
                                     BMFace *f,
                                     const float face_tol_sq,
                                     float hit_co[3],
                                     float hit_cageco[3])
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);

  int tottri, tri_i;
  float raydir[3];
  float tri_norm[3], tri_plane[4];
  float se1[2], se2[2];
  float d, lambda;
  BMLoop **tri;
  ListBase *list;
  Ref *ref;
  KnifeEdge *kfe;

  sub_v3_v3v3(raydir, v2, v1);
  normalize_v3(raydir);
  tri_i = get_lowest_face_tri(kcd, f);
  tottri = em->tottri;
  BLI_assert(tri_i >= 0 && tri_i < tottri);

  for (; tri_i < tottri; tri_i++) {
    float tri_cos[3][3];
    float ray_tri_uv[2];

    tri = em->looptris[tri_i];
    if (tri[0]->f != f) {
      break;
    }

    knife_bm_tri_cagecos_get_worldspace(kcd, ob_index, tri_i, tri_cos);

    /* Using epsilon test in case ray is directly through an internal
     * tessellation edge and might not hit either tessellation tri with
     * an exact test;
     * We will exclude hits near real edges by a later test. */
    if (isect_ray_tri_epsilon_v3(v1, raydir, UNPACK3(tri_cos), &lambda, ray_tri_uv, KNIFE_FLT_EPS))
    {
      /* Check if line coplanar with tri. */
      normal_tri_v3(tri_norm, UNPACK3(tri_cos));
      plane_from_point_normal_v3(tri_plane, tri_cos[0], tri_norm);
      if ((dist_squared_to_plane_v3(v1, tri_plane) < KNIFE_FLT_EPS) &&
          (dist_squared_to_plane_v3(v2, tri_plane) < KNIFE_FLT_EPS))
      {
        return false;
      }
      interp_v3_v3v3v3_uv(hit_cageco, UNPACK3(tri_cos), ray_tri_uv);
      /* Now check that far enough away from verts and edges. */
      list = knife_get_face_kedges(kcd, ob, ob_index, f);
      for (ref = list->first; ref; ref = ref->next) {
        kfe = ref->ref;
        if (kfe->is_invalid) {
          continue;
        }
        knife_project_v2(kcd, kfe->v1->cageco, se1);
        knife_project_v2(kcd, kfe->v2->cageco, se2);
        d = dist_squared_to_line_segment_v2(s, se1, se2);
        if (d < face_tol_sq) {
          return false;
        }
      }
      interp_v3_v3v3v3_uv(hit_co, tri[0]->v->co, tri[1]->v->co, tri[2]->v->co, ray_tri_uv);
      return true;
    }
  }
  return false;
}

/**
 * Calculate the center and maximum excursion of mesh.
 * (Considers all meshes in multi-object edit mode)
 */
static void calc_ortho_extent(KnifeTool_OpData *kcd)
{
  Object *ob;
  BMEditMesh *em;
  BMIter iter;
  BMVert *v;
  float min[3], max[3];
  float ws[3];
  INIT_MINMAX(min, max);

  for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
    ob = kcd->objects[ob_index];
    em = BKE_editmesh_from_object(ob);

    const float(*cagecos)[3] = kcd->objects_info[ob_index].cagecos;
    if (cagecos) {
      for (int i = 0; i < em->bm->totvert; i++) {
        copy_v3_v3(ws, cagecos[i]);
        mul_m4_v3(ob->object_to_world, ws);
        minmax_v3v3_v3(min, max, ws);
      }
    }
    else {
      BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
        copy_v3_v3(ws, v->co);
        mul_m4_v3(ob->object_to_world, ws);
        minmax_v3v3_v3(min, max, ws);
      }
    }
  }

  kcd->ortho_extent = len_v3v3(min, max) / 2;
  mid_v3_v3v3(kcd->ortho_extent_center, min, max);
}

/* Do edges e1 and e2 go between exactly the same coordinates? */
static bool coinciding_edges(BMEdge *e1, BMEdge *e2)
{
  const float *co11, *co12, *co21, *co22;

  co11 = e1->v1->co;
  co12 = e1->v2->co;
  co21 = e2->v1->co;
  co22 = e2->v2->co;
  if ((equals_v3v3(co11, co21) && equals_v3v3(co12, co22)) ||
      (equals_v3v3(co11, co22) && equals_v3v3(co12, co21)))
  {
    return true;
  }
  return false;
}

/* Callback used in point_is_visible to exclude hits on the faces that are the same
 * as or contain the hitting element (which is in user_data).
 * Also (see #44492) want to exclude hits on faces that butt up to the hitting element
 * (e.g., when you double an edge by an edge split).
 */
static bool bm_ray_cast_cb_elem_not_in_face_check(BMFace *f, void *user_data)
{
  bool ans;
  BMEdge *e, *e2;
  BMIter iter;

  switch (((BMElem *)user_data)->head.htype) {
    case BM_FACE:
      ans = (BMFace *)user_data != f;
      break;
    case BM_EDGE:
      e = (BMEdge *)user_data;
      ans = !BM_edge_in_face(e, f);
      if (ans) {
        /* Is it a boundary edge, coincident with a split edge? */
        if (BM_edge_is_boundary(e)) {
          BM_ITER_ELEM (e2, &iter, f, BM_EDGES_OF_FACE) {
            if (coinciding_edges(e, e2)) {
              ans = false;
              break;
            }
          }
        }
      }
      break;
    case BM_VERT:
      ans = !BM_vert_in_face((BMVert *)user_data, f);
      break;
    default:
      ans = true;
      break;
  }
  return ans;
}

/**
 * Check if \a p is visible (not clipped, not occluded by another face).
 * s in screen projection of p.
 *
 * \param ele_test: Optional vert/edge/face to use when \a p is on the surface of the geometry,
 * intersecting faces matching this face (or connected when an vert/edge) will be ignored.
 */
static bool point_is_visible(KnifeTool_OpData *kcd,
                             const float p[3],
                             const float s[2],
                             BMElem *ele_test)
{
  BMFace *f_hit;

  /* If box clipping on, make sure p is not clipped. */
  if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d) &&
      ED_view3d_clipping_test(kcd->vc.rv3d, p, false))
  {
    return false;
  }

  /* If not cutting through, make sure no face is in front of p. */
  if (!kcd->cut_through) {
    float dist;
    float view[3], p_ofs[3];

    /* TODO: I think there's a simpler way to get the required raycast ray. */
    ED_view3d_unproject_v3(kcd->vc.region, s[0], s[1], 0.0f, view);

    /* Make p_ofs a little towards view, so ray doesn't hit p's face. */
    sub_v3_v3(view, p);
    dist = normalize_v3(view);
    copy_v3_v3(p_ofs, p);

    /* Avoid projecting behind the viewpoint. */
    if (kcd->is_ortho && (kcd->vc.rv3d->persp != RV3D_CAMOB)) {
      dist = kcd->vc.v3d->clip_end * 2.0f;
    }

    if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
      float view_clip[2][3];
      /* NOTE: view_clip[0] should never get clipped. */
      copy_v3_v3(view_clip[0], p_ofs);
      madd_v3_v3v3fl(view_clip[1], p_ofs, view, dist);

      if (clip_segment_v3_plane_n(
              view_clip[0], view_clip[1], kcd->vc.rv3d->clip_local, 6, view_clip[0], view_clip[1]))
      {
        dist = len_v3v3(p_ofs, view_clip[1]);
      }
    }

    /* See if there's a face hit between p1 and the view. */
    if (ele_test) {
      f_hit = knife_bvh_raycast_filter(kcd,
                                       p_ofs,
                                       view,
                                       KNIFE_FLT_EPS,
                                       &dist,
                                       NULL,
                                       NULL,
                                       NULL,
                                       bm_ray_cast_cb_elem_not_in_face_check,
                                       ele_test);
    }
    else {
      f_hit = knife_bvh_raycast(kcd, p_ofs, view, KNIFE_FLT_EPS, &dist, NULL, NULL, NULL);
    }

    if (f_hit) {
      return false;
    }
  }

  return true;
}

/* Clip the line (v1, v2) to planes perpendicular to it and distances d from
 * the closest point on the line to the origin. */
static void clip_to_ortho_planes(float v1[3], float v2[3], const float center[3], const float d)
{
  float closest[3], dir[3];

  sub_v3_v3v3(dir, v1, v2);
  normalize_v3(dir);

  /* could be v1 or v2 */
  sub_v3_v3(v1, center);
  project_plane_normalized_v3_v3v3(closest, v1, dir);
  add_v3_v3(closest, center);

  madd_v3_v3v3fl(v1, closest, dir, d);
  madd_v3_v3v3fl(v2, closest, dir, -d);
}

static void set_linehit_depth(KnifeTool_OpData *kcd, KnifeLineHit *lh)
{
  lh->m = dot_m4_v3_row_z(kcd->vc.rv3d->persmatob, lh->cagehit);
}

/* Finds visible (or all, if cutting through) edges that intersects the current screen drag line.
 */
static void knife_find_line_hits(KnifeTool_OpData *kcd)
{
  SmallHash faces, kfes, kfvs, fobs;
  float v1[3], v2[3], v3[3], v4[3], s1[2], s2[2];
  int *results, *result;
  BMLoop **ls;
  BMFace *f;
  KnifeEdge *kfe;
  KnifeVert *v;
  ListBase *list;
  Ref *ref;
  KnifeLineHit *linehits = NULL;
  BLI_array_declare(linehits);
  SmallHashIter hiter;
  KnifeLineHit hit;
  void *val;
  void **val_p;
  float s[2], se1[2], se2[2], sint[2];
  float r1[3], r2[3];
  float d1, d2, lambda;
  float vert_tol, vert_tol_sq;
  float line_tol, line_tol_sq;
  float face_tol, face_tol_sq;
  uint tot;
  int i;

  if (kcd->linehits) {
    MEM_freeN(kcd->linehits);
    kcd->linehits = NULL;
    kcd->totlinehit = 0;
  }

  copy_v3_v3(v1, kcd->prev.cage);
  copy_v3_v3(v2, kcd->curr.cage);

  /* Project screen line's 3d coordinates back into 2d. */
  knife_project_v2(kcd, v1, s1);
  knife_project_v2(kcd, v2, s2);

  if (kcd->is_interactive) {
    if (len_squared_v2v2(s1, s2) < 1.0f) {
      return;
    }
  }
  else {
    if (len_squared_v2v2(s1, s2) < KNIFE_FLT_EPS_SQUARED) {
      return;
    }
  }

  /* Unproject screen line. */
  ED_view3d_win_to_segment_clipped(kcd->vc.depsgraph, kcd->region, kcd->vc.v3d, s1, v1, v3, true);
  ED_view3d_win_to_segment_clipped(kcd->vc.depsgraph, kcd->region, kcd->vc.v3d, s2, v2, v4, true);

  /* Numeric error, 'v1' -> 'v2', 'v2' -> 'v4'
   * can end up being ~2000 units apart with an orthogonal perspective.
   *
   * (from ED_view3d_win_to_segment_clipped() above)
   * This gives precision error; rather than solving properly
   * (which may involve using doubles everywhere!),
   * limit the distance between these points. */
  if (kcd->is_ortho && (kcd->vc.rv3d->persp != RV3D_CAMOB)) {
    if (kcd->ortho_extent == 0.0f) {
      calc_ortho_extent(kcd);
    }
    clip_to_ortho_planes(v1, v3, kcd->ortho_extent_center, kcd->ortho_extent + 10.0f);
    clip_to_ortho_planes(v2, v4, kcd->ortho_extent_center, kcd->ortho_extent + 10.0f);
  }

  float plane[4];
  {
    float v1_v2[3], v1_v3[3];
    sub_v3_v3v3(v1_v2, v2, v1);
    sub_v3_v3v3(v1_v3, v3, v1);
    cross_v3_v3v3(plane, v1_v2, v1_v3);
    plane_from_point_normal_v3(plane, v1, plane);
  }

  /* First use BVH tree to find faces, knife edges, and knife verts that might
   * intersect the cut plane with rays v1-v3 and v2-v4.
   * This de-duplicates the candidates before doing more expensive intersection tests. */

  results = BLI_bvhtree_intersect_plane(kcd->bvh.tree, plane, &tot);
  if (!results) {
    return;
  }

  BLI_smallhash_init(&faces);
  BLI_smallhash_init(&kfes);
  BLI_smallhash_init(&kfvs);
  BLI_smallhash_init(&fobs);

  Object *ob;
  BMEditMesh *em;

  for (i = 0, result = results; i < tot; i++, result++) {
    uint ob_index = 0;
    for (ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
      ob = kcd->objects[ob_index];
      em = BKE_editmesh_from_object(ob);
      if (*result >= 0 && *result < em->tottri) {
        ls = (BMLoop **)em->looptris[*result];
        break;
      }
      *result -= em->tottri;
    }

    f = ls[0]->f;
    set_lowest_face_tri(kcd, em, f, *result);

    /* Occlude but never cut unselected faces (when only_select is used). */
    if (kcd->only_select && !BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      continue;
    }
    /* For faces, store index of lowest hit looptri in hash. */
    if (BLI_smallhash_haskey(&faces, (uintptr_t)f)) {
      continue;
    }
    /* Don't care what the value is except that it is non-NULL, for iterator. */
    BLI_smallhash_insert(&faces, (uintptr_t)f, f);
    BLI_smallhash_insert(&fobs, (uintptr_t)f, (void *)(uintptr_t)ob_index);

    list = knife_get_face_kedges(kcd, ob, ob_index, f);
    for (ref = list->first; ref; ref = ref->next) {
      kfe = ref->ref;
      if (kfe->is_invalid) {
        continue;
      }
      if (BLI_smallhash_haskey(&kfes, (uintptr_t)kfe)) {
        continue;
      }
      BLI_smallhash_insert(&kfes, (uintptr_t)kfe, kfe);
      v = kfe->v1;
      BLI_smallhash_reinsert(&kfvs, (uintptr_t)v, v);
      v = kfe->v2;
      BLI_smallhash_reinsert(&kfvs, (uintptr_t)v, v);
    }
  }

  /* Now go through the candidates and find intersections. */
  /* These tolerances, in screen space, are for intermediate hits,
   * as ends are already snapped to screen. */

  if (kcd->is_interactive) {
    vert_tol = KNIFE_FLT_EPS_PX_VERT;
    line_tol = KNIFE_FLT_EPS_PX_EDGE;
    face_tol = KNIFE_FLT_EPS_PX_FACE;
  }
  else {
    /* Use 1/100th of a pixel, see #43896 (too big), #47910 (too small).
     *
     * Update, leave this as is until we investigate not using pixel coords
     * for geometry calculations: #48023. */
    vert_tol = line_tol = face_tol = 0.5f;
  }

  vert_tol_sq = vert_tol * vert_tol;
  line_tol_sq = line_tol * line_tol;
  face_tol_sq = face_tol * face_tol;

  /* Assume these tolerances swamp floating point rounding errors in calculations below. */

  /* First look for vertex hits. */
  for (val_p = BLI_smallhash_iternew_p(&kfvs, &hiter, (uintptr_t *)&v); val_p;
       val_p = BLI_smallhash_iternext_p(&hiter, (uintptr_t *)&v))
  {
    KnifeEdge *kfe_hit = NULL;

    bool kfv_is_in_cut = false;
    if (ELEM(v, kcd->prev.vert, kcd->curr.vert)) {
      /* This KnifeVert was captured by the snap system.
       * Since the tolerance distance can be different, add this vertex directly.
       * Otherwise, the cut may fail or a close cut on a connected edge can be performed. */
      bm_elem_from_knife_vert(v, &kfe_hit);
      copy_v2_v2(s, (v == kcd->prev.vert) ? kcd->prev.mval : kcd->curr.mval);
      kfv_is_in_cut = true;
    }
    else {
      knife_project_v2(kcd, v->cageco, s);
      float d = dist_squared_to_line_segment_v2(s, s1, s2);
      if ((d <= vert_tol_sq) &&
          point_is_visible(kcd, v->cageco, s, bm_elem_from_knife_vert(v, &kfe_hit)))
      {
        kfv_is_in_cut = true;
      }
    }

    if (kfv_is_in_cut) {
      memset(&hit, 0, sizeof(hit));
      hit.v = v;

      /* If this isn't from an existing BMVert, it may have been added to a BMEdge originally.
       * Knowing if the hit comes from an edge is important for edge-in-face checks later on.
       * See: #knife_add_single_cut -> #knife_verts_edge_in_face, #42611. */
      if (kfe_hit) {
        hit.kfe = kfe_hit;
      }

      hit.ob = v->ob;
      hit.ob_index = v->ob_index;
      copy_v3_v3(hit.hit, v->co);
      copy_v3_v3(hit.cagehit, v->cageco);
      copy_v2_v2(hit.schit, s);
      set_linehit_depth(kcd, &hit);
      BLI_array_append(linehits, hit);
    }
    else {
      /* This vertex isn't used so remove from `kfvs`.
       * This is useful to detect KnifeEdges that can be skipped.
       * And it optimizes smallhash_iternext a little bit. */
      *val_p = NULL;
    }
  }

  /* Now edge hits; don't add if a vertex at end of edge should have hit. */
  for (val = BLI_smallhash_iternew(&kfes, &hiter, (uintptr_t *)&kfe); val;
       val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&kfe))
  {

    /* If we intersect any of the vertices, don't attempt to intersect the edge. */
    if (BLI_smallhash_lookup(&kfvs, (intptr_t)kfe->v1) ||
        BLI_smallhash_lookup(&kfvs, (intptr_t)kfe->v2))
    {
      continue;
    }

    knife_project_v2(kcd, kfe->v1->cageco, se1);
    knife_project_v2(kcd, kfe->v2->cageco, se2);
    int isect_kind = 1;
    if (kfe == kcd->prev.edge) {
      /* This KnifeEdge was captured by the snap system. */
      copy_v2_v2(sint, kcd->prev.mval);
    }
    else if (kfe == kcd->curr.edge) {
      /* This KnifeEdge was captured by the snap system. */
      copy_v2_v2(sint, kcd->curr.mval);
    }
    else {
      isect_kind = isect_seg_seg_v2_point_ex(s1, s2, se1, se2, 0.0f, sint);
      if (isect_kind == -1) {
        /* isect_seg_seg_v2_point doesn't do tolerance test around ends of s1-s2. */
        closest_to_line_segment_v2(sint, s1, se1, se2);
        if (len_squared_v2v2(sint, s1) <= line_tol_sq) {
          isect_kind = 1;
        }
        else {
          closest_to_line_segment_v2(sint, s2, se1, se2);
          if (len_squared_v2v2(sint, s2) <= line_tol_sq) {
            isect_kind = 1;
          }
        }
      }
    }
    if (isect_kind == 1) {
      d1 = len_v2v2(sint, se1);
      d2 = len_v2v2(se2, se1);
      if (!(d1 <= line_tol || d2 <= line_tol || fabsf(d1 - d2) <= line_tol)) {
        float p_cage[3], p_cage_tmp[3];
        lambda = d1 / d2;
        /* Can't just interpolate between ends of kfe because
         * that doesn't work with perspective transformation.
         * Need to find 3d intersection of ray through sint. */
        knife_input_ray_segment(kcd, sint, 1.0f, r1, r2);
        isect_kind = isect_line_line_v3(
            kfe->v1->cageco, kfe->v2->cageco, r1, r2, p_cage, p_cage_tmp);
        if (isect_kind >= 1 && point_is_visible(kcd, p_cage, sint, bm_elem_from_knife_edge(kfe))) {
          memset(&hit, 0, sizeof(hit));
          if (kcd->snap_midpoints) {
            /* Choose intermediate point snap too. */
            mid_v3_v3v3(p_cage, kfe->v1->cageco, kfe->v2->cageco);
            mid_v2_v2v2(sint, se1, se2);
            lambda = 0.5f;
          }
          hit.kfe = kfe;
          transform_point_by_seg_v3(
              hit.hit, p_cage, kfe->v1->co, kfe->v2->co, kfe->v1->cageco, kfe->v2->cageco);
          hit.ob = kfe->v1->ob;
          hit.ob_index = kfe->v1->ob_index;
          copy_v3_v3(hit.cagehit, p_cage);
          copy_v2_v2(hit.schit, sint);
          hit.perc = lambda;
          set_linehit_depth(kcd, &hit);
          BLI_array_append(linehits, hit);
        }
      }
    }
  }

  /* Now face hits; don't add if a vertex or edge in face should have hit. */
  const bool use_hit_prev = (kcd->prev.vert == NULL) && (kcd->prev.edge == NULL);
  const bool use_hit_curr = (kcd->curr.vert == NULL) && (kcd->curr.edge == NULL) &&
                            !kcd->is_drag_hold;
  if (use_hit_prev || use_hit_curr) {
    for (val = BLI_smallhash_iternew(&faces, &hiter, (uintptr_t *)&f); val;
         val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&f))
    {
      float p[3], p_cage[3];

      uint ob_index = (uint)(uintptr_t)BLI_smallhash_lookup(&fobs, (uintptr_t)f);
      ob = kcd->objects[ob_index];

      if (use_hit_prev &&
          knife_ray_intersect_face(kcd, s1, v1, v3, ob, ob_index, f, face_tol_sq, p, p_cage))
      {
        if (point_is_visible(kcd, p_cage, s1, (BMElem *)f)) {
          memset(&hit, 0, sizeof(hit));
          hit.f = f;
          hit.ob = ob;
          hit.ob_index = ob_index;
          copy_v3_v3(hit.hit, p);
          copy_v3_v3(hit.cagehit, p_cage);
          copy_v2_v2(hit.schit, s1);
          set_linehit_depth(kcd, &hit);
          BLI_array_append(linehits, hit);
        }
      }

      if (use_hit_curr &&
          knife_ray_intersect_face(kcd, s2, v2, v4, ob, ob_index, f, face_tol_sq, p, p_cage))
      {
        if (point_is_visible(kcd, p_cage, s2, (BMElem *)f)) {
          memset(&hit, 0, sizeof(hit));
          hit.f = f;
          hit.ob = ob;
          hit.ob_index = ob_index;
          copy_v3_v3(hit.hit, p);
          copy_v3_v3(hit.cagehit, p_cage);
          copy_v2_v2(hit.schit, s2);
          set_linehit_depth(kcd, &hit);
          BLI_array_append(linehits, hit);
        }
      }
    }
  }

  kcd->linehits = linehits;
  kcd->totlinehit = BLI_array_len(linehits);

  /* Find position along screen line, used for sorting. */
  for (i = 0; i < kcd->totlinehit; i++) {
    KnifeLineHit *lh = kcd->linehits + i;

    lh->l = len_v2v2(lh->schit, s1) / len_v2v2(s2, s1);
  }

  BLI_smallhash_release(&faces);
  BLI_smallhash_release(&kfes);
  BLI_smallhash_release(&kfvs);
  BLI_smallhash_release(&fobs);
  MEM_freeN(results);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KnifePosData Utils
 * \{ */

static void knife_pos_data_clear(KnifePosData *kpd)
{
  zero_v3(kpd->co);
  zero_v3(kpd->cage);
  kpd->vert = NULL;
  kpd->edge = NULL;
  kpd->bmface = NULL;
  zero_v2(kpd->mval);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snapping (#knife_snap_update_from_mval)
 * \{ */

static BMFace *knife_find_closest_face(KnifeTool_OpData *kcd,
                                       Object **r_ob,
                                       uint *r_ob_index,
                                       bool *is_space,
                                       float r_co[3],
                                       float r_cageco[3])
{
  BMFace *f;
  float dist = KMAXDIST;
  float origin[3];
  float origin_ofs[3];
  float ray[3], ray_normal[3];

  /* Unproject to find view ray. */
  knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);
  sub_v3_v3v3(ray, origin_ofs, origin);
  normalize_v3_v3(ray_normal, ray);

  f = knife_bvh_raycast(kcd, origin, ray_normal, 0.0f, NULL, r_co, r_cageco, r_ob_index);

  if (f && kcd->only_select && BM_elem_flag_test(f, BM_ELEM_SELECT) == 0) {
    f = NULL;
  }

  if (is_space) {
    *is_space = !f;
  }

  if (f) {
    *r_ob = kcd->objects[*r_ob_index];
  }
  else {
    if (kcd->is_interactive) {
      /* Try to use back-buffer selection method if ray casting failed.
       *
       * Apply the mouse coordinates to a copy of the view-context
       * since we don't want to rely on this being set elsewhere. */
      ViewContext vc = kcd->vc;
      vc.mval[0] = (int)kcd->curr.mval[0];
      vc.mval[1] = (int)kcd->curr.mval[1];

      f = EDBM_face_find_nearest(&vc, &dist);

      /* Cheat for now; just put in the origin instead
       * of a true coordinate on the face.
       * This just puts a point 1.0f in front of the view. */
      add_v3_v3v3(r_co, origin, ray);
      /* Use this value for the cage location too as it's used to find near edges/vertices. */
      copy_v3_v3(r_cageco, r_co);
    }
  }

  return f;
}

/**
 * Find the 2d screen space density of vertices within a radius.
 * Used to scale snapping distance for picking edges/verts.
 *
 * Arguments `f` and `cageco` should be the result of a call to #knife_find_closest_face.
 */
static int knife_sample_screen_density_from_closest_face(KnifeTool_OpData *kcd,
                                                         const float radius,
                                                         Object *ob,
                                                         uint ob_index,
                                                         BMFace *f,
                                                         const float cageco[3])
{
  const float radius_sq = radius * radius;
  ListBase *list;
  Ref *ref;
  float sco[2];
  float dis_sq;
  int c = 0;

  knife_project_v2(kcd, cageco, sco);

  list = knife_get_face_kedges(kcd, ob, ob_index, f);
  for (ref = list->first; ref; ref = ref->next) {
    KnifeEdge *kfe = ref->ref;
    int i;

    if (kfe->is_invalid) {
      continue;
    }

    for (i = 0; i < 2; i++) {
      KnifeVert *kfv = i ? kfe->v2 : kfe->v1;
      float kfv_sco[2];

      if (kfv->is_invalid) {
        continue;
      }

      knife_project_v2(kcd, kfv->cageco, kfv_sco);

      dis_sq = len_squared_v2v2(kfv_sco, sco);
      if (dis_sq < radius_sq) {
        if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
          if (ED_view3d_clipping_test(kcd->vc.rv3d, kfv->cageco, false) == 0) {
            c++;
          }
        }
        else {
          c++;
        }
      }
    }
  }

  return c;
}

/**
 * \return the snapping distance for edges/verts, scaled by the density of the
 * surrounding mesh (in screen space).
 *
 * \note Face values in `kcd->curr` must be up to date.
 */
static float knife_snap_size(KnifeTool_OpData *kcd, float maxsize)
{
  BLI_assert(kcd->is_interactive == true);
  int density = 0;

  if (!kcd->curr.is_space) {
    density = (float)knife_sample_screen_density_from_closest_face(
        kcd, maxsize * 2.0f, kcd->curr.ob, kcd->curr.ob_index, kcd->curr.bmface, kcd->curr.cage);
  }

  return density ? min_ff(maxsize / ((float)density * 0.5f), maxsize) : maxsize;
}

/* Snap to edge when in a constrained mode.
 * Returns 'lambda' calculated (in screen-space). */
static bool knife_snap_edge_constrained(KnifeTool_OpData *kcd,
                                        const float sco[3],
                                        const float kfv1_sco[2],
                                        const float kfv2_sco[2],
                                        float *r_dist_sq,
                                        float *r_lambda)
{
  /* If snapping, check we're in bounds. */
  float sco_snap[2];
  isect_line_line_v2_point(kfv1_sco, kfv2_sco, kcd->prev.mval, kcd->curr.mval, sco_snap);
  float lambda = line_point_factor_v2(sco_snap, kfv1_sco, kfv2_sco);

  /* Be strict when constrained within edge. */
  if ((lambda < 0.0f - KNIFE_FLT_EPSBIG) || (lambda > 1.0f + KNIFE_FLT_EPSBIG)) {
    return false;
  }

  float dis_sq = len_squared_v2v2(sco, sco_snap);
  if (dis_sq < *r_dist_sq) {
    *r_dist_sq = dis_sq;
    *r_lambda = lambda;
    return true;
  }
  return false;
}

/* Use when lambda is in screen-space. */
static void knife_interp_v3_v3v3(const KnifeTool_OpData *kcd,
                                 float r_co[3],
                                 const float v1[3],
                                 const float v2[3],
                                 float lambda_ss)
{
  if (kcd->is_ortho) {
    interp_v3_v3v3(r_co, v1, v2, lambda_ss);
  }
  else {
    /* Transform into screen-space, interp, then transform back. */
    float v1_ss[3], v2_ss[3];

    mul_v3_project_m4_v3(v1_ss, (float(*)[4])kcd->vc.rv3d->persmat, v1);
    mul_v3_project_m4_v3(v2_ss, (float(*)[4])kcd->vc.rv3d->persmat, v2);

    interp_v3_v3v3(r_co, v1_ss, v2_ss, lambda_ss);

    mul_project_m4_v3((float(*)[4])kcd->vc.rv3d->persinv, r_co);
  }
}

/* p is closest point on edge to the mouse cursor. */
static KnifeEdge *knife_find_closest_edge_of_face(
    KnifeTool_OpData *kcd, Object *ob, uint ob_index, BMFace *f, float p[3], float cagep[3])
{
  float sco[2];
  float maxdist;

  if (kcd->is_interactive) {
    maxdist = knife_snap_size(kcd, kcd->ethresh);

    if (kcd->ignore_vert_snapping) {
      maxdist *= 0.5f;
    }
  }
  else {
    maxdist = KNIFE_FLT_EPS;
  }

  const float maxdist_sq = maxdist * maxdist;
  KnifeEdge *cure = NULL;
  float cur_cagep[3];
  ListBase *list;
  Ref *ref;
  float dis_sq, curdis_sq = maxdist_sq;

  knife_project_v2(kcd, cagep, sco);

  /* Look through all edges associated with this face. */
  list = knife_get_face_kedges(kcd, ob, ob_index, f);
  for (ref = list->first; ref; ref = ref->next) {
    KnifeEdge *kfe = ref->ref;
    float kfv1_sco[2], kfv2_sco[2], test_cagep[3];
    float lambda;

    if (kfe->is_invalid) {
      continue;
    }

    /* Project edge vertices into screen space. */
    knife_project_v2(kcd, kfe->v1->cageco, kfv1_sco);
    knife_project_v2(kcd, kfe->v2->cageco, kfv2_sco);

    /* Check if we're close enough and calculate 'lambda'. */
    /* In constrained mode calculate lambda differently, unless constrained along kcd->prev.edge */
    if ((kcd->is_angle_snapping || kcd->axis_constrained) && (kfe != kcd->prev.edge) &&
        (kcd->mode == MODE_DRAGGING))
    {
      dis_sq = curdis_sq;
      if (!knife_snap_edge_constrained(kcd, sco, kfv1_sco, kfv2_sco, &dis_sq, &lambda)) {
        continue;
      }
    }
    else {
      dis_sq = dist_squared_to_line_segment_v2(sco, kfv1_sco, kfv2_sco);
      if (dis_sq < curdis_sq) {
        lambda = line_point_factor_v2(sco, kfv1_sco, kfv2_sco);
      }
      else {
        continue;
      }
    }

    /* Now we have 'lambda' calculated (in screen-space). */
    knife_interp_v3_v3v3(kcd, test_cagep, kfe->v1->cageco, kfe->v2->cageco, lambda);

    if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
      /* Check we're in the view */
      if (ED_view3d_clipping_test(kcd->vc.rv3d, test_cagep, false)) {
        continue;
      }
    }

    cure = kfe;
    curdis_sq = dis_sq;
    copy_v3_v3(cur_cagep, test_cagep);
  }

  if (cure && !kcd->ignore_edge_snapping) {
    KnifeVert *edgesnap = NULL;

    if (kcd->snap_midpoints) {
      mid_v3_v3v3(p, cure->v1->co, cure->v2->co);
      mid_v3_v3v3(cagep, cure->v1->cageco, cure->v2->cageco);
    }
    else {
      float lambda = line_point_factor_v3(cur_cagep, cure->v1->cageco, cure->v2->cageco);
      copy_v3_v3(cagep, cur_cagep);
      interp_v3_v3v3(p, cure->v1->co, cure->v2->co, lambda);
    }

    /* Update mouse coordinates to the snapped-to edge's screen coordinates
     * this is important for angle snap, which uses the previous mouse position. */
    edgesnap = new_knife_vert(kcd, p, cagep);
    edgesnap->ob = ob;
    edgesnap->ob_index = ob_index;
    knife_project_v2(kcd, edgesnap->cageco, kcd->curr.mval);
  }

  return cure;
}

/* Find a vertex near the mouse cursor, if it exists. */
static KnifeVert *knife_find_closest_vert_of_edge(KnifeTool_OpData *kcd,
                                                  KnifeEdge *kfe,
                                                  float p[3],
                                                  float cagep[3])
{
  float sco[2];
  float maxdist;

  if (kcd->is_interactive) {
    maxdist = knife_snap_size(kcd, kcd->vthresh);
    if (kcd->ignore_vert_snapping) {
      maxdist *= 0.5f;
    }
  }
  else {
    maxdist = KNIFE_FLT_EPS;
  }

  const float maxdist_sq = maxdist * maxdist;
  KnifeVert *curv = NULL;
  float cur_kfv_sco[2];
  float dis_sq, curdis_sq = FLT_MAX;

  knife_project_v2(kcd, cagep, sco);

  for (int i = 0; i < 2; i++) {
    KnifeVert *kfv = i ? kfe->v2 : kfe->v1;
    float kfv_sco[2];

    knife_project_v2(kcd, kfv->cageco, kfv_sco);

    /* Be strict when in a constrained mode, the vertex needs to be very close to the cut line,
     * or we ignore. */
    if ((kcd->is_angle_snapping || kcd->axis_constrained) && (kcd->mode == MODE_DRAGGING)) {
      if (dist_squared_to_line_segment_v2(kfv_sco, kcd->prev.mval, kcd->curr.mval) >
          KNIFE_FLT_EPSBIG) {
        continue;
      }
    }

    dis_sq = len_squared_v2v2(kfv_sco, sco);
    if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
      if (!RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d) ||
          !ED_view3d_clipping_test(kcd->vc.rv3d, kfv->cageco, false))
      {
        curv = kfv;
        curdis_sq = dis_sq;
        copy_v2_v2(cur_kfv_sco, kfv_sco);
      }
    }
  }

  if (curv && !kcd->ignore_vert_snapping) {
    copy_v3_v3(p, curv->co);
    copy_v3_v3(cagep, curv->cageco);

    /* Update mouse coordinates to the snapped-to vertex's screen coordinates
     * this is important for angle snap, which uses the previous mouse position. */
    copy_v2_v2(kcd->curr.mval, cur_kfv_sco);
  }

  return curv;
}

/**
 * Snaps a 2d vector to an angle, relative to \a v_ref.
 */
static float snap_v2_angle(float r[2], const float v[2], const float v_ref[2], float angle_snap)
{
  float m2[2][2];
  float v_unit[2];
  float angle, angle_delta;

  BLI_ASSERT_UNIT_V2(v_ref);

  normalize_v2_v2(v_unit, v);
  angle = angle_signed_v2v2(v_unit, v_ref);
  angle_delta = (roundf(angle / angle_snap) * angle_snap) - angle;
  angle_to_mat2(m2, angle_delta);

  mul_v2_m2v2(r, m2, v);
  return angle + angle_delta;
}

/* Update both kcd->curr.mval and kcd->mval to snap to required angle. */
static bool knife_snap_angle_screen(KnifeTool_OpData *kcd)
{
  const float dvec_ref[2] = {1.0f, 0.0f};
  float dvec[2], dvec_snap[2];

  float snap_step;
  /* Currently user can input any float between 0 and 180. */
  if (kcd->angle_snapping_increment > KNIFE_MIN_ANGLE_SNAPPING_INCREMENT &&
      kcd->angle_snapping_increment <= KNIFE_MAX_ANGLE_SNAPPING_INCREMENT)
  {
    snap_step = DEG2RADF(kcd->angle_snapping_increment);
  }
  else {
    snap_step = DEG2RADF(KNIFE_DEFAULT_ANGLE_SNAPPING_INCREMENT);
  }

  sub_v2_v2v2(dvec, kcd->curr.mval, kcd->prev.mval);
  if (is_zero_v2(dvec)) {
    return false;
  }

  kcd->angle = snap_v2_angle(dvec_snap, dvec, dvec_ref, snap_step);

  add_v2_v2v2(kcd->curr.mval, kcd->prev.mval, dvec_snap);

  copy_v2_v2(kcd->mval, kcd->curr.mval);

  return true;
}

/**
 * Snaps a 3d vector to an angle, relative to \a v_ref, along the plane with normal \a plane_no.
 */
static float snap_v3_angle_plane(
    float r[3], const float v[3], const float v_ref[3], const float plane_no[3], float snap_step)
{
  /* Calculate angle between current cut vector and reference vector. */
  float angle, angle_delta;
  angle = angle_signed_on_axis_v3v3_v3(v, v_ref, plane_no);
  /* Use this to calculate the angle to rotate by based on snap_step. */
  angle_delta = (roundf(angle / snap_step) * snap_step) - angle;

  /* Snap to angle. */
  rotate_v3_v3v3fl(r, v, plane_no, angle_delta);
  return angle + angle_delta;
}

/* Snap to required angle along the plane of the face nearest to kcd->prev. */
static bool knife_snap_angle_relative(KnifeTool_OpData *kcd)
{
  Ref *ref;
  KnifeEdge *kfe;
  KnifeVert *kfv;
  BMFace *f;
  float refv[3];

  /* Ray for kcd->curr. */
  float curr_origin[3];
  float curr_origin_ofs[3];
  float curr_ray[3], curr_ray_normal[3];
  float curr_co[3], curr_cage[3]; /* Unused. */

  float plane[4];
  float ray_hit[3];
  float lambda;

  knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, curr_origin, curr_origin_ofs);
  sub_v3_v3v3(curr_ray, curr_origin_ofs, curr_origin);
  normalize_v3_v3(curr_ray_normal, curr_ray);

  BMFace *fcurr = knife_bvh_raycast(
      kcd, curr_origin, curr_ray_normal, 0.0f, NULL, curr_co, curr_cage, NULL);

  if (!fcurr) {
    return false;
  }

  /* Calculate a reference vector using previous cut segment, edge or vertex.
   * If none exists then exit. */
  if (kcd->prev.vert) {
    int count = 0;
    for (ref = kcd->prev.vert->edges.first; ref; ref = ref->next) {
      kfe = ((KnifeEdge *)(ref->ref));
      if (kfe->is_invalid) {
        continue;
      }
      if (kfe->e) {
        if (!BM_edge_in_face(kfe->e, fcurr)) {
          continue;
        }
      }
      if (count == kcd->snap_edge) {
        kfv = compare_v3v3(kfe->v1->cageco, kcd->prev.cage, KNIFE_FLT_EPSBIG) ? kfe->v2 : kfe->v1;
        sub_v3_v3v3(refv, kfv->cageco, kcd->prev.cage);
        kcd->snap_ref_edge = kfe;
        break;
      }
      count++;
    }
  }
  else if (kcd->prev.edge) {
    kfv = compare_v3v3(kcd->prev.edge->v1->cageco, kcd->prev.cage, KNIFE_FLT_EPSBIG) ?
              kcd->prev.edge->v2 :
              kcd->prev.edge->v1;
    sub_v3_v3v3(refv, kfv->cageco, kcd->prev.cage);
    kcd->snap_ref_edge = kcd->prev.edge;
  }
  else {
    return false;
  }

  /* Choose best face for plane. */
  BMFace *fprev = NULL;
  if (kcd->prev.vert && kcd->prev.vert->v) {
    for (ref = kcd->prev.vert->faces.first; ref; ref = ref->next) {
      f = ((BMFace *)(ref->ref));
      if (f == fcurr) {
        fprev = f;
      }
    }
  }
  else if (kcd->prev.edge) {
    for (ref = kcd->prev.edge->faces.first; ref; ref = ref->next) {
      f = ((BMFace *)(ref->ref));
      if (f == fcurr) {
        fprev = f;
      }
    }
  }
  else {
    /* Cut segment was started in a face. */
    float prev_origin[3];
    float prev_origin_ofs[3];
    float prev_ray[3], prev_ray_normal[3];
    float prev_co[3], prev_cage[3]; /* Unused. */

    knife_input_ray_segment(kcd, kcd->prev.mval, 1.0f, prev_origin, prev_origin_ofs);

    sub_v3_v3v3(prev_ray, prev_origin_ofs, prev_origin);
    normalize_v3_v3(prev_ray_normal, prev_ray);

    /* kcd->prev.face is usually not set. */
    fprev = knife_bvh_raycast(
        kcd, prev_origin, prev_ray_normal, 0.0f, NULL, prev_co, prev_cage, NULL);
  }

  if (!fprev || fprev != fcurr) {
    return false;
  }

  /* Re-calculate current ray in object space. */
  knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, curr_origin, curr_origin_ofs);
  sub_v3_v3v3(curr_ray, curr_origin_ofs, curr_origin);
  normalize_v3_v3(curr_ray_normal, curr_ray);

  plane_from_point_normal_v3(plane, kcd->prev.cage, fprev->no);

  if (isect_ray_plane_v3(curr_origin, curr_ray_normal, plane, &lambda, false)) {
    madd_v3_v3v3fl(ray_hit, curr_origin, curr_ray_normal, lambda);

    /* Calculate snap step. */
    float snap_step;
    if (kcd->angle_snapping_increment > KNIFE_MIN_ANGLE_SNAPPING_INCREMENT &&
        kcd->angle_snapping_increment <= KNIFE_MAX_ANGLE_SNAPPING_INCREMENT)
    {
      snap_step = DEG2RADF(kcd->angle_snapping_increment);
    }
    else {
      snap_step = DEG2RADF(KNIFE_DEFAULT_ANGLE_SNAPPING_INCREMENT);
    }

    float v1[3];
    float v2[3];
    float rotated_vec[3];
    /* Maybe check for vectors being zero here? */
    sub_v3_v3v3(v1, ray_hit, kcd->prev.cage);
    copy_v3_v3(v2, refv);
    kcd->angle = snap_v3_angle_plane(rotated_vec, v1, v2, fprev->no, snap_step);
    add_v3_v3(rotated_vec, kcd->prev.cage);

    knife_project_v2(kcd, rotated_vec, kcd->curr.mval);
    copy_v2_v2(kcd->mval, kcd->curr.mval);
    return true;
  }
  return false;
}

static int knife_calculate_snap_ref_edges(KnifeTool_OpData *kcd)
{
  Ref *ref;
  KnifeEdge *kfe;

  /* Ray for kcd->curr. */
  float curr_origin[3];
  float curr_origin_ofs[3];
  float curr_ray[3], curr_ray_normal[3];
  float curr_co[3], curr_cage[3]; /* Unused. */

  knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, curr_origin, curr_origin_ofs);
  sub_v3_v3v3(curr_ray, curr_origin_ofs, curr_origin);
  normalize_v3_v3(curr_ray_normal, curr_ray);

  BMFace *fcurr = knife_bvh_raycast(
      kcd, curr_origin, curr_ray_normal, 0.0f, NULL, curr_co, curr_cage, NULL);

  int count = 0;

  if (!fcurr) {
    return count;
  }

  if (kcd->prev.vert) {
    for (ref = kcd->prev.vert->edges.first; ref; ref = ref->next) {
      kfe = ((KnifeEdge *)(ref->ref));
      if (kfe->is_invalid) {
        continue;
      }
      if (kfe->e) {
        if (!BM_edge_in_face(kfe->e, fcurr)) {
          continue;
        }
      }
      count++;
    }
  }
  else if (kcd->prev.edge) {
    return 1;
  }
  return count;
}

/* Reset the snapping angle num input. */
static void knife_reset_snap_angle_input(KnifeTool_OpData *kcd)
{
  kcd->num.val[0] = 0;
  while (kcd->num.str_cur > 0) {
    kcd->num.str[kcd->num.str_cur - 1] = '\0';
    kcd->num.str_cur--;
  }
}
/**
 * Constrains the current cut to an axis.
 * If scene orientation is set to anything other than global it takes priority.
 * Otherwise kcd->constrain_axis_mode is used.
 */
static void knife_constrain_axis(KnifeTool_OpData *kcd)
{
  /* Obtain current mouse position in world space. */
  float curr_cage_adjusted[3];
  ED_view3d_win_to_3d(
      kcd->vc.v3d, kcd->region, kcd->prev.cage, kcd->curr.mval, curr_cage_adjusted);

  /* Constrain axes. */
  Scene *scene = kcd->scene;
  ViewLayer *view_layer = kcd->vc.view_layer;
  Object *obedit = (kcd->prev.ob) ? kcd->prev.ob : kcd->vc.obedit;
  RegionView3D *rv3d = kcd->region->regiondata;
  const short scene_orientation = BKE_scene_orientation_get_index(scene, SCE_ORIENT_DEFAULT);
  /* Scene orientation takes priority. */
  const short orientation_type = scene_orientation ? scene_orientation :
                                                     kcd->constrain_axis_mode - 1;
  const int pivot_point = scene->toolsettings->transform_pivot_point;
  float mat[3][3];
  ED_transform_calc_orientation_from_type_ex(
      scene, view_layer, kcd->vc.v3d, rv3d, obedit, obedit, orientation_type, pivot_point, mat);

  /* Apply orientation matrix (can be simplified?). */
  float co[3][3];
  copy_v3_v3(co[0], kcd->prev.cage);
  copy_v3_v3(co[2], curr_cage_adjusted);
  invert_m3(mat);
  mul_m3_m3_pre(co, mat);
  for (int i = 0; i <= 2; i++) {
    if ((kcd->constrain_axis - 1) != i) {
      /* kcd->curr_cage_adjusted[i] = prev_cage_adjusted[i]; */
      co[2][i] = co[0][i];
    }
  }
  invert_m3(mat);
  mul_m3_m3_pre(co, mat);
  copy_v3_v3(kcd->prev.cage, co[0]);
  copy_v3_v3(curr_cage_adjusted, co[2]);

  /* Set mval to closest point on constrained line in screen space. */
  float curr_screenspace[2];
  float prev_screenspace[2];
  knife_project_v2(kcd, curr_cage_adjusted, curr_screenspace);
  knife_project_v2(kcd, kcd->prev.cage, prev_screenspace);
  float intersection[2];
  if (closest_to_line_v2(intersection, kcd->curr.mval, prev_screenspace, curr_screenspace)) {
    copy_v2_v2(kcd->curr.mval, intersection);
  }
  else {
    copy_v2_v2(kcd->curr.mval, curr_screenspace);
  }
  copy_v2_v2(kcd->mval, kcd->curr.mval);
}

/**
 * \return true when `kcd->curr.co` & `kcd->curr.cage` are set.
 *
 * In this case `is_space` is nearly always false.
 * There are some situations when vertex or edge can be snapped to, when `is_space` is true.
 * In this case the selection-buffer is used to select the face,
 * then the closest `vert` or `edge` is set, and those will enable `is_co_set`.
 */
static bool knife_snap_update_from_mval(KnifeTool_OpData *kcd, const float mval[2])
{
  knife_pos_data_clear(&kcd->curr);
  copy_v2_v2(kcd->curr.mval, mval);

  /* view matrix may have changed, reproject */
  knife_project_v2(kcd, kcd->prev.cage, kcd->prev.mval);

  kcd->is_angle_snapping = false;
  if (kcd->mode == MODE_DRAGGING) {
    if (kcd->angle_snapping) {
      if (kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_SCREEN) {
        kcd->is_angle_snapping = knife_snap_angle_screen(kcd);
      }
      else if (kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) {
        kcd->is_angle_snapping = knife_snap_angle_relative(kcd);
        if (kcd->is_angle_snapping) {
          kcd->snap_ref_edges_count = knife_calculate_snap_ref_edges(kcd);
        }
      }
    }

    if (kcd->axis_constrained) {
      knife_constrain_axis(kcd);
    }
  }

  {
    kcd->curr.ob = kcd->vc.obedit;
    kcd->curr.bmface = knife_find_closest_face(kcd,
                                               &kcd->curr.ob,
                                               &kcd->curr.ob_index,
                                               &kcd->curr.is_space,
                                               kcd->curr.co,
                                               kcd->curr.cage);

    if (kcd->curr.bmface) {
      kcd->curr.edge = knife_find_closest_edge_of_face(
          kcd, kcd->curr.ob, kcd->curr.ob_index, kcd->curr.bmface, kcd->curr.co, kcd->curr.cage);
    }

    if (kcd->curr.edge) {
      kcd->curr.vert = knife_find_closest_vert_of_edge(
          kcd, kcd->curr.edge, kcd->curr.co, kcd->curr.cage);

      if (kcd->ignore_edge_snapping) {
        kcd->curr.edge = NULL;
      }
    }
  }

  return kcd->curr.vert || kcd->curr.edge || (kcd->curr.bmface && !kcd->curr.is_space);
}

/**
 * TODO: Undo currently assumes that the most recent cut segment added is
 * the last valid KnifeEdge in the kcd->kedges mempool. This could break in
 * the future so it may be better to store the KnifeEdges for each KnifeUndoFrame
 * on a stack. This stack could then be used instead of iterating over the mempool.
 */
static void knifetool_undo(KnifeTool_OpData *kcd)
{
  Ref *ref;
  KnifeEdge *kfe, *newkfe;
  KnifeEdge *lastkfe = NULL;
  KnifeVert *v1, *v2;
  KnifeUndoFrame *undo;
  BLI_mempool_iter iterkfe;

  undo = BLI_stack_peek(kcd->undostack);

  /* Undo edge splitting. */
  for (int i = 0; i < undo->splits; i++) {
    BLI_stack_pop(kcd->splitstack, &newkfe);
    BLI_stack_pop(kcd->splitstack, &kfe);
    knife_join_edge(newkfe, kfe);
  }

  for (int i = 0; i < undo->cuts; i++) {

    BLI_mempool_iternew(kcd->kedges, &iterkfe);
    for (kfe = BLI_mempool_iterstep(&iterkfe); kfe; kfe = BLI_mempool_iterstep(&iterkfe)) {
      if (!kfe->is_cut || kfe->is_invalid || kfe->splits) {
        continue;
      }
      lastkfe = kfe;
    }

    if (lastkfe) {
      lastkfe->is_invalid = true;

      /* TODO: Are they always guaranteed to be in this order? */
      v1 = lastkfe->v1;
      v2 = lastkfe->v2;

      /* Only remove first vertex if it is the start segment of the cut. */
      if (!v1->is_invalid && !v1->is_splitting) {
        v1->is_invalid = true;
        /* If the first vertex is touching any other cut edges don't remove it. */
        for (ref = v1->edges.first; ref; ref = ref->next) {
          kfe = ref->ref;
          if (kfe->is_cut && !kfe->is_invalid) {
            v1->is_invalid = false;
            break;
          }
        }
      }

      /* Only remove second vertex if it is the end segment of the cut. */
      if (!v2->is_invalid && !v2->is_splitting) {
        v2->is_invalid = true;
        /* If the second vertex is touching any other cut edges don't remove it. */
        for (ref = v2->edges.first; ref; ref = ref->next) {
          kfe = ref->ref;
          if (kfe->is_cut && !kfe->is_invalid) {
            v2->is_invalid = false;
            break;
          }
        }
      }
    }
  }

  if (kcd->mode == MODE_DRAGGING) {
    /* Restore kcd->prev. */
    kcd->prev = undo->pos;
  }

  /* Restore data for distance and angle measurements. */
  kcd->mdata = undo->mdata;

  BLI_stack_discard(kcd->undostack);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #KnifeObjectInfo (#kcd->objects_info) Init and Free
 * \{ */

static void knifetool_init_obinfo(KnifeTool_OpData *kcd,
                                  Object *ob,
                                  uint ob_index,
                                  bool use_tri_indices)
{

  Scene *scene_eval = (Scene *)DEG_get_evaluated_id(kcd->vc.depsgraph, &kcd->scene->id);
  Object *obedit_eval = (Object *)DEG_get_evaluated_id(kcd->vc.depsgraph, &ob->id);
  BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);

  BM_mesh_elem_index_ensure(em_eval->bm, BM_VERT);

  KnifeObjectInfo *obinfo = &kcd->objects_info[ob_index];
  obinfo->em = em_eval;
  obinfo->cagecos = (const float(*)[3])BKE_editmesh_vert_coords_alloc(
      kcd->vc.depsgraph, em_eval, scene_eval, obedit_eval, NULL);

  if (use_tri_indices) {
    BMLoop *(*looptris)[3] = em_eval->looptris;
    int(*tri_indices)[3] = MEM_mallocN(sizeof(int[3]) * em_eval->tottri, __func__);
    for (int i = 0; i < em_eval->tottri; i++) {
      BMLoop **tri = looptris[i];
      tri_indices[i][0] = BM_elem_index_get(tri[0]->v);
      tri_indices[i][1] = BM_elem_index_get(tri[1]->v);
      tri_indices[i][2] = BM_elem_index_get(tri[2]->v);
    }
    obinfo->tri_indices = tri_indices;
  }
}

static void knifetool_free_obinfo(KnifeTool_OpData *kcd, uint ob_index)
{
  MEM_SAFE_FREE(kcd->objects_info[ob_index].cagecos);
  MEM_SAFE_FREE(kcd->objects_info[ob_index].tri_indices);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #KnifeTool_OpData (#op->customdata) Init and Free
 * \{ */

static void knife_init_colors(KnifeColors *colors)
{
  /* Possible BMESH_TODO: add explicit themes or calculate these by
   * figuring out contrasting colors with grid / edges / verts
   * a la UI_make_axis_color. */
  UI_GetThemeColorType3ubv(TH_NURB_VLINE, SPACE_VIEW3D, colors->line);
  UI_GetThemeColorType3ubv(TH_NURB_ULINE, SPACE_VIEW3D, colors->edge);
  UI_GetThemeColorType3ubv(TH_NURB_SEL_ULINE, SPACE_VIEW3D, colors->edge_extra);
  UI_GetThemeColorType3ubv(TH_HANDLE_SEL_VECT, SPACE_VIEW3D, colors->curpoint);
  UI_GetThemeColorType3ubv(TH_HANDLE_SEL_VECT, SPACE_VIEW3D, colors->curpoint_a);
  colors->curpoint_a[3] = 102;
  UI_GetThemeColorType3ubv(TH_ACTIVE_SPLINE, SPACE_VIEW3D, colors->point);
  UI_GetThemeColorType3ubv(TH_ACTIVE_SPLINE, SPACE_VIEW3D, colors->point_a);
  colors->point_a[3] = 102;

  UI_GetThemeColorType3ubv(TH_AXIS_X, SPACE_VIEW3D, colors->xaxis);
  UI_GetThemeColorType3ubv(TH_AXIS_Y, SPACE_VIEW3D, colors->yaxis);
  UI_GetThemeColorType3ubv(TH_AXIS_Z, SPACE_VIEW3D, colors->zaxis);
  UI_GetThemeColorType3ubv(TH_TRANSFORM, SPACE_VIEW3D, colors->axis_extra);
}

/* called when modal loop selection gets set up... */
static void knifetool_init(ViewContext *vc,
                           KnifeTool_OpData *kcd,
                           Object **objects,
                           const int objects_len,
                           const bool only_select,
                           const bool cut_through,
                           const bool xray,
                           const int visible_measurements,
                           const int angle_snapping,
                           const float angle_snapping_increment,
                           const bool is_interactive)
{
  /* Needed so multiple non-interactive cuts (also called knife-project)
   * doesn't access indices of loops that were created by cutting, see: #97153. */
  bool use_tri_indices = !is_interactive;

  kcd->vc = *vc;

  Scene *scene = vc->scene;

  /* Assign the drawing handle for drawing preview line... */
  kcd->scene = scene;
  kcd->region = vc->region;

  if (objects) {
    kcd->objects = objects;
    kcd->objects_len = objects_len;
    kcd->objects_free = false;
  }
  else {
    kcd->objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        vc->scene, vc->view_layer, vc->v3d, &kcd->objects_len);
    kcd->objects_free = true;
  }

  Object *ob;
  BMEditMesh *em;
  kcd->objects_info = MEM_callocN(sizeof(*kcd->objects_info) * kcd->objects_len, "knife cagecos");
  for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
    ob = kcd->objects[ob_index];
    em = BKE_editmesh_from_object(ob);
    knifetool_init_obinfo(kcd, ob, ob_index, use_tri_indices);

    /* Can't usefully select resulting edges in face mode. */
    kcd->select_result = (em->selectmode != SCE_SELECT_FACE);
  }
  knife_bvh_init(kcd);

  /* Cut all the way through the mesh if use_occlude_geometry button not pushed. */
  kcd->is_interactive = is_interactive;
  kcd->cut_through = cut_through;
  kcd->only_select = only_select;
  kcd->depth_test = xray;
  kcd->dist_angle_mode = visible_measurements;
  kcd->show_dist_angle = (kcd->dist_angle_mode != KNF_MEASUREMENT_NONE);
  kcd->angle_snapping_mode = angle_snapping;
  kcd->angle_snapping = (kcd->angle_snapping_mode != KNF_CONSTRAIN_ANGLE_MODE_NONE);
  kcd->angle_snapping_increment = angle_snapping_increment;

  kcd->arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 15), "knife");
#ifdef USE_NET_ISLAND_CONNECT
  kcd->edgenet.arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 15), __func__);
#endif
  kcd->edgenet.edge_visit = BLI_gset_ptr_new(__func__);

  kcd->vthresh = KMAXDIST - 1;
  kcd->ethresh = KMAXDIST;

  knife_recalc_ortho(kcd);

  ED_region_tag_redraw(kcd->region);

  kcd->refs = BLI_mempool_create(sizeof(Ref), 0, 2048, 0);
  kcd->kverts = BLI_mempool_create(sizeof(KnifeVert), 0, 512, BLI_MEMPOOL_ALLOW_ITER);
  kcd->kedges = BLI_mempool_create(sizeof(KnifeEdge), 0, 512, BLI_MEMPOOL_ALLOW_ITER);

  kcd->undostack = BLI_stack_new(sizeof(KnifeUndoFrame), "knife undostack");
  kcd->splitstack = BLI_stack_new(sizeof(KnifeEdge *), "knife splitstack");

  kcd->origedgemap = BLI_ghash_ptr_new("knife origedgemap");
  kcd->origvertmap = BLI_ghash_ptr_new("knife origvertmap");
  kcd->kedgefacemap = BLI_ghash_ptr_new("knife kedgefacemap");
  kcd->facetrimap = BLI_ghash_ptr_new("knife facetrimap");

  knife_pos_data_clear(&kcd->curr);
  knife_pos_data_clear(&kcd->prev);

  if (is_interactive) {
    kcd->draw_handle = ED_region_draw_cb_activate(
        kcd->region->type, knifetool_draw, kcd, REGION_DRAW_POST_VIEW);

    knife_init_colors(&kcd->colors);
  }

  kcd->no_cuts = true;

  kcd->axis_string[0] = ' ';
  kcd->axis_string[1] = '\0';

  /* Initialize number input handling for angle snapping. */
  initNumInput(&kcd->num);
  kcd->num.idx_max = 0;
  kcd->num.val_flag[0] |= NUM_NO_NEGATIVE;
  kcd->num.unit_sys = scene->unit.system;
  kcd->num.unit_type[0] = B_UNIT_NONE;
}

/* called when modal loop selection is done... */
static void knifetool_exit_ex(KnifeTool_OpData *kcd)
{
  if (!kcd) {
    return;
  }

  if (kcd->is_interactive) {
    WM_cursor_modal_restore(kcd->vc.win);

    /* Deactivate the extra drawing stuff in 3D-View. */
    ED_region_draw_cb_exit(kcd->region->type, kcd->draw_handle);
  }

  /* Free the custom data. */
  BLI_mempool_destroy(kcd->refs);
  BLI_mempool_destroy(kcd->kverts);
  BLI_mempool_destroy(kcd->kedges);

  BLI_stack_free(kcd->undostack);
  BLI_stack_free(kcd->splitstack);

  BLI_ghash_free(kcd->origedgemap, NULL, NULL);
  BLI_ghash_free(kcd->origvertmap, NULL, NULL);
  BLI_ghash_free(kcd->kedgefacemap, NULL, NULL);
  BLI_ghash_free(kcd->facetrimap, NULL, NULL);

  BLI_memarena_free(kcd->arena);
#ifdef USE_NET_ISLAND_CONNECT
  BLI_memarena_free(kcd->edgenet.arena);
#endif
  BLI_gset_free(kcd->edgenet.edge_visit, NULL);

  /* Tag for redraw. */
  ED_region_tag_redraw(kcd->region);

  /* Knife BVH cleanup. */
  for (int i = 0; i < kcd->objects_len; i++) {
    knifetool_free_obinfo(kcd, i);
  }
  MEM_freeN((void *)kcd->objects_info);
  knife_bvh_free(kcd);

  /* Line-hits cleanup. */
  if (kcd->linehits) {
    MEM_freeN(kcd->linehits);
  }

  /* Free object bases. */
  if (kcd->objects_free) {
    MEM_freeN(kcd->objects);
  }

  /* Destroy kcd itself. */
  MEM_freeN(kcd);
}

static void knifetool_exit(wmOperator *op)
{
  KnifeTool_OpData *kcd = op->customdata;
  knifetool_exit_ex(kcd);
  op->customdata = NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse-Moving Event Updates
 * \{ */

/** Update active knife edge/vert pointers. */
static int knife_update_active(KnifeTool_OpData *kcd)
{
  /* If no hits are found this would normally default to (0, 0, 0) so instead
   * get a point at the mouse ray closest to the previous point.
   * Note that drawing lines in `free-space` isn't properly supported
   * but there's no guarantee (0, 0, 0) has any geometry either - campbell */
  if (!knife_snap_update_from_mval(kcd, kcd->mval)) {
    float origin[3];
    float origin_ofs[3];

    knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);

    if (!isect_line_plane_v3(
            kcd->curr.cage, origin, origin_ofs, kcd->prev.cage, kcd->vc.rv3d->viewinv[2]))
    {
      copy_v3_v3(kcd->curr.cage, kcd->prev.cage);

      /* Should never fail! */
      BLI_assert(0);
    }
  }

  if (kcd->mode == MODE_DRAGGING) {
    knife_find_line_hits(kcd);
  }
  return 1;
}

static void knifetool_update_mval(KnifeTool_OpData *kcd, const float mval[2])
{
  knife_recalc_ortho(kcd);
  copy_v2_v2(kcd->mval, mval);

  if (knife_update_active(kcd)) {
    ED_region_tag_redraw(kcd->region);
  }
}

static void knifetool_update_mval_i(KnifeTool_OpData *kcd, const int mval_i[2])
{
  const float mval[2] = {UNPACK2(mval_i)};
  knifetool_update_mval(kcd, mval);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Finalization
 * \{ */

static void knifetool_finish_single_pre(KnifeTool_OpData *kcd, Object *ob)
{
  knife_make_cuts(kcd, ob);
}

/**
 * A post version is needed to delay recalculating tessellation after making cuts.
 * Without this, knife-project can't use the BVH tree to select geometry after a cut, see: #98349.
 */
static void knifetool_finish_single_post(KnifeTool_OpData *UNUSED(kcd), Object *ob)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  EDBM_selectmode_flush(em);
  EDBM_update(ob->data,
              &(const struct EDBMUpdate_Params){
                  .calc_looptri = true,
                  .calc_normals = true,
                  .is_destructive = true,
              });
}

/* Called on tool confirmation. */
static void knifetool_finish_ex(KnifeTool_OpData *kcd)
{
  /* Separate pre/post passes are needed because `em->looptris` recalculation from the 'post' pass
   * causes triangle indices in #KnifeTool_OpData.bvh to get out of sync.
   * So perform all the cuts before doing any mesh recalculation, see: #101721. */
  for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
    Object *ob = kcd->objects[ob_index];
    knifetool_finish_single_pre(kcd, ob);
  }
  for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
    Object *ob = kcd->objects[ob_index];
    knifetool_finish_single_post(kcd, ob);
  }
}

static void knifetool_finish(wmOperator *op)
{
  KnifeTool_OpData *kcd = op->customdata;
  knifetool_finish_ex(kcd);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator (#MESH_OT_knife_tool)
 * \{ */

static void knifetool_cancel(bContext *UNUSED(C), wmOperator *op)
{
  /* this is just a wrapper around exit() */
  knifetool_exit(op);
}

wmKeyMap *knifetool_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {KNF_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {KNF_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {KNF_MODAL_UNDO, "UNDO", 0, "Undo", ""},
      {KNF_MODAL_MIDPOINT_ON, "SNAP_MIDPOINTS_ON", 0, "Snap to Midpoints On", ""},
      {KNF_MODAL_MIDPOINT_OFF, "SNAP_MIDPOINTS_OFF", 0, "Snap to Midpoints Off", ""},
      {KNF_MODAL_IGNORE_SNAP_ON, "IGNORE_SNAP_ON", 0, "Ignore Snapping On", ""},
      {KNF_MODAL_IGNORE_SNAP_OFF, "IGNORE_SNAP_OFF", 0, "Ignore Snapping Off", ""},
      {KNF_MODAL_ANGLE_SNAP_TOGGLE, "ANGLE_SNAP_TOGGLE", 0, "Toggle Angle Snapping", ""},
      {KNF_MODAL_CYCLE_ANGLE_SNAP_EDGE,
       "CYCLE_ANGLE_SNAP_EDGE",
       0,
       "Cycle Angle Snapping Relative Edge",
       ""},
      {KNF_MODAL_CUT_THROUGH_TOGGLE, "CUT_THROUGH_TOGGLE", 0, "Toggle Cut Through", ""},
      {KNF_MODAL_SHOW_DISTANCE_ANGLE_TOGGLE,
       "SHOW_DISTANCE_ANGLE_TOGGLE",
       0,
       "Toggle Distance and Angle Measurements",
       ""},
      {KNF_MODAL_DEPTH_TEST_TOGGLE, "DEPTH_TEST_TOGGLE", 0, "Toggle Depth Testing", ""},
      {KNF_MODAL_NEW_CUT, "NEW_CUT", 0, "End Current Cut", ""},
      {KNF_MODAL_ADD_CUT, "ADD_CUT", 0, "Add Cut", ""},
      {KNF_MODAL_ADD_CUT_CLOSED, "ADD_CUT_CLOSED", 0, "Add Cut Closed", ""},
      {KNF_MODAL_PANNING, "PANNING", 0, "Panning", ""},
      {KNF_MODAL_X_AXIS, "X_AXIS", 0, "X Axis Locking", ""},
      {KNF_MODAL_Y_AXIS, "Y_AXIS", 0, "Y Axis Locking", ""},
      {KNF_MODAL_Z_AXIS, "Z_AXIS", 0, "Z Axis Locking", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Knife Tool Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return NULL;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Knife Tool Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "MESH_OT_knife_tool");

  return keymap;
}

/* Turn off angle snapping. */
static void knifetool_disable_angle_snapping(KnifeTool_OpData *kcd)
{
  kcd->angle_snapping_mode = KNF_CONSTRAIN_ANGLE_MODE_NONE;
  kcd->angle_snapping = false;
  kcd->is_angle_snapping = false;
}

/* Turn off orientation locking. */
static void knifetool_disable_orientation_locking(KnifeTool_OpData *kcd)
{
  kcd->constrain_axis = KNF_CONSTRAIN_AXIS_MODE_NONE;
  kcd->constrain_axis_mode = KNF_CONSTRAIN_AXIS_MODE_NONE;
  kcd->axis_constrained = false;
}

static int knifetool_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  KnifeTool_OpData *kcd = op->customdata;
  bool do_refresh = false;

  if (!kcd->curr.ob || kcd->curr.ob->type != OB_MESH) {
    knifetool_exit(op);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_FINISHED;
  }

  kcd->region = kcd->vc.region;

  ED_view3d_init_mats_rv3d(kcd->curr.ob, kcd->vc.rv3d); /* Needed to initialize clipping. */

  if (kcd->mode == MODE_PANNING) {
    kcd->mode = kcd->prevmode;
  }

  bool handled = false;
  float snapping_increment_temp;

  if (kcd->angle_snapping) {
    if (kcd->num.str_cur >= 3 ||
        kcd->angle_snapping_increment > KNIFE_MAX_ANGLE_SNAPPING_INCREMENT / 10)
    {
      knife_reset_snap_angle_input(kcd);
    }
    knife_update_header(C, op, kcd); /* Update the angle multiple. */
    /* Modal numinput active, try to handle numeric inputs first... */
    if (event->val == KM_PRESS && hasNumInput(&kcd->num) && handleNumInput(C, &kcd->num, event)) {
      handled = true;
      applyNumInput(&kcd->num, &snapping_increment_temp);
      /* Restrict number key input to 0 - 180 degree range. */
      if (snapping_increment_temp > KNIFE_MIN_ANGLE_SNAPPING_INCREMENT &&
          snapping_increment_temp <= KNIFE_MAX_ANGLE_SNAPPING_INCREMENT)
      {
        kcd->angle_snapping_increment = snapping_increment_temp;
      }
      knife_update_active(kcd);
      knife_update_header(C, op, kcd);
      ED_region_tag_redraw(kcd->region);
      return OPERATOR_RUNNING_MODAL;
    }
  }

  /* Handle modal keymap. */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case KNF_MODAL_CANCEL:
        /* finish */
        ED_region_tag_redraw(kcd->region);

        knifetool_exit(op);
        ED_workspace_status_text(C, NULL);

        return OPERATOR_CANCELLED;
      case KNF_MODAL_CONFIRM: {
        const bool changed = (kcd->totkvert != 0);
        /* finish */
        ED_region_tag_redraw(kcd->region);

        knifetool_finish(op);
        knifetool_exit(op);
        ED_workspace_status_text(C, NULL);

        /* Cancel to prevent undo push for empty cuts. */
        if (!changed) {
          return OPERATOR_CANCELLED;
        }
        return OPERATOR_FINISHED;
      }
      case KNF_MODAL_UNDO:
        if (BLI_stack_is_empty(kcd->undostack)) {
          ED_region_tag_redraw(kcd->region);
          knifetool_exit(op);
          ED_workspace_status_text(C, NULL);
          return OPERATOR_CANCELLED;
        }
        knifetool_undo(kcd);
        knife_update_active(kcd);
        ED_region_tag_redraw(kcd->region);
        handled = true;
        break;
      case KNF_MODAL_MIDPOINT_ON:
        kcd->snap_midpoints = true;

        knife_recalc_ortho(kcd);
        knife_update_active(kcd);
        knife_update_header(C, op, kcd);
        ED_region_tag_redraw(kcd->region);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_MIDPOINT_OFF:
        kcd->snap_midpoints = false;

        knife_recalc_ortho(kcd);
        knife_update_active(kcd);
        knife_update_header(C, op, kcd);
        ED_region_tag_redraw(kcd->region);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_IGNORE_SNAP_ON:
        ED_region_tag_redraw(kcd->region);
        kcd->ignore_vert_snapping = kcd->ignore_edge_snapping = true;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_IGNORE_SNAP_OFF:
        ED_region_tag_redraw(kcd->region);
        kcd->ignore_vert_snapping = kcd->ignore_edge_snapping = false;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_ANGLE_SNAP_TOGGLE:
        if (kcd->angle_snapping_mode != KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) {
          kcd->angle_snapping_mode++;
          kcd->snap_ref_edges_count = 0;
          kcd->snap_edge = 0;
        }
        else {
          kcd->angle_snapping_mode = KNF_CONSTRAIN_ANGLE_MODE_NONE;
        }
        kcd->angle_snapping = (kcd->angle_snapping_mode != KNF_CONSTRAIN_ANGLE_MODE_NONE);
        kcd->angle_snapping_increment = RAD2DEGF(
            RNA_float_get(op->ptr, "angle_snapping_increment"));
        knifetool_disable_orientation_locking(kcd);
        knife_reset_snap_angle_input(kcd);
        knife_update_active(kcd);
        knife_update_header(C, op, kcd);
        ED_region_tag_redraw(kcd->region);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_CYCLE_ANGLE_SNAP_EDGE:
        if (kcd->angle_snapping && kcd->angle_snapping_mode == KNF_CONSTRAIN_ANGLE_MODE_RELATIVE) {
          if (kcd->snap_ref_edges_count) {
            kcd->snap_edge++;
            kcd->snap_edge %= kcd->snap_ref_edges_count;
          }
        }
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_CUT_THROUGH_TOGGLE:
        kcd->cut_through = !kcd->cut_through;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_SHOW_DISTANCE_ANGLE_TOGGLE:
        if (kcd->dist_angle_mode != KNF_MEASUREMENT_ANGLE) {
          kcd->dist_angle_mode++;
        }
        else {
          kcd->dist_angle_mode = KNF_MEASUREMENT_NONE;
        }
        kcd->show_dist_angle = (kcd->dist_angle_mode != KNF_MEASUREMENT_NONE);
        knife_update_header(C, op, kcd);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_DEPTH_TEST_TOGGLE:
        kcd->depth_test = !kcd->depth_test;
        ED_region_tag_redraw(kcd->region);
        knife_update_header(C, op, kcd);
        do_refresh = true;
        handled = true;
        break;
      case KNF_MODAL_NEW_CUT:
        /* If no cuts have been made, exit.
         * Preserves right click cancel workflow which most tools use,
         * but stops accidentally deleting entire cuts with right click.
         */
        if (kcd->no_cuts) {
          ED_region_tag_redraw(kcd->region);
          knifetool_exit(op);
          ED_workspace_status_text(C, NULL);
          return OPERATOR_CANCELLED;
        }
        ED_region_tag_redraw(kcd->region);
        knife_finish_cut(kcd);
        kcd->mode = MODE_IDLE;
        handled = true;
        break;
      case KNF_MODAL_ADD_CUT:
        kcd->no_cuts = false;
        knife_recalc_ortho(kcd);

        /* Get the value of the event which triggered this one. */
        if (event->prev_val != KM_RELEASE) {
          if (kcd->mode == MODE_DRAGGING) {
            knife_add_cut(kcd);
          }
          else if (kcd->mode != MODE_PANNING) {
            knife_start_cut(kcd);
            kcd->mode = MODE_DRAGGING;
            kcd->init = kcd->curr;
          }

          /* Freehand drawing is incompatible with cut-through. */
          if (kcd->cut_through == false) {
            kcd->is_drag_hold = true;
            /* No edge snapping while dragging (edges are too sticky when cuts are immediate). */
            kcd->ignore_edge_snapping = true;
          }
        }
        else {
          kcd->is_drag_hold = false;
          kcd->ignore_edge_snapping = false;
          kcd->is_drag_undo = false;

          /* Needed because the last face 'hit' is ignored when dragging. */
          knifetool_update_mval(kcd, kcd->curr.mval);
        }

        ED_region_tag_redraw(kcd->region);
        handled = true;
        break;
      case KNF_MODAL_ADD_CUT_CLOSED:
        if (kcd->mode == MODE_DRAGGING) {

          /* Shouldn't be possible with default key-layout, just in case. */
          if (kcd->is_drag_hold) {
            kcd->is_drag_hold = false;
            kcd->is_drag_undo = false;
            knifetool_update_mval(kcd, kcd->curr.mval);
          }

          kcd->prev = kcd->curr;
          kcd->curr = kcd->init;

          knife_project_v2(kcd, kcd->curr.cage, kcd->curr.mval);
          knifetool_update_mval(kcd, kcd->curr.mval);

          knife_add_cut(kcd);

          /* KNF_MODAL_NEW_CUT */
          knife_finish_cut(kcd);
          kcd->mode = MODE_IDLE;
        }
        handled = true;
        break;
      case KNF_MODAL_PANNING:
        if (event->val != KM_RELEASE) {
          if (kcd->mode != MODE_PANNING) {
            kcd->prevmode = kcd->mode;
            kcd->mode = MODE_PANNING;
          }
        }
        else {
          kcd->mode = kcd->prevmode;
        }

        ED_region_tag_redraw(kcd->region);
        return OPERATOR_PASS_THROUGH;
    }
  }
  else { /* non-modal-mapped events */
    switch (event->type) {
      case MOUSEPAN:
      case MOUSEZOOM:
      case MOUSEROTATE:
      case WHEELUPMOUSE:
      case WHEELDOWNMOUSE:
      case NDOF_MOTION:
        return OPERATOR_PASS_THROUGH;
      case MOUSEMOVE: /* Mouse moved somewhere to select another loop. */
        if (kcd->mode != MODE_PANNING) {
          knifetool_update_mval_i(kcd, event->mval);
          knife_update_header(C, op, kcd);

          if (kcd->is_drag_hold) {
            if (kcd->totlinehit >= 2) {
              knife_add_cut(kcd);
            }
          }
        }

        break;
    }
  }

  if (kcd->angle_snapping) {
    if (kcd->num.str_cur >= 3 ||
        kcd->angle_snapping_increment > KNIFE_MAX_ANGLE_SNAPPING_INCREMENT / 10)
    {
      knife_reset_snap_angle_input(kcd);
    }
    if (event->type != EVT_MODAL_MAP) {
      /* Modal number-input inactive, try to handle numeric inputs last. */
      if (!handled && event->val == KM_PRESS && handleNumInput(C, &kcd->num, event)) {
        applyNumInput(&kcd->num, &snapping_increment_temp);
        /* Restrict number key input to 0 - 180 degree range. */
        if (snapping_increment_temp > KNIFE_MIN_ANGLE_SNAPPING_INCREMENT &&
            snapping_increment_temp <= KNIFE_MAX_ANGLE_SNAPPING_INCREMENT)
        {
          kcd->angle_snapping_increment = snapping_increment_temp;
        }
        knife_update_active(kcd);
        knife_update_header(C, op, kcd);
        ED_region_tag_redraw(kcd->region);
        return OPERATOR_RUNNING_MODAL;
      }
    }
  }

  /* Constrain axes with X,Y,Z keys. */
  if (event->type == EVT_MODAL_MAP) {
    if (ELEM(event->val, KNF_MODAL_X_AXIS, KNF_MODAL_Y_AXIS, KNF_MODAL_Z_AXIS)) {
      if (event->val == KNF_MODAL_X_AXIS && kcd->constrain_axis != KNF_CONSTRAIN_AXIS_X) {
        kcd->constrain_axis = KNF_CONSTRAIN_AXIS_X;
        kcd->constrain_axis_mode = KNF_CONSTRAIN_AXIS_MODE_GLOBAL;
        kcd->axis_string[0] = 'X';
      }
      else if (event->val == KNF_MODAL_Y_AXIS && kcd->constrain_axis != KNF_CONSTRAIN_AXIS_Y) {
        kcd->constrain_axis = KNF_CONSTRAIN_AXIS_Y;
        kcd->constrain_axis_mode = KNF_CONSTRAIN_AXIS_MODE_GLOBAL;
        kcd->axis_string[0] = 'Y';
      }
      else if (event->val == KNF_MODAL_Z_AXIS && kcd->constrain_axis != KNF_CONSTRAIN_AXIS_Z) {
        kcd->constrain_axis = KNF_CONSTRAIN_AXIS_Z;
        kcd->constrain_axis_mode = KNF_CONSTRAIN_AXIS_MODE_GLOBAL;
        kcd->axis_string[0] = 'Z';
      }
      else {
        /* Cycle through modes with repeated key presses. */
        if (kcd->constrain_axis_mode != KNF_CONSTRAIN_AXIS_MODE_LOCAL) {
          kcd->constrain_axis_mode++;
          kcd->axis_string[0] += 32; /* Lower case. */
        }
        else {
          kcd->constrain_axis = KNF_CONSTRAIN_AXIS_NONE;
          kcd->constrain_axis_mode = KNF_CONSTRAIN_AXIS_MODE_NONE;
        }
      }
      kcd->axis_constrained = (kcd->constrain_axis != KNF_CONSTRAIN_AXIS_NONE);
      knifetool_disable_angle_snapping(kcd);
      knife_update_header(C, op, kcd);
      ED_region_tag_redraw(kcd->region);
      do_refresh = true;
    }
  }

  if (kcd->mode == MODE_DRAGGING) {
    op->flag &= ~OP_IS_MODAL_CURSOR_REGION;
  }
  else {
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
  }

  if (do_refresh) {
    /* We don't really need to update mval,
     * but this happens to be the best way to refresh at the moment. */
    knifetool_update_mval_i(kcd, event->mval);
    knife_update_header(C, op, kcd);
  }

  /* Keep going until the user confirms. */
  return OPERATOR_RUNNING_MODAL;
}

static int knifetool_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool only_select = RNA_boolean_get(op->ptr, "only_selected");
  const bool cut_through = !RNA_boolean_get(op->ptr, "use_occlude_geometry");
  const bool xray = !RNA_boolean_get(op->ptr, "xray");
  const int visible_measurements = RNA_enum_get(op->ptr, "visible_measurements");
  const int angle_snapping = RNA_enum_get(op->ptr, "angle_snapping");
  const bool wait_for_input = RNA_boolean_get(op->ptr, "wait_for_input");
  const float angle_snapping_increment = RAD2DEGF(
      RNA_float_get(op->ptr, "angle_snapping_increment"));

  ViewContext vc;
  KnifeTool_OpData *kcd;

  em_setup_viewcontext(C, &vc);

  /* alloc new customdata */
  kcd = op->customdata = MEM_callocN(sizeof(KnifeTool_OpData), __func__);

  knifetool_init(&vc,
                 kcd,
                 NULL,
                 0,
                 only_select,
                 cut_through,
                 xray,
                 visible_measurements,
                 angle_snapping,
                 angle_snapping_increment,
                 true);

  if (only_select) {
    Object *obedit;
    BMEditMesh *em;
    bool faces_selected = false;

    for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
      obedit = kcd->objects[ob_index];
      em = BKE_editmesh_from_object(obedit);
      if (em->bm->totfacesel != 0) {
        faces_selected = true;
      }
    }

    if (!faces_selected) {
      BKE_report(op->reports, RPT_ERROR, "Selected faces required");
      knifetool_cancel(C, op);
      return OPERATOR_CANCELLED;
    }
  }

  op->flag |= OP_IS_MODAL_CURSOR_REGION;

  /* Add a modal handler for this operator - handles loop selection. */
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_KNIFE);
  WM_event_add_modal_handler(C, op);

  knifetool_update_mval_i(kcd, event->mval);

  if (wait_for_input == false) {
    /* Avoid copy-paste logic. */
    wmEvent event_modal = {
        .prev_val = KM_NOTHING,
        .type = EVT_MODAL_MAP,
        .val = KNF_MODAL_ADD_CUT,
    };
    int ret = knifetool_modal(C, op, &event_modal);
    BLI_assert(ret == OPERATOR_RUNNING_MODAL);
    UNUSED_VARS_NDEBUG(ret);
  }

  knife_update_header(C, op, kcd);

  return OPERATOR_RUNNING_MODAL;
}

void MESH_OT_knife_tool(wmOperatorType *ot)
{
  /* Description. */
  ot->name = "Knife Topology Tool";
  ot->idname = "MESH_OT_knife_tool";
  ot->description = "Cut new topology";

  /* Callbacks. */
  ot->invoke = knifetool_invoke;
  ot->modal = knifetool_modal;
  ot->cancel = knifetool_cancel;
  ot->poll = ED_operator_editmesh_view3d;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  PropertyRNA *prop;
  static const EnumPropertyItem visible_measurements_items[] = {
      {KNF_MEASUREMENT_NONE, "NONE", 0, "None", "Show no measurements"},
      {KNF_MEASUREMENT_BOTH, "BOTH", 0, "Both", "Show both distances and angles"},
      {KNF_MEASUREMENT_DISTANCE, "DISTANCE", 0, "Distance", "Show just distance measurements"},
      {KNF_MEASUREMENT_ANGLE, "ANGLE", 0, "Angle", "Show just angle measurements"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem angle_snapping_items[] = {
      {KNF_CONSTRAIN_ANGLE_MODE_NONE, "NONE", 0, "None", "No angle snapping"},
      {KNF_CONSTRAIN_ANGLE_MODE_SCREEN, "SCREEN", 0, "Screen", "Screen space angle snapping"},
      {KNF_CONSTRAIN_ANGLE_MODE_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Angle snapping relative to the previous cut edge"},
      {0, NULL, 0, NULL, NULL},
  };

  RNA_def_boolean(ot->srna,
                  "use_occlude_geometry",
                  true,
                  "Occlude Geometry",
                  "Only cut the front most geometry");
  RNA_def_boolean(ot->srna, "only_selected", false, "Only Selected", "Only cut selected geometry");
  RNA_def_boolean(ot->srna, "xray", true, "X-Ray", "Show cuts hidden by geometry");

  RNA_def_enum(ot->srna,
               "visible_measurements",
               visible_measurements_items,
               KNF_MEASUREMENT_NONE,
               "Measurements",
               "Visible distance and angle measurements");
  prop = RNA_def_enum(ot->srna,
                      "angle_snapping",
                      angle_snapping_items,
                      KNF_CONSTRAIN_ANGLE_MODE_NONE,
                      "Angle Snapping",
                      "Angle snapping mode");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);

  prop = RNA_def_float(ot->srna,
                       "angle_snapping_increment",
                       DEG2RADF(KNIFE_DEFAULT_ANGLE_SNAPPING_INCREMENT),
                       DEG2RADF(KNIFE_MIN_ANGLE_SNAPPING_INCREMENT),
                       DEG2RADF(KNIFE_MAX_ANGLE_SNAPPING_INCREMENT),
                       "Angle Snap Increment",
                       "The angle snap increment used when in constrained angle mode",
                       DEG2RADF(KNIFE_MIN_ANGLE_SNAPPING_INCREMENT),
                       DEG2RADF(KNIFE_MAX_ANGLE_SNAPPING_INCREMENT));
  RNA_def_property_subtype(prop, PROP_ANGLE);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knife tool as a utility function
 *
 * Can be used for internal slicing operations.
 * \{ */

static bool edbm_mesh_knife_point_isect(LinkNode *polys, const float cent_ss[2])
{
  LinkNode *p = polys;
  int isect = 0;

  while (p) {
    const float(*mval_fl)[2] = p->link;
    const int mval_tot = MEM_allocN_len(mval_fl) / sizeof(*mval_fl);
    isect += (int)isect_point_poly_v2(cent_ss, mval_fl, mval_tot - 1, false);
    p = p->next;
  }

  if (isect % 2) {
    return true;
  }
  return false;
}

void EDBM_mesh_knife(ViewContext *vc,
                     Object **objects,
                     const int objects_len,
                     LinkNode *polys,
                     bool use_tag,
                     bool cut_through)
{
  KnifeTool_OpData *kcd;

  /* Init. */
  {
    const bool only_select = false;
    const bool is_interactive = false; /* Can enable for testing. */
    const bool xray = false;
    const int visible_measurements = KNF_MEASUREMENT_NONE;
    const int angle_snapping = KNF_CONSTRAIN_ANGLE_MODE_NONE;
    const float angle_snapping_increment = KNIFE_DEFAULT_ANGLE_SNAPPING_INCREMENT;

    kcd = MEM_callocN(sizeof(KnifeTool_OpData), __func__);

    knifetool_init(vc,
                   kcd,
                   objects,
                   objects_len,
                   only_select,
                   cut_through,
                   xray,
                   visible_measurements,
                   angle_snapping,
                   angle_snapping_increment,
                   is_interactive);

    kcd->ignore_edge_snapping = true;
    kcd->ignore_vert_snapping = true;
  }

  /* Execute. */
  {
    LinkNode *p = polys;

    knife_recalc_ortho(kcd);

    while (p) {
      const float(*mval_fl)[2] = p->link;
      const int mval_tot = MEM_allocN_len(mval_fl) / sizeof(*mval_fl);
      int i;

      for (i = 0; i < mval_tot; i++) {
        knifetool_update_mval(kcd, mval_fl[i]);
        if (i == 0) {
          knife_start_cut(kcd);
          kcd->mode = MODE_DRAGGING;
        }
        else {
          knife_add_cut(kcd);
        }
      }
      knife_finish_cut(kcd);
      kcd->mode = MODE_IDLE;
      p = p->next;
    }
  }

  /* Finish. */
  {
    /* See #knifetool_finish_ex for why multiple passes are needed. */
    for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
      Object *ob = kcd->objects[ob_index];
      BMEditMesh *em = BKE_editmesh_from_object(ob);

      if (use_tag) {
        BM_mesh_elem_hflag_enable_all(em->bm, BM_EDGE, BM_ELEM_TAG, false);
      }

      knifetool_finish_single_pre(kcd, ob);
    }

    for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
      Object *ob = kcd->objects[ob_index];
      BMEditMesh *em = BKE_editmesh_from_object(ob);

      /* Tag faces inside! */
      if (use_tag) {
        BMesh *bm = em->bm;
        BMEdge *e;
        BMIter iter;
        bool keep_search;

        /* Use face-loop tag to store if we have intersected. */
#define F_ISECT_IS_UNKNOWN(f) BM_elem_flag_test(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
#define F_ISECT_SET_UNKNOWN(f) BM_elem_flag_enable(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
#define F_ISECT_SET_OUTSIDE(f) BM_elem_flag_disable(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
        {
          BMFace *f;
          BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
            F_ISECT_SET_UNKNOWN(f);
            BM_elem_flag_disable(f, BM_ELEM_TAG);
          }
        }

        /* Tag all faces linked to cut edges. */
        BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
          /* Check are we tagged?, then we are an original face. */
          if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
            continue;
          }

          BMFace *f;
          BMIter fiter;
          BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
            float cent[3], cent_ss[2];
            BM_face_calc_point_in_face(f, cent);
            mul_m4_v3(ob->object_to_world, cent);
            knife_project_v2(kcd, cent, cent_ss);
            if (edbm_mesh_knife_point_isect(polys, cent_ss)) {
              BM_elem_flag_enable(f, BM_ELEM_TAG);
            }
          }
        }

        /* Expand tags for faces which are not cut, but are inside the polys. */
        do {
          BMFace *f;
          keep_search = false;
          BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
            if (BM_elem_flag_test(f, BM_ELEM_TAG) || !F_ISECT_IS_UNKNOWN(f)) {
              continue;
            }

            /* Am I connected to a tagged face via an un-tagged edge
             * (ie, not across a cut)? */
            BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
            BMLoop *l_iter = l_first;
            bool found = false;

            do {
              if (BM_elem_flag_test(l_iter->e, BM_ELEM_TAG) != false) {
                /* Now check if the adjacent faces is tagged. */
                BMLoop *l_radial_iter = l_iter->radial_next;
                if (l_radial_iter != l_iter) {
                  do {
                    if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_TAG)) {
                      found = true;
                    }
                  } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter &&
                           (found == false));
                }
              }
            } while ((l_iter = l_iter->next) != l_first && (found == false));

            if (found) {
              float cent[3], cent_ss[2];
              BM_face_calc_point_in_face(f, cent);
              mul_m4_v3(ob->object_to_world, cent);
              knife_project_v2(kcd, cent, cent_ss);
              if ((kcd->cut_through || point_is_visible(kcd, cent, cent_ss, (BMElem *)f)) &&
                  edbm_mesh_knife_point_isect(polys, cent_ss))
              {
                BM_elem_flag_enable(f, BM_ELEM_TAG);
                keep_search = true;
              }
              else {
                /* Don't lose time on this face again, set it as outside. */
                F_ISECT_SET_OUTSIDE(f);
              }
            }
          }
        } while (keep_search);

#undef F_ISECT_IS_UNKNOWN
#undef F_ISECT_SET_UNKNOWN
#undef F_ISECT_SET_OUTSIDE
      }
    }

    for (uint ob_index = 0; ob_index < kcd->objects_len; ob_index++) {
      /* Defer freeing data until the BVH tree is finished with, see: #point_is_visible and
       * the doc-string for #knifetool_finish_single_post. */
      Object *ob = kcd->objects[ob_index];
      knifetool_finish_single_post(kcd, ob);
    }

    knifetool_exit_ex(kcd);
    kcd = NULL;
  }
}

/** \} */
