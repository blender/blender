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

# <pep8-80 compliant>

import os

# depends on pep8, frosted, pylint
# for Ubuntu
#
#   sudo apt-get install pylint
#
#   sudo apt-get install python-setuptools python-pip
#   sudo pip install pep8
#   sudo pip install frosted
#
# in Debian install pylint pep8 with apt-get/aptitude/etc
#
# on *nix run
#   python source/tests/pep8.py > test_pep8.log 2>&1

# how many lines to read into the file, pep8 comment
# should be directly after the license header, ~20 in most cases
PEP8_SEEK_COMMENT = 40
SKIP_PREFIX = "./tools", "./config", "./scons", "./extern"
SKIP_ADDONS = True
FORCE_PEP8_ALL = False


def file_list_py(path):
    for dirpath, dirnames, filenames in os.walk(path):
        for filename in filenames:
            if filename.endswith((".py", ".cfg")):
                yield os.path.join(dirpath, filename)


def is_pep8(path):
    print(path)
    if open(path, 'rb').read(3) == b'\xef\xbb\xbf':
        print("\nfile contains BOM, remove first 3 bytes: %r\n" % path)

    # templates don't have a header but should be pep8
    for d in ("presets", "templates_py", "examples"):
        if ("%s%s%s" % (os.sep, d, os.sep)) in path:
            return 1

    f = open(path, 'r', encoding="utf8")
    for i in range(PEP8_SEEK_COMMENT):
        line = f.readline()
        if line.startswith("# <pep8"):
            if line.startswith("# <pep8 compliant>"):
                return 1
            elif line.startswith("# <pep8-80 compliant>"):
                return 2
    f.close()
    return 0


def main():
    files = []
    files_skip = []
    for f in file_list_py("."):
        if [None for prefix in SKIP_PREFIX if f.startswith(prefix)]:
            continue

        if SKIP_ADDONS:
            if (os.sep + "addons") in f:
                continue

        pep8_type = FORCE_PEP8_ALL or is_pep8(f)

        if pep8_type:
            # so we can batch them for each tool.
            files.append((os.path.abspath(f), pep8_type))
        else:
            files_skip.append(f)

    print("\nSkipping...")
    for f in files_skip:
        print("    %s" % f)

    # strict imports
    print("\n\n\n# running pep8...")
    import re
    import_check = re.compile(r"\s*from\s+[A-z\.]+\s+import \*\s*")
    for f, pep8_type in files:
        for i, l in enumerate(open(f, 'r', encoding='utf8')):
            if import_check.match(l):
                print("%s:%d:0: global import bad practice" % (f, i + 1))

    print("\n\n\n# running pep8...")

    # these are very picky and often hard to follow
    # while keeping common script formatting.
    ignore = "E122", "E123", "E124", "E125", "E126", "E127", "E128"

    for f, pep8_type in files:

        if pep8_type == 1:
            # E501:80 line length
            ignore_tmp = ignore + ("E501", )
        else:
            ignore_tmp = ignore

        os.system("pep8 --repeat --ignore=%s '%s'" % (",".join(ignore_tmp), f))

    # frosted
    print("\n\n\n# running frosted...")
    for f, pep8_type in files:
        os.system("frosted '%s'" % f)

    print("\n\n\n# running pylint...")
    for f, pep8_type in files:
        # let pep8 complain about line length
        os.system("pylint "
                  "--disable="
                  "C0111,"  # missing doc string
                  "C0103,"  # invalid name
                  "W0613,"  # unused argument, may add this back
                            # but happens a lot for 'context' for eg.
                  "W0232,"  # class has no __init__, Operator/Panel/Menu etc
                  "W0142,"  # Used * or ** magic
                            # even needed in some cases
                  "R0902,"  # Too many instance attributes
                  "R0903,"  # Too many statements
                  "R0911,"  # Too many return statements
                  "R0912,"  # Too many branches
                  "R0913,"  # Too many arguments
                  "R0914,"  # Too many local variables
                  "R0915,"  # Too many statements
                  " "
                  "--include-ids=y "
                  "--output-format=parseable "
                  "--reports=n "
                  "--max-line-length=1000"
                  " '%s'" % f)

if __name__ == "__main__":
    main()
