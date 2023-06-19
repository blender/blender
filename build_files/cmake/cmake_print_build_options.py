# SPDX-FileCopyrightText: 2018-2023 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

# Simple utility that prints all WITH_* options in a CMakeLists.txt
# Called by 'make help_features'

import re
import sys

from typing import Optional

cmakelists_file = sys.argv[-1]


def count_backslashes_before_pos(file_data: str, pos: int) -> int:
    slash_count = 0
    pos -= 1
    while pos >= 0:
        if file_data[pos] != '\\':
            break
        pos -= 1
        slash_count += 1
    return slash_count


def extract_cmake_string_at_pos(file_data: str, pos_beg: int) -> Optional[str]:
    assert file_data[pos_beg - 1] == '"'

    pos = pos_beg
    # Dummy assignment.
    pos_end = pos_beg
    while True:
        pos_next = file_data.find('"', pos)
        if pos_next == -1:
            raise Exception("Un-terminated string (parse error?)")

        count_slashes = count_backslashes_before_pos(file_data, pos_next)
        if (count_slashes % 2) == 0:
            pos_end = pos_next
            # Found the closing quote.
            break

        # The quote was back-slash escaped, step over it.
        pos = pos_next + 1
        file_data[pos_next]

    assert file_data[pos_end] == '"'

    if pos_beg == pos_end:
        return None

    # See: https://cmake.org/cmake/help/latest/manual/cmake-language.7.html#escape-sequences
    text = file_data[pos_beg: pos_end].replace(
        # Handle back-slash literals.
        "\\\\", "\\",
    ).replace(
        # Handle tabs.
        "\\t", "\t",
    ).replace(
        # Handle escaped quotes.
        "\\\"", "\"",
    ).replace(
        # Handle tabs.
        "\\;", ";",
    ).replace(
        # Handle trailing newlines.
        "\\\n", "",
    )

    return text


def main() -> None:
    options = []
    with open(cmakelists_file, 'r', encoding="utf-8") as fh:
        file_data = fh.read()
        for m in re.finditer(r"^\s*option\s*\(\s*(WITH_[a-zA-Z0-9_]+)\s+(\")", file_data, re.MULTILINE):
            option_name = m.group(1)
            option_descr = extract_cmake_string_at_pos(file_data, m.span(2)[1])
            if option_descr is None:
                # Possibly a parsing error, at least show something.
                option_descr = "(UNDOCUMENTED)"
            options.append("{:s}: {:s}".format(option_name, option_descr))

    print('\n'.join(options))


if __name__ == "__main__":
    main()
