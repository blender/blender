/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#include <cstdio>
#include <cstring>

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_screen.hh"

#include "RE_pipeline.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "ED_image.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "image_intern.hh"

#define B_NOP -1
#define MAX_IMAGE_INFO_LEN 128

ImageUser *ntree_get_active_iuser(bNodeTree *ntree)
{
  if (ntree) {
    for (bNode *node : ntree->all_nodes()) {
      if (node->type_legacy == CMP_NODE_VIEWER) {
        if (node->flag & NODE_DO_OUTPUT) {
          return static_cast<ImageUser *>(node->storage);
        }
      }
    }
  }
  return nullptr;
}

/* ********************* callbacks for standard image buttons *************** */

static void ui_imageuser_slot_menu(bContext *C, uiLayout *layout, void *image_p)
{
  uiBlock *block = layout->block();
  Image *image = static_cast<Image *>(image_p);

  /* The scene isn't expected to be null, check since it's not a requirement
   * for the value to be non-null for this function to work.
   * It's OK if `has_active_render` is false. */
  Scene *scene = CTX_data_scene(C);
  bool has_active_render = scene && (RE_GetSceneRender(scene) != nullptr);

  int slot_id;
  LISTBASE_FOREACH_INDEX (RenderSlot *, slot, &image->renderslots, slot_id) {
    char str[64];
    if (slot->name[0] != '\0') {
      STRNCPY_UTF8(str, slot->name);
    }
    else {
      SNPRINTF_UTF8(str, IFACE_("Slot %d"), slot_id + 1);
    }
    /* Default to "blank" for nicer alignment. */
    int icon = ICON_BLANK1;
    if (slot_id == image->last_render_slot) {
      if (has_active_render) {
        icon = ICON_RENDER_RESULT;
      }
    }
    else if (slot->render != nullptr) {
      icon = ICON_DOT;
    }
    uiBut *but = uiDefIconTextBut(
        block, ButType::ButMenu, B_NOP, icon, str, 0, 0, UI_UNIT_X * 5, UI_UNIT_X, nullptr, "");
    UI_but_func_set(but, [image, slot_id](bContext & /*C*/) { image->render_slot = slot_id; });
  }

  layout->separator();
  uiDefBut(block,
           ButType::Label,
           0,
           IFACE_("Slot"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           "");
}

static bool ui_imageuser_slot_menu_step(bContext *C, int direction, void *image_p)
{
  Image *image = static_cast<Image *>(image_p);

  if (ED_image_slot_cycle(image, direction)) {
    WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);
    return true;
  }
  return true;
}

static const char *ui_imageuser_layer_fake_name(RenderResult *rr)
{
  RenderView *rv = RE_RenderViewGetById(rr, 0);
  ImBuf *ibuf = rv->ibuf;
  if (!ibuf) {
    return nullptr;
  }
  if (ibuf->float_buffer.data) {
    return IFACE_("Composite");
  }
  if (ibuf->byte_buffer.data) {
    return IFACE_("Sequence");
  }
  return nullptr;
}

/* workaround for passing many args */
struct ImageUI_Data {
  Image *image;
  ImageUser *iuser;
  int rpass_index;
};

static ImageUI_Data *ui_imageuser_data_copy(const ImageUI_Data *rnd_pt_src)
{
  ImageUI_Data *rnd_pt_dst = static_cast<ImageUI_Data *>(
      MEM_mallocN(sizeof(*rnd_pt_src), __func__));
  memcpy(rnd_pt_dst, rnd_pt_src, sizeof(*rnd_pt_src));
  return rnd_pt_dst;
}

static void ui_imageuser_layer_menu(bContext * /*C*/, uiLayout *layout, void *rnd_pt)
{
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  uiBlock *block = layout->block();
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  Scene *scene = iuser->scene;

  /* May have been freed since drawing. */
  RenderResult *rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == nullptr)) {
    BKE_image_release_renderresult(scene, image, rr);
    return;
  }

  blender::ui::block_layout_set_current(block, layout);
  layout->column(false);

  const char *fake_name = ui_imageuser_layer_fake_name(rr);
  if (fake_name) {
    uiDefButS(block,
              ButType::ButMenu,
              B_NOP,
              fake_name,
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->layer,
              0.0,
              0.0,
              "");
  }

  int nr = fake_name ? 1 : 0;
  for (RenderLayer *rl = static_cast<RenderLayer *>(rr->layers.first); rl; rl = rl->next, nr++) {
    uiDefButS(block,
              ButType::ButMenu,
              B_NOP,
              rl->name,
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->layer,
              float(nr),
              0.0,
              "");
  }

  layout->separator();
  uiDefBut(block,
           ButType::Label,
           0,
           IFACE_("Layer"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           "");

  BKE_image_release_renderresult(scene, image, rr);
}

