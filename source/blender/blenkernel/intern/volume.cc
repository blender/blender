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
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_fileops.h"
#include "BLI_float3.hh"
#include "BLI_float4x4.hh"
#include "BLI_ghash.h"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_geometry_set.hh"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_volume.h"

#include "BLT_translation.h"

#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

#include "CLG_log.h"

#ifdef WITH_OPENVDB
static CLG_LogRef LOG = {"bke.volume"};
#endif

#define VOLUME_FRAME_NONE INT_MAX

using blender::float3;
using blender::float4x4;
using blender::IndexRange;

#ifdef WITH_OPENVDB
#  include <atomic>
#  include <list>
#  include <mutex>
#  include <unordered_set>

#  include <openvdb/openvdb.h>
#  include <openvdb/points/PointDataGrid.h>
#  include <openvdb/tools/GridTransformer.h>

/* Global Volume File Cache
 *
 * Global cache of grids read from VDB files. This is used for sharing grids
 * between multiple volume datablocks with the same filepath, and sharing grids
 * between original and copy-on-write datablocks created by the depsgraph.
 *
 * There are two types of users. Some datablocks only need the grid metadata,
 * example an original datablock volume showing the list of grids in the
 * properties editor. Other datablocks also need the tree and voxel data, for
 * rendering for example. So, depending on the users the grid in the cache may
 * have a tree or not.
 *
 * When the number of users drops to zero, the grid data is immediately deleted.
 *
 * TODO: also add a cache for OpenVDB files rather than individual grids,
 * so getting the list of grids is also cached.
 * TODO: Further, we could cache openvdb::io::File so that loading a grid
 * does not re-open it every time. But then we have to take care not to run
 * out of file descriptors or prevent other applications from writing to it.
 */

static struct VolumeFileCache {
  /* Cache Entry */
  struct Entry {
    Entry(const std::string &filepath, const openvdb::GridBase::Ptr &grid)
        : filepath(filepath),
          grid_name(grid->getName()),
          grid(grid),
          is_loaded(false),
          num_metadata_users(0),
          num_tree_users(0)
    {
    }

    Entry(const Entry &other)
        : filepath(other.filepath),
          grid_name(other.grid_name),
          grid(other.grid),
          is_loaded(other.is_loaded),
          num_metadata_users(0),
          num_tree_users(0)
    {
    }

    /* Returns the original grid or a simplified version depending on the given #simplify_level. */
    openvdb::GridBase::Ptr simplified_grid(const int simplify_level)
    {
      BLI_assert(simplify_level >= 0);
      if (simplify_level == 0 || !is_loaded) {
        return grid;
      }

      std::lock_guard<std::mutex> lock(mutex);
      return simplified_grids.lookup_or_add_cb(simplify_level, [&]() {
        const float resolution_factor = 1.0f / (1 << simplify_level);
        const VolumeGridType grid_type = BKE_volume_grid_type_openvdb(*grid);
        return BKE_volume_grid_create_with_changed_resolution(grid_type, *grid, resolution_factor);
      });
    }

    /* Unique key: filename + grid name. */
    std::string filepath;
    std::string grid_name;

    /* OpenVDB grid. */
    openvdb::GridBase::Ptr grid;

    /* Simplified versions of #grid. The integer key is the simplification level. */
    blender::Map<int, openvdb::GridBase::Ptr> simplified_grids;

    /* Has the grid tree been loaded? */
    mutable bool is_loaded;
    /* Error message if an error occurred while loading. */
    std::string error_msg;
    /* User counting. */
    int num_metadata_users;
    int num_tree_users;
    /* Mutex for on-demand reading of tree. */
    mutable std::mutex mutex;
  };

  struct EntryHasher {
    std::size_t operator()(const Entry &entry) const
    {
      std::hash<std::string> string_hasher;
      return BLI_ghashutil_combine_hash(string_hasher(entry.filepath),
                                        string_hasher(entry.grid_name));
    }
  };

  struct EntryEqual {
    bool operator()(const Entry &a, const Entry &b) const
    {
      return a.filepath == b.filepath && a.grid_name == b.grid_name;
    }
  };

  /* Cache */
  ~VolumeFileCache()
  {
    BLI_assert(cache.empty());
  }

  Entry *add_metadata_user(const Entry &template_entry)
  {
    std::lock_guard<std::mutex> lock(mutex);
    EntrySet::iterator it = cache.find(template_entry);
    if (it == cache.end()) {
      it = cache.emplace(template_entry).first;
    }

    /* Casting const away is weak, but it's convenient having key and value in one. */
    Entry &entry = (Entry &)*it;
    entry.num_metadata_users++;

    /* NOTE: pointers to unordered_set values are not invalidated when adding
     * or removing other values. */
    return &entry;
  }

  void copy_user(Entry &entry, const bool tree_user)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (tree_user) {
      entry.num_tree_users++;
    }
    else {
      entry.num_metadata_users++;
    }
  }

  void remove_user(Entry &entry, const bool tree_user)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (tree_user) {
      entry.num_tree_users--;
    }
    else {
      entry.num_metadata_users--;
    }
    update_for_remove_user(entry);
  }

  void change_to_tree_user(Entry &entry)
  {
    std::lock_guard<std::mutex> lock(mutex);
    entry.num_tree_users++;
    entry.num_metadata_users--;
    update_for_remove_user(entry);
  }

  void change_to_metadata_user(Entry &entry)
  {
    std::lock_guard<std::mutex> lock(mutex);
    entry.num_metadata_users++;
    entry.num_tree_users--;
    update_for_remove_user(entry);
  }

 protected:
  void update_for_remove_user(Entry &entry)
  {
    if (entry.num_metadata_users + entry.num_tree_users == 0) {
      cache.erase(entry);
    }
    else if (entry.num_tree_users == 0) {
      /* Note we replace the grid rather than clearing, so that if there is
       * any other shared pointer to the grid it will keep the tree. */
      entry.grid = entry.grid->copyGridWithNewTree();
      entry.simplified_grids.clear();
      entry.is_loaded = false;
    }
  }

  /* Cache contents */
  using EntrySet = std::unordered_set<Entry, EntryHasher, EntryEqual>;
  EntrySet cache;
  /* Mutex for multithreaded access. */
  std::mutex mutex;
} GLOBAL_CACHE;

