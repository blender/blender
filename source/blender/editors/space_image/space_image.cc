/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_image_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "IMB_imbuf_types.hh"

#include "ED_asset_shelf.hh"
#include "ED_image.hh"
#include "ED_mask.hh"
#include "ED_node.hh"
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_transform.hh"
#include "ED_util.hh"
#include "ED_uvedit.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "DRW_engine.hh"

#include "image_intern.hh"

/**************************** common state *****************************/

static void image_scopes_tag_refresh(ScrArea *area)
{
  SpaceImage *sima = (SpaceImage *)area->spacedata.first;

  /* only while histogram is visible */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_TOOL_PROPS && region->flag & RGN_FLAG_HIDDEN) {
      return;
    }
  }

  sima->scopes.ok = 0;
}

static void image_user_refresh_scene(const bContext *C, SpaceImage *sima)
{
  /* Update scene image user for acquiring render results. */
  Scene *sequencer_scene = CTX_data_sequencer_scene(C);
  sima->iuser.scene = (sima->iuser.flag & IMA_SHOW_SEQUENCER_SCENE) && sequencer_scene ?
                          sequencer_scene :
                          CTX_data_scene(C);

  if (sima->image && sima->image->type == IMA_TYPE_R_RESULT) {
    /* While rendering, prefer scene that is being rendered. */
    Scene *render_scene = ED_render_job_get_current_scene(C);
    if (render_scene) {
      sima->iuser.scene = render_scene;
      SET_FLAG_FROM_TEST(
          sima->iuser.flag, render_scene == CTX_data_sequencer_scene(C), IMA_SHOW_SEQUENCER_SCENE);
    }
  }

  /* Auto switch image to show in UV editor when selection changes. */
  ED_space_image_auto_set(C, sima);
}

/* ******************** default callbacks for image space ***************** */

static SpaceLink *image_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceImage *simage;

  simage = MEM_callocN<SpaceImage>("initimage");
  simage->spacetype = SPACE_IMAGE;
  simage->zoom = 1.0f;
  simage->lock = true;
  simage->flag = SI_SHOW_GPENCIL | SI_USE_ALPHA | SI_COORDFLOATS;
  simage->uv_opacity = 1.0f;
  simage->uv_face_opacity = 1.0f;
  simage->stretch_opacity = 1.0f;
  simage->overlay.flag = SI_OVERLAY_SHOW_OVERLAYS | SI_OVERLAY_SHOW_GRID_BACKGROUND;
  simage->overlay.passepartout_alpha = 0.5f;

  BKE_imageuser_default(&simage->iuser);
  simage->iuser.flag = IMA_SHOW_STEREO | IMA_ANIM_ALWAYS;

  BKE_scopes_new(&simage->scopes);
  simage->sample_line_hist.height = 100;

  simage->tile_grid_shape[0] = 1;
  simage->tile_grid_shape[1] = 1;

  simage->custom_grid_subdiv[0] = 10;
  simage->custom_grid_subdiv[1] = 10;

  simage->mask_info = *DNA_struct_default_get(MaskSpaceInfo);

  /* header */
  region = BKE_area_region_new();

  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* asset shelf */
  region = BKE_area_region_new();
  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_ASSET_SHELF;
  region->alignment = RGN_ALIGN_BOTTOM;
  region->flag |= RGN_FLAG_HIDDEN;

  /* asset shelf header */
  region = BKE_area_region_new();
  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_ASSET_SHELF_HEADER;
  region->alignment = RGN_ALIGN_BOTTOM | RGN_ALIGN_HIDE_WITH_PREV;

  /* tool header */
  region = BKE_area_region_new();

  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_TOOL_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  region->flag = RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;

  /* buttons/list view */
  region = BKE_area_region_new();

  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;
  region->flag = RGN_FLAG_HIDDEN;

  /* scopes/uv sculpt/paint */
  region = BKE_area_region_new();

  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;
  region->flag = RGN_FLAG_HIDDEN;

  /* main area */
  region = BKE_area_region_new();

  BLI_addtail(&simage->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)simage;
}

/* Doesn't free the space-link itself. */
static void image_free(SpaceLink *sl)
{
  SpaceImage *simage = (SpaceImage *)sl;

  BKE_scopes_free(&simage->scopes);
}

/* spacetype; init callback, add handlers */
static void image_init(wmWindowManager * /*wm*/, ScrArea *area)
{
  ListBase *lb = WM_dropboxmap_find("Image", SPACE_IMAGE, RGN_TYPE_WINDOW);

  /* add drop boxes */
  WM_event_add_dropbox_handler(&area->handlers, lb);
}

