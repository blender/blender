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
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_threads.h"

#include "DNA_image_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_paint.h"
#include "ED_undo.h"
#include "ED_util.h"
#include "ED_object.h"

#include "GPU_draw.h"

#include "paint_intern.h"

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoImageTile {
  struct UndoImageTile *next, *prev;

  char ibufname[IMB_FILENAME_SIZE];

  union {
    float *fp;
    unsigned int *uint;
    void *pt;
  } rect;

  unsigned short *mask;

  int x, y;

  /* TODO(campbell): avoid storing the ID per tile,
   * adds unnecessary overhead restoring undo steps when most tiles share the same image. */
  UndoRefID_Image image_ref;

  short source, use_float;
  char gen_type;
  bool valid;

  size_t undo_size;
} UndoImageTile;

/* this is a static resource for non-globality,
 * Maybe it should be exposed as part of the
 * paint operation, but for now just give a public interface */
static SpinLock undolock;

void image_undo_init_locks(void)
{
  BLI_spin_init(&undolock);
}

void image_undo_end_locks(void)
{
  BLI_spin_end(&undolock);
}

/* UNDO */
typedef enum {
  COPY = 0,
  RESTORE = 1,
  RESTORE_COPY = 2,
} CopyMode;

static void undo_copy_tile(UndoImageTile *tile, ImBuf *tmpibuf, ImBuf *ibuf, CopyMode mode)
{
  if (mode == COPY) {
    /* copy or swap contents of tile->rect and region in ibuf->rect */
    IMB_rectcpy(tmpibuf,
                ibuf,
                0,
                0,
                tile->x * IMAPAINT_TILE_SIZE,
                tile->y * IMAPAINT_TILE_SIZE,
                IMAPAINT_TILE_SIZE,
                IMAPAINT_TILE_SIZE);

    if (ibuf->rect_float) {
      SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
    }
    else {
      SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
    }
  }
  else {
    if (mode == RESTORE_COPY) {
      IMB_rectcpy(tmpibuf,
                  ibuf,
                  0,
                  0,
                  tile->x * IMAPAINT_TILE_SIZE,
                  tile->y * IMAPAINT_TILE_SIZE,
                  IMAPAINT_TILE_SIZE,
                  IMAPAINT_TILE_SIZE);
    }
    /* swap to the tmpbuf for easy copying */
    if (ibuf->rect_float) {
      SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
    }
    else {
      SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
    }

    IMB_rectcpy(ibuf,
                tmpibuf,
                tile->x * IMAPAINT_TILE_SIZE,
                tile->y * IMAPAINT_TILE_SIZE,
                0,
                0,
                IMAPAINT_TILE_SIZE,
                IMAPAINT_TILE_SIZE);

    if (mode == RESTORE) {
      if (ibuf->rect_float) {
        SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
      }
      else {
        SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
      }
    }
  }
}

void *image_undo_find_tile(ListBase *undo_tiles,
                           Image *ima,
                           ImBuf *ibuf,
                           int x_tile,
                           int y_tile,
                           unsigned short **mask,
                           bool validate)
{
  UndoImageTile *tile;
  short use_float = ibuf->rect_float ? 1 : 0;

  for (tile = undo_tiles->first; tile; tile = tile->next) {
    if (tile->x == x_tile && tile->y == y_tile && ima->gen_type == tile->gen_type &&
        ima->source == tile->source) {
      if (tile->use_float == use_float) {
        if (STREQ(tile->ibufname, ibuf->name)) {
          if (mask) {
            /* allocate mask if requested */
            if (!tile->mask) {
              tile->mask = MEM_callocN(sizeof(unsigned short) * IMAPAINT_TILE_SIZE *
                                           IMAPAINT_TILE_SIZE,
                                       "UndoImageTile.mask");
            }

            *mask = tile->mask;
          }
          if (validate) {
            tile->valid = true;
          }
          return tile->rect.pt;
        }
      }
    }
  }

  return NULL;
}

