# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# TODO: file-type icons are currently not setup.
# Currently `xdg-icon-resource` doesn't support SVG's, so we would need to generate PNG's.
# Or wait until SVG's are supported, see: https://gitlab.freedesktop.org/xdg/xdg-utils/-/merge_requests/41
#
# NOTE: Typically this will run from Blender, you may also run this directly from Python
# which can be useful for testing.

__all__ = (
    "register",
    "unregister",
)

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

from collections.abc import (
    Callable,
)

VERBOSE = True


# -----------------------------------------------------------------------------
# Environment

HOME_DIR = os.path.normpath(os.path.expanduser("~"))

# https://wiki.archlinux.org/title/XDG_Base_Directory
# Typically: `~/.local/share`.
XDG_DATA_HOME = os.environ.get("XDG_DATA_HOME") or os.path.join(HOME_DIR, ".local", "share")

HOMEDIR_LOCAL_BIN = os.path.join(HOME_DIR, ".local", "bin")

BLENDER_ENV = "bpy" in sys.modules

# -----------------------------------------------------------------------------
# Programs

# The command `xdg-mime` handles most of the file association actions.
XDG_MIME_PROG = shutil.which("xdg-mime") or ""

# Initialize by `bpy` or command line arguments.
BLENDER_BIN = ""
# Set to `os.path.dirname(BLENDER_BIN)`.
BLENDER_DIR = ""


# -----------------------------------------------------------------------------
# Path Constants

# These files are included along side a portable Blender installation.
BLENDER_DESKTOP = "blender.desktop"
# The target binary.
BLENDER_FILENAME = "blender"
# The target binary (thumbnailer).
BLENDER_THUMBNAILER_FILENAME = "blender-thumbnailer"


# -----------------------------------------------------------------------------
# Other Constants

# The mime type Blender users.
BLENDER_MIME = "application/x-blender"
# Use `/usr/local` because this is not managed by the systems package manager.
SYSTEM_PREFIX = "/usr/local"


# -----------------------------------------------------------------------------
# Utility Functions


# Display a short path, for nicer display only.
def filepath_repr(filepath: str) -> str:
    if filepath.startswith(HOME_DIR):
        return "~" + filepath[len(HOME_DIR):]
    return filepath


def system_path_contains(dirpath: str) -> bool:
    dirpath = os.path.normpath(dirpath)
    for path in os.environ.get("PATH", "").split(os.pathsep):
        # `$PATH` can include relative locations.
        path = os.path.normpath(os.path.abspath(path))
        if path == dirpath:
            return True
    return False


def filepath_ensure_removed(path: str) -> bool:
    # When removing files to make way for newly copied file an `os.path.exists`
    # check isn't sufficient as the path may be a broken symbolic-link.
    if os.path.lexists(path):
        os.remove(path)
        return True
    return False


# -----------------------------------------------------------------------------
# Handle Associations
#
# On registration when handlers return False this causes registration to fail and unregister to be called.
# Non fatal errors should print a message and return True instead.

def handle_bin(do_register: bool, all_users: bool) -> str | None:
    if all_users:
        dirpath_dst = os.path.join(SYSTEM_PREFIX, "bin")
    else:
        dirpath_dst = HOMEDIR_LOCAL_BIN

    if VERBOSE:
        sys.stdout.write("- {:s} symbolic-links in: {:s}\n".format(
            ("Setup" if do_register else "Remove"),
            filepath_repr(dirpath_dst),
        ))

    if do_register:
        if not all_users:
            if not system_path_contains(dirpath_dst):
                sys.stdout.write(
                    "The PATH environment variable doesn't contain \"{:s}\", not creating symlinks\n".format(
                        dirpath_dst,
                    ))
                # NOTE: this is not an error, don't consider it a failure.
                return None

        os.makedirs(dirpath_dst, exist_ok=True)

    # Full path, then name to create at the destination.
    files_to_link = [
        (BLENDER_BIN, BLENDER_FILENAME, False),
    ]

    blender_thumbnailer_src = os.path.join(BLENDER_DIR, BLENDER_THUMBNAILER_FILENAME)
    if os.path.exists(blender_thumbnailer_src):
        # Unfortunately the thumbnailer must be copied for `bwrap` to find it.
        files_to_link.append((blender_thumbnailer_src, BLENDER_THUMBNAILER_FILENAME, True))
    else:
        sys.stdout.write("  Thumbnailer not found, skipping: \"{:s}\"\n".format(blender_thumbnailer_src))

    for filepath_src, filename, do_full_copy in files_to_link:
        filepath_dst = os.path.join(dirpath_dst, filename)
        filepath_ensure_removed(filepath_dst)
        if not do_register:
            continue

        if not os.path.exists(filepath_src):
            sys.stderr.write("File not found, skipping link: \"{:s}\" -> \"{:s}\"\n".format(
                filepath_src, filepath_dst,
            ))
        if do_full_copy:
            shutil.copyfile(filepath_src, filepath_dst)
            os.chmod(filepath_dst, 0o755)
        else:
            os.symlink(filepath_src, filepath_dst)
    return None


