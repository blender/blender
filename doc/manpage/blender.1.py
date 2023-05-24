#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script generates the blender.1 man page, embedding the help text
from the Blender executable itself. Invoke it as follows:

    blender.1.py --blender <path-to-blender> --output <output-filename>

where <path-to-blender> is the path to the Blender executable,
and <output-filename> is where to write the generated man page.
"""

import argparse
import os
import subprocess
import time

from typing import (
    Dict,
    TextIO,
)


def man_format(data: str) -> str:
    data = data.replace("-", "\\-")
    data = data.replace("\t", "  ")
    return data


def blender_extract_info(blender_bin: str) -> Dict[str, str]:

    blender_env = {
        "ASAN_OPTIONS": (
            os.environ.get("ASAN_OPTIONS", "") +
            ":exitcode=0:check_initialization_order=0:strict_init_order=0"
        ).lstrip(":"),
    }

    blender_help = subprocess.run(
        [blender_bin, "--help"],
        env=blender_env,
        check=True,
        stdout=subprocess.PIPE,
    ).stdout.decode(encoding="utf-8")

    blender_version_output = subprocess.run(
        [blender_bin, "--version"],
        env=blender_env,
        check=True,
        stdout=subprocess.PIPE,
    ).stdout.decode(encoding="utf-8")

    # Extract information from the version string.
    # Note that some internal modules may print errors (e.g. color management),
    # check for each lines prefix to ensure these aren't included.
    blender_version = ""
    blender_date = ""
    for l in blender_version_output.split("\n"):
        if l.startswith("Blender "):
            # Remove 'Blender' prefix.
            blender_version = l.split(" ", 1)[1].strip()
        elif l.lstrip().startswith("build date:"):
            # Remove 'build date:' prefix.
            blender_date = l.split(":", 1)[1].strip()
        if blender_version and blender_date:
            break

    if not blender_date:
        # Happens when built without WITH_BUILD_INFO e.g.
        date_string = time.strftime("%B %d, %Y", time.gmtime(int(os.environ.get('SOURCE_DATE_EPOCH', time.time()))))
    else:
        date_string = time.strftime("%B %d, %Y", time.strptime(blender_date, "%Y-%m-%d"))

    return {
        "help": blender_help,
        "version": blender_version,
        "date": date_string,
    }


def man_page_from_blender_help(fh: TextIO, blender_bin: str, verbose: bool) -> None:
    if verbose:
        print("Extracting help text:", blender_bin)
    blender_info = blender_extract_info(blender_bin)

    # Header Content.
    fh.write(
        '.TH "BLENDER" "1" "%s" "Blender %s"\n' %
        (blender_info["date"], blender_info["version"].replace(".", "\\&."))
    )

    fh.write(r"""
.SH NAME
blender \- a full-featured 3D application""")

    fh.write(r"""
.SH SYNOPSIS
.B blender [args ...] [file] [args ...]""")

    fh.write(r"""
.br
.SH DESCRIPTION
.PP
.B blender
is a full-featured 3D application. It supports the entirety of the 3D pipeline - """
             """modeling, rigging, animation, simulation, rendering, compositing, motion tracking, and video editing.

Use Blender to create 3D images and animations, films and commercials, content for games, """
             r"""architectural and industrial visualizations, and scientific visualizations.

https://www.blender.org""")

    fh.write(r"""
.SH OPTIONS""")

    fh.write("\n\n")

    # Body Content.

    lines = [line.rstrip() for line in blender_info["help"].split("\n")]

    while lines:
        l = lines.pop(0)
        if l.startswith("Environment Variables:"):
            fh.write('.SH "ENVIRONMENT VARIABLES"\n')
        elif l.endswith(":"):  # One line.
            fh.write('.SS "%s"\n\n' % l)
        elif l.startswith("-") or l.startswith("/"):  # Can be multi line.
            fh.write('.TP\n')
            fh.write('.B %s\n' % man_format(l))

            while lines:
                # line with no
                if lines[0].strip() and len(lines[0].lstrip()) == len(lines[0]):  # No white space.
                    break

                if not l:  # Second blank line.
                    fh.write('.IP\n')
                else:
                    fh.write('.br\n')

                l = lines.pop(0)
                if l:
                    assert l.startswith('\t')
                    l = l[1:]  # Remove first white-space (tab).

                fh.write('%s\n' % man_format(l))

        else:
            if not l.strip():
                fh.write('.br\n')
            else:
                fh.write('%s\n' % man_format(l))

    # Footer Content.

    fh.write(r"""
.br
.SH SEE ALSO
.B luxrender(1)

.br
.SH AUTHORS
This manpage was written for a Debian GNU/Linux system by Daniel Mester
<mester@uni-bremen.de> and updated by Cyril Brulebois
<cyril.brulebois@enst-bretagne.fr> and Dan Eicher <dan@trollwerks.org>.
""")


def create_argparse() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        required=True,
        help="The man page to write to."
    )
    parser.add_argument(
        "--blender",
        required=True,
        help="Path to the blender binary."
    )
    parser.add_argument(
        "--verbose",
        default=False,
        required=False,
        action='store_true',
        help="Print additional progress."
    )

    return parser


def main() -> None:
    parser = create_argparse()
    args = parser.parse_args()

    blender_bin = args.blender
    output_filename = args.output
    verbose = args.verbose

    with open(output_filename, "w", encoding="utf-8") as fh:
        man_page_from_blender_help(fh, blender_bin, verbose)
        if verbose:
            print("Written:", output_filename)


if __name__ == "__main__":
    main()
