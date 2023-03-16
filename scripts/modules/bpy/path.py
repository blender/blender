# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module has a similar scope to os.path, containing utility
functions for dealing with paths in Blender.
"""

__all__ = (
    "abspath",
    "basename",
    "clean_name",
    "display_name",
    "display_name_to_filepath",
    "display_name_from_filepath",
    "ensure_ext",
    "extensions_image",
    "extensions_movie",
    "extensions_audio",
    "is_subdir",
    "module_names",
    "native_pathsep",
    "reduce_dirs",
    "relpath",
    "resolve_ncase",
)

import bpy as _bpy
import os as _os

from _bpy_path import (
    extensions_audio,
    extensions_movie,
    extensions_image,
)


def _getattr_bytes(var, attr):
    return var.path_resolve(attr, False).as_bytes()


def abspath(path, *, start=None, library=None):
    """
    Returns the absolute path relative to the current blend file
    using the "//" prefix.

    :arg start: Relative to this path,
       when not set the current filename is used.
    :type start: string or bytes
    :arg library: The library this path is from. This is only included for
       convenience, when the library is not None its path replaces *start*.
    :type library: :class:`bpy.types.Library`
    :return: The absolute path.
    :rtype: string
    """
    if isinstance(path, bytes):
        if path.startswith(b"//"):
            if library:
                start = _os.path.dirname(abspath(_getattr_bytes(library, "filepath")))
            return _os.path.join(
                _os.path.dirname(_getattr_bytes(_bpy.data, "filepath"))
                if start is None
                else start,
                path[2:],
            )
    else:
        if path.startswith("//"):
            if library:
                start = _os.path.dirname(abspath(library.filepath))
            return _os.path.join(
                _os.path.dirname(_bpy.data.filepath) if start is None else start,
                path[2:],
            )

    return path


def relpath(path, *, start=None):
    """
    Returns the path relative to the current blend file using the "//" prefix.

    :arg path: An absolute path.
    :type path: string or bytes
    :arg start: Relative to this path,
       when not set the current filename is used.
    :type start: string or bytes
    :return: The relative path.
    :rtype: string
    """
    if isinstance(path, bytes):
        if not path.startswith(b"//"):
            if start is None:
                start = _os.path.dirname(_getattr_bytes(_bpy.data, "filepath"))
            return b"//" + _os.path.relpath(path, start)
    else:
        if not path.startswith("//"):
            if start is None:
                start = _os.path.dirname(_bpy.data.filepath)
            return "//" + _os.path.relpath(path, start)

    return path


def is_subdir(path, directory):
    """
    Returns true if *path* in a subdirectory of *directory*.
    Both paths must be absolute.

    :arg path: An absolute path.
    :type path: string or bytes
    :return: Whether or not the path is a subdirectory.
    :rtype: boolean
    """
    from os.path import normpath, normcase, sep

    path = normpath(normcase(path))
    directory = normpath(normcase(directory))
    if len(path) > len(directory):
        sep = sep.encode("ascii") if isinstance(directory, bytes) else sep
        if path.startswith(directory.rstrip(sep) + sep):
            return True
    return False


def clean_name(name, *, replace="_"):
    """
    Returns a name with characters replaced that
    may cause problems under various circumstances,
    such as writing to a file.
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the *replace* argument if defined.
    :arg name: The path name.
    :type name: string or bytes
    :arg replace: The replacement for non-valid characters.
    :type replace: string
    :return: The cleaned name.
    :rtype: string
    """

    if replace != "_":
        if len(replace) != 1 or ord(replace) > 255:
            raise ValueError("Value must be a single ascii character")

    def maketrans_init():
        trans_cache = clean_name._trans_cache
        trans = trans_cache.get(replace)
        if trans is None:
            bad_chars = (
                0x00,
                0x01,
                0x02,
                0x03,
                0x04,
                0x05,
                0x06,
                0x07,
                0x08,
                0x09,
                0x0A,
                0x0B,
                0x0C,
                0x0D,
                0x0E,
                0x0F,
                0x10,
                0x11,
                0x12,
                0x13,
                0x14,
                0x15,
                0x16,
                0x17,
                0x18,
                0x19,
                0x1A,
                0x1B,
                0x1C,
                0x1D,
                0x1E,
                0x1F,
                0x20,
                0x21,
                0x22,
                0x23,
                0x24,
                0x25,
                0x26,
                0x27,
                0x28,
                0x29,
                0x2A,
                0x2B,
                0x2C,
                0x2E,
                0x2F,
                0x3A,
                0x3B,
                0x3C,
                0x3D,
                0x3E,
                0x3F,
                0x40,
                0x5B,
                0x5C,
                0x5D,
                0x5E,
                0x60,
                0x7B,
                0x7C,
                0x7D,
                0x7E,
                0x7F,
                0x80,
                0x81,
                0x82,
                0x83,
                0x84,
                0x85,
                0x86,
                0x87,
                0x88,
                0x89,
                0x8A,
                0x8B,
                0x8C,
                0x8D,
                0x8E,
                0x8F,
                0x90,
                0x91,
                0x92,
                0x93,
                0x94,
                0x95,
                0x96,
                0x97,
                0x98,
                0x99,
                0x9A,
                0x9B,
                0x9C,
                0x9D,
                0x9E,
                0x9F,
                0xA0,
                0xA1,
                0xA2,
                0xA3,
                0xA4,
                0xA5,
                0xA6,
                0xA7,
                0xA8,
                0xA9,
                0xAA,
                0xAB,
                0xAC,
                0xAD,
                0xAE,
                0xAF,
                0xB0,
                0xB1,
                0xB2,
                0xB3,
                0xB4,
                0xB5,
                0xB6,
                0xB7,
                0xB8,
                0xB9,
                0xBA,
                0xBB,
                0xBC,
                0xBD,
                0xBE,
                0xBF,
                0xC0,
                0xC1,
                0xC2,
                0xC3,
                0xC4,
                0xC5,
                0xC6,
                0xC7,
                0xC8,
                0xC9,
                0xCA,
                0xCB,
                0xCC,
                0xCD,
                0xCE,
                0xCF,
                0xD0,
                0xD1,
                0xD2,
                0xD3,
                0xD4,
                0xD5,
                0xD6,
                0xD7,
                0xD8,
                0xD9,
                0xDA,
                0xDB,
                0xDC,
                0xDD,
                0xDE,
                0xDF,
                0xE0,
                0xE1,
                0xE2,
                0xE3,
                0xE4,
                0xE5,
                0xE6,
                0xE7,
                0xE8,
                0xE9,
                0xEA,
                0xEB,
                0xEC,
                0xED,
                0xEE,
                0xEF,
                0xF0,
                0xF1,
                0xF2,
                0xF3,
                0xF4,
                0xF5,
                0xF6,
                0xF7,
                0xF8,
                0xF9,
                0xFA,
                0xFB,
                0xFC,
                0xFD,
                0xFE,
            )
            trans = str.maketrans({char: replace for char in bad_chars})
            trans_cache[replace] = trans
        return trans

    trans = maketrans_init()
    return name.translate(trans)


clean_name._trans_cache = {}


def _clean_utf8(name):
    if type(name) is bytes:
        return name.decode("utf8", "replace")
    else:
        return name.encode("utf8", "replace").decode("utf8")


_display_name_literals = {
    ":": "_colon_",
    "+": "_plus_",
    "/": "_slash_",
}


def display_name(name, *, has_ext=True, title_case=True):
    """
    Creates a display string from name to be used menus and the user interface.
    Intended for use with filenames and module names.

    :arg name: The name to be used for displaying the user interface.
    :type name: string
    :arg has_ext: Remove file extension from name.
    :type has_ext: boolean
    :arg title_case: Convert lowercase names to title case.
    :type title_case: boolean
    :return: The display string.
    :rtype: string
    """

    if has_ext:
        name = _os.path.splitext(basename(name))[0]

    # string replacements
    for disp_value, file_value in _display_name_literals.items():
        name = name.replace(file_value, disp_value)

    # strip to allow underscore prefix
    # (when paths can't start with numbers for eg).
    name = name.replace("_", " ").lstrip(" ")

    if title_case and name.islower():
        name = name.lower().title()

    name = _clean_utf8(name)
    return name


def display_name_to_filepath(name):
    """
    Performs the reverse of display_name using literal versions of characters
    which aren't supported in a filepath.
    :arg name: The display name to convert.
    :type name: string
    :return: The file path.
    :rtype: string
    """
    for disp_value, file_value in _display_name_literals.items():
        name = name.replace(disp_value, file_value)
    return name


def display_name_from_filepath(name):
    """
    Returns the path stripped of directory and extension,
    ensured to be utf8 compatible.
    :arg name: The file path to convert.
    :type name: string
    :return: The display name.
    :rtype: string
    """

    name = _os.path.splitext(basename(name))[0]
    name = _clean_utf8(name)
    return name


def resolve_ncase(path):
    """
    Resolve a case insensitive path on a case sensitive system,
    returning a string with the path if found else return the original path.
    :arg path: The path name to resolve.
    :type path: string
    :return: The resolved path.
    :rtype: string
    """

    def _ncase_path_found(path):
        if not path or _os.path.exists(path):
            return path, True

        # filename may be a directory or a file
        filename = _os.path.basename(path)
        dirpath = _os.path.dirname(path)

        suffix = path[:0]  # "" but ensure byte/str match
        if not filename:  # dir ends with a slash?
            if len(dirpath) < len(path):
                suffix = path[: len(path) - len(dirpath)]

            filename = _os.path.basename(dirpath)
            dirpath = _os.path.dirname(dirpath)

        if not _os.path.exists(dirpath):
            if dirpath == path:
                return path, False

            dirpath, found = _ncase_path_found(dirpath)

            if not found:
                return path, False

        # at this point, the directory exists but not the file

        # we are expecting 'dirpath' to be a directory, but it could be a file
        if _os.path.isdir(dirpath):
            try:
                files = _os.listdir(dirpath)
            except PermissionError:
                # We might not have the permission to list dirpath...
                return path, False
        else:
            return path, False

        filename_low = filename.lower()
        f_iter_nocase = None

        for f_iter in files:
            if f_iter.lower() == filename_low:
                f_iter_nocase = f_iter
                break

        if f_iter_nocase:
            return _os.path.join(dirpath, f_iter_nocase) + suffix, True
        else:
            # can't find the right one, just return the path as is.
            return path, False

    ncase_path, found = _ncase_path_found(path)
    return ncase_path if found else path


def ensure_ext(filepath, ext, *, case_sensitive=False):
    """
    Return the path with the extension added if it is not already set.

    :arg filepath: The file path.
    :type filepath: string
    :arg ext: The extension to check for, can be a compound extension. Should
              start with a dot, such as '.blend' or '.tar.gz'.
    :type ext: string
    :arg case_sensitive: Check for matching case when comparing extensions.
    :type case_sensitive: boolean
    :return: The file path with the given extension.
    :rtype: string
    """

    if case_sensitive:
        if filepath.endswith(ext):
            return filepath
    else:
        if filepath[-len(ext):].lower().endswith(ext.lower()):
            return filepath

    return filepath + ext


def module_names(path, *, recursive=False):
    """
    Return a list of modules which can be imported from *path*.

    :arg path: a directory to scan.
    :type path: string
    :arg recursive: Also return submodule names for packages.
    :type recursive: bool
    :return: a list of string pairs (module_name, module_file).
    :rtype: list of strings
    """

    from os.path import join, isfile

    modules = []

    for filename in sorted(_os.listdir(path)):
        if filename == "modules":
            pass  # XXX, hard coded exception.
        elif filename.endswith(".py") and filename != "__init__.py":
            fullpath = join(path, filename)
            modules.append((filename[0:-3], fullpath))
        elif not filename.startswith("."):
            # Skip hidden files since they are used by for version control.
            directory = join(path, filename)
            fullpath = join(directory, "__init__.py")
            if isfile(fullpath):
                modules.append((filename, fullpath))
                if recursive:
                    for mod_name, mod_path in module_names(directory, recursive=True):
                        modules.append(
                            (
                                "%s.%s" % (filename, mod_name),
                                mod_path,
                            )
                        )

    return modules


def basename(path):
    """
    Equivalent to ``os.path.basename``, but skips a "//" prefix.

    Use for Windows compatibility.
    :return: The base name of the given path.
    :rtype: string
    """
    return _os.path.basename(path[2:] if path[:2] in {"//", b"//"} else path)


def native_pathsep(path):
    """
    Replace the path separator with the systems native ``os.sep``.
    :arg path: The path to replace.
    :type path: string
    :return: The path with system native separators.
    :rtype: string
    """
    if type(path) is str:
        if _os.sep == "/":
            return path.replace("\\", "/")
        else:
            if path.startswith("//"):
                return "//" + path[2:].replace("/", "\\")
            else:
                return path.replace("/", "\\")
    else:  # bytes
        if _os.sep == "/":
            return path.replace(b"\\", b"/")
        else:
            if path.startswith(b"//"):
                return b"//" + path[2:].replace(b"/", b"\\")
            else:
                return path.replace(b"/", b"\\")


def reduce_dirs(dirs):
    """
    Given a sequence of directories, remove duplicates and
    any directories nested in one of the other paths.
    (Useful for recursive path searching).

    :arg dirs: Sequence of directory paths.
    :type dirs: sequence of strings
    :return: A unique list of paths.
    :rtype: list of strings
    """
    dirs = list({_os.path.normpath(_os.path.abspath(d)) for d in dirs})
    dirs.sort(key=lambda d: len(d))
    for i in range(len(dirs) - 1, -1, -1):
        for j in range(i):
            print(i, j)
            if len(dirs[i]) == len(dirs[j]):
                break
            elif is_subdir(dirs[i], dirs[j]):
                del dirs[i]
                break
    return dirs