/* VolumeGrid
 *
 * Wrapper around OpenVDB grid. Grids loaded from OpenVDB files are always
 * stored in the global cache. Procedurally generated grids are not. */

struct VolumeGrid {
  VolumeGrid(const VolumeFileCache::Entry &template_entry, const int simplify_level)
      : entry(nullptr), simplify_level(simplify_level), is_loaded(false)
  {
    entry = GLOBAL_CACHE.add_metadata_user(template_entry);
  }

  VolumeGrid(const openvdb::GridBase::Ptr &grid)
      : entry(nullptr), local_grid(grid), is_loaded(true)
  {
  }

  VolumeGrid(const VolumeGrid &other)
      : entry(other.entry),
        simplify_level(other.simplify_level),
        local_grid(other.local_grid),
        is_loaded(other.is_loaded)
  {
    if (entry) {
      GLOBAL_CACHE.copy_user(*entry, is_loaded);
    }
  }

  ~VolumeGrid()
  {
    if (entry) {
      GLOBAL_CACHE.remove_user(*entry, is_loaded);
    }
  }

  void load(const char *volume_name, const char *filepath) const
  {
    /* If already loaded or not file-backed, nothing to do. */
    if (is_loaded || entry == nullptr) {
      return;
    }

    /* Double-checked lock. */
    std::lock_guard<std::mutex> lock(entry->mutex);
    if (is_loaded) {
      return;
    }

    /* Change metadata user to tree user. */
    GLOBAL_CACHE.change_to_tree_user(*entry);

    /* If already loaded by another user, nothing further to do. */
    if (entry->is_loaded) {
      is_loaded = true;
      return;
    }

    /* Load grid from file. */
    CLOG_INFO(&LOG, 1, "Volume %s: load grid '%s'", volume_name, name());

    openvdb::io::File file(filepath);

    /* Isolate file loading since that's potentially multithreaded and we are
     * holding a mutex lock. */
    blender::threading::isolate_task([&] {
      try {
        file.setCopyMaxBytes(0);
        file.open();
        openvdb::GridBase::Ptr vdb_grid = file.readGrid(name());
        entry->grid->setTree(vdb_grid->baseTreePtr());
      }
      catch (const openvdb::IoError &e) {
        entry->error_msg = e.what();
      }
    });

    std::atomic_thread_fence(std::memory_order_release);
    entry->is_loaded = true;
    is_loaded = true;
  }

  void unload(const char *volume_name) const
  {
    /* Not loaded or not file-backed, nothing to do. */
    if (!is_loaded || entry == nullptr) {
      return;
    }

    /* Double-checked lock. */
    std::lock_guard<std::mutex> lock(entry->mutex);
    if (!is_loaded) {
      return;
    }

    CLOG_INFO(&LOG, 1, "Volume %s: unload grid '%s'", volume_name, name());

    /* Change tree user to metadata user. */
    GLOBAL_CACHE.change_to_metadata_user(*entry);

    /* Indicate we no longer have a tree. The actual grid may still
     * have it due to another user. */
    std::atomic_thread_fence(std::memory_order_release);
    is_loaded = false;
  }

  void clear_reference(const char *UNUSED(volume_name))
  {
    /* Clear any reference to a grid in the file cache. */
    local_grid = grid()->copyGridWithNewTree();
    if (entry) {
      GLOBAL_CACHE.remove_user(*entry, is_loaded);
      entry = nullptr;
    }
    is_loaded = true;
  }

  void duplicate_reference(const char *volume_name, const char *filepath)
  {
    /* Make a deep copy of the grid and remove any reference to a grid in the
     * file cache. Load file grid into memory first if needed. */
    load(volume_name, filepath);
    /* TODO: avoid deep copy if we are the only user. */
    local_grid = grid()->deepCopyGrid();
    if (entry) {
      GLOBAL_CACHE.remove_user(*entry, is_loaded);
      entry = nullptr;
    }
    is_loaded = true;
  }

  const char *name() const
  {
    /* Don't use vdb.getName() since it copies the string, we want a pointer to the
     * original so it doesn't get freed out of scope. */
    openvdb::StringMetadata::ConstPtr name_meta =
        main_grid()->getMetadata<openvdb::StringMetadata>(openvdb::GridBase::META_GRID_NAME);
    return (name_meta) ? name_meta->value().c_str() : "";
  }

  const char *error_message() const
  {
    if (is_loaded && entry && !entry->error_msg.empty()) {
      return entry->error_msg.c_str();
    }

    return nullptr;
  }

  bool grid_is_loaded() const
  {
    return is_loaded;
  }

  openvdb::GridBase::Ptr grid() const
  {
    if (entry) {
      return entry->simplified_grid(simplify_level);
    }
    return local_grid;
  }

  void set_simplify_level(const int simplify_level)
  {
    BLI_assert(simplify_level >= 0);
    this->simplify_level = simplify_level;
  }

 private:
  const openvdb::GridBase::Ptr &main_grid() const
  {
    return (entry) ? entry->grid : local_grid;
  }

 protected:
  /* File cache entry when grid comes directly from a file and may be shared
   * with other volume datablocks. */
  VolumeFileCache::Entry *entry;
  /* If this volume grid is in the global file cache, we can reference a simplified version of it,
   * instead of the original high resolution grid. */
  int simplify_level = 0;
  /* OpenVDB grid if it's not shared through the file cache. */
  openvdb::GridBase::Ptr local_grid;
  /**
   * Indicates if the tree has been loaded for this grid. Note that vdb.tree()
   * may actually be loaded by another user while this is false. But only after
   * calling load() and is_loaded changes to true is it safe to access.
   *
   * Const write access to this must be protected by `entry->mutex`.
   */
  mutable bool is_loaded;
};

