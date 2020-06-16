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

# Signing process overview.
#
# From buildbot worker side:
#  - Files which needs to be signed are collected from either a directory to
#    sign all signable files in there, or by filename of a single file to sign.
#  - Those files gets packed into an archive and stored in a location location
#    which is watched by the signing server.
#  - A marker READY file is created which indicates the archive is ready for
#    access.
#  - Wait for the server to provide an archive with signed files.
#    This is done by watching for the READY file which corresponds to an archive
#    coming from the signing server.
#  - Unpack the signed signed files from the archives and replace original ones.
#
# From code sign server:
#  - Watch special location for a READY file which indicates the there is an
#    archive with files which are to be signed.
#  - Unpack the archive to a temporary location.
#  - Run codesign tool and make sure all the files are signed.
#  - Pack the signed files and store them in a location which is watched by
#    the buildbot worker.
#  - Create a READY file which indicates that the archive with signed files is
#    ready.

import abc
import logging
import shutil
import subprocess
import time
import tarfile
import uuid

from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Iterable, List

import codesign.util as util

from codesign.absolute_and_relative_filename import AbsoluteAndRelativeFileName
from codesign.archive_with_indicator import ArchiveWithIndicator


logger = logging.getLogger(__name__)
logger_builder = logger.getChild('builder')
logger_server = logger.getChild('server')


def pack_files(files: Iterable[AbsoluteAndRelativeFileName],
               archive_filepath: Path) -> None:
    """
    Create tar archive from given files for the signing pipeline.
    Is used by buildbot worker to create an archive of files which are to be
    signed, and by signing server to send signed files back to the worker.
    """
    with tarfile.TarFile.open(archive_filepath, 'w') as tar_file_handle:
        for file_info in files:
            tar_file_handle.add(file_info.absolute_filepath,
                                arcname=file_info.relative_filepath)


def extract_files(archive_filepath: Path,
                  extraction_dir: Path) -> None:
    """
    Extract all files form the given archive into the given direcotry.
    """

    # TODO(sergey): Verify files in the archive have relative path.

    with tarfile.TarFile.open(archive_filepath, mode='r') as tar_file_handle:
        tar_file_handle.extractall(path=extraction_dir)


