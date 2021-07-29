
import numpy as np 
from sverchok.utils.testing import *
from sverchok.utils.logging import debug, info
from sverchok.utils.geom import CubicSpline

class CubicSplineTests(SverchokTestCase):
    def setUp(self):
        super().setUp()
        vertices = [(-1, -1, 0), (0, 0, 0), (1, 2, 0), (2, 3, 0)]
        self.spline = CubicSpline(vertices, metric="DISTANCE")

    def test_eval(self):
        t_in = np.array([0.0, 0.1, 0.4, 0.5, 0.7, 1.0])
        result = self.spline.eval(t_in)
        #info(result)
        expected_result = np.array(
                [[-1.0,        -1.0,         0.0 ],
                 [-0.60984526, -0.66497986,  0.0 ],
                 [ 0.29660356,  0.5303721,   0.0 ],
                 [ 0.5,         1.0,         0.0 ],
                 [ 0.94256655,  1.91347161,  0.0 ],
                 [ 2.0,         3.0,         0.0 ]])
        self.assert_numpy_arrays_equal(result, expected_result, precision=8)

    def test_tangent(self):
        t_in = np.array([0.0, 0.1, 0.4, 0.5, 0.7, 1.0])
        result = self.spline.tangent(t_in)
        #info(result)
        expected_result = np.array(
                [[ 0.00789736,  0.00663246,  0.0 ],
                 [ 0.00761454,  0.0068363,   0.0 ],
                 [ 0.00430643,  0.00922065,  0.0 ],
                 [ 0.0039487,   0.0094785,   0.0 ],
                 [ 0.00537964,  0.00844713,  0.0 ],
                 [ 0.00789736,  0.00663246,  0.0 ]])
        self.assert_numpy_arrays_equal(result, expected_result, precision=8)

