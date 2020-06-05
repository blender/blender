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
#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_fluid_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_layer_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_simulation_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_volume_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_mempool.h"
#include "MEM_guardedalloc.h"  // MEM_freeN

#include "BKE_action.h"
#include "BKE_blender_version.h"
#include "BKE_bpath.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.h"  // for G
#include "BKE_gpencil_modifier.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
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

/* for SDNA_TYPE_FROM_STRUCT() macro */
#include "dna_type_offsets.h"

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
  else {
    return false;
  }
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
  else {
    return false;
  }
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
  int buf_used_len;

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

static void writedata_do_write(WriteData *wd, const void *mem, int memlen)
{
  if ((wd == NULL) || wd->error || (mem == NULL) || memlen < 1) {
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
  if (wd->buf_used_len) {
    writedata_do_write(wd, wd->buf, wd->buf_used_len);
    wd->buf_used_len = 0;
  }
}

/**
 * Low level WRITE(2) wrapper that buffers data
 * \param adr: Pointer to new chunk of data
 * \param len: Length of new chunk of data
 */
static void mywrite(WriteData *wd, const void *adr, int len)
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
      if (wd->buf_used_len) {
        writedata_do_write(wd, wd->buf, wd->buf_used_len);
        wd->buf_used_len = 0;
      }

      do {
        int writelen = MIN2(len, MYWRITE_MAX_CHUNK);
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
  if (wd->buf_used_len) {
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
  const short *sp;

  BLI_assert(struct_nr > 0 && struct_nr < SDNA_TYPE_MAX);

  if (adr == NULL || data == NULL || nr == 0) {
    return;
  }

  /* init BHead */
  bh.code = filecode;
  bh.old = adr;
  bh.nr = nr;

  bh.SDNAnr = struct_nr;
  sp = wd->sdna->structs[bh.SDNAnr];

  bh.len = nr * wd->sdna->types_size[sp[0]];

  if (bh.len == 0) {
    return;
  }

  mywrite(wd, &bh, sizeof(BHead));
  mywrite(wd, data, bh.len);
}

static void writestruct_at_address_id(
    WriteData *wd, int filecode, const char *structname, int nr, const void *adr, const void *data)
{
  if (adr == NULL || data == NULL || nr == 0) {
    return;
  }

  const int SDNAnr = DNA_struct_find_nr(wd->sdna, structname);
  if (UNLIKELY(SDNAnr == -1)) {
    printf("error: can't find SDNA code <%s>\n", structname);
    return;
  }

  writestruct_at_address_nr(wd, filecode, SDNAnr, nr, adr, data);
}

static void writestruct_nr(
    WriteData *wd, int filecode, const int struct_nr, int nr, const void *adr)
{
  writestruct_at_address_nr(wd, filecode, struct_nr, nr, adr, adr);
}

static void writestruct_id(
    WriteData *wd, int filecode, const char *structname, int nr, const void *adr)
{
  writestruct_at_address_id(wd, filecode, structname, nr, adr, adr);
}

/* do not use for structs */
static void writedata(WriteData *wd, int filecode, int len, const void *adr)
{
  BHead bh;

  if (adr == NULL || len == 0) {
    return;
  }

  /* align to 4 (writes uninitialized bytes in some cases) */
  len = (len + 3) & ~3;

  /* init BHead */
  bh.code = filecode;
  bh.old = adr;
  bh.nr = 1;
  bh.SDNAnr = 0;
  bh.len = len;

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

void IDP_WriteProperty_OnlyData(const IDProperty *prop, BlendWriter *writer);
void IDP_WriteProperty(const IDProperty *prop, WriteData *wd);
void IDP_WriteProperty_new_api(const IDProperty *prop, BlendWriter *writer);

static void IDP_WriteArray(const IDProperty *prop, BlendWriter *writer)
{
  /*REMEMBER to set totalen to len in the linking code!!*/
  if (prop->data.pointer) {
    BLO_write_raw(writer, MEM_allocN_len(prop->data.pointer), prop->data.pointer);

    if (prop->subtype == IDP_GROUP) {
      IDProperty **array = prop->data.pointer;
      int a;

      for (a = 0; a < prop->len; a++) {
        IDP_WriteProperty_new_api(array[a], writer);
      }
    }
  }
}

static void IDP_WriteIDPArray(const IDProperty *prop, BlendWriter *writer)
{
  /*REMEMBER to set totalen to len in the linking code!!*/
  if (prop->data.pointer) {
    const IDProperty *array = prop->data.pointer;
    int a;

    BLO_write_struct_array(writer, IDProperty, prop->len, array);

    for (a = 0; a < prop->len; a++) {
      IDP_WriteProperty_OnlyData(&array[a], writer);
    }
  }
}

static void IDP_WriteString(const IDProperty *prop, BlendWriter *writer)
{
  /*REMEMBER to set totalen to len in the linking code!!*/
  BLO_write_raw(writer, prop->len, prop->data.pointer);
}

static void IDP_WriteGroup(const IDProperty *prop, BlendWriter *writer)
{
  IDProperty *loop;

  for (loop = prop->data.group.first; loop; loop = loop->next) {
    IDP_WriteProperty_new_api(loop, writer);
  }
}

/* Functions to read/write ID Properties */
void IDP_WriteProperty_OnlyData(const IDProperty *prop, BlendWriter *writer)
{
  switch (prop->type) {
    case IDP_GROUP:
      IDP_WriteGroup(prop, writer);
      break;
    case IDP_STRING:
      IDP_WriteString(prop, writer);
      break;
    case IDP_ARRAY:
      IDP_WriteArray(prop, writer);
      break;
    case IDP_IDPARRAY:
      IDP_WriteIDPArray(prop, writer);
      break;
  }
}

void IDP_WriteProperty_new_api(const IDProperty *prop, BlendWriter *writer)
{
  BLO_write_struct(writer, IDProperty, prop);
  IDP_WriteProperty_OnlyData(prop, writer);
}

void IDP_WriteProperty(const IDProperty *prop, WriteData *wd)
{
  BlendWriter writer = {wd};
  IDP_WriteProperty_new_api(prop, &writer);
}

static void write_iddata(WriteData *wd, ID *id)
{
  /* ID_WM's id->properties are considered runtime only, and never written in .blend file. */
  if (id->properties && !ELEM(GS(id->name), ID_WM)) {
    IDP_WriteProperty(id->properties, wd);
  }

  if (id->override_library) {
    writestruct(wd, DATA, IDOverrideLibrary, 1, id->override_library);

    writelist(wd, DATA, IDOverrideLibraryProperty, &id->override_library->properties);
    LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &id->override_library->properties) {
      writedata(wd, DATA, strlen(op->rna_path) + 1, op->rna_path);

      writelist(wd, DATA, IDOverrideLibraryPropertyOperation, &op->operations);
      LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
        if (opop->subitem_reference_name) {
          writedata(
              wd, DATA, strlen(opop->subitem_reference_name) + 1, opop->subitem_reference_name);
        }
        if (opop->subitem_local_name) {
          writedata(wd, DATA, strlen(opop->subitem_local_name) + 1, opop->subitem_local_name);
        }
      }
    }
  }
}

static void write_previews(WriteData *wd, const PreviewImage *prv_orig)
{
  /* Note we write previews also for undo steps. It takes up some memory,
   * but not doing so would causes all previews to be re-rendered after
   * undo which is too expensive. */
  if (prv_orig) {
    PreviewImage prv = *prv_orig;

    /* don't write out large previews if not requested */
    if (!(U.flag & USER_SAVE_PREVIEWS)) {
      prv.w[1] = 0;
      prv.h[1] = 0;
      prv.rect[1] = NULL;
    }
    writestruct_at_address(wd, DATA, PreviewImage, 1, prv_orig, &prv);
    if (prv.rect[0]) {
      writedata(wd, DATA, prv.w[0] * prv.h[0] * sizeof(uint), prv.rect[0]);
    }
    if (prv.rect[1]) {
      writedata(wd, DATA, prv.w[1] * prv.h[1] * sizeof(uint), prv.rect[1]);
    }
  }
}

static void write_fmodifiers(WriteData *wd, ListBase *fmodifiers)
{
  FModifier *fcm;

  /* Write all modifiers first (for faster reloading) */
  writelist(wd, DATA, FModifier, fmodifiers);

  /* Modifiers */
  for (fcm = fmodifiers->first; fcm; fcm = fcm->next) {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

    /* Write the specific data */
    if (fmi && fcm->data) {
      /* firstly, just write the plain fmi->data struct */
      writestruct_id(wd, DATA, fmi->structName, 1, fcm->data);

      /* do any modifier specific stuff */
      switch (fcm->type) {
        case FMODIFIER_TYPE_GENERATOR: {
          FMod_Generator *data = fcm->data;

          /* write coefficients array */
          if (data->coefficients) {
            writedata(wd, DATA, sizeof(float) * (data->arraysize), data->coefficients);
          }

          break;
        }
        case FMODIFIER_TYPE_ENVELOPE: {
          FMod_Envelope *data = fcm->data;

          /* write envelope data */
          if (data->data) {
            writestruct(wd, DATA, FCM_EnvelopeData, data->totvert, data->data);
          }

          break;
        }
        case FMODIFIER_TYPE_PYTHON: {
          FMod_Python *data = fcm->data;

          /* Write ID Properties -- and copy this comment EXACTLY for easy finding
           * of library blocks that implement this.*/
          IDP_WriteProperty(data->prop, wd);

          break;
        }
      }
    }
  }
}

static void write_fcurves(WriteData *wd, ListBase *fcurves)
{
  FCurve *fcu;

  writelist(wd, DATA, FCurve, fcurves);
  for (fcu = fcurves->first; fcu; fcu = fcu->next) {
    /* curve data */
    if (fcu->bezt) {
      writestruct(wd, DATA, BezTriple, fcu->totvert, fcu->bezt);
    }
    if (fcu->fpt) {
      writestruct(wd, DATA, FPoint, fcu->totvert, fcu->fpt);
    }

    if (fcu->rna_path) {
      writedata(wd, DATA, strlen(fcu->rna_path) + 1, fcu->rna_path);
    }

    /* driver data */
    if (fcu->driver) {
      ChannelDriver *driver = fcu->driver;
      DriverVar *dvar;

      writestruct(wd, DATA, ChannelDriver, 1, driver);

      /* variables */
      writelist(wd, DATA, DriverVar, &driver->variables);
      for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
        DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
          if (dtar->rna_path) {
            writedata(wd, DATA, strlen(dtar->rna_path) + 1, dtar->rna_path);
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }

    /* write F-Modifiers */
    write_fmodifiers(wd, &fcu->modifiers);
  }
}

static void write_action(WriteData *wd, bAction *act, const void *id_address)
{
  if (act->id.us > 0 || wd->use_memfile) {
    writestruct_at_address(wd, ID_AC, bAction, 1, id_address, act);
    write_iddata(wd, &act->id);

    write_fcurves(wd, &act->curves);

    LISTBASE_FOREACH (bActionGroup *, grp, &act->groups) {
      writestruct(wd, DATA, bActionGroup, 1, grp);
    }

    LISTBASE_FOREACH (TimeMarker *, marker, &act->markers) {
      writestruct(wd, DATA, TimeMarker, 1, marker);
    }
  }
}

static void write_keyingsets(WriteData *wd, ListBase *list)
{
  KeyingSet *ks;
  KS_Path *ksp;

  for (ks = list->first; ks; ks = ks->next) {
    /* KeyingSet */
    writestruct(wd, DATA, KeyingSet, 1, ks);

    /* Paths */
    for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
      /* Path */
      writestruct(wd, DATA, KS_Path, 1, ksp);

      if (ksp->rna_path) {
        writedata(wd, DATA, strlen(ksp->rna_path) + 1, ksp->rna_path);
      }
    }
  }
}

static void write_nlastrips(WriteData *wd, ListBase *strips)
{
  NlaStrip *strip;

  writelist(wd, DATA, NlaStrip, strips);
  for (strip = strips->first; strip; strip = strip->next) {
    /* write the strip's F-Curves and modifiers */
    write_fcurves(wd, &strip->fcurves);
    write_fmodifiers(wd, &strip->modifiers);

    /* write the strip's children */
    write_nlastrips(wd, &strip->strips);
  }
}

static void write_nladata(WriteData *wd, ListBase *nlabase)
{
  NlaTrack *nlt;

  /* write all the tracks */
  for (nlt = nlabase->first; nlt; nlt = nlt->next) {
    /* write the track first */
    writestruct(wd, DATA, NlaTrack, 1, nlt);

    /* write the track's strips */
    write_nlastrips(wd, &nlt->strips);
  }
}

static void write_animdata(WriteData *wd, AnimData *adt)
{
  AnimOverride *aor;

  /* firstly, just write the AnimData block */
  writestruct(wd, DATA, AnimData, 1, adt);

  /* write drivers */
  write_fcurves(wd, &adt->drivers);

  /* write overrides */
  // FIXME: are these needed?
  for (aor = adt->overrides.first; aor; aor = aor->next) {
    /* overrides consist of base data + rna_path */
    writestruct(wd, DATA, AnimOverride, 1, aor);
    writedata(wd, DATA, strlen(aor->rna_path) + 1, aor->rna_path);
  }

  // TODO write the remaps (if they are needed)

  /* write NLA data */
  write_nladata(wd, &adt->nla_tracks);
}

static void write_curvemapping_curves(WriteData *wd, CurveMapping *cumap)
{
  for (int a = 0; a < CM_TOT; a++) {
    writestruct(wd, DATA, CurveMapPoint, cumap->cm[a].totpoint, cumap->cm[a].curve);
  }
}

static void write_curvemapping(WriteData *wd, CurveMapping *cumap)
{
  writestruct(wd, DATA, CurveMapping, 1, cumap);

  write_curvemapping_curves(wd, cumap);
}

static void write_CurveProfile(WriteData *wd, CurveProfile *profile)
{
  writestruct(wd, DATA, CurveProfile, 1, profile);
  writestruct(wd, DATA, CurveProfilePoint, profile->path_len, profile->path);
}

static void write_node_socket_default_value(WriteData *wd, bNodeSocket *sock)
{
  if (sock->default_value == NULL) {
    return;
  }

  switch ((eNodeSocketDatatype)sock->type) {
    case SOCK_FLOAT:
      writestruct(wd, DATA, bNodeSocketValueFloat, 1, sock->default_value);
      break;
    case SOCK_VECTOR:
      writestruct(wd, DATA, bNodeSocketValueVector, 1, sock->default_value);
      break;
    case SOCK_RGBA:
      writestruct(wd, DATA, bNodeSocketValueRGBA, 1, sock->default_value);
      break;
    case SOCK_BOOLEAN:
      writestruct(wd, DATA, bNodeSocketValueBoolean, 1, sock->default_value);
      break;
    case SOCK_INT:
      writestruct(wd, DATA, bNodeSocketValueInt, 1, sock->default_value);
      break;
    case SOCK_STRING:
      writestruct(wd, DATA, bNodeSocketValueString, 1, sock->default_value);
      break;
    case SOCK_OBJECT:
      writestruct(wd, DATA, bNodeSocketValueObject, 1, sock->default_value);
      break;
    case SOCK_IMAGE:
      writestruct(wd, DATA, bNodeSocketValueImage, 1, sock->default_value);
      break;
    case __SOCK_MESH:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_EMITTERS:
    case SOCK_EVENTS:
    case SOCK_FORCES:
    case SOCK_CONTROL_FLOW:
      BLI_assert(false);
      break;
  }
}

static void write_node_socket(WriteData *wd, bNodeSocket *sock)
{
  /* actual socket writing */
  writestruct(wd, DATA, bNodeSocket, 1, sock);

  if (sock->prop) {
    IDP_WriteProperty(sock->prop, wd);
  }

  write_node_socket_default_value(wd, sock);
}
static void write_node_socket_interface(WriteData *wd, bNodeSocket *sock)
{
  /* actual socket writing */
  writestruct(wd, DATA, bNodeSocket, 1, sock);

  if (sock->prop) {
    IDP_WriteProperty(sock->prop, wd);
  }

  write_node_socket_default_value(wd, sock);
}
/* this is only direct data, tree itself should have been written */
static void write_nodetree_nolib(WriteData *wd, bNodeTree *ntree)
{
  bNode *node;
  bNodeSocket *sock;
  bNodeLink *link;

  /* for link_list() speed, we write per list */

  if (ntree->adt) {
    write_animdata(wd, ntree->adt);
  }

  for (node = ntree->nodes.first; node; node = node->next) {
    writestruct(wd, DATA, bNode, 1, node);

    if (node->prop) {
      IDP_WriteProperty(node->prop, wd);
    }

    for (sock = node->inputs.first; sock; sock = sock->next) {
      write_node_socket(wd, sock);
    }
    for (sock = node->outputs.first; sock; sock = sock->next) {
      write_node_socket(wd, sock);
    }

    for (link = node->internal_links.first; link; link = link->next) {
      writestruct(wd, DATA, bNodeLink, 1, link);
    }

    if (node->storage) {
      /* could be handlerized at some point, now only 1 exception still */
      if ((ntree->type == NTREE_SHADER) &&
          ELEM(node->type, SH_NODE_CURVE_VEC, SH_NODE_CURVE_RGB)) {
        write_curvemapping(wd, node->storage);
      }
      else if (ntree->type == NTREE_SHADER && (node->type == SH_NODE_SCRIPT)) {
        NodeShaderScript *nss = (NodeShaderScript *)node->storage;
        if (nss->bytecode) {
          writedata(wd, DATA, strlen(nss->bytecode) + 1, nss->bytecode);
        }
        writestruct_id(wd, DATA, node->typeinfo->storagename, 1, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && ELEM(node->type,
                                                       CMP_NODE_TIME,
                                                       CMP_NODE_CURVE_VEC,
                                                       CMP_NODE_CURVE_RGB,
                                                       CMP_NODE_HUECORRECT)) {
        write_curvemapping(wd, node->storage);
      }
      else if ((ntree->type == NTREE_TEXTURE) &&
               (node->type == TEX_NODE_CURVE_RGB || node->type == TEX_NODE_CURVE_TIME)) {
        write_curvemapping(wd, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_MOVIEDISTORTION)) {
        /* pass */
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_GLARE)) {
        /* Simple forward compatibility for fix for T50736.
         * Not ideal (there is no ideal solution here), but should do for now. */
        NodeGlare *ndg = node->storage;
        /* Not in undo case. */
        if (wd->use_memfile == false) {
          switch (ndg->type) {
            case 2: /* Grrrr! magic numbers :( */
              ndg->angle = ndg->streaks;
              break;
            case 0:
              ndg->angle = ndg->star_45;
              break;
            default:
              break;
          }
        }
        writestruct_id(wd, DATA, node->typeinfo->storagename, 1, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_CRYPTOMATTE)) {
        NodeCryptomatte *nc = (NodeCryptomatte *)node->storage;
        if (nc->matte_id) {
          writedata(wd, DATA, strlen(nc->matte_id) + 1, nc->matte_id);
        }
        writestruct_id(wd, DATA, node->typeinfo->storagename, 1, node->storage);
      }
      else {
        writestruct_id(wd, DATA, node->typeinfo->storagename, 1, node->storage);
      }
    }

    if (node->type == CMP_NODE_OUTPUT_FILE) {
      /* inputs have own storage data */
      for (sock = node->inputs.first; sock; sock = sock->next) {
        writestruct(wd, DATA, NodeImageMultiFileSocket, 1, sock->storage);
      }
    }
    if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
      /* write extra socket info */
      for (sock = node->outputs.first; sock; sock = sock->next) {
        writestruct(wd, DATA, NodeImageLayer, 1, sock->storage);
      }
    }
  }

  for (link = ntree->links.first; link; link = link->next) {
    writestruct(wd, DATA, bNodeLink, 1, link);
  }

  for (sock = ntree->inputs.first; sock; sock = sock->next) {
    write_node_socket_interface(wd, sock);
  }
  for (sock = ntree->outputs.first; sock; sock = sock->next) {
    write_node_socket_interface(wd, sock);
  }
}

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
  Scene *sce, *curscene = NULL;
  ViewLayer *view_layer;
  RenderInfo data;

  /* XXX in future, handle multiple windows with multiple screens? */
  current_screen_compat(mainvar, false, &curscreen, &curscene, &view_layer);

  for (sce = mainvar->scenes.first; sce; sce = sce->id.next) {
    if (sce->id.lib == NULL && (sce == curscene || (sce->r.scemode & R_BG_RENDER))) {
      data.sfra = sce->r.sfra;
      data.efra = sce->r.efra;
      memset(data.scene_name, 0, sizeof(data.scene_name));

      BLI_strncpy(data.scene_name, sce->id.name + 2, sizeof(data.scene_name));

      writedata(wd, REND, sizeof(data), &data);
    }
  }
}

