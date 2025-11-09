/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#include "fmt/core.h"

#include <cerrno>
#include <cstdarg> /* for va_start/end. */
#include <cstddef> /* for offsetof. */
#include <cstdlib> /* for atoi. */
#include <cstring>
#include <ctime>   /* for gmtime. */
#include <fcntl.h> /* for open flags (O_BINARY, O_RDONLY). */
#include <queue>

#ifndef WIN32
#  include <unistd.h> /* for read close */
#else
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h> /* for open close read */
#endif

#include <fmt/format.h>

#include "CLG_log.h"

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_asset_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_key_types.h"
#include "DNA_layer_types.h"
#include "DNA_node_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_alloc_string_storage.hh"
#include "MEM_guardedalloc.h"

#include "BLI_endian_defines.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_map.hh"
#include "BLI_memarena.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_asset.hh"
#include "BKE_blender_version.h"
#include "BKE_collection.hh"
#include "BKE_global.hh" /* for G */
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh" /* for Main */
#include "BKE_main_idmap.hh"
#include "BKE_main_invariants.hh"
#include "BKE_main_namemap.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_nla.hh"
#include "BKE_node.hh" /* for tree type defines */
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_packedFile.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_undo_system.hh"
#include "BKE_workspace.hh"

#include "DRW_engine.hh"

#include "DEG_depsgraph.hh"

#include "BLO_blend_validate.hh"
#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_undofile.hh"

#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "IMB_colormanagement.hh"

#include "readfile.hh"
#include "versioning_common.hh"

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

static CLG_LogRef LOG = {"blend.readfile"};
static CLG_LogRef LOG_UNDO = {"undo"};

#if ENDIAN_ORDER == B_ENDIAN
#  warning "Support for Big Endian endianness is deprecated and will be removed in Blender 5.0"
#endif

/* local prototypes */
static void read_libraries(FileData *basefd);
static void *read_struct(FileData *fd, BHead *bh, const char *blockname, const int id_type_index);
static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name);

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

void BLO_reportf_wrap(BlendFileReadReport *reports,
                      const eReportType type,
                      const char *format,
                      ...)
{
  char fixed_buf[1024]; /* should be long enough */

  va_list args;

  va_start(args, format);
  vsnprintf(fixed_buf, sizeof(fixed_buf), format, args);
  va_end(args);

  fixed_buf[sizeof(fixed_buf) - 1] = '\0';

  BKE_report(reports->reports, type, fixed_buf);

  if (G.background == 0) {
    BKE_report_log(type, fixed_buf, &LOG);
  }
}

/* for reporting linking messages */
static const char *library_parent_filepath(Library *lib)
{
  return lib->runtime->parent ? lib->runtime->parent->runtime->filepath_abs : "<direct>";
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

/**
 * \return `true` if the \a oldaddr key has been successfully added to the \a onm, and no existing
 * entry was overwritten.
 */
static bool oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, const int nr)
{
  if (oldaddr == nullptr || newaddr == nullptr) {
    return false;
  }

  return onm->map.add_overwrite(oldaddr, NewAddress{newaddr, nr});
}

static void oldnewmap_lib_insert(FileData *fd, const void *oldaddr, ID *newaddr, const int id_code)
{
  oldnewmap_insert(fd->libmap, oldaddr, newaddr, id_code);
}

void blo_do_versions_oldnewmap_insert(OldNewMap *onm,
                                      const void *oldaddr,
                                      void *newaddr,
                                      const int nr)
{
  oldnewmap_insert(onm, oldaddr, newaddr, nr);
}

static void *oldnewmap_lookup_and_inc(OldNewMap *onm, const void *addr, const bool increase_users)
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
  onm->map.clear();
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
  if (from->is_read_invalid) {
    mainvar->is_read_invalid = true;
  }

  MainListsArray lbarray = BKE_main_lists_get(*mainvar);
  MainListsArray fromarray = BKE_main_lists_get(*from);
  int a = fromarray.size();
  while (a--) {
    BLI_movelisttolist(lbarray[a], fromarray[a]);
  }
}

void blo_join_main(Main *bmain)
{
  BLI_assert(bmain->split_mains);
  /* For now, we could relax this requirement in the future if needed. */
  BLI_assert((*bmain->split_mains)[0] == bmain);

  if (bmain->split_mains->size() == 1) {
    bmain->split_mains.reset();
    return;
  }

  if (bmain->id_map != nullptr) {
    /* Cannot keep this since we add some IDs from joined mains. */
    BKE_main_idmap_destroy(bmain->id_map);
    bmain->id_map = nullptr;
  }
  /* Will no longer be valid after joining. */
  BKE_main_namemap_clear(*bmain);

  for (Main *tojoin : *bmain->split_mains) {
    if (tojoin == bmain) {
      continue;
    }
    BLI_assert(((tojoin->curlib->runtime->tag & LIBRARY_IS_ASSET_EDIT_FILE) != 0) ==
               tojoin->is_asset_edit_file);
    add_main_to_main(bmain, tojoin);
    tojoin->split_mains.reset();
    BKE_main_free(tojoin);
  }

  BLI_assert(bmain->split_mains.use_count() == 1);
  bmain->split_mains.reset();
}

