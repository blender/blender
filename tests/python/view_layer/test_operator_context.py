# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

from view_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(ViewLayerTesting):
    def test_operator_context(self):
        """
        See if view layer context is properly set/get with operators overrides
        when we set view_layer in context, the collection should change as well
        """
        import bpy
        import os

        class SampleOperator(bpy.types.Operator):
            bl_idname = "testing.sample"
            bl_label = "Sample Operator"

            view_layer = bpy.props.StringProperty(
                default="Not Set",
                options={'SKIP_SAVE'},
            )

            scene_collection = bpy.props.StringProperty(
                default="",
                options={'SKIP_SAVE'},
            )

            use_verbose = bpy.props.BoolProperty(
                default=False,
                options={'SKIP_SAVE'},
            )

            def execute(self, context):
                view_layer = context.view_layer
                ret = {'FINISHED'}

                # this is simply playing safe
                if view_layer.name != self.view_layer:
                    if self.use_verbose:
                        print('ERROR: Render Layer mismatch: "{0}" != "{1}"'.format(
                            view_layer.name, self.view_layer))
                    ret = {'CANCELLED'}

                scene_collection_name = None
                if self.scene_collection:
                    scene_collection_name = self.scene_collection
                else:
                    scene_collection_name = view_layer.collections.active.name

                # while this is the real test
                if context.scene_collection.name != scene_collection_name:
                    if self.use_verbose:
                        print('ERROR: Scene Collection mismatch: "{0}" != "{1}"'.format(
                            context.scene_collection.name, scene_collection_name))
                    ret = {'CANCELLED'}
                return ret

        bpy.utils.register_class(SampleOperator)

        # open sample file
        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
        self.rename_collections()

        # change the file
        three_b = bpy.data.objects.get('T.3b')
        three_c = bpy.data.objects.get('T.3c')
        scene = bpy.context.scene
        subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
        scorpion = subzero.collections.new('scorpion')
        subzero.objects.link(three_b)
        scorpion.objects.link(three_c)
        layer = scene.view_layers.new('Fresh new Layer')
        layer.collections.unlink(layer.collections.active)
        layer.collections.link(subzero)
        layer.collections.active_index = 3
        self.assertEqual(layer.collections.active.name, 'scorpion')

        # Change active scene layer (do it for window too just to don't get mangled in window bugs)
        scene = bpy.context.scene
        bpy.context.window.view_layer = bpy.context.scene.view_layers['Viewport']

        # old layer
        self.assertEqual(bpy.ops.testing.sample(view_layer='Viewport', use_verbose=True), {'FINISHED'})

        # expected to fail
        self.assertTrue(bpy.ops.testing.sample(view_layer=layer.name), {'CANCELLED'})

        # set view layer and scene collection
        override = bpy.context.copy()
        override["view_layer"] = layer
        override["scene_collection"] = subzero
        self.assertEqual(bpy.ops.testing.sample(
            override,
            view_layer=layer.name,
            scene_collection=subzero.name,  # 'sub-zero'
            use_verbose=True), {'FINISHED'})

        # set only view layer
        override = bpy.context.copy()
        override["view_layer"] = layer

        self.assertNotEqual(bpy.context.view_layer.name, layer.name)
        self.assertNotEqual(bpy.context.scene_collection.name, layer.collections.active.name)

        self.assertEqual(bpy.ops.testing.sample(
            override,
            view_layer=layer.name,
            scene_collection=layer.collections.active.name,  # 'scorpion'
            use_verbose=False), {'CANCELLED'})


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