class BaseCodeSigner(metaclass=abc.ABCMeta):
    """
    Base class for a platform-specific signer of binaries.

    Contains all the logic shared across platform-specific implementations, such
    as synchronization and notification logic.

    Platform specific bits (such as actual command for signing the binary) are
    to be implemented as a subclass.

    Provides utilities code signing as a whole, including functionality needed
    by a signing server and a buildbot worker.

    The signer and builder may run on separate machines, the only requirement is
    that they have access to a directory which is shared between them. For the
    security concerns this is to be done as a separate machine (or as a Shared
    Folder configuration in VirtualBox configuration). This directory might be
    mounted under different base paths, but its underlying storage is to be
    the same.

    The code signer is short-lived on a buildbot worker side, and is living
    forever on a code signing server side.
    """

    # TODO(sergey): Find a neat way to have config annotated.
    # config: Config

    # Storage directory where builder puts files which are requested to be
    # signed.
    # Consider this an input of the code signing server.
    unsigned_storage_dir: Path

    # Storage where signed files are stored.
    # Consider this an output of the code signer server.
    signed_storage_dir: Path

    # Platform the code is currently executing on.
    platform: util.Platform

    def __init__(self, config):
        self.config = config

        absolute_shared_storage_dir = config.SHARED_STORAGE_DIR.resolve()

        # Unsigned (signing server input) configuration.
        self.unsigned_storage_dir = absolute_shared_storage_dir / 'unsigned'

        # Signed (signing server output) configuration.
        self.signed_storage_dir = absolute_shared_storage_dir / 'signed'

        self.platform = util.get_current_platform()

    def cleanup_environment_for_builder(self) -> None:
        # TODO(sergey): Revisit need of cleaning up the existing files.
        # In practice it wasn't so helpful, and with multiple clients
        # talking to the same server it becomes even mor etricky.
        pass

    def cleanup_environment_for_signing_server(self) -> None:
        # TODO(sergey): Revisit need of cleaning up the existing files.
        # In practice it wasn't so helpful, and with multiple clients
        # talking to the same server it becomes even mor etricky.
        pass

    def generate_request_id(self) -> str:
        """
        Generate an unique identifier for code signing request.
        """
        return str(uuid.uuid4())

    def archive_info_for_request_id(
            self, path: Path, request_id: str) -> ArchiveWithIndicator:
        return ArchiveWithIndicator(
            path, f'{request_id}.tar', f'{request_id}.ready')

    def signed_archive_info_for_request_id(
            self, request_id: str) -> ArchiveWithIndicator:
        return self.archive_info_for_request_id(
            self.signed_storage_dir, request_id);

    def unsigned_archive_info_for_request_id(
            self, request_id: str) -> ArchiveWithIndicator:
        return self.archive_info_for_request_id(
            self.unsigned_storage_dir, request_id);

    ############################################################################
    # Buildbot worker side helpers.

    @abc.abstractmethod
    def check_file_is_to_be_signed(
            self, file: AbsoluteAndRelativeFileName) -> bool:
        """
        Check whether file is to be signed.

        Is used by both single file signing pipeline and recursive directory
        signing pipeline.

        This is where code signer is to check whether file is to be signed or
        not. This check might be based on a simple extension test or on actual
        test whether file have a digital signature already or not.
        """

    def collect_files_to_sign(self, path: Path) \
            -> List[AbsoluteAndRelativeFileName]:
        """
        Get all files which need to be signed from the given path.

        NOTE: The path might either be a file or directory.

        This function is run from the buildbot worker side.
        """

        # If there is a single file provided trust the buildbot worker that it
        # is eligible for signing.
        if path.is_file():
            file = AbsoluteAndRelativeFileName.from_path(path)
            if not self.check_file_is_to_be_signed(file):
                return []
            return [file]

        all_files = AbsoluteAndRelativeFileName.recursively_from_directory(
            path)
        files_to_be_signed = [file for file in all_files
                              if self.check_file_is_to_be_signed(file)]
        return files_to_be_signed

    def wait_for_signed_archive_or_die(self, request_id) -> None:
        """
        Wait until archive with signed files is available.

        Will only wait for the configured time. If that time exceeds and there
        is still no responce from the signing server the application will exit
        with a non-zero exit code.
        """

        signed_archive_info = self.signed_archive_info_for_request_id(
            request_id)
        unsigned_archive_info = self.unsigned_archive_info_for_request_id(
            request_id)

        timeout_in_seconds = self.config.TIMEOUT_IN_SECONDS
        time_start = time.monotonic()
        while not signed_archive_info.is_ready():
            time.sleep(1)
            time_slept_in_seconds = time.monotonic() - time_start
            if time_slept_in_seconds > timeout_in_seconds:
                unsigned_archive_info.clean()
                raise SystemExit("Signing server didn't finish signing in "
                                 f"{timeout_in_seconds} seconds, dying :(")

    def copy_signed_files_to_directory(
            self, signed_dir: Path, destination_dir: Path) -> None:
        """
        Copy all files from signed_dir to destination_dir.

        This function will overwrite any existing file. Permissions are copied
        from the source files, but other metadata, such as timestamps, are not.
        """
        for signed_filepath in signed_dir.glob('**/*'):
            if not signed_filepath.is_file():
                continue

            relative_filepath = signed_filepath.relative_to(signed_dir)
            destination_filepath = destination_dir / relative_filepath
            destination_filepath.parent.mkdir(parents=True, exist_ok=True)

            shutil.copy(signed_filepath, destination_filepath)

    def run_buildbot_path_sign_pipeline(self, path: Path) -> None:
        """
        Run all steps needed to make given path signed.

        Path points to an unsigned file or a directory which contains unsigned
        files.

        If the path points to a single file then this file will be signed.
        This is used to sign a final bundle such as .msi on Windows or .dmg on
        macOS.

        NOTE: The code signed implementation might actually reject signing the
        file, in which case the file will be left unsigned. This isn't anything
        to be considered a failure situation, just might happen when buildbot
        worker can not detect whether signing is really required in a specific
        case or not.

        If the path points to a directory then code signer will sign all
        signable files from it (finding them recursively).
        """

        self.cleanup_environment_for_builder()

        # Make sure storage directory exists.
        self.unsigned_storage_dir.mkdir(parents=True, exist_ok=True)

        # Collect all files which needs to be signed and pack them into a single
        # archive which will be sent to the signing server.
        logger_builder.info('Collecting files which are to be signed...')
        files = self.collect_files_to_sign(path)
        if not files:
            logger_builder.info('No files to be signed, ignoring.')
            return
        logger_builder.info('Found %d files to sign.', len(files))

        request_id = self.generate_request_id()
        signed_archive_info = self.signed_archive_info_for_request_id(
            request_id)
        unsigned_archive_info = self.unsigned_archive_info_for_request_id(
            request_id)

        pack_files(files=files,
                   archive_filepath=unsigned_archive_info.archive_filepath)
        unsigned_archive_info.tag_ready()

        # Wait for the signing server to finish signing.
        logger_builder.info('Waiting signing server to sign the files...')
        self.wait_for_signed_archive_or_die(request_id)

        # Extract signed files from archive and move files to final location.
        with TemporaryDirectory(prefix='blender-buildbot-') as temp_dir_str:
            unpacked_signed_files_dir = Path(temp_dir_str)

            logger_builder.info('Extracting signed files from archive...')
            extract_files(
                archive_filepath=signed_archive_info.archive_filepath,
                extraction_dir=unpacked_signed_files_dir)

            destination_dir = path
            if destination_dir.is_file():
                destination_dir = destination_dir.parent
            self.copy_signed_files_to_directory(
                unpacked_signed_files_dir, destination_dir)

        logger_builder.info('Removing archive with signed files...')
        signed_archive_info.clean()

    ############################################################################
    # Signing server side helpers.

    def wait_for_sign_request(self) -> str:
        """
        Wait for the buildbot to request signing of an archive.

        Returns an identifier of signing request.
        """

        # TOOD(sergey): Support graceful shutdown on Ctrl-C.

        logger_server.info(
            'Waiting for a READY indicator of any signign request.')
        request_id = None
        while request_id is None:
            for file in self.unsigned_storage_dir.iterdir():
                if file.suffix != '.ready':
                    continue
                request_id = file.stem
                logger_server.info(f'Found READY for request ID {request_id}.')
            if request_id is None:
                time.sleep(1)

        unsigned_archive_info = self.unsigned_archive_info_for_request_id(
            request_id)
        while not unsigned_archive_info.is_ready():
            time.sleep(1)

        return request_id

    @abc.abstractmethod
    def sign_all_files(self, files: List[AbsoluteAndRelativeFileName]) -> None:
        """
        Sign all files in the given directory.

        NOTE: Signing should happen in-place.
        """

    def run_signing_pipeline(self, request_id: str):
        """
        Run the full signing pipeline starting from the point when buildbot
        worker have requested signing.
        """

        # Make sure storage directory exists.
        self.signed_storage_dir.mkdir(parents=True, exist_ok=True)

        with TemporaryDirectory(prefix='blender-codesign-') as temp_dir_str:
            temp_dir = Path(temp_dir_str)

            signed_archive_info = self.signed_archive_info_for_request_id(
                request_id)
            unsigned_archive_info = self.unsigned_archive_info_for_request_id(
                request_id)

            logger_server.info('Extracting unsigned files from archive...')
            extract_files(
                archive_filepath=unsigned_archive_info.archive_filepath,
                extraction_dir=temp_dir)

            logger_server.info('Collecting all files which needs signing...')
            files = AbsoluteAndRelativeFileName.recursively_from_directory(
                temp_dir)

            logger_server.info('Signing all requested files...')
            self.sign_all_files(files)

            logger_server.info('Packing signed files...')
            pack_files(files=files,
                       archive_filepath=signed_archive_info.archive_filepath)
            signed_archive_info.tag_ready()

            logger_server.info('Removing signing request...')
            unsigned_archive_info.clean()

            logger_server.info('Signing is complete.')

    def run_signing_server(self):
        logger_server.info('Starting new code signing server...')
        self.cleanup_environment_for_signing_server()
        logger_server.info('Code signing server is ready')
        while True:
            logger_server.info('Waiting for the signing request in %s...',
                               self.unsigned_storage_dir)
            request_id = self.wait_for_sign_request()

            logger_server.info(
                f'Beging signign procedure for request ID {request_id}.')
            self.run_signing_pipeline(request_id)

    ############################################################################
    # Command executing.
    #
    # Abstracted to a degree that allows to run commands from a foreign
    # platform.
    # The goal with this is to allow performing dry-run tests of code signer
    # server from other platforms (for example, to test that macOS code signer
    # does what it is supposed to after doing a refactor on Linux).

    # TODO(sergey): What is the type annotation for the command?
    def run_command_or_mock(self, command, platform: util.Platform) -> None:
        """
        Run given command if current platform matches given one

        If the platform is different then it will only be printed allowing
        to verify logic of the code signing process.
        """

        if platform != self.platform:
            logger_server.info(
                f'Will run command for {platform}: {command}')
            return

        logger_server.info(f'Running command: {command}')
        subprocess.run(command)

    # TODO(sergey): What is the type annotation for the command?
    def check_output_or_mock(self, command,
                             platform: util.Platform,
                             allow_nonzero_exit_code=False) -> str:
        """
        Run given command if current platform matches given one

        If the platform is different then it will only be printed allowing
        to verify logic of the code signing process.

        If allow_nonzero_exit_code is truth then the output will be returned
        even if application quit with non-zero exit code.
        Otherwise an subprocess.CalledProcessError exception will be raised
        in such case.
        """

        if platform != self.platform:
            logger_server.info(
                f'Will run command for {platform}: {command}')
            return

        if allow_nonzero_exit_code:
            process = subprocess.Popen(command,
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT)
            output = process.communicate()[0]
            return output.decode()

        logger_server.info(f'Running command: {command}')
        return subprocess.check_output(
            command, stderr=subprocess.STDOUT).decode()
