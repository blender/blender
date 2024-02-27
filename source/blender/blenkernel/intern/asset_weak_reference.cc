/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory>
#include <utility>

#include "BLI_string.h"

#include "DNA_space_types.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"

#include "BKE_asset.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "BLI_vector.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"

#include "DNA_asset_types.h"

#include "MEM_guardedalloc.h"

using namespace blender;

/* #AssetWeakReference -------------------------------------------- */

AssetWeakReference::AssetWeakReference()
    : asset_library_type(0), asset_library_identifier(nullptr), relative_asset_identifier(nullptr)
{
}

AssetWeakReference::AssetWeakReference(const AssetWeakReference &other)
    : asset_library_type(other.asset_library_type),
      asset_library_identifier(BLI_strdup_null(other.asset_library_identifier)),
      relative_asset_identifier(BLI_strdup_null(other.relative_asset_identifier))
{
}

AssetWeakReference::AssetWeakReference(AssetWeakReference &&other)
    : asset_library_type(other.asset_library_type),
      asset_library_identifier(other.asset_library_identifier),
      relative_asset_identifier(other.relative_asset_identifier)
{
  other.asset_library_type = 0; /* Not a valid type. */
  other.asset_library_identifier = nullptr;
  other.relative_asset_identifier = nullptr;
}

AssetWeakReference::~AssetWeakReference()
{
  MEM_delete(asset_library_identifier);
  MEM_delete(relative_asset_identifier);
}

AssetWeakReference &AssetWeakReference::operator=(const AssetWeakReference &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) AssetWeakReference(other);
  return *this;
}

AssetWeakReference &AssetWeakReference::operator=(AssetWeakReference &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) AssetWeakReference(std::move(other));
  return *this;
}

bool operator==(const AssetWeakReference &a, const AssetWeakReference &b)
{
  if (a.asset_library_type != b.asset_library_type) {
    return false;
  }
  if (StringRef(a.asset_library_identifier) != StringRef(b.asset_library_identifier)) {
    return false;
  }
  if (StringRef(a.relative_asset_identifier) != StringRef(b.relative_asset_identifier)) {
    return false;
  }
  return true;
}

AssetWeakReference AssetWeakReference::make_reference(
    const asset_system::AssetLibrary &library,
    const asset_system::AssetIdentifier &asset_identifier)
{
  AssetWeakReference weak_ref{};

  weak_ref.asset_library_type = library.library_type();
  StringRefNull name = library.name();
  if (!name.is_empty()) {
    weak_ref.asset_library_identifier = BLI_strdupn(name.c_str(), name.size());
  }

  StringRefNull relative_identifier = asset_identifier.library_relative_identifier();
  weak_ref.relative_asset_identifier = BLI_strdupn(relative_identifier.c_str(),
                                                   relative_identifier.size());

  return weak_ref;
}

void BKE_asset_weak_reference_write(BlendWriter *writer, const AssetWeakReference *weak_ref)
{
  BLO_write_struct(writer, AssetWeakReference, weak_ref);
  BLO_write_string(writer, weak_ref->asset_library_identifier);
  BLO_write_string(writer, weak_ref->relative_asset_identifier);
}

void BKE_asset_weak_reference_read(BlendDataReader *reader, AssetWeakReference *weak_ref)
{
  BLO_read_data_address(reader, &weak_ref->asset_library_identifier);
  BLO_read_data_address(reader, &weak_ref->relative_asset_identifier);
}

/* Main database for each brush asset blend file.
 *
 * This avoids mixing asset datablocks in the regular main, which leads to naming conflicts and
 * confusing user interface.
 *
 * TODO: Heavily WIP code. */

struct AssetWeakReferenceMain {
  /* TODO: not sure if this is the best unique identifier. */
  std::string filepath;
  Main *main;

