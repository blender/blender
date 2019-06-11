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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spimage
 */

#include <string.h>
#include <stdio.h>

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_node.h"
#include "BKE_screen.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_image.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "image_intern.h"

#define B_NOP -1
#define MAX_IMAGE_INFO_LEN 128

/* gets active viewer user */
struct ImageUser *ntree_get_active_iuser(bNodeTree *ntree)
{
  bNode *node;

  if (ntree) {
    for (node = ntree->nodes.first; node; node = node->next) {
      if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
        if (node->flag & NODE_DO_OUTPUT) {
          return node->storage;
        }
      }
    }
  }
  return NULL;
}

/* ********************* callbacks for standard image buttons *************** */

static void ui_imageuser_slot_menu(bContext *UNUSED(C), uiLayout *layout, void *image_p)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  Image *image = image_p;
  int slot_id;

  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("Slot"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");
  uiItemS(layout);

  slot_id = BLI_listbase_count(&image->renderslots) - 1;
  for (RenderSlot *slot = image->renderslots.last; slot; slot = slot->prev) {
    char str[64];
    if (slot->name[0] != '\0') {
      BLI_strncpy(str, slot->name, sizeof(str));
    }
    else {
      BLI_snprintf(str, sizeof(str), IFACE_("Slot %d"), slot_id + 1);
    }
    uiDefButS(block,
              UI_BTYPE_BUT_MENU,
              B_NOP,
              str,
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &image->render_slot,
              (float)slot_id,
              0.0,
              0,
              -1,
              "");
    slot_id--;
  }
}

static bool ui_imageuser_slot_menu_step(bContext *C, int direction, void *image_p)
{
  Image *image = image_p;

  if (ED_image_slot_cycle(image, direction)) {
    WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
    return true;
  }
  else {
    return true;
  }
}

static const char *ui_imageuser_layer_fake_name(RenderResult *rr)
{
  RenderView *rv = RE_RenderViewGetById(rr, 0);
  if (rv->rectf) {
    return IFACE_("Composite");
  }
  else if (rv->rect32) {
    return IFACE_("Sequence");
  }
  else {
    return NULL;
  }
}

/* workaround for passing many args */
struct ImageUI_Data {
  Image *image;
  ImageUser *iuser;
  int rpass_index;
};

static struct ImageUI_Data *ui_imageuser_data_copy(const struct ImageUI_Data *rnd_pt_src)
{
  struct ImageUI_Data *rnd_pt_dst = MEM_mallocN(sizeof(*rnd_pt_src), __func__);
  memcpy(rnd_pt_dst, rnd_pt_src, sizeof(*rnd_pt_src));
  return rnd_pt_dst;
}

static void ui_imageuser_layer_menu(bContext *UNUSED(C), uiLayout *layout, void *rnd_pt)
{
  struct ImageUI_Data *rnd_data = rnd_pt;
  uiBlock *block = uiLayoutGetBlock(layout);
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  Scene *scene = iuser->scene;
  RenderResult *rr;
  RenderLayer *rl;
  RenderLayer rl_fake = {NULL};
  const char *fake_name;
  int nr;

  /* may have been freed since drawing */
  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == NULL)) {
    return;
  }

  UI_block_layout_set_current(block, layout);
  uiLayoutColumn(layout, false);

  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("Layer"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");
  uiItemS(layout);

  nr = BLI_listbase_count(&rr->layers) - 1;
  fake_name = ui_imageuser_layer_fake_name(rr);

  if (fake_name) {
    BLI_strncpy(rl_fake.name, fake_name, sizeof(rl_fake.name));
    nr += 1;
  }

  for (rl = rr->layers.last; rl; rl = rl->prev, nr--) {
  final:
    uiDefButS(block,
              UI_BTYPE_BUT_MENU,
              B_NOP,
              rl->name,
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->layer,
              (float)nr,
              0.0,
              0,
              -1,
              "");
  }

  if (fake_name) {
    fake_name = NULL;
    rl = &rl_fake;
    goto final;
  }

  BLI_assert(nr == -1);

  BKE_image_release_renderresult(scene, image);
}

