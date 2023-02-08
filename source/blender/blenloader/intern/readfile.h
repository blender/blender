/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup blenloader
 * blenloader readfile private function prototypes.
 */

#pragma once

#include <stdio.h> /* Include header using off_t before poisoning it below. */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_filereader.h"
#include "DNA_sdna_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h" /* for eReportType */

#ifdef __cplusplus
extern "C" {
#endif

struct BLOCacheStorage;
struct IDNameLib_Map;
struct Key;
struct MemFile;
struct Object;
struct OldNewMap;
struct ReportList;
struct UserDef;

typedef struct IDNameLib_Map IDNameLib_Map;

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

typedef struct FileData {
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
  struct SDNA *filesdna;
  const struct SDNA *memsdna;
  /** Array of #eSDNA_StructCompare. */
  const char *compflags;
  struct DNA_ReconstructInfo *reconstruct_info;

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

  struct OldNewMap *datamap;
  struct OldNewMap *globmap;
  struct OldNewMap *libmap;
  struct OldNewMap *packedmap;
  struct BLOCacheStorage *cache_storage;

  struct BHeadSort *bheadmap;
  int tot_bheadmap;

  /** See: #USE_GHASH_BHEAD. */
  struct GHash *bhead_idname_hash;

  ListBase *mainlist;
  /** Used for undo. */
  ListBase *old_mainlist;
  struct IDNameLib_Map *old_idmap;

  struct BlendFileReadReport *reports;
} FileData;

#define SIZEOFBLENDERHEADER 12

/***/
struct Main;
void blo_join_main(ListBase *mainlist);
void blo_split_main(ListBase *mainlist, struct Main *main);

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath);

/**
 * On each new library added, it now checks for the current #FileData and expands relativeness
 *
 * cannot be called with relative paths anymore!
 */
FileData *blo_filedata_from_file(const char *filepath, struct BlendFileReadReport *reports);
FileData *blo_filedata_from_memory(const void *mem,
                                   int memsize,
                                   struct BlendFileReadReport *reports);
FileData *blo_filedata_from_memfile(struct MemFile *memfile,
                                    const struct BlendFileReadParams *params,
                                    struct BlendFileReadReport *reports);

void blo_make_packed_pointer_map(FileData *fd, struct Main *oldmain);
/**
 * Set old main packed data to zero if it has been restored
 * this works because freeing old main only happens after this call.
 */
void blo_end_packed_pointer_map(FileData *fd, struct Main *oldmain);
/**
 * Undo file support: add all library pointers in lookup.
 */
void blo_add_library_pointer_map(ListBase *old_mainlist, FileData *fd);
/**
 * Build a #GSet of old main (we only care about local data here,
 * so we can do that after #blo_split_main() call.
 */
void blo_make_old_idmap_from_main(FileData *fd, struct Main *bmain);

BHead *blo_read_asset_data_block(FileData *fd, BHead *bhead, struct AssetMetaData **r_asset_data);

void blo_cache_storage_init(FileData *fd, struct Main *bmain);
void blo_cache_storage_old_bmain_clear(FileData *fd, struct Main *bmain_old);
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
struct AssetMetaData *blo_bhead_id_asset_data_address(const FileData *fd, const BHead *bhead);

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
void blo_do_versions_dna(struct SDNA *sdna, int versionfile, int subversionfile);

void blo_do_versions_oldnewmap_insert(struct OldNewMap *onm,
                                      const void *oldaddr,
                                      void *newaddr,
                                      int nr);
/**
 * Only library data.
 */
void *blo_do_versions_newlibadr(struct FileData *fd, const void *lib, const void *adr);
void *blo_do_versions_newlibadr_us(struct FileData *fd, const void *lib, const void *adr);

/**
 * \note this version patch is intended for versions < 2.52.2,
 * but was initially introduced in 2.27 already.
 */
void blo_do_version_old_trackto_to_constraints(struct Object *ob);
void blo_do_versions_key_uidgen(struct Key *key);

/**
 * Patching #UserDef struct and Themes.
 */
void blo_do_versions_userdef(struct UserDef *userdef);

void blo_do_versions_pre250(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_250(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_260(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_270(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_280(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_290(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_300(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_400(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_cycles(struct FileData *fd, struct Library *lib, struct Main *bmain);

void do_versions_after_linking_250(struct Main *bmain);
void do_versions_after_linking_260(struct Main *bmain);
void do_versions_after_linking_270(struct Main *bmain);
void do_versions_after_linking_280(struct Main *bmain, struct ReportList *reports);
void do_versions_after_linking_290(struct Main *bmain, struct ReportList *reports);
void do_versions_after_linking_300(struct Main *bmain, struct ReportList *reports);
void do_versions_after_linking_cycles(struct Main *bmain);

/**
 * Direct data-blocks with global linking.
 *
 * \note This is rather unfortunate to have to expose this here,
 * but better use that nasty hack in do_version than readfile itself.
 */
void *blo_read_get_new_globaldata_address(struct FileData *fd, const void *adr);

#ifdef __cplusplus
}
#endif
