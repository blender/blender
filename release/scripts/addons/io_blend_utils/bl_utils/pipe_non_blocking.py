# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
Example use:

    p = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            )

    pipe_non_blocking_set(p.stdout.fileno())

    try:
        data = os.read(p.stdout.fileno(), 1)
    except PortableBlockingIOError as ex:
        if not pipe_non_blocking_is_error_blocking(ex):
            raise ex
"""


__all__ = (
    "pipe_non_blocking_set",
    "pipe_non_blocking_is_error_blocking",
    "PortableBlockingIOError",
    )

import os


if os.name == "nt":
    # MS-Windows Version
    def pipe_non_blocking_set(fd):
        # Constant could define globally but avoid polluting the name-space
        # thanks to: http://stackoverflow.com/questions/34504970
        import msvcrt

        from ctypes import windll, byref, wintypes, WinError, POINTER
        from ctypes.wintypes import HANDLE, DWORD, BOOL

        LPDWORD = POINTER(DWORD)

        PIPE_NOWAIT = wintypes.DWORD(0x00000001)

        def pipe_no_wait(pipefd):
            SetNamedPipeHandleState = windll.kernel32.SetNamedPipeHandleState
            SetNamedPipeHandleState.argtypes = [HANDLE, LPDWORD, LPDWORD, LPDWORD]
            SetNamedPipeHandleState.restype = BOOL

            h = msvcrt.get_osfhandle(pipefd)

            res = windll.kernel32.SetNamedPipeHandleState(h, byref(PIPE_NOWAIT), None, None)
            if res == 0:
                print(WinError())
                return False
            return True

        return pipe_no_wait(fd)

    def pipe_non_blocking_is_error_blocking(ex):
        if not isinstance(ex, PortableBlockingIOError):
            return False
        from ctypes import GetLastError
        ERROR_NO_DATA = 232

        return (GetLastError() == ERROR_NO_DATA)

    PortableBlockingIOError = OSError
else:
    # Posix Version
    def pipe_non_blocking_set(fd):
        import fcntl
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
        return True

    # only for compatibility with 'nt' version.
    def pipe_non_blocking_is_error_blocking(ex):
        if not isinstance(ex, PortableBlockingIOError):
            return False
        return True

    PortableBlockingIOError = BlockingIOError
