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
 */

/** \file
 * \ingroup blenloader
 */

#include "zlib.h"

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

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_effect_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_fluid_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_layer_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcache_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_simulation_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_volume_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_endian_switch.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_brush.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_curveprofile.h"
#include "BKE_deform.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_fluid.h"
#include "BKE_global.h" /* for G */
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_hair.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_main.h" /* for Main */
#include "BKE_main_idmap.h"
#include "BKE_material.h"
#include "BKE_mesh.h" /* for ME_ defines (patching) */
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_nla.h"
#include "BKE_node.h" /* for tree type defines */
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_shader_fx.h"
#include "BKE_simulation.h"
#include "BKE_sound.h"
#include "BKE_volume.h"
#include "BKE_workspace.h"

#include "DRW_engine.h"

#include "DEG_depsgraph.h"

#include "NOD_socket.h"

#include "BLO_blend_defs.h"
#include "BLO_blend_validate.h"
#include "BLO_read_write.h"
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "RE_engine.h"

#include "engines/eevee/eevee_lightcache.h"

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
 * while zlib supports seek it's unusably slow, see: T61880.
 */
#define USE_BHEAD_READ_ON_DEMAND

/* use GHash for BHead name-based lookups (speeds up linking) */
#define USE_GHASH_BHEAD

/* Use GHash for restoring pointers by name */
#define USE_GHASH_RESTORE_POINTER

/* Define this to have verbose debug prints. */
//#define USE_DEBUG_PRINT

#ifdef USE_DEBUG_PRINT
#  define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#  define DEBUG_PRINTF(...)
#endif

/* local prototypes */
static void read_libraries(FileData *basefd, ListBase *mainlist);
static void *read_struct(FileData *fd, BHead *bh, const char *blockname);
static void direct_link_modifiers(BlendDataReader *reader, ListBase *lb, Object *ob);
static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name);
static BHead *find_bhead_from_idname(FileData *fd, const char *idname);
static bool library_link_idcode_needs_tag_check(const short idcode, const int flag);

#ifdef USE_COLLECTION_COMPAT_28
static void expand_scene_collection(BlendExpander *expander, SceneCollection *sc);
#endif

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

/* We could change this in the future, for now it's simplest if only data is delayed
 * because ID names are used in lookup tables. */
#define BHEAD_USE_READ_ON_DEMAND(bhead) ((bhead)->code == DATA)

/**
 * This function ensures that reports are printed,
 * in the case of library linking errors this is important!
 *
 * bit kludge but better then doubling up on prints,
 * we could alternatively have a versions of a report function which forces printing - campbell
 */
