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

# Some misc utilities...

import os
import sys
import collections

from bl_i18n_utils import settings


COMMENT_PREFIX = settings.COMMENT_PREFIX
WARN_NC = settings.WARN_MSGID_NOT_CAPITALIZED
NC_ALLOWED = settings.WARN_MSGID_NOT_CAPITALIZED_ALLOWED


def stripeol(s):
    return s.rstrip("\n\r")


# XXX For now, we assume that all messages > 30 chars are tooltips!
def is_tooltip(msgid):
    return len(msgid) > 30

def parse_messages(fname):
    """
    Returns a tupple (messages, states, stats).
    messages is an odereddict of dicts
        {(ctxt, msgid): {msgid_lines:, msgstr_lines:,
                         comment_lines:, msgctxt_lines:}}.
    states is a dict of three sets of (msgid, ctxt), and a boolean flag
        indicating the .po is somewhat broken
        {trans_msg:, fuzzy_msg:, comm_msg:, is_broken:}.
    stats is a dict of values
        {tot_msg:, trans_msg:, tot_ttips:, trans_ttips:, comm_msg:,
         nbr_signs:, nbr_trans_signs:, contexts: set()}.
    Note: This function will silently "arrange" mis-formated entries, thus
        using afterward write_messages() should always produce a po-valid file,
        though not correct!
    """
    tot_messages = 0
    tot_tooltips = 0
    trans_messages = 0
    trans_tooltips = 0
    comm_messages = 0
    nbr_signs = 0
    nbr_trans_signs = 0
    contexts = set()
    reading_msgid = False
    reading_msgstr = False
    reading_msgctxt = False
    reading_comment = False
    is_translated = False
    is_fuzzy = False
    is_commented = False
    is_broken = False
    msgid_lines = []
    msgstr_lines = []
    msgctxt_lines = []
    comment_lines = []

    messages = getattr(collections, 'OrderedDict', dict)()
    translated_messages = set()
    fuzzy_messages = set()
    commented_messages = set()


    def clean_vars():
        nonlocal reading_msgid, reading_msgstr, reading_msgctxt, \
                 reading_comment, is_fuzzy, is_translated, is_commented, \
                 msgid_lines, msgstr_lines, msgctxt_lines, comment_lines
        reading_msgid = reading_msgstr = reading_msgctxt = \
                        reading_comment = False
        is_tooltip = is_fuzzy = is_translated = is_commented = False
        msgid_lines = []
        msgstr_lines = []
        msgctxt_lines = []
        comment_lines = []


    def finalize_message():
        nonlocal reading_msgid, reading_msgstr, reading_msgctxt, \
                 reading_comment, is_fuzzy, is_translated, is_commented, \
                 msgid_lines, msgstr_lines, msgctxt_lines, comment_lines, \
                 messages, translated_messages, fuzzy_messages, \
                 commented_messages, \
                 tot_messages, tot_tooltips, trans_messages, trans_tooltips, \
                 comm_messages, nbr_signs, nbr_trans_signs, contexts

        msgid = "".join(msgid_lines)
        msgctxt = "".join(msgctxt_lines)
        msgkey = (msgctxt, msgid)
        is_ttip = is_tooltip(msgid)

        # Never allow overriding existing msgid/msgctxt pairs!
        if msgkey in messages:
            clean_vars()
            return

        nbr_signs += len(msgid)
        if is_commented:
            commented_messages.add(msgkey)
        elif is_fuzzy:
            fuzzy_messages.add(msgkey)
        elif is_translated:
            translated_messages.add(msgkey)
            nbr_trans_signs += len("".join(msgstr_lines))
        messages[msgkey] = {"msgid_lines"  : msgid_lines,
                            "msgstr_lines" : msgstr_lines,
                            "comment_lines": comment_lines,
                            "msgctxt_lines": msgctxt_lines}

        if is_commented:
            comm_messages += 1
        else:
            tot_messages += 1
            if is_ttip:
                tot_tooltips += 1
            if not is_fuzzy and is_translated:
                trans_messages += 1
                if is_ttip:
                    trans_tooltips += 1
            if msgctxt not in contexts:
                contexts.add(msgctxt)

        clean_vars()


    with open(fname, 'r', encoding="utf-8") as f:
        for line_nr, line in enumerate(f):
            line = stripeol(line)
            if line == "":
                finalize_message()

            elif line.startswith("msgctxt") or \
                 line.startswith("".join((COMMENT_PREFIX, "msgctxt"))):
                reading_comment = False
                reading_ctxt = True
                if line.startswith(COMMENT_PREFIX):
                    is_commented = True
                    line = line[9+len(COMMENT_PREFIX):-1]
                else:
                    line = line[9:-1]
                msgctxt_lines.append(line)

            elif line.startswith("msgid") or \
                 line.startswith("".join((COMMENT_PREFIX, "msgid"))):
                reading_comment = False
                reading_msgid = True
                if line.startswith(COMMENT_PREFIX):
                    is_commented = True
                    line = line[7+len(COMMENT_PREFIX):-1]
                else:
                    line = line[7:-1]
                msgid_lines.append(line)

            elif line.startswith("msgstr") or \
                 line.startswith("".join((COMMENT_PREFIX, "msgstr"))):
                if not reading_msgid:
                    is_broken = True
                else:
                    reading_msgid = False
                reading_msgstr = True
                if line.startswith(COMMENT_PREFIX):
                    line = line[8+len(COMMENT_PREFIX):-1]
                    if not is_commented:
                        is_broken = True
                else:
                    line = line[8:-1]
                    if is_commented:
                        is_broken = True
                msgstr_lines.append(line)
                if line:
                    is_translated = True

            elif line.startswith("#"):
                if reading_msgid:
                    if is_commented:
                        msgid_lines.append(line[1+len(COMMENT_PREFIX):-1])
                    else:
                        msgid_lines.append(line)
                        is_broken = True
                elif reading_msgstr:
                    if is_commented:
                        msgstr_lines.append(line[1+len(COMMENT_PREFIX):-1])
                    else:
                        msgstr_lines.append(line)
                        is_broken = True
                else:
                    if line.startswith("#, fuzzy"):
                        is_fuzzy = True
                    else:
                        comment_lines.append(line)
                    reading_comment = True

            else:
                if reading_msgid:
                    msgid_lines.append(line[1:-1])
                elif reading_msgstr:
                    line = line[1:-1]
                    msgstr_lines.append(line)
                    if not is_translated and line:
                        is_translated = True
                else:
                    is_broken = True

        # If no final empty line, last message is not finalized!
        if reading_msgstr:
            finalize_message()


    return (messages,
            {"trans_msg": translated_messages,
             "fuzzy_msg": fuzzy_messages,
             "comm_msg" : commented_messages,
             "is_broken": is_broken},
            {"tot_msg"        : tot_messages,
             "trans_msg"      : trans_messages,
             "tot_ttips"      : tot_tooltips,
             "trans_ttips"    : trans_tooltips,
             "comm_msg"       : comm_messages,
             "nbr_signs"      : nbr_signs,
             "nbr_trans_signs": nbr_trans_signs,
             "contexts"       : contexts})


