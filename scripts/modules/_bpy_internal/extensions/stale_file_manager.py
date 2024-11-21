# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Schedule files for later removal, needed for situations where files are locked.
#
# This is mainly a workaround for WIN32 error where an add-on DLL
# is considered *used* making it impossible to remove.
#
# This is also used on other systems as permissions can also prevent sub-directories from removed.
# In this case renaming can make way the path to be replaced however it doesn't address
# the problem of the "stale" path failing to be removed.
# The user would need to change the permissions in this case (although this really a corner case).
__all__ = (
    "StaleFiles",
)


from collections.abc import (
    Sequence,
)

# The stale file-format is very simple and works as follows.
#
# - Every line references a path relative to the stale file.
# - Paths must always references files within this directory
#   (anything else must be ignored).
# - Paths must always use forward slashes (even on WIN32).
#   This is done since a repository may be accessed from different systems.
# - Paths must end with a newline `\n`.
#
# Further notes:
# - Corrupted "stale" files must be handled gracefully (it may be random bytes).
# - Non UTF8 characters in paths are supported via `surrogateescape`.
# - File names containing newlines are *not* supported.


class StaleFiles:
    __slots__ = (
        # Files outside of this directory must *never* be removed.
        "_base_directory",
        # The name (within `_base_directory`) to load/store paths.
        "_stale_filename",
        # Stale paths relative to `_base_directory`.
        "_paths",
        # When true, print extra debug output.
        "_debug",
        # Store the cache index per-directory, avoids looking up an index every time a stale name needs to be created.
        "_index_cache",
        # True when the run-time state is different to the on-disk state.
        "_is_modified",
    )

    def __init__(
            self,
            base_directory: str,
            *,
            stale_filename: str,
            debug: bool = False,
    ):
        import os
        from os import sep
        assert base_directory not in ("", ".", "..")
        # NOTE: on WIN32 `normpath` won't remove the trailing `sep`,
        # it's important to add only if it's not there.
        base_directory = os.path.normpath(base_directory)
        self._base_directory = base_directory if base_directory.endswith(sep) else (base_directory + sep)
        self._stale_filename = stale_filename
        self._paths: list[str] = []
        self._debug: bool = debug

        self._index_cache: dict[str, int] = {}
        self._is_modified: bool = True

    def is_empty(self) -> bool:
        return not bool(self._paths)

    def is_modified(self) -> bool:
        return self._is_modified

    def state_load(self, *, check_exists: bool) -> None:
        import contextlib
        import os
        from os import sep

        base_directory = self._base_directory
        paths = self._paths
        debug = self._debug

        assert base_directory.endswith(sep)
        # Don't support loading multiple times or after adding files.
        assert len(paths) == 0

        stale_filepath = os.path.join(base_directory, self._stale_filename)

        line_count = 0

        # Set here before early exit.
        # Assume modified so any corrupt causes a re-write.
        self._is_modified = True

        try:
            # pylint: disable-next=consider-using-with
            fh_context = open(stale_filepath, "r", encoding="utf8", errors="surrogateescape")
        except FileNotFoundError:
            self._is_modified = False
            return
        except Exception as ex:
            if debug:
                print(base_directory, "error opening file for read", str(ex))
            return

        with contextlib.closing(fh_context) as fh:
            fh_iter = iter(fh)
            while True:
                try:
                    path = next(fh_iter)
                except StopIteration:
                    break
                except Exception as ex:
                    if debug:
                        print(base_directory, "error reading line", str(ex))
                    break

                line_count += 1
                # Not expected, file may be truncated.
                if not path.endswith("\n"):
                    if debug:
                        print(base_directory, "expected line endings on each line")
                    continue
                path = path[:-1]
                # Not expected but harmless, ignore if it does.
                if not path:
                    if debug:
                        print(base_directory, "expected line not to be empty")
                    continue

                path_abs = base_directory + (path if sep == "/" else path.replace("/", "\\"))

                if check_exists:
                    # Harmless, somehow the file was removed.
                    if not os.path.exists(path_abs):
                        continue

                path_abs = os.path.normpath(path_abs)
                # Not expected, ensure under *no* conditions paths outside this directory are removed.
                if not path_abs.startswith(base_directory):
                    if debug:
                        print(base_directory, "stale file points to parent path (unexpected but harmless)", repr(path))
                    continue

                # Ensure the `base_directory` & `path_abs` they are not the same.
                # One could be forgiven for thinking they must never be the same since `path`
                # is known not be be an empty string, one would be mistaken!
                # WIN32 which considers `C:\path\` the same as `C:\path\. ` to be the same.
                # Therefor, literal lines containing any combination of trailing full-stop
                # or space characters would be considered files that cannot be removed.
                # While this should never under normal conditions happen,
                # guarantee that stale file removal *never* removes anything it should not,
                # including situations when random bytes are written into this file
                # (except in the case the random bytes happen to match a patch - which can't be avoided).
                #
                # If this ever did happen besides potentially trying to remove `base_directory`,
                # this path could be treated as a file which could not be removed and queued for
                # removal again causing a single space (for e.g.) to be left in the stale file,
                # trying to be removed every startup and failing.
                # Avoid all these issues by checking the path doesn't resolve to being the same path as it's parent.
                is_same = False
                try:
                    is_same = os.path.samefile(base_directory, path_abs)
                except FileNotFoundError:
                    pass
                except Exception as ex:
                    if debug:
                        print(base_directory, "error checking the same path", str(ex))

                if is_same:
                    if debug:
                        print(base_directory, "path results to it's parent", repr(path))
                    continue

                # NOTE: duplicates are not checked, while they aren't expected, duplicates won't cause errors.
                paths.append(path)

        self._is_modified = len(paths) != line_count

    def state_store(self, *, check_exists: bool) -> None:
        import contextlib
        import os
        from os import sep

        base_directory = self._base_directory
        debug = self._debug

        stale_filepath = os.path.join(base_directory, self._stale_filename)

        if not self._paths:
            self._is_modified = False
            try:
                os.remove(stale_filepath)
            except FileNotFoundError:
                pass
            except Exception as ex:
                if debug:
                    print(base_directory, "failed to remove!", str(ex))
                self._is_modified = True

            return

        try:
            # pylint: disable-next=consider-using-with
            fh_context = open(stale_filepath, "w", encoding="utf8", errors="surrogateescape")
        except Exception as ex:
            if debug:
                print(base_directory, "error opening file for write", str(ex))
            self._is_modified = True
            return

        # Assume success, any errors can set to true.
        is_modified = False

        with contextlib.closing(fh_context) as fh:
            for path in self._paths:
                if check_exists:
                    path_abs = base_directory + (path if sep == "/" else path.replace("/", "\\"))
                    # Harmless, somehow the file was removed.
                    if not os.path.exists(path_abs):
                        continue

                try:
                    fh.write(path + "\n")
                except Exception as ex:
                    if debug:
                        print(base_directory, "failed to write path", str(ex))
                    is_modified = True
                    break

        self._is_modified = is_modified

    def state_remove_all(self) -> bool:
        import stat
        import shutil
        import os

        from os import sep

        base_directory = self._base_directory
        debug = self._debug

        paths_next = []

        for path in self._paths:
            path_abs = base_directory + (path if sep == "/" else path.replace("/", "\\"))
            path_abs = os.path.normpath(path_abs)

            # Should be unreachable, extra paranoid check so we *never*
            # recursively remove anything outside of the base directory.
            if not path_abs.startswith(base_directory):
                print("Internal error detected attempting to remove file outside of:", base_directory)
                continue

            try:
                st = os.stat(path_abs)
            except FileNotFoundError:
                # Not a problem if it's already removed.
                continue
            except Exception as ex:
                if debug:
                    print(base_directory, "failed to stat file", path, str(ex))
                continue

            if stat.S_ISDIR(st.st_mode):
                try:
                    shutil.rmtree(path_abs)
                except Exception as ex:
                    # May be necessary with links.
                    try:
                        os.remove(path_abs)
                    except Exception:
                        if debug:
                            print(base_directory, "failed to remove dir", path, str(ex))
            else:
                try:
                    os.remove(path_abs)
                except Exception as ex:
                    if debug:
                        print(base_directory, "failed to remove file", path, str(ex))

            # Failed to remove, add back to the list.
            if os.path.exists(path_abs):
                paths_next.append(path)

        if len(self._paths) == len(paths_next):
            return False

        self._is_modified = True
        self._paths[:] = paths_next
        return True

    def state_load_add_and_store(
            self,
            *,
            # A sequence of absolute paths within `_base_directory`.
            paths: Sequence[str],
    ) -> bool:
        # Convenience function for a common operation.
        # Return true when one or more items from "paths" were added to the "state".

        self.state_load(check_exists=True)
        if not self.is_empty():
            self.state_remove_all()

        result = False
        for path_abs in paths:
            self.filepath_add(path_abs, rename=True)
            result = True

        if self.is_modified():
            self.state_store(check_exists=False)

        return result

    def state_load_remove_and_store(
            self,
            *,
            # A sequence of absolute paths within `_base_directory`.
            paths: Sequence[str],
    ) -> bool:
        # Convenience function for a common operation.
        # Return true when one or more items from "paths" were removed from the "state".

        self.state_load(check_exists=False)
        # Accounts for the common case where nothing has been marked for removal.
        if not self._paths:
            return False

        paths_remove_canonical = {
            self._filepath_relative_and_canonicalize(path_abs) for path_abs in paths
            if self._filepath_relative_test(path_abs)
        }

        paths_next = [path for path in self._paths if path not in paths_remove_canonical]
        if len(self._paths) == len(paths_next):
            return False

        self._paths[:] = paths_next
        self._is_modified = True

        self.state_store(check_exists=False)

        return True

    def _filepath_relative_test(self, path_abs: str) -> bool:
        debug = self._debug
        base_directory = self._base_directory
        if not path_abs.startswith(base_directory):
            if debug:
                print(base_directory, "is not a sub-directory", path_abs)
            return False
        return True

    def _filepath_relative_and_canonicalize(self, path_abs: str) -> str:
        from os import sep

        assert self._filepath_relative_test(path_abs)

        path = path_abs[len(self._base_directory):].lstrip(sep)
        if sep == "\\":
            path = path.replace("\\", "/")
        return path

    def _filepath_rename_to_stale(self, path_abs: str) -> str:
        import os

        base_directory = self._base_directory
        debug = self._debug

        # These need not necessarily match, it could be optional.
        prefix = self._stale_filename

        dirpath = os.path.dirname(path_abs)
        stale_index = self._index_cache.get(dirpath, 1)
        while True:
            path_abs_stale = os.path.join(dirpath, "{:s}{:04x}".format(prefix, stale_index))
            if not os.path.exists(path_abs_stale):
                break
            stale_index += 1

        rename_ok = False
        try:
            os.rename(path_abs, path_abs_stale)
            rename_ok = True
        except Exception as ex:
            if debug:
                print(base_directory, "failed to rename path", str(ex))

        if rename_ok:
            self._index_cache[dirpath] = stale_index + 1
        else:
            # Failed to rename, make the previous name stale as we have no better options.
            path_abs_stale = path_abs
            if debug:
                print("failed to rename:", path_abs)

        return path_abs_stale

    def filepath_add(self, path_abs: str, *, rename: bool) -> bool:
        if not self._filepath_relative_test(path_abs):
            return False

        if rename:
            path_abs = self._filepath_rename_to_stale(path_abs)
        path = self._filepath_relative_and_canonicalize(path_abs)

        self._is_modified = True
        self._paths.append(path)
        return True
