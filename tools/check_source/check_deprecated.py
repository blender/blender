# SPDX-License-Identifier: GPL-2.0-or-later

"""
Utility for reporting deprecated code which should be removed,
noted by the date which must be included with the *DEPRECATED* comment.

Once this date is past, the code should be removed.
"""

from typing import (
    Callable,
    Generator,
    List,
    Tuple,
    Optional,
)


import os
import datetime
from os.path import splitext

SKIP_DIRS = (
    "extern",
    # Not this directory.
    "tests",
)


class term_colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def is_c_header(filename: str) -> bool:
    ext = splitext(filename)[1]
    return (ext in {".h", ".hh", ".hpp", ".hxx", ".hh"})


def is_c(filename: str) -> bool:
    ext = splitext(filename)[1]
    return (ext in {".c", ".cc", ".cpp", ".cxx", ".m", ".mm", ".rc", ".inl"})


def is_c_any(filename: str) -> bool:
    return is_c(filename) or is_c_header(filename)


def is_py(filename: str) -> bool:
    ext = splitext(filename)[1]
    return (ext == ".py")


def is_source_any(filename: str) -> bool:
    return is_c_any(filename) or is_py(filename)


def source_list(path: str, filename_check: Optional[Callable[[str], bool]] = None) -> Generator[str, None, None]:
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]

        for filename in filenames:
            if filename_check is None or filename_check(filename):
                yield os.path.join(dirpath, filename)


def deprecations() -> List[Tuple[datetime.datetime, Tuple[str, int], str]]:
    """
    Searches out source code for lines like

    /* *DEPRECATED* 2011/7/17 ``bgl.Buffer.list`` info text. */

    Or...

    # *DEPRECATED* 2010/12/22 ``some.py.func`` more info.

    """
    SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..")))

    SKIP_DIRS_ABS = [os.path.join(SOURCE_DIR, p) for p in SKIP_DIRS]

    DEPRECATED_ID = "*DEPRECATED*"
    depprecation_list = []

    scan_count = 0

    print("Scanning in %r for '%s YYYY/MM/DD info'" % (SOURCE_DIR, DEPRECATED_ID), end="...")

    for fn in source_list(SOURCE_DIR, is_source_any):
        if os.path.samefile(fn, __file__):
            continue

        skip = False
        for p in SKIP_DIRS_ABS:
            if fn.startswith(p):
                skip = True
                break
        if skip:
            continue

        with open(fn, 'r', encoding="utf8") as fh:
            fn = os.path.relpath(fn, SOURCE_DIR)
            buf = fh.read()
            index = 0
            while True:
                index = buf.find(DEPRECATED_ID, index)
                if index == -1:
                    break
                index_end = buf.find("\n", index)
                if index_end == -1:
                    index_end = len(buf)
                line_number = buf[:index].count("\n") + 1
                l = buf[index + len(DEPRECATED_ID): index_end].strip()
                try:
                    data = [w.strip() for w in l.split('/', 2)]
                    data[-1], info = data[-1].split(' ', 1)
                    info = info.split("*/", 1)[0].strip()
                    if len(data) != 3:
                        print(
                            "    poorly formatting line:\n"
                            "    %r:%d\n"
                            "    %s" %
                            (fn, line_number, data)
                        )
                    else:
                        depprecation_list.append((
                            datetime.datetime(int(data[0]), int(data[1]), int(data[2])),
                            (fn, line_number),
                            info,
                        ))
                except:
                    print("Error file - %r:%d" % (fn, line_number))
                    import traceback
                    traceback.print_exc()

                index = index_end

        scan_count += 1

    print(" {:d} files done, found {:d} deprecation(s)!".format(scan_count, len(depprecation_list)))

    return depprecation_list


def main() -> None:
    import datetime
    now = datetime.datetime.now()

    deps = deprecations()

    for data, fileinfo, info in deps:
        days_old = (now - data).days
        info = term_colors.BOLD + info + term_colors.ENDC
        if days_old > 0:
            info = "[" + term_colors.FAIL + "REMOVE" + term_colors.ENDC + "] " + info
        else:
            info = "[" + term_colors.OKBLUE + "OK" + term_colors.ENDC + "] " + info

        print("{:s}: days-old({:d}), {:s}:{:d} {:s}".format(
            data.strftime("%Y/%m/%d"),
            days_old,
            fileinfo[0],
            fileinfo[1],
            info,
        ))


if __name__ == '__main__':
    main()
