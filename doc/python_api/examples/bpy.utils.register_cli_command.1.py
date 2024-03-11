"""
**Using Python Argument Parsing**

This example shows how the Python ``argparse`` module can be used with a custom command.

Using ``argparse`` is generally recommended as it has many useful utilities and
generates a ``--help`` message for your command.
"""

import os
import sys

import bpy


def argparse_create():
    import argparse

    parser = argparse.ArgumentParser(
        prog=os.path.basename(sys.argv[0]) + " --command keyconfig_export",
        description="Write key-configuration to a file.",
    )

    parser.add_argument(
        "-o", "--output",
        dest="output",
        metavar='OUTPUT',
        type=str,
        help="The path to write the keymap to.",
        required=True,
    )

    parser.add_argument(
        "-a", "--all",
        dest="all",
        action="store_true",
        help="Write all key-maps (not only customized key-maps).",
        required=False,
    )

    return parser


def keyconfig_export(argv):
    parser = argparse_create()
    args = parser.parse_args(argv)

    # Ensure the key configuration is loaded in background mode.
    bpy.utils.keyconfig_init()

    bpy.ops.preferences.keyconfig_export(
        filepath=args.output,
        all=args.all,
    )

    return 0


cli_commands = []


def register():
    cli_commands.append(bpy.utils.register_cli_command("keyconfig_export", keyconfig_export))


def unregister():
    for cmd in cli_commands:
        bpy.utils.unregister_cli_command(cmd)
    cli_commands.clear()


if __name__ == "__main__":
    register()