static void write_keymapitem(WriteData *wd, const wmKeyMapItem *kmi)
{
  writestruct(wd, DATA, wmKeyMapItem, 1, kmi);
  if (kmi->properties) {
    IDP_WriteProperty(kmi->properties, wd);
  }
}

static void write_userdef(WriteData *wd, const UserDef *userdef)
{
  writestruct(wd, USER, UserDef, 1, userdef);

  LISTBASE_FOREACH (const bTheme *, btheme, &userdef->themes) {
    writestruct(wd, DATA, bTheme, 1, btheme);
  }

  LISTBASE_FOREACH (const wmKeyMap *, keymap, &userdef->user_keymaps) {
    writestruct(wd, DATA, wmKeyMap, 1, keymap);

    LISTBASE_FOREACH (const wmKeyMapDiffItem *, kmdi, &keymap->diff_items) {
      writestruct(wd, DATA, wmKeyMapDiffItem, 1, kmdi);
      if (kmdi->remove_item) {
        write_keymapitem(wd, kmdi->remove_item);
      }
      if (kmdi->add_item) {
        write_keymapitem(wd, kmdi->add_item);
      }
    }

    LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &keymap->items) {
      write_keymapitem(wd, kmi);
    }
  }

  LISTBASE_FOREACH (const wmKeyConfigPref *, kpt, &userdef->user_keyconfig_prefs) {
    writestruct(wd, DATA, wmKeyConfigPref, 1, kpt);
    if (kpt->prop) {
      IDP_WriteProperty(kpt->prop, wd);
    }
  }

  LISTBASE_FOREACH (const bUserMenu *, um, &userdef->user_menus) {
    writestruct(wd, DATA, bUserMenu, 1, um);
    LISTBASE_FOREACH (const bUserMenuItem *, umi, &um->items) {
      if (umi->type == USER_MENU_TYPE_OPERATOR) {
        const bUserMenuItem_Op *umi_op = (const bUserMenuItem_Op *)umi;
        writestruct(wd, DATA, bUserMenuItem_Op, 1, umi_op);
        if (umi_op->prop) {
          IDP_WriteProperty(umi_op->prop, wd);
        }
      }
      else if (umi->type == USER_MENU_TYPE_MENU) {
        const bUserMenuItem_Menu *umi_mt = (const bUserMenuItem_Menu *)umi;
        writestruct(wd, DATA, bUserMenuItem_Menu, 1, umi_mt);
      }
      else if (umi->type == USER_MENU_TYPE_PROP) {
        const bUserMenuItem_Prop *umi_pr = (const bUserMenuItem_Prop *)umi;
        writestruct(wd, DATA, bUserMenuItem_Prop, 1, umi_pr);
      }
      else {
        writestruct(wd, DATA, bUserMenuItem, 1, umi);
      }
    }
  }

  LISTBASE_FOREACH (const bAddon *, bext, &userdef->addons) {
    writestruct(wd, DATA, bAddon, 1, bext);
    if (bext->prop) {
      IDP_WriteProperty(bext->prop, wd);
    }
  }

  LISTBASE_FOREACH (const bPathCompare *, path_cmp, &userdef->autoexec_paths) {
    writestruct(wd, DATA, bPathCompare, 1, path_cmp);
  }

  LISTBASE_FOREACH (const uiStyle *, style, &userdef->uistyles) {
    writestruct(wd, DATA, uiStyle, 1, style);
  }
}

static void write_boid_state(WriteData *wd, BoidState *state)
{
  BoidRule *rule = state->rules.first;

  writestruct(wd, DATA, BoidState, 1, state);

  for (; rule; rule = rule->next) {
    switch (rule->type) {
      case eBoidRuleType_Goal:
      case eBoidRuleType_Avoid:
        writestruct(wd, DATA, BoidRuleGoalAvoid, 1, rule);
        break;
      case eBoidRuleType_AvoidCollision:
        writestruct(wd, DATA, BoidRuleAvoidCollision, 1, rule);
        break;
      case eBoidRuleType_FollowLeader:
        writestruct(wd, DATA, BoidRuleFollowLeader, 1, rule);
        break;
      case eBoidRuleType_AverageSpeed:
        writestruct(wd, DATA, BoidRuleAverageSpeed, 1, rule);
        break;
      case eBoidRuleType_Fight:
        writestruct(wd, DATA, BoidRuleFight, 1, rule);
        break;
      default:
        writestruct(wd, DATA, BoidRule, 1, rule);
        break;
    }
  }
#if 0
  BoidCondition *cond = state->conditions.first;
  for (; cond; cond = cond->next) {
    writestruct(wd, DATA, BoidCondition, 1, cond);
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
};
static void write_pointcaches(WriteData *wd, ListBase *ptcaches)
{
  PointCache *cache = ptcaches->first;
  int i;

  for (; cache; cache = cache->next) {
    writestruct(wd, DATA, PointCache, 1, cache);

    if ((cache->flag & PTCACHE_DISK_CACHE) == 0) {
      PTCacheMem *pm = cache->mem_cache.first;

      for (; pm; pm = pm->next) {
        PTCacheExtra *extra = pm->extradata.first;

        writestruct(wd, DATA, PTCacheMem, 1, pm);

        for (i = 0; i < BPHYS_TOT_DATA; i++) {
          if (pm->data[i] && pm->data_types & (1 << i)) {
            if (ptcache_data_struct[i][0] == '\0') {
              writedata(wd, DATA, MEM_allocN_len(pm->data[i]), pm->data[i]);
            }
            else {
              writestruct_id(wd, DATA, ptcache_data_struct[i], pm->totpoint, pm->data[i]);
            }
          }
        }

        for (; extra; extra = extra->next) {
          if (ptcache_extra_struct[extra->type][0] == '\0') {
            continue;
          }
          writestruct(wd, DATA, PTCacheExtra, 1, extra);
          writestruct_id(wd, DATA, ptcache_extra_struct[extra->type], extra->totdata, extra->data);
        }
      }
    }
  }
}

static void write_particlesettings(WriteData *wd, ParticleSettings *part, const void *id_address)
{
  if (part->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_PA, ParticleSettings, 1, id_address, part);
    write_iddata(wd, &part->id);

    if (part->adt) {
      write_animdata(wd, part->adt);
    }
    writestruct(wd, DATA, PartDeflect, 1, part->pd);
    writestruct(wd, DATA, PartDeflect, 1, part->pd2);
    writestruct(wd, DATA, EffectorWeights, 1, part->effector_weights);

    if (part->clumpcurve) {
      write_curvemapping(wd, part->clumpcurve);
    }
    if (part->roughcurve) {
      write_curvemapping(wd, part->roughcurve);
    }
    if (part->twistcurve) {
      write_curvemapping(wd, part->twistcurve);
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
      writestruct(wd, DATA, ParticleDupliWeight, 1, dw);
    }

    if (part->boids && part->phystype == PART_PHYS_BOIDS) {
      writestruct(wd, DATA, BoidSettings, 1, part->boids);

      LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
        write_boid_state(wd, state);
      }
    }
    if (part->fluid && part->phystype == PART_PHYS_FLUID) {
      writestruct(wd, DATA, SPHFluidSettings, 1, part->fluid);
    }

    for (int a = 0; a < MAX_MTEX; a++) {
      if (part->mtex[a]) {
        writestruct(wd, DATA, MTex, 1, part->mtex[a]);
      }
    }
  }
}