  AssetWeakReferenceMain(std::string filepath)
      : filepath(std::move(filepath)), main(BKE_main_new())
  {
    main->is_asset_weak_reference_main = true;
  }
  AssetWeakReferenceMain(const AssetWeakReferenceMain &) = delete;
  AssetWeakReferenceMain(AssetWeakReferenceMain &&other)
      : filepath(std::exchange(other.filepath, "")), main(std::exchange(other.main, nullptr))
  {
  }

  ~AssetWeakReferenceMain()
  {
    if (main) {
      BKE_main_free(main);
    }
  }
};

static Vector<AssetWeakReferenceMain> &get_weak_reference_mains()
{
  static Vector<AssetWeakReferenceMain> mains;
  return mains;
}

Main *BKE_asset_weak_reference_main(Main *global_main, const ID *id)
{
  if (!(id->tag & LIB_TAG_ASSET_MAIN)) {
    return global_main;
  }

  for (const AssetWeakReferenceMain &weak_ref_main : get_weak_reference_mains()) {
    /* TODO: Look into make this whole thing more efficient. */
    ListBase *lb = which_libbase(weak_ref_main.main, GS(id->name));
    LISTBASE_FOREACH (ID *, other_id, lb) {
      if (id == other_id) {
        return weak_ref_main.main;
      }
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

static Main &asset_weak_reference_main_ensure(const StringRef filepath)
{
  for (const AssetWeakReferenceMain &weak_ref_main : get_weak_reference_mains()) {
    if (weak_ref_main.filepath == filepath) {
      return *weak_ref_main.main;
    }
  }

  get_weak_reference_mains().append_as(filepath);
  return *get_weak_reference_mains().last().main;
}

void BKE_asset_weak_reference_main_free()
{
  get_weak_reference_mains().clear_and_shrink();
}

ID *BKE_asset_weak_reference_ensure(Main &global_main,
                                    const ID_Type id_type,
                                    const AssetWeakReference &weak_ref)
{
  char asset_full_path_buffer[FILE_MAX_LIBEXTRA];
  char *asset_lib_path, *asset_group, *asset_name;

  AS_asset_full_path_explode_from_weak_ref(
      &weak_ref, asset_full_path_buffer, &asset_lib_path, &asset_group, &asset_name);

  if (asset_lib_path == nullptr && asset_group == nullptr && asset_name == nullptr) {
    return nullptr;
  }

  BLI_assert(asset_name != nullptr);

  /* If weak reference resolves to a null library path, assume we are in local asset case. */
  Main &bmain = asset_lib_path ? asset_weak_reference_main_ensure(asset_lib_path) : global_main;

  /* Check if we have the asset already, or if it's global main and there is nothing we can add. */
  ID *local_asset = BKE_libblock_find_name(&bmain, id_type, asset_name);
  if (local_asset || asset_lib_path == nullptr) {
    BLI_assert(local_asset == nullptr || ID_IS_ASSET(local_asset));
    return local_asset;
  }

  /* Load asset from asset library. */
  LibraryLink_Params lapp_params{};
  lapp_params.bmain = &bmain;
  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_flag_set(lapp_context, BLO_LIBLINK_FORCE_INDIRECT, true);
  BKE_blendfile_link_append_context_flag_set(lapp_context, 0, true);

  BKE_blendfile_link_append_context_library_add(lapp_context, asset_lib_path, nullptr);

  BlendfileLinkAppendContextItem *lapp_item = BKE_blendfile_link_append_context_item_add(
      lapp_context, asset_name, id_type, nullptr);
  BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, lapp_item, 0);

  BKE_blendfile_link(lapp_context, nullptr);
  BKE_blendfile_append(lapp_context, nullptr);

  local_asset = BKE_blendfile_link_append_context_item_newid_get(lapp_context, lapp_item);

  BKE_blendfile_link_append_context_free(lapp_context);

  /* TODO: only do for new ones? */
  BKE_main_id_tag_all(&bmain, LIB_TAG_ASSET_MAIN, true);

  /* Verify that the name matches. It must for referencing the same asset again to work.  */
  BLI_assert(local_asset == nullptr || STREQ(local_asset->name + 2, asset_name));

  return local_asset;
}
