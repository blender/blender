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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"
#include "BKE_volume.h"

#include "BLT_translation.h"

#include "DEG_depsgraph_query.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.volume"};

#define VOLUME_FRAME_NONE INT_MAX

#ifdef WITH_OPENVDB
#  include <atomic>
#  include <list>
#  include <mutex>
#  include <unordered_set>

#  include <openvdb/openvdb.h>
#  include <openvdb/points/PointDataGrid.h>

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

struct VolumeFileCache {
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

    /* Unique key: filename + grid name. */
    std::string filepath;
    std::string grid_name;

    /* OpenVDB grid. */
    openvdb::GridBase::Ptr grid;
    /* Has the grid tree been loaded? */
    bool is_loaded;
    /* Error message if an error occured during loading. */
    std::string error_msg;
    /* User counting. */
    int num_metadata_users;
    int num_tree_users;
    /* Mutex for on-demand reading of tree. */
    std::mutex mutex;
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
  VolumeFileCache()
  {
  }

  ~VolumeFileCache()
  {
    assert(cache.size() == 0);
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

    /* Note: pointers to unordered_set values are not invalidated when adding
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
      entry.grid->clear();
      entry.is_loaded = false;
    }
  }

  /* Cache contents */
  typedef std::unordered_set<Entry, EntryHasher, EntryEqual> EntrySet;
  EntrySet cache;
  /* Mutex for multithreaded access. */
  std::mutex mutex;
} GLOBAL_CACHE;

/* VolumeGrid
 *
 * Wrapper around OpenVDB grid. Grids loaded from OpenVDB files are always
 * stored in the global cache. Procedurally generated grids are not. */

struct VolumeGrid {
  VolumeGrid(const VolumeFileCache::Entry &template_entry) : entry(NULL), is_loaded(false)
  {
    entry = GLOBAL_CACHE.add_metadata_user(template_entry);
    vdb = entry->grid;
  }

  VolumeGrid(const openvdb::GridBase::Ptr &vdb) : vdb(vdb), entry(NULL), is_loaded(true)
  {
  }

  VolumeGrid(const VolumeGrid &other)
      : vdb(other.vdb), entry(other.entry), is_loaded(other.is_loaded)
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

