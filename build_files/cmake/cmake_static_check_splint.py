#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

CHECKER_IGNORE_PREFIX = [
    "extern",
    "intern/moto",
]

CHECKER_BIN = "splint"

CHECKER_ARGS = [
    "-weak",
    "-posix-lib",
    "-linelen", "10000",
    "+ignorequals",
    "+relaxtypes",
    "-retvalother",
    "+matchanyintegral",
    "+longintegral",
    "+ignoresigns",
    "-nestcomment",
    "-predboolothers",
    "-ifempty",
    "-unrecogcomments",

    # we may want to remove these later
    "-type",
    "-fixedformalarray",
    "-fullinitblock",
    "-fcnuse",
    "-initallelements",
    "-castfcnptr",
    # -forcehints,
    "-bufferoverflowhigh",  # warns a lot about sprintf()

    # re-definitions, rna causes most of these
    "-redef",
    "-syntax",

    # dummy, witjout this splint complains with:
    #  /usr/include/bits/confname.h:31:27: *** Internal Bug at cscannerHelp.c:2428: Unexpanded macro not function or constant: int _PC_MAX_CANON
    "-D_PC_MAX_CANON=0",
]


import project_source_info
import subprocess
import sys
import os

USE_QUIET = (os.environ.get("QUIET", None) is not None)


def main():
    source_info = project_source_info.build_info(use_cxx=False, ignore_prefix_list=CHECKER_IGNORE_PREFIX)

    check_commands = []
    for c, inc_dirs, defs in source_info:
        cmd = ([CHECKER_BIN] +
               CHECKER_ARGS +
               [c] +
               [("-I%s" % i) for i in inc_dirs] +
               [("-D%s" % d) for d in defs]
               )

        check_commands.append((c, cmd))

    def my_process(i, c, cmd):
        if not USE_QUIET:
            percent = 100.0 * (i / len(check_commands))
            percent_str = "[" + ("%.2f]" % percent).rjust(7) + " %:"

            sys.stdout.write("%s %s\n" % (percent_str, c))
            sys.stdout.flush()

        return subprocess.Popen(cmd)

    process_functions = []
    for i, (c, cmd) in enumerate(check_commands):
        process_functions.append((my_process, (i, c, cmd)))

    project_source_info.queue_processes(process_functions)


if __name__ == "__main__":
    main()
