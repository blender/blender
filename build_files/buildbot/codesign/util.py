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

import sys

from enum import Enum
from pathlib import Path


class Platform(Enum):
    LINUX = 1
    MACOS = 2
    WINDOWS = 3


def get_current_platform() -> Platform:
    if sys.platform == 'linux':
        return Platform.LINUX
    elif sys.platform == 'darwin':
        return Platform.MACOS
    elif sys.platform == 'win32':
        return Platform.WINDOWS
    raise Exception(f'Unknown platform {sys.platform}')


def ensure_file_does_not_exist_or_die(filepath: Path) -> None:
    """
    If the file exists, unlink it.
    If the file path exists and is not a file an assert will trigger.
    If the file path does not exists nothing happens.
    """
    if not filepath.exists():
        return
    if not filepath.is_file():
        # TODO(sergey): Provide information about what the filepath actually is.
        raise SystemExit(f'{filepath} is expected to be a file, but is not')
    filepath.unlink()