void blo_reportf_wrap(ReportList *reports, ReportType type, const char *format, ...)
{
  char fixed_buf[1024]; /* should be long enough */

  va_list args;

  va_start(args, format);
  vsnprintf(fixed_buf, sizeof(fixed_buf), format, args);
  va_end(args);

  fixed_buf[sizeof(fixed_buf) - 1] = '\0';

  BKE_report(reports, type, fixed_buf);

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
  /* Hashmap that stores indices into the `entries` array. */
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

static OldNewMap *oldnewmap_new(void)
{
  OldNewMap *onm = MEM_callocN(sizeof(*onm), "OldNewMap");

  onm->capacity_exp = DEFAULT_SIZE_EXP;
  onm->entries = MEM_malloc_arrayN(
      ENTRIES_CAPACITY(onm), sizeof(*onm->entries), "OldNewMap.entries");
  onm->map = MEM_malloc_arrayN(MAP_CAPACITY(onm), sizeof(*onm->map), "OldNewMap.map");
  oldnewmap_clear_map(onm);

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

  onm->capacity_exp = DEFAULT_SIZE_EXP;
  oldnewmap_clear_map(onm);
  onm->nentries = 0;
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
  ListBase *lbarray[MAX_LIBARRAY], *fromarray[MAX_LIBARRAY];
  int a;

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
  while ((tojoin = mainl->next)) {
    add_main_to_main(mainl, tojoin);
    BLI_remlink(mainlist, tojoin);
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
        printf("%s: invalid library for '%s'\n", __func__, id->name);
        BLI_assert(0);
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

  /* (Library.temp_index -> Main), lookup table */
  const uint lib_main_array_len = BLI_listbase_count(&main->libraries);
  Main **lib_main_array = MEM_malloc_arrayN(lib_main_array_len, sizeof(*lib_main_array), __func__);

  int i = 0;
  for (Library *lib = main->libraries.first; lib; lib = lib->id.next, i++) {
    Main *libmain = BKE_main_new();
    libmain->curlib = lib;
    libmain->versionfile = lib->versionfile;
    libmain->subversionfile = lib->subversionfile;
    BLI_addtail(mainlist, libmain);
    lib->temp_index = i;
    lib_main_array[i] = libmain;
  }

  ListBase *lbarray[MAX_LIBARRAY];
  i = set_listbasepointers(main, lbarray);
  while (i--) {
    ID *id = lbarray[i]->first;
    if (id == NULL || GS(id->name) == ID_LI) {
      /* No ID_LI data-lock should ever be linked anyway, but just in case, better be explicit. */
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
    if (bhead->code == GLOB) {
      FileGlobal *fg = read_struct(fd, bhead, "Global");
      if (fg) {
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
      is_link = BKE_idtype_idcode_is_valid(code_prev) ? BKE_idtype_idcode_is_linkable(code_prev) :
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
      is_link = BKE_idtype_idcode_is_valid(code_prev) ? BKE_idtype_idcode_is_linkable(code_prev) :
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
    const char *libname = (m->curlib) ? m->curlib->filepath_abs : m->name;

    if (BLI_path_cmp(name1, libname) == 0) {
      if (G.debug & G_DEBUG) {
        printf("blo_find_main: found library %s\n", libname);
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
    printf("blo_find_main: added new lib %s\n", filepath);
  }
  return m;
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

    /* this patch is to avoid a long long being read from not-eight aligned positions
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
        readsize = fd->read(fd, &bhead4, sizeof(bhead4), NULL);

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
        readsize = fd->read(fd, &bhead8, sizeof(bhead8), NULL);

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
      else if (fd->seek != NULL && BHEAD_USE_READ_ON_DEMAND(&bhead)) {
        /* Delay reading bhead content. */
        new_bhead = MEM_mallocN(sizeof(BHeadN), "new_bhead");
        if (new_bhead) {
          new_bhead->next = new_bhead->prev = NULL;
          new_bhead->file_offset = fd->file_offset;
          new_bhead->has_data = false;
          new_bhead->is_memchunk_identical = false;
          new_bhead->bhead = bhead;
          off64_t seek_new = fd->seek(fd, bhead.len, SEEK_CUR);
          if (seek_new == -1) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = NULL;
          }
          BLI_assert(fd->file_offset == seek_new);
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

          readsize = fd->read(
              fd, new_bhead + 1, (size_t)bhead.len, &new_bhead->is_memchunk_identical);

          if (readsize != (ssize_t)bhead.len) {
            fd->is_eof = true;
            MEM_freeN(new_bhead);
            new_bhead = NULL;
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
  off64_t offset_backup = fd->file_offset;
  if (UNLIKELY(fd->seek(fd, new_bhead->file_offset, SEEK_SET) == -1)) {
    success = false;
  }
  else {
    if (fd->read(fd, buf, (size_t)new_bhead->bhead.len, &new_bhead->is_memchunk_identical) !=
        (ssize_t)new_bhead->bhead.len) {
      success = false;
    }
  }
  if (fd->seek(fd, offset_backup, SEEK_SET) == -1) {
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

/* Warning! Caller's responsibility to ensure given bhead **is** and ID one! */
const char *blo_bhead_id_name(const FileData *fd, const BHead *bhead)
{
  return (const char *)POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_name_offs);
}

static void decode_blender_header(FileData *fd)
{
  char header[SIZEOFBLENDERHEADER], num[4];
  ssize_t readsize;

  /* read in the header data */
  readsize = fd->read(fd, header, sizeof(header), NULL);

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
        fd->id_name_offs = DNA_elem_offset(fd->filesdna, "ID", "char", "name[]");
        BLI_assert(fd->id_name_offs != -1);

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

/* Regular file reading. */

static ssize_t fd_read_data_from_file(FileData *filedata,
                                      void *buffer,
                                      size_t size,
                                      bool *UNUSED(r_is_memchunck_identical))
{
  ssize_t readsize = read(filedata->filedes, buffer, size);

  if (readsize < 0) {
    readsize = EOF;
  }
  else {
    filedata->file_offset += readsize;
  }

  return readsize;
}

static off64_t fd_seek_data_from_file(FileData *filedata, off64_t offset, int whence)
{
  filedata->file_offset = BLI_lseek(filedata->filedes, offset, whence);
  return filedata->file_offset;
}

/* GZip file reading. */

static ssize_t fd_read_gzip_from_file(FileData *filedata,
                                      void *buffer,
                                      size_t size,
                                      bool *UNUSED(r_is_memchunck_identical))
{
  BLI_assert(size <= INT_MAX);

  ssize_t readsize = gzread(filedata->gzfiledes, buffer, (uint)size);

  if (readsize < 0) {
    readsize = EOF;
  }
  else {
    filedata->file_offset += readsize;
  }

  return readsize;
}

/* Memory reading. */

static ssize_t fd_read_from_memory(FileData *filedata,
                                   void *buffer,
                                   size_t size,
                                   bool *UNUSED(r_is_memchunck_identical))
{
  /* don't read more bytes then there are available in the buffer */
  ssize_t readsize = (ssize_t)MIN2(size, filedata->buffersize - (size_t)filedata->file_offset);

  memcpy(buffer, filedata->buffer + filedata->file_offset, (size_t)readsize);
  filedata->file_offset += readsize;

  return readsize;
}

/* MemFile reading. */

static ssize_t fd_read_from_memfile(FileData *filedata,
                                    void *buffer,
                                    size_t size,
                                    bool *r_is_memchunck_identical)
{
  static size_t seek = SIZE_MAX; /* the current position */
  static size_t offset = 0;      /* size of previous chunks */
  static MemFileChunk *chunk = NULL;
  size_t chunkoffset, readsize, totread;

  if (r_is_memchunck_identical != NULL) {
    *r_is_memchunck_identical = true;
  }

  if (size == 0) {
    return 0;
  }

  if (seek != (size_t)filedata->file_offset) {
    chunk = filedata->memfile->chunks.first;
    seek = 0;

    while (chunk) {
      if (seek + chunk->size > (size_t)filedata->file_offset) {
        break;
      }
      seek += chunk->size;
      chunk = chunk->next;
    }
    offset = seek;
    seek = (size_t)filedata->file_offset;
  }

  if (chunk) {
    totread = 0;

    do {
      /* first check if it's on the end if current chunk */
      if (seek - offset == chunk->size) {
        offset += chunk->size;
        chunk = chunk->next;
      }

      /* debug, should never happen */
      if (chunk == NULL) {
        printf("illegal read, chunk zero\n");
        return 0;
      }

      chunkoffset = seek - offset;
      readsize = size - totread;

      /* data can be spread over multiple chunks, so clamp size
       * to within this chunk, and then it will read further in
       * the next chunk */
      if (chunkoffset + readsize > chunk->size) {
        readsize = chunk->size - chunkoffset;
      }

      memcpy(POINTER_OFFSET(buffer, totread), chunk->buf + chunkoffset, readsize);
      totread += readsize;
      filedata->file_offset += readsize;
      seek += readsize;
      if (r_is_memchunck_identical != NULL) {
        /* `is_identical` of current chunk represents whether it changed compared to previous undo
         * step. this is fine in redo case (filedata->undo_direction > 0), but not in undo case,
         * where we need an extra flag defined when saving the next (future) step after the one we
         * want to restore, as we are supposed to 'come from' that future undo step, and not the
         * one before current one. */
        *r_is_memchunck_identical &= filedata->undo_direction > 0 ? chunk->is_identical :
                                                                    chunk->is_identical_future;
      }
    } while (totread < size);

    return (ssize_t)totread;
  }

  return 0;
}

static FileData *filedata_new(void)
{
  FileData *fd = MEM_callocN(sizeof(FileData), "FileData");

  fd->filedes = -1;
  fd->gzfiledes = NULL;

  fd->memsdna = DNA_sdna_current_get();

  fd->datamap = oldnewmap_new();
  fd->globmap = oldnewmap_new();
  fd->libmap = oldnewmap_new();

  return fd;
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
                                                   ReportList *reports,
                                                   int file)
{
  FileDataReadFn *read_fn = NULL;
  FileDataSeekFn *seek_fn = NULL; /* Optional. */

  gzFile gzfile = (gzFile)Z_NULL;

  char header[7];

  /* Regular file. */
  errno = 0;
  if (read(file, header, sizeof(header)) != sizeof(header)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Unable to read '%s': %s",
                filepath,
                errno ? strerror(errno) : TIP_("insufficient content"));
    return NULL;
  }

  BLI_lseek(file, 0, SEEK_SET);

  /* Regular file. */
  if (memcmp(header, "BLENDER", sizeof(header)) == 0) {
    read_fn = fd_read_data_from_file;
    seek_fn = fd_seek_data_from_file;
  }

  /* Gzip file. */
  errno = 0;
  if ((read_fn == NULL) &&
      /* Check header magic. */
      (header[0] == 0x1f && header[1] == 0x8b)) {
    gzfile = BLI_gzopen(filepath, "rb");
    if (gzfile == (gzFile)Z_NULL) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Unable to open '%s': %s",
                  filepath,
                  errno ? strerror(errno) : TIP_("unknown error reading file"));
      return NULL;
    }

    /* 'seek_fn' is too slow for gzip, don't set it. */
    read_fn = fd_read_gzip_from_file;
    /* Caller must close. */
    file = -1;
  }

  if (read_fn == NULL) {
    BKE_reportf(reports, RPT_WARNING, "Unrecognized file format '%s'", filepath);
    return NULL;
  }

  FileData *fd = filedata_new();

  fd->filedes = file;
  fd->gzfiledes = gzfile;

  fd->read = read_fn;
  fd->seek = seek_fn;

  return fd;
}

static FileData *blo_filedata_from_file_open(const char *filepath, ReportList *reports)
{
  errno = 0;
  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Unable to open '%s': %s",
                filepath,
                errno ? strerror(errno) : TIP_("unknown error reading file"));
    return NULL;
  }
  FileData *fd = blo_filedata_from_file_descriptor(filepath, reports, file);
  if ((fd == NULL) || (fd->filedes == -1)) {
    close(file);
  }
  return fd;
}

/* cannot be called with relative paths anymore! */
/* on each new library added, it now checks for the current FileData and expands relativeness */
FileData *blo_filedata_from_file(const char *filepath, ReportList *reports)
{
  FileData *fd = blo_filedata_from_file_open(filepath, reports);
  if (fd != NULL) {
    /* needed for library_append and read_libraries */
    BLI_strncpy(fd->relabase, filepath, sizeof(fd->relabase));

    return blo_decode_and_check(fd, reports);
  }
  return NULL;
}

/**
 * Same as blo_filedata_from_file(), but does not reads DNA data, only header.
 * Use it for light access (e.g. thumbnail reading).
 */
static FileData *blo_filedata_from_file_minimal(const char *filepath)
{
  FileData *fd = blo_filedata_from_file_open(filepath, NULL);
  if (fd != NULL) {
    decode_blender_header(fd);
    if (fd->flags & FD_FLAGS_FILE_OK) {
      return fd;
    }
    blo_filedata_free(fd);
  }
  return NULL;
}

static ssize_t fd_read_gzip_from_memory(FileData *filedata,
                                        void *buffer,
                                        size_t size,
                                        bool *UNUSED(r_is_memchunck_identical))
{
  int err;

  filedata->strm.next_out = (Bytef *)buffer;
  filedata->strm.avail_out = (uint)size;

  /* Inflate another chunk. */
  err = inflate(&filedata->strm, Z_SYNC_FLUSH);

  if (err == Z_STREAM_END) {
    return 0;
  }
  if (err != Z_OK) {
    printf("fd_read_gzip_from_memory: zlib error\n");
    return 0;
  }

  filedata->file_offset += size;

  return (ssize_t)size;
}

static int fd_read_gzip_from_memory_init(FileData *fd)
{

  fd->strm.next_in = (Bytef *)fd->buffer;
  fd->strm.avail_in = fd->buffersize;
  fd->strm.total_out = 0;
  fd->strm.zalloc = Z_NULL;
  fd->strm.zfree = Z_NULL;

  if (inflateInit2(&fd->strm, (16 + MAX_WBITS)) != Z_OK) {
    return 0;
  }

  fd->read = fd_read_gzip_from_memory;

  return 1;
}

FileData *blo_filedata_from_memory(const void *mem, int memsize, ReportList *reports)
{
  if (!mem || memsize < SIZEOFBLENDERHEADER) {
    BKE_report(reports, RPT_WARNING, (mem) ? TIP_("Unable to read") : TIP_("Unable to open"));
    return NULL;
  }

  FileData *fd = filedata_new();
  const char *cp = mem;

  fd->buffer = mem;
  fd->buffersize = memsize;

  /* test if gzip */
  if (cp[0] == 0x1f && cp[1] == 0x8b) {
    if (0 == fd_read_gzip_from_memory_init(fd)) {
      blo_filedata_free(fd);
      return NULL;
    }
  }
  else {
    fd->read = fd_read_from_memory;
  }

  fd->flags |= FD_FLAGS_NOT_MY_BUFFER;

  return blo_decode_and_check(fd, reports);
}

FileData *blo_filedata_from_memfile(MemFile *memfile,
                                    const struct BlendFileReadParams *params,
                                    ReportList *reports)
{
  if (!memfile) {
    BKE_report(reports, RPT_WARNING, "Unable to open blend <memory>");
    return NULL;
  }

  FileData *fd = filedata_new();
  fd->memfile = memfile;
  fd->undo_direction = params->undo_direction;

  fd->read = fd_read_from_memfile;
  fd->flags |= FD_FLAGS_NOT_MY_BUFFER;

  return blo_decode_and_check(fd, reports);
}

void blo_filedata_free(FileData *fd)
{
  if (fd) {
    if (fd->filedes != -1) {
      close(fd->filedes);
    }

    if (fd->gzfiledes != NULL) {
      gzclose(fd->gzfiledes);
    }

    if (fd->strm.next_in) {
      if (inflateEnd(&fd->strm) != Z_OK) {
        printf("close gzip stream error\n");
      }
    }

    if (fd->buffer && !(fd->flags & FD_FLAGS_NOT_MY_BUFFER)) {
      MEM_freeN((void *)fd->buffer);
      fd->buffer = NULL;
    }

    /* Free all BHeadN data blocks */
#ifndef NDEBUG
    BLI_freelistN(&fd->bhead_list);
#else
    /* Sanity check we're not keeping memory we don't need. */
    LISTBASE_FOREACH_MUTABLE (BHeadN *, new_bhead, &fd->bhead_list) {
      if (fd->seek != NULL && BHEAD_USE_READ_ON_DEMAND(&new_bhead->bhead)) {
        BLI_assert(new_bhead->has_data == 0);
      }
      MEM_freeN(new_bhead);
    }
#endif

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

/**
 * Check whether given path ends with a blend file compatible extension
 * (`.blend`, `.ble` or `.blend.gz`).
 *
 * \param str: The path to check.
 * \return true is this path ends with a blender file extension.
 */
bool BLO_has_bfile_extension(const char *str)
{
  const char *ext_test[4] = {".blend", ".ble", ".blend.gz", NULL};
  return BLI_path_extension_check_array(str, ext_test);
}

/**
 * Try to explode given path into its 'library components'
 * (i.e. a .blend file, id type/group, and data-block itself).
 *
 * \param path: the full path to explode.
 * \param r_dir: the string that'll contain path up to blend file itself ('library' path).
 * WARNING! Must be #FILE_MAX_LIBEXTRA long (it also stores group and name strings)!
 * \param r_group: the string that'll contain 'group' part of the path, if any. May be NULL.
 * \param r_name: the string that'll contain data's name part of the path, if any. May be NULL.
 * \return true if path contains a blend file.
 */
bool BLO_library_path_explode(const char *path, char *r_dir, char **r_group, char **r_name)
{
  /* We might get some data names with slashes,
   * so we have to go up in path until we find blend file itself,
   * then we now next path item is group, and everything else is data name. */
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

/**
 * Does a very light reading of given .blend file to extract its stored thumbnail.
 *
 * \param filepath: The path of the file to extract thumbnail from.
 * \return The raw thumbnail
 * (MEM-allocated, as stored in file, use #BKE_main_thumbnail_to_imbuf()
 * to convert it to ImBuf image).
 */
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
      const size_t sz = BLEN_THUMB_MEMSIZE(width, height);
      data = MEM_mallocN(sz, __func__);
      if (data) {
        BLI_assert((sz - sizeof(*data)) ==
                   (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*fd_data) * 2)));
        data->width = width;
        data->height = height;
        memcpy(data->rect, &fd_data[2], sz - sizeof(*data));
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

/* only direct databocks */
static void *newdataadr(FileData *fd, const void *adr)
{
  return oldnewmap_lookup_and_inc(fd->datamap, adr, true);
}

/* only direct databocks */
static void *newdataadr_no_us(FileData *fd, const void *adr)
{
  return oldnewmap_lookup_and_inc(fd->datamap, adr, false);
}

/* direct datablocks with global linking */
void *blo_read_get_new_globaldata_address(FileData *fd, const void *adr)
{
  return oldnewmap_lookup_and_inc(fd->globmap, adr, true);
}

/* used to restore packed data after undo */
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

/* only lib data */
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

/* lib linked proxy objects point to our local data, we need
 * to clear that pointer before reading the undo memfile since
 * the object might be removed, it is set again in reading
 * if the local object still exists.
 * This is only valid for local proxy objects though, linked ones should not be affected here.
 */
void blo_clear_proxy_pointers_from_lib(Main *oldmain)
{
  LISTBASE_FOREACH (Object *, ob, &oldmain->objects) {
    if (ob->id.lib != NULL && ob->proxy_from != NULL && ob->proxy_from->id.lib == NULL) {
      ob->proxy_from = NULL;
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

/* set old main packed data to zero if it has been restored */
/* this works because freeing old main only happens after this call */
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

/* undo file support: add all library pointers in lookup */
void blo_add_library_pointer_map(ListBase *old_mainlist, FileData *fd)
{
  ListBase *lbarray[MAX_LIBARRAY];

  LISTBASE_FOREACH (Main *, ptr, old_mainlist) {
    int i = set_listbasepointers(ptr, lbarray);
    while (i--) {
      LISTBASE_FOREACH (ID *, id, lbarray[i]) {
        oldnewmap_insert(fd->libmap, id, id, GS(id->name));
      }
    }
  }

  fd->old_mainlist = old_mainlist;
}

/* Build a GSet of old main (we only care about local data here, so we can do that after
 * split_main() call. */
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

/** Register a cache data entry to be preserved when reading some undo memfile. */
static void blo_cache_storage_entry_register(ID *id,
                                             const IDCacheKey *key,
                                             void **UNUSED(cache_p),
                                             uint UNUSED(flags),
                                             void *cache_storage_v)
{
  BLI_assert(key->id_session_uuid == id->session_uuid);
  UNUSED_VARS_NDEBUG(id);

  BLOCacheStorage *cache_storage = cache_storage_v;
  BLI_assert(!BLI_ghash_haskey(cache_storage->cache_map, key));

  IDCacheKey *storage_key = BLI_memarena_alloc(cache_storage->memarena, sizeof(*storage_key));
  *storage_key = *key;
  BLI_ghash_insert(cache_storage->cache_map, storage_key, POINTER_FROM_UINT(0));
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

  void **value = BLI_ghash_lookup_p(cache_storage->cache_map, key);
  if (value == NULL) {
    *cache_p = NULL;
    return;
  }
  *value = POINTER_FROM_UINT(POINTER_AS_UINT(*value) + 1);
  *cache_p = key->cache_v;
}

/** Clear as needed a cache data entry from old ID, when reading some undo memfile. */
static void blo_cache_storage_entry_clear_in_old(ID *UNUSED(id),
                                                 const IDCacheKey *key,
                                                 void **cache_p,
                                                 uint UNUSED(flags),
                                                 void *cache_storage_v)
{
  BLOCacheStorage *cache_storage = cache_storage_v;

  void **value = BLI_ghash_lookup_p(cache_storage->cache_map, key);
  if (value == NULL) {
    *cache_p = NULL;
    return;
  }
  /* If that cache has been restored into some new ID, we want to remove it from old one, otherwise
   * keep it there so that it gets properly freed together with its ID. */
  *cache_p = POINTER_AS_UINT(*value) != 0 ? NULL : key->cache_v;
}

void blo_cache_storage_init(FileData *fd, Main *bmain)
{
  if (fd->memfile != NULL) {
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
  BLI_assert(fd->memfile != NULL);
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
static void lib_link_collection(BlendLibReader *reader, Collection *collection);

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
      lib_link_collection(reader, scene->master_collection);
    }
  }
}

static void lib_link_id(BlendLibReader *reader, ID *id)
{
  /* Note: WM IDProperties are never written to file, hence they should always be NULL here. */
  BLI_assert((GS(id->name) != ID_WM) || id->properties == NULL);
  IDP_BlendReadLib(reader, id->properties);

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt != NULL) {
    BKE_animdata_blend_read_lib(reader, id, adt);
  }

  if (id->override_library) {
    BLO_read_id_address(reader, id->lib, &id->override_library->reference);
    BLO_read_id_address(reader, id->lib, &id->override_library->storage);
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
static void direct_link_collection(BlendDataReader *reader, Collection *collection);

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
      direct_link_collection(reader, scene->master_collection);
    }
  }
}

static int direct_link_id_restore_recalc_exceptions(const ID *id_current)
{
  /* Exception for armature objects, where the pose has direct points to the
   * armature databolock. */
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
    if (fd->undo_direction < 0) {
      /* Undo: tags from target to the current state. */
      recalc |= id_current->recalc_up_to_undo_push;
    }
    else {
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
    /* When actually reading a file , we do want to reset/re-generate session uuids.
     * In undo case, we want to re-use existing ones. */
    id->session_uuid = MAIN_ID_SESSION_UUID_UNSET;
  }

  BKE_lib_libblock_session_uuid_ensure(id);

  id->lib = current_library;
  id->us = ID_FAKE_USERS(id);
  id->icon_id = 0;
  id->newid = NULL; /* Needed because .blend may have been saved with crap value here... */
  id->orig_id = NULL;
  id->py_instance = NULL;

  /* Initialize with provided tag. */
  id->tag = tag;

  if (tag & LIB_TAG_ID_LINK_PLACEHOLDER) {
    /* For placeholder we only need to set the tag and properly initialize generic ID fields above,
     * no further data to read. */
    return;
  }

  /*link direct data of ID properties*/
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
    BLO_read_list_cb(
        reader, &id->override_library->properties, direct_link_id_override_property_cb);
    id->override_library->runtime = NULL;
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

/* XXX deprecated - old animation system */
static void lib_link_ipo(BlendLibReader *reader, Ipo *ipo)
{
  LISTBASE_FOREACH (IpoCurve *, icu, &ipo->curve) {
    if (icu->driver) {
      BLO_read_id_address(reader, ipo->id.lib, &icu->driver->ob);
    }
  }
}

/* XXX deprecated - old animation system */
static void direct_link_ipo(BlendDataReader *reader, Ipo *ipo)
{
  BLO_read_list(reader, &(ipo->curve));

  LISTBASE_FOREACH (IpoCurve *, icu, &ipo->curve) {
    BLO_read_data_address(reader, &icu->bezt);
    BLO_read_data_address(reader, &icu->bp);
    BLO_read_data_address(reader, &icu->driver);

    /* Undo generic endian switching. */
    if (BLO_read_requires_endian_switch(reader)) {
      BLI_endian_switch_int16(&icu->blocktype);
      if (icu->driver != NULL) {

        /* Undo generic endian switching. */
        if (BLO_read_requires_endian_switch(reader)) {
          BLI_endian_switch_int16(&icu->blocktype);
          if (icu->driver != NULL) {
            BLI_endian_switch_int16(&icu->driver->blocktype);
          }
        }
      }

      /* Undo generic endian switching. */
      if (BLO_read_requires_endian_switch(reader)) {
        BLI_endian_switch_int16(&ipo->blocktype);
        BLI_endian_switch_int16(&icu->driver->blocktype);
      }
    }
  }

  /* Undo generic endian switching. */
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_int16(&ipo->blocktype);
  }
}

/* XXX deprecated - old animation system */
static void lib_link_nlastrips(BlendLibReader *reader, ID *id, ListBase *striplist)
{
  LISTBASE_FOREACH (bActionStrip *, strip, striplist) {
    BLO_read_id_address(reader, id->lib, &strip->object);
    BLO_read_id_address(reader, id->lib, &strip->act);
    BLO_read_id_address(reader, id->lib, &strip->ipo);
    LISTBASE_FOREACH (bActionModifier *, amod, &strip->modifiers) {
      BLO_read_id_address(reader, id->lib, &amod->ob);
    }
  }
}

/* XXX deprecated - old animation system */
static void direct_link_nlastrips(BlendDataReader *reader, ListBase *strips)
{
  BLO_read_list(reader, strips);

  LISTBASE_FOREACH (bActionStrip *, strip, strips) {
    BLO_read_list(reader, &strip->modifiers);
  }
}

/* XXX deprecated - old animation system */
static void lib_link_constraint_channels(BlendLibReader *reader, ID *id, ListBase *chanbase)
{
  LISTBASE_FOREACH (bConstraintChannel *, chan, chanbase) {
    BLO_read_id_address(reader, id->lib, &chan->ipo);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: WorkSpace
 * \{ */

static void lib_link_workspaces(BlendLibReader *reader, WorkSpace *workspace)
{
  ID *id = (ID *)workspace;

  /* Restore proper 'parent' pointers to relevant data, and clean up unused/invalid entries. */
  LISTBASE_FOREACH_MUTABLE (WorkSpaceDataRelation *, relation, &workspace->hook_layout_relations) {
    relation->parent = NULL;
    LISTBASE_FOREACH (wmWindowManager *, wm, &reader->main->wm) {
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        if (win->winid == relation->parentid) {
          relation->parent = win->workspace_hook;
        }
      }
    }
    if (relation->parent == NULL) {
      BLI_freelinkN(&workspace->hook_layout_relations, relation);
    }
  }

  LISTBASE_FOREACH_MUTABLE (WorkSpaceLayout *, layout, &workspace->layouts) {
    BLO_read_id_address(reader, id->lib, &layout->screen);

    if (layout->screen) {
      if (ID_IS_LINKED(id)) {
        layout->screen->winid = 0;
        if (layout->screen->temp) {
          /* delete temp layouts when appending */
          BKE_workspace_layout_remove(reader->main, workspace, layout);
        }
      }
    }
    else {
      /* If we're reading a layout without screen stored, it's useless and we shouldn't keep it
       * around. */
      BKE_workspace_layout_remove(reader->main, workspace, layout);
    }
  }
}

static void direct_link_workspace(BlendDataReader *reader, WorkSpace *workspace)
{
  BLO_read_list(reader, &workspace->layouts);
  BLO_read_list(reader, &workspace->hook_layout_relations);
  BLO_read_list(reader, &workspace->owner_ids);
  BLO_read_list(reader, &workspace->tools);

  LISTBASE_FOREACH (WorkSpaceDataRelation *, relation, &workspace->hook_layout_relations) {
    /* parent pointer does not belong to workspace data and is therefore restored in lib_link step
     * of window manager.*/
    BLO_read_data_address(reader, &relation->value);
  }

  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    tref->runtime = NULL;
    BLO_read_data_address(reader, &tref->properties);
    IDP_BlendDataRead(reader, &tref->properties);
  }

  workspace->status_text = NULL;

  id_us_ensure_real(&workspace->id);
}

static void lib_link_workspace_instance_hook(BlendLibReader *reader,
                                             WorkSpaceInstanceHook *hook,
                                             ID *id)
{
  WorkSpace *workspace = BKE_workspace_active_get(hook);
  BLO_read_id_address(reader, id->lib, &workspace);
  BKE_workspace_active_set(hook, workspace);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Armature
 * \{ */

/* temp struct used to transport needed info to lib_link_constraint_cb() */
typedef struct tConstraintLinkData {
  BlendLibReader *reader;
  ID *id;
} tConstraintLinkData;
/* callback function used to relink constraint ID-links */
static void lib_link_constraint_cb(bConstraint *UNUSED(con),
                                   ID **idpoin,
                                   bool UNUSED(is_reference),
                                   void *userdata)
{
  tConstraintLinkData *cld = (tConstraintLinkData *)userdata;
  BLO_read_id_address(cld->reader, cld->id->lib, idpoin);
}

static void lib_link_constraints(BlendLibReader *reader, ID *id, ListBase *conlist)
{
  tConstraintLinkData cld;

  /* legacy fixes */
  LISTBASE_FOREACH (bConstraint *, con, conlist) {
    /* patch for error introduced by changing constraints (dunno how) */
    /* if con->data type changes, dna cannot resolve the pointer! (ton) */
    if (con->data == NULL) {
      con->type = CONSTRAINT_TYPE_NULL;
    }
    /* own ipo, all constraints have it */
    BLO_read_id_address(reader, id->lib, &con->ipo); /* XXX deprecated - old animation system */

    /* If linking from a library, clear 'local' library override flag. */
    if (id->lib != NULL) {
      con->flag &= ~CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;
    }
  }

  /* relink all ID-blocks used by the constraints */
  cld.reader = reader;
  cld.id = id;

  BKE_constraints_id_loop(conlist, lib_link_constraint_cb, &cld);
}

static void direct_link_constraints(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    BLO_read_data_address(reader, &con->data);

    switch (con->type) {
      case CONSTRAINT_TYPE_PYTHON: {
        bPythonConstraint *data = con->data;

        BLO_read_list(reader, &data->targets);

        BLO_read_data_address(reader, &data->prop);
        IDP_BlendDataRead(reader, &data->prop);
        break;
      }
      case CONSTRAINT_TYPE_ARMATURE: {
        bArmatureConstraint *data = con->data;

        BLO_read_list(reader, &data->targets);

        break;
      }
      case CONSTRAINT_TYPE_SPLINEIK: {
        bSplineIKConstraint *data = con->data;

        BLO_read_data_address(reader, &data->points);
        break;
      }
      case CONSTRAINT_TYPE_KINEMATIC: {
        bKinematicConstraint *data = con->data;

        con->lin_error = 0.f;
        con->rot_error = 0.f;

        /* version patch for runtime flag, was not cleared in some case */
        data->flag &= ~CONSTRAINT_IK_AUTO;
        break;
      }
      case CONSTRAINT_TYPE_CHILDOF: {
        /* XXX version patch, in older code this flag wasn't always set, and is inherent to type */
        if (con->ownspace == CONSTRAINT_SPACE_POSE) {
          con->flag |= CONSTRAINT_SPACEONCE;
        }
        break;
      }
      case CONSTRAINT_TYPE_TRANSFORM_CACHE: {
        bTransformCacheConstraint *data = con->data;
        data->reader = NULL;
        data->reader_object_path[0] = '\0';
      }
    }
  }
}

static void lib_link_pose(BlendLibReader *reader, Object *ob, bPose *pose)
{
  bArmature *arm = ob->data;

  if (!pose || !arm) {
    return;
  }

  /* always rebuild to match proxy or lib changes, but on Undo */
  bool rebuild = false;

  if (!BLO_read_lib_is_undo(reader)) {
    if (ob->proxy || ob->id.lib != arm->id.lib) {
      rebuild = true;
    }
  }

  if (ob->proxy) {
    /* sync proxy layer */
    if (pose->proxy_layer) {
      arm->layer = pose->proxy_layer;
    }

    /* sync proxy active bone */
    if (pose->proxy_act_bone[0]) {
      Bone *bone = BKE_armature_find_bone_name(arm, pose->proxy_act_bone);
      if (bone) {
        arm->act_bone = bone;
      }
    }
  }

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    lib_link_constraints(reader, (ID *)ob, &pchan->constraints);

    pchan->bone = BKE_armature_find_bone_name(arm, pchan->name);

    IDP_BlendReadLib(reader, pchan->prop);

    BLO_read_id_address(reader, arm->id.lib, &pchan->custom);
    if (UNLIKELY(pchan->bone == NULL)) {
      rebuild = true;
    }
    else if ((ob->id.lib == NULL) && arm->id.lib) {
      /* local pose selection copied to armature, bit hackish */
      pchan->bone->flag &= ~BONE_SELECTED;
      pchan->bone->flag |= pchan->selectflag;
    }
  }

  if (rebuild) {
    DEG_id_tag_update_ex(
        reader->main, &ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
    BKE_pose_tag_recalc(reader->main, pose);
  }
}

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
/** \name Read ID: Particle Settings
 * \{ */

/* update this also to writefile.c */
static const char *ptcache_data_struct[] = {
    "",         /* BPHYS_DATA_INDEX */
    "",         /* BPHYS_DATA_LOCATION */
    "",         /* BPHYS_DATA_VELOCITY */
    "",         /* BPHYS_DATA_ROTATION */
    "",         /* BPHYS_DATA_AVELOCITY / BPHYS_DATA_XCONST */
    "",         /* BPHYS_DATA_SIZE: */
    "",         /* BPHYS_DATA_TIMES: */
    "BoidData", /* case BPHYS_DATA_BOIDS: */
};

static void direct_link_pointcache_cb(BlendDataReader *reader, void *data)
{
  PTCacheMem *pm = data;
  for (int i = 0; i < BPHYS_TOT_DATA; i++) {
    BLO_read_data_address(reader, &pm->data[i]);

    /* the cache saves non-struct data without DNA */
    if (pm->data[i] && ptcache_data_struct[i][0] == '\0' &&
        BLO_read_requires_endian_switch(reader)) {
      /* data_size returns bytes. */
      int tot = (BKE_ptcache_data_size(i) * pm->totpoint) / sizeof(int);

      int *poin = pm->data[i];

      BLI_endian_switch_int32_array(poin, tot);
    }
  }

  BLO_read_list(reader, &pm->extradata);

  LISTBASE_FOREACH (PTCacheExtra *, extra, &pm->extradata) {
    BLO_read_data_address(reader, &extra->data);
  }
}

static void direct_link_pointcache(BlendDataReader *reader, PointCache *cache)
{
  if ((cache->flag & PTCACHE_DISK_CACHE) == 0) {
    BLO_read_list_cb(reader, &cache->mem_cache, direct_link_pointcache_cb);
  }
  else {
    BLI_listbase_clear(&cache->mem_cache);
  }

  cache->flag &= ~PTCACHE_SIMULATION_VALID;
  cache->simframe = 0;
  cache->edit = NULL;
  cache->free_edit = NULL;
  cache->cached_frames = NULL;
  cache->cached_frames_len = 0;
}

static void direct_link_pointcache_list(BlendDataReader *reader,
                                        ListBase *ptcaches,
                                        PointCache **ocache,
                                        int force_disk)
{
  if (ptcaches->first) {
    BLO_read_list(reader, ptcaches);
    LISTBASE_FOREACH (PointCache *, cache, ptcaches) {
      direct_link_pointcache(reader, cache);
      if (force_disk) {
        cache->flag |= PTCACHE_DISK_CACHE;
        cache->step = 1;
      }
    }

    BLO_read_data_address(reader, ocache);
  }
  else if (*ocache) {
    /* old "single" caches need to be linked too */
    BLO_read_data_address(reader, ocache);
    direct_link_pointcache(reader, *ocache);
    if (force_disk) {
      (*ocache)->flag |= PTCACHE_DISK_CACHE;
      (*ocache)->step = 1;
    }

    ptcaches->first = ptcaches->last = *ocache;
  }
}

static void lib_link_partdeflect(BlendLibReader *reader, ID *id, PartDeflect *pd)
{
  if (pd && pd->tex) {
    BLO_read_id_address(reader, id->lib, &pd->tex);
  }
  if (pd && pd->f_source) {
    BLO_read_id_address(reader, id->lib, &pd->f_source);
  }
}

static void lib_link_particlesettings(BlendLibReader *reader, ParticleSettings *part)
{
  BLO_read_id_address(
      reader, part->id.lib, &part->ipo); /* XXX deprecated - old animation system */

  BLO_read_id_address(reader, part->id.lib, &part->instance_object);
  BLO_read_id_address(reader, part->id.lib, &part->instance_collection);
  BLO_read_id_address(reader, part->id.lib, &part->force_group);
  BLO_read_id_address(reader, part->id.lib, &part->bb_ob);
  BLO_read_id_address(reader, part->id.lib, &part->collision_group);

  lib_link_partdeflect(reader, &part->id, part->pd);
  lib_link_partdeflect(reader, &part->id, part->pd2);

  if (part->effector_weights) {
    BLO_read_id_address(reader, part->id.lib, &part->effector_weights->group);
  }
  else {
    part->effector_weights = BKE_effector_add_weights(part->force_group);
  }

  if (part->instance_weights.first && part->instance_collection) {
    LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
      BLO_read_id_address(reader, part->id.lib, &dw->ob);
    }
  }
  else {
    BLI_listbase_clear(&part->instance_weights);
  }

  if (part->boids) {
    LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
      LISTBASE_FOREACH (BoidRule *, rule, &state->rules) {
        switch (rule->type) {
          case eBoidRuleType_Goal:
          case eBoidRuleType_Avoid: {
            BoidRuleGoalAvoid *brga = (BoidRuleGoalAvoid *)rule;
            BLO_read_id_address(reader, part->id.lib, &brga->ob);
            break;
          }
          case eBoidRuleType_FollowLeader: {
            BoidRuleFollowLeader *brfl = (BoidRuleFollowLeader *)rule;
            BLO_read_id_address(reader, part->id.lib, &brfl->ob);
            break;
          }
        }
      }
    }
  }

  for (int a = 0; a < MAX_MTEX; a++) {
    MTex *mtex = part->mtex[a];
    if (mtex) {
      BLO_read_id_address(reader, part->id.lib, &mtex->tex);
      BLO_read_id_address(reader, part->id.lib, &mtex->object);
    }
  }
}

static void direct_link_partdeflect(PartDeflect *pd)
{
  if (pd) {
    pd->rng = NULL;
  }
}

static void direct_link_particlesettings(BlendDataReader *reader, ParticleSettings *part)
{
  BLO_read_data_address(reader, &part->adt);
  BLO_read_data_address(reader, &part->pd);
  BLO_read_data_address(reader, &part->pd2);

  BKE_animdata_blend_read_data(reader, part->adt);
  direct_link_partdeflect(part->pd);
  direct_link_partdeflect(part->pd2);

  BLO_read_data_address(reader, &part->clumpcurve);
  if (part->clumpcurve) {
    BKE_curvemapping_blend_read(reader, part->clumpcurve);
  }
  BLO_read_data_address(reader, &part->roughcurve);
  if (part->roughcurve) {
    BKE_curvemapping_blend_read(reader, part->roughcurve);
  }
  BLO_read_data_address(reader, &part->twistcurve);
  if (part->twistcurve) {
    BKE_curvemapping_blend_read(reader, part->twistcurve);
  }

  BLO_read_data_address(reader, &part->effector_weights);
  if (!part->effector_weights) {
    part->effector_weights = BKE_effector_add_weights(part->force_group);
  }

  BLO_read_list(reader, &part->instance_weights);

  BLO_read_data_address(reader, &part->boids);
  BLO_read_data_address(reader, &part->fluid);

  if (part->boids) {
    BLO_read_list(reader, &part->boids->states);

    LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
      BLO_read_list(reader, &state->rules);
      BLO_read_list(reader, &state->conditions);
      BLO_read_list(reader, &state->actions);
    }
  }
  for (int a = 0; a < MAX_MTEX; a++) {
    BLO_read_data_address(reader, &part->mtex[a]);
  }

  /* Protect against integer overflow vulnerability. */
  CLAMP(part->trail_count, 1, 100000);
}

static void lib_link_particlesystems(BlendLibReader *reader,
                                     Object *ob,
                                     ID *id,
                                     ListBase *particles)
{
  LISTBASE_FOREACH_MUTABLE (ParticleSystem *, psys, particles) {

    BLO_read_id_address(reader, id->lib, &psys->part);
    if (psys->part) {
      LISTBASE_FOREACH (ParticleTarget *, pt, &psys->targets) {
        BLO_read_id_address(reader, id->lib, &pt->ob);
      }

      BLO_read_id_address(reader, id->lib, &psys->parent);
      BLO_read_id_address(reader, id->lib, &psys->target_ob);

      if (psys->clmd) {
        /* XXX - from reading existing code this seems correct but intended usage of
         * pointcache /w cloth should be added in 'ParticleSystem' - campbell */
        psys->clmd->point_cache = psys->pointcache;
        psys->clmd->ptcaches.first = psys->clmd->ptcaches.last = NULL;
        BLO_read_id_address(reader, id->lib, &psys->clmd->coll_parms->group);
        psys->clmd->modifier.error = NULL;
      }
    }
    else {
      /* particle modifier must be removed before particle system */
      ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
      BLI_remlink(&ob->modifiers, psmd);
      BKE_modifier_free((ModifierData *)psmd);

      BLI_remlink(particles, psys);
      MEM_freeN(psys);
    }
  }
}
static void direct_link_particlesystems(BlendDataReader *reader, ListBase *particles)
{
  ParticleData *pa;
  int a;

  LISTBASE_FOREACH (ParticleSystem *, psys, particles) {
    BLO_read_data_address(reader, &psys->particles);

    if (psys->particles && psys->particles->hair) {
      for (a = 0, pa = psys->particles; a < psys->totpart; a++, pa++) {
        BLO_read_data_address(reader, &pa->hair);
      }
    }

    if (psys->particles && psys->particles->keys) {
      for (a = 0, pa = psys->particles; a < psys->totpart; a++, pa++) {
        pa->keys = NULL;
        pa->totkey = 0;
      }

      psys->flag &= ~PSYS_KEYED;
    }

    if (psys->particles && psys->particles->boid) {
      pa = psys->particles;
      BLO_read_data_address(reader, &pa->boid);

      /* This is purely runtime data, but still can be an issue if left dangling. */
      pa->boid->ground = NULL;

      for (a = 1, pa++; a < psys->totpart; a++, pa++) {
        pa->boid = (pa - 1)->boid + 1;
        pa->boid->ground = NULL;
      }
    }
    else if (psys->particles) {
      for (a = 0, pa = psys->particles; a < psys->totpart; a++, pa++) {
        pa->boid = NULL;
      }
    }

    BLO_read_data_address(reader, &psys->fluid_springs);

    BLO_read_data_address(reader, &psys->child);
    psys->effectors = NULL;

    BLO_read_list(reader, &psys->targets);

    psys->edit = NULL;
    psys->free_edit = NULL;
    psys->pathcache = NULL;
    psys->childcache = NULL;
    BLI_listbase_clear(&psys->pathcachebufs);
    BLI_listbase_clear(&psys->childcachebufs);
    psys->pdd = NULL;

    if (psys->clmd) {
      BLO_read_data_address(reader, &psys->clmd);
      psys->clmd->clothObject = NULL;
      psys->clmd->hairdata = NULL;

      BLO_read_data_address(reader, &psys->clmd->sim_parms);
      BLO_read_data_address(reader, &psys->clmd->coll_parms);

      if (psys->clmd->sim_parms) {
        psys->clmd->sim_parms->effector_weights = NULL;
        if (psys->clmd->sim_parms->presets > 10) {
          psys->clmd->sim_parms->presets = 0;
        }
      }

      psys->hair_in_mesh = psys->hair_out_mesh = NULL;
      psys->clmd->solver_result = NULL;
    }

    direct_link_pointcache_list(reader, &psys->ptcaches, &psys->pointcache, 0);
    if (psys->clmd) {
      psys->clmd->point_cache = psys->pointcache;
    }

    psys->tree = NULL;
    psys->bvhtree = NULL;

    psys->orig_psys = NULL;
    psys->batch_cache = NULL;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Mesh
 * \{ */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Object
 * \{ */

static void lib_link_modifiers_common(void *userData, Object *ob, ID **idpoin, int cb_flag)
{
  BlendLibReader *reader = userData;

  BLO_read_id_address(reader, ob->id.lib, idpoin);
  if (*idpoin != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus_no_lib(*idpoin);
  }
}

static void lib_link_modifiers(BlendLibReader *reader, Object *ob)
{
  BKE_modifiers_foreach_ID_link(ob, lib_link_modifiers_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ob->id.lib != NULL) {
    LISTBASE_FOREACH (ModifierData *, mod, &ob->modifiers) {
      mod->flag &= ~eModifierFlag_OverrideLibrary_Local;
    }
  }
}

static void lib_link_gpencil_modifiers(BlendLibReader *reader, Object *ob)
{
  BKE_gpencil_modifiers_foreach_ID_link(ob, lib_link_modifiers_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ob->id.lib != NULL) {
    LISTBASE_FOREACH (GpencilModifierData *, mod, &ob->greasepencil_modifiers) {
      mod->flag &= ~eGpencilModifierFlag_OverrideLibrary_Local;
    }
  }
}

static void lib_link_shaderfxs(BlendLibReader *reader, Object *ob)
{
  BKE_shaderfx_foreach_ID_link(ob, lib_link_modifiers_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ob->id.lib != NULL) {
    LISTBASE_FOREACH (ShaderFxData *, fx, &ob->shader_fx) {
      fx->flag &= ~eShaderFxFlag_OverrideLibrary_Local;
    }
  }
}

static void lib_link_object(BlendLibReader *reader, Object *ob)
{
  bool warn = false;

  /* XXX deprecated - old animation system <<< */
  BLO_read_id_address(reader, ob->id.lib, &ob->ipo);
  BLO_read_id_address(reader, ob->id.lib, &ob->action);
  /* >>> XXX deprecated - old animation system */

  BLO_read_id_address(reader, ob->id.lib, &ob->parent);
  BLO_read_id_address(reader, ob->id.lib, &ob->track);
  BLO_read_id_address(reader, ob->id.lib, &ob->poselib);

  /* 2.8x drops support for non-empty dupli instances. */
  if (ob->type == OB_EMPTY) {
    BLO_read_id_address(reader, ob->id.lib, &ob->instance_collection);
  }
  else {
    if (ob->instance_collection != NULL) {
      ID *id = BLO_read_get_new_id_address(reader, ob->id.lib, &ob->instance_collection->id);
      blo_reportf_wrap(reader->fd->reports,
                       RPT_WARNING,
                       TIP_("Non-Empty object '%s' cannot duplicate collection '%s' "
                            "anymore in Blender 2.80, removed instancing"),
                       ob->id.name + 2,
                       id->name + 2);
    }
    ob->instance_collection = NULL;
    ob->transflag &= ~OB_DUPLICOLLECTION;
  }

  BLO_read_id_address(reader, ob->id.lib, &ob->proxy);
  if (ob->proxy) {
    /* paranoia check, actually a proxy_from pointer should never be written... */
    if (ob->proxy->id.lib == NULL) {
      ob->proxy->proxy_from = NULL;
      ob->proxy = NULL;

      if (ob->id.lib) {
        printf("Proxy lost from  object %s lib %s\n", ob->id.name + 2, ob->id.lib->filepath);
      }
      else {
        printf("Proxy lost from  object %s lib <NONE>\n", ob->id.name + 2);
      }
    }
    else {
      /* this triggers object_update to always use a copy */
      ob->proxy->proxy_from = ob;
    }
  }
  BLO_read_id_address(reader, ob->id.lib, &ob->proxy_group);

  void *poin = ob->data;
  BLO_read_id_address(reader, ob->id.lib, &ob->data);

  if (ob->data == NULL && poin != NULL) {
    if (ob->id.lib) {
      printf("Can't find obdata of %s lib %s\n", ob->id.name + 2, ob->id.lib->filepath);
    }
    else {
      printf("Object %s lost data.\n", ob->id.name + 2);
    }

    ob->type = OB_EMPTY;
    warn = true;

    if (ob->pose) {
      /* we can't call #BKE_pose_free() here because of library linking
       * freeing will recurse down into every pose constraints ID pointers
       * which are not always valid, so for now free directly and suffer
       * some leaked memory rather then crashing immediately
       * while bad this _is_ an exceptional case - campbell */
#if 0
      BKE_pose_free(ob->pose);
#else
      MEM_freeN(ob->pose);
#endif
      ob->pose = NULL;
      ob->mode &= ~OB_MODE_POSE;
    }
  }
  for (int a = 0; a < ob->totcol; a++) {
    BLO_read_id_address(reader, ob->id.lib, &ob->mat[a]);
  }

  /* When the object is local and the data is library its possible
   * the material list size gets out of sync. T22663. */
  if (ob->data && ob->id.lib != ((ID *)ob->data)->lib) {
    const short *totcol_data = BKE_object_material_len_p(ob);
    /* Only expand so as not to loose any object materials that might be set. */
    if (totcol_data && (*totcol_data > ob->totcol)) {
      /* printf("'%s' %d -> %d\n", ob->id.name, ob->totcol, *totcol_data); */
      BKE_object_material_resize(reader->main, ob, *totcol_data, false);
    }
  }

  BLO_read_id_address(reader, ob->id.lib, &ob->gpd);

  /* if id.us==0 a new base will be created later on */

  /* WARNING! Also check expand_object(), should reflect the stuff below. */
  lib_link_pose(reader, ob, ob->pose);
  lib_link_constraints(reader, &ob->id, &ob->constraints);

  /* XXX deprecated - old animation system <<< */
  lib_link_constraint_channels(reader, &ob->id, &ob->constraintChannels);
  lib_link_nlastrips(reader, &ob->id, &ob->nlastrips);
  /* >>> XXX deprecated - old animation system */

  LISTBASE_FOREACH (PartEff *, paf, &ob->effect) {
    if (paf->type == EFF_PARTICLE) {
      BLO_read_id_address(reader, ob->id.lib, &paf->group);
    }
  }

  {
    FluidsimModifierData *fluidmd = (FluidsimModifierData *)BKE_modifiers_findby_type(
        ob, eModifierType_Fluidsim);

    if (fluidmd && fluidmd->fss) {
      BLO_read_id_address(
          reader, ob->id.lib, &fluidmd->fss->ipo); /* XXX deprecated - old animation system */
    }
  }

  {
    FluidModifierData *fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob,
                                                                            eModifierType_Fluid);

    if (fmd && (fmd->type == MOD_FLUID_TYPE_DOMAIN) && fmd->domain) {
      /* Flag for refreshing the simulation after loading */
      fmd->domain->flags |= FLUID_DOMAIN_FILE_LOAD;
    }
    else if (fmd && (fmd->type == MOD_FLUID_TYPE_FLOW) && fmd->flow) {
      fmd->flow->flags &= ~FLUID_FLOW_NEEDS_UPDATE;
    }
    else if (fmd && (fmd->type == MOD_FLUID_TYPE_EFFEC) && fmd->effector) {
      fmd->effector->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;
    }
  }

  /* texture field */
  if (ob->pd) {
    lib_link_partdeflect(reader, &ob->id, ob->pd);
  }

  if (ob->soft) {
    BLO_read_id_address(reader, ob->id.lib, &ob->soft->collision_group);

    BLO_read_id_address(reader, ob->id.lib, &ob->soft->effector_weights->group);
  }

  lib_link_particlesystems(reader, ob, &ob->id, &ob->particlesystem);
  lib_link_modifiers(reader, ob);
  lib_link_gpencil_modifiers(reader, ob);
  lib_link_shaderfxs(reader, ob);

  if (ob->rigidbody_constraint) {
    BLO_read_id_address(reader, ob->id.lib, &ob->rigidbody_constraint->ob1);
    BLO_read_id_address(reader, ob->id.lib, &ob->rigidbody_constraint->ob2);
  }

  if (warn) {
    BKE_report(reader->fd->reports, RPT_WARNING, "Warning in console");
  }
}

/* direct data for cache */
static void direct_link_motionpath(BlendDataReader *reader, bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == NULL) {
    return;
  }

  /* relink points cache */
  BLO_read_data_address(reader, &mpath->points);

  mpath->points_vbo = NULL;
  mpath->batch_line = NULL;
  mpath->batch_points = NULL;
}

static void direct_link_pose(BlendDataReader *reader, bPose *pose)
{
  if (!pose) {
    return;
  }

  BLO_read_list(reader, &pose->chanbase);
  BLO_read_list(reader, &pose->agroups);

  pose->chanhash = NULL;
  pose->chan_array = NULL;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    BKE_pose_channel_runtime_reset(&pchan->runtime);
    BKE_pose_channel_session_uuid_generate(pchan);

    pchan->bone = NULL;
    BLO_read_data_address(reader, &pchan->parent);
    BLO_read_data_address(reader, &pchan->child);
    BLO_read_data_address(reader, &pchan->custom_tx);

    BLO_read_data_address(reader, &pchan->bbone_prev);
    BLO_read_data_address(reader, &pchan->bbone_next);

    direct_link_constraints(reader, &pchan->constraints);

    BLO_read_data_address(reader, &pchan->prop);
    IDP_BlendDataRead(reader, &pchan->prop);

    BLO_read_data_address(reader, &pchan->mpath);
    if (pchan->mpath) {
      direct_link_motionpath(reader, pchan->mpath);
    }

    BLI_listbase_clear(&pchan->iktree);
    BLI_listbase_clear(&pchan->siktree);

    /* in case this value changes in future, clamp else we get undefined behavior */
    CLAMP(pchan->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

    pchan->draw_data = NULL;
  }
  pose->ikdata = NULL;
  if (pose->ikparam != NULL) {
    BLO_read_data_address(reader, &pose->ikparam);
  }
}

/* TODO(sergey): Find a better place for this.
 *
 * Unfortunately, this can not be done as a regular do_versions() since the modifier type is
 * set to NONE, so the do_versions code wouldn't know where the modifier came from.
 *
 * The best approach seems to have the functionality in versioning_280.c but still call the
 * function from #direct_link_modifiers().
 */

/* Domain, inflow, ... */
static void modifier_ensure_type(FluidModifierData *fluid_modifier_data, int type)
{
  fluid_modifier_data->type = type;
  BKE_fluid_modifier_free(fluid_modifier_data);
  BKE_fluid_modifier_create_type_data(fluid_modifier_data);
}

/**
 * \note The old_modifier_data is NOT linked.
 * This means that in order to access sub-data pointers #newdataadr is to be used.
 */
static ModifierData *modifier_replace_with_fluid(FileData *fd,
                                                 Object *object,
                                                 ListBase *modifiers,
                                                 ModifierData *old_modifier_data)
{
  ModifierData *new_modifier_data = BKE_modifier_new(eModifierType_Fluid);
  FluidModifierData *fluid_modifier_data = (FluidModifierData *)new_modifier_data;

  if (old_modifier_data->type == eModifierType_Fluidsim) {
    FluidsimModifierData *old_fluidsim_modifier_data = (FluidsimModifierData *)old_modifier_data;
    FluidsimSettings *old_fluidsim_settings = newdataadr(fd, old_fluidsim_modifier_data->fss);
    switch (old_fluidsim_settings->type) {
      case OB_FLUIDSIM_ENABLE:
        modifier_ensure_type(fluid_modifier_data, 0);
        break;
      case OB_FLUIDSIM_DOMAIN:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_DOMAIN);
        BKE_fluid_domain_type_set(object, fluid_modifier_data->domain, FLUID_DOMAIN_TYPE_LIQUID);
        break;
      case OB_FLUIDSIM_FLUID:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_FLOW);
        BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_LIQUID);
        /* No need to emit liquid far away from surface. */
        fluid_modifier_data->flow->surface_distance = 0.0f;
        break;
      case OB_FLUIDSIM_OBSTACLE:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_EFFEC);
        BKE_fluid_effector_type_set(
            object, fluid_modifier_data->effector, FLUID_EFFECTOR_TYPE_COLLISION);
        break;
      case OB_FLUIDSIM_INFLOW:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_FLOW);
        BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_LIQUID);
        BKE_fluid_flow_behavior_set(object, fluid_modifier_data->flow, FLUID_FLOW_BEHAVIOR_INFLOW);
        /* No need to emit liquid far away from surface. */
        fluid_modifier_data->flow->surface_distance = 0.0f;
        break;
      case OB_FLUIDSIM_OUTFLOW:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_FLOW);
        BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_LIQUID);
        BKE_fluid_flow_behavior_set(
            object, fluid_modifier_data->flow, FLUID_FLOW_BEHAVIOR_OUTFLOW);
        break;
      case OB_FLUIDSIM_PARTICLE:
        /* "Particle" type objects not being used by Mantaflow fluid simulations.
         * Skip this object, secondary particles can only be enabled through the domain object. */
        break;
      case OB_FLUIDSIM_CONTROL:
        /* "Control" type objects not being used by Mantaflow fluid simulations.
         * Use guiding type instead which is similar. */
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_EFFEC);
        BKE_fluid_effector_type_set(
            object, fluid_modifier_data->effector, FLUID_EFFECTOR_TYPE_GUIDE);
        break;
    }
  }
  else if (old_modifier_data->type == eModifierType_Smoke) {
    SmokeModifierData *old_smoke_modifier_data = (SmokeModifierData *)old_modifier_data;
    modifier_ensure_type(fluid_modifier_data, old_smoke_modifier_data->type);
    if (fluid_modifier_data->type == MOD_FLUID_TYPE_DOMAIN) {
      BKE_fluid_domain_type_set(object, fluid_modifier_data->domain, FLUID_DOMAIN_TYPE_GAS);
    }
    else if (fluid_modifier_data->type == MOD_FLUID_TYPE_FLOW) {
      BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_SMOKE);
    }
    else if (fluid_modifier_data->type == MOD_FLUID_TYPE_EFFEC) {
      BKE_fluid_effector_type_set(
          object, fluid_modifier_data->effector, FLUID_EFFECTOR_TYPE_COLLISION);
    }
  }

  /* Replace modifier data in the stack. */
  new_modifier_data->next = old_modifier_data->next;
  new_modifier_data->prev = old_modifier_data->prev;
  if (new_modifier_data->prev != NULL) {
    new_modifier_data->prev->next = new_modifier_data;
  }
  if (new_modifier_data->next != NULL) {
    new_modifier_data->next->prev = new_modifier_data;
  }
  if (modifiers->first == old_modifier_data) {
    modifiers->first = new_modifier_data;
  }
  if (modifiers->last == old_modifier_data) {
    modifiers->last = new_modifier_data;
  }

  /* Free old modifier data. */
  MEM_freeN(old_modifier_data);

  return new_modifier_data;
}

