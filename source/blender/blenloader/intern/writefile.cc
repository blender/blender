/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup blenloader
 */

/**
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
 *
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
 * - write #USER (#UserDef struct) if filename is `~/.config/blender/X.XX/config/startup.blend`.
 */

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h>
#else
#  include <unistd.h> /* FreeBSD, for write() and close(). */
#endif

#include "BLI_utildefines.h"

#include "CLG_log.h"

/* allow writefile to use deprecated functionality (for forward compatibility code) */
#define DNA_DEPRECATED_ALLOW

#include "DNA_collection_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_link_utils.h"
#include "BLI_linklist.h"
#include "BLI_math_base.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"
#include "MEM_guardedalloc.h" /* MEM_freeN */

#include "BKE_blender_version.h"
#include "BKE_bpath.h"
#include "BKE_global.h" /* for G */
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
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

#include <zstd.h>

/* Make preferences read-only. */
#define U (*((const UserDef *)&U))

/* ********* my write, buffered writing with minimum size chunks ************ */

/* Use optimal allocation since blocks of this size are kept in memory for undo. */
#define MEM_BUFFER_SIZE MEM_SIZE_OPTIMAL(1 << 17) /* 128kb */
#define MEM_CHUNK_SIZE MEM_SIZE_OPTIMAL(1 << 15)  /* ~32kb */

#define ZSTD_BUFFER_SIZE (1 << 21) /* 2mb */
#define ZSTD_CHUNK_SIZE (1 << 20)  /* 1mb */

#define ZSTD_COMPRESSION_LEVEL 3

static CLG_LogRef LOG = {"blo.writefile"};

/** Use if we want to store how many bytes have been written to the file. */
// #define USE_WRITE_DATA_LEN

/* -------------------------------------------------------------------- */
/** \name Internal Write Wrapper's (Abstracts Compression)
 * \{ */

enum eWriteWrapType {
  WW_WRAP_NONE = 1,
  WW_WRAP_ZSTD,
};

struct ZstdFrame {
  struct ZstdFrame *next, *prev;

  uint32_t compressed_size;
  uint32_t uncompressed_size;
};

struct WriteWrap {
  /* callbacks */
  bool (*open)(WriteWrap *ww, const char *filepath);
  bool (*close)(WriteWrap *ww);
  size_t (*write)(WriteWrap *ww, const char *data, size_t data_len);

  /* Buffer output (we only want when output isn't already buffered). */
  bool use_buf;

  /* internal */
  int file_handle;
  struct {
    ListBase threadpool;
    ListBase tasks;
    ThreadMutex mutex;
    ThreadCondition condition;
    int next_frame;
    int num_frames;

    int level;
    ListBase frames;

    bool write_error;
  } zstd;
};

/* none */
static bool ww_open_none(WriteWrap *ww, const char *filepath)
{
  int file;

  file = BLI_open(filepath, O_BINARY + O_WRONLY + O_CREAT + O_TRUNC, 0666);

  if (file != -1) {
    ww->file_handle = file;
    return true;
  }

  return false;
}
static bool ww_close_none(WriteWrap *ww)
{
  return (close(ww->file_handle) != -1);
}
static size_t ww_write_none(WriteWrap *ww, const char *buf, size_t buf_len)
{
  return write(ww->file_handle, buf, buf_len);
}

/* zstd */

struct ZstdWriteBlockTask {
  ZstdWriteBlockTask *next, *prev;
  void *data;
  size_t size;
  int frame_number;
  WriteWrap *ww;
};

static void *zstd_write_task(void *userdata)
{
  ZstdWriteBlockTask *task = static_cast<ZstdWriteBlockTask *>(userdata);
  WriteWrap *ww = task->ww;

  size_t out_buf_len = ZSTD_compressBound(task->size);
  void *out_buf = MEM_mallocN(out_buf_len, "Zstd out buffer");
  size_t out_size = ZSTD_compress(
      out_buf, out_buf_len, task->data, task->size, ZSTD_COMPRESSION_LEVEL);

  MEM_freeN(task->data);

  BLI_mutex_lock(&ww->zstd.mutex);

  while (ww->zstd.next_frame != task->frame_number) {
    BLI_condition_wait(&ww->zstd.condition, &ww->zstd.mutex);
  }

  if (ZSTD_isError(out_size)) {
    ww->zstd.write_error = true;
  }
  else {
    if (ww_write_none(ww, static_cast<const char *>(out_buf), out_size) == out_size) {
      ZstdFrame *frameinfo = static_cast<ZstdFrame *>(
          MEM_mallocN(sizeof(ZstdFrame), "zstd frameinfo"));
      frameinfo->uncompressed_size = task->size;
      frameinfo->compressed_size = out_size;
      BLI_addtail(&ww->zstd.frames, frameinfo);
    }
    else {
      ww->zstd.write_error = true;
    }
  }

  ww->zstd.next_frame++;

  BLI_mutex_unlock(&ww->zstd.mutex);
  BLI_condition_notify_all(&ww->zstd.condition);

  MEM_freeN(out_buf);
  return nullptr;
}

static bool ww_open_zstd(WriteWrap *ww, const char *filepath)
{
  if (!ww_open_none(ww, filepath)) {
    return false;
  }

  /* Leave one thread open for the main writing logic, unless we only have one HW thread. */
  int num_threads = max_ii(1, BLI_system_thread_count() - 1);
  BLI_threadpool_init(&ww->zstd.threadpool, zstd_write_task, num_threads);
  BLI_mutex_init(&ww->zstd.mutex);
  BLI_condition_init(&ww->zstd.condition);

  return true;
}