/* Volume Grid Vector
 *
 * List of grids contained in a volume datablock. This is runtime-only data,
 * the actual grids are always saved in a VDB file. */

struct VolumeGridVector : public std::list<VolumeGrid> {
  VolumeGridVector() : metadata(new openvdb::MetaMap())
  {
    filepath[0] = '\0';
  }

  VolumeGridVector(const VolumeGridVector &other)
      : std::list<VolumeGrid>(other), error_msg(other.error_msg), metadata(other.metadata)
  {
    memcpy(filepath, other.filepath, sizeof(filepath));
  }

  bool is_loaded() const
  {
    return filepath[0] != '\0';
  }

  void clear_all()
  {
    std::list<VolumeGrid>::clear();
    filepath[0] = '\0';
    error_msg.clear();
    metadata.reset();
  }

  /* Mutex for file loading of grids list. Const write access to the fields after this must be
   * protected by locking with this mutex. */
  mutable std::mutex mutex;
  /* Absolute file path that grids have been loaded from. */
  char filepath[FILE_MAX];
  /* File loading error message. */
  std::string error_msg;
  /* File Metadata. */
  openvdb::MetaMap::Ptr metadata;
};
#endif

/* Module */

void BKE_volumes_init()
{
#ifdef WITH_OPENVDB
  openvdb::initialize();
#endif
}

/* Volume datablock */

static void volume_init_data(ID *id)
{
  Volume *volume = (Volume *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(volume, id));

  MEMCPY_STRUCT_AFTER(volume, DNA_struct_default_get(Volume), id);

  BKE_volume_init_grids(volume);
}

static void volume_copy_data(Main *UNUSED(bmain),
                             ID *id_dst,
                             const ID *id_src,
                             const int UNUSED(flag))
{
  Volume *volume_dst = (Volume *)id_dst;
  const Volume *volume_src = (const Volume *)id_src;

  if (volume_src->packedfile) {
    volume_dst->packedfile = BKE_packedfile_duplicate(volume_src->packedfile);
  }

  volume_dst->mat = (Material **)MEM_dupallocN(volume_src->mat);
#ifdef WITH_OPENVDB
  if (volume_src->runtime.grids) {
    const VolumeGridVector &grids_src = *(volume_src->runtime.grids);
    volume_dst->runtime.grids = OBJECT_GUARDED_NEW(VolumeGridVector, grids_src);
  }
#endif

  volume_dst->batch_cache = nullptr;
}

static void volume_free_data(ID *id)
{
  Volume *volume = (Volume *)id;
  BKE_animdata_free(&volume->id, false);
  BKE_volume_batch_cache_free(volume);
  MEM_SAFE_FREE(volume->mat);
#ifdef WITH_OPENVDB
  OBJECT_GUARDED_SAFE_DELETE(volume->runtime.grids, VolumeGridVector);
#endif
}

static void volume_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Volume *volume = (Volume *)id;
  for (int i = 0; i < volume->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, volume->mat[i], IDWALK_CB_USER);
  }
}

static void volume_foreach_cache(ID *id,
                                 IDTypeForeachCacheFunctionCallback function_callback,
                                 void *user_data)
{
  Volume *volume = (Volume *)id;
  IDCacheKey key = {
      /* id_session_uuid */ id->session_uuid,
      /*offset_in_ID*/ offsetof(Volume, runtime.grids),
      /* cache_v */ volume->runtime.grids,
  };

  function_callback(id, &key, (void **)&volume->runtime.grids, 0, user_data);
}

static void volume_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Volume *volume = (Volume *)id;
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  volume->runtime.grids = nullptr;

  /* Do not store packed files in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(volume) && !is_undo) {
    volume->packedfile = nullptr;
  }

  /* write LibData */
  BLO_write_id_struct(writer, Volume, id_address, &volume->id);
  BKE_id_blend_write(writer, &volume->id);

  /* direct data */
  BLO_write_pointer_array(writer, volume->totcol, volume->mat);
  if (volume->adt) {
    BKE_animdata_blend_write(writer, volume->adt);
  }

  BKE_packedfile_blend_write(writer, volume->packedfile);
}

static void volume_blend_read_data(BlendDataReader *reader, ID *id)
{
  Volume *volume = (Volume *)id;
  BLO_read_data_address(reader, &volume->adt);
  BKE_animdata_blend_read_data(reader, volume->adt);

  BKE_packedfile_blend_read(reader, &volume->packedfile);
  volume->runtime.frame = 0;

  /* materials */
  BLO_read_pointer_array(reader, (void **)&volume->mat);
}

static void volume_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Volume *volume = (Volume *)id;
  /* Needs to be done *after* cache pointers are restored (call to
   * `foreach_cache`/`blo_cache_storage_entry_restore_in_new`), easier for now to do it in
   * lib_link... */
  BKE_volume_init_grids(volume);

  for (int a = 0; a < volume->totcol; a++) {
    BLO_read_id_address(reader, volume->id.lib, &volume->mat[a]);
  }
}

static void volume_blend_read_expand(BlendExpander *expander, ID *id)
{
  Volume *volume = (Volume *)id;
  for (int a = 0; a < volume->totcol; a++) {
    BLO_expand(expander, volume->mat[a]);
  }
}

IDTypeInfo IDType_ID_VO = {
    /* id_code */ ID_VO,
    /* id_filter */ FILTER_ID_VO,
    /* main_listbase_index */ INDEX_ID_VO,
    /* struct_size */ sizeof(Volume),
    /* name */ "Volume",
    /* name_plural */ "volumes",
    /* translation_context */ BLT_I18NCONTEXT_ID_VOLUME,
    /* flags */ IDTYPE_FLAGS_APPEND_IS_REUSABLE,

    /* init_data */ volume_init_data,
    /* copy_data */ volume_copy_data,
    /* free_data */ volume_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ volume_foreach_id,
    /* foreach_cache */ volume_foreach_cache,
    /* owner_get */ nullptr,

    /* blend_write */ volume_blend_write,
    /* blend_read_data */ volume_blend_read_data,
    /* blend_read_lib */ volume_blend_read_lib,
    /* blend_read_expand */ volume_blend_read_expand,

    /* blend_read_undo_preserve */ nullptr,

    /* lib_override_apply_post */ nullptr,
};