static void direct_link_modifiers(BlendDataReader *reader, ListBase *lb, Object *ob)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (ModifierData *, md, lb) {
    BKE_modifier_session_uuid_generate(md);

    md->error = NULL;
    md->runtime = NULL;

    /* Modifier data has been allocated as a part of data migration process and
     * no reading of nested fields from file is needed. */
    bool is_allocated = false;

    if (md->type == eModifierType_Fluidsim) {
      blo_reportf_wrap(
          reader->fd->reports,
          RPT_WARNING,
          TIP_("Possible data loss when saving this file! %s modifier is deprecated (Object: %s)"),
          md->name,
          ob->id.name + 2);
      md = modifier_replace_with_fluid(reader->fd, ob, lb, md);
      is_allocated = true;
    }
    else if (md->type == eModifierType_Smoke) {
      blo_reportf_wrap(
          reader->fd->reports,
          RPT_WARNING,
          TIP_("Possible data loss when saving this file! %s modifier is deprecated (Object: %s)"),
          md->name,
          ob->id.name + 2);
      md = modifier_replace_with_fluid(reader->fd, ob, lb, md);
      is_allocated = true;
    }

    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    /* if modifiers disappear, or for upward compatibility */
    if (mti == NULL) {
      md->type = eModifierType_None;
    }

    if (is_allocated) {
      /* All the fields has been properly allocated. */
    }
    else if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;

      clmd->clothObject = NULL;
      clmd->hairdata = NULL;

      BLO_read_data_address(reader, &clmd->sim_parms);
      BLO_read_data_address(reader, &clmd->coll_parms);

      direct_link_pointcache_list(reader, &clmd->ptcaches, &clmd->point_cache, 0);

      if (clmd->sim_parms) {
        if (clmd->sim_parms->presets > 10) {
          clmd->sim_parms->presets = 0;
        }

        clmd->sim_parms->reset = 0;

        BLO_read_data_address(reader, &clmd->sim_parms->effector_weights);

        if (!clmd->sim_parms->effector_weights) {
          clmd->sim_parms->effector_weights = BKE_effector_add_weights(NULL);
        }
      }

      clmd->solver_result = NULL;
    }
    else if (md->type == eModifierType_Fluid) {

      FluidModifierData *fmd = (FluidModifierData *)md;

      if (fmd->type == MOD_FLUID_TYPE_DOMAIN) {
        fmd->flow = NULL;
        fmd->effector = NULL;
        BLO_read_data_address(reader, &fmd->domain);
        fmd->domain->fmd = fmd;

        fmd->domain->fluid = NULL;
        fmd->domain->fluid_mutex = BLI_rw_mutex_alloc();
        fmd->domain->tex_density = NULL;
        fmd->domain->tex_color = NULL;
        fmd->domain->tex_shadow = NULL;
        fmd->domain->tex_flame = NULL;
        fmd->domain->tex_flame_coba = NULL;
        fmd->domain->tex_coba = NULL;
        fmd->domain->tex_field = NULL;
        fmd->domain->tex_velocity_x = NULL;
        fmd->domain->tex_velocity_y = NULL;
        fmd->domain->tex_velocity_z = NULL;
        fmd->domain->tex_wt = NULL;
        fmd->domain->mesh_velocities = NULL;
        BLO_read_data_address(reader, &fmd->domain->coba);

        BLO_read_data_address(reader, &fmd->domain->effector_weights);
        if (!fmd->domain->effector_weights) {
          fmd->domain->effector_weights = BKE_effector_add_weights(NULL);
        }

        direct_link_pointcache_list(
            reader, &(fmd->domain->ptcaches[0]), &(fmd->domain->point_cache[0]), 1);

        /* Manta sim uses only one cache from now on, so store pointer convert */
        if (fmd->domain->ptcaches[1].first || fmd->domain->point_cache[1]) {
          if (fmd->domain->point_cache[1]) {
            PointCache *cache = BLO_read_get_new_data_address(reader, fmd->domain->point_cache[1]);
            if (cache->flag & PTCACHE_FAKE_SMOKE) {
              /* Manta-sim/smoke was already saved in "new format" and this cache is a fake one. */
            }
            else {
              printf(
                  "High resolution manta cache not available due to pointcache update. Please "
                  "reset the simulation.\n");
            }
            BKE_ptcache_free(cache);
          }
          BLI_listbase_clear(&fmd->domain->ptcaches[1]);
          fmd->domain->point_cache[1] = NULL;
        }
      }
      else if (fmd->type == MOD_FLUID_TYPE_FLOW) {
        fmd->domain = NULL;
        fmd->effector = NULL;
        BLO_read_data_address(reader, &fmd->flow);
        fmd->flow->fmd = fmd;
        fmd->flow->mesh = NULL;
        fmd->flow->verts_old = NULL;
        fmd->flow->numverts = 0;
        BLO_read_data_address(reader, &fmd->flow->psys);
      }
      else if (fmd->type == MOD_FLUID_TYPE_EFFEC) {
        fmd->flow = NULL;
        fmd->domain = NULL;
        BLO_read_data_address(reader, &fmd->effector);
        if (fmd->effector) {
          fmd->effector->fmd = fmd;
          fmd->effector->verts_old = NULL;
          fmd->effector->numverts = 0;
          fmd->effector->mesh = NULL;
        }
        else {
          fmd->type = 0;
          fmd->flow = NULL;
          fmd->domain = NULL;
          fmd->effector = NULL;
        }
      }
    }
    else if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

      if (pmd->canvas) {
        BLO_read_data_address(reader, &pmd->canvas);
        pmd->canvas->pmd = pmd;
        pmd->canvas->flags &= ~MOD_DPAINT_BAKING; /* just in case */

        if (pmd->canvas->surfaces.first) {
          BLO_read_list(reader, &pmd->canvas->surfaces);

          LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
            surface->canvas = pmd->canvas;
            surface->data = NULL;
            direct_link_pointcache_list(reader, &(surface->ptcaches), &(surface->pointcache), 1);

            BLO_read_data_address(reader, &surface->effector_weights);
            if (surface->effector_weights == NULL) {
              surface->effector_weights = BKE_effector_add_weights(NULL);
            }
          }
        }
      }
      if (pmd->brush) {
        BLO_read_data_address(reader, &pmd->brush);
        pmd->brush->pmd = pmd;
        BLO_read_data_address(reader, &pmd->brush->psys);
        BLO_read_data_address(reader, &pmd->brush->paint_ramp);
        BLO_read_data_address(reader, &pmd->brush->vel_ramp);
      }
    }

    if (mti->blendRead != NULL) {
      mti->blendRead(reader, md);
    }
  }
}

