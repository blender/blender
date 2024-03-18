/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_node_tree_update.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

const EnumPropertyItem rna_enum_image_generated_type_items[] = {
    {IMA_GENTYPE_BLANK, "BLANK", 0, "Blank", "Generate a blank image"},
    {IMA_GENTYPE_GRID, "UV_GRID", 0, "UV Grid", "Generated grid to test UV mappings"},
    {IMA_GENTYPE_GRID_COLOR,
     "COLOR_GRID",
     0,
     "Color Grid",
     "Generated improved UV grid to test UV mappings"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem image_source_items[] = {
    {IMA_SRC_FILE, "FILE", 0, "Single Image", "Single image file"},
    {IMA_SRC_SEQUENCE, "SEQUENCE", 0, "Image Sequence", "Multiple image files, as a sequence"},
    {IMA_SRC_MOVIE, "MOVIE", 0, "Movie", "Movie file"},
    {IMA_SRC_GENERATED, "GENERATED", 0, "Generated", "Generated image"},
    {IMA_SRC_VIEWER, "VIEWER", 0, "Viewer", "Compositing node viewer"},
    {IMA_SRC_TILED, "TILED", 0, "UDIM Tiles", "Tiled UDIM image texture"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include "BLI_math_base.h"
#  include "BLI_math_vector.h"

#  include "BKE_global.h"

#  include "GPU_texture.h"

#  include "IMB_imbuf.hh"
#  include "IMB_imbuf_types.hh"

#  include "ED_node.hh"

static bool rna_Image_is_stereo_3d_get(PointerRNA *ptr)
{
  return BKE_image_is_stereo((Image *)ptr->data);
}

static bool rna_Image_is_multiview_get(PointerRNA *ptr)
{
  return BKE_image_is_multiview((Image *)ptr->data);
}

static bool rna_Image_dirty_get(PointerRNA *ptr)
{
  return BKE_image_is_dirty((Image *)ptr->data);
}

static void rna_Image_source_set(PointerRNA *ptr, int value)
{
  Image *ima = (Image *)ptr->owner_id;

  if (value != ima->source) {
    ima->source = value;
    BLI_assert(BKE_id_is_in_global_main(&ima->id));
    BKE_image_signal(G_MAIN, ima, nullptr, IMA_SIGNAL_SRC_CHANGE);
    if (ima->source == IMA_SRC_TILED) {
      BKE_image_signal(G_MAIN, ima, nullptr, IMA_SIGNAL_RELOAD);
    }

    DEG_id_tag_update(&ima->id, 0);
    DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS | ID_RECALC_SOURCE);
    DEG_relations_tag_update(G_MAIN);
  }
}

static void rna_Image_reload_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_RELOAD);
  WM_main_add_notifier(NC_IMAGE | NA_EDITED, &ima->id);
  DEG_id_tag_update(&ima->id, 0);
  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS | ID_RECALC_SOURCE);
}

static int rna_Image_generated_type_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  return base_tile->gen_type;
}

static void rna_Image_generated_type_set(PointerRNA *ptr, int value)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  base_tile->gen_type = value;
}

static int rna_Image_generated_width_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  return base_tile->gen_x;
}

static void rna_Image_generated_width_set(PointerRNA *ptr, int value)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  base_tile->gen_x = std::clamp(value, 1, 65536);
}

static int rna_Image_generated_height_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  return base_tile->gen_y;
}

static void rna_Image_generated_height_set(PointerRNA *ptr, int value)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  base_tile->gen_y = std::clamp(value, 1, 65536);
}

static bool rna_Image_generated_float_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  return (base_tile->gen_flag & IMA_GEN_FLOAT) != 0;
}

static void rna_Image_generated_float_set(PointerRNA *ptr, bool value)
{
  Image *ima = (Image *)ptr->data;
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  if (value) {
    base_tile->gen_flag |= IMA_GEN_FLOAT;
  }
  else {
    base_tile->gen_flag &= ~IMA_GEN_FLOAT;
  }
}

void rna_Image_generated_color_get(PointerRNA *ptr, float values[4])
{
  Image *ima = (Image *)(ptr->data);
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  copy_v4_v4(values, base_tile->gen_color);
}

void rna_Image_generated_color_set(PointerRNA *ptr, const float values[4])
{
  Image *ima = (Image *)(ptr->data);
  ImageTile *base_tile = BKE_image_get_tile(ima, 0);
  for (uint i = 0; i < 4; i++) {
    base_tile->gen_color[i] = std::clamp(values[i], 0.0f, FLT_MAX);
  }
}

static void rna_Image_generated_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_FREE);
  BKE_image_partial_update_mark_full_update(ima);
  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS | ID_RECALC_SOURCE);
}

