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

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_fluid_types.h"
#include "DNA_genfile.h"
#include "DNA_lightprobe_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcache_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_mempool.h"
#include "MEM_guardedalloc.h" /* MEM_freeN */

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_blender_version.h"
#include "BKE_bpath.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_curveprofile.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.h" /* for G */
#include "BKE_gpencil_modifier.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"
#include "BKE_shader_fx.h"
#include "BKE_subsurf.h"
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

  LISTBASE_FOREACH (const uiStyle *, style, &userdef->uistyles) {
    BLO_write_struct(writer, uiStyle, style);
  }
}

static void write_boid_state(BlendWriter *writer, BoidState *state)
{
  BLO_write_struct(writer, BoidState, state);

  LISTBASE_FOREACH (BoidRule *, rule, &state->rules) {
    switch (rule->type) {
      case eBoidRuleType_Goal:
      case eBoidRuleType_Avoid:
        BLO_write_struct(writer, BoidRuleGoalAvoid, rule);
        break;
      case eBoidRuleType_AvoidCollision:
        BLO_write_struct(writer, BoidRuleAvoidCollision, rule);
        break;
      case eBoidRuleType_FollowLeader:
        BLO_write_struct(writer, BoidRuleFollowLeader, rule);
        break;
      case eBoidRuleType_AverageSpeed:
        BLO_write_struct(writer, BoidRuleAverageSpeed, rule);
        break;
      case eBoidRuleType_Fight:
        BLO_write_struct(writer, BoidRuleFight, rule);
        break;
      default:
        BLO_write_struct(writer, BoidRule, rule);
        break;
    }
  }
#if 0
  BoidCondition *cond = state->conditions.first;
  for (; cond; cond = cond->next) {
    BLO_write_struct(writer, BoidCondition, cond);
  }
#endif
}

/* update this also to readfile.c */
static const char *ptcache_data_struct[] = {
    "",          // BPHYS_DATA_INDEX
    "",          // BPHYS_DATA_LOCATION
    "",          // BPHYS_DATA_VELOCITY
    "",          // BPHYS_DATA_ROTATION
    "",          // BPHYS_DATA_AVELOCITY / BPHYS_DATA_XCONST */
    "",          // BPHYS_DATA_SIZE:
    "",          // BPHYS_DATA_TIMES:
    "BoidData",  // case BPHYS_DATA_BOIDS:
};
static const char *ptcache_extra_struct[] = {
    "",
    "ParticleSpring",
    "vec3f",
};
static void write_pointcaches(BlendWriter *writer, ListBase *ptcaches)
{
  LISTBASE_FOREACH (PointCache *, cache, ptcaches) {
    BLO_write_struct(writer, PointCache, cache);

    if ((cache->flag & PTCACHE_DISK_CACHE) == 0) {
      LISTBASE_FOREACH (PTCacheMem *, pm, &cache->mem_cache) {
        BLO_write_struct(writer, PTCacheMem, pm);

        for (int i = 0; i < BPHYS_TOT_DATA; i++) {
          if (pm->data[i] && pm->data_types & (1 << i)) {
            if (ptcache_data_struct[i][0] == '\0') {
              BLO_write_raw(writer, MEM_allocN_len(pm->data[i]), pm->data[i]);
            }
            else {
              BLO_write_struct_array_by_name(
                  writer, ptcache_data_struct[i], pm->totpoint, pm->data[i]);
            }
          }
        }

        LISTBASE_FOREACH (PTCacheExtra *, extra, &pm->extradata) {
          if (ptcache_extra_struct[extra->type][0] == '\0') {
            continue;
          }
          BLO_write_struct(writer, PTCacheExtra, extra);
          BLO_write_struct_array_by_name(
              writer, ptcache_extra_struct[extra->type], extra->totdata, extra->data);
        }
      }
    }
  }
}

static void write_particlesettings(BlendWriter *writer,
                                   ParticleSettings *part,
                                   const void *id_address)
{
  if (part->id.us > 0 || BLO_write_is_undo(writer)) {
    /* write LibData */
    BLO_write_id_struct(writer, ParticleSettings, id_address, &part->id);
    BKE_id_blend_write(writer, &part->id);

    if (part->adt) {
      BKE_animdata_blend_write(writer, part->adt);
    }
    BLO_write_struct(writer, PartDeflect, part->pd);
    BLO_write_struct(writer, PartDeflect, part->pd2);
    BLO_write_struct(writer, EffectorWeights, part->effector_weights);

    if (part->clumpcurve) {
      BKE_curvemapping_blend_write(writer, part->clumpcurve);
    }
    if (part->roughcurve) {
      BKE_curvemapping_blend_write(writer, part->roughcurve);
    }
    if (part->twistcurve) {
      BKE_curvemapping_blend_write(writer, part->twistcurve);
    }

    LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
      /* update indices, but only if dw->ob is set (can be NULL after loading e.g.) */
      if (dw->ob != NULL) {
        dw->index = 0;
        if (part->instance_collection) { /* can be NULL if lining fails or set to None */
          FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (part->instance_collection, object) {
            if (object == dw->ob) {
              break;
            }
            dw->index++;
          }
          FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
        }
      }
      BLO_write_struct(writer, ParticleDupliWeight, dw);
    }

    if (part->boids && part->phystype == PART_PHYS_BOIDS) {
      BLO_write_struct(writer, BoidSettings, part->boids);

      LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
        write_boid_state(writer, state);
      }
    }
    if (part->fluid && part->phystype == PART_PHYS_FLUID) {
      BLO_write_struct(writer, SPHFluidSettings, part->fluid);
    }

    for (int a = 0; a < MAX_MTEX; a++) {
      if (part->mtex[a]) {
        BLO_write_struct(writer, MTex, part->mtex[a]);
      }
    }
  }
}

