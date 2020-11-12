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
import re
import stat
import subprocess
import time

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
EXTENSIONS_TO_BE_SIGNED = {'.dylib', '.so', '.dmg'}

# Prefixes of a file (not directory) name which are to be signed.
# Used to sign extra executable files in Contents/Resources.
NAME_PREFIXES_TO_BE_SIGNED = {'python'}


class NotarizationException(CodeSignException):
    pass


def is_file_from_bundle(file: AbsoluteAndRelativeFileName) -> bool:
    """
    Check whether file is coming from an .app bundle
    """
    parts = file.relative_filepath.parts
    if not parts:
        return False
    if not parts[0].endswith('.app'):
        return False
    return True


def get_bundle_from_file(
        file: AbsoluteAndRelativeFileName) -> AbsoluteAndRelativeFileName:
    """
    Get AbsoluteAndRelativeFileName descriptor of bundle
    """
    assert(is_file_from_bundle(file))

    parts = file.relative_filepath.parts
    bundle_name = parts[0]

    base_dir = file.base_dir
    bundle_filepath = file.base_dir / bundle_name
    return AbsoluteAndRelativeFileName(base_dir, bundle_filepath)


def is_bundle_executable_file(file: AbsoluteAndRelativeFileName) -> bool:
    """
    Check whether given file is an executable within an app bundle
    """
    if not is_file_from_bundle(file):
        return False

    parts = file.relative_filepath.parts
    num_parts = len(parts)
    if num_parts < 3:
        return False

    if parts[1:3] != ('Contents', 'MacOS'):
        return False

    return True


def xcrun_field_value_from_output(field: str, output: str) -> str:
    """
    Get value of a given field from xcrun output.

    If field is not found empty string is returned.
    """

    field_prefix = field + ': '
    for line in output.splitlines():
        line = line.strip()
        if line.startswith(field_prefix):
            return line[len(field_prefix):]
    return ''


