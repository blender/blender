/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

/**
 * FILE FORMAT
 * ===========
 *
 * IFF-style structure (but not IFF compatible!)
 *
 * Start of the file:
 *
 * Historic Blend-files (pre-Blender 5.0):
 * `BLENDER_V100`  : Fixed 12 bytes length. See #BLEND_FILE_FORMAT_VERSION_0 for details.
 *
 * Current Blend-files (Blender 5.0 and later):
 * `BLENDER17-01v0500`: Variable bytes length. See #BLEND_FILE_FORMAT_VERSION_1 for details.
 *
 * data-blocks: (also see struct #BHead).
 * <pre>
 * `bh.code`       `char[4]` see `BLO_core_bhead.hh` for a list of known types.
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
 * - write #BLO_CODE_GLOB (#RenderInfo struct. 128x128 blend file preview is optional).
 * - write #BLO_CODE_GLOB (#FileGlobal struct) (some global vars).
 * - write #BLO_CODE_DNA1 (#SDNA struct)
 * - write #BLO_CODE_USER (#UserDef struct) for file paths:
 *   - #BLENDER_STARTUP_FILE (on UNIX `~/.config/blender/X.X/config/startup.blend`).
 *   - #BLENDER_USERPREF_FILE (on UNIX `~/.config/blender/X.X/config/userpref.blend`).
 */

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <xxhash.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h>
#else
#  include <unistd.h> /* FreeBSD, for write() and close(). */
#endif

#include <fmt/format.h>

#include "BLI_utildefines.h"

#include "CLG_log.h"

/* Allow writefile to use deprecated functionality (for forward compatibility code). */
#define DNA_DEPRECATED_ALLOW

#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_key_types.h"
#include "DNA_print.hh"
#include "DNA_sdna_pointers.hh"
#include "DNA_sdna_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_endian_defines.h"
#include "BLI_fileops.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_multi_value_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "MEM_guardedalloc.h" /* MEM_freeN */

#include "BKE_asset.hh"
#include "BKE_blender_version.h"
#include "BKE_bpath.hh"
#include "BKE_global.hh" /* For #Global `G`. */
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_node.hh"
#include "BKE_packedFile.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"
#include "BKE_workspace.hh"

#include "DRW_engine.hh"

#include "BLO_blend_validate.hh"
#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_undofile.hh"
#include "BLO_writefile.hh"

#include "readfile.hh"

#include <zstd.h>

/* Make preferences read-only. */
#define U (*((const UserDef *)&U))

/**
 * Generate an additional file next to every saved .blend file that contains the file content in a
 * more human readable form.
 */
#define GENERATE_DEBUG_BLEND_FILE 0
#define DEBUG_BLEND_FILE_SUFFIX ".debug.txt"

/* ********* my write, buffered writing with minimum size chunks ************ */

/* Use optimal allocation since blocks of this size are kept in memory for undo. */
#define MEM_BUFFER_SIZE MEM_SIZE_OPTIMAL(1 << 17) /* 128kb */
#define MEM_CHUNK_SIZE MEM_SIZE_OPTIMAL(1 << 15)  /* ~32kb */

#define ZSTD_BUFFER_SIZE (1 << 21) /* 2mb */
#define ZSTD_CHUNK_SIZE (1 << 20)  /* 1mb */

#define ZSTD_COMPRESSION_LEVEL 3

static CLG_LogRef LOG = {"blend.writefile"};

/** Use if we want to store how many bytes have been written to the file. */
// #define USE_WRITE_DATA_LEN

/* -------------------------------------------------------------------- */
/** \name Internal Write Wrapper's (Abstracts Compression)
 * \{ */

struct ZstdFrame {
  ZstdFrame *next, *prev;

  uint32_t compressed_size;
  uint32_t uncompressed_size;
};

class WriteWrap {
 public:
  virtual bool open(const char *filepath) = 0;
  virtual bool close() = 0;
  virtual bool write(const void *buf, size_t buf_len) = 0;

  /** Buffer output (we only want when output isn't already buffered). */
  bool use_buf = true;
};

class RawWriteWrap : public WriteWrap {
 public:
  bool open(const char *filepath) override;
  bool close() override;
  bool write(const void *buf, size_t buf_len) override;

 private:
  int file_handle = 0;
};

bool RawWriteWrap::open(const char *filepath)
{
  int file;

  file = BLI_open(filepath, O_BINARY + O_WRONLY + O_CREAT + O_TRUNC, 0666);

  if (file != -1) {
    file_handle = file;
    return true;
  }

  return false;
}
bool RawWriteWrap::close()
{
  return (::close(file_handle) != -1);
}
bool RawWriteWrap::write(const void *buf, size_t buf_len)
{
  return ::write(file_handle, buf, buf_len) == buf_len;
}

class ZstdWriteWrap : public WriteWrap {
  WriteWrap &base_wrap;

  ListBase threadpool = {};
  ListBase tasks = {};
  ThreadMutex mutex = {};
  ThreadCondition condition = {};
  int next_frame = 0;
  int num_frames = 0;

  ListBase frames = {};

  bool write_error = false;

 public:
  ZstdWriteWrap(WriteWrap &base_wrap) : base_wrap(base_wrap) {}

  bool open(const char *filepath) override;
  bool close() override;
  bool write(const void *buf, size_t buf_len) override;

 private:
  struct ZstdWriteBlockTask;
  void write_task(ZstdWriteBlockTask *task);
  void write_u32_le(uint32_t val);
  void write_seekable_frames();
};

struct ZstdWriteWrap::ZstdWriteBlockTask {
  ZstdWriteBlockTask *next, *prev;
  void *data;
  size_t size;
  int frame_number;
  ZstdWriteWrap *ww;

  static void *write_task(void *userdata)
  {
    auto *task = static_cast<ZstdWriteBlockTask *>(userdata);
    task->ww->write_task(task);
    return nullptr;
  }
};

void ZstdWriteWrap::write_task(ZstdWriteBlockTask *task)
{
  size_t out_buf_len = ZSTD_compressBound(task->size);
  void *out_buf = MEM_mallocN(out_buf_len, "Zstd out buffer");
  size_t out_size = ZSTD_compress(
      out_buf, out_buf_len, task->data, task->size, ZSTD_COMPRESSION_LEVEL);

  MEM_freeN(task->data);

  BLI_mutex_lock(&mutex);

  while (next_frame != task->frame_number) {
    BLI_condition_wait(&condition, &mutex);
  }

  if (ZSTD_isError(out_size)) {
    write_error = true;
  }
  else {
    if (base_wrap.write(out_buf, out_size)) {
      ZstdFrame *frameinfo = MEM_mallocN<ZstdFrame>("zstd frameinfo");
      frameinfo->uncompressed_size = task->size;
      frameinfo->compressed_size = out_size;
      BLI_addtail(&frames, frameinfo);
    }
    else {
      write_error = true;
    }
  }

  next_frame++;

  BLI_mutex_unlock(&mutex);
  BLI_condition_notify_all(&condition);

  MEM_freeN(out_buf);
}

bool ZstdWriteWrap::open(const char *filepath)
{
  if (!base_wrap.open(filepath)) {
    return false;
  }

  /* Leave one thread open for the main writing logic, unless we only have one HW thread. */
  int num_threads = max_ii(1, BLI_system_thread_count() - 1);
  BLI_threadpool_init(&threadpool, ZstdWriteBlockTask::write_task, num_threads);
  BLI_mutex_init(&mutex);
  BLI_condition_init(&condition);

  return true;
}

void ZstdWriteWrap::write_u32_le(uint32_t val)
{
  /* NOTE: this is endianness-sensitive.
   * This value must always be written as little-endian. */
  BLI_assert(ENDIAN_ORDER == L_ENDIAN);
  base_wrap.write(&val, sizeof(uint32_t));
}

/* In order to implement efficient seeking when reading the .blend, we append
 * a skippable frame that encodes information about the other frames present
 * in the file.
 * The format here follows the upstream spec for seekable files:
 * https://github.com/facebook/zstd/blob/master/contrib/seekable_format/zstd_seekable_compression_format.md
 * If this information is not present in a file (e.g. if it was compressed
 * with external tools), it can still be opened in Blender, but seeking will
 * not be supported, so more memory might be needed. */
