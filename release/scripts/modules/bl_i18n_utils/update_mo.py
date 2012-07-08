#!/usr/bin/python3

# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Create or update mo’s under /trunk/locale/…

import subprocess
import os
import sys

try:
    import settings
    import utils
except:
    from . import (settings, utils)


GETTEXT_MSGFMT_EXECUTABLE = settings.GETTEXT_MSGFMT_EXECUTABLE

SOURCE_DIR = settings.SOURCE_DIR
TRUNK_MO_DIR = settings.TRUNK_MO_DIR
TRUNK_PO_DIR = settings.TRUNK_PO_DIR

DOMAIN = settings.DOMAIN


def process_po(po, lang, mo=None):
    if not mo:
        mo_dir = os.path.join(TRUNK_MO_DIR, lang, "LC_MESSAGES")
        # Create dirs if not existing!
        if not os.path.isdir(mo_dir):
            os.makedirs(mo_dir, exist_ok = True)

    # show stats
    cmd = (GETTEXT_MSGFMT_EXECUTABLE,
        "--statistics",
        po,
        "-o",
        mo or os.path.join(mo_dir, ".".join((DOMAIN, "mo"))),
        )

    print("Running ", " ".join(cmd))
    ret = subprocess.call(cmd)
    print("Finished.")
    return ret


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Create or update mo’s " \
                                                 "under {}.".format(TRUNK_MO_DIR))
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    parser.add_argument('po', help="Only process that po file (implies --mo).",
                        nargs='?')
    parser.add_argument('mo', help="Mo file to generate (implies --po).",
                        nargs='?')
    args = parser.parse_args()

    ret = 0

    if args.po and args.mo:
        if os.path.exists(args.po):
            t = process_po(args.po, None, args.mo)
            if t:
                ret = t
    elif args.langs:
        for lang in args.langs:
            po = os.path.join(TRUNK_PO_DIR, ".".join((lang, "po")))
            if os.path.exists(po):
                t = process_po(po, lang)
                if t:
                    ret = t
    else:
        for po in os.listdir(TRUNK_PO_DIR):
            if po.endswith(".po") and not po.endswith("_raw.po"):
                lang = os.path.basename(po)[:-3]
                po = os.path.join(TRUNK_PO_DIR, po)
                t = process_po(po, lang)
                if t:
                    ret = t
    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
