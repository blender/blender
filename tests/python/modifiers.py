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

import bpy
import os
import sys
from random import shuffle, seed
seed(0)

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import ModifierTest, ModifierSpec


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

        ############################################
        # One 'Generate' modifier on primitive meshes
        #############################################
        # 4
        ["testCubeArray", "expectedCubeArray", [ModifierSpec('array', 'ARRAY', {})]],
        ["testCylinderBuild", "expectedCylinderBuild", [ModifierSpec('build', 'BUILD', {'frame_start': 0, 'frame_duration': 1})]],

        # 6
        ["testConeDecimate", "expectedConeDecimate", [ModifierSpec('decimate', 'DECIMATE', {'ratio': 0.5})]],
        ["testCubeEdgeSplit", "expectedCubeEdgeSplit", [ModifierSpec('edge split', 'EDGE_SPLIT', {})]],
        ["testSphereMirror", "expectedSphereMirror", [ModifierSpec('mirror', 'MIRROR', {})]],
        ["testCylinderMask", "expectedCylinderMask", [ModifierSpec('mask', 'MASK', {'vertex_group': "mask_vertex_group"})]],
        ["testConeMultiRes", "expectedConeMultiRes", [ModifierSpec('multires', 'MULTIRES', {})]],

        # 11
        ["testCubeScrew", "expectedCubeScrew", [ModifierSpec('screw', 'SCREW', {})]],
        ["testCubeSolidify", "expectedCubeSolidify", [ModifierSpec('solidify', 'SOLIDIFY', {})]],
        ["testMonkeySubsurf", "expectedMonkeySubsurf", [ModifierSpec('subsurf', 'SUBSURF', {})]],
        ["testSphereTriangulate", "expectedSphereTriangulate", [ModifierSpec('triangulate', 'TRIANGULATE', {})]],
        ["testMonkeyWireframe", "expectedMonkeyWireframe", [ModifierSpec('wireframe', 'WIREFRAME', {})]],
        #ModifierSpec('skin', 'SKIN', {}), # skin is not reproducible .

        #############################################
        # One 'Deform' modifier on primitive meshes
        #############################################
        # 16
        ["testMonkeyArmature", "expectedMonkeyArmature",
         [ModifierSpec('armature', 'ARMATURE', {'object': bpy.data.objects['testArmature'], 'use_vertex_groups': True})]],
        ["testTorusCast", "expectedTorusCast", [ModifierSpec('cast', 'CAST', {'factor': 2.64})]],
        ["testCubeCurve", "expectedCubeCurve",
         [ModifierSpec('curve', 'CURVE', {'object': bpy.data.objects['testBezierCurve']})]],
        ["testMonkeyDisplace", "expectedMonkeyDisplace", [ModifierSpec('displace', "DISPLACE", {})]],

        # Hook modifier requires moving the hook object to get a mesh change, so can't test it with the current framework
        # ["testMonkeyHook", "expectedMonkeyHook",
        # [ModifierSpec('hook', 'HOOK', {'object': bpy.data.objects["EmptyHook"], 'vertex_group': "HookVertexGroup"})]],

        # 20
        #ModifierSpec('laplacian_deform', 'LAPLACIANDEFORM', {}) Laplacian requires a more complex mesh
        ["testCubeLattice", "expectedCubeLattice",
         [ModifierSpec('lattice', 'LATTICE', {'object': bpy.data.objects["testLattice"]})]],
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
