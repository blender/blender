
from sverchok.core.socket_conversions import ImplicitConversionProhibited
from sverchok.utils.testing import *
from sverchok.utils.logging import debug, info

class SocketConversionTests(EmptyTreeTestCase):
    
    def test_vertices_to_matrices(self):
        """
        Test that vertices -> matrices conversion work correctly.
        """
        ngon = create_node("SvNGonNode")
        ngon.sides_ = 4
        matrix_apply = create_node("MatrixApplyNode")

        # Connect NGon node to MatrixApply node
        self.tree.links.new(ngon.outputs['Vertices'], matrix_apply.inputs['Matrixes'])

        # Trigger processing of NGon node
        ngon.process()
        # Read what MatrixApply node sees
        data = matrix_apply.inputs['Matrixes'].sv_get()

        # It should see this list of matrices.
        expected_data = [
                [(1.0, 0.0, 0.0, 1.0),
                 (0.0, 1.0, 0.0, 0.0),
                 (0.0, 0.0, 1.0, 0),
                 (0.0, 0.0, 0.0, 1.0)],
                [(1.0, 0.0, 0.0, 0.0),
                 (0.0, 1.0, 0.0, 1.0),
                 (0.0, 0.0, 1.0, 0),
                 (0.0, 0.0, 0.0, 1.0)],
                [(1.0, 0.0, 0.0, -1.0),
                 (0.0, 1.0, 0.0, 0),
                 (0.0, 0.0, 1.0, 0),
                 (0.0, 0.0, 0.0, 1.0)],
                [(1.0, 0.0, 0.0, 0),
                 (0.0, 1.0, 0.0, -1.0),
                 (0.0, 0.0, 1.0, 0),
                 (0.0, 0.0, 0.0, 1.0)]
            ]

        self.assert_sverchok_data_equal(data, expected_data, precision=8)

    def test_no_edges_to_verts(self):
        """
        Test that edges -> vertices conversion raises an exception.
        """

        ngon = create_node("SvNGonNode")
        matrix_apply = create_node("MatrixApplyNode")

        # Connect NGon node to MatrixApply node
        self.tree.links.new(ngon.outputs['Edges'], matrix_apply.inputs['Vectors'])

        # Trigger processing of NGon node
        ngon.process()

        with self.assertRaises(ImplicitConversionProhibited):
            # Try to read from Vectors input of MatrixApply node
            # This should raise an exception
            data = matrix_apply.inputs['Vectors'].sv_get()
            error(data)

    def test_adaptive_sockets(self):
        """
        Test for nodes that allow arbitrary data at input.
        """

        tested_nodes = {
                'SvListDecomposeNode': ["data"],
                'ListJoinNode': ["data", "data 1"],
                'ListLevelsNode': ["data"],
                'ZipNode': ["data", "data 1"],
                'MaskListNode': ["data"],
                'ListFlipNode': ["data"],
                'ListItem2Node': ["Data"],
                'ListRepeaterNode': ["Data"],
                'ListReverseNode': ["data"],
                'ListSliceNode': ["Data"],
                'ShiftNodeMK2': ["data"],
                'ListShuffleNode': ['data'],
                'ListSortNodeMK2': ['data'],
                'SvListSplitNode': ['Data'],
                'ListFLNode': ['Data'],
                'Formula2Node': ["X", "n[0]"],
                'SvSetDataObjectNodeMK2': ["Objects"]
            }
        for bl_idname in tested_nodes.keys():
            with self.subTest(bl_idname = bl_idname):
                # Create NGon node and tested node
                ngon = create_node("SvNGonNode")
                node = create_node(bl_idname)
                # Link NGon node to tested inputs
                for input_name in tested_nodes[bl_idname]:
                    self.tree.links.new(ngon.outputs["Vertices"], node.inputs[input_name])
                # Trigger processing of the NGon node,
                # so that there will be some data at input
                # of tested node.
                ngon.process()
                try:
                    for input_name in tested_nodes[bl_idname]:
                        with self.subTest(input_name = input_name):
                            # Read the data from input.
                            # We do not actually care about the data
                            # itself, it is only important that there 
                            # was no exception.
                            data = node.inputs[input_name].sv_get()
                except ImplicitConversionProhibited as e:
                    raise e
                except Exception as e:
                    info(e)
                finally:
                    self.tree.nodes.remove(node)
                    self.tree.nodes.remove(ngon)

