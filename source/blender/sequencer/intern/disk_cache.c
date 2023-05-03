/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup sequencer
 */

#include <memory.h>
#include <stddef.h>
#include <time.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h" /* for FILE_MAX. */

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_blenlib.h"
#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_path_util.h"
#include "BLI_threads.h"

#include "BKE_main.h"
#include "BKE_scene.h"

#include "SEQ_prefetch.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

#include "disk_cache.h"
#include "image_cache.h"
#include "prefetch.h"
#include "strip_time.h"

/**
 * Disk Cache Design Notes
 * =======================
 *
 * Disk cache uses directory specified in user preferences
 * For each cached non-temp image, image data and supplementary info are written to HDD.
 * Multiple(DCACHE_IMAGES_PER_FILE) images share the same file.
 * Each of these files contains header DiskCacheHeader followed by image data.
 * Zlib compression with user definable level can be used to compress image data(per image)
 * Images are written in order in which they are rendered.
 * Overwriting of individual entry is not possible.
 * Stored images are deleted by invalidation, or when size of all files exceeds maximum
 * size specified in user preferences.
 * To distinguish 2 blend files with same name, scene->ed->disk_cache_timestamp
 * is used as UID. Blend file can still be copied manually which may cause conflict.
 */

/* Format string:
 * `<cache type>-<resolution X>x<resolution Y>-<rendersize>%(<view_id>)-<frame no>.dcf`. */
#define DCACHE_FNAME_FORMAT "%d-%dx%d-%d%%(%d)-%d.dcf"
#define DCACHE_IMAGES_PER_FILE 100
#define DCACHE_CURRENT_VERSION 2
#define COLORSPACE_NAME_MAX 64 /* XXX: defined in IMB intern. */

typedef struct DiskCacheHeaderEntry {
  uchar encoding;
  uint64_t frameno;
  uint64_t size_compressed;
  uint64_t size_raw;
  uint64_t offset;
  char colorspace_name[COLORSPACE_NAME_MAX];
} DiskCacheHeaderEntry;

typedef struct DiskCacheHeader {
  DiskCacheHeaderEntry entry[DCACHE_IMAGES_PER_FILE];
} DiskCacheHeader;

typedef struct SeqDiskCache {
  Main *bmain;
  int64_t timestamp;
  ListBase files;
  ThreadMutex read_write_mutex;
  size_t size_total;
} SeqDiskCache;

typedef struct DiskCacheFile {
  struct DiskCacheFile *next, *prev;
  char path[FILE_MAX];
  char dir[FILE_MAXDIR];
  char file[FILE_MAX];
  BLI_stat_t fstat;
  int cache_type;
  int rectx;
  int recty;
  int render_size;
  int view_id;
  int start_frame;
} DiskCacheFile;

static ThreadMutex cache_create_lock = BLI_MUTEX_INITIALIZER;

static char *seq_disk_cache_base_dir(void)
{
  return U.sequencer_disk_cache_dir;
}

static int seq_disk_cache_compression_level(void)
{
  switch (U.sequencer_disk_cache_compression) {
    case USER_SEQ_DISK_CACHE_COMPRESSION_NONE:
      return 0;
    case USER_SEQ_DISK_CACHE_COMPRESSION_LOW:
      return 1;
    case USER_SEQ_DISK_CACHE_COMPRESSION_HIGH:
      return 9;
  }

  return U.sequencer_disk_cache_compression;
}

static size_t seq_disk_cache_size_limit(void)
{
  return (size_t)U.sequencer_disk_cache_size_limit * (1024 * 1024 * 1024);
}

bool seq_disk_cache_is_enabled(Main *bmain)
{
  return (U.sequencer_disk_cache_dir[0] != '\0' && U.sequencer_disk_cache_size_limit != 0 &&
          (U.sequencer_disk_cache_flag & SEQ_CACHE_DISK_CACHE_ENABLE) != 0 &&
          bmain->filepath[0] != '\0');
}

