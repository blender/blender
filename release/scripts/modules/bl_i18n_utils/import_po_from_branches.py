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

# Import in trunk/po all po from branches translated above the given threshold.

import os
import shutil
import sys
import subprocess
from codecs import open

import settings
import utils
import rtl_preprocess


TRUNK_PO_DIR = settings.TRUNK_PO_DIR
BRANCHES_DIR = settings.BRANCHES_DIR

RTL_PREPROCESS_FILE = settings.RTL_PREPROCESS_FILE

PY3 = settings.PYTHON3_EXEC


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Import advanced enough po’s " \
                                                 "from branches to trunk.")
    parser.add_argument('-t', '--threshold', type=int,
                        help="Import threshold, as a percentage.")
    parser.add_argument('-s', '--strict', action="store_true",
                        help="Raise an error if a po is broken.")
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    args = parser.parse_args()


    ret = 0

    threshold = float(settings.IMPORT_MIN_LEVEL)/100.0
    if args.threshold is not None:
        threshold = float(args.threshold)/100.0

    for lang in os.listdir(BRANCHES_DIR):
        if args.langs and lang not in args.langs:
            continue
        po = os.path.join(BRANCHES_DIR, lang, ".".join((lang, "po")))
        if os.path.exists(po):
            po_is_rtl = os.path.join(BRANCHES_DIR, lang, RTL_PREPROCESS_FILE)
            msgs, state, stats = utils.parse_messages(po)
            tot_msgs = stats["tot_msg"]
            trans_msgs = stats["trans_msg"]
            lvl = 0.0
            if tot_msgs:
                lvl = float(trans_msgs)/float(tot_msgs)
            if lvl > threshold:
                if state["is_broken"] and args.strict:
                    print("{:<10}: {:>6.1%} done, but BROKEN, skipped." \
                          "".format(lang, lvl))
                    ret = 1
                else:
                    if os.path.exists(po_is_rtl):
                        out_po = os.path.join(TRUNK_PO_DIR,
                                              ".".join((lang, "po")))
                        out_raw_po = os.path.join(TRUNK_PO_DIR,
                                                  "_".join((lang, "raw.po")))
                        keys = []
                        trans = []
                        for k, m in msgs.items():
                            keys.append(k)
                            trans.append("".join(m["msgstr_lines"]))
                        trans = rtl_preprocess.log2vis(trans)
                        for k, t in zip(keys, trans):
                            # Mono-line for now...
                            msgs[k]["msgstr_lines"] = [t]
                        utils.write_messages(out_po, msgs, state["comm_msg"],
                                             state["fuzzy_msg"])
                        # Also copies org po!
                        shutil.copy(po, out_raw_po)
                        print("{:<10}: {:>6.1%} done, enough translated " \
                              "messages, processed and copied to trunk." \
                              "".format(lang, lvl))
                    else:
                        shutil.copy(po, TRUNK_PO_DIR)
                        print("{:<10}: {:>6.1%} done, enough translated " \
                              "messages, copied to trunk.".format(lang, lvl))
            else:
                if state["is_broken"] and args.strict:
                    print("{:<10}: {:>6.1%} done, BROKEN and not enough " \
                          "translated messages, skipped".format(lang, lvl))
                    ret = 1
                else:
                    print("{:<10}: {:>6.1%} done, not enough translated " \
                          "messages, skipped.".format(lang, lvl))
    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
