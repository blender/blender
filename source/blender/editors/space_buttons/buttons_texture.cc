/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation */

/** \file
 * \ingroup spbuttons
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_node_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_layer.h"
#include "BKE_linestyle.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_node_runtime.hh"
#include "BKE_paint.h"
#include "BKE_particle.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_node.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "../interface/interface_intern.hh"

#include "buttons_intern.h" /* own include */

static ScrArea *find_area_properties(const bContext *C);
static SpaceProperties *find_space_properties(const bContext *C);

/************************* Texture User **************************/

static void buttons_texture_user_socket_property_add(ListBase *users,
                                                     ID *id,
                                                     PointerRNA ptr,
                                                     PropertyRNA *prop,
                                                     bNodeTree *ntree,
                                                     bNode *node,
                                                     bNodeSocket *socket,
                                                     const char *category,
                                                     int icon,
                                                     const char *name)
{
  ButsTextureUser *user = MEM_cnew<ButsTextureUser>("ButsTextureUser");

  user->id = id;
  user->ptr = ptr;
  user->prop = prop;
  user->ntree = ntree;
  user->node = node;
  user->socket = socket;
  user->category = category;
  user->icon = icon;
  user->name = name;
  user->index = BLI_listbase_count(users);

  BLI_addtail(users, user);
}

static void buttons_texture_user_property_add(ListBase *users,
                                              ID *id,
                                              PointerRNA ptr,
                                              PropertyRNA *prop,
                                              const char *category,
                                              int icon,
                                              const char *name)
{
  ButsTextureUser *user = MEM_cnew<ButsTextureUser>("ButsTextureUser");

  user->id = id;
  user->ptr = ptr;
  user->prop = prop;
  user->category = category;
  user->icon = icon;
  user->name = name;
  user->index = BLI_listbase_count(users);

  BLI_addtail(users, user);
}

static void buttons_texture_user_node_add(ListBase *users,
                                          ID *id,
                                          bNodeTree *ntree,
                                          bNode *node,
                                          const char *category,
                                          int icon,
                                          const char *name)
{
  ButsTextureUser *user = MEM_cnew<ButsTextureUser>("ButsTextureUser");

  user->id = id;
  user->ntree = ntree;
  user->node = node;
  user->category = category;
  user->icon = icon;
  user->name = name;
  user->index = BLI_listbase_count(users);

  BLI_addtail(users, user);
}

static void buttons_texture_users_find_nodetree(ListBase *users,
                                                ID *id,
                                                bNodeTree *ntree,
                                                const char *category)
{
  bNode *node;

  if (ntree) {
    for (node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
      if (node->typeinfo->nclass == NODE_CLASS_TEXTURE) {
        PointerRNA ptr;
        // PropertyRNA *prop; /* UNUSED */

        RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
        // prop = RNA_struct_find_property(&ptr, "texture"); /* UNUSED */

        buttons_texture_user_node_add(
            users, id, ntree, node, category, RNA_struct_ui_icon(ptr.type), node->name);
      }
      else if (node->type == NODE_GROUP && node->id) {
        buttons_texture_users_find_nodetree(users, id, (bNodeTree *)node->id, category);
      }
    }
  }
}

static void buttons_texture_modifier_geonodes_users_add(
    Object *ob,
    NodesModifierData *nmd,
    bNodeTree *node_tree,
    ListBase *users,
    blender::Set<const bNodeTree *> &handled_groups)
{
  PointerRNA ptr;
  PropertyRNA *prop;

  for (bNode *node : node_tree->all_nodes()) {
    if (node->type == NODE_GROUP && node->id) {
      if (handled_groups.add(reinterpret_cast<bNodeTree *>(node->id))) {
        /* Recurse into the node group */
        buttons_texture_modifier_geonodes_users_add(
            ob, nmd, (bNodeTree *)node->id, users, handled_groups);
      }
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      if (socket->flag & SOCK_UNAVAIL) {
        continue;
      }
      if (socket->type != SOCK_TEXTURE) {
        continue;
      }
      RNA_pointer_create(&node_tree->id, &RNA_NodeSocket, socket, &ptr);
      prop = RNA_struct_find_property(&ptr, "default_value");

      PointerRNA texptr = RNA_property_pointer_get(&ptr, prop);
      Tex *tex = RNA_struct_is_a(texptr.type, &RNA_Texture) ? (Tex *)texptr.data : nullptr;
      if (tex != nullptr) {
        buttons_texture_user_socket_property_add(users,
                                                 &ob->id,
                                                 ptr,
                                                 prop,
                                                 node_tree,
                                                 node,
                                                 socket,
                                                 N_("Geometry Nodes"),
                                                 RNA_struct_ui_icon(ptr.type),
                                                 nmd->modifier.name);
      }
    }
  }
}