static void direct_link_gpencil_modifiers(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (GpencilModifierData *, md, lb) {
    md->error = NULL;

    /* if modifiers disappear, or for upward compatibility */
    if (NULL == BKE_gpencil_modifier_get_info(md->type)) {
      md->type = eModifierType_None;
    }

    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;
      gpmd->cache_data = NULL;
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *hmd = (HookGpencilModifierData *)md;

      BLO_read_data_address(reader, &hmd->curfalloff);
      if (hmd->curfalloff) {
        BKE_curvemapping_blend_read(reader, hmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        /* initialize the curve. Maybe this could be moved to modififer logic */
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      BLO_read_data_address(reader, &gpmd->curve_thickness);
      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_thickness);
        BKE_curvemapping_init(gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->colorband);
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
  }
}

static void direct_link_shaderfxs(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (ShaderFxData *, fx, lb) {
    fx->error = NULL;

    /* if shader disappear, or for upward compatibility */
    if (NULL == BKE_shaderfx_get_info(fx->type)) {
      fx->type = eShaderFxType_None;
    }
  }
}

static void direct_link_object(BlendDataReader *reader, Object *ob)
{
  PartEff *paf;

  /* XXX This should not be needed - but seems like it can happen in some cases,
   * so for now play safe. */
  ob->proxy_from = NULL;

  /* loading saved files with editmode enabled works, but for undo we like
   * to stay in object mode during undo presses so keep editmode disabled.
   *
   * Also when linking in a file don't allow edit and pose modes.
   * See [T34776, T42780] for more information.
   */
  const bool is_undo = BLO_read_data_is_undo(reader);
  if (is_undo || (ob->id.tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT))) {
    ob->mode &= ~(OB_MODE_EDIT | OB_MODE_PARTICLE_EDIT);
    if (!is_undo) {
      ob->mode &= ~OB_MODE_POSE;
    }
  }

  BLO_read_data_address(reader, &ob->adt);
  BKE_animdata_blend_read_data(reader, ob->adt);

  BLO_read_data_address(reader, &ob->pose);
  direct_link_pose(reader, ob->pose);

  BLO_read_data_address(reader, &ob->mpath);
  if (ob->mpath) {
    direct_link_motionpath(reader, ob->mpath);
  }

  BLO_read_list(reader, &ob->defbase);
  BLO_read_list(reader, &ob->fmaps);
  /* XXX deprecated - old animation system <<< */
  direct_link_nlastrips(reader, &ob->nlastrips);
  BLO_read_list(reader, &ob->constraintChannels);
  /* >>> XXX deprecated - old animation system */

  BLO_read_pointer_array(reader, (void **)&ob->mat);
  BLO_read_data_address(reader, &ob->matbits);

  /* do it here, below old data gets converted */
  direct_link_modifiers(reader, &ob->modifiers, ob);
  direct_link_gpencil_modifiers(reader, &ob->greasepencil_modifiers);
  direct_link_shaderfxs(reader, &ob->shader_fx);

  BLO_read_list(reader, &ob->effect);
  paf = ob->effect.first;
  while (paf) {
    if (paf->type == EFF_PARTICLE) {
      paf->keys = NULL;
    }
    if (paf->type == EFF_WAVE) {
      WaveEff *wav = (WaveEff *)paf;
      PartEff *next = paf->next;
      WaveModifierData *wmd = (WaveModifierData *)BKE_modifier_new(eModifierType_Wave);

      wmd->damp = wav->damp;
      wmd->flag = wav->flag;
      wmd->height = wav->height;
      wmd->lifetime = wav->lifetime;
      wmd->narrow = wav->narrow;
      wmd->speed = wav->speed;
      wmd->startx = wav->startx;
      wmd->starty = wav->startx;
      wmd->timeoffs = wav->timeoffs;
      wmd->width = wav->width;

      BLI_addtail(&ob->modifiers, wmd);

      BLI_remlink(&ob->effect, paf);
      MEM_freeN(paf);

      paf = next;
      continue;
    }
    if (paf->type == EFF_BUILD) {
      BuildEff *baf = (BuildEff *)paf;
      PartEff *next = paf->next;
      BuildModifierData *bmd = (BuildModifierData *)BKE_modifier_new(eModifierType_Build);

      bmd->start = baf->sfra;
      bmd->length = baf->len;
      bmd->randomize = 0;
      bmd->seed = 1;

      BLI_addtail(&ob->modifiers, bmd);

      BLI_remlink(&ob->effect, paf);
      MEM_freeN(paf);

      paf = next;
      continue;
    }
    paf = paf->next;
  }

  BLO_read_data_address(reader, &ob->pd);
  direct_link_partdeflect(ob->pd);
  BLO_read_data_address(reader, &ob->soft);
  if (ob->soft) {
    SoftBody *sb = ob->soft;

    sb->bpoint = NULL; /* init pointers so it gets rebuilt nicely */
    sb->bspring = NULL;
    sb->scratch = NULL;
    /* although not used anymore */
    /* still have to be loaded to be compatible with old files */
    BLO_read_pointer_array(reader, (void **)&sb->keys);
    if (sb->keys) {
      for (int a = 0; a < sb->totkey; a++) {
        BLO_read_data_address(reader, &sb->keys[a]);
      }
    }

    BLO_read_data_address(reader, &sb->effector_weights);
    if (!sb->effector_weights) {
      sb->effector_weights = BKE_effector_add_weights(NULL);
    }

    BLO_read_data_address(reader, &sb->shared);
    if (sb->shared == NULL) {
      /* Link deprecated caches if they exist, so we can use them for versioning.
       * We should only do this when sb->shared == NULL, because those pointers
       * are always set (for compatibility with older Blenders). We mustn't link
       * the same pointcache twice. */
      direct_link_pointcache_list(reader, &sb->ptcaches, &sb->pointcache, false);
    }
    else {
      /* link caches */
      direct_link_pointcache_list(reader, &sb->shared->ptcaches, &sb->shared->pointcache, false);
    }
  }
  BLO_read_data_address(reader, &ob->fluidsimSettings); /* NT */

  BLO_read_data_address(reader, &ob->rigidbody_object);
  if (ob->rigidbody_object) {
    RigidBodyOb *rbo = ob->rigidbody_object;
    /* Allocate runtime-only struct */
    rbo->shared = MEM_callocN(sizeof(*rbo->shared), "RigidBodyObShared");
  }
  BLO_read_data_address(reader, &ob->rigidbody_constraint);
  if (ob->rigidbody_constraint) {
    ob->rigidbody_constraint->physics_constraint = NULL;
  }

  BLO_read_list(reader, &ob->particlesystem);
  direct_link_particlesystems(reader, &ob->particlesystem);

  direct_link_constraints(reader, &ob->constraints);

  BLO_read_list(reader, &ob->hooks);
  while (ob->hooks.first) {
    ObHook *hook = ob->hooks.first;
    HookModifierData *hmd = (HookModifierData *)BKE_modifier_new(eModifierType_Hook);

    BLO_read_int32_array(reader, hook->totindex, &hook->indexar);

    /* Do conversion here because if we have loaded
     * a hook we need to make sure it gets converted
     * and freed, regardless of version.
     */
    copy_v3_v3(hmd->cent, hook->cent);
    hmd->falloff = hook->falloff;
    hmd->force = hook->force;
    hmd->indexar = hook->indexar;
    hmd->object = hook->parent;
    memcpy(hmd->parentinv, hook->parentinv, sizeof(hmd->parentinv));
    hmd->totindex = hook->totindex;

    BLI_addhead(&ob->modifiers, hmd);
    BLI_remlink(&ob->hooks, hook);

    BKE_modifier_unique_name(&ob->modifiers, (ModifierData *)hmd);

    MEM_freeN(hook);
  }

  BLO_read_data_address(reader, &ob->iuser);
  if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE && !ob->iuser) {
    BKE_object_empty_draw_type_set(ob, ob->empty_drawtype);
  }

  BKE_object_runtime_reset(ob);
  BLO_read_list(reader, &ob->pc_ids);

  /* in case this value changes in future, clamp else we get undefined behavior */
  CLAMP(ob->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

  if (ob->sculpt) {
    ob->sculpt = NULL;
    /* Only create data on undo, otherwise rely on editor mode switching. */
    if (BLO_read_data_is_undo(reader) && (ob->mode & OB_MODE_ALL_SCULPT)) {
      BKE_object_sculpt_data_create(ob);
    }
  }

  BLO_read_data_address(reader, &ob->preview);
  BKE_previewimg_blend_read(reader, ob->preview);
}

static void direct_link_view_settings(BlendDataReader *reader,
                                      ColorManagedViewSettings *view_settings)
{
  BLO_read_data_address(reader, &view_settings->curve_mapping);

  if (view_settings->curve_mapping) {
    BKE_curvemapping_blend_read(reader, view_settings->curve_mapping);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read View Layer (Collection Data)
 * \{ */

static void direct_link_layer_collections(BlendDataReader *reader, ListBase *lb, bool master)
{
  BLO_read_list(reader, lb);
  LISTBASE_FOREACH (LayerCollection *, lc, lb) {
#ifdef USE_COLLECTION_COMPAT_28
    BLO_read_data_address(reader, &lc->scene_collection);
#endif

    /* Master collection is not a real data-lock. */
    if (master) {
      BLO_read_data_address(reader, &lc->collection);
    }

    direct_link_layer_collections(reader, &lc->layer_collections, false);
  }
}

static void direct_link_view_layer(BlendDataReader *reader, ViewLayer *view_layer)
{
  view_layer->stats = NULL;
  BLO_read_list(reader, &view_layer->object_bases);
  BLO_read_data_address(reader, &view_layer->basact);

  direct_link_layer_collections(reader, &view_layer->layer_collections, true);
  BLO_read_data_address(reader, &view_layer->active_collection);

  BLO_read_data_address(reader, &view_layer->id_properties);
  IDP_BlendDataRead(reader, &view_layer->id_properties);

  BLO_read_list(reader, &(view_layer->freestyle_config.modules));
  BLO_read_list(reader, &(view_layer->freestyle_config.linesets));

  BLI_listbase_clear(&view_layer->drawdata);
  view_layer->object_bases_array = NULL;
  view_layer->object_bases_hash = NULL;
}

static void lib_link_layer_collection(BlendLibReader *reader,
                                      Library *lib,
                                      LayerCollection *layer_collection,
                                      bool master)
{
  /* Master collection is not a real data-lock. */
  if (!master) {
    BLO_read_id_address(reader, lib, &layer_collection->collection);
  }

  LISTBASE_FOREACH (
      LayerCollection *, layer_collection_nested, &layer_collection->layer_collections) {
    lib_link_layer_collection(reader, lib, layer_collection_nested, false);
  }
}

static void lib_link_view_layer(BlendLibReader *reader, Library *lib, ViewLayer *view_layer)
{
  LISTBASE_FOREACH (FreestyleModuleConfig *, fmc, &view_layer->freestyle_config.modules) {
    BLO_read_id_address(reader, lib, &fmc->script);
  }

  LISTBASE_FOREACH (FreestyleLineSet *, fls, &view_layer->freestyle_config.linesets) {
    BLO_read_id_address(reader, lib, &fls->linestyle);
    BLO_read_id_address(reader, lib, &fls->group);
  }

  for (Base *base = view_layer->object_bases.first, *base_next = NULL; base; base = base_next) {
    base_next = base->next;

    /* we only bump the use count for the collection objects */
    BLO_read_id_address(reader, lib, &base->object);

    if (base->object == NULL) {
      /* Free in case linked object got lost. */
      BLI_freelinkN(&view_layer->object_bases, base);
      if (view_layer->basact == base) {
        view_layer->basact = NULL;
      }
    }
  }

  LISTBASE_FOREACH (LayerCollection *, layer_collection, &view_layer->layer_collections) {
    lib_link_layer_collection(reader, lib, layer_collection, true);
  }

  BLO_read_id_address(reader, lib, &view_layer->mat_override);

  IDP_BlendReadLib(reader, view_layer->id_properties);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Collection
 * \{ */

#ifdef USE_COLLECTION_COMPAT_28
static void direct_link_scene_collection(BlendDataReader *reader, SceneCollection *sc)
{
  BLO_read_list(reader, &sc->objects);
  BLO_read_list(reader, &sc->scene_collections);

  LISTBASE_FOREACH (SceneCollection *, nsc, &sc->scene_collections) {
    direct_link_scene_collection(reader, nsc);
  }
}

static void lib_link_scene_collection(BlendLibReader *reader, Library *lib, SceneCollection *sc)
{
  LISTBASE_FOREACH (LinkData *, link, &sc->objects) {
    BLO_read_id_address(reader, lib, &link->data);
    BLI_assert(link->data);
  }

  LISTBASE_FOREACH (SceneCollection *, nsc, &sc->scene_collections) {
    lib_link_scene_collection(reader, lib, nsc);
  }
}
#endif

static void direct_link_collection(BlendDataReader *reader, Collection *collection)
{
  BLO_read_list(reader, &collection->gobject);
  BLO_read_list(reader, &collection->children);

  BLO_read_data_address(reader, &collection->preview);
  BKE_previewimg_blend_read(reader, collection->preview);

  collection->flag &= ~COLLECTION_HAS_OBJECT_CACHE;
  collection->tag = 0;
  BLI_listbase_clear(&collection->object_cache);
  BLI_listbase_clear(&collection->parents);

#ifdef USE_COLLECTION_COMPAT_28
  /* This runs before the very first doversion. */
  BLO_read_data_address(reader, &collection->collection);
  if (collection->collection != NULL) {
    direct_link_scene_collection(reader, collection->collection);
  }

  BLO_read_data_address(reader, &collection->view_layer);
  if (collection->view_layer != NULL) {
    direct_link_view_layer(reader, collection->view_layer);
  }
#endif
}

static void lib_link_collection_data(BlendLibReader *reader, Library *lib, Collection *collection)
{
  LISTBASE_FOREACH_MUTABLE (CollectionObject *, cob, &collection->gobject) {
    BLO_read_id_address(reader, lib, &cob->ob);

    if (cob->ob == NULL) {
      BLI_freelinkN(&collection->gobject, cob);
    }
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BLO_read_id_address(reader, lib, &child->collection);
  }
}

static void lib_link_collection(BlendLibReader *reader, Collection *collection)
{
#ifdef USE_COLLECTION_COMPAT_28
  if (collection->collection) {
    lib_link_scene_collection(reader, collection->id.lib, collection->collection);
  }

  if (collection->view_layer) {
    lib_link_view_layer(reader, collection->id.lib, collection->view_layer);
  }
#endif

  lib_link_collection_data(reader, collection->id.lib, collection);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Scene
 * \{ */

/* patch for missing scene IDs, can't be in do-versions */
static void composite_patch(bNodeTree *ntree, Scene *scene)
{

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->id == NULL && node->type == CMP_NODE_R_LAYERS) {
      node->id = &scene->id;
    }
  }
}

static void link_paint(BlendLibReader *reader, Scene *sce, Paint *p)
{
  if (p) {
    BLO_read_id_address(reader, sce->id.lib, &p->brush);
    for (int i = 0; i < p->tool_slots_len; i++) {
      if (p->tool_slots[i].brush != NULL) {
        BLO_read_id_address(reader, sce->id.lib, &p->tool_slots[i].brush);
      }
    }
    BLO_read_id_address(reader, sce->id.lib, &p->palette);
    p->paint_cursor = NULL;

    BKE_paint_runtime_init(sce->toolsettings, p);
  }
}

static void lib_link_sequence_modifiers(BlendLibReader *reader, Scene *scene, ListBase *lb)
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, lb) {
    if (smd->mask_id) {
      BLO_read_id_address(reader, scene->id.lib, &smd->mask_id);
    }
  }
}

static void direct_link_lightcache_texture(BlendDataReader *reader, LightCacheTexture *lctex)
{
  lctex->tex = NULL;

  if (lctex->data) {
    BLO_read_data_address(reader, &lctex->data);
    if (lctex->data && BLO_read_requires_endian_switch(reader)) {
      int data_size = lctex->components * lctex->tex_size[0] * lctex->tex_size[1] *
                      lctex->tex_size[2];

      if (lctex->data_type == LIGHTCACHETEX_FLOAT) {
        BLI_endian_switch_float_array((float *)lctex->data, data_size * sizeof(float));
      }
      else if (lctex->data_type == LIGHTCACHETEX_UINT) {
        BLI_endian_switch_uint32_array((uint *)lctex->data, data_size * sizeof(uint));
      }
    }
  }

  if (lctex->data == NULL) {
    zero_v3_int(lctex->tex_size);
  }
}

static void direct_link_lightcache(BlendDataReader *reader, LightCache *cache)
{
  cache->flag &= ~LIGHTCACHE_NOT_USABLE;
  direct_link_lightcache_texture(reader, &cache->cube_tx);
  direct_link_lightcache_texture(reader, &cache->grid_tx);

  if (cache->cube_mips) {
    BLO_read_data_address(reader, &cache->cube_mips);
    for (int i = 0; i < cache->mips_len; i++) {
      direct_link_lightcache_texture(reader, &cache->cube_mips[i]);
    }
  }

  BLO_read_data_address(reader, &cache->cube_data);
  BLO_read_data_address(reader, &cache->grid_data);
}

static void direct_link_view3dshading(BlendDataReader *reader, View3DShading *shading)
{
  if (shading->prop) {
    BLO_read_data_address(reader, &shading->prop);
    IDP_BlendDataRead(reader, &shading->prop);
  }
}

/* check for cyclic set-scene,
 * libs can cause this case which is normally prevented, see (T#####) */
#define USE_SETSCENE_CHECK

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

static void lib_link_scene(BlendLibReader *reader, Scene *sce)
{
  BKE_keyingsets_blend_read_lib(reader, &sce->id, &sce->keyingsets);

  BLO_read_id_address(reader, sce->id.lib, &sce->camera);
  BLO_read_id_address(reader, sce->id.lib, &sce->world);
  BLO_read_id_address(reader, sce->id.lib, &sce->set);
  BLO_read_id_address(reader, sce->id.lib, &sce->gpd);

  link_paint(reader, sce, &sce->toolsettings->imapaint.paint);
  if (sce->toolsettings->sculpt) {
    link_paint(reader, sce, &sce->toolsettings->sculpt->paint);
  }
  if (sce->toolsettings->vpaint) {
    link_paint(reader, sce, &sce->toolsettings->vpaint->paint);
  }
  if (sce->toolsettings->wpaint) {
    link_paint(reader, sce, &sce->toolsettings->wpaint->paint);
  }
  if (sce->toolsettings->uvsculpt) {
    link_paint(reader, sce, &sce->toolsettings->uvsculpt->paint);
  }
  if (sce->toolsettings->gp_paint) {
    link_paint(reader, sce, &sce->toolsettings->gp_paint->paint);
  }
  if (sce->toolsettings->gp_vertexpaint) {
    link_paint(reader, sce, &sce->toolsettings->gp_vertexpaint->paint);
  }
  if (sce->toolsettings->gp_sculptpaint) {
    link_paint(reader, sce, &sce->toolsettings->gp_sculptpaint->paint);
  }
  if (sce->toolsettings->gp_weightpaint) {
    link_paint(reader, sce, &sce->toolsettings->gp_weightpaint->paint);
  }

  if (sce->toolsettings->sculpt) {
    BLO_read_id_address(reader, sce->id.lib, &sce->toolsettings->sculpt->gravity_object);
  }

  if (sce->toolsettings->imapaint.stencil) {
    BLO_read_id_address(reader, sce->id.lib, &sce->toolsettings->imapaint.stencil);
  }

  if (sce->toolsettings->imapaint.clone) {
    BLO_read_id_address(reader, sce->id.lib, &sce->toolsettings->imapaint.clone);
  }

  if (sce->toolsettings->imapaint.canvas) {
    BLO_read_id_address(reader, sce->id.lib, &sce->toolsettings->imapaint.canvas);
  }

  BLO_read_id_address(reader, sce->id.lib, &sce->toolsettings->particle.shape_object);

  BLO_read_id_address(reader, sce->id.lib, &sce->toolsettings->gp_sculpt.guide.reference_object);

  LISTBASE_FOREACH_MUTABLE (Base *, base_legacy, &sce->base) {
    BLO_read_id_address(reader, sce->id.lib, &base_legacy->object);

    if (base_legacy->object == NULL) {
      blo_reportf_wrap(reader->fd->reports,
                       RPT_WARNING,
                       TIP_("LIB: object lost from scene: '%s'"),
                       sce->id.name + 2);
      BLI_remlink(&sce->base, base_legacy);
      if (base_legacy == sce->basact) {
        sce->basact = NULL;
      }
      MEM_freeN(base_legacy);
    }
  }

  Sequence *seq;
  SEQ_ALL_BEGIN (sce->ed, seq) {
    IDP_BlendReadLib(reader, seq->prop);

    if (seq->ipo) {
      BLO_read_id_address(
          reader, sce->id.lib, &seq->ipo); /* XXX deprecated - old animation system */
    }
    seq->scene_sound = NULL;
    if (seq->scene) {
      BLO_read_id_address(reader, sce->id.lib, &seq->scene);
      seq->scene_sound = NULL;
    }
    if (seq->clip) {
      BLO_read_id_address(reader, sce->id.lib, &seq->clip);
    }
    if (seq->mask) {
      BLO_read_id_address(reader, sce->id.lib, &seq->mask);
    }
    if (seq->scene_camera) {
      BLO_read_id_address(reader, sce->id.lib, &seq->scene_camera);
    }
    if (seq->sound) {
      seq->scene_sound = NULL;
      if (seq->type == SEQ_TYPE_SOUND_HD) {
        seq->type = SEQ_TYPE_SOUND_RAM;
      }
      else {
        BLO_read_id_address(reader, sce->id.lib, &seq->sound);
      }
      if (seq->sound) {
        id_us_plus_no_lib((ID *)seq->sound);
        seq->scene_sound = NULL;
      }
    }
    if (seq->type == SEQ_TYPE_TEXT) {
      TextVars *t = seq->effectdata;
      BLO_read_id_address(reader, sce->id.lib, &t->text_font);
    }
    BLI_listbase_clear(&seq->anims);

    lib_link_sequence_modifiers(reader, sce, &seq->modifiers);
  }
  SEQ_ALL_END;

  LISTBASE_FOREACH (TimeMarker *, marker, &sce->markers) {
    IDP_BlendReadLib(reader, marker->prop);

    if (marker->camera) {
      BLO_read_id_address(reader, sce->id.lib, &marker->camera);
    }
  }

  /* rigidbody world relies on it's linked collections */
  if (sce->rigidbody_world) {
    RigidBodyWorld *rbw = sce->rigidbody_world;
    if (rbw->group) {
      BLO_read_id_address(reader, sce->id.lib, &rbw->group);
    }
    if (rbw->constraints) {
      BLO_read_id_address(reader, sce->id.lib, &rbw->constraints);
    }
    if (rbw->effector_weights) {
      BLO_read_id_address(reader, sce->id.lib, &rbw->effector_weights->group);
    }
  }

  if (sce->nodetree) {
    composite_patch(sce->nodetree, sce);
  }

  LISTBASE_FOREACH (SceneRenderLayer *, srl, &sce->r.layers) {
    BLO_read_id_address(reader, sce->id.lib, &srl->mat_override);
    LISTBASE_FOREACH (FreestyleModuleConfig *, fmc, &srl->freestyleConfig.modules) {
      BLO_read_id_address(reader, sce->id.lib, &fmc->script);
    }
    LISTBASE_FOREACH (FreestyleLineSet *, fls, &srl->freestyleConfig.linesets) {
      BLO_read_id_address(reader, sce->id.lib, &fls->linestyle);
      BLO_read_id_address(reader, sce->id.lib, &fls->group);
    }
  }
  /* Motion Tracking */
  BLO_read_id_address(reader, sce->id.lib, &sce->clip);

#ifdef USE_COLLECTION_COMPAT_28
  if (sce->collection) {
    lib_link_scene_collection(reader, sce->id.lib, sce->collection);
  }
#endif

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    lib_link_view_layer(reader, sce->id.lib, view_layer);
  }

  if (sce->r.bake.cage_object) {
    BLO_read_id_address(reader, sce->id.lib, &sce->r.bake.cage_object);
  }

#ifdef USE_SETSCENE_CHECK
  if (sce->set != NULL) {
    sce->flag |= SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK;
  }
#endif
}

static void lib_link_scenes_check_set(Main *bmain)
{
#ifdef USE_SETSCENE_CHECK
  const int totscene = BLI_listbase_count(&bmain->scenes);
  LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
    if (sce->flag & SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK) {
      sce->flag &= ~SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK;
      if (!scene_validate_setscene__liblink(sce, totscene)) {
        printf("Found cyclic background scene when linking %s\n", sce->id.name + 2);
      }
    }
  }
#else
  UNUSED_VARS(bmain, totscene);
#endif
}

#undef USE_SETSCENE_CHECK

static void link_recurs_seq(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (Sequence *, seq, lb) {
    if (seq->seqbase.first) {
      link_recurs_seq(reader, &seq->seqbase);
    }
  }
}

static void direct_link_paint(BlendDataReader *reader, const Scene *scene, Paint *p)
{
  if (p->num_input_samples < 1) {
    p->num_input_samples = 1;
  }

  BLO_read_data_address(reader, &p->cavity_curve);
  if (p->cavity_curve) {
    BKE_curvemapping_blend_read(reader, p->cavity_curve);
  }
  else {
    BKE_paint_cavity_curve_preset(p, CURVE_PRESET_LINE);
  }

  BLO_read_data_address(reader, &p->tool_slots);

  /* Workaround for invalid data written in older versions. */
  const size_t expected_size = sizeof(PaintToolSlot) * p->tool_slots_len;
  if (p->tool_slots && MEM_allocN_len(p->tool_slots) < expected_size) {
    MEM_freeN(p->tool_slots);
    p->tool_slots = MEM_callocN(expected_size, "PaintToolSlot");
  }

  BKE_paint_runtime_init(scene->toolsettings, p);
}

static void direct_link_paint_helper(BlendDataReader *reader, const Scene *scene, Paint **paint)
{
  /* TODO. is this needed */
  BLO_read_data_address(reader, paint);

  if (*paint) {
    direct_link_paint(reader, scene, *paint);
  }
}

static void direct_link_sequence_modifiers(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (SequenceModifierData *, smd, lb) {
    if (smd->mask_sequence) {
      BLO_read_data_address(reader, &smd->mask_sequence);
    }

    if (smd->type == seqModifierType_Curves) {
      CurvesModifierData *cmd = (CurvesModifierData *)smd;

      BKE_curvemapping_blend_read(reader, &cmd->curve_mapping);
    }
    else if (smd->type == seqModifierType_HueCorrect) {
      HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

      BKE_curvemapping_blend_read(reader, &hcmd->curve_mapping);
    }
  }
}

static void direct_link_scene(BlendDataReader *reader, Scene *sce)
{
  sce->depsgraph_hash = NULL;
  sce->fps_info = NULL;

  memset(&sce->customdata_mask, 0, sizeof(sce->customdata_mask));
  memset(&sce->customdata_mask_modal, 0, sizeof(sce->customdata_mask_modal));

  BKE_sound_reset_scene_runtime(sce);

  /* set users to one by default, not in lib-link, this will increase it for compo nodes */
  id_us_ensure_real(&sce->id);

  BLO_read_list(reader, &(sce->base));

  BLO_read_data_address(reader, &sce->adt);
  BKE_animdata_blend_read_data(reader, sce->adt);

  BLO_read_list(reader, &sce->keyingsets);
  BKE_keyingsets_blend_read_data(reader, &sce->keyingsets);

  BLO_read_data_address(reader, &sce->basact);

  BLO_read_data_address(reader, &sce->toolsettings);
  if (sce->toolsettings) {

    /* Reset last_location and last_hit, so they are not remembered across sessions. In some files
     * these are also NaN, which could lead to crashes in painting. */
    struct UnifiedPaintSettings *ups = &sce->toolsettings->unified_paint_settings;
    zero_v3(ups->last_location);
    ups->last_hit = 0;

    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->sculpt);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->vpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->wpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->uvsculpt);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_paint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_vertexpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_sculptpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_weightpaint);

    direct_link_paint(reader, sce, &sce->toolsettings->imapaint.paint);

    sce->toolsettings->particle.paintcursor = NULL;
    sce->toolsettings->particle.scene = NULL;
    sce->toolsettings->particle.object = NULL;
    sce->toolsettings->gp_sculpt.paintcursor = NULL;

    /* relink grease pencil interpolation curves */
    BLO_read_data_address(reader, &sce->toolsettings->gp_interpolate.custom_ipo);
    if (sce->toolsettings->gp_interpolate.custom_ipo) {
      BKE_curvemapping_blend_read(reader, sce->toolsettings->gp_interpolate.custom_ipo);
    }
    /* relink grease pencil multiframe falloff curve */
    BLO_read_data_address(reader, &sce->toolsettings->gp_sculpt.cur_falloff);
    if (sce->toolsettings->gp_sculpt.cur_falloff) {
      BKE_curvemapping_blend_read(reader, sce->toolsettings->gp_sculpt.cur_falloff);
    }
    /* relink grease pencil primitive curve */
    BLO_read_data_address(reader, &sce->toolsettings->gp_sculpt.cur_primitive);
    if (sce->toolsettings->gp_sculpt.cur_primitive) {
      BKE_curvemapping_blend_read(reader, sce->toolsettings->gp_sculpt.cur_primitive);
    }

    /* Relink toolsettings curve profile */
    BLO_read_data_address(reader, &sce->toolsettings->custom_bevel_profile_preset);
    if (sce->toolsettings->custom_bevel_profile_preset) {
      BKE_curveprofile_blend_read(reader, sce->toolsettings->custom_bevel_profile_preset);
    }
  }

  if (sce->ed) {
    ListBase *old_seqbasep = &sce->ed->seqbase;

    BLO_read_data_address(reader, &sce->ed);
    Editing *ed = sce->ed;

    BLO_read_data_address(reader, &ed->act_seq);
    ed->cache = NULL;
    ed->prefetch_job = NULL;

    /* recursive link sequences, lb will be correctly initialized */
    link_recurs_seq(reader, &ed->seqbase);

    Sequence *seq;
    SEQ_ALL_BEGIN (ed, seq) {
      /* Do as early as possible, so that other parts of reading can rely on valid session UUID. */
      BKE_sequence_session_uuid_generate(seq);

      BLO_read_data_address(reader, &seq->seq1);
      BLO_read_data_address(reader, &seq->seq2);
      BLO_read_data_address(reader, &seq->seq3);

      /* a patch: after introduction of effects with 3 input strips */
      if (seq->seq3 == NULL) {
        seq->seq3 = seq->seq2;
      }

      BLO_read_data_address(reader, &seq->effectdata);
      BLO_read_data_address(reader, &seq->stereo3d_format);

      if (seq->type & SEQ_TYPE_EFFECT) {
        seq->flag |= SEQ_EFFECT_NOT_LOADED;
      }

      if (seq->type == SEQ_TYPE_SPEED) {
        SpeedControlVars *s = seq->effectdata;
        s->frameMap = NULL;
      }

      if (seq->type == SEQ_TYPE_TEXT) {
        TextVars *t = seq->effectdata;
        t->text_blf_id = SEQ_FONT_NOT_LOADED;
      }

      BLO_read_data_address(reader, &seq->prop);
      IDP_BlendDataRead(reader, &seq->prop);

      BLO_read_data_address(reader, &seq->strip);
      if (seq->strip && seq->strip->done == 0) {
        seq->strip->done = true;

        if (ELEM(seq->type,
                 SEQ_TYPE_IMAGE,
                 SEQ_TYPE_MOVIE,
                 SEQ_TYPE_SOUND_RAM,
                 SEQ_TYPE_SOUND_HD)) {
          BLO_read_data_address(reader, &seq->strip->stripdata);
        }
        else {
          seq->strip->stripdata = NULL;
        }
        BLO_read_data_address(reader, &seq->strip->crop);
        BLO_read_data_address(reader, &seq->strip->transform);
        BLO_read_data_address(reader, &seq->strip->proxy);
        if (seq->strip->proxy) {
          seq->strip->proxy->anim = NULL;
        }
        else if (seq->flag & SEQ_USE_PROXY) {
          BKE_sequencer_proxy_set(seq, true);
        }

        /* need to load color balance to it could be converted to modifier */
        BLO_read_data_address(reader, &seq->strip->color_balance);
      }

      direct_link_sequence_modifiers(reader, &seq->modifiers);
    }
    SEQ_ALL_END;

    /* link metastack, slight abuse of structs here,
     * have to restore pointer to internal part in struct */
    {
      Sequence temp;
      void *poin;
      intptr_t offset;

      offset = ((intptr_t) & (temp.seqbase)) - ((intptr_t)&temp);

      /* root pointer */
      if (ed->seqbasep == old_seqbasep) {
        ed->seqbasep = &ed->seqbase;
      }
      else {
        poin = POINTER_OFFSET(ed->seqbasep, -offset);

        poin = BLO_read_get_new_data_address(reader, poin);

        if (poin) {
          ed->seqbasep = (ListBase *)POINTER_OFFSET(poin, offset);
        }
        else {
          ed->seqbasep = &ed->seqbase;
        }
      }
      /* stack */
      BLO_read_list(reader, &(ed->metastack));

      LISTBASE_FOREACH (MetaStack *, ms, &ed->metastack) {
        BLO_read_data_address(reader, &ms->parseq);

        if (ms->oldbasep == old_seqbasep) {
          ms->oldbasep = &ed->seqbase;
        }
        else {
          poin = POINTER_OFFSET(ms->oldbasep, -offset);
          poin = BLO_read_get_new_data_address(reader, poin);
          if (poin) {
            ms->oldbasep = (ListBase *)POINTER_OFFSET(poin, offset);
          }
          else {
            ms->oldbasep = &ed->seqbase;
          }
        }
      }
    }
  }