static SpaceLink *image_duplicate(SpaceLink *sl)
{
  SpaceImage *simagen = static_cast<SpaceImage *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */

  BKE_scopes_new(&simagen->scopes);

  return (SpaceLink *)simagen;
}

static void image_operatortypes()
{
  WM_operatortype_append(IMAGE_OT_view_all);
  WM_operatortype_append(IMAGE_OT_view_pan);
  WM_operatortype_append(IMAGE_OT_view_selected);
  WM_operatortype_append(IMAGE_OT_view_center_cursor);
  WM_operatortype_append(IMAGE_OT_view_cursor_center);
  WM_operatortype_append(IMAGE_OT_view_zoom);
  WM_operatortype_append(IMAGE_OT_view_zoom_in);
  WM_operatortype_append(IMAGE_OT_view_zoom_out);
  WM_operatortype_append(IMAGE_OT_view_zoom_ratio);
  WM_operatortype_append(IMAGE_OT_view_zoom_border);
#ifdef WITH_INPUT_NDOF
  WM_operatortype_append(IMAGE_OT_view_ndof);
#endif

  WM_operatortype_append(IMAGE_OT_new);
  WM_operatortype_append(IMAGE_OT_open);
  WM_operatortype_append(IMAGE_OT_file_browse);
  WM_operatortype_append(IMAGE_OT_match_movie_length);
  WM_operatortype_append(IMAGE_OT_replace);
  WM_operatortype_append(IMAGE_OT_reload);
  WM_operatortype_append(IMAGE_OT_save);
  WM_operatortype_append(IMAGE_OT_save_as);
  WM_operatortype_append(IMAGE_OT_save_sequence);
  WM_operatortype_append(IMAGE_OT_save_all_modified);
  WM_operatortype_append(IMAGE_OT_pack);
  WM_operatortype_append(IMAGE_OT_unpack);
  WM_operatortype_append(IMAGE_OT_clipboard_copy);
  WM_operatortype_append(IMAGE_OT_clipboard_paste);

  WM_operatortype_append(IMAGE_OT_flip);
  WM_operatortype_append(IMAGE_OT_rotate_orthogonal);
  WM_operatortype_append(IMAGE_OT_invert);
  WM_operatortype_append(IMAGE_OT_resize);

  WM_operatortype_append(IMAGE_OT_cycle_render_slot);
  WM_operatortype_append(IMAGE_OT_clear_render_slot);
  WM_operatortype_append(IMAGE_OT_add_render_slot);
  WM_operatortype_append(IMAGE_OT_remove_render_slot);

  WM_operatortype_append(IMAGE_OT_sample);
  WM_operatortype_append(IMAGE_OT_sample_line);
  WM_operatortype_append(IMAGE_OT_curves_point_set);

  WM_operatortype_append(IMAGE_OT_change_frame);

  WM_operatortype_append(IMAGE_OT_read_viewlayers);
  WM_operatortype_append(IMAGE_OT_render_border);
  WM_operatortype_append(IMAGE_OT_clear_render_border);

  WM_operatortype_append(IMAGE_OT_tile_add);
  WM_operatortype_append(IMAGE_OT_tile_remove);
  WM_operatortype_append(IMAGE_OT_tile_fill);
}

static void image_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Image Generic", SPACE_IMAGE, RGN_TYPE_WINDOW);
  WM_keymap_ensure(keyconf, "Image", SPACE_IMAGE, RGN_TYPE_WINDOW);
}

/* area+region dropbox definition */
static void image_dropboxes() {}

/**
 * \note take care not to get into feedback loop here,
 *       calling composite job causes viewer to refresh.
 */
static void image_refresh(const bContext *C, ScrArea *area)
{
  Scene *scene = CTX_data_scene(C);
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
  Image *ima;

  ima = ED_space_image(sima);
  BKE_image_user_frame_calc(ima, &sima->iuser, scene->r.cfra);

  /* Check if we have to set the image from the edit-mesh. */
  if (ima && (ima->source == IMA_SRC_VIEWER && sima->mode == SI_MODE_MASK)) {
    if (scene->compositing_node_group) {
      Mask *mask = ED_space_image_get_mask(sima);
      if (mask) {
        ED_node_composite_job(C, scene->compositing_node_group, scene);
      }
    }
  }
}