static void ui_imageuser_pass_menu(bContext *UNUSED(C), uiLayout *layout, void *rnd_pt)
{
  struct ImageUI_Data *rnd_data = rnd_pt;
  uiBlock *block = uiLayoutGetBlock(layout);
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  /* (rpass_index == -1) means composite result */
  const int rpass_index = rnd_data->rpass_index;
  Scene *scene = iuser->scene;
  RenderResult *rr;
  RenderLayer *rl;
  RenderPass *rpass;
  int nr;

  /* may have been freed since drawing */
  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == NULL)) {
    return;
  }

  rl = BLI_findlink(&rr->layers, rpass_index);

  UI_block_layout_set_current(block, layout);
  uiLayoutColumn(layout, false);

  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("Pass"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");

  uiItemS(layout);

  nr = (rl == NULL) ? 1 : 0;

  ListBase added_passes;
  BLI_listbase_clear(&added_passes);

  /* rendered results don't have a Combined pass */
  /* multiview: the ordering must be ascending, so the left-most pass is always the one picked */
  for (rpass = rl ? rl->passes.first : NULL; rpass; rpass = rpass->next, nr++) {
    /* just show one pass of each kind */
    if (BLI_findstring_ptr(&added_passes, rpass->name, offsetof(LinkData, data))) {
      continue;
    }
    BLI_addtail(&added_passes, BLI_genericNodeN(rpass->name));

    uiDefButS(block,
              UI_BTYPE_BUT_MENU,
              B_NOP,
              IFACE_(rpass->name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->pass,
              (float)nr,
              0.0,
              0,
              -1,
              "");
  }

  BLI_freelistN(&added_passes);

  BKE_image_release_renderresult(scene, image);
}

/**************************** view menus *****************************/
static void ui_imageuser_view_menu_rr(bContext *UNUSED(C), uiLayout *layout, void *rnd_pt)
{
  struct ImageUI_Data *rnd_data = rnd_pt;
  uiBlock *block = uiLayoutGetBlock(layout);
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  RenderResult *rr;
  RenderView *rview;
  int nr;
  Scene *scene = iuser->scene;

  /* may have been freed since drawing */
  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == NULL)) {
    return;
  }

  UI_block_layout_set_current(block, layout);
  uiLayoutColumn(layout, false);

  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("View"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");

  uiItemS(layout);

  nr = (rr ? BLI_listbase_count(&rr->views) : 0) - 1;
  for (rview = rr ? rr->views.last : NULL; rview; rview = rview->prev, nr--) {
    uiDefButS(block,
              UI_BTYPE_BUT_MENU,
              B_NOP,
              IFACE_(rview->name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->view,
              (float)nr,
              0.0,
              0,
              -1,
              "");
  }

  BKE_image_release_renderresult(scene, image);
}

static void ui_imageuser_view_menu_multiview(bContext *UNUSED(C), uiLayout *layout, void *rnd_pt)
{
  struct ImageUI_Data *rnd_data = rnd_pt;
  uiBlock *block = uiLayoutGetBlock(layout);
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  int nr;
  ImageView *iv;

  UI_block_layout_set_current(block, layout);
  uiLayoutColumn(layout, false);

  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("View"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");

  uiItemS(layout);

  nr = BLI_listbase_count(&image->views) - 1;
  for (iv = image->views.last; iv; iv = iv->prev, nr--) {
    uiDefButS(block,
              UI_BTYPE_BUT_MENU,
              B_NOP,
              IFACE_(iv->name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->view,
              (float)nr,
              0.0,
              0,
              -1,
              "");
  }
}

/* 5 layer button callbacks... */
static void image_multi_cb(bContext *C, void *rnd_pt, void *rr_v)
{
  struct ImageUI_Data *rnd_data = rnd_pt;
  ImageUser *iuser = rnd_data->iuser;

  BKE_image_multilayer_index(rr_v, iuser);
  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
}

static bool ui_imageuser_layer_menu_step(bContext *C, int direction, void *rnd_pt)
{
  Scene *scene = CTX_data_scene(C);
  struct ImageUI_Data *rnd_data = rnd_pt;
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  RenderResult *rr;
  bool changed = false;

  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == NULL)) {
    return false;
  }

  if (direction == -1) {
    if (iuser->layer > 0) {
      iuser->layer--;
      changed = true;
    }
  }
  else if (direction == 1) {
    int tot = BLI_listbase_count(&rr->layers);

    if (RE_HasCombinedLayer(rr)) {
      tot++; /* fake compo/sequencer layer */
    }

    if (iuser->layer < tot - 1) {
      iuser->layer++;
      changed = true;
    }
  }
  else {
    BLI_assert(0);
  }

  BKE_image_release_renderresult(scene, image);

  if (changed) {
    BKE_image_multilayer_index(rr, iuser);
    WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
  }

  return changed;
}

static bool ui_imageuser_pass_menu_step(bContext *C, int direction, void *rnd_pt)
{
  Scene *scene = CTX_data_scene(C);
  struct ImageUI_Data *rnd_data = rnd_pt;
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  RenderResult *rr;
  bool changed = false;
  int layer = iuser->layer;
  RenderLayer *rl;
  RenderPass *rpass;

  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == NULL)) {
    BKE_image_release_renderresult(scene, image);
    return false;
  }

  if (RE_HasCombinedLayer(rr)) {
    layer -= 1;
  }

  rl = BLI_findlink(&rr->layers, layer);
  if (rl == NULL) {
    BKE_image_release_renderresult(scene, image);
    return false;
  }

  rpass = BLI_findlink(&rl->passes, iuser->pass);
  if (rpass == NULL) {
    BKE_image_release_renderresult(scene, image);
    return false;
  }

  /* note, this looks reversed, but matches menu direction */
  if (direction == -1) {
    RenderPass *rp;
    int rp_index = iuser->pass + 1;

    for (rp = rpass->next; rp; rp = rp->next, rp_index++) {
      if (!STREQ(rp->name, rpass->name)) {
        iuser->pass = rp_index;
        changed = true;
        break;
      }
    }
  }
  else if (direction == 1) {
    RenderPass *rp;
    int rp_index = 0;

    if (iuser->pass == 0) {
      BKE_image_release_renderresult(scene, image);
      return false;
    }

    for (rp = rl->passes.first; rp; rp = rp->next, rp_index++) {
      if (STREQ(rp->name, rpass->name)) {
        iuser->pass = rp_index - 1;
        changed = true;
        break;
      }
    }
  }
  else {
    BLI_assert(0);
  }

  BKE_image_release_renderresult(scene, image);

  if (changed) {
    BKE_image_multilayer_index(rr, iuser);
    WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
  }

  return changed;
}