static DiskCacheFile *seq_disk_cache_add_file_to_list(SeqDiskCache *disk_cache, const char *path)
{

  DiskCacheFile *cache_file = MEM_callocN(sizeof(DiskCacheFile), "SeqDiskCacheFile");
  char dir[FILE_MAXDIR], file[FILE_MAX];
  BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));
  BLI_strncpy(cache_file->path, path, sizeof(cache_file->path));
  BLI_strncpy(cache_file->dir, dir, sizeof(cache_file->dir));
  BLI_strncpy(cache_file->file, file, sizeof(cache_file->file));
  sscanf(file,
         DCACHE_FNAME_FORMAT,
         &cache_file->cache_type,
         &cache_file->rectx,
         &cache_file->recty,
         &cache_file->render_size,
         &cache_file->view_id,
         &cache_file->start_frame);
  cache_file->start_frame *= DCACHE_IMAGES_PER_FILE;
  BLI_addtail(&disk_cache->files, cache_file);
  return cache_file;
}

static void seq_disk_cache_get_files(SeqDiskCache *disk_cache, char *path)
{
  struct direntry *filelist, *fl;
  uint i;
  disk_cache->size_total = 0;

  const int filelist_num = BLI_filelist_dir_contents(path, &filelist);
  i = filelist_num;
  fl = filelist;
  while (i--) {
    /* Don't follow links. */
    const eFileAttributes file_attrs = BLI_file_attributes(fl->path);
    if (file_attrs & FILE_ATTR_ANY_LINK) {
      fl++;
      continue;
    }

    char file[FILE_MAX];
    BLI_path_split_file_part(fl->path, file, sizeof(file));

    bool is_dir = BLI_is_dir(fl->path);
    if (is_dir && !FILENAME_IS_CURRPAR(file)) {
      char subpath[FILE_MAX];
      BLI_strncpy(subpath, fl->path, sizeof(subpath));
      BLI_path_slash_ensure(subpath, sizeof(sizeof(subpath)));
      seq_disk_cache_get_files(disk_cache, subpath);
    }

    if (!is_dir) {
      const char *ext = BLI_path_extension(fl->path);
      if (ext && ext[1] == 'd' && ext[2] == 'c' && ext[3] == 'f') {
        DiskCacheFile *cache_file = seq_disk_cache_add_file_to_list(disk_cache, fl->path);
        cache_file->fstat = fl->s;
        disk_cache->size_total += cache_file->fstat.st_size;
      }
    }
    fl++;
  }
  BLI_filelist_free(filelist, filelist_num);
}

static DiskCacheFile *seq_disk_cache_get_oldest_file(SeqDiskCache *disk_cache)
{
  DiskCacheFile *oldest_file = disk_cache->files.first;
  if (oldest_file == NULL) {
    return NULL;
  }
  for (DiskCacheFile *cache_file = oldest_file->next; cache_file; cache_file = cache_file->next) {
    if (cache_file->fstat.st_mtime < oldest_file->fstat.st_mtime) {
      oldest_file = cache_file;
    }
  }

  return oldest_file;
}

static void seq_disk_cache_delete_file(SeqDiskCache *disk_cache, DiskCacheFile *file)
{
  disk_cache->size_total -= file->fstat.st_size;
  BLI_delete(file->path, false, false);
  BLI_remlink(&disk_cache->files, file);
  MEM_freeN(file);
}

bool seq_disk_cache_enforce_limits(SeqDiskCache *disk_cache)
{
  BLI_mutex_lock(&disk_cache->read_write_mutex);
  while (disk_cache->size_total > seq_disk_cache_size_limit()) {
    DiskCacheFile *oldest_file = seq_disk_cache_get_oldest_file(disk_cache);

    if (!oldest_file) {
      /* We shouldn't enforce limits with no files, do re-scan. */
      seq_disk_cache_get_files(disk_cache, seq_disk_cache_base_dir());
      continue;
    }

    if (BLI_exists(oldest_file->path) == 0) {
      /* File may have been manually deleted during runtime, do re-scan. */
      BLI_freelistN(&disk_cache->files);
      seq_disk_cache_get_files(disk_cache, seq_disk_cache_base_dir());
      continue;
    }

    seq_disk_cache_delete_file(disk_cache, oldest_file);
  }
  BLI_mutex_unlock(&disk_cache->read_write_mutex);

  return true;
}

