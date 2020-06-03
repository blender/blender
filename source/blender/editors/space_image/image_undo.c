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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_paint.h"
#include "ED_undo.h"
#include "ED_util.h"

#include "GPU_draw.h"

#include "WM_api.h"

static CLG_LogRef LOG = {"ed.image.undo"};

/* -------------------------------------------------------------------- */
/** \name Thread Locking
 * \{ */

/* This is a non-global static resource,
 * Maybe it should be exposed as part of the
 * paint operation, but for now just give a public interface */
static SpinLock paint_tiles_lock;

void ED_image_paint_tile_lock_init(void)
{
  BLI_spin_init(&paint_tiles_lock);
}

void ED_image_paint_tile_lock_end(void)
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

static ImBuf *imbuf_alloc_temp_tile(void)
{
  return IMB_allocImBuf(
      ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE, 32, IB_rectfloat | IB_rect);
}

typedef struct PaintTile {
  struct PaintTile *next, *prev;
  Image *image;
  ImBuf *ibuf;
  /* For 2D image painting the ImageUser uses most of the values.
   * Even though views and passes are stored they are currently not supported for painting.
   * For 3D projection painting this only uses a tile & frame number.
   * The scene pointer must be cleared (or temporarily set it as needed, but leave cleared). */
  ImageUser iuser;
  union {
    float *fp;
    uint *uint;
    void *pt;
  } rect;
  ushort *mask;
  bool valid;
  bool use_float;
  int x_tile, y_tile;
} PaintTile;

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

static void ptile_free_list(ListBase *paint_tiles)
{
  for (PaintTile *ptile = paint_tiles->first, *ptile_next; ptile; ptile = ptile_next) {
    ptile_next = ptile->next;
    ptile_free(ptile);
  }
  BLI_listbase_clear(paint_tiles);
}

static void ptile_invalidate_list(ListBase *paint_tiles)
{
  LISTBASE_FOREACH (PaintTile *, ptile, paint_tiles) {
    ptile->valid = false;
  }
}

void *ED_image_paint_tile_find(ListBase *paint_tiles,
                               Image *image,
                               ImBuf *ibuf,
                               ImageUser *iuser,
                               int x_tile,
                               int y_tile,
                               ushort **r_mask,
                               bool validate)
{
  LISTBASE_FOREACH (PaintTile *, ptile, paint_tiles) {
    if (ptile->x_tile == x_tile && ptile->y_tile == y_tile) {
      if (ptile->image == image && ptile->ibuf == ibuf && ptile->iuser.tile == iuser->tile) {
        if (r_mask) {
          /* allocate mask if requested. */
          if (!ptile->mask) {
            ptile->mask = MEM_callocN(sizeof(ushort) * square_i(ED_IMAGE_UNDO_TILE_SIZE),
                                      "UndoImageTile.mask");
          }
          *r_mask = ptile->mask;
        }
        if (validate) {
          ptile->valid = true;
        }
        return ptile->rect.pt;
      }
    }
  }
  return NULL;
}

void *ED_image_paint_tile_push(ListBase *paint_tiles,
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
  const bool has_float = (ibuf->rect_float != NULL);

  /* check if tile is already pushed */

  /* in projective painting we keep accounting of tiles, so if we need one pushed, just push! */
  if (find_prev) {
    void *data = ED_image_paint_tile_find(
        paint_tiles, image, ibuf, iuser, x_tile, y_tile, r_mask, true);
    if (data) {
      return data;
    }
  }

  if (*tmpibuf == NULL) {
    *tmpibuf = imbuf_alloc_temp_tile();
  }

  PaintTile *ptile = MEM_callocN(sizeof(PaintTile), "PaintTile");

  ptile->image = image;
  ptile->ibuf = ibuf;
  ptile->iuser = *iuser;
  ptile->iuser.scene = NULL;

  ptile->x_tile = x_tile;
  ptile->y_tile = y_tile;

  /* add mask explicitly here */
  if (r_mask) {
    *r_mask = ptile->mask = MEM_callocN(sizeof(ushort) * square_i(ED_IMAGE_UNDO_TILE_SIZE),
                                        "PaintTile.mask");
  }

  ptile->rect.pt = MEM_callocN((ibuf->rect_float ? sizeof(float[4]) : sizeof(char[4])) *
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
    SWAP(float *, ptile->rect.fp, (*tmpibuf)->rect_float);
  }
  else {
    SWAP(uint *, ptile->rect.uint, (*tmpibuf)->rect);
  }

  if (use_thread_lock) {
    BLI_spin_lock(&paint_tiles_lock);
  }
  BLI_addtail(paint_tiles, ptile);

  if (use_thread_lock) {
    BLI_spin_unlock(&paint_tiles_lock);
  }
  return ptile->rect.pt;
}

