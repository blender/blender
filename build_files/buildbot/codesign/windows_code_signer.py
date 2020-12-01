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

from pathlib import Path
from typing import List

import codesign.util as util

from buildbot_utils import Builder

from codesign.absolute_and_relative_filename import AbsoluteAndRelativeFileName
from codesign.base_code_signer import BaseCodeSigner
from codesign.exception import CodeSignException

logger = logging.getLogger(__name__)
logger_server = logger.getChild('server')

# NOTE: Check is done as filename.endswith(), so keep the dot
EXTENSIONS_TO_BE_SIGNED = {'.exe', '.dll', '.pyd', '.msi'}

BLACKLIST_FILE_PREFIXES = (
    'api-ms-', 'concrt', 'msvcp', 'ucrtbase', 'vcomp', 'vcruntime')


class SigntoolException(CodeSignException):
    pass

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
            '/f', self.config.WIN_CERTIFICATE_FILEPATH,
            '/tr', self.config.WIN_TIMESTAMP_AUTHORITY_URL]


    def run_codesign_tool(self, filepath: Path) -> None:
        command = self.get_sign_command_prefix() + [filepath]

        try:
            codesign_output = self.check_output_or_mock(command, util.Platform.WINDOWS)
        except subprocess.CalledProcessError as e:
            raise SigntoolException(f'Error running signtool {e}')

        logger_server.info(f'signtool output:\n{codesign_output}')

        got_number_of_success = False

        for line in codesign_output.split('\n'):
            line_clean = line.strip()
            line_clean_lower = line_clean.lower()

            if line_clean_lower.startswith('number of warnings') or \
               line_clean_lower.startswith('number of errors'):
                 number = int(line_clean_lower.split(':')[1])
                 if number != 0:
                     raise SigntoolException('Non-clean success of signtool')

            if line_clean_lower.startswith('number of files successfully signed'):
                 got_number_of_success = True
                 number = int(line_clean_lower.split(':')[1])
                 if number != 1:
                     raise SigntoolException('Signtool did not consider codesign a success')

        if not got_number_of_success:
            raise SigntoolException('Signtool did not report number of files signed')


    def sign_all_files(self, files: List[AbsoluteAndRelativeFileName]) -> None:
        # NOTE: Sign files one by one to avoid possible command line length
        # overflow (which could happen if we ever decide to sign every binary
        # in the install folder, for example).
        #
        # TODO(sergey): Consider doing batched signing of handful of files in
        # one go (but only if this actually known to be much faster).
        num_files = len(files)
        for file_index, file in enumerate(files):
            # Ignore file if it is not to be signed.
            # Allows to manually construct ZIP of package and get it signed.
            if not self.check_file_is_to_be_signed(file):
                logger_server.info(
                    'Ignoring file [%d/%d] %s',
                    file_index + 1, num_files, file.relative_filepath)
                continue

            logger_server.info(
                'Running signtool command for file [%d/%d] %s...',
                file_index + 1, num_files, file.relative_filepath)
            self.run_codesign_tool(file.absolute_filepath)