/* 5 view button callbacks... */
static void image_multiview_cb(bContext *C, void *rnd_pt, void *UNUSED(arg_v))
{
  struct ImageUI_Data *rnd_data = rnd_pt;
  Image *ima = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;

  BKE_image_multiview_index(ima, iuser);
  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
}

static void uiblock_layer_pass_buttons(
    uiLayout *layout, Image *image, RenderResult *rr, ImageUser *iuser, int w, short *render_slot)
{
  struct ImageUI_Data rnd_pt_local, *rnd_pt = NULL;
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but;
  RenderLayer *rl = NULL;
  int wmenu1, wmenu2, wmenu3, wmenu4;
  const char *fake_name;
  const char *display_name = "";
  const bool show_stereo = (iuser->flag & IMA_SHOW_STEREO) != 0;

  if (iuser->scene == NULL) {
    return;
  }

  uiLayoutRow(layout, true);

  /* layer menu is 1/3 larger than pass */
  wmenu1 = (2 * w) / 5;
  wmenu2 = (3 * w) / 5;
  wmenu3 = (3 * w) / 6;
  wmenu4 = (3 * w) / 6;

  rnd_pt_local.image = image;
  rnd_pt_local.iuser = iuser;
  rnd_pt_local.rpass_index = 0;

  /* menu buts */
  if (render_slot) {
    char str[64];
    RenderSlot *slot = BKE_image_get_renderslot(image, *render_slot);
    if (slot && slot->name[0] != '\0') {
      BLI_strncpy(str, slot->name, sizeof(str));
    }
    else {
      BLI_snprintf(str, sizeof(str), IFACE_("Slot %d"), *render_slot + 1);
    }

    rnd_pt = ui_imageuser_data_copy(&rnd_pt_local);
    but = uiDefMenuBut(
        block, ui_imageuser_slot_menu, image, str, 0, 0, wmenu1, UI_UNIT_Y, TIP_("Select Slot"));
    UI_but_func_menu_step_set(but, ui_imageuser_slot_menu_step);
    UI_but_funcN_set(but, image_multi_cb, rnd_pt, rr);
    UI_but_type_set_menu_from_pulldown(but);
    rnd_pt = NULL;
  }

  if (rr) {
    RenderPass *rpass;
    RenderView *rview;
    int rpass_index;

    /* layer */
    fake_name = ui_imageuser_layer_fake_name(rr);
    rpass_index = iuser->layer - (fake_name ? 1 : 0);
    rl = BLI_findlink(&rr->layers, rpass_index);
    rnd_pt_local.rpass_index = rpass_index;

    if (RE_layers_have_name(rr)) {
      display_name = rl ? rl->name : (fake_name ? fake_name : "");
      rnd_pt = ui_imageuser_data_copy(&rnd_pt_local);
      but = uiDefMenuBut(block,
                         ui_imageuser_layer_menu,
                         rnd_pt,
                         display_name,
                         0,
                         0,
                         wmenu2,
                         UI_UNIT_Y,
                         TIP_("Select Layer"));
      UI_but_func_menu_step_set(but, ui_imageuser_layer_menu_step);
      UI_but_funcN_set(but, image_multi_cb, rnd_pt, rr);
      UI_but_type_set_menu_from_pulldown(but);
      rnd_pt = NULL;
    }

    /* pass */
    rpass = (rl ? BLI_findlink(&rl->passes, iuser->pass) : NULL);

    if (rpass && RE_passes_have_name(rl)) {
      display_name = rpass->name;
      rnd_pt = ui_imageuser_data_copy(&rnd_pt_local);
      but = uiDefMenuBut(block,
                         ui_imageuser_pass_menu,
                         rnd_pt,
                         IFACE_(display_name),
                         0,
                         0,
                         wmenu3,
                         UI_UNIT_Y,
                         TIP_("Select Pass"));
      UI_but_func_menu_step_set(but, ui_imageuser_pass_menu_step);
      UI_but_funcN_set(but, image_multi_cb, rnd_pt, rr);
      UI_but_type_set_menu_from_pulldown(but);
      rnd_pt = NULL;
    }

    /* view */
    if (BLI_listbase_count_at_most(&rr->views, 2) > 1 &&
        ((!show_stereo) || (!RE_RenderResult_is_stereo(rr)))) {
      rview = BLI_findlink(&rr->views, iuser->view);
      display_name = rview ? rview->name : "";

      rnd_pt = ui_imageuser_data_copy(&rnd_pt_local);
      but = uiDefMenuBut(block,
                         ui_imageuser_view_menu_rr,
                         rnd_pt,
                         display_name,
                         0,
                         0,
                         wmenu4,
                         UI_UNIT_Y,
                         TIP_("Select View"));
      UI_but_funcN_set(but, image_multi_cb, rnd_pt, rr);
      UI_but_type_set_menu_from_pulldown(but);
      rnd_pt = NULL;
    }
  }

  /* stereo image */
  else if ((BKE_image_is_stereo(image) && (!show_stereo)) ||
           (BKE_image_is_multiview(image) && !BKE_image_is_stereo(image))) {
    ImageView *iv;
    int nr = 0;

    for (iv = image->views.first; iv; iv = iv->next) {
      if (nr++ == iuser->view) {
        display_name = iv->name;
        break;
      }
    }

    rnd_pt = ui_imageuser_data_copy(&rnd_pt_local);
    but = uiDefMenuBut(block,
                       ui_imageuser_view_menu_multiview,
                       rnd_pt,
                       display_name,
                       0,
                       0,
                       wmenu1,
                       UI_UNIT_Y,
                       TIP_("Select View"));
    UI_but_funcN_set(but, image_multiview_cb, rnd_pt, NULL);
    UI_but_type_set_menu_from_pulldown(but);
    rnd_pt = NULL;
  }
}

