# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_mathutils.py -- --verbose
import unittest
from mathutils import Matrix, Vector
from mathutils import kdtree
import math

# keep globals immutable
vector_data = (
    (1.0, 0.0, 0.0),
    (0.0, 1.0, 0.0),
    (0.0, 0.0, 1.0),

    (1.0, 1.0, 1.0),

    (0.33783, 0.715698, -0.611206),
    (-0.944031, -0.326599, -0.045624),
    (-0.101074, -0.416443, -0.903503),
    (0.799286, 0.49411, -0.341949),
    (-0.854645, 0.518036, 0.033936),
    (0.42514, -0.437866, -0.792114),
    (-0.358948, 0.597046, 0.717377),
    (-0.985413,0.144714, 0.089294),
    )

# get data at different scales
vector_data = sum(
    (tuple(tuple(a * scale for a in v) for v in vector_data)
    for scale in (s * sign for s in (0.0001, 0.1, 1.0, 10.0, 1000.0, 100000.0)
                           for sign in (1.0, -1.0))), ()) + ((0.0, 0.0, 0.0),)


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

        self.assertEqual(mat1 * mat2, prod_mat1)
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

        self.assertRaises(ValueError, mat1.__mul__, vec)
        self.assertRaises(ValueError, vec.__mul__, mat1)

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


class VectorTesting(unittest.TestCase):

    def test_orthogonal(self):

        angle_90d = math.pi / 2.0
        for v in vector_data:
            v = Vector(v)
            if v.length_squared != 0.0:
                self.assertAlmostEqual(v.angle(v.orthogonal()), angle_90d)


class KDTreeTesting(unittest.TestCase):

    @staticmethod
    def kdtree_create_grid_3d(tot):
        k = kdtree.KDTree(tot * tot * tot)
        index = 0
        mul = 1.0 / (tot - 1)
        for x in range(tot):
            for y in range(tot):
                for z in range(tot):
                    k.insert((x * mul, y * mul, z * mul), index)
                    index += 1
        k.balance()
        return k

    def test_kdtree_single(self):
        co = (0,) * 3
        index = 2

        k = kdtree.KDTree(1)
        k.insert(co, index)
        k.balance()

        co_found, index_found, dist_found = k.find(co)

        self.assertEqual(tuple(co_found), co)
        self.assertEqual(index_found, index)
        self.assertEqual(dist_found, 0.0)

    def test_kdtree_empty(self):
        co = (0,) * 3

        k = kdtree.KDTree(0)
        k.balance()

        co_found, index_found, dist_found = k.find(co)

        self.assertIsNone(co_found)
        self.assertIsNone(index_found)
        self.assertIsNone(dist_found)

    def test_kdtree_line(self):
        tot = 10

        k = kdtree.KDTree(tot)

        for i in range(tot):
            k.insert((i,) * 3, i)

        k.balance()

        co_found, index_found, dist_found = k.find((-1,) * 3)
        self.assertEqual(tuple(co_found), (0,) * 3)

        co_found, index_found, dist_found = k.find((tot,) * 3)
        self.assertEqual(tuple(co_found), (tot - 1,) * 3)

    def test_kdtree_grid(self):
        size = 10
        k = self.kdtree_create_grid_3d(size)

        # find_range
        ret = k.find_range((0.5,) * 3, 2.0)
        self.assertEqual(len(ret), size * size * size)

        ret = k.find_range((1.0,) * 3, 1.0 / size)
        self.assertEqual(len(ret), 1)

        ret = k.find_range((1.0,) * 3, 2.0 / size)
        self.assertEqual(len(ret), 8)

        ret = k.find_range((10,) * 3, 0.5)
        self.assertEqual(len(ret), 0)

        # find_n
        tot = 0
        ret = k.find_n((1.0,) * 3, tot)
        self.assertEqual(len(ret), tot)

        tot = 10
        ret = k.find_n((1.0,) * 3, tot)
        self.assertEqual(len(ret), tot)
        self.assertEqual(ret[0][2], 0.0)

        tot = size * size * size
        ret = k.find_n((1.0,) * 3, tot)
        self.assertEqual(len(ret), tot)

    def test_kdtree_invalid_size(self):
        with self.assertRaises(ValueError):
            kdtree.KDTree(-1)

    def test_kdtree_invalid_balance(self):
        co = (0,) * 3
        index = 2

        k = kdtree.KDTree(2)
        k.insert(co, index)
        k.balance()
        k.insert(co, index)
        with self.assertRaises(RuntimeError):
            k.find(co)

if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
