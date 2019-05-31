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
 * \ingroup spview3d
 */

/* defines VIEW3D_OT_navigate - walk modal operator */

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLT_translation.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform_snap_object_context.h"

#include "PIL_time.h" /* smoothview */

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_immediate.h"

#include "DEG_depsgraph.h"

#include "view3d_intern.h" /* own include */

#ifdef WITH_INPUT_NDOF
//#  define NDOF_WALK_DEBUG
/* is this needed for ndof? - commented so redraw doesn't thrash - campbell */
//#  define NDOF_WALK_DRAW_TOOMUCH
#endif

#define USE_TABLET_SUPPORT

/* ensure the target position is one we can reach, see: T45771 */
#define USE_PIXELSIZE_NATIVE_SUPPORT

/* prototypes */
static float getVelocityZeroTime(const float gravity, const float velocity);

/* NOTE: these defines are saved in keymap files,
 * do not change values but just add new ones */
enum {
  WALK_MODAL_CANCEL = 1,
  WALK_MODAL_CONFIRM,
  WALK_MODAL_DIR_FORWARD,
  WALK_MODAL_DIR_FORWARD_STOP,
  WALK_MODAL_DIR_BACKWARD,
  WALK_MODAL_DIR_BACKWARD_STOP,
  WALK_MODAL_DIR_LEFT,
  WALK_MODAL_DIR_LEFT_STOP,
  WALK_MODAL_DIR_RIGHT,
  WALK_MODAL_DIR_RIGHT_STOP,
  WALK_MODAL_DIR_UP,
  WALK_MODAL_DIR_UP_STOP,
  WALK_MODAL_DIR_DOWN,
  WALK_MODAL_DIR_DOWN_STOP,
  WALK_MODAL_FAST_ENABLE,
  WALK_MODAL_FAST_DISABLE,
  WALK_MODAL_SLOW_ENABLE,
  WALK_MODAL_SLOW_DISABLE,
  WALK_MODAL_JUMP,
  WALK_MODAL_JUMP_STOP,
  WALK_MODAL_TELEPORT,
  WALK_MODAL_TOGGLE,
  WALK_MODAL_ACCELERATE,
  WALK_MODAL_DECELERATE,
};

enum {
  WALK_BIT_FORWARD = 1 << 0,
  WALK_BIT_BACKWARD = 1 << 1,
  WALK_BIT_LEFT = 1 << 2,
  WALK_BIT_RIGHT = 1 << 3,
  WALK_BIT_UP = 1 << 4,
  WALK_BIT_DOWN = 1 << 5,
};

typedef enum eWalkTeleportState {
  WALK_TELEPORT_STATE_OFF = 0,
  WALK_TELEPORT_STATE_ON,
} eWalkTeleportState;

typedef enum eWalkMethod {
  WALK_MODE_FREE = 0,
  WALK_MODE_GRAVITY,
} eWalkMethod;

typedef enum eWalkGravityState {
  WALK_GRAVITY_STATE_OFF = 0,
  WALK_GRAVITY_STATE_JUMP,
  WALK_GRAVITY_STATE_START,
  WALK_GRAVITY_STATE_ON,
} eWalkGravityState;