void ZstdWriteWrap::write_seekable_frames()
{
  /* Write seek table header (magic number and frame size). */
  write_u32_le(0x184D2A5E);

  /* The actual frame number might not match num_frames if there was a write error. */
  const uint32_t num_frames = BLI_listbase_count(&frames);
  /* Each frame consists of two u32, so 8 bytes each.
   * After the frames, a footer containing two u32 and one byte (9 bytes total) is written. */
  const uint32_t frame_size = num_frames * 8 + 9;
  write_u32_le(frame_size);

  /* Write seek table entries. */
  LISTBASE_FOREACH (ZstdFrame *, frame, &frames) {
    write_u32_le(frame->compressed_size);
    write_u32_le(frame->uncompressed_size);
  }

  /* Write seek table footer (number of frames, option flags and second magic number). */
  write_u32_le(num_frames);
  const char flags = 0; /* We don't store checksums for each frame. */
  base_wrap.write(&flags, 1);
  write_u32_le(0x8F92EAB1);
}

bool ZstdWriteWrap::close()
{
  BLI_threadpool_end(&threadpool);
  BLI_freelistN(&tasks);

  BLI_mutex_end(&mutex);
  BLI_condition_end(&condition);

  write_seekable_frames();
  BLI_freelistN(&frames);

  return base_wrap.close() && !write_error;
}