#ifdef DURIAN_CAMERA_SWITCH
  /* Runtime */
  sce->r.mode &= ~R_NO_CAMERA_SWITCH;
#endif

  BLO_read_data_address(reader, &sce->r.avicodecdata);
  if (sce->r.avicodecdata) {
    BLO_read_data_address(reader, &sce->r.avicodecdata->lpFormat);
    BLO_read_data_address(reader, &sce->r.avicodecdata->lpParms);
  }
  if (sce->r.ffcodecdata.properties) {
    BLO_read_data_address(reader, &sce->r.ffcodecdata.properties);
    IDP_BlendDataRead(reader, &sce->r.ffcodecdata.properties);
  }

  BLO_read_list(reader, &(sce->markers));
  LISTBASE_FOREACH (TimeMarker *, marker, &sce->markers) {
    BLO_read_data_address(reader, &marker->prop);
    IDP_BlendDataRead(reader, &marker->prop);
  }

  BLO_read_list(reader, &(sce->transform_spaces));
  BLO_read_list(reader, &(sce->r.layers));
  BLO_read_list(reader, &(sce->r.views));

  LISTBASE_FOREACH (SceneRenderLayer *, srl, &sce->r.layers) {
    BLO_read_data_address(reader, &srl->prop);
    IDP_BlendDataRead(reader, &srl->prop);
    BLO_read_list(reader, &(srl->freestyleConfig.modules));
    BLO_read_list(reader, &(srl->freestyleConfig.linesets));
  }

  direct_link_view_settings(reader, &sce->view_settings);

  BLO_read_data_address(reader, &sce->rigidbody_world);
  RigidBodyWorld *rbw = sce->rigidbody_world;
  if (rbw) {
    BLO_read_data_address(reader, &rbw->shared);

    if (rbw->shared == NULL) {
      /* Link deprecated caches if they exist, so we can use them for versioning.
       * We should only do this when rbw->shared == NULL, because those pointers
       * are always set (for compatibility with older Blenders). We mustn't link
       * the same pointcache twice. */
      direct_link_pointcache_list(reader, &rbw->ptcaches, &rbw->pointcache, false);

      /* make sure simulation starts from the beginning after loading file */
      if (rbw->pointcache) {
        rbw->ltime = (float)rbw->pointcache->startframe;
      }
    }
    else {
      /* must nullify the reference to physics sim object, since it no-longer exist
       * (and will need to be recalculated)
       */
      rbw->shared->physics_world = NULL;

      /* link caches */
      direct_link_pointcache_list(reader, &rbw->shared->ptcaches, &rbw->shared->pointcache, false);

      /* make sure simulation starts from the beginning after loading file */
      if (rbw->shared->pointcache) {
        rbw->ltime = (float)rbw->shared->pointcache->startframe;
      }
    }
    rbw->objects = NULL;
    rbw->numbodies = 0;

    /* set effector weights */
    BLO_read_data_address(reader, &rbw->effector_weights);
    if (!rbw->effector_weights) {
      rbw->effector_weights = BKE_effector_add_weights(NULL);
    }
  }

  BLO_read_data_address(reader, &sce->preview);
  BKE_previewimg_blend_read(reader, sce->preview);

  BKE_curvemapping_blend_read(reader, &sce->r.mblur_shutter_curve);

#ifdef USE_COLLECTION_COMPAT_28
  /* this runs before the very first doversion */
  if (sce->collection) {
    BLO_read_data_address(reader, &sce->collection);
    direct_link_scene_collection(reader, sce->collection);
  }
#endif

  /* insert into global old-new map for reading without UI (link_global accesses it again) */
  link_glob_list(reader->fd, &sce->view_layers);
  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    direct_link_view_layer(reader, view_layer);
  }

  if (BLO_read_data_is_undo(reader)) {
    /* If it's undo do nothing here, caches are handled by higher-level generic calling code. */
  }
  else {
    /* else try to read the cache from file. */
    BLO_read_data_address(reader, &sce->eevee.light_cache_data);
    if (sce->eevee.light_cache_data) {
      direct_link_lightcache(reader, sce->eevee.light_cache_data);
    }
  }
  EEVEE_lightcache_info_update(&sce->eevee);

  direct_link_view3dshading(reader, &sce->display.shading);

  BLO_read_data_address(reader, &sce->layer_properties);
  IDP_BlendDataRead(reader, &sce->layer_properties);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Screen Area/Region (Screen Data)
 * \{ */

static void direct_link_panel_list(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (Panel *, panel, lb) {
    panel->runtime_flag = 0;
    panel->activedata = NULL;
    panel->type = NULL;
    panel->runtime.custom_data_ptr = NULL;
    direct_link_panel_list(reader, &panel->children);
  }
}

static void direct_link_region(BlendDataReader *reader, ARegion *region, int spacetype)
{
  direct_link_panel_list(reader, &region->panels);

  BLO_read_list(reader, &region->panels_category_active);

  BLO_read_list(reader, &region->ui_lists);

  /* The area's search filter is runtime only, so we need to clear the active flag on read. */
  region->flag &= ~RGN_FLAG_SEARCH_FILTER_ACTIVE;

  LISTBASE_FOREACH (uiList *, ui_list, &region->ui_lists) {
    ui_list->type = NULL;
    ui_list->dyn_data = NULL;
    BLO_read_data_address(reader, &ui_list->properties);
    IDP_BlendDataRead(reader, &ui_list->properties);
  }

  BLO_read_list(reader, &region->ui_previews);

  if (spacetype == SPACE_EMPTY) {
    /* unknown space type, don't leak regiondata */
    region->regiondata = NULL;
  }
  else if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
    /* Runtime data, don't use. */
    region->regiondata = NULL;
  }
  else {
    BLO_read_data_address(reader, &region->regiondata);
    if (region->regiondata) {
      if (spacetype == SPACE_VIEW3D) {
        RegionView3D *rv3d = region->regiondata;

        BLO_read_data_address(reader, &rv3d->localvd);
        BLO_read_data_address(reader, &rv3d->clipbb);

        rv3d->depths = NULL;
        rv3d->render_engine = NULL;
        rv3d->sms = NULL;
        rv3d->smooth_timer = NULL;

        rv3d->rflag &= ~(RV3D_NAVIGATING | RV3D_PAINTING);
        rv3d->runtime_viewlock = 0;
      }
    }
  }

  region->v2d.sms = NULL;
  region->v2d.alpha_hor = region->v2d.alpha_vert = 255; /* visible by default */
  BLI_listbase_clear(&region->panels_category);
  BLI_listbase_clear(&region->handlers);
  BLI_listbase_clear(&region->uiblocks);
  region->headerstr = NULL;
  region->visible = 0;
  region->type = NULL;
  region->do_draw = 0;
  region->gizmo_map = NULL;
  region->regiontimer = NULL;
  region->draw_buffer = NULL;
  memset(&region->drawrct, 0, sizeof(region->drawrct));
}

