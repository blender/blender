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
This module has a similar scope to os.path, containing utility
functions for dealing with paths in Blender.
"""

import bpy as _bpy
import os as _os


def abspath(path, start=None):
    """
    Returns the absolute path relative to the current blend file using the "//" prefix.

    :arg start: Relative to this path, when not set the current filename is used.
    :type start: string
    """
    if path.startswith("//"):
        return _os.path.join(_os.path.dirname(_bpy.data.filepath) if start is None else start, path[2:])

    return path


def relpath(path, start=None):
    """
    Returns the path relative to the current blend file using the "//" prefix.

    :arg start: Relative to this path, when not set the current filename is used.
    :type start: string
    """
    if not path.startswith("//"):
        if start is None:
            start = _os.path.dirname(_bpy.data.filepath)
        return "//" + _os.path.relpath(path, start)

    return path


def is_subdir(path, directory):
    """
    Returns true if *path* in a subdirectory of *directory*.
    Both paths must be absolute.
    """
    from os.path import normpath, normcase
    path = normpath(normcase(path))
    directory = normpath(normcase(directory))
    return path.startswith(directory)


def clean_name(name, replace="_"):
    """
    Returns a name with characters replaced that may cause problems under various circumstances, such as writing to a file.
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the replace argument if defined.
    """

    unclean_chars = \
                 "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\
                  \x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\
                  \x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\
                  \x2e\x2f\x3a\x3b\x3c\x3d\x3e\x3f\x40\x5b\x5c\x5d\x5e\x60\x7b\
                  \x7c\x7d\x7e\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\
                  \x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\
                  \x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\
                  \xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\
                  \xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\
                  \xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\
                  \xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\
                  \xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\
                  \xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe"

    for ch in unclean_chars:
        name = name.replace(ch, replace)
    return name


def display_name(name):
    """
    Creates a display string from name to be used menus and the user interface.
    Capitalize the first letter in all lowercase names, mixed case names are kept as is.
    Intended for use with filenames and module names.
    """

    name_base = _os.path.splitext(name)[0]

    # string replacements
    name_base = name_base.replace("_colon_", ":")

    name_base = name_base.replace("_", " ")

    if name_base.islower():
        return name_base.lower().title()
    else:
        return name_base


def display_name_from_filepath(name):
    """
    Returns the path stripped of directort and extension, ensured to be utf8 compatible.
    """
    return _os.path.splitext(_os.path.basename(name))[0].encode("utf8", "replace").decode("utf8")


def resolve_ncase(path):
    """
    Resolve a case insensitive path on a case sensitive system,
    returning a string with the path if found else return the original path.
    """

    import os

    def _ncase_path_found(path):
        if not path or os.path.exists(path):
            return path, True

        filename = os.path.basename(path)  # filename may be a directory or a file
        dirpath = os.path.dirname(path)

        suffix = path[:0]  # "" but ensure byte/str match
        if not filename:  # dir ends with a slash?
            if len(dirpath) < len(path):
                suffix = path[:len(path) - len(dirpath)]

            filename = os.path.basename(dirpath)
            dirpath = os.path.dirname(dirpath)

        if not os.path.exists(dirpath):
            if dirpath == path:
                return path, False

            dirpath, found = _ncase_path_found(dirpath)

            if not found:
                return path, False

        # at this point, the directory exists but not the file

        # we are expecting 'dirpath' to be a directory, but it could be a file
        if os.path.isdir(dirpath):
            files = os.listdir(dirpath)
        else:
            return path, False

        filename_low = filename.lower()
        f_iter_nocase = None

        for f_iter in files:
            if f_iter.lower() == filename_low:
                f_iter_nocase = f_iter
                break

        if f_iter_nocase:
            return os.path.join(dirpath, f_iter_nocase) + suffix, True
        else:
            # cant find the right one, just return the path as is.
            return path, False

    ncase_path, found = _ncase_path_found(path)
    return ncase_path if found else path


def ensure_ext(filepath, ext, case_sensitive=False):
    """
    Return the path with the extension added its its not alredy set.

    :arg ext: The extension to check for.
    :type ext: string
    :arg case_sensitive: Check for matching case when comparing extensions.
    :type case_sensitive: bool
    """
    import os
    fn_base, fn_ext = os.path.splitext(filepath)
    if fn_base and fn_ext:
        if (case_sensitive and ext == fn_ext) or (ext.lower() == fn_ext.lower()):
            return filepath
        else:
            return fn_base + ext

    else:
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
        elif ("." not in filename):
            directory = join(path, filename)
            fullpath = join(directory, "__init__.py")
            if isfile(fullpath):
                modules.append((filename, fullpath))
                if recursive:
                    for mod_name, mod_path in module_names(directory, True):
                        modules.append(("%s.%s" % (filename, mod_name), mod_path))

    return modules