static void rna_Image_colormanage_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_COLORMANAGE);
  DEG_id_tag_update(&ima->id, 0);
  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS | ID_RECALC_SOURCE);
  WM_main_add_notifier(NC_IMAGE | ND_DISPLAY, &ima->id);
  WM_main_add_notifier(NC_IMAGE | NA_EDITED, &ima->id);
}

static void rna_Image_alpha_mode_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  /* When operating on a generated image, avoid re-generating when changing the alpha-mode
   * as it doesn't impact generated images, causing them to reload pixel data, see #82785. */
  if (ima->source == IMA_SRC_GENERATED) {
    return;
  }
  rna_Image_colormanage_update(bmain, scene, ptr);
}

static void rna_Image_views_format_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  ImBuf *ibuf;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    ImageUser iuser = {nullptr};
    iuser.scene = scene;
    BKE_image_signal(bmain, ima, &iuser, IMA_SIGNAL_FREE);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
  BKE_image_partial_update_mark_full_update(ima);
  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS | ID_RECALC_SOURCE);
}

static void rna_ImageUser_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ImageUser *iuser = static_cast<ImageUser *>(ptr->data);
  ID *id = ptr->owner_id;

  if (scene != nullptr) {
    BKE_image_user_frame_calc(nullptr, iuser, scene->r.cfra);
  }

  if (id) {
    if (GS(id->name) == ID_NT) {
      /* Special update for node-trees. */
      BKE_ntree_update_tag_image_user_changed((bNodeTree *)id, iuser);
      ED_node_tree_propagate_change(nullptr, bmain, nullptr);
    }
    else {
      /* Update material or texture for render preview. */
      DEG_id_tag_update(id, 0);
      DEG_id_tag_update(id, ID_RECALC_EDITORS);
    }
  }
}

static void rna_ImageUser_relations_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_ImageUser_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

static std::optional<std::string> rna_ImageUser_path(const PointerRNA *ptr)
{
  if (ptr->owner_id) {
    // ImageUser *iuser = ptr->data;

    switch (GS(ptr->owner_id->name)) {
      case ID_OB:
      case ID_TE:
        return "image_user";
      case ID_NT:
        return rna_Node_ImageUser_path(ptr);
      case ID_CA:
        return rna_CameraBackgroundImage_image_or_movieclip_user_path(ptr);
      default:
        break;
    }
  }

  return "";
}

static void rna_Image_gpu_texture_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;

  if (!G.background) {
    BKE_image_free_gputextures(ima);
  }

  WM_main_add_notifier(NC_IMAGE | ND_DISPLAY, &ima->id);
}

static const EnumPropertyItem *rna_Image_source_itemf(bContext * /*C*/,
                                                      PointerRNA *ptr,
                                                      PropertyRNA * /*prop*/,
                                                      bool *r_free)
{
  Image *ima = (Image *)ptr->data;
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  if (ima->source == IMA_SRC_VIEWER) {
    RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_VIEWER);
  }
  else {
    RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_FILE);
    RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_SEQUENCE);
    RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_MOVIE);
    RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_GENERATED);
    RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_TILED);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int rna_Image_file_format_get(PointerRNA *ptr)
{
  Image *image = (Image *)ptr->data;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, nullptr, nullptr);
  int imtype = BKE_ftype_to_imtype(ibuf ? ibuf->ftype : IMB_FTYPE_NONE,
                                   ibuf ? &ibuf->foptions : nullptr);

  BKE_image_release_ibuf(image, ibuf, nullptr);

  return imtype;
}

static void rna_Image_file_format_set(PointerRNA *ptr, int value)
{
  Image *image = (Image *)ptr->data;
  if (BKE_imtype_is_movie(value) == 0) { /* should be able to throw an error here */
    ImbFormatOptions options;
    int ftype = BKE_imtype_to_ftype(value, &options);
    BKE_image_file_format_set(image, ftype, &options);
  }
}

static void rna_UDIMTile_size_get(PointerRNA *ptr, int *values)
{
  ImageTile *tile = (ImageTile *)ptr->data;
  Image *image = (Image *)ptr->owner_id;

  ImageUser image_user;
  BKE_imageuser_default(&image_user);
  image_user.tile = tile->tile_number;

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, &lock);
  if (ibuf) {
    values[0] = ibuf->x;
    values[1] = ibuf->y;
  }
  else {
    values[0] = 0;
    values[1] = 0;
  }

  BKE_image_release_ibuf(image, ibuf, lock);
}

static int rna_UDIMTile_channels_get(PointerRNA *ptr)
{
  ImageTile *tile = (ImageTile *)ptr->data;
  Image *image = (Image *)ptr->owner_id;

  ImageUser image_user;
  BKE_imageuser_default(&image_user);
  image_user.tile = tile->tile_number;

  int channels = 0;

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, &lock);
  if (ibuf) {
    channels = ibuf->channels;
  }

  BKE_image_release_ibuf(image, ibuf, lock);

  return channels;
}