void BKE_volume_init_grids(Volume *volume)
{
#ifdef WITH_OPENVDB
  if (volume->runtime.grids == nullptr) {
    volume->runtime.grids = OBJECT_GUARDED_NEW(VolumeGridVector);
  }
#else
  UNUSED_VARS(volume);
#endif
}

void *BKE_volume_add(Main *bmain, const char *name)
{
  Volume *volume = (Volume *)BKE_id_new(bmain, ID_VO, name);

  return volume;
}

/* Sequence */

static int volume_sequence_frame(const Depsgraph *depsgraph, const Volume *volume)
{
  if (!volume->is_sequence) {
    return 0;
  }

  char filepath[FILE_MAX];
  STRNCPY(filepath, volume->filepath);
  int path_frame, path_digits;
  if (!(volume->is_sequence && BLI_path_frame_get(filepath, &path_frame, &path_digits))) {
    return 0;
  }

  const int scene_frame = DEG_get_ctime(depsgraph);
  const VolumeSequenceMode mode = (VolumeSequenceMode)volume->sequence_mode;
  const int frame_duration = volume->frame_duration;
  const int frame_start = volume->frame_start;
  const int frame_offset = volume->frame_offset;

  if (frame_duration == 0) {
    return VOLUME_FRAME_NONE;
  }

  int frame = scene_frame - frame_start + 1;

  switch (mode) {
    case VOLUME_SEQUENCE_CLIP: {
      if (frame < 1 || frame > frame_duration) {
        return VOLUME_FRAME_NONE;
      }
      break;
    }
    case VOLUME_SEQUENCE_EXTEND: {
      frame = clamp_i(frame, 1, frame_duration);
      break;
    }
    case VOLUME_SEQUENCE_REPEAT: {
      frame = frame % frame_duration;
      if (frame < 0) {
        frame += frame_duration;
      }
      if (frame == 0) {
        frame = frame_duration;
      }
      break;
    }
    case VOLUME_SEQUENCE_PING_PONG: {
      const int pingpong_duration = frame_duration * 2 - 2;
      frame = frame % pingpong_duration;
      if (frame < 0) {
        frame += pingpong_duration;
      }
      if (frame == 0) {
        frame = pingpong_duration;
      }
      if (frame > frame_duration) {
        frame = frame_duration * 2 - frame;
      }
      break;
    }
  }

  /* Important to apply after, else we can't loop on e.g. frames 100 - 110. */
  frame += frame_offset;

  return frame;
}

#ifdef WITH_OPENVDB
static void volume_filepath_get(const Main *bmain, const Volume *volume, char r_filepath[FILE_MAX])
{
  BLI_strncpy(r_filepath, volume->filepath, FILE_MAX);
  BLI_path_abs(r_filepath, ID_BLEND_PATH(bmain, &volume->id));

  int path_frame, path_digits;
  if (volume->is_sequence && BLI_path_frame_get(r_filepath, &path_frame, &path_digits)) {
    char ext[32];
    BLI_path_frame_strip(r_filepath, ext);
    BLI_path_frame(r_filepath, volume->runtime.frame, path_digits);
    BLI_path_extension_ensure(r_filepath, FILE_MAX, ext);
  }
}
#endif

/* File Load */

bool BKE_volume_is_loaded(const Volume *volume)
{
#ifdef WITH_OPENVDB
  /* Test if there is a file to load, or if already loaded. */
  return (volume->filepath[0] == '\0' || volume->runtime.grids->is_loaded());
#else
  UNUSED_VARS(volume);
  return true;
#endif
}

bool BKE_volume_load(const Volume *volume, const Main *bmain)
{
#ifdef WITH_OPENVDB
  const VolumeGridVector &const_grids = *volume->runtime.grids;

  if (volume->runtime.frame == VOLUME_FRAME_NONE) {
    /* Skip loading this frame, outside of sequence range. */
    return true;
  }

  if (BKE_volume_is_loaded(volume)) {
    return const_grids.error_msg.empty();
  }

  /* Double-checked lock. */
  std::lock_guard<std::mutex> lock(const_grids.mutex);
  if (BKE_volume_is_loaded(volume)) {
    return const_grids.error_msg.empty();
  }

  /* Guarded by the lock, we can continue to access the grid vector,
   * adding error messages or a new grid, etc. */
  VolumeGridVector &grids = const_cast<VolumeGridVector &>(const_grids);

  /* Get absolute file path at current frame. */
  const char *volume_name = volume->id.name + 2;
  char filepath[FILE_MAX];
  volume_filepath_get(bmain, volume, filepath);

  CLOG_INFO(&LOG, 1, "Volume %s: load %s", volume_name, filepath);

  /* Test if file exists. */
  if (!BLI_exists(filepath)) {
    char filename[FILE_MAX];
    BLI_split_file_part(filepath, filename, sizeof(filename));
    grids.error_msg = filename + std::string(" not found");
    CLOG_INFO(&LOG, 1, "Volume %s: %s", volume_name, grids.error_msg.c_str());
    return false;
  }

  /* Open OpenVDB file. */
  openvdb::io::File file(filepath);
  openvdb::GridPtrVec vdb_grids;

  try {
    file.setCopyMaxBytes(0);
    file.open();
    vdb_grids = *(file.readAllGridMetadata());
    grids.metadata = file.getMetadata();
  }
  catch (const openvdb::IoError &e) {
    grids.error_msg = e.what();
    CLOG_INFO(&LOG, 1, "Volume %s: %s", volume_name, grids.error_msg.c_str());
  }

  /* Add grids read from file to own vector, filtering out any NULL pointers. */
  for (const openvdb::GridBase::Ptr &vdb_grid : vdb_grids) {
    if (vdb_grid) {
      VolumeFileCache::Entry template_entry(filepath, vdb_grid);
      grids.emplace_back(template_entry, volume->runtime.default_simplify_level);
    }
  }

  BLI_strncpy(grids.filepath, filepath, FILE_MAX);

  return grids.error_msg.empty();
#else
  UNUSED_VARS(bmain, volume);
  return true;
#endif
}

