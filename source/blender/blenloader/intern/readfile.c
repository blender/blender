/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup blenloader
 */

#include <ctype.h> /* for isdigit. */
#include <fcntl.h> /* for open flags (O_BINARY, O_RDONLY). */
#include <limits.h>
#include <stdarg.h> /* for va_start/end. */
#include <stddef.h> /* for offsetof. */
#include <stdlib.h> /* for atoi. */
#include <time.h>   /* for gmtime. */

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
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_asset.h"
#include "BKE_blender_version.h"
#include "BKE_collection.h"
#include "BKE_global.h" /* for G */
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_main.h" /* for Main */
#include "BKE_main_idmap.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h" /* for tree type defines */
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_undo_system.h"
#include "BKE_workspace.h"

#include "DRW_engine.h"

#include "DEG_depsgraph.h"

#include "BLO_blend_defs.h"
#include "BLO_blend_validate.h"
#include "BLO_read_write.h"
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "SEQ_clipboard.h"
#include "SEQ_iterator.h"
#include "SEQ_modifier.h"
#include "SEQ_sequencer.h"
#include "SEQ_utils.h"

#include "readfile.h"

#include <errno.h>

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
 * - read #USER data, only when indicated (file is `~/.config/blender/X.XX/config/userpref.blend`)
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
 * while ZLIB supports seek it's unusably slow, see: T61880.
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

typedef struct BHeadN {
  struct BHeadN *next, *prev;
#ifdef USE_BHEAD_READ_ON_DEMAND
  /** Use to read the data from the file directly into memory as needed. */
  off64_t file_offset;
  /** When set, the remainder of this allocation is the data, otherwise it needs to be read. */
  bool has_data;
#endif
  bool is_memchunk_identical;
  struct BHead bhead;
} BHeadN;

#define BHEADN_FROM_BHEAD(bh) ((BHeadN *)POINTER_OFFSET(bh, -(int)offsetof(BHeadN, bhead)))

/**
 * We could change this in the future, for now it's simplest if only data is delayed
 * because ID names are used in lookup tables.
 */
#define BHEAD_USE_READ_ON_DEMAND(bhead) ((bhead)->code == DATA)

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

/* -------------------------------------------------------------------- */
/** \name OldNewMap API
 * \{ */

typedef struct OldNew {
  const void *oldp;
  void *newp;
  /* `nr` is "user count" for data, and ID code for libdata. */
  int nr;
} OldNew;

typedef struct OldNewMap {
  /* Array that stores the actual entries. */
  OldNew *entries;
  int nentries;
  /* Hash-map that stores indices into the `entries` array. */
  int32_t *map;

  int capacity_exp;
} OldNewMap;

#define ENTRIES_CAPACITY(onm) (1ll << (onm)->capacity_exp)
#define MAP_CAPACITY(onm) (1ll << ((onm)->capacity_exp + 1))
#define SLOT_MASK(onm) (MAP_CAPACITY(onm) - 1)
#define DEFAULT_SIZE_EXP 6
#define PERTURB_SHIFT 5

/* based on the probing algorithm used in Python dicts. */
#define ITER_SLOTS(onm, KEY, SLOT_NAME, INDEX_NAME) \
  uint32_t hash = BLI_ghashutil_ptrhash(KEY); \
  uint32_t mask = SLOT_MASK(onm); \
  uint perturb = hash; \
  int SLOT_NAME = mask & hash; \
  int INDEX_NAME = onm->map[SLOT_NAME]; \
  for (;; SLOT_NAME = mask & ((5 * SLOT_NAME) + 1 + perturb), \
          perturb >>= PERTURB_SHIFT, \
          INDEX_NAME = onm->map[SLOT_NAME])

static void oldnewmap_insert_index_in_map(OldNewMap *onm, const void *ptr, int index)
{
  ITER_SLOTS (onm, ptr, slot, stored_index) {
    if (stored_index == -1) {
      onm->map[slot] = index;
      break;
    }
  }
}

static void oldnewmap_insert_or_replace(OldNewMap *onm, OldNew entry)
{
  ITER_SLOTS (onm, entry.oldp, slot, index) {
    if (index == -1) {
      onm->entries[onm->nentries] = entry;
      onm->map[slot] = onm->nentries;
      onm->nentries++;
      break;
    }
    if (onm->entries[index].oldp == entry.oldp) {
      onm->entries[index] = entry;
      break;
    }
  }
}

static OldNew *oldnewmap_lookup_entry(const OldNewMap *onm, const void *addr)
{
  ITER_SLOTS (onm, addr, slot, index) {
    if (index >= 0) {
      OldNew *entry = &onm->entries[index];
      if (entry->oldp == addr) {
        return entry;
      }
    }
    else {
      return NULL;
    }
  }
}

static void oldnewmap_clear_map(OldNewMap *onm)
{
  memset(onm->map, 0xFF, MAP_CAPACITY(onm) * sizeof(*onm->map));
}

static void oldnewmap_increase_size(OldNewMap *onm)
{
  onm->capacity_exp++;
  onm->entries = MEM_reallocN(onm->entries, sizeof(*onm->entries) * ENTRIES_CAPACITY(onm));
  onm->map = MEM_reallocN(onm->map, sizeof(*onm->map) * MAP_CAPACITY(onm));
  oldnewmap_clear_map(onm);
  for (int i = 0; i < onm->nentries; i++) {
    oldnewmap_insert_index_in_map(onm, onm->entries[i].oldp, i);
  }
}

/* Public OldNewMap API */

static void oldnewmap_init_data(OldNewMap *onm, const int capacity_exp)
{
  memset(onm, 0x0, sizeof(*onm));

  onm->capacity_exp = capacity_exp;
  onm->entries = MEM_malloc_arrayN(
      ENTRIES_CAPACITY(onm), sizeof(*onm->entries), "OldNewMap.entries");
  onm->map = MEM_malloc_arrayN(MAP_CAPACITY(onm), sizeof(*onm->map), "OldNewMap.map");
  oldnewmap_clear_map(onm);
}

static OldNewMap *oldnewmap_new(void)
{
  OldNewMap *onm = MEM_mallocN(sizeof(*onm), "OldNewMap");

  oldnewmap_init_data(onm, DEFAULT_SIZE_EXP);

  return onm;
}

static void oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr)
{
  if (oldaddr == NULL || newaddr == NULL) {
    return;
  }

  if (UNLIKELY(onm->nentries == ENTRIES_CAPACITY(onm))) {
    oldnewmap_increase_size(onm);
  }

  OldNew entry;
  entry.oldp = oldaddr;
  entry.newp = newaddr;
  entry.nr = nr;
  oldnewmap_insert_or_replace(onm, entry);
}

static void oldnewmap_lib_insert(FileData *fd, const void *oldaddr, ID *newaddr, int nr)
{
  oldnewmap_insert(fd->libmap, oldaddr, newaddr, nr);
}

void blo_do_versions_oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr)
{
  oldnewmap_insert(onm, oldaddr, newaddr, nr);
}

static void *oldnewmap_lookup_and_inc(OldNewMap *onm, const void *addr, bool increase_users)
{
  OldNew *entry = oldnewmap_lookup_entry(onm, addr);
  if (entry == NULL) {
    return NULL;
  }
  if (increase_users) {
    entry->nr++;
  }
  return entry->newp;
}

/* for libdata, OldNew.nr has ID code, no increment */
static void *oldnewmap_liblookup(OldNewMap *onm, const void *addr, const void *lib)
{
  if (addr == NULL) {
    return NULL;
  }

  ID *id = oldnewmap_lookup_and_inc(onm, addr, false);
  if (id == NULL) {
    return NULL;
  }
  if (!lib || id->lib) {
    return id;
  }
  return NULL;
}

static void oldnewmap_clear(OldNewMap *onm)
{
  /* Free unused data. */
  for (int i = 0; i < onm->nentries; i++) {
    OldNew *entry = &onm->entries[i];
    if (entry->nr == 0) {
      MEM_freeN(entry->newp);
      entry->newp = NULL;
    }
  }

  MEM_freeN(onm->entries);
  MEM_freeN(onm->map);

  oldnewmap_init_data(onm, DEFAULT_SIZE_EXP);
}

static void oldnewmap_free(OldNewMap *onm)
{
  MEM_freeN(onm->entries);
  MEM_freeN(onm->map);
  MEM_freeN(onm);
}

#undef ENTRIES_CAPACITY
#undef MAP_CAPACITY
#undef SLOT_MASK
#undef DEFAULT_SIZE_EXP
#undef PERTURB_SHIFT
#undef ITER_SLOTS

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

  mainl = mainlist->first;

  if (mainl->id_map != NULL) {
    /* Cannot keep this since we add some IDs from joined mains. */
    BKE_main_idmap_destroy(mainl->id_map);
    mainl->id_map = NULL;
  }

  while ((tojoin = mainl->next)) {
    add_main_to_main(mainl, tojoin);
    BLI_remlink(mainlist, tojoin);
    tojoin->next = tojoin->prev = NULL;
    BKE_main_free(tojoin);
  }
}

