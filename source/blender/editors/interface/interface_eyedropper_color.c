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
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 *
 * Eyedropper (RGB Color)
 *
 * Defines:
 * - #UI_OT_eyedropper_color
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_cryptomatte.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "interface_intern.h"

#include "ED_clip.h"
#include "ED_image.h"
#include "ED_node.h"
#include "ED_screen.h"

#include "RE_pipeline.h"

#include "RE_pipeline.h"

#include "interface_eyedropper_intern.h"

typedef struct Eyedropper {
  struct ColorManagedDisplay *display;

  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  bool is_undo;

  bool is_set;
  float init_col[3]; /* for resetting on cancel */

  bool accum_start; /* has mouse been pressed */
  float accum_col[3];
  int accum_tot;

  void *draw_handle_sample_text;
  char sample_text[MAX_NAME];

  bNode *crypto_node;
  struct CryptomatteSession *cryptomatte_session;
} Eyedropper;

static void eyedropper_draw_cb(const wmWindow *window, void *arg)
{
  Eyedropper *eye = arg;
  eyedropper_draw_cursor_text_window(window, eye->sample_text);
}

static bool eyedropper_init(bContext *C, wmOperator *op)
{
  Eyedropper *eye = MEM_callocN(sizeof(Eyedropper), __func__);

  uiBut *but = UI_context_active_but_prop_get(C, &eye->ptr, &eye->prop, &eye->index);
  const enum PropertySubType prop_subtype = eye->prop ? RNA_property_subtype(eye->prop) : 0;

  if ((eye->ptr.data == NULL) || (eye->prop == NULL) ||
      (RNA_property_editable(&eye->ptr, eye->prop) == false) ||
      (RNA_property_array_length(&eye->ptr, eye->prop) < 3) ||
      (RNA_property_type(eye->prop) != PROP_FLOAT) ||
      (ELEM(prop_subtype, PROP_COLOR, PROP_COLOR_GAMMA) == 0)) {
    MEM_freeN(eye);
    return false;
  }
  op->customdata = eye;

  eye->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);

  float col[4];
  RNA_property_float_get_array(&eye->ptr, eye->prop, col);
  if (eye->ptr.type == &RNA_CompositorNodeCryptomatteV2) {
    eye->crypto_node = (bNode *)eye->ptr.data;
    eye->cryptomatte_session = ntreeCompositCryptomatteSession(CTX_data_scene(C),
                                                               eye->crypto_node);
    eye->draw_handle_sample_text = WM_draw_cb_activate(CTX_wm_window(C), eyedropper_draw_cb, eye);
  }

  if (prop_subtype != PROP_COLOR) {
    Scene *scene = CTX_data_scene(C);
    const char *display_device;

    display_device = scene->display_settings.display_device;
    eye->display = IMB_colormanagement_display_get_named(display_device);

    /* store initial color */
    if (eye->display) {
      IMB_colormanagement_display_to_scene_linear_v3(col, eye->display);
    }
  }
  copy_v3_v3(eye->init_col, col);

  return true;
}

static void eyedropper_exit(bContext *C, wmOperator *op)
{
  Eyedropper *eye = op->customdata;
  wmWindow *window = CTX_wm_window(C);
  WM_cursor_modal_restore(window);

  if (eye->draw_handle_sample_text) {
    WM_draw_cb_exit(window, eye->draw_handle_sample_text);
    eye->draw_handle_sample_text = NULL;
  }

  if (eye->cryptomatte_session) {
    BKE_cryptomatte_free(eye->cryptomatte_session);
    eye->cryptomatte_session = NULL;
  }

  MEM_SAFE_FREE(op->customdata);
}

/* *** eyedropper_color_ helper functions *** */