def write_messages(fname, messages, commented, fuzzy):
    "Write in fname file the content of messages (similar to parse_messages " \
    "returned values). commented and fuzzy are two sets containing msgid. " \
    "Returns the number of written messages."
    num = 0
    with open(fname, 'w', encoding="utf-8") as f:
        for msgkey, val in messages.items():
            msgctxt, msgid = msgkey
            f.write("\n".join(val["comment_lines"]))
            # Only mark as fuzzy if msgstr is not empty!
            if msgkey in fuzzy and "".join(val["msgstr_lines"]):
                f.write("\n#, fuzzy")
            if msgkey in commented:
                if msgctxt:
                    f.write("\n{}msgctxt \"".format(COMMENT_PREFIX))
                    f.write("\"\n{}\"".format(COMMENT_PREFIX).join(
                                       val["msgctxt_lines"]))
                    f.write("\"")
                f.write("\n{}msgid \"".format(COMMENT_PREFIX))
                f.write("\"\n{}\"".format(COMMENT_PREFIX).join(
                                   val["msgid_lines"]))
                f.write("\"\n{}msgstr \"".format(COMMENT_PREFIX))
                f.write("\"\n{}\"".format(COMMENT_PREFIX).join(
                                   val["msgstr_lines"]))
                f.write("\"\n\n")
            else:
                if msgctxt:
                    f.write("\nmsgctxt \"")
                    f.write("\"\n\"".join(val["msgctxt_lines"]))
                    f.write("\"")
                f.write("\nmsgid \"")
                f.write("\"\n\"".join(val["msgid_lines"]))
                f.write("\"\nmsgstr \"")
                f.write("\"\n\"".join(val["msgstr_lines"]))
                f.write("\"\n\n")
            num += 1
    return num