static void write_particlesystems(WriteData *wd, ListBase *particles)
{
  ParticleSystem *psys = particles->first;
  ParticleTarget *pt;
  int a;

  for (; psys; psys = psys->next) {
    writestruct(wd, DATA, ParticleSystem, 1, psys);

    if (psys->particles) {
      writestruct(wd, DATA, ParticleData, psys->totpart, psys->particles);

      if (psys->particles->hair) {
        ParticleData *pa = psys->particles;

        for (a = 0; a < psys->totpart; a++, pa++) {
          writestruct(wd, DATA, HairKey, pa->totkey, pa->hair);
        }
      }

      if (psys->particles->boid && (psys->part->phystype == PART_PHYS_BOIDS)) {
        writestruct(wd, DATA, BoidParticle, psys->totpart, psys->particles->boid);
      }

      if (psys->part->fluid && (psys->part->phystype == PART_PHYS_FLUID) &&
          (psys->part->fluid->flag & SPH_VISCOELASTIC_SPRINGS)) {
        writestruct(wd, DATA, ParticleSpring, psys->tot_fluidsprings, psys->fluid_springs);
      }
    }
    pt = psys->targets.first;
    for (; pt; pt = pt->next) {
      writestruct(wd, DATA, ParticleTarget, 1, pt);
    }

    if (psys->child) {
      writestruct(wd, DATA, ChildParticle, psys->totchild, psys->child);
    }

    if (psys->clmd) {
      writestruct(wd, DATA, ClothModifierData, 1, psys->clmd);
      writestruct(wd, DATA, ClothSimSettings, 1, psys->clmd->sim_parms);
      writestruct(wd, DATA, ClothCollSettings, 1, psys->clmd->coll_parms);
    }

    write_pointcaches(wd, &psys->ptcaches);
  }
}

static void write_motionpath(WriteData *wd, bMotionPath *mpath)
{
  /* sanity checks */
  if (mpath == NULL) {
    return;
  }

  /* firstly, just write the motionpath struct */
  writestruct(wd, DATA, bMotionPath, 1, mpath);

  /* now write the array of data */
  writestruct(wd, DATA, bMotionPathVert, mpath->length, mpath->points);
}

static void write_constraints(WriteData *wd, ListBase *conlist)
{
  bConstraint *con;

  for (con = conlist->first; con; con = con->next) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    /* Write the specific data */
    if (cti && con->data) {
      /* firstly, just write the plain con->data struct */
      writestruct_id(wd, DATA, cti->structName, 1, con->data);

      /* do any constraint specific stuff */
      switch (con->type) {
        case CONSTRAINT_TYPE_PYTHON: {
          bPythonConstraint *data = con->data;
          bConstraintTarget *ct;

          /* write targets */
          for (ct = data->targets.first; ct; ct = ct->next) {
            writestruct(wd, DATA, bConstraintTarget, 1, ct);
          }

          /* Write ID Properties -- and copy this comment EXACTLY for easy finding
           * of library blocks that implement this.*/
          IDP_WriteProperty(data->prop, wd);

          break;
        }
        case CONSTRAINT_TYPE_ARMATURE: {
          bArmatureConstraint *data = con->data;
          bConstraintTarget *ct;

          /* write targets */
          for (ct = data->targets.first; ct; ct = ct->next) {
            writestruct(wd, DATA, bConstraintTarget, 1, ct);
          }

          break;
        }
        case CONSTRAINT_TYPE_SPLINEIK: {
          bSplineIKConstraint *data = con->data;

          /* write points array */
          writedata(wd, DATA, sizeof(float) * (data->numpoints), data->points);

          break;
        }
      }
    }

    /* Write the constraint */
    writestruct(wd, DATA, bConstraint, 1, con);
  }
}

static void write_pose(WriteData *wd, bPose *pose)
{
  bPoseChannel *chan;
  bActionGroup *grp;

  /* Write each channel */
  if (pose == NULL) {
    return;
  }

  /* Write channels */
  for (chan = pose->chanbase.first; chan; chan = chan->next) {
    /* Write ID Properties -- and copy this comment EXACTLY for easy finding
     * of library blocks that implement this.*/
    if (chan->prop) {
      IDP_WriteProperty(chan->prop, wd);
    }

    write_constraints(wd, &chan->constraints);

    write_motionpath(wd, chan->mpath);

    /* prevent crashes with autosave,
     * when a bone duplicated in editmode has not yet been assigned to its posechannel */
    if (chan->bone) {
      /* gets restored on read, for library armatures */
      chan->selectflag = chan->bone->flag & BONE_SELECTED;
    }

    writestruct(wd, DATA, bPoseChannel, 1, chan);
  }

  /* Write groups */
  for (grp = pose->agroups.first; grp; grp = grp->next) {
    writestruct(wd, DATA, bActionGroup, 1, grp);
  }

  /* write IK param */
  if (pose->ikparam) {
    const char *structname = BKE_pose_ikparam_get_name(pose);
    if (structname) {
      writestruct_id(wd, DATA, structname, 1, pose->ikparam);
    }
  }

  /* Write this pose */
  writestruct(wd, DATA, bPose, 1, pose);
}

static void write_defgroups(WriteData *wd, ListBase *defbase)
{
  LISTBASE_FOREACH (bDeformGroup *, defgroup, defbase) {
    writestruct(wd, DATA, bDeformGroup, 1, defgroup);
  }
}

static void write_fmaps(WriteData *wd, ListBase *fbase)
{
  LISTBASE_FOREACH (bFaceMap *, fmap, fbase) {
    writestruct(wd, DATA, bFaceMap, 1, fmap);
  }
}

static void write_modifiers(WriteData *wd, ListBase *modbase)
{
  ModifierData *md;

  if (modbase == NULL) {
    return;
  }

  for (md = modbase->first; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
    if (mti == NULL) {
      return;
    }

    writestruct_id(wd, DATA, mti->structName, 1, md);

    if (md->type == eModifierType_Hook) {
      HookModifierData *hmd = (HookModifierData *)md;

      if (hmd->curfalloff) {
        write_curvemapping(wd, hmd->curfalloff);
      }

      writedata(wd, DATA, sizeof(int) * hmd->totindex, hmd->indexar);
    }
    else if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;

      writestruct(wd, DATA, ClothSimSettings, 1, clmd->sim_parms);
      writestruct(wd, DATA, ClothCollSettings, 1, clmd->coll_parms);
      writestruct(wd, DATA, EffectorWeights, 1, clmd->sim_parms->effector_weights);
      write_pointcaches(wd, &clmd->ptcaches);
    }
    else if (md->type == eModifierType_Fluid) {
      FluidModifierData *mmd = (FluidModifierData *)md;

      if (mmd->type & MOD_FLUID_TYPE_DOMAIN) {
        writestruct(wd, DATA, FluidDomainSettings, 1, mmd->domain);

        if (mmd->domain) {
          write_pointcaches(wd, &(mmd->domain->ptcaches[0]));

          /* create fake pointcache so that old blender versions can read it */
          mmd->domain->point_cache[1] = BKE_ptcache_add(&mmd->domain->ptcaches[1]);
          mmd->domain->point_cache[1]->flag |= PTCACHE_DISK_CACHE | PTCACHE_FAKE_SMOKE;
          mmd->domain->point_cache[1]->step = 1;

          write_pointcaches(wd, &(mmd->domain->ptcaches[1]));

          if (mmd->domain->coba) {
            writestruct(wd, DATA, ColorBand, 1, mmd->domain->coba);
          }

          /* cleanup the fake pointcache */
          BKE_ptcache_free_list(&mmd->domain->ptcaches[1]);
          mmd->domain->point_cache[1] = NULL;

          writestruct(wd, DATA, EffectorWeights, 1, mmd->domain->effector_weights);
        }
      }
      else if (mmd->type & MOD_FLUID_TYPE_FLOW) {
        writestruct(wd, DATA, FluidFlowSettings, 1, mmd->flow);
      }
      else if (mmd->type & MOD_FLUID_TYPE_EFFEC) {
        writestruct(wd, DATA, FluidEffectorSettings, 1, mmd->effector);
      }
    }
    else if (md->type == eModifierType_Fluidsim) {
      FluidsimModifierData *fluidmd = (FluidsimModifierData *)md;

      writestruct(wd, DATA, FluidsimSettings, 1, fluidmd->fss);
    }
    else if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

      if (pmd->canvas) {
        DynamicPaintSurface *surface;
        writestruct(wd, DATA, DynamicPaintCanvasSettings, 1, pmd->canvas);

        /* write surfaces */
        for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
          writestruct(wd, DATA, DynamicPaintSurface, 1, surface);
        }
        /* write caches and effector weights */
        for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
          write_pointcaches(wd, &(surface->ptcaches));

          writestruct(wd, DATA, EffectorWeights, 1, surface->effector_weights);
        }
      }
      if (pmd->brush) {
        writestruct(wd, DATA, DynamicPaintBrushSettings, 1, pmd->brush);
        writestruct(wd, DATA, ColorBand, 1, pmd->brush->paint_ramp);
        writestruct(wd, DATA, ColorBand, 1, pmd->brush->vel_ramp);
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
    else if (md->type == eModifierType_MeshDeform) {
      MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
      int size = mmd->dyngridsize;

      writestruct(wd, DATA, MDefInfluence, mmd->totinfluence, mmd->bindinfluences);
      writedata(wd, DATA, sizeof(int) * (mmd->totvert + 1), mmd->bindoffsets);
      writedata(wd, DATA, sizeof(float) * 3 * mmd->totcagevert, mmd->bindcagecos);
      writestruct(wd, DATA, MDefCell, size * size * size, mmd->dyngrid);
      writestruct(wd, DATA, MDefInfluence, mmd->totinfluence, mmd->dyninfluences);
      writedata(wd, DATA, sizeof(int) * mmd->totvert, mmd->dynverts);
    }
    else if (md->type == eModifierType_Warp) {
      WarpModifierData *tmd = (WarpModifierData *)md;
      if (tmd->curfalloff) {
        write_curvemapping(wd, tmd->curfalloff);
      }
    }
    else if (md->type == eModifierType_WeightVGEdit) {
      WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

      if (wmd->cmap_curve) {
        write_curvemapping(wd, wmd->cmap_curve);
      }
    }
    else if (md->type == eModifierType_LaplacianDeform) {
      LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;

      writedata(wd, DATA, sizeof(float) * lmd->total_verts * 3, lmd->vertexco);
    }
    else if (md->type == eModifierType_CorrectiveSmooth) {
      CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

      if (csmd->bind_coords) {
        writedata(wd, DATA, sizeof(float[3]) * csmd->bind_coords_num, csmd->bind_coords);
      }
    }
    else if (md->type == eModifierType_SurfaceDeform) {
      SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

      writestruct(wd, DATA, SDefVert, smd->numverts, smd->verts);

      if (smd->verts) {
        for (int i = 0; i < smd->numverts; i++) {
          writestruct(wd, DATA, SDefBind, smd->verts[i].numbinds, smd->verts[i].binds);

          if (smd->verts[i].binds) {
            for (int j = 0; j < smd->verts[i].numbinds; j++) {
              writedata(wd,
                        DATA,
                        sizeof(int) * smd->verts[i].binds[j].numverts,
                        smd->verts[i].binds[j].vert_inds);

              if (smd->verts[i].binds[j].mode == MOD_SDEF_MODE_CENTROID ||
                  smd->verts[i].binds[j].mode == MOD_SDEF_MODE_LOOPTRI) {
                writedata(wd, DATA, sizeof(float) * 3, smd->verts[i].binds[j].vert_weights);
              }
              else {
                writedata(wd,
                          DATA,
                          sizeof(float) * smd->verts[i].binds[j].numverts,
                          smd->verts[i].binds[j].vert_weights);
              }
            }
          }
        }
      }
    }
    else if (md->type == eModifierType_Bevel) {
      BevelModifierData *bmd = (BevelModifierData *)md;
      if (bmd->custom_profile) {
        write_CurveProfile(wd, bmd->custom_profile);
      }
    }
  }
}