static void ptile_restore_runtime_list(ListBase *paint_tiles)
{
  ImBuf *tmpibuf = imbuf_alloc_temp_tile();

  LISTBASE_FOREACH (PaintTile *, ptile, paint_tiles) {
    Image *image = ptile->image;
    ImBuf *ibuf = BKE_image_acquire_ibuf(image, &ptile->iuser, NULL);
    const bool has_float = (ibuf->rect_float != NULL);

    if (has_float) {
      SWAP(float *, ptile->rect.fp, tmpibuf->rect_float);
    }
    else {
      SWAP(uint *, ptile->rect.uint, tmpibuf->rect);
    }

    IMB_rectcpy(ibuf,
                tmpibuf,
                ptile->x_tile * ED_IMAGE_UNDO_TILE_SIZE,
                ptile->y_tile * ED_IMAGE_UNDO_TILE_SIZE,
                0,
                0,
                ED_IMAGE_UNDO_TILE_SIZE,
                ED_IMAGE_UNDO_TILE_SIZE);

    if (has_float) {
      SWAP(float *, ptile->rect.fp, tmpibuf->rect_float);
    }
    else {
      SWAP(uint *, ptile->rect.uint, tmpibuf->rect);
    }

    GPU_free_image(image); /* force OpenGL reload (maybe partial update will operate better?) */
    if (ibuf->rect_float) {
      ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
    }
    if (ibuf->mipmap[0]) {
      ibuf->userflags |= IB_MIPMAP_INVALID; /* force mip-map recreation. */
    }
    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

    BKE_image_release_ibuf(image, ibuf, NULL);
  }

  IMB_freeImBuf(tmpibuf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Undo Tile
 * \{ */

static uint index_from_xy(uint tile_x, uint tile_y, const uint tiles_dims[2])
{
  BLI_assert(tile_x < tiles_dims[0] && tile_y < tiles_dims[1]);
  return (tile_y * tiles_dims[0]) + tile_x;
}

typedef struct UndoImageTile {
  union {
    float *fp;
    uint *uint;
    void *pt;
  } rect;
  int users;
} UndoImageTile;

static UndoImageTile *utile_alloc(bool has_float)
{
  UndoImageTile *utile = MEM_callocN(sizeof(*utile), "ImageUndoTile");
  if (has_float) {
    utile->rect.fp = MEM_mallocN(sizeof(float[4]) * square_i(ED_IMAGE_UNDO_TILE_SIZE), __func__);
  }
  else {
    utile->rect.uint = MEM_mallocN(sizeof(uint) * square_i(ED_IMAGE_UNDO_TILE_SIZE), __func__);
  }
  return utile;
}

static void utile_init_from_imbuf(
    UndoImageTile *utile, const uint x, const uint y, const ImBuf *ibuf, ImBuf *tmpibuf)
{
  const bool has_float = ibuf->rect_float;

  if (has_float) {
    SWAP(float *, utile->rect.fp, tmpibuf->rect_float);
  }
  else {
    SWAP(uint *, utile->rect.uint, tmpibuf->rect);
  }

  IMB_rectcpy(tmpibuf, ibuf, 0, 0, x, y, ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE);

  if (has_float) {
    SWAP(float *, utile->rect.fp, tmpibuf->rect_float);
  }
  else {
    SWAP(uint *, utile->rect.uint, tmpibuf->rect);
  }
}

static void utile_restore(
    const UndoImageTile *utile, const uint x, const uint y, ImBuf *ibuf, ImBuf *tmpibuf)
{
  const bool has_float = ibuf->rect_float;
  float *prev_rect_float = tmpibuf->rect_float;
  uint *prev_rect = tmpibuf->rect;

  if (has_float) {
    tmpibuf->rect_float = utile->rect.fp;
  }
  else {
    tmpibuf->rect = utile->rect.uint;
  }

  IMB_rectcpy(ibuf, tmpibuf, x, y, 0, 0, ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE);

  tmpibuf->rect_float = prev_rect_float;
  tmpibuf->rect = prev_rect;
}

static void utile_decref(UndoImageTile *utile)
{
  utile->users -= 1;
  BLI_assert(utile->users >= 0);
  if (utile->users == 0) {
    MEM_freeN(utile->rect.pt);
    MEM_freeN(utile);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Undo Buffer
 * \{ */

typedef struct UndoImageBuf {
  struct UndoImageBuf *next, *prev;

  /**
   * The buffer after the undo step has executed.
   */
  struct UndoImageBuf *post;

  char ibuf_name[IMB_FILENAME_SIZE];

  UndoImageTile **tiles;

  /** Can calculate these from dims, just for convenience. */
  uint tiles_len;
  uint tiles_dims[2];

  uint image_dims[2];

  /** Store variables from the image. */
  struct {
    short source;
    bool use_float;
    char gen_type;
  } image_state;

} UndoImageBuf;

static UndoImageBuf *ubuf_from_image_no_tiles(Image *image, const ImBuf *ibuf)
{
  UndoImageBuf *ubuf = MEM_callocN(sizeof(*ubuf), __func__);

  ubuf->image_dims[0] = ibuf->x;
  ubuf->image_dims[1] = ibuf->y;

  ubuf->tiles_dims[0] = ED_IMAGE_UNDO_TILE_NUMBER(ubuf->image_dims[0]);
  ubuf->tiles_dims[1] = ED_IMAGE_UNDO_TILE_NUMBER(ubuf->image_dims[1]);

  ubuf->tiles_len = ubuf->tiles_dims[0] * ubuf->tiles_dims[1];
  ubuf->tiles = MEM_callocN(sizeof(*ubuf->tiles) * ubuf->tiles_len, __func__);

  BLI_strncpy(ubuf->ibuf_name, ibuf->name, sizeof(ubuf->ibuf_name));
  ubuf->image_state.gen_type = image->gen_type;
  ubuf->image_state.source = image->source;
  ubuf->image_state.use_float = ibuf->rect_float != NULL;

  return ubuf;
}

static void ubuf_from_image_all_tiles(UndoImageBuf *ubuf, const ImBuf *ibuf)
{
  ImBuf *tmpibuf = imbuf_alloc_temp_tile();

  const bool has_float = ibuf->rect_float;
  int i = 0;
  for (uint y_tile = 0; y_tile < ubuf->tiles_dims[1]; y_tile += 1) {
    uint y = y_tile << ED_IMAGE_UNDO_TILE_BITS;
    for (uint x_tile = 0; x_tile < ubuf->tiles_dims[0]; x_tile += 1) {
      uint x = x_tile << ED_IMAGE_UNDO_TILE_BITS;

      BLI_assert(ubuf->tiles[i] == NULL);
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
  if ((ibuf->rect_float != NULL) && (ubuf->image_state.use_float == false)) {
    imb_freerectfloatImBuf(ibuf);
  }

  if (ibuf->x == ubuf->image_dims[0] && ibuf->y == ubuf->image_dims[1] &&
      (ubuf->image_state.use_float ? (void *)ibuf->rect_float : (void *)ibuf->rect)) {
    return;
  }

  imb_freerectImbuf_all(ibuf);
  IMB_rect_size_set(ibuf, ubuf->image_dims);

  if (ubuf->image_state.use_float) {
    imb_addrectfloatImBuf(ibuf);
  }
  else {
    imb_addrectImBuf(ibuf);
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

typedef struct UndoImageHandle {
  struct UndoImageHandle *next, *prev;

  /** Each undo handle refers to a single image which may have multiple buffers. */
  UndoRefID_Image image_ref;

  /** Each tile of a tiled image has its own UndoImageHandle.
   * The tile number of this IUser is used to distinguish them.
   */
  ImageUser iuser;

  /**
   * List of #UndoImageBuf's to support multiple buffers per image.
   */
  ListBase buffers;

} UndoImageHandle;

static void uhandle_restore_list(ListBase *undo_handles, bool use_init)
{
  ImBuf *tmpibuf = imbuf_alloc_temp_tile();

  LISTBASE_FOREACH (UndoImageHandle *, uh, undo_handles) {
    /* Tiles only added to second set of tiles. */
    Image *image = uh->image_ref.ptr;

    ImBuf *ibuf = BKE_image_acquire_ibuf(image, &uh->iuser, NULL);
    if (UNLIKELY(ibuf == NULL)) {
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
      GPU_free_image(image); /* force OpenGL reload */

      if (ibuf->rect_float) {
        ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
      }
      if (ibuf->mipmap[0]) {
        ibuf->userflags |= IB_MIPMAP_INVALID; /* force mip-map recreation. */
      }
      ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

      DEG_id_tag_update(&image->id, 0);
    }
    BKE_image_release_ibuf(image, ibuf, NULL);
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
                                         const Image *UNUSED(image),
                                         const char *ibuf_name)
{
  LISTBASE_FOREACH (UndoImageBuf *, ubuf, &uh->buffers) {
    if (STREQ(ubuf->ibuf_name, ibuf_name)) {
      return ubuf;
    }
  }
  return NULL;
}

static UndoImageBuf *uhandle_add_ubuf(UndoImageHandle *uh, Image *image, ImBuf *ibuf)
{
  BLI_assert(uhandle_lookup_ubuf(uh, image, ibuf->name) == NULL);
  UndoImageBuf *ubuf = ubuf_from_image_no_tiles(image, ibuf);
  BLI_addtail(&uh->buffers, ubuf);

  ubuf->post = NULL;

  return ubuf;
}

static UndoImageBuf *uhandle_ensure_ubuf(UndoImageHandle *uh, Image *image, ImBuf *ibuf)
{
  UndoImageBuf *ubuf = uhandle_lookup_ubuf(uh, image, ibuf->name);
  if (ubuf == NULL) {
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
  return NULL;
}

static UndoImageHandle *uhandle_lookup(ListBase *undo_handles, const Image *image, int tile_number)
{
  LISTBASE_FOREACH (UndoImageHandle *, uh, undo_handles) {
    if (image == uh->image_ref.ptr && uh->iuser.tile == tile_number) {
      return uh;
    }
  }
  return NULL;
}

static UndoImageHandle *uhandle_add(ListBase *undo_handles, Image *image, ImageUser *iuser)
{
  BLI_assert(uhandle_lookup(undo_handles, image, iuser->tile) == NULL);
  UndoImageHandle *uh = MEM_callocN(sizeof(*uh), __func__);
  uh->image_ref.ptr = image;
  uh->iuser = *iuser;
  uh->iuser.scene = NULL;
  uh->iuser.ok = 1;
  BLI_addtail(undo_handles, uh);
  return uh;
}

static UndoImageHandle *uhandle_ensure(ListBase *undo_handles, Image *image, ImageUser *iuser)
{
  UndoImageHandle *uh = uhandle_lookup(undo_handles, image, iuser->tile);
  if (uh == NULL) {
    uh = uhandle_add(undo_handles, image, iuser);
  }
  return uh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct ImageUndoStep {
  UndoStep step;

  /** #UndoImageHandle */
  ListBase handles;

  /**
   * #PaintTile
   * Run-time only data (active during a paint stroke).
   */
  ListBase paint_tiles;

  bool is_encode_init;
  ePaintMode paint_mode;

} ImageUndoStep;

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
  if (uh_prev != NULL) {
    UndoImageBuf *ubuf_reference = uhandle_lookup_ubuf(uh_prev, image, ubuf->ibuf_name);
    if (ubuf_reference) {
      ubuf_reference = ubuf_reference->post;
      if ((ubuf_reference->image_dims[0] == ubuf->image_dims[0]) &&
          (ubuf_reference->image_dims[1] == ubuf->image_dims[1])) {
        return ubuf_reference;
      }
    }
  }
  return NULL;
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

static void image_undosys_step_encode_init(struct bContext *UNUSED(C), UndoStep *us_p)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  /* dummy, memory is cleared anyway. */
  us->is_encode_init = true;
  BLI_listbase_clear(&us->handles);
  BLI_listbase_clear(&us->paint_tiles);
}

static bool image_undosys_step_encode(struct bContext *C,
                                      struct Main *UNUSED(bmain),
                                      UndoStep *us_p)
{
  /* Encoding is done along the way by adding tiles
   * to the current 'ImageUndoStep' added by encode_init.
   *
   * This function ensures there are previous and current states of the image in the undo buffer.
   */
  ImageUndoStep *us = (ImageUndoStep *)us_p;

  BLI_assert(us->step.data_size == 0);

  if (us->is_encode_init) {

    ImBuf *tmpibuf = imbuf_alloc_temp_tile();

    ImageUndoStep *us_reference = (ImageUndoStep *)ED_undo_stack_get()->step_active;
    while (us_reference && us_reference->step.type != BKE_UNDOSYS_TYPE_IMAGE) {
      us_reference = (ImageUndoStep *)us_reference->step.prev;
    }

    /* Initialize undo tiles from ptiles (if they exist). */
    for (PaintTile *ptile = us->paint_tiles.first, *ptile_next; ptile; ptile = ptile_next) {
      if (ptile->valid) {
        UndoImageHandle *uh = uhandle_ensure(&us->handles, ptile->image, &ptile->iuser);
        UndoImageBuf *ubuf_pre = uhandle_ensure_ubuf(uh, ptile->image, ptile->ibuf);

        UndoImageTile *utile = MEM_callocN(sizeof(*utile), "UndoImageTile");
        utile->users = 1;
        utile->rect.pt = ptile->rect.pt;
        ptile->rect.pt = NULL;
        const uint tile_index = index_from_xy(ptile->x_tile, ptile->y_tile, ubuf_pre->tiles_dims);

        BLI_assert(ubuf_pre->tiles[tile_index] == NULL);
        ubuf_pre->tiles[tile_index] = utile;
      }
      ptile_next = ptile->next;
      ptile_free(ptile);
    }
    BLI_listbase_clear(&us->paint_tiles);

    LISTBASE_FOREACH (UndoImageHandle *, uh, &us->handles) {
      LISTBASE_FOREACH (UndoImageBuf *, ubuf_pre, &uh->buffers) {

        ImBuf *ibuf = BKE_image_acquire_ibuf(uh->image_ref.ptr, &uh->iuser, NULL);

        const bool has_float = ibuf->rect_float;

        BLI_assert(ubuf_pre->post == NULL);
        ubuf_pre->post = ubuf_from_image_no_tiles(uh->image_ref.ptr, ibuf);
        UndoImageBuf *ubuf_post = ubuf_pre->post;

        if (ubuf_pre->image_dims[0] != ubuf_post->image_dims[0] ||
            ubuf_pre->image_dims[1] != ubuf_post->image_dims[1]) {
          ubuf_from_image_all_tiles(ubuf_post, ibuf);
        }
        else {
          /* Search for the previous buffer. */
          UndoImageBuf *ubuf_reference =
              (us_reference ? ubuf_lookup_from_reference(
                                  us_reference, uh->image_ref.ptr, uh->iuser.tile, ubuf_post) :
                              NULL);

          int i = 0;
          for (uint y_tile = 0; y_tile < ubuf_pre->tiles_dims[1]; y_tile += 1) {
            uint y = y_tile << ED_IMAGE_UNDO_TILE_BITS;
            for (uint x_tile = 0; x_tile < ubuf_pre->tiles_dims[0]; x_tile += 1) {
              uint x = x_tile << ED_IMAGE_UNDO_TILE_BITS;

              if ((ubuf_reference != NULL) && ((ubuf_pre->tiles[i] == NULL) ||
                                               /* In this case the paint stroke as has added a tile
                                                * which we have a duplicate reference available. */
                                               (ubuf_pre->tiles[i]->users == 1))) {
                if (ubuf_pre->tiles[i] != NULL) {
                  /* If we have a reference, re-use this single use tile for the post state. */
                  BLI_assert(ubuf_pre->tiles[i]->users == 1);
                  ubuf_post->tiles[i] = ubuf_pre->tiles[i];
                  ubuf_pre->tiles[i] = NULL;
                  utile_init_from_imbuf(ubuf_post->tiles[i], x, y, ibuf, tmpibuf);
                }
                else {
                  BLI_assert(ubuf_post->tiles[i] == NULL);
                  ubuf_post->tiles[i] = ubuf_reference->tiles[i];
                  ubuf_post->tiles[i]->users += 1;
                }
                BLI_assert(ubuf_pre->tiles[i] == NULL);
                ubuf_pre->tiles[i] = ubuf_reference->tiles[i];
                ubuf_pre->tiles[i]->users += 1;

                BLI_assert(ubuf_pre->tiles[i] != NULL);
                BLI_assert(ubuf_post->tiles[i] != NULL);
              }
              else {
                UndoImageTile *utile = utile_alloc(has_float);
                utile_init_from_imbuf(utile, x, y, ibuf, tmpibuf);

                if (ubuf_pre->tiles[i] != NULL) {
                  ubuf_post->tiles[i] = utile;
                  utile->users = 1;
                }
                else {
                  ubuf_pre->tiles[i] = utile;
                  ubuf_post->tiles[i] = utile;
                  utile->users = 2;
                }
              }
              BLI_assert(ubuf_pre->tiles[i] != NULL);
              BLI_assert(ubuf_post->tiles[i] != NULL);
              i += 1;
            }
          }
          BLI_assert(i == ubuf_pre->tiles_len);
          BLI_assert(i == ubuf_post->tiles_len);
        }
        BKE_image_release_ibuf(uh->image_ref.ptr, ibuf, NULL);
      }
    }

    IMB_freeImBuf(tmpibuf);

    /* Useful to debug tiles are stored correctly. */
    if (false) {
      uhandle_restore_list(&us->handles, false);
    }
  }
  else {
    /* Happens when switching modes. */
    ePaintMode paint_mode = BKE_paintmode_get_active_from_context(C);
    BLI_assert(ELEM(paint_mode, PAINT_MODE_TEXTURE_2D, PAINT_MODE_TEXTURE_3D));
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
  ImageUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = (ImageUndoStep *)us_iter->step.next;
  }
  while (us_iter != us || (!is_final && us_iter == us)) {

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
    struct bContext *C, struct Main *bmain, UndoStep *us_p, int dir, bool is_final)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  if (dir < 0) {
    image_undosys_step_decode_undo(us, is_final);
  }
  else {
    image_undosys_step_decode_redo(us);
  }

  if (us->paint_mode == PAINT_MODE_TEXTURE_3D) {
    ED_object_mode_set_ex(C, OB_MODE_TEXTURE_PAINT, false, NULL);
  }

  /* Refresh texture slots. */
  ED_editors_init_for_undo(bmain);
}

static void image_undosys_step_free(UndoStep *us_p)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  uhandle_free_list(&us->handles);

  /* Typically this list will have been cleared. */
  ptile_free_list(&us->paint_tiles);
}

static void image_undosys_foreach_ID_ref(UndoStep *us_p,
                                         UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                         void *user_data)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  LISTBASE_FOREACH (UndoImageHandle *, uh, &us->handles) {
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&uh->image_ref));
  }
}

/* Export for ED_undo_sys. */
void ED_image_undosys_type(UndoType *ut)
{
  ut->name = "Image";
  ut->poll = image_undosys_poll;
  ut->step_encode_init = image_undosys_step_encode_init;
  ut->step_encode = image_undosys_step_encode;
  ut->step_decode = image_undosys_step_decode;
  ut->step_free = image_undosys_step_free;

  ut->step_foreach_ID_ref = image_undosys_foreach_ID_ref;

  ut->use_context = true;

  ut->step_size = sizeof(ImageUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

ListBase *ED_image_paint_tile_list_get(void)
{
  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us_prev = ustack->step_init;
  UndoStep *us_p = BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_IMAGE);
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  /* We should always have an undo push started when accessing tiles,
   * not doing this means we won't have paint_mode correctly set. */
  BLI_assert(us_p == us_prev);
  if (us_p != us_prev) {
    /* Fallback value until we can be sure this never happens. */
    us->paint_mode = PAINT_MODE_TEXTURE_2D;
  }
  return &us->paint_tiles;
}

/* restore painting image to previous state. Used for anchored and drag-dot style brushes*/
void ED_image_undo_restore(UndoStep *us)
{
  ListBase *paint_tiles = &((ImageUndoStep *)us)->paint_tiles;
  ptile_restore_runtime_list(paint_tiles);
  ptile_invalidate_list(paint_tiles);
}

static ImageUndoStep *image_undo_push_begin(const char *name, int paint_mode)
{
  UndoStack *ustack = ED_undo_stack_get();
  bContext *C = NULL; /* special case, we never read from this. */
  UndoStep *us_p = BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_IMAGE);
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  BLI_assert(ELEM(paint_mode, PAINT_MODE_TEXTURE_2D, PAINT_MODE_TEXTURE_3D));
  us->paint_mode = paint_mode;
  return us;
}

void ED_image_undo_push_begin(const char *name, int paint_mode)
{
  image_undo_push_begin(name, paint_mode);
}

void ED_image_undo_push_begin_with_image(const char *name,
                                         Image *image,
                                         ImBuf *ibuf,
                                         ImageUser *iuser)
{
  ImageUndoStep *us = image_undo_push_begin(name, PAINT_MODE_TEXTURE_2D);

  BLI_assert(BKE_image_get_tile(image, iuser->tile));
  UndoImageHandle *uh = uhandle_ensure(&us->handles, image, iuser);
  UndoImageBuf *ubuf_pre = uhandle_ensure_ubuf(uh, image, ibuf);
  BLI_assert(ubuf_pre->post == NULL);

  ImageUndoStep *us_reference = (ImageUndoStep *)ED_undo_stack_get()->step_active;
  while (us_reference && us_reference->step.type != BKE_UNDOSYS_TYPE_IMAGE) {
    us_reference = (ImageUndoStep *)us_reference->step.prev;
  }
  UndoImageBuf *ubuf_reference = (us_reference ? ubuf_lookup_from_reference(
                                                     us_reference, image, iuser->tile, ubuf_pre) :
                                                 NULL);

  if (ubuf_reference) {
    memcpy(ubuf_pre->tiles, ubuf_reference->tiles, sizeof(*ubuf_pre->tiles) * ubuf_pre->tiles_len);
    for (uint i = 0; i < ubuf_pre->tiles_len; i++) {
      UndoImageTile *utile = ubuf_pre->tiles[i];
      utile->users += 1;
    }
  }
  else {
    ubuf_from_image_all_tiles(ubuf_pre, ibuf);
  }
}

void ED_image_undo_push_end(void)
{
  UndoStack *ustack = ED_undo_stack_get();
  BKE_undosys_step_push(ustack, NULL, NULL);
  BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
  WM_file_tag_modified();
}

/** \} */