  void load(const char *volume_name, const char *filepath)
  {
    /* If already loaded or not file-backed, nothing to do. */
    if (is_loaded || entry == NULL) {
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

    try {
      file.setCopyMaxBytes(0);
      file.open();
      openvdb::GridBase::Ptr vdb_grid = file.readGrid(name());
      entry->grid->setTree(vdb_grid->baseTreePtr());
    }
    catch (const openvdb::IoError &e) {
      entry->error_msg = e.what();
    }

    std::atomic_thread_fence(std::memory_order_release);
    entry->is_loaded = true;
    is_loaded = true;
  }

  void unload(const char *volume_name)
  {
    /* Not loaded or not file-backed, nothing to do. */
    if (!is_loaded || entry == NULL) {
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
    vdb = vdb->copyGridWithNewTree();
    if (entry) {
      GLOBAL_CACHE.remove_user(*entry, is_loaded);
      entry = NULL;
    }
    is_loaded = true;
  }

  void duplicate_reference(const char *volume_name, const char *filepath)
  {
    /* Make a deep copy of the grid and remove any reference to a grid in the
     * file cache. Load file grid into memory first if needed. */
    load(volume_name, filepath);
    /* TODO: avoid deep copy if we are the only user. */
    vdb = vdb->deepCopyGrid();
    if (entry) {
      GLOBAL_CACHE.remove_user(*entry, is_loaded);
      entry = NULL;
    }
    is_loaded = true;
  }

  const char *name() const
  {
    /* Don't use vdb.getName() since it copies the string, we want a pointer to the
     * original so it doesn't get freed out of scope. */
    openvdb::StringMetadata::ConstPtr name_meta = vdb->getMetadata<openvdb::StringMetadata>(
        openvdb::GridBase::META_GRID_NAME);
    return (name_meta) ? name_meta->value().c_str() : "";
  }

  const char *error_message() const
  {
    if (is_loaded && entry && !entry->error_msg.empty()) {
      return entry->error_msg.c_str();
    }
    else {
      return NULL;
    }
  }

  /* OpenVDB grid. */
  openvdb::GridBase::Ptr vdb;
  /* File cache entry. */
  VolumeFileCache::Entry *entry;
  /* Indicates if the tree has been loaded for this grid. Note that vdb.tree()
   * may actually be loaded by another user while this is false. But only after
   * calling load() and is_loaded changes to true is it safe to access. */
  bool is_loaded;
};

/* Volume Grid Vector
 *
 * List of grids contained in a volume datablock. This is runtime-only data,
 * the actual grids are always saved in a VDB file. */

struct VolumeGridVector : public std::list<VolumeGrid> {
  VolumeGridVector()
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

  /* Absolute file path that grids have been loaded from. */
  char filepath[FILE_MAX];
  /* File loading error message. */
  std::string error_msg;
  /* File Metadata. */
  openvdb::MetaMap::Ptr metadata;
  /* Mutex for file loading of grids list. */
  std::mutex mutex;
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

void BKE_volume_init_grids(Volume *volume)
{
#ifdef WITH_OPENVDB
  if (volume->runtime.grids == NULL) {
    volume->runtime.grids = OBJECT_GUARDED_NEW(VolumeGridVector);
  }
#else
  UNUSED_VARS(volume);
#endif
}

void *BKE_volume_add(Main *bmain, const char *name)
{
  Volume *volume = (Volume *)BKE_libblock_alloc(bmain, ID_VO, name, 0);

  volume_init_data(&volume->id);

  return volume;
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
}

Volume *BKE_volume_copy(Main *bmain, const Volume *volume)
{
  Volume *volume_copy;
  BKE_id_copy(bmain, &volume->id, (ID **)&volume_copy);
  return volume_copy;
}

static void volume_make_local(Main *bmain, ID *id, const int flags)
{
  BKE_lib_id_make_local_generic(bmain, id, flags);
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

IDTypeInfo IDType_ID_VO = {
    /* id_code */ ID_VO,
    /* id_filter */ FILTER_ID_VO,
    /* main_listbase_index */ INDEX_ID_VO,
    /* struct_size */ sizeof(Volume),
    /* name */ "Volume",
    /* name_plural */ "volumes",
    /* translation_context */ BLT_I18NCONTEXT_ID_VOLUME,
    /* flags */ 0,

    /* init_data */ volume_init_data,
    /* copy_data */ volume_copy_data,
    /* free_data */ volume_free_data,
    /* make_local */ volume_make_local,
};

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

  /* Important to apply after, else we cant loop on e.g. frames 100 - 110. */
  frame += frame_offset;

  return frame;
}

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

bool BKE_volume_load(Volume *volume, Main *bmain)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;

  if (volume->runtime.frame == VOLUME_FRAME_NONE) {
    /* Skip loading this frame, outside of sequence range. */
    return true;
  }

  if (BKE_volume_is_loaded(volume)) {
    return grids.error_msg.empty();
  }

  /* Double-checked lock. */
  std::lock_guard<std::mutex> lock(grids.mutex);
  if (BKE_volume_is_loaded(volume)) {
    return grids.error_msg.empty();
  }

  /* Get absolute file path at current frame. */
  const char *volume_name = volume->id.name + 2;
  volume_filepath_get(bmain, volume, grids.filepath);

  CLOG_INFO(&LOG, 1, "Volume %s: load %s", volume_name, grids.filepath);

  /* Test if file exists. */
  if (!BLI_exists(grids.filepath)) {
    char filename[FILE_MAX];
    BLI_split_file_part(grids.filepath, filename, sizeof(filename));
    grids.error_msg = filename + std::string(" not found");
    CLOG_INFO(&LOG, 1, "Volume %s: %s", volume_name, grids.error_msg.c_str());
    return false;
  }

  /* Test if file exists. */
  if (!BLI_exists(grids.filepath)) {
    char filename[FILE_MAX];
    BLI_split_file_part(grids.filepath, filename, sizeof(filename));
    grids.error_msg = filename + std::string(" not found");
    CLOG_INFO(&LOG, 1, "Volume %s: %s", volume_name, grids.error_msg.c_str());
    return false;
  }

  /* Open OpenVDB file. */
  openvdb::io::File file(grids.filepath);
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
      VolumeFileCache::Entry template_entry(grids.filepath, vdb_grid);
      grids.emplace_back(template_entry);
    }
  }

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

BoundBox *BKE_volume_boundbox_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);