static void write_gpencil_modifiers(WriteData *wd, ListBase *modbase)
{
  GpencilModifierData *md;

  if (modbase == NULL) {
    return;
  }

  for (md = modbase->first; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
    if (mti == NULL) {
      return;
    }

    writestruct_id(wd, DATA, mti->struct_name, 1, md);

    if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      if (gpmd->curve_thickness) {
        write_curvemapping(wd, gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      if (gpmd->curve_intensity) {
        write_curvemapping(wd, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;

      if (gpmd->curfalloff) {
        write_curvemapping(wd, gpmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      if (gpmd->colorband) {
        writestruct(wd, DATA, ColorBand, 1, gpmd->colorband);
      }
      if (gpmd->curve_intensity) {
        write_curvemapping(wd, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        write_curvemapping(wd, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        write_curvemapping(wd, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        write_curvemapping(wd, gpmd->curve_intensity);
      }
    }
  }
}

static void write_shaderfxs(WriteData *wd, ListBase *fxbase)
{
  ShaderFxData *fx;

  if (fxbase == NULL) {
    return;
  }

  for (fx = fxbase->first; fx; fx = fx->next) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);
    if (fxi == NULL) {
      return;
    }

    writestruct_id(wd, DATA, fxi->struct_name, 1, fx);
  }
}

static void write_object(WriteData *wd, Object *ob, const void *id_address)
{
  if (ob->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    BKE_object_runtime_reset(ob);

    /* write LibData */
    writestruct_at_address(wd, ID_OB, Object, 1, id_address, ob);
    write_iddata(wd, &ob->id);

    if (ob->adt) {
      write_animdata(wd, ob->adt);
    }

    /* direct data */
    writedata(wd, DATA, sizeof(void *) * ob->totcol, ob->mat);
    writedata(wd, DATA, sizeof(char) * ob->totcol, ob->matbits);
    /* write_effects(wd, &ob->effect); */ /* not used anymore */

    if (ob->type == OB_ARMATURE) {
      bArmature *arm = ob->data;
      if (arm && ob->pose && arm->act_bone) {
        BLI_strncpy(
            ob->pose->proxy_act_bone, arm->act_bone->name, sizeof(ob->pose->proxy_act_bone));
      }
    }

    write_pose(wd, ob->pose);
    write_defgroups(wd, &ob->defbase);
    write_fmaps(wd, &ob->fmaps);
    write_constraints(wd, &ob->constraints);
    write_motionpath(wd, ob->mpath);

    writestruct(wd, DATA, PartDeflect, 1, ob->pd);
    if (ob->soft) {
      /* Set deprecated pointers to prevent crashes of older Blenders */
      ob->soft->pointcache = ob->soft->shared->pointcache;
      ob->soft->ptcaches = ob->soft->shared->ptcaches;
      writestruct(wd, DATA, SoftBody, 1, ob->soft);
      writestruct(wd, DATA, SoftBody_Shared, 1, ob->soft->shared);
      write_pointcaches(wd, &(ob->soft->shared->ptcaches));
      writestruct(wd, DATA, EffectorWeights, 1, ob->soft->effector_weights);
    }

    if (ob->rigidbody_object) {
      /* TODO: if any extra data is added to handle duplis, will need separate function then */
      writestruct(wd, DATA, RigidBodyOb, 1, ob->rigidbody_object);
    }
    if (ob->rigidbody_constraint) {
      writestruct(wd, DATA, RigidBodyCon, 1, ob->rigidbody_constraint);
    }

    if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
      writestruct(wd, DATA, ImageUser, 1, ob->iuser);
    }

    write_particlesystems(wd, &ob->particlesystem);
    write_modifiers(wd, &ob->modifiers);
    write_gpencil_modifiers(wd, &ob->greasepencil_modifiers);
    write_shaderfxs(wd, &ob->shader_fx);

    writelist(wd, DATA, LinkData, &ob->pc_ids);
    writelist(wd, DATA, LodLevel, &ob->lodlevels);

    write_previews(wd, ob->preview);
  }
}

static void write_vfont(WriteData *wd, VFont *vf, const void *id_address)
{
  if (vf->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    vf->data = NULL;
    vf->temp_pf = NULL;

    /* write LibData */
    writestruct_at_address(wd, ID_VF, VFont, 1, id_address, vf);
    write_iddata(wd, &vf->id);

    /* direct data */
    if (vf->packedfile) {
      PackedFile *pf = vf->packedfile;
      writestruct(wd, DATA, PackedFile, 1, pf);
      writedata(wd, DATA, pf->size, pf->data);
    }
  }
}

static void write_key(WriteData *wd, Key *key, const void *id_address)
{
  if (key->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_KE, Key, 1, id_address, key);
    write_iddata(wd, &key->id);

    if (key->adt) {
      write_animdata(wd, key->adt);
    }

    /* direct data */
    LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
      writestruct(wd, DATA, KeyBlock, 1, kb);
      if (kb->data) {
        writedata(wd, DATA, kb->totelem * key->elemsize, kb->data);
      }
    }
  }
}

static void write_camera(WriteData *wd, Camera *cam, const void *id_address)
{
  if (cam->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_CA, Camera, 1, id_address, cam);
    write_iddata(wd, &cam->id);

    if (cam->adt) {
      write_animdata(wd, cam->adt);
    }

    LISTBASE_FOREACH (CameraBGImage *, bgpic, &cam->bg_images) {
      writestruct(wd, DATA, CameraBGImage, 1, bgpic);
    }
  }
}

static void write_mball(WriteData *wd, MetaBall *mb, const void *id_address)
{
  if (mb->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    BLI_listbase_clear(&mb->disp);
    mb->editelems = NULL;
    /* Must always be cleared (meta's don't have their own edit-data). */
    mb->needs_flush_to_id = 0;
    mb->lastelem = NULL;
    mb->batch_cache = NULL;

    /* write LibData */
    writestruct_at_address(wd, ID_MB, MetaBall, 1, id_address, mb);
    write_iddata(wd, &mb->id);

    /* direct data */
    writedata(wd, DATA, sizeof(void *) * mb->totcol, mb->mat);
    if (mb->adt) {
      write_animdata(wd, mb->adt);
    }

    LISTBASE_FOREACH (MetaElem *, ml, &mb->elems) {
      writestruct(wd, DATA, MetaElem, 1, ml);
    }
  }
}

static void write_curve(WriteData *wd, Curve *cu, const void *id_address)
{
  if (cu->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    cu->editnurb = NULL;
    cu->editfont = NULL;
    cu->batch_cache = NULL;

    /* write LibData */
    writestruct_at_address(wd, ID_CU, Curve, 1, id_address, cu);
    write_iddata(wd, &cu->id);

    /* direct data */
    writedata(wd, DATA, sizeof(void *) * cu->totcol, cu->mat);
    if (cu->adt) {
      write_animdata(wd, cu->adt);
    }

    if (cu->vfont) {
      writedata(wd, DATA, cu->len + 1, cu->str);
      writestruct(wd, DATA, CharInfo, cu->len_wchar + 1, cu->strinfo);
      writestruct(wd, DATA, TextBox, cu->totbox, cu->tb);
    }
    else {
      /* is also the order of reading */
      LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
        writestruct(wd, DATA, Nurb, 1, nu);
      }
      LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
        if (nu->type == CU_BEZIER) {
          writestruct(wd, DATA, BezTriple, nu->pntsu, nu->bezt);
        }
        else {
          writestruct(wd, DATA, BPoint, nu->pntsu * nu->pntsv, nu->bp);
          if (nu->knotsu) {
            writedata(wd, DATA, KNOTSU(nu) * sizeof(float), nu->knotsu);
          }
          if (nu->knotsv) {
            writedata(wd, DATA, KNOTSV(nu) * sizeof(float), nu->knotsv);
          }
        }
      }
    }
  }
}

static void write_dverts(WriteData *wd, int count, MDeformVert *dvlist)
{
  if (dvlist) {

    /* Write the dvert list */
    writestruct(wd, DATA, MDeformVert, count, dvlist);

    /* Write deformation data for each dvert */
    for (int i = 0; i < count; i++) {
      if (dvlist[i].dw) {
        writestruct(wd, DATA, MDeformWeight, dvlist[i].totweight, dvlist[i].dw);
      }
    }
  }
}

static void write_mdisps(WriteData *wd, int count, MDisps *mdlist, int external)
{
  if (mdlist) {
    int i;

    writestruct(wd, DATA, MDisps, count, mdlist);
    for (i = 0; i < count; i++) {
      MDisps *md = &mdlist[i];
      if (md->disps) {
        if (!external) {
          writedata(wd, DATA, sizeof(float) * 3 * md->totdisp, md->disps);
        }
      }

      if (md->hidden) {
        writedata(wd, DATA, BLI_BITMAP_SIZE(md->totdisp), md->hidden);
      }
    }
  }
}

static void write_grid_paint_mask(WriteData *wd, int count, GridPaintMask *grid_paint_mask)
{
  if (grid_paint_mask) {
    int i;

    writestruct(wd, DATA, GridPaintMask, count, grid_paint_mask);
    for (i = 0; i < count; i++) {
      GridPaintMask *gpm = &grid_paint_mask[i];
      if (gpm->data) {
        const int gridsize = BKE_ccg_gridsize(gpm->level);
        writedata(wd, DATA, sizeof(*gpm->data) * gridsize * gridsize, gpm->data);
      }
    }
  }
}

static void write_customdata(WriteData *wd,
                             ID *id,
                             int count,
                             CustomData *data,
                             CustomDataLayer *layers,
                             CustomDataMask cddata_mask)
{
  int i;

  /* write external customdata (not for undo) */
  if (data->external && (wd->use_memfile == false)) {
    CustomData_external_write(data, id, cddata_mask, count, 0);
  }

  writestruct_at_address(wd, DATA, CustomDataLayer, data->totlayer, data->layers, layers);

  for (i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &layers[i];
    const char *structname;
    int structnum, datasize;

    if (layer->type == CD_MDEFORMVERT) {
      /* layer types that allocate own memory need special handling */
      write_dverts(wd, count, layer->data);
    }
    else if (layer->type == CD_MDISPS) {
      write_mdisps(wd, count, layer->data, layer->flag & CD_FLAG_EXTERNAL);
    }
    else if (layer->type == CD_PAINT_MASK) {
      const float *layer_data = layer->data;
      writedata(wd, DATA, sizeof(*layer_data) * count, layer_data);
    }
    else if (layer->type == CD_SCULPT_FACE_SETS) {
      const float *layer_data = layer->data;
      writedata(wd, DATA, sizeof(*layer_data) * count, layer_data);
    }
    else if (layer->type == CD_GRID_PAINT_MASK) {
      write_grid_paint_mask(wd, count, layer->data);
    }
    else if (layer->type == CD_FACEMAP) {
      const int *layer_data = layer->data;
      writedata(wd, DATA, sizeof(*layer_data) * count, layer_data);
    }
    else {
      CustomData_file_write_info(layer->type, &structname, &structnum);
      if (structnum) {
        datasize = structnum * count;
        writestruct_id(wd, DATA, structname, datasize, layer->data);
      }
      else if (!wd->use_memfile) { /* Do not warn on undo. */
        printf("%s error: layer '%s':%d - can't be written to file\n",
               __func__,
               structname,
               layer->type);
      }
    }
  }

  if (data->external) {
    writestruct(wd, DATA, CustomDataExternal, 1, data->external);
  }
}

static void write_mesh(WriteData *wd, Mesh *mesh, const void *id_address)
{
  if (mesh->id.us > 0 || wd->use_memfile) {
    /* cache only - don't write */
    mesh->mface = NULL;
    mesh->totface = 0;
    memset(&mesh->fdata, 0, sizeof(mesh->fdata));
    memset(&mesh->runtime, 0, sizeof(mesh->runtime));

    /* Reduce xdata layers, fill xlayers with layers to be written.
     * This makes xdata invalid for Blender, which is why we made a
     * temporary local copy. */
    CustomDataLayer *vlayers = NULL, vlayers_buff[CD_TEMP_CHUNK_SIZE];
    CustomDataLayer *elayers = NULL, elayers_buff[CD_TEMP_CHUNK_SIZE];
    CustomDataLayer *flayers = NULL, flayers_buff[CD_TEMP_CHUNK_SIZE];
    CustomDataLayer *llayers = NULL, llayers_buff[CD_TEMP_CHUNK_SIZE];
    CustomDataLayer *players = NULL, players_buff[CD_TEMP_CHUNK_SIZE];

    CustomData_file_write_prepare(&mesh->vdata, &vlayers, vlayers_buff, ARRAY_SIZE(vlayers_buff));
    CustomData_file_write_prepare(&mesh->edata, &elayers, elayers_buff, ARRAY_SIZE(elayers_buff));
    flayers = flayers_buff;
    CustomData_file_write_prepare(&mesh->ldata, &llayers, llayers_buff, ARRAY_SIZE(llayers_buff));
    CustomData_file_write_prepare(&mesh->pdata, &players, players_buff, ARRAY_SIZE(players_buff));

    writestruct_at_address(wd, ID_ME, Mesh, 1, id_address, mesh);
    write_iddata(wd, &mesh->id);

    /* direct data */
    if (mesh->adt) {
      write_animdata(wd, mesh->adt);
    }

    writedata(wd, DATA, sizeof(void *) * mesh->totcol, mesh->mat);
    writedata(wd, DATA, sizeof(MSelect) * mesh->totselect, mesh->mselect);

    write_customdata(wd, &mesh->id, mesh->totvert, &mesh->vdata, vlayers, CD_MASK_MESH.vmask);
    write_customdata(wd, &mesh->id, mesh->totedge, &mesh->edata, elayers, CD_MASK_MESH.emask);
    /* fdata is really a dummy - written so slots align */
    write_customdata(wd, &mesh->id, mesh->totface, &mesh->fdata, flayers, CD_MASK_MESH.fmask);
    write_customdata(wd, &mesh->id, mesh->totloop, &mesh->ldata, llayers, CD_MASK_MESH.lmask);
    write_customdata(wd, &mesh->id, mesh->totpoly, &mesh->pdata, players, CD_MASK_MESH.pmask);

    /* free temporary data */
    if (vlayers && vlayers != vlayers_buff) {
      MEM_freeN(vlayers);
    }
    if (elayers && elayers != elayers_buff) {
      MEM_freeN(elayers);
    }
    if (flayers && flayers != flayers_buff) {
      MEM_freeN(flayers);
    }
    if (llayers && llayers != llayers_buff) {
      MEM_freeN(llayers);
    }
    if (players && players != players_buff) {
      MEM_freeN(players);
    }
  }
}

static void write_lattice(WriteData *wd, Lattice *lt, const void *id_address)
{
  if (lt->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    lt->editlatt = NULL;
    lt->batch_cache = NULL;

    /* write LibData */
    writestruct_at_address(wd, ID_LT, Lattice, 1, id_address, lt);
    write_iddata(wd, &lt->id);

    /* write animdata */
    if (lt->adt) {
      write_animdata(wd, lt->adt);
    }

    /* direct data */
    writestruct(wd, DATA, BPoint, lt->pntsu * lt->pntsv * lt->pntsw, lt->def);

    write_dverts(wd, lt->pntsu * lt->pntsv * lt->pntsw, lt->dvert);
  }
}

static void write_image(WriteData *wd, Image *ima, const void *id_address)
{
  if (ima->id.us > 0 || wd->use_memfile) {
    ImagePackedFile *imapf;

    /* Some trickery to keep forward compatibility of packed images. */
    BLI_assert(ima->packedfile == NULL);
    if (ima->packedfiles.first != NULL) {
      imapf = ima->packedfiles.first;
      ima->packedfile = imapf->packedfile;
    }

    /* write LibData */
    writestruct_at_address(wd, ID_IM, Image, 1, id_address, ima);
    write_iddata(wd, &ima->id);

    for (imapf = ima->packedfiles.first; imapf; imapf = imapf->next) {
      writestruct(wd, DATA, ImagePackedFile, 1, imapf);
      if (imapf->packedfile) {
        PackedFile *pf = imapf->packedfile;
        writestruct(wd, DATA, PackedFile, 1, pf);
        writedata(wd, DATA, pf->size, pf->data);
      }
    }

    write_previews(wd, ima->preview);

    LISTBASE_FOREACH (ImageView *, iv, &ima->views) {
      writestruct(wd, DATA, ImageView, 1, iv);
    }
    writestruct(wd, DATA, Stereo3dFormat, 1, ima->stereo3d_format);

    writelist(wd, DATA, ImageTile, &ima->tiles);

    ima->packedfile = NULL;

    writelist(wd, DATA, RenderSlot, &ima->renderslots);
  }
}

static void write_texture(WriteData *wd, Tex *tex, const void *id_address)
{
  if (tex->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_TE, Tex, 1, id_address, tex);
    write_iddata(wd, &tex->id);

    if (tex->adt) {
      write_animdata(wd, tex->adt);
    }

    /* direct data */
    if (tex->coba) {
      writestruct(wd, DATA, ColorBand, 1, tex->coba);
    }

    /* nodetree is integral part of texture, no libdata */
    if (tex->nodetree) {
      writestruct(wd, DATA, bNodeTree, 1, tex->nodetree);
      write_nodetree_nolib(wd, tex->nodetree);
    }

    write_previews(wd, tex->preview);
  }
}

