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

import math
import os
import sys
from random import shuffle, seed

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import ModifierTest, ModifierSpec

seed(0)


def get_generate_modifiers_list(test_object_name, randomize=False):
    """
    Construct a list of 'Generate' modifiers with default parameters.
    :param test_object_name: str - name of test object. Some modifiers like boolean need an extra parameter beside
                                    the default one. E.g. boolean needs object, mask needs vertex group etc...
                                    The extra parameter name will be <test_object_name>_<modifier_type>
    :param randomize: bool - if True shuffle the list of modifiers.
    :return: list of 'Generate' modifiers with default parameters.
    """

    boolean_test_object = bpy.data.objects[test_object_name + "_boolean"]

    generate_modifiers = [
        ModifierSpec('array', 'ARRAY', {}),
        ModifierSpec('bevel', 'BEVEL', {'width': 0.1}),
        ModifierSpec('boolean', 'BOOLEAN', {'object': boolean_test_object}),
        ModifierSpec('build', 'BUILD', {'frame_start': 0, 'frame_duration': 1}),
        ModifierSpec('decimate', 'DECIMATE', {}),
        ModifierSpec('edge split', 'EDGE_SPLIT', {}),

        # mask can effectively delete the mesh since the vertex group need to be updated after each
        # applied modifier. Needs to be tested separately.
        # ModifierSpec('mask', 'MASK', {'vertex_group': mask_vertex_group}, False),

        ModifierSpec('mirror', 'MIRROR', {}),
        ModifierSpec('multires', 'MULTIRES', {}),

        # remesh can also generate an empty mesh. Skip.
        # ModifierSpec('remesh', 'REMESH', {}),

        # ModifierSpec('screw', 'SCREW', {}), # screw can make the test very slow. Skipping for now.
        # ModifierSpec('skin', 'SKIN', {}), # skin is not reproducible .

        ModifierSpec('solidify', 'SOLIDIFY', {}),
        ModifierSpec('subsurf', 'SUBSURF', {}),
        ModifierSpec('triangulate', 'TRIANGULATE', {}),
        ModifierSpec('wireframe', 'WIREFRAME', {})

    ]

    if randomize:
        shuffle(generate_modifiers)

    return generate_modifiers