static void image_listener(const wmSpaceTypeListenerParams *params)
{
  wmWindow *win = params->window;
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;
  SpaceImage *sima = (SpaceImage *)area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_WINDOW:
      /* notifier comes from editing color space */
      image_scopes_tag_refresh(area);
      ED_area_tag_redraw(area);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
          image_scopes_tag_refresh(area);
          ED_area_tag_refresh(area);
          ED_area_tag_redraw(area);
          break;
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          ED_area_tag_redraw(area);
          break;
        case ND_MODE:
          ED_paint_cursor_start(&params->scene->toolsettings->imapaint.paint,
                                ED_image_tools_paint_poll);

          if (wmn->subtype == NS_EDITMODE_MESH) {
            ED_area_tag_refresh(area);
          }
          ED_area_tag_redraw(area);
          break;
        case ND_RENDER_RESULT:
        case ND_RENDER_OPTIONS:
        case ND_COMPO_RESULT:
          if (ED_space_image_show_render(sima)) {
            image_scopes_tag_refresh(area);
            BKE_image_partial_update_mark_full_update(sima->image);
          }
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_IMAGE:
      if (wmn->reference == sima->image || !wmn->reference) {
        if (wmn->action != NA_PAINTING) {
          image_scopes_tag_refresh(area);
          ED_area_tag_refresh(area);
          ED_area_tag_redraw(area);
        }
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_IMAGE) {
        image_scopes_tag_refresh(area);
        ED_area_tag_redraw(area);
      }
      break;
    case NC_MASK: {
      Scene *scene = WM_window_get_active_scene(win);
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      BKE_view_layer_synced_ensure(scene, view_layer);
      Object *obedit = BKE_view_layer_edit_object_get(view_layer);
      if (ED_space_image_check_show_maskedit(sima, obedit)) {
        switch (wmn->data) {
          case ND_SELECT:
            ED_area_tag_redraw(area);
            break;
          case ND_DATA:
          case ND_DRAW:
            /* causes node-recalc */
            ED_area_tag_redraw(area);
            ED_area_tag_refresh(area);
            break;
        }
        switch (wmn->action) {
          case NA_SELECTED:
            ED_area_tag_redraw(area);
            break;
          case NA_EDITED:
            /* causes node-recalc */
            ED_area_tag_redraw(area);
            ED_area_tag_refresh(area);
            break;
        }
      }
      break;
    }
    case NC_GEOM: {
      switch (wmn->data) {
        case ND_DATA:
        case ND_SELECT:
          image_scopes_tag_refresh(area);
          ED_area_tag_refresh(area);
          ED_area_tag_redraw(area);
          break;
      }
      break;
    }
    case NC_OBJECT: {
      switch (wmn->data) {
        case ND_TRANSFORM:
        case ND_MODIFIER: {
          const Scene *scene = WM_window_get_active_scene(win);
          ViewLayer *view_layer = WM_window_get_active_view_layer(win);
          BKE_view_layer_synced_ensure(scene, view_layer);
          Object *ob = BKE_view_layer_active_object_get(view_layer);
          /* \note With a geometry nodes modifier, the UVs on `ob` can change in response to
           * any change on `wmn->reference`. If we could track the upstream dependencies,
           * unnecessary redraws could be reduced. Until then, just redraw. See #98594. */
          if (ob && (ob->mode & OB_MODE_EDIT) && sima->mode == SI_MODE_UV) {
            if (sima->lock && (sima->flag & SI_DRAWSHADOW)) {
              ED_area_tag_refresh(area);
              ED_area_tag_redraw(area);
            }
          }
          else if (ob) {
            if (sima->lock && !(sima->flag & SI_NO_DRAW_UV_GUIDE) &&
                ELEM(sima->mode, SI_MODE_PAINT, SI_MODE_UV))
            {
              ED_area_tag_refresh(area);
              ED_area_tag_redraw(area);
            }
          }
          break;
        }
      }

      break;
    }
    case NC_ID: {
      if (wmn->action == NA_RENAME) {
        ED_area_tag_redraw(area);
      }
      break;
    }
    case NC_WM:
      if (wmn->data == ND_UNDO) {
        ED_area_tag_redraw(area);
        ED_area_tag_refresh(area);
      }
      break;
  }
}

const char *image_context_dir[] = {"edit_image", "edit_mask", nullptr};

static int /*eContextResult*/ image_context(const bContext *C,
                                            const char *member,
                                            bContextDataResult *result)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, image_context_dir);
    // return CTX_RESULT_OK; /* TODO(@sybren). */
  }
  else if (CTX_data_equals(member, "edit_image")) {
    CTX_data_id_pointer_set(result, (ID *)ED_space_image(sima));
    return CTX_RESULT_OK;
  }
  else if (CTX_data_equals(member, "edit_mask")) {
    Mask *mask = ED_space_image_get_mask(sima);
    if (mask) {
      CTX_data_id_pointer_set(result, &mask->id);
    }
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_MEMBER_NOT_FOUND;
}