static void write_particlesystems(BlendWriter *writer, ListBase *particles)
{
  LISTBASE_FOREACH (ParticleSystem *, psys, particles) {
    BLO_write_struct(writer, ParticleSystem, psys);

    if (psys->particles) {
      BLO_write_struct_array(writer, ParticleData, psys->totpart, psys->particles);

      if (psys->particles->hair) {
        ParticleData *pa = psys->particles;

        for (int a = 0; a < psys->totpart; a++, pa++) {
          BLO_write_struct_array(writer, HairKey, pa->totkey, pa->hair);
        }
      }

      if (psys->particles->boid && (psys->part->phystype == PART_PHYS_BOIDS)) {
        BLO_write_struct_array(writer, BoidParticle, psys->totpart, psys->particles->boid);
      }

      if (psys->part->fluid && (psys->part->phystype == PART_PHYS_FLUID) &&
          (psys->part->fluid->flag & SPH_VISCOELASTIC_SPRINGS)) {
        BLO_write_struct_array(
            writer, ParticleSpring, psys->tot_fluidsprings, psys->fluid_springs);
      }
    }
    LISTBASE_FOREACH (ParticleTarget *, pt, &psys->targets) {
      BLO_write_struct(writer, ParticleTarget, pt);
    }

    if (psys->child) {
      BLO_write_struct_array(writer, ChildParticle, psys->totchild, psys->child);
    }

    if (psys->clmd) {
      BLO_write_struct(writer, ClothModifierData, psys->clmd);
      BLO_write_struct(writer, ClothSimSettings, psys->clmd->sim_parms);
      BLO_write_struct(writer, ClothCollSettings, psys->clmd->coll_parms);
    }

    write_pointcaches(writer, &psys->ptcaches);
  }
}

static void write_motionpath(BlendWriter *writer, bMotionPath *mpath)
{
  /* sanity checks */
  if (mpath == NULL) {
    return;
  }

  /* firstly, just write the motionpath struct */
  BLO_write_struct(writer, bMotionPath, mpath);

  /* now write the array of data */
  BLO_write_struct_array(writer, bMotionPathVert, mpath->length, mpath->points);
}

static void write_constraints(BlendWriter *writer, ListBase *conlist)
{
  LISTBASE_FOREACH (bConstraint *, con, conlist) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    /* Write the specific data */
    if (cti && con->data) {
      /* firstly, just write the plain con->data struct */
      BLO_write_struct_by_name(writer, cti->structName, con->data);

      /* do any constraint specific stuff */
      switch (con->type) {
        case CONSTRAINT_TYPE_PYTHON: {
          bPythonConstraint *data = con->data;

          /* write targets */
          LISTBASE_FOREACH (bConstraintTarget *, ct, &data->targets) {
            BLO_write_struct(writer, bConstraintTarget, ct);
          }

          /* Write ID Properties -- and copy this comment EXACTLY for easy finding
           * of library blocks that implement this.*/
          IDP_BlendWrite(writer, data->prop);

          break;
        }
        case CONSTRAINT_TYPE_ARMATURE: {
          bArmatureConstraint *data = con->data;

          /* write targets */
          LISTBASE_FOREACH (bConstraintTarget *, ct, &data->targets) {
            BLO_write_struct(writer, bConstraintTarget, ct);
          }

          break;
        }
        case CONSTRAINT_TYPE_SPLINEIK: {
          bSplineIKConstraint *data = con->data;

          /* write points array */
          BLO_write_float_array(writer, data->numpoints, data->points);

          break;
        }
      }
    }

    /* Write the constraint */
    BLO_write_struct(writer, bConstraint, con);
  }
}

static void write_pose(BlendWriter *writer, bPose *pose, bArmature *arm)
{
  /* Write each channel */
  if (pose == NULL) {
    return;
  }

  BLI_assert(arm != NULL);

  /* Write channels */
  LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
    /* Write ID Properties -- and copy this comment EXACTLY for easy finding
     * of library blocks that implement this.*/
    if (chan->prop) {
      IDP_BlendWrite(writer, chan->prop);
    }

    write_constraints(writer, &chan->constraints);

    write_motionpath(writer, chan->mpath);

    /* Prevent crashes with autosave,
     * when a bone duplicated in edit-mode has not yet been assigned to its pose-channel.
     * Also needed with memundo, in some cases we can store a step before pose has been
     * properly rebuilt from previous undo step. */
    Bone *bone = (pose->flag & POSE_RECALC) ? BKE_armature_find_bone_name(arm, chan->name) :
                                              chan->bone;
    if (bone != NULL) {
      /* gets restored on read, for library armatures */
      chan->selectflag = bone->flag & BONE_SELECTED;
    }

    BLO_write_struct(writer, bPoseChannel, chan);
  }

  /* Write groups */
  LISTBASE_FOREACH (bActionGroup *, grp, &pose->agroups) {
    BLO_write_struct(writer, bActionGroup, grp);
  }

  /* write IK param */
  if (pose->ikparam) {
    const char *structname = BKE_pose_ikparam_get_name(pose);
    if (structname) {
      BLO_write_struct_by_name(writer, structname, pose->ikparam);
    }
  }

  /* Write this pose */
  BLO_write_struct(writer, bPose, pose);
}

static void write_defgroups(BlendWriter *writer, ListBase *defbase)
{
  LISTBASE_FOREACH (bDeformGroup *, defgroup, defbase) {
    BLO_write_struct(writer, bDeformGroup, defgroup);
  }
}

static void write_fmaps(BlendWriter *writer, ListBase *fbase)
{
  LISTBASE_FOREACH (bFaceMap *, fmap, fbase) {
    BLO_write_struct(writer, bFaceMap, fmap);
  }
}

