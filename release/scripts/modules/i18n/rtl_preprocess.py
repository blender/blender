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

# Preprocess right-to-left languages.
# You can use it either standalone, or through import_po_from_branches or
# update_trunk.
#
# Notes: This has been tested on Linux, not 100% it will work nicely on
#        Windows or OsX.
#        This uses ctypes, as there is no py3 binding for fribidi currently.
#        This implies you only need the compiled C library to run it.
#        Finally, note that it handles some formating/escape codes (like
#        \", %s, %x12, %.4f, etc.), protecting them from ugly (evil) fribidi,
#        which seems completely unaware of such things (as unicode is...).

import sys
import ctypes

import settings
import utils

FRIBIDI_LIB = settings.FRIBIDI_LIB

###### Import C library and recreate "defines". #####
fbd = ctypes.CDLL(FRIBIDI_LIB)


#define FRIBIDI_MASK_NEUTRAL	0x00000040L	/* Is neutral */
FRIBIDI_PAR_ON = 0x00000040


#define FRIBIDI_FLAG_SHAPE_MIRRORING	0x00000001
#define FRIBIDI_FLAG_REORDER_NSM	0x00000002

#define FRIBIDI_FLAG_SHAPE_ARAB_PRES	0x00000100
#define FRIBIDI_FLAG_SHAPE_ARAB_LIGA	0x00000200
#define FRIBIDI_FLAG_SHAPE_ARAB_CONSOLE	0x00000400

#define FRIBIDI_FLAG_REMOVE_BIDI	0x00010000
#define FRIBIDI_FLAG_REMOVE_JOINING	0x00020000
#define FRIBIDI_FLAG_REMOVE_SPECIALS	0x00040000

#define FRIBIDI_FLAGS_DEFAULT		( \
#	FRIBIDI_FLAG_SHAPE_MIRRORING	| \
#	FRIBIDI_FLAG_REORDER_NSM	| \
#	FRIBIDI_FLAG_REMOVE_SPECIALS	)

#define FRIBIDI_FLAGS_ARABIC		( \
#	FRIBIDI_FLAG_SHAPE_ARAB_PRES	| \
#	FRIBIDI_FLAG_SHAPE_ARAB_LIGA	)

FRIBIDI_FLAG_SHAPE_MIRRORING = 0x00000001
FRIBIDI_FLAG_REORDER_NSM = 0x00000002
FRIBIDI_FLAG_REMOVE_SPECIALS = 0x00040000

FRIBIDI_FLAG_SHAPE_ARAB_PRES = 0x00000100
FRIBIDI_FLAG_SHAPE_ARAB_LIGA = 0x00000200

FRIBIDI_FLAGS_DEFAULT = FRIBIDI_FLAG_SHAPE_MIRRORING | \
                        FRIBIDI_FLAG_REORDER_NSM | \
                        FRIBIDI_FLAG_REMOVE_SPECIALS

FRIBIDI_FLAGS_ARABIC = FRIBIDI_FLAG_SHAPE_ARAB_PRES | \
                       FRIBIDI_FLAG_SHAPE_ARAB_LIGA

##### Kernel processing funcs. #####
def protect_format_seq(msg):
    """
    Find some specific escaping/formating sequences (like \", %s, etc.,
    and protect them from any modification!
    """
    LRE = "\u202A"
    PDF = "\u202C"
    # Most likely incomplete, but seems to cover current needs.
    format_codes = set("tslfd")
    digits = set(".0123456789")

    idx = 0
    ret = []
    ln = len(msg)
    while idx < ln:
        dlt = 1
        # \" or \'
        if idx < (ln - 1) and msg[idx] == '\\' and msg[idx + 1] in "\"\'":
            dlt = 2
        # %x12
        elif idx < (ln - 2) and msg[idx] == '%' and msg[idx + 1] in "x" and \
             msg[idx + 2] in digits:
            dlt = 2
            while (idx + dlt + 1) < ln and msg[idx + dlt + 1] in digits:
                dlt += 1
        # %.4f
        elif idx < (ln - 3) and msg[idx] == '%' and msg[idx + 1] in digits:
            dlt = 2
            while (idx + dlt + 1) < ln and msg[idx + dlt + 1] in digits:
                dlt += 1
            if (idx + dlt + 1) < ln and msg[idx + dlt + 1] in format_codes:
                dlt += 1
            else:
                dlt = 1
        # %s
        elif idx < (ln - 1) and msg[idx] == '%' and \
             msg[idx + 1] in format_codes:
            dlt = 2

        if dlt > 1:
            ret.append(LRE)
        ret += msg[idx:idx + dlt]
        idx += dlt
        if dlt > 1:
            ret.append(PDF)

    return "".join(ret)


