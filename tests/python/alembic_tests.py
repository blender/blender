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


class AbstractAlembicTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import re

        parser = argparse.ArgumentParser()
        parser.add_argument('--blender', required=True)
        parser.add_argument('--testdir', required=True)
        parser.add_argument('--alembic-root', required=True)
        args, _ = parser.parse_known_args()

        cls.blender = args.blender
        cls.testdir = pathlib.Path(args.testdir)
        cls.alembic_root = pathlib.Path(args.alembic_root)

        # 'abcls' outputs ANSI colour codes, even when stdout is not a terminal.
        # See https://github.com/alembic/alembic/issues/120
        cls.ansi_remove_re = re.compile(rb'\x1b[^m]*m')

        # 'abcls' array notation, like "name[16]"
        cls.abcls_array = re.compile(r'^(?P<name>[^\[]+)(\[(?P<arraysize>\d+)\])?$')

    def run_blender(self, filepath: str, python_script: str, timeout: int=300) -> str:
        """Runs Blender by opening a blendfile and executing a script.

        Returns Blender's stdout + stderr combined into one string.

        :param filepath: taken relative to self.testdir.
        :param timeout: in seconds
        """

        blendfile = self.testdir / filepath

        command = (
            self.blender,
            '--background',
            '-noaudio',
            '--factory-startup',
            '--enable-autoexec',
            str(blendfile),
            '-E', 'CYCLES',
            '--python-exit-code', '47',
            '--python-expr', python_script,
        )

        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=timeout)
        output = proc.stdout.decode('utf8')
        if proc.returncode:
            self.fail('Error %d running Blender:\n%s' % (proc.returncode, output))

        return output

    def abcprop(self, filepath: pathlib.Path, proppath: str) -> dict:
        """Uses abcls to obtain compound property values from an Alembic object.

        A dict of subproperties is returned, where the values are just strings
        as returned by abcls.

        The Python bindings for Alembic are old, and only compatible with Python 2.x,
        so that's why we can't use them here, and have to rely on other tooling.
        """

        abcls = self.alembic_root / 'bin' / 'abcls'

        command = (str(abcls), '-vl', '%s%s' % (filepath, proppath))
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=30)

        coloured_output = proc.stdout
        output = self.ansi_remove_re.sub(b'', coloured_output).decode('utf8')

        if proc.returncode:
            self.fail('Error %d running abcls:\n%s' % (proc.returncode, output))

        # Mapping from value type to callable that can convert a string to Python values.
        converters = {
            'bool_t': int,
            'uint8_t': int,
            'float64_t': float,
        }

        result = {}

        # Ideally we'd get abcls to output JSON, see https://github.com/alembic/alembic/issues/121
        lines = output.split('\n')
        for info, value in zip(lines[0::2], lines[1::2]):
            proptype, valtype_and_arrsize, name_and_extent = info.split()

            # Parse name and extent
            m = self.abcls_array.match(name_and_extent)
            if not m:
                self.fail('Unparsable name/extent from abcls: %s' % name_and_extent)
            name, extent = m.group('name'), m.group('arraysize')

            if extent != '1':
                self.fail('Unsupported extent %s for property %s/%s' % (extent, proppath, name))

            # Parse type and convert values
            m = self.abcls_array.match(valtype_and_arrsize)
            if not m:
                self.fail('Unparsable value type from abcls: %s' % valtype_and_arrsize)
            valtype, arraysize = m.group('name'), m.group('arraysize')

            try:
                conv = converters[valtype]
            except KeyError:
                self.fail('Unsupported type %s for property %s/%s' % (valtype, proppath, name))

            if arraysize is None:
                result[name] = conv(value)
            else:
                values = [conv(v.strip()) for v in value.split(',')]
                result[name] = values

        return result

    def assertAlmostEqualFloatArray(self, actual, expect, places=6, delta=None):
        """Asserts that the arrays of floats are almost equal."""

        self.assertEqual(len(actual), len(expect),
                         'Actual array has %d items, expected %d' % (len(actual), len(expect)))

        for idx, (act, exp) in enumerate(zip(actual, expect)):
            self.assertAlmostEqual(act, exp, places=places, delta=delta,
                                   msg='%f != %f at index %d' % (act, exp, idx))


class HierarchicalAndFlatExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_hierarchical_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'cubes_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=False)" % abc
        self.run_blender('cubes-hierarchy.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Cube/Cube_002/Cube_012/.xform')
        self.assertEqual(1, xform['.inherits'])
        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.0, 0.0, 0.0, 0.0,
             0.0, 1.0, 0.0, 0.0,
             0.0, 0.0, 1.0, 0.0,
             3.07484, -2.92265, 0.0586434, 1.0]
        )

    @with_tempdir
    def test_flat_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'cubes_flat.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=True)" % abc
        self.run_blender('cubes-hierarchy.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Cube_012/.xform')
        self.assertEqual(0, xform['.inherits'])

        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [0.343134, 0.485243, 0.804238, 0,
             0.0, 0.856222, -0.516608, 0,
             -0.939287, 0.177266, 0.293799, 0,
             1, 3, 4, 1],
        )


if __name__ == '__main__':
    unittest.main(argv=sys.argv[0:1])