static void buttons_texture_modifier_foreach(void *userData,
                                             Object *ob,
                                             ModifierData *md,
                                             const char *propname)
{
  ListBase *users = static_cast<ListBase *>(userData);

  if (md->type == eModifierType_Nodes) {
    NodesModifierData *nmd = (NodesModifierData *)md;
    if (nmd->node_group != nullptr) {
      blender::Set<const bNodeTree *> handled_groups;
      buttons_texture_modifier_geonodes_users_add(ob, nmd, nmd->node_group, users, handled_groups);
    }
  }
  else {
    PointerRNA ptr;
    PropertyRNA *prop;

    RNA_pointer_create(&ob->id, &RNA_Modifier, md, &ptr);
    prop = RNA_struct_find_property(&ptr, propname);

    buttons_texture_user_property_add(
        users, &ob->id, ptr, prop, N_("Modifiers"), RNA_struct_ui_icon(ptr.type), md->name);
  }
}

static void buttons_texture_modifier_gpencil_foreach(void *userData,
                                                     Object *ob,
                                                     GpencilModifierData *md,
                                                     const char *propname)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  ListBase *users = static_cast<ListBase *>(userData);

  RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, &ptr);
  prop = RNA_struct_find_property(&ptr, propname);

  buttons_texture_user_property_add(users,
                                    &ob->id,
                                    ptr,
                                    prop,
                                    N_("Grease Pencil Modifiers"),
                                    RNA_struct_ui_icon(ptr.type),
                                    md->name);
}

static void buttons_texture_users_from_context(ListBase *users,
                                               const bContext *C,
                                               SpaceProperties *sbuts)
{
  Scene *scene = nullptr;
  Object *ob = nullptr;
  FreestyleLineStyle *linestyle = nullptr;
  Brush *brush = nullptr;
  ID *pinid = sbuts->pinid;
  bool limited_mode = (sbuts->flag & SB_TEX_USER_LIMITED) != 0;

  /* get data from context */
  if (pinid) {
    if (GS(pinid->name) == ID_SCE) {
      scene = (Scene *)pinid;
    }
    else if (GS(pinid->name) == ID_OB) {
      ob = (Object *)pinid;
    }
    else if (GS(pinid->name) == ID_BR) {
      brush = (Brush *)pinid;
    }
    else if (GS(pinid->name) == ID_LS) {
      linestyle = (FreestyleLineStyle *)pinid;
    }
  }

  if (!scene) {
    scene = CTX_data_scene(C);
  }

  const ID_Type id_type = ID_Type(pinid != nullptr ? GS(pinid->name) : -1);
  if (!pinid || id_type == ID_SCE) {
    wmWindow *win = CTX_wm_window(C);
    ViewLayer *view_layer = (win->scene == scene) ? WM_window_get_active_view_layer(win) :
                                                    BKE_view_layer_default_view(scene);

    brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
    linestyle = BKE_linestyle_active_from_view_layer(view_layer);
    BKE_view_layer_synced_ensure(scene, view_layer);
    ob = BKE_view_layer_active_object_get(view_layer);
  }

  /* fill users */
  BLI_listbase_clear(users);

  if (linestyle && !limited_mode) {
    buttons_texture_users_find_nodetree(
        users, &linestyle->id, linestyle->nodetree, N_("Line Style"));
  }

  if (ob) {
    ParticleSystem *psys = psys_get_current(ob);
    MTex *mtex;
    int a;

    /* modifiers */
    BKE_modifiers_foreach_tex_link(ob, buttons_texture_modifier_foreach, users);

    /* grease pencil modifiers */
    BKE_gpencil_modifiers_foreach_tex_link(ob, buttons_texture_modifier_gpencil_foreach, users);

    /* particle systems */
    if (psys && !limited_mode) {
      for (a = 0; a < MAX_MTEX; a++) {
        mtex = psys->part->mtex[a];

        if (mtex) {
          PointerRNA ptr;
          PropertyRNA *prop;

          RNA_pointer_create(&psys->part->id, &RNA_ParticleSettingsTextureSlot, mtex, &ptr);
          prop = RNA_struct_find_property(&ptr, "texture");

          buttons_texture_user_property_add(users,
                                            &psys->part->id,
                                            ptr,
                                            prop,
                                            N_("Particles"),
                                            RNA_struct_ui_icon(&RNA_ParticleSettings),
                                            psys->name);
        }
      }
    }

    /* field */
    if (ob->pd && ob->pd->forcefield == PFIELD_TEXTURE) {
      PointerRNA ptr;
      PropertyRNA *prop;

      RNA_pointer_create(&ob->id, &RNA_FieldSettings, ob->pd, &ptr);
      prop = RNA_struct_find_property(&ptr, "texture");

      buttons_texture_user_property_add(
          users, &ob->id, ptr, prop, N_("Fields"), ICON_FORCE_TEXTURE, IFACE_("Texture Field"));
    }
  }

  /* brush */
  if (brush) {
    PointerRNA ptr;
    PropertyRNA *prop;

    /* texture */
    RNA_pointer_create(&brush->id, &RNA_BrushTextureSlot, &brush->mtex, &ptr);
    prop = RNA_struct_find_property(&ptr, "texture");

    buttons_texture_user_property_add(
        users, &brush->id, ptr, prop, N_("Brush"), ICON_BRUSH_DATA, IFACE_("Brush"));

    /* mask texture */
    RNA_pointer_create(&brush->id, &RNA_BrushTextureSlot, &brush->mask_mtex, &ptr);
    prop = RNA_struct_find_property(&ptr, "texture");

    buttons_texture_user_property_add(
        users, &brush->id, ptr, prop, N_("Brush"), ICON_BRUSH_DATA, IFACE_("Brush Mask"));
  }
}

