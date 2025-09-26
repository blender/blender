/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 * `.blend` file reading entry point.
 */

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_path_utils.hh" /* Only for assertions. */
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_genfile.h"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_preview_image.hh"

#include "BLO_readfile.hh"

#include "readfile.hh"

#include "BLI_sys_types.h" /* Needed for `intptr_t`. */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* Access routines used by file-selector. */

void BLO_datablock_info_free(BLODataBlockInfo *datablock_info)
{
  if (datablock_info->free_asset_data) {
    BKE_asset_metadata_free(&datablock_info->asset_data);
    datablock_info->free_asset_data = false;
  }
}

void BLO_datablock_info_linklist_free(LinkNode *datablock_infos)
{
  BLI_linklist_free(datablock_infos, [](void *link) {
    BLODataBlockInfo *datablock_info = static_cast<BLODataBlockInfo *>(link);
    BLO_datablock_info_free(datablock_info);
    MEM_freeN(datablock_info);
  });
}

BlendHandle *BLO_blendhandle_from_file(const char *filepath, BlendFileReadReport *reports)
{
  BlendHandle *bh;

  bh = (BlendHandle *)blo_filedata_from_file(filepath, reports);

  return bh;
}

BlendHandle *BLO_blendhandle_from_memory(const void *mem,
                                         int memsize,
                                         BlendFileReadReport *reports)
{
  BlendHandle *bh;

  bh = (BlendHandle *)blo_filedata_from_memory(mem, memsize, reports);

  return bh;
}

blender::int3 BLO_blendhandle_get_version(const BlendHandle *bh)
{
  const FileData *fd = reinterpret_cast<const FileData *>(bh);
  return blender::int3(fd->fileversion / 100, fd->fileversion % 100, fd->filesubversion);
}

/* Return `false` if the block should be skipped because it is either an invalid block, or it does
 * not meet to required conditions. */
static bool blendhandle_load_id_data_and_validate(FileData *fd,
                                                  BHead *bhead,
                                                  bool use_assets_only,
                                                  const char *&r_idname,
                                                  short &r_idflag,
                                                  AssetMetaData *&r_asset_meta_data)
{
  r_idname = blo_bhead_id_name(fd, bhead);
  if (!r_idname || r_idname[0] == '\0') {
    return false;
  }
  r_idflag = blo_bhead_id_flag(fd, bhead);
  /* Do not list (and therefore allow direct linking of) packed data.
   * While supporting this is conceptually possible, it would require significant changes in
   * the UI (file browser) and UX (link operation) to convey this concept and handle it
   * correctly. */
  if (r_idflag & ID_FLAG_LINKED_AND_PACKED) {
    return false;
  }
  r_asset_meta_data = blo_bhead_id_asset_data_address(fd, bhead);
  if (use_assets_only && r_asset_meta_data == nullptr) {
    return false;
  }
  return true;
}

LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh,
                                              int ofblocktype,
                                              const bool use_assets_only,
                                              int *r_tot_names)
{
  FileData *fd = (FileData *)bh;
  LinkNode *names = nullptr;
  BHead *bhead;
  int tot = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == ofblocktype) {
      const char *idname;
      short idflag;
      AssetMetaData *asset_meta_data;
      if (!blendhandle_load_id_data_and_validate(
              fd, bhead, use_assets_only, idname, idflag, asset_meta_data))
      {
        continue;
      }

      BLI_linklist_prepend(&names, BLI_strdup(idname + 2));
      tot++;
    }
    else if (bhead->code == BLO_CODE_ENDB) {
      break;
    }
  }

  *r_tot_names = tot;
  return names;
}

