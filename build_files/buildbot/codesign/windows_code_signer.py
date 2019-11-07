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

import logging
import subprocess

from pathlib import Path
from typing import List

from buildbot_utils import Builder

from codesign.absolute_and_relative_filename import AbsoluteAndRelativeFileName
from codesign.base_code_signer import BaseCodeSigner

logger = logging.getLogger(__name__)
logger_server = logger.getChild('server')

# NOTE: Check is done as filename.endswith(), so keep the dot
EXTENSIONS_TO_BE_SIGNED = {'.exe', '.dll', '.pyd', '.msi'}

BLACKLIST_FILE_PREFIXES = (
    'api-ms-', 'concrt', 'msvcp', 'ucrtbase', 'vcomp', 'vcruntime')


class WindowsCodeSigner(BaseCodeSigner):
    def check_file_is_to_be_signed(
            self, file: AbsoluteAndRelativeFileName) -> bool:
        base_name = file.relative_filepath.name
        if any(base_name.startswith(prefix)
               for prefix in BLACKLIST_FILE_PREFIXES):
            return False

        return file.relative_filepath.suffix in EXTENSIONS_TO_BE_SIGNED

    def get_sign_command_prefix(self) -> List[str]:
        return [
            'signtool', 'sign', '/v',
            '/f', self.config.CERTIFICATE_FILEPATH,
            '/t', self.config.TIMESTAMP_AUTHORITY_URL]

    def sign_all_files(self, files: List[AbsoluteAndRelativeFileName]) -> None:
        # NOTE: Sign files one by one to avoid possible command line length
        # overflow (which could happen if we ever decide to sign every binary
        # in the install folder, for example).
        #
        # TODO(sergey): Consider doing batched signing of handful of files in
        # one go (but only if this actually known to be much faster).
        num_files = len(files)
        for file_index, file in enumerate(files):
            command = self.get_sign_command_prefix()
            command.append(file.absolute_filepath)
            logger_server.info(
                'Running signtool command for file [%d/%d] %s...',
                file_index + 1, num_files, file.relative_filepath)
            # TODO(sergey): Check the status somehow. With a missing certificate
            # the command still exists with a zero code.
            subprocess.run(command)
        # TODO(sergey): Report number of signed and ignored files.