void BKE_volume_unload(Volume *volume)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  if (grids.filepath[0] != '\0') {
    const char *volume_name = volume->id.name + 2;
    CLOG_INFO(&LOG, 1, "Volume %s: unload", volume_name);
    grids.clear_all();
  }
#else
  UNUSED_VARS(volume);
#endif
}

/* File Save */

bool BKE_volume_save(const Volume *volume,
                     const Main *bmain,
                     ReportList *reports,
                     const char *filepath)
{
#ifdef WITH_OPENVDB
  if (!BKE_volume_load(volume, bmain)) {
    BKE_reportf(reports, RPT_ERROR, "Could not load volume for writing");
    return false;
  }

  VolumeGridVector &grids = *volume->runtime.grids;
  openvdb::GridCPtrVec vdb_grids;

  for (VolumeGrid &grid : grids) {
    vdb_grids.push_back(BKE_volume_grid_openvdb_for_read(volume, &grid));
  }

  try {
    openvdb::io::File file(filepath);
    file.write(vdb_grids, *grids.metadata);
    file.close();
  }
  catch (const openvdb::IoError &e) {
    BKE_reportf(reports, RPT_ERROR, "Could not write volume: %s", e.what());
    return false;
  }

  return true;
#else
  UNUSED_VARS(volume, bmain, reports, filepath);
  return false;
#endif
}

bool BKE_volume_min_max(const Volume *volume, float3 &r_min, float3 &r_max)
{
  bool have_minmax = false;
#ifdef WITH_OPENVDB
  /* TODO: if we know the volume is going to be displayed, it may be good to
   * load it as part of dependency graph evaluation for better threading. We
   * could also share the bounding box computation in the global volume cache. */
  if (BKE_volume_load(const_cast<Volume *>(volume), G.main)) {
    for (const int i : IndexRange(BKE_volume_num_grids(volume))) {
      const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, i);
      openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
      float3 grid_min;
      float3 grid_max;
      if (BKE_volume_grid_bounds(grid, grid_min, grid_max)) {
        DO_MIN(grid_min, r_min);
        DO_MAX(grid_max, r_max);
        have_minmax = true;
      }
    }
  }
#else
  UNUSED_VARS(volume, r_min, r_max);
#endif
  return have_minmax;
}

BoundBox *BKE_volume_boundbox_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);

  if (ob->runtime.bb != nullptr && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }

  if (ob->runtime.bb == nullptr) {
    ob->runtime.bb = (BoundBox *)MEM_callocN(sizeof(BoundBox), __func__);
  }

  const Volume *volume = (Volume *)ob->data;

  float3 min, max;
  INIT_MINMAX(min, max);
  if (!BKE_volume_min_max(volume, min, max)) {
    min = float3(-1);
    max = float3(1);
  }

  BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);

  return ob->runtime.bb;
}

bool BKE_volume_is_y_up(const Volume *volume)
{
  /* Simple heuristic for common files to open the right way up. */
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  if (grids.metadata) {
    openvdb::StringMetadata::ConstPtr creator =
        grids.metadata->getMetadata<openvdb::StringMetadata>("creator");
    if (!creator) {
      creator = grids.metadata->getMetadata<openvdb::StringMetadata>("Creator");
    }
    return (creator && creator->str().rfind("Houdini", 0) == 0);
  }
#else
  UNUSED_VARS(volume);
#endif

  return false;
}

bool BKE_volume_is_points_only(const Volume *volume)
{
  int num_grids = BKE_volume_num_grids(volume);
  if (num_grids == 0) {
    return false;
  }

  for (int i = 0; i < num_grids; i++) {
    const VolumeGrid *grid = BKE_volume_grid_get_for_read(volume, i);
    if (BKE_volume_grid_type(grid) != VOLUME_GRID_POINTS) {
      return false;
    }
  }

  return true;
}

/* Dependency Graph */

static void volume_update_simplify_level(Volume *volume, const Depsgraph *depsgraph)
{
#ifdef WITH_OPENVDB
  const int simplify_level = BKE_volume_simplify_level(depsgraph);
  if (volume->runtime.grids) {
    for (VolumeGrid &grid : *volume->runtime.grids) {
      grid.set_simplify_level(simplify_level);
    }
  }
  volume->runtime.default_simplify_level = simplify_level;
#else
  UNUSED_VARS(volume, depsgraph);
#endif
}

static void volume_evaluate_modifiers(struct Depsgraph *depsgraph,
                                      struct Scene *scene,
                                      Object *object,
                                      GeometrySet &geometry_set)
{
  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  ModifierApplyFlag apply_flag = use_render ? MOD_APPLY_RENDER : MOD_APPLY_USECACHE;
  const ModifierEvalContext mectx = {depsgraph, object, apply_flag};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(object, &virtualModifierData);

  /* Evaluate modifiers. */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = (const ModifierTypeInfo *)BKE_modifier_get_info(
        (ModifierType)md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if (mti->modifyGeometrySet) {
      mti->modifyGeometrySet(md, &mectx, &geometry_set);
    }
  }
}

