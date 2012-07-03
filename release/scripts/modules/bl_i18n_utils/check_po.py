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

# Check po’s in branches (or in trunk) for missing/unneeded messages.

import os
import sys
from codecs import open

import settings
import utils

TRUNK_PO_DIR = settings.TRUNK_PO_DIR
BRANCHES_DIR = settings.BRANCHES_DIR

FILE_NAME_POT = settings.FILE_NAME_POT


def print_diff(ref_messages, messages, states):
    # Remove comments from messages list!
    messages = set(messages.keys()) - states["comm_msg"]
    unneeded = (messages - ref_messages)
    for msgid in unneeded:
        print('\tUnneeded message id "{}"'.format(msgid))

    missing = (ref_messages - messages)
    for msgid in missing:
        print('\tMissing message id "{}"'.format(msgid))

    for msgid in states["comm_msg"]:
        print('\tCommented message id "{}"'.format(msgid))

    print("\t{} unneeded messages, {} missing messages, {} commented messages." \
          "".format(len(unneeded), len(missing), len(states["comm_msg"])))
    return 0


def process_po(ref_messages, po, glob_stats, do_stats, do_messages):
    print("Checking {}...".format(po))
    ret = 0

    messages, states, stats = utils.parse_messages(po)
    if do_messages:
        t = print_diff(ref_messages, messages, states)
        if t:
            ret = t
    if do_stats:
        print("\tStats:")
        t = utils.print_stats(stats, glob_stats, prefix="        ")
        if t:
            ret = t
    if states["is_broken"]:
        print("\tERROR! This .po is broken!")
        ret = 1
    return ret


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Check po’s in branches " \
                                                 "(or in trunk) for missing" \
                                                 "/unneeded messages.")
    parser.add_argument('-s', '--stats', action="store_true",
                        help="Print po’s stats.")
    parser.add_argument('-m', '--messages', action="store_true",
                        help="Print po’s missing/unneeded/commented messages.")
    parser.add_argument('-t', '--trunk', action="store_true",
                        help="Check po’s in /trunk/po rather than /branches.")
    parser.add_argument('-p', '--pot',
                        help="Specify the .pot file used as reference.")
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    args = parser.parse_args()


    if args.pot:
        global FILE_NAME_POT
        FILE_NAME_POT = args.pot
    glob_stats = {"nbr"               : 0.0,
                  "lvl"               : 0.0,
                  "lvl_ttips"         : 0.0,
                  "lvl_trans_ttips"   : 0.0,
                  "lvl_ttips_in_trans": 0.0,
                  "lvl_comm"          : 0.0,
                  "nbr_signs"         : 0,
                  "nbr_trans_signs"   : 0,
                  "contexts"          : set()}
    ret = 0

    pot_messages = None
    if args.messages:
        pot_messages, u1, pot_stats = utils.parse_messages(FILE_NAME_POT)
        pot_messages = set(pot_messages.keys())
        glob_stats["nbr_signs"] = pot_stats["nbr_signs"]

    if args.langs:
        for lang in args.langs:
            if args.trunk:
                po = os.path.join(TRUNK_PO_DIR, ".".join((lang, "po")))
            else:
                po = os.path.join(BRANCHES_DIR, lang, ".".join((lang, "po")))
            if os.path.exists(po):
                t = process_po(pot_messages, po, glob_stats,
                               args.stats, args.messages)
                if t:
                    ret = t
    elif args.trunk:
        for po in os.listdir(TRUNK_PO_DIR):
            if po.endswith(".po"):
                po = os.path.join(TRUNK_PO_DIR, po)
                t = process_po(pot_messages, po, glob_stats,
                               args.stats, args.messages)
                if t:
                    ret = t
    else:
        for lang in os.listdir(BRANCHES_DIR):
            for po in os.listdir(os.path.join(BRANCHES_DIR, lang)):
                if po.endswith(".po"):
                    po = os.path.join(BRANCHES_DIR, lang, po)
                    t = process_po(pot_messages, po, glob_stats,
                                   args.stats, args.messages)
                    if t:
                        ret = t

    if args.stats and glob_stats["nbr"] != 0.0:
        nbr_contexts = len(glob_stats["contexts"]-{""})
        if nbr_contexts != 1:
            if nbr_contexts == 0:
                nbr_contexts = "No"
            _ctx_txt = "s are"
        else:
            _ctx_txt = " is"
        print("\nAverage stats for all {:.0f} processed files:\n" \
              "    {:>6.1%} done!\n" \
              "    {:>6.1%} of messages are tooltips.\n" \
              "    {:>6.1%} of tooltips are translated.\n" \
              "    {:>6.1%} of translated messages are tooltips.\n" \
              "    {:>6.1%} of messages are commented.\n" \
              "    The org msgids are currently made of {} signs.\n" \
              "    All processed translations are currently made of {} signs.\n" \
              "    {} specific context{} present:\n            {}\n" \
              "".format(glob_stats["nbr"], glob_stats["lvl"]/glob_stats["nbr"],
                        glob_stats["lvl_ttips"]/glob_stats["nbr"],
                        glob_stats["lvl_trans_ttips"]/glob_stats["nbr"],
                        glob_stats["lvl_ttips_in_trans"]/glob_stats["nbr"],
                        glob_stats["lvl_comm"]/glob_stats["nbr"], glob_stats["nbr_signs"],
                        glob_stats["nbr_trans_signs"], nbr_contexts, _ctx_txt,
                        "\n            ".join(glob_stats["contexts"]-{""})))

    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    print(" *** WARNING! Number of tooltips is only an estimation! ***\n")
    sys.exit(main())
