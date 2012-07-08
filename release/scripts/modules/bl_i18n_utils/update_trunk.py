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

# Update trunk from branches:
# * Remove po’s in trunk.
# * Copy po’s from branches advanced enough.
# * Clean po’s in trunk.
# * Compile po’s in trunk in mo’s, keeping track of those failing.
# * Remove po’s, mo’s (and their dir’s) that failed to compile or
#   are no more present in trunk.

import subprocess
import os
import sys
import shutil

try:
    import settings
except:
    from . import settings

TRUNK_PO_DIR = settings.TRUNK_PO_DIR
TRUNK_MO_DIR = settings.TRUNK_MO_DIR

PY3 = settings.PYTHON3_EXEC


def main():
    import argparse
    parser = argparse.ArgumentParser(description="" \
                        "Update trunk from branches:\n" \
                        "* Remove po’s in trunk.\n" \
                        "* Copy po’s from branches advanced enough.\n" \
                        "* Clean po’s in trunk.\n" \
                        "* Compile po’s in trunk in mo’s, keeping " \
                        "track of those failing.\n" \
                        "* Remove po’s and mo’s (and their dir’s) that " \
                        "failed to compile or are no more present in trunk.")
    parser.add_argument('-t', '--threshold', type=int,
                        help="Import threshold, as a percentage.")
    parser.add_argument('-p', '--po', action="store_false",
                        help="Do not remove failing po’s.")
    parser.add_argument('-m', '--mo', action="store_false",
                        help="Do not remove failing mo’s.")
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    args = parser.parse_args()


    ret = 0
    failed = set()

    # Remove po’s in trunk.
    for po in os.listdir(TRUNK_PO_DIR):
        if po.endswith(".po"):
            lang = os.path.basename(po)[:-3]
            if args.langs and lang not in args.langs:
                continue
            po = os.path.join(TRUNK_PO_DIR, po)
            os.remove(po)

    # Copy po’s from branches.
    cmd = [PY3, "./import_po_from_branches.py", "-s"]
    if args.threshold is not None:
        cmd += ["-t", str(args.threshold)]
    if args.langs:
        cmd += args.langs
    t = subprocess.call(cmd)
    if t:
        ret = t

    # Add in failed all mo’s no more having relevant po’s in trunk.
    for lang in os.listdir(TRUNK_MO_DIR):
        if lang == ".svn":
            continue  # !!!
        if not os.path.exists(os.path.join(TRUNK_PO_DIR, ".".join((lang, "po")))):
            failed.add(lang)

    # Check and compile each po separatly, to keep track of those failing.
    # XXX There should not be any failing at this stage, import step is
    #     supposed to have already filtered them out!
    for po in os.listdir(TRUNK_PO_DIR):
        if po.endswith(".po") and not po.endswith("_raw.po"):
            lang = os.path.basename(po)[:-3]
            if args.langs and lang not in args.langs:
                continue

            cmd = [PY3, "./clean_po.py", "-t", "-s", lang]
            t = subprocess.call(cmd)
            if t:
                ret = t
                failed.add(lang)
                continue

            cmd = [PY3, "./update_mo.py", lang]
            t = subprocess.call(cmd)
            if t:
                ret = t
                failed.add(lang)

    # Remove failing po’s, mo’s and related dir’s.
    for lang in failed:
        print("Lang “{}” failed, removing it...".format(lang))
        if args.po:
            po = os.path.join(TRUNK_PO_DIR, ".".join((lang, "po")))
            if os.path.exists(po):
                os.remove(po)
        if args.mo:
            mo = os.path.join(TRUNK_MO_DIR, lang)
            if os.path.exists(mo):
                shutil.rmtree(mo)


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