static DiskCacheFile *seq_disk_cache_get_file_entry_by_path(SeqDiskCache *disk_cache, char *path)
{
  DiskCacheFile *cache_file = disk_cache->files.first;

  for (; cache_file; cache_file = cache_file->next) {
    if (BLI_strcasecmp(cache_file->path, path) == 0) {
      return cache_file;
    }
  }

  return NULL;
}

/* Update file size and timestamp. */
static void seq_disk_cache_update_file(SeqDiskCache *disk_cache, char *path)
{
  DiskCacheFile *cache_file;
  int64_t size_before;
  int64_t size_after;

  cache_file = seq_disk_cache_get_file_entry_by_path(disk_cache, path);
  size_before = cache_file->fstat.st_size;

  if (BLI_stat(path, &cache_file->fstat) == -1) {
    BLI_assert(false);
    memset(&cache_file->fstat, 0, sizeof(BLI_stat_t));
  }

  size_after = cache_file->fstat.st_size;
  disk_cache->size_total += size_after - size_before;
}

/* Path format:
 * <cache dir>/<project name>_seq_cache/<scene name>-<timestamp>/<seq name>/DCACHE_FNAME_FORMAT
 */

static void seq_disk_cache_get_project_dir(SeqDiskCache *disk_cache, char *path, size_t path_len)
{
  char cache_dir[FILE_MAX];
  BLI_path_split_file_part(
      BKE_main_blendfile_path(disk_cache->bmain), cache_dir, sizeof(cache_dir));
  /* Use suffix, so that the cache directory name does not conflict with the bmain's blend file. */
  const char *suffix = "_seq_cache";
  strncat(cache_dir, suffix, sizeof(cache_dir) - strlen(cache_dir) - 1);

  BLI_path_join(path, path_len, seq_disk_cache_base_dir(), cache_dir);
}

static void seq_disk_cache_get_dir(
    SeqDiskCache *disk_cache, Scene *scene, Sequence *seq, char *path, size_t path_len)
{
  char scene_name[MAX_ID_NAME + 22]; /* + -%PRId64 */
  char seq_name[SEQ_NAME_MAXSTR];
  char project_dir[FILE_MAX];

  seq_disk_cache_get_project_dir(disk_cache, project_dir, sizeof(project_dir));
  BLI_snprintf(
      scene_name, sizeof(scene_name), "%s-%" PRId64, scene->id.name, disk_cache->timestamp);
  BLI_strncpy(seq_name, seq->name, sizeof(seq_name));
  BLI_path_make_safe_filename(scene_name);
  BLI_path_make_safe_filename(seq_name);

  BLI_path_join(path, path_len, project_dir, scene_name, seq_name);
}

static void seq_disk_cache_get_file_path(SeqDiskCache *disk_cache,
                                         SeqCacheKey *key,
                                         char *path,
                                         size_t path_len)
{
  seq_disk_cache_get_dir(disk_cache, key->context.scene, key->seq, path, path_len);
  int frameno = (int)key->frame_index / DCACHE_IMAGES_PER_FILE;
  char cache_filename[FILE_MAXFILE];
  BLI_snprintf(cache_filename,
               sizeof(cache_filename),
               DCACHE_FNAME_FORMAT,
               key->type,
               key->context.rectx,
               key->context.recty,
               key->context.preview_render_size,
               key->context.view_id,
               frameno);

  BLI_path_append(path, path_len, cache_filename);
}

static void seq_disk_cache_create_version_file(char *filepath)
{
  BLI_file_ensure_parent_dir_exists(filepath);

  FILE *file = BLI_fopen(filepath, "w");
  if (file) {
    fprintf(file, "%d", DCACHE_CURRENT_VERSION);
    fclose(file);
  }
}