static void write_material(WriteData *wd, Material *ma, const void *id_address)
{
  if (ma->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    ma->texpaintslot = NULL;
    BLI_listbase_clear(&ma->gpumaterial);

    /* write LibData */
    writestruct_at_address(wd, ID_MA, Material, 1, id_address, ma);
    write_iddata(wd, &ma->id);

    if (ma->adt) {
      write_animdata(wd, ma->adt);
    }

    /* nodetree is integral part of material, no libdata */
    if (ma->nodetree) {
      writestruct(wd, DATA, bNodeTree, 1, ma->nodetree);
      write_nodetree_nolib(wd, ma->nodetree);
    }

    write_previews(wd, ma->preview);

    /* grease pencil settings */
    if (ma->gp_style) {
      writestruct(wd, DATA, MaterialGPencilStyle, 1, ma->gp_style);
    }
  }
}

static void write_world(WriteData *wd, World *wrld, const void *id_address)
{
  if (wrld->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    BLI_listbase_clear(&wrld->gpumaterial);

    /* write LibData */
    writestruct_at_address(wd, ID_WO, World, 1, id_address, wrld);
    write_iddata(wd, &wrld->id);

    if (wrld->adt) {
      write_animdata(wd, wrld->adt);
    }

    /* nodetree is integral part of world, no libdata */
    if (wrld->nodetree) {
      writestruct(wd, DATA, bNodeTree, 1, wrld->nodetree);
      write_nodetree_nolib(wd, wrld->nodetree);
    }

    write_previews(wd, wrld->preview);
  }
}

static void write_light(WriteData *wd, Light *la, const void *id_address)
{
  if (la->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_LA, Light, 1, id_address, la);
    write_iddata(wd, &la->id);

    if (la->adt) {
      write_animdata(wd, la->adt);
    }

    if (la->curfalloff) {
      write_curvemapping(wd, la->curfalloff);
    }

    /* Node-tree is integral part of lights, no libdata. */
    if (la->nodetree) {
      writestruct(wd, DATA, bNodeTree, 1, la->nodetree);
      write_nodetree_nolib(wd, la->nodetree);
    }

    write_previews(wd, la->preview);
  }
}

static void write_collection_nolib(WriteData *wd, Collection *collection)
{
  /* Shared function for collection data-blocks and scene master collection. */
  write_previews(wd, collection->preview);

  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    writestruct(wd, DATA, CollectionObject, 1, cob);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    writestruct(wd, DATA, CollectionChild, 1, child);
  }
}

static void write_collection(WriteData *wd, Collection *collection, const void *id_address)
{
  if (collection->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    collection->flag &= ~COLLECTION_HAS_OBJECT_CACHE;
    collection->tag = 0;
    BLI_listbase_clear(&collection->object_cache);
    BLI_listbase_clear(&collection->parents);

    /* write LibData */
    writestruct_at_address(wd, ID_GR, Collection, 1, id_address, collection);
    write_iddata(wd, &collection->id);

    write_collection_nolib(wd, collection);
  }
}

static void write_sequence_modifiers(WriteData *wd, ListBase *modbase)
{
  SequenceModifierData *smd;

  for (smd = modbase->first; smd; smd = smd->next) {
    const SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

    if (smti) {
      writestruct_id(wd, DATA, smti->struct_name, 1, smd);

      if (smd->type == seqModifierType_Curves) {
        CurvesModifierData *cmd = (CurvesModifierData *)smd;

        write_curvemapping(wd, &cmd->curve_mapping);
      }
      else if (smd->type == seqModifierType_HueCorrect) {
        HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

        write_curvemapping(wd, &hcmd->curve_mapping);
      }
    }
    else {
      writestruct(wd, DATA, SequenceModifierData, 1, smd);
    }
  }
}

static void write_view_settings(WriteData *wd, ColorManagedViewSettings *view_settings)
{
  if (view_settings->curve_mapping) {
    write_curvemapping(wd, view_settings->curve_mapping);
  }
}

static void write_view3dshading(WriteData *wd, View3DShading *shading)
{
  if (shading->prop) {
    IDP_WriteProperty(shading->prop, wd);
  }
}

static void write_paint(WriteData *wd, Paint *p)
{
  if (p->cavity_curve) {
    write_curvemapping(wd, p->cavity_curve);
  }
  writestruct(wd, DATA, PaintToolSlot, p->tool_slots_len, p->tool_slots);
}

static void write_layer_collections(WriteData *wd, ListBase *lb)
{
  LISTBASE_FOREACH (LayerCollection *, lc, lb) {
    writestruct(wd, DATA, LayerCollection, 1, lc);

    write_layer_collections(wd, &lc->layer_collections);
  }
}

static void write_view_layer(WriteData *wd, ViewLayer *view_layer)
{
  writestruct(wd, DATA, ViewLayer, 1, view_layer);
  writelist(wd, DATA, Base, &view_layer->object_bases);

  if (view_layer->id_properties) {
    IDP_WriteProperty(view_layer->id_properties, wd);
  }

  LISTBASE_FOREACH (FreestyleModuleConfig *, fmc, &view_layer->freestyle_config.modules) {
    writestruct(wd, DATA, FreestyleModuleConfig, 1, fmc);
  }

  LISTBASE_FOREACH (FreestyleLineSet *, fls, &view_layer->freestyle_config.linesets) {
    writestruct(wd, DATA, FreestyleLineSet, 1, fls);
  }
  write_layer_collections(wd, &view_layer->layer_collections);
}

static void write_lightcache_texture(WriteData *wd, LightCacheTexture *tex)
{
  if (tex->data) {
    size_t data_size = tex->components * tex->tex_size[0] * tex->tex_size[1] * tex->tex_size[2];
    if (tex->data_type == LIGHTCACHETEX_FLOAT) {
      data_size *= sizeof(float);
    }
    else if (tex->data_type == LIGHTCACHETEX_UINT) {
      data_size *= sizeof(uint);
    }
    writedata(wd, DATA, data_size, tex->data);
  }
}

static void write_lightcache(WriteData *wd, LightCache *cache)
{
  write_lightcache_texture(wd, &cache->grid_tx);
  write_lightcache_texture(wd, &cache->cube_tx);

  if (cache->cube_mips) {
    writestruct(wd, DATA, LightCacheTexture, cache->mips_len, cache->cube_mips);
    for (int i = 0; i < cache->mips_len; i++) {
      write_lightcache_texture(wd, &cache->cube_mips[i]);
    }
  }

  writestruct(wd, DATA, LightGridCache, cache->grid_len, cache->grid_data);
  writestruct(wd, DATA, LightProbeCache, cache->cube_len, cache->cube_data);
}