static bool eyedropper_cryptomatte_sample_renderlayer_fl(RenderLayer *render_layer,
                                                         const char *prefix,
                                                         const float fpos[2],
                                                         float r_col[3])
{
  if (!render_layer) {
    return false;
  }

  const int render_layer_name_len = BLI_strnlen(render_layer->name, sizeof(render_layer->name));
  if (strncmp(prefix, render_layer->name, render_layer_name_len) != 0) {
    return false;
  }

  const int prefix_len = strlen(prefix);
  if (prefix_len <= render_layer_name_len + 1) {
    return false;
  }

  /* RenderResult from images can have no render layer name. */
  const char *render_pass_name_prefix = render_layer_name_len ?
                                            prefix + 1 + render_layer_name_len :
                                            prefix;

  LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
    if (STRPREFIX(render_pass->name, render_pass_name_prefix) &&
        !STREQLEN(render_pass->name, render_pass_name_prefix, sizeof(render_pass->name))) {
      BLI_assert(render_pass->channels == 4);
      const int x = (int)(fpos[0] * render_pass->rectx);
      const int y = (int)(fpos[1] * render_pass->recty);
      const int offset = 4 * (y * render_pass->rectx + x);
      zero_v3(r_col);
      r_col[0] = render_pass->rect[offset];
      return true;
    }
  }

  return false;
}
static bool eyedropper_cryptomatte_sample_render_fl(const bNode *node,
                                                    const char *prefix,
                                                    const float fpos[2],
                                                    float r_col[3])
{
  bool success = false;
  Scene *scene = (Scene *)node->id;
  BLI_assert(GS(scene->id.name) == ID_SCE);
  Render *re = RE_GetSceneRender(scene);

  if (re) {
    RenderResult *rr = RE_AcquireResultRead(re);
    if (rr) {
      LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
        RenderLayer *render_layer = RE_GetRenderLayer(rr, view_layer->name);
        success = eyedropper_cryptomatte_sample_renderlayer_fl(render_layer, prefix, fpos, r_col);
        if (success) {
          break;
        }
      }
    }
    RE_ReleaseResult(re);
  }
  return success;
}

static bool eyedropper_cryptomatte_sample_image_fl(const bNode *node,
                                                   NodeCryptomatte *crypto,
                                                   const char *prefix,
                                                   const float fpos[2],
                                                   float r_col[3])
{
  bool success = false;
  Image *image = (Image *)node->id;
  BLI_assert((image == NULL) || (GS(image->id.name) == ID_IM));
  ImageUser *iuser = &crypto->iuser;

  if (image && image->type == IMA_TYPE_MULTILAYER) {
    ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, NULL);
    if (image->rr) {
      LISTBASE_FOREACH (RenderLayer *, render_layer, &image->rr->layers) {
        success = eyedropper_cryptomatte_sample_renderlayer_fl(render_layer, prefix, fpos, r_col);
        if (success) {
          break;
        }
      }
    }
    BKE_image_release_ibuf(image, ibuf, NULL);
  }
  return success;
}