static void IMAGE_GGT_gizmo2d(wmGizmoGroupType *gzgt)
{
  gzgt->name = "UV Transform Gizmo";
  gzgt->idname = "IMAGE_GGT_gizmo2d";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_IMAGE;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  blender::ed::transform::ED_widgetgroup_gizmo2d_xform_callbacks_set(gzgt);
}

static void IMAGE_GGT_gizmo2d_translate(wmGizmoGroupType *gzgt)
{
  gzgt->name = "UV Translate Gizmo";
  gzgt->idname = "IMAGE_GGT_gizmo2d_translate";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_IMAGE;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  blender::ed::transform::ED_widgetgroup_gizmo2d_xform_no_cage_callbacks_set(gzgt);
}

static void IMAGE_GGT_gizmo2d_resize(wmGizmoGroupType *gzgt)
{
  gzgt->name = "UV Transform Gizmo Resize";
  gzgt->idname = "IMAGE_GGT_gizmo2d_resize";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_IMAGE;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  blender::ed::transform::ED_widgetgroup_gizmo2d_resize_callbacks_set(gzgt);
}

static void IMAGE_GGT_gizmo2d_rotate(wmGizmoGroupType *gzgt)
{
  gzgt->name = "UV Transform Gizmo Resize";
  gzgt->idname = "IMAGE_GGT_gizmo2d_rotate";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_IMAGE;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  blender::ed::transform::ED_widgetgroup_gizmo2d_rotate_callbacks_set(gzgt);
}

static void IMAGE_GGT_navigate(wmGizmoGroupType *gzgt)
{
  VIEW2D_GGT_navigate_impl(gzgt, "IMAGE_GGT_navigate");
}

static void image_widgets()
{
  const wmGizmoMapType_Params params{SPACE_IMAGE, RGN_TYPE_WINDOW};
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&params);

  WM_gizmogrouptype_append(IMAGE_GGT_gizmo2d);
  WM_gizmogrouptype_append(IMAGE_GGT_gizmo2d_translate);
  WM_gizmogrouptype_append(IMAGE_GGT_gizmo2d_resize);
  WM_gizmogrouptype_append(IMAGE_GGT_gizmo2d_rotate);

  WM_gizmogrouptype_append_and_link(gzmap_type, IMAGE_GGT_navigate);
}

/************************** main region ***************************/

/* sets up the fields of the View2D from zoom and offset */
static void image_main_region_set_view2d(SpaceImage *sima, ARegion *region)
{
  Image *ima = ED_space_image(sima);

  int width, height;
  ED_space_image_get_size(sima, &width, &height);

  float w = width;
  float h = height;

  if (ima) {
    h *= ima->aspy / ima->aspx;
  }

  int winx = BLI_rcti_size_x(&region->winrct) + 1;
  int winy = BLI_rcti_size_y(&region->winrct) + 1;

  /* For region overlap, move center so image doesn't overlap header. */
  const rcti *visible_rect = ED_region_visible_rect(region);
  const int visible_winy = BLI_rcti_size_y(visible_rect) + 1;
  int visible_centerx = 0;
  int visible_centery = visible_rect->ymin + (visible_winy - winy) / 2;

  region->v2d.tot.xmin = 0;
  region->v2d.tot.ymin = 0;
  region->v2d.tot.xmax = w;
  region->v2d.tot.ymax = h;

  region->v2d.mask.xmin = region->v2d.mask.ymin = 0;
  region->v2d.mask.xmax = winx;
  region->v2d.mask.ymax = winy;

  /* which part of the image space do we see? */
  float x1 = region->winrct.xmin + visible_centerx + (winx - sima->zoom * w) / 2.0f;
  float y1 = region->winrct.ymin + visible_centery + (winy - sima->zoom * h) / 2.0f;

  x1 -= sima->zoom * sima->xof;
  y1 -= sima->zoom * sima->yof;

  /* relative display right */
  region->v2d.cur.xmin = ((region->winrct.xmin - x1) / sima->zoom);
  region->v2d.cur.xmax = region->v2d.cur.xmin + (float(winx) / sima->zoom);

  /* relative display left */
  region->v2d.cur.ymin = ((region->winrct.ymin - y1) / sima->zoom);
  region->v2d.cur.ymax = region->v2d.cur.ymin + (float(winy) / sima->zoom);

  /* normalize 0.0..1.0 */
  region->v2d.cur.xmin /= w;
  region->v2d.cur.xmax /= w;
  region->v2d.cur.ymin /= h;
  region->v2d.cur.ymax /= h;
}

