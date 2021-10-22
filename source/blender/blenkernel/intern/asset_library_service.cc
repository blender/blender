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
 */

/** \file
 * \ingroup bke
 */

#include "asset_library_service.hh"

#include "BKE_asset_library.hh"
#include "BKE_blender.h"
#include "BKE_callbacks.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string_ref.hh"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.asset_service"};

namespace blender::bke {

std::unique_ptr<AssetLibraryService> AssetLibraryService::instance_;
bool AssetLibraryService::atexit_handler_registered_ = false;

AssetLibraryService *AssetLibraryService::get()
{
  if (!instance_) {
    allocate_service_instance();
  }
  return instance_.get();
}

void AssetLibraryService::destroy()
{
  if (!instance_) {
    return;
  }
  instance_->app_handler_unregister();
  instance_.reset();
}

namespace {
std::string normalize_directory_path(StringRefNull directory)
{

  char dir_normalized[PATH_MAX];
  STRNCPY(dir_normalized, directory.c_str());
  BLI_path_normalize_dir(nullptr, dir_normalized);
  return std::string(dir_normalized);
}
}  // namespace

AssetLibrary *AssetLibraryService::get_asset_library_on_disk(StringRefNull top_level_directory)
{
  BLI_assert_msg(!top_level_directory.is_empty(),
                 "top level directory must be given for on-disk asset library");

  std::string top_dir_trailing_slash = normalize_directory_path(top_level_directory);

  AssetLibraryPtr *lib_uptr_ptr = on_disk_libraries_.lookup_ptr(top_dir_trailing_slash);
  if (lib_uptr_ptr != nullptr) {
    CLOG_INFO(&LOG, 2, "get \"%s\" (cached)", top_dir_trailing_slash.c_str());
    AssetLibrary *lib = lib_uptr_ptr->get();
    lib->refresh();
    return lib;
  }

  AssetLibraryPtr lib_uptr = std::make_unique<AssetLibrary>();
  AssetLibrary *lib = lib_uptr.get();

  lib->on_blend_save_handler_register();
  lib->load(top_dir_trailing_slash);

  on_disk_libraries_.add_new(top_dir_trailing_slash, std::move(lib_uptr));
  CLOG_INFO(&LOG, 2, "get \"%s\" (loaded)", top_dir_trailing_slash.c_str());
  return lib;
}

AssetLibrary *AssetLibraryService::get_asset_library_current_file()
{
  if (current_file_library_) {
    CLOG_INFO(&LOG, 2, "get current file lib (cached)");
  }
  else {
    CLOG_INFO(&LOG, 2, "get current file lib (loaded)");
    current_file_library_ = std::make_unique<AssetLibrary>();
    current_file_library_->on_blend_save_handler_register();
  }

  AssetLibrary *lib = current_file_library_.get();
  return lib;
}

void AssetLibraryService::allocate_service_instance()
{
  instance_ = std::make_unique<AssetLibraryService>();
  instance_->app_handler_register();

  if (!atexit_handler_registered_) {
    /* Ensure the instance gets freed before Blender's memory leak detector runs. */
    BKE_blender_atexit_register([](void * /*user_data*/) { AssetLibraryService::destroy(); },
                                nullptr);
    atexit_handler_registered_ = true;
  }
}

static void on_blendfile_load(struct Main * /*bMain*/,
                              struct PointerRNA ** /*pointers*/,
                              const int /*num_pointers*/,
                              void * /*arg*/)
{
  AssetLibraryService::destroy();
}

/**
 * Ensure the AssetLibraryService instance is destroyed before a new blend file is loaded.
 * This makes memory management simple, and ensures a fresh start for every blend file. */
void AssetLibraryService::app_handler_register()
{
  /* The callback system doesn't own `on_load_callback_store_`. */
  on_load_callback_store_.alloc = false;

  on_load_callback_store_.func = &on_blendfile_load;
  on_load_callback_store_.arg = this;

  BKE_callback_add(&on_load_callback_store_, BKE_CB_EVT_LOAD_PRE);
}

void AssetLibraryService::app_handler_unregister()
{
  BKE_callback_remove(&on_load_callback_store_, BKE_CB_EVT_LOAD_PRE);
  on_load_callback_store_.func = nullptr;
  on_load_callback_store_.arg = nullptr;
}

bool AssetLibraryService::has_any_unsaved_catalogs() const
{
  if (current_file_library_ && current_file_library_->catalog_service->has_unsaved_changes()) {
    return true;
  }

  for (const auto &asset_lib_uptr : on_disk_libraries_.values()) {
    if (asset_lib_uptr->catalog_service->has_unsaved_changes()) {
      return true;
    }
  }

  return false;
}

}  // namespace blender::bke