static void rna_UDIMTile_label_get(PointerRNA *ptr, char *value)
{
  const ImageTile *tile = (ImageTile *)ptr->data;
  const Image *image = (Image *)ptr->owner_id;

  /* Pass in a fixed size buffer as the value may be allocated based on the callbacks length. */
  char value_buf[sizeof(tile->label)];
  int len = BKE_image_get_tile_label(image, tile, value_buf, sizeof(tile->label));
  memcpy(value, value_buf, len + 1);
}

static int rna_UDIMTile_label_length(PointerRNA *ptr)
{
  const ImageTile *tile = (ImageTile *)ptr->data;
  const Image *image = (Image *)ptr->owner_id;

  char label[sizeof(tile->label)];
  return BKE_image_get_tile_label(image, tile, label, sizeof(label));
}

static void rna_UDIMTile_tile_number_set(PointerRNA *ptr, int value)
{
  ImageTile *tile = (ImageTile *)ptr->data;
  Image *image = (Image *)ptr->owner_id;

  /* Check that no other tile already has that number. */
  ImageTile *cur_tile = BKE_image_get_tile(image, value);
  if (cur_tile == nullptr) {
    BKE_image_reassign_tile(image, tile, value);
  }
}

static void rna_UDIMTile_generated_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  ImageTile *tile = (ImageTile *)ptr->data;

  /* If the tile is still marked as generated, then update the tile as requested. */
  if ((tile->gen_flag & IMA_GEN_TILE) != 0) {
    BKE_image_fill_tile(ima, tile);
    BKE_image_partial_update_mark_full_update(ima);
  }
}

static int rna_Image_active_tile_index_get(PointerRNA *ptr)
{
  Image *image = (Image *)ptr->data;
  return image->active_tile_index;
}

static void rna_Image_active_tile_index_set(PointerRNA *ptr, int value)
{
  Image *image = (Image *)ptr->data;
  int num_tiles = BLI_listbase_count(&image->tiles);

  image->active_tile_index = min_ii(value, num_tiles - 1);
}

static void rna_Image_active_tile_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Image *image = (Image *)ptr->data;
  int num_tiles = BLI_listbase_count(&image->tiles);

  *min = 0;
  *max = max_ii(0, num_tiles - 1);
}

static PointerRNA rna_Image_active_tile_get(PointerRNA *ptr)
{
  Image *image = (Image *)ptr->data;
  ImageTile *tile = static_cast<ImageTile *>(
      BLI_findlink(&image->tiles, image->active_tile_index));

  return rna_pointer_inherit_refine(ptr, &RNA_UDIMTile, tile);
}

static void rna_Image_active_tile_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Image *image = (Image *)ptr->data;
  ImageTile *tile = (ImageTile *)value.data;
  const int index = BLI_findindex(&image->tiles, tile);
  if (index != -1) {
    image->active_tile_index = index;
  }
}

static bool rna_Image_has_data_get(PointerRNA *ptr)
{
  Image *image = (Image *)ptr->data;

  return BKE_image_has_loaded_ibuf(image);
}

static void rna_Image_size_get(PointerRNA *ptr, int *values)
{
  Image *im = (Image *)ptr->data;
  ImBuf *ibuf;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(im, nullptr, &lock);
  if (ibuf) {
    values[0] = ibuf->x;
    values[1] = ibuf->y;
  }
  else {
    values[0] = 0;
    values[1] = 0;
  }

  BKE_image_release_ibuf(im, ibuf, lock);
}

static void rna_Image_resolution_get(PointerRNA *ptr, float *values)
{
  Image *im = (Image *)ptr->data;
  ImBuf *ibuf;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(im, nullptr, &lock);
  if (ibuf) {
    values[0] = ibuf->ppm[0];
    values[1] = ibuf->ppm[1];
  }
  else {
    values[0] = 0;
    values[1] = 0;
  }

  BKE_image_release_ibuf(im, ibuf, lock);
}

static void rna_Image_resolution_set(PointerRNA *ptr, const float *values)
{
  Image *im = (Image *)ptr->data;
  ImBuf *ibuf;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(im, nullptr, &lock);
  if (ibuf) {
    ibuf->ppm[0] = values[0];
    ibuf->ppm[1] = values[1];
  }

  BKE_image_release_ibuf(im, ibuf, lock);
}

static int rna_Image_bindcode_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->data;
  GPUTexture *tex = ima->gputexture[TEXTARGET_2D][0];
  return (tex) ? GPU_texture_opengl_bindcode(tex) : 0;
}