/* add handlers, stuff you only do once or on area/region changes */
static void image_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* NOTE: don't use `UI_view2d_region_reinit(&region->v2d, ...)`
   * since the space clip manages own v2d in #image_main_region_set_view2d */

  /* mask polls mode */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Mask Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  /* image paint polls for mode */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Paint Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Image Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "UV Editor", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);

  /* own keymaps */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Image Generic", SPACE_IMAGE, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Image", SPACE_IMAGE, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void image_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceImage *sima = CTX_wm_space_image(C);
  Object *obedit = CTX_data_edit_object(C);
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  Mask *mask = nullptr;
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = &region->v2d;
  Image *image = ED_space_image(sima);
  /* Typically a render result or viewer image from the compositor. */
  const bool show_viewer = (image && image->source == IMA_SRC_VIEWER);
  const bool show_compositor_viewer = show_viewer && image->type == IMA_TYPE_COMPOSITE;

  /* Text info and render region are only relevant for the compositor. */
  const bool show_text_info = show_compositor_viewer &&
                              (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS &&
                               sima->overlay.flag & SI_OVERLAY_DRAW_TEXT_INFO &&
                               ELEM(sima->mode, SI_MODE_MASK, SI_MODE_VIEW));
  const bool show_render_region = show_compositor_viewer &&
                                  (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS &&
                                   sima->overlay.flag & SI_OVERLAY_DRAW_RENDER_REGION &&
                                   ELEM(sima->mode, SI_MODE_MASK, SI_MODE_VIEW));

  /* XXX not supported yet, disabling for now */
  scene->r.scemode &= ~R_COMP_CROP;

  image_user_refresh_scene(C, sima);

  /* we set view2d from own zoom and offset each time */
  image_main_region_set_view2d(sima, region);

  /* check for mask (delay draw) */
  if (!ED_space_image_show_uvedit(sima, obedit) && sima->mode == SI_MODE_MASK) {
    mask = ED_space_image_get_mask(sima);
  }

  if (show_viewer) {
    BLI_thread_lock(LOCK_DRAW_IMAGE);
  }
  DRW_draw_view(C);
  if (show_viewer) {
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }

  if (show_render_region) {
    int render_size_x, render_size_y;

    BKE_render_resolution(&scene->r, true, &render_size_x, &render_size_y);

    float zoomx, zoomy;
    ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);
    int width, height;
    ED_space_image_get_size(sima, &width, &height);
    int center_x = width / 2;
    int center_y = height / 2;

    int x, y;
    rcti render_region;
    BLI_rcti_init(
        &render_region, center_x, render_size_x + center_x, center_y, render_size_y + center_y);
    UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);

    ED_region_image_render_region_draw(
        x, y, &render_region, zoomx, zoomy, sima->overlay.passepartout_alpha);
  }

  draw_image_main_helpers(C, region);

  /* Draw Meta data of the image isn't added to the DrawManager as it is
   * used in other areas as well. */
  if (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS && sima->flag & SI_DRAW_METADATA) {
    void *lock;
    /* #ED_space_image_get_zoom temporarily locks the image, so this needs to be done before
     * the image is locked when calling #ED_space_image_acquire_buffer. */
    float zoomx, zoomy;
    ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);
    ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);
    if (ibuf) {
      int x, y;
      rctf frame;
      BLI_rctf_init(&frame, 0.0f, ibuf->x, 0.0f, ibuf->y);
      UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);
      ED_region_image_metadata_draw(x, y, ibuf, &frame, zoomx, zoomy);
    }
    ED_space_image_release_buffer(sima, ibuf, lock);
  }

  if (show_text_info) {

    int render_size_x, render_size_y;
    BKE_render_resolution(&scene->r, true, &render_size_x, &render_size_y);

    /* Use same positioning convention as in 3D View. */
    const rcti *rect = ED_region_visible_rect(region);
    int xoffset = rect->xmin + (0.5f * U.widget_unit);
    int yoffset = rect->ymax - (0.1f * U.widget_unit);

    int viewer_size_x, viewer_size_y;
    ED_space_image_get_size(sima, &viewer_size_x, &viewer_size_y);

    ED_region_image_overlay_info_text_draw(
        render_size_x, render_size_y, viewer_size_x, viewer_size_y, xoffset, yoffset);
  }

  /* sample line */
  UI_view2d_view_ortho(v2d);
  draw_image_sample_line(sima);
  UI_view2d_view_restore(C);

  if (mask) {
    int width, height;
    float aspx, aspy;

    if (show_viewer) {
      /* ED_space_image_get* will acquire image buffer which requires
       * lock here by the same reason why lock is needed in draw_image_main
       */
      BLI_thread_lock(LOCK_DRAW_IMAGE);
    }

    ED_space_image_get_size(sima, &width, &height);
    ED_space_image_get_aspect(sima, &aspx, &aspy);

    if (show_viewer) {
      BLI_thread_unlock(LOCK_DRAW_IMAGE);
    }

    ED_mask_draw_region(depsgraph,
                        mask,
                        region, /* Mask overlay is drawn by image/overlay engine. */
                        sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS,
                        sima->mask_info.draw_flag & ~MASK_DRAWFLAG_OVERLAY,
                        sima->mask_info.draw_type,
                        eMaskOverlayMode(sima->mask_info.overlay_mode),
                        sima->mask_info.blend_factor,
                        width,
                        height,
                        aspx,
                        aspy,
                        true,
                        false,
                        nullptr,
                        C);
  }
  if ((sima->gizmo_flag & SI_GIZMO_HIDE) == 0) {
    WM_gizmomap_draw(region->runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);
  }
  draw_image_cache(C, region);
}

