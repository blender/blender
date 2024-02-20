/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#include <cctype> /* for isdigit. */
#include <cerrno>
#include <climits>
#include <cstdarg> /* for va_start/end. */
#include <cstddef> /* for offsetof. */
#include <cstdlib> /* for atoi. */
#include <ctime>   /* for gmtime. */
#include <fcntl.h> /* for open flags (O_BINARY, O_RDONLY). */

#include "BLI_utildefines.h"
#ifndef WIN32
#  include <unistd.h> /* for read close */
#else
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h> /* for open close read */
#endif

#include "CLG_log.h"

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_asset_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_key_types.h"
#include "DNA_layer_types.h"
#include "DNA_node_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sound_types.h"
#include "DNA_vfont_types.h"
#include "DNA_volume_types.h"
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_map.hh"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_asset.hh"
#include "BKE_blender_version.h"
#include "BKE_collection.hh"
#include "BKE_global.hh" /* for G */
#include "BKE_idprop.h"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh" /* for Main */
#include "BKE_main_idmap.hh"
#include "BKE_main_namemap.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh" /* for tree type defines */
#include "BKE_object.hh"
#include "BKE_packedFile.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_undo_system.hh"
#include "BKE_workspace.h"

#include "DRW_engine.hh"

#include "DEG_depsgraph.hh"

#include "BLO_blend_defs.hh"
#include "BLO_blend_validate.hh"
#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_undofile.hh"

#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "readfile.hh"

/* Make preferences read-only. */
#define U (*((const UserDef *)&U))

/**
 * READ
 * ====
 *
 * - Existing Library (#Main) push or free
 * - allocate new #Main
 * - load file
 * - read #SDNA
 * - for each LibBlock
 *   - read LibBlock
 *   - if a Library
 *     - make a new #Main
 *     - attach ID's to it
 *   - else
 *     - read associated 'direct data'
 *     - link direct data (internal and to LibBlock)
 * - read #FileGlobal
 * - read #USER data, only when indicated (file is `~/.config/blender/X.X/config/userpref.blend`)
 * - free file
 * - per Library (per #Main)
 *   - read file
 *   - read #SDNA
 *   - find LibBlocks and attach #ID's to #Main
 *     - if external LibBlock
 *       - search all #Main's
 *         - or it's already read,
 *         - or not read yet
 *         - or make new #Main
 *   - per LibBlock
 *     - read recursive
 *     - read associated direct data
 *     - link direct data (internal and to LibBlock)
 *   - free file
 * - per Library with unread LibBlocks
 *   - read file
 *   - read #SDNA
 *   - per LibBlock
 *     - read recursive
 *     - read associated direct data
 *     - link direct data (internal and to LibBlock)
 *   - free file
 * - join all #Main's
 * - link all LibBlocks and indirect pointers to libblocks
 * - initialize #FileGlobal and copy pointers to #Global
 *
 * \note Still a weak point is the new-address function, that doesn't solve reading from
 * multiple files at the same time.
 * (added remark: oh, i thought that was solved? will look at that... (ton).
 */

/**
 * Delay reading blocks we might not use (especially applies to library linking).
 * which keeps large arrays in memory from data-blocks we may not even use.
 *
 * \note This is disabled when using compression,
 * while ZLIB supports seek it's unusably slow, see: #61880.
 */
#define USE_BHEAD_READ_ON_DEMAND

/** Use #GHash for #BHead name-based lookups (speeds up linking). */
#define USE_GHASH_BHEAD

/** Use #GHash for restoring pointers by name. */
#define USE_GHASH_RESTORE_POINTER

static CLG_LogRef LOG = {"blo.readfile"};
static CLG_LogRef LOG_UNDO = {"blo.readfile.undo"};

/* local prototypes */
static void read_libraries(FileData *basefd, ListBase *mainlist);
static void *read_struct(FileData *fd, BHead *bh, const char *blockname);
static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name);
static BHead *find_bhead_from_idname(FileData *fd, const char *idname);

struct BHeadN {
  BHeadN *next, *prev;
#ifdef USE_BHEAD_READ_ON_DEMAND
  /** Use to read the data from the file directly into memory as needed. */
  off64_t file_offset;
  /** When set, the remainder of this allocation is the data, otherwise it needs to be read. */
  bool has_data;
#endif
  bool is_memchunk_identical;
  BHead bhead;
};

#define BHEADN_FROM_BHEAD(bh) ((BHeadN *)POINTER_OFFSET(bh, -int(offsetof(BHeadN, bhead))))

/**
 * We could change this in the future, for now it's simplest if only data is delayed
 * because ID names are used in lookup tables.
 */
#define BHEAD_USE_READ_ON_DEMAND(bhead) ((bhead)->code == BLO_CODE_DATA)

/* -------------------------------------------------------------------- */
/** \name Blend Loader Reporting Wrapper
 * \{ */

void BLO_reportf_wrap(BlendFileReadReport *reports, eReportType type, const char *format, ...)
{
  char fixed_buf[1024]; /* should be long enough */

  va_list args;

  va_start(args, format);
  vsnprintf(fixed_buf, sizeof(fixed_buf), format, args);
  va_end(args);

  fixed_buf[sizeof(fixed_buf) - 1] = '\0';

  BKE_report(reports->reports, type, fixed_buf);

  if (G.background == 0) {
    printf("%s: %s\n", BKE_report_type_str(type), fixed_buf);
  }
}

/* for reporting linking messages */
static const char *library_parent_filepath(Library *lib)
{
  return lib->parent ? lib->parent->filepath_abs : "<direct>";
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OldNewMap API
 * \{ */

struct NewAddress {
  void *newp;

  /** `nr` is "user count" for data, and ID code for libdata. */
  int nr;
};

struct OldNewMap {
  blender::Map<const void *, NewAddress> map;
};

static OldNewMap *oldnewmap_new()
{
  return MEM_new<OldNewMap>(__func__);
}

static void oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr)
{
  if (oldaddr == nullptr || newaddr == nullptr) {
    return;
  }

  onm->map.add_overwrite(oldaddr, NewAddress{newaddr, nr});
}

static void oldnewmap_lib_insert(FileData *fd, const void *oldaddr, ID *newaddr, int id_code)
{
  oldnewmap_insert(fd->libmap, oldaddr, newaddr, id_code);
}

void blo_do_versions_oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr)
{
  oldnewmap_insert(onm, oldaddr, newaddr, nr);
}

static void *oldnewmap_lookup_and_inc(OldNewMap *onm, const void *addr, bool increase_users)
{
  NewAddress *entry = onm->map.lookup_ptr(addr);
  if (entry == nullptr) {
    return nullptr;
  }
  if (increase_users) {
    entry->nr++;
  }
  return entry->newp;
}

/* for libdata, NewAddress.nr has ID code, no increment */
static void *oldnewmap_liblookup(OldNewMap *onm, const void *addr, const bool is_linked_only)
{
  if (addr == nullptr) {
    return nullptr;
  }

  ID *id = static_cast<ID *>(oldnewmap_lookup_and_inc(onm, addr, false));
  if (id == nullptr) {
    return nullptr;
  }
  if (!is_linked_only || ID_IS_LINKED(id)) {
    return id;
  }
  return nullptr;
}

static void oldnewmap_clear(OldNewMap *onm)
{
  /* Free unused data. */
  for (NewAddress &new_addr : onm->map.values()) {
    if (new_addr.nr == 0) {
      MEM_freeN(new_addr.newp);
    }
  }
  onm->map.clear_and_shrink();
}

static void oldnewmap_free(OldNewMap *onm)
{
  MEM_delete(onm);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper Functions
 * \{ */

static void add_main_to_main(Main *mainvar, Main *from)
{
  ListBase *lbarray[INDEX_ID_MAX], *fromarray[INDEX_ID_MAX];
  int a;

  if (from->is_read_invalid) {
    mainvar->is_read_invalid = true;
  }

  set_listbasepointers(mainvar, lbarray);
  a = set_listbasepointers(from, fromarray);
  while (a--) {
    BLI_movelisttolist(lbarray[a], fromarray[a]);
  }
}

void blo_join_main(ListBase *mainlist)
{
  Main *tojoin, *mainl;

  mainl = static_cast<Main *>(mainlist->first);

  if (mainl->id_map != nullptr) {
    /* Cannot keep this since we add some IDs from joined mains. */
    BKE_main_idmap_destroy(mainl->id_map);
    mainl->id_map = nullptr;
  }

  while ((tojoin = mainl->next)) {
    add_main_to_main(mainl, tojoin);
    BLI_remlink(mainlist, tojoin);
    tojoin->next = tojoin->prev = nullptr;
    BKE_main_free(tojoin);
  }
}

static void split_libdata(ListBase *lb_src, Main **lib_main_array, const uint lib_main_array_len)
{
  for (ID *id = static_cast<ID *>(lb_src->first), *idnext; id; id = idnext) {
    idnext = static_cast<ID *>(id->next);

    if (id->lib) {
      if ((uint(id->lib->temp_index) < lib_main_array_len) &&
          /* this check should never fail, just in case 'id->lib' is a dangling pointer. */
          (lib_main_array[id->lib->temp_index]->curlib == id->lib))
      {
        Main *mainvar = lib_main_array[id->lib->temp_index];
        ListBase *lb_dst = which_libbase(mainvar, GS(id->name));
        BLI_remlink(lb_src, id);
        BLI_addtail(lb_dst, id);
      }
      else {
        CLOG_ERROR(&LOG, "Invalid library for '%s'", id->name);
      }
    }
  }
}

void blo_split_main(ListBase *mainlist, Main *main)
{
  mainlist->first = mainlist->last = main;
  main->next = nullptr;

  if (BLI_listbase_is_empty(&main->libraries)) {
    return;
  }

  if (main->id_map != nullptr) {
    /* Cannot keep this since we remove some IDs from given main. */
    BKE_main_idmap_destroy(main->id_map);
    main->id_map = nullptr;
  }

  /* (Library.temp_index -> Main), lookup table */
  const uint lib_main_array_len = BLI_listbase_count(&main->libraries);
  Main **lib_main_array = static_cast<Main **>(
      MEM_malloc_arrayN(lib_main_array_len, sizeof(*lib_main_array), __func__));

  int i = 0;
  for (Library *lib = static_cast<Library *>(main->libraries.first); lib;
       lib = static_cast<Library *>(lib->id.next), i++)
  {
    Main *libmain = BKE_main_new();
    libmain->curlib = lib;
    libmain->versionfile = lib->versionfile;
    libmain->subversionfile = lib->subversionfile;
    libmain->has_forward_compatibility_issues = !MAIN_VERSION_FILE_OLDER_OR_EQUAL(
        libmain, BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);
    BLI_addtail(mainlist, libmain);
    lib->temp_index = i;
    lib_main_array[i] = libmain;
  }

  ListBase *lbarray[INDEX_ID_MAX];
  i = set_listbasepointers(main, lbarray);
  while (i--) {
    ID *id = static_cast<ID *>(lbarray[i]->first);
    if (id == nullptr || GS(id->name) == ID_LI) {
      /* No ID_LI data-block should ever be linked anyway, but just in case, better be explicit. */
      continue;
    }
    split_libdata(lbarray[i], lib_main_array, lib_main_array_len);
  }

  MEM_freeN(lib_main_array);
}

static void read_file_version(FileData *fd, Main *main)
{
  BHead *bhead;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_GLOB) {
      FileGlobal *fg = static_cast<FileGlobal *>(read_struct(fd, bhead, "Global"));
      if (fg) {
        main->subversionfile = fg->subversion;
        main->minversionfile = fg->minversion;
        main->minsubversionfile = fg->minsubversion;
        MEM_freeN(fg);
      }
      else if (bhead->code == BLO_CODE_ENDB) {
        break;
      }
    }
  }
  if (main->curlib) {
    main->curlib->versionfile = main->versionfile;
    main->curlib->subversionfile = main->subversionfile;
  }
}

static bool blo_bhead_is_id(const BHead *bhead)
{
  /* BHead codes are four bytes (like 'ENDB', 'TEST', etc.), but if the two most-significant bytes
   * are zero, the values actually indicate an ID type. */
  return bhead->code <= 0xFFFF;
}

static bool blo_bhead_is_id_valid_type(const BHead *bhead)
{
  if (!blo_bhead_is_id(bhead)) {
    return false;
  }

  const short id_type_code = bhead->code & 0xFFFF;
  return BKE_idtype_idcode_is_valid(id_type_code);
}

#ifdef USE_GHASH_BHEAD
static void read_file_bhead_idname_map_create(FileData *fd)
{
  BHead *bhead;

  /* dummy values */
  bool is_link = false;
  int code_prev = BLO_CODE_ENDB;
  uint reserve = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (code_prev != bhead->code) {
      code_prev = bhead->code;
      is_link = blo_bhead_is_id_valid_type(bhead) ?
                    BKE_idtype_idcode_is_linkable(short(code_prev)) :
                    false;
    }

    if (is_link) {
      reserve += 1;
    }
  }

  BLI_assert(fd->bhead_idname_hash == nullptr);

  fd->bhead_idname_hash = BLI_ghash_str_new_ex(__func__, reserve);

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (code_prev != bhead->code) {
      code_prev = bhead->code;
      is_link = blo_bhead_is_id_valid_type(bhead) ?
                    BKE_idtype_idcode_is_linkable(short(code_prev)) :
                    false;
    }

    if (is_link) {
      BLI_ghash_insert(fd->bhead_idname_hash, (void *)blo_bhead_id_name(fd, bhead), bhead);
    }
  }
}
#endif

static Main *blo_find_main(FileData *fd, const char *filepath, const char *relabase)
{
  ListBase *mainlist = fd->mainlist;
  Main *m;
  Library *lib;
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, relabase);
  BLI_path_normalize(filepath_abs);

  //  printf("blo_find_main: relabase  %s\n", relabase);
  //  printf("blo_find_main: original in  %s\n", filepath);
  //  printf("blo_find_main: converted to %s\n", filepath_abs);

  LISTBASE_FOREACH (Main *, m, mainlist) {
    const char *libname = (m->curlib) ? m->curlib->filepath_abs : m->filepath;

    if (BLI_path_cmp(filepath_abs, libname) == 0) {
      if (G.debug & G_DEBUG) {
        CLOG_INFO(&LOG, 3, "Found library %s", libname);
      }
      return m;
    }
  }

  m = BKE_main_new();
  BLI_addtail(mainlist, m);

  /* Add library data-block itself to 'main' Main, since libraries are **never** linked data.
   * Fixes bug where you could end with all ID_LI data-blocks having the same name... */
  lib = static_cast<Library *>(BKE_libblock_alloc(
      static_cast<Main *>(mainlist->first), ID_LI, BLI_path_basename(filepath), 0));

  /* Important, consistency with main ID reading code from read_libblock(). */
  lib->id.us = ID_FAKE_USERS(lib);

  /* Matches direct_link_library(). */
  id_us_ensure_real(&lib->id);

  STRNCPY(lib->filepath, filepath);
  STRNCPY(lib->filepath_abs, filepath_abs);

  m->curlib = lib;

  read_file_version(fd, m);

  if (G.debug & G_DEBUG) {
    CLOG_INFO(&LOG, 3, "Added new lib %s", filepath);
  }
  return m;
}

void blo_readfile_invalidate(FileData *fd, Main *bmain, const char *message)
{
  /* Tag given `bmain`, and 'root 'local' main one (in case given one is a library one) as invalid.
   */
  bmain->is_read_invalid = true;
  for (; bmain->prev != nullptr; bmain = bmain->prev) {
    /* Pass. */
  }
  bmain->is_read_invalid = true;

  BLO_reportf_wrap(fd->reports,
                   RPT_ERROR,
                   "A critical error happened (the blend file is likely corrupted): %s",
                   message);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Parsing
 * \{ */

struct BlendDataReader {
  FileData *fd;
};

struct BlendLibReader {
  FileData *fd;
  Main *main;
};

static void switch_endian_bh4(BHead4 *bhead)
{
  /* the ID_.. codes */
  if ((bhead->code & 0xFFFF) == 0) {
    bhead->code >>= 16;
  }

  if (bhead->code != BLO_CODE_ENDB) {
    BLI_endian_switch_int32(&bhead->len);
    BLI_endian_switch_int32(&bhead->SDNAnr);
    BLI_endian_switch_int32(&bhead->nr);
  }
}

static void switch_endian_bh8(BHead8 *bhead)
{
  /* the ID_.. codes */
  if ((bhead->code & 0xFFFF) == 0) {
    bhead->code >>= 16;
  }

  if (bhead->code != BLO_CODE_ENDB) {
    BLI_endian_switch_int32(&bhead->len);
    BLI_endian_switch_int32(&bhead->SDNAnr);
    BLI_endian_switch_int32(&bhead->nr);
  }
}

static void bh4_from_bh8(BHead *bhead, BHead8 *bhead8, bool do_endian_swap)
{
  BHead4 *bhead4 = (BHead4 *)bhead;
  int64_t old;

  bhead4->code = bhead8->code;
  bhead4->len = bhead8->len;

  if (bhead4->code != BLO_CODE_ENDB) {
    /* perform a endian swap on 64bit pointers, otherwise the pointer might map to zero
     * 0x0000000000000000000012345678 would become 0x12345678000000000000000000000000
     */
    if (do_endian_swap) {
      BLI_endian_switch_uint64(&bhead8->old);
    }

    /* this patch is to avoid `intptr_t` being read from not-eight aligned positions
     * is necessary on any modern 64bit architecture) */
    memcpy(&old, &bhead8->old, 8);
    bhead4->old = int(old >> 3);

    bhead4->SDNAnr = bhead8->SDNAnr;
    bhead4->nr = bhead8->nr;
  }
}

static void bh8_from_bh4(BHead *bhead, BHead4 *bhead4)
{
  BHead8 *bhead8 = (BHead8 *)bhead;

  bhead8->code = bhead4->code;
  bhead8->len = bhead4->len;

  if (bhead8->code != BLO_CODE_ENDB) {
    bhead8->old = bhead4->old;
    bhead8->SDNAnr = bhead4->SDNAnr;
    bhead8->nr = bhead4->nr;
  }
}

static BHeadN *get_bhead(FileData *fd)
{
  BHeadN *new_bhead = nullptr;
  int64_t readsize;

  if (fd) {
    if (!fd->is_eof) {
      /* initializing to zero isn't strictly needed but shuts valgrind up
       * since uninitialized memory gets compared */
      BHead8 bhead8 = {0};
      BHead4 bhead4 = {0};
      BHead bhead = {0};

      /* First read the bhead structure.
       * Depending on the platform the file was written on this can
       * be a big or little endian BHead4 or BHead8 structure.
       *
       * As usual 'ENDB' (the last *partial* bhead of the file)
       * needs some special handling. We don't want to EOF just yet.
       */
      if (fd->flags & FD_FLAGS_FILE_POINTSIZE_IS_4) {
        bhead4.code = BLO_CODE_DATA;
        readsize = fd->file->read(fd->file, &bhead4, sizeof(bhead4));

        if (readsize == sizeof(bhead4) || bhead4.code == BLO_CODE_ENDB) {
          if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
            switch_endian_bh4(&bhead4);
          }

          if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
            bh8_from_bh4(&bhead, &bhead4);
          }
          else {
            /* std::min is only to quiet '-Warray-bounds' compiler warning. */
            BLI_assert(sizeof(bhead) == sizeof(bhead4));
            memcpy(&bhead, &bhead4, std::min(sizeof(bhead), sizeof(bhead4)));
          }
        }
        else {
          fd->is_eof = true;
          bhead.len = 0;
        }
      }
      else {
        bhead8.code = BLO_CODE_DATA;
        readsize = fd->file->read(fd->file, &bhead8, sizeof(bhead8));

        if (readsize == sizeof(bhead8) || bhead8.code == BLO_CODE_ENDB) {
          if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
            switch_endian_bh8(&bhead8);
          }

          if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
            bh4_from_bh8(&bhead, &bhead8, (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0);
          }
          else {
            /* std::min is only to quiet `-Warray-bounds` compiler warning. */
            BLI_assert(sizeof(bhead) == sizeof(bhead8));
            memcpy(&bhead, &bhead8, std::min(sizeof(bhead), sizeof(bhead8)));
          }
        }
        else {
          fd->is_eof = true;
          bhead.len = 0;
        }
      }

      /* make sure people are not trying to pass bad blend files */
      if (bhead.len < 0) {
        fd->is_eof = true;
      }

      /* bhead now contains the (converted) bhead structure. Now read
       * the associated data and put everything in a BHeadN (creative naming !)
       */
      if (fd->is_eof) {
        /* pass */
      }
#ifdef USE_BHEAD_READ_ON_DEMAND
      else if (fd->file->seek != nullptr && BHEAD_USE_READ_ON_DEMAND(&bhead)) {
        /* Delay reading bhead content. */
        new_bhead = static_cast<BHeadN *>(MEM_mallocN(sizeof(BHeadN), "new_bhead"));
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = nullptr;
          new_bhead->file_offset = fd->file->offset;
          new_bhead->has_data = false;
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = bhead;
          const off64_t seek_new = fd->file->seek(fd->file, bhead.len, SEEK_CUR);
          if (UNLIKELY(seek_new == -1)) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = nullptr;
          }
          else {
            BLI_assert(fd->file->offset == seek_new);
          }
        }
        else {
          fd->is_eof = true;
        }
      }