static void seq_disk_cache_handle_versioning(SeqDiskCache *disk_cache)
{
  char filepath[FILE_MAX];
  char path_version_file[FILE_MAX];
  int version = 0;

  seq_disk_cache_get_project_dir(disk_cache, filepath, sizeof(filepath));
  BLI_path_join(path_version_file, sizeof(path_version_file), filepath, "cache_version");

  if (BLI_exists(filepath) && BLI_is_dir(filepath)) {
    FILE *file = BLI_fopen(path_version_file, "r");

    if (file) {
      const int num_items_read = fscanf(file, "%d", &version);
      if (num_items_read == 0) {
        version = -1;
      }
      fclose(file);
    }

    if (version != DCACHE_CURRENT_VERSION) {
      BLI_delete(filepath, false, true);
      seq_disk_cache_create_version_file(path_version_file);
    }
  }
  else {
    seq_disk_cache_create_version_file(path_version_file);
  }
}

static void seq_disk_cache_delete_invalid_files(SeqDiskCache *disk_cache,
                                                Scene *scene,
                                                Sequence *seq,
                                                int invalidate_types,
                                                int range_start,
                                                int range_end)
{
  DiskCacheFile *next_file, *cache_file = disk_cache->files.first;
  char cache_dir[FILE_MAX];
  seq_disk_cache_get_dir(disk_cache, scene, seq, cache_dir, sizeof(cache_dir));
  BLI_path_slash_ensure(cache_dir, sizeof(cache_dir));

  while (cache_file) {
    next_file = cache_file->next;
    if (cache_file->cache_type & invalidate_types) {
      if (STREQ(cache_dir, cache_file->dir)) {
        int timeline_frame_start = seq_cache_frame_index_to_timeline_frame(
            seq, cache_file->start_frame);
        if (timeline_frame_start > range_start && timeline_frame_start <= range_end) {
          seq_disk_cache_delete_file(disk_cache, cache_file);
        }
      }
    }
    cache_file = next_file;
  }
}

void seq_disk_cache_invalidate(SeqDiskCache *disk_cache,
                               Scene *scene,
                               Sequence *seq,
                               Sequence *seq_changed,
                               int invalidate_types)
{
  int start;
  int end;

  BLI_mutex_lock(&disk_cache->read_write_mutex);

  start = SEQ_time_left_handle_frame_get(scene, seq_changed) - DCACHE_IMAGES_PER_FILE;
  end = SEQ_time_right_handle_frame_get(scene, seq_changed);

  seq_disk_cache_delete_invalid_files(disk_cache, scene, seq, invalidate_types, start, end);

  BLI_mutex_unlock(&disk_cache->read_write_mutex);
}

static size_t deflate_imbuf_to_file(ImBuf *ibuf,
                                    FILE *file,
                                    int level,
                                    DiskCacheHeaderEntry *header_entry)
{
  void *data = (ibuf->rect != NULL) ? (void *)ibuf->rect : (void *)ibuf->rect_float;

  /* Apply compression if wanted, otherwise just write directly to the file. */
  if (level > 0) {
    return BLI_file_zstd_from_mem_at_pos(
        data, header_entry->size_raw, file, header_entry->offset, level);
  }

  fseek(file, header_entry->offset, SEEK_SET);
  return fwrite(data, 1, header_entry->size_raw, file);
}

static size_t inflate_file_to_imbuf(ImBuf *ibuf, FILE *file, DiskCacheHeaderEntry *header_entry)
{
  void *data = (ibuf->rect != NULL) ? (void *)ibuf->rect : (void *)ibuf->rect_float;
  char header[4];
  fseek(file, header_entry->offset, SEEK_SET);
  if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
    return 0;
  }

  /* Check if the data is compressed or raw. */
  if (BLI_file_magic_is_zstd(header)) {
    return BLI_file_unzstd_to_mem_at_pos(data, header_entry->size_raw, file, header_entry->offset);
  }

  fseek(file, header_entry->offset, SEEK_SET);
  return fread(data, 1, header_entry->size_raw, file);
}