static void zstd_write_u32_le(WriteWrap *ww, uint32_t val)
{
#ifdef __BIG_ENDIAN__
  BLI_endian_switch_uint32(&val);
#endif
  ww_write_none(ww, (char *)&val, sizeof(uint32_t));
}

/* In order to implement efficient seeking when reading the .blend, we append
 * a skippable frame that encodes information about the other frames present
 * in the file.
 * The format here follows the upstream spec for seekable files:
 * https://github.com/facebook/zstd/blob/master/contrib/seekable_format/zstd_seekable_compression_format.md
 * If this information is not present in a file (e.g. if it was compressed
 * with external tools), it can still be opened in Blender, but seeking will
 * not be supported, so more memory might be needed. */
static void zstd_write_seekable_frames(WriteWrap *ww)
{
  /* Write seek table header (magic number and frame size). */
  zstd_write_u32_le(ww, 0x184D2A5E);

  /* The actual frame number might not match ww->zstd.num_frames if there was a write error. */
  const uint32_t num_frames = BLI_listbase_count(&ww->zstd.frames);
  /* Each frame consists of two u32, so 8 bytes each.
   * After the frames, a footer containing two u32 and one byte (9 bytes total) is written. */
  const uint32_t frame_size = num_frames * 8 + 9;
  zstd_write_u32_le(ww, frame_size);

  /* Write seek table entries. */
  LISTBASE_FOREACH (ZstdFrame *, frame, &ww->zstd.frames) {
    zstd_write_u32_le(ww, frame->compressed_size);
    zstd_write_u32_le(ww, frame->uncompressed_size);
  }

  /* Write seek table footer (number of frames, option flags and second magic number). */
  zstd_write_u32_le(ww, num_frames);
  const char flags = 0; /* We don't store checksums for each frame. */
  ww_write_none(ww, &flags, 1);
  zstd_write_u32_le(ww, 0x8F92EAB1);
}

static bool ww_close_zstd(WriteWrap *ww)
{
  BLI_threadpool_end(&ww->zstd.threadpool);
  BLI_freelistN(&ww->zstd.tasks);

  BLI_mutex_end(&ww->zstd.mutex);
  BLI_condition_end(&ww->zstd.condition);

  zstd_write_seekable_frames(ww);
  BLI_freelistN(&ww->zstd.frames);

  return ww_close_none(ww) && !ww->zstd.write_error;
}

static size_t ww_write_zstd(WriteWrap *ww, const char *buf, size_t buf_len)
{
  if (ww->zstd.write_error) {
    return 0;
  }

  ZstdWriteBlockTask *task = static_cast<ZstdWriteBlockTask *>(
      MEM_mallocN(sizeof(ZstdWriteBlockTask), __func__));
  task->data = MEM_mallocN(buf_len, __func__);
  memcpy(task->data, buf, buf_len);
  task->size = buf_len;
  task->frame_number = ww->zstd.num_frames++;
  task->ww = ww;

  BLI_mutex_lock(&ww->zstd.mutex);
  BLI_addtail(&ww->zstd.tasks, task);

  /* If there's a free worker thread, just push the block into that thread.
   * Otherwise, we wait for the earliest thread to finish.
   * We look up the earliest thread while holding the mutex, but release it
   * before joining the thread to prevent a deadlock. */
  ZstdWriteBlockTask *first_task = static_cast<ZstdWriteBlockTask *>(ww->zstd.tasks.first);
  BLI_mutex_unlock(&ww->zstd.mutex);
  if (!BLI_available_threads(&ww->zstd.threadpool)) {
    BLI_threadpool_remove(&ww->zstd.threadpool, first_task);

    /* If the task list was empty before we pushed our task, there should
     * always be a free thread. */
    BLI_assert(first_task != task);
    BLI_remlink(&ww->zstd.tasks, first_task);
    MEM_freeN(first_task);
  }
  BLI_threadpool_insert(&ww->zstd.threadpool, task);

  return buf_len;
}

/* --- end compression types --- */