static int rna_Image_depth_get(PointerRNA *ptr)
{
  Image *im = (Image *)ptr->data;
  ImBuf *ibuf;
  void *lock;
  int planes;

  ibuf = BKE_image_acquire_ibuf(im, nullptr, &lock);

  if (!ibuf) {
    planes = 0;
  }
  else if (ibuf->float_buffer.data) {
    planes = ibuf->planes * 4;
  }
  else {
    planes = ibuf->planes;
  }

  BKE_image_release_ibuf(im, ibuf, lock);

  return planes;
}

static int rna_Image_frame_duration_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;
  int duration = 1;

  if (!BKE_image_has_anim(ima)) {
    /* Ensure image has been loaded into memory and frame duration is known. */
    void *lock;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);
    BKE_image_release_ibuf(ima, ibuf, lock);
  }

  if (BKE_image_has_anim(ima)) {
    ImBufAnim *anim = ((ImageAnim *)ima->anims.first)->anim;
    if (anim) {
      duration = IMB_anim_get_duration(anim, IMB_TC_RECORD_RUN);
    }
  }

  return duration;
}

static int rna_Image_pixels_get_length(const PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
  Image *ima = (Image *)ptr->owner_id;
  ImBuf *ibuf;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    length[0] = ibuf->x * ibuf->y * ibuf->channels;
  }
  else {
    length[0] = 0;
  }

  BKE_image_release_ibuf(ima, ibuf, lock);

  return length[0];
}

static void rna_Image_pixels_get(PointerRNA *ptr, float *values)
{
  Image *ima = (Image *)ptr->owner_id;
  ImBuf *ibuf;
  void *lock;
  int i, size;

  ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    size = ibuf->x * ibuf->y * ibuf->channels;

    if (ibuf->float_buffer.data) {
      memcpy(values, ibuf->float_buffer.data, sizeof(float) * size);
    }
    else {
      for (i = 0; i < size; i++) {
        values[i] = ibuf->byte_buffer.data[i] * (1.0f / 255.0f);
      }
    }
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

static void rna_Image_pixels_set(PointerRNA *ptr, const float *values)
{
  Image *ima = (Image *)ptr->owner_id;
  ImBuf *ibuf;
  void *lock;
  int i, size;

  ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    size = ibuf->x * ibuf->y * ibuf->channels;

    if (ibuf->float_buffer.data) {
      memcpy(ibuf->float_buffer.data, values, sizeof(float) * size);
    }
    else {
      for (i = 0; i < size; i++) {
        ibuf->byte_buffer.data[i] = unit_float_to_uchar_clamp(values[i]);
      }
    }

    /* NOTE: Do update from the set() because typically pixels.foreach_set() is used to update
     * the values, and it does not invoke the update(). */

    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID | IB_MIPMAP_INVALID;
    BKE_image_mark_dirty(ima, ibuf);
    if (!G.background) {
      BKE_image_free_gputextures(ima);
    }

    BKE_image_partial_update_mark_full_update(ima);
    WM_main_add_notifier(NC_IMAGE | ND_DISPLAY, &ima->id);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

static int rna_Image_channels_get(PointerRNA *ptr)
{
  Image *im = (Image *)ptr->data;
  ImBuf *ibuf;
  void *lock;
  int channels = 0;

  ibuf = BKE_image_acquire_ibuf(im, nullptr, &lock);
  if (ibuf) {
    channels = ibuf->channels;
  }

  BKE_image_release_ibuf(im, ibuf, lock);

  return channels;
}

static bool rna_Image_is_float_get(PointerRNA *ptr)
{
  Image *im = (Image *)ptr->data;
  ImBuf *ibuf;
  void *lock;
  bool is_float = false;

  ibuf = BKE_image_acquire_ibuf(im, nullptr, &lock);
  if (ibuf) {
    is_float = ibuf->float_buffer.data != nullptr;
  }

  BKE_image_release_ibuf(im, ibuf, lock);

  return is_float;
}

static PointerRNA rna_Image_packed_file_get(PointerRNA *ptr)
{
  Image *ima = (Image *)ptr->owner_id;

  if (BKE_image_has_packedfile(ima)) {
    ImagePackedFile *imapf = static_cast<ImagePackedFile *>(ima->packedfiles.first);
    return rna_pointer_inherit_refine(ptr, &RNA_PackedFile, imapf->packedfile);
  }
  else {
    return PointerRNA_NULL;
  }
}

static void rna_RenderSlot_clear(ID *id, RenderSlot *slot, ImageUser *iuser)
{
  Image *image = (Image *)id;
  int index = BLI_findindex(&image->renderslots, slot);
  BKE_image_clear_renderslot(image, iuser, index);

  WM_main_add_notifier(NC_IMAGE | ND_DISPLAY, image);
}

static PointerRNA rna_render_slots_active_get(PointerRNA *ptr)
{
  Image *image = (Image *)ptr->owner_id;
  RenderSlot *render_slot = BKE_image_get_renderslot(image, image->render_slot);

  return rna_pointer_inherit_refine(ptr, &RNA_RenderSlot, render_slot);
}

static void rna_render_slots_active_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        ReportList * /*reports*/)
{
  Image *image = (Image *)ptr->owner_id;
  if (value.owner_id == &image->id) {
    RenderSlot *slot = (RenderSlot *)value.data;
    int index = BLI_findindex(&image->renderslots, slot);
    if (index != -1) {
      image->render_slot = index;
      BKE_image_partial_update_mark_full_update(image);
    }
  }
}

static int rna_render_slots_active_index_get(PointerRNA *ptr)
{
  Image *image = (Image *)ptr->owner_id;
  return image->render_slot;
}

static void rna_render_slots_active_index_set(PointerRNA *ptr, int value)
{
  Image *image = (Image *)ptr->owner_id;
  int num_slots = BLI_listbase_count(&image->renderslots);
  image->render_slot = value;
  BKE_image_partial_update_mark_full_update(image);
  CLAMP(image->render_slot, 0, num_slots - 1);
}

static void rna_render_slots_active_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Image *image = (Image *)ptr->owner_id;
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&image->renderslots) - 1);
}

