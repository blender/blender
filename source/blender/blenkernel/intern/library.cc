/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains code specific to the `Library` ID type.
 */

#include <optional>

#include "CLG_log.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "BKE_bpath.hh"
#include "BKE_id_hash.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_main_namemap.hh"
#include "BKE_node.hh"
#include "BKE_packedFile.hh"
#include "BKE_report.hh"

struct BlendDataReader;

static CLG_LogRef LOG = {"lib.library"};

using namespace blender::bke;
using namespace blender::bke::library;

static void library_runtime_reset(Library *lib)
{
  BKE_main_namemap_destroy(&lib->runtime->name_map);
}

static void library_init_data(ID *id)
{
  Library *library = reinterpret_cast<Library *>(id);
  library->runtime = MEM_new<LibraryRuntime>(__func__);
}

static void library_free_data(ID *id)
{
  Library *library = (Library *)id;
  library_runtime_reset(library);
  MEM_delete(library->runtime);
  if (library->packedfile) {
    BKE_packedfile_free(library->packedfile);
  }
}

static void library_copy_data(Main *bmain,
                              std::optional<Library *> owner_library,
                              ID *id_dst,
                              const ID *id_src,
                              int /*flag*/)
{
  /* Libraries are always local IDs. */
  BLI_assert(!owner_library || *owner_library == nullptr);
  UNUSED_VARS_NDEBUG(bmain, owner_library);

  const Library *library_src = reinterpret_cast<const Library *>(id_src);

  /* Libraries are copyable now, but there should still be only one library ID for each linked
   * blendfile (based on absolute filepath). */
  BLI_assert(!bmain ||
             !search_filepath_abs(&bmain->libraries, library_src->runtime->filepath_abs));

  Library *library_dst = reinterpret_cast<Library *>(id_dst);
  if (library_src->packedfile) {
    library_dst->packedfile = BKE_packedfile_duplicate(library_src->packedfile);
  }

  /* Only explicitely copy a sub-set of the runtime data. */
  library_dst->runtime = MEM_new<LibraryRuntime>(__func__);
  BLI_strncpy(library_dst->runtime->filepath_abs,
              library_src->runtime->filepath_abs,
              sizeof(library_dst->runtime->filepath_abs));
  library_dst->runtime->parent = library_src->runtime->parent;
  library_dst->runtime->tag = library_src->runtime->tag;
  library_dst->runtime->versionfile = library_src->runtime->versionfile;
  library_dst->runtime->subversionfile = library_src->runtime->subversionfile;
  library_dst->runtime->colorspace = library_src->runtime->colorspace;
}

static void library_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Library *lib = (Library *)id;
  const LibraryForeachIDFlag foreach_flag = BKE_lib_query_foreachid_process_flags_get(data);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, lib->runtime->parent, IDWALK_CB_NEVER_SELF);

  if (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
    /* Archive library must have a parent, this can't be nullptr. */
    if (lib->archive_parent_library) {
      BKE_LIB_FOREACHID_PROCESS_ID(
          data, lib->archive_parent_library, IDWALK_CB_NEVER_SELF | IDWALK_CB_NEVER_NULL);
    }

    /* Archive libraries should never 'own' other archives. */
    BLI_assert(lib->runtime->archived_libraries.is_empty());
    if (foreach_flag & IDWALK_DO_INTERNAL_RUNTIME_POINTERS) {
      for (Library *&lib_p : lib->runtime->archived_libraries) {
        BKE_LIB_FOREACHID_PROCESS_ID(
            data, lib_p, IDWALK_CB_NEVER_SELF | IDWALK_CB_INTERNAL | IDWALK_CB_LOOPBACK);
      }
    }
  }
  else {
    /* Regular libraries should never have an archive parent. */
    BLI_assert(!lib->archive_parent_library);
    BKE_LIB_FOREACHID_PROCESS_ID(data, lib->archive_parent_library, IDWALK_CB_NEVER_SELF);

    if (foreach_flag & IDWALK_DO_INTERNAL_RUNTIME_POINTERS) {
      for (Library *&lib_p : lib->runtime->archived_libraries) {
        BKE_LIB_FOREACHID_PROCESS_ID(
            data, lib_p, IDWALK_CB_NEVER_SELF | IDWALK_CB_INTERNAL | IDWALK_CB_LOOPBACK);
      }
    }
  }
}

