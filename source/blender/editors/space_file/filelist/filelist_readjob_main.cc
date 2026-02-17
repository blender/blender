/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "filelist_intern.hh"

#include "filelist_readjob.hh"

namespace blender {

#if 0
/* Kept for reference here, in case we want to add back that feature later.
 * We do not need it currently. */
/* Code ***NOT*** updated for job stuff! */
static void filelist_readjob_main_recursive(Main *bmain, FileList *filelist)
{
  ID *id;
  FileDirEntry *files, *firstlib = nullptr;
  ListBaseT<ID> *lb;
  int a, fake, idcode, ok, totlib, totbl;

  // filelist->type = FILE_MAIN; /* XXX TODO: add modes to file-browser */

  BLI_assert(filelist->filelist.entries == nullptr);

  if (filelist->filelist.root[0] == '/') {
    filelist->filelist.root[0] = '\0';
  }

  if (filelist->filelist.root[0]) {
    idcode = groupname_to_code(filelist->filelist.root);
    if (idcode == 0) {
      filelist->filelist.root[0] = '\0';
    }
  }

  if (filelist->dir[0] == 0) {
    /* make directories */
#  ifdef WITH_FREESTYLE
    filelist->filelist.entries_num = 27;
#  else
    filelist->filelist.entries_num = 26;
#  endif
    filelist_resize(filelist, filelist->filelist.entries_num);

    for (a = 0; a < filelist->filelist.entries_num; a++) {
      filelist->filelist.entries[a].typeflag |= FILE_TYPE_DIR;
    }

    filelist->filelist.entries[0].entry->relpath = BLI_strdup(FILENAME_PARENT);
    filelist->filelist.entries[1].entry->relpath = BLI_strdup("Scene");
    filelist->filelist.entries[2].entry->relpath = BLI_strdup("Object");
    filelist->filelist.entries[3].entry->relpath = BLI_strdup("Mesh");
    filelist->filelist.entries[4].entry->relpath = BLI_strdup("Curve");
    filelist->filelist.entries[5].entry->relpath = BLI_strdup("Metaball");
    filelist->filelist.entries[6].entry->relpath = BLI_strdup("Material");
    filelist->filelist.entries[7].entry->relpath = BLI_strdup("Texture");
    filelist->filelist.entries[8].entry->relpath = BLI_strdup("Image");
    filelist->filelist.entries[9].entry->relpath = BLI_strdup("Ika");
    filelist->filelist.entries[10].entry->relpath = BLI_strdup("Wave");
    filelist->filelist.entries[11].entry->relpath = BLI_strdup("Lattice");
    filelist->filelist.entries[12].entry->relpath = BLI_strdup("Light");
    filelist->filelist.entries[13].entry->relpath = BLI_strdup("Camera");
    filelist->filelist.entries[14].entry->relpath = BLI_strdup("Ipo");
    filelist->filelist.entries[15].entry->relpath = BLI_strdup("World");
    filelist->filelist.entries[16].entry->relpath = BLI_strdup("Screen");
    filelist->filelist.entries[17].entry->relpath = BLI_strdup("VFont");
    filelist->filelist.entries[18].entry->relpath = BLI_strdup("Text");
    filelist->filelist.entries[19].entry->relpath = BLI_strdup("Armature");
    filelist->filelist.entries[20].entry->relpath = BLI_strdup("Action");
    filelist->filelist.entries[21].entry->relpath = BLI_strdup("NodeTree");
    filelist->filelist.entries[22].entry->relpath = BLI_strdup("Speaker");
    filelist->filelist.entries[23].entry->relpath = BLI_strdup("Curves");
    filelist->filelist.entries[24].entry->relpath = BLI_strdup("Point Cloud");
    filelist->filelist.entries[25].entry->relpath = BLI_strdup("Volume");
#  ifdef WITH_FREESTYLE
    filelist->filelist.entries[26].entry->relpath = BLI_strdup("FreestyleLineStyle");
#  endif
  }
  else {
    /* make files */
    idcode = groupname_to_code(filelist->filelist.root);

    lb = which_libbase(bmain, idcode);
    if (lb == nullptr) {
      return;
    }

    filelist->filelist.entries_num = 0;
    for (id = lb->first; id; id = id->next) {
      if (!(filelist->filter_data.flags & FLF_HIDE_DOT) || id->name[2] != '.') {
        filelist->filelist.entries_num++;
      }
    }

    /* XXX TODO: if data-browse or append/link #FLF_HIDE_PARENT has to be set. */
    if (!(filelist->filter_data.flags & FLF_HIDE_PARENT)) {
      filelist->filelist.entries_num++;
    }

    if (filelist->filelist.entries_num > 0) {
      filelist_resize(filelist, filelist->filelist.entries_num);
    }

    files = filelist->filelist.entries;

    if (!(filelist->filter_data.flags & FLF_HIDE_PARENT)) {
      files->entry->relpath = BLI_strdup(FILENAME_PARENT);
      files->typeflag |= FILE_TYPE_DIR;

      files++;
    }

    totlib = totbl = 0;
    for (id = lb->first; id; id = id->next) {
      ok = 1;
      if (ok) {
        if (!(filelist->filter_data.flags & FLF_HIDE_DOT) || id->name[2] != '.') {
          if (!ID_IS_LINKED(id)) {
            files->entry->relpath = BLI_strdup(id->name + 2);
          }
          else {
            char relname[FILE_MAX + (MAX_ID_NAME - 2) + 3];
            SNPRINTF(relname, "%s | %s", id->lib->filepath, id->name + 2);
            files->entry->relpath = BLI_strdup(relname);
          }
//                  files->type |= S_IFREG;
#  if 0 /* XXX TODO: show the selection status of the objects. */
          if (!filelist->has_func) { /* F4 DATA BROWSE */
            if (idcode == ID_OB) {
              if (((Object *)id)->flag & SELECT) {
                files->entry->selflag |= FILE_SEL_SELECTED;
              }
            }
            else if (idcode == ID_SCE) {
              if (((Scene *)id)->r.scemode & R_BG_RENDER) {
                files->entry->selflag |= FILE_SEL_SELECTED;
              }
            }
          }
#  endif
          //                  files->entry->nr = totbl + 1;
          files->entry->poin = id;
          fake = id->flag & ID_FLAG_FAKEUSER;
          if (ELEM(idcode, ID_MA, ID_TE, ID_LA, ID_WO, ID_IM)) {
            files->typeflag |= FILE_TYPE_IMAGE;
          }
#  if 0
          if (id->lib && fake) {
            SNPRINTF_UTF8(files->extra, "LF %d", id->us);
          }
          else if (id->lib) {
            SNPRINTF_UTF8(files->extra, "L    %d", id->us);
          }
          else if (fake) {
            SNPRINTF_UTF8(files->extra, "F    %d", id->us);
          }
          else {
            SNPRINTF_UTF8(files->extra, "      %d", id->us);
          }
#  endif

          if (id->lib) {
            if (totlib == 0) {
              firstlib = files;
            }
            totlib++;
          }

          files++;
        }
        totbl++;
      }
    }

    /* only qsort of library blocks */
    if (totlib > 1) {
      qsort(firstlib, totlib, sizeof(*files), compare_name);
    }
  }
}
#endif

static bool filelist_checkdir_main(const FileList *filelist,
                                   char dirpath[FILE_MAX_LIBEXTRA],
                                   const bool do_change)
{
  /* TODO */
  return filelist_checkdir_lib(filelist, dirpath, do_change);
}

static void filelist_readjob_main(FileListReadJob *job_params,
                                  bool *stop,
                                  bool *do_update,
                                  float *progress)
{
  /* TODO! */
  filelist_readjob_dir(job_params, stop, do_update, progress);
}

void filelist_set_readjob_main(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_main;
  filelist->read_job_fn = filelist_readjob_main;
  filelist->filter_fn = is_filtered_main;
}

}  // namespace blender
