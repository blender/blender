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

"""
blender -b -noaudio --factory-startup --python tests/python/bl_animation_fcurves.py -- --testdir /path/to/lib/tests/animation
"""

import pathlib
import sys
import unittest

import bpy

class FCurveEvaluationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
      
    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)

    def test_fcurve_versioning_291(self):
      # See D8752.
      bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "fcurve-versioning-291.blend"))
      cube = bpy.data.objects['Cube']
      fcurve = cube.animation_data.action.fcurves.find('location', index=0)

      self.assertAlmostEqual(0.0, fcurve.evaluate(1))
      self.assertAlmostEqual(0.019638698548078537, fcurve.evaluate(2))
      self.assertAlmostEqual(0.0878235399723053, fcurve.evaluate(3))
      self.assertAlmostEqual(0.21927043795585632, fcurve.evaluate(4))
      self.assertAlmostEqual(0.41515052318573, fcurve.evaluate(5))
      self.assertAlmostEqual(0.6332430243492126, fcurve.evaluate(6))
      self.assertAlmostEqual(0.8106040954589844, fcurve.evaluate(7))
      self.assertAlmostEqual(0.924369215965271, fcurve.evaluate(8))
      self.assertAlmostEqual(0.9830065965652466, fcurve.evaluate(9))
      self.assertAlmostEqual(1.0, fcurve.evaluate(10))

    def test_fcurve_extreme_handles(self):
      # See D8752.
      bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "fcurve-extreme-handles.blend"))
      cube = bpy.data.objects['Cube']
      fcurve = cube.animation_data.action.fcurves.find('location', index=0)

      self.assertAlmostEqual(0.0, fcurve.evaluate(1))
      self.assertAlmostEqual(0.004713400732725859, fcurve.evaluate(2))
      self.assertAlmostEqual(0.022335870191454887, fcurve.evaluate(3))
      self.assertAlmostEqual(0.06331237405538559, fcurve.evaluate(4))
      self.assertAlmostEqual(0.16721539199352264, fcurve.evaluate(5))
      self.assertAlmostEqual(0.8327845335006714, fcurve.evaluate(6))
      self.assertAlmostEqual(0.9366875886917114, fcurve.evaluate(7))
      self.assertAlmostEqual(0.9776642322540283, fcurve.evaluate(8))
      self.assertAlmostEqual(0.9952865839004517, fcurve.evaluate(9))
      self.assertAlmostEqual(1.0, fcurve.evaluate(10))


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