static void library_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  Library *lib = (Library *)id;

  /* FIXME: Find if we should respect #BKE_BPATH_FOREACH_PATH_SKIP_PACKED here, and if not, explain
   * why. */
  if (lib->packedfile !=
      nullptr /*&& (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0 */)
  {
    return;
  }

  if (BKE_bpath_foreach_path_fixed_process(bpath_data, lib->filepath, sizeof(lib->filepath))) {
    BKE_library_filepath_set(bpath_data->bmain, lib, lib->filepath);
  }
}

static void library_blend_write_data(BlendWriter *writer, ID *id, const void *id_address)
{
  Library *library = reinterpret_cast<Library *>(id);
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clear runtime data. */
  library->runtime = nullptr;

  BLO_write_id_struct(writer, Library, id_address, id);
  BKE_id_blend_write(writer, id);

  /* Write packed file if necessary. */
  if (library->packedfile) {
    BKE_packedfile_blend_write(writer, library->packedfile);
    if (!is_undo) {
      CLOG_DEBUG(&LOG, "Write packed .blend: %s", library->filepath);
    }
  }
}

static void library_blend_read_data(BlendDataReader * /*reader*/, ID *id)
{
  Library *lib = reinterpret_cast<Library *>(id);
  lib->runtime = MEM_new<LibraryRuntime>(__func__);
}

static void library_blend_read_after_liblink(BlendLibReader * /*reader*/, ID *id)
{
  Library *lib = reinterpret_cast<Library *>(id);
  if (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
    BLI_assert(lib->archive_parent_library);
    lib->archive_parent_library->runtime->archived_libraries.append(lib);
  }
}

