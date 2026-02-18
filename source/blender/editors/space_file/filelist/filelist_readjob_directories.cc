/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 *
 * Normal directory browsing.
 */

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

void filelist_readjob_dir(FileListReadJob *job_params,
                          bool *stop,
                          bool *do_update,
                          float *progress)
{
  filelist_readjob_directories_and_libraries(
      /*do_lib=*/false, job_params, stop, do_update, progress);
}

void filelist_set_readjob_directories(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_dir;
  filelist->read_job_fn = filelist_readjob_dir;
  filelist->filter_fn = is_filtered_file;
}

}  // namespace blender
