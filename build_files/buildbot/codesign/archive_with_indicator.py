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

import dataclasses
import json
import os

from pathlib import Path
from typing import Optional

import codesign.util as util


class ArchiveStateError(Exception):
    message: str

    def __init__(self, message):
        self.message = message
        super().__init__(self.message)


@dataclasses.dataclass
class ArchiveState:
    """
    Additional information (state) of the archive

    Includes information like expected file size of the archive file in the case
    the archive file is expected to be successfully created.

    If the archive can not be created, this state will contain error message
    indicating details of error.
    """

    # Size in bytes of the corresponding archive.
    file_size: Optional[int] = None

    # Non-empty value indicates that error has happenned.
    error_message: str = ''

    def has_error(self) -> bool:
        """
        Check whether the archive is at error state
        """

        return self.error_message

    def serialize_to_string(self) -> str:
        payload = dataclasses.asdict(self)
        return json.dumps(payload, sort_keys=True, indent=4)

    def serialize_to_file(self, filepath: Path) -> None:
        string = self.serialize_to_string()
        filepath.write_text(string)

    @classmethod
    def deserialize_from_string(cls, string: str) -> 'ArchiveState':
        try:
            object_as_dict = json.loads(string)
        except json.decoder.JSONDecodeError:
            raise ArchiveStateError('Error parsing JSON')

        return cls(**object_as_dict)

    @classmethod
    def deserialize_from_file(cls, filepath: Path):
        string = filepath.read_text()
        return cls.deserialize_from_string(string)


class ArchiveWithIndicator:
    """
    The idea of this class is to wrap around logic which takes care of keeping
    track of a name of an archive and synchronization routines between buildbot
    worker and signing server.

    The synchronization is done based on creating a special file after the
    archive file is knowingly ready for access.
    """

    # Base directory where the archive is stored (basically, a basename() of
    # the absolute archive file name).
    #
    # For example, 'X:\\TEMP\\'.
    base_dir: Path

    # Absolute file name of the archive.
    #
    # For example, 'X:\\TEMP\\FOO.ZIP'.
    archive_filepath: Path

    # Absolute name of a file which acts as an indication of the fact that the
    # archive is ready and is available for access.
    #
    # This is how synchronization between buildbot worker and signing server is
    # done:
    # - First, the archive is created under archive_filepath name.
    # - Second, the indication file is created under ready_indicator_filepath
    #   name.
    # - Third, the colleague of whoever created the indicator name watches for
    #   the indication file to appear, and once it's there it access the
    #   archive.
    ready_indicator_filepath: Path

    def __init__(
            self, base_dir: Path, archive_name: str, ready_indicator_name: str):
        """
        Construct the object from given base directory and name of the archive
        file:
          ArchiveWithIndicator(Path('X:\\TEMP'), 'FOO.ZIP', 'INPUT_READY')
        """

        self.base_dir = base_dir
        self.archive_filepath = self.base_dir / archive_name
        self.ready_indicator_filepath = self.base_dir / ready_indicator_name

    def is_ready_unsafe(self) -> bool:
        """
        Check whether the archive is ready for access.

        No guarding about possible network failres is done here.
        """
        if not self.ready_indicator_filepath.exists():
            return False

        try:
            archive_state = ArchiveState.deserialize_from_file(
                self.ready_indicator_filepath)
        except ArchiveStateError as error:
            print(f'Error deserializing archive state: {error.message}')
            return False

        if archive_state.has_error():
            # If the error did happen during codesign procedure there will be no
            # corresponding archive file.
            # The caller code will deal with the error check further.
            return True

        # Sometimes on macOS indicator file appears prior to the actual archive
        # despite the order of creation and os.sync() used in tag_ready().
        # So consider archive not ready if there is an indicator without an
        # actual archive.
        if not self.archive_filepath.exists():
            print('Found indicator without actual archive, waiting for archive '
                  f'({self.archive_filepath}) to appear.')
            return False

        # Wait for until archive is fully stored.
        actual_archive_size = self.archive_filepath.stat().st_size
        if actual_archive_size != archive_state.file_size:
            print('Partial/invalid archive size (expected '
                  f'{archive_state.file_size} got {actual_archive_size})')
            return False

        return True

    def is_ready(self) -> bool:
        """
        Check whether the archive is ready for access.

        Will tolerate possible network failures: if there is a network failure
        or if there is still no proper permission on a file False is returned.
        """

        # There are some intermitten problem happening at a random which is
        # translates to "OSError : [WinError 59] An unexpected network error occurred".
        # Some reports suggests it might be due to lack of permissions to the file,
        # which might be applicable in our case since it's possible that file is
        # initially created with non-accessible permissions and gets chmod-ed
        # after initial creation.
        try:
            return self.is_ready_unsafe()
        except OSError as e:
            print(f'Exception checking archive: {e}')
            return False

    def tag_ready(self, error_message='') -> None:
        """
        Tag the archive as ready by creating the corresponding indication file.

        NOTE: It is expected that the archive was never tagged as ready before
              and that there are no subsequent tags of the same archive.
              If it is violated, an assert will fail.
        """
        assert not self.is_ready()

        # Try the best to make sure everything is synced to the file system,
        # to avoid any possibility of stamp appearing on a network share prior to
        # an actual file.
        if util.get_current_platform() != util.Platform.WINDOWS:
            os.sync()

        archive_size = -1
        if self.archive_filepath.exists():
            archive_size = self.archive_filepath.stat().st_size

        archive_info = ArchiveState(
            file_size=archive_size, error_message=error_message)

        self.ready_indicator_filepath.write_text(
            archive_info.serialize_to_string())

    def get_state(self) -> ArchiveState:
        """
        Get state object for this archive

        The state is read from the corresponding state file.
        """

        try:
            return ArchiveState.deserialize_from_file(self.ready_indicator_filepath)
        except ArchiveStateError as error:
            return ArchiveState(error_message=f'Error in information format: {error}')

    def clean(self) -> None:
        """
        Remove both archive and the ready indication file.
        """
        util.ensure_file_does_not_exist_or_die(self.ready_indicator_filepath)
        util.ensure_file_does_not_exist_or_die(self.archive_filepath)

    def is_fully_absent(self) -> bool:
        """
        Check whether both archive and its ready indicator are absent.
        Is used for a sanity check during code signing process by both
        buildbot worker and signing server.
        """
        return (not self.archive_filepath.exists() and
                not self.ready_indicator_filepath.exists())
