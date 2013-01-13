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

# Update po’s in the branches from blender.pot in /trunk/po dir.

import concurrent.futures
import os
import sys
from codecs import open
import shutil

try:
    import settings
    import utils
except:
    from . import (settings, utils)


GETTEXT_MSGMERGE_EXECUTABLE = settings.GETTEXT_MSGMERGE_EXECUTABLE
BRANCHES_DIR = settings.BRANCHES_DIR
TRUNK_PO_DIR = settings.TRUNK_PO_DIR
FILE_NAME_POT = settings.FILE_NAME_POT


def process_po(data):
    po, lang, pot_msgs = data
    # update po file
    msg = utils.I18nMessages(iso=lang, kind='PO', src=po)
    print("Updating {}...".format(po))
    msg.update(pot_msgs)
    msg.write(kind='PO', dest=po)
    print("Finished updating {}!\n".format(po))
    return 0


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Write out messages.txt from Blender.")
    parser.add_argument('-t', '--trunk', action="store_true", help="Update po’s in /trunk/po rather than /branches.")
    parser.add_argument('-i', '--input', metavar="File", help="Input pot file path.")
    parser.add_argument('-a', '--add', action="store_true",
                        help="Add missing po’s (useful only when one or more languages are given!).")
    parser.add_argument('langs', metavar='ISO_code', nargs='*', help="Restrict processed languages to those.")
    args = parser.parse_args()

    if args.input:
        global FILE_NAME_POT
        FILE_NAME_POT = args.input
    ret = 0

    pot_msgs = utils.I18nMessages(kind='PO', src=FILE_NAME_POT)
    pool_data = []

    if args.langs:
        for lang in args.langs:
            if args.trunk:
                dr = TRUNK_PO_DIR
                po = os.path.join(dr, ".".join((lang, "po")))
            else:
                dr = os.path.join(BRANCHES_DIR, lang)
                po = os.path.join(dr, ".".join((lang, "po")))
            if args.add:
                if not os.path.exists(dr):
                    os.makedirs(dr)
                if not os.path.exists(po):
                    shutil.copy(FILE_NAME_POT, po)
            if args.add or os.path.exists(po):
                pool_data.append((po, lang, pot_msgs))
    elif args.trunk:
        for po in os.listdir(TRUNK_PO_DIR):
            if po.endswith(".po"):
                lang = os.path.basename(po)[:-3]
                po = os.path.join(TRUNK_PO_DIR, po)
                pool_data.append((po, lang, pot_msgs))
    else:
        for lang in os.listdir(BRANCHES_DIR):
            po = os.path.join(BRANCHES_DIR, lang, ".".join((lang, "po")))
            if os.path.exists(po):
                pool_data.append((po, lang, pot_msgs))

    with concurrent.futures.ProcessPoolExecutor() as executor:
        for r in executor.map(process_po, pool_data, timeout=600):
            if r != 0:
                ret = r

    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())