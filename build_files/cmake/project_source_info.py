# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "build_info",
    "SOURCE_DIR",
)


import sys
if sys.version_info.major < 3:
    print("\nPython3.x or newer needed, found %s.\nAborting!\n" %
          sys.version.partition(" ")[0])
    sys.exit(1)


import os
from os.path import join, dirname, normpath, abspath

import subprocess

from typing import (
    Any,
    Callable,
    Generator,
    List,
    Optional,
    Sequence,
    Tuple,
    Union,
    cast,
)

import shlex


SOURCE_DIR = join(dirname(__file__), "..", "..")
SOURCE_DIR = normpath(SOURCE_DIR)
SOURCE_DIR = abspath(SOURCE_DIR)


def is_c_header(filename: str) -> bool:
    ext = os.path.splitext(filename)[1]
    return (ext in {".h", ".hpp", ".hxx", ".hh"})


def is_c(filename: str) -> bool:
    ext = os.path.splitext(filename)[1]
    return (ext in {".c", ".cpp", ".cxx", ".m", ".mm", ".rc", ".cc", ".inl", ".osl"})


def is_c_any(filename: str) -> bool:
    return is_c(filename) or is_c_header(filename)


# copied from project_info.py
CMAKE_DIR = "."


def cmake_cache_var_iter() -> Generator[Tuple[str, str, str], None, None]:
    import re
    re_cache = re.compile(r'([A-Za-z0-9_\-]+)?:?([A-Za-z0-9_\-]+)?=(.*)$')
    with open(join(CMAKE_DIR, "CMakeCache.txt"), 'r', encoding='utf-8') as cache_file:
        for l in cache_file:
            match = re_cache.match(l.strip())
            if match is not None:
                var, type_, val = match.groups()
                yield (var, type_ or "", val)


def cmake_cache_var(var: str) -> Optional[str]:
    for var_iter, type_iter, value_iter in cmake_cache_var_iter():
        if var == var_iter:
            return value_iter
    return None


def cmake_cache_var_or_exit(var: str) -> str:
    value = cmake_cache_var(var)
    if value is None:
        print("Unable to find %r exiting!" % value)
        sys.exit(1)
    return value


def do_ignore(filepath: str, ignore_prefix_list: Optional[Sequence[str]]) -> bool:
    if ignore_prefix_list is None:
        return False

    relpath = os.path.relpath(filepath, SOURCE_DIR)
    return any([relpath.startswith(prefix) for prefix in ignore_prefix_list])


def makefile_log() -> List[str]:
    import subprocess
    import time

    # support both make and ninja
    make_exe = cmake_cache_var_or_exit("CMAKE_MAKE_PROGRAM")

    make_exe_basename = os.path.basename(make_exe)

    if make_exe_basename.startswith(("make", "gmake")):
        print("running 'make' with --dry-run ...")
        process = subprocess.Popen([make_exe, "--always-make", "--dry-run", "--keep-going", "VERBOSE=1"],
                                   stdout=subprocess.PIPE,
                                   )
    elif make_exe_basename.startswith("ninja"):
        print("running 'ninja' with -t commands ...")
        process = subprocess.Popen([make_exe, "-t", "commands"],
                                   stdout=subprocess.PIPE,
                                   )

    if process is None:
        print("Can't execute process")
        sys.exit(1)

    while process.poll():
        time.sleep(1)

    # We know this is always true based on the input arguments to `Popen`.
    stdout: IO[bytes] = process.stdout  # type: ignore

    out = stdout.read()
    stdout.close()
    print("done!", len(out), "bytes")
    return cast(List[str], out.decode("utf-8", errors="ignore").split("\n"))


def build_info(
        use_c: bool = True,
        use_cxx: bool = True,
        ignore_prefix_list: Optional[List[str]] = None,
) -> List[Tuple[str, List[str], List[str]]]:
    makelog = makefile_log()

    source = []

    compilers = []
    if use_c:
        compilers.append(cmake_cache_var_or_exit("CMAKE_C_COMPILER"))
    if use_cxx:
        compilers.append(cmake_cache_var_or_exit("CMAKE_CXX_COMPILER"))

    print("compilers:", " ".join(compilers))

    fake_compiler = "%COMPILER%"

    print("parsing make log ...")

    for line in makelog:

        args: Union[str, List[str]] = line.split()

        if not any([(c in args) for c in compilers]):
            continue

        # join args incase they are not.
        args = ' '.join(args)
        args = args.replace(" -isystem", " -I")
        args = args.replace(" -D ", " -D")
        args = args.replace(" -I ", " -I")

        for c in compilers:
            args = args.replace(c, fake_compiler)
        args = shlex.split(args)
        # end

        # remove compiler
        args[:args.index(fake_compiler) + 1] = []

        c_files = [f for f in args if is_c(f)]
        inc_dirs = [f[2:].strip() for f in args if f.startswith('-I')]
        defs = [f[2:].strip() for f in args if f.startswith('-D')]
        for c in sorted(c_files):

            if do_ignore(c, ignore_prefix_list):
                continue

            source.append((c, inc_dirs, defs))

        # make relative includes absolute
        # not totally essential but useful
        for i, f in enumerate(inc_dirs):
            if not os.path.isabs(f):
                inc_dirs[i] = os.path.abspath(os.path.join(CMAKE_DIR, f))

        # safety check that our includes are ok
        for f in inc_dirs:
            if not os.path.exists(f):
                raise Exception("%s missing" % f)

    print("done!")

    return source


def build_defines_as_source() -> str:
    """
    Returns a string formatted as an include:
        '#defines A=B\n#define....'
    """
    import subprocess
    # works for both gcc and clang
    cmd = (cmake_cache_var_or_exit("CMAKE_C_COMPILER"), "-dM", "-E", "-")
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stdin=subprocess.DEVNULL,
    )

    # We know this is always true based on the input arguments to `Popen`.
    stdout: IO[bytes] = process.stdout  # type: ignore

    return cast(str, stdout.read().strip().decode('ascii'))


def build_defines_as_args() -> List[str]:
    return [
        ("-D" + "=".join(l.split(maxsplit=2)[1:]))
        for l in build_defines_as_source().split("\n")
        if l.startswith('#define')
    ]


# could be moved elsewhere!, this just happens to be used by scripts that also
# use this module.
def queue_processes(
        process_funcs: Sequence[Tuple[Callable[..., subprocess.Popen[Any]], Tuple[Any, ...]]],
        *,
        job_total: int = -1,
        sleep: float = 0.1,
) -> None:
    """ Takes a list of function arg pairs, each function must return a process
    """

    if job_total == -1:
        import multiprocessing
        job_total = multiprocessing.cpu_count()
        del multiprocessing

    if job_total == 1:
        for func, args in process_funcs:
            sys.stdout.flush()
            sys.stderr.flush()

            process = func(*args)
            process.wait()
    else:
        import time

        processes: List[subprocess.Popen[Any]] = []
        for func, args in process_funcs:
            # wait until a thread is free
            while 1:
                processes[:] = [p for p in processes if p.poll() is None]

                if len(processes) <= job_total:
                    break
                time.sleep(sleep)

            sys.stdout.flush()
            sys.stderr.flush()

            processes.append(func(*args))

        # Don't return until all jobs have finished.
        while 1:
            processes[:] = [p for p in processes if p.poll() is None]
            if not processes:
                break
            time.sleep(sleep)


def main() -> None:
    if not os.path.exists(join(CMAKE_DIR, "CMakeCache.txt")):
        print("This script must run from the cmake build dir")
        return

    for s in build_info():
        print(s)


if __name__ == "__main__":
    main()