static bool eyedropper_cryptomatte_sample_fl(
    bContext *C, Eyedropper *eye, int mx, int my, float r_col[3])
{
  bNode *node = eye->crypto_node;
  NodeCryptomatte *crypto = node ? ((NodeCryptomatte *)node->storage) : NULL;

  if (!crypto) {
    return false;
  }

  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, (const int[2]){mx, my});
  if (!area || !ELEM(area->spacetype, SPACE_IMAGE, SPACE_NODE, SPACE_CLIP)) {
    return false;
  }

  ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, (const int[2]){mx, my});
  if (!region) {
    return false;
  }

  int mval[2] = {mx - region->winrct.xmin, my - region->winrct.ymin};
  float fpos[2] = {-1.0f, -1.0};
  switch (area->spacetype) {
    case SPACE_IMAGE: {
      SpaceImage *sima = area->spacedata.first;
      ED_space_image_get_position(sima, region, mval, fpos);
      break;
    }
    case SPACE_NODE: {
      Main *bmain = CTX_data_main(C);
      SpaceNode *snode = area->spacedata.first;
      ED_space_node_get_position(bmain, snode, region, mval, fpos);
      break;
    }
    case SPACE_CLIP: {
      SpaceClip *sc = area->spacedata.first;
      ED_space_clip_get_position(sc, region, mval, fpos);
      break;
    }
    default: {
      break;
    }
  }

  if (fpos[0] < 0.0f || fpos[1] < 0.0f || fpos[0] >= 1.0f || fpos[1] >= 1.0f) {
    return false;
  }

  /* CMP_CRYPTOMATTE_SRC_RENDER and CMP_CRYPTOMATTE_SRC_IMAGE require a referenced image/scene to
   * work properly. */
  if (!node->id) {
    return false;
  }

  /* TODO(jbakker): Migrate this file to cc and use std::string as return param. */
  char prefix[MAX_NAME + 1];
  const Scene *scene = CTX_data_scene(C);
  ntreeCompositCryptomatteLayerPrefix(scene, node, prefix, sizeof(prefix) - 1);
  prefix[MAX_NAME] = '\0';

  if (node->custom1 == CMP_CRYPTOMATTE_SRC_RENDER) {
    return eyedropper_cryptomatte_sample_render_fl(node, prefix, fpos, r_col);
  }
  if (node->custom1 == CMP_CRYPTOMATTE_SRC_IMAGE) {
    return eyedropper_cryptomatte_sample_image_fl(node, crypto, prefix, fpos, r_col);
  }
  return false;
}

void eyedropper_color_sample_fl(bContext *C, int mx, int my, float r_col[3])
{
  /* we could use some clever */
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const char *display_device = CTX_data_scene(C)->display_settings.display_device;
  struct ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

  wmWindow *win;
  ScrArea *area;
  int mval[2] = {mx, my};
  datadropper_win_area_find(C, mval, mval, &win, &area);

  if (area) {
    if (area->spacetype == SPACE_IMAGE) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mval);
      if (region) {
        SpaceImage *sima = area->spacedata.first;
        int region_mval[2] = {mval[0] - region->winrct.xmin, mval[1] - region->winrct.ymin};

        if (ED_space_image_color_sample(sima, region, region_mval, r_col, NULL)) {
          return;
        }
      }
    }
    else if (area->spacetype == SPACE_NODE) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mval);
      if (region) {
        SpaceNode *snode = area->spacedata.first;
        int region_mval[2] = {mval[0] - region->winrct.xmin, mval[1] - region->winrct.ymin};

        if (ED_space_node_color_sample(bmain, snode, region, region_mval, r_col)) {
          return;
        }
      }
    }
    else if (area->spacetype == SPACE_CLIP) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mval);
      if (region) {
        SpaceClip *sc = area->spacedata.first;
        int region_mval[2] = {mval[0] - region->winrct.xmin, mval[1] - region->winrct.ymin};

        if (ED_space_clip_color_sample(sc, region, region_mval, r_col)) {
          return;
        }
      }
    }
  }

  if (win) {
    /* Fallback to simple opengl picker. */
    WM_window_pixel_sample_read(wm, win, mval, r_col);
    IMB_colormanagement_display_to_scene_linear_v3(r_col, display);
  }
  else {
    zero_v3(r_col);
  }
}

/* sets the sample color RGB, maintaining A */
static void eyedropper_color_set(bContext *C, Eyedropper *eye, const float col[3])
{
  float col_conv[4];

  /* to maintain alpha */
  RNA_property_float_get_array(&eye->ptr, eye->prop, col_conv);

  /* convert from linear rgb space to display space */
  if (eye->display) {
    copy_v3_v3(col_conv, col);
    IMB_colormanagement_scene_linear_to_display_v3(col_conv, eye->display);
  }
  else {
    copy_v3_v3(col_conv, col);
  }

  RNA_property_float_set_array(&eye->ptr, eye->prop, col_conv);
  eye->is_set = true;

  RNA_property_update(C, &eye->ptr, eye->prop);
}