static bool seq_disk_cache_read_header(FILE *file, DiskCacheHeader *header)
{
  BLI_fseek(file, 0LL, SEEK_SET);
  const size_t num_items_read = fread(header, sizeof(*header), 1, file);
  if (num_items_read < 1) {
    BLI_assert_msg(0, "unable to read disk cache header");
    perror("unable to read disk cache header");
    return false;
  }

  for (int i = 0; i < DCACHE_IMAGES_PER_FILE; i++) {
    if ((ENDIAN_ORDER == B_ENDIAN) && header->entry[i].encoding == 0) {
      BLI_endian_switch_uint64(&header->entry[i].frameno);
      BLI_endian_switch_uint64(&header->entry[i].offset);
      BLI_endian_switch_uint64(&header->entry[i].size_compressed);
      BLI_endian_switch_uint64(&header->entry[i].size_raw);
    }
  }

  return true;
}

static size_t seq_disk_cache_write_header(FILE *file, DiskCacheHeader *header)
{
  BLI_fseek(file, 0LL, SEEK_SET);
  return fwrite(header, sizeof(*header), 1, file);
}

static int seq_disk_cache_add_header_entry(SeqCacheKey *key, ImBuf *ibuf, DiskCacheHeader *header)
{
  int i;
  uint64_t offset = sizeof(*header);

  /* Lookup free entry, get offset for new data. */
  for (i = 0; i < DCACHE_IMAGES_PER_FILE; i++) {
    if (header->entry[i].size_compressed == 0) {
      break;
    }
  }

  /* Attempt to write beyond set entry limit.
   * Reset file header and start writing from beginning.
   */
  if (i == DCACHE_IMAGES_PER_FILE) {
    i = 0;
    memset(header, 0, sizeof(*header));
  }

  /* Calculate offset for image data. */
  if (i > 0) {
    offset = header->entry[i - 1].offset + header->entry[i - 1].size_compressed;
  }

  if (ENDIAN_ORDER == B_ENDIAN) {
    header->entry[i].encoding = 255;
  }
  else {
    header->entry[i].encoding = 0;
  }

  header->entry[i].offset = offset;
  header->entry[i].frameno = key->frame_index;

  /* Store colorspace name of ibuf. */
  const char *colorspace_name;
  if (ibuf->rect) {
    header->entry[i].size_raw = ibuf->x * ibuf->y * ibuf->channels;
    colorspace_name = IMB_colormanagement_get_rect_colorspace(ibuf);
  }
  else {
    header->entry[i].size_raw = ibuf->x * ibuf->y * ibuf->channels * 4;
    colorspace_name = IMB_colormanagement_get_float_colorspace(ibuf);
  }
  BLI_strncpy(
      header->entry[i].colorspace_name, colorspace_name, sizeof(header->entry[i].colorspace_name));

  return i;
}

static int seq_disk_cache_get_header_entry(SeqCacheKey *key, DiskCacheHeader *header)
{
  for (int i = 0; i < DCACHE_IMAGES_PER_FILE; i++) {
    if (header->entry[i].frameno == key->frame_index) {
      return i;
    }
  }

  return -1;
}

bool seq_disk_cache_write_file(SeqDiskCache *disk_cache, SeqCacheKey *key, ImBuf *ibuf)
{
  BLI_mutex_lock(&disk_cache->read_write_mutex);

  char filepath[FILE_MAX];

  seq_disk_cache_get_file_path(disk_cache, key, filepath, sizeof(filepath));
  BLI_file_ensure_parent_dir_exists(filepath);

  /* Touch the file. */
  FILE *file = BLI_fopen(filepath, "rb+");
  if (!file) {
    file = BLI_fopen(filepath, "wb+");
    if (!file) {
      BLI_mutex_unlock(&disk_cache->read_write_mutex);
      return false;
    }
    seq_disk_cache_add_file_to_list(disk_cache, filepath);
  }

  DiskCacheFile *cache_file = seq_disk_cache_get_file_entry_by_path(disk_cache, filepath);
  DiskCacheHeader header;
  memset(&header, 0, sizeof(header));
  /* The file may be empty when touched (above).
   * This is fine, don't attempt reading the header in that case. */
  if (cache_file->fstat.st_size != 0 && !seq_disk_cache_read_header(file, &header)) {
    fclose(file);
    seq_disk_cache_delete_file(disk_cache, cache_file);
    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return false;
  }
  int entry_index = seq_disk_cache_add_header_entry(key, ibuf, &header);

  size_t bytes_written = deflate_imbuf_to_file(
      ibuf, file, seq_disk_cache_compression_level(), &header.entry[entry_index]);

  if (bytes_written != 0) {
    /* Last step is writing header, as image data can be overwritten,
     * but missing data would cause problems.
     */
    header.entry[entry_index].size_compressed = bytes_written;
    seq_disk_cache_write_header(file, &header);
    seq_disk_cache_update_file(disk_cache, filepath);
    fclose(file);

    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return true;
  }

  BLI_mutex_unlock(&disk_cache->read_write_mutex);
  return false;
}