/* called in transform_ops.c, on each regeneration of keymaps  */
void walk_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {WALK_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {WALK_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},

      {WALK_MODAL_DIR_FORWARD, "FORWARD", 0, "Forward", ""},
      {WALK_MODAL_DIR_BACKWARD, "BACKWARD", 0, "Backward", ""},
      {WALK_MODAL_DIR_LEFT, "LEFT", 0, "Left", ""},
      {WALK_MODAL_DIR_RIGHT, "RIGHT", 0, "Right", ""},
      {WALK_MODAL_DIR_UP, "UP", 0, "Up", ""},
      {WALK_MODAL_DIR_DOWN, "DOWN", 0, "Down", ""},

      {WALK_MODAL_DIR_FORWARD_STOP, "FORWARD_STOP", 0, "Stop Move Forward", ""},
      {WALK_MODAL_DIR_BACKWARD_STOP, "BACKWARD_STOP", 0, "Stop Mode Backward", ""},
      {WALK_MODAL_DIR_LEFT_STOP, "LEFT_STOP", 0, "Stop Move Left", ""},
      {WALK_MODAL_DIR_RIGHT_STOP, "RIGHT_STOP", 0, "Stop Mode Right", ""},
      {WALK_MODAL_DIR_UP_STOP, "UP_STOP", 0, "Stop Move Up", ""},
      {WALK_MODAL_DIR_DOWN_STOP, "DOWN_STOP", 0, "Stop Mode Down", ""},

      {WALK_MODAL_TELEPORT, "TELEPORT", 0, "Teleport", "Move forward a few units at once"},

      {WALK_MODAL_ACCELERATE, "ACCELERATE", 0, "Accelerate", ""},
      {WALK_MODAL_DECELERATE, "DECELERATE", 0, "Decelerate", ""},

      {WALK_MODAL_FAST_ENABLE, "FAST_ENABLE", 0, "Fast", "Move faster (walk or fly)"},
      {WALK_MODAL_FAST_DISABLE, "FAST_DISABLE", 0, "Fast (Off)", "Resume regular speed"},

      {WALK_MODAL_SLOW_ENABLE, "SLOW_ENABLE", 0, "Slow", "Move slower (walk or fly)"},
      {WALK_MODAL_SLOW_DISABLE, "SLOW_DISABLE", 0, "Slow (Off)", "Resume regular speed"},

      {WALK_MODAL_JUMP, "JUMP", 0, "Jump", "Jump when in walk mode"},
      {WALK_MODAL_JUMP_STOP, "JUMP_STOP", 0, "Jump (Off)", "Stop pushing jump"},

      {WALK_MODAL_TOGGLE, "GRAVITY_TOGGLE", 0, "Toggle Gravity", "Toggle gravity effect"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Walk Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_add(keyconf, "View3D Walk Modal", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_walk");
}

typedef struct WalkTeleport {
  eWalkTeleportState state;
  float duration; /* from user preferences */
  float origin[3];
  float direction[3];
  double initial_time;
  eWalkMethod navigation_mode; /* teleport always set FREE mode on */

} WalkTeleport;

typedef struct WalkInfo {
  /* context stuff */
  RegionView3D *rv3d;
  View3D *v3d;
  ARegion *ar;
  struct Depsgraph *depsgraph;
  Scene *scene;

  wmTimer *timer; /* needed for redraws */

  short state;
  bool redraw;

  int prev_mval[2];   /* previous 2D mouse values */
  int center_mval[2]; /* center mouse values */
  int moffset[2];

#ifdef WITH_INPUT_NDOF
  wmNDOFMotionData *ndof; /* latest 3D mouse values */
#endif

  /* walk state state */
  float base_speed; /* the base speed without run/slow down modifications */
  float speed;      /* the speed the view is moving per redraw */
  float grid;       /* world scale 1.0 default */

  /* compare between last state */
  double time_lastdraw; /* time between draws */

  void *draw_handle_pixel;

  /* use for some lag */
  float dvec_prev[3]; /* old for some lag */

  /* walk/fly */
  eWalkMethod navigation_mode;

  /* teleport */
  WalkTeleport teleport;

  /* look speed factor - user preferences */
  float mouse_speed;

  /* speed adjustments */
  bool is_fast;
  bool is_slow;

  /* mouse reverse */
  bool is_reversed;

#ifdef USE_TABLET_SUPPORT
  /* check if we had a cursor event before */
  bool is_cursor_first;

  /* tablet devices (we can't relocate the cursor) */
  bool is_cursor_absolute;
#endif

  /* gravity system */
  eWalkGravityState gravity_state;
  float gravity;

  /* height to use in walk mode */
  float view_height;

  /* counting system to allow movement to continue if a direction (WASD) key is still pressed */
  int active_directions;

  float speed_jump;
  float jump_height;  /* maximum jump height */
  float speed_factor; /* to use for fast/slow speeds */

  struct SnapObjectContext *snap_context;

  struct View3DCameraControl *v3d_camera_control;

} WalkInfo;

static void drawWalkPixel(const struct bContext *UNUSED(C), ARegion *ar, void *arg)
{
  /* draws an aim/cross in the center */
  WalkInfo *walk = arg;

  const int outter_length = 24;
  const int inner_length = 14;
  int xoff, yoff;
  rctf viewborder;

  if (ED_view3d_cameracontrol_object_get(walk->v3d_camera_control)) {
    ED_view3d_calc_camera_border(
        walk->scene, walk->depsgraph, ar, walk->v3d, walk->rv3d, &viewborder, false);
    xoff = viewborder.xmin + BLI_rctf_size_x(&viewborder) * 0.5f;
    yoff = viewborder.ymin + BLI_rctf_size_y(&viewborder) * 0.5f;
  }
  else {
    xoff = walk->ar->winx / 2;
    yoff = walk->ar->winy / 2;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformThemeColor(TH_VIEW_OVERLAY);

  immBegin(GPU_PRIM_LINES, 8);

  /* North */
  immVertex2i(pos, xoff, yoff + inner_length);
  immVertex2i(pos, xoff, yoff + outter_length);

  /* East */
  immVertex2i(pos, xoff + inner_length, yoff);
  immVertex2i(pos, xoff + outter_length, yoff);

  /* South */
  immVertex2i(pos, xoff, yoff - inner_length);
  immVertex2i(pos, xoff, yoff - outter_length);

  /* West */
  immVertex2i(pos, xoff - inner_length, yoff);
  immVertex2i(pos, xoff - outter_length, yoff);

  immEnd();
  immUnbindProgram();
}

static void walk_navigation_mode_set(WalkInfo *walk, eWalkMethod mode)
{
  if (mode == WALK_MODE_FREE) {
    walk->navigation_mode = WALK_MODE_FREE;
    walk->gravity_state = WALK_GRAVITY_STATE_OFF;
  }
  else { /* WALK_MODE_GRAVITY */
    walk->navigation_mode = WALK_MODE_GRAVITY;
    walk->gravity_state = WALK_GRAVITY_STATE_START;
  }
}

/**
 * \param r_distance: Distance to the hit point
 */
static bool walk_floor_distance_get(RegionView3D *rv3d,
                                    WalkInfo *walk,
                                    const float dvec[3],
                                    float *r_distance)
{
  float ray_normal[3] = {0, 0, -1}; /* down */
  float ray_start[3];
  float r_location[3];
  float r_normal_dummy[3];
  float dvec_tmp[3];
  bool ret;

  *r_distance = BVH_RAYCAST_DIST_MAX;

  copy_v3_v3(ray_start, rv3d->viewinv[3]);

  mul_v3_v3fl(dvec_tmp, dvec, walk->grid);
  add_v3_v3(ray_start, dvec_tmp);

  ret = ED_transform_snap_object_project_ray(walk->snap_context,
                                             &(const struct SnapObjectParams){
                                                 .snap_select = SNAP_ALL,
                                             },
                                             ray_start,
                                             ray_normal,
                                             r_distance,
                                             r_location,
                                             r_normal_dummy);

  /* artificially scale the distance to the scene size */
  *r_distance /= walk->grid;
  return ret;
}

/**
 * \param ray_distance: Distance to the hit point
 * \param r_location: Location of the hit point
 * \param r_normal: Normal of the hit surface, transformed to always face the camera
 */
static bool walk_ray_cast(RegionView3D *rv3d,
                          WalkInfo *walk,
                          float r_location[3],
                          float r_normal[3],
                          float *ray_distance)
{
  float ray_normal[3] = {0, 0, -1}; /* forward */
  float ray_start[3];
  bool ret;

  *ray_distance = BVH_RAYCAST_DIST_MAX;

  copy_v3_v3(ray_start, rv3d->viewinv[3]);

  mul_mat3_m4_v3(rv3d->viewinv, ray_normal);

  normalize_v3(ray_normal);

  ret = ED_transform_snap_object_project_ray(walk->snap_context,
                                             &(const struct SnapObjectParams){
                                                 .snap_select = SNAP_ALL,
                                             },
                                             ray_start,
                                             ray_normal,
                                             NULL,
                                             r_location,
                                             r_normal);

  /* dot is positive if both rays are facing the same direction */
  if (dot_v3v3(ray_normal, r_normal) > 0) {
    negate_v3(r_normal);
  }

  /* artificially scale the distance to the scene size */
  *ray_distance /= walk->grid;

  return ret;
}

/* WalkInfo->state */
enum {
  WALK_RUNNING = 0,
  WALK_CANCEL = 1,
  WALK_CONFIRM = 2,
};

/* keep the previous speed until user changes userpreferences */
static float base_speed = -1.f;
static float userdef_speed = -1.f;

static bool initWalkInfo(bContext *C, WalkInfo *walk, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);

  walk->rv3d = CTX_wm_region_view3d(C);
  walk->v3d = CTX_wm_view3d(C);
  walk->ar = CTX_wm_region(C);
  walk->depsgraph = CTX_data_depsgraph(C);
  walk->scene = CTX_data_scene(C);

#ifdef NDOF_WALK_DEBUG
  puts("\n-- walk begin --");
#endif

  /* sanity check: for rare but possible case (if lib-linking the camera fails) */
  if ((walk->rv3d->persp == RV3D_CAMOB) && (walk->v3d->camera == NULL)) {
    walk->rv3d->persp = RV3D_PERSP;
  }

  if (walk->rv3d->persp == RV3D_CAMOB && ID_IS_LINKED(walk->v3d->camera)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot navigate a camera from an external library");
    return false;
  }

  if (ED_view3d_offset_lock_check(walk->v3d, walk->rv3d)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot navigate when the view offset is locked");
    return false;
  }

  if (walk->rv3d->persp == RV3D_CAMOB && walk->v3d->camera->constraints.first) {
    BKE_report(op->reports, RPT_ERROR, "Cannot navigate an object with constraints");
    return false;
  }

  walk->state = WALK_RUNNING;

  if (fabsf(U.walk_navigation.walk_speed - userdef_speed) > 0.1f) {
    base_speed = U.walk_navigation.walk_speed;
    userdef_speed = U.walk_navigation.walk_speed;
  }

  walk->speed = 0.0f;
  walk->is_fast = false;
  walk->is_slow = false;
  walk->grid = (walk->scene->unit.system == USER_UNIT_NONE) ? 1.f :
                                                              1.f / walk->scene->unit.scale_length;

  /* user preference settings */
  walk->teleport.duration = U.walk_navigation.teleport_time;
  walk->mouse_speed = U.walk_navigation.mouse_speed;

  if ((U.walk_navigation.flag & USER_WALK_GRAVITY)) {
    walk_navigation_mode_set(walk, WALK_MODE_GRAVITY);
  }
  else {
    walk_navigation_mode_set(walk, WALK_MODE_FREE);
  }

  walk->view_height = U.walk_navigation.view_height;
  walk->jump_height = U.walk_navigation.jump_height;
  walk->speed = U.walk_navigation.walk_speed;
  walk->speed_factor = U.walk_navigation.walk_speed_factor;

  walk->gravity_state = WALK_GRAVITY_STATE_OFF;

  if ((walk->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY)) {
    walk->gravity = fabsf(walk->scene->physics_settings.gravity[2]);
  }
  else {
    walk->gravity = 9.80668f; /* m/s2 */
  }

  walk->is_reversed = ((U.walk_navigation.flag & USER_WALK_MOUSE_REVERSE) != 0);

#ifdef USE_TABLET_SUPPORT
  walk->is_cursor_first = true;

  walk->is_cursor_absolute = false;
#endif

  walk->active_directions = 0;

#ifdef NDOF_WALK_DRAW_TOOMUCH
  walk->redraw = 1;
#endif
  zero_v3(walk->dvec_prev);

  walk->timer = WM_event_add_timer(CTX_wm_manager(C), win, TIMER, 0.01f);

#ifdef WITH_INPUT_NDOF
  walk->ndof = NULL;
#endif

  walk->time_lastdraw = PIL_check_seconds_timer();

  walk->draw_handle_pixel = ED_region_draw_cb_activate(
      walk->ar->type, drawWalkPixel, walk, REGION_DRAW_POST_PIXEL);

  walk->rv3d->rflag |= RV3D_NAVIGATING;

  walk->snap_context = ED_transform_snap_object_context_create_view3d(
      bmain, walk->scene, CTX_data_depsgraph(C), 0, walk->ar, walk->v3d);

  walk->v3d_camera_control = ED_view3d_cameracontrol_acquire(
      walk->depsgraph,
      walk->scene,
      walk->v3d,
      walk->rv3d,
      (U.uiflag & USER_CAM_LOCK_NO_PARENT) == 0);

  /* center the mouse */
  walk->center_mval[0] = walk->ar->winx * 0.5f;
  walk->center_mval[1] = walk->ar->winy * 0.5f;

#ifdef USE_PIXELSIZE_NATIVE_SUPPORT
  walk->center_mval[0] += walk->ar->winrct.xmin;
  walk->center_mval[1] += walk->ar->winrct.ymin;

  WM_cursor_compatible_xy(win, &walk->center_mval[0], &walk->center_mval[1]);

  walk->center_mval[0] -= walk->ar->winrct.xmin;
  walk->center_mval[1] -= walk->ar->winrct.ymin;
#endif

  copy_v2_v2_int(walk->prev_mval, walk->center_mval);

  WM_cursor_warp(win,
                 walk->ar->winrct.xmin + walk->center_mval[0],
                 walk->ar->winrct.ymin + walk->center_mval[1]);

  /* remove the mouse cursor temporarily */
  WM_cursor_modal_set(win, CURSOR_NONE);

  return 1;
}

static int walkEnd(bContext *C, WalkInfo *walk)
{
  wmWindow *win;
  RegionView3D *rv3d;

  if (walk->state == WALK_RUNNING) {
    return OPERATOR_RUNNING_MODAL;
  }

#ifdef NDOF_WALK_DEBUG
  puts("\n-- walk end --");
#endif

  win = CTX_wm_window(C);
  rv3d = walk->rv3d;

  WM_event_remove_timer(CTX_wm_manager(C), win, walk->timer);

  ED_region_draw_cb_exit(walk->ar->type, walk->draw_handle_pixel);

  ED_transform_snap_object_context_destroy(walk->snap_context);

  ED_view3d_cameracontrol_release(walk->v3d_camera_control, walk->state == WALK_CANCEL);

  rv3d->rflag &= ~RV3D_NAVIGATING;

#ifdef WITH_INPUT_NDOF
  if (walk->ndof) {
    MEM_freeN(walk->ndof);
  }
#endif

  /* restore the cursor */
  WM_cursor_modal_restore(win);

#ifdef USE_TABLET_SUPPORT
  if (walk->is_cursor_absolute == false)
#endif
  {
    /* center the mouse */
    WM_cursor_warp(win,
                   walk->ar->winrct.xmin + walk->center_mval[0],
                   walk->ar->winrct.ymin + walk->center_mval[1]);
  }

  if (walk->state == WALK_CONFIRM) {
    MEM_freeN(walk);
    return OPERATOR_FINISHED;
  }

  MEM_freeN(walk);
  return OPERATOR_CANCELLED;
}

static void walkEvent(bContext *C, WalkInfo *walk, const wmEvent *event)
{
  if (event->type == TIMER && event->customdata == walk->timer) {
    walk->redraw = true;
  }
  else if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {

#ifdef USE_TABLET_SUPPORT
    if (walk->is_cursor_first) {
      /* wait until we get the 'warp' event */
      if ((walk->center_mval[0] == event->mval[0]) && (walk->center_mval[1] == event->mval[1])) {
        walk->is_cursor_first = false;
      }
      else {
        /* note, its possible the system isn't giving us the warp event
         * ideally we shouldn't have to worry about this, see: T45361 */
        wmWindow *win = CTX_wm_window(C);
        WM_cursor_warp(win,
                       walk->ar->winrct.xmin + walk->center_mval[0],
                       walk->ar->winrct.ymin + walk->center_mval[1]);
      }
      return;
    }

    if ((walk->is_cursor_absolute == false) && event->is_motion_absolute) {
      walk->is_cursor_absolute = true;
      copy_v2_v2_int(walk->prev_mval, event->mval);
      copy_v2_v2_int(walk->center_mval, event->mval);
      /* without this we can't turn 180d */
      CLAMP_MIN(walk->mouse_speed, 4.0f);
    }
#endif /* USE_TABLET_SUPPORT */

    walk->moffset[0] += event->mval[0] - walk->prev_mval[0];
    walk->moffset[1] += event->mval[1] - walk->prev_mval[1];

    copy_v2_v2_int(walk->prev_mval, event->mval);

    if ((walk->center_mval[0] != event->mval[0]) || (walk->center_mval[1] != event->mval[1])) {
      walk->redraw = true;

#ifdef USE_TABLET_SUPPORT
      if (walk->is_cursor_absolute) {
        /* pass */
      }
      else
#endif
          if (WM_event_is_last_mousemove(event)) {
        wmWindow *win = CTX_wm_window(C);

#ifdef __APPLE__
        if ((abs(walk->prev_mval[0] - walk->center_mval[0]) > walk->center_mval[0] / 2) ||
            (abs(walk->prev_mval[1] - walk->center_mval[1]) > walk->center_mval[1] / 2))
#endif
        {
          WM_cursor_warp(win,
                         walk->ar->winrct.xmin + walk->center_mval[0],
                         walk->ar->winrct.ymin + walk->center_mval[1]);
          copy_v2_v2_int(walk->prev_mval, walk->center_mval);
        }
      }
    }
  }
#ifdef WITH_INPUT_NDOF
  else if (event->type == NDOF_MOTION) {
    /* do these automagically get delivered? yes. */
    // puts("ndof motion detected in walk mode!");
    // static const char *tag_name = "3D mouse position";

    const wmNDOFMotionData *incoming_ndof = event->customdata;
    switch (incoming_ndof->progress) {
      case P_STARTING:
        /* start keeping track of 3D mouse position */
#  ifdef NDOF_WALK_DEBUG
        puts("start keeping track of 3D mouse position");
#  endif
        /* fall-through */
      case P_IN_PROGRESS:
        /* update 3D mouse position */
#  ifdef NDOF_WALK_DEBUG
        putchar('.');
        fflush(stdout);
#  endif
        if (walk->ndof == NULL) {
          // walk->ndof = MEM_mallocN(sizeof(wmNDOFMotionData), tag_name);
          walk->ndof = MEM_dupallocN(incoming_ndof);
          // walk->ndof = malloc(sizeof(wmNDOFMotionData));
        }
        else {
          memcpy(walk->ndof, incoming_ndof, sizeof(wmNDOFMotionData));
        }
        break;
      case P_FINISHING:
        /* stop keeping track of 3D mouse position */
#  ifdef NDOF_WALK_DEBUG
        puts("stop keeping track of 3D mouse position");
#  endif
        if (walk->ndof) {
          MEM_freeN(walk->ndof);
          // free(walk->ndof);
          walk->ndof = NULL;
        }

        /* update the time else the view will jump when 2D mouse/timer resume */
        walk->time_lastdraw = PIL_check_seconds_timer();

        break;
      default:
        break; /* should always be one of the above 3 */
    }
  }
#endif /* WITH_INPUT_NDOF */
  /* handle modal keymap first */
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case WALK_MODAL_CANCEL:
        walk->state = WALK_CANCEL;
        break;
      case WALK_MODAL_CONFIRM:
        walk->state = WALK_CONFIRM;
        break;

      case WALK_MODAL_ACCELERATE:
        base_speed *= 1.0f + (walk->is_slow ? 0.01f : 0.1f);
        break;
      case WALK_MODAL_DECELERATE:
        base_speed /= 1.0f + (walk->is_slow ? 0.01f : 0.1f);
        break;

      /* implement WASD keys */
      case WALK_MODAL_DIR_FORWARD:
        walk->active_directions |= WALK_BIT_FORWARD;
        break;
      case WALK_MODAL_DIR_BACKWARD:
        walk->active_directions |= WALK_BIT_BACKWARD;
        break;
      case WALK_MODAL_DIR_LEFT:
        walk->active_directions |= WALK_BIT_LEFT;
        break;
      case WALK_MODAL_DIR_RIGHT:
        walk->active_directions |= WALK_BIT_RIGHT;
        break;
      case WALK_MODAL_DIR_UP:
        walk->active_directions |= WALK_BIT_UP;
        break;
      case WALK_MODAL_DIR_DOWN:
        walk->active_directions |= WALK_BIT_DOWN;
        break;

      case WALK_MODAL_DIR_FORWARD_STOP:
        walk->active_directions &= ~WALK_BIT_FORWARD;
        break;
      case WALK_MODAL_DIR_BACKWARD_STOP:
        walk->active_directions &= ~WALK_BIT_BACKWARD;
        break;
      case WALK_MODAL_DIR_LEFT_STOP:
        walk->active_directions &= ~WALK_BIT_LEFT;
        break;
      case WALK_MODAL_DIR_RIGHT_STOP:
        walk->active_directions &= ~WALK_BIT_RIGHT;
        break;
      case WALK_MODAL_DIR_UP_STOP:
        walk->active_directions &= ~WALK_BIT_UP;
        break;
      case WALK_MODAL_DIR_DOWN_STOP:
        walk->active_directions &= ~WALK_BIT_DOWN;
        break;

      case WALK_MODAL_FAST_ENABLE:
        walk->is_fast = true;
        break;
      case WALK_MODAL_FAST_DISABLE:
        walk->is_fast = false;
        break;
      case WALK_MODAL_SLOW_ENABLE:
        walk->is_slow = true;
        break;
      case WALK_MODAL_SLOW_DISABLE:
        walk->is_slow = false;
        break;

#define JUMP_SPEED_MIN 1.0f
#define JUMP_TIME_MAX 0.2f /* s */
#define JUMP_SPEED_MAX sqrtf(2.0f * walk->gravity * walk->jump_height)

      case WALK_MODAL_JUMP_STOP:
        if (walk->gravity_state == WALK_GRAVITY_STATE_JUMP) {
          float t;

          /* delta time */
          t = (float)(PIL_check_seconds_timer() - walk->teleport.initial_time);

          /* reduce the veolocity, if JUMP wasn't hold for long enough */
          t = min_ff(t, JUMP_TIME_MAX);
          walk->speed_jump = JUMP_SPEED_MIN +
                             t * (JUMP_SPEED_MAX - JUMP_SPEED_MIN) / JUMP_TIME_MAX;

          /* when jumping, duration is how long it takes before we start going down */
          walk->teleport.duration = getVelocityZeroTime(walk->gravity, walk->speed_jump);

          /* no more increase of jump speed */
          walk->gravity_state = WALK_GRAVITY_STATE_ON;
        }
        break;
      case WALK_MODAL_JUMP:
        if ((walk->navigation_mode == WALK_MODE_GRAVITY) &&
            (walk->gravity_state == WALK_GRAVITY_STATE_OFF) &&
            (walk->teleport.state == WALK_TELEPORT_STATE_OFF)) {
          /* no need to check for ground,
           * walk->gravity wouldn't be off
           * if we were over a hole */
          walk->gravity_state = WALK_GRAVITY_STATE_JUMP;
          walk->speed_jump = JUMP_SPEED_MAX;

          walk->teleport.initial_time = PIL_check_seconds_timer();
          copy_v3_v3(walk->teleport.origin, walk->rv3d->viewinv[3]);

          /* using previous vec because WASD keys are not called when SPACE is */
          copy_v2_v2(walk->teleport.direction, walk->dvec_prev);

          /* when jumping, duration is how long it takes before we start going down */
          walk->teleport.duration = getVelocityZeroTime(walk->gravity, walk->speed_jump);
        }

        break;

      case WALK_MODAL_TELEPORT: {
        float loc[3], nor[3];
        float distance;
        bool ret = walk_ray_cast(walk->rv3d, walk, loc, nor, &distance);

        /* in case we are teleporting middle way from a jump */
        walk->speed_jump = 0.0f;

        if (ret) {
          WalkTeleport *teleport = &walk->teleport;
          teleport->state = WALK_TELEPORT_STATE_ON;
          teleport->initial_time = PIL_check_seconds_timer();
          teleport->duration = U.walk_navigation.teleport_time;

          teleport->navigation_mode = walk->navigation_mode;
          walk_navigation_mode_set(walk, WALK_MODE_FREE);

          copy_v3_v3(teleport->origin, walk->rv3d->viewinv[3]);

          /* stop the camera from a distance (camera height) */
          normalize_v3_length(nor, walk->view_height);
          add_v3_v3(loc, nor);

          sub_v3_v3v3(teleport->direction, loc, teleport->origin);
        }
        else {
          walk->teleport.state = WALK_TELEPORT_STATE_OFF;
        }
        break;
      }

#undef JUMP_SPEED_MAX
#undef JUMP_TIME_MAX
#undef JUMP_SPEED_MIN

      case WALK_MODAL_TOGGLE:
        if (walk->navigation_mode == WALK_MODE_GRAVITY) {
          walk_navigation_mode_set(walk, WALK_MODE_FREE);
        }
        else { /* WALK_MODE_FREE */
          walk_navigation_mode_set(walk, WALK_MODE_GRAVITY);
        }
        break;
    }
  }
}