static ImageTile *rna_UDIMTile_new(Image *image, int tile_number, const char *label)
{
  ImageTile *tile = BKE_image_add_tile(image, tile_number, label);

  WM_main_add_notifier(NC_IMAGE | ND_DRAW, nullptr);

  return tile;
}

static void rna_UDIMTile_remove(Image *image, PointerRNA *ptr)
{
  ImageTile *tile = (ImageTile *)ptr->data;
  BKE_image_remove_tile(image, tile);

  WM_main_add_notifier(NC_IMAGE | ND_DRAW, nullptr);
}

#else

static void rna_def_imageuser(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ImageUser", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Image User",
      "Parameters defining how an Image data-block is used by another data-block");
  RNA_def_struct_path_func(srna, "rna_ImageUser_path");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_auto_refresh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMA_ANIM_ALWAYS);
  RNA_def_property_ui_text(prop, "Auto Refresh", "Always refresh image on frame changes");
  RNA_def_property_update(prop, 0, "rna_ImageUser_relations_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "frame_current", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "framenr");
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_text(
      prop, "Current Frame", "Current frame number in image sequence or movie");

  /* animation */
  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cycl", 0);
  RNA_def_property_ui_text(prop, "Cyclic", "Cycle the images in the movie");
  RNA_def_property_update(prop, 0, "rna_ImageUser_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "frames");
  RNA_def_property_range(prop, 0, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Frames", "Number of images of a movie to use");
  RNA_def_property_update(prop, 0, "rna_ImageUser_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "frame_offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "offset");
  RNA_def_property_ui_text(
      prop, "Offset", "Offset the number of the frame to use in the animation");
  RNA_def_property_update(prop, 0, "rna_ImageUser_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "sfra");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop,
      "Start Frame",
      "Global starting frame of the movie/sequence, assuming first picture has a #1");
  RNA_def_property_update(prop, 0, "rna_ImageUser_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "multilayer_layer", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "layer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* image_multi_cb */
  RNA_def_property_ui_text(prop, "Layer", "Layer in multilayer image");

  prop = RNA_def_property(srna, "multilayer_pass", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "pass");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* image_multi_cb */
  RNA_def_property_ui_text(prop, "Pass", "Pass in multilayer image");

  prop = RNA_def_property(srna, "multilayer_view", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "view");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* image_multi_cb */
  RNA_def_property_ui_text(prop, "View", "View in multilayer image");

  prop = RNA_def_property(srna, "tile", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "tile");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Tile", "Tile in tiled image");

  RNA_define_lib_overridable(false);
}

/* image.packed_files */
static void rna_def_image_packed_files(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ImagePackedFile", nullptr);
  RNA_def_struct_sdna(srna, "ImagePackedFile");

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "packedfile");
  RNA_def_property_ui_text(prop, "Packed File", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "filepath");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "view", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "view");
  RNA_def_property_ui_text(prop, "View Index", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "tile_number", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "tile_number");
  RNA_def_property_ui_text(prop, "Tile Number", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  RNA_api_image_packed_file(srna);
}

static void rna_def_render_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop, *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "RenderSlot", nullptr);
  RNA_def_struct_ui_text(srna, "Render Slot", "Parameters defining the render slot");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Name", "Render slot name");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  func = RNA_def_function(srna, "clear", "rna_RenderSlot_clear");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Clear the render slot");
  parm = RNA_def_pointer(func, "iuser", "ImageUser", "ImageUser", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_render_slots(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop, *parm;

  RNA_def_property_srna(cprop, "RenderSlots");
  srna = RNA_def_struct(brna, "RenderSlots", nullptr);
  RNA_def_struct_sdna(srna, "Image");
  RNA_def_struct_ui_text(srna, "Render Layers", "Collection of render layers");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "render_slot");
  RNA_def_property_int_funcs(prop,
                             "rna_render_slots_active_index_get",
                             "rna_render_slots_active_index_set",
                             "rna_render_slots_active_index_range");
  RNA_def_property_ui_text(prop, "Active", "Active render slot of the image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RenderSlot");
  RNA_def_property_pointer_funcs(
      prop, "rna_render_slots_active_get", "rna_render_slots_active_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active", "Active render slot of the image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  func = RNA_def_function(srna, "new", "BKE_image_add_renderslot");
  RNA_def_function_ui_description(func, "Add a render slot to the image");
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "New name for the render slot");
  parm = RNA_def_pointer(func, "result", "RenderSlot", "", "Newly created render layer");
  RNA_def_function_return(func, parm);
}

