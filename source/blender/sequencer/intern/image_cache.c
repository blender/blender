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
 * Peter Schlaile <peter [at] schlaile [dot] de> 2010
 */

/** \file
 * \ingroup bke
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
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_path_util.h"
#include "BLI_threads.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "SEQ_prefetch.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"

#include "image_cache.h"
#include "prefetch.h"
#include "strip_time.h"

/**
 * Sequencer Cache Design Notes
 * ============================
 *
 * Function:
 * All images created during rendering are added to cache, even if the cache is already full.
 * This is because:
 *  - one image may be needed multiple times during rendering.
 *  - keeping the last rendered frame allows us for faster re-render when user edits strip in stack
 *  - we can decide if we keep frame only when it's completely rendered. Otherwise we risk having
 *    "holes" in the cache, which can be annoying
 * If the cache is full all entries for pending frame will have is_temp_cache set.
 *
 * Linking: We use links to reduce number of iterations over entries needed to manage cache.
 * Entries are linked in order as they are put into cache.
 * Only permanent (is_temp_cache = 0) cache entries are linked.
 * Putting #SEQ_CACHE_STORE_FINAL_OUT will reset linking
 *
 * Only entire frame can be freed to release resources for new entries (recycling).
 * Once again, this is to reduce number of iterations, but also more controllable than removing
 * entries one by one in reverse order to their creation.
 *
 * User can exclude caching of some images. Such entries will have is_temp_cache set.
 *
 *
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
 *
 */

/* <cache type>-<resolution X>x<resolution Y>-<rendersize>%(<view_id>)-<frame no>.dcf */
#define DCACHE_FNAME_FORMAT "%d-%dx%d-%d%%(%d)-%d.dcf"
#define DCACHE_IMAGES_PER_FILE 100
#define DCACHE_CURRENT_VERSION 1
#define COLORSPACE_NAME_MAX 64 /* XXX: defined in imb intern */