void BKE_volume_eval_geometry(struct Depsgraph *depsgraph, Volume *volume)
{
  volume_update_simplify_level(volume, depsgraph);

  /* TODO: can we avoid modifier re-evaluation when frame did not change? */
  int frame = volume_sequence_frame(depsgraph, volume);
  if (frame != volume->runtime.frame) {
    BKE_volume_unload(volume);
    volume->runtime.frame = frame;
  }

  /* Flush back to original. */
  if (DEG_is_active(depsgraph)) {
    Volume *volume_orig = (Volume *)DEG_get_original_id(&volume->id);
    if (volume_orig->runtime.frame != volume->runtime.frame) {
      BKE_volume_unload(volume_orig);
      volume_orig->runtime.frame = volume->runtime.frame;
    }
  }
}

static Volume *take_volume_ownership_from_geometry_set(GeometrySet &geometry_set)
{
  if (!geometry_set.has<VolumeComponent>()) {
    return nullptr;
  }
  VolumeComponent &volume_component = geometry_set.get_component_for_write<VolumeComponent>();
  Volume *volume = volume_component.release();
  if (volume != nullptr) {
    /* Add back, but only as read-only non-owning component. */
    volume_component.replace(volume, GeometryOwnershipType::ReadOnly);
  }
  else {
    /* The component was empty, we can remove it. */
    geometry_set.remove<VolumeComponent>();
  }
  return volume;
}

void BKE_volume_data_update(struct Depsgraph *depsgraph, struct Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  Volume *volume = (Volume *)object->data;
  GeometrySet geometry_set;
  geometry_set.replace_volume(volume, GeometryOwnershipType::ReadOnly);
  volume_evaluate_modifiers(depsgraph, scene, object, geometry_set);

  Volume *volume_eval = take_volume_ownership_from_geometry_set(geometry_set);

  /* If the geometry set did not contain a volume, we still create an empty one. */
  if (volume_eval == nullptr) {
    volume_eval = BKE_volume_new_for_eval(volume);
  }

  /* Assign evaluated object. */
  const bool eval_is_owned = (volume != volume_eval);
  BKE_object_eval_assign_data(object, &volume_eval->id, eval_is_owned);
  object->runtime.geometry_set_eval = new GeometrySet(std::move(geometry_set));
}

void BKE_volume_grids_backup_restore(Volume *volume, VolumeGridVector *grids, const char *filepath)
{
#ifdef WITH_OPENVDB
  /* Restore grids after datablock was re-copied from original by depsgraph,
   * we don't want to load them again if possible. */
  BLI_assert(volume->id.tag & LIB_TAG_COPIED_ON_WRITE);
  BLI_assert(volume->runtime.grids != nullptr && grids != nullptr);

  if (!grids->is_loaded()) {
    /* No grids loaded in CoW datablock, nothing lost by discarding. */
    OBJECT_GUARDED_DELETE(grids, VolumeGridVector);
  }
  else if (!STREQ(volume->filepath, filepath)) {
    /* Filepath changed, discard grids from CoW datablock. */
    OBJECT_GUARDED_DELETE(grids, VolumeGridVector);
  }
  else {
    /* Keep grids from CoW datablock. We might still unload them a little
     * later in BKE_volume_eval_geometry if the frame changes. */
    OBJECT_GUARDED_DELETE(volume->runtime.grids, VolumeGridVector);
    volume->runtime.grids = grids;
  }
#else
  UNUSED_VARS(volume, grids, filepath);
#endif
}

/* Draw Cache */

void (*BKE_volume_batch_cache_dirty_tag_cb)(Volume *volume, int mode) = nullptr;
void (*BKE_volume_batch_cache_free_cb)(Volume *volume) = nullptr;

void BKE_volume_batch_cache_dirty_tag(Volume *volume, int mode)
{
  if (volume->batch_cache) {
    BKE_volume_batch_cache_dirty_tag_cb(volume, mode);
  }
}

void BKE_volume_batch_cache_free(Volume *volume)
{
  if (volume->batch_cache) {
    BKE_volume_batch_cache_free_cb(volume);
  }
}

/* Grids */

int BKE_volume_num_grids(const Volume *volume)
{
#ifdef WITH_OPENVDB
  return volume->runtime.grids->size();
#else
  UNUSED_VARS(volume);
  return 0;
#endif
}

const char *BKE_volume_grids_error_msg(const Volume *volume)
{
#ifdef WITH_OPENVDB
  return volume->runtime.grids->error_msg.c_str();
#else
  UNUSED_VARS(volume);
  return "";
#endif
}

const char *BKE_volume_grids_frame_filepath(const Volume *volume)
{
#ifdef WITH_OPENVDB
  return volume->runtime.grids->filepath;
#else
  UNUSED_VARS(volume);
  return "";
#endif
}

const VolumeGrid *BKE_volume_grid_get_for_read(const Volume *volume, int grid_index)
{
#ifdef WITH_OPENVDB
  const VolumeGridVector &grids = *volume->runtime.grids;
  for (const VolumeGrid &grid : grids) {
    if (grid_index-- == 0) {
      return &grid;
    }
  }
  return nullptr;
#else
  UNUSED_VARS(volume, grid_index);
  return nullptr;
#endif
}

VolumeGrid *BKE_volume_grid_get_for_write(Volume *volume, int grid_index)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  for (VolumeGrid &grid : grids) {
    if (grid_index-- == 0) {
      return &grid;
    }
  }
  return nullptr;
#else
  UNUSED_VARS(volume, grid_index);
  return nullptr;
#endif
}

const VolumeGrid *BKE_volume_grid_active_get_for_read(const Volume *volume)
{
  const int num_grids = BKE_volume_num_grids(volume);
  if (num_grids == 0) {
    return nullptr;
  }

  const int index = clamp_i(volume->active_grid, 0, num_grids - 1);
  return BKE_volume_grid_get_for_read(volume, index);
}

/* Tries to find a grid with the given name. Make sure that the volume has been loaded. */
const VolumeGrid *BKE_volume_grid_find_for_read(const Volume *volume, const char *name)
{
  int num_grids = BKE_volume_num_grids(volume);
  for (int i = 0; i < num_grids; i++) {
    const VolumeGrid *grid = BKE_volume_grid_get_for_read(volume, i);
    if (STREQ(BKE_volume_grid_name(grid), name)) {
      return grid;
    }
  }

  return nullptr;
}

