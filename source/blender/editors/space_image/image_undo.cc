/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 *
 * Overview
 * ========
 *
 * - Each undo step is a #ImageUndoStep
 * - Each #ImageUndoStep stores a list of #UndoImageHandle
 *   - Each #UndoImageHandle stores a list of #UndoImageBuf
 *     (this is the undo systems equivalent of an #ImBuf).
 *     - Each #UndoImageBuf stores an array of #UndoImageTile
 *       The tiles are shared between #UndoImageBuf's to avoid duplication.
 *
 * When the undo system manages an image, there will always be a full copy (as a #UndoImageBuf)
 * each new undo step only stores modified tiles.
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_undo_system.hh"

#include "DEG_depsgraph.hh"

#include "ED_object.hh"
#include "ED_paint.hh"
#include "ED_undo.hh"
#include "ED_util.hh"

#include "WM_api.hh"

static CLG_LogRef LOG = {"undo.image"};

/* -------------------------------------------------------------------- */
/** \name Thread Locking
 * \{ */

/* This is a non-global static resource,
 * Maybe it should be exposed as part of the
 * paint operation, but for now just give a public interface */
static SpinLock paint_tiles_lock;

void ED_image_paint_tile_lock_init()
{
  BLI_spin_init(&paint_tiles_lock);
}

void ED_image_paint_tile_lock_end()
{
  BLI_spin_end(&paint_tiles_lock);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Tiles
 *
 * Created on demand while painting,
 * use to access the previous state for some paint operations.
 *
 * These buffers are also used for undo when available.
 *
 * \{ */

static ImBuf *imbuf_alloc_temp_tile()
{
  return IMB_allocImBuf(
      ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE, 32, IB_float_data | IB_byte_data);
}

struct PaintTileKey {
  int x_tile, y_tile;
  Image *image;
  ImBuf *ibuf;
  /* Copied from iuser.tile in PaintTile. */
  int iuser_tile;

  uint64_t hash() const
  {
    return blender::get_default_hash(x_tile, y_tile, image, ibuf);
  }
  bool operator==(const PaintTileKey &other) const
  {
    return x_tile == other.x_tile && y_tile == other.y_tile && image == other.image &&
           ibuf == other.ibuf && iuser_tile == other.iuser_tile;
  }
};

struct PaintTile {
  Image *image;
  ImBuf *ibuf;
  /* For 2D image painting the ImageUser uses most of the values.
   * Even though views and passes are stored they are currently not supported for painting.
   * For 3D projection painting this only uses a tile & frame number.
   * The scene pointer must be cleared (or temporarily set it as needed, but leave cleared). */
  ImageUser iuser;
  union {
    float *fp;
    uint8_t *byte_ptr;
    void *pt;
  } rect;
  uint16_t *mask;
  bool valid;
  bool use_float;
  int x_tile, y_tile;
};

static void ptile_free(PaintTile *ptile)
{
  if (ptile->rect.pt) {
    MEM_freeN(ptile->rect.pt);
  }
  if (ptile->mask) {
    MEM_freeN(ptile->mask);
  }
  MEM_freeN(ptile);
}

struct PaintTileMap {
  blender::Map<PaintTileKey, PaintTile *> map;

  ~PaintTileMap()
  {
    for (PaintTile *ptile : map.values()) {
      ptile_free(ptile);
    }
  }
};

static void ptile_invalidate_map(PaintTileMap *paint_tile_map)
{
  for (PaintTile *ptile : paint_tile_map->map.values()) {
    ptile->valid = false;
  }
}

void *ED_image_paint_tile_find(PaintTileMap *paint_tile_map,
                               Image *image,
                               ImBuf *ibuf,
                               ImageUser *iuser,
                               int x_tile,
                               int y_tile,
                               ushort **r_mask,
                               bool validate)
{
  PaintTileKey key;
  key.ibuf = ibuf;
  key.image = image;
  key.iuser_tile = iuser->tile;
  key.x_tile = x_tile;
  key.y_tile = y_tile;
  PaintTile **pptile = paint_tile_map->map.lookup_ptr(key);
  if (pptile == nullptr) {
    return nullptr;
  }
  PaintTile *ptile = *pptile;
  if (r_mask) {
    /* allocate mask if requested. */
    if (!ptile->mask) {
      ptile->mask = MEM_calloc_arrayN<uint16_t>(square_i(ED_IMAGE_UNDO_TILE_SIZE),
                                                "UndoImageTile.mask");
    }
    *r_mask = ptile->mask;
  }
  if (validate) {
    ptile->valid = true;
  }
  return ptile->rect.pt;
}

/* Set the given buffer data as an owning data of the imbuf's buffer.
 * Returns the data pointer which was stolen from the imbuf before assignment. */
static uint8_t *image_undo_steal_and_assign_byte_buffer(ImBuf *ibuf, uint8_t *new_buffer_data)
{
  uint8_t *old_buffer_data = IMB_steal_byte_buffer(ibuf);
  IMB_assign_byte_buffer(ibuf, new_buffer_data, IB_TAKE_OWNERSHIP);
  return old_buffer_data;
}
static float *image_undo_steal_and_assign_float_buffer(ImBuf *ibuf, float *new_buffer_data)
{
  float *old_buffer_data = IMB_steal_float_buffer(ibuf);
  IMB_assign_float_buffer(ibuf, new_buffer_data, IB_TAKE_OWNERSHIP);
  return old_buffer_data;
}

void *ED_image_paint_tile_push(PaintTileMap *paint_tile_map,
                               Image *image,
                               ImBuf *ibuf,
                               ImBuf **tmpibuf,
                               ImageUser *iuser,
                               int x_tile,
                               int y_tile,
                               ushort **r_mask,
                               bool **r_valid,
                               bool use_thread_lock,
                               bool find_prev)
{
  if (use_thread_lock) {
    BLI_spin_lock(&paint_tiles_lock);
  }
  const bool has_float = (ibuf->float_buffer.data != nullptr);

  /* check if tile is already pushed */

  /* in projective painting we keep accounting of tiles, so if we need one pushed, just push! */
  if (find_prev) {
    void *data = ED_image_paint_tile_find(
        paint_tile_map, image, ibuf, iuser, x_tile, y_tile, r_mask, true);
    if (data) {
      if (use_thread_lock) {
        BLI_spin_unlock(&paint_tiles_lock);
      }
      return data;
    }
  }

  if (*tmpibuf == nullptr) {
    *tmpibuf = imbuf_alloc_temp_tile();
  }

  PaintTile *ptile = MEM_callocN<PaintTile>("PaintTile");

  ptile->image = image;
  ptile->ibuf = ibuf;
  ptile->iuser = *iuser;
  ptile->iuser.scene = nullptr;

  ptile->x_tile = x_tile;
  ptile->y_tile = y_tile;

  /* add mask explicitly here */
  if (r_mask) {
    *r_mask = ptile->mask = MEM_calloc_arrayN<uint16_t>(square_i(ED_IMAGE_UNDO_TILE_SIZE),
                                                        "PaintTile.mask");
  }

  ptile->rect.pt = MEM_callocN((ibuf->float_buffer.data ? sizeof(float[4]) : sizeof(char[4])) *
                                   square_i(ED_IMAGE_UNDO_TILE_SIZE),
                               "PaintTile.rect");

  ptile->use_float = has_float;
  ptile->valid = true;

  if (r_valid) {
    *r_valid = &ptile->valid;
  }

  IMB_rectcpy(*tmpibuf,
              ibuf,
              0,
              0,
              x_tile * ED_IMAGE_UNDO_TILE_SIZE,
              y_tile * ED_IMAGE_UNDO_TILE_SIZE,
              ED_IMAGE_UNDO_TILE_SIZE,
              ED_IMAGE_UNDO_TILE_SIZE);

  if (has_float) {
    ptile->rect.fp = image_undo_steal_and_assign_float_buffer(*tmpibuf, ptile->rect.fp);
  }
  else {
    ptile->rect.byte_ptr = image_undo_steal_and_assign_byte_buffer(*tmpibuf, ptile->rect.byte_ptr);
  }

  PaintTileKey key = {};
  key.ibuf = ibuf;
  key.image = image;
  key.iuser_tile = iuser->tile;
  key.x_tile = x_tile;
  key.y_tile = y_tile;
  PaintTile *existing_tile = nullptr;
  paint_tile_map->map.add_or_modify(
      key,
      [&](PaintTile **pptile) { *pptile = ptile; },
      [&](PaintTile **pptile) { existing_tile = *pptile; });
  if (existing_tile) {
    ptile_free(ptile);
    ptile = existing_tile;
  }

  if (use_thread_lock) {
    BLI_spin_unlock(&paint_tiles_lock);
  }
  return ptile->rect.pt;
}

static void ptile_restore_runtime_map(PaintTileMap *paint_tile_map)
{
  ImBuf *tmpibuf = imbuf_alloc_temp_tile();

  for (PaintTile *ptile : paint_tile_map->map.values()) {
    Image *image = ptile->image;
    ImBuf *ibuf = BKE_image_acquire_ibuf(image, &ptile->iuser, nullptr);
    const bool has_float = (ibuf->float_buffer.data != nullptr);

    if (has_float) {
      ptile->rect.fp = image_undo_steal_and_assign_float_buffer(tmpibuf, ptile->rect.fp);
    }
    else {
      ptile->rect.byte_ptr = image_undo_steal_and_assign_byte_buffer(tmpibuf,
                                                                     ptile->rect.byte_ptr);
    }

    /* TODO(sergey): Look into implementing API which does not require such temporary buffer
     * assignment. */
    IMB_rectcpy(ibuf,
                tmpibuf,
                ptile->x_tile * ED_IMAGE_UNDO_TILE_SIZE,
                ptile->y_tile * ED_IMAGE_UNDO_TILE_SIZE,
                0,
                0,
                ED_IMAGE_UNDO_TILE_SIZE,
                ED_IMAGE_UNDO_TILE_SIZE);

    if (has_float) {
      ptile->rect.fp = image_undo_steal_and_assign_float_buffer(tmpibuf, ptile->rect.fp);
    }
    else {
      ptile->rect.byte_ptr = image_undo_steal_and_assign_byte_buffer(tmpibuf,
                                                                     ptile->rect.byte_ptr);
    }

    /* Force OpenGL reload (maybe partial update will operate better?) */
    BKE_image_free_gputextures(image);

    if (ibuf->float_buffer.data) {
      ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
    }
    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

    BKE_image_release_ibuf(image, ibuf, nullptr);
  }

  IMB_freeImBuf(tmpibuf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Undo Tile
 * \{ */

static uint32_t index_from_xy(uint32_t tile_x, uint32_t tile_y, const uint32_t tiles_dims[2])
{
  BLI_assert(tile_x < tiles_dims[0] && tile_y < tiles_dims[1]);
  return (tile_y * tiles_dims[0]) + tile_x;
}

struct UndoImageTile {
  union {
    float *fp;
    uint8_t *byte_ptr;
    void *pt;
  } rect;
  int users;
};

static UndoImageTile *utile_alloc(bool has_float)
{
  UndoImageTile *utile = static_cast<UndoImageTile *>(
      MEM_callocN(sizeof(*utile), "ImageUndoTile"));
  if (has_float) {
    utile->rect.fp = static_cast<float *>(
        MEM_mallocN(sizeof(float[4]) * square_i(ED_IMAGE_UNDO_TILE_SIZE), __func__));
  }
  else {
    utile->rect.byte_ptr = static_cast<uint8_t *>(
        MEM_mallocN(sizeof(uint32_t) * square_i(ED_IMAGE_UNDO_TILE_SIZE), __func__));
  }
  return utile;
}

static void utile_init_from_imbuf(
    UndoImageTile *utile, const uint32_t x, const uint32_t y, const ImBuf *ibuf, ImBuf *tmpibuf)
{
  const bool has_float = ibuf->float_buffer.data;

  if (has_float) {
    utile->rect.fp = image_undo_steal_and_assign_float_buffer(tmpibuf, utile->rect.fp);
  }
  else {
    utile->rect.byte_ptr = image_undo_steal_and_assign_byte_buffer(tmpibuf, utile->rect.byte_ptr);
  }

  /* TODO(sergey): Look into implementing API which does not require such temporary buffer
   * assignment. */
  IMB_rectcpy(tmpibuf, ibuf, 0, 0, x, y, ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE);

  if (has_float) {
    utile->rect.fp = image_undo_steal_and_assign_float_buffer(tmpibuf, utile->rect.fp);
  }
  else {
    utile->rect.byte_ptr = image_undo_steal_and_assign_byte_buffer(tmpibuf, utile->rect.byte_ptr);
  }
}

static void utile_restore(
    const UndoImageTile *utile, const uint x, const uint y, ImBuf *ibuf, ImBuf *tmpibuf)
{
  const bool has_float = ibuf->float_buffer.data;
  float *prev_rect_float = tmpibuf->float_buffer.data;
  uint8_t *prev_rect = tmpibuf->byte_buffer.data;

  if (has_float) {
    tmpibuf->float_buffer.data = utile->rect.fp;
  }
  else {
    tmpibuf->byte_buffer.data = utile->rect.byte_ptr;
  }

  /* TODO(sergey): Look into implementing API which does not require such temporary buffer
   * assignment. */
  IMB_rectcpy(ibuf, tmpibuf, x, y, 0, 0, ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE);

  tmpibuf->float_buffer.data = prev_rect_float;
  tmpibuf->byte_buffer.data = prev_rect;
}

static void utile_decref(UndoImageTile *utile)
{
  utile->users -= 1;
  BLI_assert(utile->users >= 0);
  if (utile->users == 0) {
    MEM_freeN(utile->rect.pt);
    MEM_delete(utile);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Undo Buffer
 * \{ */

struct UndoImageBuf {
  UndoImageBuf *next, *prev;

  /**
   * The buffer after the undo step has executed.
   */
  UndoImageBuf *post;

  char ibuf_filepath[IMB_FILEPATH_SIZE];
  int ibuf_fileframe;

  UndoImageTile **tiles;

  /** Can calculate these from dims, just for convenience. */
  uint32_t tiles_len;
  uint32_t tiles_dims[2];

  uint32_t image_dims[2];

  /** Store variables from the image. */
  struct {
    short source;
    bool use_float;
  } image_state;
};

static UndoImageBuf *ubuf_from_image_no_tiles(Image *image, const ImBuf *ibuf)
{
  UndoImageBuf *ubuf = MEM_callocN<UndoImageBuf>(__func__);

  ubuf->image_dims[0] = ibuf->x;
  ubuf->image_dims[1] = ibuf->y;

  ubuf->tiles_dims[0] = ED_IMAGE_UNDO_TILE_NUMBER(ubuf->image_dims[0]);
  ubuf->tiles_dims[1] = ED_IMAGE_UNDO_TILE_NUMBER(ubuf->image_dims[1]);

  ubuf->tiles_len = ubuf->tiles_dims[0] * ubuf->tiles_dims[1];
  ubuf->tiles = static_cast<UndoImageTile **>(
      MEM_callocN(sizeof(*ubuf->tiles) * ubuf->tiles_len, __func__));

  STRNCPY(ubuf->ibuf_filepath, ibuf->filepath);
  ubuf->ibuf_fileframe = ibuf->fileframe;
  ubuf->image_state.source = image->source;
  ubuf->image_state.use_float = ibuf->float_buffer.data != nullptr;

  return ubuf;
}

static void ubuf_from_image_all_tiles(UndoImageBuf *ubuf, const ImBuf *ibuf)
{
  ImBuf *tmpibuf = imbuf_alloc_temp_tile();

  const bool has_float = ibuf->float_buffer.data;
  int i = 0;
  for (uint y_tile = 0; y_tile < ubuf->tiles_dims[1]; y_tile += 1) {
    uint y = y_tile << ED_IMAGE_UNDO_TILE_BITS;
    for (uint x_tile = 0; x_tile < ubuf->tiles_dims[0]; x_tile += 1) {
      uint x = x_tile << ED_IMAGE_UNDO_TILE_BITS;

      BLI_assert(ubuf->tiles[i] == nullptr);
      UndoImageTile *utile = utile_alloc(has_float);
      utile->users = 1;
      utile_init_from_imbuf(utile, x, y, ibuf, tmpibuf);
      ubuf->tiles[i] = utile;

      i += 1;
    }
  }

  BLI_assert(i == ubuf->tiles_len);

  IMB_freeImBuf(tmpibuf);
}

/** Ensure we can copy the ubuf into the ibuf. */
static void ubuf_ensure_compat_ibuf(const UndoImageBuf *ubuf, ImBuf *ibuf)
{
  /* We could have both float and rect buffers,
   * in this case free the float buffer if it's unused. */
  if ((ibuf->float_buffer.data != nullptr) && (ubuf->image_state.use_float == false)) {
    IMB_free_float_pixels(ibuf);
  }

  if (ibuf->x == ubuf->image_dims[0] && ibuf->y == ubuf->image_dims[1] &&
      (ubuf->image_state.use_float ? (void *)ibuf->float_buffer.data :
                                     (void *)ibuf->byte_buffer.data))
  {
    return;
  }

  IMB_free_all_data(ibuf);
  IMB_rect_size_set(ibuf, ubuf->image_dims);

  if (ubuf->image_state.use_float) {
    IMB_alloc_float_pixels(ibuf, 4);
  }
  else {
    IMB_alloc_byte_pixels(ibuf);
  }
}

static void ubuf_free(UndoImageBuf *ubuf)
{
  UndoImageBuf *ubuf_post = ubuf->post;
  for (uint i = 0; i < ubuf->tiles_len; i++) {
    UndoImageTile *utile = ubuf->tiles[i];
    utile_decref(utile);
  }
  MEM_freeN(ubuf->tiles);
  MEM_freeN(ubuf);
  if (ubuf_post) {
    ubuf_free(ubuf_post);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Undo Handle
 * \{ */

struct UndoImageHandle {
  UndoImageHandle *next, *prev;

  /** Each undo handle refers to a single image which may have multiple buffers. */
  UndoRefID_Image image_ref;

  /**
   * Each tile of a tiled image has its own UndoImageHandle.
   * The tile number of this IUser is used to distinguish them.
   */
  ImageUser iuser;

  /**
   * List of #UndoImageBuf's to support multiple buffers per image.
   */
  ListBase buffers;
};

static void uhandle_restore_list(ListBase *undo_handles, bool use_init)
{
  ImBuf *tmpibuf = imbuf_alloc_temp_tile();

  LISTBASE_FOREACH (UndoImageHandle *, uh, undo_handles) {
    /* Tiles only added to second set of tiles. */
    Image *image = uh->image_ref.ptr;

    ImBuf *ibuf = BKE_image_acquire_ibuf(image, &uh->iuser, nullptr);
    if (UNLIKELY(ibuf == nullptr)) {
      CLOG_ERROR(&LOG, "Unable to get buffer for image '%s'", image->id.name + 2);
      continue;
    }
    bool changed = false;
    LISTBASE_FOREACH (UndoImageBuf *, ubuf_iter, &uh->buffers) {
      UndoImageBuf *ubuf = use_init ? ubuf_iter : ubuf_iter->post;
      ubuf_ensure_compat_ibuf(ubuf, ibuf);

      int i = 0;
      for (uint y_tile = 0; y_tile < ubuf->tiles_dims[1]; y_tile += 1) {
        uint y = y_tile << ED_IMAGE_UNDO_TILE_BITS;
        for (uint x_tile = 0; x_tile < ubuf->tiles_dims[0]; x_tile += 1) {
          uint x = x_tile << ED_IMAGE_UNDO_TILE_BITS;
          utile_restore(ubuf->tiles[i], x, y, ibuf, tmpibuf);
          changed = true;
          i += 1;
        }
      }
    }

    if (changed) {
      BKE_image_mark_dirty(image, ibuf);
      /* TODO(@jbakker): only mark areas that are actually updated to improve performance. */
      BKE_image_partial_update_mark_full_update(image);

      if (ibuf->float_buffer.data) {
        ibuf->userflags |= IB_RECT_INVALID; /* Force recreate of char `rect` */
      }
      ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

      DEG_id_tag_update(&image->id, 0);
    }
    BKE_image_release_ibuf(image, ibuf, nullptr);
  }

  IMB_freeImBuf(tmpibuf);
}

static void uhandle_free_list(ListBase *undo_handles)
{
  LISTBASE_FOREACH_MUTABLE (UndoImageHandle *, uh, undo_handles) {
    LISTBASE_FOREACH_MUTABLE (UndoImageBuf *, ubuf, &uh->buffers) {
      ubuf_free(ubuf);
    }
    MEM_freeN(uh);
  }
  BLI_listbase_clear(undo_handles);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Undo Internal Utilities
 * \{ */

/** #UndoImageHandle utilities */

static UndoImageBuf *uhandle_lookup_ubuf(UndoImageHandle *uh,
                                         const Image * /*image*/,
                                         const char *ibuf_filepath,
                                         const int ibuf_fileframe)
{
  LISTBASE_FOREACH (UndoImageBuf *, ubuf, &uh->buffers) {
    if (STREQ(ubuf->ibuf_filepath, ibuf_filepath) && ubuf->ibuf_fileframe == ibuf_fileframe) {
      return ubuf;
    }
  }
  return nullptr;
}

static UndoImageBuf *uhandle_add_ubuf(UndoImageHandle *uh, Image *image, ImBuf *ibuf)
{
  BLI_assert(uhandle_lookup_ubuf(uh, image, ibuf->filepath, ibuf->fileframe) == nullptr);
  UndoImageBuf *ubuf = ubuf_from_image_no_tiles(image, ibuf);
  BLI_addtail(&uh->buffers, ubuf);

  ubuf->post = nullptr;

  return ubuf;
}

static UndoImageBuf *uhandle_ensure_ubuf(UndoImageHandle *uh, Image *image, ImBuf *ibuf)
{
  UndoImageBuf *ubuf = uhandle_lookup_ubuf(uh, image, ibuf->filepath, ibuf->fileframe);
  if (ubuf == nullptr) {
    ubuf = uhandle_add_ubuf(uh, image, ibuf);
  }
  return ubuf;
}

static UndoImageHandle *uhandle_lookup_by_name(ListBase *undo_handles,
                                               const Image *image,
                                               int tile_number)
{
  LISTBASE_FOREACH (UndoImageHandle *, uh, undo_handles) {
    if (STREQ(image->id.name + 2, uh->image_ref.name + 2) && uh->iuser.tile == tile_number) {
      return uh;
    }
  }
  return nullptr;
}

static UndoImageHandle *uhandle_lookup(ListBase *undo_handles, const Image *image, int tile_number)
{
  LISTBASE_FOREACH (UndoImageHandle *, uh, undo_handles) {
    if (image == uh->image_ref.ptr && uh->iuser.tile == tile_number) {
      return uh;
    }
  }
  return nullptr;
}

static UndoImageHandle *uhandle_add(ListBase *undo_handles, Image *image, ImageUser *iuser)
{
  BLI_assert(uhandle_lookup(undo_handles, image, iuser->tile) == nullptr);
  UndoImageHandle *uh = MEM_callocN<UndoImageHandle>(__func__);
  uh->image_ref.ptr = image;
  uh->iuser = *iuser;
  uh->iuser.scene = nullptr;
  BLI_addtail(undo_handles, uh);
  return uh;
}

static UndoImageHandle *uhandle_ensure(ListBase *undo_handles, Image *image, ImageUser *iuser)
{
  UndoImageHandle *uh = uhandle_lookup(undo_handles, image, iuser->tile);
  if (uh == nullptr) {
    uh = uhandle_add(undo_handles, image, iuser);
  }
  return uh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

struct ImageUndoStep {
  UndoStep step;

  /** #UndoImageHandle */
  ListBase handles;

  /**
   * #PaintTile
   * Run-time only data (active during a paint stroke).
   */
  PaintTileMap *paint_tile_map;

  bool is_encode_init;
  PaintMode paint_mode;
};

/**
 * Find the previous undo buffer from this one.
 * \note We could look into undo steps even further back.
 */
static UndoImageBuf *ubuf_lookup_from_reference(ImageUndoStep *us_prev,
                                                const Image *image,
                                                int tile_number,
                                                const UndoImageBuf *ubuf)
{
  /* Use name lookup because the pointer is cleared for previous steps. */
  UndoImageHandle *uh_prev = uhandle_lookup_by_name(&us_prev->handles, image, tile_number);
  if (uh_prev != nullptr) {
    UndoImageBuf *ubuf_reference = uhandle_lookup_ubuf(
        uh_prev, image, ubuf->ibuf_filepath, ubuf->ibuf_fileframe);
    if (ubuf_reference) {
      ubuf_reference = ubuf_reference->post;
      if ((ubuf_reference->image_dims[0] == ubuf->image_dims[0]) &&
          (ubuf_reference->image_dims[1] == ubuf->image_dims[1]))
      {
        return ubuf_reference;
      }
    }
  }
  return nullptr;
}

static bool image_undosys_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  ScrArea *area = CTX_wm_area(C);
  if (area && (area->spacetype == SPACE_IMAGE)) {
    SpaceImage *sima = (SpaceImage *)area->spacedata.first;
    if ((obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) || (sima->mode == SI_MODE_PAINT)) {
      return true;
    }
  }
  else {
    if (obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) {
      return true;
    }
  }
  return false;
}

static void image_undosys_step_encode_init(bContext * /*C*/, UndoStep *us_p)
{
  ImageUndoStep *us = reinterpret_cast<ImageUndoStep *>(us_p);
  /* dummy, memory is cleared anyway. */
  us->is_encode_init = true;
  BLI_listbase_clear(&us->handles);
  us->paint_tile_map = MEM_new<PaintTileMap>(__func__);
}

static bool image_undosys_step_encode(bContext *C, Main * /*bmain*/, UndoStep *us_p)
{
  /* Encoding is done along the way by adding tiles
   * to the current 'ImageUndoStep' added by encode_init.
   *
   * This function ensures there are previous and current states of the image in the undo buffer.
   */
  ImageUndoStep *us = reinterpret_cast<ImageUndoStep *>(us_p);

  BLI_assert(us->step.data_size == 0);

  if (us->is_encode_init) {

    ImBuf *tmpibuf = imbuf_alloc_temp_tile();

    ImageUndoStep *us_reference = reinterpret_cast<ImageUndoStep *>(
        ED_undo_stack_get()->step_active);
    while (us_reference && us_reference->step.type != BKE_UNDOSYS_TYPE_IMAGE) {
      us_reference = reinterpret_cast<ImageUndoStep *>(us_reference->step.prev);
    }

    /* Initialize undo tiles from paint-tiles (if they exist). */
    for (PaintTile *ptile : us->paint_tile_map->map.values()) {
      if (ptile->valid) {
        UndoImageHandle *uh = uhandle_ensure(&us->handles, ptile->image, &ptile->iuser);
        UndoImageBuf *ubuf_pre = uhandle_ensure_ubuf(uh, ptile->image, ptile->ibuf);

        UndoImageTile *utile = static_cast<UndoImageTile *>(
            MEM_callocN(sizeof(*utile), "UndoImageTile"));
        utile->users = 1;
        utile->rect.pt = ptile->rect.pt;
        ptile->rect.pt = nullptr;
        const uint tile_index = index_from_xy(ptile->x_tile, ptile->y_tile, ubuf_pre->tiles_dims);

        BLI_assert(ubuf_pre->tiles[tile_index] == nullptr);
        ubuf_pre->tiles[tile_index] = utile;
      }
      ptile_free(ptile);
    }
    us->paint_tile_map->map.clear();

    LISTBASE_FOREACH (UndoImageHandle *, uh, &us->handles) {
      LISTBASE_FOREACH (UndoImageBuf *, ubuf_pre, &uh->buffers) {

        ImBuf *ibuf = BKE_image_acquire_ibuf(uh->image_ref.ptr, &uh->iuser, nullptr);

        const bool has_float = ibuf->float_buffer.data;

        BLI_assert(ubuf_pre->post == nullptr);
        ubuf_pre->post = ubuf_from_image_no_tiles(uh->image_ref.ptr, ibuf);
        UndoImageBuf *ubuf_post = ubuf_pre->post;

        if (ubuf_pre->image_dims[0] != ubuf_post->image_dims[0] ||
            ubuf_pre->image_dims[1] != ubuf_post->image_dims[1])
        {
          ubuf_from_image_all_tiles(ubuf_post, ibuf);
        }
        else {
          /* Search for the previous buffer. */
          UndoImageBuf *ubuf_reference =
              (us_reference ? ubuf_lookup_from_reference(
                                  us_reference, uh->image_ref.ptr, uh->iuser.tile, ubuf_post) :
                              nullptr);

          int i = 0;
          for (uint y_tile = 0; y_tile < ubuf_pre->tiles_dims[1]; y_tile += 1) {
            uint y = y_tile << ED_IMAGE_UNDO_TILE_BITS;
            for (uint x_tile = 0; x_tile < ubuf_pre->tiles_dims[0]; x_tile += 1) {
              uint x = x_tile << ED_IMAGE_UNDO_TILE_BITS;

              if ((ubuf_reference != nullptr) &&
                  ((ubuf_pre->tiles[i] == nullptr) ||
                   /* In this case the paint stroke as has added a tile
                    * which we have a duplicate reference available. */
                   (ubuf_pre->tiles[i]->users == 1)))
              {
                if (ubuf_pre->tiles[i] != nullptr) {
                  /* If we have a reference, re-use this single use tile for the post state. */
                  BLI_assert(ubuf_pre->tiles[i]->users == 1);
                  ubuf_post->tiles[i] = ubuf_pre->tiles[i];
                  ubuf_pre->tiles[i] = nullptr;
                  utile_init_from_imbuf(ubuf_post->tiles[i], x, y, ibuf, tmpibuf);
                }
                else {
                  BLI_assert(ubuf_post->tiles[i] == nullptr);
                  ubuf_post->tiles[i] = ubuf_reference->tiles[i];
                  ubuf_post->tiles[i]->users += 1;
                }
                BLI_assert(ubuf_pre->tiles[i] == nullptr);
                ubuf_pre->tiles[i] = ubuf_reference->tiles[i];
                ubuf_pre->tiles[i]->users += 1;

                BLI_assert(ubuf_pre->tiles[i] != nullptr);
                BLI_assert(ubuf_post->tiles[i] != nullptr);
              }
              else {
                UndoImageTile *utile = utile_alloc(has_float);
                utile_init_from_imbuf(utile, x, y, ibuf, tmpibuf);

                if (ubuf_pre->tiles[i] != nullptr) {
                  ubuf_post->tiles[i] = utile;
                  utile->users = 1;
                }
                else {
                  ubuf_pre->tiles[i] = utile;
                  ubuf_post->tiles[i] = utile;
                  utile->users = 2;
                }
              }
              BLI_assert(ubuf_pre->tiles[i] != nullptr);
              BLI_assert(ubuf_post->tiles[i] != nullptr);
              i += 1;
            }
          }
          BLI_assert(i == ubuf_pre->tiles_len);
          BLI_assert(i == ubuf_post->tiles_len);
        }
        BKE_image_release_ibuf(uh->image_ref.ptr, ibuf, nullptr);
      }
    }

    IMB_freeImBuf(tmpibuf);

    /* Useful to debug tiles are stored correctly. */
    if (false) {
      uhandle_restore_list(&us->handles, false);
    }
  }
  else {
    BLI_assert(C != nullptr);
    /* Happens when switching modes. */
    PaintMode paint_mode = BKE_paintmode_get_active_from_context(C);
    BLI_assert(ELEM(paint_mode, PaintMode::Texture2D, PaintMode::Texture3D));
    us->paint_mode = paint_mode;
  }

  us_p->is_applied = true;

  return true;
}

static void image_undosys_step_decode_undo_impl(ImageUndoStep *us, bool is_final)
{
  BLI_assert(us->step.is_applied == true);
  uhandle_restore_list(&us->handles, !is_final);
  us->step.is_applied = false;
}

static void image_undosys_step_decode_redo_impl(ImageUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  uhandle_restore_list(&us->handles, false);
  us->step.is_applied = true;
}

static void image_undosys_step_decode_undo(ImageUndoStep *us, bool is_final)
{
  /* Walk forward over any applied steps of same type,
   * then walk back in the next loop, un-applying them. */
  ImageUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = (ImageUndoStep *)us_iter->step.next;
  }
  while (us_iter != us || (!is_final && us_iter == us)) {
    BLI_assert(us_iter->step.type == us->step.type); /* Previous loop ensures this. */
    image_undosys_step_decode_undo_impl(us_iter, is_final);
    if (us_iter == us) {
      break;
    }
    us_iter = (ImageUndoStep *)us_iter->step.prev;
  }
}

static void image_undosys_step_decode_redo(ImageUndoStep *us)
{
  ImageUndoStep *us_iter = us;
  while (us_iter->step.prev && (us_iter->step.prev->type == us_iter->step.type)) {
    if (us_iter->step.prev->is_applied == true) {
      break;
    }
    us_iter = (ImageUndoStep *)us_iter->step.prev;
  }
  while (us_iter && (us_iter->step.is_applied == false)) {
    image_undosys_step_decode_redo_impl(us_iter);
    if (us_iter == us) {
      break;
    }
    us_iter = (ImageUndoStep *)us_iter->step.next;
  }
}

static void image_undosys_step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir dir, bool is_final)
{
  /* NOTE: behavior for undo/redo closely matches sculpt undo. */
  BLI_assert(dir != STEP_INVALID);

  ImageUndoStep *us = reinterpret_cast<ImageUndoStep *>(us_p);
  if (dir == STEP_UNDO) {
    image_undosys_step_decode_undo(us, is_final);
  }
  else if (dir == STEP_REDO) {
    image_undosys_step_decode_redo(us);
  }

  if (us->paint_mode == PaintMode::Texture3D) {
    blender::ed::object::mode_set_ex(C, OB_MODE_TEXTURE_PAINT, false, nullptr);
  }

  /* Refresh texture slots. */
  ED_editors_init_for_undo(bmain);
}

static void image_undosys_step_free(UndoStep *us_p)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  uhandle_free_list(&us->handles);

  /* Typically this map will have been cleared. */
  MEM_delete(us->paint_tile_map);
  us->paint_tile_map = nullptr;
}

static void image_undosys_foreach_ID_ref(UndoStep *us_p,
                                         UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                         void *user_data)
{
  ImageUndoStep *us = reinterpret_cast<ImageUndoStep *>(us_p);
  LISTBASE_FOREACH (UndoImageHandle *, uh, &us->handles) {
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&uh->image_ref));
  }
}

void ED_image_undosys_type(UndoType *ut)
{
  ut->name = "Image";
  ut->poll = image_undosys_poll;
  ut->step_encode_init = image_undosys_step_encode_init;
  ut->step_encode = image_undosys_step_encode;
  ut->step_decode = image_undosys_step_decode;
  ut->step_free = image_undosys_step_free;

  ut->step_foreach_ID_ref = image_undosys_foreach_ID_ref;

  /* NOTE: this is actually a confusing case, since it expects a valid context, but only in a
   * specific case, see `image_undosys_step_encode` code. We cannot specify
   * `UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE` though, as it can be called with a null context by
   * current code. */
  ut->flags = UNDOTYPE_FLAG_DECODE_ACTIVE_STEP;

  ut->step_size = sizeof(ImageUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 *
 * \note image undo exposes #ED_image_undo_push_begin, #ED_image_undo_push_end
 * which must be called by the operator directly.
 *
 * Unlike most other undo stacks this is needed:
 * - So we can always access the state before the image was painted onto,
 *   which is needed if previous undo states aren't image-type.
 * - So operators can access the pixel-data before the stroke was applied, at run-time.
 * \{ */

PaintTileMap *ED_image_paint_tile_map_get()
{
  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us_prev = ustack->step_init;
  UndoStep *us_p = BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_IMAGE);
  ImageUndoStep *us = reinterpret_cast<ImageUndoStep *>(us_p);
  /* We should always have an undo push started when accessing tiles,
   * not doing this means we won't have paint_mode correctly set. */
  BLI_assert(us_p == us_prev);
  if (us_p != us_prev) {
    /* Fallback value until we can be sure this never happens. */
    us->paint_mode = PaintMode::Texture2D;
  }
  return us->paint_tile_map;
}

void ED_image_undo_restore(UndoStep *us)
{
  PaintTileMap *paint_tile_map = reinterpret_cast<ImageUndoStep *>(us)->paint_tile_map;
  ptile_restore_runtime_map(paint_tile_map);
  ptile_invalidate_map(paint_tile_map);
}

static ImageUndoStep *image_undo_push_begin(const char *name, PaintMode paint_mode)
{
  UndoStack *ustack = ED_undo_stack_get();
  bContext *C = nullptr; /* special case, we never read from this. */
  UndoStep *us_p = BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_IMAGE);
  ImageUndoStep *us = reinterpret_cast<ImageUndoStep *>(us_p);
  BLI_assert(ELEM(paint_mode, PaintMode::Texture2D, PaintMode::Texture3D, PaintMode::Sculpt));
  us->paint_mode = paint_mode;
  return us;
}

void ED_image_undo_push_begin(const char *name, PaintMode paint_mode)
{
  image_undo_push_begin(name, paint_mode);
}

void ED_image_undo_push_begin_with_image(const char *name,
                                         Image *image,
                                         ImBuf *ibuf,
                                         ImageUser *iuser)
{
  ImageUndoStep *us = image_undo_push_begin(name, PaintMode::Texture2D);

  ED_image_undo_push(image, ibuf, iuser, us);
}

void ED_image_undo_push_begin_with_image_all_udims(const char *name,
                                                   Image *image,
                                                   ImageUser *iuser)
{
  ImageUndoStep *us = image_undo_push_begin(name, PaintMode::Texture2D);

  LISTBASE_FOREACH (ImageTile *, current_tile, &image->tiles) {
    iuser->tile = current_tile->tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, nullptr);

    ED_image_undo_push(image, ibuf, iuser, us);

    // Release the image buffer to avoid leaking memory
    BKE_image_release_ibuf(image, ibuf, nullptr);
  }
}

void ED_image_undo_push(Image *image, ImBuf *ibuf, ImageUser *iuser, ImageUndoStep *us)
{
  BLI_assert(BKE_image_get_tile(image, iuser->tile));
  UndoImageHandle *uh = uhandle_ensure(&us->handles, image, iuser);
  UndoImageBuf *ubuf_pre = uhandle_ensure_ubuf(uh, image, ibuf);
  BLI_assert(ubuf_pre->post == nullptr);

  ImageUndoStep *us_reference = reinterpret_cast<ImageUndoStep *>(
      ED_undo_stack_get()->step_active);
  while (us_reference && us_reference->step.type != BKE_UNDOSYS_TYPE_IMAGE) {
    us_reference = reinterpret_cast<ImageUndoStep *>(us_reference->step.prev);
  }
  UndoImageBuf *ubuf_reference = (us_reference ? ubuf_lookup_from_reference(
                                                     us_reference, image, iuser->tile, ubuf_pre) :
                                                 nullptr);

  if (ubuf_reference) {
    memcpy(ubuf_pre->tiles, ubuf_reference->tiles, sizeof(*ubuf_pre->tiles) * ubuf_pre->tiles_len);
    for (uint32_t i = 0; i < ubuf_pre->tiles_len; i++) {
      UndoImageTile *utile = ubuf_pre->tiles[i];
      utile->users += 1;
    }
  }
  else {
    ubuf_from_image_all_tiles(ubuf_pre, ibuf);
  }
}

void ED_image_undo_push_end()
{
  UndoStack *ustack = ED_undo_stack_get();
  BKE_undosys_step_push(ustack, nullptr, nullptr);
  BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
  WM_file_tag_modified();
}

/** \} */