static void image_main_region_listener(const wmRegionListenerParams *params)
{
  ScrArea *area = params->area;
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_GEOM:
      if (ELEM(wmn->data, ND_DATA, ND_SELECT)) {
        WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
      }
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      else if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_IMAGE:
      if (wmn->action == NA_PAINTING) {
        ED_region_tag_redraw(region);
      }
      WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
      break;
    case NC_MASK:
      if (wmn->action == NA_EDITED) {
        WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
      }
      else if (ELEM(wmn->data, ND_DATA, ND_SELECT)) {
        WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
      }
      break;
    case NC_MATERIAL:
      if (wmn->data == ND_SHADING_LINKS) {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);

        if (sima->iuser.scene &&
            (sima->iuser.scene->toolsettings->uv_flag & UV_FLAG_SHOW_SAME_IMAGE))
        {
          ED_region_tag_redraw(region);
        }
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYER)) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_buttons_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Image Generic", SPACE_IMAGE, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void image_buttons_region_layout(const bContext *C, ARegion *region)
{
  const enum eContextObjectMode mode = CTX_data_mode_enum(C);
  const char *contexts_base[3] = {nullptr};

  const char **contexts = contexts_base;

  SpaceImage *sima = CTX_wm_space_image(C);
  switch (sima->mode) {
    case SI_MODE_VIEW:
      break;
    case SI_MODE_PAINT:
      ARRAY_SET_ITEMS(contexts, ".paint_common_2d", ".imagepaint_2d");
      break;
    case SI_MODE_MASK:
      break;
    case SI_MODE_UV:
      if (mode == CTX_MODE_EDIT_MESH) {
        ARRAY_SET_ITEMS(contexts, ".uv_sculpt");
      }
      break;
  }

  ED_region_panels_layout_ex(C,
                             region,
                             &region->runtime->type->paneltypes,
                             blender::wm::OpCallContext::InvokeRegionWin,
                             contexts_base,
                             nullptr);
}

static void image_buttons_region_draw(const bContext *C, ARegion *region)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  void *lock;
  /* TODO(lukas): Support tiles in scopes? */
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);
  /* XXX performance regression if name of scopes category changes! */
  PanelCategoryStack *category = UI_panel_category_active_find(region, "Scopes");

  /* only update scopes if scope category is active */
  if (category) {
    if (ibuf) {
      if (!sima->scopes.ok) {
        BKE_histogram_update_sample_line(
            &sima->sample_line_hist, ibuf, &scene->view_settings, &scene->display_settings);
      }
      if (sima->image->flag & IMA_VIEW_AS_RENDER) {
        ED_space_image_scopes_update(C, sima, ibuf, true);
      }
      else {
        ED_space_image_scopes_update(C, sima, ibuf, false);
      }
    }
  }
  ED_space_image_release_buffer(sima, ibuf, lock);

  /* Layout handles details. */
  ED_region_panels_draw(C, region);
}

static void image_buttons_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_TEXTURE:
    case NC_MATERIAL:
      /* sending by texture render job and needed to properly update displaying
       * brush texture icon */
      ED_region_tag_redraw(region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_MODE:
        case ND_RENDER_RESULT:
        case ND_COMPO_RESULT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_IMAGE:
      if (wmn->action != NA_PAINTING) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_NODE:
      ED_region_tag_redraw(region);
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_BRUSH:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* *********************** scopes region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_tools_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Image Generic", SPACE_IMAGE, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void image_tools_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void image_tools_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (wmn->data == ND_DATA || ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_BRUSH:
      /* NA_SELECTED is used on brush changes */
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_MODE:
        case ND_RENDER_RESULT:
        case ND_COMPO_RESULT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_IMAGE:
      if (wmn->action != NA_PAINTING) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_NODE:
      ED_region_tag_redraw(region);
      break;
  }
}