static void write_modifiers(BlendWriter *writer, ListBase *modbase)
{
  if (modbase == NULL) {
    return;
  }

  LISTBASE_FOREACH (ModifierData *, md, modbase) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
    if (mti == NULL) {
      return;
    }

    BLO_write_struct_by_name(writer, mti->structName, md);

    if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;

      BLO_write_struct(writer, ClothSimSettings, clmd->sim_parms);
      BLO_write_struct(writer, ClothCollSettings, clmd->coll_parms);
      BLO_write_struct(writer, EffectorWeights, clmd->sim_parms->effector_weights);
      write_pointcaches(writer, &clmd->ptcaches);
    }
    else if (md->type == eModifierType_Fluid) {
      FluidModifierData *fmd = (FluidModifierData *)md;

      if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
        BLO_write_struct(writer, FluidDomainSettings, fmd->domain);

        if (fmd->domain) {
          write_pointcaches(writer, &(fmd->domain->ptcaches[0]));

          /* create fake pointcache so that old blender versions can read it */
          fmd->domain->point_cache[1] = BKE_ptcache_add(&fmd->domain->ptcaches[1]);
          fmd->domain->point_cache[1]->flag |= PTCACHE_DISK_CACHE | PTCACHE_FAKE_SMOKE;
          fmd->domain->point_cache[1]->step = 1;

          write_pointcaches(writer, &(fmd->domain->ptcaches[1]));

          if (fmd->domain->coba) {
            BLO_write_struct(writer, ColorBand, fmd->domain->coba);
          }

          /* cleanup the fake pointcache */
          BKE_ptcache_free_list(&fmd->domain->ptcaches[1]);
          fmd->domain->point_cache[1] = NULL;

          BLO_write_struct(writer, EffectorWeights, fmd->domain->effector_weights);
        }
      }
      else if (fmd->type & MOD_FLUID_TYPE_FLOW) {
        BLO_write_struct(writer, FluidFlowSettings, fmd->flow);
      }
      else if (fmd->type & MOD_FLUID_TYPE_EFFEC) {
        BLO_write_struct(writer, FluidEffectorSettings, fmd->effector);
      }
    }
    else if (md->type == eModifierType_Fluidsim) {
      FluidsimModifierData *fluidmd = (FluidsimModifierData *)md;

      BLO_write_struct(writer, FluidsimSettings, fluidmd->fss);
    }
    else if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

      if (pmd->canvas) {
        BLO_write_struct(writer, DynamicPaintCanvasSettings, pmd->canvas);

        /* write surfaces */
        LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
          BLO_write_struct(writer, DynamicPaintSurface, surface);
        }
        /* write caches and effector weights */
        LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
          write_pointcaches(writer, &(surface->ptcaches));

          BLO_write_struct(writer, EffectorWeights, surface->effector_weights);
        }
      }
      if (pmd->brush) {
        BLO_write_struct(writer, DynamicPaintBrushSettings, pmd->brush);
        BLO_write_struct(writer, ColorBand, pmd->brush->paint_ramp);
        BLO_write_struct(writer, ColorBand, pmd->brush->vel_ramp);
      }
    }
    else if (md->type == eModifierType_Collision) {

#if 0
      CollisionModifierData *collmd = (CollisionModifierData *)md;
      // TODO: CollisionModifier should use pointcache
      // + have proper reset events before enabling this
      writestruct(wd, DATA, MVert, collmd->numverts, collmd->x);
      writestruct(wd, DATA, MVert, collmd->numverts, collmd->xnew);
      writestruct(wd, DATA, MFace, collmd->numfaces, collmd->mfaces);
#endif
    }

    if (mti->blendWrite != NULL) {
      mti->blendWrite(writer, md);
    }
  }
}

static void write_gpencil_modifiers(BlendWriter *writer, ListBase *modbase)
{
  if (modbase == NULL) {
    return;
  }

  LISTBASE_FOREACH (GpencilModifierData *, md, modbase) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
    if (mti == NULL) {
      return;
    }

    BLO_write_struct_by_name(writer, mti->struct_name, md);

    if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;

      if (gpmd->curfalloff) {
        BKE_curvemapping_blend_write(writer, gpmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      if (gpmd->colorband) {
        BLO_write_struct(writer, ColorBand, gpmd->colorband);
      }
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
  }
}

static void write_shaderfxs(BlendWriter *writer, ListBase *fxbase)
{
  if (fxbase == NULL) {
    return;
  }

  LISTBASE_FOREACH (ShaderFxData *, fx, fxbase) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);
    if (fxi == NULL) {
      return;
    }

    BLO_write_struct_by_name(writer, fxi->struct_name, fx);
  }
}