class MacOSCodeSigner(BaseCodeSigner):
    def check_file_is_to_be_signed(
            self, file: AbsoluteAndRelativeFileName) -> bool:
        if file.relative_filepath.name.startswith('.'):
            return False

        if is_bundle_executable_file(file):
            return True

        base_name = file.relative_filepath.name
        if any(base_name.startswith(prefix)
               for prefix in NAME_PREFIXES_TO_BE_SIGNED):
            return True

        mode = file.absolute_filepath.lstat().st_mode
        if mode & stat.S_IXUSR != 0:
            file_output = subprocess.check_output(
                ("file", file.absolute_filepath)).decode()
            if "64-bit executable" in file_output:
                return True

        return file.relative_filepath.suffix in EXTENSIONS_TO_BE_SIGNED

    def collect_files_to_sign(self, path: Path) \
            -> List[AbsoluteAndRelativeFileName]:
        # Include all files when signing app or dmg bundle: all the files are
        # needed to do valid signature of bundle.
        if path.name.endswith('.app'):
            return AbsoluteAndRelativeFileName.recursively_from_directory(path)
        if path.is_dir():
            files = []
            for child in path.iterdir():
                if child.name.endswith('.app'):
                    current_files = AbsoluteAndRelativeFileName.recursively_from_directory(
                        child)
                else:
                    current_files = super().collect_files_to_sign(child)
                for current_file in current_files:
                    files.append(AbsoluteAndRelativeFileName(
                        path, current_file.absolute_filepath))
            return files
        return super().collect_files_to_sign(path)

    ############################################################################
    # Codesign.

    def codesign_remove_signature(
            self, file: AbsoluteAndRelativeFileName) -> None:
        """
        Make sure given file does not have codesign signature

        This is needed because codesigning is not possible for file which has
        signature already.
        """

        logger_server.info(
            'Removing codesign signature from %s...', file.relative_filepath)

        command = ['codesign', '--remove-signature', file.absolute_filepath]
        self.run_command_or_mock(command, util.Platform.MACOS)

    def codesign_file(
            self, file: AbsoluteAndRelativeFileName) -> None:
        """
        Sign given file

        NOTE: File must not have any signatures.
        """

        logger_server.info(
            'Codesigning %s...', file.relative_filepath)

        entitlements_file = self.config.MACOS_ENTITLEMENTS_FILE
        command = ['codesign',
                   '--timestamp',
                   '--options', 'runtime',
                   f'--entitlements={entitlements_file}',
                   '--sign', self.config.MACOS_CODESIGN_IDENTITY,
                   file.absolute_filepath]
        self.run_command_or_mock(command, util.Platform.MACOS)

    def codesign_all_files(self, files: List[AbsoluteAndRelativeFileName]) -> None:
        """
        Run codesign tool on all eligible files in the given list.

        Will ignore all files which are not to be signed. For the rest will
        remove possible existing signature and add a new signature.
        """

        num_files = len(files)
        have_ignored_files = False
        signed_files = []
        for file_index, file in enumerate(files):
            # Ignore file if it is not to be signed.
            # Allows to manually construct ZIP of a bundle and get it signed.
            if not self.check_file_is_to_be_signed(file):
                logger_server.info(
                    'Ignoring file [%d/%d] %s',
                    file_index + 1, num_files, file.relative_filepath)
                have_ignored_files = True
                continue

            logger_server.info(
                'Running codesigning routines for file [%d/%d] %s...',
                file_index + 1, num_files, file.relative_filepath)

            self.codesign_remove_signature(file)
            self.codesign_file(file)

            signed_files.append(file)

        if have_ignored_files:
            logger_server.info('Signed %d files:', len(signed_files))
            num_signed_files = len(signed_files)
            for file_index, signed_file in enumerate(signed_files):
                logger_server.info(
                    '- [%d/%d] %s',
                    file_index + 1, num_signed_files,
                    signed_file.relative_filepath)

    def codesign_bundles(
            self, files: List[AbsoluteAndRelativeFileName]) -> None:
        """
        Codesign all .app bundles in the given list of files.

        Bundle is deducted from paths of the files, and every bundle is only
        signed once.
        """

        signed_bundles = set()
        extra_files = []

        for file in files:
            if not is_file_from_bundle(file):
                continue
            bundle = get_bundle_from_file(file)
            bundle_name = bundle.relative_filepath
            if bundle_name in signed_bundles:
                continue

            logger_server.info('Running codesign routines on bundle %s',
                               bundle_name)

            # It is not possible to remove signature from DMG.
            if bundle.relative_filepath.name.endswith('.app'):
                self.codesign_remove_signature(bundle)
            self.codesign_file(bundle)

            signed_bundles.add(bundle_name)

            # Codesign on a bundle adds an extra folder with information.
            # It needs to be compied to the source.
            code_signature_directory = \
                bundle.absolute_filepath / 'Contents' / '_CodeSignature'
            code_signature_files = \
                AbsoluteAndRelativeFileName.recursively_from_directory(
                    code_signature_directory)
            for code_signature_file in code_signature_files:
                bundle_relative_file = AbsoluteAndRelativeFileName(
                    bundle.base_dir,
                    code_signature_directory /
                    code_signature_file.relative_filepath)
                extra_files.append(bundle_relative_file)

        files.extend(extra_files)

    ############################################################################
    # Notarization.

    def notarize_get_bundle_id(self, file: AbsoluteAndRelativeFileName) -> str:
        """
        Get bundle ID which will be used to notarize DMG
        """
        name = file.relative_filepath.name
        app_name = name.split('-', 2)[0].lower()

        app_name_words = app_name.split()
        if len(app_name_words) > 1:
            app_name_id = ''.join(word.capitalize() for word in app_name_words)
        else:
            app_name_id = app_name_words[0]

        # TODO(sergey): Consider using "alpha" for buildbot builds.
        return f'org.blenderfoundation.{app_name_id}.release'

    def notarize_request(self, file) -> str:
        """
        Request notarization of the given file.

        Returns UUID of the notarization request. If error occurred None is
        returned instead of UUID.
        """

        bundle_id = self.notarize_get_bundle_id(file)
        logger_server.info('Bundle ID: %s', bundle_id)

        logger_server.info('Submitting file to the notarial office.')
        command = [
            'xcrun', 'altool', '--notarize-app', '--verbose',
            '-f', file.absolute_filepath,
            '--primary-bundle-id', bundle_id,
            '--username', self.config.MACOS_XCRUN_USERNAME,
            '--password', self.config.MACOS_XCRUN_PASSWORD]

        output = self.check_output_or_mock(
            command, util.Platform.MACOS, allow_nonzero_exit_code=True)

        for line in output.splitlines():
            line = line.strip()
            if line.startswith('RequestUUID = '):
                request_uuid = line[14:]
                return request_uuid

            # Check whether the package has been already submitted.
            if 'The software asset has already been uploaded.' in line:
                request_uuid = re.sub(
                    '.*The upload ID is ([A-Fa-f0-9\-]+).*', '\\1', line)
                logger_server.warning(
                    f'The package has been already submitted under UUID {request_uuid}')
                return request_uuid

        logger_server.error(output)
        logger_server.error('xcrun command did not report RequestUUID')
        return None

    def notarize_review_status(self, xcrun_output: str) -> bool:
        """
        Review status returned by xcrun's notarization info

        Returns truth if the notarization process has finished.
        If there are errors during notarization, a NotarizationException()
        exception is thrown with status message from the notarial office.
        """

        # Parse status and message
        status = xcrun_field_value_from_output('Status', xcrun_output)
        status_message = xcrun_field_value_from_output(
            'Status Message', xcrun_output)

        if status == 'success':
            logger_server.info(
                'Package successfully notarized: %s', status_message)
            return True

        if status == 'invalid':
            logger_server.error(xcrun_output)
            logger_server.error(
                'Package notarization has failed: %s', status_message)
            raise NotarizationException(status_message)

        if status == 'in progress':
            return False

        logger_server.info(
            'Unknown notarization status %s (%s)', status, status_message)

        return False

    def notarize_wait_result(self, request_uuid: str) -> None:
        """
        Wait for until notarial office have a reply
        """

        logger_server.info(
            'Waiting for a result from the notarization office.')

        command = ['xcrun', 'altool',
                   '--notarization-info', request_uuid,
                   '--username', self.config.MACOS_XCRUN_USERNAME,
                   '--password', self.config.MACOS_XCRUN_PASSWORD]

        time_start = time.monotonic()
        timeout_in_seconds = self.config.MACOS_NOTARIZE_TIMEOUT_IN_SECONDS

        while True:
            xcrun_output = self.check_output_or_mock(
                command, util.Platform.MACOS, allow_nonzero_exit_code=True)

            if self.notarize_review_status(xcrun_output):
                break

            logger_server.info('Keep waiting for notarization office.')
            time.sleep(30)

            time_slept_in_seconds = time.monotonic() - time_start
            if time_slept_in_seconds > timeout_in_seconds:
                logger_server.error(
                    "Notarial office didn't reply in %f seconds.",
                    timeout_in_seconds)

    def notarize_staple(self, file: AbsoluteAndRelativeFileName) -> bool:
        """
        Staple notarial label on the file
        """

        logger_server.info('Stapling notarial stamp.')

        command = ['xcrun', 'stapler', 'staple', '-v', file.absolute_filepath]
        self.check_output_or_mock(command, util.Platform.MACOS)

    def notarize_dmg(self, file: AbsoluteAndRelativeFileName) -> bool:
        """
        Run entire pipeline to get DMG notarized.
        """
        logger_server.info('Begin notarization routines on %s',
                           file.relative_filepath)

        # Submit file for notarization.
        request_uuid = self.notarize_request(file)
        if not request_uuid:
            return False
        logger_server.info('Received Request UUID: %s', request_uuid)

        # Wait for the status from the notarization office.
        if not self.notarize_wait_result(request_uuid):
            return False

        # Staple.
        self.notarize_staple(file)

    def notarize_all_dmg(
            self, files: List[AbsoluteAndRelativeFileName]) -> bool:
        """
        Notarize all DMG images from the input.

        Images are supposed to be codesigned already.
        """
        for file in files:
            if not file.relative_filepath.name.endswith('.dmg'):
                continue
            if not self.check_file_is_to_be_signed(file):
                continue

            self.notarize_dmg(file)

    ############################################################################
    # Entry point.

    def sign_all_files(self, files: List[AbsoluteAndRelativeFileName]) -> None:
        # TODO(sergey): Handle errors somehow.

        self.codesign_all_files(files)
        self.codesign_bundles(files)
        self.notarize_all_dmg(files)