/* Grid Loading */

bool BKE_volume_grid_load(const Volume *volume, const VolumeGrid *grid)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  const char *volume_name = volume->id.name + 2;
  grid->load(volume_name, grids.filepath);
  const char *error_msg = grid->error_message();
  if (error_msg) {
    grids.error_msg = error_msg;
    return false;
  }
  return true;
#else
  UNUSED_VARS(volume, grid);
  return true;
#endif
}

void BKE_volume_grid_unload(const Volume *volume, const VolumeGrid *grid)
{
#ifdef WITH_OPENVDB
  const char *volume_name = volume->id.name + 2;
  grid->unload(volume_name);
#else
  UNUSED_VARS(volume, grid);
#endif
}

bool BKE_volume_grid_is_loaded(const VolumeGrid *grid)
{
#ifdef WITH_OPENVDB
  return grid->grid_is_loaded();
#else
  UNUSED_VARS(grid);
  return true;
#endif
}

/* Grid Metadata */

const char *BKE_volume_grid_name(const VolumeGrid *volume_grid)
{
#ifdef WITH_OPENVDB
  return volume_grid->name();
#else
  UNUSED_VARS(volume_grid);
  return "density";
#endif
}

#ifdef WITH_OPENVDB
VolumeGridType BKE_volume_grid_type_openvdb(const openvdb::GridBase &grid)
{
  if (grid.isType<openvdb::FloatGrid>()) {
    return VOLUME_GRID_FLOAT;
  }
  if (grid.isType<openvdb::Vec3fGrid>()) {
    return VOLUME_GRID_VECTOR_FLOAT;
  }
  if (grid.isType<openvdb::BoolGrid>()) {
    return VOLUME_GRID_BOOLEAN;
  }
  if (grid.isType<openvdb::DoubleGrid>()) {
    return VOLUME_GRID_DOUBLE;
  }
  if (grid.isType<openvdb::Int32Grid>()) {
    return VOLUME_GRID_INT;
  }
  if (grid.isType<openvdb::Int64Grid>()) {
    return VOLUME_GRID_INT64;
  }
  if (grid.isType<openvdb::Vec3IGrid>()) {
    return VOLUME_GRID_VECTOR_INT;
  }
  if (grid.isType<openvdb::Vec3dGrid>()) {
    return VOLUME_GRID_VECTOR_DOUBLE;
  }
  if (grid.isType<openvdb::StringGrid>()) {
    return VOLUME_GRID_STRING;
  }
  if (grid.isType<openvdb::MaskGrid>()) {
    return VOLUME_GRID_MASK;
  }
  if (grid.isType<openvdb::points::PointDataGrid>()) {
    return VOLUME_GRID_POINTS;
  }
  return VOLUME_GRID_UNKNOWN;
}
#endif

VolumeGridType BKE_volume_grid_type(const VolumeGrid *volume_grid)
{
#ifdef WITH_OPENVDB
  const openvdb::GridBase::Ptr grid = volume_grid->grid();
  return BKE_volume_grid_type_openvdb(*grid);
#else
  UNUSED_VARS(volume_grid);
#endif
  return VOLUME_GRID_UNKNOWN;
}

int BKE_volume_grid_channels(const VolumeGrid *grid)
{
  switch (BKE_volume_grid_type(grid)) {
    case VOLUME_GRID_BOOLEAN:
    case VOLUME_GRID_FLOAT:
    case VOLUME_GRID_DOUBLE:
    case VOLUME_GRID_INT:
    case VOLUME_GRID_INT64:
    case VOLUME_GRID_MASK:
      return 1;
    case VOLUME_GRID_VECTOR_FLOAT:
    case VOLUME_GRID_VECTOR_DOUBLE:
    case VOLUME_GRID_VECTOR_INT:
      return 3;
    case VOLUME_GRID_STRING:
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN:
      return 0;
  }

  return 0;
}

/* Transformation from index space to object space. */
void BKE_volume_grid_transform_matrix(const VolumeGrid *volume_grid, float mat[4][4])
{
#ifdef WITH_OPENVDB
  const openvdb::GridBase::Ptr grid = volume_grid->grid();
  const openvdb::math::Transform &transform = grid->transform();

  /* Perspective not supported for now, getAffineMap() will leave out the
   * perspective part of the transform. */
  openvdb::math::Mat4f matrix = transform.baseMap()->getAffineMap()->getMat4();
  /* Blender column-major and OpenVDB right-multiplication conventions match. */
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      mat[col][row] = matrix(col, row);
    }
  }
#else
  unit_m4(mat);
  UNUSED_VARS(volume_grid);
#endif
}

/* Grid Tree and Voxels */

/* Volume Editing */

Volume *BKE_volume_new_for_eval(const Volume *volume_src)
{
  Volume *volume_dst = (Volume *)BKE_id_new_nomain(ID_VO, nullptr);

  STRNCPY(volume_dst->id.name, volume_src->id.name);
  volume_dst->mat = (Material **)MEM_dupallocN(volume_src->mat);
  volume_dst->totcol = volume_src->totcol;
  volume_dst->render = volume_src->render;
  volume_dst->display = volume_src->display;
  BKE_volume_init_grids(volume_dst);

  return volume_dst;
}

Volume *BKE_volume_copy_for_eval(Volume *volume_src, bool reference)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  if (reference) {
    flags |= LIB_ID_COPY_CD_REFERENCE;
  }

  Volume *result = (Volume *)BKE_id_copy_ex(nullptr, &volume_src->id, nullptr, flags);

  return result;
}

#ifdef WITH_OPENVDB
struct CreateGridOp {
  template<typename GridType> typename openvdb::GridBase::Ptr operator()()
  {
    if constexpr (std::is_same_v<GridType, openvdb::points::PointDataGrid>) {
      return {};
    }
    else {
      return GridType::create();
    }
  }
};
#endif