static void ui_imageuser_pass_menu(bContext * /*C*/, uiLayout *layout, void *rnd_pt)
{
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  uiBlock *block = layout->block();
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
  if (UNLIKELY(rr == nullptr)) {
    BKE_image_release_renderresult(scene, image, rr);
    return;
  }

  rl = static_cast<RenderLayer *>(BLI_findlink(&rr->layers, rpass_index));

  blender::ui::block_layout_set_current(block, layout);
  layout->column(false);

  nr = (rl == nullptr) ? 1 : 0;

  ListBase added_passes;
  BLI_listbase_clear(&added_passes);

  /* rendered results don't have a Combined pass */
  /* multiview: the ordering must be ascending, so the left-most pass is always the one picked */
  for (rpass = static_cast<RenderPass *>(rl ? rl->passes.first : nullptr); rpass;
       rpass = rpass->next, nr++)
  {
    /* just show one pass of each kind */
    if (BLI_findstring_ptr(&added_passes, rpass->name, offsetof(LinkData, data))) {
      continue;
    }
    BLI_addtail(&added_passes, BLI_genericNodeN(rpass->name));

    uiDefButS(block,
              ButType::ButMenu,
              B_NOP,
              IFACE_(rpass->name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->pass,
              float(nr),
              0.0,
              "");
  }

  layout->separator();
  uiDefBut(block,
           ButType::Label,
           0,
           IFACE_("Pass"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           "");

  BLI_freelistN(&added_passes);

  BKE_image_release_renderresult(scene, image, rr);
}

/**************************** view menus *****************************/
static void ui_imageuser_view_menu_rr(bContext * /*C*/, uiLayout *layout, void *rnd_pt)
{
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  uiBlock *block = layout->block();
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  RenderResult *rr;
  RenderView *rview;
  int nr;
  Scene *scene = iuser->scene;

  /* may have been freed since drawing */
  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == nullptr)) {
    BKE_image_release_renderresult(scene, image, rr);
    return;
  }

  blender::ui::block_layout_set_current(block, layout);
  layout->column(false);

  uiDefBut(block,
           ButType::Label,
           0,
           IFACE_("View"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           "");

  layout->separator();

  nr = (rr ? BLI_listbase_count(&rr->views) : 0) - 1;
  for (rview = static_cast<RenderView *>(rr ? rr->views.last : nullptr); rview;
       rview = rview->prev, nr--)
  {
    uiDefButS(block,
              ButType::ButMenu,
              B_NOP,
              IFACE_(rview->name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->view,
              float(nr),
              0.0,
              "");
  }

  BKE_image_release_renderresult(scene, image, rr);
}

static void ui_imageuser_view_menu_multiview(bContext * /*C*/, uiLayout *layout, void *rnd_pt)
{
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  uiBlock *block = layout->block();
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  int nr;
  ImageView *iv;

  blender::ui::block_layout_set_current(block, layout);
  layout->column(false);

  uiDefBut(block,
           ButType::Label,
           0,
           IFACE_("View"),
           0,
           0,
           UI_UNIT_X * 5,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           "");

  layout->separator();

  nr = BLI_listbase_count(&image->views) - 1;
  for (iv = static_cast<ImageView *>(image->views.last); iv; iv = iv->prev, nr--) {
    uiDefButS(block,
              ButType::ButMenu,
              B_NOP,
              IFACE_(iv->name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_X,
              &iuser->view,
              float(nr),
              0.0,
              "");
  }
}

/* 5 layer button callbacks... */
static void image_multi_cb(bContext *C, void *rnd_pt, void *rr_v)
{
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  ImageUser *iuser = rnd_data->iuser;

  BKE_image_multilayer_index(static_cast<RenderResult *>(rr_v), iuser);
  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);
}

static bool ui_imageuser_layer_menu_step(bContext *C, int direction, void *rnd_pt)
{
  Scene *scene = CTX_data_scene(C);
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  RenderResult *rr;
  bool changed = false;

  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == nullptr)) {
    BKE_image_release_renderresult(scene, image, rr);
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

  BKE_image_release_renderresult(scene, image, rr);

  if (changed) {
    BKE_image_multilayer_index(rr, iuser);
    WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);
  }

  return changed;
}

