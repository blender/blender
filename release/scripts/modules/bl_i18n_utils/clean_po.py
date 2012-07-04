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

# Clean (i.e. remove commented messages) po’s in branches or trunk.

import os
import sys
import collections
from codecs import open

import settings
import utils

TRUNK_PO_DIR = settings.TRUNK_PO_DIR
BRANCHES_DIR = settings.BRANCHES_DIR


def do_clean(po, strict):
    print("Cleaning {}...".format(po))
    messages, states, u1 = utils.parse_messages(po)

    if strict and states["is_broken"]:
        print("ERROR! This .po file is broken!")
        return 1

    for msgkey in states["comm_msg"]:
        del messages[msgkey]
    utils.write_messages(po, messages, states["comm_msg"], states["fuzzy_msg"])
    print("Removed {} commented messages.".format(len(states["comm_msg"])))
    return 0


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Clean po’s in branches " \
                                                 "or trunk (i.e. remove " \
                                                 "all commented messages).")
    parser.add_argument('-t', '--trunk', action="store_true",
                        help="Clean po’s in trunk rather than branches.")
    parser.add_argument('-s', '--strict', action="store_true",
                        help="Raise an error if a po is broken.")
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    args = parser.parse_args()


    ret = 0

    if args.langs:
        for lang in args.langs:
            if args.trunk:
                po = os.path.join(TRUNK_PO_DIR, ".".join((lang, "po")))
            else:
                po = os.path.join(BRANCHES_DIR, lang, ".".join((lang, "po")))
            if os.path.exists(po):
                t = do_clean(po, args.strict)
                if t:
                    ret = t
    elif args.trunk:
        for po in os.listdir(TRUNK_PO_DIR):
            if po.endswith(".po"):
                po = os.path.join(TRUNK_PO_DIR, po)
                t = do_clean(po, args.strict)
                if t:
                    ret = t
    else:
        for lang in os.listdir(BRANCHES_DIR):
            for po in os.listdir(os.path.join(BRANCHES_DIR, lang)):
                if po.endswith(".po"):
                    po = os.path.join(BRANCHES_DIR, lang, po)
                    t = do_clean(po, args.strict)
                    if t:
                        ret = t


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