static void rna_def_udim_tile(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "UDIMTile", nullptr);
  RNA_def_struct_sdna(srna, "ImageTile");
  RNA_def_struct_ui_text(srna, "UDIM Tile", "Properties of the UDIM tile");

  prop = RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "label");
  RNA_def_property_ui_text(prop, "Label", "Tile label");
  RNA_def_property_string_funcs(
      prop, "rna_UDIMTile_label_get", "rna_UDIMTile_label_length", nullptr);
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "number", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "tile_number");
  RNA_def_property_ui_text(prop, "Number", "Number of the position that this tile covers");
  RNA_def_property_int_funcs(prop, nullptr, "rna_UDIMTile_tile_number_set", nullptr);
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_int_vector(
      srna,
      "size",
      2,
      nullptr,
      0,
      0,
      "Size",
      "Width and height of the tile buffer in pixels, zero when image data can't be loaded",
      0,
      0);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_int_funcs(prop, "rna_UDIMTile_size_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "channels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_UDIMTile_channels_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Channels", "Number of channels in the tile pixels buffer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Generated tile information. */
  prop = RNA_def_property(srna, "generated_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gen_type");
  RNA_def_property_enum_items(prop, rna_enum_image_generated_type_items);
  RNA_def_property_ui_text(prop, "Generated Type", "Generated image type");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_UDIMTile_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "generated_width", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "gen_x");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 1, 65536);
  RNA_def_property_ui_text(prop, "Generated Width", "Generated image width");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_UDIMTile_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "generated_height", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "gen_y");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 1, 65536);
  RNA_def_property_ui_text(prop, "Generated Height", "Generated image height");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_UDIMTile_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_generated_float", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gen_flag", IMA_GEN_FLOAT);
  RNA_def_property_ui_text(prop, "Float Buffer", "Generate floating-point buffer");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_UDIMTile_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "generated_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gen_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "Fill color for the generated image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_UDIMTile_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

