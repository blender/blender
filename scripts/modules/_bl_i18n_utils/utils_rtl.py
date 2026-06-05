#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Pre-process right-to-left languages.
# You can use it either standalone, or through import_po_from_branches or
# update_trunk.
#
# Notes: This has been tested on Linux, not 100% it will work nicely on
#        Windows or OsX.
#        This uses ctypes, as there is no py3 binding for fribidi currently.
#        This implies you only need the compiled C library to run it.
#        Finally, note that it handles some formatting/escape codes (like
#        \", %s, %x12, %.4f, etc.), protecting them from ugly (evil) fribidi,
#        which seems completely unaware of such things (as unicode is...).

import ctypes
import re


# define FRIBIDI_MASK_NEUTRAL    0x00000040L /* Is neutral */
FRIBIDI_PAR_ON = 0x00000040


# define FRIBIDI_FLAG_SHAPE_MIRRORING    0x00000001
# define FRIBIDI_FLAG_REORDER_NSM    0x00000002

# define FRIBIDI_FLAG_SHAPE_ARAB_PRES    0x00000100
# define FRIBIDI_FLAG_SHAPE_ARAB_LIGA    0x00000200
# define FRIBIDI_FLAG_SHAPE_ARAB_CONSOLE 0x00000400

# define FRIBIDI_FLAG_REMOVE_BIDI    0x00010000
# define FRIBIDI_FLAG_REMOVE_JOINING 0x00020000
# define FRIBIDI_FLAG_REMOVE_SPECIALS    0x00040000

# define FRIBIDI_FLAGS_DEFAULT       ( \
#   FRIBIDI_FLAG_SHAPE_MIRRORING    | \
#   FRIBIDI_FLAG_REORDER_NSM    | \
#   FRIBIDI_FLAG_REMOVE_SPECIALS    )

# define FRIBIDI_FLAGS_ARABIC        ( \
#   FRIBIDI_FLAG_SHAPE_ARAB_PRES    | \
#   FRIBIDI_FLAG_SHAPE_ARAB_LIGA    )

FRIBIDI_FLAG_SHAPE_MIRRORING = 0x00000001
FRIBIDI_FLAG_REORDER_NSM = 0x00000002
FRIBIDI_FLAG_REMOVE_SPECIALS = 0x00040000

FRIBIDI_FLAG_SHAPE_ARAB_PRES = 0x00000100
FRIBIDI_FLAG_SHAPE_ARAB_LIGA = 0x00000200

FRIBIDI_FLAGS_DEFAULT = FRIBIDI_FLAG_SHAPE_MIRRORING | FRIBIDI_FLAG_REORDER_NSM | FRIBIDI_FLAG_REMOVE_SPECIALS

FRIBIDI_FLAGS_ARABIC = FRIBIDI_FLAG_SHAPE_ARAB_PRES | FRIBIDI_FLAG_SHAPE_ARAB_LIGA


MENU_DETECT_REGEX = re.compile("%x\\d+\\|")


##### Kernel processing functions. #####
def protect_format_seq(msg):
    """
    Find some specific escaping/formatting sequences (like \", %s, etc.,
    and protect them from any modification!

    NOTE: This is not covering all exotic 'printf' formatting cases!
    It also only covers the minimal `{}` syntax for the modern `format` syntax.
    """
#    LRM = "\u200E"
#    RLM = "\u200F"
    LRE = "\u202A"
#    RLE = "\u202B"
    PDF = "\u202C"
    LRO = "\u202D"