static void ww_handle_init(eWriteWrapType ww_type, WriteWrap *r_ww)
{
  memset(r_ww, 0, sizeof(*r_ww));

  switch (ww_type) {
    case WW_WRAP_ZSTD: {
      r_ww->open = ww_open_zstd;
      r_ww->close = ww_close_zstd;
      r_ww->write = ww_write_zstd;
      r_ww->use_buf = true;
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

struct WriteData {
  const SDNA *sdna;

  struct {
    /** Use for file and memory writing (size stored in max_size). */
    uchar *buf;
    /** Number of bytes used in #WriteData.buf (flushed when exceeded). */
    size_t used_len;

    /** Maximum size of the buffer. */
    size_t max_size;
    /** Threshold above which writes get their own chunk. */
    size_t chunk_size;
  } buffer;

#ifdef USE_WRITE_DATA_LEN
  /** Total number of bytes written. */
  size_t write_len;
#endif

  /** Set on unlikely case of an error (ignores further file writing). */
  bool error;

  /** #MemFile writing (used for undo). */
  MemFileWriteData mem;
  /** When true, write to #WriteData.current, could also call 'is_undo'. */
  bool use_memfile;

  /**
   * Wrap writing, so we can use zstd or
   * other compression types later, see: G_FILE_COMPRESS
   * Will be nullptr for UNDO.
   */
  WriteWrap *ww;
};

struct BlendWriter {
  WriteData *wd;
};

static WriteData *writedata_new(WriteWrap *ww)
{
  WriteData *wd = static_cast<WriteData *>(MEM_callocN(sizeof(*wd), "writedata"));

  wd->sdna = DNA_sdna_current_get();

  wd->ww = ww;

  if ((ww == nullptr) || (ww->use_buf)) {
    if (ww == nullptr) {
      wd->buffer.max_size = MEM_BUFFER_SIZE;
      wd->buffer.chunk_size = MEM_CHUNK_SIZE;
    }
    else {
      wd->buffer.max_size = ZSTD_BUFFER_SIZE;
      wd->buffer.chunk_size = ZSTD_CHUNK_SIZE;
    }
    wd->buffer.buf = static_cast<uchar *>(MEM_mallocN(wd->buffer.max_size, "wd->buffer.buf"));
  }

  return wd;
}

static void writedata_do_write(WriteData *wd, const void *mem, size_t memlen)
{
  if ((wd == nullptr) || wd->error || (mem == nullptr) || memlen < 1) {
    return;
  }

  if (memlen > INT_MAX) {
    BLI_assert_msg(0, "Cannot write chunks bigger than INT_MAX.");
    return;
  }

  if (UNLIKELY(wd->error)) {
    return;
  }

  /* memory based save */
  if (wd->use_memfile) {
    BLO_memfile_chunk_add(&wd->mem, static_cast<const char *>(mem), memlen);
  }
  else {
    if (wd->ww->write(wd->ww, static_cast<const char *>(mem), memlen) != memlen) {
      wd->error = true;
    }
  }
}

static void writedata_free(WriteData *wd)
{
  if (wd->buffer.buf) {
    MEM_freeN(wd->buffer.buf);
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
  if (wd->buffer.used_len != 0) {
    writedata_do_write(wd, wd->buffer.buf, wd->buffer.used_len);
    wd->buffer.used_len = 0;
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

  if (UNLIKELY(adr == nullptr)) {
    BLI_assert(0);
    return;
  }

#ifdef USE_WRITE_DATA_LEN
  wd->write_len += len;
#endif

  if (wd->buffer.buf == nullptr) {
    writedata_do_write(wd, adr, len);
  }
  else {
    /* if we have a single big chunk, write existing data in
     * buffer and write out big chunk in smaller pieces */
    if (len > wd->buffer.chunk_size) {
      if (wd->buffer.used_len != 0) {
        writedata_do_write(wd, wd->buffer.buf, wd->buffer.used_len);
        wd->buffer.used_len = 0;
      }

      do {
        size_t writelen = MIN2(len, wd->buffer.chunk_size);
        writedata_do_write(wd, adr, writelen);
        adr = (const char *)adr + writelen;
        len -= writelen;
      } while (len > 0);

      return;
    }

    /* if data would overflow buffer, write out the buffer */
    if (len + wd->buffer.used_len > wd->buffer.max_size - 1) {
      writedata_do_write(wd, wd->buffer.buf, wd->buffer.used_len);
      wd->buffer.used_len = 0;
    }

    /* append data at end of buffer */
    memcpy(&wd->buffer.buf[wd->buffer.used_len], adr, len);
    wd->buffer.used_len += len;
  }
}

/**
 * BeGiN initializer for mywrite
 * \param ww: File write wrapper.
 * \param compare: Previous memory file (can be nullptr).
 * \param current: The current memory file (can be nullptr).
 * \warning Talks to other functions with global parameters
 */
static WriteData *mywrite_begin(WriteWrap *ww, MemFile *compare, MemFile *current)
{
  WriteData *wd = writedata_new(ww);

  if (current != nullptr) {
    BLO_memfile_write_init(&wd->mem, current, compare);
    wd->use_memfile = true;
  }

  return wd;
}

/**
 * END the mywrite wrapper
 * \return True if write failed
 * \return unknown global variable otherwise
 * \warning Talks to other functions with global parameters
 */
static bool mywrite_end(WriteData *wd)
{
  if (wd->buffer.used_len != 0) {
    writedata_do_write(wd, wd->buffer.buf, wd->buffer.used_len);
    wd->buffer.used_len = 0;
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

    /* If current next memchunk does not match the ID we are about to write, or is not the _first_
     * one for said ID, try to find the correct memchunk in the mapping using ID's session_uuid. */
    MemFileChunk *curr_memchunk = wd->mem.reference_current_chunk;
    MemFileChunk *prev_memchunk = curr_memchunk != nullptr ?
                                      static_cast<MemFileChunk *>(curr_memchunk->prev) :
                                      nullptr;
    if (wd->mem.id_session_uuid_mapping != nullptr &&
        (curr_memchunk == nullptr || curr_memchunk->id_session_uuid != id->session_uuid ||
         (prev_memchunk != nullptr &&
          (prev_memchunk->id_session_uuid == curr_memchunk->id_session_uuid))))
    {
      void *ref = BLI_ghash_lookup(wd->mem.id_session_uuid_mapping,
                                   POINTER_FROM_UINT(id->session_uuid));
      if (ref != nullptr) {
        wd->mem.reference_current_chunk = static_cast<MemFileChunk *>(ref);
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
static void mywrite_id_end(WriteData *wd, ID * /*id*/)
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

  if (adr == nullptr || data == nullptr || nr == 0) {
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
  mywrite(wd, data, size_t(bh.len));
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

  if (adr == nullptr || len == 0) {
    return;
  }

  if (len > INT_MAX) {
    BLI_assert_msg(0, "Cannot write chunks bigger than INT_MAX.");
    return;
  }

  /* align to 4 (writes uninitialized bytes in some cases) */
  len = (len + 3) & ~size_t(3);

  /* init BHead */
  bh.code = filecode;
  bh.old = adr;
  bh.nr = 1;
  bh.SDNAnr = 0;
  bh.len = int(len);

  mywrite(wd, &bh, sizeof(BHead));
  mywrite(wd, adr, len);
}

/* use this to force writing of lists in same order as reading (using link_list) */
static void writelist_nr(WriteData *wd, int filecode, const int struct_nr, const ListBase *lb)
{
  const Link *link = static_cast<Link *>(lb->first);

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
 * Take care using 'use_active_win', since we won't want the currently active window
 * to change which scene renders (currently only used for undo).
 */
static void current_screen_compat(Main *mainvar,
                                  bool use_active_win,
                                  bScreen **r_screen,
                                  Scene **r_scene,
                                  ViewLayer **r_view_layer)
{
  wmWindowManager *wm;
  wmWindow *window = nullptr;

  /* find a global current screen in the first open window, to have
   * a reasonable default for reading in older versions */
  wm = static_cast<wmWindowManager *>(mainvar->wm.first);

  if (wm) {
    if (use_active_win) {
      /* write the active window into the file, needed for multi-window undo #43424 */
      for (window = static_cast<wmWindow *>(wm->windows.first); window; window = window->next) {
        if (window->active) {
          break;
        }
      }

      /* fallback */
      if (window == nullptr) {
        window = static_cast<wmWindow *>(wm->windows.first);
      }
    }
    else {
      window = static_cast<wmWindow *>(wm->windows.first);
    }
  }

  *r_screen = (window) ? BKE_workspace_active_screen_get(window->workspace_hook) : nullptr;
  *r_scene = (window) ? window->scene : nullptr;
  *r_view_layer = (window && *r_scene) ? BKE_view_layer_find(*r_scene, window->view_layer_name) :
                                         nullptr;
}

struct RenderInfo {
  int sfra;
  int efra;
  char scene_name[MAX_ID_NAME - 2];
};

/**
 * This was originally added for the historic render-daemon feature,
 * now write because it can be easily extracted without reading the whole blend file.
 *
 * See: `scripts/modules/blend_render_info.py`
 */
static void write_renderinfo(WriteData *wd, Main *mainvar)
{
  bScreen *curscreen;
  Scene *curscene = nullptr;
  ViewLayer *view_layer;

  /* XXX in future, handle multiple windows with multiple screens? */
  current_screen_compat(mainvar, false, &curscreen, &curscene, &view_layer);

  LISTBASE_FOREACH (Scene *, sce, &mainvar->scenes) {
    if (!ID_IS_LINKED(sce) && (sce == curscene || (sce->r.scemode & R_BG_RENDER))) {
      RenderInfo data;
      data.sfra = sce->r.sfra;
      data.efra = sce->r.efra;
      memset(data.scene_name, 0, sizeof(data.scene_name));

      BLI_strncpy(data.scene_name, sce->id.name + 2, sizeof(data.scene_name));

      writedata(wd, BLO_CODE_REND, sizeof(data), &data);
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
  writestruct(writer->wd, BLO_CODE_USER, UserDef, 1, userdef);

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

  LISTBASE_FOREACH (const bUserScriptDirectory *, script_dir, &userdef->script_directories) {
    BLO_write_struct(writer, bUserScriptDirectory, script_dir);
  }

  LISTBASE_FOREACH (const bUserAssetLibrary *, asset_library_ref, &userdef->asset_libraries) {
    BLO_write_struct(writer, bUserAssetLibrary, asset_library_ref);
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
        for (id = static_cast<ID *>(lbarray[tot]->first); id; id = static_cast<ID *>(id->next)) {
          if (id->us > 0 && ((id->tag & LIB_TAG_EXTERN) || ((id->tag & LIB_TAG_INDIRECT) &&
                                                            (id->flag & LIB_INDIRECT_WEAK_LINK))))
          {
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

      void *runtime_name_data = main->curlib->runtime.name_map;
      main->curlib->runtime.name_map = nullptr;

      BlendWriter writer = {wd};
      writestruct(wd, ID_LI, Library, 1, main->curlib);
      BKE_id_blend_write(&writer, &main->curlib->id);

      main->curlib->runtime.name_map = static_cast<UniqueName_Map *>(runtime_name_data);

      if (main->curlib->packedfile) {
        BKE_packedfile_blend_write(&writer, main->curlib->packedfile);
        if (wd->use_memfile == false) {
          CLOG_INFO(&LOG, 2, "Write packed .blend: %s", main->curlib->filepath);
        }
      }

      /* Write link placeholders for all direct linked IDs. */
      while (a--) {
        for (id = static_cast<ID *>(lbarray[a]->first); id; id = static_cast<ID *>(id->next)) {
          if (id->us > 0 && ((id->tag & LIB_TAG_EXTERN) || ((id->tag & LIB_TAG_INDIRECT) &&
                                                            (id->flag & LIB_INDIRECT_WEAK_LINK))))
          {
            if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
              CLOG_ERROR(&LOG,
                         "Data-block '%s' from lib '%s' is not linkable, but is flagged as "
                         "directly linked",
                         id->name,
                         main->curlib->filepath_abs);
            }
            writestruct(wd, ID_LINK_PLACEHOLDER, ID, 1, id);
          }
        }
      }
    }
  }

  mywrite_flush(wd);
}

#ifdef WITH_BUILDINFO
extern "C" unsigned long build_commit_timestamp;
extern "C" char build_hash[];
#endif

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
  memset(fg.filepath, 0, sizeof(fg.filepath));
  memset(fg.build_hash, 0, sizeof(fg.build_hash));
  fg._pad1 = nullptr;

  current_screen_compat(mainvar, is_undo, &screen, &scene, &view_layer);

  /* XXX still remap G */
  fg.curscreen = screen;
  fg.curscene = scene;
  fg.cur_view_layer = view_layer;

  /* prevent to save this, is not good convention, and feature with concerns... */
  fg.fileflags = (fileflags & ~G_FILE_FLAG_ALL_RUNTIME);

  fg.globalf = G.f;
  /* Write information needed for recovery. */
  if (fileflags & G_FILE_RECOVER_WRITE) {
    STRNCPY(fg.filepath, mainvar->filepath);
  }
  BLI_snprintf(subvstr, sizeof(subvstr), "%4d", BLENDER_FILE_SUBVERSION);
  memcpy(fg.subvstr, subvstr, 4);

  fg.subversion = BLENDER_FILE_SUBVERSION;
  fg.minversion = BLENDER_FILE_MIN_VERSION;
  fg.minsubversion = BLENDER_FILE_MIN_SUBVERSION;
#ifdef WITH_BUILDINFO
  /* TODO(sergey): Add branch name to file as well? */
  fg.build_commit_timestamp = build_commit_timestamp;
  BLI_strncpy(fg.build_hash, build_hash, sizeof(fg.build_hash));
#else
  fg.build_commit_timestamp = 0;
  BLI_strncpy(fg.build_hash, "unknown", sizeof(fg.build_hash));
#endif
  writestruct(wd, BLO_CODE_GLOB, FileGlobal, 1, &fg);
}

/**
 * Preview image, first 2 values are width and height
 * second are an RGBA image (uchar).
 * \note this uses 'TEST' since new types will segfault on file load for older blender versions.
 */
static void write_thumb(WriteData *wd, const BlendThumbnail *thumb)
{
  if (thumb) {
    writedata(wd, BLO_CODE_TEST, BLEN_THUMB_MEMSIZE_FILE(thumb->width, thumb->height), thumb);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Writing (Private)
 * \{ */

#define ID_BUFFER_STATIC_SIZE 8192

typedef struct BLO_Write_IDBuffer {
  const struct IDTypeInfo *id_type;
  ID *temp_id;
  char id_buffer_static[ID_BUFFER_STATIC_SIZE];
} BLO_Write_IDBuffer;

static void id_buffer_init_for_id_type(BLO_Write_IDBuffer *id_buffer, const IDTypeInfo *id_type)
{
  if (id_type != id_buffer->id_type) {
    const size_t idtype_struct_size = id_type->struct_size;
    if (idtype_struct_size > ID_BUFFER_STATIC_SIZE) {
      CLOG_ERROR(&LOG,
                 "ID maximum buffer size (%d bytes) is not big enough to fit IDs of type %s, "
                 "which needs %lu bytes",
                 ID_BUFFER_STATIC_SIZE,
                 id_type->name,
                 idtype_struct_size);
      id_buffer->temp_id = static_cast<ID *>(MEM_mallocN(idtype_struct_size, __func__));
    }
    else {
      if (static_cast<void *>(id_buffer->temp_id) != id_buffer->id_buffer_static) {
        MEM_SAFE_FREE(id_buffer->temp_id);
      }
      id_buffer->temp_id = reinterpret_cast<ID *>(id_buffer->id_buffer_static);
    }
    id_buffer->id_type = id_type;
  }
}

static void id_buffer_init_from_id(BLO_Write_IDBuffer *id_buffer, ID *id, const bool is_undo)
{
  BLI_assert(id_buffer->id_type == BKE_idtype_get_info_from_id(id));

  if (is_undo) {
    /* Record the changes that happened up to this undo push in
     * recalc_up_to_undo_push, and clear `recalc_after_undo_push` again
     * to start accumulating for the next undo push. */
    id->recalc_up_to_undo_push = id->recalc_after_undo_push;
    id->recalc_after_undo_push = 0;
  }

  /* Copy ID data itself into buffer, to be able to freely modify it. */
  const size_t idtype_struct_size = id_buffer->id_type->struct_size;
  ID *temp_id = id_buffer->temp_id;
  memcpy(temp_id, id, idtype_struct_size);

  /* Clear runtime data to reduce false detection of changed data in undo/redo context. */
  if (is_undo) {
    temp_id->tag &= LIB_TAG_KEEP_ON_UNDO;
  }
  else {
    temp_id->tag = 0;
  }
  temp_id->us = 0;
  temp_id->icon_id = 0;
  /* Those listbase data change every time we add/remove an ID, and also often when
   * renaming one (due to re-sorting). This avoids generating a lot of false 'is changed'
   * detections between undo steps. */
  temp_id->prev = nullptr;
  temp_id->next = nullptr;
  /* Those runtime pointers should never be set during writing stage, but just in case clear
   * them too. */
  temp_id->orig_id = nullptr;
  temp_id->newid = nullptr;
  /* Even though in theory we could be able to preserve this python instance across undo even
   * when we need to re-read the ID into its original address, this is currently cleared in
   * #direct_link_id_common in `readfile.c` anyway. */
  temp_id->py_instance = nullptr;
}

/* Helper callback for checking linked IDs used by given ID (assumed local), to ensure directly
 * linked data is tagged accordingly. */
static int write_id_direct_linked_data_process_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *id_self = cb_data->id_self;
  ID *id = *cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;

  if (id == nullptr || !ID_IS_LINKED(id)) {
    return IDWALK_RET_NOP;
  }
  BLI_assert(!ID_IS_LINKED(id_self));
  BLI_assert((cb_flag & IDWALK_CB_INDIRECT_USAGE) == 0);

  if (id_self->tag & LIB_TAG_RUNTIME) {
    return IDWALK_RET_NOP;
  }

  if (cb_flag & IDWALK_CB_DIRECT_WEAK_LINK) {
    id_lib_indirect_weak_link(id);
  }
  else {
    id_lib_extern(id);
  }

  return IDWALK_RET_NOP;
}

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

  wd = mywrite_begin(ww, compare, current);
  BlendWriter writer = {wd};

  /* Clear 'directly linked' flag for all linked data, these are not necessarily valid/up-to-date
   * info, they will be re-generated while write code is processing local IDs below. */
  if (!wd->use_memfile) {
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (mainvar, id_iter) {
      if (ID_IS_LINKED(id_iter) && BKE_idtype_idcode_is_linkable(GS(id_iter->name))) {
        if (USER_EXPERIMENTAL_TEST(&U, use_all_linked_data_direct)) {
          /* Forces all linked data to be considered as directly linked.
           * FIXME: Workaround some BAT tool limitations for Heist production, should be removed
           * asap afterward. */
          id_lib_extern(id_iter);
        }
        else if (ID_FAKE_USERS(id_iter) > 0 && id_iter->asset_data == nullptr) {
          /* Even though fake user is not directly editable by the user on linked data, it is a
           * common 'work-around' to set it in library files on data-blocks that need to be linked
           * but typically do not have an actual real user (e.g. texts, etc.).
           * See e.g. #105687 and #103867.
           *
           * Would be good to find a better solution, but for now consider these as directly linked
           * as well. */
          id_lib_extern(id_iter);
        }
        else {
          id_iter->tag |= LIB_TAG_INDIRECT;
          id_iter->tag &= ~LIB_TAG_EXTERN;
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }

  blo_split_main(&mainlist, mainvar);

  BLI_snprintf(buf,
               sizeof(buf),
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
                                                 nullptr :
                                                 BKE_lib_override_library_operations_store_init();

  /* This outer loop allows to save first data-blocks from real mainvar,
   * then the temp ones from override process,
   * if needed, without duplicating whole code. */
  Main *bmain = mainvar;
  BLO_Write_IDBuffer *id_buffer = BLO_write_allocate_id_buffer();
  do {
    ListBase *lbarray[INDEX_ID_MAX];
    int a = set_listbasepointers(bmain, lbarray);
    while (a--) {
      ID *id = static_cast<ID *>(lbarray[a]->first);

      if (id == nullptr || GS(id->name) == ID_LI) {
        continue; /* Libraries are handled separately below. */
      }

      const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
      id_buffer_init_for_id_type(id_buffer, id_type);

      for (; id; id = static_cast<ID *>(id->next)) {
        /* We should never attempt to write non-regular IDs
         * (i.e. all kind of temp/runtime ones). */
        BLI_assert(
            (id->tag & (LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT | LIB_TAG_NOT_ALLOCATED)) == 0);

        /* We only write unused IDs in undo case.
         * NOTE: All Scenes, WindowManagers and WorkSpaces should always be written to disk, so
         * their user-count should never be zero currently. */
        if (id->us == 0 && !wd->use_memfile) {
          BLI_assert(!ELEM(GS(id->name), ID_SCE, ID_WM, ID_WS));
          continue;
        }

        if ((id->tag & LIB_TAG_RUNTIME) != 0 && !wd->use_memfile) {
          /* Runtime IDs are never written to .blend files, and they should not influence
           * (in)direct status of linked IDs they may use. */
          continue;
        }

        const bool do_override = !ELEM(override_storage, nullptr, bmain) &&
                                 ID_IS_OVERRIDE_LIBRARY_REAL(id);

        /* If not writing undo data, properly set directly linked IDs as `LIB_TAG_EXTERN`. */
        if (!wd->use_memfile) {
          BKE_library_foreach_ID_link(
              bmain, id, write_id_direct_linked_data_process_cb, nullptr, IDWALK_READONLY);
        }

        if (do_override) {
          BKE_lib_override_library_operations_store_start(bmain, override_storage, id);
        }

        mywrite_id_begin(wd, id);

        id_buffer_init_from_id(id_buffer, id, wd->use_memfile);

        if (id_type->blend_write != nullptr) {
          id_type->blend_write(&writer, static_cast<ID *>(id_buffer->temp_id), id);
        }

        if (do_override) {
          BKE_lib_override_library_operations_store_end(override_storage, id);
        }

        mywrite_id_end(wd, id);
      }

      mywrite_flush(wd);
    }
  } while ((bmain != override_storage) && (bmain = override_storage));

  BLO_write_destroy_id_buffer(&id_buffer);

  if (override_storage) {
    BKE_lib_override_library_operations_store_finalize(override_storage);
    override_storage = nullptr;
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
  writedata(wd, BLO_CODE_DNA1, size_t(wd->sdna->data_len), wd->sdna->data);

  /* end of file */
  memset(&bhead, 0, sizeof(BHead));
  bhead.code = BLO_CODE_ENDB;
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
    return false;
  }

  if (strlen(name) < 2) {
    BKE_report(reports, RPT_ERROR, "Unable to make version backup: filename too short");
    return true;
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

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Writing (Public)
 * \{ */

bool BLO_write_file(Main *mainvar,
                    const char *filepath,
                    const int write_flags,
                    const struct BlendFileWriteParams *params,
                    ReportList *reports)
{
  BLI_assert(!BLI_path_is_rel(filepath));
  BLI_assert(BLI_path_is_abs_from_cwd(filepath));

  char tempname[FILE_MAX + 1];
  WriteWrap ww;

  eBLO_WritePathRemap remap_mode = params->remap_mode;
  const bool use_save_versions = params->use_save_versions;
  const bool use_save_as_copy = params->use_save_as_copy;
  const bool use_userdef = params->use_userdef;
  const BlendThumbnail *thumb = params->thumb;
  const bool relbase_valid = (mainvar->filepath[0] != '\0');

  /* path backup/restore */
  void *path_list_backup = nullptr;
  const eBPathForeachFlag path_list_flag = (BKE_BPATH_FOREACH_PATH_SKIP_LINKED |
                                            BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE);

  if (G.debug & G_DEBUG_IO && mainvar->lock != nullptr) {
    BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *BEFORE* save to disk");
    BLO_main_validate_libraries(mainvar, reports);
    BLO_main_validate_shapekeys(mainvar, reports);
  }

  /* open temporary file, so we preserve the original in case we crash */
  BLI_snprintf(tempname, sizeof(tempname), "%s@", filepath);

  ww_handle_init((write_flags & G_FILE_COMPRESS) ? WW_WRAP_ZSTD : WW_WRAP_NONE, &ww);

  if (ww.open(&ww, tempname) == false) {
    BKE_reportf(
        reports, RPT_ERROR, "Cannot open file %s for writing: %s", tempname, strerror(errno));
    return false;
  }

  if (remap_mode == BLO_WRITE_PATH_REMAP_ABSOLUTE) {
    /* Paths will already be absolute, no remapping to do. */
    if (relbase_valid == false) {
      remap_mode = BLO_WRITE_PATH_REMAP_NONE;
    }
  }

  /* Remapping of relative paths to new file location. */
  if (remap_mode != BLO_WRITE_PATH_REMAP_NONE) {
    if (remap_mode == BLO_WRITE_PATH_REMAP_RELATIVE) {
      /* Make all relative as none of the existing paths can be relative in an unsaved document. */
      if (relbase_valid == false) {
        remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE_ALL;
      }
    }

    /* The source path only makes sense to set if the file was saved (`relbase_valid`). */
    char dir_src[FILE_MAX];
    char dir_dst[FILE_MAX];

    /* Normalize the paths in case there is some subtle difference (so they can be compared). */
    if (relbase_valid) {
      BLI_path_split_dir_part(mainvar->filepath, dir_src, sizeof(dir_src));
      BLI_path_normalize(dir_src);
    }
    else {
      dir_src[0] = '\0';
    }
    BLI_path_split_dir_part(filepath, dir_dst, sizeof(dir_dst));
    BLI_path_normalize(dir_dst);

    /* Only for relative, not relative-all, as this means making existing paths relative. */
    if (remap_mode == BLO_WRITE_PATH_REMAP_RELATIVE) {
      if (relbase_valid && (BLI_path_cmp(dir_dst, dir_src) == 0)) {
        /* Saved to same path. Nothing to do. */
        remap_mode = BLO_WRITE_PATH_REMAP_NONE;
      }
    }
    else if (remap_mode == BLO_WRITE_PATH_REMAP_ABSOLUTE) {
      if (relbase_valid == false) {
        /* Unsaved, all paths are absolute.Even if the user manages to set a relative path,
         * there is no base-path that can be used to make it absolute. */
        remap_mode = BLO_WRITE_PATH_REMAP_NONE;
      }
    }

    if (remap_mode != BLO_WRITE_PATH_REMAP_NONE) {
      /* Some path processing (e.g. with libraries) may use the current `main->filepath`, if this
       * is not matching the path currently used for saving, unexpected paths corruptions can
       * happen. See #98201. */
      char mainvar_filepath_orig[FILE_MAX];
      STRNCPY(mainvar_filepath_orig, mainvar->filepath);
      STRNCPY(mainvar->filepath, filepath);

      /* Check if we need to backup and restore paths. */
      if (UNLIKELY(use_save_as_copy)) {
        path_list_backup = BKE_bpath_list_backup(mainvar, path_list_flag);
      }

      switch (remap_mode) {
        case BLO_WRITE_PATH_REMAP_RELATIVE:
          /* Saved, make relative paths relative to new location (if possible). */
          BLI_assert(relbase_valid);
          BKE_bpath_relative_rebase(mainvar, dir_src, dir_dst, nullptr);
          break;
        case BLO_WRITE_PATH_REMAP_RELATIVE_ALL:
          /* Make all relative (when requested or unsaved). */
          BKE_bpath_relative_convert(mainvar, dir_dst, nullptr);
          break;
        case BLO_WRITE_PATH_REMAP_ABSOLUTE:
          /* Make all absolute (when requested or unsaved). */
          BLI_assert(relbase_valid);
          BKE_bpath_absolute_convert(mainvar, dir_src, nullptr);
          break;
        case BLO_WRITE_PATH_REMAP_NONE:
          BLI_assert_unreachable(); /* Unreachable. */
          break;
      }

      STRNCPY(mainvar->filepath, mainvar_filepath_orig);
    }
  }

  /* actual file writing */
  const bool err = write_file_handle(
      mainvar, &ww, nullptr, nullptr, write_flags, use_userdef, thumb);

  ww.close(&ww);

  if (UNLIKELY(path_list_backup)) {
    BKE_bpath_list_restore(mainvar, path_list_flag, path_list_backup);
    BKE_bpath_list_free(path_list_backup);
  }

  if (err) {
    BKE_report(reports, RPT_ERROR, strerror(errno));
    remove(tempname);

    return false;
  }

  /* file save to temporary file was successful */
  /* now do reverse file history (move .blend1 -> .blend2, .blend -> .blend1) */
  if (use_save_versions) {
    const bool err_hist = do_history(filepath, reports);
    if (err_hist) {
      BKE_report(reports, RPT_ERROR, "Version backup failed (file saved with @)");
      return false;
    }
  }

  if (BLI_rename(tempname, filepath) != 0) {
    BKE_report(reports, RPT_ERROR, "Cannot change old file (file saved with @)");
    return false;
  }

  if (G.debug & G_DEBUG_IO && mainvar->lock != nullptr) {
    BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *AFTER* save to disk");
    BLO_main_validate_libraries(mainvar, reports);
  }

  return true;
}

bool BLO_write_file_mem(Main *mainvar, MemFile *compare, MemFile *current, int write_flags)
{
  bool use_userdef = false;

  const bool err = write_file_handle(
      mainvar, nullptr, compare, current, write_flags, use_userdef, nullptr);

  return (err == 0);
}

/*
 * API to handle writing IDs while clearing some of their runtime data.
 */

BLO_Write_IDBuffer *BLO_write_allocate_id_buffer()
{
  return MEM_cnew<BLO_Write_IDBuffer>(__func__);
}

void BLO_write_init_id_buffer_from_id(BLO_Write_IDBuffer *id_buffer, ID *id, const bool is_undo)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
  id_buffer_init_for_id_type(id_buffer, id_type);
  id_buffer_init_from_id(id_buffer, id, is_undo);
}

ID *BLO_write_get_id_buffer_temp_id(BLO_Write_IDBuffer *id_buffer)
{
  return id_buffer->temp_id;
}

void BLO_write_destroy_id_buffer(BLO_Write_IDBuffer **id_buffer)
{
  if (static_cast<void *>((*id_buffer)->temp_id) != (*id_buffer)->id_buffer_static) {
    MEM_SAFE_FREE((*id_buffer)->temp_id);
  }
  MEM_SAFE_FREE(*id_buffer);
}

/*
 * API to write chunks of data.
 */

void BLO_write_raw(BlendWriter *writer, size_t size_in_bytes, const void *data_ptr)
{
  writedata(writer->wd, BLO_CODE_DATA, size_in_bytes, data_ptr);
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
    CLOG_ERROR(&LOG, "Can't find SDNA code <%s>", struct_name);
    return;
  }
  BLO_write_struct_array_by_id(writer, struct_id, array_size, data_ptr);
}

void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr)
{
  writestruct_nr(writer->wd, BLO_CODE_DATA, struct_id, 1, data_ptr);
}

void BLO_write_struct_at_address_by_id(BlendWriter *writer,
                                       int struct_id,
                                       const void *address,
                                       const void *data_ptr)
{
  BLO_write_struct_at_address_by_id_with_filecode(
      writer, BLO_CODE_DATA, struct_id, address, data_ptr);
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
  writestruct_nr(writer->wd, BLO_CODE_DATA, struct_id, array_size, data_ptr);
}

void BLO_write_struct_array_at_address_by_id(
    BlendWriter *writer, int struct_id, int array_size, const void *address, const void *data_ptr)
{
  writestruct_at_address_nr(writer->wd, BLO_CODE_DATA, struct_id, array_size, address, data_ptr);
}

void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, ListBase *list)
{
  writelist_nr(writer->wd, BLO_CODE_DATA, struct_id, list);
}

void BLO_write_struct_list_by_name(BlendWriter *writer, const char *struct_name, ListBase *list)
{
  int struct_id = BLO_get_struct_id_by_name(writer, struct_name);
  if (UNLIKELY(struct_id == -1)) {
    CLOG_ERROR(&LOG, "Can't find SDNA code <%s>", struct_name);
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

void BLO_write_int8_array(BlendWriter *writer, uint num, const int8_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int8_t) * size_t(num), data_ptr);
}

void BLO_write_int32_array(BlendWriter *writer, uint num, const int32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int32_t) * size_t(num), data_ptr);
}

void BLO_write_uint32_array(BlendWriter *writer, uint num, const uint32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(uint32_t) * size_t(num), data_ptr);
}

void BLO_write_float_array(BlendWriter *writer, uint num, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float) * size_t(num), data_ptr);
}

void BLO_write_double_array(BlendWriter *writer, uint num, const double *data_ptr)
{
  BLO_write_raw(writer, sizeof(double) * size_t(num), data_ptr);
}

void BLO_write_pointer_array(BlendWriter *writer, uint num, const void *data_ptr)
{
  BLO_write_raw(writer, sizeof(void *) * size_t(num), data_ptr);
}

void BLO_write_float3_array(BlendWriter *writer, uint num, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float[3]) * size_t(num), data_ptr);
}

void BLO_write_string(BlendWriter *writer, const char *data_ptr)
{
  if (data_ptr != nullptr) {
    BLO_write_raw(writer, strlen(data_ptr) + 1, data_ptr);
  }
}

bool BLO_write_is_undo(BlendWriter *writer)
{
  return writer->wd->use_memfile;
}

/** \} */