#endif
      else {
        new_bhead = static_cast<BHeadN *>(
            MEM_mallocN(sizeof(BHeadN) + size_t(bhead.len), "new_bhead"));
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = nullptr;
#ifdef USE_BHEAD_READ_ON_DEMAND
          new_bhead->file_offset = 0; /* don't seek. */
          new_bhead->has_data = true;
#endif
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = bhead;

          readsize = fd->file->read(fd->file, new_bhead + 1, size_t(bhead.len));

          if (readsize != bhead.len) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = nullptr;
          }

          if (fd->flags & FD_FLAGS_IS_MEMFILE) {
            new_bhead->is_memchunk_identical = ((UndoReader *)fd->file)->memchunk_identical;
          }
        }
        else {
          fd->is_eof = true;
        }
      }
    }
  }

  /* We've read a new block. Now add it to the list
   * of blocks.
   */
  if (new_bhead) {
    BLI_addtail(&fd->bhead_list, new_bhead);
  }

  return new_bhead;
}

BHead *blo_bhead_first(FileData *fd)
{
  BHeadN *new_bhead;
  BHead *bhead = nullptr;

  /* Rewind the file
   * Read in a new block if necessary
   */
  new_bhead = static_cast<BHeadN *>(fd->bhead_list.first);
  if (new_bhead == nullptr) {
    new_bhead = get_bhead(fd);
  }

  if (new_bhead) {
    bhead = &new_bhead->bhead;
  }

  return bhead;
}

BHead *blo_bhead_prev(FileData * /*fd*/, BHead *thisblock)
{
  BHeadN *bheadn = BHEADN_FROM_BHEAD(thisblock);
  BHeadN *prev = bheadn->prev;

  return (prev) ? &prev->bhead : nullptr;
}

BHead *blo_bhead_next(FileData *fd, BHead *thisblock)
{
  BHeadN *new_bhead = nullptr;
  BHead *bhead = nullptr;

  if (thisblock) {
    /* bhead is actually a sub part of BHeadN
     * We calculate the BHeadN pointer from the BHead pointer below */
    new_bhead = BHEADN_FROM_BHEAD(thisblock);

    /* get the next BHeadN. If it doesn't exist we read in the next one */
    new_bhead = new_bhead->next;
    if (new_bhead == nullptr) {
      new_bhead = get_bhead(fd);
    }
  }

  if (new_bhead) {
    /* here we do the reverse:
     * go from the BHeadN pointer to the BHead pointer */
    bhead = &new_bhead->bhead;
  }

  return bhead;
}

#ifdef USE_BHEAD_READ_ON_DEMAND
static bool blo_bhead_read_data(FileData *fd, BHead *thisblock, void *buf)
{
  bool success = true;
  BHeadN *new_bhead = BHEADN_FROM_BHEAD(thisblock);
  BLI_assert(new_bhead->has_data == false && new_bhead->file_offset != 0);
  off64_t offset_backup = fd->file->offset;
  if (UNLIKELY(fd->file->seek(fd->file, new_bhead->file_offset, SEEK_SET) == -1)) {
    success = false;
  }
  else {
    if (fd->file->read(fd->file, buf, size_t(new_bhead->bhead.len)) != new_bhead->bhead.len) {
      success = false;
    }
    if (fd->flags & FD_FLAGS_IS_MEMFILE) {
      new_bhead->is_memchunk_identical = ((UndoReader *)fd->file)->memchunk_identical;
    }
  }
  if (fd->file->seek(fd->file, offset_backup, SEEK_SET) == -1) {
    success = false;
  }
  return success;
}

static BHead *blo_bhead_read_full(FileData *fd, BHead *thisblock)
{
  BHeadN *new_bhead = BHEADN_FROM_BHEAD(thisblock);
  BHeadN *new_bhead_data = static_cast<BHeadN *>(
      MEM_mallocN(sizeof(BHeadN) + new_bhead->bhead.len, "new_bhead"));
  new_bhead_data->bhead = new_bhead->bhead;
  new_bhead_data->file_offset = new_bhead->file_offset;
  new_bhead_data->has_data = true;
  new_bhead_data->is_memchunk_identical = false;
  if (!blo_bhead_read_data(fd, thisblock, new_bhead_data + 1)) {
    MEM_freeN(new_bhead_data);
    return nullptr;
  }
  return &new_bhead_data->bhead;
}
#endif /* USE_BHEAD_READ_ON_DEMAND */

const char *blo_bhead_id_name(const FileData *fd, const BHead *bhead)
{
  return (const char *)POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_name_offset);
}

AssetMetaData *blo_bhead_id_asset_data_address(const FileData *fd, const BHead *bhead)
{
  BLI_assert(blo_bhead_is_id_valid_type(bhead));
  return (fd->id_asset_data_offset >= 0) ?
             *(AssetMetaData **)POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_asset_data_offset) :
             nullptr;
}

static void decode_blender_header(FileData *fd)
{
  char header[SIZEOFBLENDERHEADER], num[4];
  int64_t readsize;

  /* read in the header data */
  readsize = fd->file->read(fd->file, header, sizeof(header));

  if (readsize == sizeof(header) && STREQLEN(header, "BLENDER", 7) && ELEM(header[7], '_', '-') &&
      ELEM(header[8], 'v', 'V') &&
      (isdigit(header[9]) && isdigit(header[10]) && isdigit(header[11])))
  {
    fd->flags |= FD_FLAGS_FILE_OK;

    /* what size are pointers in the file ? */
    if (header[7] == '_') {
      fd->flags |= FD_FLAGS_FILE_POINTSIZE_IS_4;
      if (sizeof(void *) != 4) {
        fd->flags |= FD_FLAGS_POINTSIZE_DIFFERS;
      }
    }
    else {
      if (sizeof(void *) != 8) {
        fd->flags |= FD_FLAGS_POINTSIZE_DIFFERS;
      }
    }

    /* is the file saved in a different endian
     * than we need ?
     */
    if (((header[8] == 'v') ? L_ENDIAN : B_ENDIAN) != ENDIAN_ORDER) {
      fd->flags |= FD_FLAGS_SWITCH_ENDIAN;
    }

    /* get the version number */
    memcpy(num, header + 9, 3);
    num[3] = 0;
    fd->fileversion = atoi(num);
  }
}

/**
 * \return Success if the file is read correctly, else set \a r_error_message.
 */
static bool read_file_dna(FileData *fd, const char **r_error_message)
{
  BHead *bhead;
  int subversion = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_GLOB) {
      /* Before this, the subversion didn't exist in 'FileGlobal' so the subversion
       * value isn't accessible for the purpose of DNA versioning in this case. */
      if (fd->fileversion <= 242) {
        continue;
      }
      /* We can't use read_global because this needs 'DNA1' to be decoded,
       * however the first 4 chars are _always_ the subversion. */
      FileGlobal *fg = reinterpret_cast<FileGlobal *>(&bhead[1]);
      BLI_STATIC_ASSERT(offsetof(FileGlobal, subvstr) == 0, "Must be first: subvstr")
      char num[5];
      memcpy(num, fg->subvstr, 4);
      num[4] = 0;
      subversion = atoi(num);
    }
    else if (bhead->code == BLO_CODE_DNA1) {
      const bool do_endian_swap = (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;
      const bool do_alias = false; /* Postpone until after #blo_do_versions_dna runs. */
      fd->filesdna = DNA_sdna_from_data(
          &bhead[1], bhead->len, do_endian_swap, true, do_alias, r_error_message);
      if (fd->filesdna) {
        blo_do_versions_dna(fd->filesdna, fd->fileversion, subversion);
        /* Allow aliased lookups (must be after version patching DNA). */
        DNA_sdna_alias_data_ensure_structs_map(fd->filesdna);

        fd->compflags = DNA_struct_get_compareflags(fd->filesdna, fd->memsdna);
        fd->reconstruct_info = DNA_reconstruct_info_create(
            fd->filesdna, fd->memsdna, fd->compflags);
        /* used to retrieve ID names from (bhead+1) */
        fd->id_name_offset = DNA_struct_member_offset_by_name_with_alias(
            fd->filesdna, "ID", "char", "name[]");
        BLI_assert(fd->id_name_offset != -1);
        fd->id_asset_data_offset = DNA_struct_member_offset_by_name_with_alias(
            fd->filesdna, "ID", "AssetMetaData", "*asset_data");

        return true;
      }

      return false;
    }
    else if (bhead->code == BLO_CODE_ENDB) {
      break;
    }
  }

  *r_error_message = "Missing DNA block";
  return false;
}

static int *read_file_thumbnail(FileData *fd)
{
  BHead *bhead;
  int *blend_thumb = nullptr;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_TEST) {
      const bool do_endian_swap = (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;
      int *data = (int *)(bhead + 1);

      if (bhead->len < sizeof(int[2])) {
        break;
      }

      if (do_endian_swap) {
        BLI_endian_switch_int32(&data[0]);
        BLI_endian_switch_int32(&data[1]);
      }

      const int width = data[0];
      const int height = data[1];
      if (!BLEN_THUMB_MEMSIZE_IS_VALID(width, height)) {
        break;
      }
      if (bhead->len < BLEN_THUMB_MEMSIZE_FILE(width, height)) {
        break;
      }

      blend_thumb = data;
      break;
    }
    if (bhead->code != BLO_CODE_REND) {
      /* Thumbnail is stored in TEST immediately after first REND... */
      break;
    }
  }

  return blend_thumb;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Data API
 * \{ */

static FileData *filedata_new(BlendFileReadReport *reports)
{
  BLI_assert(reports != nullptr);

  FileData *fd = static_cast<FileData *>(MEM_callocN(sizeof(FileData), "FileData"));

  fd->memsdna = DNA_sdna_current_get();

  fd->datamap = oldnewmap_new();
  fd->globmap = oldnewmap_new();
  fd->libmap = oldnewmap_new();

  fd->reports = reports;

  return fd;
}

/**
 * Check if #FileGlobal::minversion of the file is older than current Blender,
 * return false if it is not.
 * Should only be called after #read_file_dna was successfully executed.
 */
static bool is_minversion_older_than_blender(FileData *fd, ReportList *reports)
{
  BLI_assert(fd->filesdna != nullptr);
  for (BHead *bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code != BLO_CODE_GLOB) {
      continue;
    }

    FileGlobal *fg = static_cast<FileGlobal *>(read_struct(fd, bhead, "Global"));
    if ((fg->minversion > BLENDER_FILE_VERSION) ||
        (fg->minversion == BLENDER_FILE_VERSION && fg->minsubversion > BLENDER_FILE_SUBVERSION))
    {
      char writer_ver_str[16];
      char min_reader_ver_str[16];
      if (fd->fileversion == fg->minversion) {
        BKE_blender_version_blendfile_string_from_values(
            writer_ver_str, sizeof(writer_ver_str), short(fd->fileversion), fg->subversion);
        BKE_blender_version_blendfile_string_from_values(
            min_reader_ver_str, sizeof(min_reader_ver_str), fg->minversion, fg->minsubversion);
      }
      else {
        BKE_blender_version_blendfile_string_from_values(
            writer_ver_str, sizeof(writer_ver_str), short(fd->fileversion), -1);
        BKE_blender_version_blendfile_string_from_values(
            min_reader_ver_str, sizeof(min_reader_ver_str), fg->minversion, -1);
      }
      BKE_reportf(reports,
                  RPT_ERROR,
                  "The file was saved by a newer version, open it with Blender %s or later",
                  min_reader_ver_str);
      CLOG_WARN(&LOG,
                "%s: File saved by a newer version of Blender (%s), Blender %s or later is "
                "needed to open it.",
                fd->relabase,
                writer_ver_str,
                min_reader_ver_str);
      MEM_freeN(fg);
      return true;
    }
    MEM_freeN(fg);
    return false;
  }
  return false;
}

static FileData *blo_decode_and_check(FileData *fd, ReportList *reports)
{
  decode_blender_header(fd);

  if (fd->flags & FD_FLAGS_FILE_OK) {
    const char *error_message = nullptr;
    if (read_file_dna(fd, &error_message) == false) {
      BKE_reportf(
          reports, RPT_ERROR, "Failed to read blend file '%s': %s", fd->relabase, error_message);
      blo_filedata_free(fd);
      fd = nullptr;
    }
    else if (is_minversion_older_than_blender(fd, reports)) {
      blo_filedata_free(fd);
      fd = nullptr;
    }
  }
  else {
    BKE_reportf(
        reports, RPT_ERROR, "Failed to read blend file '%s', not a blend file", fd->relabase);
    blo_filedata_free(fd);
    fd = nullptr;
  }

  return fd;
}

static FileData *blo_filedata_from_file_descriptor(const char *filepath,
                                                   BlendFileReadReport *reports,
                                                   int filedes)
{
  char header[7];
  FileReader *rawfile = BLI_filereader_new_file(filedes);
  FileReader *file = nullptr;

  errno = 0;
  /* If opening the file failed or we can't read the header, give up. */
  if (rawfile == nullptr || rawfile->read(rawfile, header, sizeof(header)) != sizeof(header)) {
    BKE_reportf(reports->reports,
                RPT_WARNING,
                "Unable to read '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("insufficient content"));
    if (rawfile) {
      rawfile->close(rawfile);
    }
    else {
      close(filedes);
    }
    return nullptr;
  }

  /* Rewind the file after reading the header. */
  rawfile->seek(rawfile, 0, SEEK_SET);

  /* Check if we have a regular file. */
  if (memcmp(header, "BLENDER", sizeof(header)) == 0) {
    /* Try opening the file with memory-mapped IO. */
    file = BLI_filereader_new_mmap(filedes);
    if (file == nullptr) {
      /* `mmap` failed, so just keep using `rawfile`. */
      file = rawfile;
      rawfile = nullptr;
    }
  }
  else if (BLI_file_magic_is_gzip(header)) {
    file = BLI_filereader_new_gzip(rawfile);
    if (file != nullptr) {
      rawfile = nullptr; /* The `Gzip` #FileReader takes ownership of `rawfile`. */
    }
  }
  else if (BLI_file_magic_is_zstd(header)) {
    file = BLI_filereader_new_zstd(rawfile);
    if (file != nullptr) {
      rawfile = nullptr; /* The `Zstd` #FileReader takes ownership of `rawfile`. */
    }
  }

  /* Clean up `rawfile` if it wasn't taken over. */
  if (rawfile != nullptr) {
    rawfile->close(rawfile);
  }
  if (file == nullptr) {
    BKE_reportf(reports->reports, RPT_WARNING, "Unrecognized file format '%s'", filepath);
    return nullptr;
  }

  FileData *fd = filedata_new(reports);
  fd->file = file;

  return fd;
}

static FileData *blo_filedata_from_file_open(const char *filepath, BlendFileReadReport *reports)
{
  errno = 0;
  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    BKE_reportf(reports->reports,
                RPT_WARNING,
                "Unable to open '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unknown error reading file"));
    return nullptr;
  }
  return blo_filedata_from_file_descriptor(filepath, reports, file);
}

FileData *blo_filedata_from_file(const char *filepath, BlendFileReadReport *reports)
{
  FileData *fd = blo_filedata_from_file_open(filepath, reports);
  if (fd != nullptr) {
    /* needed for library_append and read_libraries */
    STRNCPY(fd->relabase, filepath);

    return blo_decode_and_check(fd, reports->reports);
  }
  return nullptr;
}

/**
 * Same as blo_filedata_from_file(), but does not reads DNA data, only header.
 * Use it for light access (e.g. thumbnail reading).
 */
static FileData *blo_filedata_from_file_minimal(const char *filepath)
{
  BlendFileReadReport read_report{};
  FileData *fd = blo_filedata_from_file_open(filepath, &read_report);
  if (fd != nullptr) {
    decode_blender_header(fd);
    if (fd->flags & FD_FLAGS_FILE_OK) {
      return fd;
    }
    blo_filedata_free(fd);
  }
  return nullptr;
}

FileData *blo_filedata_from_memory(const void *mem, int memsize, BlendFileReadReport *reports)
{
  if (!mem || memsize < SIZEOFBLENDERHEADER) {
    BKE_report(
        reports->reports, RPT_WARNING, (mem) ? RPT_("Unable to read") : RPT_("Unable to open"));
    return nullptr;
  }

  FileReader *mem_file = BLI_filereader_new_memory(mem, memsize);
  FileReader *file = mem_file;

  if (BLI_file_magic_is_gzip(static_cast<const char *>(mem))) {
    file = BLI_filereader_new_gzip(mem_file);
  }
  else if (BLI_file_magic_is_zstd(static_cast<const char *>(mem))) {
    file = BLI_filereader_new_zstd(mem_file);
  }

  if (file == nullptr) {
    /* Compression initialization failed. */
    mem_file->close(mem_file);
    return nullptr;
  }

  FileData *fd = filedata_new(reports);
  fd->file = file;

  return blo_decode_and_check(fd, reports->reports);
}

FileData *blo_filedata_from_memfile(MemFile *memfile,
                                    const BlendFileReadParams *params,
                                    BlendFileReadReport *reports)
{
  if (!memfile) {
    BKE_report(reports->reports, RPT_WARNING, "Unable to open blend <memory>");
    return nullptr;
  }

  FileData *fd = filedata_new(reports);
  fd->file = BLO_memfile_new_filereader(memfile, params->undo_direction);
  fd->undo_direction = params->undo_direction;
  fd->flags |= FD_FLAGS_IS_MEMFILE;

  return blo_decode_and_check(fd, reports->reports);
}

