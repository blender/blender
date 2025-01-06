#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Wrapper for Blender that launches a graphical instances of Blender
in its own display-server.

This can be useful when a graphical context is required (when ``--background`` can't be used)
and it's preferable not to have windows opening on the user's system.

The main use case for this is tests that run simulated events, see: ``bl_run_operators_event_simulate.py``.

- All arguments are forwarded to Blender.
- Headless operation checks for environment variables.
- Blender's exit code is used on exit.

Environment Variables:

- ``BLENDER_BIN``: the Blender binary to run.
  (defaults to ``blender`` which must be in the ``PATH``).
- ``USE_WINDOW``: When nonzero:
  Show the window (not actually headless).
  Useful for troubleshooting so it's possible to see the contents of the window.
  Note that using a window causes WAYLAND to define a "seat",
  where the headless session doesn't define a seat.
- ``USE_DEBUG``: When nonzero:
  Run Blender in a debugger.

WAYLAND Environment Variables:

- ``WESTON_BIN``: The weston binary to run,
  (defaults to ``weston`` which must be in the ``PATH``).
- ``WAYLAND_ROOT_DIR``: The base directory (prefix) of a portable WAYLAND installation,
  (may be left unset, in that case the system's installed WAYLAND is used).
- ``WESTON_ROOT_DIR``: The base directory (prefix) of a portable WESTON installation,
  (may be left unset, in that case the system's installed WESTON is used).

Currently only WAYLAND is supported, other systems could be added.
"""
__all__ = (
    "main",
)

import subprocess
import sys
import signal
import os
import tempfile

from typing import (
    Any,
)
from collections.abc import (
    Iterator,
    Sequence,
)


# -----------------------------------------------------------------------------
# Constants

def environ_nonzero(var: str) -> bool:
    return os.environ.get(var, "").lstrip("0") != ""


BLENDER_BIN = os.environ.get("BLENDER_BIN", "blender")

# For debugging, print out all information.
VERBOSE = environ_nonzero("VERBOSE")

# Show the window in the foreground.
USE_WINDOW = environ_nonzero("USE_WINDOW")
USE_DEBUG = environ_nonzero("USE_DEBUG")


# -----------------------------------------------------------------------------
# Generic Utilities


def scantree(path: str) -> Iterator[os.DirEntry[str]]:
    """Recursively yield DirEntry objects for given directory."""
    for entry in os.scandir(path):
        if entry.is_dir(follow_symlinks=False):
            yield from scantree(entry.path)
        else:
            yield entry


# -----------------------------------------------------------------------------
# Implementation Back-Ends

class backend_base:
    @staticmethod
    def run(args: Sequence[str]) -> int:
        sys.stderr.write("No headless back-ends for {!r} with args {!r}\n".format(sys.platform, args))
        return 1


class backend_wayland(backend_base):
    @staticmethod
    def _wait_for_wayland_server(*, socket: str, timeout: float) -> bool:
        """
        Uses the expected socket file in `XDG_RUNTIME_DIR` to detect when the WAYLAND server starts.
        """
        import time
        time_idle = min(timeout / 100.0, 0.05)

        xdg_runtime_dir = os.environ.get("XDG_RUNTIME_DIR", "")
        if not xdg_runtime_dir:
            xdg_runtime_dir = "/var/run/user/{:d}".format(os.getuid())

        filepath = os.path.join(xdg_runtime_dir, socket)

        t_beg = time.time()
        t_end = t_beg + timeout
        while True:
            if os.path.exists(filepath):
                return True
            if time.time() >= t_end:
                break
            time.sleep(time_idle)
        return False

    @staticmethod
    def _weston_env_and_ini_from_portable(
            *,
            wayland_root_dir: str | None,
            weston_root_dir: str | None,
    ) -> tuple[dict[str, str] | None, str]:
        """
        Construct a portable environment to run WESTON in.
        """
        # NOTE(@ideasman42): WESTON does not make it convenient to run a portable instance,
        # a reasonable amount of logic here is simply to get WESTON running with references to portable paths.
        # Once packages are available on the Linux distribution used for the CI-environment,
        # we can consider removing this entire function.
        weston_env = {}
        weston_ini = []
        ld_library_paths = []

        if weston_root_dir is None:
            # There is very little to do, simply write a configuration
            # that removes the panel to give some extra screen real estate.
            weston_ini.extend([
                "[shell]",
                "background-color=0x00000000",
                "panel-position=none",
                # Don't look for a background image.
                "background-image=",
            ])
        else:
            weston_ini.extend([
                "[core]",
                "",
                "[shell]",
                "background-color=0x00000000",
                "client={:s}/libexec/weston-desktop-shell".format(weston_root_dir),
                "panel-position=none",
                # Don't look for a background image.
                "background-image=",
                "",
                "[keyboard]",
                "numlock-on=true",
                "",
                "[output]",
                "seat=default",
                "",
                "[input-method]",
                "path={:s}/libexec/weston-keyboard".format(weston_root_dir),
            ])

        if wayland_root_dir is not None:
            ld_library_paths.append(os.path.join(wayland_root_dir, "lib64"))

        if weston_root_dir is not None:
            weston_lib_dir = os.path.join(weston_root_dir, "lib")
            ld_library_paths.extend([
                weston_lib_dir,
                os.path.join(weston_lib_dir, "weston"),
            ])

            # Setup the `WESTON_MODULE_MAP`.
            weston_map_filenames = {
                "wayland-backend.so": "",
                "gl-renderer.so": "",
                "headless-backend.so": "",
                "desktop-shell.so": "",
            }

            for entry in scantree(weston_lib_dir):
                if entry.name in weston_map_filenames:
                    weston_map_filenames[entry.name] = os.path.normpath(entry.path)

            module_map = []
            for key, value in sorted(weston_map_filenames.items()):
                if not value:
                    raise Exception("Failure to find {!r} in {!r}".format(key, weston_lib_dir))
                module_map.append("{:s}={:s}".format(key, value))

            weston_env["WESTON_MODULE_MAP"] = ";".join(module_map)
            del module_map

        if ld_library_paths:
            ld_library_paths_str = os.environ.get("LD_LIBRARY_PATH", "")
            if ld_library_paths_str:
                ld_library_paths.insert(0, ld_library_paths_str.rstrip(":"))
            weston_env["LD_LIBRARY_PATH"] = ":".join(ld_library_paths)
            del ld_library_paths_str

        return (
            {**os.environ, **weston_env} if weston_env else None,
            "\n".join(weston_ini),
        )

    @staticmethod
    def _weston_env_and_ini_from_system() -> tuple[dict[str, str] | None, str]:
        weston_env = None
        weston_ini = [
            "[shell]",
            "background-color=0x00000000",
            "panel-position=none",
            # Don't look for a background image.
            "background-image=",
        ]
        return (
            weston_env,
            "\n".join(weston_ini),
        )

    @staticmethod
    def _weston_env_and_ini() -> tuple[dict[str, str] | None, str]:
        wayland_root_dir = os.environ.get("WAYLAND_ROOT_DIR")
        weston_root_dir = os.environ.get("WESTON_ROOT_DIR")

        if wayland_root_dir or weston_root_dir:
            weston_env, weston_ini = backend_wayland._weston_env_and_ini_from_portable(
                wayland_root_dir=wayland_root_dir,
                weston_root_dir=weston_root_dir,
            )
        else:
            weston_env, weston_ini = backend_wayland._weston_env_and_ini_from_system()
        return weston_env, weston_ini

    @staticmethod
    def run(blender_args: Sequence[str]) -> int:
        # Use the PID to support running multiple tests at once.
        socket = "wl-blender-{:d}".format(os.getpid())

        weston_bin = os.environ.get("WESTON_BIN", "weston")

        # Ensure the WAYLAND server is NOT running (for this socket).
        if backend_wayland._wait_for_wayland_server(socket=socket, timeout=0.0):
            sys.stderr.write("Wayland server for socket \"{:s}\" already running, exiting!\n".format(socket))
            return 1

        weston_env, weston_ini = backend_wayland._weston_env_and_ini()

        cmd = [
            weston_bin,
            "--socket={:s}".format(socket),
            *(() if USE_WINDOW else ("--backend=headless",)),
            "--width=800",
            "--height=600",
            # `--config={..}` is added to point to a temp file.
        ]
        cmd_kw: dict[str, Any] = {}
        if weston_env is not None:
            cmd_kw["env"] = weston_env
        if not VERBOSE:
            cmd_kw["stderr"] = subprocess.PIPE
            cmd_kw["stdout"] = subprocess.PIPE

        if VERBOSE:
            print("Env:", weston_env)
            print("Run:", cmd)

        with tempfile.NamedTemporaryFile(
                prefix="weston_",
                suffix=".ini",
                mode='w',
                encoding="utf-8",
        ) as weston_ini_tempfile:
            weston_ini_tempfile.write(weston_ini)
            weston_ini_tempfile.flush()
            with subprocess.Popen(
                    [*cmd, "--config={:s}".format(weston_ini_tempfile.name)],
                    **cmd_kw,
            ) as proc_server:
                del cmd, cmd_kw
                if not backend_wayland._wait_for_wayland_server(socket=socket, timeout=1.0):
                    # The verbose mode will have written to standard out/error already.
                    # Only show the output is the server wasn't able to start.
                    if not VERBOSE:
                        assert proc_server.stdout is not None
                        assert proc_server.stderr is not None
                        sys.stderr.write("Unable to start wayland server, exiting!\n")
                        sys.stderr.write(proc_server.stdout.read().decode("utf-8", errors="surrogateescape"))
                        sys.stderr.write(proc_server.stderr.read().decode("utf-8", errors="surrogateescape"))
                        sys.stderr.write("\n")
                    proc_server.send_signal(signal.SIGINT)
                    # Wait for the interrupt to be handled.
                    proc_server.communicate()
                    return 1

                blender_env = {**os.environ, "WAYLAND_DISPLAY": socket}

                # Needed so Blender can find WAYLAND libraries such as `libwayland-cursor.so`.
                if weston_env is not None and "LD_LIBRARY_PATH" in weston_env:
                    blender_env["LD_LIBRARY_PATH"] = weston_env["LD_LIBRARY_PATH"]

                cmd = [
                    # "strace",  # Can be useful for debugging any startup issues.
                    BLENDER_BIN,
                    *blender_args,
                ]

                if USE_DEBUG:
                    cmd = ["gdb", BLENDER_BIN, "--ex=run", "--args", *cmd]

                if VERBOSE:
                    print("Env:", blender_env)
                    print("Run:", cmd)
                with subprocess.Popen(cmd, env=blender_env) as proc_blender:
                    proc_blender.communicate()
                    blender_exit_code = proc_blender.returncode
                del cmd

                # Blender has finished, close the server.
                proc_server.send_signal(signal.SIGINT)
                # Wait for the interrupt to be handled.
                proc_server.communicate()

                # Forward Blender's exit code.
                return blender_exit_code


# -----------------------------------------------------------------------------
# Main Function

def main() -> int:
    match sys.platform:
        case "darwin":
            backend = backend_base
        case "win32":
            backend = backend_base
        case _:
            backend = backend_wayland
    return backend.run(sys.argv[1:])


if __name__ == "__main__":
    sys.exit(main())
