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

# Merge one or more .po files into the first dest one.
# If a msgkey is present in more than one merged po, the one in the first file wins, unless 
# it’s marked as fuzzy and one later is not.
# The fuzzy flag is removed if necessary.
# All other comments are never modified.
# However, commented messages in dst will always remain commented, and commented messages are
# never merged from sources.

import sys

try:
    import settings
    import utils
except:
    from . import (settings, utils)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="" \
                    "Merge one or more .po files into the first dest one.\n" \
                    "If a msgkey (msgctxt, msgid) is present in more than " \
                    "one merged po, the one in the first file wins, unless " \
                    "it’s marked as fuzzy and one later is not.\n" \
                    "The fuzzy flag is removed if necessary.\n" \
                    "All other comments are never modified.\n" \
                    "Commented messages in dst will always remain " \
                    "commented, and commented messages are never merged " \
                    "from sources.")
    parser.add_argument('-s', '--stats', action="store_true",
                        help="Show statistics info.")
    parser.add_argument('-r', '--replace', action="store_true",
                        help="Replace existing messages of same \"level\" already in dest po.")
    parser.add_argument('dst', metavar='dst.po',
                        help="The dest po into which merge the others.")
    parser.add_argument('src', metavar='src.po', nargs='+',
                        help="The po's to merge into the dst.po one.")
    args = parser.parse_args()


    ret = 0
    done_msgkeys = set()
    done_fuzzy_msgkeys = set()
    nbr_merged = 0
    nbr_replaced = 0
    nbr_added = 0
    nbr_unfuzzied = 0

    dst_messages, dst_states, dst_stats = utils.parse_messages(args.dst)
    if dst_states["is_broken"]:
        print("Dest po is BROKEN, aborting.")
        return 1
    if args.stats:
        print("Dest po, before merging:")
        utils.print_stats(dst_stats, prefix="\t")
    # If we don’t want to replace existing valid translations, pre-populate
    # done_msgkeys and done_fuzzy_msgkeys.
    if not args.replace:
        done_msgkeys =  dst_states["trans_msg"].copy()
        done_fuzzy_msgkeys = dst_states["fuzzy_msg"].copy()
    for po in args.src:
        messages, states, stats = utils.parse_messages(po)
        if states["is_broken"]:
            print("\tSrc po {} is BROKEN, skipping.".format(po))
            ret = 1
            continue
        print("\tMerging {}...".format(po))
        if args.stats:
            print("\t\tMerged po stats:")
            utils.print_stats(stats, prefix="\t\t\t")
        for msgkey, val in messages.items():
            msgctxt, msgid = msgkey
            # This msgkey has already been completely merged, or is a commented one,
            # or the new message is commented, skip it.
            if msgkey in (done_msgkeys | dst_states["comm_msg"] | states["comm_msg"]):
                continue
            is_ttip = utils.is_tooltip(msgid)
            # New messages does not yet exists in dest.
            if msgkey not in dst_messages:
                dst_messages[msgkey] = messages[msgkey]
                if msgkey in states["fuzzy_msg"]:
                    done_fuzzy_msgkeys.add(msgkey)
                    dst_states["fuzzy_msg"].add(msgkey)
                elif msgkey in states["trans_msg"]:
                    done_msgkeys.add(msgkey)
                    dst_states["trans_msg"].add(msgkey)
                    dst_stats["trans_msg"] += 1
                    if is_ttip:
                        dst_stats["trans_ttips"] += 1
                nbr_added += 1
                dst_stats["tot_msg"] += 1
                if is_ttip:
                    dst_stats["tot_ttips"] += 1
            # From now on, the new messages is already in dst.
            # New message is neither translated nor fuzzy, skip it.
            elif msgkey not in (states["trans_msg"] | states["fuzzy_msg"]):
                continue
            # From now on, the new message is either translated or fuzzy!
            # The new message is translated.
            elif msgkey in states["trans_msg"]:
                dst_messages[msgkey]["msgstr_lines"] = messages[msgkey]["msgstr_lines"]
                done_msgkeys.add(msgkey)
                done_fuzzy_msgkeys.discard(msgkey)
                if msgkey in dst_states["fuzzy_msg"]:
                    dst_states["fuzzy_msg"].remove(msgkey)
                    nbr_unfuzzied += 1
                if msgkey not in dst_states["trans_msg"]:
                    dst_states["trans_msg"].add(msgkey)
                    dst_stats["trans_msg"] += 1
                    if is_ttip:
                        dst_stats["trans_ttips"] += 1
                else:
                    nbr_replaced += 1
                nbr_merged += 1
            # The new message is fuzzy, org one is fuzzy too,
            # and this msgkey has not yet been merged.
            elif msgkey not in (dst_states["trans_msg"] | done_fuzzy_msgkeys):
                dst_messages[msgkey]["msgstr_lines"] = messages[msgkey]["msgstr_lines"]
                done_fuzzy_msgkeys.add(msgkey)
                dst_states["fuzzy_msg"].add(msgkey)
                nbr_merged += 1
                nbr_replaced += 1

    utils.write_messages(args.dst, dst_messages, dst_states["comm_msg"], dst_states["fuzzy_msg"])

    print("Merged completed. {} messages were merged (among which {} were replaced), " \
          "{} were added, {} were \"un-fuzzied\"." \
          "".format(nbr_merged, nbr_replaced, nbr_added, nbr_unfuzzied))
    if args.stats:
        print("Final merged po stats:")
        utils.print_stats(dst_stats, prefix="\t")
    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
