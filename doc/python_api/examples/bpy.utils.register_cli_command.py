"""
Custom Commands
---------------

Registering commands makes it possible to conveniently expose command line
functionality via commands passed to (``-c`` / ``--command``).
"""

import sys
import os


def sysinfo_command(argv):
    import tempfile
    import sys_info

    if argv and argv[0] == "--help":
        print("Print system information & exit!")
        return 0

    with tempfile.TemporaryDirectory() as tempdir:
        filepath = os.path.join(tempdir, "system_info.txt")
        sys_info.write_sysinfo(filepath)
        with open(filepath, "r", encoding="utf-8") as fh:
            sys.stdout.write(fh.read())
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
