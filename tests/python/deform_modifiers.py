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

# To run the test type: blender -b /path/to/the/blend/file --python path/to/this/py/file -- --run-all-tests -- --verbose
# Type the above line in cmd/terminal, for example, look below
# blender -b c:\blender-lib\deform_modifiers.blend --python c:\deform_modifiers.py -- --run-all-tests -- --verbose


import os
import sys
import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import MeshTest, ModifierSpec, OperatorSpecObjectMode, DeformModifierSpec, RunTest

tests = [

    # Surface Deform Test, finally can bind to the Target object.
    # Actual deformation occurs by animating imitating user input.

    MeshTest("SurfaceDeform", "testObjMonkeySurfaceDeform", "expObjMonkeySurfaceDeform",
             [DeformModifierSpec(10, [
                 ModifierSpec('surface_deform', 'SURFACE_DEFORM', {'target': bpy.data.objects["Cube"]})],
                                 OperatorSpecObjectMode('surfacedeform_bind', {'modifier': 'surface_deform'}))]),

    # Mesh Deform Test, finally can bind to the Target object.
    # Actual deformation occurs by animating imitating user input.

    MeshTest("MeshDeform", "testObjMonkeyMeshDeform", "expObjMonkeyMeshDeform",
             [DeformModifierSpec(10, [ModifierSpec('mesh_deform', 'MESH_DEFORM',
                                                   {'object': bpy.data.objects["MeshCube"], 'precision': 2})],
                                 OperatorSpecObjectMode('meshdeform_bind', {'modifier': 'mesh_deform'}))]),

    # Surface Deform Test, finally can bind to the Target object.
    # Actual deformation occurs by animating imitating user input.

    MeshTest("Hook", "testObjHookPlane", "expObjHookPlane",
             [DeformModifierSpec(10, [ModifierSpec('hook', 'HOOK',
                                                   {'object': bpy.data.objects["Empty"], 'falloff_radius': 1,
                                                    'vertex_group': 'Group'})])]),

    # Laplacian Deform Test, first a hook is attached.

    MeshTest("Laplace", "testObjCubeLaplacian", "expObjCubeLaplacian",
             [DeformModifierSpec(10,
                                 [ModifierSpec('hook2', 'HOOK', {'object': bpy.data.objects["Empty.001"],
                                                                 'vertex_group': 'hook_vg'}),
                                  ModifierSpec('laplace', 'LAPLACIANDEFORM', {'vertex_group': 'laplace_vg'})],
                                 OperatorSpecObjectMode('laplaciandeform_bind', {'modifier': 'laplace'}))]),

    MeshTest("WarpPlane", "testObjPlaneWarp", "expObjPlaneWarp",
             [DeformModifierSpec(10, [ModifierSpec('warp', 'WARP',
                                                   {'object_from': bpy.data.objects["From"],
                                                    'object_to': bpy.data.objects["To"],
                                                    })])]),

    #############################################
    # Curves Deform Modifiers
    #############################################
    MeshTest("CurveArmature", "testObjBezierCurveArmature", "expObjBezierCurveArmature",
             [DeformModifierSpec(10, [ModifierSpec('curve_armature', 'ARMATURE',
                                                   {'object': bpy.data.objects['testArmatureHelper'],
                                                    'use_vertex_groups': False, 'use_bone_envelopes': True})])]),

    MeshTest("CurveLattice", "testObjBezierCurveLattice", "expObjBezierCurveLattice",
             [DeformModifierSpec(10, [ModifierSpec('curve_lattice', 'LATTICE',
                                                   {'object': bpy.data.objects['testLatticeCurve']})])]),

    # HOOK for Curves can't be tested with current framework, as it requires going to Edit Mode to select vertices,
    # here is no equivalent of a vertex group in Curves.
    # Dummy test for Hook, can also be called corner case
    MeshTest("CurveHook", "testObjBezierCurveHook", "expObjBezierCurveHook",
             [DeformModifierSpec(10,
                                 [ModifierSpec('curve_Hook', 'HOOK', {'object': bpy.data.objects['EmptyCurve']})])]),

    MeshTest("MeshDeformCurve", "testObjCurveMeshDeform", "expObjCurveMeshDeform",
             [DeformModifierSpec(10, [
                 ModifierSpec('mesh_deform_curve', 'MESH_DEFORM', {'object': bpy.data.objects["Cylinder"],
                                                                   'precision': 2})],
                                 OperatorSpecObjectMode('meshdeform_bind', {'modifier': 'mesh_deform_curve'}))]),

    MeshTest("WarpCurve", "testObjBezierCurveWarp", "expObjBezierCurveWarp",
             [DeformModifierSpec(10, [ModifierSpec('warp_curve', 'WARP',
                                                   {'object_from': bpy.data.objects["From_curve"],
                                                    'object_to': bpy.data.objects["To_curve"]})])]),

]

deform_tests = RunTest(tests)
command = list(sys.argv)
for i, cmd in enumerate(command):
    if cmd == "--run-all-tests":
        deform_tests.apply_modifiers = True
        deform_tests.do_compare = True
        deform_tests.run_all_tests()
        break
    elif cmd == "--run-test":
        deform_tests.apply_modifiers = False
        deform_tests.do_compare = False
        name = command[i + 1]
        deform_tests.run_test(name)
        break