static void write_object(BlendWriter *writer, Object *ob, const void *id_address)
{
  const bool is_undo = BLO_write_is_undo(writer);
  if (ob->id.us > 0 || is_undo) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    BKE_object_runtime_reset(ob);

    if (is_undo) {
      /* For undo we stay in object mode during undo presses, so keep edit-mode disabled on save as
       * well, can help reducing false detection of changed data-blocks. */
      ob->mode &= ~OB_MODE_EDIT;
    }

    /* write LibData */
    BLO_write_id_struct(writer, Object, id_address, &ob->id);
    BKE_id_blend_write(writer, &ob->id);

    if (ob->adt) {
      BKE_animdata_blend_write(writer, ob->adt);
    }

    /* direct data */
    BLO_write_pointer_array(writer, ob->totcol, ob->mat);
    BLO_write_raw(writer, sizeof(char) * ob->totcol, ob->matbits);

    bArmature *arm = NULL;
    if (ob->type == OB_ARMATURE) {
      arm = ob->data;
      if (arm && ob->pose && arm->act_bone) {
        BLI_strncpy(
            ob->pose->proxy_act_bone, arm->act_bone->name, sizeof(ob->pose->proxy_act_bone));
      }
    }

    write_pose(writer, ob->pose, arm);
    write_defgroups(writer, &ob->defbase);
    write_fmaps(writer, &ob->fmaps);
    write_constraints(writer, &ob->constraints);
    write_motionpath(writer, ob->mpath);

    BLO_write_struct(writer, PartDeflect, ob->pd);
    if (ob->soft) {
      /* Set deprecated pointers to prevent crashes of older Blenders */
      ob->soft->pointcache = ob->soft->shared->pointcache;
      ob->soft->ptcaches = ob->soft->shared->ptcaches;
      BLO_write_struct(writer, SoftBody, ob->soft);
      BLO_write_struct(writer, SoftBody_Shared, ob->soft->shared);
      write_pointcaches(writer, &(ob->soft->shared->ptcaches));
      BLO_write_struct(writer, EffectorWeights, ob->soft->effector_weights);
    }

    if (ob->rigidbody_object) {
      /* TODO: if any extra data is added to handle duplis, will need separate function then */
      BLO_write_struct(writer, RigidBodyOb, ob->rigidbody_object);
    }
    if (ob->rigidbody_constraint) {
      BLO_write_struct(writer, RigidBodyCon, ob->rigidbody_constraint);
    }

    if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
      BLO_write_struct(writer, ImageUser, ob->iuser);
    }

    write_particlesystems(writer, &ob->particlesystem);
    write_modifiers(writer, &ob->modifiers);
    write_gpencil_modifiers(writer, &ob->greasepencil_modifiers);
    write_shaderfxs(writer, &ob->shader_fx);

    BLO_write_struct_list(writer, LinkData, &ob->pc_ids);

    BKE_previewimg_blend_write(writer, ob->preview);
  }
}

static void write_collection_nolib(BlendWriter *writer, Collection *collection)
{
  /* Shared function for collection data-blocks and scene master collection. */
  BKE_previewimg_blend_write(writer, collection->preview);

  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    BLO_write_struct(writer, CollectionObject, cob);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BLO_write_struct(writer, CollectionChild, child);
  }
}

static void write_collection(BlendWriter *writer, Collection *collection, const void *id_address)
{
  if (collection->id.us > 0 || BLO_write_is_undo(writer)) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    collection->flag &= ~COLLECTION_HAS_OBJECT_CACHE;
    collection->tag = 0;
    BLI_listbase_clear(&collection->object_cache);
    BLI_listbase_clear(&collection->parents);

    /* write LibData */
    BLO_write_id_struct(writer, Collection, id_address, &collection->id);
    BKE_id_blend_write(writer, &collection->id);

    write_collection_nolib(writer, collection);
  }
}

static void write_sequence_modifiers(BlendWriter *writer, ListBase *modbase)
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, modbase) {
    const SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

    if (smti) {
      BLO_write_struct_by_name(writer, smti->struct_name, smd);

      if (smd->type == seqModifierType_Curves) {
        CurvesModifierData *cmd = (CurvesModifierData *)smd;

        BKE_curvemapping_blend_write(writer, &cmd->curve_mapping);
      }
      else if (smd->type == seqModifierType_HueCorrect) {
        HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

        BKE_curvemapping_blend_write(writer, &hcmd->curve_mapping);
      }
    }
    else {
      BLO_write_struct(writer, SequenceModifierData, smd);
    }
  }
}

static void write_view_settings(BlendWriter *writer, ColorManagedViewSettings *view_settings)
{
  if (view_settings->curve_mapping) {
    BKE_curvemapping_blend_write(writer, view_settings->curve_mapping);
  }
}

static void write_view3dshading(BlendWriter *writer, View3DShading *shading)
{
  if (shading->prop) {
    IDP_BlendWrite(writer, shading->prop);
  }
}

static void write_paint(BlendWriter *writer, Paint *p)
{
  if (p->cavity_curve) {
    BKE_curvemapping_blend_write(writer, p->cavity_curve);
  }
  BLO_write_struct_array(writer, PaintToolSlot, p->tool_slots_len, p->tool_slots);
}

static void write_layer_collections(BlendWriter *writer, ListBase *lb)
{
  LISTBASE_FOREACH (LayerCollection *, lc, lb) {
    BLO_write_struct(writer, LayerCollection, lc);

    write_layer_collections(writer, &lc->layer_collections);
  }
}

static void write_view_layer(BlendWriter *writer, ViewLayer *view_layer)
{
  BLO_write_struct(writer, ViewLayer, view_layer);
  BLO_write_struct_list(writer, Base, &view_layer->object_bases);

  if (view_layer->id_properties) {
    IDP_BlendWrite(writer, view_layer->id_properties);
  }

  LISTBASE_FOREACH (FreestyleModuleConfig *, fmc, &view_layer->freestyle_config.modules) {
    BLO_write_struct(writer, FreestyleModuleConfig, fmc);
  }

  LISTBASE_FOREACH (FreestyleLineSet *, fls, &view_layer->freestyle_config.linesets) {
    BLO_write_struct(writer, FreestyleLineSet, fls);
  }
  write_layer_collections(writer, &view_layer->layer_collections);
}

static void write_lightcache_texture(BlendWriter *writer, LightCacheTexture *tex)
{
  if (tex->data) {
    size_t data_size = tex->components * tex->tex_size[0] * tex->tex_size[1] * tex->tex_size[2];
    if (tex->data_type == LIGHTCACHETEX_FLOAT) {
      data_size *= sizeof(float);
    }
    else if (tex->data_type == LIGHTCACHETEX_UINT) {
      data_size *= sizeof(uint);
    }

    /* FIXME: We can't save more than what 32bit systems can handle.
     * The solution would be to split the texture but it is too late for 2.90. (see T78529) */
    if (data_size < INT_MAX) {
      BLO_write_raw(writer, data_size, tex->data);
    }
  }
}

