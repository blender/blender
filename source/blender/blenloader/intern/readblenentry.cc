/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 * `.blend` file reading entry point.
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h" /* Only for assertions. */
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "BKE_asset.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_main.h"

#include "BLO_blend_defs.h"
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "readfile.h"

#include "BLI_sys_types.h" /* Needed for `intptr_t`. */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* local prototypes --------------------- */
void BLO_blendhandle_print_sizes(BlendHandle *bh, void *fp);

/* Access routines used by filesel. */

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

void BLO_blendhandle_print_sizes(BlendHandle *bh, void *fp)
{
  FileData *fd = (FileData *)bh;
  BHead *bhead;

  fprintf(static_cast<FILE *>(fp), "[\n");
  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_ENDB) {
      break;
    }

    const SDNA_Struct *struct_info = fd->filesdna->structs[bhead->SDNAnr];
    const char *name = fd->filesdna->types[struct_info->type];
    char buf[4];

    buf[0] = (bhead->code >> 24) & 0xFF;
    buf[1] = (bhead->code >> 16) & 0xFF;
    buf[2] = (bhead->code >> 8) & 0xFF;
    buf[3] = (bhead->code >> 0) & 0xFF;

    buf[0] = buf[0] ? buf[0] : ' ';
    buf[1] = buf[1] ? buf[1] : ' ';
    buf[2] = buf[2] ? buf[2] : ' ';
    buf[3] = buf[3] ? buf[3] : ' ';

    fprintf(static_cast<FILE *>(fp),
            "['%.4s', '%s', %d, %ld ],\n",
            buf,
            name,
            bhead->nr,
            (long int)(bhead->len + sizeof(BHead)));
  }
  fprintf(static_cast<FILE *>(fp), "]\n");
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
      const char *idname = blo_bhead_id_name(fd, bhead);
      if (use_assets_only && blo_bhead_id_asset_data_address(fd, bhead) == nullptr) {
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

  const int sdna_nr_preview_image = DNA_struct_find_nr(fd->filesdna, "PreviewImage");

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_ENDB) {
      break;
    }
    if (bhead->code == ofblocktype) {
      BHead *id_bhead = bhead;

      const char *name = blo_bhead_id_name(fd, bhead) + 2;
      AssetMetaData *asset_meta_data = blo_bhead_id_asset_data_address(fd, bhead);

      const bool is_asset = asset_meta_data != nullptr;
      const bool skip_datablock = use_assets_only && !is_asset;
      if (skip_datablock) {
        continue;
      }
      BLODataBlockInfo *info = static_cast<BLODataBlockInfo *>(
          MEM_mallocN(sizeof(*info), __func__));

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
  const int sdna_preview_image = DNA_struct_find_nr(fd->filesdna, "PreviewImage");

  for (BHead *bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == BLO_CODE_DATA) {
      if (looking && bhead->SDNAnr == sdna_preview_image) {
        PreviewImage *preview_from_file = static_cast<PreviewImage *>(
            BLO_library_read_struct(fd, bhead, "PreviewImage"));

        if (preview_from_file == nullptr) {
          break;
        }

        PreviewImage *result = static_cast<PreviewImage *>(MEM_dupallocN(preview_from_file));
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
      if (STREQ(&idname[2], name)) {
        looking = true;
      }
    }
  }

  return nullptr;
}

LinkNode *BLO_blendhandle_get_previews(BlendHandle *bh, int ofblocktype, int *r_tot_prev)
{
  FileData *fd = (FileData *)bh;
  LinkNode *previews = nullptr;
  BHead *bhead;
  int looking = 0;
  PreviewImage *prv = nullptr;
  PreviewImage *new_prv = nullptr;
  int tot = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == ofblocktype) {
      const char *idname = blo_bhead_id_name(fd, bhead);
      switch (GS(idname)) {
        case ID_MA:  /* fall through */
        case ID_TE:  /* fall through */
        case ID_IM:  /* fall through */
        case ID_WO:  /* fall through */
        case ID_LA:  /* fall through */
        case ID_OB:  /* fall through */
        case ID_GR:  /* fall through */
        case ID_SCE: /* fall through */
        case ID_AC:  /* fall through */
        case ID_NT:  /* fall through */
          new_prv = static_cast<PreviewImage *>(MEM_callocN(sizeof(PreviewImage), "newpreview"));
          BLI_linklist_prepend(&previews, new_prv);
          tot++;
          looking = 1;
          break;
        default:
          break;
      }
    }
    else if (bhead->code == BLO_CODE_DATA) {
      if (looking) {
        if (bhead->SDNAnr == DNA_struct_find_nr(fd->filesdna, "PreviewImage")) {
          prv = static_cast<PreviewImage *>(BLO_library_read_struct(fd, bhead, "PreviewImage"));

          if (prv) {
            memcpy(new_prv, prv, sizeof(PreviewImage));
            bhead = blo_blendhandle_read_preview_rects(fd, bhead, new_prv, prv);
            MEM_freeN(prv);
          }
        }
      }
    }
    else if (bhead->code == BLO_CODE_ENDB) {
      break;
    }
    else {
      looking = 0;
      new_prv = nullptr;
      prv = nullptr;
    }
  }

  *r_tot_prev = tot;
  return previews;
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
  ListBase old_mainlist;
  BlendFileReadReport bf_reports{};
  bf_reports.reports = reports;

  fd = blo_filedata_from_memfile(memfile, params, &bf_reports);
  if (fd) {
    fd->skip_flags = eBLOReadSkip(params->skip_flags);
    STRNCPY(fd->relabase, filepath);

    /* Build old ID map for all old IDs. */
    blo_make_old_idmap_from_main(fd, oldmain);

    /* Separate linked data from old main. */
    blo_split_main(&old_mainlist, oldmain);
    fd->old_mainlist = &old_mainlist;

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

    /* Still in-use libraries have already been moved from oldmain to new mainlist,
     * but oldmain itself shall *never* be 'transferred' to new mainlist! */
    BLI_assert(old_mainlist.first == oldmain);

    /* That way, libraries (aka mains) we did not reuse in new undone/redone state
     * will be cleared together with `oldmain`. */
    blo_join_main(&old_mainlist);

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

  MEM_freeN(bfd);
}