void buttons_texture_context_compute(const bContext *C, SpaceProperties *sbuts)
{
  /* gather available texture users in context. runs on every draw of
   * properties editor, before the buttons are created. */
  ButsContextTexture *ct = static_cast<ButsContextTexture *>(sbuts->texuser);
  ID *pinid = sbuts->pinid;

  if (!ct) {
    ct = MEM_cnew<ButsContextTexture>("ButsContextTexture");
    sbuts->texuser = ct;
  }
  else {
    BLI_freelistN(&ct->users);
  }

  buttons_texture_users_from_context(&ct->users, C, sbuts);

  if (pinid && GS(pinid->name) == ID_TE) {
    ct->user = nullptr;
    ct->texture = (Tex *)pinid;
  }
  else {
    /* set one user as active based on active index */
    if (ct->index >= BLI_listbase_count_at_most(&ct->users, ct->index + 1)) {
      ct->index = 0;
    }

    ct->user = static_cast<ButsTextureUser *>(BLI_findlink(&ct->users, ct->index));
    ct->texture = nullptr;

    if (ct->user) {
      if (ct->user->node != nullptr) {
        /* Detect change of active texture node in same node tree, in that
         * case we also automatically switch to the other node. */
        if ((ct->user->node->flag & NODE_ACTIVE_TEXTURE) == 0) {
          ButsTextureUser *user;
          for (user = static_cast<ButsTextureUser *>(ct->users.first); user; user = user->next) {
            if (user->ntree == ct->user->ntree && user->node != ct->user->node) {
              if (user->node->flag & NODE_ACTIVE_TEXTURE) {
                ct->user = user;
                ct->index = BLI_findindex(&ct->users, user);
                break;
              }
            }
          }
        }
      }
      if (ct->user->ptr.data) {
        PointerRNA texptr;
        Tex *tex;

        /* Get texture datablock pointer if it's a property. */
        texptr = RNA_property_pointer_get(&ct->user->ptr, ct->user->prop);
        tex = RNA_struct_is_a(texptr.type, &RNA_Texture) ? static_cast<Tex *>(texptr.data) :
                                                           nullptr;

        ct->texture = tex;
      }
    }
  }
}