static void write_lightcache(BlendWriter *writer, LightCache *cache)
{
  write_lightcache_texture(writer, &cache->grid_tx);
  write_lightcache_texture(writer, &cache->cube_tx);

  if (cache->cube_mips) {
    BLO_write_struct_array(writer, LightCacheTexture, cache->mips_len, cache->cube_mips);
    for (int i = 0; i < cache->mips_len; i++) {
      write_lightcache_texture(writer, &cache->cube_mips[i]);
    }
  }

  BLO_write_struct_array(writer, LightGridCache, cache->grid_len, cache->grid_data);
  BLO_write_struct_array(writer, LightProbeCache, cache->cube_len, cache->cube_data);
}

static void write_scene(BlendWriter *writer, Scene *sce, const void *id_address)
{
  if (BLO_write_is_undo(writer)) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    /* XXX This UI data should not be stored in Scene at all... */
    memset(&sce->cursor, 0, sizeof(sce->cursor));
  }

  /* write LibData */
  BLO_write_id_struct(writer, Scene, id_address, &sce->id);
  BKE_id_blend_write(writer, &sce->id);

  if (sce->adt) {
    BKE_animdata_blend_write(writer, sce->adt);
  }
  BKE_keyingsets_blend_write(writer, &sce->keyingsets);

  /* direct data */
  ToolSettings *tos = sce->toolsettings;
  BLO_write_struct(writer, ToolSettings, tos);
  if (tos->vpaint) {
    BLO_write_struct(writer, VPaint, tos->vpaint);
    write_paint(writer, &tos->vpaint->paint);
  }
  if (tos->wpaint) {
    BLO_write_struct(writer, VPaint, tos->wpaint);
    write_paint(writer, &tos->wpaint->paint);
  }
  if (tos->sculpt) {
    BLO_write_struct(writer, Sculpt, tos->sculpt);
    write_paint(writer, &tos->sculpt->paint);
  }
  if (tos->uvsculpt) {
    BLO_write_struct(writer, UvSculpt, tos->uvsculpt);
    write_paint(writer, &tos->uvsculpt->paint);
  }
  if (tos->gp_paint) {
    BLO_write_struct(writer, GpPaint, tos->gp_paint);
    write_paint(writer, &tos->gp_paint->paint);
  }
  if (tos->gp_vertexpaint) {
    BLO_write_struct(writer, GpVertexPaint, tos->gp_vertexpaint);
    write_paint(writer, &tos->gp_vertexpaint->paint);
  }
  if (tos->gp_sculptpaint) {
    BLO_write_struct(writer, GpSculptPaint, tos->gp_sculptpaint);
    write_paint(writer, &tos->gp_sculptpaint->paint);
  }
  if (tos->gp_weightpaint) {
    BLO_write_struct(writer, GpWeightPaint, tos->gp_weightpaint);
    write_paint(writer, &tos->gp_weightpaint->paint);
  }
  /* write grease-pencil custom ipo curve to file */
  if (tos->gp_interpolate.custom_ipo) {
    BKE_curvemapping_blend_write(writer, tos->gp_interpolate.custom_ipo);
  }
  /* write grease-pencil multiframe falloff curve to file */
  if (tos->gp_sculpt.cur_falloff) {
    BKE_curvemapping_blend_write(writer, tos->gp_sculpt.cur_falloff);
  }
  /* write grease-pencil primitive curve to file */
  if (tos->gp_sculpt.cur_primitive) {
    BKE_curvemapping_blend_write(writer, tos->gp_sculpt.cur_primitive);
  }
  /* Write the curve profile to the file. */
  if (tos->custom_bevel_profile_preset) {
    BKE_curveprofile_blend_write(writer, tos->custom_bevel_profile_preset);
  }

  write_paint(writer, &tos->imapaint.paint);

  Editing *ed = sce->ed;
  if (ed) {
    Sequence *seq;

    BLO_write_struct(writer, Editing, ed);

    /* reset write flags too */

    SEQ_ALL_BEGIN (ed, seq) {
      if (seq->strip) {
        seq->strip->done = false;
      }
      BLO_write_struct(writer, Sequence, seq);
    }
    SEQ_ALL_END;

    SEQ_ALL_BEGIN (ed, seq) {
      if (seq->strip && seq->strip->done == 0) {
        /* write strip with 'done' at 0 because readfile */

        if (seq->effectdata) {
          switch (seq->type) {
            case SEQ_TYPE_COLOR:
              BLO_write_struct(writer, SolidColorVars, seq->effectdata);
              break;
            case SEQ_TYPE_SPEED:
              BLO_write_struct(writer, SpeedControlVars, seq->effectdata);
              break;
            case SEQ_TYPE_WIPE:
              BLO_write_struct(writer, WipeVars, seq->effectdata);
              break;
            case SEQ_TYPE_GLOW:
              BLO_write_struct(writer, GlowVars, seq->effectdata);
              break;
            case SEQ_TYPE_TRANSFORM:
              BLO_write_struct(writer, TransformVars, seq->effectdata);
              break;
            case SEQ_TYPE_GAUSSIAN_BLUR:
              BLO_write_struct(writer, GaussianBlurVars, seq->effectdata);
              break;
            case SEQ_TYPE_TEXT:
              BLO_write_struct(writer, TextVars, seq->effectdata);
              break;
            case SEQ_TYPE_COLORMIX:
              BLO_write_struct(writer, ColorMixVars, seq->effectdata);
              break;
          }
        }

        BLO_write_struct(writer, Stereo3dFormat, seq->stereo3d_format);

        Strip *strip = seq->strip;
        BLO_write_struct(writer, Strip, strip);
        if (strip->crop) {
          BLO_write_struct(writer, StripCrop, strip->crop);
        }
        if (strip->transform) {
          BLO_write_struct(writer, StripTransform, strip->transform);
        }
        if (strip->proxy) {
          BLO_write_struct(writer, StripProxy, strip->proxy);
        }
        if (seq->type == SEQ_TYPE_IMAGE) {
          BLO_write_struct_array(writer,
                                 StripElem,
                                 MEM_allocN_len(strip->stripdata) / sizeof(struct StripElem),
                                 strip->stripdata);
        }
        else if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
          BLO_write_struct(writer, StripElem, strip->stripdata);
        }

        strip->done = true;
      }

      if (seq->prop) {
        IDP_BlendWrite(writer, seq->prop);
      }

      write_sequence_modifiers(writer, &seq->modifiers);
    }
    SEQ_ALL_END;

    /* new; meta stack too, even when its nasty restore code */
    LISTBASE_FOREACH (MetaStack *, ms, &ed->metastack) {
      BLO_write_struct(writer, MetaStack, ms);
    }
  }

  if (sce->r.avicodecdata) {
    BLO_write_struct(writer, AviCodecData, sce->r.avicodecdata);
    if (sce->r.avicodecdata->lpFormat) {
      BLO_write_raw(writer, (size_t)sce->r.avicodecdata->cbFormat, sce->r.avicodecdata->lpFormat);
    }
    if (sce->r.avicodecdata->lpParms) {
      BLO_write_raw(writer, (size_t)sce->r.avicodecdata->cbParms, sce->r.avicodecdata->lpParms);
    }
  }
  if (sce->r.ffcodecdata.properties) {
    IDP_BlendWrite(writer, sce->r.ffcodecdata.properties);
  }

  /* writing dynamic list of TimeMarkers to the blend file */
  LISTBASE_FOREACH (TimeMarker *, marker, &sce->markers) {
    BLO_write_struct(writer, TimeMarker, marker);

    if (marker->prop != NULL) {
      IDP_BlendWrite(writer, marker->prop);
    }
  }

  /* writing dynamic list of TransformOrientations to the blend file */
  LISTBASE_FOREACH (TransformOrientation *, ts, &sce->transform_spaces) {
    BLO_write_struct(writer, TransformOrientation, ts);
  }

  /* writing MultiView to the blend file */
  LISTBASE_FOREACH (SceneRenderView *, srv, &sce->r.views) {
    BLO_write_struct(writer, SceneRenderView, srv);
  }

  if (sce->nodetree) {
    BLO_write_struct(writer, bNodeTree, sce->nodetree);
    ntreeBlendWrite(writer, sce->nodetree);
  }

  write_view_settings(writer, &sce->view_settings);

  /* writing RigidBodyWorld data to the blend file */
  if (sce->rigidbody_world) {
    /* Set deprecated pointers to prevent crashes of older Blenders */
    sce->rigidbody_world->pointcache = sce->rigidbody_world->shared->pointcache;
    sce->rigidbody_world->ptcaches = sce->rigidbody_world->shared->ptcaches;
    BLO_write_struct(writer, RigidBodyWorld, sce->rigidbody_world);

    BLO_write_struct(writer, RigidBodyWorld_Shared, sce->rigidbody_world->shared);
    BLO_write_struct(writer, EffectorWeights, sce->rigidbody_world->effector_weights);
    write_pointcaches(writer, &(sce->rigidbody_world->shared->ptcaches));
  }

  BKE_previewimg_blend_write(writer, sce->preview);
  BKE_curvemapping_curves_blend_write(writer, &sce->r.mblur_shutter_curve);

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    write_view_layer(writer, view_layer);
  }

  if (sce->master_collection) {
    BLO_write_struct(writer, Collection, sce->master_collection);
    write_collection_nolib(writer, sce->master_collection);
  }

  /* Eevee Lightcache */
  if (sce->eevee.light_cache_data && !BLO_write_is_undo(writer)) {
    BLO_write_struct(writer, LightCache, sce->eevee.light_cache_data);
    write_lightcache(writer, sce->eevee.light_cache_data);
  }

  write_view3dshading(writer, &sce->display.shading);

  /* Freed on doversion. */
  BLI_assert(sce->layer_properties == NULL);
}