  if (ob->runtime.bb != NULL && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }

  if (ob->runtime.bb == NULL) {
    Volume *volume = (Volume *)ob->data;

    ob->runtime.bb = (BoundBox *)MEM_callocN(sizeof(BoundBox), "volume boundbox");

    float min[3], max[3];
    bool have_minmax = false;
    INIT_MINMAX(min, max);

    /* TODO: if we know the volume is going to be displayed, it may be good to
     * load it as part of dependency graph evaluation for better threading. We
     * could also share the bounding box computation in the global volume cache. */
    if (BKE_volume_load(volume, G.main)) {
      const int num_grids = BKE_volume_num_grids(volume);

      for (int i = 0; i < num_grids; i++) {
        VolumeGrid *grid = BKE_volume_grid_get(volume, i);
        float grid_min[3], grid_max[3];

        BKE_volume_grid_load(volume, grid);
        if (BKE_volume_grid_bounds(grid, grid_min, grid_max)) {
          DO_MIN(grid_min, min);
          DO_MAX(grid_max, max);
          have_minmax = true;
        }
      }
    }

    if (!have_minmax) {
      min[0] = min[1] = min[2] = -1.0f;
      max[0] = max[1] = max[2] = 1.0f;
    }

    BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);
  }

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
    VolumeGrid *grid = BKE_volume_grid_get(volume, i);
    if (BKE_volume_grid_type(grid) != VOLUME_GRID_POINTS) {
      return false;
    }
  }

  return true;
}

/* Dependency Graph */

static Volume *volume_evaluate_modifiers(struct Depsgraph *UNUSED(depsgraph),
                                         struct Scene *UNUSED(scene),
                                         Object *UNUSED(object),
                                         Volume *volume_input)
{
  return volume_input;
}

void BKE_volume_eval_geometry(struct Depsgraph *depsgraph, Volume *volume)
{
  /* TODO: can we avoid modifier re-evaluation when frame did not change? */
  int frame = volume_sequence_frame(depsgraph, volume);
  if (frame != volume->runtime.frame) {
    BKE_volume_unload(volume);
    volume->runtime.frame = frame;
  }

  /* Flush back to original. */
  if (DEG_is_active(depsgraph)) {
    Volume *volume_orig = (Volume *)DEG_get_original_id(&volume->id);
    volume_orig->runtime.frame = volume->runtime.frame;
  }
}

void BKE_volume_data_update(struct Depsgraph *depsgraph, struct Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  Volume *volume = (Volume *)object->data;
  Volume *volume_eval = volume_evaluate_modifiers(depsgraph, scene, object, volume);

  /* Assign evaluated object. */
  const bool is_owned = (volume != volume_eval);
  BKE_object_eval_assign_data(object, &volume_eval->id, is_owned);
}

void BKE_volume_grids_backup_restore(Volume *volume, VolumeGridVector *grids, const char *filepath)
{
#ifdef WITH_OPENVDB
  /* Restore grids after datablock was re-copied from original by depsgraph,
   * we don't want to load them again if possible. */
  BLI_assert(volume->id.tag & LIB_TAG_COPIED_ON_WRITE);
  BLI_assert(volume->runtime.grids != NULL && grids != NULL);

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
  UNUSED_VARS(volume, grids);
#endif
}