typedef struct DiskCacheHeaderEntry {
  unsigned char encoding;
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

typedef struct SeqCache {
  Main *bmain;
  struct GHash *hash;
  ThreadMutex iterator_mutex;
  struct BLI_mempool *keys_pool;
  struct BLI_mempool *items_pool;
  struct SeqCacheKey *last_key;
  SeqDiskCache *disk_cache;
} SeqCache;

typedef struct SeqCacheItem {
  struct SeqCache *cache_owner;
  struct ImBuf *ibuf;
} SeqCacheItem;

typedef struct SeqCacheKey {
  struct SeqCache *cache_owner;
  void *userkey;
  struct SeqCacheKey *link_prev; /* Used for linking intermediate items to final frame. */
  struct SeqCacheKey *link_next; /* Used for linking intermediate items to final frame. */
  struct Sequence *seq;
  SeqRenderData context;
  float frame_index;    /* Usually same as timeline_frame. Mapped to media for RAW entries. */
  float timeline_frame; /* Only for reference - used for freeing when cache is full. */
  float cost;           /* In short: render time(s) divided by playback frame duration(s) */
  bool is_temp_cache;   /* this cache entry will be freed before rendering next frame */
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id;
  int type;
} SeqCacheKey;

static ThreadMutex cache_create_lock = BLI_MUTEX_INITIALIZER;
static float seq_cache_timeline_frame_to_frame_index(Sequence *seq,
                                                     float timeline_frame,
                                                     int type);
static float seq_cache_frame_index_to_timeline_frame(Sequence *seq, float frame_index);

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

static bool seq_disk_cache_is_enabled(Main *bmain)
{
  return (U.sequencer_disk_cache_dir[0] != '\0' && U.sequencer_disk_cache_size_limit != 0 &&
          (U.sequencer_disk_cache_flag & SEQ_CACHE_DISK_CACHE_ENABLE) != 0 &&
          bmain->name[0] != '\0');
}

static DiskCacheFile *seq_disk_cache_add_file_to_list(SeqDiskCache *disk_cache, const char *path)
{

  DiskCacheFile *cache_file = MEM_callocN(sizeof(DiskCacheFile), "SeqDiskCacheFile");
  char dir[FILE_MAXDIR], file[FILE_MAX];
  BLI_split_dirfile(path, dir, file, sizeof(dir), sizeof(file));
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
  uint nbr, i;
  disk_cache->size_total = 0;

  i = nbr = BLI_filelist_dir_contents(path, &filelist);
  fl = filelist;
  while (i--) {
    /* Don't follow links. */
    const eFileAttributes file_attrs = BLI_file_attributes(fl->path);
    if (file_attrs & FILE_ATTR_ANY_LINK) {
      fl++;
      continue;
    }

    char file[FILE_MAX];
    BLI_split_dirfile(fl->path, NULL, file, 0, sizeof(file));

    bool is_dir = BLI_is_dir(fl->path);
    if (is_dir && !FILENAME_IS_CURRPAR(file)) {
      char subpath[FILE_MAX];
      BLI_strncpy(subpath, fl->path, sizeof(subpath));
      BLI_path_slash_ensure(subpath);
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
  BLI_filelist_free(filelist, nbr);
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

static bool seq_disk_cache_enforce_limits(SeqDiskCache *disk_cache)
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
 * <cache dir>/<project name>/<scene name>-<timestamp>/<seq name>/DCACHE_FNAME_FORMAT
 */

static void seq_disk_cache_get_project_dir(SeqDiskCache *disk_cache, char *path, size_t path_len)
{
  char main_name[FILE_MAX];
  BLI_split_file_part(BKE_main_blendfile_path(disk_cache->bmain), main_name, sizeof(main_name));
  BLI_strncpy(path, seq_disk_cache_base_dir(), path_len);
  BLI_path_append(path, path_len, main_name);
}

static void seq_disk_cache_get_dir(
    SeqDiskCache *disk_cache, Scene *scene, Sequence *seq, char *path, size_t path_len)
{
  char scene_name[MAX_ID_NAME + 22]; /* + -%PRId64 */
  char seq_name[SEQ_NAME_MAXSTR];
  char project_dir[FILE_MAX];

  seq_disk_cache_get_project_dir(disk_cache, project_dir, sizeof(project_dir));
  sprintf(scene_name, "%s-%" PRId64, scene->id.name, disk_cache->timestamp);
  BLI_strncpy(seq_name, seq->name, sizeof(seq_name));
  BLI_filename_make_safe(scene_name);
  BLI_filename_make_safe(seq_name);
  BLI_strncpy(path, project_dir, path_len);
  BLI_path_append(path, path_len, scene_name);
  BLI_path_append(path, path_len, seq_name);
}

static void seq_disk_cache_get_file_path(SeqDiskCache *disk_cache,
                                         SeqCacheKey *key,
                                         char *path,
                                         size_t path_len)
{
  seq_disk_cache_get_dir(disk_cache, key->context.scene, key->seq, path, path_len);
  int frameno = (int)key->frame_index / DCACHE_IMAGES_PER_FILE;
  char cache_filename[FILE_MAXFILE];
  sprintf(cache_filename,
          DCACHE_FNAME_FORMAT,
          key->type,
          key->context.rectx,
          key->context.recty,
          key->context.preview_render_size,
          key->context.view_id,
          frameno);

  BLI_path_append(path, path_len, cache_filename);
}

static void seq_disk_cache_create_version_file(char *path)
{
  BLI_make_existing_file(path);

  FILE *file = BLI_fopen(path, "w");
  if (file) {
    fprintf(file, "%d", DCACHE_CURRENT_VERSION);
    fclose(file);
  }
}

static void seq_disk_cache_handle_versioning(SeqDiskCache *disk_cache)
{
  char path[FILE_MAX];
  char path_version_file[FILE_MAX];
  int version = 0;

  seq_disk_cache_get_project_dir(disk_cache, path, sizeof(path));
  BLI_strncpy(path_version_file, path, sizeof(path_version_file));
  BLI_path_append(path_version_file, sizeof(path_version_file), "cache_version");

  if (BLI_exists(path)) {
    FILE *file = BLI_fopen(path_version_file, "r");

    if (file) {
      const int num_items_read = fscanf(file, "%d", &version);
      if (num_items_read == 0) {
        version = -1;
      }
      fclose(file);
    }

    if (version != DCACHE_CURRENT_VERSION) {
      BLI_delete(path, false, true);
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
  BLI_path_slash_ensure(cache_dir);

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

static void seq_disk_cache_invalidate(Scene *scene,
                                      Sequence *seq,
                                      Sequence *seq_changed,
                                      int invalidate_types)
{
  int start;
  int end;
  SeqDiskCache *disk_cache = scene->ed->cache->disk_cache;

  BLI_mutex_lock(&disk_cache->read_write_mutex);

  start = seq_changed->startdisp - DCACHE_IMAGES_PER_FILE;
  end = seq_changed->enddisp;

  seq_disk_cache_delete_invalid_files(disk_cache, scene, seq, invalidate_types, start, end);

  BLI_mutex_unlock(&disk_cache->read_write_mutex);
}

static size_t deflate_imbuf_to_file(ImBuf *ibuf,
                                    FILE *file,
                                    int level,
                                    DiskCacheHeaderEntry *header_entry)
{
  if (ibuf->rect) {
    return BLI_gzip_mem_to_file_at_pos(
        ibuf->rect, header_entry->size_raw, file, header_entry->offset, level);
  }

  return BLI_gzip_mem_to_file_at_pos(
      ibuf->rect_float, header_entry->size_raw, file, header_entry->offset, level);
}

static size_t inflate_file_to_imbuf(ImBuf *ibuf, FILE *file, DiskCacheHeaderEntry *header_entry)
{
  if (ibuf->rect) {
    return BLI_ungzip_file_to_mem_at_pos(
        ibuf->rect, header_entry->size_raw, file, header_entry->offset);
  }

  return BLI_ungzip_file_to_mem_at_pos(
      ibuf->rect_float, header_entry->size_raw, file, header_entry->offset);
}

static bool seq_disk_cache_read_header(FILE *file, DiskCacheHeader *header)
{
  fseek(file, 0, 0);
  const size_t num_items_read = fread(header, sizeof(*header), 1, file);
  if (num_items_read < 1) {
    BLI_assert(!"unable to read disk cache header");
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
  fseek(file, 0, 0);
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

static bool seq_disk_cache_write_file(SeqDiskCache *disk_cache, SeqCacheKey *key, ImBuf *ibuf)
{
  char path[FILE_MAX];

  seq_disk_cache_get_file_path(disk_cache, key, path, sizeof(path));
  BLI_make_existing_file(path);

  FILE *file = BLI_fopen(path, "rb+");
  if (!file) {
    file = BLI_fopen(path, "wb+");
    if (!file) {
      return false;
    }
    seq_disk_cache_add_file_to_list(disk_cache, path);
  }

  DiskCacheFile *cache_file = seq_disk_cache_get_file_entry_by_path(disk_cache, path);
  DiskCacheHeader header;
  memset(&header, 0, sizeof(header));
  /* #BLI_make_existing_file() above may create an empty file. This is fine, don't attempt reading
   * the header in that case. */
  if (cache_file->fstat.st_size != 0 && !seq_disk_cache_read_header(file, &header)) {
    fclose(file);
    seq_disk_cache_delete_file(disk_cache, cache_file);
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
    seq_disk_cache_update_file(disk_cache, path);
    fclose(file);

    return true;
  }

  return false;
}

static ImBuf *seq_disk_cache_read_file(SeqDiskCache *disk_cache, SeqCacheKey *key)
{
  char path[FILE_MAX];
  DiskCacheHeader header;

  seq_disk_cache_get_file_path(disk_cache, key, path, sizeof(path));
  BLI_make_existing_file(path);

  FILE *file = BLI_fopen(path, "rb");
  if (!file) {
    return NULL;
  }

  if (!seq_disk_cache_read_header(file, &header)) {
    fclose(file);
    return NULL;
  }
  int entry_index = seq_disk_cache_get_header_entry(key, &header);

  /* Item not found. */
  if (entry_index < 0) {
    fclose(file);
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
    return NULL;
  }

  size_t bytes_read = inflate_file_to_imbuf(ibuf, file, &header.entry[entry_index]);

  /* Sanity check. */
  if (bytes_read != expected_size) {
    fclose(file);
    IMB_freeImBuf(ibuf);
    return NULL;
  }
  BLI_file_touch(path);
  seq_disk_cache_update_file(disk_cache, path);
  fclose(file);

  return ibuf;
}

#undef DCACHE_FNAME_FORMAT
#undef DCACHE_IMAGES_PER_FILE
#undef COLORSPACE_NAME_MAX
#undef DCACHE_CURRENT_VERSION

static bool seq_cmp_render_data(const SeqRenderData *a, const SeqRenderData *b)
{
  return ((a->preview_render_size != b->preview_render_size) || (a->rectx != b->rectx) ||
          (a->recty != b->recty) || (a->bmain != b->bmain) || (a->scene != b->scene) ||
          (a->motion_blur_shutter != b->motion_blur_shutter) ||
          (a->motion_blur_samples != b->motion_blur_samples) ||
          (a->scene->r.views_format != b->scene->r.views_format) || (a->view_id != b->view_id));
}

static unsigned int seq_hash_render_data(const SeqRenderData *a)
{
  unsigned int rval = a->rectx + a->recty;

  rval ^= a->preview_render_size;
  rval ^= ((intptr_t)a->bmain) << 6;
  rval ^= ((intptr_t)a->scene) << 6;
  rval ^= (int)(a->motion_blur_shutter * 100.0f) << 10;
  rval ^= a->motion_blur_samples << 16;
  rval ^= ((a->scene->r.views_format * 2) + a->view_id) << 24;

  return rval;
}

static unsigned int seq_cache_hashhash(const void *key_)
{
  const SeqCacheKey *key = key_;
  unsigned int rval = seq_hash_render_data(&key->context);

  rval ^= *(const unsigned int *)&key->frame_index;
  rval += key->type;
  rval ^= ((intptr_t)key->seq) << 6;

  return rval;
}

static bool seq_cache_hashcmp(const void *a_, const void *b_)
{
  const SeqCacheKey *a = a_;
  const SeqCacheKey *b = b_;

  return ((a->seq != b->seq) || (a->frame_index != b->frame_index) || (a->type != b->type) ||
          seq_cmp_render_data(&a->context, &b->context));
}

static float seq_cache_timeline_frame_to_frame_index(Sequence *seq, float timeline_frame, int type)
{
  /* With raw images, map timeline_frame to strip input media frame range. This means that static
   * images or extended frame range of movies will only generate one cache entry. No special
   * treatment in converting frame index to timeline_frame is needed. */
  if (type == SEQ_CACHE_STORE_RAW) {
    return seq_give_frame_index(seq, timeline_frame);
  }

  return timeline_frame - seq->start;
}

static float seq_cache_frame_index_to_timeline_frame(Sequence *seq, float frame_index)
{
  return frame_index + seq->start;
}

static SeqCache *seq_cache_get_from_scene(Scene *scene)
{
  if (scene && scene->ed && scene->ed->cache) {
    return scene->ed->cache;
  }

  return NULL;
}

static void seq_cache_lock(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (cache) {
    BLI_mutex_lock(&cache->iterator_mutex);
  }
}

static void seq_cache_unlock(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (cache) {
    BLI_mutex_unlock(&cache->iterator_mutex);
  }
}

static size_t seq_cache_get_mem_total(void)
{
  return ((size_t)U.memcachelimit) * 1024 * 1024;
}

static void seq_cache_keyfree(void *val)
{
  SeqCacheKey *key = val;
  BLI_mempool_free(key->cache_owner->keys_pool, key);
}

static void seq_cache_valfree(void *val)
{
  SeqCacheItem *item = (SeqCacheItem *)val;

  if (item->ibuf) {
    IMB_freeImBuf(item->ibuf);
  }

  BLI_mempool_free(item->cache_owner->items_pool, item);
}

static int get_stored_types_flag(Scene *scene, SeqCacheKey *key)
{
  int flag;
  if (key->seq->cache_flag & SEQ_CACHE_OVERRIDE) {
    flag = key->seq->cache_flag;
  }
  else {
    flag = scene->ed->cache_flag;
  }

  /* SEQ_CACHE_STORE_FINAL_OUT can not be overridden by strip cache */
  flag |= (scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT);

  return flag;
}

static void seq_cache_put_ex(Scene *scene, SeqCacheKey *key, ImBuf *ibuf)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheItem *item;
  item = BLI_mempool_alloc(cache->items_pool);
  item->cache_owner = cache;
  item->ibuf = ibuf;

  const int stored_types_flag = get_stored_types_flag(scene, key);

  /* Item stored for later use. */
  if (stored_types_flag & key->type) {
    key->is_temp_cache = false;
    key->link_prev = cache->last_key;
  }

  /* Store pointer to last cached key. */
  SeqCacheKey *temp_last_key = cache->last_key;

  if (BLI_ghash_reinsert(cache->hash, key, item, seq_cache_keyfree, seq_cache_valfree)) {
    IMB_refImBuf(ibuf);

    if (!key->is_temp_cache) {
      cache->last_key = key;
    }
  }

  /* Set last_key's reference to this key so we can look up chain backwards.
   * Item is already put in cache, so cache->last_key points to current key.
   */
  if (!key->is_temp_cache && temp_last_key) {
    temp_last_key->link_next = cache->last_key;
  }

  /* Reset linking. */
  if (key->type == SEQ_CACHE_STORE_FINAL_OUT) {
    cache->last_key = NULL;
  }
}

static ImBuf *seq_cache_get_ex(SeqCache *cache, SeqCacheKey *key)
{
  SeqCacheItem *item = BLI_ghash_lookup(cache->hash, key);

  if (item && item->ibuf) {
    IMB_refImBuf(item->ibuf);

    return item->ibuf;
  }

  return NULL;
}

static void seq_cache_relink_keys(SeqCacheKey *link_next, SeqCacheKey *link_prev)
{
  if (link_next) {
    link_next->link_prev = link_prev;
  }
  if (link_prev) {
    link_prev->link_next = link_next;
  }
}

/* Choose a key out of 2 candidates(leftmost and rightmost items)
 * to recycle based on currently used strategy */
static SeqCacheKey *seq_cache_choose_key(Scene *scene, SeqCacheKey *lkey, SeqCacheKey *rkey)
{
  SeqCacheKey *finalkey = NULL;

  /* Ideally, cache would not need to check the state of prefetching task
   * that is tricky to do however, because prefetch would need to know,
   * if a key, that is about to be created would be removed by itself.
   *
   * This can happen because only FINAL_OUT item insertion will trigger recycling
   * but that is also the point, where prefetch can be suspended.
   *
   * We could use temp cache as a shield and later make it a non-temporary entry,
   * but it is not worth of increasing system complexity.
   */
  if (scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE && seq_prefetch_job_is_running(scene)) {
    int pfjob_start, pfjob_end;
    seq_prefetch_get_time_range(scene, &pfjob_start, &pfjob_end);

    if (lkey) {
      if (lkey->timeline_frame < pfjob_start || lkey->timeline_frame > pfjob_end) {
        return lkey;
      }
    }

    if (rkey) {
      if (rkey->timeline_frame < pfjob_start || rkey->timeline_frame > pfjob_end) {
        return rkey;
      }
    }

    return NULL;
  }

  if (rkey && lkey) {
    if (lkey->timeline_frame > rkey->timeline_frame) {
      SeqCacheKey *swapkey = lkey;
      lkey = rkey;
      rkey = swapkey;
    }

    int l_diff = scene->r.cfra - lkey->timeline_frame;
    int r_diff = rkey->timeline_frame - scene->r.cfra;

    if (l_diff > r_diff) {
      finalkey = lkey;
    }
    else {
      finalkey = rkey;
    }
  }
  else {
    if (lkey) {
      finalkey = lkey;
    }
    else {
      finalkey = rkey;
    }
  }
  return finalkey;
}

static void seq_cache_recycle_linked(Scene *scene, SeqCacheKey *base)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  SeqCacheKey *next = base->link_next;

  while (base) {
    if (!BLI_ghash_haskey(cache->hash, base)) {
      break; /* Key has already been removed from cache. */
    }

    SeqCacheKey *prev = base->link_prev;
    if (prev != NULL && prev->link_next != base) {
      /* Key has been removed and replaced and doesn't belong to this chain anymore. */
      base->link_prev = NULL;
      break;
    }

    BLI_ghash_remove(cache->hash, base, seq_cache_keyfree, seq_cache_valfree);
    base = prev;
  }

  base = next;
  while (base) {
    if (!BLI_ghash_haskey(cache->hash, base)) {
      break; /* Key has already been removed from cache. */
    }

    next = base->link_next;
    if (next != NULL && next->link_prev != base) {
      /* Key has been removed and replaced and doesn't belong to this chain anymore. */
      base->link_next = NULL;
      break;
    }

    BLI_ghash_remove(cache->hash, base, seq_cache_keyfree, seq_cache_valfree);
    base = next;
  }
}

static SeqCacheKey *seq_cache_get_item_for_removal(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheKey *finalkey = NULL;
  /* Leftmost key. */
  SeqCacheKey *lkey = NULL;
  /* Rightmost key. */
  SeqCacheKey *rkey = NULL;
  SeqCacheKey *key = NULL;

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  int total_count = 0;

  while (!BLI_ghashIterator_done(&gh_iter)) {
    key = BLI_ghashIterator_getKey(&gh_iter);
    SeqCacheItem *item = BLI_ghashIterator_getValue(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    /* This shouldn't happen, but better be safe than sorry. */
    if (!item->ibuf) {
      seq_cache_recycle_linked(scene, key);
      /* Can not continue iterating after linked remove. */
      BLI_ghashIterator_init(&gh_iter, cache->hash);
      continue;
    }

    if (key->is_temp_cache || key->link_next != NULL) {
      continue;
    }

    total_count++;

    if (lkey) {
      if (key->timeline_frame < lkey->timeline_frame) {
        lkey = key;
      }
    }
    else {
      lkey = key;
    }
    if (rkey) {
      if (key->timeline_frame > rkey->timeline_frame) {
        rkey = key;
      }
    }
    else {
      rkey = key;
    }
  }

  finalkey = seq_cache_choose_key(scene, lkey, rkey);

  return finalkey;
}

/* Find only "base" keys.
 * Sources(other types) for a frame must be freed all at once.
 */
bool seq_cache_recycle_item(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return false;
  }

  seq_cache_lock(scene);

  while (seq_cache_is_full()) {
    SeqCacheKey *finalkey = seq_cache_get_item_for_removal(scene);

    if (finalkey) {
      seq_cache_recycle_linked(scene, finalkey);
    }
    else {
      seq_cache_unlock(scene);
      return false;
    }
  }
  seq_cache_unlock(scene);
  return true;
}

static void seq_cache_set_temp_cache_linked(Scene *scene, SeqCacheKey *base)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (!cache || !base) {
    return;
  }

  SeqCacheKey *next = base->link_next;

  while (base) {
    SeqCacheKey *prev = base->link_prev;
    base->is_temp_cache = true;
    base = prev;
  }

  base = next;
  while (base) {
    next = base->link_next;
    base->is_temp_cache = true;
    base = next;
  }
}

static void seq_disk_cache_create(Main *bmain, Scene *scene)
{
  BLI_mutex_lock(&cache_create_lock);
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (cache == NULL) {
    return;
  }

  if (cache->disk_cache != NULL) {
    return;
  }

  cache->disk_cache = MEM_callocN(sizeof(SeqDiskCache), "SeqDiskCache");
  cache->disk_cache->bmain = bmain;
  BLI_mutex_init(&cache->disk_cache->read_write_mutex);
  seq_disk_cache_handle_versioning(cache->disk_cache);
  seq_disk_cache_get_files(cache->disk_cache, seq_disk_cache_base_dir());
  cache->disk_cache->timestamp = scene->ed->disk_cache_timestamp;
  BLI_mutex_unlock(&cache_create_lock);
}

static void seq_cache_create(Main *bmain, Scene *scene)
{
  BLI_mutex_lock(&cache_create_lock);
  if (scene->ed->cache == NULL) {
    SeqCache *cache = MEM_callocN(sizeof(SeqCache), "SeqCache");
    cache->keys_pool = BLI_mempool_create(sizeof(SeqCacheKey), 0, 64, BLI_MEMPOOL_NOP);
    cache->items_pool = BLI_mempool_create(sizeof(SeqCacheItem), 0, 64, BLI_MEMPOOL_NOP);
    cache->hash = BLI_ghash_new(seq_cache_hashhash, seq_cache_hashcmp, "SeqCache hash");
    cache->last_key = NULL;
    cache->bmain = bmain;
    BLI_mutex_init(&cache->iterator_mutex);
    scene->ed->cache = cache;

    if (scene->ed->disk_cache_timestamp == 0) {
      scene->ed->disk_cache_timestamp = time(NULL);
    }
  }
  BLI_mutex_unlock(&cache_create_lock);
}

static void seq_cache_populate_key(SeqCacheKey *key,
                                   const SeqRenderData *context,
                                   Sequence *seq,
                                   const float timeline_frame,
                                   const int type)
{
  key->cache_owner = seq_cache_get_from_scene(context->scene);
  key->seq = seq;
  key->context = *context;
  key->frame_index = seq_cache_timeline_frame_to_frame_index(seq, timeline_frame, type);
  key->timeline_frame = timeline_frame;
  key->type = type;
  key->link_prev = NULL;
  key->link_next = NULL;
  key->is_temp_cache = true;
  key->task_id = context->task_id;
}

static SeqCacheKey *seq_cache_allocate_key(SeqCache *cache,
                                           const SeqRenderData *context,
                                           Sequence *seq,
                                           const float timeline_frame,
                                           const int type)
{
  SeqCacheKey *key = BLI_mempool_alloc(cache->keys_pool);
  seq_cache_populate_key(key, context, seq, timeline_frame, type);
  return key;
}

/* ***************************** API ****************************** */

void seq_cache_free_temp_cache(Scene *scene, short id, int timeline_frame)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    if (key->is_temp_cache && key->task_id == id) {
      /* Use frame_index here to avoid freeing raw images if they are used for multiple frames. */
      float frame_index = seq_cache_timeline_frame_to_frame_index(
          key->seq, timeline_frame, key->type);
      if (frame_index != key->frame_index || timeline_frame > key->seq->enddisp ||
          timeline_frame < key->seq->startdisp) {
        BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
      }
    }
  }
  seq_cache_unlock(scene);
}

void seq_cache_destruct(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  BLI_ghash_free(cache->hash, seq_cache_keyfree, seq_cache_valfree);
  BLI_mempool_destroy(cache->keys_pool);
  BLI_mempool_destroy(cache->items_pool);
  BLI_mutex_end(&cache->iterator_mutex);

  if (cache->disk_cache != NULL) {
    BLI_freelistN(&cache->disk_cache->files);
    BLI_mutex_end(&cache->disk_cache->read_write_mutex);
    MEM_freeN(cache->disk_cache);
  }

  MEM_freeN(cache);
  scene->ed->cache = NULL;
}

void seq_cache_cleanup_all(Main *bmain)
{
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    SEQ_cache_cleanup(scene);
  }
}
void SEQ_cache_cleanup(Scene *scene)
{
  SEQ_prefetch_stop(scene);

  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);

    BLI_ghashIterator_step(&gh_iter);
    BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
  }
  cache->last_key = NULL;
  seq_cache_unlock(scene);
}

void seq_cache_cleanup_sequence(Scene *scene,
                                Sequence *seq,
                                Sequence *seq_changed,
                                int invalidate_types,
                                bool force_seq_changed_range)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  if (seq_disk_cache_is_enabled(cache->bmain) && cache->disk_cache != NULL) {
    seq_disk_cache_invalidate(scene, seq, seq_changed, invalidate_types);
  }

  seq_cache_lock(scene);

  int range_start = seq_changed->startdisp;
  int range_end = seq_changed->enddisp;

  if (!force_seq_changed_range) {
    if (seq->startdisp > range_start) {
      range_start = seq->startdisp;
    }

    if (seq->enddisp < range_end) {
      range_end = seq->enddisp;
    }
  }

  int invalidate_composite = invalidate_types & SEQ_CACHE_STORE_FINAL_OUT;
  int invalidate_source = invalidate_types & (SEQ_CACHE_STORE_RAW | SEQ_CACHE_STORE_PREPROCESSED |
                                              SEQ_CACHE_STORE_COMPOSITE);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    /* Clean all final and composite in intersection of seq and seq_changed. */
    if (key->type & invalidate_composite && key->timeline_frame >= range_start &&
        key->timeline_frame <= range_end) {
      if (key->link_next || key->link_prev) {
        seq_cache_relink_keys(key->link_next, key->link_prev);
      }

      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
    }

    if (key->type & invalidate_source && key->seq == seq &&
        key->timeline_frame >= seq_changed->startdisp &&
        key->timeline_frame <= seq_changed->enddisp) {
      if (key->link_next || key->link_prev) {
        seq_cache_relink_keys(key->link_next, key->link_prev);
      }

      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
    }
  }
  cache->last_key = NULL;
  seq_cache_unlock(scene);
}