def log2vis(msgs):
    """
    Globally mimics deprecated fribidi_log2vis.
    msgs should be an iterable of messages to rtl-process.
    """
    for msg in msgs:
        msg = protect_format_seq(msg)

        fbc_str = ctypes.create_unicode_buffer(msg)
        ln = len(fbc_str) - 1
#        print(fbc_str.value, ln)
        btypes = (ctypes.c_int * ln)()
        embed_lvl = (ctypes.c_uint8 * ln)()
        pbase_dir = ctypes.c_int(FRIBIDI_PAR_ON)
        jtypes = (ctypes.c_uint8 * ln)()
        flags = FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAGS_ARABIC

        # Find out direction of each char.
        fbd.fribidi_get_bidi_types(fbc_str, ln, ctypes.byref(btypes))

#        print(*btypes)

        fbd.fribidi_get_par_embedding_levels(btypes, ln,
                                             ctypes.byref(pbase_dir),
                                             embed_lvl)

#        print(*embed_lvl)

        # Joinings for arabic chars.
        fbd.fribidi_get_joining_types(fbc_str, ln, jtypes)
#        print(*jtypes)
        fbd.fribidi_join_arabic(btypes, ln, embed_lvl, jtypes)
#        print(*jtypes)

        # Final Shaping!
        fbd.fribidi_shape(flags, embed_lvl, ln, jtypes, fbc_str)

#        print(fbc_str.value)
#        print(*(ord(c) for c in fbc_str))
        # And now, the reordering.
        # Note that here, we expect a single line, so no need to do
        # fancy things...
        fbd.fribidi_reorder_line(flags, btypes, ln, 0, pbase_dir, embed_lvl,
                                 fbc_str, None)
#        print(fbc_str.value)
#        print(*(ord(c) for c in fbc_str))

        yield fbc_str.value

##### Command line stuff. #####
def main():
    import argparse
    parser = argparse.ArgumentParser(description="" \
                    "Preprocesses right-to-left languages.\n" \
                    "You can use it either standalone, or through " \
                    "import_po_from_branches or update_trunk.\n\n" \
                    "Note: This has been tested on Linux, not 100% it will " \
                    "work nicely on Windows or OsX.\n" \
                    "Note: This uses ctypes, as there is no py3 binding for " \
                    "fribidi currently. This implies you only need the " \
                    "compiled C library to run it.\n" \
                    "Note: It handles some formating/escape codes (like " \
                    "\\\", %s, %x12, %.4f, etc.), protecting them from ugly " \
                    "(evil) fribidi, which seems completely unaware of such " \
                    "things (as unicode is...).")
    parser.add_argument('dst', metavar='dst.po',
                        help="The dest po into which write the " \
                             "pre-processed messages.")
    parser.add_argument('src', metavar='src.po',
                        help="The po's to pre-process messages.")
    args = parser.parse_args()


    msgs, state, u1 = utils.parse_messages(args.src)
    if state["is_broken"]:
        print("Source po is BROKEN, aborting.")
        return 1

    keys = []
    trans = []
    for key, val in msgs.items():
        keys.append(key)
        trans.append("".join(val["msgstr_lines"]))
    trans = log2vis(trans)
    for key, trn in zip(keys, trans):
        # Mono-line for now...
        msgs[key]["msgstr_lines"] = [trn]

    utils.write_messages(args.dst, msgs, state["comm_msg"], state["fuzzy_msg"])

    print("RTL pre-process completed.")
    return 0


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