static void split_libdata(ListBase *lb_src, Main **lib_main_array, const uint lib_main_array_len)
{
  for (ID *id = lb_src->first, *idnext; id; id = idnext) {
    idnext = id->next;

    if (id->lib) {
      if (((uint)id->lib->temp_index < lib_main_array_len) &&
          /* this check should never fail, just in case 'id->lib' is a dangling pointer. */
          (lib_main_array[id->lib->temp_index]->curlib == id->lib)) {
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
  main->next = NULL;

  if (BLI_listbase_is_empty(&main->libraries)) {
    return;
  }

  if (main->id_map != NULL) {
    /* Cannot keep this since we remove some IDs from given main. */
    BKE_main_idmap_destroy(main->id_map);
    main->id_map = NULL;
  }

  /* (Library.temp_index -> Main), lookup table */
  const uint lib_main_array_len = BLI_listbase_count(&main->libraries);
  Main **lib_main_array = MEM_malloc_arrayN(lib_main_array_len, sizeof(*lib_main_array), __func__);

  int i = 0;
  for (Library *lib = main->libraries.first; lib; lib = lib->id.next, i++) {
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
    ID *id = lbarray[i]->first;
    if (id == NULL || GS(id->name) == ID_LI) {
      /* No ID_LI data-block should ever be linked anyway, but just in case, better be explicit. */
      continue;
    }
    split_libdata(lbarray[i], lib_main_array, lib_main_array_len);
  }

  MEM_freeN(lib_main_array);
}

/* Hot-fix file minversion for a specific range of versions.
 *
 * This is needed for newer LTS releases of 3.6 to be able to properly open files from early
 * development versions of 4.0. */
static void file_global_minversion_hotfix(FileGlobal *fg)
{
  if ((fg->minversion > BLENDER_FILE_VERSION) ||
      (fg->minversion == BLENDER_FILE_VERSION && fg->minsubversion > BLENDER_FILE_SUBVERSION)) {
    if (fg->minversion < 306 || (fg->minversion == 306 && fg->minsubversion <= 11)) {
      fg->minversion = BLENDER_FILE_VERSION;
      fg->minsubversion = BLENDER_FILE_SUBVERSION;
    }
  }
}

static void read_file_version(FileData *fd, Main *main)
{
  BHead *bhead;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == GLOB) {
      FileGlobal *fg = read_struct(fd, bhead, "Global");
      if (fg) {
        file_global_minversion_hotfix(fg);

        main->subversionfile = fg->subversion;
        main->minversionfile = fg->minversion;
        main->minsubversionfile = fg->minsubversion;
        MEM_freeN(fg);
      }
      else if (bhead->code == ENDB) {
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
  int code_prev = ENDB;
  uint reserve = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (code_prev != bhead->code) {
      code_prev = bhead->code;
      is_link = blo_bhead_is_id_valid_type(bhead) ?
                    BKE_idtype_idcode_is_linkable((short)code_prev) :
                    false;
    }

    if (is_link) {
      reserve += 1;
    }
  }

  BLI_assert(fd->bhead_idname_hash == NULL);

  fd->bhead_idname_hash = BLI_ghash_str_new_ex(__func__, reserve);

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (code_prev != bhead->code) {
      code_prev = bhead->code;
      is_link = blo_bhead_is_id_valid_type(bhead) ?
                    BKE_idtype_idcode_is_linkable((short)code_prev) :
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
  char name1[FILE_MAX];

  BLI_strncpy(name1, filepath, sizeof(name1));
  BLI_path_normalize(relabase, name1);

  //  printf("blo_find_main: relabase  %s\n", relabase);
  //  printf("blo_find_main: original in  %s\n", filepath);
  //  printf("blo_find_main: converted to %s\n", name1);

  for (m = mainlist->first; m; m = m->next) {
    const char *libname = (m->curlib) ? m->curlib->filepath_abs : m->filepath;

    if (BLI_path_cmp(name1, libname) == 0) {
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
  lib = BKE_libblock_alloc(mainlist->first, ID_LI, BLI_path_basename(filepath), 0);

  /* Important, consistency with main ID reading code from read_libblock(). */
  lib->id.us = ID_FAKE_USERS(lib);

  /* Matches direct_link_library(). */
  id_us_ensure_real(&lib->id);

  BLI_strncpy(lib->filepath, filepath, sizeof(lib->filepath));
  BLI_strncpy(lib->filepath_abs, name1, sizeof(lib->filepath_abs));

  m->curlib = lib;

  read_file_version(fd, m);

  if (G.debug & G_DEBUG) {
    CLOG_INFO(&LOG, 3, "Added new lib %s", filepath);
  }
  return m;
}

void blo_readfile_invalidate(FileData *fd, Main *bmain, const char *message)
{
  /* Tag given bmain, and 'root 'local' main one (in case given one is a library one) as invalid.
   */
  bmain->is_read_invalid = true;
  for (; bmain->prev != NULL; bmain = bmain->prev)
    ;
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

typedef struct BlendDataReader {
  FileData *fd;
} BlendDataReader;

typedef struct BlendLibReader {
  FileData *fd;
  Main *main;
} BlendLibReader;

typedef struct BlendExpander {
  FileData *fd;
  Main *main;
} BlendExpander;

static void switch_endian_bh4(BHead4 *bhead)
{
  /* the ID_.. codes */
  if ((bhead->code & 0xFFFF) == 0) {
    bhead->code >>= 16;
  }

  if (bhead->code != ENDB) {
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

  if (bhead->code != ENDB) {
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

  if (bhead4->code != ENDB) {
    /* perform a endian swap on 64bit pointers, otherwise the pointer might map to zero
     * 0x0000000000000000000012345678 would become 0x12345678000000000000000000000000
     */
    if (do_endian_swap) {
      BLI_endian_switch_uint64(&bhead8->old);
    }

    /* this patch is to avoid `intptr_t` being read from not-eight aligned positions
     * is necessary on any modern 64bit architecture) */
    memcpy(&old, &bhead8->old, 8);
    bhead4->old = (int)(old >> 3);

    bhead4->SDNAnr = bhead8->SDNAnr;
    bhead4->nr = bhead8->nr;
  }
}

static void bh8_from_bh4(BHead *bhead, BHead4 *bhead4)
{
  BHead8 *bhead8 = (BHead8 *)bhead;

  bhead8->code = bhead4->code;
  bhead8->len = bhead4->len;

  if (bhead8->code != ENDB) {
    bhead8->old = bhead4->old;
    bhead8->SDNAnr = bhead4->SDNAnr;
    bhead8->nr = bhead4->nr;
  }
}

static BHeadN *get_bhead(FileData *fd)
{
  BHeadN *new_bhead = NULL;
  ssize_t readsize;

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
        bhead4.code = DATA;
        readsize = fd->file->read(fd->file, &bhead4, sizeof(bhead4));

        if (readsize == sizeof(bhead4) || bhead4.code == ENDB) {
          if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
            switch_endian_bh4(&bhead4);
          }

          if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
            bh8_from_bh4(&bhead, &bhead4);
          }
          else {
            /* MIN2 is only to quiet '-Warray-bounds' compiler warning. */
            BLI_assert(sizeof(bhead) == sizeof(bhead4));
            memcpy(&bhead, &bhead4, MIN2(sizeof(bhead), sizeof(bhead4)));
          }
        }
        else {
          fd->is_eof = true;
          bhead.len = 0;
        }
      }
      else {
        bhead8.code = DATA;
        readsize = fd->file->read(fd->file, &bhead8, sizeof(bhead8));

        if (readsize == sizeof(bhead8) || bhead8.code == ENDB) {
          if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
            switch_endian_bh8(&bhead8);
          }

          if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
            bh4_from_bh8(&bhead, &bhead8, (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0);
          }
          else {
            /* MIN2 is only to quiet '-Warray-bounds' compiler warning. */
            BLI_assert(sizeof(bhead) == sizeof(bhead8));
            memcpy(&bhead, &bhead8, MIN2(sizeof(bhead), sizeof(bhead8)));
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
      else if (fd->file->seek != NULL && BHEAD_USE_READ_ON_DEMAND(&bhead)) {
        /* Delay reading bhead content. */
        new_bhead = MEM_mallocN(sizeof(BHeadN), "new_bhead");
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = NULL;
          new_bhead->file_offset = fd->file->offset;
          new_bhead->has_data = false;
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = bhead;
          off64_t seek_new = fd->file->seek(fd->file, bhead.len, SEEK_CUR);
          if (seek_new == -1) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = NULL;
          }
          BLI_assert(fd->file->offset == seek_new);
        }
        else {
          fd->is_eof = true;
        }
      }
#endif
      else {
        new_bhead = MEM_mallocN(sizeof(BHeadN) + (size_t)bhead.len, "new_bhead");
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = NULL;
#ifdef USE_BHEAD_READ_ON_DEMAND
          new_bhead->file_offset = 0; /* don't seek. */
          new_bhead->has_data = true;
#endif
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = bhead;

          readsize = fd->file->read(fd->file, new_bhead + 1, (size_t)bhead.len);

          if (readsize != bhead.len) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = NULL;
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
  BHead *bhead = NULL;

  /* Rewind the file
   * Read in a new block if necessary
   */
  new_bhead = fd->bhead_list.first;
  if (new_bhead == NULL) {
    new_bhead = get_bhead(fd);
  }

  if (new_bhead) {
    bhead = &new_bhead->bhead;
  }

  return bhead;
}

BHead *blo_bhead_prev(FileData *UNUSED(fd), BHead *thisblock)
{
  BHeadN *bheadn = BHEADN_FROM_BHEAD(thisblock);
  BHeadN *prev = bheadn->prev;

  return (prev) ? &prev->bhead : NULL;
}

BHead *blo_bhead_next(FileData *fd, BHead *thisblock)
{
  BHeadN *new_bhead = NULL;
  BHead *bhead = NULL;

  if (thisblock) {
    /* bhead is actually a sub part of BHeadN
     * We calculate the BHeadN pointer from the BHead pointer below */
    new_bhead = BHEADN_FROM_BHEAD(thisblock);

    /* get the next BHeadN. If it doesn't exist we read in the next one */
    new_bhead = new_bhead->next;
    if (new_bhead == NULL) {
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
    if (fd->file->read(fd->file, buf, (size_t)new_bhead->bhead.len) != new_bhead->bhead.len) {
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
  BHeadN *new_bhead_data = MEM_mallocN(sizeof(BHeadN) + new_bhead->bhead.len, "new_bhead");
  new_bhead_data->bhead = new_bhead->bhead;
  new_bhead_data->file_offset = new_bhead->file_offset;
  new_bhead_data->has_data = true;
  new_bhead_data->is_memchunk_identical = false;
  if (!blo_bhead_read_data(fd, thisblock, new_bhead_data + 1)) {
    MEM_freeN(new_bhead_data);
    return NULL;
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
             NULL;
}

static void decode_blender_header(FileData *fd)
{
  char header[SIZEOFBLENDERHEADER], num[4];
  ssize_t readsize;

  /* read in the header data */
  readsize = fd->file->read(fd->file, header, sizeof(header));

  if (readsize == sizeof(header) && STREQLEN(header, "BLENDER", 7) && ELEM(header[7], '_', '-') &&
      ELEM(header[8], 'v', 'V') &&
      (isdigit(header[9]) && isdigit(header[10]) && isdigit(header[11]))) {
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
    if (bhead->code == GLOB) {
      /* Before this, the subversion didn't exist in 'FileGlobal' so the subversion
       * value isn't accessible for the purpose of DNA versioning in this case. */
      if (fd->fileversion <= 242) {
        continue;
      }
      /* We can't use read_global because this needs 'DNA1' to be decoded,
       * however the first 4 chars are _always_ the subversion. */
      FileGlobal *fg = (void *)&bhead[1];
      BLI_STATIC_ASSERT(offsetof(FileGlobal, subvstr) == 0, "Must be first: subvstr")
      char num[5];
      memcpy(num, fg->subvstr, 4);
      num[4] = 0;
      subversion = atoi(num);
    }
    else if (bhead->code == DNA1) {
      const bool do_endian_swap = (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;

      fd->filesdna = DNA_sdna_from_data(
          &bhead[1], bhead->len, do_endian_swap, true, r_error_message);
      if (fd->filesdna) {
        blo_do_versions_dna(fd->filesdna, fd->fileversion, subversion);
        fd->compflags = DNA_struct_get_compareflags(fd->filesdna, fd->memsdna);
        fd->reconstruct_info = DNA_reconstruct_info_create(
            fd->filesdna, fd->memsdna, fd->compflags);
        /* used to retrieve ID names from (bhead+1) */
        fd->id_name_offset = DNA_elem_offset(fd->filesdna, "ID", "char", "name[]");
        BLI_assert(fd->id_name_offset != -1);
        fd->id_asset_data_offset = DNA_elem_offset(
            fd->filesdna, "ID", "AssetMetaData", "*asset_data");

        return true;
      }

      return false;
    }
    else if (bhead->code == ENDB) {
      break;
    }
  }

  *r_error_message = "Missing DNA block";
  return false;
}

static int *read_file_thumbnail(FileData *fd)
{
  BHead *bhead;
  int *blend_thumb = NULL;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == TEST) {
      const bool do_endian_swap = (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;
      int *data = (int *)(bhead + 1);

      if (bhead->len < (sizeof(int[2]))) {
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
    if (bhead->code != REND) {
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
  BLI_assert(reports != NULL);

  FileData *fd = MEM_callocN(sizeof(FileData), "FileData");

  fd->memsdna = DNA_sdna_current_get();

  fd->datamap = oldnewmap_new();
  fd->globmap = oldnewmap_new();
  fd->libmap = oldnewmap_new();

  fd->reports = reports;

  return fd;
}

/** Check if minversion of the file is older than current Blender, return false if it is not.
 * Should only be called after #read_file_dna was successfuly executed. */
static bool is_minversion_older_than_blender(FileData *fd, ReportList *reports)
{
  BLI_assert(fd->filesdna != NULL);
  for (BHead *bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code != GLOB) {
      continue;
    }

    FileGlobal *fg = read_struct(fd, bhead, "Global");
    file_global_minversion_hotfix(fg);

    if ((fg->minversion > BLENDER_FILE_VERSION) ||
        (fg->minversion == BLENDER_FILE_VERSION && fg->minsubversion > BLENDER_FILE_SUBVERSION)) {
      char writer_ver_str[16];
      char min_reader_ver_str[16];
      if (fd->fileversion == fg->minversion) {
        BKE_blender_version_blendfile_string_from_values(
            writer_ver_str, sizeof(writer_ver_str), (short)(fd->fileversion), fg->subversion);
        BKE_blender_version_blendfile_string_from_values(
            min_reader_ver_str, sizeof(min_reader_ver_str), fg->minversion, fg->minsubversion);
      }
      else {
        BKE_blender_version_blendfile_string_from_values(
            writer_ver_str, sizeof(writer_ver_str), (short)(fd->fileversion), -1);
        BKE_blender_version_blendfile_string_from_values(
            min_reader_ver_str, sizeof(min_reader_ver_str), fg->minversion, -1);
      }
      BKE_reportf(reports,
                  RPT_ERROR,
                  TIP_("The file was saved by a newer version, open it with Blender %s or later"),
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
    const char *error_message = NULL;
    if (read_file_dna(fd, &error_message) == false) {
      BKE_reportf(
          reports, RPT_ERROR, "Failed to read blend file '%s': %s", fd->relabase, error_message);
      blo_filedata_free(fd);
      fd = NULL;
    }
    if (is_minversion_older_than_blender(fd, reports)) {
      blo_filedata_free(fd);
      fd = NULL;
    }
  }
  else {
    BKE_reportf(
        reports, RPT_ERROR, "Failed to read blend file '%s', not a blend file", fd->relabase);
    blo_filedata_free(fd);
    fd = NULL;
  }

  return fd;
}

static FileData *blo_filedata_from_file_descriptor(const char *filepath,
                                                   BlendFileReadReport *reports,
                                                   int filedes)
{
  char header[7];
  FileReader *rawfile = BLI_filereader_new_file(filedes);
  FileReader *file = NULL;

  errno = 0;
  /* If opening the file failed or we can't read the header, give up. */
  if (rawfile == NULL || rawfile->read(rawfile, header, sizeof(header)) != sizeof(header)) {
    BKE_reportf(reports->reports,
                RPT_WARNING,
                "Unable to read '%s': %s",
                filepath,
                errno ? strerror(errno) : TIP_("insufficient content"));
    if (rawfile) {
      rawfile->close(rawfile);
    }
    else {
      close(filedes);
    }
    return NULL;
  }

  /* Rewind the file after reading the header. */
  rawfile->seek(rawfile, 0, SEEK_SET);

  /* Check if we have a regular file. */
  if (memcmp(header, "BLENDER", sizeof(header)) == 0) {
    /* Try opening the file with memory-mapped IO. */
    file = BLI_filereader_new_mmap(filedes);
    if (file == NULL) {
      /* mmap failed, so just keep using rawfile. */
      file = rawfile;
      rawfile = NULL;
    }
  }
  else if (BLI_file_magic_is_gzip(header)) {
    file = BLI_filereader_new_gzip(rawfile);
    if (file != NULL) {
      rawfile = NULL; /* The `Gzip` #FileReader takes ownership of `rawfile`. */
    }
  }
  else if (BLI_file_magic_is_zstd(header)) {
    file = BLI_filereader_new_zstd(rawfile);
    if (file != NULL) {
      rawfile = NULL; /* The `Zstd` #FileReader takes ownership of `rawfile`. */
    }
  }

  /* Clean up `rawfile` if it wasn't taken over. */
  if (rawfile != NULL) {
    rawfile->close(rawfile);
  }
  if (file == NULL) {
    BKE_reportf(reports->reports, RPT_WARNING, "Unrecognized file format '%s'", filepath);
    return NULL;
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
                errno ? strerror(errno) : TIP_("unknown error reading file"));
    return NULL;
  }
  return blo_filedata_from_file_descriptor(filepath, reports, file);
}

FileData *blo_filedata_from_file(const char *filepath, BlendFileReadReport *reports)
{
  FileData *fd = blo_filedata_from_file_open(filepath, reports);
  if (fd != NULL) {
    /* needed for library_append and read_libraries */
    BLI_strncpy(fd->relabase, filepath, sizeof(fd->relabase));

    return blo_decode_and_check(fd, reports->reports);
  }
  return NULL;
}

/**
 * Same as blo_filedata_from_file(), but does not reads DNA data, only header.
 * Use it for light access (e.g. thumbnail reading).
 */
static FileData *blo_filedata_from_file_minimal(const char *filepath)
{
  FileData *fd = blo_filedata_from_file_open(filepath, &(BlendFileReadReport){.reports = NULL});
  if (fd != NULL) {
    decode_blender_header(fd);
    if (fd->flags & FD_FLAGS_FILE_OK) {
      return fd;
    }
    blo_filedata_free(fd);
  }
  return NULL;
}

FileData *blo_filedata_from_memory(const void *mem, int memsize, BlendFileReadReport *reports)
{
  if (!mem || memsize < SIZEOFBLENDERHEADER) {
    BKE_report(
        reports->reports, RPT_WARNING, (mem) ? TIP_("Unable to read") : TIP_("Unable to open"));
    return NULL;
  }

  FileReader *mem_file = BLI_filereader_new_memory(mem, memsize);
  FileReader *file = mem_file;

  if (BLI_file_magic_is_gzip(mem)) {
    file = BLI_filereader_new_gzip(mem_file);
  }
  else if (BLI_file_magic_is_zstd(mem)) {
    file = BLI_filereader_new_zstd(mem_file);
  }

  if (file == NULL) {
    /* Compression initialization failed. */
    mem_file->close(mem_file);
    return NULL;
  }

  FileData *fd = filedata_new(reports);
  fd->file = file;

  return blo_decode_and_check(fd, reports->reports);
}

FileData *blo_filedata_from_memfile(MemFile *memfile,
                                    const struct BlendFileReadParams *params,
                                    BlendFileReadReport *reports)
{
  if (!memfile) {
    BKE_report(reports->reports, RPT_WARNING, "Unable to open blend <memory>");
    return NULL;
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
      if (fd->file->seek != NULL && BHEAD_USE_READ_ON_DEMAND(&new_bhead->bhead)) {
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
    if (fd->old_idmap != NULL) {
      BKE_main_idmap_destroy(fd->old_idmap);
    }
    blo_cache_storage_end(fd);
    if (fd->bheadmap) {
      MEM_freeN(fd->bheadmap);
    }

#ifdef USE_GHASH_BHEAD
    if (fd->bhead_idname_hash) {
      BLI_ghash_free(fd->bhead_idname_hash, NULL, NULL);
    }
#endif

    MEM_freeN(fd);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Utilities
 * \{ */

bool BLO_has_bfile_extension(const char *str)
{
  const char *ext_test[4] = {".blend", ".ble", ".blend.gz", NULL};
  return BLI_path_extension_check_array(str, ext_test);
}

bool BLO_library_path_explode(const char *path, char *r_dir, char **r_group, char **r_name)
{
  /* We might get some data names with slashes,
   * so we have to go up in path until we find blend file itself,
   * then we know next path item is group, and everything else is data name. */
  char *slash = NULL, *prev_slash = NULL, c = '\0';

  r_dir[0] = '\0';
  if (r_group) {
    *r_group = NULL;
  }
  if (r_name) {
    *r_name = NULL;
  }

  /* if path leads to an existing directory, we can be sure we're not (in) a library */
  if (BLI_is_dir(path)) {
    return false;
  }

  strcpy(r_dir, path);

  while ((slash = (char *)BLI_path_slash_rfind(r_dir))) {
    char tc = *slash;
    *slash = '\0';
    if (BLO_has_bfile_extension(r_dir) && BLI_is_file(r_dir)) {
      break;
    }
    if (STREQ(r_dir, BLO_EMBEDDED_STARTUP_BLEND)) {
      break;
    }

    if (prev_slash) {
      *prev_slash = c;
    }
    prev_slash = slash;
    c = tc;
  }

  if (!slash) {
    return false;
  }

  if (slash[1] != '\0') {
    BLI_assert(strlen(slash + 1) < BLO_GROUP_MAX);
    if (r_group) {
      *r_group = slash + 1;
    }
  }

  if (prev_slash && (prev_slash[1] != '\0')) {
    BLI_assert(strlen(prev_slash + 1) < MAX_ID_NAME - 2);
    if (r_name) {
      *r_name = prev_slash + 1;
    }
  }

  return true;
}

BlendThumbnail *BLO_thumbnail_from_file(const char *filepath)
{
  FileData *fd;
  BlendThumbnail *data = NULL;
  int *fd_data;

  fd = blo_filedata_from_file_minimal(filepath);
  fd_data = fd ? read_file_thumbnail(fd) : NULL;

  if (fd_data) {
    const int width = fd_data[0];
    const int height = fd_data[1];
    if (BLEN_THUMB_MEMSIZE_IS_VALID(width, height)) {
      const size_t data_size = BLEN_THUMB_MEMSIZE(width, height);
      data = MEM_mallocN(data_size, __func__);
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
static void *newlibadr(FileData *fd, const void *lib, const void *adr)
{
  return oldnewmap_liblookup(fd->libmap, adr, lib);
}

void *blo_do_versions_newlibadr(FileData *fd, const void *lib, const void *adr)
{
  return newlibadr(fd, lib, adr);
}

/* increases user number */
static void change_link_placeholder_to_real_ID_pointer_fd(FileData *fd, const void *old, void *new)
{
  for (int i = 0; i < fd->libmap->nentries; i++) {
    OldNew *entry = &fd->libmap->entries[i];

    if (old == entry->newp && entry->nr == ID_LINK_PLACEHOLDER) {
      entry->newp = new;
      if (new) {
        entry->nr = GS(((ID *)new)->name);
      }
    }
  }
}

static void change_link_placeholder_to_real_ID_pointer(ListBase *mainlist,
                                                       FileData *basefd,
                                                       void *old,
                                                       void *new)
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
      change_link_placeholder_to_real_ID_pointer_fd(fd, old, new);
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
  OldNew *entry = fd->packedmap->entries;

  /* used entries were restored, so we put them to zero */
  for (int i = 0; i < fd->packedmap->nentries; i++, entry++) {
    if (entry->nr > 0) {
      entry->newp = NULL;
    }
  }

  LISTBASE_FOREACH (Image *, ima, &oldmain->images) {
    ima->packedfile = newpackedadr(fd, ima->packedfile);

    LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
      imapf->packedfile = newpackedadr(fd, imapf->packedfile);
    }
  }

  LISTBASE_FOREACH (VFont *, vfont, &oldmain->fonts) {
    vfont->packedfile = newpackedadr(fd, vfont->packedfile);
  }

  LISTBASE_FOREACH (bSound *, sound, &oldmain->sounds) {
    sound->packedfile = newpackedadr(fd, sound->packedfile);
  }

  LISTBASE_FOREACH (Library *, lib, &oldmain->libraries) {
    lib->packedfile = newpackedadr(fd, lib->packedfile);
  }

  LISTBASE_FOREACH (Volume *, volume, &oldmain->volumes) {
    volume->packedfile = newpackedadr(fd, volume->packedfile);
  }
}

void blo_add_library_pointer_map(ListBase *old_mainlist, FileData *fd)
{
  ListBase *lbarray[INDEX_ID_MAX];

  LISTBASE_FOREACH (Main *, ptr, old_mainlist) {
    int i = set_listbasepointers(ptr, lbarray);
    while (i--) {
      LISTBASE_FOREACH (ID *, id, lbarray[i]) {
        oldnewmap_lib_insert(fd, id, id, GS(id->name));
      }
    }
  }

  fd->old_mainlist = old_mainlist;
}

void blo_make_old_idmap_from_main(FileData *fd, Main *bmain)
{
  if (fd->old_idmap != NULL) {
    BKE_main_idmap_destroy(fd->old_idmap);
  }
  fd->old_idmap = BKE_main_idmap_create(bmain, false, NULL, MAIN_IDMAP_TYPE_UUID);
}

typedef struct BLOCacheStorage {
  GHash *cache_map;
  MemArena *memarena;
} BLOCacheStorage;

typedef struct BLOCacheStorageValue {
  void *cache_v;
  uint new_usage_count;
} BLOCacheStorageValue;

/** Register a cache data entry to be preserved when reading some undo memfile. */
static void blo_cache_storage_entry_register(
    ID *id, const IDCacheKey *key, void **cache_p, uint UNUSED(flags), void *cache_storage_v)
{
  BLI_assert(key->id_session_uuid == id->session_uuid);
  UNUSED_VARS_NDEBUG(id);

  BLOCacheStorage *cache_storage = cache_storage_v;
  BLI_assert(!BLI_ghash_haskey(cache_storage->cache_map, key));

  IDCacheKey *storage_key = BLI_memarena_alloc(cache_storage->memarena, sizeof(*storage_key));
  *storage_key = *key;
  BLOCacheStorageValue *storage_value = BLI_memarena_alloc(cache_storage->memarena,
                                                           sizeof(*storage_value));
  storage_value->cache_v = *cache_p;
  storage_value->new_usage_count = 0;
  BLI_ghash_insert(cache_storage->cache_map, storage_key, storage_value);
}

/** Restore a cache data entry from old ID into new one, when reading some undo memfile. */
static void blo_cache_storage_entry_restore_in_new(
    ID *UNUSED(id), const IDCacheKey *key, void **cache_p, uint flags, void *cache_storage_v)
{
  BLOCacheStorage *cache_storage = cache_storage_v;

  if (cache_storage == NULL) {
    /* In non-undo case, only clear the pointer if it is a purely runtime one.
     * If it may be stored in a persistent way in the .blend file, direct_link code is responsible
     * to properly deal with it. */
    if ((flags & IDTYPE_CACHE_CB_FLAGS_PERSISTENT) == 0) {
      *cache_p = NULL;
    }
    return;
  }

  BLOCacheStorageValue *storage_value = BLI_ghash_lookup(cache_storage->cache_map, key);
  if (storage_value == NULL) {
    *cache_p = NULL;
    return;
  }
  storage_value->new_usage_count++;
  *cache_p = storage_value->cache_v;
}

/** Clear as needed a cache data entry from old ID, when reading some undo memfile. */
static void blo_cache_storage_entry_clear_in_old(ID *UNUSED(id),
                                                 const IDCacheKey *key,
                                                 void **cache_p,
                                                 uint UNUSED(flags),
                                                 void *cache_storage_v)
{
  BLOCacheStorage *cache_storage = cache_storage_v;

  BLOCacheStorageValue *storage_value = BLI_ghash_lookup(cache_storage->cache_map, key);
  if (storage_value == NULL) {
    *cache_p = NULL;
    return;
  }
  /* If that cache has been restored into some new ID, we want to remove it from old one, otherwise
   * keep it there so that it gets properly freed together with its ID. */
  if (storage_value->new_usage_count != 0) {
    *cache_p = NULL;
  }
  else {
    BLI_assert(*cache_p == storage_value->cache_v);
  }
}

void blo_cache_storage_init(FileData *fd, Main *bmain)
{
  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    BLI_assert(fd->cache_storage == NULL);
    fd->cache_storage = MEM_mallocN(sizeof(*fd->cache_storage), __func__);
    fd->cache_storage->memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    fd->cache_storage->cache_map = BLI_ghash_new(
        BKE_idtype_cache_key_hash, BKE_idtype_cache_key_cmp, __func__);

    ListBase *lb;
    FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
      ID *id = lb->first;
      if (id == NULL) {
        continue;
      }

      const IDTypeInfo *type_info = BKE_idtype_get_info_from_id(id);
      if (type_info->foreach_cache == NULL) {
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
    fd->cache_storage = NULL;
  }
}

void blo_cache_storage_old_bmain_clear(FileData *fd, Main *bmain_old)
{
  if (fd->cache_storage != NULL) {
    ListBase *lb;
    FOREACH_MAIN_LISTBASE_BEGIN (bmain_old, lb) {
      ID *id = lb->first;
      if (id == NULL) {
        continue;
      }

      const IDTypeInfo *type_info = BKE_idtype_get_info_from_id(id);
      if (type_info->foreach_cache == NULL) {
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
  if (fd->cache_storage != NULL) {
    BLI_ghash_free(fd->cache_storage->cache_map, NULL, NULL);
    BLI_memarena_free(fd->cache_storage->memarena);
    MEM_freeN(fd->cache_storage);
    fd->cache_storage = NULL;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DNA Struct Loading
 * \{ */

static void switch_endian_structs(const struct SDNA *filesdna, BHead *bhead)
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
  void *temp = NULL;

  if (bh->len) {
#ifdef USE_BHEAD_READ_ON_DEMAND
    BHead *bh_orig = bh;
#endif

    /* switch is based on file dna */
    if (bh->SDNAnr && (fd->flags & FD_FLAGS_SWITCH_ENDIAN)) {
#ifdef USE_BHEAD_READ_ON_DEMAND
      if (BHEADN_FROM_BHEAD(bh)->has_data == false) {
        bh = blo_bhead_read_full(fd, bh);
        if (UNLIKELY(bh == NULL)) {
          fd->flags &= ~FD_FLAGS_FILE_OK;
          return NULL;
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
          if (UNLIKELY(bh == NULL)) {
            fd->flags &= ~FD_FLAGS_FILE_OK;
            return NULL;
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
            temp = NULL;
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
  return (bhead->len) ? (const void *)(bhead + 1) : NULL;
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

  ln = lb->first;
  prev = NULL;
  while (ln) {
    poin = newdataadr(fd, ln->next);
    if (ln->next) {
      oldnewmap_insert(fd->globmap, ln->next, poin, 0);
    }
    ln->next = poin;
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

static void lib_link_id(BlendLibReader *reader, ID *id);

static void lib_link_id_embedded_id(BlendLibReader *reader, ID *id)
{

  /* Handle 'private IDs'. */
  bNodeTree *nodetree = ntreeFromID(id);
  if (nodetree != NULL) {
    lib_link_id(reader, &nodetree->id);
    ntreeBlendReadLib(reader, nodetree);
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != NULL) {
      lib_link_id(reader, &scene->master_collection->id);
      BKE_collection_blend_read_lib(reader, scene->master_collection);
    }
  }
}

static void lib_link_id(BlendLibReader *reader, ID *id)
{
  /* NOTE: WM IDProperties are never written to file, hence they should always be NULL here. */
  BLI_assert((GS(id->name) != ID_WM) || id->properties == NULL);
  IDP_BlendReadLib(reader, id->lib, id->properties);

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt != NULL) {
    BKE_animdata_blend_read_lib(reader, id, adt);
  }

  if (id->override_library) {
    BLO_read_id_address(reader, id->lib, &id->override_library->reference);
    BLO_read_id_address(reader, id->lib, &id->override_library->storage);
    BLO_read_id_address(reader, id->lib, &id->override_library->hierarchy_root);
  }

  lib_link_id_embedded_id(reader, id);
}

static void direct_link_id_override_property_operation_cb(BlendDataReader *reader, void *data)
{
  IDOverrideLibraryPropertyOperation *opop = data;

  BLO_read_data_address(reader, &opop->subitem_reference_name);
  BLO_read_data_address(reader, &opop->subitem_local_name);

  opop->tag = 0; /* Runtime only. */
}

static void direct_link_id_override_property_cb(BlendDataReader *reader, void *data)
{
  IDOverrideLibraryProperty *op = data;

  BLO_read_data_address(reader, &op->rna_path);

  op->tag = 0; /* Runtime only. */

  BLO_read_list_cb(reader, &op->operations, direct_link_id_override_property_operation_cb);
}

static void direct_link_id_common(
    BlendDataReader *reader, Library *current_library, ID *id, ID *id_old, const int tag);

static void direct_link_id_embedded_id(BlendDataReader *reader,
                                       Library *current_library,
                                       ID *id,
                                       ID *id_old)
{
  /* Handle 'private IDs'. */
  bNodeTree **nodetree = BKE_ntree_ptr_from_id(id);
  if (nodetree != NULL && *nodetree != NULL) {
    BLO_read_data_address(reader, nodetree);
    direct_link_id_common(reader,
                          current_library,
                          (ID *)*nodetree,
                          id_old != NULL ? (ID *)ntreeFromID(id_old) : NULL,
                          0);
    ntreeBlendReadData(reader, *nodetree);
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != NULL) {
      BLO_read_data_address(reader, &scene->master_collection);
      direct_link_id_common(reader,
                            current_library,
                            &scene->master_collection->id,
                            id_old != NULL ? &((Scene *)id_old)->master_collection->id : NULL,
                            0);
      BKE_collection_blend_read_data(reader, scene->master_collection);
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

  if (id_current == NULL) {
    /* ID does not currently exist in the database, so also will not exist in
     * the dependency graphs. That means it will be newly created and as a
     * result also fully re-evaluated regardless of the recalc flag set here. */
    recalc |= ID_RECALC_ALL;
  }
  else {
    /* If the contents datablock changed, the depsgraph needs to copy the
     * datablock again to ensure it matches the original datablock. */
    if (!is_identical) {
      recalc |= ID_RECALC_COPY_ON_WRITE;
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
    BlendDataReader *reader, Library *current_library, ID *id, ID *id_old, const int tag)
{
  if (!BLO_read_data_is_undo(reader)) {
    /* When actually reading a file, we do want to reset/re-generate session uuids.
     * In undo case, we want to re-use existing ones. */
    id->session_uuid = MAIN_ID_SESSION_UUID_UNSET;
  }

  if ((tag & LIB_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uuid_ensure(id);
  }

  id->lib = current_library;
  id->us = ID_FAKE_USERS(id);
  id->icon_id = 0;
  id->newid = NULL; /* Needed because .blend may have been saved with crap value here... */
  id->orig_id = NULL;
  id->py_instance = NULL;

  /* Initialize with provided tag. */
  id->tag = tag;

  if (ID_IS_LINKED(id)) {
    id->library_weak_reference = NULL;
  }
  else {
    BLO_read_data_address(reader, &id->library_weak_reference);
  }

  if (tag & LIB_TAG_ID_LINK_PLACEHOLDER) {
    /* For placeholder we only need to set the tag and properly initialize generic ID fields above,
     * no further data to read. */
    return;
  }

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
    /* Work around file corruption on writing, see T86853. */
    if (id->override_library != NULL) {
      BLO_read_list_cb(
          reader, &id->override_library->properties, direct_link_id_override_property_cb);
      id->override_library->runtime = NULL;
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
 * A version of #BKE_scene_validate_setscene with special checks for linked libs.
 */
static bool scene_validate_setscene__liblink(Scene *sce, const int totscene)
{
  Scene *sce_iter;
  int a;

  if (sce->set == NULL) {
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
      sce->set = NULL;
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
/** \name Read ID: Screen
 * \{ */

/* how to handle user count on pointer restore */
typedef enum ePointerUserMode {
  USER_IGNORE = 0, /* ignore user count */
  USER_REAL = 1,   /* ensure at least one real user (fake user ignored) */
} ePointerUserMode;

static void restore_pointer_user(ID *id, ID *newid, ePointerUserMode user)
{
  BLI_assert(STREQ(newid->name + 2, id->name + 2));
  BLI_assert(newid->lib == id->lib);
  UNUSED_VARS_NDEBUG(id);

  if (user == USER_REAL) {
    id_us_ensure_real(newid);
  }
}

#ifndef USE_GHASH_RESTORE_POINTER
/**
 * A version of #restore_pointer_by_name that performs a full search (slow!).
 * Use only for limited lookups, when the overhead of
 * creating a #IDNameLib_Map for a single lookup isn't worthwhile.
 */
static void *restore_pointer_by_name_main(Main *mainp, ID *id, ePointerUserMode user)
{
  if (id) {
    ListBase *lb = which_libbase(mainp, GS(id->name));
    if (lb) { /* there's still risk of checking corrupt mem (freed Ids in oops) */
      ID *idn = lb->first;
      for (; idn; idn = idn->next) {
        if (STREQ(idn->name + 2, id->name + 2)) {
          if (idn->lib == id->lib) {
            restore_pointer_user(id, idn, user);
            break;
          }
        }
      }
      return idn;
    }
  }
  return NULL;
}
#endif

/**
 * Only for undo files, or to restore a screen after reading without UI...
 *
 * \param user:
 * - USER_IGNORE: no user-count change.
 * - USER_REAL: ensure a real user (even if a fake one is set).
 * \param id_map: lookup table, use when performing many lookups.
 * this could be made an optional argument (falling back to a full lookup),
 * however at the moment it's always available.
 */
static void *restore_pointer_by_name(struct IDNameLib_Map *id_map, ID *id, ePointerUserMode user)
{
#ifdef USE_GHASH_RESTORE_POINTER
  if (id) {
    /* use fast lookup when available */
    ID *idn = BKE_main_idmap_lookup_id(id_map, id);
    if (idn) {
      restore_pointer_user(id, idn, user);
    }
    return idn;
  }
  return NULL;
#else
  Main *mainp = BKE_main_idmap_main_get(id_map);
  return restore_pointer_by_name_main(mainp, id, user);
#endif
}

static void lib_link_seq_clipboard_pt_restore(ID *id, struct IDNameLib_Map *id_map)
{
  if (id) {
    /* clipboard must ensure this */
    BLI_assert(id->newid != NULL);
    id->newid = restore_pointer_by_name(id_map, id->newid, USER_REAL);
  }
}
static bool lib_link_seq_clipboard_cb(Sequence *seq, void *arg_pt)
{
  struct IDNameLib_Map *id_map = arg_pt;

  lib_link_seq_clipboard_pt_restore((ID *)seq->scene, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->scene_camera, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->clip, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->mask, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->sound, id_map);
  return true;
}

static void lib_link_clipboard_restore(struct IDNameLib_Map *id_map)
{
  /* update IDs stored in sequencer clipboard */
  SEQ_for_each_callback(&seqbase_clipboard, lib_link_seq_clipboard_cb, id_map);
}

static int lib_link_main_data_restore_cb(LibraryIDLinkCallbackData *cb_data)
{
  const int cb_flag = cb_data->cb_flag;
  ID **id_pointer = cb_data->id_pointer;
  if (cb_flag & IDWALK_CB_EMBEDDED || *id_pointer == NULL) {
    return IDWALK_RET_NOP;
  }

  /* Special ugly case here, thanks again for those non-IDs IDs... */
  /* We probably need to add more cases here (hint: nodetrees),
   * but will wait for changes from D5559 to get in first. */
  if (GS((*id_pointer)->name) == ID_GR) {
    Collection *collection = (Collection *)*id_pointer;
    if (collection->flag & COLLECTION_IS_MASTER) {
      /* We should never reach that point anymore, since master collection private ID should be
       * properly tagged with IDWALK_CB_EMBEDDED. */
      BLI_assert_unreachable();
      return IDWALK_RET_NOP;
    }
  }

  struct IDNameLib_Map *id_map = cb_data->user_data;

  /* NOTE: Handling of usercount here is really bad, defining its own system...
   * Will have to be refactored at some point, but that is not top priority task for now.
   * And all user-counts are properly recomputed at the end of the undo management code anyway. */
  *id_pointer = restore_pointer_by_name(
      id_map, *id_pointer, (cb_flag & IDWALK_CB_USER_ONE) ? USER_REAL : USER_IGNORE);

  return IDWALK_RET_NOP;
}

static void lib_link_main_data_restore(struct IDNameLib_Map *id_map, Main *newmain)
{
  ID *id;
  FOREACH_MAIN_ID_BEGIN (newmain, id) {
    BKE_library_foreach_ID_link(newmain, id, lib_link_main_data_restore_cb, id_map, IDWALK_NOP);
  }
  FOREACH_MAIN_ID_END;
}

static void lib_link_wm_xr_data_restore(struct IDNameLib_Map *id_map, wmXrData *xr_data)
{
  xr_data->session_settings.base_pose_object = restore_pointer_by_name(
      id_map, (ID *)xr_data->session_settings.base_pose_object, USER_REAL);
}

static void lib_link_window_scene_data_restore(wmWindow *win, Scene *scene, ViewLayer *view_layer)
{
  bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;

        if (v3d->camera == NULL || v3d->scenelock) {
          v3d->camera = scene->camera;
        }

        if (v3d->localvd) {
          Base *base = NULL;

          v3d->localvd->camera = scene->camera;

          /* Local-view can become invalid during undo/redo steps,
           * so we exit it when no could be found. */
          for (base = view_layer->object_bases.first; base; base = base->next) {
            if (base->local_view_bits & v3d->local_view_uuid) {
              break;
            }
          }
          if (base == NULL) {
            MEM_freeN(v3d->localvd);
            v3d->localvd = NULL;
            v3d->local_view_uuid = 0;

            /* Region-base storage is different depending if the space is active. */
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                RegionView3D *rv3d = region->regiondata;
                if (rv3d->localvd) {
                  MEM_freeN(rv3d->localvd);
                  rv3d->localvd = NULL;
                }
              }
            }
          }
        }
      }
    }
  }
}

static void lib_link_workspace_layout_restore(struct IDNameLib_Map *id_map,
                                              Main *newmain,
                                              WorkSpaceLayout *layout)
{
  bScreen *screen = BKE_workspace_layout_screen_get(layout);

  /* avoid conflicts with 2.8x branch */
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = (View3D *)sl;

          v3d->camera = restore_pointer_by_name(id_map, (ID *)v3d->camera, USER_REAL);
          v3d->ob_center = restore_pointer_by_name(id_map, (ID *)v3d->ob_center, USER_REAL);
        }
        else if (sl->spacetype == SPACE_GRAPH) {
          SpaceGraph *sipo = (SpaceGraph *)sl;
          bDopeSheet *ads = sipo->ads;

          if (ads) {
            ads->source = restore_pointer_by_name(id_map, (ID *)ads->source, USER_REAL);

            if (ads->filter_grp) {
              ads->filter_grp = restore_pointer_by_name(
                  id_map, (ID *)ads->filter_grp, USER_IGNORE);
            }
          }

          /* force recalc of list of channels (i.e. includes calculating F-Curve colors)
           * thus preventing the "black curves" problem post-undo
           */
          sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR;
        }
        else if (sl->spacetype == SPACE_PROPERTIES) {
          SpaceProperties *sbuts = (SpaceProperties *)sl;
          sbuts->pinid = restore_pointer_by_name(id_map, sbuts->pinid, USER_IGNORE);
          if (sbuts->pinid == NULL) {
            sbuts->flag &= ~SB_PIN_CONTEXT;
          }

          /* TODO: restore path pointers: T40046
           * (complicated because this contains data pointers too, not just ID). */
          MEM_SAFE_FREE(sbuts->path);
        }
        else if (sl->spacetype == SPACE_FILE) {
          SpaceFile *sfile = (SpaceFile *)sl;
          sfile->op = NULL;
          sfile->tags = FILE_TAG_REBUILD_MAIN_FILES;
        }
        else if (sl->spacetype == SPACE_ACTION) {
          SpaceAction *saction = (SpaceAction *)sl;

          saction->action = restore_pointer_by_name(id_map, (ID *)saction->action, USER_REAL);
          saction->ads.source = restore_pointer_by_name(
              id_map, (ID *)saction->ads.source, USER_REAL);

          if (saction->ads.filter_grp) {
            saction->ads.filter_grp = restore_pointer_by_name(
                id_map, (ID *)saction->ads.filter_grp, USER_IGNORE);
          }

          /* force recalc of list of channels, potentially updating the active action
           * while we're at it (as it can only be updated that way) T28962.
           */
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        }
        else if (sl->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = (SpaceImage *)sl;

          sima->image = restore_pointer_by_name(id_map, (ID *)sima->image, USER_REAL);

          /* this will be freed, not worth attempting to find same scene,
           * since it gets initialized later */
          sima->iuser.scene = NULL;

#if 0
          /* Those are allocated and freed by space code, no need to handle them here. */
          MEM_SAFE_FREE(sima->scopes.waveform_1);
          MEM_SAFE_FREE(sima->scopes.waveform_2);
          MEM_SAFE_FREE(sima->scopes.waveform_3);
          MEM_SAFE_FREE(sima->scopes.vecscope);
#endif
          sima->scopes.ok = 0;

          /* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
           * so assume that here we're doing for undo only...
           */
          sima->gpd = restore_pointer_by_name(id_map, (ID *)sima->gpd, USER_REAL);
          sima->mask_info.mask = restore_pointer_by_name(
              id_map, (ID *)sima->mask_info.mask, USER_REAL);
        }
        else if (sl->spacetype == SPACE_SEQ) {
          SpaceSeq *sseq = (SpaceSeq *)sl;

          /* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
           * so assume that here we're doing for undo only...
           */
          sseq->gpd = restore_pointer_by_name(id_map, (ID *)sseq->gpd, USER_REAL);
        }
        else if (sl->spacetype == SPACE_NLA) {
          SpaceNla *snla = (SpaceNla *)sl;
          bDopeSheet *ads = snla->ads;

          if (ads) {
            ads->source = restore_pointer_by_name(id_map, (ID *)ads->source, USER_REAL);

            if (ads->filter_grp) {
              ads->filter_grp = restore_pointer_by_name(
                  id_map, (ID *)ads->filter_grp, USER_IGNORE);
            }
          }
        }
        else if (sl->spacetype == SPACE_TEXT) {
          SpaceText *st = (SpaceText *)sl;

          st->text = restore_pointer_by_name(id_map, (ID *)st->text, USER_IGNORE);
          if (st->text == NULL) {
            st->text = newmain->texts.first;
          }
        }
        else if (sl->spacetype == SPACE_SCRIPT) {
          SpaceScript *scpt = (SpaceScript *)sl;

          scpt->script = restore_pointer_by_name(id_map, (ID *)scpt->script, USER_REAL);

          // screen->script = NULL; /* 2.45 set to null, better re-run the script. */
          if (scpt->script) {
            SCRIPT_SET_NULL(scpt->script);
          }
        }
        else if (sl->spacetype == SPACE_OUTLINER) {
          SpaceOutliner *space_outliner = (SpaceOutliner *)sl;

          space_outliner->search_tse.id = restore_pointer_by_name(
              id_map, space_outliner->search_tse.id, USER_IGNORE);

          if (space_outliner->treestore) {
            TreeStoreElem *tselem;
            BLI_mempool_iter iter;

            BLI_mempool_iternew(space_outliner->treestore, &iter);
            while ((tselem = BLI_mempool_iterstep(&iter))) {
              /* Do not try to restore pointers to drivers/sequence/etc.,
               * can crash in undo case! */
              if (TSE_IS_REAL_ID(tselem)) {
                tselem->id = restore_pointer_by_name(id_map, tselem->id, USER_IGNORE);
              }
              else {
                tselem->id = NULL;
              }
            }
            /* rebuild hash table, because it depends on ids too */
            space_outliner->storeflag |= SO_TREESTORE_REBUILD;
          }
        }
        else if (sl->spacetype == SPACE_NODE) {
          SpaceNode *snode = (SpaceNode *)sl;
          bNodeTreePath *path, *path_next;
          bNodeTree *ntree;

          /* node tree can be stored locally in id too, link this first */
          snode->id = restore_pointer_by_name(id_map, snode->id, USER_REAL);
          snode->from = restore_pointer_by_name(id_map, snode->from, USER_IGNORE);

          ntree = snode->id ? ntreeFromID(snode->id) : NULL;
          snode->nodetree = ntree ?
                                ntree :
                                restore_pointer_by_name(id_map, (ID *)snode->nodetree, USER_REAL);

          for (path = snode->treepath.first; path; path = path->next) {
            if (path == snode->treepath.first) {
              /* first nodetree in path is same as snode->nodetree */
              path->nodetree = snode->nodetree;
            }
            else {
              path->nodetree = restore_pointer_by_name(id_map, (ID *)path->nodetree, USER_REAL);
            }

            if (!path->nodetree) {
              break;
            }
          }

          /* remaining path entries are invalid, remove */
          for (; path; path = path_next) {
            path_next = path->next;

            BLI_remlink(&snode->treepath, path);
            MEM_freeN(path);
          }

          /* edittree is just the last in the path,
           * set this directly since the path may have been shortened above */
          if (snode->treepath.last) {
            path = snode->treepath.last;
            snode->edittree = path->nodetree;
          }
          else {
            snode->edittree = NULL;
          }
        }
        else if (sl->spacetype == SPACE_CLIP) {
          SpaceClip *sclip = (SpaceClip *)sl;

          sclip->clip = restore_pointer_by_name(id_map, (ID *)sclip->clip, USER_REAL);
          sclip->mask_info.mask = restore_pointer_by_name(
              id_map, (ID *)sclip->mask_info.mask, USER_REAL);

          sclip->scopes.ok = 0;
        }
        else if (sl->spacetype == SPACE_SPREADSHEET) {
          SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;

          LISTBASE_FOREACH (SpreadsheetContext *, context, &sspreadsheet->context_path) {
            if (context->type == SPREADSHEET_CONTEXT_OBJECT) {
              SpreadsheetContextObject *object_context = (SpreadsheetContextObject *)context;
              object_context->object = restore_pointer_by_name(
                  id_map, (ID *)object_context->object, USER_IGNORE);
            }
          }
        }
      }
    }
  }
}

void blo_lib_link_restore(Main *oldmain,
                          Main *newmain,
                          wmWindowManager *curwm,
                          Scene *curscene,
                          ViewLayer *cur_view_layer)
{
  struct IDNameLib_Map *id_map = BKE_main_idmap_create(
      newmain, true, oldmain, MAIN_IDMAP_TYPE_NAME);

  LISTBASE_FOREACH (WorkSpace *, workspace, &newmain->workspaces) {
    LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
      lib_link_workspace_layout_restore(id_map, newmain, layout);
    }
    workspace->pin_scene = restore_pointer_by_name(
        id_map, (ID *)workspace->pin_scene, USER_IGNORE);
  }

  LISTBASE_FOREACH (wmWindow *, win, &curwm->windows) {
    WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
    ID *workspace_id = (ID *)workspace;
    Scene *oldscene = win->scene;

    workspace = restore_pointer_by_name(id_map, workspace_id, USER_REAL);
    BKE_workspace_active_set(win->workspace_hook, workspace);
    win->scene = restore_pointer_by_name(id_map, (ID *)win->scene, USER_REAL);
    if (win->scene == NULL) {
      win->scene = curscene;
    }
    win->unpinned_scene = restore_pointer_by_name(id_map, (ID *)win->unpinned_scene, USER_IGNORE);
    if (BKE_view_layer_find(win->scene, win->view_layer_name) == NULL) {
      STRNCPY(win->view_layer_name, cur_view_layer->name);
    }
    BKE_workspace_active_set(win->workspace_hook, workspace);

    /* keep cursor location through undo */
    memcpy(&win->scene->cursor, &oldscene->cursor, sizeof(win->scene->cursor));

    /* NOTE: even though that function seems to redo part of what is done by
     * `lib_link_workspace_layout_restore()` above, it seems to have a slightly different scope:
     * while the former updates the whole UI pointers from Main db (going over all layouts of
     * all workspaces), that one only focuses one current active screen, takes care of
     * potential local view, and needs window's scene pointer to be final... */
    lib_link_window_scene_data_restore(win, win->scene, cur_view_layer);

    BLI_assert(win->screen == NULL);
  }

  lib_link_wm_xr_data_restore(id_map, &curwm->xr);

  /* Restore all ID pointers in Main database itself
   * (especially IDProperties might point to some word-space of other 'weirdly unchanged' ID
   * pointers, see T69146).
   * Note that this will re-apply again a few pointers in workspaces or so,
   * but since we are remapping final ones already set above,
   * that is just some minor harmless double-processing. */
  lib_link_main_data_restore(id_map, newmain);

  /* update IDs stored in all possible clipboards */
  lib_link_clipboard_restore(id_map);

  BKE_main_idmap_destroy(id_map);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Library
 * \{ */

static void direct_link_library(FileData *fd, Library *lib, Main *main)
{
  Main *newmain;

  /* check if the library was already read */
  for (newmain = fd->mainlist->first; newmain; newmain = newmain->next) {
    if (newmain->curlib) {
      if (BLI_path_cmp(newmain->curlib->filepath_abs, lib->filepath_abs) == 0) {
        BLO_reportf_wrap(fd->reports,
                         RPT_WARNING,
                         TIP_("Library '%s', '%s' had multiple instances, save and reload!"),
                         lib->filepath,
                         lib->filepath_abs);

        change_link_placeholder_to_real_ID_pointer(fd->mainlist, fd, lib, newmain->curlib);
        /*              change_link_placeholder_to_real_ID_pointer_fd(fd, lib, newmain->curlib); */

        BLI_remlink(&main->libraries, lib);
        MEM_freeN(lib);

        /* Now, since Blender always expect **latest** Main pointer from fd->mainlist
         * to be the active library Main pointer,
         * where to add all non-library data-blocks found in file next, we have to switch that
         * 'dupli' found Main to latest position in the list!
         * Otherwise, you get weird disappearing linked data on a rather inconsistent basis.
         * See also T53977 for reproducible case. */
        BLI_remlink(fd->mainlist, newmain);
        BLI_addtail(fd->mainlist, newmain);

        return;
      }
    }
  }

  /* Make sure we have full path in lib->filepath_abs */
  BLI_strncpy(lib->filepath_abs, lib->filepath, sizeof(lib->filepath));
  BLI_path_normalize(fd->relabase, lib->filepath_abs);

  //  printf("direct_link_library: filepath %s\n", lib->filepath);
  //  printf("direct_link_library: filepath_abs %s\n", lib->filepath_abs);

  BlendDataReader reader = {fd};
  BKE_packedfile_blend_read(&reader, &lib->packedfile);

  /* new main */
  newmain = BKE_main_new();
  BLI_addtail(fd->mainlist, newmain);
  newmain->curlib = lib;

  lib->parent = NULL;

  id_us_ensure_real(&lib->id);
}

static void lib_link_library(BlendLibReader *UNUSED(reader), Library *UNUSED(lib))
{
}

/* Always call this once you have loaded new library data to set the relative paths correctly
 * in relation to the blend file. */
static void fix_relpaths_library(const char *basepath, Main *main)
{
  /* #BLO_read_from_memory uses a blank file-path. */
  if (basepath == NULL || basepath[0] == '\0') {
    LISTBASE_FOREACH (Library *, lib, &main->libraries) {
      /* when loading a linked lib into a file which has not been saved,
       * there is nothing we can be relative to, so instead we need to make
       * it absolute. This can happen when appending an object with a relative
       * link into an unsaved blend file. See T27405.
       * The remap relative option will make it relative again on save - campbell */
      if (BLI_path_is_rel(lib->filepath)) {
        BLI_strncpy(lib->filepath, lib->filepath_abs, sizeof(lib->filepath));
      }
    }
  }
  else {
    LISTBASE_FOREACH (Library *, lib, &main->libraries) {
      /* Libraries store both relative and abs paths, recreate relative paths,
       * relative to the blend file since indirectly linked libs will be
       * relative to their direct linked library. */
      if (BLI_path_is_rel(lib->filepath)) { /* if this is relative to begin with? */
        BLI_strncpy(lib->filepath, lib->filepath_abs, sizeof(lib->filepath));
        BLI_path_rel(lib->filepath, basepath);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Library Data Block
 * \{ */

static ID *create_placeholder(Main *mainvar, const short idcode, const char *idname, const int tag)
{
  ListBase *lb = which_libbase(mainvar, idcode);
  ID *ph_id = BKE_libblock_alloc_notest(idcode);

  *((short *)ph_id->name) = idcode;
  BLI_strncpy(ph_id->name + 2, idname, sizeof(ph_id->name) - 2);
  BKE_libblock_init_empty(ph_id);
  ph_id->lib = mainvar->curlib;
  ph_id->tag = tag | LIB_TAG_MISSING;
  ph_id->us = ID_FAKE_USERS(ph_id);
  ph_id->icon_id = 0;

  BLI_addtail(lb, ph_id);
  id_sort_by_name(lb, ph_id, NULL);

  if (mainvar->id_map != NULL) {
    BKE_main_idmap_insert_id(mainvar->id_map, ph_id);
  }

  if ((tag & LIB_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uuid_ensure(ph_id);
  }

  return ph_id;
}

static void placeholders_ensure_valid(Main *bmain)
{
  /* Placeholder ObData IDs won't have any material, we have to update their objects for that,
   * otherwise the inconsistency between both will lead to crashes (especially in Eevee?). */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    ID *obdata = ob->data;
    if (obdata != NULL && obdata->tag & LIB_TAG_MISSING) {
      BKE_object_materials_test(bmain, ob, obdata);
    }
  }
}

static const char *dataname(short id_code)
{
  switch ((ID_Type)id_code) {
    case ID_OB:
      return "Data from OB";
    case ID_ME:
      return "Data from ME";
    case ID_IP:
      return "Data from IP";
    case ID_SCE:
      return "Data from SCE";
    case ID_MA:
      return "Data from MA";
    case ID_TE:
      return "Data from TE";
    case ID_CU_LEGACY:
      return "Data from CU";
    case ID_GR:
      return "Data from GR";
    case ID_AR:
      return "Data from AR";
    case ID_AC:
      return "Data from AC";
    case ID_LI:
      return "Data from LI";
    case ID_MB:
      return "Data from MB";
    case ID_IM:
      return "Data from IM";
    case ID_LT:
      return "Data from LT";
    case ID_LA:
      return "Data from LA";
    case ID_CA:
      return "Data from CA";
    case ID_KE:
      return "Data from KE";
    case ID_WO:
      return "Data from WO";
    case ID_SCR:
      return "Data from SCR";
    case ID_VF:
      return "Data from VF";
    case ID_TXT:
      return "Data from TXT";
    case ID_SPK:
      return "Data from SPK";
    case ID_LP:
      return "Data from LP";
    case ID_SO:
      return "Data from SO";
    case ID_NT:
      return "Data from NT";
    case ID_BR:
      return "Data from BR";
    case ID_PA:
      return "Data from PA";
    case ID_PAL:
      return "Data from PAL";
    case ID_PC:
      return "Data from PCRV";
    case ID_GD:
      return "Data from GD";
    case ID_WM:
      return "Data from WM";
    case ID_MC:
      return "Data from MC";
    case ID_MSK:
      return "Data from MSK";
    case ID_LS:
      return "Data from LS";
    case ID_CF:
      return "Data from CF";
    case ID_WS:
      return "Data from WS";
    case ID_CV:
      return "Data from HA";
    case ID_PT:
      return "Data from PT";
    case ID_VO:
      return "Data from VO";
    case ID_SIM:
      return "Data from SIM";
  }
  return "Data from Lib Block";
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
  if (id_type->blend_read_data != NULL) {
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
  if (id_type->foreach_cache != NULL) {
    BKE_idtype_id_foreach_cache(
        id, blo_cache_storage_entry_restore_in_new, reader.fd->cache_storage);
  }

  return success;
}

/* Read all data associated with a datablock into datamap. */
static BHead *read_data_into_datamap(FileData *fd, BHead *bhead, const char *allocname)
{
  bhead = blo_bhead_next(fd, bhead);

  while (bhead && bhead->code == DATA) {
    /* The code below is useful for debugging leaks in data read from the blend file.
     * Without this the messages only tell us what ID-type the memory came from,
     * eg: `Data from OB len 64`, see #dataname.
     * With the code below we get the struct-name to help tracking down the leak.
     * This is kept disabled as the #malloc for the text always leaks memory. */
#if 0
    {
      const short *sp = fd->filesdna->structs[bhead->SDNAnr];
      allocname = fd->filesdna->types[sp[0]];
      size_t allocname_size = strlen(allocname) + 1;
      char *allocname_buf = malloc(allocname_size);
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

  while (bhead && bhead->code == DATA) {
    if (bhead->len && !BHEADN_FROM_BHEAD(bhead)->is_memchunk_identical) {
      return false;
    }

    bhead = blo_bhead_next(fd, bhead);
  }

  return true;
}

/* For undo, restore matching library datablock from the old main. */
static bool read_libblock_undo_restore_library(FileData *fd, Main *main, const ID *id)
{
  /* In undo case, most libs and linked data should be kept as is from previous state
   * (see BLO_read_from_memfile).
   * However, some needed by the snapshot being read may have been removed in previous one,
   * and would go missing.
   * This leads e.g. to disappearing objects in some undo/redo case, see T34446.
   * That means we have to carefully check whether current lib or
   * libdata already exits in old main, if it does we merely copy it over into new main area,
   * otherwise we have to do a full read of that bhead... */
  CLOG_INFO(&LOG_UNDO, 2, "UNDO: restore library %s", id->name);

  Main *libmain = fd->old_mainlist->first;
  /* Skip oldmain itself... */
  for (libmain = libmain->next; libmain; libmain = libmain->next) {
    if (libmain->curlib && STREQ(id->name, libmain->curlib->id.name)) {
      Main *oldmain = fd->old_mainlist->first;
      CLOG_INFO(&LOG_UNDO,
                2,
                "    compare with %s -> match",
                libmain->curlib ? libmain->curlib->id.name : "<NULL>");
      /* In case of a library, we need to re-add its main to fd->mainlist,
       * because if we have later a missing ID_LINK_PLACEHOLDER,
       * we need to get the correct lib it is linked to!
       * Order is crucial, we cannot bulk-add it in BLO_read_from_memfile()
       * like it used to be. */
      BLI_remlink(fd->old_mainlist, libmain);
      BLI_remlink_safe(&oldmain->libraries, libmain->curlib);
      BLI_addtail(fd->mainlist, libmain);
      BLI_addtail(&main->libraries, libmain->curlib);
      return true;
    }
    CLOG_INFO(&LOG_UNDO,
              2,
              "    compare with %s -> NO match",
              libmain->curlib ? libmain->curlib->id.name : "<NULL>");
  }

  return false;
}

/* For undo, restore existing linked datablock from the old main. */
static bool read_libblock_undo_restore_linked(FileData *fd, Main *main, const ID *id, BHead *bhead)
{
  CLOG_INFO(&LOG_UNDO, 2, "UNDO: restore linked datablock %s", id->name);

  ID *id_old = BKE_libblock_find_name(main, GS(id->name), id->name + 2);
  if (id_old != NULL) {
    CLOG_INFO(&LOG_UNDO,
              2,
              "    from %s (%s): found",
              main->curlib ? main->curlib->id.name : "<NULL>",
              main->curlib ? main->curlib->filepath : "<NULL>");
    /* Even though we found our linked ID, there is no guarantee its address
     * is still the same. */
    if (id_old != bhead->old) {
      oldnewmap_lib_insert(fd, bhead->old, id_old, GS(id_old->name));
    }

    /* No need to do anything else for ID_LINK_PLACEHOLDER, it's assumed
     * already present in its lib's main. */
    return true;
  }

  CLOG_INFO(&LOG_UNDO,
            2,
            "    from %s (%s): NOT found",
            main->curlib ? main->curlib->id.name : "<NULL>",
            main->curlib ? main->curlib->filepath : "<NULL>");
  return false;
}

/* For undo, restore unchanged datablock from old main. */
static void read_libblock_undo_restore_identical(
    FileData *fd, Main *main, const ID *UNUSED(id), ID *id_old, const int tag)
{
  BLI_assert((fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0);
  BLI_assert(id_old != NULL);

  /* Some tags need to be preserved here. */
  id_old->tag = tag | (id_old->tag & LIB_TAG_EXTRAUSER);
  id_old->lib = main->curlib;
  id_old->us = ID_FAKE_USERS(id_old);
  /* Do not reset id->icon_id here, memory allocated for it remains valid. */
  /* Needed because .blend may have been saved with crap value here... */
  id_old->newid = NULL;
  id_old->orig_id = NULL;

  const short idcode = GS(id_old->name);
  Main *old_bmain = fd->old_mainlist->first;
  ListBase *old_lb = which_libbase(old_bmain, idcode);
  ListBase *new_lb = which_libbase(main, idcode);
  BLI_remlink(old_lb, id_old);
  BLI_addtail(new_lb, id_old);

  /* Recalc flags, mostly these just remain as they are. */
  id_old->recalc |= direct_link_id_restore_recalc_exceptions(id_old);
  id_old->recalc_after_undo_push = 0;

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
  BLI_assert(id_old != NULL);

  const short idcode = GS(id->name);

  Main *old_bmain = fd->old_mainlist->first;
  ListBase *old_lb = which_libbase(old_bmain, idcode);
  ListBase *new_lb = which_libbase(main, idcode);
  BLI_remlink(old_lb, id_old);
  BLI_remlink(new_lb, id);

  /* We do not need any remapping from this call here, since no ID pointer is valid in the data
   * currently (they are all pointing to old addresses, and need to go through `lib_link`
   * process). So we can pass NULL for the Main pointer parameter. */
  BKE_lib_id_swap_full(NULL, id, id_old);

  /* Special temporary usage of this pointer, necessary for the `undo_preserve` call after
   * lib-linking to restore some data that should never be affected by undo, e.g. the 3D cursor of
   * #Scene. */
  id_old->orig_id = id;

  BLI_addtail(new_lb, id_old);
  BLI_addtail(old_lb, id);
}

static bool read_libblock_undo_restore(
    FileData *fd, Main *main, BHead *bhead, const int tag, ID **r_id_old)
{
  /* Get pointer to memory of new ID that we will be reading. */
  const ID *id = peek_struct_undo(fd, bhead);
  const short idcode = GS(id->name);

  if (bhead->code == ID_LI) {
    /* Restore library datablock. */
    if (read_libblock_undo_restore_library(fd, main, id)) {
      return true;
    }
  }
  else if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Restore linked datablock. */
    if (read_libblock_undo_restore_linked(fd, main, id, bhead)) {
      return true;
    }
  }
  else if (ELEM(idcode, ID_WM, ID_SCR, ID_WS)) {
    /* Skip reading any UI datablocks, existing ones are kept. We don't
     * support pointers from other datablocks to UI datablocks so those
     * we also don't put UI datablocks in fd->libmap. */
    return true;
  }

  /* Restore local datablocks. */
  ID *id_old = NULL;
  const bool do_partial_undo = (fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0;
  if (do_partial_undo && (bhead->code != ID_LINK_PLACEHOLDER)) {
    /* This code should only ever be reached for local data-blocks. */
    BLI_assert(main->curlib == NULL);

    /* Find the 'current' existing ID we want to reuse instead of the one we
     * would read from the undo memfile. */
    BLI_assert(fd->old_idmap != NULL);
    id_old = BKE_main_idmap_lookup_uuid(fd->old_idmap, id->session_uuid);
  }

  if (id_old != NULL && read_libblock_is_identical(fd, bhead)) {
    /* Local datablock was unchanged, restore from the old main. */
    CLOG_INFO(&LOG_UNDO,
              2,
              "UNDO: read %s (uuid %u) -> keep identical datablock",
              id->name,
              id->session_uuid);

    /* Do not add LIB_TAG_NEW here, this should not be needed/used in undo case anyway (as
     * this is only for do_version-like code), but for sake of consistency, and also because
     * it will tell us which ID is re-used from old Main, and which one is actually new. */
    /* Also do not add LIB_TAG_NEED_LINK, those IDs will never be re-liblinked, hence that tag will
     * never be cleared, leading to critical issue in link/append code. */
    const int id_tag = tag | LIB_TAG_UNDO_OLD_ID_REUSED;
    read_libblock_undo_restore_identical(fd, main, id, id_old, id_tag);

    /* Insert into library map for lookup by newly read datablocks (with pointer value bhead->old).
     * Note that existing datablocks in memory (which pointer value would be id_old) are not
     * remapped anymore, so no need to store this info here. */
    oldnewmap_lib_insert(fd, bhead->old, id_old, bhead->code);

    *r_id_old = id_old;
    return true;
  }
  if (id_old != NULL) {
    /* Local datablock was changed. Restore at the address of the old datablock. */
    CLOG_INFO(&LOG_UNDO,
              2,
              "UNDO: read %s (uuid %u) -> read to old existing address",
              id->name,
              id->session_uuid);
    *r_id_old = id_old;
    return false;
  }

  /* Local datablock does not exist in the undo step, so read from scratch. */
  CLOG_INFO(
      &LOG_UNDO, 2, "UNDO: read %s (uuid %u) -> read at new address", id->name, id->session_uuid);
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
                            const int tag,
                            const bool placeholder_set_indirect_extern,
                            ID **r_id)
{
  /* First attempt to restore existing datablocks for undo.
   * When datablocks are changed but still exist, we restore them at the old
   * address and inherit recalc flags for the dependency graph. */
  ID *id_old = NULL;
  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    if (read_libblock_undo_restore(fd, main, bhead, tag, &id_old)) {
      if (r_id) {
        *r_id = id_old;
      }
      if (main->id_map != NULL) {
        BKE_main_idmap_insert_id(main->id_map, id_old);
      }

      return blo_bhead_next(fd, bhead);
    }
  }

  /* Read libblock struct. */
  ID *id = read_struct(fd, bhead, "lib block");
  if (id == NULL) {
    if (r_id) {
      *r_id = NULL;
    }
    return blo_bhead_next(fd, bhead);
  }

  /* Determine ID type and add to main database list. */
  const short idcode = GS(id->name);
  ListBase *lb = which_libbase(main, idcode);
  if (lb == NULL) {
    /* Unknown ID type. */
    CLOG_WARN(&LOG, "Unknown id code '%c%c'", (idcode & 0xff), (idcode >> 8));
    MEM_freeN(id);
    if (r_id) {
      *r_id = NULL;
    }
    return blo_bhead_next(fd, bhead);
  }

  /* NOTE: id must be added to the list before direct_link_id(), since
   * direct_link_library() may remove it from there in case of duplicates. */
  BLI_addtail(lb, id);

  /* Insert into library map for lookup by newly read datablocks (with pointer value bhead->old).
   * Note that existing datablocks in memory (which pointer value would be id_old) are not remapped
   * remapped anymore, so no need to store this info here. */
  ID *id_target = id_old ? id_old : id;
  oldnewmap_lib_insert(fd, bhead->old, id_target, bhead->code);

  if (r_id) {
    *r_id = id_target;
  }

  /* Set tag for new datablock to indicate lib linking and versioning needs
   * to be done still. */
  int id_tag = tag | LIB_TAG_NEED_LINK | LIB_TAG_NEW;

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

    if (main->id_map != NULL) {
      BKE_main_idmap_insert_id(main->id_map, id);
    }

    return blo_bhead_next(fd, bhead);
  }

  /* Read datablock contents.
   * Use convenient malloc name for debugging and better memory link prints. */
  const char *allocname = dataname(idcode);
  bhead = read_data_into_datamap(fd, bhead, allocname);
  const bool success = direct_link_id(fd, main, id_tag, id, id_old);
  oldnewmap_clear(fd->datamap);

  if (!success) {
    /* XXX This is probably working OK currently given the very limited scope of that flag.
     * However, it is absolutely **not** handled correctly: it is freeing an ID pointer that has
     * been added to the fd->libmap mapping, which in theory could lead to nice crashes...
     * This should be properly solved at some point. */
    BKE_id_free(main, id);
    if (r_id != NULL) {
      *r_id = NULL;
    }
  }
  else if (id_old) {
    /* For undo, store contents read into id at id_old. */
    read_libblock_undo_restore_at_old_address(fd, main, id, id_old);

    if (main->id_map != NULL) {
      BKE_main_idmap_insert_id(main->id_map, id_old);
    }
  }
  else if (main->id_map != NULL) {
    BKE_main_idmap_insert_id(main->id_map, id);
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
  FileGlobal *fg = read_struct(fd, bhead, "Global");

  /* NOTE: `bfd->main->versionfile` is supposed to have already been set from `fd->fileversion`
   * beforehand by calling code. */
  bfd->main->subversionfile = fg->subversion;
  bfd->main->has_forward_compatibility_issues = !MAIN_VERSION_FILE_OLDER_OR_EQUAL(
      bfd->main, BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);

  bfd->main->minversionfile = fg->minversion;
  bfd->main->minsubversionfile = fg->minsubversion;

  bfd->main->build_commit_timestamp = fg->build_commit_timestamp;
  BLI_strncpy(bfd->main->build_hash, fg->build_hash, sizeof(bfd->main->build_hash));

  bfd->fileflags = fg->fileflags;
  bfd->globalf = fg->globalf;
  STRNCPY(bfd->filepath, fg->filepath);

  /* Error in 2.65 and older: `main->filepath` was not set if you save from startup
   * (not after loading file). */
  if (bfd->filepath[0] == 0) {
    if (fd->fileversion < 265 || (fd->fileversion == 265 && fg->subversion < 1)) {
      if ((G.fileflags & G_FILE_RECOVER_READ) == 0) {
        STRNCPY(bfd->filepath, BKE_main_blendfile_path(bfd->main));
      }
    }

    /* early 2.50 version patch - filepath not in FileGlobal struct at all */
    if (fd->fileversion <= 250) {
      STRNCPY(bfd->filepath, BKE_main_blendfile_path(bfd->main));
    }
  }

  if (G.fileflags & G_FILE_RECOVER_READ) {
    BLI_strncpy(fd->relabase, fg->filepath, sizeof(fd->relabase));
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
  bfd->cur_view_layer = blo_read_get_new_globaldata_address(fd, bfd->cur_view_layer);
  bfd->curscreen = newlibadr(fd, NULL, bfd->curscreen);
  bfd->curscene = newlibadr(fd, NULL, bfd->curscene);
  /* this happens in files older than 2.35 */
  if (bfd->curscene == NULL) {
    if (bfd->curscreen) {
      bfd->curscene = bfd->curscreen->scene;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Versioning
 * \{ */

static void do_versions_userdef(FileData *UNUSED(fd), BlendFileData *bfd)
{
  UserDef *user = bfd->user;

  if (user == NULL) {
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
    struct tm *tm = (temp_time) ? gmtime(&temp_time) : NULL;
    if (LIKELY(tm)) {
      strftime(build_commit_datetime, sizeof(build_commit_datetime), "%Y-%m-%d %H:%M", tm);
    }
    else {
      BLI_strncpy(build_commit_datetime, "unknown", sizeof(build_commit_datetime));
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
    blo_do_versions_cycles(fd, lib, main);
  }

  /* WATCH IT!!!: pointers from libdata have not been converted yet here! */
  /* WATCH IT 2!: Userdef struct init see do_versions_userdef() above! */

  /* don't forget to set version number in BKE_blender_version.h! */

  main->is_locked_for_linking = false;
}

static void do_versions_after_linking(FileData *fd, Main *main)
{
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
    do_versions_after_linking_cycles(main);
  }

  main->is_locked_for_linking = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Library Data Block (all)
 * \{ */

static void lib_link_all(FileData *fd, Main *bmain)
{
  const bool do_partial_undo = (fd->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0;

  BlendLibReader reader = {fd, bmain};

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if ((id->tag & LIB_TAG_NEED_LINK) == 0) {
      /* This ID does not need liblink, just skip to next one. */
      continue;
    }

    if ((fd->flags & FD_FLAGS_IS_MEMFILE) && GS(id->name) == ID_WM) {
      /* No load UI for undo memfiles.
       * Only WM currently, SCR needs it still (see below), and so does WS? */
      continue;
    }

    if ((fd->flags & FD_FLAGS_IS_MEMFILE) && do_partial_undo &&
        (id->tag & LIB_TAG_UNDO_OLD_ID_REUSED) != 0) {
      /* This ID has been re-used from 'old' bmain. Since it was therefore unchanged across
       * current undo step, and old IDs re-use their old memory address, we do not need to liblink
       * it at all. */
      continue;
    }

    lib_link_id(&reader, id);

    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    if (id_type->blend_read_lib != NULL) {
      id_type->blend_read_lib(&reader, id);
    }

    if (GS(id->name) == ID_LI) {
      lib_link_library(&reader, (Library *)id); /* Only init users. */
    }

    id->tag &= ~LIB_TAG_NEED_LINK;

    /* Some data that should be persistent, like the 3DCursor or the tool settings, are
     * stored in IDs affected by undo, like Scene. So this requires some specific handling. */
    if (id_type->blend_read_undo_preserve != NULL && id->orig_id != NULL) {
      id_type->blend_read_undo_preserve(&reader, id, id->orig_id);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Cleanup `ID.orig_id`, this is now reserved for depsgraph/COW usage only. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    id->orig_id = NULL;
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
static void after_liblink_merged_bmain_process(Main *bmain)
{
  /* We only expect a merged Main here, not a split one. */
  BLI_assert((bmain->prev == NULL) && (bmain->next == NULL));

  /* Check for possible cycles in scenes' 'set' background property. */
  lib_link_scenes_check_set(bmain);

  /* We could integrate that to mesh/curve/lattice lib_link, but this is really cheap process,
   * so simpler to just use it directly in this single call. */
  BLO_main_validate_shapekeys(bmain, NULL);

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
  kmi->ptr = NULL;
  kmi->flag &= ~KMI_UPDATE;
}

static BHead *read_userdef(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
  UserDef *user;
  bfd->user = user = read_struct(fd, bhead, "user def");

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
  BLO_read_list(reader, &user->asset_libraries);

  LISTBASE_FOREACH (wmKeyMap *, keymap, &user->user_keymaps) {
    keymap->modal_items = NULL;
    keymap->poll = NULL;
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

  /* XXX */
  user->uifonts.first = user->uifonts.last = NULL;

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

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath)
{
  BHead *bhead = blo_bhead_first(fd);
  BlendFileData *bfd;
  ListBase mainlist = {NULL, NULL};

  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    CLOG_INFO(&LOG_UNDO, 2, "UNDO: read step");
  }

  bfd = MEM_callocN(sizeof(BlendFileData), "blendfiledata");

  bfd->main = BKE_main_new();
  bfd->main->versionfile = fd->fileversion;

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
        bfd->main->blen_thumb = MEM_mallocN(data_size, __func__);

        BLI_assert((data_size - sizeof(*bfd->main->blen_thumb)) ==
                   (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*data) * 2)));
        bfd->main->blen_thumb->width = width;
        bfd->main->blen_thumb->height = height;
        memcpy(bfd->main->blen_thumb->rect, &data[2], data_size - sizeof(*bfd->main->blen_thumb));
      }
    }
  }

  while (bhead) {
    switch (bhead->code) {
      case DATA:
      case DNA1:
      case TEST: /* used as preview since 2.5x */
      case REND:
        bhead = blo_bhead_next(fd, bhead);
        break;
      case GLOB:
        bhead = read_global(bfd, fd, bhead);
        break;
      case USER:
        if (fd->skip_flags & BLO_READ_SKIP_USERDEF) {
          bhead = blo_bhead_next(fd, bhead);
        }
        else {
          bhead = read_userdef(bfd, fd, bhead);
        }
        break;
      case ENDB:
        bhead = NULL;
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
          Main *libmain = mainlist.last;
          bhead = read_libblock(fd, libmain, bhead, 0, true, NULL);
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
          bhead = read_libblock(fd, bfd->main, bhead, LIB_TAG_LOCAL, false, NULL);
        }
    }

    if (bfd->main->is_read_invalid) {
      return bfd;
    }
  }

  /* do before read_libraries, but skip undo case */
  if ((fd->flags & FD_FLAGS_IS_MEMFILE) == 0) {
    if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
      do_versions(fd, NULL, bfd->main);
    }

    if ((fd->skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
      do_versions_userdef(fd, bfd);
    }
  }

  if (bfd->main->is_read_invalid) {
    return bfd;
  }

  if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    fd->reports->duration.libraries = PIL_check_seconds_timer();
    read_libraries(fd, &mainlist);

    blo_join_main(&mainlist);

    lib_link_all(fd, bfd->main);
    after_liblink_merged_bmain_process(bfd->main);

    fd->reports->duration.libraries = PIL_check_seconds_timer() - fd->reports->duration.libraries;

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
        do_versions_after_linking(fd, mainvar);
      }
      blo_join_main(&mainlist);

      /* And we have to compute those user-reference-counts again, as `do_versions_after_linking()`
       * does not always properly handle user counts, and/or that function does not take into
       * account old, deprecated data. */
      BKE_main_id_refcount_recompute(bfd->main, false);
    }

    if (bfd->main->is_read_invalid) {
      return bfd;
    }

    /* After all data has been read and versioned, uses LIB_TAG_NEW. Theoretically this should
     * not be calculated in the undo case, but it is currently needed even on undo to recalculate
     * a cache. */
    ntreeUpdateAllNew(bfd->main);

    placeholders_ensure_valid(bfd->main);

    BKE_main_id_tag_all(bfd->main, LIB_TAG_NEW, false);

    /* Now that all our data-blocks are loaded,
     * we can re-generate overrides from their references. */
    if ((fd->flags & FD_FLAGS_IS_MEMFILE) == 0) {
      /* Do not apply in undo case! */
      fd->reports->duration.lib_overrides = PIL_check_seconds_timer();

      BKE_lib_override_library_main_validate(bfd->main, fd->reports->reports);
      BKE_lib_override_library_main_update(bfd->main);

      fd->reports->duration.lib_overrides = PIL_check_seconds_timer() -
                                            fd->reports->duration.lib_overrides;
    }

    BKE_collections_after_lib_link(bfd->main);

    /* Make all relative paths, relative to the open blend file. */
    fix_relpaths_library(fd->relabase, bfd->main);

    link_global(fd, bfd); /* as last */
  }

  fd->mainlist = NULL; /* Safety, this is local variable, shall not be used afterward. */

  BLI_assert(bfd->main->id_map == NULL);

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
  const struct BHeadSort *x1 = v1, *x2 = v2;

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
  struct BHeadSort *bhs;
  int tot = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    tot++;
  }

  fd->tot_bheadmap = tot;
  if (tot == 0) {
    return;
  }

  bhs = fd->bheadmap = MEM_malloc_arrayN(tot, sizeof(struct BHeadSort), "BHeadSort");

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead), bhs++) {
    bhs->bhead = bhead;
    bhs->old = bhead->old;
  }

  qsort(fd->bheadmap, tot, sizeof(struct BHeadSort), verg_bheadsort);
}

static BHead *find_previous_lib(FileData *fd, BHead *bhead)
{
  /* Skip library data-blocks in undo, see comment in read_libblock. */
  if (fd->flags & FD_FLAGS_IS_MEMFILE) {
    return NULL;
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
  BHead* bhead;
#endif
  struct BHeadSort *bhs, bhs_s;

  if (!old) {
    return NULL;
  }

  if (fd->bheadmap == NULL) {
    sort_bhead_old_map(fd);
  }

  bhs_s.old = old;
  bhs = bsearch(&bhs_s, fd->bheadmap, fd->tot_bheadmap, sizeof(struct BHeadSort), verg_bheadsort);

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

  return NULL;
}

static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name)
{
#ifdef USE_GHASH_BHEAD

  char idname_full[MAX_ID_NAME];

  *((short *)idname_full) = idcode;
  BLI_strncpy(idname_full + 2, name, sizeof(idname_full) - 2);

  return BLI_ghash_lookup(fd->bhead_idname_hash, idname_full);

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

  return NULL;
#endif
}

static BHead *find_bhead_from_idname(FileData *fd, const char *idname)
{
#ifdef USE_GHASH_BHEAD
  return BLI_ghash_lookup(fd->bhead_idname_hash, idname);
#else
  return find_bhead_from_code_name(fd, GS(idname), idname + 2);
#endif
}

static ID *is_yet_read(FileData *fd, Main *mainvar, BHead *bhead)
{
  if (mainvar->id_map == NULL) {
    mainvar->id_map = BKE_main_idmap_create(mainvar, false, NULL, MAIN_IDMAP_TYPE_NAME);
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

static void expand_doit_library(void *fdhandle, Main *mainvar, void *old)
{
  FileData *fd = fdhandle;

  BHead *bhead = find_bhead(fd, old);
  if (bhead == NULL) {
    return;
  }

  if (bhead->code == ID_LINK_PLACEHOLDER) {
    /* Placeholder link to data-block in another library. */
    BHead *bheadlib = find_previous_lib(fd, bhead);
    if (bheadlib == NULL) {
      return;
    }

    Library *lib = read_struct(fd, bheadlib, "Library");
    Main *libmain = blo_find_main(fd, lib->filepath, fd->relabase);

    if (libmain->curlib == NULL) {
      const char *idname = blo_bhead_id_name(fd, bhead);

      BLO_reportf_wrap(fd->reports,
                       RPT_WARNING,
                       TIP_("LIB: Data refers to main .blend file: '%s' from %s"),
                       idname,
                       mainvar->curlib->filepath_abs);
      return;
    }

    ID *id = is_yet_read(fd, libmain, bhead);

    if (id == NULL) {
      /* ID has not been read yet, add placeholder to the main of the
       * library it belongs to, so that it will be read later. */
      read_libblock(fd, libmain, bhead, fd->id_tag_extra | LIB_TAG_INDIRECT, false, &id);
      BLI_assert(id != NULL);
      id_sort_by_name(which_libbase(libmain, GS(id->name)), id, id->prev);

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

      /* If "id" is a real data-block and not a placeholder, we need to
       * update fd->libmap to replace ID_LINK_PLACEHOLDER with the real
       * ID_* code.
       *
       * When the real ID is read this replacement happens for all
       * libraries read so far, but not for libraries that have not been
       * read yet at that point. */
      change_link_placeholder_to_real_ID_pointer_fd(fd, bhead->old, id);

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

    ID *id = is_yet_read(fd, mainvar, bhead);
    if (id == NULL) {
      read_libblock(fd,
                    mainvar,
                    bhead,
                    fd->id_tag_extra | LIB_TAG_NEED_EXPAND | LIB_TAG_INDIRECT,
                    false,
                    &id);
      BLI_assert(id != NULL);
      id_sort_by_name(which_libbase(mainvar, GS(id->name)), id, id->prev);
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

static void expand_id(BlendExpander *expander, ID *id);

static void expand_id_embedded_id(BlendExpander *expander, ID *id)
{
  /* Handle 'private IDs'. */
  bNodeTree *nodetree = ntreeFromID(id);
  if (nodetree != NULL) {
    expand_id(expander, &nodetree->id);
    ntreeBlendReadExpand(expander, nodetree);
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != NULL) {
      expand_id(expander, &scene->master_collection->id);
      BKE_collection_blend_read_expand(expander, scene->master_collection);
    }
  }
}

static void expand_id(BlendExpander *expander, ID *id)
{
  IDP_BlendReadExpand(expander, id->properties);

  if (id->override_library) {
    BLO_expand(expander, id->override_library->reference);
    BLO_expand(expander, id->override_library->storage);
  }

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt != NULL) {
    BKE_animdata_blend_read_expand(expander, adt);
  }

  expand_id_embedded_id(expander, id);
}

void BLO_main_expander(BLOExpandDoitCallback expand_doit_func)
{
  expand_doit = expand_doit_func;
}

void BLO_expand_main(void *fdhandle, Main *mainvar)
{
  ListBase *lbarray[INDEX_ID_MAX];
  FileData *fd = fdhandle;
  ID *id;
  int a;
  bool do_it = true;

  BlendExpander expander = {fd, mainvar};

  while (do_it) {
    do_it = false;

    a = set_listbasepointers(mainvar, lbarray);
    while (a--) {
      id = lbarray[a]->first;
      while (id) {
        if (id->tag & LIB_TAG_NEED_EXPAND) {
          expand_id(&expander, id);

          const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
          if (id_type->blend_read_expand != NULL) {
            id_type->blend_read_expand(&expander, id);
          }

          do_it = true;
          id->tag &= ~LIB_TAG_NEED_EXPAND;
        }
        id = id->next;
      }
    }
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
    id = is_yet_read(fd, mainl, bhead);
    if (id == NULL) {
      /* not read yet */
      const int tag = ((force_indirect ? LIB_TAG_INDIRECT : LIB_TAG_EXTERN) | fd->id_tag_extra);
      read_libblock(fd, mainl, bhead, tag | LIB_TAG_NEED_EXPAND, false, &id);

      if (id) {
        /* sort by name in list */
        ListBase *lb = which_libbase(mainl, idcode);
        id_sort_by_name(lb, id, NULL);
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
        mainl, idcode, name, force_indirect ? LIB_TAG_INDIRECT : LIB_TAG_EXTERN);
  }
  else {
    id = NULL;
  }

  /* if we found the id but the id is NULL, this is really bad */
  BLI_assert(!((bhead != NULL) && (id == NULL)));

  return id;
}

ID *BLO_library_link_named_part(Main *mainl,
                                BlendHandle **bh,
                                const short idcode,
                                const char *name,
                                const struct LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);

  ID *ret_id = NULL;
  if (!mainl->is_read_invalid) {
    ret_id = link_named_part(mainl, fd, idcode, name, params->flag);
  }

  if (mainl->is_read_invalid) {
    return NULL;
  }
  return ret_id;
}

/* common routine to append/link something from a library */

static Main *library_link_begin(Main *mainvar,
                                FileData **fd,
                                const char *filepath,
                                const int id_tag_extra)
{
  Main *mainl;

  /* Only allow specific tags to be set as extra,
   * otherwise this could conflict with library loading logic.
   * Other flags can be added here, as long as they are safe. */
  BLI_assert((id_tag_extra & ~LIB_TAG_TEMP_MAIN) == 0);

  (*fd)->id_tag_extra = id_tag_extra;

  (*fd)->mainlist = MEM_callocN(sizeof(ListBase), "FileData.mainlist");

  /* make mains */
  blo_split_main((*fd)->mainlist, mainvar);

  /* which one do we need? */
  mainl = blo_find_main(*fd, filepath, BKE_main_blendfile_path(mainvar));

  /* needed for do_version */
  mainl->versionfile = (*fd)->fileversion;
  read_file_version(*fd, mainl);
#ifdef USE_GHASH_BHEAD
  read_file_bhead_idname_map_create(*fd);
#endif

  return mainl;
}

void BLO_library_link_params_init(struct LibraryLink_Params *params,
                                  struct Main *bmain,
                                  const int flag,
                                  const int id_tag_extra)
{
  memset(params, 0, sizeof(*params));
  params->bmain = bmain;
  params->flag = flag;
  params->id_tag_extra = id_tag_extra;
}

void BLO_library_link_params_init_with_context(struct LibraryLink_Params *params,
                                               struct Main *bmain,
                                               const int flag,
                                               const int id_tag_extra,
                                               /* Context arguments. */
                                               struct Scene *scene,
                                               struct ViewLayer *view_layer,
                                               const struct View3D *v3d)
{
  BLO_library_link_params_init(params, bmain, flag, id_tag_extra);
  if (scene != NULL) {
    params->context.scene = scene;
    params->context.view_layer = view_layer;
    params->context.v3d = v3d;
  }
}

Main *BLO_library_link_begin(BlendHandle **bh,
                             const char *filepath,
                             const struct LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);
  return library_link_begin(params->bmain, &fd, filepath, params->id_tag_extra);
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

  if (mainl->id_map == NULL) {
    mainl->id_map = BKE_main_idmap_create(mainl, false, NULL, MAIN_IDMAP_TYPE_NAME);
  }

  /* expander now is callback function */
  BLO_main_expander(expand_doit_library);

  /* make main consistent */
  BLO_expand_main(*fd, mainl);

  /* do this when expand found other libs */
  read_libraries(*fd, (*fd)->mainlist);

  curlib = mainl->curlib;

  /* make the lib path relative if required */
  if (flag & FILE_RELPATH) {
    /* use the full path, this could have been read by other library even */
    BLI_strncpy(curlib->filepath, curlib->filepath_abs, sizeof(curlib->filepath));

    /* uses current .blend file as reference */
    BLI_path_rel(curlib->filepath, BKE_main_blendfile_path_from_global());
  }

  blo_join_main((*fd)->mainlist);
  mainvar = (*fd)->mainlist->first;
  mainl = NULL; /* blo_join_main free's mainl, can't use anymore */

  if (mainvar->is_read_invalid) {
    return;
  }

  lib_link_all(*fd, mainvar);
  after_liblink_merged_bmain_process(mainvar);

  /* Some versioning code does expect some proper userrefcounting, e.g. in conversion from
   * groups to collections... We could optimize out that first call when we are reading a
   * current version file, but again this is really not a bottle neck currently. so not worth
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

    do_versions_after_linking(*fd, main_newid);

    add_main_to_main(mainvar, main_newid);

    if (mainvar->is_read_invalid) {
      break;
    }
  }

  blo_join_main((*fd)->mainlist);
  mainvar = (*fd)->mainlist->first;
  MEM_freeN((*fd)->mainlist);

  if (mainvar->is_read_invalid) {
    BKE_main_free(main_newid);
    return;
  }

  /* This does not take into account old, deprecated data, so we also have to do it after
   * `do_versions_after_linking()`. */
  BKE_main_id_refcount_recompute(mainvar, false);

  /* After all data has been read and versioned, uses LIB_TAG_NEW. */
  ntreeUpdateAllNew(mainvar);

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

  /* Make all relative paths, relative to the open blend file. */
  fix_relpaths_library(BKE_main_blendfile_path(mainvar), mainvar);

  /* patch to prevent switch_endian happens twice */
  if ((*fd)->flags & FD_FLAGS_SWITCH_ENDIAN) {
    blo_filedata_free(*fd);
    *fd = NULL;
  }
}

void BLO_library_link_end(Main *mainl, BlendHandle **bh, const struct LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);
  if (!mainl->is_read_invalid) {
    library_link_end(mainl, &fd, params->flag);
    *bh = (BlendHandle *)fd;
  }
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
  BHead *bhead = NULL;
  const bool is_valid = BKE_idtype_idcode_is_linkable(GS(id->name)) ||
                        ((id->tag & LIB_TAG_EXTERN) == 0);

  if (fd) {
    bhead = find_bhead_from_idname(fd, id->name);
  }

  if (!is_valid) {
    BLO_reportf_wrap(basefd->reports,
                     RPT_ERROR,
                     TIP_("LIB: %s: '%s' is directly linked from '%s' (parent '%s'), but is a "
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
                     TIP_("LIB: %s: '%s' missing from '%s', parent '%s'"),
                     BKE_idtype_idcode_to_name(GS(id->name)),
                     id->name + 2,
                     mainvar->curlib->filepath_abs,
                     library_parent_filepath(mainvar->curlib));
    basefd->reports->count.missing_linked_id++;

    /* Generate a placeholder for this ID (simplified version of read_libblock actually...). */
    if (r_id) {
      *r_id = is_valid ? create_placeholder(mainvar, GS(id->name), id->name + 2, id->tag) : NULL;
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
    ID *id = lbarray[a]->first;
    ListBase pending_free_ids = {NULL};

    while (id) {
      ID *id_next = id->next;
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && !(id->flag & LIB_INDIRECT_WEAK_LINK)) {
        BLI_remlink(lbarray[a], id);
        if (mainvar->id_map != NULL) {
          BKE_main_idmap_remove_id(mainvar->id_map, id);
        }

        /* When playing with lib renaming and such, you may end with cases where
         * you have more than one linked ID of the same data-block from same
         * library. This is absolutely horrible, hence we use a ghash to ensure
         * we go back to a single linked data when loading the file. */
        ID **realid = NULL;
        if (!BLI_ghash_ensure_p(loaded_ids, id->name, (void ***)&realid)) {
          read_library_linked_id(basefd, fd, mainvar, id, realid);
        }

        /* `realid` shall never be NULL - unless some source file/lib is broken
         * (known case: some directly linked shapekey from a missing lib...). */
        // BLI_assert(*realid != NULL);

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
    BLI_ghash_clear(loaded_ids, NULL, NULL);
    BLI_freelistN(&pending_free_ids);
  }

  BLI_ghash_free(loaded_ids, NULL, NULL);
}

static void read_library_clear_weak_links(FileData *basefd, ListBase *mainlist, Main *mainvar)
{
  /* Any remaining weak links at this point have been lost, silently drop
   * those by setting them to NULL pointers. */
  ListBase *lbarray[INDEX_ID_MAX];
  int a = set_listbasepointers(mainvar, lbarray);

  while (a--) {
    ID *id = lbarray[a]->first;

    while (id) {
      ID *id_next = id->next;
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && (id->flag & LIB_INDIRECT_WEAK_LINK)) {
        CLOG_INFO(&LOG, 3, "Dropping weak link to '%s'", id->name);
        change_link_placeholder_to_real_ID_pointer(mainlist, basefd, id, NULL);
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

  if (fd != NULL) {
    /* File already open. */
    return fd;
  }

  if (mainptr->curlib->packedfile) {
    /* Read packed file. */
    PackedFile *pf = mainptr->curlib->packedfile;

    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     TIP_("Read packed library:  '%s', parent '%s'"),
                     mainptr->curlib->filepath,
                     library_parent_filepath(mainptr->curlib));
    fd = blo_filedata_from_memory(pf->data, pf->size, basefd->reports);

    /* Needed for library_append and read_libraries. */
    BLI_strncpy(fd->relabase, mainptr->curlib->filepath_abs, sizeof(fd->relabase));
  }
  else {
    /* Read file on disk. */
    BLO_reportf_wrap(basefd->reports,
                     RPT_INFO,
                     TIP_("Read library:  '%s', '%s', parent '%s'"),
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
    mainptr->curlib->filedata = NULL;
    mainptr->curlib->id.tag |= LIB_TAG_MISSING;
    /* Set lib version to current main one... Makes assert later happy. */
    mainptr->versionfile = mainptr->curlib->versionfile = mainl->versionfile;
    mainptr->subversionfile = mainptr->curlib->subversionfile = mainl->subversionfile;
  }

  if (fd == NULL) {
    BLO_reportf_wrap(
        basefd->reports, RPT_INFO, TIP_("Cannot find lib '%s'"), mainptr->curlib->filepath_abs);
    basefd->reports->count.missing_libraries++;
  }

  return fd;
}

static void read_libraries(FileData *basefd, ListBase *mainlist)
{
  Main *mainl = mainlist->first;
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

          if (mainptr->id_map == NULL) {
            mainptr->id_map = BKE_main_idmap_create(mainptr, false, NULL, MAIN_IDMAP_TYPE_NAME);
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

      /* File data can be zero with link/append. */
      if (mainptr->curlib->filedata) {
        do_versions(mainptr->curlib->filedata, mainptr->curlib, main_newid);
      }
      else {
        do_versions(basefd, NULL, main_newid);
      }

      add_main_to_main(mainptr, main_newid);
    }

    /* Lib linking. */
    if (mainptr->curlib->filedata) {
      lib_link_all(mainptr->curlib->filedata, mainptr);
    }

    /* NOTE: No need to call #do_versions_after_linking() or #BKE_main_id_refcount_recompute()
     * here, as this function is only called for library 'subset' data handling, as part of
     * either full blendfile reading (#blo_read_file_internal()), or library-data linking
     * (#library_link_end()). */

    /* Free file data we no longer need. */
    if (mainptr->curlib->filedata) {
      blo_filedata_free(mainptr->curlib->filedata);
    }
    mainptr->curlib->filedata = NULL;
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

ID *BLO_read_get_new_id_address(BlendLibReader *reader, Library *lib, ID *id)
{
  return newlibadr(reader->fd, lib, id);
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
  if (callback != NULL) {
    callback(reader, list->first);
  }
  Link *ln = list->first;
  Link *prev = NULL;
  while (ln) {
    BLO_read_data_address(reader, &ln->next);
    if (ln->next != NULL && callback != NULL) {
      callback(reader, ln->next);
    }
    ln->prev = prev;
    prev = ln;
    ln = ln->next;
  }
  list->last = prev;
}

void BLO_read_list(BlendDataReader *reader, struct ListBase *list)
{
  BLO_read_list_cb(reader, list, NULL);
}

void BLO_read_int32_array(BlendDataReader *reader, int array_size, int32_t **ptr_p)
{
  BLO_read_data_address(reader, ptr_p);
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_int32_array(*ptr_p, array_size);
  }
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
      dst[i] = (uint32_t)(ptr >> 3);
    }
  }
  else {
    for (int i = 0; i < array_size; i++) {
      dst[i] = (uint32_t)(src[i] >> 3);
    }
  }
}

static void convert_pointer_array_32_to_64(BlendDataReader *UNUSED(reader),
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
  if (orig_array == NULL) {
    *ptr_p = NULL;
    return;
  }

  int file_pointer_size = fd->filesdna->pointer_size;
  int current_pointer_size = fd->memsdna->pointer_size;

  /* Over-allocation is fine, but might be better to pass the length as parameter. */
  int array_size = MEM_allocN_len(orig_array) / file_pointer_size;

  void *final_array = NULL;

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

void BLO_expand_id(BlendExpander *expander, ID *id)
{
  expand_doit(expander->fd, expander->main, id);
}

/** \} */
