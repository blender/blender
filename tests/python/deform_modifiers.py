# SPDX-FileCopyrightText: 2020-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run the test type: blender -b /path/to/the/blend/file --python path/to/this/py/file -- --run-all-tests -- --verbose
# Type the above line in cmd/terminal, for example, look below
# blender -b c:\blender-lib\deform_modifiers.blend --python c:\deform_modifiers.py -- --run-all-tests -- --verbose


import os
import sys
import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, ModifierSpec, OperatorSpecObjectMode, DeformModifierSpec, RunTest

tests = [

    # Surface Deform Test, finally can bind to the Target object.
    # Actual deformation occurs by animating imitating user input.

    SpecMeshTest("SurfaceDeform", "testObjMonkeySurfaceDeform", "expObjMonkeySurfaceDeform",
                 [DeformModifierSpec(10, [
                     ModifierSpec('surface_deform', 'SURFACE_DEFORM', {'target': bpy.data.objects["Cube"]})],
                     OperatorSpecObjectMode('surfacedeform_bind', {'modifier': 'surface_deform'}))]),

    # Mesh Deform Test, finally can bind to the Target object.
    # Actual deformation occurs by animating imitating user input.

    SpecMeshTest("MeshDeform", "testObjMonkeyMeshDeform", "expObjMonkeyMeshDeform",
                 [DeformModifierSpec(10, [ModifierSpec('mesh_deform', 'MESH_DEFORM',
                                                       {'object': bpy.data.objects["MeshCube"], 'precision': 2})],
                                     OperatorSpecObjectMode('meshdeform_bind', {'modifier': 'mesh_deform'}))]),

    # Surface Deform Test, finally can bind to the Target object.
    # Actual deformation occurs by animating imitating user input.

    SpecMeshTest("Hook", "testObjHookPlane", "expObjHookPlane",
                 [DeformModifierSpec(10, [ModifierSpec('hook', 'HOOK',
                                                       {'object': bpy.data.objects["Empty"], 'falloff_radius': 1,
                                                        'vertex_group': 'Group'})])]),

    # Laplacian Deform Test, first a hook is attached.

    SpecMeshTest("Laplace", "testObjCubeLaplacian", "expObjCubeLaplacian",
                 [DeformModifierSpec(10,
                                     [ModifierSpec('hook2', 'HOOK', {'object': bpy.data.objects["Empty.001"],
                                                                     'vertex_group': 'hook_vg'}),
                                      ModifierSpec('laplace', 'LAPLACIANDEFORM', {'vertex_group': 'laplace_vg'})],
                                     OperatorSpecObjectMode('laplaciandeform_bind', {'modifier': 'laplace'}))]),

    SpecMeshTest("WarpPlane", "testObjPlaneWarp", "expObjPlaneWarp",
                 [DeformModifierSpec(10, [ModifierSpec('warp', 'WARP',
                                                       {'object_from': bpy.data.objects["From"],
                                                        'object_to': bpy.data.objects["To"],
                                                        })])]),

    #############################################
    # Curves Deform Modifiers
    #############################################
    SpecMeshTest("CurveArmature", "testObjBezierCurveArmature", "expObjBezierCurveArmature",
                 [DeformModifierSpec(10, [ModifierSpec('curve_armature', 'ARMATURE',
                                                       {'object': bpy.data.objects['testArmatureHelper'],
                                                        'use_vertex_groups': False, 'use_bone_envelopes': True})])]),

    SpecMeshTest("CurveLattice", "testObjBezierCurveLattice", "expObjBezierCurveLattice",
                 [DeformModifierSpec(10, [ModifierSpec('curve_lattice', 'LATTICE',
                                                       {'object': bpy.data.objects['testLatticeCurve']})])]),

    # HOOK for Curves can't be tested with current framework, as it requires going to Edit Mode to select vertices,
    # here is no equivalent of a vertex group in Curves.
    # Dummy test for Hook, can also be called corner case
    SpecMeshTest("CurveHook", "testObjBezierCurveHook", "expObjBezierCurveHook",
                 [DeformModifierSpec(10,
                                     [ModifierSpec('curve_Hook', 'HOOK', {'object': bpy.data.objects['EmptyCurve']})])]),

    SpecMeshTest("MeshDeformCurve", "testObjCurveMeshDeform", "expObjCurveMeshDeform",
                 [DeformModifierSpec(10, [
                     ModifierSpec('mesh_deform_curve', 'MESH_DEFORM', {'object': bpy.data.objects["Cylinder"],
                                                                       'precision': 2})],
                     OperatorSpecObjectMode('meshdeform_bind', {'modifier': 'mesh_deform_curve'}))]),

    SpecMeshTest("WarpCurve", "testObjBezierCurveWarp", "expObjBezierCurveWarp",
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