// XXX HACK!
// static int packdummy=0;

typedef struct RNAUpdateCb {
  PointerRNA ptr;
  PropertyRNA *prop;
  ImageUser *iuser;
} RNAUpdateCb;

static void rna_update_cb(bContext *C, void *arg_cb, void *UNUSED(arg))
{
  RNAUpdateCb *cb = (RNAUpdateCb *)arg_cb;

  /* ideally this would be done by RNA itself, but there we have
   * no image user available, so we just update this flag here */
  cb->iuser->ok = 1;

  /* we call update here on the pointer property, this way the
   * owner of the image pointer can still define it's own update
   * and notifier */
  RNA_property_update(C, &cb->ptr, cb->prop);
}

static bool image_has_alpha(Image *ima, ImageUser *iuser)
{
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
  if (ibuf == NULL) {
    return false;
  }

  int imtype = BKE_image_ftype_to_imtype(ibuf->ftype, &ibuf->foptions);
  char valid_channels = BKE_imtype_valid_channels(imtype, false);
  bool has_alpha = (valid_channels & IMA_CHAN_FLAG_ALPHA) != 0;

  BKE_image_release_ibuf(ima, ibuf, NULL);

  return has_alpha;
}

void uiTemplateImage(uiLayout *layout,
                     bContext *C,
                     PointerRNA *ptr,
                     const char *propname,
                     PointerRNA *userptr,
                     bool compact,
                     bool multiview)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (!prop) {
    printf(
        "%s: property not found: %s.%s\n", __func__, RNA_struct_identifier(ptr->type), propname);
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname);
    return;
  }

  uiBlock *block = uiLayoutGetBlock(layout);

  PointerRNA imaptr = RNA_property_pointer_get(ptr, prop);
  Image *ima = imaptr.data;
  ImageUser *iuser = userptr->data;

  Scene *scene = CTX_data_scene(C);
  BKE_image_user_frame_calc(iuser, (int)scene->r.cfra);

  uiLayoutSetContextPointer(layout, "edit_image", &imaptr);
  uiLayoutSetContextPointer(layout, "edit_image_user", userptr);

  SpaceImage *space_image = CTX_wm_space_image(C);
  if (!compact && (space_image == NULL || iuser != &space_image->iuser)) {
    uiTemplateID(layout,
                 C,
                 ptr,
                 propname,
                 ima ? NULL : "IMAGE_OT_new",
                 "IMAGE_OT_open",
                 NULL,
                 UI_TEMPLATE_ID_FILTER_ALL,
                 false);

    if (ima != NULL) {
      uiItemS(layout);
    }
  }

  if (ima == NULL) {
    return;
  }

  if (ima->source == IMA_SRC_VIEWER) {
    /* Viewer images. */
    uiTemplateImageInfo(layout, C, ima, iuser);

    if (ima->type == IMA_TYPE_COMPOSITE) {
    }
    else if (ima->type == IMA_TYPE_R_RESULT) {
      /* browse layer/passes */
      RenderResult *rr;
      const float dpi_fac = UI_DPI_FAC;
      const int menus_width = 230 * dpi_fac;

      /* use BKE_image_acquire_renderresult  so we get the correct slot in the menu */
      rr = BKE_image_acquire_renderresult(scene, ima);
      uiblock_layer_pass_buttons(layout, ima, rr, iuser, menus_width, &ima->render_slot);
      BKE_image_release_renderresult(scene, ima);
    }

    return;
  }

  /* Set custom callback for property updates. */
  RNAUpdateCb *cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
  cb->ptr = *ptr;
  cb->prop = prop;
  cb->iuser = iuser;
  UI_block_funcN_set(block, rna_update_cb, cb, NULL);

  /* Disable editing if image was modified, to avoid losing changes. */
  const bool is_dirty = BKE_image_is_dirty(ima);
  if (is_dirty) {
    uiLayout *row = uiLayoutRow(layout, true);
    uiItemO(row, IFACE_("Save"), ICON_NONE, "image.save");
    uiItemO(row, IFACE_("Discard"), ICON_NONE, "image.reload");
    uiItemS(layout);
  }

  layout = uiLayoutColumn(layout, false);
  uiLayoutSetEnabled(layout, !is_dirty);
  uiLayoutSetPropDecorate(layout, false);

  /* Image source */
  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetPropSep(col, true);
    uiItemR(col, &imaptr, "source", 0, NULL, ICON_NONE);
  }

  /* Filepath */
  const bool is_packed = BKE_image_has_packedfile(ima);
  const bool no_filepath = is_packed && !BKE_image_has_filepath(ima);

  if ((ima->source != IMA_SRC_GENERATED) && !no_filepath) {
    uiItemS(layout);

    uiLayout *row = uiLayoutRow(layout, true);
    if (is_packed) {
      uiItemO(row, "", ICON_PACKAGE, "image.unpack");
    }
    else {
      uiItemO(row, "", ICON_UGLYPACKAGE, "image.pack");
    }

    row = uiLayoutRow(row, true);
    uiLayoutSetEnabled(row, is_packed == false);
    uiItemR(row, &imaptr, "filepath", 0, "", ICON_NONE);
    uiItemO(row, "", ICON_FILE_REFRESH, "image.reload");
  }

  /* Image layers and Info */
  if (ima->source == IMA_SRC_GENERATED) {
    uiItemS(layout);

    /* Generated */
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetPropSep(col, true);

    uiLayout *sub = uiLayoutColumn(col, true);
    uiItemR(sub, &imaptr, "generated_width", 0, "X", ICON_NONE);
    uiItemR(sub, &imaptr, "generated_height", 0, "Y", ICON_NONE);

    uiItemR(col, &imaptr, "use_generated_float", 0, NULL, ICON_NONE);

    uiItemS(col);

    uiItemR(col, &imaptr, "generated_type", UI_ITEM_R_EXPAND, IFACE_("Type"), ICON_NONE);
    if (ima->gen_type == IMA_GENTYPE_BLANK) {
      uiItemR(col, &imaptr, "generated_color", 0, NULL, ICON_NONE);
    }
  }
  else if (compact == 0) {
    uiTemplateImageInfo(layout, C, ima, iuser);
  }
  if (ima->type == IMA_TYPE_MULTILAYER && ima->rr) {
    uiItemS(layout);

    const float dpi_fac = UI_DPI_FAC;
    uiblock_layer_pass_buttons(layout, ima, ima->rr, iuser, 230 * dpi_fac, NULL);
  }

  if (BKE_image_is_animated(ima)) {
    /* Animation */
    uiItemS(layout);

    uiLayout *col = uiLayoutColumn(layout, true);
    uiLayoutSetPropSep(col, true);

    uiLayout *sub = uiLayoutColumn(col, true);
    uiLayout *row = uiLayoutRow(sub, true);
    uiItemR(row, userptr, "frame_duration", 0, IFACE_("Frames"), ICON_NONE);
    uiItemO(row, "", ICON_FILE_REFRESH, "IMAGE_OT_match_movie_length");

    uiItemR(sub, userptr, "frame_start", 0, IFACE_("Start"), ICON_NONE);
    uiItemR(sub, userptr, "frame_offset", 0, NULL, ICON_NONE);

    uiItemR(col, userptr, "use_cyclic", 0, NULL, ICON_NONE);
    uiItemR(col, userptr, "use_auto_refresh", 0, NULL, ICON_NONE);

    if (ima->source == IMA_SRC_MOVIE && compact == 0) {
      uiItemR(col, &imaptr, "use_deinterlace", 0, IFACE_("Deinterlace"), ICON_NONE);
    }
  }

  /* Multiview */
  if (multiview && compact == 0) {
    if ((scene->r.scemode & R_MULTIVIEW) != 0) {
      uiItemS(layout);

      uiLayout *col = uiLayoutColumn(layout, false);
      uiLayoutSetPropSep(col, true);
      uiItemR(col, &imaptr, "use_multiview", 0, NULL, ICON_NONE);

      if (RNA_boolean_get(&imaptr, "use_multiview")) {
        uiTemplateImageViews(layout, &imaptr);
      }
    }
  }

  /* Colorspace and alpha */
  {
    uiItemS(layout);

    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetPropSep(col, true);
    uiTemplateColorspaceSettings(col, &imaptr, "colorspace_settings");

    if (compact == 0) {
      if (ima->source != IMA_SRC_GENERATED) {
        if (image_has_alpha(ima, iuser)) {
          uiLayout *sub = uiLayoutColumn(col, false);
          uiItemR(sub, &imaptr, "alpha_mode", 0, IFACE_("Alpha"), ICON_NONE);

          bool is_data = IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name);
          uiLayoutSetActive(sub, !is_data);
        }
      }

      uiItemR(col, &imaptr, "use_view_as_render", 0, NULL, ICON_NONE);
    }
  }

  UI_block_funcN_set(block, NULL, NULL, NULL);
}

