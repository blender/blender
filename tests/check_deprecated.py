# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import os
from os.path import splitext

DEPRECATE_DAYS = 120

SKIP_DIRS = ("extern",
             "tests",  # not this dir
             )


def is_c_header(filename):
    ext = splitext(filename)[1]
    return (ext in {".h", ".hpp", ".hxx", ".hh"})


def is_c(filename):
    ext = splitext(filename)[1]
    return (ext in {".c", ".cpp", ".cxx", ".m", ".mm", ".rc", ".cc", ".inl"})


def is_c_any(filename):
    return is_c(filename) or is_c_header(filename)


def is_py(filename):
    ext = splitext(filename)[1]
    return (ext == ".py")


def is_source_any(filename):
    return is_c_any(filename) or is_py(filename)


def source_list(path, filename_check=None):
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]

        for filename in filenames:
            if filename_check is None or filename_check(filename):
                yield os.path.join(dirpath, filename)


def deprecations():
    """
    Searches out source code for lines like

    /* *DEPRECATED* 2011/7/17 bgl.Buffer.list info text */

    Or...

    # *DEPRECATED* 2010/12/22 some.py.func more info */

    """
    import datetime
    SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))))

    SKIP_DIRS_ABS = [os.path.join(SOURCE_DIR, p) for p in SKIP_DIRS]

    deprecations_ls = []

    scan_tot = 0

    print("scanning in %r for '*DEPRECATED* YYYY/MM/DD info'" % SOURCE_DIR)

    for fn in source_list(SOURCE_DIR, is_source_any):
        # print(fn)
        skip = False
        for p in SKIP_DIRS_ABS:
            if fn.startswith(p):
                skip = True
                break
        if skip:
            continue

        file = open(fn, 'r', encoding="utf8")
        for i, l in enumerate(file):
            # logic for deprecation warnings
            if '*DEPRECATED*' in l:
                try:
                    l = l.strip()
                    data = l.split('*DEPRECATED*', 1)[-1].strip().strip()
                    data = [w.strip() for w in data.split('/', 2)]
                    data[-1], info = data[-1].split(' ', 1)
                    info = info.split("*/", 1)[0]
                    if len(data) != 3:
                        print("    poorly formatting line:\n"
                              "    %r:%d\n"
                              "    %s" %
                              (fn, i + 1, l)
                              )
                    else:
                        data = datetime.datetime(*tuple([int(w) for w in data]))

                        deprecations_ls.append((data, (fn, i + 1), info))
                except:
                    print("Error file - %r:%d" % (fn, i + 1))
                    import traceback
                    traceback.print_exc()

        scan_tot += 1

    print("    scanned %d files" % scan_tot)

    return deprecations_ls


def main():
    import datetime
    now = datetime.datetime.now()

    deps = deprecations()

    print("\nAll deprecations...")
    for data, fileinfo, info in deps:
        days_old = (now - data).days
        if days_old > DEPRECATE_DAYS:
            info = "*** REMOVE! *** " + info
        print("   %r, days-old(%.2d), %s:%d - %s" % (data, days_old, fileinfo[0], fileinfo[1], info))
    if deps:
        print("\ndone!")
    else:
        print("\nnone found!")


if __name__ == '__main__':
    main()