void *image_undo_push_tile(ListBase *undo_tiles,
                           Image *ima,
                           ImBuf *ibuf,
                           ImBuf **tmpibuf,
                           int x_tile,
                           int y_tile,
                           unsigned short **mask,
                           bool **valid,
                           bool proj,
                           bool find_prev)
{
  UndoImageTile *tile;
  int allocsize;
  short use_float = ibuf->rect_float ? 1 : 0;
  void *data;

  /* check if tile is already pushed */

  /* in projective painting we keep accounting of tiles, so if we need one pushed, just push! */
  if (find_prev) {
    data = image_undo_find_tile(undo_tiles, ima, ibuf, x_tile, y_tile, mask, true);
    if (data) {
      return data;
    }
  }

  if (*tmpibuf == NULL) {
    *tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, IB_rectfloat | IB_rect);
  }

  tile = MEM_callocN(sizeof(UndoImageTile), "UndoImageTile");
  tile->x = x_tile;
  tile->y = y_tile;

  /* add mask explicitly here */
  if (mask) {
    *mask = tile->mask = MEM_callocN(
        sizeof(unsigned short) * IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE, "UndoImageTile.mask");
  }
  allocsize = IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE * 4;
  allocsize *= (ibuf->rect_float) ? sizeof(float) : sizeof(char);
  tile->rect.pt = MEM_mapallocN(allocsize, "UndeImageTile.rect");

  BLI_strncpy(tile->ibufname, ibuf->name, sizeof(tile->ibufname));

  tile->gen_type = ima->gen_type;
  tile->source = ima->source;
  tile->use_float = use_float;
  tile->valid = true;
  tile->image_ref.ptr = ima;

  if (valid) {
    *valid = &tile->valid;
  }
  undo_copy_tile(tile, *tmpibuf, ibuf, COPY);

  if (proj) {
    BLI_spin_lock(&undolock);
  }
  BLI_addtail(undo_tiles, tile);

  if (proj) {
    BLI_spin_unlock(&undolock);
  }
  return tile->rect.pt;
}

void image_undo_remove_masks(void)
{
  ListBase *undo_tiles = ED_image_undo_get_tiles();
  UndoImageTile *tile;

  for (tile = undo_tiles->first; tile; tile = tile->next) {
    if (tile->mask) {
      MEM_freeN(tile->mask);
      tile->mask = NULL;
    }
  }
}

static void image_undo_restore_runtime(ListBase *lb)
{
  ImBuf *ibuf, *tmpibuf;
  UndoImageTile *tile;

  tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, IB_rectfloat | IB_rect);

  for (tile = lb->first; tile; tile = tile->next) {
    Image *ima = tile->image_ref.ptr;
    ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

    undo_copy_tile(tile, tmpibuf, ibuf, RESTORE);

    GPU_free_image(ima); /* force OpenGL reload (maybe partial update will operate better?) */
    if (ibuf->rect_float) {
      ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
    }
    if (ibuf->mipmap[0]) {
      ibuf->userflags |= IB_MIPMAP_INVALID; /* force mipmap recreatiom */
    }
    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

    BKE_image_release_ibuf(ima, ibuf, NULL);
  }

  IMB_freeImBuf(tmpibuf);
}

static void image_undo_restore_list(ListBase *lb)
{
  ImBuf *tmpibuf = IMB_allocImBuf(
      IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, IB_rectfloat | IB_rect);

  for (UndoImageTile *tile = lb->first; tile; tile = tile->next) {
    short use_float;

    Image *ima = tile->image_ref.ptr;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

    if (ima && ibuf && !STREQ(tile->ibufname, ibuf->name)) {
      /* current ImBuf filename was changed, probably current frame
       * was changed when painting on image sequence, rather than storing
       * full image user (which isn't so obvious, btw) try to find ImBuf with
       * matched file name in list of already loaded images */

      BKE_image_release_ibuf(ima, ibuf, NULL);

      ibuf = BKE_image_get_ibuf_with_name(ima, tile->ibufname);
    }

    if (!ima || !ibuf || !(ibuf->rect || ibuf->rect_float)) {
      BKE_image_release_ibuf(ima, ibuf, NULL);
      continue;
    }

    if (ima->gen_type != tile->gen_type || ima->source != tile->source) {
      BKE_image_release_ibuf(ima, ibuf, NULL);
      continue;
    }

    use_float = ibuf->rect_float ? 1 : 0;

    if (use_float != tile->use_float) {
      BKE_image_release_ibuf(ima, ibuf, NULL);
      continue;
    }

    undo_copy_tile(tile, tmpibuf, ibuf, RESTORE_COPY);

    BKE_image_mark_dirty(ima, ibuf);
    GPU_free_image(ima); /* force OpenGL reload */

    if (ibuf->rect_float) {
      ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
    }
    if (ibuf->mipmap[0]) {
      ibuf->userflags |= IB_MIPMAP_INVALID; /* force mipmap recreatiom */
    }
    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

    DEG_id_tag_update(&ima->id, 0);

    BKE_image_release_ibuf(ima, ibuf, NULL);
  }

  IMB_freeImBuf(tmpibuf);
}