static void walkMoveCamera(bContext *C,
                           WalkInfo *walk,
                           const bool do_rotate,
                           const bool do_translate)
{
  ED_view3d_cameracontrol_update(walk->v3d_camera_control, true, C, do_rotate, do_translate);
}

static float getFreeFallDistance(const float gravity, const float time)
{
  return gravity * (time * time) * 0.5f;
}

static float getVelocityZeroTime(const float gravity, const float velocity)
{
  return velocity / gravity;
}

static int walkApply(bContext *C, WalkInfo *walk)
{
#define WALK_ROTATE_FAC 2.2f /* more is faster */
#define WALK_TOP_LIMIT DEG2RADF(85.0f)
#define WALK_BOTTOM_LIMIT DEG2RADF(-80.0f)
#define WALK_MOVE_SPEED base_speed
#define WALK_BOOST_FACTOR ((void)0, walk->speed_factor)

  /* walk mode - Ctrl+Shift+F
   * a walk loop where the user can move move the view as if they are in a walk game
   */
  RegionView3D *rv3d = walk->rv3d;
  ARegion *ar = walk->ar;

  /* 3x3 copy of the view matrix so we can move along the view axis */
  float mat[3][3];
  /* this is the direction that's added to the view offset per redraw */
  float dvec[3] = {0.0f, 0.0f, 0.0f};

  int moffset[2];    /* mouse offset from the views center */
  float tmp_quat[4]; /* used for rotating the view */

#ifdef NDOF_WALK_DEBUG
  {
    static uint iteration = 1;
    printf("walk timer %d\n", iteration++);
  }
#endif

  {
    /* mouse offset from the center */
    copy_v2_v2_int(moffset, walk->moffset);

    /* apply moffset so we can re-accumulate */
    walk->moffset[0] = 0;
    walk->moffset[1] = 0;

    /* revert mouse */
    if (walk->is_reversed) {
      moffset[1] = -moffset[1];
    }

    /* Should we redraw? */
    if ((walk->active_directions) || moffset[0] || moffset[1] ||
        walk->teleport.state == WALK_TELEPORT_STATE_ON ||
        walk->gravity_state != WALK_GRAVITY_STATE_OFF) {
      float dvec_tmp[3];

      /* time how fast it takes for us to redraw,
       * this is so simple scenes don't walk too fast */
      double time_current;
      float time_redraw;
#ifdef NDOF_WALK_DRAW_TOOMUCH
      walk->redraw = 1;
#endif
      time_current = PIL_check_seconds_timer();
      time_redraw = (float)(time_current - walk->time_lastdraw);

      walk->time_lastdraw = time_current;

      /* base speed in m/s */
      walk->speed = WALK_MOVE_SPEED;

      if (walk->is_fast) {
        walk->speed *= WALK_BOOST_FACTOR;
      }
      else if (walk->is_slow) {
        walk->speed *= 1.0f / WALK_BOOST_FACTOR;
      }

      copy_m3_m4(mat, rv3d->viewinv);

      {
        /* rotate about the X axis- look up/down */
        if (moffset[1]) {
          float upvec[3];
          float angle;
          float y;

          /* relative offset */
          y = (float)moffset[1] / ar->winy;

          /* speed factor */
          y *= WALK_ROTATE_FAC;

          /* user adjustment factor */
          y *= walk->mouse_speed;

          /* clamp the angle limits */
          /* it ranges from 90.0f to -90.0f */
          angle = -asinf(rv3d->viewmat[2][2]);

          if (angle > WALK_TOP_LIMIT && y > 0.0f) {
            y = 0.0f;
          }
          else if (angle < WALK_BOTTOM_LIMIT && y < 0.0f) {
            y = 0.0f;
          }

          copy_v3_fl3(upvec, 1.0f, 0.0f, 0.0f);
          mul_m3_v3(mat, upvec);
          /* Rotate about the relative up vec */
          axis_angle_to_quat(tmp_quat, upvec, -y);
          mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);
        }

        /* rotate about the Y axis- look left/right */
        if (moffset[0]) {
          float upvec[3];
          float x;

          /* if we're upside down invert the moffset */
          copy_v3_fl3(upvec, 0.0f, 1.0f, 0.0f);
          mul_m3_v3(mat, upvec);

          if (upvec[2] < 0.0f) {
            moffset[0] = -moffset[0];
          }

          /* relative offset */
          x = (float)moffset[0] / ar->winx;

          /* speed factor */
          x *= WALK_ROTATE_FAC;

          /* user adjustment factor */
          x *= walk->mouse_speed;

          /* Rotate about the relative up vec */
          axis_angle_to_quat_single(tmp_quat, 'Z', x);
          mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);
        }
      }

      /* WASD - 'move' translation code */
      if ((walk->active_directions) && (walk->gravity_state == WALK_GRAVITY_STATE_OFF)) {

        short direction;
        zero_v3(dvec);

        if ((walk->active_directions & WALK_BIT_FORWARD) ||
            (walk->active_directions & WALK_BIT_BACKWARD)) {

          direction = 0;

          if ((walk->active_directions & WALK_BIT_FORWARD)) {
            direction += 1;
          }

          if ((walk->active_directions & WALK_BIT_BACKWARD)) {
            direction -= 1;
          }

          copy_v3_fl3(dvec_tmp, 0.0f, 0.0f, direction);
          mul_m3_v3(mat, dvec_tmp);

          if (walk->navigation_mode == WALK_MODE_GRAVITY) {
            dvec_tmp[2] = 0.0f;
          }

          normalize_v3(dvec_tmp);
          add_v3_v3(dvec, dvec_tmp);
        }

        if ((walk->active_directions & WALK_BIT_LEFT) ||
            (walk->active_directions & WALK_BIT_RIGHT)) {

          direction = 0;

          if ((walk->active_directions & WALK_BIT_LEFT)) {
            direction += 1;
          }

          if ((walk->active_directions & WALK_BIT_RIGHT)) {
            direction -= 1;
          }

          dvec_tmp[0] = direction * rv3d->viewinv[0][0];
          dvec_tmp[1] = direction * rv3d->viewinv[0][1];
          dvec_tmp[2] = 0.0f;

          normalize_v3(dvec_tmp);
          add_v3_v3(dvec, dvec_tmp);
        }

        if ((walk->active_directions & WALK_BIT_UP) || (walk->active_directions & WALK_BIT_DOWN)) {

          if (walk->navigation_mode == WALK_MODE_FREE) {

            direction = 0;

            if ((walk->active_directions & WALK_BIT_UP)) {
              direction -= 1;
            }

            if ((walk->active_directions & WALK_BIT_DOWN)) {
              direction = 1;
            }

            copy_v3_fl3(dvec_tmp, 0.0f, 0.0f, direction);
            add_v3_v3(dvec, dvec_tmp);
          }
        }

        /* apply movement */
        mul_v3_fl(dvec, walk->speed * time_redraw);
      }

      /* stick to the floor */
      if (walk->navigation_mode == WALK_MODE_GRAVITY &&
          ELEM(walk->gravity_state, WALK_GRAVITY_STATE_OFF, WALK_GRAVITY_STATE_START)) {

        bool ret;
        float ray_distance;
        float difference = -100.0f;
        float fall_distance;

        ret = walk_floor_distance_get(rv3d, walk, dvec, &ray_distance);

        if (ret) {
          difference = walk->view_height - ray_distance;
        }

        /* the distance we would fall naturally smoothly enough that we
         * can manually drop the object without activating gravity */
        fall_distance = time_redraw * walk->speed * WALK_BOOST_FACTOR;

        if (fabsf(difference) < fall_distance) {
          /* slope/stairs */
          dvec[2] -= difference;

          /* in case we switched from FREE to GRAVITY too close to the ground */
          if (walk->gravity_state == WALK_GRAVITY_STATE_START) {
            walk->gravity_state = WALK_GRAVITY_STATE_OFF;
          }
        }
        else {
          /* hijack the teleport variables */
          walk->teleport.initial_time = PIL_check_seconds_timer();
          walk->gravity_state = WALK_GRAVITY_STATE_ON;
          walk->teleport.duration = 0.0f;

          copy_v3_v3(walk->teleport.origin, walk->rv3d->viewinv[3]);
          copy_v2_v2(walk->teleport.direction, dvec);
        }
      }

      /* Falling or jumping) */
      if (ELEM(walk->gravity_state, WALK_GRAVITY_STATE_ON, WALK_GRAVITY_STATE_JUMP)) {
        float t;
        float z_cur, z_new;
        bool ret;
        float ray_distance, difference = -100.0f;

        /* delta time */
        t = (float)(PIL_check_seconds_timer() - walk->teleport.initial_time);

        /* keep moving if we were moving */
        copy_v2_v2(dvec, walk->teleport.direction);

        z_cur = walk->rv3d->viewinv[3][2];
        z_new = walk->teleport.origin[2] - getFreeFallDistance(walk->gravity, t) * walk->grid;

        /* jump */
        z_new += t * walk->speed_jump * walk->grid;

        /* duration is the jump duration */
        if (t > walk->teleport.duration) {

          /* check to see if we are landing */
          ret = walk_floor_distance_get(rv3d, walk, dvec, &ray_distance);

          if (ret) {
            difference = walk->view_height - ray_distance;
          }

          if (difference > 0.0f) {
            /* quit falling, lands at "view_height" from the floor */
            dvec[2] -= difference;
            walk->gravity_state = WALK_GRAVITY_STATE_OFF;
            walk->speed_jump = 0.0f;
          }
          else {
            /* keep falling */
            dvec[2] = z_cur - z_new;
          }
        }
        else {
          /* keep going up (jump) */
          dvec[2] = z_cur - z_new;
        }
      }

      /* Teleport */
      else if (walk->teleport.state == WALK_TELEPORT_STATE_ON) {
        float t; /* factor */
        float new_loc[3];
        float cur_loc[3];

        /* linear interpolation */
        t = (float)(PIL_check_seconds_timer() - walk->teleport.initial_time);
        t /= walk->teleport.duration;

        /* clamp so we don't go past our limit */
        if (t >= 1.0f) {
          t = 1.0f;
          walk->teleport.state = WALK_TELEPORT_STATE_OFF;
          walk_navigation_mode_set(walk, walk->teleport.navigation_mode);
        }

        mul_v3_v3fl(new_loc, walk->teleport.direction, t);
        add_v3_v3(new_loc, walk->teleport.origin);

        copy_v3_v3(cur_loc, walk->rv3d->viewinv[3]);
        sub_v3_v3v3(dvec, cur_loc, new_loc);
      }

      if (rv3d->persp == RV3D_CAMOB) {
        Object *lock_ob = ED_view3d_cameracontrol_object_get(walk->v3d_camera_control);
        if (lock_ob->protectflag & OB_LOCK_LOCX) {
          dvec[0] = 0.0f;
        }
        if (lock_ob->protectflag & OB_LOCK_LOCY) {
          dvec[1] = 0.0f;
        }
        if (lock_ob->protectflag & OB_LOCK_LOCZ) {
          dvec[2] = 0.0f;
        }
      }

      /* scale the movement to the scene size */
      mul_v3_v3fl(dvec_tmp, dvec, walk->grid);
      add_v3_v3(rv3d->ofs, dvec_tmp);

      if (rv3d->persp == RV3D_CAMOB) {
        const bool do_rotate = (moffset[0] || moffset[1]);
        const bool do_translate = (walk->speed != 0.0f);
        walkMoveCamera(C, walk, do_rotate, do_translate);
      }
    }
    else {
      /* we're not redrawing but we need to update the time else the view will jump */
      walk->time_lastdraw = PIL_check_seconds_timer();
    }
    /* end drawing */
    copy_v3_v3(walk->dvec_prev, dvec);
  }

  return OPERATOR_FINISHED;