static bool ui_imageuser_pass_menu_step(bContext *C, int direction, void *rnd_pt)
{
  Scene *scene = CTX_data_scene(C);
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  Image *image = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;
  RenderResult *rr;
  bool changed = false;
  int layer = iuser->layer;
  RenderLayer *rl;
  RenderPass *rpass;

  rr = BKE_image_acquire_renderresult(scene, image);
  if (UNLIKELY(rr == nullptr)) {
    BKE_image_release_renderresult(scene, image, rr);
    return false;
  }

  if (RE_HasCombinedLayer(rr)) {
    layer -= 1;
  }

  rl = static_cast<RenderLayer *>(BLI_findlink(&rr->layers, layer));
  if (rl == nullptr) {
    BKE_image_release_renderresult(scene, image, rr);
    return false;
  }

  rpass = static_cast<RenderPass *>(BLI_findlink(&rl->passes, iuser->pass));
  if (rpass == nullptr) {
    BKE_image_release_renderresult(scene, image, rr);
    return false;
  }

  if (direction == 1) {
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
  else if (direction == -1) {
    RenderPass *rp;
    int rp_index = 0;

    if (iuser->pass == 0) {
      BKE_image_release_renderresult(scene, image, rr);
      return false;
    }

    for (rp = static_cast<RenderPass *>(rl->passes.first); rp; rp = rp->next, rp_index++) {
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

  BKE_image_release_renderresult(scene, image, rr);

  if (changed) {
    BKE_image_multilayer_index(rr, iuser);
    WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);
  }

  return changed;
}

/* 5 view button callbacks... */
static void image_multiview_cb(bContext *C, void *rnd_pt, void * /*arg_v*/)
{
  ImageUI_Data *rnd_data = static_cast<ImageUI_Data *>(rnd_pt);
  Image *ima = rnd_data->image;
  ImageUser *iuser = rnd_data->iuser;

  BKE_image_multiview_index(ima, iuser);
  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);
}

static void uiblock_layer_pass_buttons(uiLayout *layout,
                                       Image *image,
                                       RenderResult *rr,
                                       ImageUser *iuser,
                                       int w,
                                       const short *render_slot)
{
  ImageUI_Data rnd_pt_local, *rnd_pt = nullptr;
  uiBlock *block = layout->block();
  uiBut *but;
  RenderLayer *rl = nullptr;
  int wmenu1, wmenu2, wmenu3, wmenu4;
  const char *fake_name;
  const char *display_name = "";
  const bool show_stereo = (iuser->flag & IMA_SHOW_STEREO) != 0;

  if (iuser->scene == nullptr) {
    return;
  }

  layout->row(true);

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
    RenderSlot *slot = BKE_image_get_renderslot(image, *render_slot);
    char str[sizeof(slot->name)];
    if (slot && slot->name[0] != '\0') {
      STRNCPY_UTF8(str, slot->name);
    }
    else {
      SNPRINTF_UTF8(str, IFACE_("Slot %d"), *render_slot + 1);
    }

    rnd_pt = ui_imageuser_data_copy(&rnd_pt_local);
    but = uiDefMenuBut(
        block, ui_imageuser_slot_menu, image, str, 0, 0, wmenu1, UI_UNIT_Y, TIP_("Select Slot"));
    UI_but_func_menu_step_set(but, ui_imageuser_slot_menu_step);
    UI_but_funcN_set(but, image_multi_cb, rnd_pt, rr);
    UI_but_type_set_menu_from_pulldown(but);
    rnd_pt = nullptr;
  }

  if (rr) {
    RenderPass *rpass;
    RenderView *rview;
    int rpass_index;

    /* layer */
    fake_name = ui_imageuser_layer_fake_name(rr);
    rpass_index = iuser->layer - (fake_name ? 1 : 0);
    rl = static_cast<RenderLayer *>(BLI_findlink(&rr->layers, rpass_index));
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
      rnd_pt = nullptr;
    }

    /* pass */
    rpass = static_cast<RenderPass *>(rl ? BLI_findlink(&rl->passes, iuser->pass) : nullptr);

    if (rl && RE_passes_have_name(rl)) {
      display_name = rpass ? rpass->name : "";
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
      rnd_pt = nullptr;
    }

    /* view */
    if (BLI_listbase_count_at_most(&rr->views, 2) > 1 &&
        ((!show_stereo) || !RE_RenderResult_is_stereo(rr)))
    {
      rview = static_cast<RenderView *>(BLI_findlink(&rr->views, iuser->view));
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
      rnd_pt = nullptr;
    }
  }

  /* stereo image */
  else if ((BKE_image_is_stereo(image) && (!show_stereo)) ||
           (BKE_image_is_multiview(image) && !BKE_image_is_stereo(image)))
  {
    int nr = 0;

    LISTBASE_FOREACH (ImageView *, iv, &image->views) {
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
    UI_but_funcN_set(but, image_multiview_cb, rnd_pt, nullptr);
    UI_but_type_set_menu_from_pulldown(but);
    rnd_pt = nullptr;
  }
}

