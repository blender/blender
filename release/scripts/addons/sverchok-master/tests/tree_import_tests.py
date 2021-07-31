
import unittest
import json

from sverchok.utils.testing import *
from sverchok.utils.sv_IO_panel_tools import import_tree

class ScriptUvImportTest(ReferenceTreeTestCase):

    reference_file_name = "script_uv_ref.blend.gz"

    def test_script_uv_import(self):
        with self.temporary_node_tree("ImportedTree") as new_tree:
            import_tree(new_tree, self.get_reference_file_path("script_uv.json"))

            # Check links
            self.assert_nodes_linked("ImportedTree", "Scripted Node Lite", "verts", "UV Connection", "vertices")
            self.assert_nodes_linked("ImportedTree", "UV Connection", "vertices", "Viewer Draw", "vertices")
            self.assert_nodes_linked("ImportedTree", "UV Connection", "data", "Viewer Draw", "edg_pol")

            # Check random node properties
            self.assert_node_property_equals("ImportedTree", "UV Connection", "cup_U", False)
            self.assert_node_property_equals("ImportedTree", "UV Connection", "polygons", 'Edges')
            self.assert_node_property_equals("ImportedTree", "UV Connection", "dir_check", 'U_dir')

class ProfileImportTest(ReferenceTreeTestCase):

    reference_file_name = "profile_ref.blend.gz"

    def test_profile_import(self):
        with self.temporary_node_tree("ImportedTree") as new_tree:
            import_tree(new_tree, self.get_reference_file_path("profile.json"))

class MeshExprImportTest(ReferenceTreeTestCase):

    reference_file_name = "mesh_expr_ref.blend.gz"

    def test_mesh_expr_import(self):
        with self.temporary_node_tree("ImportedTree") as new_tree:
            import_tree(new_tree, self.get_reference_file_path("mesh.json"))

class MonadImportTest(ReferenceTreeTestCase):

    reference_file_name = "monad_1_ref.blend.gz"

    def test_monad_import(self):
        with self.temporary_node_tree("ImportedTree") as new_tree:
            import_tree(new_tree, self.get_reference_file_path("monad_1.json"))
            self.assert_node_property_equals("ImportedTree", "Monad", "amplitude", 0.6199999451637268)