IDTypeInfo IDType_ID_LI = {
    /*id_code*/ Library::id_type,
    /*id_filter*/ FILTER_ID_LI,
    /*dependencies_id_types*/ FILTER_ID_LI,
    /*main_listbase_index*/ INDEX_ID_LI,
    /*struct_size*/ sizeof(Library),
    /*name*/ "Library",
    /*name_plural*/ N_("libraries"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_LIBRARY,
    /*flags*/ IDTYPE_FLAGS_NO_LIBLINKING | IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_NEVER_UNUSED,
    /*asset_type_info*/ nullptr,

    /*init_data*/ library_init_data,
    /*copy_data*/ library_copy_data,
    /*free_data*/ library_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ library_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ library_foreach_path,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ library_blend_write_data,
    /*blend_read_data*/ library_blend_read_data,
    /*blend_read_after_liblink*/ library_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

void BKE_library_filepath_set(Main *bmain, Library *lib, const char *filepath)
{
  /* in some cases this is used to update the absolute path from the
   * relative */
  if (lib->filepath != filepath) {
    STRNCPY(lib->filepath, filepath);
  }

  STRNCPY(lib->runtime->filepath_abs, filepath);

  /* Not essential but set `filepath_abs` is an absolute copy of value which
   * is more useful if its kept in sync. */
  if (BLI_path_is_rel(lib->runtime->filepath_abs)) {
    /* NOTE(@ideasman42): the file may be unsaved, in this case, setting the
     * `filepath_abs` on an indirectly linked path is not allowed from the
     * outliner, and its not really supported but allow from here for now
     * since making local could cause this to be directly linked.
     */
    /* Never make paths relative to parent lib - reading code (blenloader) always set *all*
     * `lib->filepath` relative to current main, not to their parent for indirectly linked ones. */
    const char *blendfile_path = BKE_main_blendfile_path(bmain);
    BLI_path_abs(lib->runtime->filepath_abs, blendfile_path);
  }
}

static void rebuild_hierarchy_best_parent_find(Main *bmain,
                                               blender::Set<Library *> &directly_used_libs,
                                               Library *lib)
{
  BLI_assert(!directly_used_libs.contains(lib));

  Library *best_parent_lib = nullptr;
  bool do_break = false;
  ListBase *lb;
  ID *id_iter;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id_iter) {
      if (!ID_IS_LINKED(id_iter) || id_iter->lib != lib) {
        continue;
      }
      MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
          BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_iter));
      for (MainIDRelationsEntryItem *item = entry->from_ids; item; item = item->next) {
        ID *from_id = item->id_pointer.from;
        if (!ID_IS_LINKED(from_id)) {
          BLI_assert_unreachable();
          continue;
        }
        Library *from_id_lib = from_id->lib;
        if (from_id_lib == lib) {
          continue;
        }
        if (directly_used_libs.contains(from_id_lib)) {
          /* Found the first best possible candidate, no need to search further. */
          BLI_assert(best_parent_lib == nullptr || best_parent_lib->runtime->temp_index > 0);
          best_parent_lib = from_id_lib;
          do_break = true;
          break;
        }
        if (!from_id_lib->runtime->parent) {
          rebuild_hierarchy_best_parent_find(bmain, directly_used_libs, from_id_lib);
        }
        if (!best_parent_lib ||
            best_parent_lib->runtime->temp_index > from_id_lib->runtime->temp_index)
        {
          best_parent_lib = from_id_lib;
          if (best_parent_lib->runtime->temp_index == 0) {
            /* Found the first best possible candidate, no need to search further. */
            BLI_assert(directly_used_libs.contains(best_parent_lib));
            do_break = true;
            break;
          }
        }
      }
      if (do_break) {
        break;
      }
    }
    FOREACH_MAIN_LISTBASE_ID_END;
    if (do_break) {
      break;
    }
  }
  FOREACH_MAIN_LISTBASE_END;

  /* NOTE: It may happen that no parent library is found, e.g. if after deleting a directly used
   * library, its indirect dependency is still around, but none of its linked IDs are used by local
   * data. */
  if (best_parent_lib) {
    lib->runtime->parent = best_parent_lib;
    lib->runtime->temp_index = best_parent_lib->runtime->temp_index + 1;
  }
  else {
    lib->runtime->parent = nullptr;
    lib->runtime->temp_index = 0;
    directly_used_libs.add(lib);
  }
}

