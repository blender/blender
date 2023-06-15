#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note: this code should be cleaned up / refactored.

import sys
if sys.version_info.major < 3:
    print("\nPython3.x needed, found %s.\nAborting!\n" %
          sys.version.partition(" ")[0])
    sys.exit(1)

import os
from os.path import (
    dirname,
    join,
    normpath,
    splitext,
)

from check_cmake_consistency_config import (
    IGNORE_SOURCE,
    IGNORE_SOURCE_MISSING,
    IGNORE_CMAKE,
    UTF8_CHECK,
    SOURCE_DIR,
    BUILD_DIR,
)

from typing import (
    Callable,
    Dict,
    Generator,
    Iterator,
    List,
    Optional,
    Tuple,
)


global_h = set()
global_c = set()
global_refs: Dict[str, List[Tuple[str, int]]] = {}

# Flatten `IGNORE_SOURCE_MISSING` to avoid nested looping.
IGNORE_SOURCE_MISSING_FLAT = [
    (k, ignore_path) for k, ig_list in IGNORE_SOURCE_MISSING
    for ignore_path in ig_list
]

# Ignore cmake file, path pairs.
global_ignore_source_missing: Dict[str, List[str]] = {}
for k, v in IGNORE_SOURCE_MISSING_FLAT:
    global_ignore_source_missing.setdefault(k, []).append(v)
del IGNORE_SOURCE_MISSING_FLAT


def replace_line(f: str, i: int, text: str, keep_indent: bool = True) -> None:
    file_handle = open(f, 'r')
    data = file_handle.readlines()
    file_handle.close()

    l = data[i]
    ws = l[:len(l) - len(l.lstrip())]

    data[i] = "%s%s\n" % (ws, text)

    file_handle = open(f, 'w')
    file_handle.writelines(data)
    file_handle.close()


def source_list(
        path: str,
        filename_check: Optional[Callable[[str], bool]] = None,
) -> Generator[str, None, None]:
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]

        for filename in filenames:
            if filename_check is None or filename_check(filename):
                yield os.path.join(dirpath, filename)


# extension checking
def is_cmake(filename: str) -> bool:
    ext = splitext(filename)[1]
    return (ext == ".cmake") or (filename == "CMakeLists.txt")


def is_c_header(filename: str) -> bool:
    ext = splitext(filename)[1]
    return (ext in {".h", ".hpp", ".hxx", ".hh"})


def is_c(filename: str) -> bool:
    ext = splitext(filename)[1]
    return (ext in {".c", ".cpp", ".cxx", ".m", ".mm", ".rc", ".cc", ".inl", ".metal", ".msl"})


def is_c_any(filename: str) -> bool:
    return is_c(filename) or is_c_header(filename)


