/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 * blenloader readfile private function prototypes.
 */

#pragma once

#include <cstdio> /* IWYU pragma: keep. Include header using off_t before poisoning it below. */
#include <optional>

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_enum_flags.hh"
#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_map.hh"

#include "DNA_sdna_types.h"
#include "DNA_space_types.h"

#include "BLO_core_bhead.hh"
#include "BLO_core_blend_header.hh"
#include "BLO_readfile.hh"

struct BlendFileData;
struct BlendfileLinkAppendContext;
struct BlendFileReadParams;
struct BlendFileReadReport;
struct BLOCacheStorage;
struct BHeadSort;
struct DNA_ReconstructInfo;
struct IDNameLib_Map;
struct Key;
struct Main;
struct MemFile;
struct Object;
struct OldNewMap;
struct UserDef;

/**
 * Store some critical information about the read blend-file.
 */
enum eFileDataFlag {
  FD_FLAGS_SWITCH_ENDIAN = 1 << 0,
  FD_FLAGS_FILE_POINTSIZE_IS_4 = 1 << 1,
  FD_FLAGS_POINTSIZE_DIFFERS = 1 << 2,
  FD_FLAGS_FILE_OK = 1 << 3,
  FD_FLAGS_IS_MEMFILE = 1 << 4,
  /**
   * The Blender file is not compatible with current code, but is still likely a blender file
   * 'from the future'. Improves report to the user.
   */
  FD_FLAGS_FILE_FUTURE = 1 << 5,
  /**
   * The blend-file has IDs with invalid names (either using the 5.0+ new 'long names', or
   * corrupted). I.e. their names have no null char in their first 66 bytes.
   */
  FD_FLAGS_HAS_INVALID_ID_NAMES = 1 << 6,
};
ENUM_OPERATORS(eFileDataFlag)

/* Disallow since it's 32bit on ms-windows. */
#ifdef __GNUC__
#  pragma GCC poison off_t
#endif

/**
 * General data used during a blend-file reading.
 *
 * Note that this data (and its accesses) are absolutely not thread-safe currently. It should never
 * be accessed concurrently.
 */
struct FileData {
  /** Linked list of BHeadN's. */
  ListBase bhead_list = {};
  enum eFileDataFlag flags = eFileDataFlag(0);
  bool is_eof = false;
  BlenderHeader blender_header = {};

  FileReader *file = nullptr;
  std::optional<BLI_stat_t> file_stat;

  /**
   * Whether we are undoing (< 0) or redoing (> 0), used to choose which 'unchanged' flag to use
   * to detect unchanged data from memfile.
   * #eUndoStepDir.
   */
  int undo_direction = 0;

  /** Used for relative paths handling.
   *
   * Typically the actual filepath of the read blend-file, except when recovering
   * save-on-exit/autosave files. In the latter case, it will be the path of the file that
   * generated the auto-saved one being recovered.
   *
   * NOTE: Currently expected to be the same path as #BlendFileData.filepath. */
  char relabase[FILE_MAX] = {};

  /** General reading variables. */
  SDNA *filesdna = nullptr;
  const SDNA *memsdna = nullptr;
  /** Array of #eSDNA_StructCompare. */
  const char *compflags = nullptr;
  DNA_ReconstructInfo *reconstruct_info = nullptr;

  int fileversion = 0;
  /**
   * Unlike the `fileversion` which is read from the header,
   * this is initialized from #read_file_dna.
   */
  int filesubversion = 0;

  /** Used to retrieve ID names from (bhead+1). */
  int id_name_offset = 0;
  /** Used to retrieve asset data from (bhead+1). NOTE: This may not be available in old files,
   * will be -1 then! */
  int id_asset_data_offset = 0;
  int id_flag_offset = 0;
  int id_deep_hash_offset = 0;
  /** For do_versions patching. */
  int globalf = 0;
  int fileflags = 0;

  /** Optionally skip some data-blocks when they're not needed. */
  eBLOReadSkip skip_flags = BLO_READ_SKIP_NONE;

  /**
   * Tag to apply to all loaded ID data-blocks.
   *
   * \note This is initialized from #LibraryLink_Params.id_tag_extra since passing it as an
   * argument would need an additional argument to be passed around when expanding library data.
   */
  int id_tag_extra = 0;

