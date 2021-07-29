#!/usr/bin/env python3

# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

"""
A simply utility to copy blend files and their deps to a new location.

Similar to packing, but don't attempt any path remapping.
"""

from bam.blend import blendfile_path_walker

TIMEIT = False

# ------------------
# Ensure module path
import os
import sys
path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "modules"))
if path not in sys.path:
    sys.path.append(path)
del os, sys, path
# --------


def copy_paths(
        paths,
        output,
        base,

        # load every libs dep, not just used deps.
        all_deps=False,
        # yield reports
        report=None,

        # Filename filter, allow to exclude files from the pack,
        # function takes a string returns True if the files should be included.
        filename_filter=None,
        ):

    import os
    import shutil

    from bam.utils.system import colorize, is_subdir

    path_copy_files = set(paths)

    # Avoid walking over same libs many times
    lib_visit = {}

    yield report("Reading %d blend file(s)\n" % len(paths))
    for blendfile_src in paths:
        yield report("  %s:     %r\n" % (colorize("blend", color='blue'), blendfile_src))
        for fp, (rootdir, fp_blend_basename) in blendfile_path_walker.FilePath.visit_from_blend(
                blendfile_src,
                readonly=True,
                recursive=True,
                recursive_all=all_deps,
                lib_visit=lib_visit,
                ):

            f_abs = os.path.normpath(fp.filepath_absolute)
            path_copy_files.add(f_abs)

    # Source -> Dest Map
    path_src_dst_map = {}

    for path_src in sorted(path_copy_files):

        if filename_filter and not filename_filter(path_src):
            yield report("  %s:     %r\n" % (colorize("exclude", color='yellow'), path_src))
            continue

        if not os.path.exists(path_src):
            yield report("  %s:     %r\n" % (colorize("missing path", color='red'), path_src))
            continue

        if not is_subdir(path_src, base):
            yield report("  %s:     %r\n" % (colorize("external path ignored", color='red'), path_src))
            continue

        path_rel = os.path.relpath(path_src, base)
        path_dst = os.path.join(output, path_rel)

        path_src_dst_map[path_src] = path_dst

    # Create directories
    path_dst_dir = {os.path.dirname(path_dst) for path_dst in path_src_dst_map.values()}
    yield report("Creating %d directories in %r\n" % (len(path_dst_dir), output))
    for path_dir in sorted(path_dst_dir):
        os.makedirs(path_dir, exist_ok=True)
    del path_dst_dir

    # Copy files
    yield report("Copying %d files to %r\n" % (len(path_src_dst_map), output))
    for path_src, path_dst in sorted(path_src_dst_map.items()):
        yield report("  %s:     %r -> %r\n" % (colorize("copying", color='blue'), path_src, path_dst))
        shutil.copy(path_src, path_dst)
