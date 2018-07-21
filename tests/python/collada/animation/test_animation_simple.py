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
#
# Call as follows:
# python collada_mesh_simple.py --blender PATH_TO_BLENDER_EXE --testdir PATH_TO_SVN/lib/tests/collada/mesh
#
import sys
import bpy
import argparse
import functools
import shutil
import tempfile
import unittest
import difflib
import pathlib
from pathlib import Path


def with_tempdir(wrapped):
    """Creates a temporary directory for the function, cleaning up after it returns normally.

    When the wrapped function raises an exception, the contents of the temporary directory
    remain available for manual inspection.

    The wrapped function is called with an extra positional argument containing
    the pathlib.Path() of the temporary directory.
    """

    @functools.wraps(wrapped)
    def decorator(*args, **kwargs):
        dirname = tempfile.mkdtemp(prefix='blender-collada-test')
        #print("Using tempdir %s" % dirname)
        try:
            retval = wrapped(*args, pathlib.Path(dirname), **kwargs)
        except:
            print('Exception in %s, not cleaning up temporary directory %s' % (wrapped, dirname))
            raise
        else:
            shutil.rmtree(dirname)
        return retval

    return decorator


LINE = "+----------------------------------------------------------------"


class AbstractColladaTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.testdir = pathlib.Path(args.testdir)

    def checkdae(self, reference, export):
        """
        collada verifier checks if exported dae file is the same as reference dae
        """

        ref = open(reference)
        exp = open(export)
        diff = difflib.unified_diff(ref.readlines(), exp.readlines(), lineterm='', n=0)
        ref.close()
        exp.close()

        diff_count = 0
        for line in diff:
            error = True
            for prefix in ('---', '+++', '@@'):
                # Ignore diff metadata
                if line.startswith(prefix):
                    error = False
                    break
            else:
                # Ignore time stamps
                for ignore in ('<created>', '<modified>', '<authoring_tool>'):
                    if line[1:].strip().startswith(ignore):
                        error = False
                        break
            if error:
                diff_count += 1
                pline = line.strip()
                if diff_count == 1:
                    print("\n%s" % LINE)
                    print("|Test has errors:")
                    print(LINE)
                pre = "reference" if pline[0] == "-" else "generated"
                print("| %s:%s" % (pre, pline[1:]))

        if diff_count > 0:
            print(LINE)
            print("ref :%s" % reference)
            print("test:%s" % export)
            print("%s\n" % LINE)

        return diff_count == 0


class MeshExportTest4(AbstractColladaTest):
    @with_tempdir
    def test_export_animation_suzannes_sample_matrix(self, tempdir: pathlib.Path):
        test = "suzannes_parent_inverse_sample_10_matrix"
        reference_dae = self.testdir / Path("%s.dae" % test)
        outfile = tempdir / Path("%s_out.dae" % test)

        bpy.ops.wm.collada_export(
            filepath="%s" % str(outfile),
            check_existing=True,
            filemode=8,
            display_type='DEFAULT',
            sort_method='FILE_SORT_ALPHA',
            apply_modifiers=True,
            export_mesh_type=0,
            export_mesh_type_selection='view',
            selected=True,
            include_children=True,
            include_armatures=True,
            include_shapekeys=False,
            deform_bones_only=False,
            include_animations=True,
            sample_animations=True,
            sampling_rate=10,
            active_uv_only=False,
            use_texture_copies=True,
            triangulate=False,
            use_object_instantiation=True,
            use_blender_profile=True,
            sort_by_name=False,
            export_transformation_type=0,
            export_transformation_type_selection='matrix',
            export_texture_type=0,
            export_texture_type_selection='mat',
            open_sim=False,
            limit_precision=True,
            keep_bind_info=False,
        )

        # Now check the resulting Collada file.
        if not self.checkdae(reference_dae, outfile):
            self.fail()