static void direct_link_area(BlendDataReader *reader, ScrArea *area)
{
  BLO_read_list(reader, &(area->spacedata));
  BLO_read_list(reader, &(area->regionbase));

  BLI_listbase_clear(&area->handlers);
  area->type = NULL; /* spacetype callbacks */

  /* Should always be unset so that rna_Area_type_get works correctly. */
  area->butspacetype = SPACE_EMPTY;

  area->region_active_win = -1;

  area->flag &= ~AREA_FLAG_ACTIVE_TOOL_UPDATE;

  BLO_read_data_address(reader, &area->global);

  /* if we do not have the spacetype registered we cannot
   * free it, so don't allocate any new memory for such spacetypes. */
  if (!BKE_spacetype_exists(area->spacetype)) {
    /* Hint for versioning code to replace deprecated space types. */
    area->butspacetype = area->spacetype;

    area->spacetype = SPACE_EMPTY;
  }

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    direct_link_region(reader, region, area->spacetype);
  }

  /* accident can happen when read/save new file with older version */
  /* 2.50: we now always add spacedata for info */
  if (area->spacedata.first == NULL) {
    SpaceInfo *sinfo = MEM_callocN(sizeof(SpaceInfo), "spaceinfo");
    area->spacetype = sinfo->spacetype = SPACE_INFO;
    BLI_addtail(&area->spacedata, sinfo);
  }
  /* add local view3d too */
  else if (area->spacetype == SPACE_VIEW3D) {
    blo_do_versions_view3d_split_250(area->spacedata.first, &area->regionbase);
  }

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    BLO_read_list(reader, &(sl->regionbase));

    /* if we do not have the spacetype registered we cannot
     * free it, so don't allocate any new memory for such spacetypes. */
    if (!BKE_spacetype_exists(sl->spacetype)) {
      sl->spacetype = SPACE_EMPTY;
    }

    LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
      direct_link_region(reader, region, sl->spacetype);
    }

    if (sl->spacetype == SPACE_VIEW3D) {
      View3D *v3d = (View3D *)sl;

      v3d->flag |= V3D_INVALID_BACKBUF;

      if (v3d->gpd) {
        BLO_read_data_address(reader, &v3d->gpd);
        BKE_gpencil_blend_read_data(reader, v3d->gpd);
      }
      BLO_read_data_address(reader, &v3d->localvd);

      /* Runtime data */
      v3d->runtime.properties_storage = NULL;
      v3d->runtime.flag = 0;

      /* render can be quite heavy, set to solid on load */
      if (v3d->shading.type == OB_RENDER) {
        v3d->shading.type = OB_SOLID;
      }
      v3d->shading.prev_type = OB_SOLID;

      direct_link_view3dshading(reader, &v3d->shading);

      blo_do_versions_view3d_split_250(v3d, &sl->regionbase);
    }
    else if (sl->spacetype == SPACE_GRAPH) {
      SpaceGraph *sipo = (SpaceGraph *)sl;

      BLO_read_data_address(reader, &sipo->ads);
      BLI_listbase_clear(&sipo->runtime.ghost_curves);
    }
    else if (sl->spacetype == SPACE_NLA) {
      SpaceNla *snla = (SpaceNla *)sl;

      BLO_read_data_address(reader, &snla->ads);
    }
    else if (sl->spacetype == SPACE_OUTLINER) {
      SpaceOutliner *space_outliner = (SpaceOutliner *)sl;

      /* use newdataadr_no_us and do not free old memory avoiding double
       * frees and use of freed memory. this could happen because of a
       * bug fixed in revision 58959 where the treestore memory address
       * was not unique */
      TreeStore *ts = newdataadr_no_us(reader->fd, space_outliner->treestore);
      space_outliner->treestore = NULL;
      if (ts) {
        TreeStoreElem *elems = newdataadr_no_us(reader->fd, ts->data);

        space_outliner->treestore = BLI_mempool_create(
            sizeof(TreeStoreElem), ts->usedelem, 512, BLI_MEMPOOL_ALLOW_ITER);
        if (ts->usedelem && elems) {
          for (int i = 0; i < ts->usedelem; i++) {
            TreeStoreElem *new_elem = BLI_mempool_alloc(space_outliner->treestore);
            *new_elem = elems[i];
          }
        }
        /* we only saved what was used */
        space_outliner->storeflag |= SO_TREESTORE_CLEANUP; /* at first draw */
      }
      space_outliner->treehash = NULL;
      space_outliner->tree.first = space_outliner->tree.last = NULL;
    }
    else if (sl->spacetype == SPACE_IMAGE) {
      SpaceImage *sima = (SpaceImage *)sl;

      sima->iuser.scene = NULL;
      sima->iuser.ok = 1;
      sima->scopes.waveform_1 = NULL;
      sima->scopes.waveform_2 = NULL;
      sima->scopes.waveform_3 = NULL;
      sima->scopes.vecscope = NULL;
      sima->scopes.ok = 0;

      /* WARNING: gpencil data is no longer stored directly in sima after 2.5
       * so sacrifice a few old files for now to avoid crashes with new files!
       * committed: r28002 */
#if 0
      sima->gpd = newdataadr(fd, sima->gpd);
      if (sima->gpd) {
        BKE_gpencil_blend_read_data(fd, sima->gpd);
      }
#endif
    }
    else if (sl->spacetype == SPACE_NODE) {
      SpaceNode *snode = (SpaceNode *)sl;

      if (snode->gpd) {
        BLO_read_data_address(reader, &snode->gpd);
        BKE_gpencil_blend_read_data(reader, snode->gpd);
      }

      BLO_read_list(reader, &snode->treepath);
      snode->edittree = NULL;
      snode->iofsd = NULL;
      BLI_listbase_clear(&snode->linkdrag);
    }
    else if (sl->spacetype == SPACE_TEXT) {
      SpaceText *st = (SpaceText *)sl;
      memset(&st->runtime, 0, sizeof(st->runtime));
    }
    else if (sl->spacetype == SPACE_SEQ) {
      SpaceSeq *sseq = (SpaceSeq *)sl;

      /* grease pencil data is not a direct data and can't be linked from direct_link*
       * functions, it should be linked from lib_link* functions instead
       *
       * otherwise it'll lead to lost grease data on open because it'll likely be
       * read from file after all other users of grease pencil and newdataadr would
       * simple return NULL here (sergey)
       */
#if 0
      if (sseq->gpd) {
        sseq->gpd = newdataadr(fd, sseq->gpd);
        BKE_gpencil_blend_read_data(fd, sseq->gpd);
      }
#endif
      sseq->scopes.reference_ibuf = NULL;
      sseq->scopes.zebra_ibuf = NULL;
      sseq->scopes.waveform_ibuf = NULL;
      sseq->scopes.sep_waveform_ibuf = NULL;
      sseq->scopes.vector_ibuf = NULL;
      sseq->scopes.histogram_ibuf = NULL;
    }
    else if (sl->spacetype == SPACE_PROPERTIES) {
      SpaceProperties *sbuts = (SpaceProperties *)sl;

      sbuts->path = NULL;
      sbuts->texuser = NULL;
      sbuts->mainbo = sbuts->mainb;
      sbuts->mainbuser = sbuts->mainb;
      sbuts->runtime = NULL;
    }
    else if (sl->spacetype == SPACE_CONSOLE) {
      SpaceConsole *sconsole = (SpaceConsole *)sl;

      BLO_read_list(reader, &sconsole->scrollback);
      BLO_read_list(reader, &sconsole->history);

      /* comma expressions, (e.g. expr1, expr2, expr3) evaluate each expression,
       * from left to right.  the right-most expression sets the result of the comma
       * expression as a whole*/
      LISTBASE_FOREACH_MUTABLE (ConsoleLine *, cl, &sconsole->history) {
        BLO_read_data_address(reader, &cl->line);
        if (cl->line) {
          /* the allocted length is not written, so reset here */
          cl->len_alloc = cl->len + 1;
        }
        else {
          BLI_remlink(&sconsole->history, cl);
          MEM_freeN(cl);
        }
      }
    }
    else if (sl->spacetype == SPACE_FILE) {
      SpaceFile *sfile = (SpaceFile *)sl;

      /* this sort of info is probably irrelevant for reloading...
       * plus, it isn't saved to files yet!
       */
      sfile->folders_prev = sfile->folders_next = NULL;
      sfile->files = NULL;
      sfile->layout = NULL;
      sfile->op = NULL;
      sfile->previews_timer = NULL;
      BLO_read_data_address(reader, &sfile->params);
    }
    else if (sl->spacetype == SPACE_CLIP) {
      SpaceClip *sclip = (SpaceClip *)sl;

      sclip->scopes.track_search = NULL;
      sclip->scopes.track_preview = NULL;
      sclip->scopes.ok = 0;
    }
  }

  BLI_listbase_clear(&area->actionzones);

  BLO_read_data_address(reader, &area->v1);
  BLO_read_data_address(reader, &area->v2);
  BLO_read_data_address(reader, &area->v3);
  BLO_read_data_address(reader, &area->v4);
}

static void lib_link_area(BlendLibReader *reader, ID *parent_id, ScrArea *area)
{
  BLO_read_id_address(reader, parent_id->lib, &area->full);

  memset(&area->runtime, 0x0, sizeof(area->runtime));

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    switch (sl->spacetype) {
      case SPACE_VIEW3D: {
        View3D *v3d = (View3D *)sl;

        BLO_read_id_address(reader, parent_id->lib, &v3d->camera);
        BLO_read_id_address(reader, parent_id->lib, &v3d->ob_center);

        if (v3d->localvd) {
          BLO_read_id_address(reader, parent_id->lib, &v3d->localvd->camera);
        }
        break;
      }
      case SPACE_GRAPH: {
        SpaceGraph *sipo = (SpaceGraph *)sl;
        bDopeSheet *ads = sipo->ads;

        if (ads) {
          BLO_read_id_address(reader, parent_id->lib, &ads->source);
          BLO_read_id_address(reader, parent_id->lib, &ads->filter_grp);
        }
        break;
      }
      case SPACE_PROPERTIES: {
        SpaceProperties *sbuts = (SpaceProperties *)sl;
        BLO_read_id_address(reader, parent_id->lib, &sbuts->pinid);
        if (sbuts->pinid == NULL) {
          sbuts->flag &= ~SB_PIN_CONTEXT;
        }
        break;
      }
      case SPACE_FILE:
        break;
      case SPACE_ACTION: {
        SpaceAction *saction = (SpaceAction *)sl;
        bDopeSheet *ads = &saction->ads;

        if (ads) {
          BLO_read_id_address(reader, parent_id->lib, &ads->source);
          BLO_read_id_address(reader, parent_id->lib, &ads->filter_grp);
        }

        BLO_read_id_address(reader, parent_id->lib, &saction->action);
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = (SpaceImage *)sl;

        BLO_read_id_address(reader, parent_id->lib, &sima->image);
        BLO_read_id_address(reader, parent_id->lib, &sima->mask_info.mask);

        /* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
         * so fingers crossed this works fine!
         */
        BLO_read_id_address(reader, parent_id->lib, &sima->gpd);
        break;
      }
      case SPACE_SEQ: {
        SpaceSeq *sseq = (SpaceSeq *)sl;

        /* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
         * so fingers crossed this works fine!
         */
        BLO_read_id_address(reader, parent_id->lib, &sseq->gpd);
        break;
      }
      case SPACE_NLA: {
        SpaceNla *snla = (SpaceNla *)sl;
        bDopeSheet *ads = snla->ads;

        if (ads) {
          BLO_read_id_address(reader, parent_id->lib, &ads->source);
          BLO_read_id_address(reader, parent_id->lib, &ads->filter_grp);
        }
        break;
      }
      case SPACE_TEXT: {
        SpaceText *st = (SpaceText *)sl;

        BLO_read_id_address(reader, parent_id->lib, &st->text);
        break;
      }
      case SPACE_SCRIPT: {
        SpaceScript *scpt = (SpaceScript *)sl;
        /*scpt->script = NULL; - 2.45 set to null, better re-run the script */
        if (scpt->script) {
          BLO_read_id_address(reader, parent_id->lib, &scpt->script);
          if (scpt->script) {
            SCRIPT_SET_NULL(scpt->script);
          }
        }
        break;
      }
      case SPACE_OUTLINER: {
        SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
        BLO_read_id_address(reader, NULL, &space_outliner->search_tse.id);

        if (space_outliner->treestore) {
          TreeStoreElem *tselem;
          BLI_mempool_iter iter;

          BLI_mempool_iternew(space_outliner->treestore, &iter);
          while ((tselem = BLI_mempool_iterstep(&iter))) {
            BLO_read_id_address(reader, NULL, &tselem->id);
          }
          if (space_outliner->treehash) {
            /* rebuild hash table, because it depends on ids too */
            space_outliner->storeflag |= SO_TREESTORE_REBUILD;
          }
        }
        break;
      }
      case SPACE_NODE: {
        SpaceNode *snode = (SpaceNode *)sl;

        /* node tree can be stored locally in id too, link this first */
        BLO_read_id_address(reader, parent_id->lib, &snode->id);
        BLO_read_id_address(reader, parent_id->lib, &snode->from);

        bNodeTree *ntree = snode->id ? ntreeFromID(snode->id) : NULL;
        if (ntree) {
          snode->nodetree = ntree;
        }
        else {
          BLO_read_id_address(reader, parent_id->lib, &snode->nodetree);
        }

        bNodeTreePath *path;
        for (path = snode->treepath.first; path; path = path->next) {
          if (path == snode->treepath.first) {
            /* first nodetree in path is same as snode->nodetree */
            path->nodetree = snode->nodetree;
          }
          else {
            BLO_read_id_address(reader, parent_id->lib, &path->nodetree);
          }

          if (!path->nodetree) {
            break;
          }
        }

        /* remaining path entries are invalid, remove */
        bNodeTreePath *path_next;
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
        break;
      }
      case SPACE_CLIP: {
        SpaceClip *sclip = (SpaceClip *)sl;
        BLO_read_id_address(reader, parent_id->lib, &sclip->clip);
        BLO_read_id_address(reader, parent_id->lib, &sclip->mask_info.mask);
        break;
      }
      default:
        break;
    }
  }
}

/**
 * \return false on error.
 */
static bool direct_link_area_map(BlendDataReader *reader, ScrAreaMap *area_map)
{
  BLO_read_list(reader, &area_map->vertbase);
  BLO_read_list(reader, &area_map->edgebase);
  BLO_read_list(reader, &area_map->areabase);
  LISTBASE_FOREACH (ScrArea *, area, &area_map->areabase) {
    direct_link_area(reader, area);
  }

  /* edges */
  LISTBASE_FOREACH (ScrEdge *, se, &area_map->edgebase) {
    BLO_read_data_address(reader, &se->v1);
    BLO_read_data_address(reader, &se->v2);
    BKE_screen_sort_scrvert(&se->v1, &se->v2);

    if (se->v1 == NULL) {
      BLI_remlink(&area_map->edgebase, se);

      return false;
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR-data
 * \{ */

static void direct_link_wm_xr_data(BlendDataReader *reader, wmXrData *xr_data)
{
  direct_link_view3dshading(reader, &xr_data->session_settings.shading);
}

static void lib_link_wm_xr_data(BlendLibReader *reader, ID *parent_id, wmXrData *xr_data)
{
  BLO_read_id_address(reader, parent_id->lib, &xr_data->session_settings.base_pose_object);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Window Manager
 * \{ */

static void direct_link_windowmanager(BlendDataReader *reader, wmWindowManager *wm)
{
  id_us_ensure_real(&wm->id);
  BLO_read_list(reader, &wm->windows);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    BLO_read_data_address(reader, &win->parent);

    WorkSpaceInstanceHook *hook = win->workspace_hook;
    BLO_read_data_address(reader, &win->workspace_hook);

    /* This will be NULL for any pre-2.80 blend file. */
    if (win->workspace_hook != NULL) {
      /* We need to restore a pointer to this later when reading workspaces,
       * so store in global oldnew-map.
       * Note that this is only needed for versioning of older .blend files now.. */
      oldnewmap_insert(reader->fd->globmap, hook, win->workspace_hook, 0);
      /* Cleanup pointers to data outside of this data-block scope. */
      win->workspace_hook->act_layout = NULL;
      win->workspace_hook->temp_workspace_store = NULL;
      win->workspace_hook->temp_layout_store = NULL;
    }

    direct_link_area_map(reader, &win->global_areas);

    win->ghostwin = NULL;
    win->gpuctx = NULL;
    win->eventstate = NULL;
    win->cursor_keymap_status = NULL;
    win->tweak = NULL;
#ifdef WIN32
    win->ime_data = NULL;
#endif

    BLI_listbase_clear(&win->queue);
    BLI_listbase_clear(&win->handlers);
    BLI_listbase_clear(&win->modalhandlers);
    BLI_listbase_clear(&win->gesture);

    win->active = 0;

    win->cursor = 0;
    win->lastcursor = 0;
    win->modalcursor = 0;
    win->grabcursor = 0;
    win->addmousemove = true;
    BLO_read_data_address(reader, &win->stereo3d_format);

    /* Multi-view always fallback to anaglyph at file opening
     * otherwise quad-buffer saved files can break Blender. */
    if (win->stereo3d_format) {
      win->stereo3d_format->display_mode = S3D_DISPLAY_ANAGLYPH;
    }
  }

  direct_link_wm_xr_data(reader, &wm->xr);

  BLI_listbase_clear(&wm->timers);
  BLI_listbase_clear(&wm->operators);
  BLI_listbase_clear(&wm->paintcursors);
  BLI_listbase_clear(&wm->queue);
  BKE_reports_init(&wm->reports, RPT_STORE);

  BLI_listbase_clear(&wm->keyconfigs);
  wm->defaultconf = NULL;
  wm->addonconf = NULL;
  wm->userconf = NULL;
  wm->undo_stack = NULL;

  wm->message_bus = NULL;

  wm->xr.runtime = NULL;

  BLI_listbase_clear(&wm->jobs);
  BLI_listbase_clear(&wm->drags);

  wm->windrawable = NULL;
  wm->winactive = NULL;
  wm->initialized = 0;
  wm->op_undo_depth = 0;
  wm->is_interface_locked = 0;
}

static void lib_link_windowmanager(BlendLibReader *reader, wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->workspace_hook) { /* NULL for old files */
      lib_link_workspace_instance_hook(reader, win->workspace_hook, &wm->id);
    }
    BLO_read_id_address(reader, wm->id.lib, &win->scene);
    /* deprecated, but needed for versioning (will be NULL'ed then) */
    BLO_read_id_address(reader, NULL, &win->screen);

    LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
      lib_link_area(reader, &wm->id, area);
    }

    lib_link_wm_xr_data(reader, &wm->id, &wm->xr);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read ID: Screen
 * \{ */

/* note: file read without screens option G_FILE_NO_UI;
 * check lib pointers in call below */
static void lib_link_screen(BlendLibReader *reader, bScreen *screen)
{
  /* deprecated, but needed for versioning (will be NULL'ed then) */
  BLO_read_id_address(reader, screen->id.lib, &screen->scene);

  screen->animtimer = NULL; /* saved in rare cases */
  screen->tool_tip = NULL;
  screen->scrubbing = false;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    lib_link_area(reader, &screen->id, area);
  }
}

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
static int lib_link_seq_clipboard_cb(Sequence *seq, void *arg_pt)
{
  struct IDNameLib_Map *id_map = arg_pt;

  lib_link_seq_clipboard_pt_restore((ID *)seq->scene, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->scene_camera, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->clip, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->mask, id_map);
  lib_link_seq_clipboard_pt_restore((ID *)seq->sound, id_map);
  return 1;
}

static void lib_link_clipboard_restore(struct IDNameLib_Map *id_map)
{
  /* update IDs stored in sequencer clipboard */
  BKE_sequencer_base_recursive_apply(&seqbase_clipboard, lib_link_seq_clipboard_cb, id_map);
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
      BLI_assert(0);
      return IDWALK_RET_NOP;
    }
  }

  struct IDNameLib_Map *id_map = cb_data->user_data;

  /* Note: Handling of usercount here is really bad, defining its own system...
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

          /* Localview can become invalid during undo/redo steps,
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

            /* Regionbase storage is different depending if the space is active. */
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
           * (complicated because this contains data pointers too, not just ID)*/
          MEM_SAFE_FREE(sbuts->path);
        }
        else if (sl->spacetype == SPACE_FILE) {
          SpaceFile *sfile = (SpaceFile *)sl;
          sfile->op = NULL;
          sfile->previews_timer = NULL;
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

          st->text = restore_pointer_by_name(id_map, (ID *)st->text, USER_REAL);
          if (st->text == NULL) {
            st->text = newmain->texts.first;
          }
        }
        else if (sl->spacetype == SPACE_SCRIPT) {
          SpaceScript *scpt = (SpaceScript *)sl;

          scpt->script = restore_pointer_by_name(id_map, (ID *)scpt->script, USER_REAL);

          /*screen->script = NULL; - 2.45 set to null, better re-run the script */
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
            if (space_outliner->treehash) {
              /* rebuild hash table, because it depends on ids too */
              space_outliner->storeflag |= SO_TREESTORE_REBUILD;
            }
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
      }
    }
  }
}

/**
 * Used to link a file (without UI) to the current UI.
 * Note that it assumes the old pointers in UI are still valid, so old Main is not freed.
 */
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
    if (BKE_view_layer_find(win->scene, win->view_layer_name) == NULL) {
      STRNCPY(win->view_layer_name, cur_view_layer->name);
    }
    BKE_workspace_active_set(win->workspace_hook, workspace);

    /* keep cursor location through undo */
    memcpy(&win->scene->cursor, &oldscene->cursor, sizeof(win->scene->cursor));

    /* Note: even though that function seems to redo part of what is done by
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

/* for the saved 2.50 files without regiondata */
/* and as patch for 2.48 and older */
void blo_do_versions_view3d_split_250(View3D *v3d, ListBase *regions)
{
  LISTBASE_FOREACH (ARegion *, region, regions) {
    if (region->regiontype == RGN_TYPE_WINDOW && region->regiondata == NULL) {
      RegionView3D *rv3d;

      rv3d = region->regiondata = MEM_callocN(sizeof(RegionView3D), "region v3d patch");
      rv3d->persp = (char)v3d->persp;
      rv3d->view = (char)v3d->view;
      rv3d->dist = v3d->dist;
      copy_v3_v3(rv3d->ofs, v3d->ofs);
      copy_qt_qt(rv3d->viewquat, v3d->viewquat);
    }
  }

  /* this was not initialized correct always */
  if (v3d->gridsubdiv == 0) {
    v3d->gridsubdiv = 10;
  }
}

