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
This script takes Blend-File and remaps their paths to the original locations.

(needed for uploading to the server)
"""

VERBOSE = 1

from bam.blend import blendfile_path_walker


def blendfile_remap(
        blendfile_src, blendpath_dst,
        deps_remap=None, deps_remap_cb=None,
        deps_remap_cb_userdata=None,
        ):
    import os

    def temp_remap_cb(filepath, level):
        """
        Simply point to the output dir.
        """
        basename = os.path.basename(blendfile_src)
        filepath_tmp = os.path.join(blendpath_dst, basename)

        # ideally we could avoid copying _ALL_ blends
        # TODO(cam)
        import shutil
        shutil.copy(filepath, filepath_tmp)

        return filepath_tmp

    for fp, (rootdir, fp_blend_basename) in blendfile_path_walker.FilePath.visit_from_blend(
            blendfile_src,
            readonly=False,
            temp_remap_cb=temp_remap_cb,
            recursive=False,
            ):

        # path_dst_final - current path in blend.
        # path_src_orig - original path from JSON.

        path_dst_final_b = fp.filepath

        # support 2 modes, callback or dictionary
        if deps_remap_cb is not None:
            path_src_orig = deps_remap_cb(path_dst_final_b, deps_remap_cb_userdata)
            if path_src_orig is not None:
                fp.filepath = path_src_orig
                if VERBOSE:
                    print("  Remapping:", path_dst_final_b, "->", path_src_orig)
        else:
            path_dst_final = path_dst_final_b.decode('utf-8')
            path_src_orig = deps_remap.get(path_dst_final)
            if path_src_orig is not None:
                fp.filepath = path_src_orig.encode('utf-8')
                if VERBOSE:
                    print("  Remapping:", path_dst_final, "->", path_src_orig)


def pack_restore(blendfile_dir_src, blendfile_dir_dst, pathmap):
    import os

    for dirpath, dirnames, filenames in os.walk(blendfile_dir_src):
        if dirpath.startswith(b"."):
            continue

        for filename in filenames:
            if os.path.splitext(filename)[1].lower() == b".blend":
                remap = pathmap.get(filename.decode('utf-8'))
                if remap is not None:
                    filepath = os.path.join(dirpath, filename)

                    # main function call
                    blendfile_remap(filepath, blendfile_dir_dst, remap)


def create_argparse():
    import os
    import argparse

    usage_text = (
        "Run this script to remap blend-file(s) paths using a JSON file created by 'packer.py':" +
        os.path.basename(__file__) +
        "--input=DIR --remap=JSON [options]")

    parser = argparse.ArgumentParser(description=usage_text)

    # for main_render() only, but validate args.
    parser.add_argument(
            "-i", "--input", dest="path_src", metavar='DIR', required=True,
            help="Input path(s) or a wildcard to glob many files")
    parser.add_argument(
            "-o", "--output", dest="path_dst", metavar='DIR', required=True,
            help="Output directory ")
    parser.add_argument(
            "-r", "--deps_remap", dest="deps_remap", metavar='JSON', required=True,
            help="JSON file containing the path remapping info")

    return parser


def main():
    import sys
    import json

    parser = create_argparse()
    args = parser.parse_args(sys.argv[1:])

    encoding = sys.getfilesystemencoding()

    with open(args.deps_remap, 'r', encoding='utf-8') as f:
        pathmap = json.load(f)

    pack_restore(
            args.path_src.encode(encoding),
            args.path_dst.encode(encoding),
            pathmap,
            )


if __name__ == "__main__":
    main()
