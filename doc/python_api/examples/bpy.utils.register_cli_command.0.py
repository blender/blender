"""
**Custom Commands**

Registering commands makes it possible to conveniently expose command line
functionality via commands passed to (``-c`` / ``--command``).
"""

import os

import bpy


def sysinfo_print():
    """
    Report basic system information.
    """

    import pprint
    import platform
    import textwrap

    width = 80
    indent = 2

    print("Blender {:s}".format(bpy.app.version_string))
    print("Running on: {:s}-{:s}".format(platform.platform(), platform.machine()))
    print("Processors: {!r}".format(os.cpu_count()))
    print()

    # Dump `bpy.app`.
    for attr in dir(bpy.app):
        if attr.startswith("_"):
            continue
        # Overly verbose.
        if attr in {"handlers", "build_cflags", "build_cxxflags"}:
            continue

        value = getattr(bpy.app, attr)
        if attr.startswith("build_"):
            pass
        elif isinstance(value, tuple):
            pass
        else:
            # Otherwise ignore.
            continue

        if isinstance(value, bytes):
            value = value.decode("utf-8", errors="ignore")

        if isinstance(value, str):
            pass
        elif isinstance(value, tuple) and hasattr(value, "__dir__"):
            value = {
                attr_sub: value_sub
                for attr_sub in dir(value)
                # Exclude built-ins.
                if not attr_sub.startswith(("_", "n_"))
                # Exclude methods.
                if not callable(value_sub := getattr(value, attr_sub))
            }
            value = pprint.pformat(value, indent=0, width=width)
        else:
            value = pprint.pformat(value, indent=0, width=width)

        print("{:s}:\n{:s}\n".format(attr, textwrap.indent(value, " " * indent)))


def sysinfo_command(argv):
    if argv and argv[0] == "--help":
        print("Print system information & exit!")
        return 0

    sysinfo_print()
    return 0


cli_commands = []


def register():
    cli_commands.append(bpy.utils.register_cli_command("sysinfo", sysinfo_command))


def unregister():
    for cmd in cli_commands:
        bpy.utils.unregister_cli_command(cmd)
    cli_commands.clear()


if __name__ == "__main__":
    register()
