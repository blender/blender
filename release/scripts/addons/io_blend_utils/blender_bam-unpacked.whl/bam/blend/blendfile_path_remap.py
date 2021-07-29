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
Module for remapping paths from one directory to another.
"""

import os


# ----------------------------------------------------------------------------
# private utility functions

def _is_blend(f):
    return f.lower().endswith(b'.blend')


def _warn__ascii(msg):
    print("  warning: %s" % msg)


def _info__ascii(msg):
    print(msg)


def _warn__json(msg):
    import json
    print(json.dumps(("warning", msg)), end=",\n")

def _info__json(msg):
    import json
    print(json.dumps(("info", msg)), end=",\n")


def _uuid_from_file(fn, block_size=1 << 20):
    with open(fn, 'rb') as f:
        # first get the size
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.seek(0, os.SEEK_SET)
        # done!

        import hashlib
        sha1 = hashlib.new('sha512')
        while True:
            data = f.read(block_size)
            if not data:
                break
            sha1.update(data)
        return (hex(size)[2:] + sha1.hexdigest()).encode()


def _iter_files(paths, check_ext=None):
    # note, sorting isn't needed
    # just gives predictable output
    for p in paths:
        p = os.path.abspath(p)
        for dirpath, dirnames, filenames in sorted(os.walk(p)):
            # skip '.svn'
            if dirpath.startswith(b'.') and dirpath != b'.':
                continue

            for filename in sorted(filenames):
                if check_ext is None or check_ext(filename):
                    filepath = os.path.join(dirpath, filename)
                    yield filepath


# ----------------------------------------------------------------------------
# Public Functions

def start(
        paths,
        is_quiet=False,
        dry_run=False,
        use_json=False,
        ):

    if use_json:
        warn = _warn__json
        info = _info__json
    else:
        warn = _warn__ascii
        info = _info__ascii

    if use_json:
        print("[")

    # {(sha1, length): "filepath"}
    remap_uuid = {}

    # relative paths which don't exist,
    # don't complain when they're missing on remap.
    # {f_src: [relative path deps, ...]}
    remap_lost = {}

    # all files we need to map
    # absolute paths
    files_to_map = set()

    # TODO, validate paths aren't nested! ["/foo", "/foo/bar"]
    # it will cause problems touching files twice!

    # ------------------------------------------------------------------------
    # First walk over all blends
    from bam.blend import blendfile_path_walker

    for blendfile_src in _iter_files(paths, check_ext=_is_blend):
        if not is_quiet:
            info("blend read: %r" % blendfile_src)

        remap_lost[blendfile_src] = remap_lost_blendfile_src = set()

        for fp, (rootdir, fp_blend_basename) in blendfile_path_walker.FilePath.visit_from_blend(
                blendfile_src,
                readonly=True,
                recursive=False,
                ):
            # TODO. warn when referencing files outside 'paths'

            # so we can update the reference
            f_abs = fp.filepath_absolute
            f_abs = os.path.normpath(f_abs)
            if os.path.exists(f_abs):
                files_to_map.add(f_abs)
            else:
                if not is_quiet:
                    warn("file %r not found!" % f_abs)

                # don't complain about this file being missing on remap
                remap_lost_blendfile_src.add(fp.filepath)

        # so we can know where its moved to
        files_to_map.add(blendfile_src)
    del blendfile_path_walker

    # ------------------------------------------------------------------------
    # Store UUID
    #
    # note, sorting is only to give predictable warnings/behavior
    for f in sorted(files_to_map):
        f_uuid = _uuid_from_file(f)

        f_match = remap_uuid.get(f_uuid)
        if f_match is not None:
            if not is_quiet:
                warn("duplicate file found! (%r, %r)" % (f_match, f))

        remap_uuid[f_uuid] = f

    # now find all deps
    remap_data_args = (
            remap_uuid,
            remap_lost,
            )

    if use_json:
        if not remap_uuid:
            print("\"nothing to remap!\"")
        else:
            print("\"complete\"")
        print("]")
    else:
        if not remap_uuid:
            print("Nothing to remap!")

    return remap_data_args


def finish(
        paths, remap_data_args,
        is_quiet=False,
        force_relative=False,
        dry_run=False,
        use_json=False,
        ):

    if use_json:
        warn = _warn__json
        info = _info__json
    else:
        warn = _warn__ascii
        info = _info__ascii

    if use_json:
        print("[")

    (remap_uuid,
     remap_lost,
     ) = remap_data_args

    remap_src_to_dst = {}
    remap_dst_to_src = {}

    for f_dst in _iter_files(paths):
        f_uuid = _uuid_from_file(f_dst)
        f_src = remap_uuid.get(f_uuid)
        if f_src is not None:
            remap_src_to_dst[f_src] = f_dst
            remap_dst_to_src[f_dst] = f_src

    # now the fun begins, remap _all_ paths
    from bam.blend import blendfile_path_walker

    for blendfile_dst in _iter_files(paths, check_ext=_is_blend):
        blendfile_src = remap_dst_to_src.get(blendfile_dst)
        if blendfile_src is None:
            if not is_quiet:
                warn("new blendfile added since beginning 'remap': %r" % blendfile_dst)
            continue

        # not essential, just so we can give more meaningful errors
        remap_lost_blendfile_src = remap_lost[blendfile_src]

        if not is_quiet:
            info("blend write: %r -> %r" % (blendfile_src, blendfile_dst))

        blendfile_src_basedir = os.path.dirname(blendfile_src)
        blendfile_dst_basedir = os.path.dirname(blendfile_dst)
        for fp, (rootdir, fp_blend_basename) in blendfile_path_walker.FilePath.visit_from_blend(
                blendfile_dst,
                readonly=False,
                recursive=False,
                ):
            # TODO. warn when referencing files outside 'paths'

            # so we can update the reference
            f_src_orig = fp.filepath

            if f_src_orig in remap_lost_blendfile_src:
                # this file never existed, so we can't remap it
                continue

            is_relative = f_src_orig.startswith(b'//')
            if is_relative:
                f_src_abs = fp.filepath_absolute_resolve(basedir=blendfile_src_basedir)
            else:
                f_src_abs = f_src_orig

            f_src_abs = os.path.normpath(f_src_abs)
            f_dst_abs = remap_src_to_dst.get(f_src_abs)

            if f_dst_abs is None:
                if not is_quiet:
                    warn("file %r not found in map!" % f_src_abs)
                continue

            # now remap!
            if is_relative or force_relative:
                f_dst_final = b'//' + os.path.relpath(f_dst_abs, blendfile_dst_basedir)
            else:
                f_dst_final = f_dst_abs

            if f_dst_final != f_src_orig:
                if not dry_run:
                    fp.filepath = f_dst_final
                if not is_quiet:
                    info("remap %r -> %r" % (f_src_abs, f_dst_abs))

    del blendfile_path_walker

    if use_json:
        print("\"complete\"\n]")