void BKE_library_main_rebuild_hierarchy(Main *bmain)
{
  BKE_main_relations_create(bmain, 0);

  /* Reset all values, they may have been set to irrelevant values by other processes (like the
   * liboverride handling e.g., see #lib_override_libraries_index_define). */
  LISTBASE_FOREACH (Library *, lib_iter, &bmain->libraries) {
    lib_iter->runtime->temp_index = 0;
  }

  /* Find all libraries with directly linked IDs (i.e. IDs used by local data). */
  blender::Set<Library *> directly_used_libs;
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (!ID_IS_LINKED(id_iter)) {
      continue;
    }
    id_iter->lib->runtime->temp_index = 0;
    if (directly_used_libs.contains(id_iter->lib)) {
      continue;
    }
    MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
        BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_iter));
    for (MainIDRelationsEntryItem *item = entry->from_ids; item; item = item->next) {
      if (!ID_IS_LINKED(item->id_pointer.from)) {
        directly_used_libs.add(id_iter->lib);
        id_iter->lib->runtime->parent = nullptr;
        break;
      }
    }
  }
  FOREACH_MAIN_ID_END;

  LISTBASE_FOREACH (Library *, lib_iter, &bmain->libraries) {
    /* A directly used library. */
    if (directly_used_libs.contains(lib_iter)) {
      BLI_assert(lib_iter->runtime->temp_index == 0);
      continue;
    }

    /* Assume existing parent is still valid, since it was not cleared in previous loop above.
     * Just compute 'hierarchy value' in temp index, if needed. */
    if (lib_iter->runtime->parent) {
      if (lib_iter->runtime->temp_index > 0) {
        continue;
      }
      blender::Vector<Library *> parent_libraries;
      for (Library *parent_lib_iter = lib_iter;
           parent_lib_iter && parent_lib_iter->runtime->temp_index == 0;
           parent_lib_iter = parent_lib_iter->runtime->parent)
      {
        parent_libraries.append(parent_lib_iter);
      }
      int parent_temp_index = parent_libraries.last()->runtime->temp_index +
                              int(parent_libraries.size()) - 1;
      for (Library *parent_lib_iter : parent_libraries) {
        BLI_assert(parent_lib_iter != parent_libraries.last() ||
                   parent_lib_iter->runtime->temp_index == parent_temp_index);
        parent_lib_iter->runtime->temp_index = parent_temp_index--;
      }
      continue;
    }

    /* Otherwise, it's an indirectly used library with no known parent, another loop is needed to
     * ensure all known hierarchy has valid indices when trying to find the best valid parent
     * library. */
  }

  /* For all libraries known to be indirect, but without a known parent, find a best valid parent
   * (i.e. a 'most directly used' library). */
  LISTBASE_FOREACH (Library *, lib_iter, &bmain->libraries) {
    /* A directly used library. */
    if (directly_used_libs.contains(lib_iter)) {
      BLI_assert(lib_iter->runtime->temp_index == 0);
      continue;
    }

    if (lib_iter->runtime->parent) {
      BLI_assert(lib_iter->runtime->temp_index > 0);
    }
    else {
      BLI_assert(lib_iter->runtime->temp_index == 0);
      rebuild_hierarchy_best_parent_find(bmain, directly_used_libs, lib_iter);
    }
  }

  BKE_main_relations_free(bmain);
}

Library *blender::bke::library::search_filepath_abs(ListBase *libraries,
                                                    blender::StringRef filepath_abs)
{
  LISTBASE_FOREACH (Library *, lib_iter, libraries) {
    if (lib_iter->flag & LIBRARY_FLAG_IS_ARCHIVE) {
      /* Skip archive libraries because there may be multiple of those for the same path and there
       * should also be a non-archive one. */
      continue;
    }
    if (filepath_abs == lib_iter->runtime->filepath_abs) {
      return lib_iter;
    }
  }
  return nullptr;
}

/**
 * Add a new 'archive' copy of the given reference library. It is used to store linked packed IDs.
 */
static Library *add_archive_library(Main &bmain, Library &reference_library)
{
  BLI_assert((reference_library.flag & LIBRARY_FLAG_IS_ARCHIVE) == 0);
  /* Cannot copy libraries using generic ID copying functions, so create the copy manually. */
  Library *archive_library = static_cast<Library *>(
      BKE_id_new(&bmain, ID_LI, BKE_id_name(reference_library.id)));

  /* Like in #direct_link_library. */
  id_us_ensure_real(&archive_library->id);

  archive_library->archive_parent_library = &reference_library;
  constexpr uint16_t copy_flag = ~LIBRARY_FLAG_IS_ARCHIVE;
  archive_library->flag = (reference_library.flag & copy_flag) | LIBRARY_FLAG_IS_ARCHIVE;
  BKE_library_filepath_set(&bmain, archive_library, reference_library.filepath);

  archive_library->runtime->parent = reference_library.runtime->parent;
  /* Only copy a subset of the reference library tags. E.g. an archive library should never be
   * considered as writable, so never copy #LIBRARY_ASSET_FILE_WRITABLE. This may need further
   * tweaking still. */
  constexpr uint16_t copy_tag = (LIBRARY_TAG_RESYNC_REQUIRED | LIBRARY_ASSET_EDITABLE |
                                 LIBRARY_IS_ASSET_EDIT_FILE);
  archive_library->runtime->tag = reference_library.runtime->tag & copy_tag;
  /* By definition, the file version of an archive library containing only packed linked data is
   * the same as the one of its Main container. */
  archive_library->runtime->versionfile = bmain.versionfile;
  archive_library->runtime->subversionfile = bmain.subversionfile;

  reference_library.runtime->archived_libraries.append(archive_library);

  return archive_library;
}

