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
 * \ingroup blenloader
 */

/**
 *
 * FILE FORMAT
 * ===========
 *
 * IFF-style structure (but not IFF compatible!)
 *
 * Start file:
 * <pre>
 * `BLENDER_V100`  `12` bytes  (version 1.00 is just an example).
 *                 `V` = big endian, `v` = little endian.
 *                 `_` = 4 byte pointer, `-` = 8 byte pointer.
 * </pre>
 *
 * data-blocks: (also see struct #BHead).
 * <pre>
 * `bh.code`       `char[4]` see `BLO_blend_defs.h` for a list of known types.
 * `bh.len`        `int32` length data after #BHead in bytes.
 * `bh.old`        `void *` old pointer (the address at the time of writing the file).
 * `bh.SDNAnr`     `int32` struct index of structs stored in #DNA1 data.
 * `bh.nr`         `int32` in case of array: number of structs.
 * data
 * ...
 * ...
 * </pre>
 *
 * Almost all data in Blender are structures. Each struct saved
 * gets a BHead header.  With BHead the struct can be linked again
 * and compared with #StructDNA.

 * WRITE
 * =====
 *
 * Preferred writing order: (not really a must, but why would you do it random?)
 * Any case: direct data is ALWAYS after the lib block.
 *
 * (Local file data)
 * - for each LibBlock
 *   - write LibBlock
 *   - write associated direct data
 * (External file data)
 * - per library
 *   - write library block
 *   - per LibBlock
 *     - write the ID of LibBlock
 * - write #TEST (#RenderInfo struct. 128x128 blend file preview is optional).
 * - write #GLOB (#FileGlobal struct) (some global vars).
 * - write #DNA1 (#SDNA struct)
 * - write #USER (#UserDef struct) if filename is ``~/.config/blender/X.XX/config/startup.blend``.
 */

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h>
#  include <zlib.h> /* odd include order-issue */
#else
#  include <unistd.h> /* FreeBSD, for write() and close(). */
#endif

#include "BLI_utildefines.h"

/* allow writefile to use deprecated functionality (for forward compatibility code) */
#define DNA_DEPRECATED_ALLOW

#include "DNA_collection_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_mempool.h"
#include "MEM_guardedalloc.h" /* MEM_freeN */

#include "BKE_blender_version.h"
#include "BKE_bpath.h"
#include "BKE_global.h" /* for G */
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_workspace.h"

#include "BLO_blend_defs.h"
#include "BLO_blend_validate.h"
#include "BLO_read_write.h"
#include "BLO_readfile.h"
#include "BLO_undofile.h"
#include "BLO_writefile.h"

#include "readfile.h"

#include <errno.h>

/* Make preferences read-only. */
#define U (*((const UserDef *)&U))

/* ********* my write, buffered writing with minimum size chunks ************ */

/* Use optimal allocation since blocks of this size are kept in memory for undo. */
#define MYWRITE_BUFFER_SIZE (MEM_SIZE_OPTIMAL(1 << 17)) /* 128kb */
#define MYWRITE_MAX_CHUNK (MEM_SIZE_OPTIMAL(1 << 15))   /* ~32kb */

/** Use if we want to store how many bytes have been written to the file. */
// #define USE_WRITE_DATA_LEN

/* -------------------------------------------------------------------- */
/** \name Internal Write Wrapper's (Abstracts Compression)
 * \{ */

typedef enum {
  WW_WRAP_NONE = 1,
  WW_WRAP_ZLIB,
} eWriteWrapType;

typedef struct WriteWrap WriteWrap;
struct WriteWrap {
  /* callbacks */
  bool (*open)(WriteWrap *ww, const char *filepath);
  bool (*close)(WriteWrap *ww);
  size_t (*write)(WriteWrap *ww, const char *data, size_t data_len);

  /* Buffer output (we only want when output isn't already buffered). */
  bool use_buf;

  /* internal */
  union {
    int file_handle;
    gzFile gz_handle;
  } _user_data;
};

/* none */
#define FILE_HANDLE(ww) (ww)->_user_data.file_handle

static bool ww_open_none(WriteWrap *ww, const char *filepath)
{
  int file;

  file = BLI_open(filepath, O_BINARY + O_WRONLY + O_CREAT + O_TRUNC, 0666);

  if (file != -1) {
    FILE_HANDLE(ww) = file;
    return true;
  }

  return false;
}
static bool ww_close_none(WriteWrap *ww)
{
  return (close(FILE_HANDLE(ww)) != -1);
}
static size_t ww_write_none(WriteWrap *ww, const char *buf, size_t buf_len)
{
  return write(FILE_HANDLE(ww), buf, buf_len);
}
#undef FILE_HANDLE

/* zlib */
#define FILE_HANDLE(ww) (ww)->_user_data.gz_handle

static bool ww_open_zlib(WriteWrap *ww, const char *filepath)
{
  gzFile file;

  file = BLI_gzopen(filepath, "wb1");

  if (file != Z_NULL) {
    FILE_HANDLE(ww) = file;
    return true;
  }

  return false;
}
static bool ww_close_zlib(WriteWrap *ww)
{
  return (gzclose(FILE_HANDLE(ww)) == Z_OK);
}
static size_t ww_write_zlib(WriteWrap *ww, const char *buf, size_t buf_len)
{
  return gzwrite(FILE_HANDLE(ww), buf, buf_len);
}
#undef FILE_HANDLE

/* --- end compression types --- */