def cmake_get_src(f: str) -> None:

    sources_h = []
    sources_c = []

    filen = open(f, "r", encoding="utf8")
    it: Optional[Iterator[str]] = iter(filen)
    found = False
    i = 0
    # print(f)

    def is_definition(l: str, f: str, i: int, name: str) -> Tuple[bool, int]:
        """
        Return (is_definition, single_line_offset).
        """
        if l.startswith("unset("):
            return False, -1

        single_line_offset = -1
        name_test = 'set(%s' % name
        single_line_offset = l.find(name_test)
        if (single_line_offset != -1) or ('set(' in l and l.endswith(name)):
            if single_line_offset != -1:
                single_line_offset += len(name_test)
            # if len(l.split()) > 1:
                # raise Exception("strict formatting not kept 'set(%s*' %s:%d" % (name, f, i))
            if l.endswith(")"):
                pass
                while single_line_offset < len(l) and l[single_line_offset] != " ":
                    single_line_offset += 1
            else:
                single_line_offset = -1
            return True, single_line_offset

        name_test = "list(APPEND %s" % name
        single_line_offset = l.find(name_test)
        if (single_line_offset != -1) or ('list(APPEND ' in l and l.endswith(name)):
            if single_line_offset != -1:
                single_line_offset += len(name_test)
            if l.endswith(")"):
                # raise Exception("strict formatting not kept 'list(APPEND %s...)' on 1 line %s:%d" % (name, f, i))
                pass
                while single_line_offset < len(l) and l[single_line_offset] != " ":
                    single_line_offset += 1
            else:
                single_line_offset = -1
            return True, single_line_offset
        return False, -1

    while it is not None:
        context_name = ""
        while it is not None:
            i += 1
            try:
                l = next(it)
            except StopIteration:
                it = None
                break
            l = l.strip()
            if not l.startswith("#"):
                for var in ("SRC", "INC"):
                    found, single_line_offset = is_definition(l, f, i, var)
                    if found:
                        context_name = var
                        break
                if found:
                    break

        if found:
            tokens = []
            if single_line_offset != -1:
                end = False
                for w in l[single_line_offset:].split():
                    if w.startswith("#"):
                        break
                    if w.endswith(")"):
                        w = w[:-1].rstrip()
                        end = True
                    tokens.append((w, i))
                    if end:
                        break
                del end
                if len(tokens) > 1:
                    print("Expect multi-variable to be split across multiple lines! '%s' %s:%d" % (l, f, i))
            else:
                while it is not None:
                    i += 1
                    try:
                        l = next(it)
                    except StopIteration:
                        it = None
                        break
                    l = l.strip()
                    if not l.startswith("#"):
                        # Remove in-line comments.
                        l = l.split(" # ")[0].rstrip()
                        if ")" in l:
                            if l.strip() != ")":
                                raise Exception("strict formatting not kept '*)' %s:%d" % (f, i))
                            break
                        tokens.append((l, i))

            cmake_base = dirname(f)
            cmake_base_bin = os.path.join(BUILD_DIR, os.path.relpath(cmake_base, SOURCE_DIR))

            # Find known missing sources list (if we have one).
            f_rel = os.path.relpath(f, SOURCE_DIR)
            f_rel_key = f_rel
            if os.sep != "/":
                f_rel_key = f_rel_key.replace(os.sep, "/")
            local_ignore_source_missing = global_ignore_source_missing.get(f_rel_key, [])

            for l, line_number in tokens:
                # Replace directories.
                l = l.replace("${CMAKE_SOURCE_DIR}", SOURCE_DIR)
                l = l.replace("${CMAKE_CURRENT_SOURCE_DIR}", cmake_base)
                l = l.replace("${CMAKE_CURRENT_BINARY_DIR}", cmake_base_bin)
                l = l.strip('"')

                if not l:
                    pass
                elif l in local_ignore_source_missing:
                    local_ignore_source_missing.remove(l)
                elif l.startswith("$"):
                    if context_name == "SRC":
                        # assume if it ends with context_name we know about it
                        if not l.split("}")[0].endswith(context_name):
                            print("Can't use var '%s' %s:%d" % (l, f, line_number))
                elif len(l.split()) > 1:
                    raise Exception("Multi-line define '%s' %s:%d" % (l, f, line_number))
                else:
                    new_file = normpath(join(cmake_base, l))

                    if context_name == "SRC":
                        if is_c_header(new_file):
                            sources_h.append(new_file)
                            global_refs.setdefault(new_file, []).append((f, line_number))
                        elif is_c(new_file):
                            sources_c.append(new_file)
                            global_refs.setdefault(new_file, []).append((f, line_number))
                        elif l in {"PARENT_SCOPE", }:
                            # cmake var, ignore
                            pass
                        elif new_file.endswith(".list"):
                            pass
                        elif new_file.endswith(".def"):
                            pass
                        elif new_file.endswith(".cl"):  # OPENCL.
                            pass
                        elif new_file.endswith(".cu"):  # CUDA.
                            pass
                        elif new_file.endswith(".osl"):  # open shading language.
                            pass
                        elif new_file.endswith(".glsl"):
                            pass
                        else:
                            raise Exception("unknown file type - not c or h %s -> %s" % (f, new_file))

                    elif context_name == "INC":
                        if new_file.startswith(BUILD_DIR):
                            # assume generated path
                            pass
                        elif os.path.isdir(new_file):
                            new_path_rel = os.path.relpath(new_file, cmake_base)

                            if new_path_rel != l:
                                print("overly relative path:\n  %s:%d\n  %s\n  %s" % (f, line_number, l, new_path_rel))

                                # # Save time. just replace the line
                                # replace_line(f, line_number - 1, new_path_rel)

                        else:
                            raise Exception("non existent include %s:%d -> %s" % (f, line_number, new_file))

                    # print(new_file)

            global_h.update(set(sources_h))
            global_c.update(set(sources_c))
            '''
            if not sources_h and not sources_c:
                raise Exception("No sources %s" % f)

            sources_h_fs = list(source_list(cmake_base, is_c_header))
            sources_c_fs = list(source_list(cmake_base, is_c))
            '''
            # find missing C files:
            '''
            for ff in sources_c_fs:
                if ff not in sources_c:
                    print("  missing: " + ff)
            '''

            # reset
            del sources_h[:]
            del sources_c[:]

    filen.close()


