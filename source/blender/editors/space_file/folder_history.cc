/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation */

/** \file
 * \ingroup spfile
 *
 * Storage for a list of folders for history backward and forward navigation.
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_context.h"

#include "DNA_space_types.h"

#include "ED_fileselect.h"

#include "MEM_guardedalloc.h"

#include "file_intern.h"

/* -------------------------------------------------------------------- */
/** \name FOLDERLIST (previous/next)
 * \{ */

struct FolderList {
  FolderList *next, *prev;
  char *foldername;
};

void folderlist_popdir(struct ListBase *folderlist, char *dir)
{
  const char *prev_dir;
  FolderList *folder;
  folder = static_cast<FolderList *>(folderlist->last);

  if (folder) {
    /* remove the current directory */
    MEM_freeN(folder->foldername);
    BLI_freelinkN(folderlist, folder);

    folder = static_cast<FolderList *>(folderlist->last);
    if (folder) {
      prev_dir = folder->foldername;
      BLI_strncpy(dir, prev_dir, FILE_MAXDIR);
    }
  }
  /* Delete the folder next or use set-directory directly before PREVIOUS OP. */
}

void folderlist_pushdir(ListBase *folderlist, const char *dir)
{
  if (!dir[0]) {
    return;
  }

  FolderList *folder, *previous_folder;
  previous_folder = static_cast<FolderList *>(folderlist->last);

  /* check if already exists */
  if (previous_folder && previous_folder->foldername) {
    if (BLI_path_cmp(previous_folder->foldername, dir) == 0) {
      return;
    }
  }

  /* create next folder element */
  folder = MEM_new<FolderList>(__func__);
  folder->foldername = BLI_strdup(dir);

  /* add it to the end of the list */
  BLI_addtail(folderlist, folder);
}

const char *folderlist_peeklastdir(ListBase *folderlist)
{
  FolderList *folder;

  if (!folderlist->last) {
    return nullptr;
  }

  folder = static_cast<FolderList *>(folderlist->last);
  return folder->foldername;
}

bool folderlist_clear_next(struct SpaceFile *sfile)
{
  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FolderList *folder;

  /* if there is no folder_next there is nothing we can clear */
  if (BLI_listbase_is_empty(sfile->folders_next)) {
    return false;
  }

  /* if previous_folder, next_folder or refresh_folder operators are executed
   * it doesn't clear folder_next */
  folder = static_cast<FolderList *>(sfile->folders_prev->last);
  if ((!folder) || (BLI_path_cmp(folder->foldername, params->dir) == 0)) {
    return false;
  }

  /* eventually clear flist->folders_next */
  return true;
}

void folderlist_free(ListBase *folderlist)
{
  if (folderlist) {
    LISTBASE_FOREACH (FolderList *, folder, folderlist) {
      MEM_freeN(folder->foldername);
    }
    BLI_freelistN(folderlist);
  }
}

static ListBase folderlist_duplicate(ListBase *folderlist)
{
  ListBase folderlistn = {nullptr};

  BLI_duplicatelist(&folderlistn, folderlist);

  LISTBASE_FOREACH (FolderList *, folder, &folderlistn) {
    folder->foldername = (char *)MEM_dupallocN(folder->foldername);
  }
  return folderlistn;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Folder-History (wraps/owns file list above)
 * \{ */

static FileFolderHistory *folder_history_find(const SpaceFile *sfile, eFileBrowse_Mode browse_mode)
{
  LISTBASE_FOREACH (FileFolderHistory *, history, &sfile->folder_histories) {
    if (history->browse_mode == browse_mode) {
      return history;
    }
  }

  return nullptr;
}

void folder_history_list_ensure_for_active_browse_mode(SpaceFile *sfile)
{
  FileFolderHistory *history = folder_history_find(sfile, (eFileBrowse_Mode)sfile->browse_mode);

  if (!history) {
    history = MEM_cnew<FileFolderHistory>(__func__);
    history->browse_mode = sfile->browse_mode;
    BLI_addtail(&sfile->folder_histories, history);
  }

  sfile->folders_next = &history->folders_next;
  sfile->folders_prev = &history->folders_prev;
}

static void folder_history_entry_free(SpaceFile *sfile, FileFolderHistory *history)
{
  if (sfile->folders_prev == &history->folders_prev) {
    sfile->folders_prev = nullptr;
  }
  if (sfile->folders_next == &history->folders_next) {
    sfile->folders_next = nullptr;
  }
  folderlist_free(&history->folders_prev);
  folderlist_free(&history->folders_next);
  BLI_freelinkN(&sfile->folder_histories, history);
}

void folder_history_list_free(SpaceFile *sfile)
{
  LISTBASE_FOREACH_MUTABLE (FileFolderHistory *, history, &sfile->folder_histories) {
    folder_history_entry_free(sfile, history);
  }
}

ListBase folder_history_list_duplicate(ListBase *listbase)
{
  ListBase histories = {nullptr};

  LISTBASE_FOREACH (FileFolderHistory *, history, listbase) {
    FileFolderHistory *history_new = static_cast<FileFolderHistory *>(MEM_dupallocN(history));
    history_new->folders_prev = folderlist_duplicate(&history->folders_prev);
    history_new->folders_next = folderlist_duplicate(&history->folders_next);
    BLI_addtail(&histories, history_new);
  }

  return histories;
}

/** \} */