struct ImBuf *seq_cache_get(const SeqRenderData *context,
                            Sequence *seq,
                            float timeline_frame,
                            int type)
{

  if (context->skip_cache || context->is_proxy_render || !seq) {
    return NULL;
  }

  Scene *scene = context->scene;

  if (context->is_prefetch_render) {
    context = seq_prefetch_get_original_context(context);
    scene = context->scene;
    seq = seq_prefetch_get_original_sequence(seq, scene);
  }

  if (!seq) {
    return NULL;
  }

  if (!scene->ed->cache) {
    seq_cache_create(context->bmain, scene);
  }

  seq_cache_lock(scene);
  SeqCache *cache = seq_cache_get_from_scene(scene);
  ImBuf *ibuf = NULL;
  SeqCacheKey key;

  /* Try RAM cache: */
  if (cache && seq) {
    seq_cache_populate_key(&key, context, seq, timeline_frame, type);
    ibuf = seq_cache_get_ex(cache, &key);
  }
  seq_cache_unlock(scene);

  if (ibuf) {
    return ibuf;
  }

  /* Try disk cache: */
  if (seq_disk_cache_is_enabled(context->bmain)) {
    if (cache->disk_cache == NULL) {
      seq_disk_cache_create(context->bmain, context->scene);
    }

    BLI_mutex_lock(&cache->disk_cache->read_write_mutex);
    ibuf = seq_disk_cache_read_file(cache->disk_cache, &key);
    BLI_mutex_unlock(&cache->disk_cache->read_write_mutex);

    if (ibuf == NULL) {
      return NULL;
    }

    /* Store read image in RAM. Only recycle item for final type. */
    if (key.type != SEQ_CACHE_STORE_FINAL_OUT || seq_cache_recycle_item(scene)) {
      SeqCacheKey *new_key = seq_cache_allocate_key(cache, context, seq, timeline_frame, type);
      seq_cache_put_ex(scene, new_key, ibuf);
    }
  }

  return ibuf;
}

