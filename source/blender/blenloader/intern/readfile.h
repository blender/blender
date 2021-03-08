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
 * blenloader readfile private function prototypes
 */

/** \file
 * \ingroup blenloader
 */

#pragma once

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "DNA_sdna_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h" /* for ReportType */
#include "zlib.h"

struct BLI_mmap_file;
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
  FD_FLAGS_NOT_MY_BUFFER = 1 << 4,
  /* XXX Unused in practice (checked once but never set). */
  FD_FLAGS_NOT_MY_LIBMAP = 1 << 5,
};

/* Disallow since it's 32bit on ms-windows. */
#ifdef __GNUC__
#  pragma GCC poison off_t
#endif

#if defined(_MSC_VER) || defined(__APPLE__) || defined(__HAIKU__) || defined(__NetBSD__)
typedef int64_t off64_t;
#endif

typedef ssize_t(FileDataReadFn)(struct FileData *filedata,
                                void *buffer,
                                size_t size,
                                bool *r_is_memchunk_identical);
typedef off64_t(FileDataSeekFn)(struct FileData *filedata, off64_t offset, int whence);

typedef struct FileData {
  /** Linked list of BHeadN's. */
  ListBase bhead_list;
  enum eFileDataFlag flags;
  bool is_eof;
  size_t buffersize;
  off64_t file_offset;

  FileDataReadFn *read;
  FileDataSeekFn *seek;

  /** Regular file reading. */
  int filedes;

  /** Variables needed for reading from memory / stream / memory-mapped files. */
  const char *buffer;
  struct BLI_mmap_file *mmap_file;
  /** Variables needed for reading from memfile (undo). */
  struct MemFile *memfile;
  /** Whether we are undoing (< 0) or redoing (> 0), used to choose which 'unchanged' flag to use
   * to detect unchanged data from memfile. */
  int undo_direction; /* eUndoStepDir */

  /** Variables needed for reading from file. */
  gzFile gzfiledes;
  /** Gzip stream for memory decompression. */
  z_stream strm;

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

  struct ReportList *reports;
  /* Counters for amount of missing libraries, and missing IDs in libraries.
   * Used to generate a synthetic report in the UI. */
  int library_file_missing_count;
  int library_id_missing_count;
} FileData;

#define SIZEOFBLENDERHEADER 12

/***/
struct Main;
void blo_join_main(ListBase *mainlist);
void blo_split_main(ListBase *mainlist, struct Main *main);

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath);

FileData *blo_filedata_from_file(const char *filepath, struct ReportList *reports);
FileData *blo_filedata_from_memory(const void *mem, int memsize, struct ReportList *reports);
FileData *blo_filedata_from_memfile(struct MemFile *memfile,
                                    const struct BlendFileReadParams *params,
                                    struct ReportList *reports);

void blo_clear_proxy_pointers_from_lib(struct Main *oldmain);
void blo_make_packed_pointer_map(FileData *fd, struct Main *oldmain);
void blo_end_packed_pointer_map(FileData *fd, struct Main *oldmain);
void blo_add_library_pointer_map(ListBase *old_mainlist, FileData *fd);
void blo_make_old_idmap_from_main(FileData *fd, struct Main *bmain);

BHead *blo_read_asset_data_block(FileData *fd, BHead *bhead, struct AssetMetaData **r_asset_data);

void blo_cache_storage_init(FileData *fd, struct Main *bmain);
void blo_cache_storage_old_bmain_clear(FileData *fd, struct Main *bmain_old);
void blo_cache_storage_end(FileData *fd);

void blo_filedata_free(FileData *fd);

BHead *blo_bhead_first(FileData *fd);
BHead *blo_bhead_next(FileData *fd, BHead *thisblock);
BHead *blo_bhead_prev(FileData *fd, BHead *thisblock);

const char *blo_bhead_id_name(const FileData *fd, const BHead *bhead);
struct AssetMetaData *blo_bhead_id_asset_data_address(const FileData *fd, const BHead *bhead);

/* do versions stuff */

void blo_do_versions_dna(struct SDNA *sdna, const int versionfile, const int subversionfile);

void blo_do_versions_oldnewmap_insert(struct OldNewMap *onm,
                                      const void *oldaddr,
                                      void *newaddr,
                                      int nr);
void *blo_do_versions_newlibadr(struct FileData *fd, const void *lib, const void *adr);
void *blo_do_versions_newlibadr_us(struct FileData *fd, const void *lib, const void *adr);

void blo_do_version_old_trackto_to_constraints(struct Object *ob);
void blo_do_versions_key_uidgen(struct Key *key);

void blo_do_versions_userdef(struct UserDef *userdef);

void blo_do_versions_pre250(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_250(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_260(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_270(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_280(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_290(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_cycles(struct FileData *fd, struct Library *lib, struct Main *bmain);

void do_versions_after_linking_250(struct Main *bmain);
void do_versions_after_linking_260(struct Main *bmain);
void do_versions_after_linking_270(struct Main *bmain);
void do_versions_after_linking_280(struct Main *bmain, struct ReportList *reports);
void do_versions_after_linking_290(struct Main *bmain, struct ReportList *reports);
void do_versions_after_linking_cycles(struct Main *bmain);

/* This is rather unfortunate to have to expose this here, but better use that nasty hack in
 * do_version than readfile itself. */
void *blo_read_get_new_globaldata_address(struct FileData *fd, const void *adr);