static bool direct_link_screen(BlendDataReader *reader, bScreen *screen)
{
  bool success = true;

  screen->regionbase.first = screen->regionbase.last = NULL;
  screen->context = NULL;
  screen->active_region = NULL;

  BLO_read_data_address(reader, &screen->preview);
  BKE_previewimg_blend_read(reader, screen->preview);

  if (!direct_link_area_map(reader, AREAMAP_FROM_SCREEN(screen))) {
    printf("Error reading Screen %s... removing it.\n", screen->id.name + 2);
    success = false;
  }

  return success;
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
        blo_reportf_wrap(fd->reports,
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
  /* BLO_read_from_memory uses a blank filename */
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

  BKE_lib_libblock_session_uuid_ensure(ph_id);

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
    case ID_CU:
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
    case ID_HA:
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
    case ID_WM:
      direct_link_windowmanager(&reader, (wmWindowManager *)id);
      break;
    case ID_SCR:
      success = direct_link_screen(&reader, (bScreen *)id);
      break;
    case ID_SCE:
      direct_link_scene(&reader, (Scene *)id);
      break;
    case ID_OB:
      direct_link_object(&reader, (Object *)id);
      break;
    case ID_IP:
      direct_link_ipo(&reader, (Ipo *)id);
      break;
    case ID_LI:
      direct_link_library(fd, (Library *)id, main);
      break;
    case ID_GR:
      direct_link_collection(&reader, (Collection *)id);
      break;
    case ID_PA:
      direct_link_particlesettings(&reader, (ParticleSettings *)id);
      break;
    case ID_WS:
      direct_link_workspace(&reader, (WorkSpace *)id);
      break;
    case ID_ME:
    case ID_LT:
    case ID_AC:
    case ID_NT:
    case ID_LS:
    case ID_TXT:
    case ID_VF:
    case ID_MC:
    case ID_PAL:
    case ID_PC:
    case ID_BR:
    case ID_IM:
    case ID_LA:
    case ID_MA:
    case ID_MB:
    case ID_CU:
    case ID_CA:
    case ID_WO:
    case ID_MSK:
    case ID_SPK:
    case ID_AR:
    case ID_LP:
    case ID_KE:
    case ID_TE:
    case ID_GD:
    case ID_HA:
    case ID_PT:
    case ID_VO:
    case ID_SIM:
    case ID_SO:
    case ID_CF:
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
  DEBUG_PRINTF("UNDO: restore library %s\n", id->name);

  Main *libmain = fd->old_mainlist->first;
  /* Skip oldmain itself... */
  for (libmain = libmain->next; libmain; libmain = libmain->next) {
    DEBUG_PRINTF("  compare with %s -> ", libmain->curlib ? libmain->curlib->id.name : "<NULL>");
    if (libmain->curlib && STREQ(id->name, libmain->curlib->id.name)) {
      Main *oldmain = fd->old_mainlist->first;
      DEBUG_PRINTF("match!\n");
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
    DEBUG_PRINTF("no match\n");
  }

  return false;
}

/* For undo, restore existing linked datablock from the old main. */
static bool read_libblock_undo_restore_linked(FileData *fd, Main *main, const ID *id, BHead *bhead)
{
  DEBUG_PRINTF("UNDO: restore linked datablock %s\n", id->name);
  DEBUG_PRINTF("  from %s (%s): ",
               main->curlib ? main->curlib->id.name : "<NULL>",
               main->curlib ? main->curlib->filepath : "<NULL>");

  ID *id_old = BKE_libblock_find_name(main, GS(id->name), id->name + 2);
  if (id_old != NULL) {
    DEBUG_PRINTF("  found!\n");
    /* Even though we found our linked ID, there is no guarantee its address
     * is still the same. */
    if (id_old != bhead->old) {
      oldnewmap_insert(fd->libmap, bhead->old, id_old, GS(id_old->name));
    }

    /* No need to do anything else for ID_LINK_PLACEHOLDER, it's assumed
     * already present in its lib's main. */
    return true;
  }

  DEBUG_PRINTF("  not found\n");
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

  /* As usual, proxies require some special love...
   * In `blo_clear_proxy_pointers_from_lib()` we clear all `proxy_from` pointers to local IDs, for
   * undo. This is required since we do not re-read linked data in that case, so we also do not
   * re-'lib_link' their pointers.
   * Those `proxy_from` pointers are then re-defined properly when lib_linking the newly read local
   * object. However, in case of re-used data 'as-is', we never lib_link it again, so we have to
   * fix those backward pointers here. */
  if (GS(id_old->name) == ID_OB) {
    Object *ob = (Object *)id_old;
    if (ob->proxy != NULL) {
      ob->proxy->proxy_from = ob;
    }
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

  /* XXX 3DCursor (witch is UI data and as such should not be affected by undo) is stored in
   * Scene... So this requires some special handling, previously done in `blo_lib_link_restore()`,
   * but this cannot work anymore when we overwrite existing memory... */
  if (idcode == ID_SCE) {
    Scene *scene_old = (Scene *)id_old;
    Scene *scene = (Scene *)id;
    SWAP(View3DCursor, scene_old->cursor, scene->cursor);
  }

  Main *old_bmain = fd->old_mainlist->first;
  ListBase *old_lb = which_libbase(old_bmain, idcode);
  ListBase *new_lb = which_libbase(main, idcode);
  BLI_remlink(old_lb, id_old);
  BLI_remlink(new_lb, id);

  /* We do not need any remapping from this call here, since no ID pointer is valid in the data
   * currently (they are all pointing to old addresses, and need to go through `lib_link`
   * process). So we can pass NULL for the Main pointer parameter. */
  BKE_lib_id_swap_full(NULL, id, id_old);

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
  DEBUG_PRINTF("UNDO: read %s (uuid %u) -> ", id->name, id->session_uuid);

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
    DEBUG_PRINTF("keep identical datablock\n");

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
    oldnewmap_insert(fd->libmap, bhead->old, id_old, bhead->code);

    *r_id_old = id_old;
    return true;
  }
  if (id_old != NULL) {
    /* Local datablock was changed. Restore at the address of the old datablock. */
    DEBUG_PRINTF("read to old existing address\n");
    *r_id_old = id_old;
    return false;
  }

  /* Local datablock does not exist in the undo step, so read from scratch. */
  DEBUG_PRINTF("read at new address\n");
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
  if (fd->memfile != NULL) {
    if (read_libblock_undo_restore(fd, main, bhead, tag, &id_old)) {
      if (r_id) {
        *r_id = id_old;
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
    printf("%s: unknown id code '%c%c'\n", __func__, (idcode & 0xff), (idcode >> 8));
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
  oldnewmap_insert(fd->libmap, bhead->old, id_target, bhead->code);

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
  }

  return bhead;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Global Data
 * \{ */

/* note, this has to be kept for reading older files... */
/* also version info is written here */
static BHead *read_global(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
  FileGlobal *fg = read_struct(fd, bhead, "Global");

  /* copy to bfd handle */
  bfd->main->subversionfile = fg->subversion;
  bfd->main->minversionfile = fg->minversion;
  bfd->main->minsubversionfile = fg->minsubversion;
  bfd->main->build_commit_timestamp = fg->build_commit_timestamp;
  BLI_strncpy(bfd->main->build_hash, fg->build_hash, sizeof(bfd->main->build_hash));

  bfd->fileflags = fg->fileflags;
  bfd->globalf = fg->globalf;
  BLI_strncpy(bfd->filename, fg->filename, sizeof(bfd->filename));

  /* Error in 2.65 and older: main->name was not set if you save from startup
   * (not after loading file). */
  if (bfd->filename[0] == 0) {
    if (fd->fileversion < 265 || (fd->fileversion == 265 && fg->subversion < 1)) {
      if ((G.fileflags & G_FILE_RECOVER) == 0) {
        BLI_strncpy(bfd->filename, BKE_main_blendfile_path(bfd->main), sizeof(bfd->filename));
      }
    }

    /* early 2.50 version patch - filename not in FileGlobal struct at all */
    if (fd->fileversion <= 250) {
      BLI_strncpy(bfd->filename, BKE_main_blendfile_path(bfd->main), sizeof(bfd->filename));
    }
  }

  if (G.fileflags & G_FILE_RECOVER) {
    BLI_strncpy(fd->relabase, fg->filename, sizeof(fd->relabase));
  }

  bfd->curscreen = fg->curscreen;
  bfd->curscene = fg->curscene;
  bfd->cur_view_layer = fg->cur_view_layer;

  MEM_freeN(fg);

  fd->globalf = bfd->globalf;
  fd->fileflags = bfd->fileflags;

  return blo_bhead_next(fd, bhead);
}

/* note, this has to be kept for reading older files... */
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

    printf("read file %s\n  Version %d sub %d date %s hash %s\n",
           fd->relabase,
           main->versionfile,
           main->subversionfile,
           build_commit_datetime,
           main->build_hash);
  }

  blo_do_versions_pre250(fd, lib, main);
  blo_do_versions_250(fd, lib, main);
  blo_do_versions_260(fd, lib, main);
  blo_do_versions_270(fd, lib, main);
  blo_do_versions_280(fd, lib, main);
  blo_do_versions_290(fd, lib, main);
  blo_do_versions_cycles(fd, lib, main);

  /* WATCH IT!!!: pointers from libdata have not been converted yet here! */
  /* WATCH IT 2!: Userdef struct init see do_versions_userdef() above! */

  /* don't forget to set version number in BKE_blender_version.h! */

  main->is_locked_for_linking = false;
}

static void do_versions_after_linking(Main *main, ReportList *reports)
{
  //  printf("%s for %s (%s), %d.%d\n", __func__, main->curlib ? main->curlib->filepath :
  //         main->name, main->curlib ? "LIB" : "MAIN", main->versionfile, main->subversionfile);

  /* Don't allow versioning to create new data-blocks. */
  main->is_locked_for_linking = true;

  do_versions_after_linking_250(main);
  do_versions_after_linking_260(main);
  do_versions_after_linking_270(main);
  do_versions_after_linking_280(main, reports);
  do_versions_after_linking_290(main, reports);
  do_versions_after_linking_cycles(main);

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

    if (fd->memfile != NULL && GS(id->name) == ID_WM) {
      /* No load UI for undo memfiles.
       * Only WM currently, SCR needs it still (see below), and so does WS? */
      continue;
    }

    if (fd->memfile != NULL && do_partial_undo && (id->tag & LIB_TAG_UNDO_OLD_ID_REUSED) != 0) {
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

    /* Note: ID types are processed in reverse order as defined by INDEX_ID_XXX enums in DNA_ID.h.
     * This ensures handling of most dependencies in proper order, as elsewhere in code.
     * Please keep order of entries in that switch matching that order, it's easier to quickly see
     * whether something is wrong then. */
    switch (GS(id->name)) {
      case ID_WM:
        lib_link_windowmanager(&reader, (wmWindowManager *)id);
        break;
      case ID_WS:
        /* Could we skip WS in undo case? */
        lib_link_workspaces(&reader, (WorkSpace *)id);
        break;
      case ID_SCE:
        lib_link_scene(&reader, (Scene *)id);
        break;
      case ID_OB:
        lib_link_object(&reader, (Object *)id);
        break;
      case ID_SCR:
        /* DO NOT skip screens here, 3D viewport may contains pointers
         * to other ID data (like #View3D.ob_center)! See T41411. */
        lib_link_screen(&reader, (bScreen *)id);
        break;
      case ID_PA:
        lib_link_particlesettings(&reader, (ParticleSettings *)id);
        break;
      case ID_GR:
        lib_link_collection(&reader, (Collection *)id);
        break;
      case ID_IP:
        /* XXX deprecated... still needs to be maintained for version patches still. */
        lib_link_ipo(&reader, (Ipo *)id);
        break;
      case ID_LI:
        lib_link_library(&reader, (Library *)id); /* Only init users. */
        break;
      case ID_ME:
      case ID_LT:
      case ID_AC:
      case ID_NT:
      case ID_LS:
      case ID_TXT:
      case ID_VF:
      case ID_MC:
      case ID_PAL:
      case ID_PC:
      case ID_BR:
      case ID_IM:
      case ID_LA:
      case ID_MA:
      case ID_MB:
      case ID_CU:
      case ID_CA:
      case ID_WO:
      case ID_MSK:
      case ID_SPK:
      case ID_AR:
      case ID_LP:
      case ID_KE:
      case ID_TE:
      case ID_GD:
      case ID_HA:
      case ID_PT:
      case ID_VO:
      case ID_SIM:
      case ID_SO:
      case ID_CF:
        /* Do nothing. Handled by IDTypeInfo callback. */
        break;
    }

    id->tag &= ~LIB_TAG_NEED_LINK;
  }
  FOREACH_MAIN_ID_END;

  /* Check for possible cycles in scenes' 'set' background property. */
  lib_link_scenes_check_set(bmain);

  /* We could integrate that to mesh/curve/lattice lib_link, but this is really cheap process,
   * so simpler to just use it directly in this single call. */
  BLO_main_validate_shapekeys(bmain, NULL);

  /* We have to rebuild that runtime information *after* all data-blocks have been properly linked.
   */
  BKE_main_collections_parent_relations_rebuild(bmain);

#ifndef NDEBUG
  /* Double check we do not have any 'need link' tag remaining, this should never be the case once
   * this function has run. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BLI_assert((id->tag & LIB_TAG_NEED_LINK) == 0);
  }
  FOREACH_MAIN_ID_END;
#endif
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

  if (fd->memfile != NULL) {
    DEBUG_PRINTF("\nUNDO: read step\n");
  }

  bfd = MEM_callocN(sizeof(BlendFileData), "blendfiledata");

  bfd->main = BKE_main_new();
  bfd->main->versionfile = fd->fileversion;

  bfd->type = BLENFILETYPE_BLEND;

  if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    BLI_addtail(&mainlist, bfd->main);
    fd->mainlist = &mainlist;
    BLI_strncpy(bfd->main->name, filepath, sizeof(bfd->main->name));
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
        const size_t sz = BLEN_THUMB_MEMSIZE(width, height);
        bfd->main->blen_thumb = MEM_mallocN(sz, __func__);

        BLI_assert((sz - sizeof(*bfd->main->blen_thumb)) ==
                   (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*data) * 2)));
        bfd->main->blen_thumb->width = width;
        bfd->main->blen_thumb->height = height;
        memcpy(bfd->main->blen_thumb->rect, &data[2], sz - sizeof(*bfd->main->blen_thumb));
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
  }

  /* do before read_libraries, but skip undo case */
  if (fd->memfile == NULL) {
    if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
      do_versions(fd, NULL, bfd->main);
    }

    if ((fd->skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
      do_versions_userdef(fd, bfd);
    }
  }

  if ((fd->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    read_libraries(fd, &mainlist);

    blo_join_main(&mainlist);

    lib_link_all(fd, bfd->main);

    /* Skip in undo case. */
    if (fd->memfile == NULL) {
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
        do_versions_after_linking(mainvar, fd->reports);
      }
      blo_join_main(&mainlist);

      /* And we have to compute those user-reference-counts again, as `do_versions_after_linking()`
       * does not always properly handle user counts, and/or that function does not take into
       * account old, deprecated data. */
      BKE_main_id_refcount_recompute(bfd->main, false);

      /* After all data has been read and versioned, uses LIB_TAG_NEW. */
      ntreeUpdateAllNew(bfd->main);
    }

    placeholders_ensure_valid(bfd->main);

    BKE_main_id_tag_all(bfd->main, LIB_TAG_NEW, false);

    /* Now that all our data-blocks are loaded,
     * we can re-generate overrides from their references. */
    if (fd->memfile == NULL) {
      /* Do not apply in undo case! */
      BKE_lib_override_library_main_update(bfd->main);
    }

    BKE_collections_after_lib_link(bfd->main);

    /* Make all relative paths, relative to the open blend file. */
    fix_relpaths_library(fd->relabase, bfd->main);

    link_global(fd, bfd); /* as last */
  }

  fd->mainlist = NULL; /* Safety, this is local variable, shall not be used afterward. */

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
  if (fd->memfile) {
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
  const char *idname = blo_bhead_id_name(fd, bhead);
  /* which_libbase can be NULL, intentionally not using idname+2 */
  return BLI_findstring(which_libbase(mainvar, GS(idname)), idname, offsetof(ID, name));
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
    /* Placeholder link to data-lock in another library. */
    BHead *bheadlib = find_previous_lib(fd, bhead);
    if (bheadlib == NULL) {
      return;
    }

    Library *lib = read_struct(fd, bheadlib, "Library");
    Main *libmain = blo_find_main(fd, lib->filepath, fd->relabase);

    if (libmain->curlib == NULL) {
      const char *idname = blo_bhead_id_name(fd, bhead);

      blo_reportf_wrap(fd->reports,
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
      read_libblock(fd, libmain, bhead, LIB_TAG_INDIRECT, false, NULL);
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
      oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);

      /* If "id" is a real data-lock and not a placeholder, we need to
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
      read_libblock(fd, mainvar, bhead, LIB_TAG_NEED_EXPAND | LIB_TAG_INDIRECT, false, NULL);
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
      oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);
      /* commented because this can print way too much */
      // if (G.debug & G_DEBUG) printf("expand: already read %s\n", id->name);
    }
  }
}

static BLOExpandDoitCallback expand_doit;

// XXX deprecated - old animation system
static void expand_ipo(BlendExpander *expander, Ipo *ipo)
{
  LISTBASE_FOREACH (IpoCurve *, icu, &ipo->curve) {
    if (icu->driver) {
      BLO_expand(expander, icu->driver->ob);
    }
  }
}

/* XXX deprecated - old animation system */
static void expand_constraint_channels(BlendExpander *expander, ListBase *chanbase)
{
  LISTBASE_FOREACH (bConstraintChannel *, chan, chanbase) {
    BLO_expand(expander, chan->ipo);
  }
}

static void expand_id(BlendExpander *expander, ID *id);
static void expand_collection(BlendExpander *expander, Collection *collection);

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
      expand_collection(expander, scene->master_collection);
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

static void expand_particlesettings(BlendExpander *expander, ParticleSettings *part)
{
  BLO_expand(expander, part->instance_object);
  BLO_expand(expander, part->instance_collection);
  BLO_expand(expander, part->force_group);
  BLO_expand(expander, part->bb_ob);
  BLO_expand(expander, part->collision_group);

  for (int a = 0; a < MAX_MTEX; a++) {
    if (part->mtex[a]) {
      BLO_expand(expander, part->mtex[a]->tex);
      BLO_expand(expander, part->mtex[a]->object);
    }
  }

  if (part->effector_weights) {
    BLO_expand(expander, part->effector_weights->group);
  }

  if (part->pd) {
    BLO_expand(expander, part->pd->tex);
    BLO_expand(expander, part->pd->f_source);
  }
  if (part->pd2) {
    BLO_expand(expander, part->pd2->tex);
    BLO_expand(expander, part->pd2->f_source);
  }

  if (part->boids) {
    LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
      LISTBASE_FOREACH (BoidRule *, rule, &state->rules) {
        if (rule->type == eBoidRuleType_Avoid) {
          BoidRuleGoalAvoid *gabr = (BoidRuleGoalAvoid *)rule;
          BLO_expand(expander, gabr->ob);
        }
        else if (rule->type == eBoidRuleType_FollowLeader) {
          BoidRuleFollowLeader *flbr = (BoidRuleFollowLeader *)rule;
          BLO_expand(expander, flbr->ob);
        }
      }
    }
  }

  LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
    BLO_expand(expander, dw->ob);
  }
}

static void expand_collection(BlendExpander *expander, Collection *collection)
{
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    BLO_expand(expander, cob->ob);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BLO_expand(expander, child->collection);
  }

#ifdef USE_COLLECTION_COMPAT_28
  if (collection->collection != NULL) {
    expand_scene_collection(expander, collection->collection);
  }
#endif
}

/* callback function used to expand constraint ID-links */
static void expand_constraint_cb(bConstraint *UNUSED(con),
                                 ID **idpoin,
                                 bool UNUSED(is_reference),
                                 void *userdata)
{
  BlendExpander *expander = userdata;
  BLO_expand(expander, *idpoin);
}

static void expand_constraints(BlendExpander *expander, ListBase *lb)
{
  BKE_constraints_id_loop(lb, expand_constraint_cb, expander);

  /* deprecated manual expansion stuff */
  LISTBASE_FOREACH (bConstraint *, curcon, lb) {
    if (curcon->ipo) {
      BLO_expand(expander, curcon->ipo); /* XXX deprecated - old animation system */
    }
  }
}

static void expand_pose(BlendExpander *expander, bPose *pose)
{
  if (!pose) {
    return;
  }

  LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
    expand_constraints(expander, &chan->constraints);
    IDP_BlendReadExpand(expander, chan->prop);
    BLO_expand(expander, chan->custom);
  }
}

static void expand_object_expandModifiers(void *userData,
                                          Object *UNUSED(ob),
                                          ID **idpoin,
                                          int UNUSED(cb_flag))
{
  BlendExpander *expander = userData;
  BLO_expand(expander, *idpoin);
}

static void expand_object(BlendExpander *expander, Object *ob)
{
  BLO_expand(expander, ob->data);

  /* expand_object_expandModifier() */
  if (ob->modifiers.first) {
    BKE_modifiers_foreach_ID_link(ob, expand_object_expandModifiers, expander);
  }

  /* expand_object_expandModifier() */
  if (ob->greasepencil_modifiers.first) {
    BKE_gpencil_modifiers_foreach_ID_link(ob, expand_object_expandModifiers, expander);
  }

  /* expand_object_expandShaderFx() */
  if (ob->shader_fx.first) {
    BKE_shaderfx_foreach_ID_link(ob, expand_object_expandModifiers, expander);
  }

  expand_pose(expander, ob->pose);
  BLO_expand(expander, ob->poselib);
  expand_constraints(expander, &ob->constraints);

  BLO_expand(expander, ob->gpd);

  /* XXX deprecated - old animation system (for version patching only) */
  BLO_expand(expander, ob->ipo);
  BLO_expand(expander, ob->action);

  expand_constraint_channels(expander, &ob->constraintChannels);

  LISTBASE_FOREACH (bActionStrip *, strip, &ob->nlastrips) {
    BLO_expand(expander, strip->object);
    BLO_expand(expander, strip->act);
    BLO_expand(expander, strip->ipo);
  }
  /* XXX deprecated - old animation system (for version patching only) */

  for (int a = 0; a < ob->totcol; a++) {
    BLO_expand(expander, ob->mat[a]);
  }

  PartEff *paf = blo_do_version_give_parteff_245(ob);
  if (paf && paf->group) {
    BLO_expand(expander, paf->group);
  }

  if (ob->instance_collection) {
    BLO_expand(expander, ob->instance_collection);
  }

  if (ob->proxy) {
    BLO_expand(expander, ob->proxy);
  }
  if (ob->proxy_group) {
    BLO_expand(expander, ob->proxy_group);
  }

  LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
    BLO_expand(expander, psys->part);
  }

  if (ob->pd) {
    BLO_expand(expander, ob->pd->tex);
    BLO_expand(expander, ob->pd->f_source);
  }

  if (ob->soft) {
    BLO_expand(expander, ob->soft->collision_group);

    if (ob->soft->effector_weights) {
      BLO_expand(expander, ob->soft->effector_weights->group);
    }
  }

  if (ob->rigidbody_constraint) {
    BLO_expand(expander, ob->rigidbody_constraint->ob1);
    BLO_expand(expander, ob->rigidbody_constraint->ob2);
  }
}

#ifdef USE_COLLECTION_COMPAT_28
static void expand_scene_collection(BlendExpander *expander, SceneCollection *sc)
{
  LISTBASE_FOREACH (LinkData *, link, &sc->objects) {
    BLO_expand(expander, link->data);
  }

  LISTBASE_FOREACH (SceneCollection *, nsc, &sc->scene_collections) {
    expand_scene_collection(expander, nsc);
  }
}
#endif

static void expand_scene(BlendExpander *expander, Scene *sce)
{
  LISTBASE_FOREACH (Base *, base_legacy, &sce->base) {
    BLO_expand(expander, base_legacy->object);
  }
  BLO_expand(expander, sce->camera);
  BLO_expand(expander, sce->world);

  BKE_keyingsets_blend_read_expand(expander, &sce->keyingsets);

  if (sce->set) {
    BLO_expand(expander, sce->set);
  }

  LISTBASE_FOREACH (SceneRenderLayer *, srl, &sce->r.layers) {
    BLO_expand(expander, srl->mat_override);
    LISTBASE_FOREACH (FreestyleModuleConfig *, module, &srl->freestyleConfig.modules) {
      if (module->script) {
        BLO_expand(expander, module->script);
      }
    }
    LISTBASE_FOREACH (FreestyleLineSet *, lineset, &srl->freestyleConfig.linesets) {
      if (lineset->group) {
        BLO_expand(expander, lineset->group);
      }
      BLO_expand(expander, lineset->linestyle);
    }
  }

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    IDP_BlendReadExpand(expander, view_layer->id_properties);

    LISTBASE_FOREACH (FreestyleModuleConfig *, module, &view_layer->freestyle_config.modules) {
      if (module->script) {
        BLO_expand(expander, module->script);
      }
    }

    LISTBASE_FOREACH (FreestyleLineSet *, lineset, &view_layer->freestyle_config.linesets) {
      if (lineset->group) {
        BLO_expand(expander, lineset->group);
      }
      BLO_expand(expander, lineset->linestyle);
    }
  }

  if (sce->gpd) {
    BLO_expand(expander, sce->gpd);
  }

  if (sce->ed) {
    Sequence *seq;

    SEQ_ALL_BEGIN (sce->ed, seq) {
      IDP_BlendReadExpand(expander, seq->prop);

      if (seq->scene) {
        BLO_expand(expander, seq->scene);
      }
      if (seq->scene_camera) {
        BLO_expand(expander, seq->scene_camera);
      }
      if (seq->clip) {
        BLO_expand(expander, seq->clip);
      }
      if (seq->mask) {
        BLO_expand(expander, seq->mask);
      }
      if (seq->sound) {
        BLO_expand(expander, seq->sound);
      }

      if (seq->type == SEQ_TYPE_TEXT && seq->effectdata) {
        TextVars *data = seq->effectdata;
        BLO_expand(expander, data->text_font);
      }
    }
    SEQ_ALL_END;
  }

  if (sce->rigidbody_world) {
    BLO_expand(expander, sce->rigidbody_world->group);
    BLO_expand(expander, sce->rigidbody_world->constraints);
  }

  LISTBASE_FOREACH (TimeMarker *, marker, &sce->markers) {
    IDP_BlendReadExpand(expander, marker->prop);

    if (marker->camera) {
      BLO_expand(expander, marker->camera);
    }
  }

  BLO_expand(expander, sce->clip);

#ifdef USE_COLLECTION_COMPAT_28
  if (sce->collection) {
    expand_scene_collection(expander, sce->collection);
  }
#endif

  if (sce->r.bake.cage_object) {
    BLO_expand(expander, sce->r.bake.cage_object);
  }
}

static void expand_workspace(BlendExpander *expander, WorkSpace *workspace)
{
  LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
    BLO_expand(expander, BKE_workspace_layout_screen_get(layout));
  }
}