static void ww_handle_init(eWriteWrapType ww_type, WriteWrap *r_ww)
{
  memset(r_ww, 0, sizeof(*r_ww));

  switch (ww_type) {
    case WW_WRAP_ZLIB: {
      r_ww->open = ww_open_zlib;
      r_ww->close = ww_close_zlib;
      r_ww->write = ww_write_zlib;
      r_ww->use_buf = false;
      break;
    }
    default: {
      r_ww->open = ww_open_none;
      r_ww->close = ww_close_none;
      r_ww->write = ww_write_none;
      r_ww->use_buf = true;
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write Data Type & Functions
 * \{ */

typedef struct {
  const struct SDNA *sdna;

  /** Use for file and memory writing (fixed size of #MYWRITE_BUFFER_SIZE). */
  uchar *buf;
  /** Number of bytes used in #WriteData.buf (flushed when exceeded). */
  size_t buf_used_len;

#ifdef USE_WRITE_DATA_LEN
  /** Total number of bytes written. */
  size_t write_len;
#endif

  /** Set on unlikely case of an error (ignores further file writing).  */
  bool error;

  /** #MemFile writing (used for undo). */
  MemFileWriteData mem;
  /** When true, write to #WriteData.current, could also call 'is_undo'. */
  bool use_memfile;

  /**
   * Wrap writing, so we can use zlib or
   * other compression types later, see: G_FILE_COMPRESS
   * Will be NULL for UNDO.
   */
  WriteWrap *ww;
} WriteData;

typedef struct BlendWriter {
  WriteData *wd;
} BlendWriter;

static WriteData *writedata_new(WriteWrap *ww)
{
  WriteData *wd = MEM_callocN(sizeof(*wd), "writedata");

  wd->sdna = DNA_sdna_current_get();

  wd->ww = ww;

  if ((ww == NULL) || (ww->use_buf)) {
    wd->buf = MEM_mallocN(MYWRITE_BUFFER_SIZE, "wd->buf");
  }

  return wd;
}

static void writedata_do_write(WriteData *wd, const void *mem, size_t memlen)
{
  if ((wd == NULL) || wd->error || (mem == NULL) || memlen < 1) {
    return;
  }

  if (memlen > INT_MAX) {
    BLI_assert(!"Cannot write chunks bigger than INT_MAX.");
    return;
  }

  if (UNLIKELY(wd->error)) {
    return;
  }

  /* memory based save */
  if (wd->use_memfile) {
    BLO_memfile_chunk_add(&wd->mem, mem, memlen);
  }
  else {
    if (wd->ww->write(wd->ww, mem, memlen) != memlen) {
      wd->error = true;
    }
  }
}

static void writedata_free(WriteData *wd)
{
  if (wd->buf) {
    MEM_freeN(wd->buf);
  }
  MEM_freeN(wd);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Writing API 'mywrite'
 * \{ */

/**
 * Flush helps the de-duplicating memory for undo-save by logically segmenting data,
 * so differences in one part of memory won't cause unrelated data to be duplicated.
 */
static void mywrite_flush(WriteData *wd)
{
  if (wd->buf_used_len != 0) {
    writedata_do_write(wd, wd->buf, wd->buf_used_len);
    wd->buf_used_len = 0;
  }
}

/**
 * Low level WRITE(2) wrapper that buffers data
 * \param adr: Pointer to new chunk of data
 * \param len: Length of new chunk of data
 */
static void mywrite(WriteData *wd, const void *adr, size_t len)
{
  if (UNLIKELY(wd->error)) {
    return;
  }

  if (UNLIKELY(adr == NULL)) {
    BLI_assert(0);
    return;
  }

#ifdef USE_WRITE_DATA_LEN
  wd->write_len += len;
#endif

  if (wd->buf == NULL) {
    writedata_do_write(wd, adr, len);
  }
  else {
    /* if we have a single big chunk, write existing data in
     * buffer and write out big chunk in smaller pieces */
    if (len > MYWRITE_MAX_CHUNK) {
      if (wd->buf_used_len != 0) {
        writedata_do_write(wd, wd->buf, wd->buf_used_len);
        wd->buf_used_len = 0;
      }

      do {
        size_t writelen = MIN2(len, MYWRITE_MAX_CHUNK);
        writedata_do_write(wd, adr, writelen);
        adr = (const char *)adr + writelen;
        len -= writelen;
      } while (len > 0);

      return;
    }

    /* if data would overflow buffer, write out the buffer */
    if (len + wd->buf_used_len > MYWRITE_BUFFER_SIZE - 1) {
      writedata_do_write(wd, wd->buf, wd->buf_used_len);
      wd->buf_used_len = 0;
    }

    /* append data at end of buffer */
    memcpy(&wd->buf[wd->buf_used_len], adr, len);
    wd->buf_used_len += len;
  }
}

/**
 * BeGiN initializer for mywrite
 * \param ww: File write wrapper.
 * \param compare: Previous memory file (can be NULL).
 * \param current: The current memory file (can be NULL).
 * \warning Talks to other functions with global parameters
 */
static WriteData *mywrite_begin(WriteWrap *ww, MemFile *compare, MemFile *current)
{
  WriteData *wd = writedata_new(ww);

  if (current != NULL) {
    BLO_memfile_write_init(&wd->mem, current, compare);
    wd->use_memfile = true;
  }

  return wd;
}

/**
 * END the mywrite wrapper
 * \return 1 if write failed
 * \return unknown global variable otherwise
 * \warning Talks to other functions with global parameters
 */
static bool mywrite_end(WriteData *wd)
{
  if (wd->buf_used_len != 0) {
    writedata_do_write(wd, wd->buf, wd->buf_used_len);
    wd->buf_used_len = 0;
  }

  if (wd->use_memfile) {
    BLO_memfile_write_finalize(&wd->mem);
  }

  const bool err = wd->error;
  writedata_free(wd);

  return err;
}

/**
 * Start writing of data related to a single ID.
 *
 * Only does something when storing an undo step.
 */
static void mywrite_id_begin(WriteData *wd, ID *id)
{
  if (wd->use_memfile) {
    wd->mem.current_id_session_uuid = id->session_uuid;

    /* If current next memchunk does not match the ID we are about to write, try to find the
     * correct memchunk in the mapping using ID's session_uuid. */
    if (wd->mem.id_session_uuid_mapping != NULL &&
        (wd->mem.reference_current_chunk == NULL ||
         wd->mem.reference_current_chunk->id_session_uuid != id->session_uuid)) {
      void *ref = BLI_ghash_lookup(wd->mem.id_session_uuid_mapping,
                                   POINTER_FROM_UINT(id->session_uuid));
      if (ref != NULL) {
        wd->mem.reference_current_chunk = ref;
      }
      /* Else, no existing memchunk found, i.e. this is supposed to be a new ID. */
    }
    /* Otherwise, we try with the current memchunk in any case, whether it is matching current
     * ID's session_uuid or not. */
  }
}

/**
 * Start writing of data related to a single ID.
 *
 * Only does something when storing an undo step.
 */
static void mywrite_id_end(WriteData *wd, ID *UNUSED(id))
{
  if (wd->use_memfile) {
    /* Very important to do it after every ID write now, otherwise we cannot know whether a
     * specific ID changed or not. */
    mywrite_flush(wd);
    wd->mem.current_id_session_uuid = MAIN_ID_SESSION_UUID_UNSET;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic DNA File Writing
 * \{ */

static void writestruct_at_address_nr(
    WriteData *wd, int filecode, const int struct_nr, int nr, const void *adr, const void *data)
{
  BHead bh;

  BLI_assert(struct_nr > 0 && struct_nr < SDNA_TYPE_MAX);

  if (adr == NULL || data == NULL || nr == 0) {
    return;
  }

  /* init BHead */
  bh.code = filecode;
  bh.old = adr;
  bh.nr = nr;

  bh.SDNAnr = struct_nr;
  const SDNA_Struct *struct_info = wd->sdna->structs[bh.SDNAnr];

  bh.len = nr * wd->sdna->types_size[struct_info->type];

  if (bh.len == 0) {
    return;
  }

  mywrite(wd, &bh, sizeof(BHead));
  mywrite(wd, data, (size_t)bh.len);
}

static void writestruct_nr(
    WriteData *wd, int filecode, const int struct_nr, int nr, const void *adr)
{
  writestruct_at_address_nr(wd, filecode, struct_nr, nr, adr, adr);
}

/* do not use for structs */
static void writedata(WriteData *wd, int filecode, size_t len, const void *adr)
{
  BHead bh;

  if (adr == NULL || len == 0) {
    return;
  }

  if (len > INT_MAX) {
    BLI_assert(!"Cannot write chunks bigger than INT_MAX.");
    return;
  }

  /* align to 4 (writes uninitialized bytes in some cases) */
  len = (len + 3) & ~((size_t)3);

  /* init BHead */
  bh.code = filecode;
  bh.old = adr;
  bh.nr = 1;
  bh.SDNAnr = 0;
  bh.len = (int)len;

  mywrite(wd, &bh, sizeof(BHead));
  mywrite(wd, adr, len);
}

/* use this to force writing of lists in same order as reading (using link_list) */
static void writelist_nr(WriteData *wd, int filecode, const int struct_nr, const ListBase *lb)
{
  const Link *link = lb->first;

  while (link) {
    writestruct_nr(wd, filecode, struct_nr, 1, link);
    link = link->next;
  }
}

#if 0
static void writelist_id(WriteData *wd, int filecode, const char *structname, const ListBase *lb)
{
  const Link *link = lb->first;
  if (link) {

    const int struct_nr = DNA_struct_find_nr(wd->sdna, structname);
    if (struct_nr == -1) {
      printf("error: can't find SDNA code <%s>\n", structname);
      return;
    }

    while (link) {
      writestruct_nr(wd, filecode, struct_nr, 1, link);
      link = link->next;
    }
  }
}
#endif

#define writestruct_at_address(wd, filecode, struct_id, nr, adr, data) \
  writestruct_at_address_nr(wd, filecode, SDNA_TYPE_FROM_STRUCT(struct_id), nr, adr, data)

#define writestruct(wd, filecode, struct_id, nr, adr) \
  writestruct_nr(wd, filecode, SDNA_TYPE_FROM_STRUCT(struct_id), nr, adr)

#define writelist(wd, filecode, struct_id, lb) \
  writelist_nr(wd, filecode, SDNA_TYPE_FROM_STRUCT(struct_id), lb)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Typed DNA File Writing
 *
 * These functions are used by blender's .blend system for file saving/loading.
 * \{ */

/**
 * Take care using 'use_active_win', since we wont want the currently active window
 * to change which scene renders (currently only used for undo).
 */
static void current_screen_compat(Main *mainvar,
                                  bool use_active_win,
                                  bScreen **r_screen,
                                  Scene **r_scene,
                                  ViewLayer **r_view_layer)
{
  wmWindowManager *wm;
  wmWindow *window = NULL;

  /* find a global current screen in the first open window, to have
   * a reasonable default for reading in older versions */
  wm = mainvar->wm.first;

  if (wm) {
    if (use_active_win) {
      /* write the active window into the file, needed for multi-window undo T43424 */
      for (window = wm->windows.first; window; window = window->next) {
        if (window->active) {
          break;
        }
      }

      /* fallback */
      if (window == NULL) {
        window = wm->windows.first;
      }
    }
    else {
      window = wm->windows.first;
    }
  }

  *r_screen = (window) ? BKE_workspace_active_screen_get(window->workspace_hook) : NULL;
  *r_scene = (window) ? window->scene : NULL;
  *r_view_layer = (window && *r_scene) ? BKE_view_layer_find(*r_scene, window->view_layer_name) :
                                         NULL;
}

typedef struct RenderInfo {
  int sfra;
  int efra;
  char scene_name[MAX_ID_NAME - 2];
} RenderInfo;

/**
 * This was originally added for the historic render-daemon feature,
 * now write because it can be easily extracted without reading the whole blend file.
 *
 * See: `release/scripts/modules/blend_render_info.py`
 */
static void write_renderinfo(WriteData *wd, Main *mainvar)
{
  bScreen *curscreen;
  Scene *curscene = NULL;
  ViewLayer *view_layer;

  /* XXX in future, handle multiple windows with multiple screens? */
  current_screen_compat(mainvar, false, &curscreen, &curscene, &view_layer);

  LISTBASE_FOREACH (Scene *, sce, &mainvar->scenes) {
    if (sce->id.lib == NULL && (sce == curscene || (sce->r.scemode & R_BG_RENDER))) {
      RenderInfo data;
      data.sfra = sce->r.sfra;
      data.efra = sce->r.efra;
      memset(data.scene_name, 0, sizeof(data.scene_name));

      BLI_strncpy(data.scene_name, sce->id.name + 2, sizeof(data.scene_name));

      writedata(wd, REND, sizeof(data), &data);
    }
  }
}

static void write_keymapitem(BlendWriter *writer, const wmKeyMapItem *kmi)
{
  BLO_write_struct(writer, wmKeyMapItem, kmi);
  if (kmi->properties) {
    IDP_BlendWrite(writer, kmi->properties);
  }
}

static void write_userdef(BlendWriter *writer, const UserDef *userdef)
{
  writestruct(writer->wd, USER, UserDef, 1, userdef);

  LISTBASE_FOREACH (const bTheme *, btheme, &userdef->themes) {
    BLO_write_struct(writer, bTheme, btheme);
  }

  LISTBASE_FOREACH (const wmKeyMap *, keymap, &userdef->user_keymaps) {
    BLO_write_struct(writer, wmKeyMap, keymap);

    LISTBASE_FOREACH (const wmKeyMapDiffItem *, kmdi, &keymap->diff_items) {
      BLO_write_struct(writer, wmKeyMapDiffItem, kmdi);
      if (kmdi->remove_item) {
        write_keymapitem(writer, kmdi->remove_item);
      }
      if (kmdi->add_item) {
        write_keymapitem(writer, kmdi->add_item);
      }
    }

    LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &keymap->items) {
      write_keymapitem(writer, kmi);
    }
  }

  LISTBASE_FOREACH (const wmKeyConfigPref *, kpt, &userdef->user_keyconfig_prefs) {
    BLO_write_struct(writer, wmKeyConfigPref, kpt);
    if (kpt->prop) {
      IDP_BlendWrite(writer, kpt->prop);
    }
  }

  LISTBASE_FOREACH (const bUserMenu *, um, &userdef->user_menus) {
    BLO_write_struct(writer, bUserMenu, um);
    LISTBASE_FOREACH (const bUserMenuItem *, umi, &um->items) {
      if (umi->type == USER_MENU_TYPE_OPERATOR) {
        const bUserMenuItem_Op *umi_op = (const bUserMenuItem_Op *)umi;
        BLO_write_struct(writer, bUserMenuItem_Op, umi_op);
        if (umi_op->prop) {
          IDP_BlendWrite(writer, umi_op->prop);
        }
      }
      else if (umi->type == USER_MENU_TYPE_MENU) {
        const bUserMenuItem_Menu *umi_mt = (const bUserMenuItem_Menu *)umi;
        BLO_write_struct(writer, bUserMenuItem_Menu, umi_mt);
      }
      else if (umi->type == USER_MENU_TYPE_PROP) {
        const bUserMenuItem_Prop *umi_pr = (const bUserMenuItem_Prop *)umi;
        BLO_write_struct(writer, bUserMenuItem_Prop, umi_pr);
      }
      else {
        BLO_write_struct(writer, bUserMenuItem, umi);
      }
    }
  }

  LISTBASE_FOREACH (const bAddon *, bext, &userdef->addons) {
    BLO_write_struct(writer, bAddon, bext);
    if (bext->prop) {
      IDP_BlendWrite(writer, bext->prop);
    }
  }

  LISTBASE_FOREACH (const bPathCompare *, path_cmp, &userdef->autoexec_paths) {
    BLO_write_struct(writer, bPathCompare, path_cmp);
  }

  LISTBASE_FOREACH (const bUserAssetLibrary *, asset_library, &userdef->asset_libraries) {
    BLO_write_struct(writer, bUserAssetLibrary, asset_library);
  }

  LISTBASE_FOREACH (const uiStyle *, style, &userdef->uistyles) {
    BLO_write_struct(writer, uiStyle, style);
  }
}

/* Keep it last of write_foodata functions. */
static void write_libraries(WriteData *wd, Main *main)
{
  ListBase *lbarray[INDEX_ID_MAX];
  ID *id;
  int a, tot;
  bool found_one;

  for (; main; main = main->next) {
    a = tot = set_listbasepointers(main, lbarray);

    /* test: is lib being used */
    if (main->curlib && main->curlib->packedfile) {
      found_one = true;
    }
    else if (wd->use_memfile) {
      /* When writing undo step we always write all existing libraries, makes reading undo step
       * much easier when dealing with purely indirectly used libraries. */
      found_one = true;
    }
    else {
      found_one = false;
      while (!found_one && tot--) {
        for (id = lbarray[tot]->first; id; id = id->next) {
          if (id->us > 0 &&
              ((id->tag & LIB_TAG_EXTERN) ||
               ((id->tag & LIB_TAG_INDIRECT) && (id->flag & LIB_INDIRECT_WEAK_LINK)))) {
            found_one = true;
            break;
          }
        }
      }
    }

    /* To be able to restore 'quit.blend' and temp saves,
     * the packed blend has to be in undo buffers... */
    /* XXX needs rethink, just like save UI in undo files now -
     * would be nice to append things only for the 'quit.blend' and temp saves. */
    if (found_one) {
      /* Not overridable. */

      BlendWriter writer = {wd};
      writestruct(wd, ID_LI, Library, 1, main->curlib);
      BKE_id_blend_write(&writer, &main->curlib->id);

      if (main->curlib->packedfile) {
        BKE_packedfile_blend_write(&writer, main->curlib->packedfile);
        if (wd->use_memfile == false) {
          printf("write packed .blend: %s\n", main->curlib->filepath);
        }
      }

      /* Write link placeholders for all direct linked IDs. */
      while (a--) {
        for (id = lbarray[a]->first; id; id = id->next) {
          if (id->us > 0 &&
              ((id->tag & LIB_TAG_EXTERN) ||
               ((id->tag & LIB_TAG_INDIRECT) && (id->flag & LIB_INDIRECT_WEAK_LINK)))) {
            if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
              printf(
                  "ERROR: write file: data-block '%s' from lib '%s' is not linkable "
                  "but is flagged as directly linked",
                  id->name,
                  main->curlib->filepath_abs);
              BLI_assert(0);
            }
            writestruct(wd, ID_LINK_PLACEHOLDER, ID, 1, id);
          }
        }
      }
    }
  }

  mywrite_flush(wd);
}

/* context is usually defined by WM, two cases where no WM is available:
 * - for forward compatibility, curscreen has to be saved
 * - for undofile, curscene needs to be saved */
static void write_global(WriteData *wd, int fileflags, Main *mainvar)
{
  const bool is_undo = wd->use_memfile;
  FileGlobal fg;
  bScreen *screen;
  Scene *scene;
  ViewLayer *view_layer;
  char subvstr[8];

  /* prevent mem checkers from complaining */
  memset(fg._pad, 0, sizeof(fg._pad));
  memset(fg.filename, 0, sizeof(fg.filename));
  memset(fg.build_hash, 0, sizeof(fg.build_hash));
  fg._pad1 = NULL;

  current_screen_compat(mainvar, is_undo, &screen, &scene, &view_layer);

  /* XXX still remap G */
  fg.curscreen = screen;
  fg.curscene = scene;
  fg.cur_view_layer = view_layer;

  /* prevent to save this, is not good convention, and feature with concerns... */
  fg.fileflags = (fileflags & ~G_FILE_FLAG_ALL_RUNTIME);

  fg.globalf = G.f;
  BLI_strncpy(fg.filename, mainvar->name, sizeof(fg.filename));
  sprintf(subvstr, "%4d", BLENDER_FILE_SUBVERSION);
  memcpy(fg.subvstr, subvstr, 4);

  fg.subversion = BLENDER_FILE_SUBVERSION;
  fg.minversion = BLENDER_FILE_MIN_VERSION;
  fg.minsubversion = BLENDER_FILE_MIN_SUBVERSION;
#ifdef WITH_BUILDINFO
  {
    extern unsigned long build_commit_timestamp;
    extern char build_hash[];
    /* TODO(sergey): Add branch name to file as well? */
    fg.build_commit_timestamp = build_commit_timestamp;
    BLI_strncpy(fg.build_hash, build_hash, sizeof(fg.build_hash));
  }
#else
  fg.build_commit_timestamp = 0;
  BLI_strncpy(fg.build_hash, "unknown", sizeof(fg.build_hash));
#endif
  writestruct(wd, GLOB, FileGlobal, 1, &fg);
}

/* preview image, first 2 values are width and height
 * second are an RGBA image (uchar)
 * note, this uses 'TEST' since new types will segfault on file load for older blender versions.
 */
static void write_thumb(WriteData *wd, const BlendThumbnail *thumb)
{
  if (thumb) {
    writedata(wd, TEST, BLEN_THUMB_MEMSIZE_FILE(thumb->width, thumb->height), thumb);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Writing (Private)
 * \{ */

/* if MemFile * there's filesave to memory */
static bool write_file_handle(Main *mainvar,
                              WriteWrap *ww,
                              MemFile *compare,
                              MemFile *current,
                              int write_flags,
                              bool use_userdef,
                              const BlendThumbnail *thumb)
{
  BHead bhead;
  ListBase mainlist;
  char buf[16];
  WriteData *wd;

  blo_split_main(&mainlist, mainvar);

  wd = mywrite_begin(ww, compare, current);
  BlendWriter writer = {wd};

  sprintf(buf,
          "BLENDER%c%c%.3d",
          (sizeof(void *) == 8) ? '-' : '_',
          (ENDIAN_ORDER == B_ENDIAN) ? 'V' : 'v',
          BLENDER_FILE_VERSION);

  mywrite(wd, buf, 12);

  write_renderinfo(wd, mainvar);
  write_thumb(wd, thumb);
  write_global(wd, write_flags, mainvar);

  /* The window-manager and screen often change,
   * avoid thumbnail detecting changes because of this. */
  mywrite_flush(wd);

  OverrideLibraryStorage *override_storage = wd->use_memfile ?
                                                 NULL :
                                                 BKE_lib_override_library_operations_store_init();

#define ID_BUFFER_STATIC_SIZE 8192
  /* This outer loop allows to save first data-blocks from real mainvar,
   * then the temp ones from override process,
   * if needed, without duplicating whole code. */
  Main *bmain = mainvar;
  do {
    ListBase *lbarray[INDEX_ID_MAX];
    int a = set_listbasepointers(bmain, lbarray);
    while (a--) {
      ID *id = lbarray[a]->first;

      if (id == NULL || GS(id->name) == ID_LI) {
        continue; /* Libraries are handled separately below. */
      }

      char id_buffer_static[ID_BUFFER_STATIC_SIZE];
      void *id_buffer = id_buffer_static;
      const size_t idtype_struct_size = BKE_idtype_get_info_from_id(id)->struct_size;
      if (idtype_struct_size > ID_BUFFER_STATIC_SIZE) {
        BLI_assert(0);
        id_buffer = MEM_mallocN(idtype_struct_size, __func__);
      }

      for (; id; id = id->next) {
        /* We should never attempt to write non-regular IDs
         * (i.e. all kind of temp/runtime ones). */
        BLI_assert(
            (id->tag & (LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT | LIB_TAG_NOT_ALLOCATED)) == 0);

        const bool do_override = !ELEM(override_storage, NULL, bmain) &&
                                 ID_IS_OVERRIDE_LIBRARY_REAL(id);

        if (do_override) {
          BKE_lib_override_library_operations_store_start(bmain, override_storage, id);
        }

        if (wd->use_memfile) {
          /* Record the changes that happened up to this undo push in
           * recalc_up_to_undo_push, and clear recalc_after_undo_push again
           * to start accumulating for the next undo push. */
          id->recalc_up_to_undo_push = id->recalc_after_undo_push;
          id->recalc_after_undo_push = 0;

          bNodeTree *nodetree = ntreeFromID(id);
          if (nodetree != NULL) {
            nodetree->id.recalc_up_to_undo_push = nodetree->id.recalc_after_undo_push;
            nodetree->id.recalc_after_undo_push = 0;
          }
          if (GS(id->name) == ID_SCE) {
            Scene *scene = (Scene *)id;
            if (scene->master_collection != NULL) {
              scene->master_collection->id.recalc_up_to_undo_push =
                  scene->master_collection->id.recalc_after_undo_push;
              scene->master_collection->id.recalc_after_undo_push = 0;
            }
          }
        }

        mywrite_id_begin(wd, id);

        memcpy(id_buffer, id, idtype_struct_size);

        ((ID *)id_buffer)->tag = 0;
        /* Those listbase data change every time we add/remove an ID, and also often when
         * renaming one (due to re-sorting). This avoids generating a lot of false 'is changed'
         * detections between undo steps. */
        ((ID *)id_buffer)->prev = NULL;
        ((ID *)id_buffer)->next = NULL;

        const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
        if (id_type->blend_write != NULL) {
          id_type->blend_write(&writer, (ID *)id_buffer, id);
        }

        if (do_override) {
          BKE_lib_override_library_operations_store_end(override_storage, id);
        }

        mywrite_id_end(wd, id);
      }

      if (id_buffer != id_buffer_static) {
        MEM_SAFE_FREE(id_buffer);
      }

      mywrite_flush(wd);
    }
  } while ((bmain != override_storage) && (bmain = override_storage));

  if (override_storage) {
    BKE_lib_override_library_operations_store_finalize(override_storage);
    override_storage = NULL;
  }

  /* Special handling, operating over split Mains... */
  write_libraries(wd, mainvar->next);

  /* So changes above don't cause a 'DNA1' to be detected as changed on undo. */
  mywrite_flush(wd);

  if (use_userdef) {
    write_userdef(&writer, &U);
  }

  /* Write DNA last, because (to be implemented) test for which structs are written.
   *
   * Note that we *borrow* the pointer to 'DNAstr',
   * so writing each time uses the same address and doesn't cause unnecessary undo overhead. */
  writedata(wd, DNA1, (size_t)wd->sdna->data_len, wd->sdna->data);

  /* end of file */
  memset(&bhead, 0, sizeof(BHead));
  bhead.code = ENDB;
  mywrite(wd, &bhead, sizeof(BHead));

  blo_join_main(&mainlist);

  return mywrite_end(wd);
}

/* do reverse file history: .blend1 -> .blend2, .blend -> .blend1 */
/* return: success(0), failure(1) */
static bool do_history(const char *name, ReportList *reports)
{
  char tempname1[FILE_MAX], tempname2[FILE_MAX];
  int hisnr = U.versions;

  if (U.versions == 0) {
    return 0;
  }

  if (strlen(name) < 2) {
    BKE_report(reports, RPT_ERROR, "Unable to make version backup: filename too short");
    return 1;
  }

  while (hisnr > 1) {
    BLI_snprintf(tempname1, sizeof(tempname1), "%s%d", name, hisnr - 1);
    if (BLI_exists(tempname1)) {
      BLI_snprintf(tempname2, sizeof(tempname2), "%s%d", name, hisnr);

      if (BLI_rename(tempname1, tempname2)) {
        BKE_report(reports, RPT_ERROR, "Unable to make version backup");
        return true;
      }
    }
    hisnr--;
  }

  /* is needed when hisnr==1 */
  if (BLI_exists(name)) {
    BLI_snprintf(tempname1, sizeof(tempname1), "%s%d", name, hisnr);

    if (BLI_rename(name, tempname1)) {
      BKE_report(reports, RPT_ERROR, "Unable to make version backup");
      return true;
    }
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Writing (Public)
 * \{ */

/**
 * \return Success.
 */
bool BLO_write_file(Main *mainvar,
                    const char *filepath,
                    const int write_flags,
                    const struct BlendFileWriteParams *params,
                    ReportList *reports)
{
  char tempname[FILE_MAX + 1];
  eWriteWrapType ww_type;
  WriteWrap ww;

  eBLO_WritePathRemap remap_mode = params->remap_mode;
  const bool use_save_versions = params->use_save_versions;
  const bool use_save_as_copy = params->use_save_as_copy;
  const bool use_userdef = params->use_userdef;
  const BlendThumbnail *thumb = params->thumb;

  /* path backup/restore */
  void *path_list_backup = NULL;
  const int path_list_flag = (BKE_BPATH_TRAVERSE_SKIP_LIBRARY | BKE_BPATH_TRAVERSE_SKIP_MULTIFILE);

  if (G.debug & G_DEBUG_IO && mainvar->lock != NULL) {
    BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *BEFORE* save to disk");
    BLO_main_validate_libraries(mainvar, reports);
    BLO_main_validate_shapekeys(mainvar, reports);
  }

  /* open temporary file, so we preserve the original in case we crash */
  BLI_snprintf(tempname, sizeof(tempname), "%s@", filepath);

  if (write_flags & G_FILE_COMPRESS) {
    ww_type = WW_WRAP_ZLIB;
  }
  else {
    ww_type = WW_WRAP_NONE;
  }

  ww_handle_init(ww_type, &ww);

  if (ww.open(&ww, tempname) == false) {
    BKE_reportf(
        reports, RPT_ERROR, "Cannot open file %s for writing: %s", tempname, strerror(errno));
    return 0;
  }

  /* Remapping of relative paths to new file location. */
  if (remap_mode != BLO_WRITE_PATH_REMAP_NONE) {

    if (remap_mode == BLO_WRITE_PATH_REMAP_RELATIVE) {
      /* Make all relative as none of the existing paths can be relative in an unsaved document.
       */
      if (G.relbase_valid == false) {
        remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE_ALL;
      }
    }

    char dir_src[FILE_MAX];
    char dir_dst[FILE_MAX];
    BLI_split_dir_part(mainvar->name, dir_src, sizeof(dir_src));
    BLI_split_dir_part(filepath, dir_dst, sizeof(dir_dst));

    /* Just in case there is some subtle difference. */
    BLI_path_normalize(mainvar->name, dir_dst);
    BLI_path_normalize(mainvar->name, dir_src);

    /* Only for relative, not relative-all, as this means making existing paths relative. */
    if (remap_mode == BLO_WRITE_PATH_REMAP_RELATIVE) {
      if (G.relbase_valid && (BLI_path_cmp(dir_dst, dir_src) == 0)) {
        /* Saved to same path. Nothing to do. */
        remap_mode = BLO_WRITE_PATH_REMAP_NONE;
      }
    }
    else if (remap_mode == BLO_WRITE_PATH_REMAP_ABSOLUTE) {
      if (G.relbase_valid == false) {
        /* Unsaved, all paths are absolute.Even if the user manages to set a relative path,
         * there is no base-path that can be used to make it absolute. */
        remap_mode = BLO_WRITE_PATH_REMAP_NONE;
      }
    }

    if (remap_mode != BLO_WRITE_PATH_REMAP_NONE) {
      /* Check if we need to backup and restore paths. */
      if (UNLIKELY(use_save_as_copy)) {
        path_list_backup = BKE_bpath_list_backup(mainvar, path_list_flag);
      }

      switch (remap_mode) {
        case BLO_WRITE_PATH_REMAP_RELATIVE:
          /* Saved, make relative paths relative to new location (if possible). */
          BKE_bpath_relative_rebase(mainvar, dir_src, dir_dst, NULL);
          break;
        case BLO_WRITE_PATH_REMAP_RELATIVE_ALL:
          /* Make all relative (when requested or unsaved). */
          BKE_bpath_relative_convert(mainvar, dir_dst, NULL);
          break;
        case BLO_WRITE_PATH_REMAP_ABSOLUTE:
          /* Make all absolute (when requested or unsaved). */
          BKE_bpath_absolute_convert(mainvar, dir_src, NULL);
          break;
        case BLO_WRITE_PATH_REMAP_NONE:
          BLI_assert(0); /* Unreachable. */
          break;
      }
    }
  }

  /* actual file writing */
  const bool err = write_file_handle(mainvar, &ww, NULL, NULL, write_flags, use_userdef, thumb);

  ww.close(&ww);

  if (UNLIKELY(path_list_backup)) {
    BKE_bpath_list_restore(mainvar, path_list_flag, path_list_backup);
    BKE_bpath_list_free(path_list_backup);
  }

  if (err) {
    BKE_report(reports, RPT_ERROR, strerror(errno));
    remove(tempname);

    return 0;
  }

  /* file save to temporary file was successful */
  /* now do reverse file history (move .blend1 -> .blend2, .blend -> .blend1) */
  if (use_save_versions) {
    const bool err_hist = do_history(filepath, reports);
    if (err_hist) {
      BKE_report(reports, RPT_ERROR, "Version backup failed (file saved with @)");
      return 0;
    }
  }

  if (BLI_rename(tempname, filepath) != 0) {
    BKE_report(reports, RPT_ERROR, "Cannot change old file (file saved with @)");
    return 0;
  }

  if (G.debug & G_DEBUG_IO && mainvar->lock != NULL) {
    BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *AFTER* save to disk");
    BLO_main_validate_libraries(mainvar, reports);
  }

  return 1;
}

/**
 * \return Success.
 */
bool BLO_write_file_mem(Main *mainvar, MemFile *compare, MemFile *current, int write_flags)
{
  bool use_userdef = false;

  const bool err = write_file_handle(
      mainvar, NULL, compare, current, write_flags, use_userdef, NULL);

  return (err == 0);
}

void BLO_write_raw(BlendWriter *writer, size_t size_in_bytes, const void *data_ptr)
{
  writedata(writer->wd, DATA, size_in_bytes, data_ptr);
}

void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr)
{
  BLO_write_struct_array_by_name(writer, struct_name, 1, data_ptr);
}

void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    int array_size,
                                    const void *data_ptr)
{
  int struct_id = BLO_get_struct_id_by_name(writer, struct_name);
  if (UNLIKELY(struct_id == -1)) {
    printf("error: can't find SDNA code <%s>\n", struct_name);
    return;
  }
  BLO_write_struct_array_by_id(writer, struct_id, array_size, data_ptr);
}

void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr)
{
  writestruct_nr(writer->wd, DATA, struct_id, 1, data_ptr);
}

void BLO_write_struct_at_address_by_id(BlendWriter *writer,
                                       int struct_id,
                                       const void *address,
                                       const void *data_ptr)
{
  BLO_write_struct_at_address_by_id_with_filecode(writer, DATA, struct_id, address, data_ptr);
}

void BLO_write_struct_at_address_by_id_with_filecode(
    BlendWriter *writer, int filecode, int struct_id, const void *address, const void *data_ptr)
{
  writestruct_at_address_nr(writer->wd, filecode, struct_id, 1, address, data_ptr);
}

void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  int struct_id,
                                  int array_size,
                                  const void *data_ptr)
{
  writestruct_nr(writer->wd, DATA, struct_id, array_size, data_ptr);
}

void BLO_write_struct_array_at_address_by_id(
    BlendWriter *writer, int struct_id, int array_size, const void *address, const void *data_ptr)
{
  writestruct_at_address_nr(writer->wd, DATA, struct_id, array_size, address, data_ptr);
}

void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, ListBase *list)
{
  writelist_nr(writer->wd, DATA, struct_id, list);
}

void BLO_write_struct_list_by_name(BlendWriter *writer, const char *struct_name, ListBase *list)
{
  int struct_id = BLO_get_struct_id_by_name(writer, struct_name);
  if (UNLIKELY(struct_id == -1)) {
    printf("error: can't find SDNA code <%s>\n", struct_name);
    return;
  }
  BLO_write_struct_list_by_id(writer, struct_id, list);
}

void blo_write_id_struct(BlendWriter *writer, int struct_id, const void *id_address, const ID *id)
{
  writestruct_at_address_nr(writer->wd, GS(id->name), struct_id, 1, id_address, id);
}

int BLO_get_struct_id_by_name(BlendWriter *writer, const char *struct_name)
{
  int struct_id = DNA_struct_find_nr(writer->wd->sdna, struct_name);
  return struct_id;
}

void BLO_write_int32_array(BlendWriter *writer, uint num, const int32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int32_t) * (size_t)num, data_ptr);
}

void BLO_write_uint32_array(BlendWriter *writer, uint num, const uint32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(uint32_t) * (size_t)num, data_ptr);
}

void BLO_write_float_array(BlendWriter *writer, uint num, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float) * (size_t)num, data_ptr);
}

void BLO_write_double_array(BlendWriter *writer, uint num, const double *data_ptr)
{
  BLO_write_raw(writer, sizeof(double) * (size_t)num, data_ptr);
}

void BLO_write_pointer_array(BlendWriter *writer, uint num, const void *data_ptr)
{
  BLO_write_raw(writer, sizeof(void *) * (size_t)num, data_ptr);
}

void BLO_write_float3_array(BlendWriter *writer, uint num, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float[3]) * (size_t)num, data_ptr);
}

/**
 * Write a null terminated string.
 */
void BLO_write_string(BlendWriter *writer, const char *data_ptr)
{
  if (data_ptr != NULL) {
    BLO_write_raw(writer, strlen(data_ptr) + 1, data_ptr);
  }
}

/**
 * Sometimes different data is written depending on whether the file is saved to disk or used for
 * undo. This function returns true when the current file-writing is done for undo.
 */
bool BLO_write_is_undo(BlendWriter *writer)
{
  return writer->wd->use_memfile;
}

/** \} */
