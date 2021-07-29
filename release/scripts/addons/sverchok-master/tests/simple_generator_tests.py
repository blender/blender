
from sverchok.utils.testing import *
from sverchok.utils.logging import debug, info

# Some "smoke tests" for simple generator nodes.
# These test cases exist mostly in demonstration purposes,
# I hardly think anyone is going to break them "just that easy".
# Failure of these tests can also indicate that something
# is badly broken in general node processing mechanism.

class BoxNodeTest(NodeProcessTestCase):
    node_bl_idname = "SvBoxNode"

    def test_box(self):
        # It is not in general necessary to set properties of the node
        # to their default values.
        # However you may consider setting them, to exclude influence of
        # node_defaults mechanism.
        self.node.Divx = 1
        self.node.Divy = 1
        self.node.Divz = 1

        # This one is necessary
        self.node.process()

        # You may want to inspect what the node outputs
        # data = self.get_output_data("Vers")

        expected_verts = ([[0.5, 0.5, -0.5], [0.5, -0.5, -0.5], [-0.5, -0.5, -0.5], [-0.5, 0.5, -0.5], [0.5, 0.5, 0.5], [0.5, -0.5, 0.5], [-0.5, -0.5, 0.5], [-0.5, 0.5, 0.5]],)

        self.assert_output_data_equals("Vers", expected_verts)

class NGonNodeTest(NodeProcessTestCase):
    node_bl_idname = "SvNGonNode"

    # The NGon node does not do anything in 
    # it's process() method if outputs are not 
    # linked to anything. 
    # So we ask base test case class to connect
    # something (actually a Note node) to the 
    # Vertices output.
    connect_output_sockets = ["Vertices"]

    def test_ngon(self):
        # self.node.rad_ = 2.0
        self.node.process()
        #data = self.get_output_data("Vertices")
        #info("NGon: %s", data)

        expected_data = [[(1.0, 0.0, 0), (0.30901699437494745, 0.9510565162951535, 0), (-0.8090169943749473, 0.5877852522924732, 0), (-0.8090169943749476, -0.587785252292473, 0), (0.30901699437494723, -0.9510565162951536, 0)]]
        self.assert_output_data_equals("Vertices", expected_data)