bool ZstdWriteWrap::write(const void *buf, const size_t buf_len)
{
  if (write_error) {
    return false;
  }

  ZstdWriteBlockTask *task = MEM_mallocN<ZstdWriteBlockTask>(__func__);
  task->data = MEM_mallocN(buf_len, __func__);
  memcpy(task->data, buf, buf_len);
  task->size = buf_len;
  task->frame_number = num_frames++;
  task->ww = this;

  BLI_mutex_lock(&mutex);
  BLI_addtail(&tasks, task);

  /* If there's a free worker thread, just push the block into that thread.
   * Otherwise, we wait for the earliest thread to finish.
   * We look up the earliest thread while holding the mutex, but release it
   * before joining the thread to prevent a deadlock. */
  ZstdWriteBlockTask *first_task = static_cast<ZstdWriteBlockTask *>(tasks.first);
  BLI_mutex_unlock(&mutex);
  if (!BLI_available_threads(&threadpool)) {
    BLI_threadpool_remove(&threadpool, first_task);

    /* If the task list was empty before we pushed our task, there should
     * always be a free thread. */
    BLI_assert(first_task != task);
    BLI_remlink(&tasks, first_task);
    MEM_freeN(first_task);
  }
  BLI_threadpool_insert(&threadpool, task);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write Data Type & Functions
 * \{ */

struct WriteData {
  const SDNA *sdna;
  std::ostream *debug_dst = nullptr;

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

  /** Whether writefile code is currently writing an ID. */
  bool is_writing_id;

  /** Some validation and error handling data. */
  struct {
    /**
     * Set on unlikely case of an error (ignores further file writing). Only used for very
     * low-level errors (like if the actual write on file fails).
     */
    bool critical_error;
    /**
     * A set of all 'old' addresses used as UID of written blocks for the current ID. Allows
     * detecting invalid re-uses of the same address multiple times.
     */
    blender::Set<const void *> per_id_addresses_set;
  } validation_data;

  struct {
    /**
     * Knows which DNA members are pointers. Those members are overridden when serializing the
     * .blend file to get more stable pointer identifiers.
     */
    std::unique_ptr<blender::dna::pointers::PointersInDNA> sdna_pointers;
    /**
     * Maps each runtime-pointer to a unique identifier that's written in the .blend file.
     *
     * Currently, no pointers are ever removed from this map during writing of a single file.
     * Correctness wise, this is fine. However, when some data-blocks write temporary addresses,
     * those may be reused across IDs while actually pointing to different data. This can break
     * address id stability in some situations. In the future this could be improved by clearing
     * such temporary pointers before writing the next data-block.
     */
    blender::Map<const void *, uint64_t> pointer_map;
    /**
     * Contains all the #pointer_map.values(). This is used to make sure that the same id is never
     * reused for a different pointer. While this is technically allowed in .blend files (when the
     * pointers are local data of different objects), we currently don't always know what type a
     * pointer points to when writing it. So we can't determine if a pointer is local or not.
     */
    blender::Set<uint64_t> used_ids;
    /**
     * The next stable address id is derived from this. This is modified in
     * two cases:
     * - A new stable address is needed, in which case this is just incremented.
     * - A new "section" of the .blend file starts. In this case, this should be reinitialized with
     *   some hash of an identifier of the next section. This makes sure that if the number of
     *   pointers in the previous section is modified, the pointers in the new section are not
     *   affected. A "section" can be anything, but currently a section simply starts when a new
     *   data-block starts. In the future, an API could be added that allows sections to start
     *   within a data-block which could isolate stable pointer ids even more.
     *
     * When creating the new address id, keep in mind that this may be 0 and it may collide with
     * previous hints.
     */
    uint64_t next_id_hint = 0;
  } stable_address_ids;

  /**
   * Keeps track of which shared data has been written for the current ID. This is necessary to
   * avoid writing the same data more than once.
   */
  blender::Set<const void *> per_id_written_shared_addresses;

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
  WriteData *wd = MEM_new<WriteData>(__func__);

  wd->sdna = DNA_sdna_current_get();
  wd->stable_address_ids.sdna_pointers = std::make_unique<blender::dna::pointers::PointersInDNA>(
      *wd->sdna);

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
    wd->buffer.buf = MEM_malloc_arrayN<uchar>(wd->buffer.max_size, "wd->buffer.buf");
  }

  return wd;
}

static void writedata_do_write(WriteData *wd, const void *mem, const size_t memlen)
{
  if ((wd == nullptr) || wd->validation_data.critical_error || (mem == nullptr) || memlen < 1) {
    return;
  }

  if (memlen > INT_MAX) {
    BLI_assert_msg(0, "Cannot write chunks bigger than INT_MAX.");
    return;
  }

  if (UNLIKELY(wd->validation_data.critical_error)) {
    return;
  }

  /* Memory based save. */
  if (wd->use_memfile) {
    BLO_memfile_chunk_add(&wd->mem, static_cast<const char *>(mem), memlen);
  }
  else {
    if (!wd->ww->write(mem, memlen)) {
      wd->validation_data.critical_error = true;
    }
  }
}

static void writedata_free(WriteData *wd)
{
  if (wd->buffer.buf) {
    MEM_freeN(wd->buffer.buf);
  }
  MEM_delete(wd);
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
  if (UNLIKELY(wd->validation_data.critical_error)) {
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
    /* If we have a single big chunk, write existing data in
     * buffer and write out big chunk in smaller pieces. */
    if (len > wd->buffer.chunk_size) {
      if (wd->buffer.used_len != 0) {
        writedata_do_write(wd, wd->buffer.buf, wd->buffer.used_len);
        wd->buffer.used_len = 0;
      }

      do {
        const size_t writelen = std::min(len, wd->buffer.chunk_size);
        writedata_do_write(wd, adr, writelen);
        adr = (const char *)adr + writelen;
        len -= writelen;
      } while (len > 0);

      return;
    }

    /* If data would overflow buffer, write out the buffer. */
    if (len + wd->buffer.used_len > wd->buffer.max_size - 1) {
      writedata_do_write(wd, wd->buffer.buf, wd->buffer.used_len);
      wd->buffer.used_len = 0;
    }

    /* Append data at end of buffer. */
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

  const bool err = wd->validation_data.critical_error;
  writedata_free(wd);

  return err;
}

static uint64_t get_stable_pointer_hint_for_id(const ID &id)
{
  /* Make the stable pointer dependent on the data-block name. This is somewhat arbitrary but the
   * name is at least something that doesn't really change automatically unexpectedly. */
  const uint64_t name_hash = XXH3_64bits(id.name, strlen(id.name));
  if (id.lib) {
    const uint64_t lib_hash = XXH3_64bits(id.lib->id.name, strlen(id.lib->id.name));
    return name_hash ^ lib_hash;
  }
  return name_hash;
}

/**
 * Start writing of data related to a single ID.
 *
 * Only does something when storing an undo step.
 */
static void mywrite_id_begin(WriteData *wd, ID *id)
{
  BLI_assert(wd->is_writing_id == false);
  wd->is_writing_id = true;

  BLI_assert(wd->validation_data.per_id_addresses_set.is_empty());

  BLI_assert_msg(ID_IS_PACKED(id) || id->deep_hash.is_null(),
                 "The only IDs with non-null deep-hash data should be packed linked ones");
  BLI_assert_msg((id->flag & ID_FLAG_EMBEDDED_DATA) == 0 || id->deep_hash.is_null(),
                 "Embedded IDs should always have a null deep-hash data");

  wd->stable_address_ids.next_id_hint = get_stable_pointer_hint_for_id(*id);

  if (wd->use_memfile) {
    wd->mem.current_id_session_uid = id->session_uid;

    /* If current next memchunk does not match the ID we are about to write, or is not the _first_
     * one for said ID, try to find the correct memchunk in the mapping using ID's session_uid. */
    const MemFileChunk *curr_memchunk = wd->mem.reference_current_chunk;
    const MemFileChunk *prev_memchunk = curr_memchunk != nullptr ?
                                            static_cast<MemFileChunk *>(curr_memchunk->prev) :
                                            nullptr;
    if (curr_memchunk == nullptr || curr_memchunk->id_session_uid != id->session_uid ||
        (prev_memchunk != nullptr &&
         (prev_memchunk->id_session_uid == curr_memchunk->id_session_uid)))
    {
      if (MemFileChunk *ref = wd->mem.id_session_uid_mapping.lookup_default(id->session_uid,
                                                                            nullptr))
      {
        wd->mem.reference_current_chunk = ref;
      }
      /* Else, no existing memchunk found, i.e. this is supposed to be a new ID. */
    }
    /* Otherwise, we try with the current memchunk in any case, whether it is matching current
     * ID's session_uid or not. */
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
    wd->mem.current_id_session_uid = MAIN_ID_SESSION_UID_UNSET;
  }

  wd->validation_data.per_id_addresses_set.clear();
  wd->per_id_written_shared_addresses.clear();

  BLI_assert(wd->is_writing_id == true);
  wd->is_writing_id = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic DNA File Writing
 * \{ */

/**
 * Return \a false if the given 'old' address is not valid in current context. The block should
 * not be written in that case.
 *
 * \note Currently only checks that #BLO_CODE_DATA blocks written as part of an ID data never match
 * an already written one for the same ID.
 */
static bool write_at_address_validate(WriteData *wd, const int filecode, const void *address)
{
  /* Skip in undo case. */
  if (wd->use_memfile) {
    return true;
  }

  if (wd->is_writing_id && filecode == BLO_CODE_DATA) {
    if (!wd->validation_data.per_id_addresses_set.add(address)) {
      CLOG_ERROR(&LOG,
                 "Same identifier (old address) used several times for a same ID, skipping this "
                 "block to avoid critical corruption of the Blender file.");
      return false;
    }
  }
  return true;
}

static void write_bhead(WriteData *wd, const BHead &bhead)
{
  if constexpr (sizeof(void *) == 4) {
    /* Always write #BHead4 in 32 bit builds. */
    BHead4 bh;
    bh.code = bhead.code;
    bh.old = uint32_t(uintptr_t(bhead.old));
    bh.nr = bhead.nr;
    bh.SDNAnr = bhead.SDNAnr;
    bh.len = bhead.len;
    mywrite(wd, &bh, sizeof(bh));
    return;
  }
  /* Write new #LargeBHead8 headers if enabled. Older Blender versions can't read those. */
  if (!USER_DEVELOPER_TOOL_TEST(&U, write_legacy_blend_file_format)) {
    if (SYSTEM_SUPPORTS_WRITING_FILE_VERSION_1) {
      static_assert(sizeof(BHead) == sizeof(LargeBHead8));
      mywrite(wd, &bhead, sizeof(bhead));
      return;
    }
  }
  /* Write older #SmallBHead8 headers so that Blender versions that don't support #LargeBHead8 can
   * read the file. */
  SmallBHead8 bh;
  bh.code = bhead.code;
  bh.old = uint64_t(bhead.old);
  bh.nr = bhead.nr;
  bh.SDNAnr = bhead.SDNAnr;
  bh.len = bhead.len;
  /* Check that the written buffer size is compatible with the limits of #SmallBHead8. */
  if (bhead.len > std::numeric_limits<decltype(bh.len)>::max()) {
    CLOG_ERROR(&LOG, "Written .blend file is corrupt, because a memory block is too large.");
    return;
  }
  mywrite(wd, &bh, sizeof(bh));
}

/** This bit is used to mark address ids that use implicit sharing during undo. */
constexpr uint64_t implicit_sharing_address_id_flag = uint64_t(1) << 63;

static uint64_t stable_id_from_hint(const uint64_t hint)
{
  /* Add a stride. This is not strictly necessary but may help with debugging later on because it's
   * easier to identify bad ids. */
  uint64_t stable_id = hint << 4;
  if (stable_id == 0) {
    /* Null values are reserved for nullptr. */
    stable_id = (1 << 4);
  }
  /* Remove the first bit as it reserved for pointers for implicit sharing.*/
  stable_id &= ~implicit_sharing_address_id_flag;
  return stable_id;
}

static uint64_t get_next_stable_address_id(WriteData &wd, uint64_t &hint)
{
  uint64_t stable_id = stable_id_from_hint(hint);
  while (!wd.stable_address_ids.used_ids.add(stable_id)) {
    /* Generate a new hint because there is a collision. Collisions are generally expected to be
     * very rare. It can happen when #get_stable_pointer_hint_for_id produces values that are very
     * close for different IDs. */
    hint = XXH3_64bits(&hint, sizeof(uint64_t));
    stable_id = stable_id_from_hint(hint);
  }
  hint++;
  return stable_id;
}

/**
 * When writing an undo step, implicitly shared pointers do not use stable-pointers because that
 * would lead to incorrect detection if a data-block has been changed between undo steps. That's
 * because different shared data could be mapped to the same stable pointer, leading to
 * #is_memchunk_identical to being true even if the referenced data is actually different.
 *
 * Another way to look at it is that implicit-sharing is a system for stable pointers (at runtime)
 * itself. So it does not need an additional layer of stable pointers on top.
 */
static uint64_t get_address_id_for_implicit_sharing_data(const void *data)
{
  BLI_assert(data != nullptr);
  uint64_t address_id = uint64_t(data);
  /* Adding this bit so that it never overlap with an id generated by #stable_id_from_hint.
   * Assuming that the given pointer is an actual pointer, it will stay unique when the
   * #implicit_sharing_address_id_flag bit is set. That's because the upper bits of the pointer
   * are effectively unused nowadays. */
  address_id |= implicit_sharing_address_id_flag;
  return address_id;
}

static uint64_t get_address_id_int(WriteData &wd, const void *address)
{
  if (address == nullptr) {
    return 0;
  }
  /* Either reuse an existing identifier or create a new one. */
  return wd.stable_address_ids.pointer_map.lookup_or_add_cb(address, [&]() {
    return get_next_stable_address_id(wd, wd.stable_address_ids.next_id_hint);
  });
}

static const void *get_address_id(WriteData &wd, const void *address)
{
  return reinterpret_cast<const void *>(get_address_id_int(wd, address));
}

static void writestruct_at_address_nr(WriteData *wd,
                                      const int filecode,
                                      const int struct_nr,
                                      const int64_t nr,
                                      const void *adr,
                                      const void *data)
{
  BLI_assert(struct_nr > 0 && struct_nr <= blender::dna::sdna_struct_id_get_max());

  if (adr == nullptr || data == nullptr || nr == 0) {
    return;
  }

  if (!write_at_address_validate(wd, filecode, adr)) {
    return;
  }

  const int64_t len_in_bytes = nr * DNA_struct_size(wd->sdna, struct_nr);
  if (!SYSTEM_SUPPORTS_WRITING_FILE_VERSION_1 ||
      USER_DEVELOPER_TOOL_TEST(&U, write_legacy_blend_file_format))
  {
    if (len_in_bytes > INT32_MAX) {
      CLOG_ERROR(&LOG, "Cannot write chunks bigger than INT_MAX.");
      return;
    }
  }

  /* Get the address identifier that will be written to the file.*/
  const void *address_id = get_address_id(*wd, adr);

  const blender::dna::pointers::StructInfo &struct_info =
      wd->stable_address_ids.sdna_pointers->get_for_struct(struct_nr);
  const bool can_write_raw_runtime_data = struct_info.pointers.is_empty();

  blender::DynamicStackBuffer<16 * 1024> buffer_owner(len_in_bytes, 64);
  const void *data_to_write;
  if (can_write_raw_runtime_data) {
    /* The passed in data contains no pointers, so it can be written without an additional copy. */
    data_to_write = data;
  }
  else {
    void *buffer = buffer_owner.buffer();
    data_to_write = buffer;
    memcpy(buffer, data, len_in_bytes);

    /* Overwrite pointers with their corresponding address identifiers. */
    for (const int i : blender::IndexRange(nr)) {
      for (const blender::dna::pointers::PointerInfo &pointer_info : struct_info.pointers) {
        const int offset = i * struct_info.size_in_bytes + pointer_info.offset;
        const void **p_ptr = reinterpret_cast<const void **>(POINTER_OFFSET(buffer, offset));
        const void *address_id = get_address_id(*wd, *p_ptr);
        *p_ptr = address_id;
      }
    }
  }

  BHead bh;
  bh.code = filecode;
  bh.old = address_id;
  bh.nr = nr;
  bh.SDNAnr = struct_nr;
  bh.len = len_in_bytes;

  if (bh.len == 0) {
    return;
  }

  if (wd->debug_dst) {
    blender::dna::print_structs_at_address(
        *wd->sdna, struct_nr, data_to_write, address_id, nr, *wd->debug_dst);
  }

  write_bhead(wd, bh);
  mywrite(wd, data_to_write, size_t(bh.len));
}

static void writestruct_nr(
    WriteData *wd, const int filecode, const int struct_nr, const int64_t nr, const void *adr)
{
  writestruct_at_address_nr(wd, filecode, struct_nr, nr, adr, adr);
}

static void write_raw_data_in_debug_file(WriteData *wd,
                                         const size_t len,
                                         const void *address_id,
                                         const void *data)
{
  fmt::memory_buffer buf;
  fmt::appender dst{buf};

  fmt::format_to(dst, "<Raw Data> at {} ({} bytes)\n", address_id, len);

  constexpr int bytes_per_row = 8;
  const int len_digits = std::to_string(std::max<size_t>(0, len - 1)).size();

  for (size_t i = 0; i < len; i++) {
    if (i % bytes_per_row == 0) {
      fmt::format_to(dst, "  {:{}}: ", i, len_digits);
    }
    fmt::format_to(dst, "{:02x} ", reinterpret_cast<const uint8_t *>(data)[i]);
    if (i % bytes_per_row == bytes_per_row - 1) {
      fmt::format_to(dst, "\n");
    }
  }
  if (len % bytes_per_row != 0) {
    fmt::format_to(dst, "\n");
  }

  *wd->debug_dst << fmt::to_string(buf);
}

/**
 * \warning Do not use for structs.
 */
static void writedata(
    WriteData *wd, const int filecode, const void *data, const size_t len, const void *adr)
{
  if (data == nullptr || len == 0) {
    return;
  }

  if (!write_at_address_validate(wd, filecode, adr)) {
    return;
  }

  if ((!SYSTEM_SUPPORTS_WRITING_FILE_VERSION_1 ||
       USER_DEVELOPER_TOOL_TEST(&U, write_legacy_blend_file_format)) &&
      len > INT_MAX)
  {
    BLI_assert_msg(0, "Cannot write chunks bigger than INT_MAX.");
    return;
  }

  const void *address_id = get_address_id(*wd, adr);

  BHead bh;
  bh.code = filecode;
  bh.old = address_id;
  bh.nr = 1;
  BLI_STATIC_ASSERT(SDNA_RAW_DATA_STRUCT_INDEX == 0, "'raw data' SDNA struct index should be 0")
  bh.SDNAnr = SDNA_RAW_DATA_STRUCT_INDEX;
  bh.len = int64_t(len);

  if (wd->debug_dst) {
    write_raw_data_in_debug_file(wd, len, address_id, adr);
  }

  write_bhead(wd, bh);
  mywrite(wd, data, len);
}

static void writedata(WriteData *wd, const int filecode, const size_t len, const void *adr)
{
  writedata(wd, filecode, adr, len, adr);
}

/**
 * Use this to force writing of lists in same order as reading (using link_list).
 */
static void writelist_nr(WriteData *wd,
                         const int filecode,
                         const int struct_nr,
                         const ListBase *lb)
{
  const Link *link = static_cast<Link *>(lb->first);

  while (link) {
    writestruct_nr(wd, filecode, struct_nr, 1, link);
    link = link->next;
  }
}

#if 0
static void writelist_id(WriteData *wd, const int filecode, const char *structname, const ListBase *lb)
{
  const Link *link = lb->first;
  if (link) {

    const int struct_nr = DNA_struct_find_with_alias(wd->sdna, structname);
    if (struct_nr == -1) {
      printf("error: cannot find SDNA code <%s>\n", structname);
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
  writestruct_at_address_nr( \
      wd, filecode, blender::dna::sdna_struct_id_get<struct_id>(), nr, adr, data)

#define writestruct(wd, filecode, struct_id, nr, adr) \
  writestruct_nr(wd, filecode, blender::dna::sdna_struct_id_get<struct_id>(), nr, adr)

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
                                  const bool use_active_win,
                                  bScreen **r_screen,
                                  Scene **r_scene,
                                  ViewLayer **r_view_layer)
{
  wmWindowManager *wm;
  wmWindow *window = nullptr;

  /* Find a global current screen in the first open window, to have
   * a reasonable default for reading in older versions. */
  wm = static_cast<wmWindowManager *>(mainvar->wm.first);

  if (wm) {
    if (use_active_win) {
      /* Write the active window into the file, needed for multi-window undo #43424. */
      for (window = static_cast<wmWindow *>(wm->windows.first); window; window = window->next) {
        if (window->active) {
          break;
        }
      }

      /* Fallback. */
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

  /* XXX: in future, handle multiple windows with multiple screens? */
  current_screen_compat(mainvar, false, &curscreen, &curscene, &view_layer);

  LISTBASE_FOREACH (Scene *, sce, &mainvar->scenes) {
    if (!ID_IS_LINKED(sce) && (sce == curscene || (sce->r.scemode & R_BG_RENDER))) {
      RenderInfo data;
      data.sfra = sce->r.sfra;
      data.efra = sce->r.efra;
      memset(data.scene_name, 0, sizeof(data.scene_name));

      STRNCPY(data.scene_name, sce->id.name + 2);

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

  LISTBASE_FOREACH (const bUserExtensionRepo *, repo_ref, &userdef->extension_repos) {
    BLO_write_struct(writer, bUserExtensionRepo, repo_ref);
    BKE_preferences_extension_repo_write_data(writer, repo_ref);
  }
  LISTBASE_FOREACH (
      const bUserAssetShelfSettings *, shelf_settings, &userdef->asset_shelves_settings)
  {
    BLO_write_struct(writer, bUserAssetShelfSettings, shelf_settings);
    BKE_asset_catalog_path_list_blend_write(writer, shelf_settings->enabled_catalog_paths);
  }

  LISTBASE_FOREACH (const uiStyle *, style, &userdef->uistyles) {
    BLO_write_struct(writer, uiStyle, style);
  }
}

/**
 * Writes ID and all its direct data to the file.
 */
static void write_id(WriteData *wd, ID *id)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
  mywrite_id_begin(wd, id);
  if (id_type->blend_write != nullptr) {
    BlendWriter writer = {wd};
    BLO_Write_IDBuffer id_buffer{*id, wd->use_memfile, false};
    id_type->blend_write(&writer, id_buffer.get(), id);
  }
  mywrite_id_end(wd, id);
}

static void write_id_placeholder(WriteData *wd, ID *id)
{
  mywrite_id_begin(wd, id);

  /* Only copy required data for the placeholder ID. */
  BLO_Write_IDBuffer id_buffer{*id, wd->use_memfile, true};

  writestruct_at_address(wd, ID_LINK_PLACEHOLDER, ID, 1, id, &id_buffer);

  mywrite_id_end(wd, id);
}

/** Keep it last of `write_*_data` functions. */
static void write_libraries(WriteData *wd, Main *bmain)
{
  /* Gather IDs coming from each library. */
  blender::MultiValueMap<Library *, ID *> linked_ids_by_library;
  {
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      if (!ID_IS_LINKED(id)) {
        continue;
      }
      BLI_assert(id->lib);
      linked_ids_by_library.add(id->lib, id);
    }
    FOREACH_MAIN_ID_END;
  }

  LISTBASE_FOREACH (Library *, library_ptr, &bmain->libraries) {
    Library &library = *library_ptr;
    const blender::Span<ID *> ids = linked_ids_by_library.lookup(&library);

    /* Gather IDs that are somehow directly referenced by data in the current blend file. */
    blender::Vector<ID *> ids_used_from_library;
    for (ID *id : ids) {
      if (id->us == 0) {
        continue;
      }
      if (ID_IS_PACKED(id)) {
        BLI_assert(library.flag & LIBRARY_FLAG_IS_ARCHIVE);
        ids_used_from_library.append(id);
        continue;
      }
      if (id->tag & ID_TAG_EXTERN) {
        ids_used_from_library.append(id);
        continue;
      }
      if ((id->tag & ID_TAG_INDIRECT) && (id->flag & ID_FLAG_INDIRECT_WEAK_LINK)) {
        ids_used_from_library.append(id);
        continue;
      }
    }

    bool should_write_library = false;
    if (library.packedfile) {
      should_write_library = true;
    }
    else if (!library.runtime->archived_libraries.is_empty()) {
      /* Reference 'real' blendfile library of archived 'copies' of it containing packed linked
       * IDs should always be written. */
      /* FIXME: A bit weak, as it could be that all archive libs are now empty (if all related
       * packed linked IDs have been deleted e.g.)...
       * Could be fixed by either adding more checks here, or ensuring empty archive libs are
       * deleted when no ID uses them anymore? */
      should_write_library = true;
    }
    else if (wd->use_memfile) {
      /* When writing undo step we always write all existing libraries. That makes reading undo
       * step much easier when dealing with purely indirectly used libraries. */
      should_write_library = true;
    }
    else {
      should_write_library = !ids_used_from_library.is_empty();
    }

    if (!should_write_library) {
      /* Nothing from the library is used, so it does not have to be written. */
      continue;
    }

    write_id(wd, &library.id);

    /* Write placeholders for linked data-blocks that are used, and real IDs for the packed linked
     * ones. */
    for (ID *id : ids_used_from_library) {
      if (ID_IS_PACKED(id)) {
        write_id(wd, id);
      }
      else {
        if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
          CLOG_ERROR(&LOG,
                     "Data-block '%s' from lib '%s' is not linkable, but is flagged as "
                     "directly linked",
                     id->name,
                     library.runtime->filepath_abs);
        }
        write_id_placeholder(wd, id);
      }
    }
  }

  mywrite_flush(wd);
}

#ifdef WITH_BUILDINFO
extern "C" ulong build_commit_timestamp;
extern "C" char build_hash[];
#endif

/**
 * Context is usually defined by WM, two cases where no WM is available:
 * - for forward compatibility, `curscreen` has to be saved
 * - for undo-file, `curscene` needs to be saved.
 */
static void write_global(WriteData *wd, const int fileflags, Main *mainvar)
{
  const bool is_undo = wd->use_memfile;
  FileGlobal fg;
  bScreen *screen;
  Scene *scene;
  ViewLayer *view_layer;
  char subvstr[8];

  /* Prevent memory checkers from complaining. */
  memset(fg._pad, 0, sizeof(fg._pad));
  memset(fg.filepath, 0, sizeof(fg.filepath));
  memset(fg.build_hash, 0, sizeof(fg.build_hash));
  fg._pad1 = nullptr;

  current_screen_compat(mainvar, is_undo, &screen, &scene, &view_layer);

  /* XXX: still remap `G`. */
  fg.curscreen = screen;
  fg.curscene = scene;
  fg.cur_view_layer = view_layer;

  STRNCPY(fg.colorspace_scene_linear_name, mainvar->colorspace.scene_linear_name);
  copy_m3_m3(fg.colorspace_scene_linear_to_xyz, mainvar->colorspace.scene_linear_to_xyz.ptr());

  /* Prevent to save this, is not good convention, and feature with concerns. */
  fg.fileflags = (fileflags & ~G_FILE_FLAG_ALL_RUNTIME);

  fg.globalf = G.f;
  /* Write information needed for recovery. */
  if (fileflags & G_FILE_RECOVER_WRITE) {
    STRNCPY(fg.filepath, mainvar->filepath);
    /* Compression is often turned of when writing recovery files. However, when opening the file,
     * it should be enabled again. */
    fg.fileflags = G.fileflags & G_FILE_COMPRESS;
  }
  SNPRINTF(subvstr, "%4d", BLENDER_FILE_SUBVERSION);
  memcpy(fg.subvstr, subvstr, 4);

  fg.subversion = BLENDER_FILE_SUBVERSION;
  fg.minversion = BLENDER_FILE_MIN_VERSION;
  fg.minsubversion = BLENDER_FILE_MIN_SUBVERSION;
#ifdef WITH_BUILDINFO
  /* TODO(sergey): Add branch name to file as well? */
  fg.build_commit_timestamp = build_commit_timestamp;
  STRNCPY(fg.build_hash, build_hash);
#else
  fg.build_commit_timestamp = 0;
  STRNCPY(fg.build_hash, "unknown");
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

BLO_Write_IDBuffer::BLO_Write_IDBuffer(ID &id, const bool is_undo, const bool is_placeholder)
    : buffer_(is_placeholder ? sizeof(ID) : BKE_idtype_get_info_from_id(&id)->struct_size,
              alignof(ID))
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(&id);
  ID *temp_id = static_cast<ID *>(buffer_.buffer());

  if (is_undo) {
    /* Record the changes that happened up to this undo push in
     * recalc_up_to_undo_push, and clear `recalc_after_undo_push` again
     * to start accumulating for the next undo push. */
    id.recalc_up_to_undo_push = id.recalc_after_undo_push;
    id.recalc_after_undo_push = 0;
  }

  /* Copy ID data itself into buffer, to be able to freely modify it. */

  if (is_placeholder) {
    /* For placeholders (references to linked data), zero-initialize, and only explicitly copy the
     * very small subset of required data. */
    *temp_id = ID{};
    temp_id->lib = id.lib;
    STRNCPY(temp_id->name, id.name);
    temp_id->flag = id.flag;
    temp_id->session_uid = id.session_uid;
    if (is_undo) {
      temp_id->recalc_up_to_undo_push = id.recalc_up_to_undo_push;
      temp_id->tag = id.tag & ID_TAG_KEEP_ON_UNDO;
    }
    return;
  }

  /* Regular 'full' ID writing, copy everything, then clear some runtime data irrelevant in the
   * blendfile. */
  memcpy(temp_id, &id, id_type->struct_size);

  /* Clear runtime data to reduce false detection of changed data in undo/redo context. */
  if (is_undo) {
    temp_id->tag &= ID_TAG_KEEP_ON_UNDO;
  }
  else {
    temp_id->tag = 0;
  }
  temp_id->us = 0;
  temp_id->icon_id = 0;
  temp_id->runtime = nullptr;
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
   * #direct_link_id_common in `readfile.cc` anyway. */
  temp_id->py_instance = nullptr;
}

BLO_Write_IDBuffer::BLO_Write_IDBuffer(ID &id, BlendWriter *writer)
    : BLO_Write_IDBuffer(id, BLO_write_is_undo(writer), false)
{
}

/* Helper callback for checking linked IDs used by given ID (assumed local), to ensure directly
 * linked data is tagged accordingly. */
static int write_id_direct_linked_data_process_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *self_id = cb_data->self_id;
  ID *id = *cb_data->id_pointer;
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;

  if (id == nullptr || !ID_IS_LINKED(id)) {
    return IDWALK_RET_NOP;
  }
  BLI_assert(!ID_IS_LINKED(self_id));
  BLI_assert((cb_flag & IDWALK_CB_INDIRECT_USAGE) == 0);

  if (self_id->tag & ID_TAG_RUNTIME) {
    return IDWALK_RET_NOP;
  }

  if (cb_flag & IDWALK_CB_WRITEFILE_IGNORE) {
    /* Do not consider these ID usages (typically, from the Outliner e.g.) as making the ID
     * directly linked. */
    return IDWALK_RET_NOP;
  }

  if (!BKE_idtype_idcode_is_linkable(GS(id->name))) {
    /* Usages of unlinkable IDs (aka ShapeKeys and some UI IDs) should never cause them to be
     * considered as directly linked. This can often happen e.g. from UI data (the Outliner will
     * have links to most IDs).
     */
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

static std::string get_blend_file_header()
{
  if (SYSTEM_SUPPORTS_WRITING_FILE_VERSION_1 &&
      !USER_DEVELOPER_TOOL_TEST(&U, write_legacy_blend_file_format))
  {
    const int header_size_in_bytes = SIZEOFBLENDERHEADER_VERSION_1;

    /* New blend file header format. */
    std::stringstream ss;
    ss << "BLENDER";
    ss << header_size_in_bytes;
    ss << '-';
    ss << std::setfill('0') << std::setw(2) << BLEND_FILE_FORMAT_VERSION_1;
    ss << 'v';
    ss << std::setfill('0') << std::setw(4) << BLENDER_FILE_VERSION;

    const std::string header = ss.str();
    BLI_assert(header.size() == header_size_in_bytes);
    return header;
  }

  const char pointer_size_char = sizeof(void *) == 8 ? '-' : '_';
  const char endian_char = 'v';

  /* Legacy blend file header format. */
  std::stringstream ss;
  ss << "BLENDER";
  ss << pointer_size_char;
  ss << endian_char;
  ss << BLENDER_FILE_VERSION;
  const std::string header = ss.str();
  BLI_assert(header.size() == SIZEOFBLENDERHEADER_VERSION_0);
  return header;
}

static void write_blend_file_header(WriteData *wd)
{
  const std::string header = get_blend_file_header();
  mywrite(wd, header.data(), header.size());
}

/**
 * Gathers all local IDs that should be written to the file.
 */
static blender::Vector<ID *> gather_local_ids_to_write(Main *bmain, const bool is_undo)
{
  blender::Vector<ID *> local_ids_to_write;
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (GS(id->name) == ID_LI) {
      /* Libraries are handled separately below. */
      continue;
    }
    if (ID_IS_LINKED(id)) {
      /* Linked data-blocks are handled separately below. */
      continue;
    }
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    UNUSED_VARS_NDEBUG(id_type);
    /* We should never attempt to write non-regular IDs
     * (i.e. all kind of temp/runtime ones). */
    BLI_assert((id->tag & (ID_TAG_NO_MAIN | ID_TAG_NO_USER_REFCOUNT | ID_TAG_NOT_ALLOCATED)) == 0);
    /* We only write unused IDs in undo case. */
    if (!is_undo) {
      /* NOTE: All 'never unused' local IDs (Scenes, WindowManagers, ...) should always be
       * written to disk, so their user-count should never be zero currently. Note that
       * libraries have already been skipped above, as they need a specific handling. */
      if (id->us == 0) {
        /* FIXME: #124857: Some old files seem to cause incorrect handling of their temp
         * screens.
         *
         * See e.g. file attached to #124777 (from 2.79.1).
         *
         * For now ignore, issue is not obvious to track down (`temp` bScreen ID from read data
         * _does_ have the proper `temp` tag), and seems anecdotal at worst. */
        BLI_assert((id_type->flags & IDTYPE_FLAGS_NEVER_UNUSED) == 0);
        continue;
      }

      /* XXX Special handling for ShapeKeys, as having unused shape-keys is not a good thing
       * (and reported as error by e.g. `BLO_main_validate_shapekeys`), skip writing shape-keys
       * when their 'owner' is not written.
       *
       * NOTE: Since ShapeKeys are conceptually embedded IDs (like root node trees e.g.), this
       * behavior actually makes sense anyway. This remains more of a temp hack until topic of
       * how to handle unused data on save is properly tackled. */
      if (GS(id->name) == ID_KE) {
        Key *shape_key = reinterpret_cast<Key *>(id);
        /* NOTE: Here we are accessing the real owner ID data, not it's 'proxy' shallow copy
         * generated for its file-writing. This is not expected to be an issue, but is worth
         * noting. */
        if (shape_key->from == nullptr || shape_key->from->us == 0) {
          continue;
        }
      }
    }

    if ((id->tag & ID_TAG_RUNTIME) != 0 && !is_undo) {
      /* Runtime IDs are never written to .blend files, and they should not influence
       * (in)direct status of linked IDs they may use. */
      continue;
    }

    local_ids_to_write.append(id);
  }
  FOREACH_MAIN_ID_END;
  return local_ids_to_write;
}

/**
 * Precomputes a stable pointer for each data-block before they are used. This ensures that their
 * written pointer does not depend on the order in which data-blocks are written.
 */
static void prepare_stable_data_block_ids(WriteData &wd, Main &bmain)
{
  ID *id;
  FOREACH_MAIN_ID_BEGIN (&bmain, id) {
    /* Ensure no other stable pointer has been created before. */
    BLI_assert(!wd.stable_address_ids.pointer_map.contains(id));

    /* Derive the stable pointer from the id/library name which is independent of the write-order
     * of data-blocks. */
    uint64_t hint = get_stable_pointer_hint_for_id(*id);
    const uint64_t address_id = get_next_stable_address_id(wd, hint);

    /* Store the computed stable pointer so that it is used whenever the data-block is written or
     * referenced. */
    wd.stable_address_ids.pointer_map.add(id, address_id);
  }
  FOREACH_MAIN_ID_END;
}

/**
 * When #MemFile arguments are non-null, this is a file-safe to memory.
 *
 * \param compare: Previous memory file (can be nullptr).
 * \param current: The current memory file (can be nullptr).
 */
static bool write_file_handle(Main *mainvar,
                              WriteWrap *ww,
                              MemFile *compare,
                              MemFile *current,
                              const int write_flags,
                              const bool use_userdef,
                              const BlendThumbnail *thumb,
                              std::ostream *debug_dst)
{
  WriteData *wd;

  wd = mywrite_begin(ww, compare, current);
  wd->debug_dst = debug_dst;
  BlendWriter writer = {wd};

  prepare_stable_data_block_ids(*wd, *mainvar);

  /* Clear 'directly linked' flag for all linked data, these are not necessarily valid/up-to-date
   * info, they will be re-generated while write code is processing local IDs below. */
  if (!wd->use_memfile) {
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (mainvar, id_iter) {
      if (ID_IS_LINKED(id_iter) && BKE_idtype_idcode_is_linkable(GS(id_iter->name))) {
        if (USER_DEVELOPER_TOOL_TEST(&U, use_all_linked_data_direct)) {
          /* Forces all linked data to be considered as directly linked.
           * FIXME: Workaround some BAT tool limitations for Heist production, should be removed
           * asap afterward. */
          id_lib_extern(id_iter);
        }
        else if (GS(id_iter->name) == ID_SCE) {
          /* For scenes, do not force them into 'indirectly linked' status.
           * The main reason is that scenes typically have no users, so most linked scene would be
           * systematically 'lost' on file save.
           *
           * While this change re-introduces the 'no-more-used data laying around in files for
           * ever' issue when it comes to scenes, this solution seems to be the most sensible one
           * for the time being, considering that:
           *   - Scene are a top-level container.
           *   - Linked scenes are typically explicitly linked by the user.
           *   - Cases where scenes would be indirectly linked by other data (e.g. when linking a
           *     collection or material) can be considered at the very least as not following sane
           *     practice in data dependencies.
           *   - There are typically not hundreds of scenes in a file, and they are always very
           *     easily discoverable and browsable from the main UI. */
        }
        else {
          id_iter->tag |= ID_TAG_INDIRECT;
          id_iter->tag &= ~ID_TAG_EXTERN;
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }

  /* Recompute all ID user-counts if requested. Allows to avoid skipping writing of IDs wrongly
   * detected as unused due to invalid user-count. */
  if (!wd->use_memfile) {
    if (USER_DEVELOPER_TOOL_TEST(&U, use_recompute_usercount_on_save_debug)) {
      BKE_main_id_refcount_recompute(mainvar, false);
    }
  }

  write_blend_file_header(wd);
  write_renderinfo(wd, mainvar);
  write_thumb(wd, thumb);
  write_global(wd, write_flags, mainvar);

  /* The window-manager and screen often change,
   * avoid thumbnail detecting changes because of this. */
  mywrite_flush(wd);

  const bool is_undo = wd->use_memfile;
  blender::Vector<ID *> local_ids_to_write = gather_local_ids_to_write(mainvar, is_undo);

  if (!is_undo) {
    /* If not writing undo data, properly set directly linked IDs as `ID_TAG_EXTERN`. */
    for (ID *id : local_ids_to_write) {
      BKE_library_foreach_ID_link(mainvar,
                                  id,
                                  write_id_direct_linked_data_process_cb,
                                  nullptr,
                                  IDWALK_READONLY | IDWALK_INCLUDE_UI);
    }

    /* Forcefully ensure we know about all needed override operations. */
    for (ID *id : local_ids_to_write) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id) && !ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
        BKE_lib_override_library_operations_create(mainvar, id, nullptr);
      }
    }
  }

  /* Actually write local data-blocks to the file. */
  for (ID *id : local_ids_to_write) {
    write_id(wd, id);
  }

  /* Write libraries about libraries and linked data-blocks. */
  write_libraries(wd, mainvar);

  /* So changes above don't cause a 'DNA1' to be detected as changed on undo. */
  mywrite_flush(wd);

  if (use_userdef) {
    write_userdef(&writer, &U);
  }

  /* Write DNA last, because (to be implemented) test for which structs are written.
   *
   * Note that we *borrow* the pointer to 'DNAstr',
   * so writing each time uses the same address and doesn't cause unnecessary undo overhead. */
  writedata(wd, BLO_CODE_DNA1, size_t(wd->sdna->data_size), wd->sdna->data);

  /* End of file. */
  BHead bhead{};
  bhead.code = BLO_CODE_ENDB;
  write_bhead(wd, bhead);

  return mywrite_end(wd);
}

/**
 * Do reverse file history: `.blend1` -> `.blend2`, `.blend` -> `.blend1` ... etc.
 * \return True on success.
 */
static bool do_history(const char *filepath, ReportList *reports)
{
  /* Add 2 because version number maximum is double-digits. */
  char filepath_tmp1[FILE_MAX + 2], filepath_tmp2[FILE_MAX + 2];
  int version_number = min_ii(99, U.versions);

  if (version_number == 0) {
    return true;
  }

  if (strlen(filepath) < 2) {
    BKE_report(reports, RPT_ERROR, "Unable to make version backup: filename too short");
    return false;
  }

  while (version_number > 1) {
    SNPRINTF(filepath_tmp1, "%s%d", filepath, version_number - 1);
    if (BLI_exists(filepath_tmp1)) {
      SNPRINTF(filepath_tmp2, "%s%d", filepath, version_number);

      if (BLI_rename_overwrite(filepath_tmp1, filepath_tmp2)) {
        BKE_report(reports, RPT_ERROR, "Unable to make version backup");
        return false;
      }
    }
    version_number--;
  }

  /* Needed when `version_number == 1`. */
  if (BLI_exists(filepath)) {
    SNPRINTF(filepath_tmp1, "%s%d", filepath, version_number);

    if (BLI_rename_overwrite(filepath, filepath_tmp1)) {
      BKE_report(reports, RPT_ERROR, "Unable to make version backup");
      return false;
    }
  }

  return true;
}

static void write_file_main_validate_pre(Main *bmain, ReportList *reports)
{
  if (!bmain->lock) {
    return;
  }

  if (G.debug & G_DEBUG_IO) {
    BKE_report(
        reports, RPT_DEBUG, "Checking validity of current .blend file *BEFORE* save to disk");
  }

  BLO_main_validate_shapekeys(bmain, reports);
  if (!BKE_main_namemap_validate_and_fix(*bmain)) {
    BKE_report(reports,
               RPT_ERROR,
               "Critical data corruption: Conflicts and/or otherwise invalid data-blocks names "
               "(see console for details)");
  }

  if (G.debug & G_DEBUG_IO) {
    BLO_main_validate_libraries(bmain, reports);
  }
}

static void write_file_main_validate_post(Main *bmain, ReportList *reports)
{
  if (!bmain->lock) {
    return;
  }

  if (G.debug & G_DEBUG_IO) {
    BKE_report(
        reports, RPT_DEBUG, "Checking validity of current .blend file *BEFORE* save to disk");
    BLO_main_validate_libraries(bmain, reports);
  }
}

static bool BLO_write_file_impl(Main *mainvar,
                                const char *filepath,
                                const int write_flags,
                                const BlendFileWriteParams *params,
                                ReportList *reports,
                                WriteWrap &ww)
{
  BLI_assert(!BLI_path_is_rel(filepath));
  BLI_assert(BLI_path_is_abs_from_cwd(filepath));

  char tempname[FILE_MAX + 1];

  eBLO_WritePathRemap remap_mode = params->remap_mode;
  const bool use_save_versions = params->use_save_versions;
  const bool use_save_as_copy = params->use_save_as_copy;
  const bool use_userdef = params->use_userdef;
  const BlendThumbnail *thumb = params->thumb;
  const bool relbase_valid = (mainvar->filepath[0] != '\0');

  /* Extra protection: Never save a non asset file as asset file. Otherwise a normal file is turned
   * into an asset file, which can result in data loss because the asset system will allow editing
   * this file from the UI, regenerating its content with just the asset and it dependencies. */
  if ((write_flags & G_FILE_ASSET_EDIT_FILE) && !mainvar->is_asset_edit_file) {
    BKE_reportf(reports, RPT_ERROR, "Cannot save normal file (%s) as asset system file", tempname);
    return false;
  }

  /* Path backup/restore. */
  void *path_list_backup = nullptr;
  const eBPathForeachFlag path_list_flag = (BKE_BPATH_FOREACH_PATH_SKIP_LINKED |
                                            BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE);

  write_file_main_validate_pre(mainvar, reports);

  /* Open temporary file, so we preserve the original in case we crash. */
  SNPRINTF(tempname, "%s@", filepath);

  if (ww.open(tempname) == false) {
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

#if GENERATE_DEBUG_BLEND_FILE
  std::string debug_dst_path = blender::StringRef(filepath) + DEBUG_BLEND_FILE_SUFFIX;
  blender::fstream debug_dst_file(debug_dst_path, std::ios::out);
  std::ostream *debug_dst = &debug_dst_file;
#else
  std::ostream *debug_dst = nullptr;
#endif

  /* Actual file writing. */
  const bool err = write_file_handle(
      mainvar, &ww, nullptr, nullptr, write_flags, use_userdef, thumb, debug_dst);

  ww.close();

  if (UNLIKELY(path_list_backup)) {
    BKE_bpath_list_restore(mainvar, path_list_flag, path_list_backup);
    BKE_bpath_list_free(path_list_backup);
  }

  if (err) {
    BKE_report(reports, RPT_ERROR, strerror(errno));
    remove(tempname);

    return false;
  }

  /* File save to temporary file was successful, now do reverse file history
   * (move `.blend1` -> `.blend2`, `.blend` -> `.blend1` .. etc). */
  if (use_save_versions) {
    if (!do_history(filepath, reports)) {
      BKE_report(reports, RPT_ERROR, "Version backup failed (file saved with @)");
      return false;
    }
  }

  if (BLI_rename_overwrite(tempname, filepath) != 0) {
    BKE_report(reports, RPT_ERROR, "Cannot change old file (file saved with @)");
    return false;
  }

  write_file_main_validate_post(mainvar, reports);
  if (mainvar->is_global_main && !params->use_save_as_copy) {
    /* It is used to reload Blender after a crash on Windows OS. */
    STRNCPY(G.filepath_last_blend, filepath);
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Writing (Public)
 * \{ */

bool BLO_write_file(Main *mainvar,
                    const char *filepath,
                    const int write_flags,
                    const BlendFileWriteParams *params,
                    ReportList *reports)
{
  RawWriteWrap raw_wrap;

  if (write_flags & G_FILE_COMPRESS) {
    ZstdWriteWrap zstd_wrap(raw_wrap);
    return BLO_write_file_impl(mainvar, filepath, write_flags, params, reports, zstd_wrap);
  }

  return BLO_write_file_impl(mainvar, filepath, write_flags, params, reports, raw_wrap);
}

bool BLO_write_file_mem(Main *mainvar, MemFile *compare, MemFile *current, const int write_flags)
{
  bool use_userdef = false;

  const bool err = write_file_handle(
      mainvar, nullptr, compare, current, write_flags, use_userdef, nullptr, nullptr);

  return (err == 0);
}

/*
 * API to write chunks of data.
 */

void BLO_write_raw(BlendWriter *writer, const size_t size_in_bytes, const void *data_ptr)
{
  writedata(writer->wd, BLO_CODE_DATA, size_in_bytes, data_ptr);
}

void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr)
{
  BLO_write_struct_array_by_name(writer, struct_name, 1, data_ptr);
}

void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    const int64_t array_size,
                                    const void *data_ptr)
{
  int struct_id = BLO_get_struct_id_by_name(writer, struct_name);
  if (UNLIKELY(struct_id == -1)) {
    CLOG_ERROR(&LOG, "Can't find SDNA code <%s>", struct_name);
    return;
  }
  BLO_write_struct_array_by_id(writer, struct_id, array_size, data_ptr);
}

void BLO_write_struct_by_id(BlendWriter *writer, const int struct_id, const void *data_ptr)
{
  writestruct_nr(writer->wd, BLO_CODE_DATA, struct_id, 1, data_ptr);
}

void BLO_write_struct_at_address_by_id(BlendWriter *writer,
                                       const int struct_id,
                                       const void *address,
                                       const void *data_ptr)
{
  BLO_write_struct_at_address_by_id_with_filecode(
      writer, BLO_CODE_DATA, struct_id, address, data_ptr);
}

void BLO_write_struct_at_address_by_id_with_filecode(BlendWriter *writer,
                                                     const int filecode,
                                                     const int struct_id,
                                                     const void *address,
                                                     const void *data_ptr)
{
  writestruct_at_address_nr(writer->wd, filecode, struct_id, 1, address, data_ptr);
}

void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  const int struct_id,
                                  const int64_t array_size,
                                  const void *data_ptr)
{
  writestruct_nr(writer->wd, BLO_CODE_DATA, struct_id, array_size, data_ptr);
}

void BLO_write_struct_array_at_address_by_id(BlendWriter *writer,
                                             const int struct_id,
                                             const int64_t array_size,
                                             const void *address,
                                             const void *data_ptr)
{
  writestruct_at_address_nr(writer->wd, BLO_CODE_DATA, struct_id, array_size, address, data_ptr);
}

void BLO_write_struct_list_by_id(BlendWriter *writer, const int struct_id, const ListBase *list)
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

void blo_write_id_struct(BlendWriter *writer,
                         const int struct_id,
                         const void *id_address,
                         const ID *id)
{
  writestruct_at_address_nr(writer->wd, GS(id->name), struct_id, 1, id_address, id);
}

int BLO_get_struct_id_by_name(const BlendWriter *writer, const char *struct_name)
{
  int struct_id = DNA_struct_find_with_alias(writer->wd->sdna, struct_name);
  return struct_id;
}

void BLO_write_char_array(BlendWriter *writer, const int64_t num, const char *data_ptr)
{
  BLO_write_raw(writer, sizeof(char) * size_t(num), data_ptr);
}

void BLO_write_int8_array(BlendWriter *writer, const int64_t num, const int8_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int8_t) * size_t(num), data_ptr);
}

void BLO_write_int16_array(BlendWriter *writer, const int64_t num, const int16_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int16_t) * size_t(num), data_ptr);
}

void BLO_write_uint8_array(BlendWriter *writer, const int64_t num, const uint8_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(uint8_t) * size_t(num), data_ptr);
}

void BLO_write_int32_array(BlendWriter *writer, const int64_t num, const int32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(int32_t) * size_t(num), data_ptr);
}

void BLO_write_uint32_array(BlendWriter *writer, const int64_t num, const uint32_t *data_ptr)
{
  BLO_write_raw(writer, sizeof(uint32_t) * size_t(num), data_ptr);
}

void BLO_write_float_array(BlendWriter *writer, const int64_t num, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float) * size_t(num), data_ptr);
}

