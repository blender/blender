/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

void filelist_readjob_main_assets_add_items(FileListReadJob *job_params,
                                            bool * /*stop*/,
                                            bool *do_update,
                                            float * /*progress*/)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  FileListInternEntry *entry;
  ListBaseT<FileListInternEntry> tmp_entries = {nullptr};
  ID *id_iter;
  int entries_num = 0;

  /* Make sure no IDs are added/removed/reallocated in the main thread while this is running in
   * parallel. */
  BKE_main_lock(job_params->current_main);

  FOREACH_MAIN_ID_BEGIN (job_params->current_main, id_iter) {
    if (!id_iter->asset_data || ID_IS_LINKED(id_iter)) {
      continue;
    }

    const char *id_code_name = BKE_idtype_idcode_to_name(GS(id_iter->name));

    entry = MEM_new<FileListInternEntry>(__func__);
    std::string datablock_path = StringRef(id_code_name) + SEP_STR + (id_iter->name + 2);
    entry->relpath = current_relpath_append(job_params, datablock_path.c_str());
    entry->name = id_iter->name + 2;
    entry->free_name = false;
    entry->typeflag |= FILE_TYPE_BLENDERLIB | FILE_TYPE_ASSET;
    entry->blentype = GS(id_iter->name);
    entry->uid = filelist_uid_generate(filelist);
    entry->local_data.preview_image = BKE_asset_metadata_preview_get_from_id(id_iter->asset_data,
                                                                             id_iter);
    entry->local_data.id = id_iter;
    if (job_params->load_asset_library) {
      entry->asset = job_params->load_asset_library->add_local_id_asset(*id_iter);
    }
    entries_num++;
    BLI_addtail(&tmp_entries, entry);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_unlock(job_params->current_main);

  if (entries_num) {
    *do_update = true;

    BLI_movelisttolist(&filelist->filelist.entries, &tmp_entries);
    filelist->filelist.entries_num += entries_num;
    filelist->filelist.entries_filtered_num = -1;
  }
}

static void filelist_readjob_main_assets(FileListReadJob *job_params,
                                         bool *stop,
                                         bool *do_update,
                                         float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  filelist_readjob_load_asset_library_data(job_params, do_update);

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  filelist_readjob_main_assets_add_items(job_params, stop, do_update, progress);
}

void filelist_set_readjob_current_file_asset_library(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_return_always_valid;
  filelist->read_job_fn = filelist_readjob_main_assets;
  filelist->prepare_filter_fn = prepare_filter_asset_library;
  filelist->filter_fn = is_filtered_main_assets;
  filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA | FILELIST_TAGS_NO_THREADS |
                    FILELIST_TAGS_APPLY_FUZZY_SEARCH;
}
}  // namespace blender