def is_ignore_source(f: str, ignore_used: List[bool]) -> bool:
    for index, ignore_path in enumerate(IGNORE_SOURCE):
        if ignore_path in f:
            ignore_used[index] = True
            return True
    return False


def is_ignore_cmake(f: str, ignore_used: List[bool]) -> bool:
    for index, ignore_path in enumerate(IGNORE_CMAKE):
        if ignore_path in f:
            ignore_used[index] = True
            return True
    return False


def main() -> None:

    print("Scanning:", SOURCE_DIR)

    ignore_used_source = [False] * len(IGNORE_SOURCE)
    ignore_used_cmake = [False] * len(IGNORE_CMAKE)

    for cmake in source_list(SOURCE_DIR, is_cmake):
        if not is_ignore_cmake(cmake, ignore_used_cmake):
            cmake_get_src(cmake)

    # First do stupid check, do these files exist?
    print("\nChecking for missing references:")
    is_err = False
    errs = []
    for f in (global_h | global_c):
        if f.startswith(BUILD_DIR):
            continue

        if not os.path.exists(f):
            refs = global_refs[f]
            if refs:
                for cf, i in refs:
                    errs.append((cf, i))
            else:
                raise Exception("CMake references missing, internal error, aborting!")
            is_err = True

    errs.sort()
    errs.reverse()
    for cf, i in errs:
        print("%s:%d" % (cf, i))
        # Write a 'sed' script, useful if we get a lot of these
        # print("sed '%dd' '%s' > '%s.tmp' ; mv '%s.tmp' '%s'" % (i, cf, cf, cf, cf))

    if is_err:
        raise Exception("CMake references missing files, aborting!")
    del is_err
    del errs

    # now check on files not accounted for.
    print("\nC/C++ Files CMake does not know about...")
    for cf in sorted(source_list(SOURCE_DIR, is_c)):
        if not is_ignore_source(cf, ignore_used_source):
            if cf not in global_c:
                print("missing_c: ", cf)

            # Check if automake builds a corresponding .o file.
            '''
            if cf in global_c:
                out1 = os.path.splitext(cf)[0] + ".o"
                out2 = os.path.splitext(cf)[0] + ".Po"
                out2_dir, out2_file = out2 = os.path.split(out2)
                out2 = os.path.join(out2_dir, ".deps", out2_file)
                if not os.path.exists(out1) and not os.path.exists(out2):
                    print("bad_c: ", cf)
            '''

    print("\nC/C++ Headers CMake does not know about...")
    for hf in sorted(source_list(SOURCE_DIR, is_c_header)):
        if not is_ignore_source(hf, ignore_used_source):
            if hf not in global_h:
                print("missing_h: ", hf)

    if UTF8_CHECK:
        # test encoding
        import traceback
        for files in (global_c, global_h):
            for f in sorted(files):
                if os.path.exists(f):
                    # ignore outside of our source tree
                    if "extern" not in f:
                        i = 1
                        try:
                            for _ in open(f, "r", encoding="utf8"):
                                i += 1
                        except UnicodeDecodeError:
                            print("Non utf8: %s:%d" % (f, i))
                            if i > 1:
                                traceback.print_exc()

    # Check ignores aren't stale
    print("\nCheck for unused 'IGNORE_SOURCE' paths...")
    for index, ignore_path in enumerate(IGNORE_SOURCE):
        if not ignore_used_source[index]:
            print("unused ignore: %r" % ignore_path)

    # Check ignores aren't stale
    print("\nCheck for unused 'IGNORE_SOURCE_MISSING' paths...")
    for k, v in sorted(global_ignore_source_missing.items()):
        for ignore_path in v:
            print("unused ignore: %r -> %r" % (ignore_path, k))

    # Check ignores aren't stale
    print("\nCheck for unused 'IGNORE_CMAKE' paths...")
    for index, ignore_path in enumerate(IGNORE_CMAKE):
        if not ignore_used_cmake[index]:
            print("unused ignore: %r" % ignore_path)


if __name__ == "__main__":
    main()