Library *blender::bke::library::ensure_archive_library(
    Main &bmain, ID &id, Library &reference_library, const IDHash &id_deep_hash, bool &is_new)
{
  BLI_assert(ID_IS_LINKED(&id));
  BLI_assert((reference_library.flag & LIBRARY_FLAG_IS_ARCHIVE) == 0);

  Library *archive_library = nullptr;
  for (Library *lib_iter : reference_library.runtime->archived_libraries) {
    BLI_assert((lib_iter->flag & LIBRARY_FLAG_IS_ARCHIVE) != 0);
    BLI_assert(lib_iter->archive_parent_library != nullptr);
    BLI_assert(lib_iter->archive_parent_library == &reference_library);
    /* Check if current archive library already contains an ID of same type and name. */
    if (BKE_main_namemap_contain_name(bmain, lib_iter, GS(id.name), BKE_id_name(id))) {
#ifndef NDEBUG
      ID *packed_id = BKE_libblock_find_name_and_library(
          &bmain, GS(id.name), BKE_id_name(id), BKE_id_name(lib_iter->id));
      BLI_assert_msg(
          packed_id && packed_id->deep_hash != id_deep_hash,
          "An already packed ID with same deep hash as the one to be packed, should have already "
          "be found and used (deduplication) before reaching this code-path");
#endif
      UNUSED_VARS_NDEBUG(id_deep_hash);
      continue;
    }
    archive_library = lib_iter;
    break;
  }
  if (!archive_library) {
    archive_library = add_archive_library(bmain, reference_library);
    is_new = true;
  }
  else {
    is_new = false;
  }
  BLI_assert(reference_library.runtime->archived_libraries.contains(archive_library));
  return archive_library;
}