namespace {

struct RNAUpdateCb {
  PointerRNA ptr = {};
  PropertyRNA *prop;
  ImageUser *iuser;
};

};  // namespace

static void rna_update_cb(bContext *C, void *arg_cb, void * /*arg*/)
{
  RNAUpdateCb *cb = (RNAUpdateCb *)arg_cb;

  /* we call update here on the pointer property, this way the
   * owner of the image pointer can still define its own update
   * and notifier */
  RNA_property_update(C, &cb->ptr, cb->prop);
}

void uiTemplateImage(uiLayout *layout,
                     bContext *C,
                     PointerRNA *ptr,
                     const blender::StringRefNull propname,
                     PointerRNA *userptr,
                     bool compact,
                     bool multiview)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  uiBlock *block = layout->block();

  PointerRNA imaptr = RNA_property_pointer_get(ptr, prop);
  Image *ima = static_cast<Image *>(imaptr.data);
  ImageUser *iuser = static_cast<ImageUser *>(userptr->data);

  Scene *scene = CTX_data_scene(C);
  BKE_image_user_frame_calc(ima, iuser, scene->r.cfra);

  layout->context_ptr_set("edit_image", &imaptr);
  layout->context_ptr_set("edit_image_user", userptr);

  SpaceImage *space_image = CTX_wm_space_image(C);
  if (!compact && (space_image == nullptr || iuser != &space_image->iuser)) {
    uiTemplateID(
        layout, C, ptr, propname, ima ? nullptr : "IMAGE_OT_new", "IMAGE_OT_open", nullptr);

    if (ima != nullptr) {
      layout->separator();
    }
  }

  if (ima == nullptr) {
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
      const float dpi_fac = UI_SCALE_FAC;
      const int menus_width = 230 * dpi_fac;

      /* Use #BKE_image_acquire_renderresult so we get the correct slot in the menu. */
      rr = BKE_image_acquire_renderresult(scene, ima);
      uiblock_layer_pass_buttons(layout, ima, rr, iuser, menus_width, &ima->render_slot);
      BKE_image_release_renderresult(scene, ima, rr);
    }

    return;
  }

  /* Set custom callback for property updates. */
  RNAUpdateCb *cb = MEM_new<RNAUpdateCb>(__func__);
  cb->ptr = *ptr;
  cb->prop = prop;
  cb->iuser = iuser;
  UI_block_funcN_set(block,
                     rna_update_cb,
                     cb,
                     nullptr,
                     but_func_argN_free<RNAUpdateCb>,
                     but_func_argN_copy<RNAUpdateCb>);

  /* Disable editing if image was modified, to avoid losing changes. */
  const bool is_dirty = BKE_image_is_dirty(ima);
  if (is_dirty) {
    uiLayout *row = &layout->row(true);
    row->op("image.save", IFACE_("Save"), ICON_NONE);
    row->op("image.reload", IFACE_("Discard"), ICON_NONE);
    layout->separator();
  }

  layout = &layout->column(false);
  layout->enabled_set(!is_dirty);
  layout->use_property_decorate_set(false);

  /* Image source */
  {
    uiLayout *col = &layout->column(false);
    col->use_property_split_set(true);
    col->prop(&imaptr, "source", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  /* Filepath */
  const bool is_packed = BKE_image_has_packedfile(ima);
  const bool no_filepath = is_packed && !BKE_image_has_filepath(ima);

  if ((ima->source != IMA_SRC_GENERATED) && !no_filepath) {
    layout->separator();

    uiLayout *row = &layout->row(true);
    if (is_packed) {
      row->op("image.unpack", "", ICON_PACKAGE);
    }
    else {
      row->op("image.pack", "", ICON_UGLYPACKAGE);
    }

    row = &row->row(true);
    row->enabled_set(is_packed == false);

    prop = RNA_struct_find_property(&imaptr, "filepath");
    uiDefAutoButR(block, &imaptr, prop, -1, "", ICON_NONE, 0, 0, 200, UI_UNIT_Y);
    row->op("image.file_browse", "", ICON_FILEBROWSER);
    row->op("image.reload", "", ICON_FILE_REFRESH);
  }

  /* Image layers and Info */
  if (ima->source == IMA_SRC_GENERATED) {
    layout->separator();

    /* Generated */
    uiLayout *col = &layout->column(false);
    col->use_property_split_set(true);

    uiLayout *sub = &col->column(true);
    sub->prop(&imaptr, "generated_width", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
    sub->prop(&imaptr, "generated_height", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

    col->prop(&imaptr, "use_generated_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->separator();

    col->prop(&imaptr, "generated_type", UI_ITEM_R_EXPAND, IFACE_("Type"), ICON_NONE);
    ImageTile *base_tile = BKE_image_get_tile(ima, 0);
    if (base_tile->gen_type == IMA_GENTYPE_BLANK) {
      col->prop(&imaptr, "generated_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
  else if (compact == 0) {
    uiTemplateImageInfo(layout, C, ima, iuser);
  }
  if (ima->type == IMA_TYPE_MULTILAYER && ima->rr) {
    layout->separator();

    const float dpi_fac = UI_SCALE_FAC;
    uiblock_layer_pass_buttons(layout, ima, ima->rr, iuser, 230 * dpi_fac, nullptr);
  }

  if (BKE_image_is_animated(ima)) {
    /* Animation */
    layout->separator();

    uiLayout *col = &layout->column(true);
    col->use_property_split_set(true);

    uiLayout *sub = &col->column(true);
    uiLayout *row = &sub->row(true);
    row->prop(userptr, "frame_duration", UI_ITEM_NONE, IFACE_("Frames"), ICON_NONE);
    row->op("IMAGE_OT_match_movie_length", "", ICON_FILE_REFRESH);

    sub->prop(userptr, "frame_start", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
    sub->prop(userptr, "frame_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->prop(userptr, "use_cyclic", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(userptr, "use_auto_refresh", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    if (ima->source == IMA_SRC_MOVIE && compact == 0) {
      col->prop(&imaptr, "use_deinterlace", UI_ITEM_NONE, IFACE_("Deinterlace"), ICON_NONE);
    }
  }

  /* Multiview */
  if (multiview && compact == 0) {
    if ((scene->r.scemode & R_MULTIVIEW) != 0) {
      layout->separator();

      uiLayout *col = &layout->column(false);
      col->use_property_split_set(true);
      col->prop(&imaptr, "use_multiview", UI_ITEM_NONE, std::nullopt, ICON_NONE);

      if (RNA_boolean_get(&imaptr, "use_multiview")) {
        uiTemplateImageViews(layout, &imaptr);
      }
    }
  }

  /* Color-space and alpha. */
  {
    layout->separator();

    uiLayout *col = &layout->column(false);
    col->use_property_split_set(true);
    uiTemplateColorspaceSettings(col, &imaptr, "colorspace_settings");

    if (compact == 0) {
      if (ima->source != IMA_SRC_GENERATED) {
        if (BKE_image_has_alpha(ima)) {
          uiLayout *sub = &col->column(false);
          sub->prop(&imaptr, "alpha_mode", UI_ITEM_NONE, IFACE_("Alpha"), ICON_NONE);

          bool is_data = IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name);
          sub->active_set(!is_data);
        }

        if (ima && iuser) {
          void *lock;
          ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);

          if (ibuf && ibuf->float_buffer.data && (ibuf->foptions.flag & OPENEXR_HALF) == 0) {
            col->prop(&imaptr, "use_half_precision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          }
          BKE_image_release_ibuf(ima, ibuf, lock);
        }
      }

      col->prop(&imaptr, "use_view_as_render", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(&imaptr, "seam_margin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }

  UI_block_funcN_set(block, nullptr, nullptr, nullptr);
}

void uiTemplateImageSettings(uiLayout *layout,
                             bContext *C,
                             PointerRNA *imfptr,
                             bool color_management,
                             const char *panel_idname)
{
  ImageFormatData *imf = static_cast<ImageFormatData *>(imfptr->data);
  ID *id = imfptr->owner_id;
  /* Note: this excludes any video formats; for them the image template does
   * not show the color depth. Color depth instead is shown as part of encoding UI block,
   * which is less confusing. */
  const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
  /* some settings depend on this being a scene that's rendered */
  const bool is_render_out = (id && GS(id->name) == ID_SCE);

  uiLayout *col;

  col = &layout->column(false);

  col->use_property_split_set(true);
  col->use_property_decorate_set(false);

  /* The file output node draws the media type itself. */
  const bool is_file_output = (id && GS(id->name) == ID_NT);
  if (!is_file_output) {
    col->prop(imfptr, "media_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  /* Multi layer images and video media types only have a single supported format,
   * so we needn't draw the format enum. */
  if (imf->media_type == MEDIA_TYPE_IMAGE) {
    col->prop(imfptr, "file_format", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  /* Multi-layer always saves raw unmodified channels. */
  if (imf->imtype != R_IMF_IMTYPE_MULTILAYER) {
    col->row(true).prop(imfptr, "color_mode", UI_ITEM_R_EXPAND, IFACE_("Color"), ICON_NONE);
  }

  /* only display depth setting if multiple depths can be used */
  if (ELEM(depth_ok,
           R_IMF_CHAN_DEPTH_1,
           R_IMF_CHAN_DEPTH_8,
           R_IMF_CHAN_DEPTH_10,
           R_IMF_CHAN_DEPTH_12,
           R_IMF_CHAN_DEPTH_16,
           R_IMF_CHAN_DEPTH_24,
           R_IMF_CHAN_DEPTH_32) == 0)
  {
    col->row(true).prop(imfptr, "color_depth", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }

  if (BKE_imtype_supports_quality(imf->imtype)) {
    col->prop(imfptr, "quality", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (BKE_imtype_supports_compress(imf->imtype)) {
    col->prop(imfptr, "compression", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    col->prop(imfptr, "exr_codec", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (ELEM(imf->exr_codec & OPENEXR_CODEC_MASK, R_IMF_EXR_CODEC_DWAA, R_IMF_EXR_CODEC_DWAB)) {
      col->prop(imfptr, "quality", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
  if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
    col->prop(imfptr, "use_exr_interleave", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (is_render_out && ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    col->prop(imfptr, "use_preview", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (imf->imtype == R_IMF_IMTYPE_JP2) {
    col->prop(imfptr, "jpeg2k_codec", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->prop(imfptr, "use_jpeg2k_cinema_preset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(imfptr, "use_jpeg2k_cinema_48", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->prop(imfptr, "use_jpeg2k_ycc", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (imf->imtype == R_IMF_IMTYPE_DPX) {
    col->prop(imfptr, "use_cineon_log", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (imf->imtype == R_IMF_IMTYPE_CINEON) {
#if 1
    col->label(RPT_("Hard coded Non-Linear, Gamma:1.7"), ICON_NONE);
#else
    col->prop(imfptr, "use_cineon_log", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(imfptr, "cineon_black", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(imfptr, "cineon_white", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(imfptr, "cineon_gamma", UI_ITEM_NONE, std::nullopt, ICON_NONE);
#endif
  }

  if (imf->imtype == R_IMF_IMTYPE_TIFF) {
    col->prop(imfptr, "tiff_codec", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  /* Override color management */
  if (color_management) {
    if (uiLayout *panel = col->panel(C,
                                     panel_idname ? panel_idname : "settings_color_management",
                                     true,
                                     IFACE_("Color Management")))
    {
      panel->separator();
      panel->row(true).prop(imfptr, "color_management", UI_ITEM_R_EXPAND, " ", ICON_NONE);

      uiLayout *color_settings = &panel->column(true);
      if (BKE_imtype_requires_linear_float(imf->imtype)) {
        if (imf->color_management == R_IMF_COLOR_MANAGEMENT_OVERRIDE) {
          PointerRNA linear_settings_ptr = RNA_pointer_get(imfptr, "linear_colorspace_settings");
          color_settings->prop(
              &linear_settings_ptr, "name", UI_ITEM_NONE, IFACE_("Color Space"), ICON_NONE);
        }
      }
      else {
        PointerRNA display_settings_ptr = RNA_pointer_get(imfptr, "display_settings");
        color_settings->prop(
            &display_settings_ptr, "display_device", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        uiTemplateColormanagedViewSettings(color_settings, nullptr, imfptr, "view_settings");
        color_settings->enabled_set(imf->color_management == R_IMF_COLOR_MANAGEMENT_OVERRIDE);
      }
    }
  }
}

void uiTemplateImageStereo3d(uiLayout *layout, PointerRNA *stereo3d_format_ptr)
{
  Stereo3dFormat *stereo3d_format = static_cast<Stereo3dFormat *>(stereo3d_format_ptr->data);
  uiLayout *col;

  col = &layout->column(false);
  col->prop(stereo3d_format_ptr, "display_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  switch (stereo3d_format->display_mode) {
    case S3D_DISPLAY_ANAGLYPH: {
      col->prop(stereo3d_format_ptr, "anaglyph_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    }
    case S3D_DISPLAY_INTERLACE: {
      col->prop(stereo3d_format_ptr, "interlace_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(stereo3d_format_ptr, "use_interlace_swap", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    }
    case S3D_DISPLAY_SIDEBYSIDE: {
      col->prop(
          stereo3d_format_ptr, "use_sidebyside_crosseyed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      ATTR_FALLTHROUGH;
    }
    case S3D_DISPLAY_TOPBOTTOM: {
      col->prop(stereo3d_format_ptr, "use_squeezed_frame", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    }
  }
}

static void uiTemplateViewsFormat(uiLayout *layout,
                                  PointerRNA *ptr,
                                  PointerRNA *stereo3d_format_ptr)
{
  uiLayout *col;

  col = &layout->column(false);

  col->use_property_split_set(true);
  col->use_property_decorate_set(false);

  col->prop(ptr, "views_format", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  if (stereo3d_format_ptr && RNA_enum_get(ptr, "views_format") == R_IMF_VIEWS_STEREO_3D) {
    uiTemplateImageStereo3d(col, stereo3d_format_ptr);
  }
}

void uiTemplateImageViews(uiLayout *layout, PointerRNA *imaptr)
{
  Image *ima = static_cast<Image *>(imaptr->data);

  if (ima->type != IMA_TYPE_MULTILAYER) {
    PropertyRNA *prop;
    PointerRNA stereo3d_format_ptr;

    prop = RNA_struct_find_property(imaptr, "stereo_3d_format");
    stereo3d_format_ptr = RNA_property_pointer_get(imaptr, prop);

    uiTemplateViewsFormat(layout, imaptr, &stereo3d_format_ptr);
  }
  else {
    uiTemplateViewsFormat(layout, imaptr, nullptr);
  }
}

void uiTemplateImageFormatViews(uiLayout *layout, PointerRNA *imfptr, PointerRNA *ptr)
{
  ImageFormatData *imf = static_cast<ImageFormatData *>(imfptr->data);

  if (ptr != nullptr) {
    layout->prop(ptr, "use_multiview", UI_ITEM_NONE, std::nullopt, ICON_NONE);
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
    uiTemplateViewsFormat(layout, imfptr, nullptr);
  }
}

void uiTemplateImageLayers(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
  Scene *scene = CTX_data_scene(C);

  /* render layers and passes */
  if (ima && iuser) {
    RenderResult *rr;
    const float dpi_fac = UI_SCALE_FAC;
    const int menus_width = 160 * dpi_fac;
    const bool is_render_result = (ima->type == IMA_TYPE_R_RESULT);

    /* Use BKE_image_acquire_renderresult so we get the correct slot in the menu. */
    rr = BKE_image_acquire_renderresult(scene, ima);
    uiblock_layer_pass_buttons(
        layout, ima, rr, iuser, menus_width, is_render_result ? &ima->render_slot : nullptr);
    BKE_image_release_renderresult(scene, ima, rr);
  }
}

void uiTemplateImageInfo(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
  if (ima == nullptr || iuser == nullptr) {
    return;
  }

  /* Acquire image buffer. */
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);

  uiLayout *col = &layout->column(true);
  col->alignment_set(blender::ui::LayoutAlign::Right);

  if (ibuf == nullptr) {
    col->label(RPT_("Cannot Load Image"), ICON_NONE);
  }
  else {
    char str[MAX_IMAGE_INFO_LEN] = {0};
    const int len = MAX_IMAGE_INFO_LEN;
    int ofs = 0;

    ofs += BLI_snprintf_utf8_rlen(str + ofs, len - ofs, RPT_("%d \u00D7 %d, "), ibuf->x, ibuf->y);

    if (ibuf->float_buffer.data) {
      if (ibuf->channels != 4) {
        ofs += BLI_snprintf_utf8_rlen(
            str + ofs, len - ofs, RPT_("%d float channel(s)"), ibuf->channels);
      }
      else if (ibuf->planes == R_IMF_PLANES_RGBA) {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(" RGBA float"), len - ofs);
      }
      else {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(" RGB float"), len - ofs);
      }
    }
    else {
      if (ibuf->planes == R_IMF_PLANES_RGBA) {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(" RGBA byte"), len - ofs);
      }
      else {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(" RGB byte"), len - ofs);
      }
    }

    blender::gpu::TextureFormat texture_format = blender::gpu::TextureFormat::Invalid;

    /* Try to see if this texture is a compressed format, if not, get the generic format. */
    if (!IMB_gpu_get_compressed_format(ibuf, &texture_format)) {
      texture_format = IMB_gpu_get_texture_format(
          ibuf, ima->flag & IMA_HIGH_BITDEPTH, ibuf->planes >= 8);
    }

    const char *texture_format_description = GPU_texture_format_name(texture_format);
    ofs += BLI_snprintf_utf8_rlen(str + ofs, len - ofs, RPT_(", %s"), texture_format_description);

    col->label(str, ICON_NONE);
  }

  /* Frame number, even if we can't load the image. */
  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    /* don't use iuser->framenr directly because it may not be updated if auto-refresh is off */
    Scene *scene = CTX_data_scene(C);
    const int framenr = BKE_image_user_frame_get(iuser, scene->r.cfra, nullptr);
    char str[MAX_IMAGE_INFO_LEN];
    int duration = 0;

    if (ima->source == IMA_SRC_MOVIE && BKE_image_has_anim(ima)) {
      MovieReader *anim = ((ImageAnim *)ima->anims.first)->anim;
      if (anim) {
        duration = MOV_get_duration_frames(anim, IMB_TC_RECORD_RUN);
      }
    }

    if (duration > 0) {
      /* Movie duration */
      SNPRINTF_UTF8(str, RPT_("Frame %d / %d"), framenr, duration);
    }
    else if (ima->source == IMA_SRC_SEQUENCE && ibuf) {
      /* Image sequence frame number + filename */
      const char *filename = BLI_path_basename(ibuf->filepath);
      SNPRINTF_UTF8(str, RPT_("Frame %d: %s"), framenr, filename);
    }
    else {
      /* Frame number */
      SNPRINTF_UTF8(str, RPT_("Frame %d"), framenr);
    }

    col->label(str, ICON_NONE);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

#undef MAX_IMAGE_INFO_LEN

static bool metadata_panel_context_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceImage *space_image = CTX_wm_space_image(C);
  return space_image != nullptr && space_image->image != nullptr;
}

static void metadata_panel_context_draw(const bContext *C, Panel *panel)
{
  void *lock;
  SpaceImage *space_image = CTX_wm_space_image(C);
  Image *image = space_image->image;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &space_image->iuser, &lock);
  if (ibuf != nullptr) {
    ED_region_image_metadata_panel_draw(ibuf, panel->layout);
  }
  BKE_image_release_ibuf(image, ibuf, lock);
}

void image_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype image panel metadata");
  STRNCPY_UTF8(pt->idname, "IMAGE_PT_metadata");
  STRNCPY_UTF8(pt->label, N_("Metadata"));
  STRNCPY_UTF8(pt->category, "Image");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->order = 10;
  pt->poll = metadata_panel_context_poll;
  pt->draw = metadata_panel_context_draw;
  pt->flag |= PANEL_TYPE_DEFAULT_CLOSED;
  BLI_addtail(&art->paneltypes, pt);
}
