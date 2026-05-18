# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import io
from pathlib import Path
from typing import Callable

__all__ = (
    'mutex_lock',
    'mutex_unlock',
)


# Dictionary of local library path to a tuple with:
# - lock file handle
# - path of the lock file
# - unlock function
_mutex_locks: dict[Path, tuple[io.IOBase, Path, Callable[[io.IOBase], None]]] = {}

_registered_atexit = False


def mutex_lock(local_library_path: Path) -> bool:
    """Lock the library for syncing.

    Create a file on disk that signals to other Blender instances that this
    remote asset library is being synced by this Blender.

    This uses approaches from:
        - https://www.pythontutorials.net/blog/make-sure-only-a-single-instance-of-a-program-is-running/
        - https://yakking.branchable.com/posts/procrun-2-pidfiles/

    :returns: true if the lock was created successfully, false if some other
        Blender already locked this library.
    """
    global _registered_atexit

    import atexit
    import sys

    if not _registered_atexit:
        atexit.register(_unlock_all)
        _registered_atexit = True

    # Choose platform-dependent _obtain_lock(file) and _release_lock() functions.
    if sys.platform == "win32":
        import msvcrt

        def _obtain_lock(file: io.IOBase) -> None:
            # Lock the first byte of the file (arbitrary choice)
            msvcrt.locking(file.fileno(), msvcrt.LK_NBLCK, 1)

        def _release_lock(file: io.IOBase) -> None:
            msvcrt.locking(file.fileno(), msvcrt.LK_UNLCK, 1)
    else:
        import fcntl

        def _obtain_lock(file: io.IOBase) -> None:
            fcntl.flock(file, fcntl.LOCK_EX | fcntl.LOCK_NB)

        def _release_lock(file: io.IOBase) -> None:
            # Closing the file automatically releases the lock.
            pass

    assert isinstance(local_library_path, Path)
    assert local_library_path not in _mutex_locks, "Locks are not reentrant"

    lockfile_path = local_library_path / "_sync.lock"

    # It is not suitable here to use an 'exclusive create' ('x' option) here.
    # That will still create a race condition, with the space between creation
    # of the file and locking it. So, better to make the existence of the file
    # meaningless, and only communicate the lock state with an actual filesystem
    # lock.
    try:
        # Binary mode (`wb`) is required on Windows, for the locking.
        lockfile = lockfile_path.open('wb')
    except OSError:
        # on Windows, opening a file for writing, while another process already has it open, can fail.
        # That just means somebody else has ownership of it.
        return False
    try:
        _obtain_lock(lockfile)
    except OSError:
        # Lock is already held by another Blender.
        lockfile.close()
        return False

    # We have obtained an exclusive lock, which the OS will release when this
    # process is killed.
    _mutex_locks[local_library_path] = (lockfile, lockfile_path, _release_lock)
    return True


def mutex_unlock(local_library_path: Path) -> None:
    """Remove the lock created by mutex_lock(local_library_path)."""

    assert isinstance(local_library_path, Path)
    assert local_library_path in _mutex_locks, "library was not locked"

    lockfile, lockfile_path, release_lock = _mutex_locks[local_library_path]
    release_lock(lockfile)
    lockfile.close()

    del _mutex_locks[local_library_path]

    try:
        lockfile_path.unlink(missing_ok=True)
    except IOError:
        # Ignore errors when deleting the file. By now another process may have
        # recreated it and locked it again.
        pass


def _unlock_all() -> None:
    """Unlock all file mutexes.

    This is automatically called when the Python interpreter exits.

    From the OS perspective it's not necessary, as all locks are automatically
    released when the process stops. However, Python will complain with a
    ResourceWarning if any open files are not closed.
    """

    for local_library_path in list(_mutex_locks.keys()):
        mutex_unlock(local_library_path)