static void write_scene(WriteData *wd, Scene *sce, const void *id_address)
{
  if (wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    /* XXX This UI data should not be stored in Scene at all... */
    memset(&sce->cursor, 0, sizeof(sce->cursor));
  }

  /* write LibData */
  writestruct_at_address(wd, ID_SCE, Scene, 1, id_address, sce);
  write_iddata(wd, &sce->id);

  if (sce->adt) {
    write_animdata(wd, sce->adt);
  }
  write_keyingsets(wd, &sce->keyingsets);

  /* direct data */
  ToolSettings *tos = sce->toolsettings;
  writestruct(wd, DATA, ToolSettings, 1, tos);
  if (tos->vpaint) {
    writestruct(wd, DATA, VPaint, 1, tos->vpaint);
    write_paint(wd, &tos->vpaint->paint);
  }
  if (tos->wpaint) {
    writestruct(wd, DATA, VPaint, 1, tos->wpaint);
    write_paint(wd, &tos->wpaint->paint);
  }
  if (tos->sculpt) {
    writestruct(wd, DATA, Sculpt, 1, tos->sculpt);
    write_paint(wd, &tos->sculpt->paint);
  }
  if (tos->uvsculpt) {
    writestruct(wd, DATA, UvSculpt, 1, tos->uvsculpt);
    write_paint(wd, &tos->uvsculpt->paint);
  }
  if (tos->gp_paint) {
    writestruct(wd, DATA, GpPaint, 1, tos->gp_paint);
    write_paint(wd, &tos->gp_paint->paint);
  }
  if (tos->gp_vertexpaint) {
    writestruct(wd, DATA, GpVertexPaint, 1, tos->gp_vertexpaint);
    write_paint(wd, &tos->gp_vertexpaint->paint);
  }
  if (tos->gp_sculptpaint) {
    writestruct(wd, DATA, GpSculptPaint, 1, tos->gp_sculptpaint);
    write_paint(wd, &tos->gp_sculptpaint->paint);
  }
  if (tos->gp_weightpaint) {
    writestruct(wd, DATA, GpWeightPaint, 1, tos->gp_weightpaint);
    write_paint(wd, &tos->gp_weightpaint->paint);
  }
  /* write grease-pencil custom ipo curve to file */
  if (tos->gp_interpolate.custom_ipo) {
    write_curvemapping(wd, tos->gp_interpolate.custom_ipo);
  }
  /* write grease-pencil multiframe falloff curve to file */
  if (tos->gp_sculpt.cur_falloff) {
    write_curvemapping(wd, tos->gp_sculpt.cur_falloff);
  }
  /* write grease-pencil primitive curve to file */
  if (tos->gp_sculpt.cur_primitive) {
    write_curvemapping(wd, tos->gp_sculpt.cur_primitive);
  }
  /* Write the curve profile to the file. */
  if (tos->custom_bevel_profile_preset) {
    write_CurveProfile(wd, tos->custom_bevel_profile_preset);
  }

  write_paint(wd, &tos->imapaint.paint);

  Editing *ed = sce->ed;
  if (ed) {
    Sequence *seq;

    writestruct(wd, DATA, Editing, 1, ed);

    /* reset write flags too */

    SEQ_BEGIN (ed, seq) {
      if (seq->strip) {
        seq->strip->done = false;
      }
      writestruct(wd, DATA, Sequence, 1, seq);
    }
    SEQ_END;

    SEQ_BEGIN (ed, seq) {
      if (seq->strip && seq->strip->done == 0) {
        /* write strip with 'done' at 0 because readfile */

        if (seq->effectdata) {
          switch (seq->type) {
            case SEQ_TYPE_COLOR:
              writestruct(wd, DATA, SolidColorVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_SPEED:
              writestruct(wd, DATA, SpeedControlVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_WIPE:
              writestruct(wd, DATA, WipeVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_GLOW:
              writestruct(wd, DATA, GlowVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_TRANSFORM:
              writestruct(wd, DATA, TransformVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_GAUSSIAN_BLUR:
              writestruct(wd, DATA, GaussianBlurVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_TEXT:
              writestruct(wd, DATA, TextVars, 1, seq->effectdata);
              break;
            case SEQ_TYPE_COLORMIX:
              writestruct(wd, DATA, ColorMixVars, 1, seq->effectdata);
              break;
          }
        }

        writestruct(wd, DATA, Stereo3dFormat, 1, seq->stereo3d_format);

        Strip *strip = seq->strip;
        writestruct(wd, DATA, Strip, 1, strip);
        if (strip->crop) {
          writestruct(wd, DATA, StripCrop, 1, strip->crop);
        }
        if (strip->transform) {
          writestruct(wd, DATA, StripTransform, 1, strip->transform);
        }
        if (strip->proxy) {
          writestruct(wd, DATA, StripProxy, 1, strip->proxy);
        }
        if (seq->type == SEQ_TYPE_IMAGE) {
          writestruct(wd,
                      DATA,
                      StripElem,
                      MEM_allocN_len(strip->stripdata) / sizeof(struct StripElem),
                      strip->stripdata);
        }
        else if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
          writestruct(wd, DATA, StripElem, 1, strip->stripdata);
        }

        strip->done = true;
      }

      if (seq->prop) {
        IDP_WriteProperty(seq->prop, wd);
      }

      write_sequence_modifiers(wd, &seq->modifiers);
    }
    SEQ_END;

    /* new; meta stack too, even when its nasty restore code */
    LISTBASE_FOREACH (MetaStack *, ms, &ed->metastack) {
      writestruct(wd, DATA, MetaStack, 1, ms);
    }
  }

  if (sce->r.avicodecdata) {
    writestruct(wd, DATA, AviCodecData, 1, sce->r.avicodecdata);
    if (sce->r.avicodecdata->lpFormat) {
      writedata(wd, DATA, sce->r.avicodecdata->cbFormat, sce->r.avicodecdata->lpFormat);
    }
    if (sce->r.avicodecdata->lpParms) {
      writedata(wd, DATA, sce->r.avicodecdata->cbParms, sce->r.avicodecdata->lpParms);
    }
  }
  if (sce->r.ffcodecdata.properties) {
    IDP_WriteProperty(sce->r.ffcodecdata.properties, wd);
  }

  /* writing dynamic list of TimeMarkers to the blend file */
  LISTBASE_FOREACH (TimeMarker *, marker, &sce->markers) {
    writestruct(wd, DATA, TimeMarker, 1, marker);
  }

  /* writing dynamic list of TransformOrientations to the blend file */
  LISTBASE_FOREACH (TransformOrientation *, ts, &sce->transform_spaces) {
    writestruct(wd, DATA, TransformOrientation, 1, ts);
  }

  /* writing MultiView to the blend file */
  LISTBASE_FOREACH (SceneRenderView *, srv, &sce->r.views) {
    writestruct(wd, DATA, SceneRenderView, 1, srv);
  }

  if (sce->nodetree) {
    writestruct(wd, DATA, bNodeTree, 1, sce->nodetree);
    write_nodetree_nolib(wd, sce->nodetree);
  }

  write_view_settings(wd, &sce->view_settings);

  /* writing RigidBodyWorld data to the blend file */
  if (sce->rigidbody_world) {
    /* Set deprecated pointers to prevent crashes of older Blenders */
    sce->rigidbody_world->pointcache = sce->rigidbody_world->shared->pointcache;
    sce->rigidbody_world->ptcaches = sce->rigidbody_world->shared->ptcaches;
    writestruct(wd, DATA, RigidBodyWorld, 1, sce->rigidbody_world);

    writestruct(wd, DATA, RigidBodyWorld_Shared, 1, sce->rigidbody_world->shared);
    writestruct(wd, DATA, EffectorWeights, 1, sce->rigidbody_world->effector_weights);
    write_pointcaches(wd, &(sce->rigidbody_world->shared->ptcaches));
  }

  write_previews(wd, sce->preview);
  write_curvemapping_curves(wd, &sce->r.mblur_shutter_curve);

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    write_view_layer(wd, view_layer);
  }

  if (sce->master_collection) {
    writestruct(wd, DATA, Collection, 1, sce->master_collection);
    write_collection_nolib(wd, sce->master_collection);
  }

  /* Eevee Lightcache */
  if (sce->eevee.light_cache_data && !wd->use_memfile) {
    writestruct(wd, DATA, LightCache, 1, sce->eevee.light_cache_data);
    write_lightcache(wd, sce->eevee.light_cache_data);
  }

  write_view3dshading(wd, &sce->display.shading);

  /* Freed on doversion. */
  BLI_assert(sce->layer_properties == NULL);
}

static void write_gpencil(WriteData *wd, bGPdata *gpd, const void *id_address)
{
  if (gpd->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    /* XXX not sure why the whole run-time data is not cleared in reading code,
     * for now mimicking it here. */
    gpd->runtime.sbuffer = NULL;
    gpd->runtime.sbuffer_used = 0;
    gpd->runtime.sbuffer_size = 0;
    gpd->runtime.tot_cp_points = 0;

    /* write gpd data block to file */
    writestruct_at_address(wd, ID_GD, bGPdata, 1, id_address, gpd);
    write_iddata(wd, &gpd->id);

    if (gpd->adt) {
      write_animdata(wd, gpd->adt);
    }

    writedata(wd, DATA, sizeof(void *) * gpd->totcol, gpd->mat);

    /* write grease-pencil layers to file */
    writelist(wd, DATA, bGPDlayer, &gpd->layers);
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      /* Write mask list. */
      writelist(wd, DATA, bGPDlayer_Mask, &gpl->mask_layers);
      /* write this layer's frames to file */
      writelist(wd, DATA, bGPDframe, &gpl->frames);
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        /* write strokes */
        writelist(wd, DATA, bGPDstroke, &gpf->strokes);
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          writestruct(wd, DATA, bGPDspoint, gps->totpoints, gps->points);
          writestruct(wd, DATA, bGPDtriangle, gps->tot_triangles, gps->triangles);
          write_dverts(wd, gps->totpoints, gps->dvert);
        }
      }
    }
  }
}

static void write_wm_xr_data(WriteData *wd, wmXrData *xr_data)
{
  write_view3dshading(wd, &xr_data->session_settings.shading);
}

static void write_region(WriteData *wd, ARegion *region, int spacetype)
{
  writestruct(wd, DATA, ARegion, 1, region);

  if (region->regiondata) {
    if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
      return;
    }

    switch (spacetype) {
      case SPACE_VIEW3D:
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = region->regiondata;
          writestruct(wd, DATA, RegionView3D, 1, rv3d);

          if (rv3d->localvd) {
            writestruct(wd, DATA, RegionView3D, 1, rv3d->localvd);
          }
          if (rv3d->clipbb) {
            writestruct(wd, DATA, BoundBox, 1, rv3d->clipbb);
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

static void write_uilist(WriteData *wd, uiList *ui_list)
{
  writestruct(wd, DATA, uiList, 1, ui_list);

  if (ui_list->properties) {
    IDP_WriteProperty(ui_list->properties, wd);
  }
}

static void write_soops(WriteData *wd, SpaceOutliner *so)
{
  BLI_mempool *ts = so->treestore;

  if (ts) {
    SpaceOutliner so_flat = *so;

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

      writestruct(wd, DATA, SpaceOutliner, 1, so);

      writestruct_at_address(wd, DATA, TreeStore, 1, ts, &ts_flat);
      writestruct_at_address(wd, DATA, TreeStoreElem, elems, data_addr, data);

      MEM_freeN(data);
    }
    else {
      so_flat.treestore = NULL;
      writestruct_at_address(wd, DATA, SpaceOutliner, 1, so, &so_flat);
    }
  }
  else {
    writestruct(wd, DATA, SpaceOutliner, 1, so);
  }
}

static void write_panel_list(WriteData *wd, ListBase *lb)
{
  LISTBASE_FOREACH (Panel *, panel, lb) {
    writestruct(wd, DATA, Panel, 1, panel);
    write_panel_list(wd, &panel->children);
  }
}

static void write_area_regions(WriteData *wd, ScrArea *area)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    write_region(wd, region, area->spacetype);
    write_panel_list(wd, &region->panels);

    LISTBASE_FOREACH (PanelCategoryStack *, pc_act, &region->panels_category_active) {
      writestruct(wd, DATA, PanelCategoryStack, 1, pc_act);
    }

    LISTBASE_FOREACH (uiList *, ui_list, &region->ui_lists) {
      write_uilist(wd, ui_list);
    }

    LISTBASE_FOREACH (uiPreview *, ui_preview, &region->ui_previews) {
      writestruct(wd, DATA, uiPreview, 1, ui_preview);
    }
  }

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
      write_region(wd, region, sl->spacetype);
    }

    if (sl->spacetype == SPACE_VIEW3D) {
      View3D *v3d = (View3D *)sl;
      writestruct(wd, DATA, View3D, 1, v3d);

      if (v3d->localvd) {
        writestruct(wd, DATA, View3D, 1, v3d->localvd);
      }

      write_view3dshading(wd, &v3d->shading);
    }
    else if (sl->spacetype == SPACE_GRAPH) {
      SpaceGraph *sipo = (SpaceGraph *)sl;
      ListBase tmpGhosts = sipo->runtime.ghost_curves;

      /* temporarily disable ghost curves when saving */
      BLI_listbase_clear(&sipo->runtime.ghost_curves);

      writestruct(wd, DATA, SpaceGraph, 1, sl);
      if (sipo->ads) {
        writestruct(wd, DATA, bDopeSheet, 1, sipo->ads);
      }

      /* reenable ghost curves */
      sipo->runtime.ghost_curves = tmpGhosts;
    }
    else if (sl->spacetype == SPACE_PROPERTIES) {
      writestruct(wd, DATA, SpaceProperties, 1, sl);
    }
    else if (sl->spacetype == SPACE_FILE) {
      SpaceFile *sfile = (SpaceFile *)sl;

      writestruct(wd, DATA, SpaceFile, 1, sl);
      if (sfile->params) {
        writestruct(wd, DATA, FileSelectParams, 1, sfile->params);
      }
    }
    else if (sl->spacetype == SPACE_SEQ) {
      writestruct(wd, DATA, SpaceSeq, 1, sl);
    }
    else if (sl->spacetype == SPACE_OUTLINER) {
      SpaceOutliner *so = (SpaceOutliner *)sl;
      write_soops(wd, so);
    }
    else if (sl->spacetype == SPACE_IMAGE) {
      writestruct(wd, DATA, SpaceImage, 1, sl);
    }
    else if (sl->spacetype == SPACE_TEXT) {
      writestruct(wd, DATA, SpaceText, 1, sl);
    }
    else if (sl->spacetype == SPACE_SCRIPT) {
      SpaceScript *scr = (SpaceScript *)sl;
      scr->but_refs = NULL;
      writestruct(wd, DATA, SpaceScript, 1, sl);
    }
    else if (sl->spacetype == SPACE_ACTION) {
      writestruct(wd, DATA, SpaceAction, 1, sl);
    }
    else if (sl->spacetype == SPACE_NLA) {
      SpaceNla *snla = (SpaceNla *)sl;

      writestruct(wd, DATA, SpaceNla, 1, snla);
      if (snla->ads) {
        writestruct(wd, DATA, bDopeSheet, 1, snla->ads);
      }
    }
    else if (sl->spacetype == SPACE_NODE) {
      SpaceNode *snode = (SpaceNode *)sl;
      bNodeTreePath *path;
      writestruct(wd, DATA, SpaceNode, 1, snode);

      for (path = snode->treepath.first; path; path = path->next) {
        writestruct(wd, DATA, bNodeTreePath, 1, path);
      }
    }
    else if (sl->spacetype == SPACE_CONSOLE) {
      SpaceConsole *con = (SpaceConsole *)sl;
      ConsoleLine *cl;

      for (cl = con->history.first; cl; cl = cl->next) {
        /* 'len_alloc' is invalid on write, set from 'len' on read */
        writestruct(wd, DATA, ConsoleLine, 1, cl);
        writedata(wd, DATA, cl->len + 1, cl->line);
      }
      writestruct(wd, DATA, SpaceConsole, 1, sl);
    }
#ifdef WITH_GLOBAL_AREA_WRITING
    else if (sl->spacetype == SPACE_TOPBAR) {
      writestruct(wd, DATA, SpaceTopBar, 1, sl);
    }
    else if (sl->spacetype == SPACE_STATUSBAR) {
      writestruct(wd, DATA, SpaceStatusBar, 1, sl);
    }
#endif
    else if (sl->spacetype == SPACE_USERPREF) {
      writestruct(wd, DATA, SpaceUserPref, 1, sl);
    }
    else if (sl->spacetype == SPACE_CLIP) {
      writestruct(wd, DATA, SpaceClip, 1, sl);
    }
    else if (sl->spacetype == SPACE_INFO) {
      writestruct(wd, DATA, SpaceInfo, 1, sl);
    }
  }
}

static void write_area_map(WriteData *wd, ScrAreaMap *area_map)
{
  writelist(wd, DATA, ScrVert, &area_map->vertbase);
  writelist(wd, DATA, ScrEdge, &area_map->edgebase);
  LISTBASE_FOREACH (ScrArea *, area, &area_map->areabase) {
    area->butspacetype = area->spacetype; /* Just for compatibility, will be reset below. */

    writestruct(wd, DATA, ScrArea, 1, area);

#ifdef WITH_GLOBAL_AREA_WRITING
    writestruct(wd, DATA, ScrGlobalAreaData, 1, area->global);
#endif

    write_area_regions(wd, area);

    area->butspacetype = SPACE_EMPTY; /* Unset again, was changed above. */
  }
}

static void write_windowmanager(BlendWriter *writer, wmWindowManager *wm, const void *id_address)
{
  BLO_write_id_struct(writer, wmWindowManager, id_address, &wm->id);
  write_iddata(writer->wd, &wm->id);
  write_wm_xr_data(writer->wd, &wm->xr);

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
    write_area_map(writer->wd, &win->global_areas);
#else
    win->global_areas = global_areas;
#endif

    /* data is written, clear deprecated data again */
    win->screen = NULL;
  }
}

static void write_screen(WriteData *wd, bScreen *screen, const void *id_address)
{
  /* Screens are reference counted, only saved if used by a workspace. */
  if (screen->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    /* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
    writestruct_at_address(wd, ID_SCRN, bScreen, 1, id_address, screen);
    write_iddata(wd, &screen->id);

    write_previews(wd, screen->preview);

    /* direct data */
    write_area_map(wd, AREAMAP_FROM_SCREEN(screen));
  }
}

static void write_bone(WriteData *wd, Bone *bone)
{
  /* PATCH for upward compatibility after 2.37+ armature recode */
  bone->size[0] = bone->size[1] = bone->size[2] = 1.0f;

  /* Write this bone */
  writestruct(wd, DATA, Bone, 1, bone);

  /* Write ID Properties -- and copy this comment EXACTLY for easy finding
   * of library blocks that implement this.*/
  if (bone->prop) {
    IDP_WriteProperty(bone->prop, wd);
  }

  /* Write Children */
  LISTBASE_FOREACH (Bone *, cbone, &bone->childbase) {
    write_bone(wd, cbone);
  }
}

static void write_armature(WriteData *wd, bArmature *arm, const void *id_address)
{
  if (arm->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    arm->bonehash = NULL;
    arm->edbo = NULL;
    /* Must always be cleared (armatures don't have their own edit-data). */
    arm->needs_flush_to_id = 0;
    arm->act_edbone = NULL;

    writestruct_at_address(wd, ID_AR, bArmature, 1, id_address, arm);
    write_iddata(wd, &arm->id);

    if (arm->adt) {
      write_animdata(wd, arm->adt);
    }

    /* Direct data */
    LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
      write_bone(wd, bone);
    }
  }
}

static void write_text(WriteData *wd, Text *text, const void *id_address)
{
  /* Note: we are clearing local temp data here, *not* the flag in the actual 'real' ID. */
  if ((text->flags & TXT_ISMEM) && (text->flags & TXT_ISEXT)) {
    text->flags &= ~TXT_ISEXT;
  }

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  text->compiled = NULL;

  /* write LibData */
  writestruct_at_address(wd, ID_TXT, Text, 1, id_address, text);
  write_iddata(wd, &text->id);

  if (text->name) {
    writedata(wd, DATA, strlen(text->name) + 1, text->name);
  }

  if (!(text->flags & TXT_ISEXT)) {
    /* now write the text data, in two steps for optimization in the readfunction */
    LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
      writestruct(wd, DATA, TextLine, 1, tmp);
    }

    LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
      writedata(wd, DATA, tmp->len + 1, tmp->line);
    }
  }
}

static void write_speaker(WriteData *wd, Speaker *spk, const void *id_address)
{
  if (spk->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_SPK, Speaker, 1, id_address, spk);
    write_iddata(wd, &spk->id);

    if (spk->adt) {
      write_animdata(wd, spk->adt);
    }
  }
}

static void write_sound(WriteData *wd, bSound *sound, const void *id_address)
{
  if (sound->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    sound->tags = 0;
    sound->handle = NULL;
    sound->playback_handle = NULL;
    sound->spinlock = NULL;

    /* write LibData */
    writestruct_at_address(wd, ID_SO, bSound, 1, id_address, sound);
    write_iddata(wd, &sound->id);

    if (sound->packedfile) {
      PackedFile *pf = sound->packedfile;
      writestruct(wd, DATA, PackedFile, 1, pf);
      writedata(wd, DATA, pf->size, pf->data);
    }
  }
}

static void write_probe(WriteData *wd, LightProbe *prb, const void *id_address)
{
  if (prb->id.us > 0 || wd->use_memfile) {
    /* write LibData */
    writestruct_at_address(wd, ID_LP, LightProbe, 1, id_address, prb);
    write_iddata(wd, &prb->id);

    if (prb->adt) {
      write_animdata(wd, prb->adt);
    }
  }
}

static void write_nodetree(WriteData *wd, bNodeTree *ntree, const void *id_address)
{
  if (ntree->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    ntree->init = 0; /* to set callbacks and force setting types */
    ntree->is_updating = false;
    ntree->typeinfo = NULL;
    ntree->interface_type = NULL;
    ntree->progress = NULL;
    ntree->execdata = NULL;

    writestruct_at_address(wd, ID_NT, bNodeTree, 1, id_address, ntree);
    /* Note that trees directly used by other IDs (materials etc.) are not 'real' ID, they cannot
     * be linked, etc., so we write actual id data here only, for 'real' ID trees. */
    write_iddata(wd, &ntree->id);

    write_nodetree_nolib(wd, ntree);
  }
}

