#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script generates the blender.1 man page, embedding the help text
from the Blender executable itself. Invoke it as follows:

    ./blender.bin -b --python doc/manpage/blender.1.py -- --output <output-filename>

where <path-to-blender> is the path to the Blender executable,
and <output-filename> is where to write the generated man page.
"""

import argparse
import os
import time
import sys

from typing import (
    Dict,
    TextIO,
)


def man_format(data: str) -> str:
    data = data.replace("-", "\\-")
    data = data.replace("\t", "  ")
    return data


def blender_extract_info() -> Dict[str, str]:
    # Only use of `bpy` in this file.
    import bpy  # type: ignore
    blender_help_text = bpy.app.help_text()
    blender_version_text = bpy.app.version_string
    blender_build_date_text = bpy.app.build_date

    if blender_build_date_text == b'Unknown':
        # Happens when built without WITH_BUILD_INFO e.g.
        blender_date = time.strftime("%B %d, %Y", time.gmtime(int(os.environ.get('SOURCE_DATE_EPOCH', time.time()))))
    else:
        blender_date = time.strftime("%B %d, %Y", time.strptime(blender_build_date_text, "%Y-%m-%d"))

    return {
        "help": blender_help_text,
        "version": blender_version_text,
        "date": blender_date,
    }


def man_page_from_blender_help(fh: TextIO, verbose: bool) -> None:
    blender_info = blender_extract_info()

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
        "--verbose",
        default=False,
        required=False,
        action='store_true',
        help="Print additional progress."
    )

    return parser


def main() -> None:
    argv = sys.argv[sys.argv.index("--") + 1:]
    parser = create_argparse()
    args = parser.parse_args(argv)

    output_filename = args.output
    verbose = args.verbose

    with open(output_filename, "w", encoding="utf-8") as fh:
        man_page_from_blender_help(fh, verbose)
        if verbose:
            print("Written:", output_filename)


if __name__ == "__main__":
    main()