static void write_wm_xr_data(BlendWriter *writer, wmXrData *xr_data)
{
  write_view3dshading(writer, &xr_data->session_settings.shading);
}

static void write_region(BlendWriter *writer, ARegion *region, int spacetype)
{
  BLO_write_struct(writer, ARegion, region);

  if (region->regiondata) {
    if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
      return;
    }

    switch (spacetype) {
      case SPACE_VIEW3D:
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = region->regiondata;
          BLO_write_struct(writer, RegionView3D, rv3d);

          if (rv3d->localvd) {
            BLO_write_struct(writer, RegionView3D, rv3d->localvd);
          }
          if (rv3d->clipbb) {
            BLO_write_struct(writer, BoundBox, rv3d->clipbb);
          }
        }
        else {
          printf("regiondata write missing!\n");
        }
        break;
      default:
        printf("regiondata write missing!\n");
    }
  }
}

static void write_uilist(BlendWriter *writer, uiList *ui_list)
{
  BLO_write_struct(writer, uiList, ui_list);

  if (ui_list->properties) {
    IDP_BlendWrite(writer, ui_list->properties);
  }
}

static void write_space_outliner(BlendWriter *writer, SpaceOutliner *space_outliner)
{
  BLI_mempool *ts = space_outliner->treestore;

  if (ts) {
    SpaceOutliner space_outliner_flat = *space_outliner;

    int elems = BLI_mempool_len(ts);
    /* linearize mempool to array */
    TreeStoreElem *data = elems ? BLI_mempool_as_arrayN(ts, "TreeStoreElem") : NULL;

    if (data) {
      /* In this block we use the memory location of the treestore
       * but _not_ its data, the addresses in this case are UUID's,
       * since we can't rely on malloc giving us different values each time.
       */
      TreeStore ts_flat = {0};

      /* we know the treestore is at least as big as a pointer,
       * so offsetting works to give us a UUID. */
      void *data_addr = (void *)POINTER_OFFSET(ts, sizeof(void *));

      ts_flat.usedelem = elems;
      ts_flat.totelem = elems;
      ts_flat.data = data_addr;

      BLO_write_struct(writer, SpaceOutliner, space_outliner);

      BLO_write_struct_at_address(writer, TreeStore, ts, &ts_flat);
      BLO_write_struct_array_at_address(writer, TreeStoreElem, elems, data_addr, data);

      MEM_freeN(data);
    }
    else {
      space_outliner_flat.treestore = NULL;
      BLO_write_struct_at_address(writer, SpaceOutliner, space_outliner, &space_outliner_flat);
    }
  }
  else {
    BLO_write_struct(writer, SpaceOutliner, space_outliner);
  }
}