LinkNode *BLO_blendhandle_get_datablock_info(BlendHandle *bh,
                                             int ofblocktype,
                                             const bool use_assets_only,
                                             int *r_tot_info_items)
{
  FileData *fd = (FileData *)bh;
  LinkNode *infos = nullptr;
  BHead *bhead;
  int tot = 0;

  const int sdna_nr_preview_image = DNA_struct_find_with_alias(fd->filesdna, "PreviewImage");

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_ENDB) {
      break;
    }
    if (bhead->code == ofblocktype) {
      BHead *id_bhead = bhead;

      const char *idname;
      short idflag;
      AssetMetaData *asset_meta_data;
      if (!blendhandle_load_id_data_and_validate(
              fd, id_bhead, use_assets_only, idname, idflag, asset_meta_data))
      {
        continue;
      }

      const char *name = idname + 2;
      BLODataBlockInfo *info = MEM_mallocN<BLODataBlockInfo>(__func__);

      /* Lastly, read asset data from the following blocks. */
      if (asset_meta_data) {
        bhead = blo_read_asset_data_block(fd, bhead, &asset_meta_data);
        /* blo_read_asset_data_block() reads all DATA heads and already advances bhead to the
         * next non-DATA one. Go back, so the loop doesn't skip the non-DATA head. */
        bhead = blo_bhead_prev(fd, bhead);
      }

      STRNCPY(info->name, name);
      info->asset_data = asset_meta_data;
      info->free_asset_data = true;

      bool has_preview = false;
      /* See if we can find a preview in the data of this ID. */
      for (BHead *data_bhead = blo_bhead_next(fd, id_bhead); data_bhead->code == BLO_CODE_DATA;
           data_bhead = blo_bhead_next(fd, data_bhead))
      {
        if (data_bhead->SDNAnr == sdna_nr_preview_image) {
          has_preview = true;
          break;
        }
      }
      info->no_preview_found = !has_preview;

      BLI_linklist_prepend(&infos, info);
      tot++;
    }
  }

  *r_tot_info_items = tot;
  return infos;
}

/**
 * Read the preview rects and store in `result`.
 *
 * `bhead` should point to the block that sourced the `preview_from_file`
 *     parameter.
 * `bhead` parameter is consumed. The correct bhead pointing to the next bhead in the file after
 * the preview rects is returned by this function.
 * \param fd: The filedata to read the data from.
 * \param bhead: should point to the block that sourced the `preview_from_file parameter`.
 *               bhead is consumed. the new bhead is returned by this function.
 * \param result: the Preview Image where the preview rect will be stored.
 * \param preview_from_file: The read PreviewImage where the bhead points to. The rects of this
 * \return PreviewImage or nullptr when no preview Images have been found. Caller owns the returned
 */
static BHead *blo_blendhandle_read_preview_rects(FileData *fd,
                                                 BHead *bhead,
                                                 PreviewImage *result,
                                                 const PreviewImage *preview_from_file)
{
  for (int preview_index = 0; preview_index < NUM_ICON_SIZES; preview_index++) {
    if (preview_from_file->rect[preview_index] && preview_from_file->w[preview_index] &&
        preview_from_file->h[preview_index])
    {
      bhead = blo_bhead_next(fd, bhead);
      BLI_assert((preview_from_file->w[preview_index] * preview_from_file->h[preview_index] *
                  sizeof(uint)) == bhead->len);
      result->rect[preview_index] = static_cast<uint *>(
          BLO_library_read_struct(fd, bhead, "PreviewImage Icon Rect"));
    }
    else {
      /* This should not be needed, but can happen in 'broken' .blend files,
       * better handle this gracefully than crashing. */
      BLI_assert(preview_from_file->rect[preview_index] == nullptr &&
                 preview_from_file->w[preview_index] == 0 &&
                 preview_from_file->h[preview_index] == 0);
      result->rect[preview_index] = nullptr;
      result->w[preview_index] = result->h[preview_index] = 0;
    }
    BKE_previewimg_finish(result, preview_index);
  }

  return bhead;
}

PreviewImage *BLO_blendhandle_get_preview_for_id(BlendHandle *bh,
                                                 int ofblocktype,
                                                 const char *name)
{
  FileData *fd = (FileData *)bh;
  bool looking = false;
  const int sdna_preview_image = DNA_struct_find_with_alias(fd->filesdna, "PreviewImage");

  for (BHead *bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_DATA) {
      if (looking && bhead->SDNAnr == sdna_preview_image) {
        PreviewImage *preview_from_file = static_cast<PreviewImage *>(
            BLO_library_read_struct(fd, bhead, "PreviewImage"));

        if (preview_from_file == nullptr) {
          break;
        }

        PreviewImage *result = static_cast<PreviewImage *>(MEM_dupallocN(preview_from_file));
        result->runtime = MEM_new<blender::bke::PreviewImageRuntime>(__func__);
        bhead = blo_blendhandle_read_preview_rects(fd, bhead, result, preview_from_file);
        MEM_freeN(preview_from_file);
        return result;
      }
    }
    else if (looking || bhead->code == BLO_CODE_ENDB) {
      /* We were looking for a preview image, but didn't find any belonging to block. So it doesn't
       * exist. */
      break;
    }
    else if (bhead->code == ofblocktype) {
      const char *idname = blo_bhead_id_name(fd, bhead);
      if (idname && STREQ(&idname[2], name)) {
        looking = true;
      }
    }
  }

  return nullptr;
}

LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh)
{
  FileData *fd = (FileData *)bh;
  GSet *gathered = BLI_gset_ptr_new("linkable_groups gh");
  LinkNode *names = nullptr;
  BHead *bhead;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_ENDB) {
      break;
    }
    if (BKE_idtype_idcode_is_valid(bhead->code)) {
      if (BKE_idtype_idcode_is_linkable(bhead->code)) {
        const char *str = BKE_idtype_idcode_to_name(bhead->code);

        if (BLI_gset_add(gathered, (void *)str)) {
          BLI_linklist_prepend(&names, BLI_strdup(str));
        }
      }
    }
  }

  BLI_gset_free(gathered, nullptr);

  return names;
}

void BLO_blendhandle_close(BlendHandle *bh)
{
  FileData *fd = (FileData *)bh;

  blo_filedata_free(fd);
}

void BLO_read_invalidate_message(BlendHandle *bh, Main *bmain, const char *message)
{
  FileData *fd = reinterpret_cast<FileData *>(bh);

  blo_readfile_invalidate(fd, bmain, message);
}

/**********/

BlendFileData *BLO_read_from_file(const char *filepath,
                                  eBLOReadSkip skip_flags,
                                  BlendFileReadReport *reports)
{
  BLI_assert(!BLI_path_is_rel(filepath));
  BLI_assert(BLI_path_is_abs_from_cwd(filepath));

  BlendFileData *bfd = nullptr;
  FileData *fd;

  fd = blo_filedata_from_file(filepath, reports);
  if (fd) {
    fd->skip_flags = skip_flags;
    bfd = blo_read_file_internal(fd, filepath);
    blo_filedata_free(fd);
  }

  return bfd;
}

BlendFileData *BLO_read_from_memory(const void *mem,
                                    int memsize,
                                    eBLOReadSkip skip_flags,
                                    ReportList *reports)
{
  BlendFileData *bfd = nullptr;
  FileData *fd;
  BlendFileReadReport bf_reports{};
  bf_reports.reports = reports;

  fd = blo_filedata_from_memory(mem, memsize, &bf_reports);
  if (fd) {
    fd->skip_flags = skip_flags;
    bfd = blo_read_file_internal(fd, "");
    blo_filedata_free(fd);
  }

  return bfd;
}

BlendFileData *BLO_read_from_memfile(Main *oldmain,
                                     const char *filepath,
                                     MemFile *memfile,
                                     const BlendFileReadParams *params,
                                     ReportList *reports)
{
  BlendFileData *bfd = nullptr;
  FileData *fd;
  BlendFileReadReport bf_reports{};
  bf_reports.reports = reports;

  fd = blo_filedata_from_memfile(memfile, params, &bf_reports);
  if (fd) {
    fd->skip_flags = eBLOReadSkip(params->skip_flags);
    STRNCPY(fd->relabase, filepath);

    /* Build old ID map for all old IDs. */
    blo_make_old_idmap_from_main(fd, oldmain);

    /* Separate linked data from old main.
     * WARNING: Do not split out packed IDs here, as these are handled similarly as local IDs in
     * undo context. */
    blo_split_main(oldmain, false);
    fd->old_bmain = oldmain;

    /* Removed packed data from this trick - it's internal data that needs saves. */

    /* Store all existing ID caches pointers into a mapping, to allow restoring them into newly
     * read IDs whenever possible.
     *
     * Note that this is only required for local data, since linked data are always re-used
     * 'as-is'. */
    blo_cache_storage_init(fd, oldmain);

    bfd = blo_read_file_internal(fd, filepath);

    /* Ensure relinked caches are not freed together with their old IDs. */
    blo_cache_storage_old_bmain_clear(fd, oldmain);

    /* Still in-use libraries have already been moved from oldmain to new main
     * (fd->bmain->split_mains), but oldmain itself shall *never* be 'transferred' to the new
     * split_mains!
     */
    BLI_assert(oldmain->split_mains && (*oldmain->split_mains)[0] == oldmain);

    /* That way, libraries (aka mains) we did not reuse in new undone/redone state
     * will be cleared together with `oldmain`. */
    blo_join_main(oldmain);

    blo_filedata_free(fd);
  }

  return bfd;
}

void BLO_blendfiledata_free(BlendFileData *bfd)
{
  if (bfd->main) {
    BKE_main_free(bfd->main);
  }

  if (bfd->user) {
    MEM_freeN(bfd->user);
  }

  MEM_delete(bfd);
}

void BLO_read_do_version_after_setup(Main *new_bmain,
                                     BlendfileLinkAppendContext *lapp_context,
                                     BlendFileReadReport *reports)
{
  do_versions_after_setup(new_bmain, lapp_context, reports);
}
