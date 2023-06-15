#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Run this script to check if headers are included multiple times.

    python3 check_header_duplicate.py ../../

Now build the code to find duplicate errors, resolve them manually.

Then restore the headers to their original state:

    python3 check_header_duplicate.py --restore ../../
"""

# Use GCC's __INCLUDE_LEVEL__ to find direct duplicate includes

UUID = 0


def source_filepath_guard(filepath):
    global UUID

    footer = """
#if __INCLUDE_LEVEL__ == 1
#  ifdef _DOUBLEHEADERGUARD_%d
#    error "duplicate header!"
#  endif
#endif

#if __INCLUDE_LEVEL__ == 1
#  define _DOUBLEHEADERGUARD_%d
#endif
""" % (UUID, UUID)
    UUID += 1

    with open(filepath, 'a', encoding='utf-8') as f:
        f.write(footer)


def source_filepath_restore(filepath):
    import os
    os.system("git co %s" % filepath)


def scan_source_recursive(dirpath, is_restore):
    import os
    from os.path import join, splitext

    # ensure git working dir is ok
    os.chdir(dirpath)

    def source_list(path, filename_check=None):
        for dirpath, dirnames, filenames in os.walk(path):
            # skip '.git'
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]

            for filename in filenames:
                filepath = join(dirpath, filename)
                if filename_check is None or filename_check(filepath):
                    yield filepath

    def is_source(filename):
        ext = splitext(filename)[1]
        return (ext in {".hpp", ".hxx", ".h", ".hh"})

    def is_ignore(filename):
        pass

    for filepath in sorted(source_list(dirpath, is_source)):
        print("file:", filepath)
        if is_ignore(filepath):
            continue

        if is_restore:
            source_filepath_restore(filepath)
        else:
            source_filepath_guard(filepath)


def main():
    import sys
    is_restore = ("--restore" in sys.argv[1:])
    scan_source_recursive(sys.argv[-1], is_restore)


if __name__ == "__main__":
    main()