static void write_panel_list(BlendWriter *writer, ListBase *lb)
{
  LISTBASE_FOREACH (Panel *, panel, lb) {
    BLO_write_struct(writer, Panel, panel);
    write_panel_list(writer, &panel->children);
  }
}

static void write_area_regions(BlendWriter *writer, ScrArea *area)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    write_region(writer, region, area->spacetype);
    write_panel_list(writer, &region->panels);

    LISTBASE_FOREACH (PanelCategoryStack *, pc_act, &region->panels_category_active) {
      BLO_write_struct(writer, PanelCategoryStack, pc_act);
    }

    LISTBASE_FOREACH (uiList *, ui_list, &region->ui_lists) {
      write_uilist(writer, ui_list);
    }

    LISTBASE_FOREACH (uiPreview *, ui_preview, &region->ui_previews) {
      BLO_write_struct(writer, uiPreview, ui_preview);
    }
  }

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
      write_region(writer, region, sl->spacetype);
    }

    if (sl->spacetype == SPACE_VIEW3D) {
      View3D *v3d = (View3D *)sl;
      BLO_write_struct(writer, View3D, v3d);

      if (v3d->localvd) {
        BLO_write_struct(writer, View3D, v3d->localvd);
      }

      write_view3dshading(writer, &v3d->shading);
    }
    else if (sl->spacetype == SPACE_GRAPH) {
      SpaceGraph *sipo = (SpaceGraph *)sl;
      ListBase tmpGhosts = sipo->runtime.ghost_curves;

      /* temporarily disable ghost curves when saving */
      BLI_listbase_clear(&sipo->runtime.ghost_curves);

      BLO_write_struct(writer, SpaceGraph, sl);
      if (sipo->ads) {
        BLO_write_struct(writer, bDopeSheet, sipo->ads);
      }

      /* reenable ghost curves */
      sipo->runtime.ghost_curves = tmpGhosts;
    }
    else if (sl->spacetype == SPACE_PROPERTIES) {
      BLO_write_struct(writer, SpaceProperties, sl);
    }
    else if (sl->spacetype == SPACE_FILE) {
      SpaceFile *sfile = (SpaceFile *)sl;

      BLO_write_struct(writer, SpaceFile, sl);
      if (sfile->params) {
        BLO_write_struct(writer, FileSelectParams, sfile->params);
      }
    }
    else if (sl->spacetype == SPACE_SEQ) {
      BLO_write_struct(writer, SpaceSeq, sl);
    }
    else if (sl->spacetype == SPACE_OUTLINER) {
      SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
      write_space_outliner(writer, space_outliner);
    }
    else if (sl->spacetype == SPACE_IMAGE) {
      BLO_write_struct(writer, SpaceImage, sl);
    }
    else if (sl->spacetype == SPACE_TEXT) {
      BLO_write_struct(writer, SpaceText, sl);
    }
    else if (sl->spacetype == SPACE_SCRIPT) {
      SpaceScript *scr = (SpaceScript *)sl;
      scr->but_refs = NULL;
      BLO_write_struct(writer, SpaceScript, sl);
    }
    else if (sl->spacetype == SPACE_ACTION) {
      BLO_write_struct(writer, SpaceAction, sl);
    }
    else if (sl->spacetype == SPACE_NLA) {
      SpaceNla *snla = (SpaceNla *)sl;

      BLO_write_struct(writer, SpaceNla, snla);
      if (snla->ads) {
        BLO_write_struct(writer, bDopeSheet, snla->ads);
      }
    }
    else if (sl->spacetype == SPACE_NODE) {
      SpaceNode *snode = (SpaceNode *)sl;
      BLO_write_struct(writer, SpaceNode, snode);

      LISTBASE_FOREACH (bNodeTreePath *, path, &snode->treepath) {
        BLO_write_struct(writer, bNodeTreePath, path);
      }
    }
    else if (sl->spacetype == SPACE_CONSOLE) {
      SpaceConsole *con = (SpaceConsole *)sl;

      LISTBASE_FOREACH (ConsoleLine *, cl, &con->history) {
        /* 'len_alloc' is invalid on write, set from 'len' on read */
        BLO_write_struct(writer, ConsoleLine, cl);
        BLO_write_raw(writer, (size_t)cl->len + 1, cl->line);
      }
      BLO_write_struct(writer, SpaceConsole, sl);
    }
#ifdef WITH_GLOBAL_AREA_WRITING
    else if (sl->spacetype == SPACE_TOPBAR) {
      BLO_write_struct(writer, SpaceTopBar, sl);
    }
    else if (sl->spacetype == SPACE_STATUSBAR) {
      BLO_write_struct(writer, SpaceStatusBar, sl);
    }
