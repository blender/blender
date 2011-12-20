import unittest
from test import support
from mathutils import Matrix, Vector


class MatrixTesting(unittest.TestCase):
    def test_matrix_column_access(self):
        #mat =
        #[ 1  2  3  4 ]
        #[ 1  2  3  4 ]
        #[ 1  2  3  4 ]
        mat = Matrix(((1, 11, 111),
                      (2, 22, 222),
                      (3, 33, 333),
                      (4, 44, 444)))

        self.assertEqual(mat[0], Vector((1, 11, 111)))
        self.assertEqual(mat[1], Vector((2, 22, 222)))
        self.assertEqual(mat[2], Vector((3, 33, 333)))
        self.assertEqual(mat[3], Vector((4, 44, 444)))

    def test_item_access(self):
        args = ((1, 4, 0, -1),
                (2, -1, 2, -2),
                (0, 3, 8, 3),
                (-2, 9, 1, 0))

        mat = Matrix(args)

        for i in range(4):
            for j in range(4):
                self.assertEqual(mat[i][j], args[i][j])

        self.assertEqual(mat[0][2], 0)
        self.assertEqual(mat[3][1], 9)
        self.assertEqual(mat[2][3], 3)
        self.assertEqual(mat[0][0], 1)
        self.assertEqual(mat[3][3], 0)

    def test_item_assignment(self):
        mat = Matrix() - Matrix()
        indices = (0, 0), (1, 3), (2, 0), (3, 2), (3, 1)
        checked_indices = []
        for col, row in indices:
            mat[col][row] = 1

        for col in range(4):
            for row in range(4):
                if mat[col][row]:
                    checked_indices.append((col, row))

        for item in checked_indices:
            self.assertIn(item, indices)

    def test_matrix_to_3x3(self):
        #mat =
        #[ 1  2  3  4  ]
        #[ 2  4  6  8  ]
        #[ 3  6  9  12 ]
        #[ 4  8  12 16 ]
        mat = Matrix(tuple((i, 2 * i, 3 * i, 4 * i) for i in range(1, 5)))
        mat_correct = Matrix(((1, 2, 3), (2, 4, 6), (3, 6, 9)))
        self.assertEqual(mat.to_3x3(), mat_correct)

    def test_matrix_to_translation(self):
        mat = Matrix()
        mat[3] = (1, 2, 3, 4)
        self.assertEqual(mat.to_translation(), Vector((1, 2, 3)))

    def test_matrix_inverse(self):
        mat = Matrix(((1, 4, 0, -1),
                      (2, -1, 2, -2),
                      (0, 3, 8, 3),
                      (-2, 9, 1, 0)))

        inv_mat = (1 / 285) * Matrix(((195, -57, 27, -102),
                                      (50, -19, 4, 6),
                                      (-60, 57, 18, 27),
                                      (110, -133, 43, -78)))

        self.assertEqual(mat.inverted(), inv_mat)

    def test_matrix_mult(self):
        mat = Matrix(((1, 4, 0, -1),
                      (2, -1, 2, -2),
                      (0, 3, 8, 3),
                      (-2, 9, 1, 0)))

        prod_mat = Matrix(((11, -9, 7, -9),
                           (4, -3, 12, 6),
                           (0, 48, 73, 18),
                           (16, -14, 26, -13)))

        self.assertEqual(mat * mat, prod_mat)


def test_main():
    try:
        support.run_unittest(MatrixTesting)
    except:
        import traceback
        traceback.print_exc()

        # alert CTest we failed
        import sys
        sys.exit(1)

if __name__ == '__main__':
    test_main()