def handle_desktop_file(do_register: bool, all_users: bool) -> str | None:
    # `cp ./blender.desktop ~/.local/share/applications/`

    filename = BLENDER_DESKTOP

    if all_users:
        base_dir = os.path.join(SYSTEM_PREFIX, "share")
    else:
        base_dir = XDG_DATA_HOME

    dirpath_dst = os.path.join(base_dir, "applications")

    filepath_desktop_src = os.path.join(BLENDER_DIR, filename)
    filepath_desktop_dst = os.path.join(dirpath_dst, filename)

    if VERBOSE:
        sys.stdout.write("- {:s} desktop-file: {:s}\n".format(
            ("Setup" if do_register else "Remove"),
            filepath_repr(filepath_desktop_dst),
        ))

    filepath_ensure_removed(filepath_desktop_dst)
    if not do_register:
        return None

    if not os.path.exists(filepath_desktop_src):
        # Unlike other missing things, this must be an error otherwise
        # the MIME association fails which is the main purpose of registering types.
        return "Error: desktop file not found: {:s}".format(filepath_desktop_src)

    os.makedirs(dirpath_dst, exist_ok=True)

    with open(filepath_desktop_src, "r", encoding="utf-8") as fh:
        data = fh.read()

    data = data.replace("\nExec=blender %f\n", "\nExec={:s} %f\n".format(BLENDER_BIN))

    with open(filepath_desktop_dst, "w", encoding="utf-8") as fh:
        fh.write(data)
    return None


def handle_thumbnailer(do_register: bool, all_users: bool) -> str | None:
    filename = "blender.thumbnailer"

    if all_users:
        base_dir = os.path.join(SYSTEM_PREFIX, "share")
    else:
        base_dir = XDG_DATA_HOME

    dirpath_dst = os.path.join(base_dir, "thumbnailers")
    filepath_thumbnailer_dst = os.path.join(dirpath_dst, filename)

    if VERBOSE:
        sys.stdout.write("- {:s} thumbnailer: {:s}\n".format(
            ("Setup" if do_register else "Remove"),
            filepath_repr(filepath_thumbnailer_dst),
        ))

    filepath_ensure_removed(filepath_thumbnailer_dst)
    if not do_register:
        return None

    blender_thumbnailer_bin = os.path.join(BLENDER_DIR, BLENDER_THUMBNAILER_FILENAME)
    if not os.path.exists(blender_thumbnailer_bin):
        sys.stderr.write("Thumbnailer not found, this may not be a portable installation: {:s}\n".format(
            blender_thumbnailer_bin,
        ))
        return None

    os.makedirs(dirpath_dst, exist_ok=True)

    # NOTE: unfortunately this can't be `blender_thumbnailer_bin` because GNOME calls the command
    # with wrapper that means the command *must* be in the users `$PATH`.
    # and it cannot be a SYMLINK.
    if shutil.which("bwrap") is not None:
        command = BLENDER_THUMBNAILER_FILENAME
    else:
        command = blender_thumbnailer_bin

    with open(filepath_thumbnailer_dst, "w", encoding="utf-8") as fh:
        fh.write("[Thumbnailer Entry]\n")
        fh.write("TryExec={:s}\n".format(command))
        fh.write("Exec={:s} %i %o\n".format(command))
        fh.write("MimeType={:s};\n".format(BLENDER_MIME))
    return None