bool seq_cache_put_if_possible(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, int type, ImBuf *ibuf)
{
  Scene *scene = context->scene;

  if (context->is_prefetch_render) {
    context = seq_prefetch_get_original_context(context);
    scene = context->scene;
    seq = seq_prefetch_get_original_sequence(seq, scene);
  }

  if (!seq) {
    return false;
  }

  if (seq_cache_recycle_item(scene)) {
    seq_cache_put(context, seq, timeline_frame, type, ibuf);
    return true;
  }

  seq_cache_set_temp_cache_linked(scene, scene->ed->cache->last_key);
  scene->ed->cache->last_key = NULL;
  return false;
}

void seq_cache_put(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, int type, ImBuf *i)
{
  if (i == NULL || context->skip_cache || context->is_proxy_render || !seq) {
    return;
  }

  Scene *scene = context->scene;

  if (context->is_prefetch_render) {
    context = seq_prefetch_get_original_context(context);
    scene = context->scene;
    seq = seq_prefetch_get_original_sequence(seq, scene);
    BLI_assert(seq != NULL);
  }

  /* Prevent reinserting, it breaks cache key linking. */
  ImBuf *test = seq_cache_get(context, seq, timeline_frame, type);
  if (test) {
    IMB_freeImBuf(test);
    return;
  }

  if (!scene->ed->cache) {
    seq_cache_create(context->bmain, scene);
  }

  seq_cache_lock(scene);
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheKey *key = seq_cache_allocate_key(cache, context, seq, timeline_frame, type);
  seq_cache_put_ex(scene, key, i);
  seq_cache_unlock(scene);

  if (!key->is_temp_cache) {
    if (seq_disk_cache_is_enabled(context->bmain)) {
      if (cache->disk_cache == NULL) {
        seq_disk_cache_create(context->bmain, context->scene);
      }

      BLI_mutex_lock(&cache->disk_cache->read_write_mutex);
      seq_disk_cache_write_file(cache->disk_cache, key, i);
      BLI_mutex_unlock(&cache->disk_cache->read_write_mutex);
      seq_disk_cache_enforce_limits(cache->disk_cache);
    }
  }
}

void SEQ_cache_iterate(
    struct Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, struct Sequence *seq, int timeline_frame, int cache_type))
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);
  bool interrupt = callback_init(userdata, BLI_ghash_len(cache->hash));

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);

  while (!BLI_ghashIterator_done(&gh_iter) && !interrupt) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    interrupt = callback_iter(userdata, key->seq, key->timeline_frame, key->type);
  }

  cache->last_key = NULL;
  seq_cache_unlock(scene);
}

bool seq_cache_is_full(void)
{
  return seq_cache_get_mem_total() < MEM_get_memory_in_use();
}
