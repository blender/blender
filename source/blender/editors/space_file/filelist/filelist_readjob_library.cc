/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 *
 * Directory browsing with support for displaying .blend file contents.
 */

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

static void filelist_readjob_lib(FileListReadJob *job_params,
                                 bool *stop,
                                 bool *do_update,
                                 float *progress)
{
  filelist_readjob_directories_and_libraries(true, job_params, stop, do_update, progress);
}

void filelist_set_readjob_library(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_lib;
  filelist->read_job_fn = filelist_readjob_lib;
  filelist->filter_fn = is_filtered_lib;
}

}  // namespace blender