class MeshExportTest3(AbstractColladaTest):
    @with_tempdir
    def test_export_animation_suzannes_sample_locrotscale(self, tempdir: pathlib.Path):
        test = "suzannes_parent_inverse_sample_10_channels"
        reference_dae = self.testdir / Path("%s.dae" % test)
        outfile = tempdir / Path("%s_out.dae" % test)

        bpy.ops.wm.collada_export(
            filepath="%s" % str(outfile),
            check_existing=True,
            filemode=8,
            display_type='DEFAULT',
            sort_method='FILE_SORT_ALPHA',
            apply_modifiers=True,
            export_mesh_type=0,
            export_mesh_type_selection='view',
            selected=True,
            include_children=True,
            include_armatures=True,
            include_shapekeys=False,
            deform_bones_only=False,
            include_animations=True,
            sample_animations=True,
            sampling_rate=10,
            active_uv_only=False,
            use_texture_copies=True,
            triangulate=False,
            use_object_instantiation=True,
            use_blender_profile=True,
            sort_by_name=False,
            export_transformation_type=0,
            export_transformation_type_selection='transrotloc',
            export_texture_type=0,
            export_texture_type_selection='mat',
            open_sim=False,
            limit_precision=True,
            keep_bind_info=False,
        )

        # Now check the resulting Collada file.
        if not self.checkdae(reference_dae, outfile):
            self.fail()


class MeshExportTest2(AbstractColladaTest):
    @with_tempdir
    def test_export_animation_suzannes_keyframe_matrix(self, tempdir: pathlib.Path):
        test = "suzannes_parent_inverse_keyframes_matrix"
        reference_dae = self.testdir / Path("%s.dae" % test)
        outfile = tempdir / Path("%s_out.dae" % test)

        bpy.ops.wm.collada_export(
            filepath="%s" % str(outfile),
            check_existing=True,
            filemode=8,
            display_type='DEFAULT',
            sort_method='FILE_SORT_ALPHA',
            apply_modifiers=True,
            export_mesh_type=0,
            export_mesh_type_selection='view',
            selected=True,
            include_children=True,
            include_armatures=True,
            include_shapekeys=False,
            deform_bones_only=False,
            include_animations=True,
            sample_animations=False,
            sampling_rate=1,
            active_uv_only=False,
            use_texture_copies=True,
            triangulate=False,
            use_object_instantiation=True,
            use_blender_profile=True,
            sort_by_name=False,
            export_transformation_type=0,
            export_transformation_type_selection='matrix',
            export_texture_type=0,
            export_texture_type_selection='mat',
            open_sim=False,
            limit_precision=True,
            keep_bind_info=False,
        )

        # Now check the resulting Collada file.
        if not self.checkdae(reference_dae, outfile):
            self.fail()


class MeshExportTest1(AbstractColladaTest):
    @with_tempdir
    def test_export_animation_suzannes_keyframe_locrotscale(self, tempdir: pathlib.Path):
        test = "suzannes_parent_inverse_keyframes_channels"
        reference_dae = self.testdir / Path("%s.dae" % test)
        outfile = tempdir / Path("%s_out.dae" % test)

        bpy.ops.wm.collada_export(
            filepath="%s" % str(outfile),
            check_existing=True,
            filemode=8,
            display_type='DEFAULT',
            sort_method='FILE_SORT_ALPHA',
            apply_modifiers=True,
            export_mesh_type=0,
            export_mesh_type_selection='view',
            selected=True,
            include_children=True,
            include_armatures=True,
            include_shapekeys=False,
            deform_bones_only=False,
            include_animations=True,
            sample_animations=False,
            sampling_rate=1,
            active_uv_only=False,
            use_texture_copies=True,
            triangulate=False,
            use_object_instantiation=True,
            use_blender_profile=True,
            sort_by_name=False,
            export_transformation_type=0,
            export_transformation_type_selection='transrotloc',
            export_texture_type=0,
            export_texture_type_selection='mat',
            open_sim=False,
            limit_precision=True,
            keep_bind_info=False,
        )

        # Now check the resulting Collada file.
        if not self.checkdae(reference_dae, outfile):
            self.fail()


if __name__ == '__main__':
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True)
    args, remaining = parser.parse_known_args()
    unittest.main(argv=sys.argv[0:1] + remaining)