def gen_empty_messages(blender_rev, time_str, year_str):
    """Generate an empty messages & state data (only header if present!)."""
    header_key = ("", "")

    messages = getattr(collections, 'OrderedDict', dict)()
    messages[header_key] = {
        "msgid_lines": [""],
        "msgctxt_lines": [],
        "msgstr_lines": [
            "Project-Id-Version: Blender r{}\\n"
            "".format(blender_rev),
            "Report-Msgid-Bugs-To: \\n",
            "POT-Creation-Date: {}\\n"
            "".format(time_str),
            "PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n",
            "Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n",
            "Language-Team: LANGUAGE <LL@li.org>\\n",
            "Language: \\n",
            "MIME-Version: 1.0\\n",
            "Content-Type: text/plain; charset=UTF-8\\n",
            "Content-Transfer-Encoding: 8bit\\n"
        ],
        "comment_lines": [
            "# Blender's translation file (po format).",
            "# Copyright (C) {} The Blender Foundation."
            "".format(year_str),
            "# This file is distributed under the same "
            "# license as the Blender package.",
            "# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.",
            "#",
        ],
    }

    states = {"trans_msg": set(),
              "fuzzy_msg": {header_key},
              "comm_msg": set(),
              "is_broken": False}

    return messages, states


def print_stats(stats, glob_stats=None, prefix=""):
    """
    Print out some stats about a po file.
    glob_stats is for making global stats over several po's.
    """
    tot_msgs        = stats["tot_msg"]
    trans_msgs      = stats["trans_msg"]
    tot_ttips       = stats["tot_ttips"]
    trans_ttips     = stats["trans_ttips"]
    comm_msgs       = stats["comm_msg"]
    nbr_signs       = stats["nbr_signs"]
    nbr_trans_signs = stats["nbr_trans_signs"]
    contexts        = stats["contexts"]
    lvl = lvl_ttips = lvl_trans_ttips = lvl_ttips_in_trans = lvl_comm = 0.0

    if tot_msgs > 0:
        lvl = float(trans_msgs)/float(tot_msgs)
        lvl_ttips = float(tot_ttips)/float(tot_msgs)
        lvl_comm = float(comm_msgs)/float(tot_msgs+comm_msgs)
    if tot_ttips > 0:
        lvl_trans_ttips = float(trans_ttips)/float(tot_ttips)
    if trans_msgs > 0:
        lvl_ttips_in_trans = float(trans_ttips)/float(trans_msgs)

    if glob_stats:
        glob_stats["nbr"]                += 1.0
        glob_stats["lvl"]                += lvl
        glob_stats["lvl_ttips"]          += lvl_ttips
        glob_stats["lvl_trans_ttips"]    += lvl_trans_ttips
        glob_stats["lvl_ttips_in_trans"] += lvl_ttips_in_trans
        glob_stats["lvl_comm"]           += lvl_comm
        glob_stats["nbr_trans_signs"]    += nbr_trans_signs
        if glob_stats["nbr_signs"] == 0:
            glob_stats["nbr_signs"] = nbr_signs
        glob_stats["contexts"] |= contexts

    lines = ("",
             "{:>6.1%} done! ({} translated messages over {}).\n"
             "".format(lvl, trans_msgs, tot_msgs),
             "{:>6.1%} of messages are tooltips ({} over {}).\n"
             "".format(lvl_ttips, tot_ttips, tot_msgs),
             "{:>6.1%} of tooltips are translated ({} over {}).\n"
             "".format(lvl_trans_ttips, trans_ttips, tot_ttips),
             "{:>6.1%} of translated messages are tooltips ({} over {}).\n"
             "".format(lvl_ttips_in_trans, trans_ttips, trans_msgs),
             "{:>6.1%} of messages are commented ({} over {}).\n"
             "".format(lvl_comm, comm_msgs, comm_msgs+tot_msgs),
             "This translation is currently made of {} signs.\n"
             "".format(nbr_trans_signs))
    print(prefix.join(lines))
    return 0

