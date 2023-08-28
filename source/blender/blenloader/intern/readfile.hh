/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 * blenloader readfile private function prototypes.
 */

#pragma once

#include <cstdio> /* Include header using off_t before poisoning it below. */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_filereader.h"
#include "DNA_sdna_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h" /* for eReportType */

#include "BLO_readfile.h"

struct BlendFileData;
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
struct ReportList;
struct UserDef;

enum eFileDataFlag {
  FD_FLAGS_SWITCH_ENDIAN = 1 << 0,
  FD_FLAGS_FILE_POINTSIZE_IS_4 = 1 << 1,
  FD_FLAGS_POINTSIZE_DIFFERS = 1 << 2,
  FD_FLAGS_FILE_OK = 1 << 3,
  FD_FLAGS_IS_MEMFILE = 1 << 4,
  /* XXX Unused in practice (checked once but never set). */
  FD_FLAGS_NOT_MY_LIBMAP = 1 << 5,
};
ENUM_OPERATORS(eFileDataFlag, FD_FLAGS_NOT_MY_LIBMAP)

/* Disallow since it's 32bit on ms-windows. */
#ifdef __GNUC__
#  pragma GCC poison off_t
#endif

struct FileData {
  /** Linked list of BHeadN's. */
  ListBase bhead_list;
  enum eFileDataFlag flags;
  bool is_eof;

  FileReader *file;

  /** Whether we are undoing (< 0) or redoing (> 0), used to choose which 'unchanged' flag to use
   * to detect unchanged data from memfile. */
  int undo_direction; /* eUndoStepDir */

  /** Now only in use for library appending. */
  char relabase[FILE_MAX];

  /** General reading variables. */
  SDNA *filesdna;
  const SDNA *memsdna;
  /** Array of #eSDNA_StructCompare. */
  const char *compflags;
  DNA_ReconstructInfo *reconstruct_info;

  int fileversion;
  /** Used to retrieve ID names from (bhead+1). */
  int id_name_offset;
  /** Used to retrieve asset data from (bhead+1). NOTE: This may not be available in old files,
   * will be -1 then! */
  int id_asset_data_offset;
  /** For do_versions patching. */
  int globalf, fileflags;

  /** Optionally skip some data-blocks when they're not needed. */
  eBLOReadSkip skip_flags;

  /**
   * Tag to apply to all loaded ID data-blocks.
   *
   * \note This is initialized from #LibraryLink_Params.id_tag_extra since passing it as an
   * argument would need an additional argument to be passed around when expanding library data.
   */
  int id_tag_extra;

  OldNewMap *datamap;
  OldNewMap *globmap;

  /**
   * Store mapping from old ID pointers (the values they have in the .blend file) to new ones,
   * typically from value in `bhead->old` to address in memory where the ID was read.
   * Used during library-linking process (see #lib_link_all).
   */
  OldNewMap *libmap;

  OldNewMap *packedmap;
  BLOCacheStorage *cache_storage;

  BHeadSort *bheadmap;
  int tot_bheadmap;

  /** See: #USE_GHASH_BHEAD. */
  GHash *bhead_idname_hash;

  ListBase *mainlist;
  /** Used for undo. */
  ListBase *old_mainlist;
  /**
   * IDMap using UUID's as keys of all the old IDs in the old bmain. Used during undo to find a
   * matching old data when reading a new ID. */
  IDNameLib_Map *old_idmap_uuid;
  /**
   * IDMap using uuids as keys of the IDs read (or moved) in the new main(s).
   *
   * Used during undo to ensure that the ID pointers from the 'no undo' IDs remain valid (these
   * IDs are re-used from old main even if their content is not the same as in the memfile undo
   * step, so they could point e.g. to an ID that does not exist in the newly read undo step).
   *
   * Also used to find current valid pointers (or none) of these 'no undo' IDs existing in
   * read memfile. */
  IDNameLib_Map *new_idmap_uuid;

  BlendFileReadReport *reports;
};

#define SIZEOFBLENDERHEADER 12

/***/
void blo_join_main(ListBase *mainlist);
void blo_split_main(ListBase *mainlist, Main *main);

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath);

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

void blo_make_packed_pointer_map(FileData *fd, Main *oldmain);
/**
 * Set old main packed data to zero if it has been restored
 * this works because freeing old main only happens after this call.
 */
void blo_end_packed_pointer_map(FileData *fd, Main *oldmain);
/**
 * Build a #GSet of old main (we only care about local data here,
 * so we can do that after #blo_split_main() call.
 */
void blo_make_old_idmap_from_main(FileData *fd, Main *bmain);

BHead *blo_read_asset_data_block(FileData *fd, BHead *bhead, AssetMetaData **r_asset_data);

void blo_cache_storage_init(FileData *fd, Main *bmain);
void blo_cache_storage_old_bmain_clear(FileData *fd, Main *bmain_old);
void blo_cache_storage_end(FileData *fd);

void blo_filedata_free(FileData *fd);

BHead *blo_bhead_first(FileData *fd);
BHead *blo_bhead_next(FileData *fd, BHead *thisblock);
BHead *blo_bhead_prev(FileData *fd, BHead *thisblock);

/**
 * Warning! Caller's responsibility to ensure given bhead **is** an ID one!
 */
const char *blo_bhead_id_name(const FileData *fd, const BHead *bhead);
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
void blo_do_versions_cycles(FileData *fd, Library *lib, Main *bmain);

void do_versions_after_linking_250(Main *bmain);
void do_versions_after_linking_260(Main *bmain);
void do_versions_after_linking_270(Main *bmain);
void do_versions_after_linking_280(FileData *fd, Main *bmain);
void do_versions_after_linking_290(FileData *fd, Main *bmain);
void do_versions_after_linking_300(FileData *fd, Main *bmain);
void do_versions_after_linking_400(FileData *fd, Main *bmain);
void do_versions_after_linking_cycles(Main *bmain);

void do_versions_after_setup(Main *new_bmain, BlendFileReadReport *reports);

/**
 * Direct data-blocks with global linking.
 *
 * \note This is rather unfortunate to have to expose this here,
 * but better use that nasty hack in do_version than readfile itself.
 */
void *blo_read_get_new_globaldata_address(FileData *fd, const void *adr);

/* Mark the Main data as invalid (.blend file reading should be aborted ASAP, and the already read
 * data should be discarded). Also add an error report to `fd` including given `message`. */
void blo_readfile_invalidate(FileData *fd, Main *bmain, const char *message);
