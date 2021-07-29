
import json

from sverchok.utils.testing import *
from sverchok.utils.sv_IO_panel_tools import import_tree

class ImportSingleSimpleNode(EmptyTreeTestCase):

    def test_box_import(self):
        node = create_node("SvBoxNode", self.tree.name)
        node.Divx = 1
        node.Divy = 3
        node.Divz = 4
        node.Size = 1.0299999713897705

        self.assert_node_equals_file(node, "Box", "box.json")

    def test_cylinder_import(self):
        node = create_node("CylinderNode", self.tree.name)
        node.Separate = 1
        node.cap_ = 0
        node.radTop_ = 1.0299999713897705
        node.radBot_ = 1.0299999713897705
        node.vert_ = 33
        node.height_ = 2.0299999713897705
        node.subd_ = 1

        self.assert_node_equals_file(node, "Cylinder", "cylinder.json")

    def test_torus_import(self):
        node = create_node("SvTorusNode", self.tree.name)
        node.mode = "MAJOR_MINOR"
        node.Separate = 0
        node.torus_eR = 1.2799999713897705
        node.torus_R = 1.0299999713897705
        node.torus_r = 0.25
        node.torus_iR = 0.7799999713897705
        node.torus_n1 = 33
        node.torus_n2 = 17
        node.torus_rP = 0.029999999329447746
        node.torus_sP = 0.029999999329447746
        node.torus_sT = 1

        self.assert_node_equals_file(node, "Torus", "torus.json")

