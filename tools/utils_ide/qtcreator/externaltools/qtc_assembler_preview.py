#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Takes 2 args

 qtc_assembler_preview.py <build_dir> <file.c/c++>

Currently GCC is assumed
"""


import sys
import os
import shlex
import subprocess

VERBOSE = os.environ.get("VERBOSE", False)
BUILD_DIR = sys.argv[-2]
SOURCE_FILE = sys.argv[-1]

# TODO, support other compilers
COMPILER_ID = 'GCC'


def find_arg(source, data):
    source_base = os.path.basename(source)
    for l in data:
        # chances are high that we found the file
        if source_base in l:
            # check if this file is in the line
            l_split = shlex.split(l)
            for w in l_split:
                if w.endswith(source_base):
                    if os.path.isabs(w):
                        if os.path.samefile(w, source):
                            # print(l)
                            return l
                    else:
                        # check trailing path (a/b/c/d/e.c == d/e.c)
                        w_sep = os.path.normpath(w).split(os.sep)
                        s_sep = os.path.normpath(source).split(os.sep)
                        m = min(len(w_sep), len(s_sep))
                        if w_sep[-m:] == s_sep[-m:]:
                            # print(l)
                            return l


def find_build_args_ninja(source):
    make_exe = "ninja"
    process = subprocess.Popen(
        [make_exe, "-t", "commands"],
        stdout=subprocess.PIPE,
        cwd=BUILD_DIR,
    )
    while process.poll():
        time.sleep(1)

    out = process.stdout.read()
    process.stdout.close()
    # print("done!", len(out), "bytes")
    data = out.decode("utf-8", errors="ignore").split("\n")
    return find_arg(source, data)


def find_build_args_make(source):
    make_exe = "make"
    process = subprocess.Popen(
        [make_exe, "--always-make", "--dry-run", "--keep-going", "VERBOSE=1"],
        stdout=subprocess.PIPE,
        cwd=BUILD_DIR,
    )
    while process.poll():
        time.sleep(1)

    out = process.stdout.read()
    process.stdout.close()

    # print("done!", len(out), "bytes")
    data = out.decode("utf-8", errors="ignore").split("\n")
    return find_arg(source, data)


def main():
    import re

    # currently only supports ninja or makefiles
    build_file_ninja = os.path.join(BUILD_DIR, "build.ninja")
    build_file_make = os.path.join(BUILD_DIR, "Makefile")
    if os.path.exists(build_file_ninja):
        if VERBOSE:
            print("Using Ninja")
        arg = find_build_args_ninja(SOURCE_FILE)
    elif os.path.exists(build_file_make):
        if VERBOSE:
            print("Using Make")
        arg = find_build_args_make(SOURCE_FILE)
    else:
        sys.stderr.write(f"Can't find Ninja or Makefile ({build_file_ninja!r} or {build_file_make!r}), aborting")
        return

    if arg is None:
        sys.stderr.write(f"Can't find file {SOURCE_FILE!r} in build command output of {BUILD_DIR!r}, aborting")
        return

    # now we need to get arg and modify it to produce assembler
    arg_split = shlex.split(arg)

    # get rid of: 'cd /a/b/c && ' prefix used by make (ninja doesn't need)
    try:
        i = arg_split.index("&&")
    except ValueError:
        i = -1
    if i != -1:
        del arg_split[:i + 1]

    if COMPILER_ID == 'GCC':
        # --- Switch debug for optimized ---
        for arg, n in (
                # regular flags which prevent asm output
                ("-o", 2),
                ("-MF", 2),
                ("-MT", 2),
                ("-MMD", 1),

                # debug flags
                ("-O0", 1),
                (re.compile(r"\-g\d*"), 1),
                (re.compile(r"\-ggdb\d*"), 1),
                ("-fno-inline", 1),
                ("-fno-builtin", 1),
                ("-fno-nonansi-builtins", 1),
                ("-fno-common", 1),
                ("-DDEBUG", 1), ("-D_DEBUG", 1),

                # ASAN flags.
                (re.compile(r"\-fsanitize=.*"), 1),
        ):
            if isinstance(arg, str):
                # exact string compare
                while arg in arg_split:
                    i = arg_split.index(arg)
                    del arg_split[i: i + n]
            else:
                # regex match
                for i in reversed(range(len(arg_split))):
                    if arg.match(arg_split[i]):
                        del arg_split[i: i + n]

        # add optimized args
        arg_split += ["-O3", "-fomit-frame-pointer", "-DNDEBUG", "-Wno-error"]

        # not essential but interesting to know
        arg_split += ["-ftree-vectorizer-verbose=1"]

        arg_split += ["-S"]
        if False:
            arg_split += ["-masm=intel"]  # Optional.
            arg_split += ["-fverbose-asm"]  # Optional but handy.
    else:
        sys.stderr.write(f"Compiler {COMPILER_ID!r} not supported")
        return

    source_asm = f"{SOURCE_FILE}.asm"

    # Never overwrite existing files
    i = 1
    while os.path.exists(source_asm):
        source_asm = f"{SOURCE_FILE}.asm.{i:d}"
        i += 1

    arg_split += ["-o", source_asm]

    # print("Executing:", arg_split)
    kwargs = {}
    if not VERBOSE:
        kwargs["stdout"] = subprocess.DEVNULL

    os.chdir(BUILD_DIR)
    subprocess.call(arg_split, **kwargs)

    del kwargs

    if not os.path.exists(source_asm):
        sys.stderr.write(f"Did not create {source_asm!r} from calling {arg_split!r}")
        return
    if VERBOSE:
        print(f"Running: {arg_split}")
    print(f"Created: {source_asm!r}")


if __name__ == "__main__":
    main()