void BLO_write_double_array(BlendWriter *writer, const int64_t num, const double *data_ptr)
{
  BLO_write_raw(writer, sizeof(double) * size_t(num), data_ptr);
}

void BLO_write_pointer_array(BlendWriter *writer, const int64_t num, const void *data_ptr)
{
  /* Create a temporary copy of the pointer array, because all pointers need to be remapped to
   * their stable address ids. */
  blender::Array<const void *, 32> data = blender::Span<const void *>(
      reinterpret_cast<const void *const *>(data_ptr), num);
  for (const int64_t i : data.index_range()) {
    data[i] = get_address_id(*writer->wd, data[i]);
  }

  writedata(writer->wd, BLO_CODE_DATA, data.data(), data.as_span().size_in_bytes(), data_ptr);
}

void BLO_write_float3_array(BlendWriter *writer, const int64_t num, const float *data_ptr)
{
  BLO_write_raw(writer, sizeof(float[3]) * size_t(num), data_ptr);
}

void BLO_write_string(BlendWriter *writer, const char *data_ptr)
{
  if (data_ptr != nullptr) {
    BLO_write_raw(writer, strlen(data_ptr) + 1, data_ptr);
  }
}

void BLO_write_shared_tag(BlendWriter *writer, const void *data)
{
  if (!data) {
    return;
  }
  if (!BLO_write_is_undo(writer)) {
    return;
  }
  const uint64_t address_id = get_address_id_for_implicit_sharing_data(data);
  /* Check that the pointer has not been written before it was tagged as being shared. */
  BLI_assert(writer->wd->stable_address_ids.pointer_map.lookup_default(data, address_id) ==
             address_id);
  writer->wd->stable_address_ids.pointer_map.add(data, address_id);
}

