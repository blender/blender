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

# <pep8-80 compliant>

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


def abspath(path, start=None, library=None):
    """
    Returns the absolute path relative to the current blend file
    using the "//" prefix.

    :arg start: Relative to this path,
       when not set the current filename is used.
    :type start: string or bytes
    :arg library: The library this path is from. This is only included for
       convenience, when the library is not None its path replaces *start*.
    :type library: :class:`bpy.types.Library`
    """
    if isinstance(path, bytes):
        if path.startswith(b"//"):
            if library:
                start = _os.path.dirname(
                    abspath(_getattr_bytes(library, "filepath")))
            return _os.path.join(
                _os.path.dirname(_getattr_bytes(_bpy.data, "filepath"))
                if start is None else start,
                path[2:],
            )
    else:
        if path.startswith("//"):
            if library:
                start = _os.path.dirname(
                    abspath(library.filepath))
            return _os.path.join(
                _os.path.dirname(_bpy.data.filepath)
                if start is None else start,
                path[2:],
            )

    return path


def relpath(path, start=None):
    """
    Returns the path relative to the current blend file using the "//" prefix.

    :arg path: An absolute path.
    :type path: string or bytes
    :arg start: Relative to this path,
       when not set the current filename is used.
    :type start: string or bytes
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
    """
    from os.path import normpath, normcase, sep
    path = normpath(normcase(path))
    directory = normpath(normcase(directory))
    if len(path) > len(directory):
        sep = sep.encode('ascii') if isinstance(directory, bytes) else sep
        if path.startswith(directory.rstrip(sep) + sep):
            return True
    return False


def clean_name(name, replace="_"):
    """
    Returns a name with characters replaced that
    may cause problems under various circumstances,
    such as writing to a file.
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the *replace* argument if defined.
    """

    if replace != "_":
        if len(replace) != 1 or ord(replace) > 255:
            raise ValueError("Value must be a single ascii character")

    def maketrans_init():
        trans_cache = clean_name._trans_cache
        trans = trans_cache.get(replace)
        if trans is None:
            bad_chars = (
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
                0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2e, 0x2f, 0x3a,
                0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x5b, 0x5c,
                0x5d, 0x5e, 0x60, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
                0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
                0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
                0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
                0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
                0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
                0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
                0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
                0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
                0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
                0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
                0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
                0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
                0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
                0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
            )
            trans = str.maketrans({char: replace for char in bad_chars})
            trans_cache[replace] = trans
        return trans

    trans = maketrans_init()
    return name.translate(trans)


clean_name._trans_cache = {}


def _clean_utf8(name):
    if type(name) == bytes:
        return name.decode("utf8", "replace")
    else:
        return name.encode("utf8", "replace").decode("utf8")


_display_name_literals = {
    ":": "_colon_",
    "+": "_plus_",
}


def display_name(name):
    """
    Creates a display string from name to be used menus and the user interface.
    Capitalize the first letter in all lowercase names,
    mixed case names are kept as is. Intended for use with
    filenames and module names.
    """

    name = _os.path.splitext(basename(name))[0]

    # string replacements
    for disp_value, file_value in _display_name_literals.items():
        name = name.replace(file_value, disp_value)

    # strip to allow underscore prefix
    # (when paths can't start with numbers for eg).
    name = name.replace("_", " ").lstrip(" ")

    if name.islower():
        name = name.lower().title()

    name = _clean_utf8(name)
    return name


def display_name_to_filepath(name):
    """
    Performs the reverse of display_name using literal versions of characters
    which aren't supported in a filepath.
    """
    for disp_value, file_value in _display_name_literals.items():
        name = name.replace(disp_value, file_value)
    return name


def display_name_from_filepath(name):
    """
    Returns the path stripped of directory and extension,
    ensured to be utf8 compatible.
    """

    name = _os.path.splitext(basename(name))[0]
    name = _clean_utf8(name)
    return name


def resolve_ncase(path):
    """
    Resolve a case insensitive path on a case sensitive system,
    returning a string with the path if found else return the original path.
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
                suffix = path[:len(path) - len(dirpath)]

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
            # cant find the right one, just return the path as is.
            return path, False

    ncase_path, found = _ncase_path_found(path)
    return ncase_path if found else path


def ensure_ext(filepath, ext, case_sensitive=False):
    """
    Return the path with the extension added if it is not already set.

    :arg ext: The extension to check for, can be a compound extension. Should
              start with a dot, such as '.blend' or '.tar.gz'.
    :type ext: string
    :arg case_sensitive: Check for matching case when comparing extensions.
    :type case_sensitive: bool
    """

    if case_sensitive:
        if filepath.endswith(ext):
            return filepath
    else:
        if filepath[-len(ext):].lower().endswith(ext.lower()):
            return filepath

    return filepath + ext


def module_names(path, recursive=False):
    """
    Return a list of modules which can be imported from *path*.

    :arg path: a directory to scan.
    :type path: string
    :arg recursive: Also return submodule names for packages.
    :type recursive: bool
    :return: a list of string pairs (module_name, module_file).
    :rtype: list
    """

    from os.path import join, isfile

    modules = []

    for filename in sorted(_os.listdir(path)):
        if filename == "modules":
            pass  # XXX, hard coded exception.
        elif filename.endswith(".py") and filename != "__init__.py":
            fullpath = join(path, filename)
            modules.append((filename[0:-3], fullpath))
        elif "." not in filename:
            directory = join(path, filename)
            fullpath = join(directory, "__init__.py")
            if isfile(fullpath):
                modules.append((filename, fullpath))
                if recursive:
                    for mod_name, mod_path in module_names(directory, True):
                        modules.append(("%s.%s" % (filename, mod_name),
                                        mod_path,
                                        ))

    return modules


def basename(path):
    """
    Equivalent to os.path.basename, but skips a "//" prefix.

    Use for Windows compatibility.
    """
    return _os.path.basename(path[2:] if path[:2] in {"//", b"//"} else path)


def native_pathsep(path):
    """
    Replace the path separator with the systems native ``os.sep``.
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
    :type dirs: sequence
    :return: A unique list of paths.
    :rtype: list
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