/************************* Tool header region **************************/

static void image_tools_header_region_draw(const bContext *C, ARegion *region)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);

  image_user_refresh_scene(C, sima);

  ED_region_header_with_button_sections(
      C,
      region,
      (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_TOP) ?
          uiButtonSectionsAlign::Top :
          uiButtonSectionsAlign::Bottom);
}

/************************* header region **************************/

/* add handlers, stuff you only do once or on area/region changes */
static void image_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void image_header_region_draw(const bContext *C, ARegion *region)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);

  image_user_refresh_scene(C, sima);

  ED_region_header(C, region);
}

static void image_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_MODE:
        case ND_TOOLSETTINGS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_DATA:
        case ND_SELECT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_BRUSH:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(region);
      }
      else if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void image_asset_shelf_region_init(wmWindowManager *wm, ARegion *region)
{
  using namespace blender::ed;
  wmKeyMap *keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Image Generic", SPACE_IMAGE, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);

  asset::shelf::region_init(wm, region);
}

static void image_id_remap(ScrArea * /*area*/,
                           SpaceLink *slink,
                           const blender::bke::id::IDRemapper &mappings)
{
  SpaceImage *simg = (SpaceImage *)slink;

  if (!mappings.contains_mappings_for_any(FILTER_ID_IM | FILTER_ID_GD_LEGACY | FILTER_ID_MSK)) {
    return;
  }

  mappings.apply(reinterpret_cast<ID **>(&simg->image), ID_REMAP_APPLY_ENSURE_REAL);
  mappings.apply(reinterpret_cast<ID **>(&simg->gpd), ID_REMAP_APPLY_UPDATE_REFCOUNT);
  mappings.apply(reinterpret_cast<ID **>(&simg->mask_info.mask), ID_REMAP_APPLY_ENSURE_REAL);
}

static void image_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceImage *simg = reinterpret_cast<SpaceImage *>(space_link);
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);
  const bool is_readonly = (data_flags & IDWALK_READONLY) != 0;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(
      data, simg->image, IDWALK_CB_USER_ONE | IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, simg->iuser.scene, IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(
      data, simg->mask_info.mask, IDWALK_CB_USER_ONE | IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, simg->gpd, IDWALK_CB_USER | IDWALK_CB_DIRECT_WEAK_LINK);
  if (!is_readonly) {
    simg->scopes.ok = 0;
  }
}

/**
 * \note Used for splitting out a subset of modes is more involved,
 * The previous non-uv-edit mode is stored so switching back to the
 * image doesn't always reset the sub-mode.
 */
static int image_space_subtype_get(ScrArea *area)
{
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
  return sima->mode == SI_MODE_UV ? SI_MODE_UV : SI_MODE_VIEW;
}

static void image_space_subtype_set(ScrArea *area, int value)
{
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
  if (value == SI_MODE_UV) {
    if (sima->mode != SI_MODE_UV) {
      sima->mode_prev = sima->mode;
    }
    sima->mode = value;
  }
  else {
    sima->mode = sima->mode_prev;
  }
}

static void image_space_subtype_item_extend(bContext * /*C*/,
                                            EnumPropertyItem **item,
                                            int *totitem)
{
  RNA_enum_items_add(item, totitem, rna_enum_space_image_mode_items);
}

static blender::StringRefNull image_space_name_get(const ScrArea *area)
{
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
  int index = RNA_enum_from_value(rna_enum_space_image_mode_items, sima->mode);
  if (index < 0) {
    index = SI_MODE_VIEW;
  }
  const EnumPropertyItem item = rna_enum_space_image_mode_items[index];
  return item.name;
}

static int image_space_icon_get(const ScrArea *area)
{
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
  int index = RNA_enum_from_value(rna_enum_space_image_mode_items, sima->mode);
  if (index < 0) {
    index = SI_MODE_VIEW;
  }
  const EnumPropertyItem item = rna_enum_space_image_mode_items[index];
  return item.icon;
}

