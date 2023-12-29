# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b -noaudio --factory-startup --python tests/python/bl_animation_armature.py
"""

import unittest

import bpy


class BoneCollectionTest(unittest.TestCase):
    arm_ob: bpy.types.Object
    arm: bpy.types.Armature

    def setUp(self):
        bpy.ops.wm.read_homefile()
        self.arm_ob, self.arm = self.create_armature()

    def create_armature(self) -> tuple[bpy.types.Object, bpy.types.Armature]:
        arm = bpy.data.armatures.new('Armature')
        arm_ob = bpy.data.objects.new('ArmObject', arm)

        # Link to the scene just for giggles. And ease of debugging when things
        # go bad.
        bpy.context.scene.collection.objects.link(arm_ob)

        return arm_ob, arm

    def add_bones(self, arm_ob: bpy.types.Object) -> None:
        # Switch to edit mode to add some bones.

        bpy.context.view_layer.objects.active = arm_ob
        bpy.ops.object.mode_set(mode='EDIT')

        try:
            pass
        finally:
            bpy.ops.object.mode_set(mode='OBJECT')

    def test_bone_collection_api(self):
        # Just to keep the rest of the code shorter.
        bcolls = self.arm.collections

        self.assertEqual([], list(bcolls), "By default an Armature should have no collections")

        # Build a hierarchy.
        root1 = bcolls.new('root1')
        r1_child1 = bcolls.new('r1_child1', parent=root1)
        r1_child1_001 = bcolls.new('r1_child1', parent=root1)
        root2 = bcolls.new('root2')
        r2_child1 = bcolls.new('r2_child1', parent=root2)
        r2_child2 = bcolls.new('r2_child2', parent=root2)

        self.assertEqual('r1_child1.001', r1_child1_001.name, 'Names should be unique')

        # Check the hierarchy.
        self.assertEqual([root1, root2], list(bcolls), 'armature.collections should reflect only the roots')
        self.assertEqual([r1_child1, r1_child1_001], list(root1.children), 'root1.children should have its children')
        self.assertEqual([r2_child1, r2_child2], list(root2.children), 'root2.children should have its children')
        self.assertEqual([], list(r1_child1.children))

        # Check the array order.
        self.assertEqual([root1, root2, r1_child1, r1_child1_001, r2_child1, r2_child2], list(bcolls.all))

        # Move root2 to become the child of r1_child1.
        self.assertEqual(5, root2.move_to_parent(r1_child1))

        # Check the hierarchy.
        self.assertEqual([root1], list(bcolls), 'armature.collections should reflect only the roots')
        self.assertEqual([root2], list(r1_child1.children))
        self.assertEqual([r1_child1, r1_child1_001], list(root1.children), 'root1.children should have its children')
        self.assertEqual([r2_child1, r2_child2], list(root2.children), 'root2.children should have its children')

        # Check the array order.
        self.assertEqual([root1, r1_child1, r1_child1_001, r2_child1, r2_child2, root2], list(bcolls.all))

        # Move root2 between r1_child1 and r1_child1_001.
        self.assertEqual(2, root2.move_to_parent(root1, to_child_num=1))

        # Check the hierarchy.
        self.assertEqual([root1], list(bcolls), 'armature.collections should reflect only the roots')
        self.assertEqual([r1_child1, root2, r1_child1_001], list(
            root1.children), 'root1.children should have its children')
        self.assertEqual([r2_child1, r2_child2], list(root2.children), 'root2.children should have its children')

        # Check the array order.
        self.assertEqual([root1, r1_child1, root2, r1_child1_001, r2_child1, r2_child2], list(bcolls.all))

    def test_bone_collection_armature_join(self):
        other_arm_ob, other_arm = self.create_armature()

        # Build a hierarchy on the main armature.
        main_bcolls = self.arm.collections
        main_root = main_bcolls.new('root')
        main_child1 = main_bcolls.new('child1', parent=main_root)
        main_child2 = main_bcolls.new('child2', parent=main_root)

        # Build a hierarchy on the other armature.
        other_bcolls = other_arm.collections
        other_root = other_bcolls.new('root')
        other_child1 = other_bcolls.new('child1', parent=other_root)
        other_child3 = other_bcolls.new('child3', parent=other_root)

        # Create some custom properties on the collections.
        main_root['float'] = 0.2
        main_child1['string'] = 'main_string'
        main_child2['dict'] = {'agent': 47}

        other_root['float'] = 0.42
        other_child1['strange'] = 'other_string'
        other_child3['dict'] = {'agent': 327}

        # Join the two armatures together.
        self.assertEqual({'FINISHED'}, bpy.ops.object.select_all(action='DESELECT'))
        bpy.context.view_layer.objects.active = self.arm_ob
        self.arm_ob.select_set(True)
        other_arm_ob.select_set(True)
        self.assertEqual({'FINISHED'}, bpy.ops.object.join())

        # Check the custom properties.
        bcolls = self.arm.collections
        self.assertEqual(0.2, bcolls.all['root']['float'])
        self.assertEqual('main_string', bcolls.all['child1']['string'])
        self.assertEqual({'agent': 47}, bcolls.all['child2']['dict'].to_dict())
        self.assertNotIn(
            'strange',
            bcolls.all['child1'],
            'Bone collections that already existed in the active armature are not expected to be updated')
        self.assertEqual({'agent': 327}, bcolls.all['child3']['dict'].to_dict())


def main():
    import sys

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        # Avoid passing all of Blender's arguments to unittest.main()
        argv = [sys.argv[0]]

    unittest.main(argv=argv, exit=False)


if __name__ == "__main__":
    main()