#endif
    else if (sl->spacetype == SPACE_USERPREF) {
      BLO_write_struct(writer, SpaceUserPref, sl);
    }
    else if (sl->spacetype == SPACE_CLIP) {
      BLO_write_struct(writer, SpaceClip, sl);
    }
    else if (sl->spacetype == SPACE_INFO) {
      BLO_write_struct(writer, SpaceInfo, sl);
    }
  }
}

static void write_area_map(BlendWriter *writer, ScrAreaMap *area_map)
{
  BLO_write_struct_list(writer, ScrVert, &area_map->vertbase);
  BLO_write_struct_list(writer, ScrEdge, &area_map->edgebase);
  LISTBASE_FOREACH (ScrArea *, area, &area_map->areabase) {
    area->butspacetype = area->spacetype; /* Just for compatibility, will be reset below. */

    BLO_write_struct(writer, ScrArea, area);

#ifdef WITH_GLOBAL_AREA_WRITING
    BLO_write_struct(writer, ScrGlobalAreaData, area->global);
#endif

    write_area_regions(writer, area);

    area->butspacetype = SPACE_EMPTY; /* Unset again, was changed above. */
  }
}

static void write_windowmanager(BlendWriter *writer, wmWindowManager *wm, const void *id_address)
{
  BLO_write_id_struct(writer, wmWindowManager, id_address, &wm->id);
  BKE_id_blend_write(writer, &wm->id);
  write_wm_xr_data(writer, &wm->xr);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
#ifndef WITH_GLOBAL_AREA_WRITING
    /* Don't write global areas yet, while we make changes to them. */
    ScrAreaMap global_areas = win->global_areas;
    memset(&win->global_areas, 0, sizeof(win->global_areas));
#endif

    /* update deprecated screen member (for so loading in 2.7x uses the correct screen) */
    win->screen = BKE_workspace_active_screen_get(win->workspace_hook);

    BLO_write_struct(writer, wmWindow, win);
    BLO_write_struct(writer, WorkSpaceInstanceHook, win->workspace_hook);
    BLO_write_struct(writer, Stereo3dFormat, win->stereo3d_format);

#ifdef WITH_GLOBAL_AREA_WRITING
    write_area_map(writer, &win->global_areas);
#else
    win->global_areas = global_areas;
#endif

    /* data is written, clear deprecated data again */
    win->screen = NULL;
  }
}

static void write_screen(BlendWriter *writer, bScreen *screen, const void *id_address)
{
  /* Screens are reference counted, only saved if used by a workspace. */
  if (screen->id.us > 0 || BLO_write_is_undo(writer)) {
    /* write LibData */
    /* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
    writestruct_at_address(writer->wd, ID_SCRN, bScreen, 1, id_address, screen);
    BKE_id_blend_write(writer, &screen->id);

    BKE_previewimg_blend_write(writer, screen->preview);

    /* direct data */
    write_area_map(writer, AREAMAP_FROM_SCREEN(screen));
  }
}

static void write_workspace(BlendWriter *writer, WorkSpace *workspace, const void *id_address)
{
  BLO_write_id_struct(writer, WorkSpace, id_address, &workspace->id);
  BKE_id_blend_write(writer, &workspace->id);
  BLO_write_struct_list(writer, WorkSpaceLayout, &workspace->layouts);
  BLO_write_struct_list(writer, WorkSpaceDataRelation, &workspace->hook_layout_relations);
  BLO_write_struct_list(writer, wmOwnerID, &workspace->owner_ids);
  BLO_write_struct_list(writer, bToolRef, &workspace->tools);
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if (tref->properties) {
      IDP_BlendWrite(writer, tref->properties);
    }
  }
}

/* Keep it last of write_foodata functions. */
static void write_libraries(WriteData *wd, Main *main)
{
  ListBase *lbarray[MAX_LIBARRAY];
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

  /* The windowmanager and screen often change,
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
    ListBase *lbarray[MAX_LIBARRAY];
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

        switch ((ID_Type)GS(id->name)) {
          case ID_WM:
            write_windowmanager(&writer, (wmWindowManager *)id_buffer, id);
            break;
          case ID_WS:
            write_workspace(&writer, (WorkSpace *)id_buffer, id);
            break;
          case ID_SCR:
            write_screen(&writer, (bScreen *)id_buffer, id);
            break;
          case ID_SCE:
            write_scene(&writer, (Scene *)id_buffer, id);
            break;
          case ID_GR:
            write_collection(&writer, (Collection *)id_buffer, id);
            break;
          case ID_OB:
            write_object(&writer, (Object *)id_buffer, id);
            break;
          case ID_PA:
            write_particlesettings(&writer, (ParticleSettings *)id_buffer, id);
            break;
          case ID_ME:
          case ID_LT:
          case ID_AC:
          case ID_NT:
          case ID_LS:
          case ID_TXT:
          case ID_VF:
          case ID_MC:
          case ID_PC:
          case ID_PAL:
          case ID_BR:
          case ID_IM:
          case ID_LA:
          case ID_MA:
          case ID_MB:
          case ID_CU:
          case ID_CA:
          case ID_WO:
          case ID_MSK:
          case ID_SPK:
          case ID_AR:
          case ID_LP:
          case ID_KE:
          case ID_TE:
          case ID_GD:
          case ID_HA:
          case ID_PT:
          case ID_VO:
          case ID_SIM:
          case ID_SO:
          case ID_CF:
            /* Do nothing, handled in IDTypeInfo callback. */
            break;
          case ID_LI:
            /* Do nothing, handled below - and should never be reached. */
            BLI_assert(0);
            break;
          case ID_IP:
            /* Do nothing, deprecated. */
            break;
          default:
            /* Should never be reached. */
            BLI_assert(0);
            break;
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
  writestruct_at_address_nr(writer->wd, DATA, struct_id, 1, address, data_ptr);
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
