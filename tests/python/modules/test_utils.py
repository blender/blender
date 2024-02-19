#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2018-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import functools
import shutil
import pathlib
import subprocess
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

    def run_blender(self, filepath: str, python_script: str, timeout: int = 300) -> str:
        """Runs Blender by opening a blendfile and executing a script.

        Returns Blender's stdout + stderr combined into one string.

        :arg filepath: taken relative to self.testdir.
        :arg timeout: in seconds
        """

        assert self.blender, "Path to Blender binary is to be set in setUpClass()"
        assert self.testdir, "Path to tests binary is to be set in setUpClass()"

        blendfile = self.testdir / filepath if filepath else ""

        command = [
            self.blender,
            '--background',
            '--factory-startup',
            '--enable-autoexec',
            '--debug-memory',
            '--debug-exit-on-error',
        ]

        if blendfile:
            command.append(str(blendfile))

        command.extend([
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
