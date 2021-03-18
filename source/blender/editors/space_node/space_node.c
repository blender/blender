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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spnode
 */

#include "DNA_gpencil_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "ED_node.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "node_intern.h" /* own include */

/* ******************** tree path ********************* */

void ED_node_tree_start(SpaceNode *snode, bNodeTree *ntree, ID *id, ID *from)
{
  bNodeTreePath *path, *path_next;
  for (path = snode->treepath.first; path; path = path_next) {
    path_next = path->next;
    MEM_freeN(path);
  }
  BLI_listbase_clear(&snode->treepath);

  if (ntree) {
    path = MEM_callocN(sizeof(bNodeTreePath), "node tree path");
    path->nodetree = ntree;
    path->parent_key = NODE_INSTANCE_KEY_BASE;

    /* copy initial offset from bNodeTree */
    copy_v2_v2(path->view_center, ntree->view_center);

    if (id) {
      BLI_strncpy(path->node_name, id->name + 2, sizeof(path->node_name));
    }

    BLI_addtail(&snode->treepath, path);

    if (ntree->type != NTREE_GEOMETRY) {
      /* This can probably be removed for all node tree types. It mainly exists because it was not
       * possible to store id references in custom properties. Also see T36024. I don't want to
       * remove it for all tree types in bcon3 though. */
      id_us_ensure_real(&ntree->id);
    }
  }

  /* update current tree */
  snode->nodetree = snode->edittree = ntree;
  snode->id = id;
  snode->from = from;

  ED_node_set_active_viewer_key(snode);

  WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

void ED_node_tree_push(SpaceNode *snode, bNodeTree *ntree, bNode *gnode)
{
  bNodeTreePath *path = MEM_callocN(sizeof(bNodeTreePath), "node tree path");
  bNodeTreePath *prev_path = snode->treepath.last;
  path->nodetree = ntree;
  if (gnode) {
    if (prev_path) {
      path->parent_key = BKE_node_instance_key(prev_path->parent_key, prev_path->nodetree, gnode);
    }
    else {
      path->parent_key = NODE_INSTANCE_KEY_BASE;
    }

    BLI_strncpy(path->node_name, gnode->name, sizeof(path->node_name));
  }
  else {
    path->parent_key = NODE_INSTANCE_KEY_BASE;
  }

  /* copy initial offset from bNodeTree */
  copy_v2_v2(path->view_center, ntree->view_center);

  BLI_addtail(&snode->treepath, path);

  id_us_ensure_real(&ntree->id);

  /* update current tree */
  snode->edittree = ntree;

  ED_node_set_active_viewer_key(snode);

  WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

void ED_node_tree_pop(SpaceNode *snode)
{
  bNodeTreePath *path = snode->treepath.last;

  /* don't remove root */
  if (path == snode->treepath.first) {
    return;
  }

  BLI_remlink(&snode->treepath, path);
  MEM_freeN(path);

  /* update current tree */
  path = snode->treepath.last;
  snode->edittree = path->nodetree;

  ED_node_set_active_viewer_key(snode);

  /* listener updates the View2D center from edittree */
  WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

int ED_node_tree_depth(SpaceNode *snode)
{
  return BLI_listbase_count(&snode->treepath);
}

bNodeTree *ED_node_tree_get(SpaceNode *snode, int level)
{
  bNodeTreePath *path;
  int i;
  for (path = snode->treepath.last, i = 0; path; path = path->prev, i++) {
    if (i == level) {
      return path->nodetree;
    }
  }
  return NULL;
}

int ED_node_tree_path_length(SpaceNode *snode)
{
  int length = 0;
  int i = 0;
  LISTBASE_FOREACH_INDEX (bNodeTreePath *, path, &snode->treepath, i) {
    length += strlen(path->node_name);
    if (i > 0) {
      length += 1; /* for separator char */
    }
  }
  return length;
}

void ED_node_tree_path_get(SpaceNode *snode, char *value)
{
  int i = 0;

  value[0] = '\0';
  LISTBASE_FOREACH_INDEX (bNodeTreePath *, path, &snode->treepath, i) {
    if (i == 0) {
      strcpy(value, path->node_name);
      value += strlen(path->node_name);
    }
    else {
      sprintf(value, "/%s", path->node_name);
      value += strlen(path->node_name) + 1;
    }
  }
}

void ED_node_tree_path_get_fixedbuf(SpaceNode *snode, char *value, int max_length)
{
  int size;

  value[0] = '\0';
  int i = 0;
  LISTBASE_FOREACH_INDEX (bNodeTreePath *, path, &snode->treepath, i) {
    if (i == 0) {
      size = BLI_strncpy_rlen(value, path->node_name, max_length);
    }
    else {
      size = BLI_snprintf_rlen(value, max_length, "/%s", path->node_name);
    }
    max_length -= size;
    if (max_length <= 0) {
      break;
    }
    value += size;
  }
}

void ED_node_set_active_viewer_key(SpaceNode *snode)
{
  bNodeTreePath *path = snode->treepath.last;
  if (snode->nodetree && path) {
    snode->nodetree->active_viewer_key = path->parent_key;
  }
}

void space_node_group_offset(SpaceNode *snode, float *x, float *y)
{
  bNodeTreePath *path = snode->treepath.last;

  if (path && path->prev) {
    float dcenter[2];
    sub_v2_v2v2(dcenter, path->view_center, path->prev->view_center);
    *x = dcenter[0];
    *y = dcenter[1];
  }
  else {
    *x = *y = 0.0f;
  }
}

/* ******************** default callbacks for node space ***************** */

static SpaceLink *node_create(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  ARegion *region;
  SpaceNode *snode;

  snode = MEM_callocN(sizeof(SpaceNode), "initnode");
  snode->spacetype = SPACE_NODE;

  snode->flag = SNODE_SHOW_GPENCIL | SNODE_USE_ALPHA;

  /* backdrop */
  snode->zoom = 1.0f;

  /* select the first tree type for valid type */
  NODE_TREE_TYPES_BEGIN (treetype) {
    strcpy(snode->tree_idname, treetype->idname);
    break;
  }
  NODE_TREE_TYPES_END;

  /* header */
  region = MEM_callocN(sizeof(ARegion), "header for node");

  BLI_addtail(&snode->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* buttons/list view */
  region = MEM_callocN(sizeof(ARegion), "buttons for node");

  BLI_addtail(&snode->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;

  /* toolbar */
  region = MEM_callocN(sizeof(ARegion), "node tools");

  BLI_addtail(&snode->regionbase, region);
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;

  region->flag = RGN_FLAG_HIDDEN;

  /* main region */
  region = MEM_callocN(sizeof(ARegion), "main region for node");

  BLI_addtail(&snode->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  region->v2d.tot.xmin = -12.8f * U.widget_unit;
  region->v2d.tot.ymin = -12.8f * U.widget_unit;
  region->v2d.tot.xmax = 38.4f * U.widget_unit;
  region->v2d.tot.ymax = 38.4f * U.widget_unit;

  region->v2d.cur = region->v2d.tot;

  region->v2d.min[0] = 1.0f;
  region->v2d.min[1] = 1.0f;

  region->v2d.max[0] = 32000.0f;
  region->v2d.max[1] = 32000.0f;

  region->v2d.minzoom = 0.09f;
  region->v2d.maxzoom = 2.31f;

  region->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
  region->v2d.keepzoom = V2D_LIMITZOOM | V2D_KEEPASPECT;
  region->v2d.keeptot = 0;

  return (SpaceLink *)snode;
}

static void node_free(SpaceLink *sl)
{
  SpaceNode *snode = (SpaceNode *)sl;

  LISTBASE_FOREACH_MUTABLE (bNodeTreePath *, path, &snode->treepath) {
    MEM_freeN(path);
  }

  MEM_SAFE_FREE(snode->runtime);
}

/* spacetype; init callback */
static void node_init(struct wmWindowManager *UNUSED(wm), ScrArea *area)
{
  SpaceNode *snode = (SpaceNode *)area->spacedata.first;

  if (snode->runtime == NULL) {
    snode->runtime = MEM_callocN(sizeof(SpaceNode_Runtime), __func__);
  }
}

static void node_area_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  wmNotifier *wmn = params->notifier;

  /* note, ED_area_tag_refresh will re-execute compositor */
  SpaceNode *snode = area->spacedata.first;
  /* shaderfrom is only used for new shading nodes, otherwise all shaders are from objects */
  short shader_type = snode->shaderfrom;

  /* preview renders */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_NODES: {
          ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
          bNodeTreePath *path = snode->treepath.last;
          /* shift view to node tree center */
          if (region && path) {
            UI_view2d_center_set(&region->v2d, path->view_center[0], path->view_center[1]);
          }

          ED_area_tag_refresh(area);
          break;
        }
        case ND_FRAME:
          ED_area_tag_refresh(area);
          break;
        case ND_COMPO_RESULT:
          ED_area_tag_redraw(area);
          break;
        case ND_TRANSFORM_DONE:
          if (ED_node_is_compositor(snode)) {
            if (snode->flag & SNODE_AUTO_RENDER) {
              snode->runtime->recalc = true;
              ED_area_tag_refresh(area);
            }
          }
          break;
        case ND_LAYER_CONTENT:
          ED_area_tag_refresh(area);
          break;
      }
      break;

    /* future: add ID checks? */
    case NC_MATERIAL:
      if (ED_node_is_shader(snode)) {
        if (wmn->data == ND_SHADING) {
          ED_area_tag_refresh(area);
        }
        else if (wmn->data == ND_SHADING_DRAW) {
          ED_area_tag_refresh(area);
        }
        else if (wmn->data == ND_SHADING_LINKS) {
          ED_area_tag_refresh(area);
        }
        else if (wmn->action == NA_ADDED && snode->edittree) {
          nodeSetActiveID(snode->edittree, ID_MA, wmn->reference);
        }
      }
      break;
    case NC_TEXTURE:
      if (ED_node_is_shader(snode) || ED_node_is_texture(snode)) {
        if (wmn->data == ND_NODES) {
          ED_area_tag_refresh(area);
        }
      }
      break;
    case NC_WORLD:
      if (ED_node_is_shader(snode) && shader_type == SNODE_SHADER_WORLD) {
        ED_area_tag_refresh(area);
      }
      break;
    case NC_OBJECT:
      if (ED_node_is_shader(snode)) {
        if (wmn->data == ND_OB_SHADING) {
          ED_area_tag_refresh(area);
        }
      }
      else if (ED_node_is_geometry(snode)) {
        /* Rather strict check: only redraw when the reference matches the current editor's ID. */
        if (wmn->data == ND_MODIFIER) {
          if (wmn->reference == snode->id || snode->id == NULL) {
            ED_area_tag_refresh(area);
          }
        }
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_NODE) {
        ED_area_tag_refresh(area);
      }
      else if (wmn->data == ND_SPACE_NODE_VIEW) {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_NODE:
      if (wmn->action == NA_EDITED) {
        ED_area_tag_refresh(area);
      }
      else if (wmn->action == NA_SELECTED) {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_SCREEN:
      switch (wmn->data) {
        case ND_ANIMPLAY:
          ED_area_tag_refresh(area);
          break;
      }
      break;
    case NC_MASK:
      if (wmn->action == NA_EDITED) {
        if (snode->nodetree && snode->nodetree->type == NTREE_COMPOSIT) {
          ED_area_tag_refresh(area);
        }
      }
      break;

    case NC_IMAGE:
      if (wmn->action == NA_EDITED) {
        if (ED_node_is_compositor(snode)) {
          /* note that nodeUpdateID is already called by BKE_image_signal() on all
           * scenes so really this is just to know if the images is used in the compo else
           * painting on images could become very slow when the compositor is open. */
          if (nodeUpdateID(snode->nodetree, wmn->reference)) {
            ED_area_tag_refresh(area);
          }
        }
      }
      break;

    case NC_MOVIECLIP:
      if (wmn->action == NA_EDITED) {
        if (ED_node_is_compositor(snode)) {
          if (nodeUpdateID(snode->nodetree, wmn->reference)) {
            ED_area_tag_refresh(area);
          }
        }
      }
      break;

    case NC_LINESTYLE:
      if (ED_node_is_shader(snode) && shader_type == SNODE_SHADER_LINESTYLE) {
        ED_area_tag_refresh(area);
      }
      break;
    case NC_WM:
      if (wmn->data == ND_UNDO) {
        ED_area_tag_refresh(area);
      }
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_area_tag_redraw(area);
      }
      break;
  }
}

static void node_area_refresh(const struct bContext *C, ScrArea *area)
{
  /* default now: refresh node is starting preview */
  SpaceNode *snode = area->spacedata.first;

  snode_set_context(C);

  if (snode->nodetree) {
    if (snode->nodetree->type == NTREE_SHADER) {
      if (GS(snode->id->name) == ID_MA) {
        Material *ma = (Material *)snode->id;
        if (ma->use_nodes) {
          ED_preview_shader_job(C, area, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
        }
      }
      else if (GS(snode->id->name) == ID_LA) {
        Light *la = (Light *)snode->id;
        if (la->use_nodes) {
          ED_preview_shader_job(C, area, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
        }
      }
      else if (GS(snode->id->name) == ID_WO) {
        World *wo = (World *)snode->id;
        if (wo->use_nodes) {
          ED_preview_shader_job(C, area, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
        }
      }
    }
    else if (snode->nodetree->type == NTREE_COMPOSIT) {
      Scene *scene = (Scene *)snode->id;
      if (scene->use_nodes) {
        /* recalc is set on 3d view changes for auto compo */
        if (snode->runtime->recalc) {
          snode->runtime->recalc = false;
          node_render_changed_exec((struct bContext *)C, NULL);
        }
        else {
          ED_node_composite_job(C, snode->nodetree, scene);
        }
      }
    }
    else if (snode->nodetree->type == NTREE_TEXTURE) {
      Tex *tex = (Tex *)snode->id;
      if (tex->use_nodes) {
        ED_preview_shader_job(C, area, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
      }
    }
  }
}

static SpaceLink *node_duplicate(SpaceLink *sl)
{
  SpaceNode *snode = (SpaceNode *)sl;
  SpaceNode *snoden = MEM_dupallocN(snode);

  BLI_duplicatelist(&snoden->treepath, &snode->treepath);

  if (snode->runtime != NULL) {
    snoden->runtime = MEM_dupallocN(snode->runtime);
    BLI_listbase_clear(&snoden->runtime->linkdrag);
  }

  /* Note: no need to set node tree user counts,
   * the editor only keeps at least 1 (id_us_ensure_real),
   * which is already done by the original SpaceNode.
   */

  return (SpaceLink *)snoden;
}

/* add handlers, stuff you only do once or on area/region changes */
static void node_buttons_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void node_buttons_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

/* add handlers, stuff you only do once or on area/region changes */
static void node_toolbar_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void node_toolbar_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

void ED_node_cursor_location_get(const SpaceNode *snode, float value[2])
{
  copy_v2_v2(value, snode->runtime->cursor);
}

void ED_node_cursor_location_set(SpaceNode *snode, const float value[2])
{
  copy_v2_v2(snode->runtime->cursor, value);
}

static void node_cursor(wmWindow *win, ScrArea *area, ARegion *region)
{
  SpaceNode *snode = area->spacedata.first;

  /* convert mouse coordinates to v2d space */
  UI_view2d_region_to_view(&region->v2d,
                           win->eventstate->x - region->winrct.xmin,
                           win->eventstate->y - region->winrct.ymin,
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);

  /* here snode->runtime->cursor is used to detect the node edge for sizing */
  node_set_cursor(win, snode, snode->runtime->cursor);

  /* XXX snode->runtime->cursor is in placing new nodes space */
  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;
}

/* Initialize main region, setting handlers. */
static void node_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;
  ListBase *lb;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* own keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Node Editor", SPACE_NODE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  /* add drop boxes */
  lb = WM_dropboxmap_find("Node Editor", SPACE_NODE, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&region->handlers, lb);
}

static void node_main_region_draw(const bContext *C, ARegion *region)
{
  node_draw_space(C, region);
}

/* ************* dropboxes ************* */

static bool node_group_drop_poll(bContext *UNUSED(C),
                                 wmDrag *drag,
                                 const wmEvent *UNUSED(event),
                                 const char **UNUSED(r_tooltip))
{
  return WM_drag_is_ID_type(drag, ID_NT);
}

static bool node_object_drop_poll(bContext *UNUSED(C),
                                  wmDrag *drag,
                                  const wmEvent *UNUSED(event),
                                  const char **UNUSED(r_tooltip))
{
  return WM_drag_is_ID_type(drag, ID_OB);
}

static bool node_collection_drop_poll(bContext *UNUSED(C),
                                      wmDrag *drag,
                                      const wmEvent *UNUSED(event),
                                      const char **UNUSED(r_tooltip))
{
  return WM_drag_is_ID_type(drag, ID_GR);
}

static bool node_texture_drop_poll(bContext *UNUSED(C),
                                   wmDrag *drag,
                                   const wmEvent *UNUSED(event),
                                   const char **UNUSED(r_tooltip))
{
  return WM_drag_is_ID_type(drag, ID_TE);
}

static bool node_ima_drop_poll(bContext *UNUSED(C),
                               wmDrag *drag,
                               const wmEvent *UNUSED(event),
                               const char **UNUSED(r_tooltip))
{
  if (drag->type == WM_DRAG_PATH) {
    /* rule might not work? */
    return (ELEM(drag->icon, 0, ICON_FILE_IMAGE, ICON_FILE_MOVIE));
  }
  return WM_drag_is_ID_type(drag, ID_IM);
}

static bool node_mask_drop_poll(bContext *UNUSED(C),
                                wmDrag *drag,
                                const wmEvent *UNUSED(event),
                                const char **UNUSED(r_tooltip))
{
  return WM_drag_is_ID_type(drag, ID_MSK);
}

static void node_group_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(drag, 0);

  RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void node_id_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(drag, 0);

  RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void node_id_path_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(drag, 0);

  if (id) {
    RNA_string_set(drop->ptr, "name", id->name + 2);
    RNA_struct_property_unset(drop->ptr, "filepath");
  }
  else if (drag->path[0]) {
    RNA_string_set(drop->ptr, "filepath", drag->path);
    RNA_struct_property_unset(drop->ptr, "name");
  }
}

/* this region dropbox definition */
static void node_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Node Editor", SPACE_NODE, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb,
                 "NODE_OT_add_object",
                 node_object_drop_poll,
                 node_id_drop_copy,
                 WM_drag_free_imported_drag_ID);
  WM_dropbox_add(lb,
                 "NODE_OT_add_collection",
                 node_collection_drop_poll,
                 node_id_drop_copy,
                 WM_drag_free_imported_drag_ID);
  WM_dropbox_add(lb,
                 "NODE_OT_add_texture",
                 node_texture_drop_poll,
                 node_id_drop_copy,
                 WM_drag_free_imported_drag_ID);
  WM_dropbox_add(lb,
                 "NODE_OT_add_group",
                 node_group_drop_poll,
                 node_group_drop_copy,
                 WM_drag_free_imported_drag_ID);
  WM_dropbox_add(lb,
                 "NODE_OT_add_file",
                 node_ima_drop_poll,
                 node_id_path_drop_copy,
                 WM_drag_free_imported_drag_ID);
  WM_dropbox_add(lb,
                 "NODE_OT_add_mask",
                 node_mask_drop_poll,
                 node_id_drop_copy,
                 WM_drag_free_imported_drag_ID);
}

/* ************* end drop *********** */

/* add handlers, stuff you only do once or on area/region changes */
static void node_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void node_header_region_draw(const bContext *C, ARegion *region)
{
  /* find and set the context */
  snode_set_context(C);

  ED_region_header(C, region);
}

/* used for header + main region */
static void node_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  wmNotifier *wmn = params->notifier;
  wmGizmoMap *gzmap = region->gizmo_map;

  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_NODE:
          ED_region_tag_redraw(region);
          break;
        case ND_SPACE_NODE_VIEW:
          WM_gizmomap_tag_refresh(gzmap);
          break;
      }
      break;
    case NC_SCREEN:
      if (wmn->data == ND_LAYOUTSET || wmn->action == NA_EDITED) {
        WM_gizmomap_tag_refresh(gzmap);
      }
      switch (wmn->data) {
        case ND_ANIMPLAY:
        case ND_LAYER:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_WM:
      if (wmn->data == ND_JOB) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      ED_region_tag_redraw(region);
      if (wmn->data == ND_RENDER_RESULT) {
        WM_gizmomap_tag_refresh(gzmap);
      }
      break;
    case NC_NODE:
      ED_region_tag_redraw(region);
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        WM_gizmomap_tag_refresh(gzmap);
      }
      break;
    case NC_MATERIAL:
    case NC_TEXTURE:
    case NC_WORLD:
    case NC_LINESTYLE:
      ED_region_tag_redraw(region);
      break;
    case NC_OBJECT:
      if (wmn->data == ND_OB_SHADING) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      else if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

const char *node_context_dir[] = {
    "selected_nodes", "active_node", "light", "material", "world", NULL};
static int /*eContextResult*/ node_context(const bContext *C,
                                           const char *member,
                                           bContextDataResult *result)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, node_context_dir);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "selected_nodes")) {
    bNode *node;

    if (snode->edittree) {
      for (node = snode->edittree->nodes.last; node; node = node->prev) {
        if (node->flag & NODE_SELECT) {
          CTX_data_list_add(result, &snode->edittree->id, &RNA_Node, node);
        }
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "active_node")) {
    if (snode->edittree) {
      bNode *node = nodeGetActive(snode->edittree);
      CTX_data_pointer_set(result, &snode->edittree->id, &RNA_Node, node);
    }

    CTX_data_type_set(result, CTX_DATA_TYPE_POINTER);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "node_previews")) {
    if (snode->nodetree) {
      CTX_data_pointer_set(
          result, &snode->nodetree->id, &RNA_NodeInstanceHash, snode->nodetree->previews);
    }

    CTX_data_type_set(result, CTX_DATA_TYPE_POINTER);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "material")) {
    if (snode->id && GS(snode->id->name) == ID_MA) {
      CTX_data_id_pointer_set(result, snode->id);
    }
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "light")) {
    if (snode->id && GS(snode->id->name) == ID_LA) {
      CTX_data_id_pointer_set(result, snode->id);
    }
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "world")) {
    if (snode->id && GS(snode->id->name) == ID_WO) {
      CTX_data_id_pointer_set(result, snode->id);
    }
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

static void node_widgets(void)
{
  /* create the widgetmap for the area here */
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(
      &(const struct wmGizmoMapType_Params){SPACE_NODE, RGN_TYPE_WINDOW});
  WM_gizmogrouptype_append_and_link(gzmap_type, NODE_GGT_backdrop_transform);
  WM_gizmogrouptype_append_and_link(gzmap_type, NODE_GGT_backdrop_crop);
  WM_gizmogrouptype_append_and_link(gzmap_type, NODE_GGT_backdrop_sun_beams);
  WM_gizmogrouptype_append_and_link(gzmap_type, NODE_GGT_backdrop_corner_pin);
}

static void node_id_remap(ScrArea *UNUSED(area), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceNode *snode = (SpaceNode *)slink;

  if (snode->id == old_id) {
    /* nasty DNA logic for SpaceNode:
     * ideally should be handled by editor code, but would be bad level call
     */
    BLI_freelistN(&snode->treepath);

    /* XXX Untested in case new_id != NULL... */
    snode->id = new_id;
    snode->from = NULL;
    snode->nodetree = NULL;
    snode->edittree = NULL;
  }
  else if (GS(old_id->name) == ID_OB) {
    if (snode->from == old_id) {
      if (new_id == NULL) {
        snode->flag &= ~SNODE_PIN;
      }
      snode->from = new_id;
    }
  }
  else if (GS(old_id->name) == ID_GD) {
    if ((ID *)snode->gpd == old_id) {
      snode->gpd = (bGPdata *)new_id;
      id_us_min(old_id);
      id_us_plus(new_id);
    }
  }
  else if (GS(old_id->name) == ID_NT) {
    bNodeTreePath *path, *path_next;

    for (path = snode->treepath.first; path; path = path->next) {
      if ((ID *)path->nodetree == old_id) {
        path->nodetree = (bNodeTree *)new_id;
        id_us_ensure_real(new_id);
      }
      if (path == snode->treepath.first) {
        /* first nodetree in path is same as snode->nodetree */
        snode->nodetree = path->nodetree;
      }
      if (path->nodetree == NULL) {
        break;
      }
    }

    /* remaining path entries are invalid, remove */
    for (; path; path = path_next) {
      path_next = path->next;

      BLI_remlink(&snode->treepath, path);
      MEM_freeN(path);
    }

    /* edittree is just the last in the path,
     * set this directly since the path may have been shortened above */
    if (snode->treepath.last) {
      path = snode->treepath.last;
      snode->edittree = path->nodetree;
    }
    else {
      snode->edittree = NULL;
    }
  }
}

static int node_space_subtype_get(ScrArea *area)
{
  SpaceNode *snode = area->spacedata.first;
  return rna_node_tree_idname_to_enum(snode->tree_idname);
}

static void node_space_subtype_set(ScrArea *area, int value)
{
  SpaceNode *snode = area->spacedata.first;
  ED_node_set_tree_type(snode, rna_node_tree_type_from_enum(value));
}

static void node_space_subtype_item_extend(bContext *C, EnumPropertyItem **item, int *totitem)
{
  bool free;
  const EnumPropertyItem *item_src = RNA_enum_node_tree_types_itemf_impl(C, &free);
  RNA_enum_items_add(item, totitem, item_src);
  if (free) {
    MEM_freeN((void *)item_src);
  }
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_node(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype node");
  ARegionType *art;

  st->spaceid = SPACE_NODE;
  strncpy(st->name, "Node", BKE_ST_MAXNAME);

  st->create = node_create;
  st->free = node_free;
  st->init = node_init;
  st->duplicate = node_duplicate;
  st->operatortypes = node_operatortypes;
  st->keymap = node_keymap;
  st->listener = node_area_listener;
  st->refresh = node_area_refresh;
  st->context = node_context;
  st->dropboxes = node_dropboxes;
  st->gizmos = node_widgets;
  st->id_remap = node_id_remap;
  st->space_subtype_item_extend = node_space_subtype_item_extend;
  st->space_subtype_get = node_space_subtype_get;
  st->space_subtype_set = node_space_subtype_set;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype node region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = node_main_region_init;
  art->draw = node_main_region_draw;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_GIZMO | ED_KEYMAP_TOOL | ED_KEYMAP_VIEW2D |
                    ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;
  art->listener = node_region_listener;
  art->cursor = node_cursor;
  art->event_cursor = true;
  art->clip_gizmo_events_by_ui = true;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype node region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = node_region_listener;
  art->init = node_header_region_init;
  art->draw = node_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: listview/buttons */
  art = MEM_callocN(sizeof(ARegionType), "spacetype node region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = node_region_listener;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_ui;
  art->init = node_buttons_region_init;
  art->draw = node_buttons_region_draw;
  BLI_addhead(&st->regiontypes, art);

  node_buttons_register(art);

  /* regions: toolbar */
  art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tools region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = 58; /* XXX */
  art->prefsizey = 50; /* XXX */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = node_region_listener;
  art->message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art->snap_size = ED_region_generic_tools_region_snap_size;
  art->init = node_toolbar_region_init;
  art->draw = node_toolbar_region_draw;
  BLI_addhead(&st->regiontypes, art);

  node_toolbar_register(art);

  BKE_spacetype_register(st);
}