void blo_filedata_free(FileData *fd)
{
  if (fd) {

    /* Free all BHeadN data blocks */
#ifndef NDEBUG
    BLI_freelistN(&fd->bhead_list);
#else
    /* Sanity check we're not keeping memory we don't need. */
    LISTBASE_FOREACH_MUTABLE (BHeadN *, new_bhead, &fd->bhead_list) {
      if (fd->file->seek != nullptr && BHEAD_USE_READ_ON_DEMAND(&new_bhead->bhead)) {
        BLI_assert(new_bhead->has_data == 0);
      }
      MEM_freeN(new_bhead);
    }
#endif
    fd->file->close(fd->file);

    if (fd->filesdna) {
      DNA_sdna_free(fd->filesdna);
    }
    if (fd->compflags) {
      MEM_freeN((void *)fd->compflags);
    }
    if (fd->reconstruct_info) {
      DNA_reconstruct_info_free(fd->reconstruct_info);
    }

    if (fd->datamap) {
      oldnewmap_free(fd->datamap);
    }
    if (fd->globmap) {
      oldnewmap_free(fd->globmap);
    }
    if (fd->packedmap) {
      oldnewmap_free(fd->packedmap);
    }
    if (fd->libmap && !(fd->flags & FD_FLAGS_NOT_MY_LIBMAP)) {
      oldnewmap_free(fd->libmap);
    }
    if (fd->old_idmap_uid != nullptr) {
      BKE_main_idmap_destroy(fd->old_idmap_uid);
    }
    if (fd->new_idmap_uid != nullptr) {
      BKE_main_idmap_destroy(fd->new_idmap_uid);
    }
    blo_cache_storage_end(fd);
    if (fd->bheadmap) {
      MEM_freeN(fd->bheadmap);
    }

#ifdef USE_GHASH_BHEAD
    if (fd->bhead_idname_hash) {
      BLI_ghash_free(fd->bhead_idname_hash, nullptr, nullptr);
    }
#endif

    MEM_freeN(fd);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Thumbnail from Blend File
 * \{ */

BlendThumbnail *BLO_thumbnail_from_file(const char *filepath)
{
  FileData *fd;
  BlendThumbnail *data = nullptr;
  int *fd_data;

  fd = blo_filedata_from_file_minimal(filepath);
  fd_data = fd ? read_file_thumbnail(fd) : nullptr;

  if (fd_data) {
    const int width = fd_data[0];
    const int height = fd_data[1];
    if (BLEN_THUMB_MEMSIZE_IS_VALID(width, height)) {
      const size_t data_size = BLEN_THUMB_MEMSIZE(width, height);
      data = static_cast<BlendThumbnail *>(MEM_mallocN(data_size, __func__));
      if (data) {
        BLI_assert((data_size - sizeof(*data)) ==
                   (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*fd_data) * 2)));
        data->width = width;
        data->height = height;
        memcpy(data->rect, &fd_data[2], data_size - sizeof(*data));
      }
    }
  }

  blo_filedata_free(fd);

  return data;
}

short BLO_version_from_file(const char *filepath)
{
  short version = 0;
  FileData *fd = blo_filedata_from_file_minimal(filepath);
  if (fd) {
    version = fd->fileversion;
    blo_filedata_free(fd);
  }
  return version;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Old/New Pointer Map
 * \{ */

/* Only direct data-blocks. */
static void *newdataadr(FileData *fd, const void *adr)
{
  return oldnewmap_lookup_and_inc(fd->datamap, adr, true);
}

/* Only direct data-blocks. */
static void *newdataadr_no_us(FileData *fd, const void *adr)
{
  return oldnewmap_lookup_and_inc(fd->datamap, adr, false);
}

void *blo_read_get_new_globaldata_address(FileData *fd, const void *adr)
{
  return oldnewmap_lookup_and_inc(fd->globmap, adr, true);
}

/* Used to restore packed data after undo. */
static void *newpackedadr(FileData *fd, const void *adr)
{
  if (fd->packedmap && adr) {
    return oldnewmap_lookup_and_inc(fd->packedmap, adr, true);
  }

  return oldnewmap_lookup_and_inc(fd->datamap, adr, true);
}

/* only lib data */
static void *newlibadr(FileData *fd, ID * /*self_id*/, const bool is_linked_only, const void *adr)
{
  return oldnewmap_liblookup(fd->libmap, adr, is_linked_only);
}

void *blo_do_versions_newlibadr(FileData *fd,
                                ID *self_id,
                                const bool is_linked_only,
                                const void *adr)
{
  return newlibadr(fd, self_id, is_linked_only, adr);
}

/* increases user number */
static void change_link_placeholder_to_real_ID_pointer_fd(FileData *fd,
                                                          const void *old,
                                                          void *newp)
{
  for (NewAddress &entry : fd->libmap->map.values()) {
    if (old == entry.newp && entry.nr == ID_LINK_PLACEHOLDER) {
      entry.newp = newp;
      if (newp) {
        entry.nr = GS(((ID *)newp)->name);
      }
    }
  }
}

static void change_link_placeholder_to_real_ID_pointer(ListBase *mainlist,
                                                       FileData *basefd,
                                                       void *old,
                                                       void *newp)
{
  LISTBASE_FOREACH (Main *, mainptr, mainlist) {
    FileData *fd;

    if (mainptr->curlib) {
      fd = mainptr->curlib->filedata;
    }
    else {
      fd = basefd;
    }

    if (fd) {
      change_link_placeholder_to_real_ID_pointer_fd(fd, old, newp);
    }
  }
}

/* XXX disabled this feature - packed files also belong in temp saves and quit.blend,
 * to make restore work. */

static void insert_packedmap(FileData *fd, PackedFile *pf)
{
  oldnewmap_insert(fd->packedmap, pf, pf, 0);
  oldnewmap_insert(fd->packedmap, pf->data, pf->data, 0);
}

void blo_make_packed_pointer_map(FileData *fd, Main *oldmain)
{
  fd->packedmap = oldnewmap_new();

  LISTBASE_FOREACH (Image *, ima, &oldmain->images) {
    if (ima->packedfile) {
      insert_packedmap(fd, ima->packedfile);
    }

    LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
      if (imapf->packedfile) {
        insert_packedmap(fd, imapf->packedfile);
      }
    }
  }

  LISTBASE_FOREACH (VFont *, vfont, &oldmain->fonts) {
    if (vfont->packedfile) {
      insert_packedmap(fd, vfont->packedfile);
    }
  }

  LISTBASE_FOREACH (bSound *, sound, &oldmain->sounds) {
    if (sound->packedfile) {
      insert_packedmap(fd, sound->packedfile);
    }
  }

  LISTBASE_FOREACH (Volume *, volume, &oldmain->volumes) {
    if (volume->packedfile) {
      insert_packedmap(fd, volume->packedfile);
    }
  }

  LISTBASE_FOREACH (Library *, lib, &oldmain->libraries) {
    if (lib->packedfile) {
      insert_packedmap(fd, lib->packedfile);
    }
  }
}

void blo_end_packed_pointer_map(FileData *fd, Main *oldmain)
{
  /* used entries were restored, so we put them to zero */
  for (NewAddress &entry : fd->packedmap->map.values()) {
    if (entry.nr > 0) {
      entry.newp = nullptr;
    }
  }

  LISTBASE_FOREACH (Image *, ima, &oldmain->images) {
    ima->packedfile = static_cast<PackedFile *>(newpackedadr(fd, ima->packedfile));

    LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
      imapf->packedfile = static_cast<PackedFile *>(newpackedadr(fd, imapf->packedfile));
    }
  }

  LISTBASE_FOREACH (VFont *, vfont, &oldmain->fonts) {
    vfont->packedfile = static_cast<PackedFile *>(newpackedadr(fd, vfont->packedfile));
  }

  LISTBASE_FOREACH (bSound *, sound, &oldmain->sounds) {
    sound->packedfile = static_cast<PackedFile *>(newpackedadr(fd, sound->packedfile));
  }

  LISTBASE_FOREACH (Library *, lib, &oldmain->libraries) {
    lib->packedfile = static_cast<PackedFile *>(newpackedadr(fd, lib->packedfile));
  }

  LISTBASE_FOREACH (Volume *, volume, &oldmain->volumes) {
    volume->packedfile = static_cast<PackedFile *>(newpackedadr(fd, volume->packedfile));
  }
}

void blo_make_old_idmap_from_main(FileData *fd, Main *bmain)
{
  if (fd->old_idmap_uid != nullptr) {
    BKE_main_idmap_destroy(fd->old_idmap_uid);
  }
  fd->old_idmap_uid = BKE_main_idmap_create(bmain, false, nullptr, MAIN_IDMAP_TYPE_UID);
}

struct BLOCacheStorage {
  GHash *cache_map;
  MemArena *memarena;
};

struct BLOCacheStorageValue {
  void *cache_v;
  uint new_usage_count;
};

/** Register a cache data entry to be preserved when reading some undo memfile. */
static void blo_cache_storage_entry_register(
    ID *id, const IDCacheKey *key, void **cache_p, uint /*flags*/, void *cache_storage_v)
{
  BLI_assert(key->id_session_uid == id->session_uid);
  UNUSED_VARS_NDEBUG(id);

  BLOCacheStorage *cache_storage = static_cast<BLOCacheStorage *>(cache_storage_v);
  BLI_assert(!BLI_ghash_haskey(cache_storage->cache_map, key));

  IDCacheKey *storage_key = static_cast<IDCacheKey *>(
      BLI_memarena_alloc(cache_storage->memarena, sizeof(*storage_key)));
  *storage_key = *key;
  BLOCacheStorageValue *storage_value = static_cast<BLOCacheStorageValue *>(
      BLI_memarena_alloc(cache_storage->memarena, sizeof(*storage_value)));
  storage_value->cache_v = *cache_p;
  storage_value->new_usage_count = 0;
  BLI_ghash_insert(cache_storage->cache_map, storage_key, storage_value);
}

/** Restore a cache data entry from old ID into new one, when reading some undo memfile. */
static void blo_cache_storage_entry_restore_in_new(
    ID *id, const IDCacheKey *key, void **cache_p, uint flags, void *cache_storage_v)
{
  BLOCacheStorage *cache_storage = static_cast<BLOCacheStorage *>(cache_storage_v);

  if (cache_storage == nullptr) {
    /* In non-undo case, only clear the pointer if it is a purely runtime one.
     * If it may be stored in a persistent way in the .blend file, direct_link code is responsible
     * to properly deal with it. */
    if ((flags & IDTYPE_CACHE_CB_FLAGS_PERSISTENT) == 0) {
      *cache_p = nullptr;
    }
    return;
  }

  /* Assume that when ID source is tagged as changed, its caches need to be cleared.
   * NOTE: This is mainly a work-around for some IDs, like Image, which use a non-depsgraph-handled
   * process for part of their updates.
   */
  if (id->recalc & ID_RECALC_SOURCE) {
    *cache_p = nullptr;
    return;
  }

  BLOCacheStorageValue *storage_value = static_cast<BLOCacheStorageValue *>(
      BLI_ghash_lookup(cache_storage->cache_map, key));
  if (storage_value == nullptr) {
    *cache_p = nullptr;
    return;
  }
  storage_value->new_usage_count++;
  *cache_p = storage_value->cache_v;
}

/** Clear as needed a cache data entry from old ID, when reading some undo memfile. */
static void blo_cache_storage_entry_clear_in_old(
    ID * /*id*/, const IDCacheKey *key, void **cache_p, uint /*flags*/, void *cache_storage_v)
{
  BLOCacheStorage *cache_storage = static_cast<BLOCacheStorage *>(cache_storage_v);

  BLOCacheStorageValue *storage_value = static_cast<BLOCacheStorageValue *>(
      BLI_ghash_lookup(cache_storage->cache_map, key));
  if (storage_value == nullptr) {
    *cache_p = nullptr;
    return;
  }
  /* If that cache has been restored into some new ID, we want to remove it from old one, otherwise
   * keep it there so that it gets properly freed together with its ID. */
  if (storage_value->new_usage_count != 0) {
    *cache_p = nullptr;
  }
  else {
    BLI_assert(*cache_p == storage_value->cache_v);
  }
}

void blo_cache_storage_init(FileData *fd, Main *bmain)
{
  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    BLI_assert(fd->cache_storage == nullptr);
    fd->cache_storage = static_cast<BLOCacheStorage *>(
        MEM_mallocN(sizeof(*fd->cache_storage), __func__));
    fd->cache_storage->memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    fd->cache_storage->cache_map = BLI_ghash_new(
        BKE_idtype_cache_key_hash, BKE_idtype_cache_key_cmp, __func__);

    ListBase *lb;
    FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
      ID *id = static_cast<ID *>(lb->first);
      if (id == nullptr) {
        continue;
      }

      const IDTypeInfo *type_info = BKE_idtype_get_info_from_id(id);
      if (type_info->foreach_cache == nullptr) {
        continue;
      }

      FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
        if (ID_IS_LINKED(id)) {
          continue;
        }
        BKE_idtype_id_foreach_cache(id, blo_cache_storage_entry_register, fd->cache_storage);
      }
      FOREACH_MAIN_LISTBASE_ID_END;
    }
    FOREACH_MAIN_LISTBASE_END;
  }
  else {
    fd->cache_storage = nullptr;
  }
}

void blo_cache_storage_old_bmain_clear(FileData *fd, Main *bmain_old)
{
  if (fd->cache_storage != nullptr) {
    ListBase *lb;
    FOREACH_MAIN_LISTBASE_BEGIN (bmain_old, lb) {
      ID *id = static_cast<ID *>(lb->first);
      if (id == nullptr) {
        continue;
      }

      const IDTypeInfo *type_info = BKE_idtype_get_info_from_id(id);
      if (type_info->foreach_cache == nullptr) {
        continue;
      }

      FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
        if (ID_IS_LINKED(id)) {
          continue;
        }
        BKE_idtype_id_foreach_cache(id, blo_cache_storage_entry_clear_in_old, fd->cache_storage);
      }
      FOREACH_MAIN_LISTBASE_ID_END;
    }
    FOREACH_MAIN_LISTBASE_END;
  }
}

