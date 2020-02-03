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

from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass
class AbsoluteAndRelativeFileName:
    """
    Helper class which keeps track of absolute file path for a direct access and
    corresponding relative path against given base.

    The relative part is used to construct a file name within an archive which
    contains files which are to be signed or which has been signed already
    (depending on whether the archive is addressed to signing server or back
    to the buildbot worker).
    """

    # Base directory which is where relative_filepath is relative to.
    base_dir: Path

    # Full absolute path of the corresponding file.
    absolute_filepath: Path

    # Derived from full file path, contains part of the path which is relative
    # to a desired base path.
    relative_filepath: Path

    def __init__(self, base_dir: Path, filepath: Path):
        self.base_dir = base_dir
        self.absolute_filepath = filepath.resolve()
        self.relative_filepath = self.absolute_filepath.relative_to(
            self.base_dir)

    @classmethod
    def from_path(cls, path: Path) -> 'AbsoluteAndRelativeFileName':
        assert path.is_absolute()
        assert path.is_file()

        base_dir = path.parent
        return AbsoluteAndRelativeFileName(base_dir, path)

    @classmethod
    def recursively_from_directory(cls, base_dir: Path) \
            -> List['AbsoluteAndRelativeFileName']:
        """
        Create list of AbsoluteAndRelativeFileName for all the files in the
        given directory.

        NOTE: Result will be pointing to a resolved paths.
        """
        assert base_dir.is_absolute()
        assert base_dir.is_dir()

        base_dir = base_dir.resolve()

        result = []
        for filename in base_dir.glob('**/*'):
            if not filename.is_file():
                continue
            result.append(AbsoluteAndRelativeFileName(base_dir, filename))
        return result
