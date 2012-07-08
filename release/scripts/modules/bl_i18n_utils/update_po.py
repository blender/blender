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

import subprocess
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
BRANCHES_DIR  = settings.BRANCHES_DIR
TRUNK_PO_DIR  = settings.TRUNK_PO_DIR
FILE_NAME_POT = settings.FILE_NAME_POT


def pproc_newcontext_po(po, pot_messages, pot_stats):
    print("Adding new contexts to {}...".format(po))
    messages, state, stats = utils.parse_messages(po)
    known_ctxt = stats["contexts"]
    print("Already known (present) context(s): {}".format(str(known_ctxt)))

    new_ctxt = set()
    added = 0
    # Only use valid already translated messages!
    allowed_keys = state["trans_msg"] - state["fuzzy_msg"] - state["comm_msg"]
    for key in pot_messages.keys():
        ctxt, msgid = key
        if ctxt in known_ctxt:
            continue
        new_ctxt.add(ctxt)
        for t_ctxt in known_ctxt:
            # XXX The first match will win, this might not be optimal...
            t_key = (t_ctxt, msgid)
            if t_key in allowed_keys:
                # Wrong comments (sources) will be removed by msgmerge...
                messages[key] = messages[t_key]
                messages[key]["msgctxt_lines"] = [ctxt]
                added += 1

    utils.write_messages(po, messages, state["comm_msg"], state["fuzzy_msg"])
    print("Finished!\n    {} new context(s) was/were added {}, adding {} new "
          "messages.\n".format(len(new_ctxt), str(new_ctxt), added))
    return 0


def process_po(po, lang):
    # update po file
    cmd = (GETTEXT_MSGMERGE_EXECUTABLE,
           "--update",
           "-w", "1",  # XXX Ugly hack to prevent msgmerge merging
                       #     short source comments together!
           "--no-wrap",
           "--backup=none",
           "--lang={}".format(lang),
           po,
           FILE_NAME_POT,
           )

    print("Updating {}...".format(po))
    print("Running ", " ".join(cmd))
    ret = subprocess.call(cmd)
    print("Finished!\n")
    return ret


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Write out messages.txt "
                                                 "from Blender.")
    parser.add_argument('-t', '--trunk', action="store_true",
                        help="Update po’s in /trunk/po rather than /branches.")
    parser.add_argument('-i', '--input', metavar="File",
                        help="Input pot file path.")
    parser.add_argument('--pproc-contexts', action="store_true",
                        help="Pre-process po’s to avoid having plenty of "
                             "fuzzy msgids just because a context was "
                             "added/changed!")
    parser.add_argument('-a', '--add', action="store_true",
                        help="Add missing po’s (useful only when one or "
                             "more languages are given!).")
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    args = parser.parse_args()

    if args.input:
        global FILE_NAME_POT
        FILE_NAME_POT = args.input
    ret = 0

    if args.pproc_contexts:
        _ctxt_proc = pproc_newcontext_po
        pot_messages, _a, pot_stats = utils.parse_messages(FILE_NAME_POT)
    else:
        _ctxt_proc = lambda a, b, c: 0
        pot_messages, pot_stats = None, None

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
                t = _ctxt_proc(po, pot_messages, pot_stats)
                if t:
                    ret = t
                t = process_po(po, lang)
                if t:
                    ret = t
    elif args.trunk:
        for po in os.listdir(TRUNK_PO_DIR):
            if po.endswith(".po"):
                lang = os.path.basename(po)[:-3]
                po = os.path.join(TRUNK_PO_DIR, po)
                t = _ctxt_proc(po, pot_messages, pot_stats)
                if t:
                    ret = t
                t = process_po(po, lang)
                if t:
                    ret = t
    else:
        for lang in os.listdir(BRANCHES_DIR):
            po = os.path.join(BRANCHES_DIR, lang, ".".join((lang, "po")))
            if os.path.exists(po):
                t = _ctxt_proc(po, pot_messages, pot_stats)
                if t:
                    ret = t
                t = process_po(po, lang)
                if t:
                    ret = t

    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