void uiTemplateImageSettings(uiLayout *layout, PointerRNA *imfptr, bool color_management)
{
  ImageFormatData *imf = imfptr->data;
  ID *id = imfptr->id.data;
  PointerRNA display_settings_ptr;
  PropertyRNA *prop;
  const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
  /* some settings depend on this being a scene that's rendered */
  const bool is_render_out = (id && GS(id->name) == ID_SCE);

  uiLayout *col;
  bool show_preview = false;

  col = uiLayoutColumn(layout, false);

  uiLayoutSetPropSep(col, true);
  uiLayoutSetPropDecorate(col, false);

  uiItemR(col, imfptr, "file_format", 0, NULL, ICON_NONE);
  uiItemR(
      uiLayoutRow(col, true), imfptr, "color_mode", UI_ITEM_R_EXPAND, IFACE_("Color"), ICON_NONE);

  /* only display depth setting if multiple depths can be used */
  if ((ELEM(depth_ok,
            R_IMF_CHAN_DEPTH_1,
            R_IMF_CHAN_DEPTH_8,
            R_IMF_CHAN_DEPTH_10,
            R_IMF_CHAN_DEPTH_12,
            R_IMF_CHAN_DEPTH_16,
            R_IMF_CHAN_DEPTH_24,
            R_IMF_CHAN_DEPTH_32)) == 0) {
    uiItemR(uiLayoutRow(col, true), imfptr, "color_depth", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  }

  if (BKE_imtype_supports_quality(imf->imtype)) {
    uiItemR(col, imfptr, "quality", 0, NULL, ICON_NONE);
  }

  if (BKE_imtype_supports_compress(imf->imtype)) {
    uiItemR(col, imfptr, "compression", 0, NULL, ICON_NONE);
  }

  if (ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    uiItemR(col, imfptr, "exr_codec", 0, NULL, ICON_NONE);
  }

  if (BKE_imtype_supports_zbuf(imf->imtype)) {
    uiItemR(col, imfptr, "use_zbuffer", 0, NULL, ICON_NONE);
  }

  if (is_render_out && ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    show_preview = true;
    uiItemR(col, imfptr, "use_preview", 0, NULL, ICON_NONE);
  }

  if (imf->imtype == R_IMF_IMTYPE_JP2) {
    uiItemR(col, imfptr, "jpeg2k_codec", 0, NULL, ICON_NONE);

    uiItemR(col, imfptr, "use_jpeg2k_cinema_preset", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "use_jpeg2k_cinema_48", 0, NULL, ICON_NONE);

    uiItemR(col, imfptr, "use_jpeg2k_ycc", 0, NULL, ICON_NONE);
  }

  if (imf->imtype == R_IMF_IMTYPE_DPX) {
    uiItemR(col, imfptr, "use_cineon_log", 0, NULL, ICON_NONE);
  }

  if (imf->imtype == R_IMF_IMTYPE_CINEON) {
#if 1
    uiItemL(col, IFACE_("Hard coded Non-Linear, Gamma:1.7"), ICON_NONE);
#else
    uiItemR(col, imfptr, "use_cineon_log", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "cineon_black", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "cineon_white", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "cineon_gamma", 0, NULL, ICON_NONE);
#endif
  }

  if (imf->imtype == R_IMF_IMTYPE_TIFF) {
    uiItemR(col, imfptr, "tiff_codec", 0, NULL, ICON_NONE);
  }

  /* color management */
  if (color_management && (!BKE_imtype_requires_linear_float(imf->imtype) ||
                           (show_preview && imf->flag & R_IMF_FLAG_PREVIEW_JPG))) {
    prop = RNA_struct_find_property(imfptr, "display_settings");
    display_settings_ptr = RNA_property_pointer_get(imfptr, prop);

    col = uiLayoutColumn(layout, false);
    uiItemL(col, IFACE_("Color Management"), ICON_NONE);

    uiItemR(col, &display_settings_ptr, "display_device", 0, NULL, ICON_NONE);

    uiTemplateColormanagedViewSettings(col, NULL, imfptr, "view_settings");
  }
}

void uiTemplateImageStereo3d(uiLayout *layout, PointerRNA *stereo3d_format_ptr)
{
  Stereo3dFormat *stereo3d_format = stereo3d_format_ptr->data;
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, stereo3d_format_ptr, "display_mode", 0, NULL, ICON_NONE);

  switch (stereo3d_format->display_mode) {
    case S3D_DISPLAY_ANAGLYPH: {
      uiItemR(col, stereo3d_format_ptr, "anaglyph_type", 0, NULL, ICON_NONE);
      break;
    }
    case S3D_DISPLAY_INTERLACE: {
      uiItemR(col, stereo3d_format_ptr, "interlace_type", 0, NULL, ICON_NONE);
      uiItemR(col, stereo3d_format_ptr, "use_interlace_swap", 0, NULL, ICON_NONE);
      break;
    }
    case S3D_DISPLAY_SIDEBYSIDE: {
      uiItemR(col, stereo3d_format_ptr, "use_sidebyside_crosseyed", 0, NULL, ICON_NONE);
      ATTR_FALLTHROUGH;
    }
    case S3D_DISPLAY_TOPBOTTOM: {
      uiItemR(col, stereo3d_format_ptr, "use_squeezed_frame", 0, NULL, ICON_NONE);
      break;
    }
  }
}