static void pack_linked_id(Main &bmain,
                           ID *linked_id,
                           const id_hash::ValidDeepHashes &deep_hashes,
                           blender::Map<IDHash, ID *> &already_packed_ids,
                           blender::VectorSet<ID *> &ids_to_remap,
                           blender::bke::id::IDRemapper &id_remapper)
{
  BLI_assert(linked_id->newid == nullptr);

  const IDHash linked_id_deep_hash = deep_hashes.hashes.lookup(linked_id);
  ID *packed_id = already_packed_ids.lookup_default(linked_id_deep_hash, nullptr);

  if (packed_id) {
    /* Exact same ID (and all of its dependencies) have already been linked and packed before,
     * re-use these packed data. */

    auto existing_id_process = [&deep_hashes, &id_remapper](ID *linked_id, ID *packed_id) {
      BLI_assert(packed_id);
      BLI_assert(ID_IS_PACKED(packed_id));
      /* Note: linked_id and packed_id may have the same deep hash while still coming from
       * different original libraries. This easily happens copying an asset file such that each
       * asset exists twice. */
      BLI_assert(packed_id->deep_hash == deep_hashes.hashes.lookup(linked_id));
      UNUSED_VARS_NDEBUG(deep_hashes);

      id_remapper.add(linked_id, packed_id);
      linked_id->newid = packed_id;
      /* No need to remap this packed ID - otherwise there would be something very wrong in
       * packed IDs state. */
    };

    existing_id_process(linked_id, packed_id);

    /* Handle 'fake-embedded' ShapeKeys IDs. */
    Key *linked_key = BKE_key_from_id(linked_id);
    if (linked_key) {
      Key *packed_key = BKE_key_from_id(packed_id);
      BLI_assert(packed_key);
      existing_id_process(&linked_key->id, &packed_key->id);
    }
  }
  else {
    /* This exact version of the ID and its dependencies have not been packed before, creates a
     * new copy of it and pack it. */

    /* Find an existing archive Library not containing a 'version' of this ID yet (to prevent names
     * collisions). */
    bool is_new;
    Library *archive_lib = ensure_archive_library(
        bmain, *linked_id, *linked_id->lib, linked_id_deep_hash, is_new);

    auto copied_id_process =
        [&archive_lib, &deep_hashes, &ids_to_remap, &id_remapper, &already_packed_ids](
            ID *linked_id, ID *packed_id) {
          BLI_assert(packed_id);
          BLI_assert(ID_IS_PACKED(packed_id));
          BLI_assert(packed_id->lib == archive_lib);
          UNUSED_VARS_NDEBUG(archive_lib);

          if (GS(packed_id->name) == ID_SCE) {
            /* Like in #scene_blend_read_data. */
            id_us_ensure_real(packed_id);
          }

          packed_id->deep_hash = deep_hashes.hashes.lookup(linked_id);
          id_remapper.add(linked_id, packed_id);
          ids_to_remap.add(packed_id);
          already_packed_ids.add(packed_id->deep_hash, packed_id);
        };

    packed_id = BKE_id_copy_in_lib(&bmain,
                                   archive_lib,
                                   linked_id,
                                   std::nullopt,
                                   nullptr,
                                   LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ID_NEW_SET |
                                       LIB_ID_COPY_NO_ANIMDATA | LIB_ID_COPY_ASSET_METADATA);
    id_us_min(packed_id);
    copied_id_process(linked_id, packed_id);

    /* Handle 'fake-embedded' ShapeKeys IDs. */
    Key *linked_key = BKE_key_from_id(linked_id);
    if (linked_key) {
      Key *embedded_key = BKE_key_from_id(packed_id);
      BLI_assert(embedded_key);
      copied_id_process(&linked_key->id, &embedded_key->id);
    }
  }
}

/**
 * Pack given linked IDs. Low-level code, assumes all given IDs are valid and safe to pack.
 *
 * Will set final packed ID into each ID::newid pointers.
 */
static void pack_linked_ids(Main &bmain, const blender::Set<ID *> &ids_to_pack)
{
  blender::VectorSet<ID *> final_ids_to_pack;
  blender::VectorSet<ID *> ids_to_remap;
  blender::bke::id::IDRemapper id_remapper;

  for (ID *id : ids_to_pack) {
    BLI_assert(ID_IS_LINKED(id));
    if (ID_IS_PACKED(id)) {
      /* Should not happen, but also not critical issue. */
      CLOG_ERROR(&LOG,
                 "Trying to pack an already packed ID '%s' (from '%s')",
                 id->name,
                 id->lib->runtime->filepath_abs);
      /* Already packed. */
      continue;
    }
    final_ids_to_pack.add(id);
  }

  const id_hash::IDHashResult hash_result = id_hash::compute_linked_id_deep_hashes(
      bmain, final_ids_to_pack.as_span());
  if (const auto *errors = std::get_if<id_hash::DeepHashErrors>(&hash_result)) {
    if (!errors->missing_files.is_empty()) {
      CLOG_ERROR(&LOG,
                 "Trying to pack IDs that depend on missing linked libraries: %s",
                 errors->missing_files[0].c_str());
    }
    if (!errors->updated_files.is_empty()) {
      CLOG_ERROR(&LOG,
                 "Trying to pack linked ID that has been modified on disk: %s",
                 errors->updated_files[0].c_str());
    }
    return;
  }
  const auto &deep_hashes = std::get<id_hash::ValidDeepHashes>(hash_result);

  blender::Map<IDHash, ID *> already_packed_ids;
  {
    ID *id;
    FOREACH_MAIN_ID_BEGIN (&bmain, id) {
      if (ID_IS_PACKED(id)) {
        already_packed_ids.add(id->deep_hash, id);
      }
    }
    FOREACH_MAIN_ID_END;
  }

  for (ID *linked_id : final_ids_to_pack) {
    pack_linked_id(bmain, linked_id, deep_hashes, already_packed_ids, ids_to_remap, id_remapper);
  }

  BKE_libblock_relink_multiple(
      &bmain, ids_to_remap.as_span(), ID_REMAP_TYPE_REMAP, id_remapper, 0);
  BKE_main_ensure_invariants(bmain);
}