  OldNewMap *datamap = nullptr;
  OldNewMap *globmap = nullptr;
  /** Used to keep track of already loaded packed IDs to avoid loading them multiple times. */
  std::shared_ptr<blender::Map<IDHash, ID *>> id_by_deep_hash;

  /**
   * Store mapping from old ID pointers (the values they have in the .blend file) to new ones,
   * typically from value in `bhead->old` to address in memory where the ID was read.
   * Used during library-linking process (see #lib_link_all).
   */
  OldNewMap *libmap = nullptr;

  BLOCacheStorage *cache_storage = nullptr;

  BHeadSort *bheadmap = nullptr;
  int tot_bheadmap = 0;

  std::optional<blender::Map<blender::StringRefNull, BHead *>> bhead_idname_map;

  /**
   * The root (main, local) Main.
   * The Main that will own Library IDs.
   *
   * When reading libraries, this is typically _not_ the same Main as the one being populated from
   * the content of this filedata, see #fd_bmain.
   */
  Main *bmain = nullptr;
  /** The existing root (main, local) Main, used for undo. */
  Main *old_bmain = nullptr;
  /**
   * The main for the (local) data loaded from this filedata.
   *
   * This is the same as #bmain when opening a blendfile, but not when reading/loading from
   * libraries blendfiles.
   */
  Main *fd_bmain = nullptr;

  /**
   * IDMap using UID's as keys of all the old IDs in the old bmain. Used during undo to find a
   * matching old data when reading a new ID. */
  IDNameLib_Map *old_idmap_uid = nullptr;
  /**
   * IDMap using uids as keys of the IDs read (or moved) in the new main(s).
   *
   * Used during undo to ensure that the ID pointers from the 'no undo' IDs remain valid (these
   * IDs are re-used from old main even if their content is not the same as in the memfile undo
   * step, so they could point e.g. to an ID that does not exist in the newly read undo step).
   *
   * Also used to find current valid pointers (or none) of these 'no undo' IDs existing in
   * read memfile. */
  IDNameLib_Map *new_idmap_uid = nullptr;

  BlendFileReadReport *reports = nullptr;

  /** Opaque handle to the storage system used for non-static allocation strings. */
  void *storage_handle = nullptr;
};

/**
 * Split a single main into a vector of Mains, each containing only IDs from a given library.
 *
 * The vector is accessible in all of the split mains through the shared pointer
 * #Main::split_mains.
 *
 * The first Main of the vector is the same as the given `main`, and contains local IDs.
 *
 * If `do_split_packed_ids` is `false`, packed linked IDs remain in the local (first) main as well.
 */
void blo_split_main(Main *bmain, bool do_split_packed_ids = true);
/**
 * Join the set of split mains (found in given `main` #Main::split_mains vector shared pointer)
 * back into that 'main' main.
 */
void blo_join_main(Main *bmain);

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath) ATTR_NONNULL(1, 2);

/**
 * On each new library added, it now checks for the current #FileData and expands relativeness
 *
 * cannot be called with relative paths anymore!
 */
FileData *blo_filedata_from_file(const char *filepath, BlendFileReadReport *reports);
FileData *blo_filedata_from_memory(const void *mem, int memsize, BlendFileReadReport *reports);
FileData *blo_filedata_from_memfile(MemFile *memfile,
                                    const BlendFileReadParams *params,
                                    BlendFileReadReport *reports);

/**
 * Build a #GSet of old main (we only care about local data here,
 * so we can do that after #blo_split_main() call.
 */
void blo_make_old_idmap_from_main(FileData *fd, Main *bmain) ATTR_NONNULL(1, 2);

BHead *blo_read_asset_data_block(FileData *fd, BHead *bhead, AssetMetaData **r_asset_data)
    ATTR_NONNULL(1, 2);

void blo_cache_storage_init(FileData *fd, Main *bmain) ATTR_NONNULL(1, 2);
void blo_cache_storage_old_bmain_clear(FileData *fd, Main *bmain_old) ATTR_NONNULL(1, 2);
void blo_cache_storage_end(FileData *fd) ATTR_NONNULL(1);

void blo_filedata_free(FileData *fd) ATTR_NONNULL(1);