/**
 * Set the callback func used over all ID data found by \a BLO_expand_main func.
 *
 * \param expand_doit_func: Called for each ID block it finds.
 */
void BLO_main_expander(BLOExpandDoitCallback expand_doit_func)
{
  expand_doit = expand_doit_func;
}

/**
 * Loop over all ID data in Main to mark relations.
 * Set (id->tag & LIB_TAG_NEED_EXPAND) to mark expanding. Flags get cleared after expanding.
 *
 * \param fdhandle: usually filedata, or own handle.
 * \param mainvar: the Main database to expand.
 */
void BLO_expand_main(void *fdhandle, Main *mainvar)
{
  ListBase *lbarray[MAX_LIBARRAY];
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

          switch (GS(id->name)) {
            case ID_OB:
              expand_object(&expander, (Object *)id);
              break;
            case ID_SCE:
              expand_scene(&expander, (Scene *)id);
              break;
            case ID_GR:
              expand_collection(&expander, (Collection *)id);
              break;
            case ID_IP:
              expand_ipo(&expander, (Ipo *)id); /* XXX deprecated - old animation system */
              break;
            case ID_PA:
              expand_particlesettings(&expander, (ParticleSettings *)id);
              break;
            case ID_WS:
              expand_workspace(&expander, (WorkSpace *)id);
              break;
            default:
              break;
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

static bool object_in_any_scene(Main *bmain, Object *ob)
{
  LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
    if (BKE_scene_object_find(sce, ob)) {
      return true;
    }
  }

  return false;
}

static bool object_in_any_collection(Main *bmain, Object *ob)
{
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (BKE_collection_has_object(collection, ob)) {
      return true;
    }
  }

  return false;
}

static void add_loose_objects_to_scene(Main *mainvar,
                                       Main *bmain,
                                       Scene *scene,
                                       ViewLayer *view_layer,
                                       const View3D *v3d,
                                       Library *lib,
                                       const short flag)
{
  Collection *active_collection = NULL;
  const bool do_append = (flag & FILE_LINK) == 0;

  BLI_assert(scene);

  /* Give all objects which are LIB_TAG_INDIRECT a base,
   * or for a collection when *lib has been set. */
  LISTBASE_FOREACH (Object *, ob, &mainvar->objects) {
    bool do_it = (ob->id.tag & LIB_TAG_DOIT) != 0;
    if (do_it || ((ob->id.tag & LIB_TAG_INDIRECT) && (ob->id.tag & LIB_TAG_PRE_EXISTING) == 0)) {
      if (do_append) {
        if (ob->id.us == 0) {
          do_it = true;
        }
        else if ((ob->id.lib == lib) && (object_in_any_collection(bmain, ob) == 0)) {
          /* When appending, make sure any indirectly loaded object gets a base,
           * when they are not part of any collection yet. */
          do_it = true;
        }
      }

      if (do_it) {
        /* Find or add collection as needed. */
        if (active_collection == NULL) {
          if (flag & FILE_ACTIVE_COLLECTION) {
            LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
            active_collection = lc->collection;
          }
          else {
            active_collection = BKE_collection_add(bmain, scene->master_collection, NULL);
          }
        }

        CLAMP_MIN(ob->id.us, 0);
        ob->mode = OB_MODE_OBJECT;

        BKE_collection_object_add(bmain, active_collection, ob);
        Base *base = BKE_view_layer_base_find(view_layer, ob);

        if (v3d != NULL) {
          base->local_view_bits |= v3d->local_view_uuid;
        }

        if ((flag & FILE_AUTOSELECT) && (base->flag & BASE_SELECTABLE)) {
          /* Do NOT make base active here! screws up GUI stuff,
           * if you want it do it at the editor level. */
          base->flag |= BASE_SELECTED;
        }

        BKE_scene_object_base_flag_sync_from_base(base);

        ob->id.tag &= ~LIB_TAG_INDIRECT;
        ob->id.flag &= ~LIB_INDIRECT_WEAK_LINK;
        ob->id.tag |= LIB_TAG_EXTERN;
      }
    }
  }
}

static void add_loose_object_data_to_scene(Main *mainvar,
                                           Main *bmain,
                                           Scene *scene,
                                           ViewLayer *view_layer,
                                           const View3D *v3d,
                                           const short flag)
{
  if ((flag & FILE_OBDATA_INSTANCE) == 0) {
    return;
  }

  Collection *active_collection = scene->master_collection;
  if (flag & FILE_ACTIVE_COLLECTION) {
    LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
    active_collection = lc->collection;
  }

  /* Loop over all ID types, instancing object-data for ID types that have support for it. */
  ListBase *lbarray[MAX_LIBARRAY];
  int i = set_listbasepointers(mainvar, lbarray);
  while (i--) {
    const short idcode = BKE_idtype_idcode_from_index(i);
    if (!OB_DATA_SUPPORT_ID(idcode)) {
      continue;
    }

    LISTBASE_FOREACH (ID *, id, lbarray[i]) {
      if (id->tag & LIB_TAG_DOIT) {
        const int type = BKE_object_obdata_to_type(id);
        BLI_assert(type != -1);
        Object *ob = BKE_object_add_only_object(bmain, type, id->name + 2);
        ob->data = id;
        id_us_plus(id);
        BKE_object_materials_test(bmain, ob, ob->data);

        BKE_collection_object_add(bmain, active_collection, ob);
        Base *base = BKE_view_layer_base_find(view_layer, ob);

        if (v3d != NULL) {
          base->local_view_bits |= v3d->local_view_uuid;
        }

        if ((flag & FILE_AUTOSELECT) && (base->flag & BASE_SELECTABLE)) {
          /* Do NOT make base active here! screws up GUI stuff,
           * if you want it do it at the editor level. */
          base->flag |= BASE_SELECTED;
        }

        BKE_scene_object_base_flag_sync_from_base(base);

        copy_v3_v3(ob->loc, scene->cursor.location);
      }
    }
  }
}

static void add_collections_to_scene(Main *mainvar,
                                     Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     const View3D *v3d,
                                     Library *lib,
                                     const short flag)
{
  Collection *active_collection = scene->master_collection;
  if (flag & FILE_ACTIVE_COLLECTION) {
    LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
    active_collection = lc->collection;
  }

  /* Give all objects which are tagged a base. */
  LISTBASE_FOREACH (Collection *, collection, &mainvar->collections) {
    if ((flag & FILE_COLLECTION_INSTANCE) && (collection->id.tag & LIB_TAG_DOIT)) {
      /* Any indirect collection should not have been tagged. */
      BLI_assert((collection->id.tag & LIB_TAG_INDIRECT) == 0);

      /* BKE_object_add(...) messes with the selection. */
      Object *ob = BKE_object_add_only_object(bmain, OB_EMPTY, collection->id.name + 2);
      ob->type = OB_EMPTY;
      ob->empty_drawsize = U.collection_instance_empty_size;

      BKE_collection_object_add(bmain, active_collection, ob);
      Base *base = BKE_view_layer_base_find(view_layer, ob);

      if (v3d != NULL) {
        base->local_view_bits |= v3d->local_view_uuid;
      }

      if ((flag & FILE_AUTOSELECT) && (base->flag & BASE_SELECTABLE)) {
        base->flag |= BASE_SELECTED;
      }

      BKE_scene_object_base_flag_sync_from_base(base);
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

      if (flag & FILE_AUTOSELECT) {
        view_layer->basact = base;
      }

      /* Assign the collection. */
      ob->instance_collection = collection;
      id_us_plus(&collection->id);
      ob->transflag |= OB_DUPLICOLLECTION;
      copy_v3_v3(ob->loc, scene->cursor.location);
    }
    /* We do not want to force instantiation of indirectly linked collections,
     * not even when appending. Users can now easily instantiate collections (and their objects)
     * as needed by themselves. See T67032. */
    else if ((collection->id.tag & LIB_TAG_INDIRECT) == 0) {
      bool do_add_collection = (collection->id.tag & LIB_TAG_DOIT) != 0;
      if (!do_add_collection) {
        /* We need to check that objects in that collections are already instantiated in a scene.
         * Otherwise, it's better to add the collection to the scene's active collection, than to
         * instantiate its objects in active scene's collection directly. See T61141.
         * Note that we only check object directly into that collection,
         * not recursively into its children.
         */
        LISTBASE_FOREACH (CollectionObject *, coll_ob, &collection->gobject) {
          Object *ob = coll_ob->ob;
          if ((ob->id.tag & (LIB_TAG_PRE_EXISTING | LIB_TAG_DOIT | LIB_TAG_INDIRECT)) == 0 &&
              (ob->id.lib == lib) && (object_in_any_scene(bmain, ob) == 0)) {
            do_add_collection = true;
            break;
          }
        }
      }
      if (do_add_collection) {
        /* Add collection as child of active collection. */
        BKE_collection_child_add(bmain, active_collection, collection);

        if (flag & FILE_AUTOSELECT) {
          LISTBASE_FOREACH (CollectionObject *, coll_ob, &collection->gobject) {
            Object *ob = coll_ob->ob;
            Base *base = BKE_view_layer_base_find(view_layer, ob);
            if (base) {
              base->flag |= BASE_SELECTED;
              BKE_scene_object_base_flag_sync_from_base(base);
            }
          }
        }

        /* Those are kept for safety and consistency, but should not be needed anymore? */
        collection->id.tag &= ~LIB_TAG_INDIRECT;
        collection->id.flag &= ~LIB_INDIRECT_WEAK_LINK;
        collection->id.tag |= LIB_TAG_EXTERN;
      }
    }
  }
}

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
      const int tag = force_indirect ? LIB_TAG_INDIRECT : LIB_TAG_EXTERN;
      read_libblock(fd, mainl, bhead, tag | LIB_TAG_NEED_EXPAND, false, &id);

      if (id) {
        /* sort by name in list */
        ListBase *lb = which_libbase(mainl, idcode);
        id_sort_by_name(lb, id, NULL);
      }
    }
    else {
      /* already linked */
      if (G.debug) {
        printf("append: already linked\n");
      }
      oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);
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

  /* Tag as loose object (or data associated with objects)
   * needing to be instantiated in #LibraryLink_Params.scene. */
  if ((id != NULL) && (flag & BLO_LIBLINK_NEEDS_ID_TAG_DOIT)) {
    if (library_link_idcode_needs_tag_check(idcode, flag)) {
      id->tag |= LIB_TAG_DOIT;
    }
  }

  return id;
}

/**
 * Simple reader for copy/paste buffers.
 */
int BLO_library_link_copypaste(Main *mainl, BlendHandle *bh, const uint64_t id_types_mask)
{
  FileData *fd = (FileData *)(bh);
  BHead *bhead;
  int num_directly_linked = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    ID *id = NULL;

    if (bhead->code == ENDB) {
      break;
    }

    if (BKE_idtype_idcode_is_valid(bhead->code) && BKE_idtype_idcode_is_linkable(bhead->code) &&
        (id_types_mask == 0 ||
         (BKE_idtype_idcode_to_idfilter((short)bhead->code) & id_types_mask) != 0)) {
      read_libblock(fd, mainl, bhead, LIB_TAG_NEED_EXPAND | LIB_TAG_INDIRECT, false, &id);
      num_directly_linked++;
    }

    if (id) {
      /* sort by name in list */
      ListBase *lb = which_libbase(mainl, GS(id->name));
      id_sort_by_name(lb, id, NULL);

      if (bhead->code == ID_OB) {
        /* Instead of instancing Base's directly, postpone until after collections are loaded
         * otherwise the base's flag is set incorrectly when collections are used */
        Object *ob = (Object *)id;
        ob->mode = OB_MODE_OBJECT;
        /* ensure add_loose_objects_to_scene runs on this object */
        BLI_assert(id->us == 0);
      }
    }
  }

  return num_directly_linked;
}

/**
 * Link a named data-block from an external blend file.
 *
 * \param mainl: The main database to link from (not the active one).
 * \param bh: The blender file handle.
 * \param idcode: The kind of data-block to link.
 * \param name: The name of the data-block (without the 2 char ID prefix).
 * \return the linked ID when found.
 */
ID *BLO_library_link_named_part(Main *mainl,
                                BlendHandle **bh,
                                const short idcode,
                                const char *name,
                                const struct LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);
  return link_named_part(mainl, fd, idcode, name, params->flag);
}

/* common routine to append/link something from a library */

/**
 * Checks if the \a idcode needs to be tagged with #LIB_TAG_DOIT when linking/appending.
 */
static bool library_link_idcode_needs_tag_check(const short idcode, const int flag)
{
  if (flag & BLO_LIBLINK_NEEDS_ID_TAG_DOIT) {
    /* Always true because of #add_loose_objects_to_scene & #add_collections_to_scene. */
    if (ELEM(idcode, ID_OB, ID_GR)) {
      return true;
    }
    if (flag & FILE_OBDATA_INSTANCE) {
      if (OB_DATA_SUPPORT_ID(idcode)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Clears #LIB_TAG_DOIT based on the result of #library_link_idcode_needs_tag_check.
 */
static void library_link_clear_tag(Main *mainvar, const int flag)
{
  for (int i = 0; i < MAX_LIBARRAY; i++) {
    const short idcode = BKE_idtype_idcode_from_index(i);
    BLI_assert(idcode != -1);
    if (library_link_idcode_needs_tag_check(idcode, flag)) {
      BKE_main_id_tag_idcode(mainvar, idcode, LIB_TAG_DOIT, false);
    }
  }
}

static Main *library_link_begin(Main *mainvar, FileData **fd, const char *filepath, const int flag)
{
  Main *mainl;

  (*fd)->mainlist = MEM_callocN(sizeof(ListBase), "FileData.mainlist");

  if (flag & BLO_LIBLINK_NEEDS_ID_TAG_DOIT) {
    /* Clear for objects and collections instantiating tag. */
    library_link_clear_tag(mainvar, flag);
  }

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
                                  const int flag)
{
  memset(params, 0, sizeof(*params));
  params->bmain = bmain;
  params->flag = flag;
}

void BLO_library_link_params_init_with_context(struct LibraryLink_Params *params,
                                               struct Main *bmain,
                                               const int flag,
                                               /* Context arguments. */
                                               struct Scene *scene,
                                               struct ViewLayer *view_layer,
                                               const struct View3D *v3d)
{
  BLO_library_link_params_init(params, bmain, flag);
  if (scene != NULL) {
    /* Tagging is needed for instancing. */
    params->flag |= BLO_LIBLINK_NEEDS_ID_TAG_DOIT;

    params->context.scene = scene;
    params->context.view_layer = view_layer;
    params->context.v3d = v3d;
  }
}

/**
 * Initialize the #BlendHandle for linking library data.
 *
 * \param bh: A blender file handle as returned by
 * #BLO_blendhandle_from_file or #BLO_blendhandle_from_memory.
 * \param filepath: Used for relative linking, copied to the `lib->filepath`.
 * \param params: Settings for linking that don't change from beginning to end of linking.
 * \return the library #Main, to be passed to #BLO_library_link_named_part as \a mainl.
 */
Main *BLO_library_link_begin(BlendHandle **bh,
                             const char *filepath,
                             const struct LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);
  return library_link_begin(params->bmain, &fd, filepath, params->flag);
}

static void split_main_newid(Main *mainptr, Main *main_newid)
{
  /* We only copy the necessary subset of data in this temp main. */
  main_newid->versionfile = mainptr->versionfile;
  main_newid->subversionfile = mainptr->subversionfile;
  BLI_strncpy(main_newid->name, mainptr->name, sizeof(main_newid->name));
  main_newid->curlib = mainptr->curlib;

  ListBase *lbarray[MAX_LIBARRAY];
  ListBase *lbarray_newid[MAX_LIBARRAY];
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

/**
 * \param scene: The scene in which to instantiate objects/collections
 * (if NULL, no instantiation is done).
 * \param v3d: The active 3D viewport.
 * (only to define active layers for instantiated objects & collections, can be NULL).
 */
static void library_link_end(Main *mainl,
                             FileData **fd,
                             Main *bmain,
                             const int flag,
                             Scene *scene,
                             ViewLayer *view_layer,
                             const View3D *v3d)
{
  Main *mainvar;
  Library *curlib;

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
  mainl = NULL; /* blo_join_main free's mainl, cant use anymore */

  lib_link_all(*fd, mainvar);

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

    do_versions_after_linking(main_newid, (*fd)->reports);

    add_main_to_main(mainvar, main_newid);
  }

  BKE_main_free(main_newid);
  blo_join_main((*fd)->mainlist);
  mainvar = (*fd)->mainlist->first;
  MEM_freeN((*fd)->mainlist);

  /* This does not take into account old, deprecated data, so we also have to do it after
   * `do_versions_after_linking()`. */
  BKE_main_id_refcount_recompute(mainvar, false);

  /* After all data has been read and versioned, uses LIB_TAG_NEW. */
  ntreeUpdateAllNew(mainvar);

  placeholders_ensure_valid(mainvar);

  BKE_main_id_tag_all(mainvar, LIB_TAG_NEW, false);

  /* Make all relative paths, relative to the open blend file. */
  fix_relpaths_library(BKE_main_blendfile_path(mainvar), mainvar);

  /* Give a base to loose objects and collections.
   * Only directly linked objects & collections are instantiated by
   * #BLO_library_link_named_part & co,
   * here we handle indirect ones and other possible edge-cases. */
  if (flag & BLO_LIBLINK_NEEDS_ID_TAG_DOIT) {
    /* Should always be true. */
    if (scene != NULL) {
      add_collections_to_scene(mainvar, bmain, scene, view_layer, v3d, curlib, flag);
      add_loose_objects_to_scene(mainvar, bmain, scene, view_layer, v3d, curlib, flag);
      add_loose_object_data_to_scene(mainvar, bmain, scene, view_layer, v3d, flag);
    }

    /* Clear objects and collections instantiating tag. */
    library_link_clear_tag(mainvar, flag);
  }

  /* patch to prevent switch_endian happens twice */
  if ((*fd)->flags & FD_FLAGS_SWITCH_ENDIAN) {
    blo_filedata_free(*fd);
    *fd = NULL;
  }
}

/**
 * Finalize linking from a given .blend file (library).
 * Optionally instance the indirect object/collection in the scene when the flags are set.
 * \note Do not use \a bh after calling this function, it may frees it.
 *
 * \param mainl: The main database to link from (not the active one).
 * \param bh: The blender file handle (WARNING! may be freed by this function!).
 * \param params: Settings for linking that don't change from beginning to end of linking.
 */
void BLO_library_link_end(Main *mainl, BlendHandle **bh, const struct LibraryLink_Params *params)
{
  FileData *fd = (FileData *)(*bh);
  library_link_end(mainl,
                   &fd,
                   params->bmain,
                   params->flag,
                   params->context.scene,
                   params->context.view_layer,
                   params->context.v3d);
  *bh = (BlendHandle *)fd;
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
  ListBase *lbarray[MAX_LIBARRAY];
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
    ReportList *reports, FileData *fd, Main *mainvar, ID *id, ID **r_id)
{
  BHead *bhead = NULL;
  const bool is_valid = BKE_idtype_idcode_is_linkable(GS(id->name)) ||
                        ((id->tag & LIB_TAG_EXTERN) == 0);

  if (fd) {
    bhead = find_bhead_from_idname(fd, id->name);
  }

  if (!is_valid) {
    blo_reportf_wrap(reports,
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
    blo_reportf_wrap(reports,
                     RPT_WARNING,
                     TIP_("LIB: %s: '%s' missing from '%s', parent '%s'"),
                     BKE_idtype_idcode_to_name(GS(id->name)),
                     id->name + 2,
                     mainvar->curlib->filepath_abs,
                     library_parent_filepath(mainvar->curlib));

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

  ListBase *lbarray[MAX_LIBARRAY];
  int a = set_listbasepointers(mainvar, lbarray);

  while (a--) {
    ID *id = lbarray[a]->first;
    ListBase pending_free_ids = {NULL};

    while (id) {
      ID *id_next = id->next;
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && !(id->flag & LIB_INDIRECT_WEAK_LINK)) {
        BLI_remlink(lbarray[a], id);

        /* When playing with lib renaming and such, you may end with cases where
         * you have more than one linked ID of the same data-block from same
         * library. This is absolutely horrible, hence we use a ghash to ensure
         * we go back to a single linked data when loading the file. */
        ID **realid = NULL;
        if (!BLI_ghash_ensure_p(loaded_ids, id->name, (void ***)&realid)) {
          read_library_linked_id(basefd->reports, fd, mainvar, id, realid);
        }

        /* realid shall never be NULL - unless some source file/lib is broken
         * (known case: some directly linked shapekey from a missing lib...). */
        /* BLI_assert(*realid != NULL); */

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
  ListBase *lbarray[MAX_LIBARRAY];
  int a = set_listbasepointers(mainvar, lbarray);

  while (a--) {
    ID *id = lbarray[a]->first;

    while (id) {
      ID *id_next = id->next;
      if ((id->tag & LIB_TAG_ID_LINK_PLACEHOLDER) && (id->flag & LIB_INDIRECT_WEAK_LINK)) {
        /* printf("Dropping weak link to %s\n", id->name); */
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

    blo_reportf_wrap(basefd->reports,
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
    blo_reportf_wrap(basefd->reports,
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
    blo_reportf_wrap(
        basefd->reports, RPT_WARNING, TIP_("Cannot find lib '%s'"), mainptr->curlib->filepath_abs);
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
#if 0
        printf("Reading linked data-blocks from %s (%s)\n",
          mainptr->curlib->id.name,
          mainptr->curlib->filepath);
#endif

        /* Open file if it has not been done yet. */
        FileData *fd = read_library_file_data(basefd, mainlist, mainl, mainptr);

        if (fd) {
          do_it = true;
        }

        /* Read linked data-locks for each link placeholder, and replace
         * the placeholder with the real data-lock. */
        read_library_linked_ids(basefd, fd, mainlist, mainptr);

        /* Test if linked data-locks need to read further linked data-locks
         * and create link placeholders for them. */
        BLO_expand_main(fd, mainptr);
      }
    }
  }

  Main *main_newid = BKE_main_new();
  for (Main *mainptr = mainl->next; mainptr; mainptr = mainptr->next) {
    /* Drop weak links for which no data-block was found. */
    read_library_clear_weak_links(basefd, mainlist, mainptr);

    /* Do versioning for newly added linked data-locks. If no data-locks
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

    /* Note: No need to call #do_versions_after_linking() or #BKE_main_id_refcount_recompute()
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

/**
 * Updates all ->prev and ->next pointers of the list elements.
 * Updates the list->first and list->last pointers.
 * When not NULL, calls the callback on every element.
 */
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
  return reader->fd->memfile != NULL;
}

bool BLO_read_lib_is_undo(BlendLibReader *reader)
{
  return reader->fd->memfile != NULL;
}

void BLO_expand_id(BlendExpander *expander, ID *id)
{
  expand_doit(expander->fd, expander->main, id);
}

/** \} */