#undef WALK_ROTATE_FAC
#undef WALK_ZUP_CORRECT_FAC
#undef WALK_ZUP_CORRECT_ACCEL
#undef WALK_SMOOTH_FAC
#undef WALK_TOP_LIMIT
#undef WALK_BOTTOM_LIMIT
#undef WALK_MOVE_SPEED
#undef WALK_BOOST_FACTOR
}

#ifdef WITH_INPUT_NDOF
static void walkApply_ndof(bContext *C, WalkInfo *walk)
{
  Object *lock_ob = ED_view3d_cameracontrol_object_get(walk->v3d_camera_control);
  bool has_translate, has_rotate;

  view3d_ndof_fly(walk->ndof,
                  walk->v3d,
                  walk->rv3d,
                  walk->is_slow,
                  lock_ob ? lock_ob->protectflag : 0,
                  &has_translate,
                  &has_rotate);

  if (has_translate || has_rotate) {
    walk->redraw = true;

    if (walk->rv3d->persp == RV3D_CAMOB) {
      walkMoveCamera(C, walk, has_rotate, has_translate);
    }
  }
}
#endif /* WITH_INPUT_NDOF */

/****** walk operator ******/
static int walk_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  WalkInfo *walk;

  if (rv3d->viewlock & RV3D_LOCKED) {
    return OPERATOR_CANCELLED;
  }

  walk = MEM_callocN(sizeof(WalkInfo), "NavigationWalkOperation");

  op->customdata = walk;

  if (initWalkInfo(C, walk, op) == false) {
    MEM_freeN(op->customdata);
    return OPERATOR_CANCELLED;
  }

  walkEvent(C, walk, event);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void walk_cancel(bContext *C, wmOperator *op)
{
  WalkInfo *walk = op->customdata;

  walk->state = WALK_CANCEL;
  walkEnd(C, walk);
  op->customdata = NULL;
}