static void template_texture_select(bContext *C, void *user_p, void * /*arg*/)
{
  /* callback when selecting a texture user in the menu */
  SpaceProperties *sbuts = find_space_properties(C);
  ButsContextTexture *ct = (sbuts) ? static_cast<ButsContextTexture *>(sbuts->texuser) : nullptr;
  ButsTextureUser *user = (ButsTextureUser *)user_p;
  PointerRNA texptr;
  Tex *tex;

  if (!ct) {
    return;
  }

  /* set user as active */
  if (user->node) {
    ED_node_set_active(CTX_data_main(C), nullptr, user->ntree, user->node, nullptr);
    ct->texture = nullptr;

    /* Not totally sure if we should also change selection? */
    for (bNode *node : user->ntree->all_nodes()) {
      nodeSetSelected(node, false);
    }
    nodeSetSelected(user->node, true);
    WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  }
  if (user->ptr.data) {
    texptr = RNA_property_pointer_get(&user->ptr, user->prop);
    tex = RNA_struct_is_a(texptr.type, &RNA_Texture) ? static_cast<Tex *>(texptr.data) : nullptr;

    ct->texture = tex;

    if (user->ptr.type == &RNA_ParticleSettingsTextureSlot) {
      /* stupid exception for particle systems which still uses influence
       * from the old texture system, set the active texture slots as well */
      ParticleSettings *part = (ParticleSettings *)user->ptr.owner_id;
      int a;

      for (a = 0; a < MAX_MTEX; a++) {
        if (user->ptr.data == part->mtex[a]) {
          part->texact = a;
        }
      }
    }

    if (sbuts && tex) {
      sbuts->preview = 1;
    }
  }

  ct->user = user;
  ct->index = user->index;
}

static void template_texture_user_menu(bContext *C, uiLayout *layout, void * /*arg*/)
{
  /* callback when opening texture user selection menu, to create buttons. */
  SpaceProperties *sbuts = CTX_wm_space_properties(C);
  ButsContextTexture *ct = static_cast<ButsContextTexture *>(sbuts->texuser);
  ButsTextureUser *user;
  uiBlock *block = uiLayoutGetBlock(layout);
  const char *last_category = nullptr;

  for (user = static_cast<ButsTextureUser *>(ct->users.first); user; user = user->next) {
    uiBut *but;
    char name[UI_MAX_NAME_STR];

    /* add label per category */
    if (!last_category || !STREQ(last_category, user->category)) {
      uiItemL(layout, IFACE_(user->category), ICON_NONE);
      but = static_cast<uiBut *>(block->buttons.last);
      but->drawflag = UI_BUT_TEXT_LEFT;
    }

    /* create button */
    if (user->prop) {
      PointerRNA texptr = RNA_property_pointer_get(&user->ptr, user->prop);
      Tex *tex = static_cast<Tex *>(texptr.data);

      if (tex) {
        BLI_snprintf(name, UI_MAX_NAME_STR, "  %s - %s", user->name, tex->id.name + 2);
      }
      else {
        BLI_snprintf(name, UI_MAX_NAME_STR, "  %s", user->name);
      }
    }
    else {
      BLI_snprintf(name, UI_MAX_NAME_STR, "  %s", user->name);
    }

    but = uiDefIconTextBut(block,
                           UI_BTYPE_BUT,
                           0,
                           user->icon,
                           name,
                           0,
                           0,
                           UI_UNIT_X * 4,
                           UI_UNIT_Y,
                           nullptr,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           "");
    UI_but_funcN_set(but, template_texture_select, MEM_dupallocN(user), nullptr);

    last_category = user->category;
  }

  UI_block_flag_enable(block, UI_BLOCK_NO_FLIP);
}

void uiTemplateTextureUser(uiLayout *layout, bContext *C)
{
  /* Texture user selection drop-down menu. the available users have been
   * gathered before drawing in #ButsContextTexture, we merely need to
   * display the current item. */
  SpaceProperties *sbuts = CTX_wm_space_properties(C);
  ButsContextTexture *ct = (sbuts) ? static_cast<ButsContextTexture *>(sbuts->texuser) : nullptr;
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but;
  ButsTextureUser *user;
  char name[UI_MAX_NAME_STR];

  if (!ct) {
    return;
  }

  /* get current user */
  user = ct->user;

  if (!user) {
    uiItemL(layout, TIP_("No textures in context"), ICON_NONE);
    return;
  }

  /* create button */
  BLI_strncpy(name, user->name, UI_MAX_NAME_STR);

  if (user->icon) {
    but = uiDefIconTextMenuBut(block,
                               template_texture_user_menu,
                               nullptr,
                               user->icon,
                               name,
                               0,
                               0,
                               UI_UNIT_X * 4,
                               UI_UNIT_Y,
                               "");
  }
  else {
    but = uiDefMenuBut(
        block, template_texture_user_menu, nullptr, name, 0, 0, UI_UNIT_X * 4, UI_UNIT_Y, "");
  }

  /* some cosmetic tweaks */
  UI_but_type_set_menu_from_pulldown(but);

  but->flag &= ~UI_BUT_ICON_SUBMENU;
}