def handle_mime_association_xml(do_register: bool, all_users: bool) -> str | None:
    # `xdg-mime install x-blender.xml`
    filename = "x-blender.xml"

    if all_users:
        base_dir = os.path.join(SYSTEM_PREFIX, "share")
    else:
        base_dir = XDG_DATA_HOME

    # Ensure directories exist `xdg-mime` will fail with an error if these don't exist.
    for dirpath_dst in (
            os.path.join(base_dir, "mime", "application"),
            os.path.join(base_dir, "mime", "packages")
    ):
        os.makedirs(dirpath_dst, exist_ok=True)
    del dirpath_dst

    # Unfortunately there doesn't seem to be a way to know the installed location.
    # Use hard-coded location.
    package_xml_dst = os.path.join(base_dir, "mime", "application", filename)

    if VERBOSE:
        sys.stdout.write("- {:s} mime type: {:s}\n".format(
            ("Setup" if do_register else "Remove"),
            filepath_repr(package_xml_dst),
        ))

    env = {
        **os.environ,
        "XDG_DATA_DIRS": os.path.join(SYSTEM_PREFIX, "share")
    }

    if not do_register:
        if not os.path.exists(package_xml_dst):
            return None
        # NOTE: `xdg-mime query default application/x-blender` could be used to check
        # if the XML is installed, however there is some slim chance the XML is installed
        # but the default doesn't point to Blender, just uninstall as it's harmless.
        cmd = (
            XDG_MIME_PROG,
            "uninstall",
            "--mode", "system" if all_users else "user",
            package_xml_dst,
        )
        subprocess.check_output(cmd, env=env)
        return None

    with tempfile.TemporaryDirectory() as tempdir:
        package_xml_src = os.path.join(tempdir, filename)
        with open(package_xml_src, mode="w", encoding="utf-8") as fh:
            fh.write("""<?xml version="1.0" encoding="UTF-8"?>\n""")
            fh.write("""<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">\n""")
            fh.write("""  <mime-type type="{:s}">\n""".format(BLENDER_MIME))
            # NOTE: not using a trailing full-stop seems to be the convention here.
            fh.write("""    <comment>Blender scene</comment>\n""")
            fh.write("""    <glob pattern="*.blend"/>\n""")
            # TODO: this doesn't seem to work, GNOME's Nautilus & KDE's Dolphin
            # already have a file-type icon for this so we might consider this low priority.
            if False:
                fh.write("""    <icon name="application-x-blender"/>\n""")
            fh.write("""  </mime-type>\n""")
            fh.write("""</mime-info>\n""")

        cmd = (
            XDG_MIME_PROG,
            "install",
            "--mode", "system" if all_users else "user",
            package_xml_src,
        )
        subprocess.check_output(cmd, env=env)
    return None


def handle_mime_association_default(do_register: bool, all_users: bool) -> str | None:
    # `xdg-mime default blender.desktop application/x-blender`

    if VERBOSE:
        sys.stdout.write("- {:s} mime type as default\n".format(
            ("Setup" if do_register else "Remove"),
        ))

    # NOTE: there doesn't seem to be a way to reverse this action.
    if not do_register:
        return None

    cmd = (
        XDG_MIME_PROG,
        "default",
        BLENDER_DESKTOP,
        BLENDER_MIME,
    )
    subprocess.check_output(cmd)
    return None


def handle_icon(do_register: bool, all_users: bool) -> str | None:
    filename = "blender.svg"
    if all_users:
        base_dir = os.path.join(SYSTEM_PREFIX, "share")
    else:
        base_dir = XDG_DATA_HOME

    dirpath_dst = os.path.join(base_dir, "icons", "hicolor", "scalable", "apps")

    filepath_desktop_src = os.path.join(BLENDER_DIR, filename)
    filepath_desktop_dst = os.path.join(dirpath_dst, filename)

    if VERBOSE:
        sys.stdout.write("- {:s} icon: {:s}\n".format(
            ("Setup" if do_register else "Remove"),
            filepath_repr(filepath_desktop_dst),
        ))

    filepath_ensure_removed(filepath_desktop_dst)
    if not do_register:
        return None

    if not os.path.exists(filepath_desktop_src):
        sys.stderr.write("  Icon file not found, skipping: \"{:s}\"\n".format(filepath_desktop_src))
        # Not an error.
        return None

    os.makedirs(dirpath_dst, exist_ok=True)

    with open(filepath_desktop_src, "rb") as fh:
        data = fh.read()

    with open(filepath_desktop_dst, "wb") as fh:
        fh.write(data)

    return None


# -----------------------------------------------------------------------------
# Escalate Privileges

