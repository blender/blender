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
 * .blend file reading entry point
 */

/** \file
 * \ingroup blenloader
 */

#include <stddef.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_main.h"

#include "BLO_blend_defs.h"
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "readfile.h"

#include "BLI_sys_types.h"  // needed for intptr_t

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* local prototypes --------------------- */
void BLO_blendhandle_print_sizes(BlendHandle *bh, void *fp);

/* Access routines used by filesel. */

/**
 * Open a blendhandle from a file path.
 *
 * \param filepath: The file path to open.
 * \param reports: Report errors in opening the file (can be NULL).
 * \return A handle on success, or NULL on failure.
 */
BlendHandle *BLO_blendhandle_from_file(const char *filepath, ReportList *reports)
{
  BlendHandle *bh;

  bh = (BlendHandle *)blo_filedata_from_file(filepath, reports);

  return bh;
}

/**
 * Open a blendhandle from memory.
 *
 * \param mem: The data to load from.
 * \param memsize: The size of the data.
 * \return A handle on success, or NULL on failure.
 */
BlendHandle *BLO_blendhandle_from_memory(const void *mem, int memsize)
{
  BlendHandle *bh;

  bh = (BlendHandle *)blo_filedata_from_memory(mem, memsize, NULL);

  return bh;
}

void BLO_blendhandle_print_sizes(BlendHandle *bh, void *fp)
{
  FileData *fd = (FileData *)bh;
  BHead *bhead;

  fprintf(fp, "[\n");
  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == ENDB) {
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

    fprintf(fp,
            "['%.4s', '%s', %d, %ld ],\n",
            buf,
            name,
            bhead->nr,
            (long int)(bhead->len + sizeof(BHead)));
  }
  fprintf(fp, "]\n");
}

/**
 * Gets the names of all the data-blocks in a file of a certain type
 * (e.g. all the scene names in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param tot_names: The length of the returned list.
 * \param use_assets_only: Only list IDs marked as assets.
 * \return A BLI_linklist of strings. The string links should be freed with #MEM_freeN().
 */
LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh,
                                              int ofblocktype,
                                              const bool use_assets_only,
                                              int *r_tot_names)
{
  FileData *fd = (FileData *)bh;
  LinkNode *names = NULL;
  BHead *bhead;
  int tot = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == ofblocktype) {
      const char *idname = blo_bhead_id_name(fd, bhead);
      if (use_assets_only && blo_bhead_id_asset_data_address(fd, bhead) == NULL) {
        continue;
      }

      BLI_linklist_prepend(&names, BLI_strdup(idname + 2));
      tot++;
    }
    else if (bhead->code == ENDB) {
      break;
    }
  }

  *r_tot_names = tot;
  return names;
}

/**
 * Gets the names and asset-data (if ID is an asset) of all the data-blocks in a file of a certain
 * type (e.g. all the scene names in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param tot_info_items: The length of the returned list.
 * \return A BLI_linklist of BLODataBlockInfo *. The links and #BLODataBlockInfo.asset_data should
 *         be freed with MEM_freeN.
 */
LinkNode *BLO_blendhandle_get_datablock_info(BlendHandle *bh,
                                             int ofblocktype,
                                             int *r_tot_info_items)
{
  FileData *fd = (FileData *)bh;
  LinkNode *infos = NULL;
  BHead *bhead;
  int tot = 0;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == ofblocktype) {
      struct BLODataBlockInfo *info = MEM_mallocN(sizeof(*info), __func__);
      const char *name = blo_bhead_id_name(fd, bhead) + 2;

      STRNCPY(info->name, name);

      /* Lastly, read asset data from the following blocks. */
      info->asset_data = blo_bhead_id_asset_data_address(fd, bhead);
      if (info->asset_data) {
        bhead = blo_read_asset_data_block(fd, bhead, &info->asset_data);
        /* blo_read_asset_data_block() reads all DATA heads and already advances bhead to the next
         * non-DATA one. Go back, so the loop doesn't skip the non-DATA head. */
        bhead = blo_bhead_prev(fd, bhead);
      }

      BLI_linklist_prepend(&infos, info);
      tot++;
    }
    else if (bhead->code == ENDB) {
      break;
    }
  }

  *r_tot_info_items = tot;
  return infos;
}

