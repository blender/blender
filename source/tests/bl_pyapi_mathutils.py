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

        for row in range(4):
            for col in range(4):
                self.assertEqual(mat[row][col], args[row][col])

        self.assertEqual(mat[0][2], 0)
        self.assertEqual(mat[3][1], 9)
        self.assertEqual(mat[2][3], 3)
        self.assertEqual(mat[0][0], 1)
        self.assertEqual(mat[3][3], 0)

    def test_item_assignment(self):
        mat = Matrix() - Matrix()
        indices = (0, 0), (1, 3), (2, 0), (3, 2), (3, 1)
        checked_indices = []
        for row, col in indices:
            mat[row][col] = 1

        for row in range(4):
            for col in range(4):
                if mat[row][col]:
                    checked_indices.append((row, col))

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
        mat[0][3] = 1
        mat[1][3] = 2
        mat[2][3] = 3
        self.assertEqual(mat.to_translation(), Vector((1, 2, 3)))

    def test_matrix_translation(self):
        mat = Matrix()
        mat.translation = Vector((1, 2, 3))
        self.assertEqual(mat[0][3], 1)
        self.assertEqual(mat[1][3], 2)
        self.assertEqual(mat[2][3], 3)

    def test_non_square_mult(self):
        mat1 = Matrix(((1, 2, 3),
                       (4, 5, 6)))
        mat2 = Matrix(((1, 2),
                       (3, 4),
                       (5, 6)))

        prod_mat1 = Matrix(((22, 28),
                            (49, 64)))
        prod_mat2 = Matrix(((9, 12, 15),
                            (19, 26, 33),
                            (29, 40, 51)))

        self.assertEqual(mat1*mat2, prod_mat1)
        self.assertEqual(mat2 * mat1, prod_mat2)

    def test_mat4x4_vec3D_mult(self):
        mat = Matrix(((1, 0, 2, 0),
                      (0, 6, 0, 0),
                      (0, 0, 1, 1),
                      (0, 0, 0, 1)))

        vec = Vector((1, 2, 3))
        
        prod_mat_vec = Vector((7, 12, 4))
        prod_vec_mat = Vector((1, 12, 5))

        self.assertEqual(mat * vec, prod_mat_vec)
        self.assertEqual(vec * mat, prod_vec_mat)

    def test_mat_vec_mult(self):
        mat1 = Matrix()

        vec = Vector((1, 2))

        self.assertRaises(TypeError, mat1.__mul__, vec)
        self.assertRaises(ValueError, vec.__mul__, mat1)  # Why are these different?!

        mat2 = Matrix(((1, 2),
                       (-2, 3)))

        prod = Vector((5, 4))

        self.assertEqual(mat2 * vec, prod)

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