/************************* Texture Show **************************/

static ScrArea *find_area_properties(const bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  Object *ob = CTX_data_active_object(C);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->spacetype == SPACE_PROPERTIES) {
      /* Only if unpinned, or if pinned object matches. */
      SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);
      ID *pinid = sbuts->pinid;
      if (pinid == nullptr || ((GS(pinid->name) == ID_OB) && (Object *)pinid == ob)) {
        return area;
      }
    }
  }

  return nullptr;
}

static SpaceProperties *find_space_properties(const bContext *C)
{
  ScrArea *area = find_area_properties(C);
  if (area != nullptr) {
    return static_cast<SpaceProperties *>(area->spacedata.first);
  }

  return nullptr;
}

static void template_texture_show(bContext *C, void *data_p, void *prop_p)
{
  if (data_p == nullptr || prop_p == nullptr) {
    return;
  }

  ScrArea *area = find_area_properties(C);
  if (area == nullptr) {
    return;
  }

  SpaceProperties *sbuts = (SpaceProperties *)area->spacedata.first;
  ButsContextTexture *ct = (sbuts) ? static_cast<ButsContextTexture *>(sbuts->texuser) : nullptr;
  if (!ct) {
    return;
  }

  ButsTextureUser *user;
  for (user = static_cast<ButsTextureUser *>(ct->users.first); user; user = user->next) {
    if (user->ptr.data == data_p && user->prop == prop_p) {
      break;
    }
  }

  if (user) {
    /* select texture */
    template_texture_select(C, user, nullptr);

    /* change context */
    sbuts->mainb = BCONTEXT_TEXTURE;
    sbuts->mainbuser = sbuts->mainb;
    sbuts->preview = 1;

    /* redraw editor */
    ED_area_tag_redraw(area);
  }
}

void uiTemplateTextureShow(uiLayout *layout, const bContext *C, PointerRNA *ptr, PropertyRNA *prop)
{
  /* Only show the button if there is actually a texture assigned. */
  Tex *texture = static_cast<Tex *>(RNA_property_pointer_get(ptr, prop).data);
  if (texture == nullptr) {
    return;
  }

  /* Only show the button if we are not in the Properties Editor's texture tab. */
  SpaceProperties *sbuts_context = CTX_wm_space_properties(C);
  if (sbuts_context != nullptr && sbuts_context->mainb == BCONTEXT_TEXTURE) {
    return;
  }

  SpaceProperties *sbuts = find_space_properties(C);
  ButsContextTexture *ct = (sbuts) ? static_cast<ButsContextTexture *>(sbuts->texuser) : nullptr;

  /* find corresponding texture user */
  ButsTextureUser *user;
  bool user_found = false;
  if (ct != nullptr) {
    for (user = static_cast<ButsTextureUser *>(ct->users.first); user; user = user->next) {
      if (user->ptr.data == ptr->data && user->prop == prop) {
        user_found = true;
        break;
      }
    }
  }

  /* Draw button (disabled if we cannot find a Properties Editor to display this in). */
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but;
  but = uiDefIconBut(block,
                     UI_BTYPE_BUT,
                     0,
                     ICON_PROPERTIES,
                     0,
                     0,
                     UI_UNIT_X,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0.0,
                     0.0,
                     TIP_("Show texture in texture tab"));
  UI_but_func_set(but,
                  template_texture_show,
                  user_found ? user->ptr.data : nullptr,
                  user_found ? user->prop : nullptr);
  if (ct == nullptr) {
    UI_but_disable(but, TIP_("No (unpinned) Properties Editor found to display texture in"));
  }
  else if (!user_found) {
    UI_but_disable(but, TIP_("No texture user found"));
  }
}
