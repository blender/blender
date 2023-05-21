/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "IO_path_util.hh"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

namespace blender::io {

std::string path_reference(StringRefNull filepath,
                           StringRefNull base_src,
                           StringRefNull base_dst,
                           ePathReferenceMode mode,
                           Set<std::pair<std::string, std::string>> *copy_set)
{
  const bool is_relative = BLI_path_is_rel(filepath.c_str());
  char filepath_abs[PATH_MAX];
  STRNCPY(filepath_abs, filepath.c_str());
  BLI_path_abs(filepath_abs, base_src.c_str());
  BLI_path_normalize(filepath_abs);

  /* Figure out final mode to be used. */
  if (mode == PATH_REFERENCE_MATCH) {
    mode = is_relative ? PATH_REFERENCE_RELATIVE : PATH_REFERENCE_ABSOLUTE;
  }
  else if (mode == PATH_REFERENCE_AUTO) {
    mode = BLI_path_contains(base_dst.c_str(), filepath_abs) ? PATH_REFERENCE_RELATIVE :
                                                               PATH_REFERENCE_ABSOLUTE;
  }
  else if (mode == PATH_REFERENCE_COPY) {
    char filepath_cpy[PATH_MAX];
    BLI_path_join(filepath_cpy, PATH_MAX, base_dst.c_str(), BLI_path_basename(filepath_abs));
    copy_set->add(std::make_pair(filepath_abs, filepath_cpy));
    STRNCPY(filepath_abs, filepath_cpy);
    mode = PATH_REFERENCE_RELATIVE;
  }

  /* Now we know the final path mode. */
  if (mode == PATH_REFERENCE_ABSOLUTE) {
    return filepath_abs;
  }
  if (mode == PATH_REFERENCE_RELATIVE) {
    char rel_path[PATH_MAX];
    STRNCPY(rel_path, filepath_abs);
    BLI_path_rel(rel_path, base_dst.c_str());
    /* Can't always find relative path (e.g. between different drives). */
    if (!BLI_path_is_rel(rel_path)) {
      return filepath_abs;
    }
    return rel_path + 2; /* Skip blender's internal "//" prefix. */
  }
  if (mode == PATH_REFERENCE_STRIP) {
    return BLI_path_basename(filepath_abs);
  }
  BLI_assert_msg(false, "Invalid path reference mode");
  return filepath_abs;
}

void path_reference_copy(const Set<std::pair<std::string, std::string>> &copy_set)
{
  for (const auto &copy : copy_set) {
    const char *src = copy.first.c_str();
    const char *dst = copy.second.c_str();
    if (!BLI_exists(src)) {
      fprintf(stderr, "Missing source file '%s', not copying\n", src);
      continue;
    }
    if (0 == BLI_path_cmp_normalized(src, dst)) {
      continue; /* Source and destination are the same. */
    }
    if (!BLI_file_ensure_parent_dir_exists(dst)) {
      fprintf(stderr, "Can't make directory for '%s', not copying\n", dst);
      continue;
    }
    if (!BLI_copy(src, dst)) {
      fprintf(stderr, "Can't copy '%s' to '%s'\n", src, dst);
      continue;
    }
  }
}

}  // namespace blender::io
