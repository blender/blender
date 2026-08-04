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

from _bl_i18n_utils import utils_format
from _bl_i18n_utils.utils_format import FormatToken


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

    # Current position in the text parsing.
    idx = 0
    # Number of tokens already processed, used to generate token's keys (index based on their order of appearance)
    # when no explicit key is specified in the token itself. Currently unused here.
    token_idx = 0
    # Amount of chars to skip (keep unmodified) from current `idx`,
    # before protecting the next LtR block with unicode characters.
    # Typically 'regular' RtL text.
    stride = 0
    # Length of the next detected block of text to protect as LtR, starting at `idx + stride`.
    ltr_len = 0
    ret = []
    ln = len(msg)
    has_remaining_format = True
    has_remaining_escape = True
    last_idx_escape = -2
    while idx < ln:
        next_format_candidate = -1
        next_escape_candidate = -1
        if has_remaining_format:
            next_format_candidate = FormatToken.next_potential_formatting_index(msg, idx)
            if next_format_candidate == -1:
                has_remaining_format = False
        if has_remaining_escape:
            next_escape_candidate = msg.find('\\', idx)
            if next_escape_candidate == -1:
                has_remaining_escape = False
        assert next_format_candidate != next_escape_candidate or next_format_candidate == -1

        stride = 0
        ltr_len = 0

#        # If we find a control char, skip any additional protection!
#        if msg[idx] in uctrl:
#            ret.append(msg[idx:])
#            break
        if not (has_remaining_format or has_remaining_escape):
            stride = len(msg[idx:])
        elif has_remaining_escape and (not has_remaining_format or next_escape_candidate < next_format_candidate):
            # \\, \', \"
            idx_esc = next_escape_candidate
            if idx_esc < (ln - 1) and msg[idx_esc] == '\\' and msg[idx_esc + 1] in '\\\'"':
                stride = idx_esc - idx
                ltr_len = 2
                last_idx_escape = idx_esc
            else:
                # Potential next escape is not a valid one, stride past it.
                stride = idx_esc - idx + 1
        elif has_remaining_format and (not has_remaining_escape or next_format_candidate < next_escape_candidate):
            idx_fmt = next_format_candidate
            # %%, {{, }}
            if idx_fmt < (ln - 1) and msg[idx_fmt] in '%{}' and msg[idx_fmt + 1] == msg[idx_fmt]:
                stride = idx_fmt - idx
                ltr_len = 2
            else:
                # Formatting tokens (%s, {:.4f}, etc.).
                # Find if potential next token is actually a valid one.
                token = FormatToken.parse_string_lookup_first_token(
                    msg, start_idx=idx_fmt, token_idx=token_idx, only_at_start_idx=True)
                if token is not None:
                    assert token.start_index == idx_fmt
                    tk_start_index = token.start_index
                    tk_len = len(token.token)
                    # Also attempt to make a potential formatting token inside of quotes ('%s' etc.) part of a single
                    # block.
                    # NOTE: All escape groups currently are two chars long, so knowing the start index of the last
                    # processed escape group is enough to avoid wrongly including e.g. the '"' with the '%s' in
                    # unlikely cases like this: `'foo\"%s" bar'`
                    if (tk_start_index > idx and tk_start_index > last_idx_escape + 2 and
                            (tk_start_index + tk_len) < ln and
                            msg[tk_start_index - 1] in '\'"' and
                            msg[tk_start_index + tk_len] == msg[tk_start_index - 1]
                            ):
                        stride = token.start_index - idx - 1
                        ltr_len = len(token.token) + 2
                    else:
                        stride = token.start_index - idx
                        ltr_len = len(token.token)
                    token_idx += 1
                else:
                    # Potential next token is not a valid one, stride past it.
                    stride = next_format_candidate - idx + 1

        if stride > 0:
            ret.append(msg[idx:idx + stride])
            idx += stride
        if ltr_len > 0:
            ret.append(LRE)
            ret.append(msg[idx:idx + ltr_len])
            idx += ltr_len
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