ImBuf *seq_disk_cache_read_file(SeqDiskCache *disk_cache, SeqCacheKey *key)
{
  BLI_mutex_lock(&disk_cache->read_write_mutex);

  char filepath[FILE_MAX];
  DiskCacheHeader header;

  seq_disk_cache_get_file_path(disk_cache, key, filepath, sizeof(filepath));
  BLI_file_ensure_parent_dir_exists(filepath);

  FILE *file = BLI_fopen(filepath, "rb");
  if (!file) {
    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return NULL;
  }

  if (!seq_disk_cache_read_header(file, &header)) {
    fclose(file);
    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return NULL;
  }
  int entry_index = seq_disk_cache_get_header_entry(key, &header);

  /* Item not found. */
  if (entry_index < 0) {
    fclose(file);
    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return NULL;
  }

  ImBuf *ibuf;
  uint64_t size_char = (uint64_t)key->context.rectx * key->context.recty * 4;
  uint64_t size_float = (uint64_t)key->context.rectx * key->context.recty * 16;
  size_t expected_size;

  if (header.entry[entry_index].size_raw == size_char) {
    expected_size = size_char;
    ibuf = IMB_allocImBuf(key->context.rectx, key->context.recty, 32, IB_rect);
    IMB_colormanagement_assign_rect_colorspace(ibuf, header.entry[entry_index].colorspace_name);
  }
  else if (header.entry[entry_index].size_raw == size_float) {
    expected_size = size_float;
    ibuf = IMB_allocImBuf(key->context.rectx, key->context.recty, 32, IB_rectfloat);
    IMB_colormanagement_assign_float_colorspace(ibuf, header.entry[entry_index].colorspace_name);
  }
  else {
    fclose(file);
    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return NULL;
  }

  size_t bytes_read = inflate_file_to_imbuf(ibuf, file, &header.entry[entry_index]);

  /* Sanity check. */
  if (bytes_read != expected_size) {
    fclose(file);
    IMB_freeImBuf(ibuf);
    BLI_mutex_unlock(&disk_cache->read_write_mutex);
    return NULL;
  }
  BLI_file_touch(filepath);
  seq_disk_cache_update_file(disk_cache, filepath);
  fclose(file);

  BLI_mutex_unlock(&disk_cache->read_write_mutex);
  return ibuf;
}

SeqDiskCache *seq_disk_cache_create(Main *bmain, Scene *scene)
{
  SeqDiskCache *disk_cache = MEM_callocN(sizeof(SeqDiskCache), "SeqDiskCache");
  disk_cache->bmain = bmain;
  BLI_mutex_init(&disk_cache->read_write_mutex);
  seq_disk_cache_handle_versioning(disk_cache);
  seq_disk_cache_get_files(disk_cache, seq_disk_cache_base_dir());
  disk_cache->timestamp = scene->ed->disk_cache_timestamp;
  BLI_mutex_unlock(&cache_create_lock);
  return disk_cache;
}

void seq_disk_cache_free(SeqDiskCache *disk_cache)
{
  BLI_freelistN(&disk_cache->files);
  BLI_mutex_end(&disk_cache->read_write_mutex);
  MEM_freeN(disk_cache);
}