def main():

    mask_first_list = get_generate_modifiers_list("testCubeMaskFirst", randomize=True)
    mask_vertex_group = "testCubeMaskFirst" + "_mask"
    mask_first_list.insert(0, ModifierSpec('mask', 'MASK', {'vertex_group': mask_vertex_group}))

    tests = [
        ###############################
        # List of 'Generate' modifiers on a cube
        ###############################
        # 0
        # ["testCube", "expectedCube", get_generate_modifiers_list("testCube")],
        ["testCubeRandom", "expectedCubeRandom", get_generate_modifiers_list("testCubeRandom", randomize=True)],
        ["testCubeMaskFirst", "expectedCubeMaskFirst", mask_first_list],

        ["testCollapseDecimate", "expectedCollapseDecimate",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2}),
          ModifierSpec('decimate', 'DECIMATE', {'decimate_type': 'COLLAPSE', 'ratio': 0.25, 'use_collapse_triangulate': True})]],
        ["testPlanarDecimate", "expectedPlanarDecimate",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2}),
          ModifierSpec('decimate', 'DECIMATE', {'decimate_type': 'DISSOLVE', 'angle_limit': math.radians(30)})]],
        ["testUnsubdivideDecimate", "expectedUnsubdivideDecimate",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2}),
          ModifierSpec('decimate', 'DECIMATE', {'decimate_type': 'UNSUBDIV', 'iterations': 2})]],

        # 5
        ["testRadialBisectMirror", "expectedRadialBisectMirror",
         [ModifierSpec('mirror1', 'MIRROR', {'use_bisect_axis': (True, False, False)}),
          ModifierSpec('mirror2', 'MIRROR', {'use_bisect_axis': (True, False, False), 'mirror_object': bpy.data.objects["testRadialBisectMirrorHelper"]}),
          ModifierSpec('mirror3', 'MIRROR', {'use_axis': (False, True, False), 'use_bisect_axis': (False, True, False), 'use_bisect_flip_axis': (False, True, False), 'mirror_object': bpy.data.objects["testRadialBisectMirrorHelper"]})]],
        ["regressT58411Mirror", "expectedT58411Mirror",
         [ModifierSpec('mirror', 'MIRROR', {}),
          ModifierSpec('bevel', 'BEVEL', {'segments': 2, 'limit_method': 'WEIGHT'}),
          ModifierSpec('subd', 'SUBSURF', {'levels': 1})]],

        ["testBasicScrew", "expectedBasicScrew",
         [ModifierSpec('mirror', 'MIRROR', {'mirror_object': bpy.data.objects["testBasicScrewHelper"]}),
          ModifierSpec("screw", 'SCREW', {'angle': math.radians(400), 'steps': 20, 'iterations': 2, 'screw_offset': 2, 'use_normal_calculate': True})]],
        ["testObjectScrew", "expectedObjectScrew",
         [ModifierSpec('mirror', 'MIRROR', {'mirror_object': bpy.data.objects["testObjectScrewHelper2"]}),
          ModifierSpec("screw", 'SCREW', {"angle": math.radians(600), 'steps': 32, 'iterations': 1, 'use_object_screw_offset': True, 'use_normal_calculate': True, 'object': bpy.data.objects["testObjectScrewHelper1"]})]],

        # 9
        ["testMergedScrewWeld", "expectedMergedScrewWeld",
         [ModifierSpec("screw", 'SCREW', {'angle': math.radians(360), 'steps': 12, 'iterations': 1, 'screw_offset': 1, 'use_normal_calculate': True, 'use_merge_vertices': True}),
          ModifierSpec("weld", 'WELD', {"merge_threshold": 0.001})]],
        ["regressT72380Weld", "expectedT72380Weld",
         [ModifierSpec('vedit', 'VERTEX_WEIGHT_EDIT', {'vertex_group': 'Group', 'use_remove': True, 'remove_threshold': 1}),
          ModifierSpec("weld", 'WELD', {"merge_threshold": 0.2, "vertex_group": "Group"})]],
        ["regressT72792Weld", "expectedT72792Weld",
         [ModifierSpec('array', 'ARRAY', {'fit_type': 'FIXED_COUNT', 'count': 2}),
          ModifierSpec("weld", 'WELD', {"merge_threshold": 0.1, "vertex_group": "Group"})]],

        ############################################
        # One 'Generate' modifier on primitive meshes
        #############################################
        # 12
        ["testCubeArray", "expectedCubeArray", [ModifierSpec('array', 'ARRAY', {})]],
        ["testCapArray", "expectedCapArray",
         [ModifierSpec('array', 'ARRAY', {'fit_type': 'FIT_LENGTH', 'fit_length': 2.0, 'start_cap': bpy.data.objects["testCapStart"], 'end_cap': bpy.data.objects["testCapEnd"]})]],
        ["testCurveArray", "expectedCurveArray",
         [ModifierSpec('array', 'ARRAY', {'fit_type': 'FIT_CURVE', 'curve': bpy.data.objects["testCurveArrayHelper"], 'use_relative_offset': False, 'use_constant_offset': True, 'constant_offset_displace': (0.5, 0, 0)})]],
        ["testRadialArray", "expectedRadialArray",
         [ModifierSpec('array', 'ARRAY', {'fit_type': 'FIXED_COUNT', 'count': 3, 'use_merge_vertices': True, 'use_merge_vertices_cap': True, 'use_relative_offset': False, 'use_object_offset': True, 'offset_object': bpy.data.objects["testRadialArrayHelper"]})]],

        ["testCylinderBuild", "expectedCylinderBuild", [ModifierSpec('build', 'BUILD', {'frame_start': 0, 'frame_duration': 1})]],

        # 17
        ["testConeDecimate", "expectedConeDecimate", [ModifierSpec('decimate', 'DECIMATE', {'ratio': 0.5})]],
        ["testCubeEdgeSplit", "expectedCubeEdgeSplit", [ModifierSpec('edge split', 'EDGE_SPLIT', {})]],

        ["testSphereMirror", "expectedSphereMirror", [ModifierSpec('mirror', 'MIRROR', {})]],
        ["testLocalMirror", "expectedLocalMirror",
         [ModifierSpec('mirror', 'MIRROR', {'use_clip': True})]],
        ["testObjectOffsetMirror", "expectedObjectOffsetMirror",
         [ModifierSpec('mirror', 'MIRROR', {'mirror_object': bpy.data.objects["testObjectOffsetMirrorHelper"]})]],

        ["testCylinderMask", "expectedCylinderMask", [ModifierSpec('mask', 'MASK', {'vertex_group': "mask_vertex_group"})]],
        ["testConeMultiRes", "expectedConeMultiRes", [ModifierSpec('multires', 'MULTIRES', {})]],

        # 24
        ["testCubeScrew", "expectedCubeScrew", [ModifierSpec('screw', 'SCREW', {})]],

        ["testCubeSolidify", "expectedCubeSolidify", [ModifierSpec('solidify', 'SOLIDIFY', {})]],
        ["testComplexSolidify", "expectedComplexSolidify",
         [ModifierSpec('solidify', 'SOLIDIFY', {'solidify_mode': 'NON_MANIFOLD', 'thickness': 0.05, 'offset': 0, 'nonmanifold_thickness_mode': 'CONSTRAINTS'})]],
        ["regressT63063Solidify", "expectedT63063Solidify",
         [ModifierSpec('solid', 'SOLIDIFY', {'thickness': 0.1, 'offset': 0.7})]],
        ["regressT61979Solidify", "expectedT61979Solidify",
         [ModifierSpec('solid', 'SOLIDIFY', {'thickness': -0.25, 'use_even_offset': True, 'use_quality_normals': True})]],

        ["testMonkeySubsurf", "expectedMonkeySubsurf", [ModifierSpec('subsurf', 'SUBSURF', {})]],
        ["testCatmullClarkSubdivisionSurface", "expectedCatmullClarkSubdivisionSurface",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2})]],
        ["testSimpleSubdivisionSurface", "expectedSimpleSubdivisionSurface",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2, 'subdivision_type': 'SIMPLE'})]],
        ["testCrease2dSubdivisionSurface", "expectedCrease2dSubdivisionSurface",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2})]],
        ["testCrease3dSubdivisionSurface", "expectedCrease3dSubdivisionSurface",
         [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2})]],

        # 34
        ["testSphereTriangulate", "expectedSphereTriangulate", [ModifierSpec('triangulate', 'TRIANGULATE', {})]],
        ["testMonkeyWireframe", "expectedMonkeyWireframe", [ModifierSpec('wireframe', 'WIREFRAME', {})]],
        #ModifierSpec('skin', 'SKIN', {}), # skin is not reproducible .

        ["testMergedWeld", "expectedMergedWeld",
         [ModifierSpec("weld", 'WELD', {"merge_threshold": 0.021})]],
        ["testMergedAllWeld", "expectedMergedAllWeld",
         [ModifierSpec("weld", 'WELD', {"merge_threshold": 1.1})]],
        ["testMergedNoneWeld", "expectedMergedNoneWeld",
         [ModifierSpec("weld", 'WELD', {"merge_threshold": 0.019})]],

        #############################################
        # One 'Deform' modifier on primitive meshes
        #############################################
        # 39
        ["testMonkeyArmature", "expectedMonkeyArmature",
         [ModifierSpec('armature', 'ARMATURE', {'object': bpy.data.objects['testArmature'], 'use_vertex_groups': True})]],
        ["testTorusCast", "expectedTorusCast", [ModifierSpec('cast', 'CAST', {'factor': 2.64})]],
        ["testCubeCurve", "expectedCubeCurve",
         [ModifierSpec('curve', 'CURVE', {'object': bpy.data.objects['testBezierCurve']})]],
        ["testMonkeyDisplace", "expectedMonkeyDisplace", [ModifierSpec('displace', "DISPLACE", {})]],

        # Hook modifier requires moving the hook object to get a mesh change, so can't test it with the current framework
        # ["testMonkeyHook", "expectedMonkeyHook",
        # [ModifierSpec('hook', 'HOOK', {'object': bpy.data.objects["EmptyHook"], 'vertex_group': "HookVertexGroup"})]],

        # 43
        #ModifierSpec('laplacian_deform', 'LAPLACIANDEFORM', {}) Laplacian requires a more complex mesh
        ["testCubeLattice", "expectedCubeLattice",
         [ModifierSpec('lattice', 'LATTICE', {'object': bpy.data.objects["testLattice"]})]],

        # ModifierSpec('laplacian_deform', 'LAPLACIANDEFORM', {}) Laplacian requires a more complex mesh

        # Mesh Deform Modifier requires user input, so skip.

        # mesh_test = MeshTest("testMonkeyDeform", "expectedMonkeyDeform",[
        #        ModifierSpec('mesh_deform', 'MESH_DEFORM', {'object': bpy.data.objects["testDeformStructure"]}),
        #        OperatorSpec('meshdeform_bind',{'modifier':'MeshDeform'},'FACE',{i for in range(500)})
        # ] ,True)

        ["testPlaneShrinkWrap", "expectedPlaneShrinkWrap",
         [ModifierSpec('shrinkwrap', 'SHRINKWRAP', {'target': bpy.data.objects["testCubeWrap"], 'offset': 0.5})]],

        ["testCylinderSimpleDeform", "expectedCylinderSimpleDeform",
         [ModifierSpec('simple_deform', 'SIMPLE_DEFORM', {'angle': math.radians(180), 'deform_axis': 'Z'})]],

        ["testPlaneSmooth", "expectedPlaneSmooth",
         [ModifierSpec('smooth', 'SMOOTH', {'iterations': 11})]],

        # Smooth corrective requires a complex mesh.

        ["testBalloonLaplacianSmooth", "expectedBalloonLaplacianSmooth",
         [ModifierSpec('laplaciansmooth', 'LAPLACIANSMOOTH', {'lambda_factor': 12, 'lambda_border': 12})]],

        # Surface Deform and Warp requires user input, so skip.

        # Wave - requires complex mesh, so skip.

    ]

    modifiers_test = ModifierTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            modifiers_test.apply_modifiers = True
            modifiers_test.run_all_tests()
            break
        elif cmd == "--run-test":
            modifiers_test.apply_modifiers = False
            index = int(command[i + 1])
            modifiers_test.run_test(index)
            break


if __name__ == "__main__":
    try:
        main()
    except:
        import traceback
        traceback.print_exc()
        sys.exit(1)