VolumeGrid *BKE_volume_grid_add(Volume *volume, const char *name, VolumeGridType type)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  BLI_assert(BKE_volume_grid_find_for_read(volume, name) == nullptr);
  BLI_assert(type != VOLUME_GRID_UNKNOWN);

  openvdb::GridBase::Ptr vdb_grid = BKE_volume_grid_type_operation(type, CreateGridOp{});
  if (!vdb_grid) {
    return nullptr;
  }

  vdb_grid->setName(name);
  grids.emplace_back(vdb_grid);
  return &grids.back();
#else
  UNUSED_VARS(volume, name, type);
  return nullptr;
#endif
}

void BKE_volume_grid_remove(Volume *volume, VolumeGrid *grid)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  for (VolumeGridVector::iterator it = grids.begin(); it != grids.end(); it++) {
    if (&*it == grid) {
      grids.erase(it);
      break;
    }
  }
#else
  UNUSED_VARS(volume, grid);
#endif
}

int BKE_volume_simplify_level(const Depsgraph *depsgraph)
{
  if (DEG_get_mode(depsgraph) != DAG_EVAL_RENDER) {
    const Scene *scene = DEG_get_input_scene(depsgraph);
    if (scene->r.mode & R_SIMPLIFY) {
      const float simplify = scene->r.simplify_volumes;
      if (simplify == 0.0f) {
        /* log2 is not defined at 0.0f, so just use some high simplify level. */
        return 16;
      }
      return ceilf(-log2(simplify));
    }
  }
  return 0;
}

float BKE_volume_simplify_factor(const Depsgraph *depsgraph)
{
  if (DEG_get_mode(depsgraph) != DAG_EVAL_RENDER) {
    const Scene *scene = DEG_get_input_scene(depsgraph);
    if (scene->r.mode & R_SIMPLIFY) {
      return scene->r.simplify_volumes;
    }
  }
  return 1.0f;
}

/* OpenVDB Grid Access */

#ifdef WITH_OPENVDB

bool BKE_volume_grid_bounds(openvdb::GridBase::ConstPtr grid, float3 &r_min, float3 &r_max)
{
  /* TODO: we can get this from grid metadata in some cases? */
  openvdb::CoordBBox coordbbox;
  if (!grid->baseTree().evalLeafBoundingBox(coordbbox)) {
    return false;
  }

  openvdb::BBoxd bbox = grid->transform().indexToWorld(coordbbox);

  r_min = float3((float)bbox.min().x(), (float)bbox.min().y(), (float)bbox.min().z());
  r_max = float3((float)bbox.max().x(), (float)bbox.max().y(), (float)bbox.max().z());

  return true;
}

/**
 * Return a new grid pointer with only the metadata and transform changed.
 * This is useful for instances, where there is a separate transform on top of the original
 * grid transform that must be applied for some operations that only take a grid argument.
 */
openvdb::GridBase::ConstPtr BKE_volume_grid_shallow_transform(openvdb::GridBase::ConstPtr grid,
                                                              const blender::float4x4 &transform)
{
  openvdb::math::Transform::Ptr grid_transform = grid->transform().copy();
  grid_transform->postMult(openvdb::Mat4d(((float *)transform.values)));

  /* Create a transformed grid. The underlying tree is shared. */
  return grid->copyGridReplacingTransform(grid_transform);
}

openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_metadata(const VolumeGrid *grid)
{
  return grid->grid();
}

openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_read(const Volume *volume,
                                                             const VolumeGrid *grid)
{
  BKE_volume_grid_load(volume, grid);
  return grid->grid();
}

openvdb::GridBase::Ptr BKE_volume_grid_openvdb_for_write(const Volume *volume,
                                                         VolumeGrid *grid,
                                                         const bool clear)
{
  const char *volume_name = volume->id.name + 2;
  if (clear) {
    grid->clear_reference(volume_name);
  }
  else {
    VolumeGridVector &grids = *volume->runtime.grids;
    grid->duplicate_reference(volume_name, grids.filepath);
  }

  return grid->grid();
}

/* Changing the resolution of a grid. */

/**
 * Returns a grid of the same type as the input, but with more/less resolution. If
 * resolution_factor is 1/2, the resolution on each axis is halved. The transform of the returned
 * grid is adjusted to match the original grid. */
template<typename GridType>
static typename GridType::Ptr create_grid_with_changed_resolution(const GridType &old_grid,
                                                                  const float resolution_factor)
{
  BLI_assert(resolution_factor > 0.0f);

  openvdb::Mat4R xform;
  xform.setToScale(openvdb::Vec3d(resolution_factor));
  openvdb::tools::GridTransformer transformer{xform};

  typename GridType::Ptr new_grid = old_grid.copyWithNewTree();
  transformer.transformGrid<openvdb::tools::BoxSampler>(old_grid, *new_grid);
  new_grid->transform() = old_grid.transform();
  new_grid->transform().preScale(1.0f / resolution_factor);
  new_grid->transform().postTranslate(-new_grid->voxelSize() / 2.0f);
  return new_grid;
}

struct CreateGridWithChangedResolutionOp {
  const openvdb::GridBase &grid;
  const float resolution_factor;

  template<typename GridType> typename openvdb::GridBase::Ptr operator()()
  {
    if constexpr (std::is_same_v<GridType, openvdb::StringGrid>) {
      return {};
    }
    else {
      return create_grid_with_changed_resolution(static_cast<const GridType &>(grid),
                                                 resolution_factor);
    }
  }
};

openvdb::GridBase::Ptr BKE_volume_grid_create_with_changed_resolution(
    const VolumeGridType grid_type,
    const openvdb::GridBase &old_grid,
    const float resolution_factor)
{
  CreateGridWithChangedResolutionOp op{old_grid, resolution_factor};
  return BKE_volume_grid_type_operation(grid_type, op);
}

#endif