static void write_brush(WriteData *wd, Brush *brush, const void *id_address)
{
  if (brush->id.us > 0 || wd->use_memfile) {
    writestruct_at_address(wd, ID_BR, Brush, 1, id_address, brush);
    write_iddata(wd, &brush->id);

    if (brush->curve) {
      write_curvemapping(wd, brush->curve);
    }

    if (brush->gpencil_settings) {
      writestruct(wd, DATA, BrushGpencilSettings, 1, brush->gpencil_settings);

      if (brush->gpencil_settings->curve_sensitivity) {
        write_curvemapping(wd, brush->gpencil_settings->curve_sensitivity);
      }
      if (brush->gpencil_settings->curve_strength) {
        write_curvemapping(wd, brush->gpencil_settings->curve_strength);
      }
      if (brush->gpencil_settings->curve_jitter) {
        write_curvemapping(wd, brush->gpencil_settings->curve_jitter);
      }
      if (brush->gpencil_settings->curve_rand_pressure) {
        write_curvemapping(wd, brush->gpencil_settings->curve_rand_pressure);
      }
      if (brush->gpencil_settings->curve_rand_strength) {
        write_curvemapping(wd, brush->gpencil_settings->curve_rand_strength);
      }
      if (brush->gpencil_settings->curve_rand_uv) {
        write_curvemapping(wd, brush->gpencil_settings->curve_rand_uv);
      }
      if (brush->gpencil_settings->curve_rand_hue) {
        write_curvemapping(wd, brush->gpencil_settings->curve_rand_hue);
      }
      if (brush->gpencil_settings->curve_rand_saturation) {
        write_curvemapping(wd, brush->gpencil_settings->curve_rand_saturation);
      }
      if (brush->gpencil_settings->curve_rand_value) {
        write_curvemapping(wd, brush->gpencil_settings->curve_rand_value);
      }
    }
    if (brush->gradient) {
      writestruct(wd, DATA, ColorBand, 1, brush->gradient);
    }
  }
}

static void write_palette(WriteData *wd, Palette *palette, const void *id_address)
{
  if (palette->id.us > 0 || wd->use_memfile) {
    PaletteColor *color;
    writestruct_at_address(wd, ID_PAL, Palette, 1, id_address, palette);
    write_iddata(wd, &palette->id);

    for (color = palette->colors.first; color; color = color->next) {
      writestruct(wd, DATA, PaletteColor, 1, color);
    }
  }
}

static void write_paintcurve(WriteData *wd, PaintCurve *pc, const void *id_address)
{
  if (pc->id.us > 0 || wd->use_memfile) {
    writestruct_at_address(wd, ID_PC, PaintCurve, 1, id_address, pc);
    write_iddata(wd, &pc->id);

    writestruct(wd, DATA, PaintCurvePoint, pc->tot_points, pc->points);
  }
}

static void write_movieTracks(WriteData *wd, ListBase *tracks)
{
  MovieTrackingTrack *track;

  track = tracks->first;
  while (track) {
    writestruct(wd, DATA, MovieTrackingTrack, 1, track);

    if (track->markers) {
      writestruct(wd, DATA, MovieTrackingMarker, track->markersnr, track->markers);
    }

    track = track->next;
  }
}

static void write_moviePlaneTracks(WriteData *wd, ListBase *plane_tracks_base)
{
  MovieTrackingPlaneTrack *plane_track;

  for (plane_track = plane_tracks_base->first; plane_track; plane_track = plane_track->next) {
    writestruct(wd, DATA, MovieTrackingPlaneTrack, 1, plane_track);

    writedata(wd,
              DATA,
              sizeof(MovieTrackingTrack *) * plane_track->point_tracksnr,
              plane_track->point_tracks);
    writestruct(wd, DATA, MovieTrackingPlaneMarker, plane_track->markersnr, plane_track->markers);
  }
}

static void write_movieReconstruction(WriteData *wd, MovieTrackingReconstruction *reconstruction)
{
  if (reconstruction->camnr) {
    writestruct(
        wd, DATA, MovieReconstructedCamera, reconstruction->camnr, reconstruction->cameras);
  }
}

static void write_movieclip(WriteData *wd, MovieClip *clip, const void *id_address)
{
  if (clip->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    clip->anim = NULL;
    clip->tracking_context = NULL;
    clip->tracking.stats = NULL;

    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *object;

    writestruct_at_address(wd, ID_MC, MovieClip, 1, id_address, clip);
    write_iddata(wd, &clip->id);

    if (clip->adt) {
      write_animdata(wd, clip->adt);
    }

    write_movieTracks(wd, &tracking->tracks);
    write_moviePlaneTracks(wd, &tracking->plane_tracks);
    write_movieReconstruction(wd, &tracking->reconstruction);

    object = tracking->objects.first;
    while (object) {
      writestruct(wd, DATA, MovieTrackingObject, 1, object);

      write_movieTracks(wd, &object->tracks);
      write_moviePlaneTracks(wd, &object->plane_tracks);
      write_movieReconstruction(wd, &object->reconstruction);

      object = object->next;
    }
  }
}

static void write_mask(WriteData *wd, Mask *mask, const void *id_address)
{
  if (mask->id.us > 0 || wd->use_memfile) {
    MaskLayer *masklay;

    writestruct_at_address(wd, ID_MSK, Mask, 1, id_address, mask);
    write_iddata(wd, &mask->id);

    if (mask->adt) {
      write_animdata(wd, mask->adt);
    }

    for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
      MaskSpline *spline;
      MaskLayerShape *masklay_shape;

      writestruct(wd, DATA, MaskLayer, 1, masklay);

      for (spline = masklay->splines.first; spline; spline = spline->next) {
        int i;

        void *points_deform = spline->points_deform;
        spline->points_deform = NULL;

        writestruct(wd, DATA, MaskSpline, 1, spline);
        writestruct(wd, DATA, MaskSplinePoint, spline->tot_point, spline->points);

        spline->points_deform = points_deform;

        for (i = 0; i < spline->tot_point; i++) {
          MaskSplinePoint *point = &spline->points[i];

          if (point->tot_uw) {
            writestruct(wd, DATA, MaskSplinePointUW, point->tot_uw, point->uw);
          }
        }
      }

      for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
           masklay_shape = masklay_shape->next) {
        writestruct(wd, DATA, MaskLayerShape, 1, masklay_shape);
        writedata(wd,
                  DATA,
                  masklay_shape->tot_vert * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE,
                  masklay_shape->data);
      }
    }
  }
}

static void write_linestyle_color_modifiers(WriteData *wd, ListBase *modifiers)
{
  LineStyleModifier *m;

  for (m = modifiers->first; m; m = m->next) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_AlongStroke);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_DistanceFromCamera);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_DistanceFromObject);
        break;
      case LS_MODIFIER_MATERIAL:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_Material);
        break;
      case LS_MODIFIER_TANGENT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_Tangent);
        break;
      case LS_MODIFIER_NOISE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_Noise);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_CreaseAngle);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleColorModifier_Curvature_3D);
        break;
      default:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleModifier); /* this should not happen */
    }
    writestruct_nr(wd, DATA, struct_nr, 1, m);
  }
  for (m = modifiers->first; m; m = m->next) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        writestruct(wd, DATA, ColorBand, 1, ((LineStyleColorModifier_AlongStroke *)m)->color_ramp);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        writestruct(
            wd, DATA, ColorBand, 1, ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        writestruct(
            wd, DATA, ColorBand, 1, ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp);
        break;
      case LS_MODIFIER_MATERIAL:
        writestruct(wd, DATA, ColorBand, 1, ((LineStyleColorModifier_Material *)m)->color_ramp);
        break;
      case LS_MODIFIER_TANGENT:
        writestruct(wd, DATA, ColorBand, 1, ((LineStyleColorModifier_Tangent *)m)->color_ramp);
        break;
      case LS_MODIFIER_NOISE:
        writestruct(wd, DATA, ColorBand, 1, ((LineStyleColorModifier_Noise *)m)->color_ramp);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        writestruct(wd, DATA, ColorBand, 1, ((LineStyleColorModifier_CreaseAngle *)m)->color_ramp);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        writestruct(
            wd, DATA, ColorBand, 1, ((LineStyleColorModifier_Curvature_3D *)m)->color_ramp);
        break;
    }
  }
}

static void write_linestyle_alpha_modifiers(WriteData *wd, ListBase *modifiers)
{
  LineStyleModifier *m;

  for (m = modifiers->first; m; m = m->next) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_AlongStroke);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_DistanceFromCamera);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_DistanceFromObject);
        break;
      case LS_MODIFIER_MATERIAL:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_Material);
        break;
      case LS_MODIFIER_TANGENT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_Tangent);
        break;
      case LS_MODIFIER_NOISE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_Noise);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_CreaseAngle);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleAlphaModifier_Curvature_3D);
        break;
      default:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleModifier); /* this should not happen */
    }
    writestruct_nr(wd, DATA, struct_nr, 1, m);
  }
  for (m = modifiers->first; m; m = m->next) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        write_curvemapping(wd, ((LineStyleAlphaModifier_AlongStroke *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        write_curvemapping(wd, ((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        write_curvemapping(wd, ((LineStyleAlphaModifier_DistanceFromObject *)m)->curve);
        break;
      case LS_MODIFIER_MATERIAL:
        write_curvemapping(wd, ((LineStyleAlphaModifier_Material *)m)->curve);
        break;
      case LS_MODIFIER_TANGENT:
        write_curvemapping(wd, ((LineStyleAlphaModifier_Tangent *)m)->curve);
        break;
      case LS_MODIFIER_NOISE:
        write_curvemapping(wd, ((LineStyleAlphaModifier_Noise *)m)->curve);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        write_curvemapping(wd, ((LineStyleAlphaModifier_CreaseAngle *)m)->curve);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        write_curvemapping(wd, ((LineStyleAlphaModifier_Curvature_3D *)m)->curve);
        break;
    }
  }
}

static void write_linestyle_thickness_modifiers(WriteData *wd, ListBase *modifiers)
{
  LineStyleModifier *m;

  for (m = modifiers->first; m; m = m->next) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_AlongStroke);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_DistanceFromCamera);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_DistanceFromObject);
        break;
      case LS_MODIFIER_MATERIAL:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_Material);
        break;
      case LS_MODIFIER_CALLIGRAPHY:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_Calligraphy);
        break;
      case LS_MODIFIER_TANGENT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_Tangent);
        break;
      case LS_MODIFIER_NOISE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_Noise);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_CreaseAngle);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleThicknessModifier_Curvature_3D);
        break;
      default:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleModifier); /* this should not happen */
    }
    writestruct_nr(wd, DATA, struct_nr, 1, m);
  }
  for (m = modifiers->first; m; m = m->next) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        write_curvemapping(wd, ((LineStyleThicknessModifier_AlongStroke *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        write_curvemapping(wd, ((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        write_curvemapping(wd, ((LineStyleThicknessModifier_DistanceFromObject *)m)->curve);
        break;
      case LS_MODIFIER_MATERIAL:
        write_curvemapping(wd, ((LineStyleThicknessModifier_Material *)m)->curve);
        break;
      case LS_MODIFIER_TANGENT:
        write_curvemapping(wd, ((LineStyleThicknessModifier_Tangent *)m)->curve);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        write_curvemapping(wd, ((LineStyleThicknessModifier_CreaseAngle *)m)->curve);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        write_curvemapping(wd, ((LineStyleThicknessModifier_Curvature_3D *)m)->curve);
        break;
    }
  }
}

static void write_linestyle_geometry_modifiers(WriteData *wd, ListBase *modifiers)
{
  LineStyleModifier *m;

  for (m = modifiers->first; m; m = m->next) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_SAMPLING:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_Sampling);
        break;
      case LS_MODIFIER_BEZIER_CURVE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_BezierCurve);
        break;
      case LS_MODIFIER_SINUS_DISPLACEMENT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_SinusDisplacement);
        break;
      case LS_MODIFIER_SPATIAL_NOISE:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_SpatialNoise);
        break;
      case LS_MODIFIER_PERLIN_NOISE_1D:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_PerlinNoise1D);
        break;
      case LS_MODIFIER_PERLIN_NOISE_2D:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_PerlinNoise2D);
        break;
      case LS_MODIFIER_BACKBONE_STRETCHER:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_BackboneStretcher);
        break;
      case LS_MODIFIER_TIP_REMOVER:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_TipRemover);
        break;
      case LS_MODIFIER_POLYGONIZATION:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_Polygonalization);
        break;
      case LS_MODIFIER_GUIDING_LINES:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_GuidingLines);
        break;
      case LS_MODIFIER_BLUEPRINT:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_Blueprint);
        break;
      case LS_MODIFIER_2D_OFFSET:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_2DOffset);
        break;
      case LS_MODIFIER_2D_TRANSFORM:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_2DTransform);
        break;
      case LS_MODIFIER_SIMPLIFICATION:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleGeometryModifier_Simplification);
        break;
      default:
        struct_nr = SDNA_TYPE_FROM_STRUCT(LineStyleModifier); /* this should not happen */
    }
    writestruct_nr(wd, DATA, struct_nr, 1, m);
  }
}

static void write_linestyle(WriteData *wd, FreestyleLineStyle *linestyle, const void *id_address)
{
  if (linestyle->id.us > 0 || wd->use_memfile) {
    writestruct_at_address(wd, ID_LS, FreestyleLineStyle, 1, id_address, linestyle);
    write_iddata(wd, &linestyle->id);

    if (linestyle->adt) {
      write_animdata(wd, linestyle->adt);
    }

    write_linestyle_color_modifiers(wd, &linestyle->color_modifiers);
    write_linestyle_alpha_modifiers(wd, &linestyle->alpha_modifiers);
    write_linestyle_thickness_modifiers(wd, &linestyle->thickness_modifiers);
    write_linestyle_geometry_modifiers(wd, &linestyle->geometry_modifiers);
    for (int a = 0; a < MAX_MTEX; a++) {
      if (linestyle->mtex[a]) {
        writestruct(wd, DATA, MTex, 1, linestyle->mtex[a]);
      }
    }
    if (linestyle->nodetree) {
      writestruct(wd, DATA, bNodeTree, 1, linestyle->nodetree);
      write_nodetree_nolib(wd, linestyle->nodetree);
    }
  }
}