static void uiTemplateViewsFormat(uiLayout *layout,
                                  PointerRNA *ptr,
                                  PointerRNA *stereo3d_format_ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiLayoutSetPropSep(col, true);
  uiLayoutSetPropDecorate(col, false);

  uiItemR(col, ptr, "views_format", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  if (stereo3d_format_ptr && RNA_enum_get(ptr, "views_format") == R_IMF_VIEWS_STEREO_3D) {
    uiTemplateImageStereo3d(col, stereo3d_format_ptr);
  }
}

void uiTemplateImageViews(uiLayout *layout, PointerRNA *imaptr)
{
  Image *ima = imaptr->data;

  if (ima->type != IMA_TYPE_MULTILAYER) {
    PropertyRNA *prop;
    PointerRNA stereo3d_format_ptr;

    prop = RNA_struct_find_property(imaptr, "stereo_3d_format");
    stereo3d_format_ptr = RNA_property_pointer_get(imaptr, prop);

    uiTemplateViewsFormat(layout, imaptr, &stereo3d_format_ptr);
  }
  else {
    uiTemplateViewsFormat(layout, imaptr, NULL);
  }
}

void uiTemplateImageFormatViews(uiLayout *layout, PointerRNA *imfptr, PointerRNA *ptr)
{
  ImageFormatData *imf = imfptr->data;

  if (ptr != NULL) {
    uiItemR(layout, ptr, "use_multiview", 0, NULL, ICON_NONE);
    if (!RNA_boolean_get(ptr, "use_multiview")) {
      return;
    }
  }

  if (imf->imtype != R_IMF_IMTYPE_MULTILAYER) {
    PropertyRNA *prop;
    PointerRNA stereo3d_format_ptr;

    prop = RNA_struct_find_property(imfptr, "stereo_3d_format");
    stereo3d_format_ptr = RNA_property_pointer_get(imfptr, prop);

    uiTemplateViewsFormat(layout, imfptr, &stereo3d_format_ptr);
  }
  else {
    uiTemplateViewsFormat(layout, imfptr, NULL);
  }
}

void uiTemplateImageLayers(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
  Scene *scene = CTX_data_scene(C);

  /* render layers and passes */
  if (ima && iuser) {
    RenderResult *rr;
    const float dpi_fac = UI_DPI_FAC;
    const int menus_width = 160 * dpi_fac;
    const bool is_render_result = (ima->type == IMA_TYPE_R_RESULT);

    /* use BKE_image_acquire_renderresult  so we get the correct slot in the menu */
    rr = BKE_image_acquire_renderresult(scene, ima);
    uiblock_layer_pass_buttons(
        layout, ima, rr, iuser, menus_width, is_render_result ? &ima->render_slot : NULL);
    BKE_image_release_renderresult(scene, ima);
  }
}

void uiTemplateImageInfo(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
  if (ima == NULL || iuser == NULL) {
    return;
  }

  /* Acquire image buffer. */
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayoutSetAlignment(col, UI_LAYOUT_ALIGN_RIGHT);

  if (ibuf == NULL) {
    uiItemL(col, TIP_("Can't Load Image"), ICON_NONE);
  }
  else {
    char str[MAX_IMAGE_INFO_LEN] = {0};
    const int len = MAX_IMAGE_INFO_LEN;
    int ofs = 0;

    ofs += BLI_snprintf(str + ofs, len - ofs, TIP_("%d x %d, "), ibuf->x, ibuf->y);

    if (ibuf->rect_float) {
      if (ibuf->channels != 4) {
        ofs += BLI_snprintf(str + ofs, len - ofs, TIP_("%d float channel(s)"), ibuf->channels);
      }
      else if (ibuf->planes == R_IMF_PLANES_RGBA) {
        ofs += BLI_strncpy_rlen(str + ofs, TIP_(" RGBA float"), len - ofs);
      }
      else {
        ofs += BLI_strncpy_rlen(str + ofs, TIP_(" RGB float"), len - ofs);
      }
    }
    else {
      if (ibuf->planes == R_IMF_PLANES_RGBA) {
        ofs += BLI_strncpy_rlen(str + ofs, TIP_(" RGBA byte"), len - ofs);
      }
      else {
        ofs += BLI_strncpy_rlen(str + ofs, TIP_(" RGB byte"), len - ofs);
      }
    }
    if (ibuf->zbuf || ibuf->zbuf_float) {
      ofs += BLI_strncpy_rlen(str + ofs, TIP_(" + Z"), len - ofs);
    }

    uiItemL(col, str, ICON_NONE);
  }

  /* Frame number, even if we can't load the image. */
  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    /* don't use iuser->framenr directly because it may not be updated if auto-refresh is off */
    Scene *scene = CTX_data_scene(C);
    const int framenr = BKE_image_user_frame_get(iuser, CFRA, NULL);
    char str[MAX_IMAGE_INFO_LEN];
    int duration = 0;

    if (ima->source == IMA_SRC_MOVIE && BKE_image_has_anim(ima)) {
      struct anim *anim = ((ImageAnim *)ima->anims.first)->anim;
      if (anim) {
        duration = IMB_anim_get_duration(anim, IMB_TC_RECORD_RUN);
      }
    }

    if (duration > 0) {
      /* Movie duration */
      BLI_snprintf(str, MAX_IMAGE_INFO_LEN, TIP_("Frame %d / %d"), framenr, duration);
    }
    else if (ima->source == IMA_SRC_SEQUENCE && ibuf) {
      /* Image sequence frame number + filename */
      const char *filename = BLI_last_slash(ibuf->name);
      filename = (filename == NULL) ? ibuf->name : filename + 1;
      BLI_snprintf(str, MAX_IMAGE_INFO_LEN, TIP_("Frame %d: %s"), framenr, filename);
    }
    else {
      /* Frame number */
      BLI_snprintf(str, MAX_IMAGE_INFO_LEN, TIP_("Frame %d"), framenr);
    }

    uiItemL(col, str, ICON_NONE);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

#undef MAX_IMAGE_INFO_LEN

static bool metadata_panel_context_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceImage *space_image = CTX_wm_space_image(C);
  return space_image != NULL && space_image->image != NULL;
}

static void metadata_panel_context_draw(const bContext *C, Panel *panel)
{
  void *lock;
  SpaceImage *space_image = CTX_wm_space_image(C);
  Image *image = space_image->image;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &space_image->iuser, &lock);
  if (ibuf != NULL) {
    ED_region_image_metadata_panel_draw(ibuf, panel->layout);
  }
  BKE_image_release_ibuf(image, ibuf, lock);
}

void image_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN(sizeof(PanelType), "spacetype image panel metadata");
  strcpy(pt->idname, "IMAGE_PT_metadata");
  strcpy(pt->label, N_("Metadata"));
  strcpy(pt->category, "Image");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->order = 10;
  pt->poll = metadata_panel_context_poll;
  pt->draw = metadata_panel_context_draw;
  pt->flag |= PNL_DEFAULT_CLOSED;
  BLI_addtail(&art->paneltypes, pt);
}