BHead *blo_bhead_first(FileData *fd) ATTR_NONNULL(1);
BHead *blo_bhead_next(FileData *fd, BHead *thisblock) ATTR_NONNULL(1);
BHead *blo_bhead_prev(FileData *fd, BHead *thisblock) ATTR_NONNULL(1, 2);

/**
 * Warning! Caller's responsibility to ensure given bhead **is** an ID one!
 *
 * Will return `nullptr` if the name is not valid (e.g. because it has no null-char terminator, if
 * it was saved in a version of Blender with higher MAX_ID_NAME value).
 */
const char *blo_bhead_id_name(FileData *fd, const BHead *bhead);
/**
 * Warning! It's the caller's responsibility to ensure that the given bhead **is** an ID one!
 *
 * Returns the ID flag value (or `0` if the blendfile is too old and the offset of the ID::flag
 * member could not be computed).
 */
short blo_bhead_id_flag(const FileData *fd, const BHead *bhead);
/**
 * Warning! Caller's responsibility to ensure given bhead **is** an ID one!
 */
AssetMetaData *blo_bhead_id_asset_data_address(const FileData *fd, const BHead *bhead);

/* do versions stuff */

/**
 * Manipulates SDNA before calling #DNA_struct_get_compareflags,
 * allowing us to rename structs and struct members.
 *
 * - This means older versions of Blender won't have access to this data **USE WITH CARE**.
 * - These changes are applied on file load (run-time), similar to versioning for compatibility.
 *
 * \attention ONLY USE THIS KIND OF VERSIONING WHEN `dna_rename_defs.h` ISN'T SUFFICIENT.
 */
void blo_do_versions_dna(SDNA *sdna, int versionfile, int subversionfile);

void blo_do_versions_oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr);
/**
 * Only library data.
 */
void *blo_do_versions_newlibadr(FileData *fd,
                                ID *self_id,
                                const bool is_linked_only,
                                const void *adr);

/**
 * \note this version patch is intended for versions < 2.52.2,
 * but was initially introduced in 2.27 already.
 */
void blo_do_version_old_trackto_to_constraints(Object *ob);
void blo_do_versions_key_uidgen(Key *key);

/**
 * Patching #UserDef struct and Themes.
 */
void blo_do_versions_userdef(UserDef *userdef);

void blo_do_versions_pre250(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_250(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_260(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_270(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_280(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_290(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_300(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_400(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_410(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_420(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_430(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_440(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_450(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_500(FileData *fd, Library *lib, Main *bmain);
void blo_do_versions_510(FileData *fd, Library *lib, Main *bmain);

void do_versions_after_linking_250(Main *bmain);
void do_versions_after_linking_260(Main *bmain);
void do_versions_after_linking_270(Main *bmain);
void do_versions_after_linking_280(FileData *fd, Main *bmain);
void do_versions_after_linking_290(FileData *fd, Main *bmain);
void do_versions_after_linking_300(FileData *fd, Main *bmain);
void do_versions_after_linking_400(FileData *fd, Main *bmain);
void do_versions_after_linking_410(FileData *fd, Main *bmain);
void do_versions_after_linking_420(FileData *fd, Main *bmain);
void do_versions_after_linking_430(FileData *fd, Main *bmain);
void do_versions_after_linking_440(FileData *fd, Main *bmain);
void do_versions_after_linking_450(FileData *fd, Main *bmain);
void do_versions_after_linking_500(FileData *fd, Main *bmain);
void do_versions_after_linking_510(FileData *fd, Main *bmain);

void do_versions_after_setup(Main *new_bmain,
                             BlendfileLinkAppendContext *lapp_context,
                             BlendFileReadReport *reports);

/**
 * Direct data-blocks with global linking.
 *
 * \note This is rather unfortunate to have to expose this here,
 * but better use that nasty hack in do_version than readfile itself.
 */
void *blo_read_get_new_globaldata_address(FileData *fd, const void *adr) ATTR_NONNULL(1);

/* Mark the Main data as invalid (.blend file reading should be aborted ASAP, and the already read
 * data should be discarded). Also add an error report to `fd` including given `message`. */
void blo_readfile_invalidate(FileData *fd, Main *bmain, const char *message) ATTR_NONNULL(1, 2, 3);