static void split_libdata(ListBase *lb_src,
                          blender::Vector<Main *> &lib_main_array,
                          const bool do_split_packed_ids)
{
  for (ID *id = static_cast<ID *>(lb_src->first), *idnext; id; id = idnext) {
    idnext = static_cast<ID *>(id->next);

    if (id->lib && (do_split_packed_ids || (id->lib->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0)) {
      if (uint(id->lib->runtime->temp_index) < lib_main_array.size()) {
        Main *mainvar = lib_main_array[id->lib->runtime->temp_index];
        BLI_assert(mainvar->curlib == id->lib);
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

void blo_split_main(Main *bmain, const bool do_split_packed_ids)
{
  BLI_assert(!bmain->split_mains);
  bmain->split_mains = std::make_shared<blender::VectorSet<Main *>>();
  bmain->split_mains->add_new(bmain);

  if (BLI_listbase_is_empty(&bmain->libraries)) {
    return;
  }

  if (bmain->id_map != nullptr) {
    /* Cannot keep this since we remove some IDs from given main. */
    BKE_main_idmap_destroy(bmain->id_map);
    bmain->id_map = nullptr;
  }

  /* Will no longer be valid after splitting. */
  BKE_main_namemap_clear(*bmain);

  /* (Library.temp_index -> Main), lookup table */
  blender::Vector<Main *> lib_main_array;

  int i = 0;
  int lib_index = 0;
  for (Library *lib = static_cast<Library *>(bmain->libraries.first); lib;
       lib = static_cast<Library *>(lib->id.next), i++)
  {
    if (!do_split_packed_ids && (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) != 0) {
      continue;
    }
    Main *libmain = BKE_main_new();
    libmain->curlib = lib;
    libmain->versionfile = lib->runtime->versionfile;
    libmain->subversionfile = lib->runtime->subversionfile;
    libmain->has_forward_compatibility_issues = !MAIN_VERSION_FILE_OLDER_OR_EQUAL(
        libmain, BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);
    libmain->is_asset_edit_file = (lib->runtime->tag & LIBRARY_IS_ASSET_EDIT_FILE) != 0;
    libmain->colorspace = lib->runtime->colorspace;
    bmain->split_mains->add_new(libmain);
    libmain->split_mains = bmain->split_mains;
    lib->runtime->temp_index = lib_index;
    lib_main_array.append(libmain);
    lib_index++;
  }

  MainListsArray lbarray = BKE_main_lists_get(*bmain);
  i = lbarray.size();
  while (i--) {
    ID *id = static_cast<ID *>(lbarray[i]->first);
    if (id == nullptr || GS(id->name) == ID_LI) {
      /* No ID_LI data-block should ever be linked anyway, but just in case, better be explicit. */
      continue;
    }
    split_libdata(lbarray[i], lib_main_array, do_split_packed_ids);
  }
}

static void read_file_version_and_colorspace(FileData *fd, Main *main)
{
  BHead *bhead;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_GLOB) {
      FileGlobal *fg = static_cast<FileGlobal *>(
          read_struct(fd, bhead, "Data from Global block", INDEX_ID_NULL));
      if (fg) {
        if (main->versionfile != fd->fileversion) {
          /* `versionfile` remains unset when linking from a new library (`main` has then just be
           * created by `blo_find_main`). */
          BLI_assert(main->versionfile == 0);
          main->versionfile = short(fd->fileversion);
        }
        main->subversionfile = fg->subversion;
        main->minversionfile = fg->minversion;
        main->minsubversionfile = fg->minsubversion;
        main->has_forward_compatibility_issues = !MAIN_VERSION_FILE_OLDER_OR_EQUAL(
            main, BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);
        main->is_asset_edit_file = (fg->fileflags & G_FILE_ASSET_EDIT_FILE) != 0;
        STRNCPY(main->colorspace.scene_linear_name, fg->colorspace_scene_linear_name);
        main->colorspace.scene_linear_to_xyz = blender::float3x3(
            fg->colorspace_scene_linear_to_xyz);
        MEM_freeN(fg);
      }
      else if (bhead->code == BLO_CODE_ENDB) {
        break;
      }
    }
  }
  if (main->curlib) {
    main->curlib->runtime->versionfile = main->versionfile;
    main->curlib->runtime->subversionfile = main->subversionfile;
    SET_FLAG_FROM_TEST(
        main->curlib->runtime->tag, main->is_asset_edit_file, LIBRARY_IS_ASSET_EDIT_FILE);
    main->curlib->runtime->colorspace = main->colorspace;
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

static void read_file_bhead_idname_map_create(FileData *fd)
{
  /* dummy values */
  bool is_link = false;
  int code_prev = BLO_CODE_ENDB;

  fd->bhead_idname_map.emplace();
  for (BHead *bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (code_prev != bhead->code) {
      code_prev = bhead->code;
      is_link = blo_bhead_is_id_valid_type(bhead) ?
                    BKE_idtype_idcode_is_linkable(short(code_prev)) :
                    false;
    }

    if (is_link) {
      /* #idname may be null in case the ID name of the given BHead is detected as invalid (e.g.
       * because it comes from a future version of Blender allowing for longer ID names). These
       * 'invalid-named IDs' are skipped here, which will e.g. prevent them from being linked. */
      const char *idname = blo_bhead_id_name(fd, bhead);
      if (idname) {
        fd->bhead_idname_map->add(idname, bhead);
      }
    }
  }
}

void blo_readfile_invalidate(FileData *fd, Main *bmain, const char *message)
{
  /* Tag given `bmain`, and 'root 'local' main one (in case given one is a library one) as invalid.
   */
  bmain->is_read_invalid = true;
  if (bmain->split_mains) {
    (*bmain->split_mains)[0]->is_read_invalid = true;
  }

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

  /**
   * The key is the old address id referencing shared data that's written to a file, typically an
   * array. The corresponding value is the shared data at run-time.
   */
  blender::Map<uint64_t, blender::ImplicitSharingInfoAndData> shared_data_by_stored_address;
};

struct BlendLibReader {
  FileData *fd;
  Main *main;
};

static BHeadN *get_bhead(FileData *fd)
{
  BHeadN *new_bhead = nullptr;

  if (fd) {
    if (!fd->is_eof) {
      std::optional<BHead> bhead_opt = BLO_readfile_read_bhead(fd->file,
                                                               fd->blender_header.bhead_type());
      BHead *bhead = nullptr;
      if (!bhead_opt.has_value()) {
        fd->is_eof = true;
      }
      else if (bhead_opt->len < 0) {
        /* Make sure people are not trying to parse bad blend files. */
        fd->is_eof = true;
      }
      else {
        bhead = &bhead_opt.value();
      }

      /* bhead now contains the (converted) bhead structure. Now read
       * the associated data and put everything in a BHeadN (creative naming !)
       */
      if (fd->is_eof) {
        /* pass */
      }
#ifdef USE_BHEAD_READ_ON_DEMAND
      else if (fd->file->seek != nullptr && BHEAD_USE_READ_ON_DEMAND(bhead)) {
        /* Delay reading bhead content. */
        new_bhead = MEM_mallocN<BHeadN>("new_bhead");
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = nullptr;
          new_bhead->file_offset = fd->file->offset;
          new_bhead->has_data = false;
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = *bhead;
          const off64_t seek_new = fd->file->seek(fd->file, bhead->len, SEEK_CUR);
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
            MEM_mallocN(sizeof(BHeadN) + size_t(bhead->len), "new_bhead"));
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = nullptr;
#ifdef USE_BHEAD_READ_ON_DEMAND
          new_bhead->file_offset = 0; /* don't seek. */
          new_bhead->has_data = true;
#endif
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = *bhead;

          const int64_t readsize = fd->file->read(fd->file, new_bhead + 1, size_t(bhead->len));

          if (UNLIKELY(readsize != bhead->len)) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = nullptr;
          }
          else {
            if (fd->flags & FD_FLAGS_IS_MEMFILE) {
              new_bhead->is_memchunk_identical = ((UndoReader *)fd->file)->memchunk_identical;
            }
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
    if (UNLIKELY(fd->file->read(fd->file, buf, size_t(new_bhead->bhead.len)) !=
                 new_bhead->bhead.len))
    {
      success = false;
    }
    else {
      if (fd->flags & FD_FLAGS_IS_MEMFILE) {
        new_bhead->is_memchunk_identical = ((UndoReader *)fd->file)->memchunk_identical;
      }
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

const char *blo_bhead_id_name(FileData *fd, const BHead *bhead)
{
  BLI_assert(blo_bhead_is_id(bhead));
  const char *id_name = reinterpret_cast<const char *>(
      POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_name_offset));
  if (std::memchr(id_name, '\0', MAX_ID_NAME)) {
    return id_name;
  }

  /* ID name longer than MAX_ID_NAME - 1, or otherwise corrupted. */
  fd->flags |= FD_FLAGS_HAS_INVALID_ID_NAMES;
  return nullptr;
}

short blo_bhead_id_flag(const FileData *fd, const BHead *bhead)
{
  BLI_assert(blo_bhead_is_id(bhead));
  if (fd->id_flag_offset < 0) {
    return 0;
  }
  return *reinterpret_cast<const short *>(
      POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_flag_offset));
}

AssetMetaData *blo_bhead_id_asset_data_address(const FileData *fd, const BHead *bhead)
{
  BLI_assert(blo_bhead_is_id_valid_type(bhead));
  return (fd->id_asset_data_offset >= 0) ?
             *(AssetMetaData **)POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_asset_data_offset) :
             nullptr;
}

static const IDHash *blo_bhead_id_deep_hash(const FileData *fd, const BHead *bhead)
{
  BLI_assert(blo_bhead_is_id_valid_type(bhead));
  if (fd->id_flag_offset < 0 || fd->id_deep_hash_offset < 0) {
    return nullptr;
  }
  const short flag = blo_bhead_id_flag(fd, bhead);
  if (!(flag & ID_FLAG_LINKED_AND_PACKED)) {
    return nullptr;
  }
  return reinterpret_cast<const IDHash *>(
      POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_deep_hash_offset));
}

static void read_blender_header(FileData *fd)
{
  const BlenderHeaderVariant header_variant = BLO_readfile_blender_header_decode(fd->file);
  if (std::holds_alternative<BlenderHeaderInvalid>(header_variant)) {
    return;
  }
  if (std::holds_alternative<BlenderHeaderUnknown>(header_variant)) {
    fd->flags |= FD_FLAGS_FILE_FUTURE;
    return;
  }
  const BlenderHeader &header = std::get<BlenderHeader>(header_variant);
  fd->flags |= FD_FLAGS_FILE_OK;
  if (header.pointer_size == 4) {
    fd->flags |= FD_FLAGS_FILE_POINTSIZE_IS_4;
  }
  if (header.pointer_size != sizeof(void *)) {
    fd->flags |= FD_FLAGS_POINTSIZE_DIFFERS;
  }
  if (header.endian != ENDIAN_ORDER) {
    fd->flags |= FD_FLAGS_SWITCH_ENDIAN;
  }
  fd->fileversion = header.file_version;
  fd->blender_header = header;
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
      const FileGlobal *fg = reinterpret_cast<const FileGlobal *>(&bhead[1]);
      BLI_STATIC_ASSERT(offsetof(FileGlobal, subvstr) == 0, "Must be first: subvstr")
      char num[5];
      memcpy(num, fg->subvstr, 4);
      num[4] = 0;
      subversion = atoi(num);
    }
    else if (bhead->code == BLO_CODE_DNA1) {
      BLI_assert((fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
      const bool do_alias = false; /* Postpone until after #blo_do_versions_dna runs. */
      fd->filesdna = DNA_sdna_from_data(&bhead[1], bhead->len, true, do_alias, r_error_message);
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
        fd->id_flag_offset = DNA_struct_member_offset_by_name_with_alias(
            fd->filesdna, "ID", "short", "flag");
        fd->id_deep_hash_offset = DNA_struct_member_offset_by_name_with_alias(
            fd->filesdna, "ID", "IDHash", "deep_hash");

        fd->filesubversion = subversion;

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
      BLI_assert((fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
      int *data = (int *)(bhead + 1);

      if (bhead->len < sizeof(int[2])) {
        break;
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

/**
 * ID names are truncated to their maximum allowed length at a very low level of the readfile code
 * (see #read_id_struct).
 *
 * However, ensuring they remain unique can only be done once all IDs have been read and put in
 * Main.
 *
 * \note #BKE_main_namemap_validate_and_fix could also be used here - but it is designed for a more
 * general usage, where names are typically expected to be valid, and would generate noisy logs in
 * this case, where names are expected to _not_ be valid.
 */
static void long_id_names_ensure_unique_id_names(Main *bmain)
{
  ListBase *lb_iter;
  /* Using a set is needed, to avoid renaming names when there is no collision, and deal with IDs
   * being moved around in their list when renamed. A simple set is enough, since here only local
   * IDs are processed. */
  blender::Set<blender::StringRef> used_names;
  blender::Set<ID *> processed_ids;

  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb_iter) {
    LISTBASE_FOREACH_MUTABLE (ID *, id_iter, lb_iter) {
      if (processed_ids.contains(id_iter)) {
        continue;
      }
      processed_ids.add_new(id_iter);
      /* Linked IDs can be fully ignored here, 'long names' IDs cannot be linked in any way. */
      if (ID_IS_LINKED(id_iter)) {
        continue;
      }
      if (!used_names.contains(id_iter->name)) {
        used_names.add_new(id_iter->name);
        continue;
      }

      BKE_id_new_name_validate(
          *bmain, *lb_iter, *id_iter, nullptr, IDNewNameMode::RenameExistingNever, false);
      BLI_assert(!used_names.contains(id_iter->name));
      used_names.add_new(id_iter->name);
      CLOG_DEBUG(&LOG, "ID name has been de-duplicated to '%s'", id_iter->name);
    }
  }
  FOREACH_MAIN_LISTBASE_END;
}

/**
 * Iterate all IDs from Actions and look for non-null terminated #ActionSlot.identifier. Also
 * handle slot users (in Action constraint, AnimData, and NLA strips).
 *
 * This is for forward compatibility, if the blendfile was saved from a version allowing larger
 * MAX_ID_NAME value than the current one (introduced when switching from MAX_ID_NAME = 66 to
 * MAX_ID_NAME = 258).
 */
static void long_id_names_process_action_slots_identifiers(Main *bmain)
{
  /* NOTE: A large part of this code follows a similar logic to
   * #foreach_action_slot_use_with_references.
   *
   * However, no slot identifier should ever be skipped here, even if it is not in use in any way,
   * since it is critical to remove all non-null terminated strings.
   */

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    switch (GS(id_iter->name)) {
      case ID_AC: {
        bool has_truncated_slot_identifer = false;
        bAction *act = reinterpret_cast<bAction *>(id_iter);
        for (int i = 0; i < act->slot_array_num; i++) {
          if (BLI_str_utf8_truncate_at_size(act->slot_array[i]->identifier, MAX_ID_NAME)) {
            CLOG_DEBUG(&LOG,
                       "Truncated too long action slot name to '%s'",
                       act->slot_array[i]->identifier);
            has_truncated_slot_identifer = true;
          }
        }
        if (!has_truncated_slot_identifer) {
          continue;
        }

        /* If there are truncated slots identifiers, ensuring their uniqueness must happen in a
         * second loop, to avoid e.g. an attempt to read a slot identifier that has not yet been
         * truncated. */
        for (int i = 0; i < act->slot_array_num; i++) {
          BLI_uniquename_cb(
              [&](const blender::StringRef name) -> bool {
                for (int j = 0; j < act->slot_array_num; j++) {
                  if (i == j) {
                    continue;
                  }
                  if (act->slot_array[j]->identifier == name) {
                    return true;
                  }
                }
                return false;
              },
              "",
              '.',
              act->slot_array[i]->identifier,
              sizeof(act->slot_array[i]->identifier));
        }
        break;
      }
      case ID_OB: {
        auto visit_constraint = [](const bConstraint &constraint) -> bool {
          if (constraint.type != CONSTRAINT_TYPE_ACTION) {
            return true;
          }
          bActionConstraint *constraint_data = static_cast<bActionConstraint *>(constraint.data);
          if (BLI_str_utf8_truncate_at_size(constraint_data->last_slot_identifier, MAX_ID_NAME)) {
            CLOG_DEBUG(&LOG,
                       "Truncated too long bActionConstraint.last_slot_identifier to '%s'",
                       constraint_data->last_slot_identifier);
          }
          return true;
        };

        Object *object = reinterpret_cast<Object *>(id_iter);
        LISTBASE_FOREACH (bConstraint *, con, &object->constraints) {
          visit_constraint(*con);
        }
        if (object->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
            LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
              visit_constraint(*con);
            }
          }
        }
      }
        ATTR_FALLTHROUGH;
      default: {
        AnimData *anim_data = BKE_animdata_from_id(id_iter);
        if (anim_data) {
          if (BLI_str_utf8_truncate_at_size(anim_data->last_slot_identifier, MAX_ID_NAME)) {
            CLOG_DEBUG(&LOG,
                       "Truncated too long AnimData.last_slot_identifier to '%s'",
                       anim_data->last_slot_identifier);
          }
          if (BLI_str_utf8_truncate_at_size(anim_data->tmp_last_slot_identifier, MAX_ID_NAME)) {
            CLOG_DEBUG(&LOG,
                       "Truncated too long AnimData.tmp_last_slot_identifier to '%s'",
                       anim_data->tmp_last_slot_identifier);
          }

          blender::bke::nla::foreach_strip_adt(*anim_data, [&](NlaStrip *strip) -> bool {
            if (BLI_str_utf8_truncate_at_size(strip->last_slot_identifier, MAX_ID_NAME)) {
              CLOG_DEBUG(&LOG,
                         "Truncated too long NlaStrip.last_slot_identifier to '%s'",
                         strip->last_slot_identifier);
            }

            return true;
          });
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Data API
 * \{ */

static FileData *filedata_new(BlendFileReadReport *reports)
{
  BLI_assert(reports != nullptr);

  FileData *fd = MEM_new<FileData>(__func__);

  fd->memsdna = DNA_sdna_current_get();

  fd->datamap = oldnewmap_new();
  fd->globmap = oldnewmap_new();
  fd->libmap = oldnewmap_new();
  fd->id_by_deep_hash = std::make_shared<blender::Map<IDHash, ID *>>();

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

    FileGlobal *fg = static_cast<FileGlobal *>(
        read_struct(fd, bhead, "Data from Global block", INDEX_ID_NULL));
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
  read_blender_header(fd);

  if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
    BLI_STATIC_ASSERT(ENDIAN_ORDER == L_ENDIAN, "Blender only builds on little endian systems")
    BKE_reportf(reports,
                RPT_ERROR,
                "Blend file '%s' created by a Big Endian version of Blender, support for "
                "these files has been removed in Blender 5.0, use an older version of Blender "
                "to open and convert it.",
                fd->relabase);
    blo_filedata_free(fd);
    fd = nullptr;
  }
  else if (fd->flags & FD_FLAGS_FILE_OK) {
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
  else if (fd->flags & FD_FLAGS_FILE_FUTURE) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Cannot read blend file '%s', incomplete header, may be from a newer version of Blender",
        fd->relabase);
    blo_filedata_free(fd);
    fd = nullptr;
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Failed to read file '%s', not a blend file", fd->relabase);
    blo_filedata_free(fd);
    fd = nullptr;
  }

  return fd;
}

static FileData *blo_filedata_from_file_descriptor(const char *filepath,
                                                   BlendFileReadReport *reports,
                                                   const int filedes)
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

  BLI_stat_t stat;
  if (BLI_stat(filepath, &stat) != -1) {
    fd->file_stat = stat;
  }

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
    read_blender_header(fd);
    if (fd->flags & FD_FLAGS_FILE_OK) {
      return fd;
    }
    blo_filedata_free(fd);
  }
  return nullptr;
}

FileData *blo_filedata_from_memory(const void *mem,
                                   const int memsize,
                                   BlendFileReadReport *reports)
{
  if (!mem || memsize < MIN_SIZEOFBLENDERHEADER) {
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
  /* Free all BHeadN data blocks */
#ifdef NDEBUG
  BLI_freelistN(&fd->bhead_list);
#else
  /* Sanity check we're not keeping memory we don't need. */
  LISTBASE_FOREACH_MUTABLE (BHeadN *, new_bhead, &fd->bhead_list) {
#  ifdef USE_BHEAD_READ_ON_DEMAND
    if (fd->file->seek != nullptr && BHEAD_USE_READ_ON_DEMAND(&new_bhead->bhead)) {
      BLI_assert(new_bhead->has_data == 0);
    }
#  endif
    MEM_freeN(new_bhead);
  }
#endif
  fd->file->close(fd->file);

  if (fd->filesdna) {
    DNA_sdna_free(fd->filesdna);
  }
  if (fd->compflags) {
    MEM_freeN(fd->compflags);
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
  if (fd->libmap) {
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

  MEM_delete(fd);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Thumbnail from Blend File
 * \{ */

BlendThumbnail *BLO_thumbnail_from_file(const char *filepath)
{
  BlendThumbnail *data = nullptr;

  FileData *fd = blo_filedata_from_file_minimal(filepath);
  if (fd) {
    if (const int *fd_data = read_file_thumbnail(fd)) {
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
  }

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

/* Very rarely needed, allows some form of ID remapping as part of readfile process.
 *
 * Currently only used to remap duplicate library pointers.
 */
static void change_ID_pointer_to_real_ID_pointer_fd(FileData *fd, const void *old, void *newp)
{
  for (NewAddress &entry : fd->libmap->map.values()) {
    if (old == entry.newp) {
      BLI_assert(BKE_idtype_idcode_is_valid(short(entry.nr)));
      entry.newp = newp;
      if (newp) {
        entry.nr = GS(((ID *)newp)->name);
      }
    }
  }
}

static FileData *change_ID_link_filedata_get(Main *bmain, FileData *basefd)
{
  if (bmain->curlib) {
    return bmain->curlib->runtime->filedata;
  }
  else {
    return basefd;
  }
}

static void change_link_placeholder_to_real_ID_pointer(FileData *basefd, void *old, void *newp)
{
  for (Main *mainptr : *basefd->bmain->split_mains) {
    FileData *fd = change_ID_link_filedata_get(mainptr, basefd);
    if (fd) {
      change_link_placeholder_to_real_ID_pointer_fd(fd, old, newp);
    }
  }
}

static void change_ID_pointer_to_real_ID_pointer(FileData *basefd, void *old, void *newp)
{
  for (Main *mainptr : *basefd->bmain->split_mains) {
    FileData *fd = change_ID_link_filedata_get(mainptr, basefd);
    if (fd) {
      change_ID_pointer_to_real_ID_pointer_fd(fd, old, newp);
    }
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
    ID *id, const IDCacheKey *key, void **cache_p, const uint flags, void *cache_storage_v)
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
static void blo_cache_storage_entry_clear_in_old(ID * /*id*/,
                                                 const IDCacheKey *key,
                                                 void **cache_p,
                                                 const uint /*flags*/,
                                                 void *cache_storage_v)
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
    fd->cache_storage = MEM_mallocN<BLOCacheStorage>(__func__);
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

/**
 * Generate the final allocation string reference for read blocks of data. If \a blockname is
 * given, use it as 'owner block' info, otherwise use the id type index to get that info.
 *
 * \note These strings are stored until Blender exits
 */
static const char *get_alloc_name(FileData *fd,
                                  BHead *bh,
                                  const char *blockname,
                                  const int id_type_index = INDEX_ID_NULL)
{
#ifndef NDEBUG
  /* Storage key is a pair of (string , int), where the first is the concatenation of the 'owner
   * block' string and DNA struct type name, and the second the length of the array, as defined by
   * the #BHead.nr value. */
  using keyT = const std::pair<const std::string, const int>;
#else
  /* Storage key is simple int, which is the ID type index. */
  using keyT = int;
#endif
  constexpr std::string_view STORAGE_ID = "readfile";

  /* NOTE: This is thread_local storage, so as long as the handling of a same FileData is not
   * spread across threads (which is not supported at all currently), this is thread-safe. */
  if (!fd->storage_handle) {
    fd->storage_handle = &intern::memutil::alloc_string_storage_get<keyT, blender::DefaultHash>(
        std::string(STORAGE_ID));
  }
  intern::memutil::AllocStringStorage<keyT, blender::DefaultHash> &storage =
      *static_cast<intern::memutil::AllocStringStorage<keyT, blender::DefaultHash> *>(
          fd->storage_handle);

  const bool is_id_data = !blockname && (id_type_index >= 0 && id_type_index < INDEX_ID_MAX);

#ifndef NDEBUG
  /* Local storage of id type names, for fast access to this info. */
  static const std::array<std::string, INDEX_ID_MAX> id_alloc_names = [] {
    auto n = decltype(id_alloc_names)();
    for (int idtype_index = 0; idtype_index < INDEX_ID_MAX; idtype_index++) {
      const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_idtype_index(idtype_index);
      BLI_assert(idtype_info);
      if (idtype_index == INDEX_ID_NULL) {
        /* #INDEX_ID_NULL returns the #IDType_ID_LINK_PLACEHOLDER type info, here we will rather
         * use it for unknown/invalid ID types. */
        n[size_t(idtype_index)] = "UNKNWOWN";
      }
      else {
        n[size_t(idtype_index)] = idtype_info->name;
      }
    }
    return n;
  }();

  const std::string block_alloc_name = is_id_data ? id_alloc_names[id_type_index] : blockname;
  const std::string struct_name = DNA_struct_identifier(fd->filesdna, bh->SDNAnr);
  keyT key{block_alloc_name + struct_name, bh->nr};
  if (!storage.contains(key)) {
    const std::string alloc_string = fmt::format(
        fmt::runtime(is_id_data ? "{}{} (for ID type '{}')" : "{}{} (for block '{}')"),
        struct_name,
        bh->nr > 1 ? fmt::format("[{}]", bh->nr) : "",
        block_alloc_name);
    return storage.insert(key, alloc_string);
  }
  return storage.find(key);
#else
  /* Simple storage for pure release builds, using integer as key, one entry for each ID type. */
  UNUSED_VARS_NDEBUG(bh);
  if (is_id_data) {
    if (UNLIKELY(!storage.contains(id_type_index))) {
      if (id_type_index == INDEX_ID_NULL) {
        return storage.insert(id_type_index, "Data from UNKNOWN");
      }
      const IDTypeInfo *id_type = BKE_idtype_get_info_from_idtype_index(id_type_index);
      const std::string alloc_string = fmt::format("Data from '{}' ID type", id_type->name);
      return storage.insert(id_type_index, alloc_string);
    }
    return storage.find(id_type_index);
  }
  return blockname;
#endif
}

static void *read_struct(FileData *fd, BHead *bh, const char *blockname, const int id_type_index)
{
  void *temp = nullptr;

  if (bh->len) {
#ifdef USE_BHEAD_READ_ON_DEMAND
    BHead *bh_orig = bh;
#endif

    /* Endianness switch is based on file DNA.
     *
     * NOTE: raw data (aka #SDNA_RAW_DATA_STRUCT_INDEX #SDNAnr) is not handled here, it's up to
     * the calling code to manage this. */
    BLI_assert((fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
    BLI_STATIC_ASSERT(SDNA_RAW_DATA_STRUCT_INDEX == 0, "'raw data' SDNA struct index should be 0")
    if (bh->SDNAnr > SDNA_RAW_DATA_STRUCT_INDEX && (fd->flags & FD_FLAGS_SWITCH_ENDIAN)) {
#ifdef USE_BHEAD_READ_ON_DEMAND
      if (BHEADN_FROM_BHEAD(bh)->has_data == false) {
        bh = blo_bhead_read_full(fd, bh);
        if (UNLIKELY(bh == nullptr)) {
          fd->flags &= ~FD_FLAGS_FILE_OK;
          return nullptr;
        }
      }
#endif
    }

    if (fd->compflags[bh->SDNAnr] != SDNA_CMP_REMOVED) {
      const char *alloc_name = get_alloc_name(fd, bh, blockname, id_type_index);
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
        temp = DNA_struct_reconstruct(
            fd->reconstruct_info, bh->SDNAnr, bh->nr, (bh + 1), alloc_name);
      }
      else {
        /* SDNA_CMP_EQUAL */
        const int alignment = DNA_struct_alignment(fd->filesdna, bh->SDNAnr);
        temp = MEM_mallocN_aligned(bh->len, alignment, alloc_name);
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

static ID *read_id_struct(FileData *fd, BHead *bh, const char *blockname, const int id_type_index)
{
  ID *id = static_cast<ID *>(read_struct(fd, bh, blockname, id_type_index));
  if (!id) {
    return id;
  }

  /* Invalid ID name (probably from 'too long' ID name from a future Blender version).
   *
   * They can only be truncated here, ensuring that all ID names remain unique happens later, after
   * reading all local IDs, but before linking them, see the call to
   * #long_id_names_ensure_unique_id_names in #blo_read_file_internal. */
  if (BLI_str_utf8_truncate_at_size(id->name + 2, MAX_ID_NAME - 2)) {
    fd->flags |= FD_FLAGS_HAS_INVALID_ID_NAMES;
    CLOG_DEBUG(&LOG, "Truncated too long ID name to '%s'", id->name);
  }

  return id;
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
  bNodeTree *nodetree = blender::bke::node_tree_from_id(id);
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

static void direct_link_id_override_property(BlendDataReader *reader,
                                             IDOverrideLibraryProperty *op)
{
  BLO_read_string(reader, &op->rna_path);

  op->tag = 0; /* Runtime only. */

  BLO_read_struct_list(reader, IDOverrideLibraryPropertyOperation, &op->operations);

  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    BLO_read_string(reader, &opop->subitem_reference_name);
    BLO_read_string(reader, &opop->subitem_local_name);

    opop->tag = 0; /* Runtime only. */
  }
}

static void direct_link_id_common(BlendDataReader *reader,
                                  Library *current_library,
                                  ID *id,
                                  ID *id_old,
                                  int id_tag,
                                  ID_Readfile_Data::Tags id_read_tags);

static void direct_link_id_embedded_id(BlendDataReader *reader,
                                       Library *current_library,
                                       ID *id,
                                       ID *id_old)
{
  /* Handle 'private IDs'. */
  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->compositing_node_group) {
      /* If `scene->compositing_node_group != nullptr`, then this means the blend file was created
       * by a version that wrote the compositing_node_group as its own ID datablock. Since
       * `scene->nodetree` was written for forward compatibility reasons only, we can ignore it. */
      scene->nodetree = nullptr;
    }
  }
  bNodeTree **nodetree = blender::bke::node_tree_ptr_from_id(id);
  if (nodetree != nullptr && *nodetree != nullptr) {
    BLO_read_struct(reader, bNodeTree, nodetree);
    if (!*nodetree || !BKE_idtype_idcode_is_valid(GS((*nodetree)->id.name))) {
      BLO_reportf_wrap(
          reader->fd->reports,
          RPT_ERROR,
          RPT_("Data-block '%s' had an invalid embedded node group, which has not been read"),
          id->name);
      MEM_SAFE_FREE(*nodetree);
    }
    else {
      direct_link_id_common(reader,
                            current_library,
                            (ID *)*nodetree,
                            id_old != nullptr ? (ID *)blender::bke::node_tree_from_id(id_old) :
                                                nullptr,
                            0,
                            ID_Readfile_Data::Tags{});
      blender::bke::node_tree_blend_read_data(reader, id, *nodetree);
    }
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != nullptr) {
      BLO_read_struct(reader, Collection, &scene->master_collection);
      if (!scene->master_collection ||
          !BKE_idtype_idcode_is_valid(GS(scene->master_collection->id.name)))
      {
        BLO_reportf_wrap(
            reader->fd->reports,
            RPT_ERROR,
            RPT_("Scene '%s' had an invalid root collection, which has not been read"),
            BKE_id_name(*id));
        MEM_SAFE_FREE(scene->master_collection);
      }
      else {
        direct_link_id_common(reader,
                              current_library,
                              &scene->master_collection->id,
                              id_old != nullptr ? &((Scene *)id_old)->master_collection->id :
                                                  nullptr,
                              0,
                              ID_Readfile_Data::Tags{});
        BKE_collection_blend_read_data(reader, scene->master_collection, &scene->id);
      }
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

static void readfile_id_runtime_data_ensure(ID &id)
{
  if (id.runtime->readfile_data) {
    return;
  }
  id.runtime->readfile_data = MEM_callocN<ID_Readfile_Data>(__func__);
}

ID_Readfile_Data::Tags BLO_readfile_id_runtime_tags(ID &id)
{
  if (!id.runtime->readfile_data) {
    return ID_Readfile_Data::Tags{};
  }
  return id.runtime->readfile_data->tags;
}

ID_Readfile_Data::Tags &BLO_readfile_id_runtime_tags_for_write(ID &id)
{
  readfile_id_runtime_data_ensure(id);
  return id.runtime->readfile_data->tags;
}

void BLO_readfile_id_runtime_data_free(ID &id)
{
  MEM_SAFE_FREE(id.runtime->readfile_data);
}

void BLO_readfile_id_runtime_data_free_all(Main &bmain)
{
  ID *id;
  FOREACH_MAIN_ID_BEGIN (&bmain, id) {
    /* Handle the ID itself. */
    BLO_readfile_id_runtime_data_free(*id);

    /* Handle its embedded IDs, because they do not get referenced by bmain. */
    if (GS(id->name) == ID_SCE) {
      Collection *collection = reinterpret_cast<Scene *>(id)->master_collection;
      if (collection) {
        BLO_readfile_id_runtime_data_free(collection->id);
      }
    }

    bNodeTree *node_tree = blender::bke::node_tree_from_id(id);
    if (node_tree) {
      BLO_readfile_id_runtime_data_free(node_tree->id);
    }
  }
  FOREACH_MAIN_ID_END;
}

static void direct_link_id_common(BlendDataReader *reader,
                                  Library *current_library,
                                  ID *id,
                                  ID *id_old,
                                  const int id_tag,
                                  const ID_Readfile_Data::Tags id_read_tags)
{
  /* This should have been caught already, either by a call to `#blo_bhead_is_id_valid_type` for
   * regular IDs, or in `#direct_link_id_embedded_id` for embedded ones. */
  BLI_assert_msg(BKE_idtype_idcode_is_valid(GS(id->name)),
                 "Unknown or invalid ID type, this should never happen");

  BLI_assert(id->runtime == nullptr);
  BKE_libblock_runtime_ensure(*id);

  if (!BLO_read_data_is_undo(reader)) {
    /* When actually reading a file, we do want to reset/re-generate session UIDS.
     * In undo case, we want to re-use existing ones. */
    id->session_uid = MAIN_ID_SESSION_UID_UNSET;
  }

  if (id->flag & ID_FLAG_LINKED_AND_PACKED) {
    if (!current_library) {
      CLOG_ERROR(&LOG,
                 "Data-block '%s' flagged as packed, but without a valid library, fixing by "
                 "making fully local...",
                 id->name);
      id->flag &= ~ID_FLAG_LINKED_AND_PACKED;
    }
    else if ((current_library->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0) {
      CLOG_ERROR(&LOG,
                 "Data-block '%s' flagged as packed, but using a regular library, fixing by "
                 "making fully linked...",
                 id->name);
      id->flag &= ~ID_FLAG_LINKED_AND_PACKED;
    }
  }
  id->lib = current_library;
  if (id->lib) {
    /* Always fully clear fake user flag for linked data. */
    id->flag &= ~ID_FLAG_FAKEUSER;
  }
  id->us = ID_FAKE_USERS(id);
  id->icon_id = 0;
  id->newid = nullptr; /* Needed because .blend may have been saved with crap value here... */
  id->orig_id = nullptr;
  id->py_instance = nullptr;

  /* Initialize with provided tag. */
  if (BLO_read_data_is_undo(reader)) {
    id->tag = (id_tag & ~ID_TAG_KEEP_ON_UNDO) | (id->tag & ID_TAG_KEEP_ON_UNDO);
  }
  else {
    id->tag = id_tag;
  }

  readfile_id_runtime_data_ensure(*id);
  id->runtime->readfile_data->tags = id_read_tags;

  if ((id_tag & ID_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uid_ensure(id);
  }

  if (ID_IS_LINKED(id)) {
    id->library_weak_reference = nullptr;
  }
  else {
    BLO_read_struct(reader, LibraryWeakReference, &id->library_weak_reference);
  }

  if (BLO_readfile_id_runtime_tags(*id).is_link_placeholder) {
    /* For placeholder we only need to set the tag and properly initialize generic ID fields above,
     * no further data to read. */
    return;
  }

  BKE_animdata_blend_read_data(reader, id);

  if (id->asset_data) {
    BLO_read_struct(reader, AssetMetaData, &id->asset_data);
    BKE_asset_metadata_read(reader, id->asset_data);
    /* Restore runtime asset type info. */
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    id->asset_data->local_type_info = id_type->asset_type_info;
  }

  /* Link direct data of ID properties. */
  if (id->properties) {
    BLO_read_struct(reader, IDProperty, &id->properties);
    /* this case means the data was written incorrectly, it should not happen */
    IDP_BlendDataRead(reader, &id->properties);
  }

  if (id->system_properties) {
    BLO_read_struct(reader, IDProperty, &id->system_properties);
    IDP_BlendDataRead(reader, &id->system_properties);
  }

  id->flag &= ~ID_FLAG_INDIRECT_WEAK_LINK;

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
    BLO_read_struct(reader, IDOverrideLibrary, &id->override_library);
    /* Work around file corruption on writing, see #86853. */
    if (id->override_library != nullptr) {
      BLO_read_struct_list(reader, IDOverrideLibraryProperty, &id->override_library->properties);
      LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &id->override_library->properties) {
        direct_link_id_override_property(reader, op);
      }
      id->override_library->runtime = nullptr;
    }
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
     * Also, other libraries may not have been linked yet, while we could check for
     * #ID_Readfile_Data::Tags.needs_linking the library pointer check is sufficient. */
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

static void library_filedata_release(Library *lib)
{
  if (lib->runtime->filedata) {
    BLI_assert(lib->runtime->versionfile != 0);
    BLI_assert_msg(!lib->runtime->is_filedata_owner || (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0,
                   "Packed Archive libraries should never own their filedata");
    if (lib->runtime->is_filedata_owner) {
      blo_filedata_free(lib->runtime->filedata);
    }

    lib->runtime->filedata = nullptr;
  }
  lib->runtime->is_filedata_owner = false;
}

static void direct_link_library(FileData *fd, Library *lib, Main *main)
{
  /* Make sure we have full path in lib->runtime->filepath_abs */
  /* NOTE: Since existing libraries are searched by their absolute path, this has to be generated
   * before the lookup below. Otherwise, in case the stored absolute filepath is not 'correct' (may
   * be empty, or have been stored in a different 'relative path context'), the comparison below
   * will always fail, leading to creating duplicates IDs of a same library. */
  /* TODO: May be worth checking whether comparison below could use `lib->filepath` instead? */
  STRNCPY(lib->runtime->filepath_abs, lib->filepath);
  BLI_path_abs(lib->runtime->filepath_abs, fd->relabase);
  BLI_path_normalize(lib->runtime->filepath_abs);

  /* check if the library was already read */
  for (Main *newmain : *fd->bmain->split_mains) {
    if (!newmain->curlib) {
      continue;
    }
    if (newmain->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE || lib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
      /* Archive library should never be used to link new data, and there can be many such
       * archive libraries for a same 'real' blendfile one. */
      continue;
    }
    if (BLI_path_cmp(newmain->curlib->runtime->filepath_abs, lib->runtime->filepath_abs) == 0) {
      BLO_reportf_wrap(fd->reports,
                       RPT_WARNING,
                       RPT_("Library '%s', '%s' had multiple instances, save and reload!"),
                       lib->filepath,
                       lib->runtime->filepath_abs);

      change_ID_pointer_to_real_ID_pointer(fd, lib, newmain->curlib);
      // change_link_placeholder_to_real_ID_pointer_fd(fd, lib, newmain->curlib);

      BLI_remlink(&main->libraries, lib);
      MEM_freeN(lib);

      /* Now, since Blender always expect **last** Main pointer from fd->bmain->split_mains
       * to be the active library Main pointer, where to add all non-library data-blocks found in
       * file next, we have to switch that 'dupli' found Main to latest position in the list!
       * Otherwise, you get weird disappearing linked data on a rather inconsistent basis.
       * See also #53977 for reproducible case. */
      /* Note: the change in order in `fd->bmain->split_mains` should not be an issue here, and we
       * return immediately. */
      fd->bmain->split_mains->remove_contained(newmain);
      fd->bmain->split_mains->add_new(newmain);
      BLI_assert((*fd->bmain->split_mains)[0] == fd->bmain);

      return;
    }
  }

  //  printf("direct_link_library: filepath %s\n", lib->filepath);
  //  printf("direct_link_library: filepath_abs %s\n", lib->runtime->filepath_abs);

  BlendDataReader reader = {fd};
  BKE_packedfile_blend_read(&reader, &lib->packedfile, lib->filepath);

  /* new main */
  Main *newmain = BKE_main_new();
  fd->bmain->split_mains->add_new(newmain);
  newmain->split_mains = fd->bmain->split_mains;
  newmain->curlib = lib;

  if (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
    /* Archive libraries contains only embedded linked IDs, which by definition have the same
     * fileversion as the blendfile that contains them. */
    lib->runtime->versionfile = newmain->versionfile = fd->bmain->versionfile;
    lib->runtime->subversionfile = newmain->subversionfile = fd->bmain->subversionfile;

    /* The filedata of a packed archive library should always be the one of the blendfile which
     * defines the library ID and packs its linked IDs. */
    lib->runtime->filedata = fd;
    lib->runtime->is_filedata_owner = false;
  }

  lib->runtime->parent = nullptr;

  id_us_ensure_real(&lib->id);

  /* Should always be null, Library IDs in Blender are always local. */
  lib->id.lib = nullptr;
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
        STRNCPY(lib->filepath, lib->runtime->filepath_abs);
      }
    }
  }
  else {
    LISTBASE_FOREACH (Library *, lib, &main->libraries) {
      /* Libraries store both relative and abs paths, recreate relative paths,
       * relative to the blend file since indirectly linked libraries will be
       * relative to their direct linked library. */
      if (BLI_path_is_rel(lib->filepath)) { /* if this is relative to begin with? */
        STRNCPY(lib->filepath, lib->runtime->filepath_abs);
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
  ID *ph_id = BKE_libblock_alloc_notest(idcode);
  BKE_libblock_runtime_ensure(*ph_id);

  *((short *)ph_id->name) = idcode;
  BLI_strncpy(ph_id->name + 2, idname, sizeof(ph_id->name) - 2);
  BKE_libblock_init_empty(ph_id);
  ph_id->lib = mainvar->curlib;
  ph_id->tag = tag | ID_TAG_MISSING;
  ph_id->us = ID_FAKE_USERS(ph_id);
  ph_id->icon_id = 0;

  if (was_liboverride) {
    /* 'Abuse' `ID_TAG_LIBOVERRIDE_NEED_RESYNC` to mark that placeholder missing linked ID as
     * being a liboverride.
     *
     * This will be used by the liboverride resync process, see #lib_override_library_resync. */
    ph_id->tag |= ID_TAG_LIBOVERRIDE_NEED_RESYNC;
  }

  BLI_addtail(lb, ph_id);
  id_sort_by_name(lb, ph_id, nullptr);

  if (mainvar->id_map != nullptr) {
    BKE_main_idmap_insert_id(mainvar->id_map, ph_id);
  }

  if ((tag & ID_TAG_TEMP_MAIN) == 0) {
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
    if (obdata != nullptr && obdata->tag & ID_TAG_MISSING) {
      BKE_object_materials_sync_length(bmain, ob, obdata);
    }
  }
}

static bool direct_link_id(FileData *fd,
                           Main *main,
                           const int tag,
                           const ID_Readfile_Data::Tags id_read_tags,
                           ID *id,
                           ID *id_old)
{
  BlendDataReader reader = {fd};
  /* Sharing is only allowed within individual data-blocks currently. The clearing is done
   * explicitly here, in case the `reader` is used by multiple IDs in the future. */
  reader.shared_data_by_stored_address.clear();

  /* Read part of datablock that is common between real and embedded datablocks. */
  direct_link_id_common(&reader, main->curlib, id, id_old, tag, id_read_tags);

  if (BLO_readfile_id_runtime_tags(*id).is_link_placeholder) {
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
static BHead *read_data_into_datamap(FileData *fd,
                                     BHead *bhead,
                                     const char *allocname,
                                     const int id_type_index)
{
  bhead = blo_bhead_next(fd, bhead);

  while (bhead && bhead->code == BLO_CODE_DATA) {
    void *data = read_struct(fd, bhead, allocname, id_type_index);
    if (data) {
      const bool is_new = oldnewmap_insert(fd->datamap, bhead->old, data, 0);
      if (!is_new) {
        CLOG_ERROR(&LOG,
                   "Blendfile corruption: Invalid, or multiple `bhead` with same old address "
                   "value (%p) for a given ID.",
                   bhead->old);
      }
    }

    bhead = blo_bhead_next(fd, bhead);
  }

  return bhead;
}

/* Add a Main (and optionally create a matching Library ID), for the given filepath.
 *
 * - If `lib` is `nullptr`, create a new Library ID, otherwise only create a new Main for the given
 * library.
 * - `reference_lib` is the 'archive parent' of an archive (packed) library, can be null and will
 * be ignored otherwise. */
static Main *blo_add_main_for_library(FileData *fd,
                                      Library *lib,
                                      Library *reference_lib,
                                      const char *lib_filepath,
                                      char (&filepath_abs)[FILE_MAX],
                                      const bool is_packed_library)
{
  Main *bmain = BKE_main_new();
  fd->bmain->split_mains->add_new(bmain);
  bmain->split_mains = fd->bmain->split_mains;

  if (!lib) {
    /* Add library data-block itself to 'main' Main, since libraries are **never** linked data.
     * Fixes bug where you could end with all ID_LI data-blocks having the same name... */
    lib = BKE_id_new<Library>(fd->bmain,
                              reference_lib ? BKE_id_name(reference_lib->id) :
                                              BLI_path_basename(lib_filepath));

    /* Important, consistency with main ID reading code from read_libblock(). */
    lib->id.us = ID_FAKE_USERS(lib);

    /* Matches direct_link_library(). */
    id_us_ensure_real(&lib->id);

    STRNCPY(lib->filepath, lib_filepath);
    STRNCPY(lib->runtime->filepath_abs, filepath_abs);

    if (is_packed_library) {
      /* FIXME: This logic is very similar to the code in BKE_library dealing with archived
       * libraries (e.g. #add_archive_library). Might be good to try to factorize it. */
      lib->archive_parent_library = reference_lib;
      constexpr uint16_t copy_flag = ~LIBRARY_FLAG_IS_ARCHIVE;
      lib->flag = (reference_lib->flag & copy_flag) | LIBRARY_FLAG_IS_ARCHIVE;

      lib->runtime->parent = reference_lib->runtime->parent;
      /* Only copy a subset of the reference library tags. E.g. an archive library should never be
       * considered as writable, so never copy #LIBRARY_ASSET_FILE_WRITABLE. This may need further
       * tweaking still. */
      constexpr uint16_t copy_tag = (LIBRARY_TAG_RESYNC_REQUIRED | LIBRARY_ASSET_EDITABLE |
                                     LIBRARY_IS_ASSET_EDIT_FILE);
      lib->runtime->tag = reference_lib->runtime->tag & copy_tag;

      /* The filedata of a packed archive library should always be the one of the blendfile which
       * defines the library ID and packs its linked IDs. */
      lib->runtime->filedata = fd;
      lib->runtime->is_filedata_owner = false;

      reference_lib->runtime->archived_libraries.append(lib);
    }
  }
  else {
    if (is_packed_library) {
      BLI_assert(lib->flag & LIBRARY_FLAG_IS_ARCHIVE);
      BLI_assert(lib->archive_parent_library == reference_lib);

      /* If there is already an archive library in the new set of Mains, but not a 'libmain' for it
       * yet, it is the first time that this archive library is effectively used to own a packed
       * ID. Since regular libraries have their list of owned archive libs cleared when reused on
       * undo, it means that this archive library should yet be listed in its regular owner one,
       * and needs to be added there. See also #read_undo_move_libmain_data. */
      BLI_assert(!reference_lib->runtime->archived_libraries.contains(lib));
      reference_lib->runtime->archived_libraries.append(lib);

      BLI_assert(lib->runtime->filedata == nullptr);
      lib->runtime->filedata = fd;
      lib->runtime->is_filedata_owner = false;
    }
    else {
      /* Should never happen currently. */
      BLI_assert_unreachable();
    }
  }

  bmain->curlib = lib;
  return bmain;
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
  Main *new_bmain = fd->bmain;
  Main *old_bmain = fd->old_bmain;

  BLI_assert(old_bmain->curlib == nullptr);
  BLI_assert(old_bmain->split_mains);

  MainListsArray lbarray = BKE_main_lists_get(*old_bmain);
  int i = lbarray.size();
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

    ListBase *new_lb = which_libbase(new_bmain, id_type->id_code);
    BLI_assert(BLI_listbase_is_empty(new_lb));
    BLI_movelisttolist(new_lb, lbarray[i]);

    /* Update mappings accordingly. */
    LISTBASE_FOREACH (ID *, id_iter, new_lb) {
      BKE_main_idmap_insert_id(fd->new_idmap_uid, id_iter);
      id_iter->tag |= ID_TAG_UNDO_OLD_ID_REUSED_NOUNDO;
    }
  }
}

static void read_undo_move_libmain_data(FileData *fd, Main *libmain, BHead *bhead)
{
  Main *old_main = fd->old_bmain;
  Main *new_main = fd->bmain;
  Library *curlib = libmain->curlib;

  /* NOTE: This may change the order of items in `old_main->split_mains`. So calling code cannot
   * directly iterate over it. */
  old_main->split_mains->remove_contained(libmain);
  BLI_remlink_safe(&old_main->libraries, curlib);
  new_main->split_mains->add_new(libmain);
  BLI_addtail(&new_main->libraries, curlib);

  /* Remove all references to the archive libraries owned by this 'regular' library. The
   * archive ones are only moved over into the new Main if some of their IDs are actually
   * re-used. Otherwise they are deleted, so the 'regular' library cannot keep references to
   * them at this point. See also #blo_add_main_for_library. */
  curlib->runtime->archived_libraries = {};

  curlib->id.tag |= ID_TAG_UNDO_OLD_ID_REUSED_NOUNDO;
  BKE_main_idmap_insert_id(fd->new_idmap_uid, &curlib->id);
  if (bhead != nullptr) {
    oldnewmap_lib_insert(fd, bhead->old, &curlib->id, GS(curlib->id.name));
  }

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (libmain, id_iter) {
    /* Packed IDs are read from the memfile, so don't add them here already. */
    if (!ID_IS_PACKED(id_iter)) {
      BKE_main_idmap_insert_id(fd->new_idmap_uid, id_iter);
    }
  }
  FOREACH_MAIN_ID_END;
}

/* For undo, restore matching library datablock from the old main. */
static bool read_libblock_undo_restore_library(FileData *fd,
                                               const ID *id,
                                               ID *id_old,
                                               BHead *bhead)
{
  /* In undo case, most libraries and linked data should be kept as is from previous state
   * (see BLO_read_from_memfile).
   * However, some needed by the snapshot being read may have been removed in previous one,
   * and would go missing.
   * This leads e.g. to disappearing objects in some undo/redo case, see #34446.
   * That means we have to carefully check whether current lib or
   * libdata already exits in old main, if it does we merely copy it over into new main area,
   * otherwise we have to do a full read of that bhead... */
  CLOG_DEBUG(&LOG_UNDO, "UNDO: restore library %s", id->name);

  if (id_old == nullptr) {
    CLOG_DEBUG(&LOG_UNDO, "    -> NO match");
    return false;
  }

  /* Skip `oldmain` itself. */
  /* NOTE: Only one item is removed from `old_main->split_mains`, so it is safe to iterate directly
   * on it here. The fact that the order of the other mains contained in this split_mains may be
   * modified should not be an issue currently. */
  for (Main *libmain : fd->old_bmain->split_mains->as_span().drop_front(1)) {
    if (&libmain->curlib->id == id_old) {
      BLI_assert(libmain->curlib);
      BLI_assert((libmain->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0);
      CLOG_DEBUG(&LOG_UNDO,
                 "    compare with %s -> match (existing libpath: %s)",
                 libmain->curlib->id.name,
                 libmain->curlib->runtime->filepath_abs);

      /* In case of a library, we need to re-add its main to fd->bmain->split_mains,
       * because if we have later a missing ID_LINK_PLACEHOLDER,
       * we need to get the correct lib it is linked to!
       * Order is crucial, we cannot bulk-add it in BLO_read_from_memfile()
       * like it used to be. */
      read_undo_move_libmain_data(fd, libmain, bhead);
      BLI_assert(fd->old_bmain->split_mains);
      BLI_assert(fd->old_bmain->split_mains->size() >= 1);
      BLI_assert((*fd->old_bmain->split_mains)[0] == fd->old_bmain);
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
  CLOG_DEBUG(&LOG_UNDO, "UNDO: restore linked datablock %s", id->name);

  if (*r_id_old == nullptr) {
    /* If the linked ID had to be re-read at some point, its session_uid may not be the same as
     * its reference stored in the memfile anymore. Do a search by name then. */
    *r_id_old = library_id_is_yet_read(fd, libmain, bhead);

    if (*r_id_old == nullptr) {
      CLOG_DEBUG(&LOG_UNDO,
                 "    from %s (%s): NOT found",
                 libmain->curlib ? libmain->curlib->id.name : "<nullptr>",
                 libmain->curlib ? libmain->curlib->filepath : "<nullptr>");
      return false;
    }

    CLOG_DEBUG(&LOG_UNDO,
               "    from %s (%s): found by name",
               libmain->curlib ? libmain->curlib->id.name : "<nullptr>",
               libmain->curlib ? libmain->curlib->filepath : "<nullptr>");
    /* The Library ID 'owning' this linked ID should already have been moved to new main by a call
     * to #read_libblock_undo_restore_library. */
    BLI_assert(*r_id_old == static_cast<ID *>(BKE_main_idmap_lookup_uid(
                                fd->new_idmap_uid, (*r_id_old)->session_uid)));
  }
  else {
    CLOG_DEBUG(&LOG_UNDO,
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

  /* Do not add ID_TAG_NEW here, this should not be needed/used in undo case anyway (as
   * this is only for do_version-like code), but for sake of consistency, and also because
   * it will tell us which ID is re-used from old Main, and which one is actually newly read. */
  /* Also do not set #ID_Readfile_Data::Tags.needs_linking, this ID will never be re-liblinked,
   * hence that tag will never be cleared, leading to critical issue in link/append code. */
  /* Some tags need to be preserved here. */
  id_old->tag = ((id_tag | ID_TAG_UNDO_OLD_ID_REUSED_UNCHANGED) & ~ID_TAG_KEEP_ON_UNDO) |
                (id_old->tag & ID_TAG_KEEP_ON_UNDO);
  id_old->lib = main->curlib;
  id_old->us = ID_FAKE_USERS(id_old);
  /* Do not reset id->icon_id here, memory allocated for it remains valid. */
  /* Needed because .blend may have been saved with crap value here... */
  id_old->newid = nullptr;
  id_old->orig_id = nullptr;

  const short idcode = GS(id_old->name);
  Main *old_bmain = fd->old_bmain;
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
  if (GS(id_old->name) == ID_LI) {
    Library *lib = reinterpret_cast<Library *>(id_old);
    if (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
      BLI_assert(lib->runtime->filedata == nullptr);
      BLI_assert(lib->archive_parent_library);
      /* The 'normal' parent of this archive library should already have been moved into the new
       * Main. */
      BLI_assert(BKE_main_idmap_lookup_uid(fd->new_idmap_uid,
                                           lib->archive_parent_library->id.session_uid) ==
                 &lib->archive_parent_library->id);
      /* The archive library ID has been moved in the new Main, but not its own old split main, as
       * these packed IDs should be handled like local ones in undo case. So a new split libmain
       * needs to be created to contain its packed IDs. */
      blo_add_main_for_library(
          fd, lib, lib->archive_parent_library, lib->filepath, lib->runtime->filepath_abs, true);
    }
    else {
      BLI_assert_unreachable();
    }
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

  Main *old_bmain = fd->old_bmain;
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
                        ID_REMAP_SKIP_UPDATE_TAGGING | ID_REMAP_SKIP_USER_REFCOUNT |
                        ID_REMAP_SKIP_USER_CLEAR));

  /* Special temporary usage of this pointer, necessary for the `undo_preserve` call after
   * lib-linking to restore some data that should never be affected by undo, e.g. the 3D cursor of
   * #Scene. */
  id_old->orig_id = id;
  id_old->tag |= ID_TAG_UNDO_OLD_ID_REREAD_IN_PLACE;
  BLO_readfile_id_runtime_tags_for_write(*id_old).needs_linking = true;

  BLI_addtail(new_lb, id_old);
  BLI_addtail(old_lb, id);

  /* In case a library has been re-read, it has added already its own split main to the new Main
   * (see #direct_link_library code).
   *
   * Since we are replacing it with the 'id_old' address, we need to update that Main::curlib
   * pointer accordingly.
   *
   * Note that:
   *   - This code is only for undo, and on undo we do not re-read regular libraries, only archive
   *     ones for packed data.
   *   - The new split main should still be empty at this stage (this code and adding the split
   *     Main in #direct_link_library are part of the same #read_libblock call).
   */
  if (GS(id_old->name) == ID_LI) {
    Library *lib_old = blender::id_cast<Library *>(id_old);
    Library *lib = blender::id_cast<Library *>(id);
    BLI_assert(lib_old->flag & LIBRARY_FLAG_IS_ARCHIVE);

    for (Main *bmain_iter : *fd->bmain->split_mains) {
      if (bmain_iter->curlib == lib) {
        BLI_assert(BKE_main_is_empty(bmain_iter));
        bmain_iter->curlib = lib_old;
      }
    }
  }
}

static bool read_libblock_undo_restore(
    FileData *fd, Main *main, BHead *bhead, const int id_tag, ID **r_id_old)
{
  BLI_assert(fd->old_idmap_uid != nullptr);

  /* Get pointer to memory of new ID that we will be reading. */
  const ID *id = static_cast<const ID *>(peek_struct_undo(fd, bhead));
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);

  const bool do_partial_undo = (fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0;
#ifndef NDEBUG
  if (do_partial_undo && (bhead->code != ID_LINK_PLACEHOLDER) &&
      (blo_bhead_id_flag(fd, bhead) & ID_FLAG_LINKED_AND_PACKED) == 0)
  {
    /* This code should only ever be reached for local or packed data-blocks. */
    BLI_assert(main->curlib == nullptr);
  }
#endif

  /* Find the 'current' existing ID we want to reuse instead of the one we
   * would read from the undo memfile. */
  ID *id_old = (fd->old_idmap_uid != nullptr) ?
                   BKE_main_idmap_lookup_uid(fd->old_idmap_uid, id->session_uid) :
                   nullptr;

  if (bhead->code == ID_LI) {
    /* Restore library datablock, if possible.
     *
     * Never handle archive libraries and their packed IDs as normal ones. These are local data,
     * and need to be fully handled like local IDs. */
    if (id_old && (reinterpret_cast<Library *>(id_old)->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0 &&
        read_libblock_undo_restore_library(fd, id, id_old, bhead))
    {
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
    CLOG_DEBUG(
        &LOG_UNDO, "UNDO: skip restore datablock %s, 'NO_MEMFILE_UNDO' type of ID", id->name);

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
    CLOG_DEBUG(&LOG_UNDO,
               "UNDO: read %s (uid %u) -> no partial undo, always read at new address",
               id->name,
               id->session_uid);
    return false;
  }

  /* Restore local datablocks. */
  if (id_old != nullptr && read_libblock_is_identical(fd, bhead)) {
    /* Local datablock was unchanged, restore from the old main. */
    CLOG_DEBUG(&LOG_UNDO,
               "UNDO: read %s (uid %u) -> keep identical data-block",
               id->name,
               id->session_uid);

    read_libblock_undo_restore_identical(fd, main, id, id_old, bhead, id_tag);

    *r_id_old = id_old;
    return true;
  }
  if (id_old != nullptr) {
    /* Local datablock was changed. Restore at the address of the old datablock. */
    CLOG_DEBUG(&LOG_UNDO,
               "UNDO: read %s (uid %u) -> read to old existing address",
               id->name,
               id->session_uid);
    *r_id_old = id_old;
    return false;
  }

  /* Local datablock does not exist in the undo step, so read from scratch. */
  CLOG_DEBUG(
      &LOG_UNDO, "UNDO: read %s (uid %u) -> read at new address", id->name, id->session_uid);
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
                            ID_Readfile_Data::Tags id_read_tags,
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
  const int id_type_index = BKE_idtype_idcode_to_index(bhead->code);
#ifndef NDEBUG
  const char *blockname = nullptr;
#else
  /* Avoid looking up in the mapping for all read BHead, since this only contains the ID type name
   * in release builds. */
  const char *blockname = get_alloc_name(fd, bhead, nullptr, id_type_index);
#endif
  ID *id = read_id_struct(fd, bhead, blockname, id_type_index);
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
  id_tag |= ID_TAG_NEW;
  id_read_tags.needs_linking = true;

  if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Read placeholder for linked datablock. */
    id_read_tags.is_link_placeholder = true;

    if (placeholder_set_indirect_extern) {
      if (id->flag & ID_FLAG_INDIRECT_WEAK_LINK) {
        id_tag |= ID_TAG_INDIRECT;
      }
      else {
        id_tag |= ID_TAG_EXTERN;
      }
    }

    direct_link_id(fd, main, id_tag, id_read_tags, id, id_old);

    if (main->id_map != nullptr) {
      BKE_main_idmap_insert_id(main->id_map, id);
    }

    return blo_bhead_next(fd, bhead);
  }

  /* Read datablock contents.
   * Use convenient malloc name for debugging and better memory link prints. */
  bhead = read_data_into_datamap(fd, bhead, blockname, id_type_index);
  const bool success = direct_link_id(fd, main, id_tag, id_read_tags, id, id_old);
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
    if (ID_IS_PACKED(id)) {
      BLI_assert(id->deep_hash != IDHash::get_null());
      fd->id_by_deep_hash->add_new(id->deep_hash, id);
      BLI_assert(main->curlib);
    }
    if (fd->file_stat) {
      id->runtime->src_blend_modifification_time = fd->file_stat->st_mtime;
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

  bhead = read_data_into_datamap(fd, bhead, "Data for Asset meta-data", INDEX_ID_NULL);

  BlendDataReader reader = {fd};
  BLO_read_struct(&reader, AssetMetaData, r_asset_data);
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
  FileGlobal *fg = static_cast<FileGlobal *>(
      read_struct(fd, bhead, "Data from Global block", INDEX_ID_NULL));

  /* NOTE: `bfd->main->versionfile` is supposed to have already been set from `fd->fileversion`
   * beforehand by calling code. */
  bfd->main->subversionfile = fg->subversion;
  bfd->main->has_forward_compatibility_issues = !MAIN_VERSION_FILE_OLDER_OR_EQUAL(
      bfd->main, BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);

  bfd->main->minversionfile = fg->minversion;
  bfd->main->minsubversionfile = fg->minsubversion;

  bfd->main->build_commit_timestamp = fg->build_commit_timestamp;
  STRNCPY(bfd->main->build_hash, fg->build_hash);
  bfd->main->is_asset_edit_file = (fg->fileflags & G_FILE_ASSET_EDIT_FILE) != 0;

  STRNCPY(bfd->main->colorspace.scene_linear_name, fg->colorspace_scene_linear_name);
  bfd->main->colorspace.scene_linear_to_xyz = blender::float3x3(
      fg->colorspace_scene_linear_to_xyz);

  bfd->fileflags = fg->fileflags;
  bfd->globalf = fg->globalf;

  /* NOTE: since 88b24bc6bb, `fg->filepath` is only written for crash recovery and autosave files,
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
  if (bfd->curscene == nullptr) {
    bfd->curscene = static_cast<Scene *>(bfd->main->scenes.first);
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

  /* Code ensuring conversion to/from new 'system IDProperties'. This needs to run before any other
   * data versioning. Otherwise, things like Cycles versioning code cannot work as expected. */
  if (!MAIN_VERSION_FILE_ATLEAST(main, 500, 27)) {
    /* Generate System IDProperties by copying the whole 'user-defined' historic IDProps into new
     * system-defined-only storage. While not optimal (as it also duplicates actual user-defined
     * IDProperties), this seems to be the only safe and sound way to handle the migration. */
    version_system_idprops_generate(main);
  }
  if (!MAIN_VERSION_FILE_ATLEAST(main, 500, 70)) {
    /* Same as above, but decision to keep user-defined (aka custom properties) in nodes was taken
     * later during 5.0 development process. */
    version_system_idprops_nodes_generate(main);
  }
  if (!MAIN_VERSION_FILE_ATLEAST(main, 500, 110)) {
    /* Same as above, but children bones were missed by initial versioning code, attempt to
     * transfer idprops data still in case they have no system properties defined yet. */
    version_system_idprops_children_bones_generate(main);
  }

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

    CLOG_INFO(&LOG, "Read file %s", fd->relabase);
    CLOG_INFO(&LOG,
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
  if (!main->is_read_invalid) {
    blo_do_versions_410(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_420(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_430(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_440(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_450(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_500(fd, lib, main);
  }
  if (!main->is_read_invalid) {
    blo_do_versions_510(fd, lib, main);
  }

  /* WATCH IT!!!: pointers from libdata have not been converted yet here! */
  /* WATCH IT 2!: #UserDef struct init see #do_versions_userdef() above! */

  /* don't forget to set version number in BKE_blender_version.h! */

  main->is_locked_for_linking = false;
}

static void do_versions_after_linking(FileData *fd, Main *main)
{
  BLI_assert(fd != nullptr);

  CLOG_DEBUG(&LOG,
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
  if (!main->is_read_invalid) {
    do_versions_after_linking_410(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_420(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_430(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_440(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_450(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_500(fd, main);
  }
  if (!main->is_read_invalid) {
    do_versions_after_linking_510(fd, main);
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

    if ((id->tag & (ID_TAG_UNDO_OLD_ID_REUSED_UNCHANGED | ID_TAG_UNDO_OLD_ID_REUSED_NOUNDO)) != 0)
    {
      BLI_assert(fd->flags & FD_FLAGS_IS_MEMFILE);
      /* This ID has been re-used from 'old' bmain. Since it was therefore unchanged across
       * current undo step, and old IDs re-use their old memory address, we do not need to liblink
       * it at all. */
      BLI_assert(!BLO_readfile_id_runtime_tags(*id).needs_linking);

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

    if (BLO_readfile_id_runtime_tags(*id).needs_linking) {
      /* Not all original pointer values can be considered as valid.
       * Handling of DNA deprecated data should never be needed in undo case. */
      const LibraryForeachIDFlag flag = IDWALK_NO_ORIG_POINTERS_ACCESS | IDWALK_INCLUDE_UI |
                                        ((fd->flags & FD_FLAGS_IS_MEMFILE) ?
                                             IDWALK_NOP :
                                             IDWALK_DO_DEPRECATED_POINTERS);
      BKE_library_foreach_ID_link(bmain, id, lib_link_cb, &reader, flag);

      after_liblink_id_process(&reader, id);

      BLO_readfile_id_runtime_tags_for_write(*id).needs_linking = false;
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
    BLI_assert(!BLO_readfile_id_runtime_tags(*id).needs_linking);
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
  BLI_assert(!bmain->split_mains);

  if (!BKE_main_namemap_validate_and_fix(*bmain)) {
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

  BLO_main_validate_embedded_flag(bmain, reports ? reports->reports : nullptr);
  BLO_main_validate_embedded_liboverrides(bmain, reports ? reports->reports : nullptr);

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
  BLO_read_struct(reader, IDProperty, &kmi->properties);
  IDP_BlendDataRead(reader, &kmi->properties);
  kmi->ptr = nullptr;
  kmi->flag &= ~KMI_UPDATE;
}

static BHead *read_userdef(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
  UserDef *user;
  bfd->user = user = static_cast<UserDef *>(
      read_struct(fd, bhead, "Data for User Def", INDEX_ID_NULL));

  /* User struct has separate do-version handling */
  user->versionfile = bfd->main->versionfile;
  user->subversionfile = bfd->main->subversionfile;

  /* read all data into fd->datamap */
  bhead = read_data_into_datamap(fd, bhead, "Data for User Def", INDEX_ID_NULL);

  BlendDataReader reader_ = {fd};
  BlendDataReader *reader = &reader_;

  BLO_read_struct_list(reader, bTheme, &user->themes);
  BLO_read_struct_list(reader, wmKeyMap, &user->user_keymaps);
  BLO_read_struct_list(reader, wmKeyConfigPref, &user->user_keyconfig_prefs);
  BLO_read_struct_list(reader, bUserMenu, &user->user_menus);
  BLO_read_struct_list(reader, bAddon, &user->addons);
  BLO_read_struct_list(reader, bPathCompare, &user->autoexec_paths);
  BLO_read_struct_list(reader, bUserScriptDirectory, &user->script_directories);
  BLO_read_struct_list(reader, bUserAssetLibrary, &user->asset_libraries);
  BLO_read_struct_list(reader, bUserExtensionRepo, &user->extension_repos);
  BLO_read_struct_list(reader, bUserAssetShelfSettings, &user->asset_shelves_settings);

  LISTBASE_FOREACH (wmKeyMap *, keymap, &user->user_keymaps) {
    keymap->modal_items = nullptr;
    keymap->poll = nullptr;
    keymap->flag &= ~KEYMAP_UPDATE;

    BLO_read_struct_list(reader, wmKeyMapDiffItem, &keymap->diff_items);
    BLO_read_struct_list(reader, wmKeyMapItem, &keymap->items);

    LISTBASE_FOREACH (wmKeyMapDiffItem *, kmdi, &keymap->diff_items) {
      BLO_read_struct(reader, wmKeyMapItem, &kmdi->remove_item);
      BLO_read_struct(reader, wmKeyMapItem, &kmdi->add_item);

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
    BLO_read_struct(reader, IDProperty, &kpt->prop);
    IDP_BlendDataRead(reader, &kpt->prop);
  }

  LISTBASE_FOREACH (bUserMenu *, um, &user->user_menus) {
    BLO_read_struct_list(reader, bUserMenuItem, &um->items);
    LISTBASE_FOREACH (bUserMenuItem *, umi, &um->items) {
      if (umi->type == USER_MENU_TYPE_OPERATOR) {
        bUserMenuItem_Op *umi_op = (bUserMenuItem_Op *)umi;
        BLO_read_struct(reader, IDProperty, &umi_op->prop);
        IDP_BlendDataRead(reader, &umi_op->prop);
      }
    }
  }

  LISTBASE_FOREACH (bAddon *, addon, &user->addons) {
    BLO_read_struct(reader, IDProperty, &addon->prop);
    IDP_BlendDataRead(reader, &addon->prop);
  }

  LISTBASE_FOREACH (bUserExtensionRepo *, repo_ref, &user->extension_repos) {
    BKE_preferences_extension_repo_read_data(reader, repo_ref);
  }

  LISTBASE_FOREACH (bUserAssetShelfSettings *, shelf_settings, &user->asset_shelves_settings) {
    BKE_asset_catalog_path_list_blend_read_data(reader, shelf_settings->enabled_catalog_paths);
  }

  /* XXX */
  user->uifonts.first = user->uifonts.last = nullptr;

  BLO_read_struct_list(reader, uiStyle, &user->uistyles);

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
  Main *new_bmain = fd->bmain;
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (new_bmain, id_iter) {
    if (ID_IS_LINKED(id_iter)) {
      continue;
    }
    if ((id_iter->tag & ID_TAG_UNDO_OLD_ID_REUSED_NOUNDO) == 0) {
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
  BLI_assert(!bmain->split_mains);
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

  const bool is_undo = (fd->flags & FD_FLAGS_IS_MEMFILE) != 0;
  if (is_undo) {
    CLOG_DEBUG(&LOG_UNDO, "UNDO: read step");
  }

  /* Prevent any run of layer collections rebuild during readfile process, and the do_versions
   * calls.
   *
   * NOTE: Typically readfile code should not trigger such updates anyway. But some calls to
   * non-BLO functions (e.g. ID deletion) can indirectly trigger it. */
  BKE_layer_collection_resync_forbid();

  bfd = MEM_new<BlendFileData>(__func__);

  bfd->main = BKE_main_new();
  bfd->main->versionfile = fd->fileversion;
  STRNCPY(bfd->filepath, filepath);

  fd->bmain = bfd->main;
  fd->fd_bmain = bfd->main;

  bfd->type = BLENFILETYPE_BLEND;

  if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    blo_split_main(bfd->main);
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
    /* This idmap will store UIDs of all IDs ending up in the new main, whether they are newly
     * read, or re-used from the old main. */
    fd->new_idmap_uid = BKE_main_idmap_create(fd->bmain, false, nullptr, MAIN_IDMAP_TYPE_UID);

    /* Copy all 'no undo' local data from old to new bmain. */
    read_undo_reuse_noundo_local_ids(fd);
  }

  while (bhead) {
    /* If not-null after the `switch`, the BHead is an ID one and needs to be read. */
    Main *bmain_to_read_into = nullptr;
    bool placeholder_set_indirect_extern = false;

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
        if ((fd->skip_flags & BLO_READ_SKIP_DATA) != 0) {
          bhead = blo_bhead_next(fd, bhead);
          break;
        }
        /* Add link placeholder to the main of the library it belongs to.
         *
         * The library is the most recently loaded #ID_LI block, according to the file format
         * definition. So we can use the entry at the end of `fd->bmain->split_mains`, typically
         * the one last added in #direct_link_library. */
        bmain_to_read_into = (*fd->bmain->split_mains)[fd->bmain->split_mains->size() - 1];
        placeholder_set_indirect_extern = true;
        break;
      case ID_LI:
        if ((fd->skip_flags & BLO_READ_SKIP_DATA) != 0) {
          bhead = blo_bhead_next(fd, bhead);
          break;
        }
        /* Library IDs are always read into the first (aka 'local') Main, even if they are written
         * in 'library' blendfile-space (for archive libraries e.g.). */
        bmain_to_read_into = fd->bmain;
        break;
      case ID_SCRN:
        /* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
        bhead->code = ID_SCR;
        /* pass on to default */
        ATTR_FALLTHROUGH;
      default: {
        if ((fd->skip_flags & BLO_READ_SKIP_DATA) != 0 || !blo_bhead_is_id_valid_type(bhead)) {
          bhead = blo_bhead_next(fd, bhead);
          break;
        }
        /* Put read real ID into the main of the library it belongs to.
         *
         * Local IDs should all be written before any Library in the blendfile, so this code will
         * always select `fd->bmain` for these.
         *
         * Packed linked IDs are real ID data in the currently read blendfile (unlike placeholders
         * for regular linked data). But they are in their archive library 'name space' and
         * 'blendfile space', so this follows the same logic as for placeholders to select the
         * Main.
         *
         * The library is the most recently loaded #ID_LI block, according to the file format
         * definition. So we can use the entry at the end of `fd->bmain->split_mains`, typically
         * the one last added in #direct_link_library. */
        bmain_to_read_into = (*fd->bmain->split_mains)[fd->bmain->split_mains->size() - 1];
        BLI_assert_msg((bmain_to_read_into == fd->bmain ||
                        (blo_bhead_id_flag(fd, bhead) & ID_FLAG_LINKED_AND_PACKED) != 0),
                       "Local IDs should always be put in the first Main split data-base, not in "
                       "a 'linked data' one");
      }
    }
    if (bmain_to_read_into) {
      bhead = read_libblock(
          fd, bmain_to_read_into, bhead, 0, {}, placeholder_set_indirect_extern, nullptr);
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

    Main *old_main = fd->old_bmain;
    BLI_assert(old_main != nullptr);
    BLI_assert(old_main->curlib == nullptr);
    BLI_assert(old_main->split_mains);
    /* Cannot iterate directly over `old_main->split_mains`, as this is likely going to remove some
     * of its items. */
    blender::Vector<Main *> old_main_split_mains = {old_main->split_mains->as_span()};
    for (Main *libmain : old_main_split_mains.as_span().drop_front(1)) {
      BLI_assert(libmain->curlib);
      if (libmain->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
        /* Never move archived libraries and their content, these are 'local' data in undo context,
         * so all packed linked IDs should have been handled like local ones undo-wise, and if
         * packed libraries remain unused at this point, then they are indeed fully unused/removed
         * from the new main. */
        continue;
      }
      read_undo_move_libmain_data(fd, libmain, nullptr);
    }
  }

  /* Ensure fully valid and unique ID names before calling first stage of versioning. */
  if (!is_undo && (fd->flags & FD_FLAGS_HAS_INVALID_ID_NAMES) != 0) {
    long_id_names_ensure_unique_id_names(bfd->main);

    if (bfd->main->has_forward_compatibility_issues) {
      BKE_reportf(fd->reports->reports,
                  RPT_WARNING,
                  "Blendfile '%s' was created by a future version of Blender and contains ID "
                  "names longer than currently supported. These have been truncated.",
                  bfd->filepath);
    }
    else {
      BKE_reportf(fd->reports->reports,
                  RPT_ERROR,
                  "Blendfile '%s' appears corrupted, it contains invalid ID names. These have "
                  "been truncated.",
                  bfd->filepath);
    }

    /* This part is only to ensure forward compatibility with 5.0+ blend-files in 4.5.
     * It will be removed in 5.0. */
    long_id_names_process_action_slots_identifiers(bfd->main);
  }
  else {
    /* Getting invalid ID names from memfile undo data would be a critical error. */
    BLI_assert((fd->flags & FD_FLAGS_HAS_INVALID_ID_NAMES) == 0);
    if ((fd->flags & FD_FLAGS_HAS_INVALID_ID_NAMES) != 0) {
      bfd->main->is_read_invalid = true;
    }
  }

  if (bfd->main->is_read_invalid) {
    return bfd;
  }

  /* Do versioning before read_libraries, but skip in undo case. */
  if (!is_undo) {
    if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
      for (Main *bmain : *fd->bmain->split_mains) {
        /* Packed IDs are stored in the current .blend file, but belong to dedicated 'archive
         * library' Mains, not the first, 'local' Main. So they do need versioning here, as for
         * local IDs, which is why all the split Mains in the list need to be checked.
         *
         * Placeholders (of 'real' linked data) can't be versioned yet. Since they also belong to
         * dedicated 'library' Mains, and are not mixed with the 'packed' ones, these Mains can be
         * entirely skipped. */
        const bool contains_link_placeholder = (bmain->curlib != nullptr &&
                                                (bmain->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE) ==
                                                    0);
#ifndef NDEBUG
        MainListsArray lbarray = BKE_main_lists_get(*bmain);
        for (ListBase *lb_array : lbarray) {
          LISTBASE_FOREACH_MUTABLE (ID *, id, lb_array) {
            BLI_assert_msg((id->runtime->readfile_data->tags.is_link_placeholder ==
                            contains_link_placeholder),
                           contains_link_placeholder ?
                               "Real Library split Main contains non-placeholder IDs" :
                               (bmain->curlib == nullptr ?
                                    "Local data split Main contains placeholder IDs" :
                                    "Archive Library split Main contains placeholder IDs"));
          }
        }
#endif
        if (contains_link_placeholder) {
          continue;
        }
        do_versions(fd, bmain->curlib, bmain);
      }
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
    read_libraries(fd);
    BLI_assert((*bfd->main->split_mains)[0] == bfd->main);
    blo_join_main(bfd->main);

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
    if (!is_undo) {
      /* Note that we can't recompute user-counts at this point in undo case, we play too much with
       * IDs from different memory realms, and Main database is not in a fully valid state yet.
       */
      /* Some versioning code does expect some proper user-reference-counting, e.g. in conversion
       * from groups to collections... We could optimize out that first call when we are reading a
       * current version file, but again this is really not a bottle neck currently.
       * So not worth it. */
      BKE_main_id_refcount_recompute(bfd->main, false);

      /* Necessary to allow 2.80 layer collections conversion code to work. */
      BKE_layer_collection_resync_allow();

      /* Yep, second splitting... but this is a very cheap operation, so no big deal. */
      blo_split_main(bfd->main);
      for (Main *mainvar : *bfd->main->split_mains) {
        /* Do versioning for newly added linked data-blocks. If no data-blocks were read from a
         * library versionfile will still be zero and we can skip it. */
        if (mainvar->versionfile == 0) {
          continue;
        }
        do_versions_after_linking((mainvar->curlib && mainvar->curlib->runtime->filedata) ?
                                      mainvar->curlib->runtime->filedata :
                                      fd,
                                  mainvar);
        IMB_colormanagement_working_space_convert(mainvar, bfd->main);
      }
      blo_join_main(bfd->main);

      BKE_layer_collection_resync_forbid();

      /* And we have to compute those user-reference-counts again, as `do_versions_after_linking()`
       * does not always properly handle user counts, and/or that function does not take into
       * account old, deprecated data. */
      BKE_main_id_refcount_recompute(bfd->main, false);
    }

    LISTBASE_FOREACH_MUTABLE (Library *, lib, &bfd->main->libraries) {
      /* Now we can clear this runtime library filedata, it is not needed anymore.
       *
       * NOTE: This is also important to do for archive libraries. */
      library_filedata_release(lib);
      /* If no data-blocks were read from a library (should only happen when all references to a
       * library's data are `ID_FLAG_INDIRECT_WEAK_LINK`), its versionfile will still be zero and
       * it can be deleted.
       *
       * NOTES:
       *  - In case the library blendfile exists but is missing all the referenced linked IDs, the
       *    placeholders IDs created will reference the library ID, and the library ID will have a
       *    valid version number as the file was read to search for the linked IDs.
       *  - In case the library blendfile does not exist, its local Library ID will get the version
       *    of the current local Main (i.e. the loaded blendfile).
       *  - In case it is a reference library for archived ones, its runtime #archived_libraries
       *    vector will not be empty, and it must be kept, even if no data is directly linked from
       *    it anymore.
       */
      if (lib->runtime->versionfile == 0 && lib->runtime->archived_libraries.is_empty()) {
#ifndef NDEBUG
        ID *id_iter;
        FOREACH_MAIN_ID_BEGIN (bfd->main, id_iter) {
          BLI_assert(id_iter->lib != lib);
        }
        FOREACH_MAIN_ID_END;
#endif
        BKE_id_delete(bfd->main, lib);
      }
    }

    if (bfd->main->is_read_invalid) {
      return bfd;
    }

    /* After all data has been read and versioned, uses ID_TAG_NEW. Theoretically this should
     * not be calculated in the undo case, but it is currently needed even on undo to recalculate
     * a cache. */
    blender::bke::node_tree_update_all_new(*bfd->main);

    placeholders_ensure_valid(bfd->main);

    BKE_main_id_tag_all(bfd->main, ID_TAG_NEW, false);

    /* Must happen before applying liboverrides, as this process may fully invalidate e.g. view
     * layer pointers in case a Scene is a liboverride. */
    link_global(fd, bfd);

    /* Now that all our data-blocks are loaded,
     * we can re-generate overrides from their references. */
    if (!is_undo) {
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

      /* Update invariants after re-generating overrides. */
      BKE_main_ensure_invariants(*bfd->main);

      fd->reports->duration.lib_overrides = BLI_time_now_seconds() -
                                            fd->reports->duration.lib_overrides;
    }

    BKE_layer_collection_resync_allow();

    BKE_collections_after_lib_link(bfd->main);

    /* Make all relative paths, relative to the open blend file. */
    fix_relpaths_library(fd->relabase, bfd->main);
  }
  else {
    BKE_layer_collection_resync_allow();
  }

  BLI_assert(!bfd->main->split_mains);
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

  bhs = fd->bheadmap = MEM_malloc_arrayN<BHeadSort>(tot, "BHeadSort");

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
  char idname_full[MAX_ID_NAME];
  *((short *)idname_full) = idcode;
  BLI_strncpy(idname_full + 2, name, sizeof(idname_full) - 2);

  return fd->bhead_idname_map->lookup_default(idname_full, nullptr);
}

static BHead *find_bhead_from_idname(FileData *fd, const char *idname)
{
  BHead *bhead = fd->bhead_idname_map->lookup_default(idname, nullptr);
  if (LIKELY(bhead)) {
    return bhead;
  }

  /* Expected ID was not found, attempt to load the same name, but for an older, deprecated and
   * converted ID type. */
  const short id_code_old = do_versions_new_to_old_idcode_get(GS(idname));
  if (id_code_old == ID_LINK_PLACEHOLDER) {
    return bhead;
  }
  return find_bhead_from_code_name(fd, id_code_old, idname + 2);
}

static ID *library_id_is_yet_read_deep_hash(FileData *fd, BHead *bhead)
{
  if (const IDHash *deep_hash = blo_bhead_id_deep_hash(fd, bhead)) {
    if (ID *existing_id = fd->id_by_deep_hash->lookup_default(*deep_hash, nullptr)) {
      return existing_id;
    }
  }
  return nullptr;
}

static ID *library_id_is_yet_read_main(Main *mainvar, const char *idname)
{
  if (mainvar->id_map == nullptr) {
    mainvar->id_map = BKE_main_idmap_create(mainvar, false, nullptr, MAIN_IDMAP_TYPE_NAME);
  }
  BLI_assert(BKE_main_idmap_main_get(mainvar->id_map) == mainvar);

  ID *existing_id = BKE_main_idmap_lookup_name(
      mainvar->id_map, GS(idname), idname + 2, mainvar->curlib);
  BLI_assert(existing_id ==
             BLI_findstring(which_libbase(mainvar, GS(idname)), idname, offsetof(ID, name)));
  return existing_id;
}

static ID *library_id_is_yet_read(FileData *fd, Main *mainvar, BHead *bhead)
{
  if (ID *existing_id = library_id_is_yet_read_deep_hash(fd, bhead)) {
    return existing_id;
  }

  const char *idname = blo_bhead_id_name(fd, bhead);
  if (!idname) {
    return nullptr;
  }
  return library_id_is_yet_read_main(mainvar, idname);
}

static void read_libraries_report_invalid_id_names(FileData *fd,
                                                   ReportList *reports,
                                                   const bool has_forward_compatibility_issues,
                                                   const char *filepath)
{
  if (!fd || (fd->flags & FD_FLAGS_HAS_INVALID_ID_NAMES) == 0) {
    return;
  }
  if (has_forward_compatibility_issues) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Library '%s' was created by a future version of Blender and contains ID names "
                "longer than currently supported. This may cause missing linked data, consider "
                "opening and re-saving that library with the current Blender version.",
                filepath);
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Library '%s' appears corrupted, it contains invalid ID names. This may cause "
                "missing linked data.",
                filepath);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Linking (expand pointers)
 * \{ */

using BLOExpandDoitCallback = void (*)(void *fdhandle,
                                       std::queue<ID *> &ids_to_expand,
                                       Main *mainvar,
                                       void *idv);

struct BlendExpander {
  FileData *fd;
  std::queue<ID *> ids_to_expand;
  Main *main;
  BLOExpandDoitCallback callback;
};

/* Find the existing Main matching the given blendfile library filepath, or create a new one (with
 * the matching Library ID) if needed.
 *
 * NOTE: The process is a bit more complex for packed linked IDs and their archive libraries, as
 * in this case, this function also needs to find or create a new suitable archive library, i.e.
 * one which does not contain yet the given ID (from its name & type). */
static Main *blo_find_main_for_library_and_idname(FileData *fd,
                                                  const char *lib_filepath,
                                                  const char *relabase,
                                                  const BHead *id_bhead,
                                                  const char *id_name,
                                                  const bool is_packed_id)
{
  Library *parent_lib = nullptr;
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_abs, lib_filepath);
  BLI_path_abs(filepath_abs, relabase);
  BLI_path_normalize(filepath_abs);

  for (Main *main_it : *fd->bmain->split_mains) {
    const char *libname = (main_it->curlib) ? main_it->curlib->runtime->filepath_abs :
                                              main_it->filepath;

    if (BLI_path_cmp(filepath_abs, libname) == 0) {
      CLOG_DEBUG(&LOG,
                 "Found library '%s' for file path '%s'",
                 main_it->curlib ? main_it->curlib->id.name : "<None>",
                 lib_filepath);
      /* Due to how parent and archive libraries are created and written in the blend-file,
       * the first library matching a given filepath should never be an archive one. */
      BLI_assert(!main_it->curlib || (main_it->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0);
      if (!is_packed_id) {
        return main_it;
      }
      /* For packed IDs, the Main of the main owner library is not a valid one. Another loop is
       * needed into all the Mains matching the archive libraries of this main library. */
      BLI_assert(main_it->curlib);
      parent_lib = main_it->curlib;
      break;
    }
  }

  if (is_packed_id) {
    if (parent_lib) {
      /* Try to find an 'available' existing archive Main library, i.e. one that does not yet
       * contain an ID of the same type and name. */
      for (Main *main_it : *fd->bmain->split_mains) {
        if (!main_it->curlib || (main_it->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0 ||
            main_it->curlib->archive_parent_library != parent_lib)
        {
          continue;
        }
        if (ID *packed_id = library_id_is_yet_read_main(main_it, id_name)) {
          /* Archive Main library already contains a 'same' ID - but it should have a different
           * deep_hash. Otherwise, a previous call to `library_id_is_yet_read()` should have
           * returned this ID, and this code should not be reached. */
          BLI_assert(packed_id->deep_hash != *blo_bhead_id_deep_hash(fd, id_bhead));
          UNUSED_VARS_NDEBUG(packed_id, id_bhead);
          continue;
        }
        BLI_assert(ELEM(main_it->curlib->runtime->filedata, fd, nullptr));
        main_it->curlib->runtime->filedata = fd;
        main_it->curlib->runtime->is_filedata_owner = false;
        BLI_assert(main_it->versionfile != 0);
        CLOG_DEBUG(&LOG,
                   "Found archive library '%s' for the packed ID '%s'",
                   main_it->curlib->id.name,
                   id_name);
        return main_it;
      }
    }
    else {
      /* An archive library requires an existing parent library, create an empty, 'virtual' one if
       * needed. */
      Main *reference_bmain = blo_add_main_for_library(
          fd, nullptr, nullptr, lib_filepath, filepath_abs, false);
      parent_lib = reference_bmain->curlib;
      CLOG_DEBUG(&LOG,
                 "Added new parent library '%s' for file path '%s'",
                 parent_lib->id.name,
                 lib_filepath);
    }
  }
  BLI_assert(parent_lib || !is_packed_id);

  Main *bmain = blo_add_main_for_library(
      fd, nullptr, parent_lib, lib_filepath, filepath_abs, is_packed_id);

  read_file_version_and_colorspace(fd, bmain);

  if (is_packed_id) {
    CLOG_DEBUG(&LOG,
               "Added new archive library '%s' for the packed ID '%s'",
               bmain->curlib->id.name,
               id_name);
  }
  else {
    CLOG_DEBUG(
        &LOG, "Added new library '%s' for file path '%s'", bmain->curlib->id.name, lib_filepath);
  }
  return bmain;
}

/* Actually load an ID from a library. There are three possible cases here:
 *   - `existing_id` is non-null: calling code already found a suitable existing ID, this function
 *     essentially then only updates the mappings for `bhead->old` address to point to the given
 *     ID. This is the only case where `libmain` may be `nullptr`.
 *   - The given bhead has an already loaded matching ID (found by a call to
 *     `library_id_is_yet_read`), then once that ID is found behavior is as in the previous case.
 *   - No matching existing ID is found, then a new one is actually read from the given FileData.
 */
static void read_id_in_lib(FileData *fd,
                           std::queue<ID *> &ids_to_expand,
                           Main *libmain,
                           Library *parent_lib,
                           BHead *bhead,
                           ID *existing_id,
                           ID_Readfile_Data::Tags id_read_tags)
{
  ID *id = existing_id;

  if (id == nullptr) {
    BLI_assert(libmain);
    id = library_id_is_yet_read(fd, libmain, bhead);
  }
  if (id == nullptr) {
    /* ID has not been read yet, add placeholder to the main of the
     * library it belongs to, so that it will be read later. */
    read_libblock(
        fd, libmain, bhead, fd->id_tag_extra | ID_TAG_INDIRECT, id_read_tags, false, &id);
    BLI_assert(id != nullptr);
    id_sort_by_name(which_libbase(libmain, GS(id->name)), id, static_cast<ID *>(id->prev));

    /* commented because this can print way too much */
    // if (G.debug & G_DEBUG) printf("expand_doit: other lib %s\n", lib->filepath);

    /* For outliner dependency only. */
    if (parent_lib) {
      libmain->curlib->runtime->parent = parent_lib;
    }

    /* Only newly read ID needs to be added to the expand TODO queue, existing ones should already
     * be in it - or already have been expanded. */
    if (id_read_tags.needs_expanding) {
      ids_to_expand.push(id);
    }
  }
  else {
    /* Convert any previously read weak link to regular link to signal that we want to read this
     * data-block.
     *
     * Note that this function also visits already-loaded data-blocks, and thus their
     * `readfile_data` field might already have been freed. */
    if (BLO_readfile_id_runtime_tags(*id).is_link_placeholder) {
      id->flag &= ~ID_FLAG_INDIRECT_WEAK_LINK;
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
}

static void expand_doit_library(void *fdhandle,
                                std::queue<ID *> &ids_to_expand,
                                Main *mainvar,
                                void *old)
{
  FileData *fd = static_cast<FileData *>(fdhandle);

  if (mainvar->is_read_invalid) {
    return;
  }

  BHead *bhead = find_bhead(fd, old);
  if (bhead == nullptr) {
    return;
  }
  /* In 2.50+ file identifier for screens is patched, forward compatibility. */
  if (bhead->code == ID_SCRN) {
    bhead->code = ID_SCR;
  }
  if (!blo_bhead_is_id_valid_type(bhead)) {
    return;
  }
  const char *id_name = blo_bhead_id_name(fd, bhead);
  if (!id_name) {
    /* Do not allow linking ID which names are invalid (likely coming from a future version of
     * Blender allowing longer names). */
    return;
  }
  const bool is_packed_id = (blo_bhead_id_flag(fd, bhead) & ID_FLAG_LINKED_AND_PACKED) != 0;

  BLI_assert_msg(!is_packed_id || bhead->code != ID_LINK_PLACEHOLDER,
                 "A link placeholder ID (aka reference to some ID linked from another library) "
                 "should never be packed.");

  if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Placeholder link to data-block in another library. */
    BHead *bheadlib = find_previous_lib(fd, bhead);
    if (bheadlib == nullptr) {
      BLO_reportf_wrap(fd->reports,
                       RPT_ERROR,
                       RPT_("LIB: .blend file %s seems corrupted, no owner 'Library' data found "
                            "for the linked data-block '%s'. Try saving the file again."),
                       mainvar->curlib->runtime->filepath_abs,
                       id_name ? id_name : "<InvalidIDName>");
      return;
    }

    Library *lib = reinterpret_cast<Library *>(
        read_id_struct(fd, bheadlib, "Data for Library ID type", INDEX_ID_NULL));
    Main *libmain = blo_find_main_for_library_and_idname(
        fd, lib->filepath, fd->relabase, nullptr, nullptr, false);
    MEM_freeN(lib);

    if (libmain->curlib == nullptr) {
      BLO_reportf_wrap(fd->reports,
                       RPT_WARNING,
                       RPT_("LIB: Data refers to main .blend file: '%s' from %s"),
                       id_name ? id_name : "<InvalidIDName>",
                       mainvar->curlib->runtime->filepath_abs);
      return;
    }

    /* Placeholders never need expanding, as they are a mere reference to ID from another
     * library/blendfile. */
    read_id_in_lib(fd, ids_to_expand, libmain, mainvar->curlib, bhead, nullptr, {});
  }
  else if (is_packed_id) {
    /* Packed data-block from another library. */

    /* That exact same packed ID may have already been read before. */
    if (ID *existing_id = library_id_is_yet_read_deep_hash(fd, bhead)) {
      /* Ensure that the current BHead's `old` pointer will also be remapped to the found existing
       * ID. */
      read_id_in_lib(fd, ids_to_expand, nullptr, nullptr, bhead, existing_id, {});
      return;
    }

    BHead *bheadlib = find_previous_lib(fd, bhead);
    if (bheadlib == nullptr) {
      BLO_reportf_wrap(fd->reports,
                       RPT_ERROR,
                       RPT_("LIB: .blend file %s seems corrupted, no owner 'Library' data found "
                            "for the packed linked data-block %s. Try saving the file again."),
                       mainvar->curlib->runtime->filepath_abs,
                       id_name ? id_name : "<InvalidIDName>");
      return;
    }

    Library *lib = reinterpret_cast<Library *>(
        read_id_struct(fd, bheadlib, "Data for Library ID type", INDEX_ID_NULL));
    Main *libmain = blo_find_main_for_library_and_idname(
        fd, lib->filepath, fd->relabase, bhead, id_name, is_packed_id);
    MEM_freeN(lib);

    if (libmain->curlib == nullptr) {
      BLO_reportf_wrap(fd->reports,
                       RPT_WARNING,
                       RPT_("LIB: Data refers to main .blend file: '%s' from %s"),
                       id_name ? id_name : "<InvalidIDName>",
                       mainvar->curlib->runtime->filepath_abs);
      return;
    }

    ID_Readfile_Data::Tags id_read_tags{};
    id_read_tags.needs_expanding = true;
    read_id_in_lib(fd, ids_to_expand, libmain, nullptr, bhead, nullptr, id_read_tags);
  }
  else {
    /* Data-block in same library. */
    ID_Readfile_Data::Tags id_read_tags{};
    id_read_tags.needs_expanding = true;
    read_id_in_lib(fd, ids_to_expand, mainvar, nullptr, bhead, nullptr, id_read_tags);
  }
}

static int expand_cb(LibraryIDLinkCallbackData *cb_data)
{
  /* Embedded IDs are not known by lib_link code, so they would be remapped to `nullptr`. But there
   * is no need to process them anyway, as they are already handled during the 'read_data' phase.
   */
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    return IDWALK_RET_NOP;
  }

  /* Do not expand weak links. These are used when the user interface links to scene data,
   * but we don't want to bring along such datablocks with a workspace. */
  if (cb_data->cb_flag & IDWALK_CB_DIRECT_WEAK_LINK) {
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

  if (id) {
    expander->callback(expander->fd, expander->ids_to_expand, expander->main, id);
  }

  return IDWALK_RET_NOP;
}

static void expand_main(void *fdhandle, Main *mainvar, BLOExpandDoitCallback callback)
{
  FileData *fd = static_cast<FileData *>(fdhandle);
  BlendExpander expander = {fd, {}, mainvar, callback};

  /* Note: Packed IDs are the only current case where IDs read/loaded from a library blendfile will
   * end up in another Main (outside of placeholders, which never need to be expanded). This is not
   * a problem for initialization of the 'to be expanded' queue though, as no packed ID can be
   * directly linked currently, they are only brough in indirectly, i.e. during the expansion
   * process itself.
   *
   * So just looping on the 'main'/root Main of the read library is fine here currently. */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (mainvar, id_iter) {
    if (BLO_readfile_id_runtime_tags(*id_iter).needs_expanding) {
      expander.ids_to_expand.push(id_iter);
    }
  }
  FOREACH_MAIN_ID_END;

  while (!expander.ids_to_expand.empty()) {
    id_iter = expander.ids_to_expand.front();
    expander.ids_to_expand.pop();
    BLI_assert(BLO_readfile_id_runtime_tags(*id_iter).needs_expanding);

    /* Original (current) ID pointer can be considered as valid, but _not_ its own pointers to
     * other IDs - the already loaded ones will be valid, but the yet-to-be-read ones will not.
     * Expanding should _not_ require processing of UI ID pointers.
     * Expanding should never modify ID pointers themselves.
     * Handling of DNA deprecated data should never be needed in undo case. */
    const LibraryForeachIDFlag flag = IDWALK_READONLY | IDWALK_NO_ORIG_POINTERS_ACCESS |
                                      ((!fd || (fd->flags & FD_FLAGS_IS_MEMFILE)) ?
                                           IDWALK_NOP :
                                           IDWALK_DO_DEPRECATED_POINTERS);
    BKE_library_foreach_ID_link(nullptr, id_iter, expand_cb, &expander, flag);

    BLO_readfile_id_runtime_tags_for_write(*id_iter).needs_expanding = false;
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

  if (bhead && blo_bhead_is_id_valid_type(bhead)) {
    id = library_id_is_yet_read(fd, mainl, bhead);
    if (id == nullptr) {
      /* not read yet */
      const int tag = ((force_indirect ? ID_TAG_INDIRECT : ID_TAG_EXTERN) | fd->id_tag_extra);
      ID_Readfile_Data::Tags id_read_tags{};
      id_read_tags.needs_expanding = true;
      read_libblock(fd, mainl, bhead, tag, id_read_tags, false, &id);

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
      if (!force_indirect && (id->tag & ID_TAG_INDIRECT)) {
        id->tag &= ~ID_TAG_INDIRECT;
        id->flag &= ~ID_FLAG_INDIRECT_WEAK_LINK;
        id->tag |= ID_TAG_EXTERN;
      }
    }
  }
  else if (use_placeholders) {
    /* XXX flag part is weak! */
    id = create_placeholder(
        mainl, idcode, name, force_indirect ? ID_TAG_INDIRECT : ID_TAG_EXTERN, false);
  }
  else {
    id = nullptr;
  }

  /* NOTE: `id` may be `nullptr` even if a BHead was found, in case e.g. it is an invalid BHead. */

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
  BLI_assert((id_tag_extra & ~ID_TAG_TEMP_MAIN) == 0);

  fd->id_tag_extra = id_tag_extra;

  fd->bmain = mainvar;

  /* Add already existing packed data-blocks to map so that they are not loaded again. */
  ID *id;
  FOREACH_MAIN_ID_BEGIN (mainvar, id) {
    if (ID_IS_PACKED(id)) {
      fd->id_by_deep_hash->add(id->deep_hash, id);
    }
  }
  FOREACH_MAIN_ID_END;

  /* make mains */
  blo_split_main(mainvar);

  /* Find or create a Main matching the current library filepath. */
  /* Note: Directly linking packed IDs is not supported currently. */
  mainl = blo_find_main_for_library_and_idname(
      fd, filepath, BKE_main_blendfile_path(mainvar), nullptr, nullptr, false);
  fd->fd_bmain = mainl;
  if (mainl->curlib) {
    mainl->curlib->runtime->filedata = fd;
    /* This filedata is owned and managed by the calling code. */
    mainl->curlib->runtime->is_filedata_owner = false;
  }

  /* needed for do_version */
  mainl->versionfile = short(fd->fileversion);
  read_file_version_and_colorspace(fd, mainl);
  read_file_bhead_idname_map_create(fd);

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
  main_newid->colorspace = mainptr->colorspace;

  MainListsArray lbarray = BKE_main_lists_get(*mainptr);
  MainListsArray lbarray_newid = BKE_main_lists_get(*main_newid);
  int i = lbarray.size();
  while (i--) {
    BLI_listbase_clear(lbarray_newid[i]);

    LISTBASE_FOREACH_MUTABLE (ID *, id, lbarray[i]) {
      if (id->tag & ID_TAG_NEW) {
        BLI_remlink(lbarray[i], id);
        BLI_addtail(lbarray_newid[i], id);
      }
    }
  }
}

static void library_link_end(Main *mainl, FileData **fd, const int flag, ReportList *reports)
{
  Main *mainvar = (*fd)->bmain;
  Library *curlib;

  if (mainl->id_map == nullptr) {
    mainl->id_map = BKE_main_idmap_create(mainl, false, nullptr, MAIN_IDMAP_TYPE_NAME);
  }

  /* make main consistent */
  expand_main(*fd, mainl, expand_doit_library);

  read_libraries_report_invalid_id_names(
      *fd, reports, mainl->has_forward_compatibility_issues, mainl->curlib->runtime->filepath_abs);

  /* Do this when expand found other libraries. */
  read_libraries(*fd);

  curlib = mainl->curlib;

  /* make the lib path relative if required */
  if (flag & FILE_RELPATH) {
    /* use the full path, this could have been read by other library even */
    STRNCPY(curlib->filepath, curlib->runtime->filepath_abs);

    /* uses current .blend file as reference */
    BLI_path_rel(curlib->filepath, BKE_main_blendfile_path_from_global());
  }

  blo_join_main(mainvar);
  mainl = nullptr; /* blo_join_main free's mainl, can't use anymore */

  if (mainvar->is_read_invalid) {
    return;
  }

  lib_link_all(*fd, mainvar);
  if ((flag & BLO_LIBLINK_COLLECTION_NO_HIERARCHY_REBUILD) == 0) {
    after_liblink_merged_bmain_process(mainvar, (*fd)->reports);
  }

  /* Some versioning code does expect some proper userrefcounting, e.g. in conversion from
   * groups to collections... We could optimize out that first call when we are reading a
   * current version file, but again this is really not a bottle neck currently. So not worth
   * it. */
  BKE_main_id_refcount_recompute(mainvar, false);

  /* FIXME: This is suspiciously early call compared to similar process in
   * #blo_read_file_internal, where it is called towards the very end, after all do_version,
   * liboverride updates etc. have been done. */
  /* FIXME: Probably also need to forbid layer collections updates until this call, as done in
   * #blo_read_file_internal? */
  BKE_collections_after_lib_link(mainvar);

  /* Yep, second splitting... but this is a very cheap operation, so no big deal. */
  blo_split_main(mainvar);
  Main *main_newid = BKE_main_new();
  for (Main *mainlib : mainvar->split_mains->as_span().drop_front(1)) {

    BLI_assert(mainlib->versionfile != 0 || BKE_main_is_empty(mainlib));
    /* We need to split out IDs already existing,
     * or they will go again through do_versions - bad, very bad! */
    split_main_newid(mainlib, main_newid);

    do_versions_after_linking((main_newid->curlib && main_newid->curlib->runtime->filedata) ?
                                  main_newid->curlib->runtime->filedata :
                                  *fd,
                              main_newid);
    IMB_colormanagement_working_space_convert(main_newid, mainvar);

    add_main_to_main(mainlib, main_newid);

    if (mainlib->is_read_invalid) {
      break;
    }
  }

  blo_join_main(mainvar);

  if (mainvar->is_read_invalid) {
    BKE_main_free(main_newid);
    return;
  }

  /* This does not take into account old, deprecated data, so we also have to do it after
   * `do_versions_after_linking()`. */
  BKE_main_id_refcount_recompute(mainvar, false);

  /* After all data has been read and versioned, uses ID_TAG_NEW. */
  blender::bke::node_tree_update_all_new(*mainvar);

  placeholders_ensure_valid(mainvar);

  /* Apply overrides of newly linked data if needed. Already existing IDs need to split out, to
   * avoid re-applying their own overrides. */
  BLI_assert(BKE_main_is_empty(main_newid));
  split_main_newid(mainvar, main_newid);
  BKE_lib_override_library_main_validate(main_newid, (*fd)->reports->reports);
  BKE_lib_override_library_main_update(main_newid);
  add_main_to_main(mainvar, main_newid);
  BKE_main_free(main_newid);

  BKE_main_id_tag_all(mainvar, ID_TAG_NEW, false);

  /* FIXME Temporary 'fix' to a problem in how temp ID are copied in
   * `BKE_lib_override_library_main_update`, see #103062.
   * Proper fix involves first addressing #90610. */
  if ((flag & BLO_LIBLINK_COLLECTION_NO_HIERARCHY_REBUILD) == 0) {
    BKE_main_collections_parent_relations_rebuild(mainvar);
  }

  /* Make all relative paths, relative to the open blend file. */
  fix_relpaths_library(BKE_main_blendfile_path(mainvar), mainvar);

  /* patch to prevent switch_endian happens twice */
  /* FIXME This is extremely bad design, #library_link_end should probably _always_ free the file
   * data? */
  if ((*fd)->flags & FD_FLAGS_SWITCH_ENDIAN) {
    /* Big Endian blend-files are not supported for linking. */
    BLI_assert_unreachable();
    blo_filedata_free(*fd);
    *fd = nullptr;
  }

  /* Sanity checks. */
  blo_read_file_checks(mainvar);
}

void BLO_library_link_end(Main *mainl,
                          BlendHandle **bh,
                          const LibraryLink_Params *params,
                          ReportList *reports)
{
  FileData *fd = reinterpret_cast<FileData *>(*bh);

  if (!mainl->is_read_invalid) {
    library_link_end(mainl, &fd, params->flag, reports);
  }

  LISTBASE_FOREACH (Library *, lib, &params->bmain->libraries) {
    /* Now we can clear this runtime library filedata, it is not needed anymore. */
    /* TODO: In the future, could be worth keeping them in case data are linked from several
     * libraries at once? To avoid closing and re-opening the same file several times. Would need
     * a global cleanup callback then once all linking is done, though. */
    library_filedata_release(lib);
  }

  *bh = reinterpret_cast<BlendHandle *>(fd);
}

void *BLO_library_read_struct(FileData *fd, BHead *bh, const char *blockname)
{
  return read_struct(fd, bh, blockname, INDEX_ID_NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Reading
 * \{ */

static int has_linked_ids_to_read(Main *mainvar)
{
  MainListsArray lbarray = BKE_main_lists_get(*mainvar);
  int a = lbarray.size();
  while (a--) {
    LISTBASE_FOREACH (ID *, id, lbarray[a]) {
      if (BLO_readfile_id_runtime_tags(*id).is_link_placeholder &&
          !(id->flag & ID_FLAG_INDIRECT_WEAK_LINK))
      {
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
                        ((id->tag & ID_TAG_EXTERN) == 0);

  if (fd) {
    /* About future longer ID names: This is one of the main places that prevent linking IDs with
     * names longer than MAX_ID_NAME - 1.
     *
     * See also #read_file_bhead_idname_map_create. */
    bhead = find_bhead_from_idname(fd, id->name);
  }

  if (!is_valid) {
    BLO_reportf_wrap(basefd->reports,
                     RPT_ERROR,
                     RPT_("LIB: %s: '%s' is directly linked from '%s' (parent '%s'), but is a "
                          "non-linkable data type"),
                     BKE_idtype_idcode_to_name(GS(id->name)),
                     id->name + 2,
                     mainvar->curlib->runtime->filepath_abs,
                     library_parent_filepath(mainvar->curlib));
  }

  BLO_readfile_id_runtime_tags_for_write(*id).is_link_placeholder = false;
  id->flag &= ~ID_FLAG_INDIRECT_WEAK_LINK;

  if (bhead) {
    BLO_readfile_id_runtime_tags_for_write(*id).needs_expanding = true;
    // printf("read lib block %s\n", id->name);
    read_libblock(fd, mainvar, bhead, id->tag, BLO_readfile_id_runtime_tags(*id), false, r_id);
  }
  else {
    CLOG_DEBUG(&LOG,
               "LIB: %s: '%s' missing from '%s', parent '%s'",
               BKE_idtype_idcode_to_name(GS(id->name)),
               id->name + 2,
               mainvar->curlib->runtime->filepath_abs,
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

static void read_library_linked_ids(FileData *basefd, FileData *fd, Main *mainvar)
{
  blender::Map<std::string, ID *> loaded_ids;

  MainListsArray lbarray = BKE_main_lists_get(*mainvar);
  int a = lbarray.size();
  while (a--) {
    ID *id = static_cast<ID *>(lbarray[a]->first);

    while (id) {
      ID *id_next = static_cast<ID *>(id->next);
      if (BLO_readfile_id_runtime_tags(*id).is_link_placeholder &&
          !(id->flag & ID_FLAG_INDIRECT_WEAK_LINK))
      {
        BLI_remlink(lbarray[a], id);
        if (mainvar->id_map != nullptr) {
          BKE_main_idmap_remove_id(mainvar->id_map, id);
        }

        /* When playing with lib renaming and such, you may end with cases where
         * you have more than one linked ID of the same data-block from same
         * library. This is absolutely horrible, hence we use a ghash to ensure
         * we go back to a single linked data when loading the file. */
        ID *realid = loaded_ids.lookup_default(id->name, nullptr);
        if (!realid) {
          read_library_linked_id(basefd, fd, mainvar, id, &realid);
          loaded_ids.add_overwrite(id->name, realid);
        }

        /* `realid` shall never be nullptr - unless some source file/lib is broken
         * (known case: some directly linked shape-key from a missing lib...). */
        // BLI_assert(*realid != nullptr);

        /* Now that we have a real ID, replace all pointers to placeholders in
         * fd->libmap with pointers to the real data-blocks. We do this for all
         * libraries since multiple might be referencing this ID. */
        change_link_placeholder_to_real_ID_pointer(basefd, id, realid);

        /* Transfer the readfile data from the placeholder to the real ID, but
         * only if the real ID has no readfile data yet. The same realid may be
         * referred to by multiple placeholders. */
        if (realid && !realid->runtime->readfile_data) {
          realid->runtime->readfile_data = id->runtime->readfile_data;
          id->runtime->readfile_data = nullptr;
        }

        /* Ensure that the runtime pointer, and its 'readfile' sub-data, are properly freed, as
         * this ID placeholder does not go through versioning (the usual place where this data is
         * freed). Since `id` is not a real ID, this shouldn't follow any pointers to embedded IDs.
         *
         * WARNING! This placeholder ID is only an ID struct, with a very small subset of regular
         * ID common data actually valid and needing to be freed. Therefore, calling
         * #BKE_libblock_free_data on it would not work. */
        BKE_libblock_free_runtime_data(id);

        MEM_freeN(id);
      }
      id = id_next;
    }

    loaded_ids.clear();
  }

  read_libraries_report_invalid_id_names(fd,
                                         basefd->reports->reports,
                                         mainvar->has_forward_compatibility_issues,
                                         mainvar->curlib->runtime->filepath_abs);
}

static void read_library_clear_weak_links(FileData *basefd, Main *mainvar)
{
  /* Any remaining weak links at this point have been lost, silently drop
   * those by setting them to nullptr pointers. */
  MainListsArray lbarray = BKE_main_lists_get(*mainvar);
  int a = lbarray.size();
  while (a--) {
    ID *id = static_cast<ID *>(lbarray[a]->first);

    while (id) {
      ID *id_next = static_cast<ID *>(id->next);

      /* This function also visits already-loaded data-blocks, and thus their
       * `readfile_data` field might already have been freed. */
      if (BLO_readfile_id_runtime_tags(*id).is_link_placeholder &&
          (id->flag & ID_FLAG_INDIRECT_WEAK_LINK))
      {
        CLOG_DEBUG(&LOG, "Dropping weak link to '%s'", id->name);
        change_link_placeholder_to_real_ID_pointer(basefd, id, nullptr);
        BLI_freelinkN(lbarray[a], id);
      }
      id = id_next;
    }
  }
}

static FileData *read_library_file_data(FileData *basefd, Main *bmain, Main *lib_bmain)
{
  FileData *fd = lib_bmain->curlib->runtime->filedata;

  if (fd != nullptr) {
    /* File already open. */
    return fd;
  }

  if (lib_bmain->curlib->packedfile) {
    /* Read packed file. */
    const PackedFile *pf = lib_bmain->curlib->packedfile;

    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     RPT_("Read packed library: '%s', parent '%s'"),
                     lib_bmain->curlib->filepath,
                     library_parent_filepath(lib_bmain->curlib));
    fd = blo_filedata_from_memory(pf->data, pf->size, basefd->reports);

    /* Needed for library_append and read_libraries. */
    STRNCPY(fd->relabase, lib_bmain->curlib->runtime->filepath_abs);
  }
  else {
    /* Read file on disk. */
    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     RPT_("Read library: '%s', '%s', parent '%s'"),
                     lib_bmain->curlib->runtime->filepath_abs,
                     lib_bmain->curlib->filepath,
                     library_parent_filepath(lib_bmain->curlib));
    fd = blo_filedata_from_file(lib_bmain->curlib->runtime->filepath_abs, basefd->reports);
  }

  if (fd) {
    /* `mainptr` is sharing the same `split_mains`, so all libraries are added immediately in a
     * single vectorset. It used to be that all FileData's had their own list, but with indirectly
     * linking this meant that not all duplicate libraries were catched properly. */
    fd->bmain = bmain;
    fd->fd_bmain = lib_bmain;

    fd->reports = basefd->reports;

    if (fd->libmap) {
      oldnewmap_free(fd->libmap);
    }

    fd->libmap = oldnewmap_new();
    fd->id_by_deep_hash = basefd->id_by_deep_hash;

    lib_bmain->curlib->runtime->filedata = fd;
    lib_bmain->curlib->runtime->is_filedata_owner = true;
    lib_bmain->versionfile = fd->fileversion;

    /* subversion */
    read_file_version_and_colorspace(fd, lib_bmain);
    read_file_bhead_idname_map_create(fd);
  }
  else {
    lib_bmain->curlib->runtime->filedata = nullptr;
    lib_bmain->curlib->runtime->is_filedata_owner = false;
    lib_bmain->curlib->id.tag |= ID_TAG_MISSING;
    /* Set lib version to current main one... Makes assert later happy. */
    lib_bmain->versionfile = lib_bmain->curlib->runtime->versionfile = bmain->versionfile;
    lib_bmain->subversionfile = lib_bmain->curlib->runtime->subversionfile = bmain->subversionfile;
    lib_bmain->colorspace = lib_bmain->curlib->runtime->colorspace = bmain->colorspace;
  }

  if (fd == nullptr) {
    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     RPT_("Cannot find lib '%s'"),
                     lib_bmain->curlib->runtime->filepath_abs);
    basefd->reports->count.missing_libraries++;
  }

  return fd;
}

static void read_libraries(FileData *basefd)
{
  Main *bmain = basefd->bmain;
  BLI_assert(bmain->split_mains);
  bool do_it = true;

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
    for (int i = 1; i < bmain->split_mains->size(); i++) {
      Main *libmain = (*bmain->split_mains)[i];
      BLI_assert(libmain->curlib);
      /* Always skip archived libraries here, these should _never_ need to be processed here, as
       * their data is local data from a blendfile perspective. */
      if (libmain->curlib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
        BLI_assert(!has_linked_ids_to_read(libmain));
        continue;
      }
      /* Does this library have any more linked data-blocks we need to read? */
      if (has_linked_ids_to_read(libmain)) {
        CLOG_DEBUG(&LOG,
                   "Reading linked data-blocks from %s (%s)",
                   libmain->curlib->id.name,
                   libmain->curlib->filepath);

        /* Open file if it has not been done yet. */
        FileData *fd = read_library_file_data(basefd, bmain, libmain);

        if (fd) {
          do_it = true;

          if (libmain->id_map == nullptr) {
            libmain->id_map = BKE_main_idmap_create(libmain, false, nullptr, MAIN_IDMAP_TYPE_NAME);
          }
        }

        /* Read linked data-blocks for each link placeholder, and replace
         * the placeholder with the real data-block. */
        read_library_linked_ids(basefd, fd, libmain);

        /* Test if linked data-blocks need to read further linked data-blocks
         * and create link placeholders for them. */
        expand_main(fd, libmain, expand_doit_library);
      }
    }
  }

  for (Main *libmain : bmain->split_mains->as_span().drop_front(1)) {
    /* Drop weak links for which no data-block was found.
     * Since this can remap pointers in `libmap` of all libraries, it needs to be performed in its
     * own loop, before any call to `lib_link_all` (and the freeing of the libraries' filedata). */
    read_library_clear_weak_links(basefd, libmain);
  }

  Main *main_newid = BKE_main_new();
  for (Main *libmain : bmain->split_mains->as_span().drop_front(1)) {
    /* Do versioning for newly added linked data-blocks. If no data-blocks
     * were read from a library versionfile will still be zero and we can
     * skip it. */
    if (libmain->versionfile) {
      /* Split out already existing IDs to avoid them going through
       * do_versions multiple times, which would have bad consequences. */
      split_main_newid(libmain, main_newid);

      /* `filedata` can be NULL when loading linked data from nonexistent or invalid library
       * reference. Or during linking/appending, when processing data from a library not involved
       * in the current linking/appending operation.
       *
       * Skip versioning in these cases, since the only IDs here will be placeholders (missing
       * lib), or already existing IDs (linking/appending). */
      if (libmain->curlib->runtime->filedata) {
        do_versions(libmain->curlib->runtime->filedata, libmain->curlib, main_newid);
      }

      add_main_to_main(libmain, main_newid);
    }

    /* Lib linking. */
    if (libmain->curlib->runtime->filedata) {
      lib_link_all(libmain->curlib->runtime->filedata, libmain);
    }

    /* NOTE: No need to call #do_versions_after_linking() or #BKE_main_id_refcount_recompute()
     * here, as this function is only called for library 'subset' data handling, as part of
     * either full blend-file reading (#blo_read_file_internal()), or library-data linking
     * (#library_link_end()).
     *
     * For this to work reliably, `mainptr->curlib->runtime->filedata` also needs to be freed after
     * said versioning code has run. */
  }
  BKE_main_free(main_newid);
}

static void *blo_verify_data_address(FileData *fd,
                                     void *new_address,
                                     const void * /*old_address*/,
                                     const size_t expected_size)
{
  if (new_address != nullptr) {
    /* Not testing equality, since size might have been aligned up,
     * or might be passed the size of a base struct with inheritance. */
    if (MEM_allocN_len(new_address) < expected_size) {
      blo_readfile_invalidate(fd,
                              (*fd->bmain->split_mains)[fd->bmain->split_mains->size() - 1],
                              "Corrupt .blend file, unexpected data size.");
      /* Return null to trigger a hard-crash rather than allowing readfile code to further access
       * this invalid block of memory.
       *
       * It could also potentially allow the calling code to do its own error checking and abort
       * reading process, but that is not implemented currently. */
      return nullptr;
    }
  }

  return new_address;
}

void *BLO_read_get_new_data_address(BlendDataReader *reader, const void *old_address)
{
  return newdataadr(reader->fd, old_address);
}

void *BLO_read_get_new_data_address_no_us(BlendDataReader *reader,
                                          const void *old_address,
                                          const size_t expected_size)
{
  void *new_address = newdataadr_no_us(reader->fd, old_address);
  return blo_verify_data_address(reader->fd, new_address, old_address, expected_size);
}

void *BLO_read_struct_array_with_size(BlendDataReader *reader,
                                      const void *old_address,
                                      const size_t expected_size)
{
  void *new_address = newdataadr(reader->fd, old_address);
  return blo_verify_data_address(reader->fd, new_address, old_address, expected_size);
}

void *BLO_read_struct_by_name_array(BlendDataReader *reader,
                                    const char *struct_name,
                                    const int64_t items_num,
                                    const void *old_address)
{
  const int struct_index = DNA_struct_find_with_alias(reader->fd->memsdna, struct_name);
  BLI_assert(STREQ(DNA_struct_identifier(const_cast<SDNA *>(reader->fd->memsdna), struct_index),
                   struct_name));
  const size_t struct_size = size_t(DNA_struct_size(reader->fd->memsdna, struct_index));
  return BLO_read_struct_array_with_size(reader, old_address, struct_size * items_num);
}

ID *BLO_read_get_new_id_address(BlendLibReader *reader,
                                ID *self_id,
                                const bool is_linked_only,
                                ID *id)
{
  return static_cast<ID *>(newlibadr(reader->fd, self_id, is_linked_only, id));
}

ID *BLO_read_get_new_id_address_from_session_uid(BlendLibReader *reader, const uint session_uid)
{
  return BKE_main_idmap_lookup_uid(reader->fd->new_idmap_uid, session_uid);
}

int BLO_read_fileversion_get(BlendDataReader *reader)
{
  return reader->fd->fileversion;
}

void BLO_read_struct_list_with_size(BlendDataReader *reader,
                                    const size_t expected_elem_size,
                                    ListBase *list)
{
  if (BLI_listbase_is_empty(list)) {
    return;
  }

  list->first = BLO_read_struct_array_with_size(reader, list->first, expected_elem_size);
  Link *ln = static_cast<Link *>(list->first);
  Link *prev = nullptr;
  while (ln) {
    ln->next = static_cast<Link *>(
        BLO_read_struct_array_with_size(reader, ln->next, expected_elem_size));
    ln->prev = prev;
    prev = ln;
    ln = ln->next;
  }
  list->last = prev;
}

void BLO_read_char_array(BlendDataReader *reader, const int64_t array_size, char **ptr_p)
{
  *ptr_p = reinterpret_cast<char *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(char) * array_size));
}

void BLO_read_uint8_array(BlendDataReader *reader, const int64_t array_size, uint8_t **ptr_p)
{
  *ptr_p = reinterpret_cast<uint8_t *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(uint8_t) * array_size));
}

void BLO_read_int8_array(BlendDataReader *reader, const int64_t array_size, int8_t **ptr_p)
{
  *ptr_p = reinterpret_cast<int8_t *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(int8_t) * array_size));
}

void BLO_read_int16_array(BlendDataReader *reader, const int64_t array_size, int16_t **ptr_p)
{
  *ptr_p = reinterpret_cast<int16_t *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(int16_t) * array_size));
  BLI_assert((reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
}

void BLO_read_int32_array(BlendDataReader *reader, const int64_t array_size, int32_t **ptr_p)
{
  *ptr_p = reinterpret_cast<int32_t *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(int32_t) * array_size));
  BLI_assert((reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
}

void BLO_read_uint32_array(BlendDataReader *reader, const int64_t array_size, uint32_t **ptr_p)
{
  *ptr_p = reinterpret_cast<uint32_t *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(uint32_t) * array_size));
  BLI_assert((reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
}

void BLO_read_float_array(BlendDataReader *reader, const int64_t array_size, float **ptr_p)
{
  *ptr_p = reinterpret_cast<float *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(float) * array_size));
  BLI_assert((reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
}

void BLO_read_float3_array(BlendDataReader *reader, const int64_t array_size, float **ptr_p)
{
  BLO_read_float_array(reader, array_size * 3, ptr_p);
}

void BLO_read_double_array(BlendDataReader *reader, const int64_t array_size, double **ptr_p)
{
  *ptr_p = reinterpret_cast<double *>(
      BLO_read_struct_array_with_size(reader, *((void **)ptr_p), sizeof(double) * array_size));
  BLI_assert((reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
}

void BLO_read_string(BlendDataReader *reader, char **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);

#ifndef NDEBUG
  const char *str = *ptr_p;
  if (str) {
    /* Verify that we have a null terminator. */
    for (size_t len = MEM_allocN_len(str); len > 0; len--) {
      if (str[len - 1] == '\0') {
        return;
      }
    }

    BLI_assert_msg(0, "Corrupt .blend file, expected string to be null terminated.");
  }
#endif
}

void BLO_read_string(BlendDataReader *reader, char *const *ptr_p)
{
  BLO_read_string(reader, const_cast<char **>(ptr_p));
}

void BLO_read_string(BlendDataReader *reader, const char **ptr_p)
{
  BLO_read_string(reader, const_cast<char **>(ptr_p));
}

static void convert_pointer_array_64_to_32(BlendDataReader *reader,
                                           const int64_t array_size,
                                           const uint64_t *src,
                                           uint32_t *dst)
{
  BLI_assert((reader->fd->flags & FD_FLAGS_SWITCH_ENDIAN) == 0);
  UNUSED_VARS_NDEBUG(reader);
  for (int i = 0; i < array_size; i++) {
    dst[i] = uint32_from_uint64_ptr(src[i]);
  }
}

static void convert_pointer_array_32_to_64(BlendDataReader * /*reader*/,
                                           const int64_t array_size,
                                           const uint32_t *src,
                                           uint64_t *dst)
{
  /* Match pointer conversion rules from bh8_from_bh4 and cast_pointer_32_to_64. */
  for (int i = 0; i < array_size; i++) {
    dst[i] = src[i];
  }
}

void BLO_read_pointer_array(BlendDataReader *reader, const int64_t array_size, void **ptr_p)
{
  FileData *fd = reader->fd;

  void *orig_array = newdataadr(fd, *ptr_p);
  if (orig_array == nullptr) {
    *ptr_p = nullptr;
    return;
  }

  int file_pointer_size = fd->filesdna->pointer_size;
  int current_pointer_size = fd->memsdna->pointer_size;

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
    BLI_assert_unreachable();
  }

  *ptr_p = final_array;
}

blender::ImplicitSharingInfoAndData blo_read_shared_impl(
    BlendDataReader *reader,
    const void **ptr_p,
    const blender::FunctionRef<const blender::ImplicitSharingInfo *()> read_fn)
{
  const uint64_t old_address_id = uint64_t(*ptr_p);
  if (BLO_read_data_is_undo(reader)) {
    if (reader->fd->flags & FD_FLAGS_IS_MEMFILE) {
      UndoReader *undo_reader = reinterpret_cast<UndoReader *>(reader->fd->file);
      const MemFile &memfile = *undo_reader->memfile;
      if (memfile.shared_storage) {
        /* Check if the data was saved with sharing-info. */
        if (const blender::ImplicitSharingInfoAndData *sharing_info_data =
                memfile.shared_storage->sharing_info_by_address_id.lookup_ptr(old_address_id))
        {
          /* Add a new owner of the data that is passed to the caller. */
          sharing_info_data->sharing_info->add_user();
          return *sharing_info_data;
        }
      }
    }
  }

  if (const blender::ImplicitSharingInfoAndData *shared_data =
          reader->shared_data_by_stored_address.lookup_ptr(old_address_id))
  {
    /* The data was loaded before. No need to load it again. Just increase the user count to
     * indicate that it is shared. */
    if (shared_data->sharing_info) {
      shared_data->sharing_info->add_user();
    }
    return *shared_data;
  }

  /* This is the first time this data is loaded. The callback also creates the corresponding
   * sharing info which may be reused later. */
  const blender::ImplicitSharingInfo *sharing_info = read_fn();
  const void *new_address = *ptr_p;
  const blender::ImplicitSharingInfoAndData shared_data{sharing_info, new_address};
  reader->shared_data_by_stored_address.add(old_address_id, shared_data);
  return shared_data;
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
