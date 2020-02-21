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

import os
from pathlib import Path

import codesign.util as util


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

        # Sometimes on macOS indicator file appears prior to the actual archive
        # despite the order of creation and os.sync() used in tag_ready().
        # So consider archive not ready if there is an indicator without an
        # actual archive.
        if not self.archive_filepath.exists():
            print('Found indicator without actual archive, waiting for archive '
                  f'({self.archive_filepath}) to appear.')
            return False

        # Read archive size from indicator/
        #
        # Assume that file is either empty or is fully written. This is being checked
        # by performing ValueError check since empty string will throw this exception
        # when attempted to be converted to int.
        expected_archive_size_str = self.ready_indicator_filepath.read_text()
        try:
            expected_archive_size = int(expected_archive_size_str)
        except ValueError:
            print(f'Invalid archive size "{expected_archive_size_str}"')
            return False

        # Wait for until archive is fully stored.
        actual_archive_size = self.archive_filepath.stat().st_size
        if  actual_archive_size != expected_archive_size:
            print('Partial/invalid archive size (expected '
                  f'{expected_archive_size} got {actual_archive_size})')
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

    def tag_ready(self) -> None:
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
        archive_size = self.archive_filepath.stat().st_size
        self.ready_indicator_filepath.write_text(str(archive_size))

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