void blo_cache_storage_end(FileData *fd)
{
  if (fd->cache_storage != nullptr) {
    BLI_ghash_free(fd->cache_storage->cache_map, nullptr, nullptr);
    BLI_memarena_free(fd->cache_storage->memarena);
    MEM_freeN(fd->cache_storage);
    fd->cache_storage = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DNA Struct Loading
 * \{ */

static void switch_endian_structs(const SDNA *filesdna, BHead *bhead)
{
  int blocksize, nblocks;
  char *data;

  data = (char *)(bhead + 1);
  blocksize = filesdna->types_size[filesdna->structs[bhead->SDNAnr]->type];

  nblocks = bhead->nr;
  while (nblocks--) {
    DNA_struct_switch_endian(filesdna, bhead->SDNAnr, data);

    data += blocksize;
  }
}

static void *read_struct(FileData *fd, BHead *bh, const char *blockname)
{
  void *temp = nullptr;

  if (bh->len) {
#ifdef USE_BHEAD_READ_ON_DEMAND
    BHead *bh_orig = bh;
#endif

    /* switch is based on file dna */
    if (bh->SDNAnr && (fd->flags & FD_FLAGS_SWITCH_ENDIAN)) {
#ifdef USE_BHEAD_READ_ON_DEMAND
      if (BHEADN_FROM_BHEAD(bh)->has_data == false) {
        bh = blo_bhead_read_full(fd, bh);
        if (UNLIKELY(bh == nullptr)) {
          fd->flags &= ~FD_FLAGS_FILE_OK;
          return nullptr;
        }
      }
#endif
      switch_endian_structs(fd->filesdna, bh);
    }

    if (fd->compflags[bh->SDNAnr] != SDNA_CMP_REMOVED) {
      if (fd->compflags[bh->SDNAnr] == SDNA_CMP_NOT_EQUAL) {
#ifdef USE_BHEAD_READ_ON_DEMAND
        if (BHEADN_FROM_BHEAD(bh)->has_data == false) {
          bh = blo_bhead_read_full(fd, bh);
          if (UNLIKELY(bh == nullptr)) {
            fd->flags &= ~FD_FLAGS_FILE_OK;
            return nullptr;
          }
        }
#endif
        temp = DNA_struct_reconstruct(fd->reconstruct_info, bh->SDNAnr, bh->nr, (bh + 1));
      }
      else {
        /* SDNA_CMP_EQUAL */
        temp = MEM_mallocN(bh->len, blockname);
#ifdef USE_BHEAD_READ_ON_DEMAND
        if (BHEADN_FROM_BHEAD(bh)->has_data) {
          memcpy(temp, (bh + 1), bh->len);
        }
        else {
          /* Instead of allocating the bhead, then copying it,
           * read the data from the file directly into the memory. */
          if (UNLIKELY(!blo_bhead_read_data(fd, bh, temp))) {
            fd->flags &= ~FD_FLAGS_FILE_OK;
            MEM_freeN(temp);
            temp = nullptr;
          }
        }
#else
        memcpy(temp, (bh + 1), bh->len);
#endif
      }
    }

#ifdef USE_BHEAD_READ_ON_DEMAND
    if (bh_orig != bh) {
      MEM_freeN(BHEADN_FROM_BHEAD(bh));
    }
#endif
  }

  return temp;
}

/* Like read_struct, but gets a pointer without allocating. Only works for
 * undo since DNA must match. */
static const void *peek_struct_undo(FileData *fd, BHead *bhead)
{
  BLI_assert(fd->flags & FD_FLAGS_IS_MEMFILE);
  UNUSED_VARS_NDEBUG(fd);
  return (bhead->len) ? (const void *)(bhead + 1) : nullptr;
}

static void link_glob_list(FileData *fd, ListBase *lb) /* for glob data */
{
  Link *ln, *prev;
  void *poin;

  if (BLI_listbase_is_empty(lb)) {
    return;
  }
  poin = newdataadr(fd, lb->first);
  if (lb->first) {
    oldnewmap_insert(fd->globmap, lb->first, poin, 0);
  }
  lb->first = poin;

  ln = static_cast<Link *>(lb->first);
  prev = nullptr;
  while (ln) {
    poin = newdataadr(fd, ln->next);
    if (ln->next) {
      oldnewmap_insert(fd->globmap, ln->next, poin, 0);
    }
    ln->next = static_cast<Link *>(poin);
    ln->prev = prev;
    prev = ln;
    ln = ln->next;
  }
  lb->last = prev;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID
 * \{ */

static void after_liblink_id_process(BlendLibReader *reader, ID *id);

static void after_liblink_id_embedded_id_process(BlendLibReader *reader, ID *id)
{

  /* Handle 'private IDs'. */
  bNodeTree *nodetree = ntreeFromID(id);
  if (nodetree != nullptr) {
    after_liblink_id_process(reader, &nodetree->id);

    if (nodetree->owner_id == nullptr) {
      CLOG_WARN(&LOG,
                "NULL owner_id pointer for embedded NodeTree of %s, should never happen",
                id->name);
      nodetree->owner_id = id;
    }
    else if (nodetree->owner_id != id) {
      CLOG_WARN(&LOG,
                "Inconsistent owner_id pointer for embedded NodeTree of %s, should never happen",
                id->name);
      nodetree->owner_id = id;
    }
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != nullptr) {
      after_liblink_id_process(reader, &scene->master_collection->id);

      if (scene->master_collection->owner_id == nullptr) {
        CLOG_WARN(&LOG,
                  "NULL owner_id pointer for embedded Scene Collection of %s, should never happen",
                  id->name);
        scene->master_collection->owner_id = id;
      }
      else if (scene->master_collection->owner_id != id) {
        CLOG_WARN(&LOG,
                  "Inconsistent owner_id pointer for embedded Scene Collection of %s, should "
                  "never happen",
                  id->name);
        scene->master_collection->owner_id = id;
      }
    }
  }
}

static void after_liblink_id_process(BlendLibReader *reader, ID *id)
{
  /* NOTE: WM IDProperties are never written to file, hence they should always be nullptr here. */
  BLI_assert((GS(id->name) != ID_WM) || id->properties == nullptr);

  after_liblink_id_embedded_id_process(reader, id);

  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
  if (id_type->blend_read_after_liblink != nullptr) {
    id_type->blend_read_after_liblink(reader, id);
  }
}

static void direct_link_id_override_property_operation_cb(BlendDataReader *reader, void *data)
{
  IDOverrideLibraryPropertyOperation *opop = static_cast<IDOverrideLibraryPropertyOperation *>(
      data);

  BLO_read_data_address(reader, &opop->subitem_reference_name);
  BLO_read_data_address(reader, &opop->subitem_local_name);

  opop->tag = 0; /* Runtime only. */
}

static void direct_link_id_override_property_cb(BlendDataReader *reader, void *data)
{
  IDOverrideLibraryProperty *op = static_cast<IDOverrideLibraryProperty *>(data);

  BLO_read_data_address(reader, &op->rna_path);

  op->tag = 0; /* Runtime only. */

  BLO_read_list_cb(reader, &op->operations, direct_link_id_override_property_operation_cb);
}

static void direct_link_id_common(
    BlendDataReader *reader, Library *current_library, ID *id, ID *id_old, const int id_tag);

static void direct_link_id_embedded_id(BlendDataReader *reader,
                                       Library *current_library,
                                       ID *id,
                                       ID *id_old)
{
  /* Handle 'private IDs'. */
  bNodeTree **nodetree = BKE_ntree_ptr_from_id(id);
  if (nodetree != nullptr && *nodetree != nullptr) {
    BLO_read_data_address(reader, nodetree);
    direct_link_id_common(reader,
                          current_library,
                          (ID *)*nodetree,
                          id_old != nullptr ? (ID *)ntreeFromID(id_old) : nullptr,
                          0);
    blender::bke::ntreeBlendReadData(reader, id, *nodetree);
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != nullptr) {
      BLO_read_data_address(reader, &scene->master_collection);
      direct_link_id_common(reader,
                            current_library,
                            &scene->master_collection->id,
                            id_old != nullptr ? &((Scene *)id_old)->master_collection->id :
                                                nullptr,
                            0);
      BKE_collection_blend_read_data(reader, scene->master_collection, &scene->id);
    }
  }
}

static int direct_link_id_restore_recalc_exceptions(const ID *id_current)
{
  /* Exception for armature objects, where the pose has direct points to the
   * armature data-block. */
  if (GS(id_current->name) == ID_OB && ((Object *)id_current)->pose) {
    return ID_RECALC_GEOMETRY;
  }

  return 0;
}

static int direct_link_id_restore_recalc(const FileData *fd,
                                         const ID *id_target,
                                         const ID *id_current,
                                         const bool is_identical)
{
  /* These are the evaluations that had not been performed yet at the time the
   * target undo state was written. These need to be done again, since they may
   * flush back changes to the original datablock. */
  int recalc = id_target->recalc;

  if (id_current == nullptr) {
    /* ID does not currently exist in the database, so also will not exist in
     * the dependency graphs. That means it will be newly created and as a
     * result also fully re-evaluated regardless of the recalc flag set here. */
    recalc |= ID_RECALC_ALL;
  }
  else {
    /* If the contents datablock changed, the depsgraph needs to copy the
     * datablock again to ensure it matches the original datablock. */
    if (!is_identical) {
      recalc |= ID_RECALC_SYNC_TO_EVAL;
    }

    /* Special exceptions. */
    recalc |= direct_link_id_restore_recalc_exceptions(id_current);

    /* Evaluations for the current state that have not been performed yet
     * by the time we are performing this undo step. */
    recalc |= id_current->recalc;

    /* Tags that were set between the target state and the current state,
     * that we need to perform again. */
    if (fd->undo_direction == STEP_UNDO) {
      /* Undo: tags from target to the current state. */
      recalc |= id_current->recalc_up_to_undo_push;
    }
    else {
      BLI_assert(fd->undo_direction == STEP_REDO);
      /* Redo: tags from current to the target state. */
      recalc |= id_target->recalc_up_to_undo_push;
    }
  }

  return recalc;
}

static void direct_link_id_common(
    BlendDataReader *reader, Library *current_library, ID *id, ID *id_old, const int id_tag)
{
  if (!BLO_read_data_is_undo(reader)) {
    /* When actually reading a file, we do want to reset/re-generate session UIDS.
     * In undo case, we want to re-use existing ones. */
    id->session_uid = MAIN_ID_SESSION_UID_UNSET;
  }

  if ((id_tag & LIB_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uid_ensure(id);
  }

  id->lib = current_library;
  if (id->lib) {
    /* Always fully clear fake user flag for linked data. */
    id->flag &= ~LIB_FAKEUSER;
  }
  id->us = ID_FAKE_USERS(id);
  id->icon_id = 0;
  id->newid = nullptr; /* Needed because .blend may have been saved with crap value here... */
  id->orig_id = nullptr;
  id->py_instance = nullptr;

  /* Initialize with provided tag. */
  if (BLO_read_data_is_undo(reader)) {
    id->tag = (id_tag & ~LIB_TAG_KEEP_ON_UNDO) | (id->tag & LIB_TAG_KEEP_ON_UNDO);
  }
  else {
    id->tag = id_tag;
  }

  if (ID_IS_LINKED(id)) {
    id->library_weak_reference = nullptr;
  }
  else {
    BLO_read_data_address(reader, &id->library_weak_reference);
  }

  if (id_tag & LIB_TAG_ID_LINK_PLACEHOLDER) {
    /* For placeholder we only need to set the tag and properly initialize generic ID fields above,
     * no further data to read. */
    return;
  }

  BKE_animdata_blend_read_data(reader, id);

  if (id->asset_data) {
    BLO_read_data_address(reader, &id->asset_data);
    BKE_asset_metadata_read(reader, id->asset_data);
    /* Restore runtime asset type info. */
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    id->asset_data->local_type_info = id_type->asset_type_info;
  }

  /* Link direct data of ID properties. */
  if (id->properties) {
    BLO_read_data_address(reader, &id->properties);
    /* this case means the data was written incorrectly, it should not happen */
    IDP_BlendDataRead(reader, &id->properties);
  }

  id->flag &= ~LIB_INDIRECT_WEAK_LINK;

  /* NOTE: It is important to not clear the recalc flags for undo/redo.
   * Preserving recalc flags on redo/undo is the only way to make dependency graph detect
   * that animation is to be evaluated on undo/redo. If this is not enforced by the recalc
   * flags dependency graph does not do animation update to avoid loss of unkeyed changes.,
   * which conflicts with undo/redo of changes to animation data itself.
   *
   * But for regular file load we clear the flag, since the flags might have been changed since
   * the version the file has been saved with. */
  if (!BLO_read_data_is_undo(reader)) {
    id->recalc = 0;
    id->recalc_after_undo_push = 0;
  }
  else if ((reader->fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0) {
    id->recalc = direct_link_id_restore_recalc(reader->fd, id, id_old, false);
    id->recalc_after_undo_push = 0;
  }

  /* Link direct data of overrides. */
  if (id->override_library) {
    BLO_read_data_address(reader, &id->override_library);
    /* Work around file corruption on writing, see #86853. */
    if (id->override_library != nullptr) {
      BLO_read_list_cb(
          reader, &id->override_library->properties, direct_link_id_override_property_cb);
      id->override_library->runtime = nullptr;
    }
  }

  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);
  if (drawdata) {
    BLI_listbase_clear((ListBase *)drawdata);
  }

  /* Handle 'private IDs'. */
  direct_link_id_embedded_id(reader, current_library, id, id_old);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Animation (legacy for version patching)
 * \{ */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Shape Keys
 * \{ */

void blo_do_versions_key_uidgen(Key *key)
{
  key->uidgen = 1;
  LISTBASE_FOREACH (KeyBlock *, block, &key->block) {
    block->uid = key->uidgen++;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Scene
 * \{ */

#ifdef USE_SETSCENE_CHECK
/**
 * A version of #BKE_scene_validate_setscene with special checks for linked libraries.
 */
static bool scene_validate_setscene__liblink(Scene *sce, const int totscene)
{
  Scene *sce_iter;
  int a;

  if (sce->set == nullptr) {
    return true;
  }

  for (a = 0, sce_iter = sce; sce_iter->set; sce_iter = sce_iter->set, a++) {
    /* This runs per library (before each libraries #Main has been joined),
     * so we can't step into other libraries since `totscene` is only for this library.
     *
     * Also, other libraries may not have been linked yet,
     * while we could check #LIB_TAG_NEED_LINK the library pointer check is sufficient. */
    if (sce->id.lib != sce_iter->id.lib) {
      return true;
    }
    if (sce_iter->flag & SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK) {
      return true;
    }

    if (a > totscene) {
      sce->set = nullptr;
      return false;
    }
  }

  return true;
}
#endif

static void lib_link_scenes_check_set(Main *bmain)
{
#ifdef USE_SETSCENE_CHECK
  const int totscene = BLI_listbase_count(&bmain->scenes);
  LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
    if (sce->flag & SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK) {
      sce->flag &= ~SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK;
      if (!scene_validate_setscene__liblink(sce, totscene)) {
        CLOG_WARN(&LOG, "Found cyclic background scene when linking %s", sce->id.name + 2);
      }
    }
  }
#else
  UNUSED_VARS(bmain, totscene);
#endif
}

#undef USE_SETSCENE_CHECK

/** \} */

/* -------------------------------------------------------------------- */

/** \name Read ID: Library
 * \{ */

static void direct_link_library(FileData *fd, Library *lib, Main *main)
{
  Main *newmain;

  /* Make sure we have full path in lib->filepath_abs */
  /* NOTE: Since existing libraries are searched by their absolute path, this has to be generated
   * before the lookup below. Otherwise, in case the stored absolute filepath is not 'correct' (may
   * be empty, or have been stored in a different 'relative path context'), the comparison below
   * will always fail, leading to creating duplicates IDs of a same library. */
  /* TODO: May be worth checking whether comparison below could use `lib->filepath` instead? */
  STRNCPY(lib->filepath_abs, lib->filepath);
  BLI_path_abs(lib->filepath_abs, fd->relabase);
  BLI_path_normalize(lib->filepath_abs);

  /* check if the library was already read */
  LISTBASE_FOREACH (Main *, newmain, fd->mainlist) {
    if (newmain->curlib) {
      if (BLI_path_cmp(newmain->curlib->filepath_abs, lib->filepath_abs) == 0) {
        BLO_reportf_wrap(fd->reports,
                         RPT_WARNING,
                         RPT_("Library '%s', '%s' had multiple instances, save and reload!"),
                         lib->filepath,
                         lib->filepath_abs);

        change_link_placeholder_to_real_ID_pointer(fd->mainlist, fd, lib, newmain->curlib);
        // change_link_placeholder_to_real_ID_pointer_fd(fd, lib, newmain->curlib);

        BLI_remlink(&main->libraries, lib);
        MEM_freeN(lib);

        /* Now, since Blender always expect **latest** Main pointer from fd->mainlist
         * to be the active library Main pointer,
         * where to add all non-library data-blocks found in file next, we have to switch that
         * 'dupli' found Main to latest position in the list!
         * Otherwise, you get weird disappearing linked data on a rather inconsistent basis.
         * See also #53977 for reproducible case. */
        BLI_remlink(fd->mainlist, newmain);
        BLI_addtail(fd->mainlist, newmain);

        return;
      }
    }
  }

  //  printf("direct_link_library: filepath %s\n", lib->filepath);
  //  printf("direct_link_library: filepath_abs %s\n", lib->filepath_abs);

  BlendDataReader reader = {fd};
  BKE_packedfile_blend_read(&reader, &lib->packedfile);

  /* new main */
  newmain = BKE_main_new();
  BLI_addtail(fd->mainlist, newmain);
  newmain->curlib = lib;

  lib->parent = nullptr;

  id_us_ensure_real(&lib->id);
}

/* Always call this once you have loaded new library data to set the relative paths correctly
 * in relation to the blend file. */
static void fix_relpaths_library(const char *basepath, Main *main)
{
  /* #BLO_read_from_memory uses a blank file-path. */
  if (basepath == nullptr || basepath[0] == '\0') {
    LISTBASE_FOREACH (Library *, lib, &main->libraries) {
      /* when loading a linked lib into a file which has not been saved,
       * there is nothing we can be relative to, so instead we need to make
       * it absolute. This can happen when appending an object with a relative
       * link into an unsaved blend file. See #27405.
       * The remap relative option will make it relative again on save - campbell */
      if (BLI_path_is_rel(lib->filepath)) {
        STRNCPY(lib->filepath, lib->filepath_abs);
      }
    }
  }
  else {
    LISTBASE_FOREACH (Library *, lib, &main->libraries) {
      /* Libraries store both relative and abs paths, recreate relative paths,
       * relative to the blend file since indirectly linked libraries will be
       * relative to their direct linked library. */
      if (BLI_path_is_rel(lib->filepath)) { /* if this is relative to begin with? */
        STRNCPY(lib->filepath, lib->filepath_abs);
        BLI_path_rel(lib->filepath, basepath);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Library Data Block
 * \{ */

static ID *create_placeholder(Main *mainvar,
                              const short idcode,
                              const char *idname,
                              const int tag,
                              const bool was_liboverride)
{
  ListBase *lb = which_libbase(mainvar, idcode);
  ID *ph_id = static_cast<ID *>(BKE_libblock_alloc_notest(idcode));

  *((short *)ph_id->name) = idcode;
  BLI_strncpy(ph_id->name + 2, idname, sizeof(ph_id->name) - 2);
  BKE_libblock_init_empty(ph_id);
  ph_id->lib = mainvar->curlib;
  ph_id->tag = tag | LIB_TAG_MISSING;
  ph_id->us = ID_FAKE_USERS(ph_id);
  ph_id->icon_id = 0;

  if (was_liboverride) {
    /* 'Abuse' `LIB_TAG_LIBOVERRIDE_NEED_RESYNC` to mark that placeholder missing linked ID as
     * being a liboverride.
     *
     * This will be used by the liboverride resync process, see #lib_override_library_resync. */
    ph_id->tag |= LIB_TAG_LIBOVERRIDE_NEED_RESYNC;
  }

  BLI_addtail(lb, ph_id);
  id_sort_by_name(lb, ph_id, nullptr);

  if (mainvar->id_map != nullptr) {
    BKE_main_idmap_insert_id(mainvar->id_map, ph_id);
  }

  if ((tag & LIB_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uid_ensure(ph_id);
  }

  return ph_id;
}

static void placeholders_ensure_valid(Main *bmain)
{
  /* Placeholder ObData IDs won't have any material, we have to update their objects for that,
   * otherwise the inconsistency between both will lead to crashes (especially in Eevee?). */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    ID *obdata = static_cast<ID *>(ob->data);
    if (obdata != nullptr && obdata->tag & LIB_TAG_MISSING) {
      BKE_object_materials_test(bmain, ob, obdata);
    }
  }
}

static const char *idtype_alloc_name_get(short id_code)
{
  static const std::array<std::string, INDEX_ID_MAX> id_alloc_names = [] {
    auto n = decltype(id_alloc_names)();
    for (int idtype_index = 0; idtype_index < INDEX_ID_MAX; idtype_index++) {
      const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_idtype_index(idtype_index);
      BLI_assert(idtype_info);
      if (idtype_index == INDEX_ID_NULL) {
        /* #INDEX_ID_NULL returns the #IDType_ID_LINK_PLACEHOLDER type info, here we will rather
         * use it for unknown/invalid ID types. */
        n[size_t(idtype_index)] = "Data from UNKNWOWN ID Type";
      }
      else {
        n[size_t(idtype_index)] = std::string("Data from '") + idtype_info->name + "'";
      }
    }
    return n;
  }();

  const int idtype_index = BKE_idtype_idcode_to_index(id_code);
  if (LIKELY(idtype_index >= 0 && idtype_index < INDEX_ID_MAX)) {
    return id_alloc_names[size_t(idtype_index)].c_str();
  }
  return id_alloc_names[INDEX_ID_NULL].c_str();
}

static bool direct_link_id(FileData *fd, Main *main, const int tag, ID *id, ID *id_old)
{
  BlendDataReader reader = {fd};

  /* Read part of datablock that is common between real and embedded datablocks. */
  direct_link_id_common(&reader, main->curlib, id, id_old, tag);

  if (tag & LIB_TAG_ID_LINK_PLACEHOLDER) {
    /* For placeholder we only need to set the tag, no further data to read. */
    id->tag = tag;
    return true;
  }

  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
  if (id_type->blend_read_data != nullptr) {
    id_type->blend_read_data(&reader, id);
  }

  /* XXX Very weakly handled currently, see comment in read_libblock() before trying to
   * use it for anything new. */
  bool success = true;

  switch (GS(id->name)) {
    case ID_SCR:
      success = BKE_screen_blend_read_data(&reader, (bScreen *)id);
      break;
    case ID_LI:
      direct_link_library(fd, (Library *)id, main);
      break;
    default:
      /* Do nothing. Handled by IDTypeInfo callback. */
      break;
  }

  /* try to restore (when undoing) or clear ID's cache pointers. */
  if (id_type->foreach_cache != nullptr) {
    BKE_idtype_id_foreach_cache(
        id, blo_cache_storage_entry_restore_in_new, reader.fd->cache_storage);
  }

  return success;
}

/* Read all data associated with a datablock into datamap. */
static BHead *read_data_into_datamap(FileData *fd, BHead *bhead, const char *allocname)
{
  bhead = blo_bhead_next(fd, bhead);

  while (bhead && bhead->code == BLO_CODE_DATA) {
    /* The code below is useful for debugging leaks in data read from the blend file.
     * Without this the messages only tell us what ID-type the memory came from,
     * eg: `Data from OB len 64`, see #dataname.
     * With the code below we get the struct-name to help tracking down the leak.
     * This is kept disabled as the #malloc for the text always leaks memory. */
#if 0
    if (bhead->SDNAnr == 0) {
      /* The data type here is unclear because #writedata sets SDNAnr to 0. */
      allocname = "likely raw data";
    }
    else {
      SDNA_Struct *sp = fd->filesdna->structs[bhead->SDNAnr];
      allocname = fd->filesdna->types[sp->type];
      size_t allocname_size = strlen(allocname) + 1;
      char *allocname_buf = static_cast<char *>(malloc(allocname_size));
      memcpy(allocname_buf, allocname, allocname_size);
      allocname = allocname_buf;
    }
#endif

    void *data = read_struct(fd, bhead, allocname);
    if (data) {
      oldnewmap_insert(fd->datamap, bhead->old, data, 0);
    }

    bhead = blo_bhead_next(fd, bhead);
  }

  return bhead;
}

/* Verify if the datablock and all associated data is identical. */
static bool read_libblock_is_identical(FileData *fd, BHead *bhead)
{
  /* Test ID itself. */
  if (bhead->len && !BHEADN_FROM_BHEAD(bhead)->is_memchunk_identical) {
    return false;
  }

  /* Test any other data that is part of ID (logic must match read_data_into_datamap). */
  bhead = blo_bhead_next(fd, bhead);

  while (bhead && bhead->code == BLO_CODE_DATA) {
    if (bhead->len && !BHEADN_FROM_BHEAD(bhead)->is_memchunk_identical) {
      return false;
    }

    bhead = blo_bhead_next(fd, bhead);
  }

  return true;
}

/* Re-use the whole 'noundo' local IDs by moving them from old to new main. Linked ones are handled
 * separately together with their libraries.
 *
 * NOTE: While in theory Library IDs (and their related linked IDs) are also 'noundo' data, in
 * practice they need to be handled separately, to ensure that their order in the new bmain list
 * matches the one from the read blend-file. Reading linked 'placeholder' entries in a memfile
 * relies on current library being the last item in the new main list. */
static void read_undo_reuse_noundo_local_ids(FileData *fd)
{
  Main *old_bmain = static_cast<Main *>(fd->old_mainlist->first);
  ListBase *lbarray[INDEX_ID_MAX];

  BLI_assert(old_bmain->curlib == nullptr);
  BLI_assert(BLI_listbase_count_at_most(fd->mainlist, 2) == 1);

  int i = set_listbasepointers(old_bmain, lbarray);
  while (i--) {
    if (BLI_listbase_is_empty(lbarray[i])) {
      continue;
    }

    /* Only move 'noundo' local IDs. */
    ID *id = static_cast<ID *>(lbarray[i]->first);
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    if ((id_type->flags & IDTYPE_FLAGS_NO_MEMFILE_UNDO) == 0) {
      continue;
    }

    Main *new_bmain = static_cast<Main *>(fd->mainlist->first);
    ListBase *new_lb = which_libbase(new_bmain, id_type->id_code);
    BLI_assert(BLI_listbase_is_empty(new_lb));
    BLI_movelisttolist(new_lb, lbarray[i]);

    /* Update mappings accordingly. */
    LISTBASE_FOREACH (ID *, id_iter, new_lb) {
      BKE_main_idmap_insert_id(fd->new_idmap_uid, id_iter);
      id_iter->tag |= LIB_TAG_UNDO_OLD_ID_REUSED_NOUNDO;
    }
  }
}

static void read_undo_move_libmain_data(
    FileData *fd, Main *new_main, Main *old_main, Main *libmain, BHead *bhead)
{
  Library *curlib = libmain->curlib;

  BLI_remlink(fd->old_mainlist, libmain);
  BLI_remlink_safe(&old_main->libraries, libmain->curlib);
  BLI_addtail(fd->mainlist, libmain);
  BLI_addtail(&new_main->libraries, libmain->curlib);

  curlib->id.tag |= LIB_TAG_UNDO_OLD_ID_REUSED_NOUNDO;
  BKE_main_idmap_insert_id(fd->new_idmap_uid, &curlib->id);
  if (bhead != nullptr) {
    oldnewmap_lib_insert(fd, bhead->old, &curlib->id, GS(curlib->id.name));
  }

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (libmain, id_iter) {
    BKE_main_idmap_insert_id(fd->new_idmap_uid, id_iter);
  }
  FOREACH_MAIN_ID_END;
}

/* For undo, restore matching library datablock from the old main. */
static bool read_libblock_undo_restore_library(
    FileData *fd, Main *new_main, const ID *id, ID *id_old, BHead *bhead)
{
  /* In undo case, most libraries and linked data should be kept as is from previous state
   * (see BLO_read_from_memfile).
   * However, some needed by the snapshot being read may have been removed in previous one,
   * and would go missing.
   * This leads e.g. to disappearing objects in some undo/redo case, see #34446.
   * That means we have to carefully check whether current lib or
   * libdata already exits in old main, if it does we merely copy it over into new main area,
   * otherwise we have to do a full read of that bhead... */
  CLOG_INFO(&LOG_UNDO, 2, "UNDO: restore library %s", id->name);

  if (id_old == nullptr) {
    CLOG_INFO(&LOG_UNDO, 2, "    -> NO match");
    return false;
  }

  Main *libmain = static_cast<Main *>(fd->old_mainlist->first);
  /* Skip oldmain itself... */
  for (libmain = libmain->next; libmain; libmain = libmain->next) {
    if (&libmain->curlib->id == id_old) {
      Main *old_main = static_cast<Main *>(fd->old_mainlist->first);
      CLOG_INFO(&LOG_UNDO,
                2,
                "    compare with %s -> match (existing libpath: %s)",
                libmain->curlib ? libmain->curlib->id.name : "<none>",
                libmain->curlib ? libmain->curlib->filepath_abs : "<none>");
      /* In case of a library, we need to re-add its main to fd->mainlist,
       * because if we have later a missing ID_LINK_PLACEHOLDER,
       * we need to get the correct lib it is linked to!
       * Order is crucial, we cannot bulk-add it in BLO_read_from_memfile()
       * like it used to be. */
      read_undo_move_libmain_data(fd, new_main, old_main, libmain, bhead);
      return true;
    }
  }

  return false;
}

static ID *library_id_is_yet_read(FileData *fd, Main *mainvar, BHead *bhead);

/* For undo, restore existing linked datablock from the old main.
 *
 * Note that IDs from existing libs have already been moved into the new main when their (local)
 * ID_LI library ID was handled by #read_libblock_undo_restore_library, so this function has very
 * little to do. */
static bool read_libblock_undo_restore_linked(
    FileData *fd, Main *libmain, const ID *id, ID **r_id_old, BHead *bhead)
{
  CLOG_INFO(&LOG_UNDO, 2, "UNDO: restore linked datablock %s", id->name);

  if (*r_id_old == nullptr) {
    /* If the linked ID had to be re-read at some point, its session_uid may not be the same as
     * its reference stored in the memfile anymore. Do a search by name then. */
    *r_id_old = library_id_is_yet_read(fd, libmain, bhead);

    if (*r_id_old == nullptr) {
      CLOG_INFO(&LOG_UNDO,
                2,
                "    from %s (%s): NOT found",
                libmain->curlib ? libmain->curlib->id.name : "<nullptr>",
                libmain->curlib ? libmain->curlib->filepath : "<nullptr>");
      return false;
    }

    CLOG_INFO(&LOG_UNDO,
              2,
              "    from %s (%s): found by name",
              libmain->curlib ? libmain->curlib->id.name : "<nullptr>",
              libmain->curlib ? libmain->curlib->filepath : "<nullptr>");
    /* The Library ID 'owning' this linked ID should already have been moved to new main by a call
     * to #read_libblock_undo_restore_library. */
    BLI_assert(*r_id_old == static_cast<ID *>(BKE_main_idmap_lookup_uid(
                                fd->new_idmap_uid, (*r_id_old)->session_uid)));
  }
  else {
    CLOG_INFO(&LOG_UNDO,
              2,
              "    from %s (%s): found by session_uid",
              libmain->curlib ? libmain->curlib->id.name : "<nullptr>",
              libmain->curlib ? libmain->curlib->filepath : "<nullptr>");
    /* The Library ID 'owning' this linked ID should already have been moved to new main by a call
     * to #read_libblock_undo_restore_library. */
    BLI_assert(*r_id_old ==
               static_cast<ID *>(BKE_main_idmap_lookup_uid(fd->new_idmap_uid, id->session_uid)));
  }

  oldnewmap_lib_insert(fd, bhead->old, *r_id_old, GS((*r_id_old)->name));

  /* No need to do anything else for ID_LINK_PLACEHOLDER, it's assumed
   * already present in its lib's main. */
  return true;
}

/* For undo, restore unchanged local datablock from old main. */
static void read_libblock_undo_restore_identical(
    FileData *fd, Main *main, const ID * /*id*/, ID *id_old, BHead *bhead, const int id_tag)
{
  BLI_assert((fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0);
  BLI_assert(id_old != nullptr);

  /* Do not add LIB_TAG_NEW here, this should not be needed/used in undo case anyway (as
   * this is only for do_version-like code), but for sake of consistency, and also because
   * it will tell us which ID is re-used from old Main, and which one is actually newly read. */
  /* Also do not add LIB_TAG_NEED_LINK, this ID will never be re-liblinked, hence that tag will
   * never be cleared, leading to critical issue in link/append code. */
  /* Some tags need to be preserved here. */
  id_old->tag = ((id_tag | LIB_TAG_UNDO_OLD_ID_REUSED_UNCHANGED) & ~LIB_TAG_KEEP_ON_UNDO) |
                (id_old->tag & LIB_TAG_KEEP_ON_UNDO);
  id_old->lib = main->curlib;
  id_old->us = ID_FAKE_USERS(id_old);
  /* Do not reset id->icon_id here, memory allocated for it remains valid. */
  /* Needed because .blend may have been saved with crap value here... */
  id_old->newid = nullptr;
  id_old->orig_id = nullptr;

  const short idcode = GS(id_old->name);
  Main *old_bmain = static_cast<Main *>(fd->old_mainlist->first);
  ListBase *old_lb = which_libbase(old_bmain, idcode);
  ListBase *new_lb = which_libbase(main, idcode);
  BLI_remlink(old_lb, id_old);
  BLI_addtail(new_lb, id_old);

  /* Recalc flags, mostly these just remain as they are. */
  id_old->recalc |= direct_link_id_restore_recalc_exceptions(id_old);
  id_old->recalc_after_undo_push = 0;

  /* Insert into library map for lookup by newly read datablocks (with pointer value bhead->old).
   * Note that existing datablocks in memory (which pointer value would be id_old) are not
   * remapped, so no need to store this info here. */
  oldnewmap_lib_insert(fd, bhead->old, id_old, bhead->code);

  BKE_main_idmap_insert_id(fd->new_idmap_uid, id_old);

  if (GS(id_old->name) == ID_OB) {
    Object *ob = (Object *)id_old;
    /* For undo we stay in object mode during undo presses, so keep editmode disabled for re-used
     * data-blocks too. */
    ob->mode &= ~OB_MODE_EDIT;
  }
}

/* For undo, store changed datablock at old address. */
static void read_libblock_undo_restore_at_old_address(FileData *fd, Main *main, ID *id, ID *id_old)
{
  /* During memfile undo, if an ID changed and we cannot directly re-use existing one from old
   * bmain, we do a full read of the new id from the memfile, and then fully swap its content
   * with the old id. This allows us to keep the same pointer even for modified data, which
   * helps reducing further detected changes by the depsgraph (since unchanged IDs remain fully
   * unchanged, even if they are using/pointing to a changed one). */
  BLI_assert((fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0);
  BLI_assert(id_old != nullptr);

  const short idcode = GS(id->name);

  Main *old_bmain = static_cast<Main *>(fd->old_mainlist->first);
  ListBase *old_lb = which_libbase(old_bmain, idcode);
  ListBase *new_lb = which_libbase(main, idcode);
  BLI_remlink(old_lb, id_old);
  BLI_remlink(new_lb, id);

  /* We do need remapping of internal pointers to the ID itself here.
   *
   * Passing a null #Main means that not all potential runtime data (like collections' parent
   * pointers etc.) will be up-to-date. However, this should not be a problem here, since these
   * data are re-generated later in file-read process anyway. */
  BKE_lib_id_swap_full(nullptr,
                       id,
                       id_old,
                       true,
                       (ID_REMAP_NO_ORIG_POINTERS_ACCESS | ID_REMAP_SKIP_NEVER_NULL_USAGE |
                        ID_REMAP_SKIP_UPDATE_TAGGING | ID_REMAP_SKIP_USER_REFCOUNT));

  /* Special temporary usage of this pointer, necessary for the `undo_preserve` call after
   * lib-linking to restore some data that should never be affected by undo, e.g. the 3D cursor of
   * #Scene. */
  id_old->orig_id = id;
  id_old->tag |= LIB_TAG_UNDO_OLD_ID_REREAD_IN_PLACE | LIB_TAG_NEED_LINK;

  BLI_addtail(new_lb, id_old);
  BLI_addtail(old_lb, id);
}

static bool read_libblock_undo_restore(
    FileData *fd, Main *main, BHead *bhead, int id_tag, ID **r_id_old)
{
  BLI_assert(fd->old_idmap_uid != nullptr);

  /* Get pointer to memory of new ID that we will be reading. */
  const ID *id = static_cast<const ID *>(peek_struct_undo(fd, bhead));
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);

  const bool do_partial_undo = (fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0;
#ifndef NDEBUG
  if (do_partial_undo && (bhead->code != ID_LINK_PLACEHOLDER)) {
    /* This code should only ever be reached for local data-blocks. */
    BLI_assert(main->curlib == nullptr);
  }
#endif

  /* Find the 'current' existing ID we want to reuse instead of the one we
   * would read from the undo memfile. */
  ID *id_old = (fd->old_idmap_uid != nullptr) ?
                   BKE_main_idmap_lookup_uid(fd->old_idmap_uid, id->session_uid) :
                   nullptr;

  if (bhead->code == ID_LI) {
    /* Restore library datablock, if possible. */
    if (read_libblock_undo_restore_library(fd, main, id, id_old, bhead)) {
      return true;
    }
  }
  else if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Restore linked datablock. */
    if (read_libblock_undo_restore_linked(fd, main, id, &id_old, bhead)) {
      return true;
    }
  }
  else if (id_type->flags & IDTYPE_FLAGS_NO_MEMFILE_UNDO) {
    CLOG_INFO(
        &LOG_UNDO, 2, "UNDO: skip restore datablock %s, 'NO_MEMFILE_UNDO' type of ID", id->name);

    /* If that local noundo ID still exists currently, the call to
     * #read_undo_reuse_noundo_local_ids at the beginning of #blo_read_file_internal will already
     * have moved it into the new main, and populated accordingly the new_idmap_uid.
     *
     * If this is the case, it can also be remapped for newly read data. Otherwise, this is 'lost'
     * data that cannot be restored on undo, so no remapping should exist for it in the ID
     * oldnewmap. */
    if (id_old) {
      BLI_assert(id_old ==
                 static_cast<ID *>(BKE_main_idmap_lookup_uid(fd->new_idmap_uid, id->session_uid)));
      oldnewmap_lib_insert(fd, bhead->old, id_old, bhead->code);
    }
    return true;
  }

  if (!do_partial_undo) {
    CLOG_INFO(&LOG_UNDO,
              2,
              "UNDO: read %s (uid %u) -> no partial undo, always read at new address",
              id->name,
              id->session_uid);
    return false;
  }

  /* Restore local datablocks. */
  if (id_old != nullptr && read_libblock_is_identical(fd, bhead)) {
    /* Local datablock was unchanged, restore from the old main. */
    CLOG_INFO(&LOG_UNDO,
              2,
              "UNDO: read %s (uid %u) -> keep identical datablock",
              id->name,
              id->session_uid);

    read_libblock_undo_restore_identical(fd, main, id, id_old, bhead, id_tag);

    *r_id_old = id_old;
    return true;
  }
  if (id_old != nullptr) {
    /* Local datablock was changed. Restore at the address of the old datablock. */
    CLOG_INFO(&LOG_UNDO,
              2,
              "UNDO: read %s (uid %u) -> read to old existing address",
              id->name,
              id->session_uid);
    *r_id_old = id_old;
    return false;
  }

  /* Local datablock does not exist in the undo step, so read from scratch. */
  CLOG_INFO(
      &LOG_UNDO, 2, "UNDO: read %s (uid %u) -> read at new address", id->name, id->session_uid);
  return false;
}

/* This routine reads a datablock and its direct data, and advances bhead to
 * the next datablock. For library linked datablocks, only a placeholder will
 * be generated, to be replaced in read_library_linked_ids.
 *
 * When reading for undo, libraries, linked datablocks and unchanged datablocks
 * will be restored from the old database. Only new or changed datablocks will
 * actually be read. */
static BHead *read_libblock(FileData *fd,
                            Main *main,
                            BHead *bhead,
                            int id_tag,
                            const bool placeholder_set_indirect_extern,
                            ID **r_id)
{
  const bool do_partial_undo = (fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0;

  /* First attempt to restore existing datablocks for undo.
   * When datablocks are changed but still exist, we restore them at the old
   * address and inherit recalc flags for the dependency graph. */
  ID *id_old = nullptr;
  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    if (read_libblock_undo_restore(fd, main, bhead, id_tag, &id_old)) {
      if (r_id) {
        *r_id = id_old;
      }
      if (main->id_map != nullptr && id_old != nullptr) {
        BKE_main_idmap_insert_id(main->id_map, id_old);
      }

      return blo_bhead_next(fd, bhead);
    }
  }

  /* Read libblock struct. */
  ID *id = static_cast<ID *>(read_struct(fd, bhead, "lib block"));
  if (id == nullptr) {
    if (r_id) {
      *r_id = nullptr;
    }
    return blo_bhead_next(fd, bhead);
  }

  /* Determine ID type and add to main database list. */
  const short idcode = GS(id->name);
  ListBase *lb = which_libbase(main, idcode);
  if (lb == nullptr) {
    /* Unknown ID type. */
    CLOG_WARN(&LOG, "Unknown id code '%c%c'", (idcode & 0xff), (idcode >> 8));
    MEM_freeN(id);
    if (r_id) {
      *r_id = nullptr;
    }
    return blo_bhead_next(fd, bhead);
  }

  /* NOTE: id must be added to the list before direct_link_id(), since
   * direct_link_library() may remove it from there in case of duplicates. */
  BLI_addtail(lb, id);

  /* Insert into library map for lookup by newly read datablocks (with pointer value bhead->old).
   * Note that existing datablocks in memory (which pointer value would be id_old) are not remapped
   * remapped anymore, so no need to store this info here. */
  ID *id_target = (do_partial_undo && id_old != nullptr) ? id_old : id;
  oldnewmap_lib_insert(fd, bhead->old, id_target, bhead->code);

  if (r_id) {
    *r_id = id_target;
  }

  /* Set tag for new datablock to indicate lib linking and versioning needs
   * to be done still. */
  id_tag |= (LIB_TAG_NEED_LINK | LIB_TAG_NEW);

  if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Read placeholder for linked datablock. */
    id_tag |= LIB_TAG_ID_LINK_PLACEHOLDER;

    if (placeholder_set_indirect_extern) {
      if (id->flag & LIB_INDIRECT_WEAK_LINK) {
        id_tag |= LIB_TAG_INDIRECT;
      }
      else {
        id_tag |= LIB_TAG_EXTERN;
      }
    }

    direct_link_id(fd, main, id_tag, id, id_old);

    if (main->id_map != nullptr) {
      BKE_main_idmap_insert_id(main->id_map, id);
    }

    return blo_bhead_next(fd, bhead);
  }

  /* Read datablock contents.
   * Use convenient malloc name for debugging and better memory link prints. */
  const char *allocname = idtype_alloc_name_get(idcode);
  bhead = read_data_into_datamap(fd, bhead, allocname);
  const bool success = direct_link_id(fd, main, id_tag, id, id_old);
  oldnewmap_clear(fd->datamap);

  if (!success) {
    /* XXX This is probably working OK currently given the very limited scope of that flag.
     * However, it is absolutely **not** handled correctly: it is freeing an ID pointer that has
     * been added to the fd->libmap mapping, which in theory could lead to nice crashes...
     * This should be properly solved at some point. */
    BKE_id_free(main, id);
    if (r_id != nullptr) {
      *r_id = nullptr;
    }
  }
  else {
    if (do_partial_undo && id_old != nullptr) {
      /* For undo, store contents read into id at id_old. */
      read_libblock_undo_restore_at_old_address(fd, main, id, id_old);
    }
    if (fd->new_idmap_uid != nullptr) {
      BKE_main_idmap_insert_id(fd->new_idmap_uid, id_target);
    }
    if (main->id_map != nullptr) {
      BKE_main_idmap_insert_id(main->id_map, id_target);
    }
  }

  return bhead;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Asset Data
 * \{ */

BHead *blo_read_asset_data_block(FileData *fd, BHead *bhead, AssetMetaData **r_asset_data)
{
  BLI_assert(blo_bhead_is_id_valid_type(bhead));

  bhead = read_data_into_datamap(fd, bhead, "asset-data read");

  BlendDataReader reader = {fd};
  BLO_read_data_address(&reader, r_asset_data);
  BKE_asset_metadata_read(&reader, *r_asset_data);

  oldnewmap_clear(fd->datamap);

  return bhead;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Global Data
 * \{ */

/* NOTE: this has to be kept for reading older files... */
/* also version info is written here */
static BHead *read_global(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
  FileGlobal *fg = static_cast<FileGlobal *>(read_struct(fd, bhead, "Global"));

  /* NOTE: `bfd->main->versionfile` is supposed to have already been set from `fd->fileversion`
   * beforehand by calling code. */
  bfd->main->subversionfile = fg->subversion;
  bfd->main->has_forward_compatibility_issues = !MAIN_VERSION_FILE_OLDER_OR_EQUAL(
      bfd->main, BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);

  bfd->main->minversionfile = fg->minversion;
  bfd->main->minsubversionfile = fg->minsubversion;

  bfd->main->build_commit_timestamp = fg->build_commit_timestamp;
  STRNCPY(bfd->main->build_hash, fg->build_hash);

  bfd->fileflags = fg->fileflags;
  bfd->globalf = fg->globalf;

  /* Note: since 88b24bc6bb, `fg->filepath` is only written for crash recovery and autosave files,
   * so only overwrite `fd->relabase` if it is not empty, in case a regular blendfile is opened
   * through one of the 'recover' operators.
   *
   * In all other cases, the path is just set to the current path of the blendfile being read, so
   * there is no need to handle anymore older files (pre-2.65) that did not store (correctly) their
   * path. */
  if (G.fileflags & G_FILE_RECOVER_READ) {
    if (fg->filepath[0] != '\0') {
      STRNCPY(fd->relabase, fg->filepath);
      /* Used to set expected original filepath in read Main, instead of the path of the recovery
       * file itself. */
      STRNCPY(bfd->filepath, fg->filepath);
    }
  }

  bfd->curscreen = fg->curscreen;
  bfd->curscene = fg->curscene;
  bfd->cur_view_layer = fg->cur_view_layer;

  MEM_freeN(fg);

  fd->globalf = bfd->globalf;
  fd->fileflags = bfd->fileflags;

  return blo_bhead_next(fd, bhead);
}

/* NOTE: this has to be kept for reading older files... */
static void link_global(FileData *fd, BlendFileData *bfd)
{
  bfd->cur_view_layer = static_cast<ViewLayer *>(
      blo_read_get_new_globaldata_address(fd, bfd->cur_view_layer));
  bfd->curscreen = static_cast<bScreen *>(newlibadr(fd, nullptr, false, bfd->curscreen));
  bfd->curscene = static_cast<Scene *>(newlibadr(fd, nullptr, false, bfd->curscene));
  /* this happens in files older than 2.35 */
  if (bfd->curscene == nullptr) {
    if (bfd->curscreen) {
      bfd->curscene = bfd->curscreen->scene;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Versioning
 * \{ */

static void do_versions_userdef(FileData * /*fd*/, BlendFileData *bfd)
{
  UserDef *user = bfd->user;

  if (user == nullptr) {
    return;
  }

  blo_do_versions_userdef(user);
}

static void do_versions(FileData *fd, Library *lib, Main *main)
{
  /* WATCH IT!!!: pointers from libdata have not been converted */

  /* Don't allow versioning to create new data-blocks. */
  main->is_locked_for_linking = true;

  if (G.debug & G_DEBUG) {
    char build_commit_datetime[32];
    time_t temp_time = main->build_commit_timestamp;
    tm *tm = (temp_time) ? gmtime(&temp_time) : nullptr;
    if (LIKELY(tm)) {
      strftime(build_commit_datetime, sizeof(build_commit_datetime), "%Y-%m-%d %H:%M", tm);
    }
    else {
      STRNCPY(build_commit_datetime, "unknown");
    }

    CLOG_INFO(&LOG, 0, "Read file %s", fd->relabase);
    CLOG_INFO(&LOG,
              0,
              "    Version %d sub %d date %s hash %s",
              main->versionfile,
              main->subversionfile,
              build_commit_datetime,
              main->build_hash);
  }

  if (!main->is_read_invalid) {
    blo_do_versions_pre250(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_250(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_260(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_270(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_280(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_290(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_300(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_400(fd, lib, main);
  }

  /* WATCH IT!!!: pointers from libdata have not been converted yet here! */
  /* WATCH IT 2!: Userdef struct init see do_versions_userdef() above! */

  /* don't forget to set version number in BKE_blender_version.h! */

  main->is_locked_for_linking = false;
}

static void do_versions_after_linking(FileData *fd, Main *main)
{
  BLI_assert(fd != nullptr);

  CLOG_INFO(&LOG,
            2,
            "Processing %s (%s), %d.%d",
            main->curlib ? main->curlib->filepath : main->filepath,
            main->curlib ? "LIB" : "MAIN",
            main->versionfile,
            main->subversionfile);

  /* Don't allow versioning to create new data-blocks. */
  main->is_locked_for_linking = true;

  if (!main->is_read_invalid) {
    do_versions_after_linking_250(main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_260(main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_270(main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_280(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_290(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_300(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_400(fd, main);
  }

  main->is_locked_for_linking = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Library Data Block (all)
 * \{ */

static int lib_link_cb(LibraryIDLinkCallbackData *cb_data)
{
  /* Embedded IDs are not known by lib_link code, so they would be remapped to `nullptr`. But there
   * is no need to process them anyway, as they are already handled during the 'read_data' phase.
   *
   * NOTE: Some external non-owning pointers to embedded IDs (like the node-tree pointers of the
   * Node editor) will not be detected as embedded ones though at 'lib_link' stage (because their
   * source data cannot be accessed). This is handled on a case-by-case basis in 'after_lib_link'
   * validation code. */
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    return IDWALK_RET_NOP;
  }

  /* Explicitly requested to be ignored during readfile processing. Means the read_data code
   * already handled this pointer. Typically, the 'owner_id' pointer of an embedded ID. */
  if (cb_data->cb_flag & IDWALK_CB_READFILE_IGNORE) {
    return IDWALK_RET_NOP;
  }

  BlendLibReader *reader = static_cast<BlendLibReader *>(cb_data->user_data);
  ID **id_ptr = cb_data->id_pointer;
  ID *owner_id = cb_data->owner_id;

  *id_ptr = BLO_read_get_new_id_address(reader, owner_id, ID_IS_LINKED(owner_id), *id_ptr);

  return IDWALK_RET_NOP;
}

static void lib_link_all(FileData *fd, Main *bmain)
{
  BlendLibReader reader = {fd, bmain};

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);

    if ((id->tag & (LIB_TAG_UNDO_OLD_ID_REUSED_UNCHANGED | LIB_TAG_UNDO_OLD_ID_REUSED_NOUNDO)) !=
        0)
    {
      BLI_assert(fd->flags & FD_FLAGS_IS_MEMFILE);
      /* This ID has been re-used from 'old' bmain. Since it was therefore unchanged across
       * current undo step, and old IDs re-use their old memory address, we do not need to liblink
       * it at all. */
      BLI_assert((id->tag & LIB_TAG_NEED_LINK) == 0);

      /* Some data that should be persistent, like the 3DCursor or the tool settings, are
       * stored in IDs affected by undo, like Scene. So this requires some specific handling. */
      /* NOTE: even though the ID may have been detected as unchanged, the 'undo_preserve' may have
       * to actually change some of its ID pointers, it's e.g. the case with Scene's tool-settings
       * Brush/Palette pointers. This is the case where both new and old ID may be the same. */
      if (id_type->blend_read_undo_preserve != nullptr) {
        BLI_assert(fd->flags & FD_FLAGS_IS_MEMFILE);
        id_type->blend_read_undo_preserve(&reader, id, id->orig_id ? id->orig_id : id);
      }
      continue;
    }

    if ((id->tag & LIB_TAG_NEED_LINK) != 0) {
      /* Not all original pointer values can be considered as valid.
       * Handling of DNA deprecated data should never be needed in undo case. */
      const int flag = IDWALK_NO_ORIG_POINTERS_ACCESS | IDWALK_INCLUDE_UI |
                       ((fd->flags & FD_FLAGS_IS_MEMFILE) ? 0 : IDWALK_DO_DEPRECATED_POINTERS);
      BKE_library_foreach_ID_link(nullptr, id, lib_link_cb, &reader, flag);

      after_liblink_id_process(&reader, id);

      id->tag &= ~LIB_TAG_NEED_LINK;
    }

    /* Some data that should be persistent, like the 3DCursor or the tool settings, are
     * stored in IDs affected by undo, like Scene. So this requires some specific handling. */
    if (id_type->blend_read_undo_preserve != nullptr && id->orig_id != nullptr) {
      BLI_assert(fd->flags & FD_FLAGS_IS_MEMFILE);
      id_type->blend_read_undo_preserve(&reader, id, id->orig_id);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Cleanup `ID.orig_id`, this is now reserved for depsgraph/copy-on-eval usage only. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    id->orig_id = nullptr;
  }
  FOREACH_MAIN_ID_END;

#ifndef NDEBUG
  /* Double check we do not have any 'need link' tag remaining, this should never be the case once
   * this function has run. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BLI_assert((id->tag & LIB_TAG_NEED_LINK) == 0);
  }
  FOREACH_MAIN_ID_END;
#endif
}

/**
 * Checks to perform after `lib_link_all`.
 * Those operations cannot perform properly in a split bmain case, since some data from other
 * bmain's (aka libraries) may not have been processed yet.
 */
static void after_liblink_merged_bmain_process(Main *bmain, BlendFileReadReport *reports)
{
  /* We only expect a merged Main here, not a split one. */
  BLI_assert((bmain->prev == nullptr) && (bmain->next == nullptr));

  if (!BKE_main_namemap_validate_and_fix(bmain)) {
    BKE_report(
        reports ? reports->reports : nullptr,
        RPT_ERROR,
        "Critical blend-file corruption: Conflicts and/or otherwise invalid data-blocks names "
        "(see console for details)");
  }

  /* Check for possible cycles in scenes' 'set' background property. */
  lib_link_scenes_check_set(bmain);

  /* We could integrate that to mesh/curve/lattice lib_link, but this is really cheap process,
   * so simpler to just use it directly in this single call. */
  BLO_main_validate_shapekeys(bmain, reports ? reports->reports : nullptr);

  /* We have to rebuild that runtime information *after* all data-blocks have been properly linked.
   */
  BKE_main_collections_parent_relations_rebuild(bmain);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read User Preferences
 * \{ */

static void direct_link_keymapitem(BlendDataReader *reader, wmKeyMapItem *kmi)
{
  BLO_read_data_address(reader, &kmi->properties);
  IDP_BlendDataRead(reader, &kmi->properties);
  kmi->ptr = nullptr;
  kmi->flag &= ~KMI_UPDATE;
}

static BHead *read_userdef(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
  UserDef *user;
  bfd->user = user = static_cast<UserDef *>(read_struct(fd, bhead, "user def"));

  /* User struct has separate do-version handling */
  user->versionfile = bfd->main->versionfile;
  user->subversionfile = bfd->main->subversionfile;

  /* read all data into fd->datamap */
  bhead = read_data_into_datamap(fd, bhead, "user def");

  BlendDataReader reader_ = {fd};
  BlendDataReader *reader = &reader_;

  BLO_read_list(reader, &user->themes);
  BLO_read_list(reader, &user->user_keymaps);
  BLO_read_list(reader, &user->user_keyconfig_prefs);
  BLO_read_list(reader, &user->user_menus);
  BLO_read_list(reader, &user->addons);
  BLO_read_list(reader, &user->autoexec_paths);
  BLO_read_list(reader, &user->script_directories);
  BLO_read_list(reader, &user->asset_libraries);
  BLO_read_list(reader, &user->extension_repos);
  BLO_read_list(reader, &user->asset_shelves_settings);

  LISTBASE_FOREACH (wmKeyMap *, keymap, &user->user_keymaps) {
    keymap->modal_items = nullptr;
    keymap->poll = nullptr;
    keymap->flag &= ~KEYMAP_UPDATE;

    BLO_read_list(reader, &keymap->diff_items);
    BLO_read_list(reader, &keymap->items);

    LISTBASE_FOREACH (wmKeyMapDiffItem *, kmdi, &keymap->diff_items) {
      BLO_read_data_address(reader, &kmdi->remove_item);
      BLO_read_data_address(reader, &kmdi->add_item);

      if (kmdi->remove_item) {
        direct_link_keymapitem(reader, kmdi->remove_item);
      }
      if (kmdi->add_item) {
        direct_link_keymapitem(reader, kmdi->add_item);
      }
    }

    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
      direct_link_keymapitem(reader, kmi);
    }
  }

  LISTBASE_FOREACH (wmKeyConfigPref *, kpt, &user->user_keyconfig_prefs) {
    BLO_read_data_address(reader, &kpt->prop);
    IDP_BlendDataRead(reader, &kpt->prop);
  }

  LISTBASE_FOREACH (bUserMenu *, um, &user->user_menus) {
    BLO_read_list(reader, &um->items);
    LISTBASE_FOREACH (bUserMenuItem *, umi, &um->items) {
      if (umi->type == USER_MENU_TYPE_OPERATOR) {
        bUserMenuItem_Op *umi_op = (bUserMenuItem_Op *)umi;
        BLO_read_data_address(reader, &umi_op->prop);
        IDP_BlendDataRead(reader, &umi_op->prop);
      }
    }
  }

  LISTBASE_FOREACH (bAddon *, addon, &user->addons) {
    BLO_read_data_address(reader, &addon->prop);
    IDP_BlendDataRead(reader, &addon->prop);
  }

  LISTBASE_FOREACH (bUserAssetShelfSettings *, shelf_settings, &user->asset_shelves_settings) {
    BLO_read_list(reader, &shelf_settings->enabled_catalog_paths);
    LISTBASE_FOREACH (LinkData *, path_link, &shelf_settings->enabled_catalog_paths) {
      BLO_read_data_address(reader, &path_link->data);
    }
  }

  /* XXX */
  user->uifonts.first = user->uifonts.last = nullptr;

  BLO_read_list(reader, &user->uistyles);

  /* Don't read the active app template, use the default one. */
  user->app_template[0] = '\0';

  /* Clear runtime data. */
  user->runtime.is_dirty = false;
  user->edit_studio_light = 0;

  /* free fd->datamap again */
  oldnewmap_clear(fd->datamap);

  return bhead;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read File (Internal)
 * \{ */

static int read_undo_remap_noundo_data_cb(LibraryIDLinkCallbackData *cb_data)
{
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    return IDWALK_RET_NOP;
  }

  IDNameLib_Map *new_idmap_uid = static_cast<IDNameLib_Map *>(cb_data->user_data);
  ID **id_pointer = cb_data->id_pointer;
  if (*id_pointer != nullptr) {
    *id_pointer = BKE_main_idmap_lookup_uid(new_idmap_uid, (*id_pointer)->session_uid);
  }

  return IDWALK_RET_NOP;
}

/* Remap 'no undo' ID usages to matching IDs in new main.
 *
 * 'no undo' IDs have simply be moved from old to new main so far. However, unlike the other
 * re-used IDs (the 'unchanged' ones), there is no guarantee that all the ID pointers they use are
 * still valid.
 *
 * This code performs a remapping based on the session_uid. */
static void read_undo_remap_noundo_data(FileData *fd)
{
  Main *new_bmain = static_cast<Main *>(fd->mainlist->first);
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (new_bmain, id_iter) {
    if (ID_IS_LINKED(id_iter)) {
      continue;
    }
    if ((id_iter->tag & LIB_TAG_UNDO_OLD_ID_REUSED_NOUNDO) == 0) {
      continue;
    }

    BKE_library_foreach_ID_link(
        new_bmain, id_iter, read_undo_remap_noundo_data_cb, fd->new_idmap_uid, IDWALK_INCLUDE_UI);
  }
  FOREACH_MAIN_ID_END;
}

/**
 * Contains sanity/debug checks to be performed at the very end of the reading process (i.e. after
 * data, liblink, linked data, etc. has been done).
 */
static void blo_read_file_checks(Main *bmain)
{
#ifndef NDEBUG
  BLI_assert(bmain->next == nullptr);
  BLI_assert(!bmain->is_read_invalid);

  LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      /* This pointer is deprecated and should always be nullptr. */
      BLI_assert(win->screen == nullptr);
    }
  }
#endif
  UNUSED_VARS_NDEBUG(bmain);
}

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath)
{
  BHead *bhead = blo_bhead_first(fd);
  BlendFileData *bfd;
  ListBase mainlist = {nullptr, nullptr};

  const bool is_undo = (fd->flags & FD_FLAGS_IS_MEMFILE) != 0;
  if (is_undo) {
    CLOG_INFO(&LOG_UNDO, 2, "UNDO: read step");
  }

  bfd = static_cast<BlendFileData *>(MEM_callocN(sizeof(BlendFileData), "blendfiledata"));

  bfd->main = BKE_main_new();
  bfd->main->versionfile = fd->fileversion;
  STRNCPY(bfd->filepath, filepath);

  bfd->type = BLENFILETYPE_BLEND;

  if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    BLI_addtail(&mainlist, bfd->main);
    fd->mainlist = &mainlist;
    STRNCPY(bfd->main->filepath, filepath);
  }

  if (G.background) {
    /* We only read & store .blend thumbnail in background mode
     * (because we cannot re-generate it, no OpenGL available).
     */
    const int *data = read_file_thumbnail(fd);

    if (data) {
      const int width = data[0];
      const int height = data[1];
      if (BLEN_THUMB_MEMSIZE_IS_VALID(width, height)) {
        const size_t data_size = BLEN_THUMB_MEMSIZE(width, height);
        bfd->main->blen_thumb = static_cast<BlendThumbnail *>(MEM_mallocN(data_size, __func__));

        BLI_assert((data_size - sizeof(*bfd->main->blen_thumb)) ==
                   (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*data) * 2)));
        bfd->main->blen_thumb->width = width;
        bfd->main->blen_thumb->height = height;
        memcpy(bfd->main->blen_thumb->rect, &data[2], data_size - sizeof(*bfd->main->blen_thumb));
      }
    }
  }

  if (is_undo) {
    /* This idmap will store uids of all IDs ending up in the new main, whether they are newly
     * read, or re-used from the old main. */
    fd->new_idmap_uid = BKE_main_idmap_create(
        static_cast<Main *>(fd->mainlist->first), false, nullptr, MAIN_IDMAP_TYPE_UID);

    /* Copy all 'no undo' local data from old to new bmain. */
    read_undo_reuse_noundo_local_ids(fd);
  }

  while (bhead) {
    switch (bhead->code) {
      case BLO_CODE_DATA:
      case BLO_CODE_DNA1:
      case BLO_CODE_TEST: /* used as preview since 2.5x */
      case BLO_CODE_REND:
        bhead = blo_bhead_next(fd, bhead);
        break;
      case BLO_CODE_GLOB:
        bhead = read_global(bfd, fd, bhead);
        break;
      case BLO_CODE_USER:
        if (fd->skip_flags & BLO_READ_SKIP_USERDEF) {
          bhead = blo_bhead_next(fd, bhead);
        }
        else {
          bhead = read_userdef(bfd, fd, bhead);
        }
        break;
      case BLO_CODE_ENDB:
        bhead = nullptr;
        break;

      case ID_LINK_PLACEHOLDER:
        if (fd->skip_flags & BLO_READ_SKIP_DATA) {
          bhead = blo_bhead_next(fd, bhead);
        }
        else {
          /* Add link placeholder to the main of the library it belongs to.
           * The library is the most recently loaded ID_LI block, according
           * to the file format definition. So we can use the entry at the
           * end of mainlist, added in direct_link_library. */
          Main *libmain = static_cast<Main *>(mainlist.last);
          bhead = read_libblock(fd, libmain, bhead, 0, true, nullptr);
        }
        break;
        /* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
      case ID_SCRN:
        bhead->code = ID_SCR;
        /* pass on to default */
        ATTR_FALLTHROUGH;
      default:
        if (fd->skip_flags & BLO_READ_SKIP_DATA) {
          bhead = blo_bhead_next(fd, bhead);
        }
        else {
          bhead = read_libblock(fd, bfd->main, bhead, LIB_TAG_LOCAL, false, nullptr);
        }
    }

    if (bfd->main->is_read_invalid) {
      return bfd;
    }
  }

  if (is_undo) {
    /* Move the remaining Library IDs and their linked data to the new main.
     *
     * NOTE: These linked IDs have not been detected as used in newly read main. However, they
     * could be dependencies from some 'no undo' IDs that were unconditionally moved from the old
     * to the new main.
     *
     * While there could be some more refined check here to detect such cases and only move these
     * into the new bmain, in practice it is simpler to systematically move all linked data. The
     * handling of libraries already moves all their linked IDs too, regardless of whether they are
     * effectively used or not. */

    Main *new_main = bfd->main;
    Main *old_main = static_cast<Main *>(fd->old_mainlist->first);
    BLI_assert(old_main != nullptr);
    BLI_assert(old_main->curlib == nullptr);
    for (Main *libmain = old_main->next; libmain != nullptr; libmain = libmain->next) {
      read_undo_move_libmain_data(fd, new_main, old_main, libmain, nullptr);
    }
  }

  /* Do versioning before read_libraries, but skip in undo case. */
  if (!is_undo) {
    if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
      do_versions(fd, nullptr, bfd->main);
    }

    if ((fd->skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
      do_versions_userdef(fd, bfd);
    }
  }

  if (bfd->main->is_read_invalid) {
    return bfd;
  }

  if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    fd->reports->duration.libraries = BLI_time_now_seconds();
    read_libraries(fd, &mainlist);

    blo_join_main(&mainlist);

    lib_link_all(fd, bfd->main);
    after_liblink_merged_bmain_process(bfd->main, fd->reports);

    if (is_undo) {
      /* Ensure ID usages of reused 'no undo' IDs remain valid. */
      /* Although noundo data was reused as-is from the old main, it may have ID pointers to data
       * that has been removed, or that have a new address. */
      read_undo_remap_noundo_data(fd);
    }

    fd->reports->duration.libraries = BLI_time_now_seconds() - fd->reports->duration.libraries;

    /* Skip in undo case. */
    if ((fd->flags & FD_FLAGS_IS_MEMFILE) == 0) {
      /* Note that we can't recompute user-counts at this point in undo case, we play too much with
       * IDs from different memory realms, and Main database is not in a fully valid state yet.
       */
      /* Some versioning code does expect some proper user-reference-counting, e.g. in conversion
       * from groups to collections... We could optimize out that first call when we are reading a
       * current version file, but again this is really not a bottle neck currently.
       * So not worth it. */
      BKE_main_id_refcount_recompute(bfd->main, false);

      /* Yep, second splitting... but this is a very cheap operation, so no big deal. */
      blo_split_main(&mainlist, bfd->main);
      LISTBASE_FOREACH (Main *, mainvar, &mainlist) {
        BLI_assert(mainvar->versionfile != 0);
        do_versions_after_linking(
            (mainvar->curlib && mainvar->curlib->filedata) ? mainvar->curlib->filedata : fd,
            mainvar);
      }
      blo_join_main(&mainlist);

      /* And we have to compute those user-reference-counts again, as `do_versions_after_linking()`
       * does not always properly handle user counts, and/or that function does not take into
       * account old, deprecated data. */
      BKE_main_id_refcount_recompute(bfd->main, false);
    }

    LISTBASE_FOREACH (Library *, lib, &bfd->main->libraries) {
      /* Now we can clear this runtime library filedata, it is not needed anymore. */
      if (lib->filedata) {
        blo_filedata_free(lib->filedata);
        lib->filedata = nullptr;
      }
    }

    if (bfd->main->is_read_invalid) {
      return bfd;
    }

    /* After all data has been read and versioned, uses LIB_TAG_NEW. Theoretically this should
     * not be calculated in the undo case, but it is currently needed even on undo to recalculate
     * a cache. */
    blender::bke::ntreeUpdateAllNew(bfd->main);

    placeholders_ensure_valid(bfd->main);

    BKE_main_id_tag_all(bfd->main, LIB_TAG_NEW, false);

    /* Must happen before applying liboverrides, as this process may fully invalidate e.g. view
     * layer pointers in case a Scene is a liboverride. */
    link_global(fd, bfd);

    /* Now that all our data-blocks are loaded,
     * we can re-generate overrides from their references. */
    if ((fd->flags & FD_FLAGS_IS_MEMFILE) == 0) {
      /* Do not apply in undo case! */
      fd->reports->duration.lib_overrides = BLI_time_now_seconds();

      std::string cur_view_layer_name = bfd->cur_view_layer != nullptr ?
                                            bfd->cur_view_layer->name :
                                            "";

      BKE_lib_override_library_main_validate(bfd->main, fd->reports->reports);
      BKE_lib_override_library_main_update(bfd->main);

      /* In case the current scene is a liboverride, while the ID pointer itself remains valid,
       * above update of liboverrides will have completely invalidated its old content, so the
       * current view-layer needs to be searched for again. */
      if (bfd->cur_view_layer != nullptr) {
        bfd->cur_view_layer = BKE_view_layer_find(bfd->curscene, cur_view_layer_name.c_str());
      }

      /* FIXME Temporary 'fix' to a problem in how temp ID are copied in
       * `BKE_lib_override_library_main_update`, see #103062.
       * Proper fix involves first addressing #90610. */
      BKE_main_collections_parent_relations_rebuild(bfd->main);

      fd->reports->duration.lib_overrides = BLI_time_now_seconds() -
                                            fd->reports->duration.lib_overrides;
    }

    BKE_collections_after_lib_link(bfd->main);

    /* Make all relative paths, relative to the open blend file. */
    fix_relpaths_library(fd->relabase, bfd->main);
  }

  fd->mainlist = nullptr; /* Safety, this is local variable, shall not be used afterward. */

  BLI_assert(bfd->main->id_map == nullptr);

  /* Sanity checks. */
  blo_read_file_checks(bfd->main);

  return bfd;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Linking
 *
 * Also used for append.
 * \{ */

struct BHeadSort {
  BHead *bhead;
  const void *old;
};

static int verg_bheadsort(const void *v1, const void *v2)
{
  const BHeadSort *x1 = static_cast<const BHeadSort *>(v1),
                  *x2 = static_cast<const BHeadSort *>(v2);

  if (x1->old > x2->old) {
    return 1;
  }
  if (x1->old < x2->old) {
    return -1;
  }
  return 0;
}

static void sort_bhead_old_map(FileData *fd)
{
  BHead *bhead;
  BHeadSort *bhs;
  int tot = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    tot++;
  }

  fd->tot_bheadmap = tot;
  if (tot == 0) {
    return;
  }

  bhs = fd->bheadmap = static_cast<BHeadSort *>(
      MEM_malloc_arrayN(tot, sizeof(BHeadSort), "BHeadSort"));

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead), bhs++) {
    bhs->bhead = bhead;
    bhs->old = bhead->old;
  }

  qsort(fd->bheadmap, tot, sizeof(BHeadSort), verg_bheadsort);
}

static BHead *find_previous_lib(FileData *fd, BHead *bhead)
{
  /* Skip library data-blocks in undo, see comment in read_libblock. */
  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    return nullptr;
  }

  for (; bhead; bhead = blo_bhead_prev(fd, bhead)) {
    if (bhead->code == ID_LI) {
      break;
    }
  }

  return bhead;
}

static BHead *find_bhead(FileData *fd, void *old)
{
#if 0
  BHead *bhead;
#endif
  BHeadSort *bhs, bhs_s;

  if (!old) {
    return nullptr;
  }

  if (fd->bheadmap == nullptr) {
    sort_bhead_old_map(fd);
  }

  bhs_s.old = old;
  bhs = static_cast<BHeadSort *>(
      bsearch(&bhs_s, fd->bheadmap, fd->tot_bheadmap, sizeof(BHeadSort), verg_bheadsort));

  if (bhs) {
    return bhs->bhead;
  }

#if 0
  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->old == old) {
      return bhead;
    }
  }
#endif

  return nullptr;
}

static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name)
{
#ifdef USE_GHASH_BHEAD

  char idname_full[MAX_ID_NAME];

  *((short *)idname_full) = idcode;
  BLI_strncpy(idname_full + 2, name, sizeof(idname_full) - 2);

  return static_cast<BHead *>(BLI_ghash_lookup(fd->bhead_idname_hash, idname_full));

#else
  BHead *bhead;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == idcode) {
      const char *idname_test = blo_bhead_id_name(fd, bhead);
      if (STREQ(idname_test + 2, name)) {
        return bhead;
      }
    }
    else if (bhead->code == ENDB) {
      break;
    }
  }

  return nullptr;
#endif
}

static BHead *find_bhead_from_idname(FileData *fd, const char *idname)
{
#ifdef USE_GHASH_BHEAD
  return static_cast<BHead *>(BLI_ghash_lookup(fd->bhead_idname_hash, idname));
#else
  return find_bhead_from_code_name(fd, GS(idname), idname + 2);
#endif
}

static ID *library_id_is_yet_read(FileData *fd, Main *mainvar, BHead *bhead)
{
  if (mainvar->id_map == nullptr) {
    mainvar->id_map = BKE_main_idmap_create(mainvar, false, nullptr, MAIN_IDMAP_TYPE_NAME);
  }
  BLI_assert(BKE_main_idmap_main_get(mainvar->id_map) == mainvar);

  const char *idname = blo_bhead_id_name(fd, bhead);

  ID *id = BKE_main_idmap_lookup_name(mainvar->id_map, GS(idname), idname + 2, mainvar->curlib);
  BLI_assert(id == BLI_findstring(which_libbase(mainvar, GS(idname)), idname, offsetof(ID, name)));
  return id;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Linking (expand pointers)
 * \{ */

struct BlendExpander {
  FileData *fd;
  Main *main;
};

static void expand_doit_library(void *fdhandle, Main *mainvar, void *old)
{
  FileData *fd = static_cast<FileData *>(fdhandle);

  if (mainvar->is_read_invalid) {
    return;
  }

  BHead *bhead = find_bhead(fd, old);
  if (bhead == nullptr) {
    return;
  }

  if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Placeholder link to data-block in another library. */
    BHead *bheadlib = find_previous_lib(fd, bhead);
    if (bheadlib == nullptr) {
      return;
    }

    Library *lib = static_cast<Library *>(read_struct(fd, bheadlib, "Library"));
    Main *libmain = blo_find_main(fd, lib->filepath, fd->relabase);

    if (libmain->curlib == nullptr) {
      const char *idname = blo_bhead_id_name(fd, bhead);

      BLO_reportf_wrap(fd->reports,
                       RPT_WARNING,
                       RPT_("LIB: Data refers to main .blend file: '%s' from %s"),
                       idname,
                       mainvar->curlib->filepath_abs);
      return;
    }

    ID *id = library_id_is_yet_read(fd, libmain, bhead);

    if (id == nullptr) {
      /* ID has not been read yet, add placeholder to the main of the
       * library it belongs to, so that it will be read later. */
      read_libblock(fd, libmain, bhead, fd->id_tag_extra | LIB_TAG_INDIRECT, false, &id);
      BLI_assert(id != nullptr);
      id_sort_by_name(which_libbase(libmain, GS(id->name)), id, static_cast<ID *>(id->prev));

      /* commented because this can print way too much */
      // if (G.debug & G_DEBUG) printf("expand_doit: other lib %s\n", lib->filepath);

      /* for outliner dependency only */
      libmain->curlib->parent = mainvar->curlib;
    }
    else {
      /* Convert any previously read weak link to regular link
       * to signal that we want to read this data-block. */
      if (id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) {
        id->flag &= ~LIB_INDIRECT_WEAK_LINK;
      }

      /* "id" is either a placeholder or real ID that is already in the
       * main of the library (A) it belongs to. However it might have been
       * put there by another library (C) which only updated its own
       * fd->libmap. In that case we also need to update the fd->libmap
       * of the current library (B) so we can find it for lookups.
       *
       * An example of such a setup is:
       * (A) tree.blend: contains Tree object.
       * (B) forest.blend: contains Forest collection linking in Tree from tree.blend.
       * (C) shot.blend: links in both Tree from tree.blend and Forest from forest.blend.
       */
      oldnewmap_lib_insert(fd, bhead->old, id, bhead->code);

      /* Commented because this can print way too much. */
#if 0
      if (G.debug & G_DEBUG) {
        printf("expand_doit: already linked: %s lib: %s\n", id->name, lib->filepath);
      }
#endif
    }

    MEM_freeN(lib);
  }
  else {
    /* Data-block in same library. */
    /* In 2.50+ file identifier for screens is patched, forward compatibility. */
    if (bhead->code == ID_SCRN) {
      bhead->code = ID_SCR;
    }

    ID *id = library_id_is_yet_read(fd, mainvar, bhead);
    if (id == nullptr) {
      read_libblock(fd,
                    mainvar,
                    bhead,
                    fd->id_tag_extra | LIB_TAG_NEED_EXPAND | LIB_TAG_INDIRECT,
                    false,
                    &id);
      BLI_assert(id != nullptr);
      id_sort_by_name(which_libbase(mainvar, GS(id->name)), id, static_cast<ID *>(id->prev));
    }
    else {
      /* Convert any previously read weak link to regular link
       * to signal that we want to read this data-block. */
      if (id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) {
        id->flag &= ~LIB_INDIRECT_WEAK_LINK;
      }

      /* this is actually only needed on UI call? when ID was already read before,
       * and another append happens which invokes same ID...
       * in that case the lookup table needs this entry */
      oldnewmap_lib_insert(fd, bhead->old, id, bhead->code);
      /* commented because this can print way too much */
      // if (G.debug & G_DEBUG) printf("expand: already read %s\n", id->name);
    }
  }
}

static BLOExpandDoitCallback expand_doit;

void BLO_main_expander(BLOExpandDoitCallback expand_doit_func)
{
  expand_doit = expand_doit_func;
}

static int expand_cb(LibraryIDLinkCallbackData *cb_data)
{
  /* Embedded IDs are not known by lib_link code, so they would be remapped to `nullptr`. But there
   * is no need to process them anyway, as they are already handled during the 'read_data' phase.
   */
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    return IDWALK_RET_NOP;
  }

  /* Explicitly requested to be ignored during readfile processing. Means the read_data code
   * already handled this pointer. Typically, the 'owner_id' pointer of an embedded ID. */
  if (cb_data->cb_flag & IDWALK_CB_READFILE_IGNORE) {
    return IDWALK_RET_NOP;
  }

  /* Expand process can be re-entrant or have other complex interactions that will not work well
   * with loop-back pointers. Further more, processing such data should not be needed here anyway.
   */
  if (cb_data->cb_flag & (IDWALK_CB_LOOPBACK)) {
    return IDWALK_RET_NOP;
  }

  BlendExpander *expander = static_cast<BlendExpander *>(cb_data->user_data);
  ID *id = *(cb_data->id_pointer);

  expand_doit(expander->fd, expander->main, id);

  return IDWALK_RET_NOP;
}

void BLO_expand_main(void *fdhandle, Main *mainvar)
{
  FileData *fd = static_cast<FileData *>(fdhandle);
  BlendExpander expander = {fd, mainvar};

  for (bool do_it = true; do_it;) {
    do_it = false;
    ID *id_iter;

    FOREACH_MAIN_ID_BEGIN (mainvar, id_iter) {
      if ((id_iter->tag & LIB_TAG_NEED_EXPAND) == 0) {
        continue;
      }

      /* Original (current) ID pointer can be considered as valid, but _not_ its own pointers to
       * other IDs - the already loaded ones will be valid, but the yet-to-be-read ones will not.
       * Expanding should _not_ require processing of UI ID pointers.
       * Expanding should never modify ID pointers themselves.
       * Handling of DNA deprecated data should never be needed in undo case. */
      const int flag = IDWALK_READONLY | IDWALK_NO_ORIG_POINTERS_ACCESS |
                       ((!fd || (fd->flags & FD_FLAGS_IS_MEMFILE)) ?
                            0 :
                            IDWALK_DO_DEPRECATED_POINTERS);
      BKE_library_foreach_ID_link(nullptr, id_iter, expand_cb, &expander, flag);

      do_it = true;
      id_iter->tag &= ~LIB_TAG_NEED_EXPAND;
    }
    FOREACH_MAIN_ID_END;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Linking (helper functions)
 * \{ */

/* returns true if the item was found
 * but it may already have already been appended/linked */
static ID *link_named_part(
    Main *mainl, FileData *fd, const short idcode, const char *name, const int flag)
{
  BHead *bhead = find_bhead_from_code_name(fd, idcode, name);
  ID *id;

  const bool use_placeholders = (flag & BLO_LIBLINK_USE_PLACEHOLDERS) != 0;
  const bool force_indirect = (flag & BLO_LIBLINK_FORCE_INDIRECT) != 0;

  BLI_assert(BKE_idtype_idcode_is_linkable(idcode) && BKE_idtype_idcode_is_valid(idcode));

  if (bhead) {
    id = library_id_is_yet_read(fd, mainl, bhead);
    if (id == nullptr) {
      /* not read yet */
      const int tag = ((force_indirect ? LIB_TAG_INDIRECT : LIB_TAG_EXTERN) | fd->id_tag_extra);
      read_libblock(fd, mainl, bhead, tag | LIB_TAG_NEED_EXPAND, false, &id);

      if (id) {
        /* sort by name in list */
        ListBase *lb = which_libbase(mainl, idcode);
        id_sort_by_name(lb, id, nullptr);
      }
    }
    else {
      /* already linked */
      CLOG_WARN(&LOG, "Append: ID '%s' is already linked", id->name);
      oldnewmap_lib_insert(fd, bhead->old, id, bhead->code);
      if (!force_indirect && (id->tag & LIB_TAG_INDIRECT)) {
        id->tag &= ~LIB_TAG_INDIRECT;
        id->flag &= ~LIB_INDIRECT_WEAK_LINK;
        id->tag |= LIB_TAG_EXTERN;
      }
    }
  }
  else if (use_placeholders) {
    /* XXX flag part is weak! */
    id = create_placeholder(
        mainl, idcode, name, force_indirect ? LIB_TAG_INDIRECT : LIB_TAG_EXTERN, false);
  }
  else {
    id = nullptr;
  }

  /* if we found the id but the id is nullptr, this is really bad */
  BLI_assert(!((bhead != nullptr) && (id == nullptr)));

  return id;
}

ID *BLO_library_link_named_part(Main *mainl,
                                BlendHandle **bh,
                                const short idcode,
                                const char *name,
                                const LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);

  ID *ret_id = nullptr;
  if (!mainl->is_read_invalid) {
    ret_id = link_named_part(mainl, fd, idcode, name, params->flag);
  }

  if (mainl->is_read_invalid) {
    return nullptr;
  }
  return ret_id;
}

/* common routine to append/link something from a library */

static Main *library_link_begin(Main *mainvar,
                                FileData *fd,
                                const char *filepath,
                                const int id_tag_extra)
{
  Main *mainl;

  /* Only allow specific tags to be set as extra,
   * otherwise this could conflict with library loading logic.
   * Other flags can be added here, as long as they are safe. */
  BLI_assert((id_tag_extra & ~LIB_TAG_TEMP_MAIN) == 0);

  fd->id_tag_extra = id_tag_extra;

  fd->mainlist = static_cast<ListBase *>(MEM_callocN(sizeof(ListBase), "FileData.mainlist"));

  /* make mains */
  blo_split_main(fd->mainlist, mainvar);

  /* which one do we need? */
  mainl = blo_find_main(fd, filepath, BKE_main_blendfile_path(mainvar));
  if (mainl->curlib) {
    mainl->curlib->filedata = fd;
  }

  /* needed for do_version */
  mainl->versionfile = short(fd->fileversion);
  read_file_version(fd, mainl);
#ifdef USE_GHASH_BHEAD
  read_file_bhead_idname_map_create(fd);
#endif

  return mainl;
}

void BLO_library_link_params_init(LibraryLink_Params *params,
                                  Main *bmain,
                                  const int flag,
                                  const int id_tag_extra)
{
  memset(params, 0, sizeof(*params));
  params->bmain = bmain;
  params->flag = flag;
  params->id_tag_extra = id_tag_extra;
}

void BLO_library_link_params_init_with_context(LibraryLink_Params *params,
                                               Main *bmain,
                                               const int flag,
                                               const int id_tag_extra,
                                               /* Context arguments. */
                                               Scene *scene,
                                               ViewLayer *view_layer,
                                               const View3D *v3d)
{
  BLO_library_link_params_init(params, bmain, flag, id_tag_extra);
  if (scene != nullptr) {
    params->context.scene = scene;
    params->context.view_layer = view_layer;
    params->context.v3d = v3d;
  }
}

Main *BLO_library_link_begin(BlendHandle **bh,
                             const char *filepath,
                             const LibraryLink_Params *params)
{
  FileData *fd = reinterpret_cast<FileData *>(*bh);
  return library_link_begin(params->bmain, fd, filepath, params->id_tag_extra);
}

static void split_main_newid(Main *mainptr, Main *main_newid)
{
  /* We only copy the necessary subset of data in this temp main. */
  main_newid->versionfile = mainptr->versionfile;
  main_newid->subversionfile = mainptr->subversionfile;
  STRNCPY(main_newid->filepath, mainptr->filepath);
  main_newid->curlib = mainptr->curlib;

  ListBase *lbarray[INDEX_ID_MAX];
  ListBase *lbarray_newid[INDEX_ID_MAX];
  int i = set_listbasepointers(mainptr, lbarray);
  set_listbasepointers(main_newid, lbarray_newid);
  while (i--) {
    BLI_listbase_clear(lbarray_newid[i]);

    LISTBASE_FOREACH_MUTABLE (ID *, id, lbarray[i]) {
      if (id->tag & LIB_TAG_NEW) {
        BLI_remlink(lbarray[i], id);
        BLI_addtail(lbarray_newid[i], id);
      }
    }
  }
}

static void library_link_end(Main *mainl, FileData **fd, const int flag)
{
  Main *mainvar;
  Library *curlib;

  if (mainl->id_map == nullptr) {
    mainl->id_map = BKE_main_idmap_create(mainl, false, nullptr, MAIN_IDMAP_TYPE_NAME);
  }

  /* expander now is callback function */
  BLO_main_expander(expand_doit_library);

  /* make main consistent */
  BLO_expand_main(*fd, mainl);

  /* Do this when expand found other libraries. */
  read_libraries(*fd, (*fd)->mainlist);

  curlib = mainl->curlib;

  /* make the lib path relative if required */
  if (flag & FILE_RELPATH) {
    /* use the full path, this could have been read by other library even */
    STRNCPY(curlib->filepath, curlib->filepath_abs);

    /* uses current .blend file as reference */
    BLI_path_rel(curlib->filepath, BKE_main_blendfile_path_from_global());
  }

  blo_join_main((*fd)->mainlist);
  mainvar = static_cast<Main *>((*fd)->mainlist->first);
  mainl = nullptr; /* blo_join_main free's mainl, can't use anymore */

  if (mainvar->is_read_invalid) {
    return;
  }

  lib_link_all(*fd, mainvar);
  after_liblink_merged_bmain_process(mainvar, (*fd)->reports);

  /* Some versioning code does expect some proper userrefcounting, e.g. in conversion from
   * groups to collections... We could optimize out that first call when we are reading a
   * current version file, but again this is really not a bottle neck currently. So not worth
   * it. */
  BKE_main_id_refcount_recompute(mainvar, false);

  BKE_collections_after_lib_link(mainvar);

  /* Yep, second splitting... but this is a very cheap operation, so no big deal. */
  blo_split_main((*fd)->mainlist, mainvar);
  Main *main_newid = BKE_main_new();
  for (mainvar = ((Main *)(*fd)->mainlist->first)->next; mainvar; mainvar = mainvar->next) {
    BLI_assert(mainvar->versionfile != 0);
    /* We need to split out IDs already existing,
     * or they will go again through do_versions - bad, very bad! */
    split_main_newid(mainvar, main_newid);

    do_versions_after_linking(
        (main_newid->curlib && main_newid->curlib->filedata) ? main_newid->curlib->filedata : *fd,
        main_newid);

    add_main_to_main(mainvar, main_newid);

    if (mainvar->is_read_invalid) {
      break;
    }
  }

  blo_join_main((*fd)->mainlist);
  mainvar = static_cast<Main *>((*fd)->mainlist->first);
  MEM_freeN((*fd)->mainlist);

  if (mainvar->is_read_invalid) {
    BKE_main_free(main_newid);
    return;
  }

  /* This does not take into account old, deprecated data, so we also have to do it after
   * `do_versions_after_linking()`. */
  BKE_main_id_refcount_recompute(mainvar, false);

  /* After all data has been read and versioned, uses LIB_TAG_NEW. */
  blender::bke::ntreeUpdateAllNew(mainvar);

  placeholders_ensure_valid(mainvar);

  /* Apply overrides of newly linked data if needed. Already existing IDs need to split out, to
   * avoid re-applying their own overrides. */
  BLI_assert(BKE_main_is_empty(main_newid));
  split_main_newid(mainvar, main_newid);
  BKE_lib_override_library_main_validate(main_newid, (*fd)->reports->reports);
  BKE_lib_override_library_main_update(main_newid);
  add_main_to_main(mainvar, main_newid);
  BKE_main_free(main_newid);

  BKE_main_id_tag_all(mainvar, LIB_TAG_NEW, false);

  /* FIXME Temporary 'fix' to a problem in how temp ID are copied in
   * `BKE_lib_override_library_main_update`, see #103062.
   * Proper fix involves first addressing #90610. */
  BKE_main_collections_parent_relations_rebuild(mainvar);

  /* Make all relative paths, relative to the open blend file. */
  fix_relpaths_library(BKE_main_blendfile_path(mainvar), mainvar);

  /* patch to prevent switch_endian happens twice */
  if ((*fd)->flags & FD_FLAGS_SWITCH_ENDIAN) {
    blo_filedata_free(*fd);
    *fd = nullptr;
  }

  /* Sanity checks. */
  blo_read_file_checks(mainvar);
}

void BLO_library_link_end(Main *mainl, BlendHandle **bh, const LibraryLink_Params *params)
{
  FileData *fd = reinterpret_cast<FileData *>(*bh);

  if (!mainl->is_read_invalid) {
    library_link_end(mainl, &fd, params->flag);
  }

  LISTBASE_FOREACH (Library *, lib, &params->bmain->libraries) {
    /* Now we can clear this runtime library filedata, it is not needed anymore. */
    if (lib->filedata == reinterpret_cast<FileData *>(*bh)) {
      /* The filedata is owned and managed by caller code, only clear matching library pointer. */
      lib->filedata = nullptr;
    }
    else if (lib->filedata) {
      /* In case other libraries had to be read as dependencies of the main linked one, they need
       * to be cleared here.
       *
       * TODO: In the future, could be worth keeping them in case data are linked from several
       * libraries at once? To avoid closing and re-opening the same file several times. Would need
       * a global cleanup callback then once all linking is done, though. */
      blo_filedata_free(lib->filedata);
      lib->filedata = nullptr;
    }
  }

  *bh = reinterpret_cast<BlendHandle *>(fd);
}

void *BLO_library_read_struct(FileData *fd, BHead *bh, const char *blockname)
{
  return read_struct(fd, bh, blockname);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Reading
 * \{ */

static int has_linked_ids_to_read(Main *mainvar)
{
  ListBase *lbarray[INDEX_ID_MAX];
  int a = set_listbasepointers(mainvar, lbarray);

  while (a--) {
    LISTBASE_FOREACH (ID *, id, lbarray[a]) {
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && !(id->flag & LIB_INDIRECT_WEAK_LINK)) {
        return true;
      }
    }
  }

  return false;
}

static void read_library_linked_id(
    FileData *basefd, FileData *fd, Main *mainvar, ID *id, ID **r_id)
{
  BHead *bhead = nullptr;
  const bool is_valid = BKE_idtype_idcode_is_linkable(GS(id->name)) ||
                        ((id->tag & LIB_TAG_EXTERN) == 0);

  if (fd) {
    bhead = find_bhead_from_idname(fd, id->name);
  }

  if (!is_valid) {
    BLO_reportf_wrap(basefd->reports,
                     RPT_ERROR,
                     RPT_("LIB: %s: '%s' is directly linked from '%s' (parent '%s'), but is a "
                          "non-linkable data type"),
                     BKE_idtype_idcode_to_name(GS(id->name)),
                     id->name + 2,
                     mainvar->curlib->filepath_abs,
                     library_parent_filepath(mainvar->curlib));
  }

  id->tag &= ~LIB_TAG_ID_LINK_PLACEHOLDER;
  id->flag &= ~LIB_INDIRECT_WEAK_LINK;

  if (bhead) {
    id->tag |= LIB_TAG_NEED_EXPAND;
    // printf("read lib block %s\n", id->name);
    read_libblock(fd, mainvar, bhead, id->tag, false, r_id);
  }
  else {
    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     RPT_("LIB: %s: '%s' missing from '%s', parent '%s'"),
                     BKE_idtype_idcode_to_name(GS(id->name)),
                     id->name + 2,
                     mainvar->curlib->filepath_abs,
                     library_parent_filepath(mainvar->curlib));
    basefd->reports->count.missing_linked_id++;

    /* Generate a placeholder for this ID (simplified version of read_libblock actually...). */
    if (r_id) {
      *r_id = is_valid ? create_placeholder(mainvar,
                                            GS(id->name),
                                            id->name + 2,
                                            id->tag,
                                            id->override_library != nullptr) :
                         nullptr;
    }
  }
}

static void read_library_linked_ids(FileData *basefd,
                                    FileData *fd,
                                    ListBase *mainlist,
                                    Main *mainvar)
{
  GHash *loaded_ids = BLI_ghash_str_new(__func__);

  ListBase *lbarray[INDEX_ID_MAX];
  int a = set_listbasepointers(mainvar, lbarray);

  while (a--) {
    ID *id = static_cast<ID *>(lbarray[a]->first);
    ListBase pending_free_ids = {nullptr};

    while (id) {
      ID *id_next = static_cast<ID *>(id->next);
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && !(id->flag & LIB_INDIRECT_WEAK_LINK)) {
        BLI_remlink(lbarray[a], id);
        if (mainvar->id_map != nullptr) {
          BKE_main_idmap_remove_id(mainvar->id_map, id);
        }

        /* When playing with lib renaming and such, you may end with cases where
         * you have more than one linked ID of the same data-block from same
         * library. This is absolutely horrible, hence we use a ghash to ensure
         * we go back to a single linked data when loading the file. */
        ID **realid = nullptr;
        if (!BLI_ghash_ensure_p(loaded_ids, id->name, (void ***)&realid)) {
          read_library_linked_id(basefd, fd, mainvar, id, realid);
        }

        /* `realid` shall never be nullptr - unless some source file/lib is broken
         * (known case: some directly linked shape-key from a missing lib...). */
        // BLI_assert(*realid != nullptr);

        /* Now that we have a real ID, replace all pointers to placeholders in
         * fd->libmap with pointers to the real data-blocks. We do this for all
         * libraries since multiple might be referencing this ID. */
        change_link_placeholder_to_real_ID_pointer(mainlist, basefd, id, *realid);

        /* We cannot free old lib-ref placeholder ID here anymore, since we use
         * its name as key in loaded_ids hash. */
        BLI_addtail(&pending_free_ids, id);
      }
      id = id_next;
    }

    /* Clear GHash and free link placeholder IDs of the current type. */
    BLI_ghash_clear(loaded_ids, nullptr, nullptr);
    BLI_freelistN(&pending_free_ids);
  }

  BLI_ghash_free(loaded_ids, nullptr, nullptr);
}

static void read_library_clear_weak_links(FileData *basefd, ListBase *mainlist, Main *mainvar)
{
  /* Any remaining weak links at this point have been lost, silently drop
   * those by setting them to nullptr pointers. */
  ListBase *lbarray[INDEX_ID_MAX];
  int a = set_listbasepointers(mainvar, lbarray);

  while (a--) {
    ID *id = static_cast<ID *>(lbarray[a]->first);

    while (id) {
      ID *id_next = static_cast<ID *>(id->next);
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && (id->flag & LIB_INDIRECT_WEAK_LINK)) {
        CLOG_INFO(&LOG, 3, "Dropping weak link to '%s'", id->name);
        change_link_placeholder_to_real_ID_pointer(mainlist, basefd, id, nullptr);
        BLI_freelinkN(lbarray[a], id);
      }
      id = id_next;
    }
  }
}

static FileData *read_library_file_data(FileData *basefd,
                                        ListBase *mainlist,
                                        Main *mainl,
                                        Main *mainptr)
{
  FileData *fd = mainptr->curlib->filedata;

  if (fd != nullptr) {
    /* File already open. */
    return fd;
  }

  if (mainptr->curlib->packedfile) {
    /* Read packed file. */
    PackedFile *pf = mainptr->curlib->packedfile;

    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     RPT_("Read packed library:  '%s', parent '%s'"),
                     mainptr->curlib->filepath,
                     library_parent_filepath(mainptr->curlib));
    fd = blo_filedata_from_memory(pf->data, pf->size, basefd->reports);

    /* Needed for library_append and read_libraries. */
    STRNCPY(fd->relabase, mainptr->curlib->filepath_abs);
  }
  else {
    /* Read file on disk. */
    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     RPT_("Read library:  '%s', '%s', parent '%s'"),
                     mainptr->curlib->filepath_abs,
                     mainptr->curlib->filepath,
                     library_parent_filepath(mainptr->curlib));
    fd = blo_filedata_from_file(mainptr->curlib->filepath_abs, basefd->reports);
  }

  if (fd) {
    /* Share the mainlist, so all libraries are added immediately in a
     * single list. It used to be that all FileData's had their own list,
     * but with indirectly linking this meant we didn't catch duplicate
     * libraries properly. */
    fd->mainlist = mainlist;

    fd->reports = basefd->reports;

    if (fd->libmap) {
      oldnewmap_free(fd->libmap);
    }

    fd->libmap = oldnewmap_new();

    mainptr->curlib->filedata = fd;
    mainptr->versionfile = fd->fileversion;

    /* subversion */
    read_file_version(fd, mainptr);
#ifdef USE_GHASH_BHEAD
    read_file_bhead_idname_map_create(fd);
#endif
  }
  else {
    mainptr->curlib->filedata = nullptr;
    mainptr->curlib->id.tag |= LIB_TAG_MISSING;
    /* Set lib version to current main one... Makes assert later happy. */
    mainptr->versionfile = mainptr->curlib->versionfile = mainl->versionfile;
    mainptr->subversionfile = mainptr->curlib->subversionfile = mainl->subversionfile;
  }

  if (fd == nullptr) {
    BLO_reportf_wrap(
        basefd->reports, RPT_INFO, RPT_("Cannot find lib '%s'"), mainptr->curlib->filepath_abs);
    basefd->reports->count.missing_libraries++;
  }

  return fd;
}

static void read_libraries(FileData *basefd, ListBase *mainlist)
{
  Main *mainl = static_cast<Main *>(mainlist->first);
  bool do_it = true;

  /* Expander is now callback function. */
  BLO_main_expander(expand_doit_library);

  /* At this point the base blend file has been read, and each library blend
   * encountered so far has a main with placeholders for linked data-blocks.
   *
   * Now we will read the library blend files and replace the placeholders
   * with actual data-blocks. We loop over library mains multiple times in
   * case a library needs to link additional data-blocks from another library
   * that had been read previously. */
  while (do_it) {
    do_it = false;

    /* Loop over mains of all library blend files encountered so far. Note
     * this list gets longer as more indirectly library blends are found. */
    for (Main *mainptr = mainl->next; mainptr; mainptr = mainptr->next) {
      /* Does this library have any more linked data-blocks we need to read? */
      if (has_linked_ids_to_read(mainptr)) {
        CLOG_INFO(&LOG,
                  3,
                  "Reading linked data-blocks from %s (%s)",
                  mainptr->curlib->id.name,
                  mainptr->curlib->filepath);

        /* Open file if it has not been done yet. */
        FileData *fd = read_library_file_data(basefd, mainlist, mainl, mainptr);

        if (fd) {
          do_it = true;

          if (mainptr->id_map == nullptr) {
            mainptr->id_map = BKE_main_idmap_create(mainptr, false, nullptr, MAIN_IDMAP_TYPE_NAME);
          }
        }

        /* Read linked data-blocks for each link placeholder, and replace
         * the placeholder with the real data-block. */
        read_library_linked_ids(basefd, fd, mainlist, mainptr);

        /* Test if linked data-blocks need to read further linked data-blocks
         * and create link placeholders for them. */
        BLO_expand_main(fd, mainptr);
      }
    }
  }

  for (Main *mainptr = mainl->next; mainptr; mainptr = mainptr->next) {
    /* Drop weak links for which no data-block was found.
     * Since this can remap pointers in `libmap` of all libraries, it needs to be performed in its
     * own loop, before any call to `lib_link_all` (and the freeing of the libraries' filedata). */
    read_library_clear_weak_links(basefd, mainlist, mainptr);
  }

  Main *main_newid = BKE_main_new();
  for (Main *mainptr = mainl->next; mainptr; mainptr = mainptr->next) {
    /* Do versioning for newly added linked data-blocks. If no data-blocks
     * were read from a library versionfile will still be zero and we can
     * skip it. */
    if (mainptr->versionfile) {
      /* Split out already existing IDs to avoid them going through
       * do_versions multiple times, which would have bad consequences. */
      split_main_newid(mainptr, main_newid);

      /* `filedata` can be NULL when loading linked data from nonexistent or invalid library
       * reference. Or during linking/appending, when processing data from a library not involved
       * in the current linking/appending operation.
       *
       * Skip versioning in these cases, since the only IDs here will be placeholders (missing
       * lib), or already existing IDs (linking/appending). */
      if (mainptr->curlib->filedata) {
        do_versions(mainptr->curlib->filedata, mainptr->curlib, main_newid);
      }

      add_main_to_main(mainptr, main_newid);
    }

    /* Lib linking. */
    if (mainptr->curlib->filedata) {
      lib_link_all(mainptr->curlib->filedata, mainptr);
    }

    /* NOTE: No need to call #do_versions_after_linking() or #BKE_main_id_refcount_recompute()
     * here, as this function is only called for library 'subset' data handling, as part of
     * either full blend-file reading (#blo_read_file_internal()), or library-data linking
     * (#library_link_end()).
     *
     * For this to work reliably, `mainptr->curlib->filedata` also needs to be freed after said
     * versioning code has run. */
  }
  BKE_main_free(main_newid);
}

void *BLO_read_get_new_data_address(BlendDataReader *reader, const void *old_address)
{
  return newdataadr(reader->fd, old_address);
}

void *BLO_read_get_new_data_address_no_us(BlendDataReader *reader, const void *old_address)
{
  return newdataadr_no_us(reader->fd, old_address);
}

void *BLO_read_get_new_packed_address(BlendDataReader *reader, const void *old_address)
{
  return newpackedadr(reader->fd, old_address);
}

ID *BLO_read_get_new_id_address(BlendLibReader *reader,
                                ID *self_id,
                                const bool is_linked_only,
                                ID *id)
{
  return static_cast<ID *>(newlibadr(reader->fd, self_id, is_linked_only, id));
}

ID *BLO_read_get_new_id_address_from_session_uid(BlendLibReader *reader, uint session_uid)
{
  return BKE_main_idmap_lookup_uid(reader->fd->new_idmap_uid, session_uid);
}

int BLO_read_fileversion_get(BlendDataReader *reader)
{
  return reader->fd->fileversion;
}

bool BLO_read_requires_endian_switch(BlendDataReader *reader)
{
  return (reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;
}

void BLO_read_list_cb(BlendDataReader *reader, ListBase *list, BlendReadListFn callback)
{
  if (BLI_listbase_is_empty(list)) {
    return;
  }

  BLO_read_data_address(reader, &list->first);
  if (callback != nullptr) {
    callback(reader, list->first);
  }
  Link *ln = static_cast<Link *>(list->first);
  Link *prev = nullptr;
  while (ln) {
    BLO_read_data_address(reader, &ln->next);
    if (ln->next != nullptr && callback != nullptr) {
      callback(reader, ln->next);
    }
    ln->prev = prev;
    prev = ln;
    ln = ln->next;
  }
  list->last = prev;
}

void BLO_read_list(BlendDataReader *reader, ListBase *list)
{
  BLO_read_list_cb(reader, list, nullptr);
}

void BLO_read_int32_array(BlendDataReader *reader, int array_size, int32_t **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_int32_array(*ptr_p, array_size);
  }
}

void BLO_read_int8_array(BlendDataReader *reader, int /*array_size*/, int8_t **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);
}

void BLO_read_uint32_array(BlendDataReader *reader, int array_size, uint32_t **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_uint32_array(*ptr_p, array_size);
  }
}

void BLO_read_float_array(BlendDataReader *reader, int array_size, float **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_float_array(*ptr_p, array_size);
  }
}

void BLO_read_float3_array(BlendDataReader *reader, int array_size, float **ptr_p)
{
  BLO_read_float_array(reader, array_size * 3, ptr_p);
}

void BLO_read_double_array(BlendDataReader *reader, int array_size, double **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_double_array(*ptr_p, array_size);
  }
}

static void convert_pointer_array_64_to_32(BlendDataReader *reader,
                                           uint array_size,
                                           const uint64_t *src,
                                           uint32_t *dst)
{
  /* Match pointer conversion rules from bh4_from_bh8 and cast_pointer. */
  if (BLO_read_requires_endian_switch(reader)) {
    for (int i = 0; i < array_size; i++) {
      uint64_t ptr = src[i];
      BLI_endian_switch_uint64(&ptr);
      dst[i] = uint32_t(ptr >> 3);
    }
  }
  else {
    for (int i = 0; i < array_size; i++) {
      dst[i] = uint32_t(src[i] >> 3);
    }
  }
}

static void convert_pointer_array_32_to_64(BlendDataReader * /*reader*/,
                                           uint array_size,
                                           const uint32_t *src,
                                           uint64_t *dst)
{
  /* Match pointer conversion rules from bh8_from_bh4 and cast_pointer_32_to_64. */
  for (int i = 0; i < array_size; i++) {
    dst[i] = src[i];
  }
}

void BLO_read_pointer_array(BlendDataReader *reader, void **ptr_p)
{
  FileData *fd = reader->fd;

  void *orig_array = newdataadr(fd, *ptr_p);
  if (orig_array == nullptr) {
    *ptr_p = nullptr;
    return;
  }

  int file_pointer_size = fd->filesdna->pointer_size;
  int current_pointer_size = fd->memsdna->pointer_size;

  /* Over-allocation is fine, but might be better to pass the length as parameter. */
  int array_size = MEM_allocN_len(orig_array) / file_pointer_size;

  void *final_array = nullptr;

  if (file_pointer_size == current_pointer_size) {
    /* No pointer conversion necessary. */
    final_array = orig_array;
  }
  else if (file_pointer_size == 8 && current_pointer_size == 4) {
    /* Convert pointers from 64 to 32 bit. */
    final_array = MEM_malloc_arrayN(array_size, 4, "new pointer array");
    convert_pointer_array_64_to_32(
        reader, array_size, (uint64_t *)orig_array, (uint32_t *)final_array);
    MEM_freeN(orig_array);
  }
  else if (file_pointer_size == 4 && current_pointer_size == 8) {
    /* Convert pointers from 32 to 64 bit. */
    final_array = MEM_malloc_arrayN(array_size, 8, "new pointer array");
    convert_pointer_array_32_to_64(
        reader, array_size, (uint32_t *)orig_array, (uint64_t *)final_array);
    MEM_freeN(orig_array);
  }
  else {
    BLI_assert(false);
  }

  *ptr_p = final_array;
}

bool BLO_read_data_is_undo(BlendDataReader *reader)
{
  return (reader->fd->flags & FD_FLAGS_IS_MEMFILE);
}

void BLO_read_data_globmap_add(BlendDataReader *reader, void *oldaddr, void *newaddr)
{
  oldnewmap_insert(reader->fd->globmap, oldaddr, newaddr, 0);
}

void BLO_read_glob_list(BlendDataReader *reader, ListBase *list)
{
  link_glob_list(reader->fd, list);
}

BlendFileReadReport *BLO_read_data_reports(BlendDataReader *reader)
{
  return reader->fd->reports;
}

bool BLO_read_lib_is_undo(BlendLibReader *reader)
{
  return (reader->fd->flags & FD_FLAGS_IS_MEMFILE);
}

Main *BLO_read_lib_get_main(BlendLibReader *reader)
{
  return reader->main;
}

BlendFileReadReport *BLO_read_lib_reports(BlendLibReader *reader)
{
  return reader->fd->reports;
}

/** \} */