static void image_undo_free_list(ListBase *lb)
{
  for (UndoImageTile *tile = lb->first, *tile_next; tile; tile = tile_next) {
    tile_next = tile->next;
    MEM_freeN(tile->rect.pt);
    MEM_freeN(tile);
  }
}

static void image_undo_invalidate(void)
{
  UndoImageTile *tile;
  ListBase *lb = ED_image_undo_get_tiles();

  for (tile = lb->first; tile; tile = tile->next) {
    tile->valid = false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct ImageUndoStep {
  UndoStep step;
  ListBase tiles;
  bool is_encode_init;
  ePaintMode paint_mode;
} ImageUndoStep;

static bool image_undosys_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  ScrArea *sa = CTX_wm_area(C);
  if (sa && (sa->spacetype == SPACE_IMAGE)) {
    SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
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
  BLI_listbase_clear(&us->tiles);
}

static bool image_undosys_step_encode(struct bContext *C,
                                      struct Main *UNUSED(bmain),
                                      UndoStep *us_p)
{
  /* dummy, encoding is done along the way by adding tiles
   * to the current 'ImageUndoStep' added by encode_init. */
  ImageUndoStep *us = (ImageUndoStep *)us_p;

  BLI_assert(us->step.data_size == 0);

  int allocsize = IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE * 4;

  if (us->is_encode_init) {
    /* first dispose of invalid tiles (may happen due to drag dot for instance) */
    for (UndoImageTile *tile = us->tiles.first; tile;) {
      if (!tile->valid) {
        UndoImageTile *tmp_tile = tile->next;
        MEM_freeN(tile->rect.pt);
        BLI_freelinkN(&us->tiles, tile);
        tile = tmp_tile;
      }
      else {
        us->step.data_size += allocsize * ((tile->use_float) ? sizeof(float) : sizeof(char));
        tile = tile->next;
      }
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

static void image_undosys_step_decode_undo_impl(ImageUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);
  image_undo_restore_list(&us->tiles);
  us->step.is_applied = false;
}

static void image_undosys_step_decode_redo_impl(ImageUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  image_undo_restore_list(&us->tiles);
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
    image_undosys_step_decode_undo_impl(us_iter);
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
    ED_object_mode_set(C, OB_MODE_TEXTURE_PAINT);
  }

  /* Refresh texture slots. */
  ED_editors_init_for_undo(bmain);
}

static void image_undosys_step_free(UndoStep *us_p)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  image_undo_free_list(&us->tiles);
}

static void image_undosys_foreach_ID_ref(UndoStep *us_p,
                                         UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                         void *user_data)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  for (UndoImageTile *tile = us->tiles.first; tile; tile = tile->next) {
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&tile->image_ref));
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

ListBase *ED_image_undosys_step_get_tiles(UndoStep *us_p)
{
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  return &us->tiles;
}

ListBase *ED_image_undo_get_tiles(void)
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
  return ED_image_undosys_step_get_tiles(us_p);
}

/* restore painting image to previous state. Used for anchored and drag-dot style brushes*/
void ED_image_undo_restore(UndoStep *us)
{
  ListBase *lb = ED_image_undosys_step_get_tiles(us);
  image_undo_restore_runtime(lb);
  image_undo_invalidate();
}

void ED_image_undo_push_begin(const char *name, int paint_mode)
{
  UndoStack *ustack = ED_undo_stack_get();
  bContext *C = NULL; /* special case, we never read from this. */
  UndoStep *us_p = BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_IMAGE);
  ImageUndoStep *us = (ImageUndoStep *)us_p;
  BLI_assert(ELEM(paint_mode, PAINT_MODE_TEXTURE_2D, PAINT_MODE_TEXTURE_3D));
  us->paint_mode = paint_mode;
}

void ED_image_undo_push_end(void)
{
  UndoStack *ustack = ED_undo_stack_get();
  BKE_undosys_step_push(ustack, NULL, NULL);
}

/** \} */