static int walk_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  int exit_code;
  bool do_draw = false;
  WalkInfo *walk = op->customdata;
  RegionView3D *rv3d = walk->rv3d;
  Object *walk_object = ED_view3d_cameracontrol_object_get(walk->v3d_camera_control);

  walk->redraw = false;

  walkEvent(C, walk, event);

#ifdef WITH_INPUT_NDOF
  if (walk->ndof) { /* 3D mouse overrules [2D mouse + timer] */
    if (event->type == NDOF_MOTION) {
      walkApply_ndof(C, walk);
    }
  }
  else
#endif /* WITH_INPUT_NDOF */
      if (event->type == TIMER && event->customdata == walk->timer) {
    walkApply(C, walk);
  }

  do_draw |= walk->redraw;

  exit_code = walkEnd(C, walk);

  if (exit_code != OPERATOR_RUNNING_MODAL) {
    do_draw = true;
  }

  if (do_draw) {
    if (rv3d->persp == RV3D_CAMOB) {
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, walk_object);
    }

    // too frequent, commented with NDOF_WALK_DRAW_TOOMUCH for now
    // puts("redraw!");
    ED_region_tag_redraw(CTX_wm_region(C));
  }
  return exit_code;
}

void VIEW3D_OT_walk(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Walk Navigation";
  ot->description = "Interactively walk around the scene";
  ot->idname = "VIEW3D_OT_walk";

  /* api callbacks */
  ot->invoke = walk_invoke;
  ot->cancel = walk_cancel;
  ot->modal = walk_modal;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;
}
