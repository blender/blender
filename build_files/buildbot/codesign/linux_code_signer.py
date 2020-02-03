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

# NOTE: This is a no-op signer (since there isn't really a procedure to sign
# Linux binaries yet). Used to debug and verify the code signing routines on
# a Linux environment.

import logging

from pathlib import Path
from typing import List

from codesign.absolute_and_relative_filename import AbsoluteAndRelativeFileName
from codesign.base_code_signer import BaseCodeSigner

logger = logging.getLogger(__name__)
logger_server = logger.getChild('server')


class LinuxCodeSigner(BaseCodeSigner):
    def is_active(self) -> bool:
        """
        Check whether this signer is active.

        if it is inactive, no files will be signed.

        Is used to be able to debug code signing pipeline on Linux, where there
        is no code signing happening in the actual buildbot and release
        environment.
        """
        return False

    def check_file_is_to_be_signed(
            self, file: AbsoluteAndRelativeFileName) -> bool:
        if file.relative_filepath == Path('blender'):
            return True
        if (file.relative_filepath.parts[-3:-1] == ('python', 'bin') and
                file.relative_filepath.name.startwith('python')):
            return True
        if file.relative_filepath.suffix == '.so':
            return True
        return False

    def collect_files_to_sign(self, path: Path) \
            -> List[AbsoluteAndRelativeFileName]:
        if not self.is_active():
            return []

        return super().collect_files_to_sign(path)

    def sign_all_files(self, files: List[AbsoluteAndRelativeFileName]) -> None:
        num_files = len(files)
        for file_index, file in enumerate(files):
            logger.info('Server: Signed file [%d/%d] %s',
                        file_index + 1, num_files, file.relative_filepath)
