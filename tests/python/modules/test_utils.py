#!/usr/bin/env python3
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

import argparse
import functools
import shutil
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest


def with_tempdir(wrapped):
    """Creates a temporary directory for the function, cleaning up after it returns normally.

    When the wrapped function raises an exception, the contents of the temporary directory
    remain available for manual inspection.

    The wrapped function is called with an extra positional argument containing
    the pathlib.Path() of the temporary directory.
    """

    @functools.wraps(wrapped)
    def decorator(*args, **kwargs):
        dirname = tempfile.mkdtemp(prefix='blender-alembic-test')
        try:
            retval = wrapped(*args, pathlib.Path(dirname), **kwargs)
        except:
            print('Exception in %s, not cleaning up temporary directory %s' % (wrapped, dirname))
            raise
        else:
            shutil.rmtree(dirname)
        return retval

    return decorator


class AbstractBlenderRunnerTest(unittest.TestCase):
    """Base class for all test suites which needs to run Blender"""

    # Set in a subclass
    blender: pathlib.Path = None
    testdir: pathlib.Path = None

    def run_blender(self, filepath: str, python_script: str, timeout: int=300) -> str:
        """Runs Blender by opening a blendfile and executing a script.

        Returns Blender's stdout + stderr combined into one string.

        :param filepath: taken relative to self.testdir.
        :param timeout: in seconds
        """

        assert self.blender, "Path to Blender binary is to be set in setUpClass()"
        assert self.testdir, "Path to tests binary is to be set in setUpClass()"

        blendfile = self.testdir / filepath if filepath else ""

        command = [
            self.blender,
            '--background',
            '-noaudio',
            '--factory-startup',
            '--enable-autoexec',
        ]

        if blendfile:
            command.append(str(blendfile))

        command.extend([
            '-E', 'CYCLES',
            '--python-exit-code', '47',
            '--python-expr', python_script,
        ]
        )

        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=timeout)
        output = proc.stdout.decode('utf8')
        if proc.returncode:
            self.fail('Error %d running Blender:\n%s' % (proc.returncode, output))

        return output
