# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

from render_layer_common import *
import unittest
import os
import sys

sys.path.append(os.path.dirname(__file__))


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def do_scene_copy(self, filepath_json_reference, copy_mode, data_callbacks):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            (self.path_exists(f) for f in (
                filepath_layers,
                filepath_json_reference,
                ))

            filepath_saved = os.path.join(dirpath, '{0}.blend'.format(copy_mode))
            filepath_json = os.path.join(dirpath, "{0}.json".format(copy_mode))

            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()
            bpy.ops.scene.new(type=copy_mode)
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

            datas = query_scene(filepath_saved, 'Main.001', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            with open(filepath_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_json,
                filepath_json_reference,
                ),
                "Scene copy \"{0}\" test failed".format(copy_mode.title()))

    def test_scene_collections_copy_full(self):
        """
        See if scene copying 'FULL_COPY' is working for scene collections
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_full_simple.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'FULL_COPY',
                (get_scene_collections,))

    def test_scene_collections_link(self):
        """
        See if scene copying 'LINK_OBJECTS' is working for scene collections
        """
        import os
        ROOT = self.get_root()

        # note: nothing should change, so using `layers_simple.json`
        filepath_layers_json_copy = os.path.join(ROOT, 'layers_simple.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'LINK_OBJECTS',
                (get_scene_collections,))

    def test_scene_layers_copy(self):
        """
        See if scene copying 'FULL_COPY' is working for scene layers
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_full.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'FULL_COPY',
                (get_scene_collections, get_layers))

    def test_scene_layers_link(self):
        """
        See if scene copying 'FULL_COPY' is working for scene layers
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_link.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'LINK_OBJECTS',
                (get_scene_collections, get_layers))


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