#    RLO = "\u202E"
    # uctrl = {LRE, RLE, PDF, LRO, RLO}

    # 'printf' format, from https://cplusplus.com/reference/cstdio/printf/
    printf_format_flags = set("-+ #0")
    printf_format_widthprec = set(".0123456789")  # For width and precision.
    printf_format_datasize = set("hljztL")
    printf_format_codes = set("diuoxXfFeEgGaAcsp")
    # 'fmt::format' (and Python 'format()'),
    # see https://fmt.dev/12.0/syntax/ and https://docs.python.org/3.13/library/string.html#formatstrings
    fmt_format_widthprec = set(".0123456789")  # For width and precision.
    fmt_format_codes = set("aAbBcdeEfFgGnopsxX?%")

    if not msg:
        return msg
    elif MENU_DETECT_REGEX.search(msg):
        # An ugly "menu" message, just force it whole LRE if not yet done.
        if msg[0] not in {LRE, LRO}:
            msg = LRE + msg

    idx = 0
    ret = []
    ln = len(msg)
    while idx < ln:
        dlt = 1
#        # If we find a control char, skip any additional protection!
#        if msg[idx] in uctrl:
#            ret.append(msg[idx:])
#            break
        # \", \' or \\
        if idx < (ln - 1) and msg[idx] == '\\' and msg[idx + 1] in "\"\'\\":
            dlt = 2
        elif idx < (ln - 1) and msg[idx] == '{':
            # The whole 'format' syntax...
            # Coverage of this one is still fairly limited and basic currently.
            # TODO: suport more of the 'format' mini-language (and check how much fmt::format matches with Python's).
            orig_dlt = dlt
            valid_format = False

            # {3}, {scale} (positional indicator or named reference)
            while (idx + dlt) < ln and msg[idx + dlt].isalnum():
                dlt += 1
            if (idx + dlt) < ln and msg[idx + dlt] == ":":
                dlt += 1
            # {:.4}, {:6d}, ...
            while (idx + dlt) < ln and msg[idx + dlt] in fmt_format_widthprec:
                dlt += 1
            # {:f}, {:s}, ...
            while (idx + dlt) < ln and msg[idx + dlt] in fmt_format_codes:
                dlt += 1
            if (idx + dlt) < ln and msg[idx + dlt] == "}":
                dlt += 1
                valid_format = True

            if not valid_format:
                dlt = orig_dlt
        # %%
        elif idx < (ln - 1) and msg[idx] == '%' and msg[idx + 1] == '%':
            dlt = 2
        elif idx < (ln - 1) and msg[idx] == '%':
            # The whole 'printf' syntax...
            # Not fully covering the format, but most of it, and should cover all Blender usages.
            orig_dlt = dlt
            valid_format = False

            # %x12| - What is this for actually?
            if idx < (ln - 2) and msg[idx + 1] in "x" and msg[idx + 2] in printf_format_widthprec:
                dlt = 2
                while (idx + dlt) < ln and msg[idx + dlt] in printf_format_widthprec:
                    dlt += 1
                if (idx + dlt) < ln and msg[idx + dlt] == '|':
                    dlt += 1
                    valid_format = True
            else:
                # %+d, %-40s, ...
                while (idx + dlt) < ln and msg[idx + dlt] in printf_format_flags:
                    dlt += 1
                # %.4f, %6d, ...
                while (idx + dlt) < ln and msg[idx + dlt] in printf_format_widthprec:
                    dlt += 1
                # %lld, %zu, ...
                while (idx + dlt) < ln and msg[idx + dlt] in printf_format_datasize:
                    dlt += 1
                # %s, %d, ...
                if (idx + dlt) < ln and msg[idx + dlt] in printf_format_codes:
                    dlt += 1
                    valid_format = True

            if not valid_format:
                dlt = orig_dlt

        if dlt > 1:
            ret.append(LRE)
        ret += msg[idx:idx + dlt]
        idx += dlt
        if dlt > 1:
            ret.append(PDF)

    return "".join(ret)


def log2vis(msgs, settings):
    """
    Globally mimics deprecated fribidi_log2vis.
    msgs should be an iterable of messages to RTL-process.
    """
    fbd = ctypes.CDLL(settings.FRIBIDI_LIB)

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

        fbd.fribidi_get_par_embedding_levels(
            btypes, ln,
            ctypes.byref(pbase_dir),
            embed_lvl,
        )

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
