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
import subprocess
import shutil

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
#   python tests/pep8.py > test_pep8.log 2>&1

# how many lines to read into the file, pep8 comment
# should be directly after the license header, ~20 in most cases
PEP8_SEEK_COMMENT = 40
SKIP_PREFIX = "./tools", "./config", "./extern"
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


def check_files_flake8(files):
    print("\n\n\n# running flake8...")

    # these are very picky and often hard to follow
    # while keeping common script formatting.
    ignore = (
        "E122",
        "E123",
        "E124",
        "E125",
        "E126",
        "E127",
        "E128",
        # "imports not at top of file."
        # prefer to load as needed (lazy load addons etc).
        "E402",
        # "do not compare types, use 'isinstance()'"
        # times types are compared,
        # I rather keep them specific
        "E721",
    )

    for f, pep8_type in files:

        if pep8_type == 1:
            # E501:80 line length
            ignore_tmp = ignore + ("E501", )
        else:
            ignore_tmp = ignore

        subprocess.call((
            "flake8",
            "--isolated",
            "--ignore=%s" % ",".join(ignore_tmp),
            f,
        ))


def check_files_frosted(files):
    print("\n\n\n# running frosted...")
    for f, pep8_type in files:
        subprocess.call(("frosted", f))


def check_files_pylint(files):
    print("\n\n\n# running pylint...")
    for f, pep8_type in files:
        # let pep8 complain about line length
        subprocess.call((
            "pylint",
            "--disable="
            "C0111,"  # missing doc string
            "C0103,"  # invalid name
            "C0413,"  # import should be placed at the top
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
            "R0915,",  # Too many statements
            "--output-format=parseable",
            "--reports=n",
            "--max-line-length=1000",
            f,
        ))


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
    print("\n\n\n# checking imports...")
    import re
    import_check = re.compile(r"\s*from\s+[A-z\.]+\s+import \*\s*")
    for f, pep8_type in files:
        for i, l in enumerate(open(f, 'r', encoding='utf8')):
            if import_check.match(l):
                print("%s:%d:0: global import bad practice" % (f, i + 1))
    del re, import_check

    print("\n\n\n# checking class definitions...")
    import re
    class_check = re.compile(r"\s*class\s+.*\(\):.*")
    for f, pep8_type in files:
        for i, l in enumerate(open(f, 'r', encoding='utf8')):
            if class_check.match(l):
                print("%s:%d:0: empty class (), remove" % (f, i + 1))
    del re, class_check

    if shutil.which("flake8"):
        check_files_flake8(files)
    else:
        print("Skipping flake8 checks (command not found)")

    if shutil.which("frosted"):
        check_files_frosted(files)
    else:
        print("Skipping frosted checks (command not found)")

    if shutil.which("pylint"):
        check_files_pylint(files)
    else:
        print("Skipping pylint checks (command not found)")


if __name__ == "__main__":
    main()