void BLO_write_shared(BlendWriter *writer,
                      const void *data,
                      const size_t approximate_size_in_bytes,
                      const blender::ImplicitSharingInfo *sharing_info,
                      const blender::FunctionRef<void()> write_fn)
{
  if (data == nullptr) {
    return;
  }
  if (sharing_info) {
    BLO_write_shared_tag(writer, data);
  }
  const uint64_t address_id = get_address_id_int(*writer->wd, data);
  if (BLO_write_is_undo(writer)) {
    MemFile &memfile = *writer->wd->mem.written_memfile;
    if (sharing_info != nullptr) {
      if (memfile.shared_storage == nullptr) {
        memfile.shared_storage = MEM_new<MemFileSharedStorage>(__func__);
      }
      if (memfile.shared_storage->sharing_info_by_address_id.add(address_id, {sharing_info, data}))
      {
        /* The undo-step takes (shared) ownership of the data, which also makes it immutable. */
        sharing_info->add_user();
        /* This size is an estimate, but good enough to count data with many users less. */
        memfile.size += approximate_size_in_bytes / sharing_info->strong_users();
        return;
      }
    }
  }
  if (sharing_info != nullptr) {
    if (!writer->wd->per_id_written_shared_addresses.add(data)) {
      /* Was written already. */
      return;
    }
  }
  write_fn();
}

bool BLO_write_is_undo(BlendWriter *writer)
{
  return writer->wd->use_memfile;
}

/** \} */