static void image_space_blend_read_data(BlendDataReader * /*reader*/, SpaceLink *sl)
{
  SpaceImage *sima = (SpaceImage *)sl;

  sima->iuser.scene = nullptr;
  sima->scopes.waveform_1 = nullptr;
  sima->scopes.waveform_2 = nullptr;
  sima->scopes.waveform_3 = nullptr;
  sima->scopes.vecscope = nullptr;
  sima->scopes.vecscope_rgb = nullptr;
  sima->scopes.ok = 0;

/* WARNING: gpencil data is no longer stored directly in sima after 2.5
 * so sacrifice a few old files for now to avoid crashes with new files!
 * committed: r28002 */
#if 0
  sima->gpd = newdataadr(fd, sima->gpd);
  if (sima->gpd) {
    BKE_gpencil_blend_read_data(fd, sima->gpd);
  }
#endif
}

static void image_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceImage, sl);
}

/**************************** spacetype *****************************/

void ED_spacetype_image()
{
  using namespace blender::ed;
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_IMAGE;
  STRNCPY_UTF8(st->name, "Image");

  st->create = image_create;
  st->free = image_free;
  st->init = image_init;
  st->duplicate = image_duplicate;
  st->operatortypes = image_operatortypes;
  st->keymap = image_keymap;
  st->dropboxes = image_dropboxes;
  st->refresh = image_refresh;
  st->listener = image_listener;
  st->context = image_context;
  st->gizmos = image_widgets;
  st->id_remap = image_id_remap;
  st->foreach_id = image_foreach_id;
  st->space_subtype_item_extend = image_space_subtype_item_extend;
  st->space_subtype_get = image_space_subtype_get;
  st->space_subtype_set = image_space_subtype_set;
  st->space_name_get = image_space_name_get;
  st->space_icon_get = image_space_icon_get;
  st->blend_read_data = image_space_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = image_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype image region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_TOOL | ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;
  art->init = image_main_region_init;
  art->draw = image_main_region_draw;
  art->listener = image_main_region_listener;
  art->lock = REGION_DRAW_LOCK_BAKING;
  BLI_addhead(&st->regiontypes, art);

  /* regions: list-view/buttons/scopes */
  art = MEM_callocN<ARegionType>("spacetype image region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = image_buttons_region_listener;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_ui;
  art->init = image_buttons_region_init;
  art->snap_size = ED_region_generic_panel_region_snap_size;
  art->layout = image_buttons_region_layout;
  art->draw = image_buttons_region_draw;
  BLI_addhead(&st->regiontypes, art);

  ED_uvedit_buttons_register(art);
  image_buttons_register(art);

  /* regions: tool(bar) */
  art = MEM_callocN<ARegionType>("spacetype image region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = int(UI_TOOLBAR_WIDTH);
  art->prefsizey = 50; /* XXX */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = image_tools_region_listener;
  art->message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art->snap_size = ED_region_generic_tools_region_snap_size;
  art->init = image_tools_region_init;
  art->draw = image_tools_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: tool header */
  art = MEM_callocN<ARegionType>("spacetype image tool header region");
  art->regionid = RGN_TYPE_TOOL_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = image_header_region_listener;
  art->init = image_header_region_init;
  art->draw = image_tools_header_region_draw;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_header;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype image region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = image_header_region_listener;
  art->init = image_header_region_init;
  art->draw = image_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: asset shelf */
  art = MEM_callocN<ARegionType>("spacetype image asset shelf region");
  art->regionid = RGN_TYPE_ASSET_SHELF;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_ASSET_SHELF | ED_KEYMAP_FRAMES;
  art->duplicate = asset::shelf::region_duplicate;
  art->free = asset::shelf::region_free;
  art->on_poll_success = asset::shelf::region_on_poll_success;
  art->listener = asset::shelf::region_listen;
  art->message_subscribe = asset::shelf::region_message_subscribe;
  art->poll = asset::shelf::regions_poll;
  art->snap_size = asset::shelf::region_snap;
  art->on_user_resize = asset::shelf::region_on_user_resize;
  art->context = asset::shelf::context;
  art->init = image_asset_shelf_region_init;
  art->layout = asset::shelf::region_layout;
  art->draw = asset::shelf::region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: asset shelf header */
  art = MEM_callocN<ARegionType>("spacetype image asset shelf header region");
  art->regionid = RGN_TYPE_ASSET_SHELF_HEADER;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_ASSET_SHELF | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;
  art->init = asset::shelf::header_region_init;
  art->poll = asset::shelf::regions_poll;
  art->draw = asset::shelf::header_region;
  art->listener = asset::shelf::header_region_listen;
  art->context = asset::shelf::context;
  BLI_addhead(&st->regiontypes, art);
  asset::shelf::types_register(art, SPACE_IMAGE);

  /* regions: hud */
  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}