static void write_cachefile(WriteData *wd, CacheFile *cache_file, const void *id_address)
{
  if (cache_file->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    BLI_listbase_clear(&cache_file->object_paths);
    cache_file->handle = NULL;
    memset(cache_file->handle_filepath, 0, sizeof(cache_file->handle_filepath));
    cache_file->handle_readers = NULL;

    writestruct_at_address(wd, ID_CF, CacheFile, 1, id_address, cache_file);

    if (cache_file->adt) {
      write_animdata(wd, cache_file->adt);
    }
  }
}

static void write_workspace(BlendWriter *writer, WorkSpace *workspace, const void *id_address)
{
  BLO_write_id_struct(writer, WorkSpace, id_address, &workspace->id);
  write_iddata(writer->wd, &workspace->id);
  BLO_write_struct_list(writer, WorkSpaceLayout, &workspace->layouts);
  BLO_write_struct_list(writer, WorkSpaceDataRelation, &workspace->hook_layout_relations);
  BLO_write_struct_list(writer, wmOwnerID, &workspace->owner_ids);
  BLO_write_struct_list(writer, bToolRef, &workspace->tools);
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if (tref->properties) {
      IDP_WriteProperty_new_api(tref->properties, writer);
    }
  }
}

static void write_hair(WriteData *wd, Hair *hair, const void *id_address)
{
  if (hair->id.us > 0 || wd->use_memfile) {
    CustomDataLayer *players = NULL, players_buff[CD_TEMP_CHUNK_SIZE];
    CustomDataLayer *clayers = NULL, clayers_buff[CD_TEMP_CHUNK_SIZE];
    CustomData_file_write_prepare(&hair->pdata, &players, players_buff, ARRAY_SIZE(players_buff));
    CustomData_file_write_prepare(&hair->cdata, &clayers, clayers_buff, ARRAY_SIZE(clayers_buff));

    /* Write LibData */
    writestruct_at_address(wd, ID_HA, Hair, 1, id_address, hair);
    write_iddata(wd, &hair->id);

    /* Direct data */
    write_customdata(wd, &hair->id, hair->totpoint, &hair->pdata, players, CD_MASK_ALL);
    write_customdata(wd, &hair->id, hair->totcurve, &hair->cdata, clayers, CD_MASK_ALL);
    writedata(wd, DATA, sizeof(void *) * hair->totcol, hair->mat);
    if (hair->adt) {
      write_animdata(wd, hair->adt);
    }

    /* Remove temporary data. */
    if (players && players != players_buff) {
      MEM_freeN(players);
    }
    if (clayers && clayers != clayers_buff) {
      MEM_freeN(clayers);
    }
  }
}

static void write_pointcloud(WriteData *wd, PointCloud *pointcloud, const void *id_address)
{
  if (pointcloud->id.us > 0 || wd->use_memfile) {
    CustomDataLayer *players = NULL, players_buff[CD_TEMP_CHUNK_SIZE];
    CustomData_file_write_prepare(
        &pointcloud->pdata, &players, players_buff, ARRAY_SIZE(players_buff));

    /* Write LibData */
    writestruct_at_address(wd, ID_PT, PointCloud, 1, id_address, pointcloud);
    write_iddata(wd, &pointcloud->id);

    /* Direct data */
    write_customdata(
        wd, &pointcloud->id, pointcloud->totpoint, &pointcloud->pdata, players, CD_MASK_ALL);
    writedata(wd, DATA, sizeof(void *) * pointcloud->totcol, pointcloud->mat);
    if (pointcloud->adt) {
      write_animdata(wd, pointcloud->adt);
    }

    /* Remove temporary data. */
    if (players && players != players_buff) {
      MEM_freeN(players);
    }
  }
}

static void write_volume(WriteData *wd, Volume *volume, const void *id_address)
{
  if (volume->id.us > 0 || wd->use_memfile) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    volume->runtime.grids = 0;

    /* write LibData */
    writestruct_at_address(wd, ID_VO, Volume, 1, id_address, volume);
    write_iddata(wd, &volume->id);

    /* direct data */
    writedata(wd, DATA, sizeof(void *) * volume->totcol, volume->mat);
    if (volume->adt) {
      write_animdata(wd, volume->adt);
    }

    if (volume->packedfile) {
      PackedFile *pf = volume->packedfile;
      writestruct(wd, DATA, PackedFile, 1, pf);
      writedata(wd, DATA, pf->size, pf->data);
    }
  }
}

static void write_simulation(WriteData *wd, Simulation *simulation)
{
  if (simulation->id.us > 0 || wd->use_memfile) {
    writestruct(wd, ID_SIM, Simulation, 1, simulation);
    write_iddata(wd, &simulation->id);

    if (simulation->adt) {
      write_animdata(wd, simulation->adt);
    }

    /* nodetree is integral part of simulation, no libdata */
    if (simulation->nodetree) {
      writestruct(wd, DATA, bNodeTree, 1, simulation->nodetree);
      write_nodetree_nolib(wd, simulation->nodetree);
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

      writestruct(wd, ID_LI, Library, 1, main->curlib);
      write_iddata(wd, &main->curlib->id);

      if (main->curlib->packedfile) {
        PackedFile *pf = main->curlib->packedfile;
        writestruct(wd, DATA, PackedFile, 1, pf);
        writedata(wd, DATA, pf->size, pf->data);
        if (wd->use_memfile == false) {
          printf("write packed .blend: %s\n", main->curlib->name);
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
                  main->curlib->filepath);
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
                              const BlendThumbnail *thumb)
{
  BHead bhead;
  ListBase mainlist;
  char buf[16];
  WriteData *wd;

  blo_split_main(&mainlist, mainvar);

  wd = mywrite_begin(ww, compare, current);

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

  OverrideLibraryStorage *override_storage =
      wd->use_memfile ? NULL : BKE_lib_override_library_operations_store_initialize();

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

        const bool do_override = !ELEM(override_storage, NULL, bmain) && id->override_library;

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
        /* Those listbase data change every time we add/remove an ID, and also often when renaming
         * one (due to re-sorting). This avoids generating a lot of false 'is changed' detections
         * between undo steps. */
        ((ID *)id_buffer)->prev = NULL;
        ((ID *)id_buffer)->next = NULL;

        BlendWriter writer = {wd};

        switch ((ID_Type)GS(id->name)) {
          case ID_WM:
            write_windowmanager(&writer, (wmWindowManager *)id_buffer, id);
            break;
          case ID_WS:
            write_workspace(&writer, (WorkSpace *)id_buffer, id);
            break;
          case ID_SCR:
            write_screen(wd, (bScreen *)id_buffer, id);
            break;
          case ID_MC:
            write_movieclip(wd, (MovieClip *)id_buffer, id);
            break;
          case ID_MSK:
            write_mask(wd, (Mask *)id_buffer, id);
            break;
          case ID_SCE:
            write_scene(wd, (Scene *)id_buffer, id);
            break;
          case ID_CU:
            write_curve(wd, (Curve *)id_buffer, id);
            break;
          case ID_MB:
            write_mball(wd, (MetaBall *)id_buffer, id);
            break;
          case ID_IM:
            write_image(wd, (Image *)id_buffer, id);
            break;
          case ID_CA:
            write_camera(wd, (Camera *)id_buffer, id);
            break;
          case ID_LA:
            write_light(wd, (Light *)id_buffer, id);
            break;
          case ID_LT:
            write_lattice(wd, (Lattice *)id_buffer, id);
            break;
          case ID_VF:
            write_vfont(wd, (VFont *)id_buffer, id);
            break;
          case ID_KE:
            write_key(wd, (Key *)id_buffer, id);
            break;
          case ID_WO:
            write_world(wd, (World *)id_buffer, id);
            break;
          case ID_TXT:
            write_text(wd, (Text *)id_buffer, id);
            break;
          case ID_SPK:
            write_speaker(wd, (Speaker *)id_buffer, id);
            break;
          case ID_LP:
            write_probe(wd, (LightProbe *)id_buffer, id);
            break;
          case ID_SO:
            write_sound(wd, (bSound *)id_buffer, id);
            break;
          case ID_GR:
            write_collection(wd, (Collection *)id_buffer, id);
            break;
          case ID_AR:
            write_armature(wd, (bArmature *)id_buffer, id);
            break;
          case ID_AC:
            write_action(wd, (bAction *)id_buffer, id);
            break;
          case ID_OB:
            write_object(wd, (Object *)id_buffer, id);
            break;
          case ID_MA:
            write_material(wd, (Material *)id_buffer, id);
            break;
          case ID_TE:
            write_texture(wd, (Tex *)id_buffer, id);
            break;
          case ID_ME:
            write_mesh(wd, (Mesh *)id_buffer, id);
            break;
          case ID_PA:
            write_particlesettings(wd, (ParticleSettings *)id_buffer, id);
            break;
          case ID_NT:
            write_nodetree(wd, (bNodeTree *)id_buffer, id);
            break;
          case ID_BR:
            write_brush(wd, (Brush *)id_buffer, id);
            break;
          case ID_PAL:
            write_palette(wd, (Palette *)id_buffer, id);
            break;
          case ID_PC:
            write_paintcurve(wd, (PaintCurve *)id_buffer, id);
            break;
          case ID_GD:
            write_gpencil(wd, (bGPdata *)id_buffer, id);
            break;
          case ID_LS:
            write_linestyle(wd, (FreestyleLineStyle *)id_buffer, id);
            break;
          case ID_CF:
            write_cachefile(wd, (CacheFile *)id_buffer, id);
            break;
          case ID_HA:
            write_hair(wd, (Hair *)id_buffer, id);
            break;
          case ID_PT:
            write_pointcloud(wd, (PointCloud *)id_buffer, id);
            break;
          case ID_VO:
            write_volume(wd, (Volume *)id_buffer, id);
            break;
          case ID_SIM:
            write_simulation(wd, (Simulation *)id);
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

  if (write_flags & G_FILE_USERPREFS) {
    write_userdef(wd, &U);
  }

  /* Write DNA last, because (to be implemented) test for which structs are written.
   *
   * Note that we *borrow* the pointer to 'DNAstr',
   * so writing each time uses the same address and doesn't cause unnecessary undo overhead. */
  writedata(wd, DNA1, wd->sdna->data_len, wd->sdna->data);

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
                    int write_flags,
                    ReportList *reports,
                    const BlendThumbnail *thumb)
{
  char tempname[FILE_MAX + 1];
  eWriteWrapType ww_type;
  WriteWrap ww;

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
  if (write_flags & G_FILE_RELATIVE_REMAP) {
    char dir_src[FILE_MAX];
    char dir_dst[FILE_MAX];
    BLI_split_dir_part(mainvar->name, dir_src, sizeof(dir_src));
    BLI_split_dir_part(filepath, dir_dst, sizeof(dir_dst));

    /* Just in case there is some subtle difference. */
    BLI_path_normalize(mainvar->name, dir_dst);
    BLI_path_normalize(mainvar->name, dir_src);

    if (G.relbase_valid && (BLI_path_cmp(dir_dst, dir_src) == 0)) {
      /* Saved to same path. Nothing to do. */
      write_flags &= ~G_FILE_RELATIVE_REMAP;
    }
    else {
      /* Check if we need to backup and restore paths. */
      if (UNLIKELY(G_FILE_SAVE_COPY & write_flags)) {
        path_list_backup = BKE_bpath_list_backup(mainvar, path_list_flag);
      }

      if (G.relbase_valid) {
        /* Saved, make relative paths relative to new location (if possible). */
        BKE_bpath_relative_rebase(mainvar, dir_src, dir_dst, NULL);
      }
      else {
        /* Unsaved, make all relative. */
        BKE_bpath_relative_convert(mainvar, dir_dst, NULL);
      }
    }
  }

  /* actual file writing */
  const bool err = write_file_handle(mainvar, &ww, NULL, NULL, write_flags, thumb);

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
  if (write_flags & G_FILE_HISTORY) {
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
  write_flags &= ~G_FILE_USERPREFS;

  const bool err = write_file_handle(mainvar, NULL, compare, current, write_flags, NULL);

  return (err == 0);
}

void BLO_write_raw(BlendWriter *writer, int size_in_bytes, const void *data_ptr)
{
  writedata(writer->wd, DATA, size_in_bytes, data_ptr);
}

void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr)
{
  int struct_id = BLO_get_struct_id_by_name(writer, struct_name);
  BLO_write_struct_by_id(writer, struct_id, data_ptr);
}

void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    int array_size,
                                    const void *data_ptr)
{
  int struct_id = BLO_get_struct_id_by_name(writer, struct_name);
  BLO_write_struct_array_by_id(writer, struct_id, array_size, data_ptr);
}

void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr)
{
  writestruct_nr(writer->wd, DATA, struct_id, 1, data_ptr);
}

void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  int struct_id,
                                  int array_size,
                                  const void *data_ptr)
{
  writestruct_nr(writer->wd, DATA, struct_id, array_size, data_ptr);
}

void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, ListBase *list)
{
  writelist_nr(writer->wd, DATA, struct_id, list);
}

void BLO_write_struct_list_by_name(BlendWriter *writer, const char *struct_name, ListBase *list)
{
  BLO_write_struct_list_by_id(writer, BLO_get_struct_id_by_name(writer, struct_name), list);
}

void blo_write_id_struct(BlendWriter *writer, int struct_id, const void *id_address, const ID *id)
{
  writestruct_at_address_nr(writer->wd, GS(id->name), struct_id, 1, id_address, id);
}

int BLO_get_struct_id_by_name(BlendWriter *writer, const char *struct_name)
{
  int struct_id = DNA_struct_find_nr(writer->wd->sdna, struct_name);
  BLI_assert(struct_id >= 0);
  return struct_id;
}

void BLO_write_int32_array(BlendWriter *writer, int size, const int32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int32_t) * size, data_ptr);
}

void BLO_write_uint32_array(BlendWriter *writer, int size, const uint32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(uint32_t) * size, data_ptr);
}

void BLO_write_float_array(BlendWriter *writer, int size, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float) * size, data_ptr);
}

void BLO_write_float3_array(BlendWriter *writer, int size, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float) * 3 * size, data_ptr);
}

/**
 * Write a null terminated string.
 */
void BLO_write_string(BlendWriter *writer, const char *str)
{
  if (str != NULL) {
    BLO_write_raw(writer, strlen(str) + 1, str);
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