static void rna_def_udim_tiles(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "UDIMTiles");
  srna = RNA_def_struct(brna, "UDIMTiles", nullptr);
  RNA_def_struct_sdna(srna, "Image");
  RNA_def_struct_ui_text(srna, "UDIM Tiles", "Collection of UDIM tiles");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_tile_index");
  RNA_def_property_int_funcs(prop,
                             "rna_Image_active_tile_index_get",
                             "rna_Image_active_tile_index_set",
                             "rna_Image_active_tile_index_range");
  RNA_def_property_ui_text(prop, "Active Tile Index", "Active index in tiles array");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "UDIMTile");
  RNA_def_property_pointer_funcs(
      prop, "rna_Image_active_tile_get", "rna_Image_active_tile_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Active Image Tile", "Active Image Tile");

  func = RNA_def_function(srna, "new", "rna_UDIMTile_new");
  RNA_def_function_ui_description(func, "Add a tile to the image");
  parm = RNA_def_int(
      func, "tile_number", 1, 1, INT_MAX, "", "Number of the newly created tile", 1, 100);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "label", nullptr, 0, "", "Optional label for the tile");
  parm = RNA_def_pointer(func, "result", "UDIMTile", "", "Newly created image tile");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "get", "BKE_image_get_tile");
  RNA_def_function_ui_description(func, "Get a tile based on its tile number");
  parm = RNA_def_int(func, "tile_number", 0, 0, INT_MAX, "", "Number of the tile", 0, 100);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "UDIMTile", "", "The tile");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_UDIMTile_remove");
  RNA_def_function_ui_description(func, "Remove an image tile");
  parm = RNA_def_pointer(func, "tile", "UDIMTile", "", "Image tile to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_image(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static const EnumPropertyItem prop_type_items[] = {
      {IMA_TYPE_IMAGE, "IMAGE", 0, "Image", ""},
      {IMA_TYPE_MULTILAYER, "MULTILAYER", 0, "Multilayer", ""},
      {IMA_TYPE_UV_TEST, "UV_TEST", 0, "UV Test", ""},
      {IMA_TYPE_R_RESULT, "RENDER_RESULT", 0, "Render Result", ""},
      {IMA_TYPE_COMPOSITE, "COMPOSITING", 0, "Compositing", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem alpha_mode_items[] = {
      {IMA_ALPHA_STRAIGHT,
       "STRAIGHT",
       0,
       "Straight",
       "Store RGB and alpha channels separately with alpha acting as a mask, also known as "
       "unassociated alpha. Commonly used by image editing applications and file formats like "
       "PNG"},
      {IMA_ALPHA_PREMUL,
       "PREMUL",
       0,
       "Premultiplied",
       "Store RGB channels with alpha multiplied in, also known as associated alpha. The natural "
       "format for renders and used by file formats like OpenEXR"},
      {IMA_ALPHA_CHANNEL_PACKED,
       "CHANNEL_PACKED",
       0,
       "Channel Packed",
       "Different images are packed in the RGB and alpha channels, and they should not "
       "affect each other. Channel packing is commonly used by game engines to save memory"},
      {IMA_ALPHA_IGNORE,
       "NONE",
       0,
       "None",
       "Ignore alpha channel from the file and make image fully opaque"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Image", "ID");
  RNA_def_struct_ui_text(
      srna, "Image", "Image data-block referencing an external or packed image");
  RNA_def_struct_ui_icon(srna, ICON_IMAGE_DATA);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_string_sdna(prop, nullptr, "filepath");
  RNA_def_property_ui_text(prop, "File Name", "Image/Movie file name");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_reload_update");

  /* eek. this is horrible but needed so we can save to a new name without blanking the data :( */
  prop = RNA_def_property(srna, "filepath_raw", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "filepath");
  RNA_def_property_ui_text(prop, "File Name", "Image/Movie file name (without data refreshing)");

  prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_image_type_items);
  RNA_def_property_enum_funcs(
      prop, "rna_Image_file_format_get", "rna_Image_file_format_set", nullptr);
  RNA_def_property_ui_text(prop, "File Format", "Format used for re-saving this file");

  prop = RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, image_source_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Image_source_set", "rna_Image_source_itemf");
  RNA_def_property_ui_text(prop, "Source", "Where the image comes from");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "How to generate the image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "PackedFile");
  RNA_def_property_pointer_sdna(prop, nullptr, "packedfile");
  RNA_def_property_pointer_funcs(prop, "rna_Image_packed_file_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Packed File", "First packed file of the image");

  prop = RNA_def_property(srna, "packed_files", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "packedfiles", nullptr);
  RNA_def_property_struct_type(prop, "ImagePackedFile");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Packed Files", "Collection of packed images");

  prop = RNA_def_property(srna, "use_view_as_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMA_VIEW_AS_RENDER);
  RNA_def_property_ui_text(
      prop,
      "View as Render",
      "Apply render part of display transformation when displaying this image on the screen");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "use_deinterlace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMA_DEINTERLACE);
  RNA_def_property_ui_text(prop, "Deinterlace", "Deinterlace movie file on load");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_reload_update");

  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMA_USE_VIEWS);
  RNA_def_property_ui_text(prop, "Use Multi-View", "Use Multiple Views (when available)");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_views_format_update");

  prop = RNA_def_property(srna, "is_stereo_3d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, "rna_Image_is_stereo_3d_get", nullptr);
  RNA_def_property_ui_text(prop, "Stereo 3D", "Image has left and right views");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, "rna_Image_is_multiview_get", nullptr);
  RNA_def_property_ui_text(prop, "Multiple Views", "Image has more than one view");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, "rna_Image_dirty_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Dirty", "Image has changed and is not saved");

  /* generated image (image_generated_change_cb) */
  prop = RNA_def_property(srna, "generated_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gen_type");
  RNA_def_property_enum_items(prop, rna_enum_image_generated_type_items);
  RNA_def_property_ui_text(prop, "Generated Type", "Generated image type");
  RNA_def_property_enum_funcs(
      prop, "rna_Image_generated_type_get", "rna_Image_generated_type_set", nullptr);
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "generated_width", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "gen_x");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 1, 65536);
  RNA_def_property_ui_text(prop, "Generated Width", "Generated image width");
  RNA_def_property_int_funcs(
      prop, "rna_Image_generated_width_get", "rna_Image_generated_width_set", nullptr);
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "generated_height", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "gen_y");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 1, 65536);
  RNA_def_property_ui_text(prop, "Generated Height", "Generated image height");
  RNA_def_property_int_funcs(
      prop, "rna_Image_generated_height_get", "rna_Image_generated_height_set", nullptr);
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_generated_float", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gen_flag", IMA_GEN_FLOAT);
  RNA_def_property_ui_text(prop, "Float Buffer", "Generate floating-point buffer");
  RNA_def_property_boolean_funcs(
      prop, "rna_Image_generated_float_get", "rna_Image_generated_float_set");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "generated_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gen_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "Fill color for the generated image");
  RNA_def_property_float_funcs(
      prop, "rna_Image_generated_color_get", "rna_Image_generated_color_set", nullptr);
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_generated_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "display_aspect", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_float_sdna(prop, nullptr, "aspx");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.1f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.1f, 5000.0f, 1, 2);
  RNA_def_property_ui_text(
      prop, "Display Aspect", "Display Aspect for this image, does not affect rendering");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "bindcode", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Image_bindcode_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Bindcode", "OpenGL bindcode");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "render_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "RenderSlot");
  RNA_def_property_collection_sdna(prop, nullptr, "renderslots", nullptr);
  RNA_def_property_ui_text(prop, "Render Slots", "Render slots of the image");
  rna_def_render_slots(brna, prop);

  prop = RNA_def_property(srna, "tiles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "UDIMTile");
  RNA_def_property_collection_sdna(prop, nullptr, "tiles", nullptr);
  RNA_def_property_ui_text(prop, "Image Tiles", "Tiles of the image");
  rna_def_udim_tiles(brna, prop);

  prop = RNA_def_property(srna, "has_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Image_has_data_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has Data", "True if the image data is loaded into memory");

  prop = RNA_def_property(srna, "depth", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Image_depth_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Depth", "Image bit depth");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int_vector(
      srna,
      "size",
      2,
      nullptr,
      0,
      0,
      "Size",
      "Width and height of the image buffer in pixels, zero when image data can't be loaded",
      0,
      0);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_int_funcs(prop, "rna_Image_size_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_float_vector(srna,
                              "resolution",
                              2,
                              nullptr,
                              0,
                              0,
                              "Resolution",
                              "X/Y pixels per meter, for the image buffer",
                              0,
                              0);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_float_funcs(
      prop, "rna_Image_resolution_get", "rna_Image_resolution_set", nullptr);

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Image_frame_duration_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Duration", "Duration (in frames) of the image (1 when not a video/sequence)");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* NOTE: About pixels/channels/is_float:
   * These properties describe how the image is stored internally (inside of ImBuf),
   * not how it was saved to disk or how it'll be saved on disk.
   */
  prop = RNA_def_property(srna, "pixels", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_multi_array(prop, 1, nullptr);
  RNA_def_property_ui_text(prop, "Pixels", "Image buffer pixels in floating-point values");
  RNA_def_property_dynamic_array_funcs(prop, "rna_Image_pixels_get_length");
  RNA_def_property_float_funcs(prop, "rna_Image_pixels_get", "rna_Image_pixels_set", nullptr);

  prop = RNA_def_property(srna, "channels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Image_channels_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Channels", "Number of channels in pixels buffer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_float", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Image_is_float_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Float", "True if this image is stored in floating-point buffer");

  prop = RNA_def_property(srna, "colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "colorspace_settings");
  RNA_def_property_struct_type(prop, "ColorManagedInputColorspaceSettings");
  RNA_def_property_ui_text(prop, "Color Space Settings", "Input color space settings");

  prop = RNA_def_property(srna, "alpha_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_enum_items(prop, alpha_mode_items);
  RNA_def_property_ui_text(prop,
                           "Alpha Mode",
                           "Representation of alpha in the image file, to convert to and from "
                           "when saving and loading the image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_alpha_mode_update");

  prop = RNA_def_property(srna, "use_half_precision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", IMA_HIGH_BITDEPTH);
  RNA_def_property_ui_text(prop,
                           "Half Float Precision",
                           "Use 16 bits per channel to lower the memory usage during rendering");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_gpu_texture_update");

  prop = RNA_def_property(srna, "seam_margin", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Seam Margin",
      "Margin to take into account when fixing UV seams during painting. Higher "
      "number would improve seam-fixes for mipmaps, but decreases performance");
  RNA_def_property_ui_range(prop, 1, 100, 1, 1);

  /* multiview */
  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_enum_sdna(prop, nullptr, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load image views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Image_views_format_update");

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3d");

  RNA_api_image(srna);
}

void RNA_def_image(BlenderRNA *brna)
{
  rna_def_render_slot(brna);
  rna_def_udim_tile(brna);
  rna_def_image(brna);
  rna_def_imageuser(brna);
  rna_def_image_packed_files(brna);
}

#endif
