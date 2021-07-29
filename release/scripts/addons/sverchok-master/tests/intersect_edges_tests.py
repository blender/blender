
import unittest
from sverchok.core.socket_data import SvSetSocket, get_output_socket_data
from sverchok.utils.testing import *
from sverchok.utils.logging import debug, info

class IntersectEdgesTest2(ReferenceTreeTestCase):
    # There are 2 3x3 planes intersecting
    reference_file_name = "intersecting_planes.blend.gz"

    def test_intersect_edges(self):
        self.tree.process()

        node = self.tree.nodes["Intersect Edges MK2"]

        result_verts = get_output_socket_data(node, "Verts_out")
        result_edges = get_output_socket_data(node, "Edges_out")
        #info("Result: %s", result_verts)

        self.assert_sverchok_data_equals_file(result_verts, "intersecting_planes_result_verts.txt", precision=8)
        self.assert_sverchok_data_equals_file(result_edges, "intersecting_planes_result_faces.txt", precision=8)