/**
 * Gets the previews of all the data-blocks in a file of a certain type
 * (e.g. all the scene previews in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param r_tot_prev: The length of the returned list.
 * \return A BLI_linklist of PreviewImage. The PreviewImage links should be freed with malloc.
 */
LinkNode *BLO_blendhandle_get_previews(BlendHandle *bh, int ofblocktype, int *r_tot_prev)
{
  FileData *fd = (FileData *)bh;
  LinkNode *previews = NULL;
  BHead *bhead;
  int looking = 0;
  PreviewImage *prv = NULL;
  PreviewImage *new_prv = NULL;
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
          new_prv = MEM_callocN(sizeof(PreviewImage), "newpreview");
          BLI_linklist_prepend(&previews, new_prv);
          tot++;
          looking = 1;
          break;
        default:
          break;
      }
    }
    else if (bhead->code == DATA) {
      if (looking) {
        if (bhead->SDNAnr == DNA_struct_find_nr(fd->filesdna, "PreviewImage")) {
          prv = BLO_library_read_struct(fd, bhead, "PreviewImage");

          if (prv) {
            memcpy(new_prv, prv, sizeof(PreviewImage));
            if (prv->rect[0] && prv->w[0] && prv->h[0]) {
              bhead = blo_bhead_next(fd, bhead);
              BLI_assert((new_prv->w[0] * new_prv->h[0] * sizeof(uint)) == bhead->len);
              new_prv->rect[0] = BLO_library_read_struct(fd, bhead, "PreviewImage Icon Rect");
            }
            else {
              /* This should not be needed, but can happen in 'broken' .blend files,
               * better handle this gracefully than crashing. */
              BLI_assert(prv->rect[0] == NULL && prv->w[0] == 0 && prv->h[0] == 0);
              new_prv->rect[0] = NULL;
              new_prv->w[0] = new_prv->h[0] = 0;
            }
            BKE_previewimg_finish(new_prv, 0);

            if (prv->rect[1] && prv->w[1] && prv->h[1]) {
              bhead = blo_bhead_next(fd, bhead);
              BLI_assert((new_prv->w[1] * new_prv->h[1] * sizeof(uint)) == bhead->len);
              new_prv->rect[1] = BLO_library_read_struct(fd, bhead, "PreviewImage Image Rect");
            }
            else {
              /* This should not be needed, but can happen in 'broken' .blend files,
               * better handle this gracefully than crashing. */
              BLI_assert(prv->rect[1] == NULL && prv->w[1] == 0 && prv->h[1] == 0);
              new_prv->rect[1] = NULL;
              new_prv->w[1] = new_prv->h[1] = 0;
            }
            BKE_previewimg_finish(new_prv, 1);
            MEM_freeN(prv);
          }
        }
      }
    }
    else if (bhead->code == ENDB) {
      break;
    }
    else {
      looking = 0;
      new_prv = NULL;
      prv = NULL;
    }
  }

  *r_tot_prev = tot;
  return previews;
}

/**
 * Gets the names of all the linkable data-block types available in a file.
 * (e.g. "Scene", "Mesh", "Light", etc.).
 *
 * \param bh: The blendhandle to access.
 * \return A BLI_linklist of strings. The string links should be freed with #MEM_freeN().
 */
LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh)
{
  FileData *fd = (FileData *)bh;
  GSet *gathered = BLI_gset_ptr_new("linkable_groups gh");
  LinkNode *names = NULL;
  BHead *bhead;

  for (bhead = blo_bhead_first(fd); bhead; bhead = blo_bhead_next(fd, bhead)) {
    if (bhead->code == ENDB) {
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

  BLI_gset_free(gathered, NULL);

  return names;
}

/**
 * Close and free a blendhandle. The handle becomes invalid after this call.
 *
 * \param bh: The handle to close.
 */
void BLO_blendhandle_close(BlendHandle *bh)
{
  FileData *fd = (FileData *)bh;

  blo_filedata_free(fd);
}

/**********/

/**
 * Open a blender file from a pathname. The function returns NULL
 * and sets a report in the list if it cannot open the file.
 *
 * \param filepath: The path of the file to open.
 * \param reports: If the return value is NULL, errors indicating the cause of the failure.
 * \return The data of the file.
 */
BlendFileData *BLO_read_from_file(const char *filepath,
                                  eBLOReadSkip skip_flags,
                                  ReportList *reports)
{
  BlendFileData *bfd = NULL;
  FileData *fd;

  fd = blo_filedata_from_file(filepath, reports);
  if (fd) {
    fd->reports = reports;
    fd->skip_flags = skip_flags;
    bfd = blo_read_file_internal(fd, filepath);
    blo_filedata_free(fd);
  }

  return bfd;
}

/**
 * Open a blender file from memory. The function returns NULL
 * and sets a report in the list if it cannot open the file.
 *
 * \param mem: The file data.
 * \param memsize: The length of \a mem.
 * \param reports: If the return value is NULL, errors indicating the cause of the failure.
 * \return The data of the file.
 */
BlendFileData *BLO_read_from_memory(const void *mem,
                                    int memsize,
                                    eBLOReadSkip skip_flags,
                                    ReportList *reports)
{
  BlendFileData *bfd = NULL;
  FileData *fd;

  fd = blo_filedata_from_memory(mem, memsize, reports);
  if (fd) {
    fd->reports = reports;
    fd->skip_flags = skip_flags;
    bfd = blo_read_file_internal(fd, "");
    blo_filedata_free(fd);
  }

  return bfd;
}

/**
 * Used for undo/redo, skips part of libraries reading
 * (assuming their data are already loaded & valid).
 *
 * \param oldmain: old main,
 * from which we will keep libraries and other data-blocks that should not have changed.
 * \param filename: current file, only for retrieving library data.
 */
BlendFileData *BLO_read_from_memfile(Main *oldmain,
                                     const char *filename,
                                     MemFile *memfile,
                                     const struct BlendFileReadParams *params,
                                     ReportList *reports)
{
  BlendFileData *bfd = NULL;
  FileData *fd;
  ListBase old_mainlist;

  fd = blo_filedata_from_memfile(memfile, params, reports);
  if (fd) {
    fd->reports = reports;
    fd->skip_flags = params->skip_flags;
    BLI_strncpy(fd->relabase, filename, sizeof(fd->relabase));

    /* clear ob->proxy_from pointers in old main */
    blo_clear_proxy_pointers_from_lib(oldmain);

    /* separate libraries from old main */
    blo_split_main(&old_mainlist, oldmain);
    /* add the library pointers in oldmap lookup */
    blo_add_library_pointer_map(&old_mainlist, fd);

    if ((params->skip_flags & BLO_READ_SKIP_UNDO_OLD_MAIN) == 0) {
      /* Build idmap of old main (we only care about local data here, so we can do that after
       * split_main() call. */
      blo_make_old_idmap_from_main(fd, old_mainlist.first);
    }

    /* removed packed data from this trick - it's internal data that needs saves */

    /* Store all existing ID caches pointers into a mapping, to allow restoring them into newly
     * read IDs whenever possible. */
    blo_cache_storage_init(fd, oldmain);

    bfd = blo_read_file_internal(fd, filename);

    /* Ensure relinked caches are not freed together with their old IDs. */
    blo_cache_storage_old_bmain_clear(fd, oldmain);

    /* Still in-use libraries have already been moved from oldmain to new mainlist,
     * but oldmain itself shall *never* be 'transferred' to new mainlist! */
    BLI_assert(old_mainlist.first == oldmain);

    /* That way, libs (aka mains) we did not reuse in new undone/redone state
     * will be cleared together with oldmain... */
    blo_join_main(&old_mainlist);

    blo_filedata_free(fd);
  }

  return bfd;
}

/**
 * Frees a BlendFileData structure and *all* the data associated with it
 * (the userdef data, and the main libblock data).
 *
 * \param bfd: The structure to free.
 */
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