void blender::bke::library::pack_linked_id_hierarchy(Main &bmain, ID &root_id)
{
  BLI_assert(ID_IS_LINKED(&root_id));
  BLI_assert(!ID_IS_PACKED(&root_id));

  blender::Set<ID *> ids_to_pack;
  ids_to_pack.add(&root_id);
  BKE_library_foreach_ID_link(
      &bmain,
      &root_id,
      [&ids_to_pack](LibraryIDLinkCallbackData *cb_data) -> int {
        if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
          return IDWALK_RET_NOP;
        }
        if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
          return IDWALK_RET_NOP;
        }

        ID *self_id = cb_data->self_id;
        ID *referenced_id = *cb_data->id_pointer;
        if (!referenced_id) {
          return IDWALK_RET_NOP;
        }
        if (!ID_IS_LINKED(referenced_id)) {
          CLOG_ERROR(&LOG, "Linked data-block references non-linked data-block");
          return IDWALK_RET_NOP;
        }
        if (ID_IS_PACKED(referenced_id)) {
          /* A linked ID can use another packed linked ID, as long as it is not from the same
           * library. */
          BLI_assert(referenced_id->lib && referenced_id->lib->archive_parent_library);
          if (referenced_id->lib->archive_parent_library == self_id->lib) {
            CLOG_ERROR(&LOG,
                       "Non-packed data-block references packed data-block from the same library, "
                       "which is not allowed");
          }
          return IDWALK_RET_NOP;
        }
        if (referenced_id->newid && ID_IS_PACKED(referenced_id->newid)) {
          return IDWALK_RET_NOP;
        }
        if (GS(referenced_id->name) == ID_KE) {
          /* Shape keys cannot be directly linked, from linking code PoV they behave as embedded
           * data (i.e. their owning data is responsible to handle them). */
          return IDWALK_RET_NOP;
        }

        ids_to_pack.add(referenced_id);
        return IDWALK_RET_NOP;
      },
      nullptr,
      IDWALK_READONLY | IDWALK_RECURSE);

  pack_linked_ids(bmain, ids_to_pack);
}

void blender::bke::library::main_cleanup_parent_archives(Main &bmain)
{
  LISTBASE_FOREACH (Library *, lib, &bmain.libraries) {
    if (lib->flag & LIBRARY_FLAG_IS_ARCHIVE) {
      BLI_assert(!lib->runtime || lib->runtime->archived_libraries.is_empty());
    }
    else {
      int i_read_curr = 0;
      int i_insert_curr = 0;
      for (; i_read_curr < lib->runtime->archived_libraries.size(); i_read_curr++) {
        if (!lib->runtime->archived_libraries[i_read_curr]) {
          continue;
        }
        if (i_insert_curr < i_read_curr) {
          lib->runtime->archived_libraries[i_insert_curr] =
              lib->runtime->archived_libraries[i_read_curr];
        }
        i_insert_curr++;
      }
      BLI_assert(i_insert_curr <= i_read_curr);
      if (i_insert_curr < i_read_curr) {
        lib->runtime->archived_libraries.resize(i_insert_curr);
      }
    }
  }
}