/* Draw Cache */

void (*BKE_volume_batch_cache_dirty_tag_cb)(Volume *volume, int mode) = NULL;
void (*BKE_volume_batch_cache_free_cb)(Volume *volume) = NULL;

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

VolumeGrid *BKE_volume_grid_get(const Volume *volume, int grid_index)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  for (VolumeGrid &grid : grids) {
    if (grid_index-- == 0) {
      return &grid;
    }
  }
  return NULL;
#else
  UNUSED_VARS(volume, grid_index);
  return NULL;
#endif
}

VolumeGrid *BKE_volume_grid_active_get(const Volume *volume)
{
  const int num_grids = BKE_volume_num_grids(volume);
  if (num_grids == 0) {
    return NULL;
  }

  const int index = clamp_i(volume->active_grid, 0, num_grids - 1);
  return BKE_volume_grid_get(volume, index);
}

VolumeGrid *BKE_volume_grid_find(const Volume *volume, const char *name)
{
  int num_grids = BKE_volume_num_grids(volume);
  for (int i = 0; i < num_grids; i++) {
    VolumeGrid *grid = BKE_volume_grid_get(volume, i);
    if (STREQ(BKE_volume_grid_name(grid), name)) {
      return grid;
    }
  }

  return NULL;
}

/* Grid Loading */

bool BKE_volume_grid_load(const Volume *volume, VolumeGrid *grid)
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

void BKE_volume_grid_unload(const Volume *volume, VolumeGrid *grid)
{
#ifdef WITH_OPENVDB
  const char *volume_name = volume->id.name + 2;
  grid->unload(volume_name);
#else
  UNUSED_VARS(grid);
#endif
}

