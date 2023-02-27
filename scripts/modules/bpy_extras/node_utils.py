# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "find_node_input",
)


# XXX Names are not unique. Returns the first match.
def find_node_input(node, name):
    for input in node.inputs:
        if input.name == name:
            return input

    return None