def main_run_as_root(
        do_register: bool,
        *,
        python_args: tuple[str, ...],
) -> str | None:
    # If the system prefix doesn't exist, fail with an error because it's highly likely that the
    # system won't use this when it has not been created.
    if not os.path.exists(SYSTEM_PREFIX):
        return "Error: system path does not exist {!r}".format(SYSTEM_PREFIX)

    prog: str | None = shutil.which("pkexec")
    if prog is None:
        return "Error: command \"pkexec\" not found"

    python_args_extra = (
        # Skips users `site-packages` because they are only additional overhead for running this script.
        "-s",
    )

    python_args = (
        *python_args,
        *(arg for arg in python_args_extra if arg not in python_args)
    )

    cmd = [
        prog,
        sys.executable,
        *python_args,
        __file__,
        BLENDER_BIN,
        "--action={:s}".format("register-allusers" if do_register else "unregister-allusers"),
    ]
    if VERBOSE:
        sys.stdout.write("Executing: {:s}\n".format(shlex.join(cmd)))

    proc = subprocess.run(cmd, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        if proc.stderr:
            return proc.stderr.decode("utf-8", errors="surrogateescape")
        return "Error: pkexec returned non-zero returncode"

    return None


# -----------------------------------------------------------------------------
# Checked Call
#
# While exceptions should not happen, we can't entirely prevent this as it's always possible
# a file write fails or a command doesn't work as expected anymore.
# Handle these cases gracefully.

def call_handle_checked(
        fn: Callable[[bool, bool], str | None],
        *,
        do_register: bool,
        all_users: bool,
) -> str | None:
    try:
        result = fn(do_register, all_users)
    except Exception as ex:
        # This should never happen.
        result = "Internal Error: {!r}".format(ex)
    return result


# -----------------------------------------------------------------------------
# Main Registration Functions

def register_impl(do_register: bool, all_users: bool) -> str | None:
    # A non-empty string indicates an error (which is forwarded to the user), otherwise None for success.

    global BLENDER_BIN
    global BLENDER_DIR

    if BLENDER_ENV:
        # File association expects a "portable" build (see `WITH_INSTALL_PORTABLE` CMake option),
        # while it's possible support registering a "system" installation, the paths aren't located
        # relative to the blender binary and in general it's not needed because system installations
        # are used by package managers which can handle file association themselves.
        # The Linux builds provided by https://blender.org are portable, register is intended to be used for these.
        if not __import__("bpy").app.portable:
            return "System Installation, registration is handled by the package manager"
        # While snap builds are portable, the snap system handled file associations.
        # Blender is also launched via a wrapper, again, we could support this if it were
        # important but we can rely on the snap packaging in this case.
        if os.environ.get("SNAP"):
            return "Snap Package Installation, registration is handled by the package manager"

    if BLENDER_ENV:
        # Only use of `bpy`.
        BLENDER_BIN = os.path.normpath(__import__("bpy").app.binary_path)

        # Running inside Blender, detect the need for privilege escalation (which will run outside of Blender).
        if all_users:
            if os.geteuid() != 0:
                # Run this script with escalated privileges.
                return main_run_as_root(
                    do_register,
                    python_args=__import__("bpy").app.python_args,
                )
    else:
        assert BLENDER_BIN != ""

    BLENDER_DIR = os.path.dirname(BLENDER_BIN)

    if all_users:
        if not os.access(SYSTEM_PREFIX, os.W_OK):
            return "Error: {:s} not writable, this command may need to run as a superuser!".format(SYSTEM_PREFIX)

    if VERBOSE:
        sys.stdout.write("{:s}: {:s}\n".format("Register" if do_register else "Unregister", BLENDER_BIN))

    if XDG_MIME_PROG == "":
        return "Could not find \"xdg-mime\", unable to associate mime-types"

    handlers = (
        handle_bin,
        handle_icon,
        handle_desktop_file,
        handle_mime_association_xml,
        # This only makes sense for users, although there may be a way to do this for all users.
        *(() if all_users else (handle_mime_association_default,)),
        # The thumbnailer only works when installed for all users.
        *((handle_thumbnailer,) if all_users else ()),
    )

    error_or_none = None
    for i, fn in enumerate(handlers):
        if (error_or_none := call_handle_checked(fn, do_register=do_register, all_users=all_users)) is not None:
            break

    if error_or_none is not None:
        # Roll back registration on failure.
        if do_register:
            for fn in reversed(handlers[:i + 1]):
                error_or_none_reverse = call_handle_checked(fn, do_register=False, all_users=all_users)
                if error_or_none_reverse is not None:
                    sys.stdout.write("Error reverting action: {:s}\n".format(error_or_none_reverse))

        # Print to the `stderr`, in case the user has a console open, it can be helpful
        # especially if it's multi-line.
        sys.stdout.write("{:s}\n".format(error_or_none))

    return error_or_none


def register(all_users: bool = False) -> str | None:
    # Return an empty string for success.
    return register_impl(True, all_users)


def unregister(all_users: bool = False) -> str | None:
    # Return an empty string for success.
    return register_impl(False, all_users)


# -----------------------------------------------------------------------------
# Running directly (Escalated Privileges)
#
# Needed when running as an administer.

register_actions = {
    "register": (True, False),
    "unregister": (False, False),
    "register-allusers": (True, True),
    "unregister-allusers": (False, True),
}


def argparse_create() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "blender_bin",
        metavar="BLENDER_BIN",
        type=str,
        help="The location of Blender's binary",
    )

    parser.add_argument(
        "--action",
        choices=register_actions.keys(),
        dest="register_action",
        required=True,
    )

    return parser


def main() -> int:
    global BLENDER_BIN
    assert BLENDER_BIN == ""
    args = argparse_create().parse_args()
    BLENDER_BIN = args.blender_bin
    do_register, all_users = register_actions[args.register_action]

    if do_register:
        result = register(all_users=all_users)
    else:
        result = unregister(all_users=all_users)

    if result:
        sys.stderr.write("{:s}\n".format(result))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