bool BKE_volume_grid_is_loaded(const VolumeGrid *grid)
{
#ifdef WITH_OPENVDB
  return grid->is_loaded;
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

VolumeGridType BKE_volume_grid_type(const VolumeGrid *volume_grid)
{
#ifdef WITH_OPENVDB
  const openvdb::GridBase::Ptr &grid = volume_grid->vdb;

  if (grid->isType<openvdb::FloatGrid>()) {
    return VOLUME_GRID_FLOAT;
  }
  else if (grid->isType<openvdb::Vec3fGrid>()) {
    return VOLUME_GRID_VECTOR_FLOAT;
  }
  else if (grid->isType<openvdb::BoolGrid>()) {
    return VOLUME_GRID_BOOLEAN;
  }
  else if (grid->isType<openvdb::DoubleGrid>()) {
    return VOLUME_GRID_DOUBLE;
  }
  else if (grid->isType<openvdb::Int32Grid>()) {
    return VOLUME_GRID_INT;
  }
  else if (grid->isType<openvdb::Int64Grid>()) {
    return VOLUME_GRID_INT64;
  }
  else if (grid->isType<openvdb::Vec3IGrid>()) {
    return VOLUME_GRID_VECTOR_INT;
  }
  else if (grid->isType<openvdb::Vec3dGrid>()) {
    return VOLUME_GRID_VECTOR_DOUBLE;
  }
  else if (grid->isType<openvdb::StringGrid>()) {
    return VOLUME_GRID_STRING;
  }
  else if (grid->isType<openvdb::MaskGrid>()) {
    return VOLUME_GRID_MASK;
  }
  else if (grid->isType<openvdb::points::PointDataGrid>()) {
    return VOLUME_GRID_POINTS;
  }
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
  const openvdb::GridBase::Ptr &grid = volume_grid->vdb;
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

bool BKE_volume_grid_bounds(const VolumeGrid *volume_grid, float min[3], float max[3])
{
#ifdef WITH_OPENVDB
  /* TODO: we can get this from grid metadata in some cases? */
  const openvdb::GridBase::Ptr &grid = volume_grid->vdb;
  BLI_assert(BKE_volume_grid_is_loaded(volume_grid));

  openvdb::CoordBBox coordbbox;
  if (!grid->baseTree().evalLeafBoundingBox(coordbbox)) {
    INIT_MINMAX(min, max);
    return false;
  }

  openvdb::BBoxd bbox = grid->transform().indexToWorld(coordbbox);
  min[0] = (float)bbox.min().x();
  min[1] = (float)bbox.min().y();
  min[2] = (float)bbox.min().z();
  max[0] = (float)bbox.max().x();
  max[1] = (float)bbox.max().y();
  max[2] = (float)bbox.max().z();
  return true;
#else
  UNUSED_VARS(volume_grid);
  INIT_MINMAX(min, max);
  return false;
#endif
}

/* Volume Editing */

Volume *BKE_volume_new_for_eval(const Volume *volume_src)
{
  Volume *volume_dst = (Volume *)BKE_id_new_nomain(ID_VO, NULL);

  STRNCPY(volume_dst->id.name, volume_src->id.name);
  volume_dst->mat = (Material **)MEM_dupallocN(volume_src->mat);
  volume_dst->totcol = volume_src->totcol;
  BKE_volume_init_grids(volume_dst);

  return volume_dst;
}

Volume *BKE_volume_copy_for_eval(Volume *volume_src, bool reference)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  if (reference) {
    flags |= LIB_ID_COPY_CD_REFERENCE;
  }

  Volume *result;
  BKE_id_copy_ex(NULL, &volume_src->id, (ID **)&result, flags);
  result->filepath[0] = '\0';

  return result;
}

VolumeGrid *BKE_volume_grid_add(Volume *volume, const char *name, VolumeGridType type)
{
#ifdef WITH_OPENVDB
  VolumeGridVector &grids = *volume->runtime.grids;
  BLI_assert(BKE_volume_grid_find(volume, name) == NULL);

  openvdb::GridBase::Ptr vdb_grid;
  switch (type) {
    case VOLUME_GRID_FLOAT:
      vdb_grid = openvdb::FloatGrid::create();
      break;
    case VOLUME_GRID_VECTOR_FLOAT:
      vdb_grid = openvdb::Vec3fGrid::create();
      break;
    case VOLUME_GRID_BOOLEAN:
      vdb_grid = openvdb::BoolGrid::create();
      break;
    case VOLUME_GRID_DOUBLE:
      vdb_grid = openvdb::DoubleGrid::create();
      break;
    case VOLUME_GRID_INT:
      vdb_grid = openvdb::Int32Grid::create();
      break;
    case VOLUME_GRID_INT64:
      vdb_grid = openvdb::Int64Grid::create();
      break;
    case VOLUME_GRID_VECTOR_INT:
      vdb_grid = openvdb::Vec3IGrid::create();
      break;
    case VOLUME_GRID_VECTOR_DOUBLE:
      vdb_grid = openvdb::Vec3dGrid::create();
      break;
    case VOLUME_GRID_STRING:
      vdb_grid = openvdb::StringGrid::create();
      break;
    case VOLUME_GRID_MASK:
      vdb_grid = openvdb::MaskGrid::create();
      break;
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN:
      return NULL;
  }

  vdb_grid->setName(name);
  grids.emplace_back(vdb_grid);
  return &grids.back();
#else
  UNUSED_VARS(volume, name, type);
  return NULL;
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

/* OpenVDB Grid Access */

#ifdef WITH_OPENVDB
openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_metadata(const VolumeGrid *grid)
{
  return grid->vdb;
}

openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_read(const Volume *volume,
                                                             VolumeGrid *grid)
{
  BKE_volume_grid_load(volume, grid);
  return grid->vdb;
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

  return grid->vdb;
}
#endif
