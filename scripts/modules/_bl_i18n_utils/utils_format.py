#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Parse 'printf' and 'format' formatting tokens in a given string.
#
# used both by the general 'sanity' validation code for translated messages
# (as broken fromatting items in translations can easily crash Blender), and
# the LTR code pre-processing languages like Arabic or Hebrew.

import re


class FormatToken:
    # 'printf' format, from https://cplusplus.com/reference/cstdio/printf/
    # Note: Do not add the ' ' (space) flag here,
    #       it generates too much false positives with non-formatted strings like '75% of' ...
    printf_format_flags = set("-+#0")
    printf_format_widthprec = set(".0123456789")  # For width and precision.
    printf_format_datasize = set("hljztL")
    printf_format_codes = set("diuoxXfFeEgGaAcsp")
    # 'fmt::format' (and Python 'format()'),
    # see https://fmt.dev/12.0/syntax/ and https://docs.python.org/3.13/library/string.html#formatstrings
    fmt_format_widthprec = set(".0123456789")  # For width and precision.
    fmt_format_codes = set("aAbBcdeEfFgGnopsxX?%")

    # Note: We also need to consider a closing `}` here, to handle properly the escaping `}}` case.
    quick_re_check = re.compile(r"%|{|}")

    __slots__ = [
        "token",
        "key",
        "start_index",
    ]

    def __init__(self, token="", key=..., start_index=-1):
        # The formatting token itself
        self.token = token
        # The key (by default, index based on order in parsed string,
        # but can also be an explicit index or name with the modern 'format syntax'
        self.key = key
        # The position of the token in the parsed string.
        self.start_index = start_index

    def __repr__(self):
        return f"FormatToken(token={self.token!s}, key={self.key!s}, start_index={self.start_index})"

    @classmethod
    def has_potential_formatting(cls, string, start=0):
        return cls.quick_re_check.search(string, start) is not None

    @classmethod
    def next_potential_formatting_index(cls, string, start=0):
        match = cls.quick_re_check.search(string, start)
        if match is None:
            return -1
        return match.start()

    @classmethod
    def parse_string_lookup_first_token(cls, string, start_idx, token_idx=0, only_at_start_idx=False):
        """
        Search for the first instance of a formatting sequences (like %s, {:.4f}, etc.) in the given string,
        and return a FormatToken for it (or None is none is found).

        NOTE: This is not covering all exotic syntax cases of 'printf' or 'format'!

        If `only_at_start_idx` is True, this function will only return a valid token if it starts at given `start_idx`.
        Useful e.g. for code already calling `next_potential_formatting_index` itself.
        """
        # Current position in the parsed text.
        idx = start_idx
        # Amount of chars to skip (keep unmodified) from current `idx`,
        # before reaching the next token.
        stride = 0
        # Length of the next detected block of text to protect as LtR, starting at `idx + stride`.
        tk_len = 0
        ln = len(string)
        while idx < ln:
            next_format_candidate = -1
            next_format_candidate = FormatToken.next_potential_formatting_index(string, idx)
            if next_format_candidate == -1:
                return None
            if only_at_start_idx and next_format_candidate != start_idx:
                return None

            stride = 0
            tk_len = 0
            is_valid_format = False

            idx_fmt = next_format_candidate
            key = ...
            # `{{`/`}}` are escaping `{`/`}` in 'format' syntax.
            if idx_fmt < (ln - 1) and string[idx_fmt] in '{}' and string[idx_fmt + 1] == string[idx_fmt]:
                stride = idx_fmt - idx + 2
            elif idx_fmt < (ln - 1) and string[idx_fmt] == '{':
                # The whole 'format' syntax (both from C++ `fmt::format` and Python).
                # Coverage of this one is still fairly limited and basic currently.
                # TODO: support more of the 'format' mini-language.
                orig_tk_len = tk_len
                tk_len = 1
                is_valid_format = False
                has_explicit_key = False

                # {3}, {scale} (positional indicator or named reference).
                while ((idx_fmt + tk_len) < ln and string[idx_fmt + tk_len].isascii() and
                       (string[idx_fmt + tk_len].isalnum() or string[idx_fmt + tk_len] == '_')):
                    has_explicit_key = True
                    tk_len += 1
                if (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] == ':':
                    if has_explicit_key:
                        key = string[idx_fmt + 1:idx_fmt + tk_len]
                        if (key.isdecimal()):
                            key = int(key)
                    tk_len += 1
                # {:.4}, {:6d}, ...
                while (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.fmt_format_widthprec:
                    tk_len += 1
                # {:f}, {:s}, ...
                while (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.fmt_format_codes:
                    tk_len += 1
                if (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] == '}':
                    # {my_key}, {2}...
                    if key is ...:
                        if has_explicit_key:
                            key = string[idx_fmt + 1: idx_fmt + tk_len]
                            if (key.isdecimal()):
                                key = int(key)
                        else:
                            key = token_idx
                    tk_len += 1
                    is_valid_format = True
                    stride = idx_fmt - idx
                if not is_valid_format:
                    key = ...
                    tk_len = orig_tk_len
                    # Stride past the invalid potential next format token.
                    stride = idx_fmt - idx + 1
            # `%%` is escaping `%` in 'printf' syntax.
            elif idx_fmt < (ln - 1) and string[idx_fmt] == '%' and string[idx_fmt + 1] == '%':
                stride = idx_fmt - idx + 2
            elif idx_fmt < (ln - 1) and string[idx_fmt] == '%':
                # The whole 'printf' syntax...
                # Not fully covering the format, but most of it, and should cover all of Blender usages.
                orig_tk_len = tk_len
                tk_len = 1
                is_valid_format = False

                # `%x12|` - What is this for actually?
                # It also 'steals' the standard printf format for hexadecimal prints...
                if (idx_fmt < (ln - 2) and string[idx_fmt + 1] == 'x' and
                        string[idx_fmt + 2] in cls.printf_format_widthprec
                    ):
                    tk_len = 2
                    while (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.printf_format_widthprec:
                        tk_len += 1
                    if (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] == '|':
                        tk_len += 1
                        key = token_idx
                        is_valid_format = True
                        stride = idx_fmt - idx
                else:
                    # `%+d, %-40s`, ...
                    while (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.printf_format_flags:
                        tk_len += 1
                    # `%.4f, %6d`, ...
                    while (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.printf_format_widthprec:
                        tk_len += 1
                    # `%lld, %zu`, ...
                    while (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.printf_format_datasize:
                        tk_len += 1
                    # `%s, %d`, ...
                    if (idx_fmt + tk_len) < ln and string[idx_fmt + tk_len] in cls.printf_format_codes:
                        tk_len += 1
                        key = token_idx
                        is_valid_format = True
                        stride = idx_fmt - idx
                if not is_valid_format:
                    key = ...
                    tk_len = orig_tk_len
                    # Stride past the invalid potential next format token.
                    stride = idx_fmt - idx + 1
            else:
                # Stride past the invalid potential next format token.
                stride = idx_fmt - idx + 1

            if only_at_start_idx and not (is_valid_format and stride == 0):
                return None

            assert is_valid_format or tk_len == 0
            if stride > 0:
                idx += stride
            if is_valid_format:
                return FormatToken(string[idx:idx + tk_len], key, idx)
        return None

    @classmethod
    def parse_string(cls, string):
        """
        Iterator over specific formatting sequences (like %s, {:.4f}, etc.) in the given string,
        yielding instances of FormatToken.

        NOTE: This is not covering all exotic syntax cases of 'printf' or 'format'!
        """
        if not cls.has_potential_formatting(string):
            return
        idx = 0
        token_idx = 0
        ln = len(string)
        while idx < ln:
            token = FormatToken.parse_string_lookup_first_token(string, idx, token_idx)
            if token is None:
                return
            yield token
            idx = token.start_index + len(token.token)
            token_idx += 1
            if not cls.has_potential_formatting(string[idx:]):
                return