static void eyedropper_color_sample(bContext *C, Eyedropper *eye, int mx, int my)
{
  /* Accumulate color. */
  float col[3];
  if (eye->crypto_node) {
    if (!eyedropper_cryptomatte_sample_fl(C, eye, mx, my, col)) {
      return;
    }
  }
  else {
    eyedropper_color_sample_fl(C, mx, my, col);
  }

  if (!eye->crypto_node) {
    add_v3_v3(eye->accum_col, col);
    eye->accum_tot++;
  }
  else {
    copy_v3_v3(eye->accum_col, col);
    eye->accum_tot = 1;
  }

  /* Apply to property. */
  float accum_col[3];
  if (eye->accum_tot > 1) {
    mul_v3_v3fl(accum_col, eye->accum_col, 1.0f / (float)eye->accum_tot);
  }
  else {
    copy_v3_v3(accum_col, eye->accum_col);
  }
  eyedropper_color_set(C, eye, accum_col);
}

static void eyedropper_color_sample_text_update(bContext *C, Eyedropper *eye, int mx, int my)
{
  float col[3];
  eye->sample_text[0] = '\0';

  if (eye->cryptomatte_session) {
    if (eyedropper_cryptomatte_sample_fl(C, eye, mx, my, col)) {
      BKE_cryptomatte_find_name(
          eye->cryptomatte_session, col[0], eye->sample_text, sizeof(eye->sample_text));
      eye->sample_text[sizeof(eye->sample_text) - 1] = '\0';
    }
  }
}

static void eyedropper_cancel(bContext *C, wmOperator *op)
{
  Eyedropper *eye = op->customdata;
  if (eye->is_set) {
    eyedropper_color_set(C, eye, eye->init_col);
  }
  eyedropper_exit(C, op);
}

/* main modal status check */
static int eyedropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Eyedropper *eye = (Eyedropper *)op->customdata;

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        eyedropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = eye->is_undo;
        if (eye->accum_tot == 0) {
          eyedropper_color_sample(C, eye, event->xy[0], event->xy[1]);
        }
        eyedropper_exit(C, op);
        /* Could support finished & undo-skip. */
        return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
      }
      case EYE_MODAL_SAMPLE_BEGIN:
        /* enable accum and make first sample */
        eye->accum_start = true;
        eyedropper_color_sample(C, eye, event->xy[0], event->xy[1]);
        break;
      case EYE_MODAL_SAMPLE_RESET:
        eye->accum_tot = 0;
        zero_v3(eye->accum_col);
        eyedropper_color_sample(C, eye, event->xy[0], event->xy[1]);
        break;
    }
  }
  else if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    if (eye->accum_start) {
      /* button is pressed so keep sampling */
      eyedropper_color_sample(C, eye, event->xy[0], event->xy[1]);
    }

    if (eye->draw_handle_sample_text) {
      eyedropper_color_sample_text_update(C, eye, event->xy[0], event->xy[1]);
      ED_region_tag_redraw(CTX_wm_region(C));
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int eyedropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* init */
  if (eyedropper_init(C, op)) {
    wmWindow *win = CTX_wm_window(C);
    /* Workaround for de-activating the button clearing the cursor, see T76794 */
    UI_context_active_but_clear(C, win, CTX_wm_region(C));
    WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

/* Repeat operator */
static int eyedropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (eyedropper_init(C, op)) {

    /* do something */

    /* cleanup */
    eyedropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

static bool eyedropper_poll(bContext *C)
{
  /* Actual test for active button happens later, since we don't
   * know which one is active until mouse over. */
  return (CTX_wm_window(C) != NULL);
}

void UI_OT_eyedropper_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper";
  ot->idname = "UI_OT_eyedropper_color";
  ot->description = "Sample a color from the Blender window to store in a property";

  /* api callbacks */
  ot->invoke = eyedropper_invoke;
  ot->modal = eyedropper_modal;
  ot->cancel = eyedropper_cancel;
  ot->exec = eyedropper_exec;
  ot->poll = eyedropper_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;
}
