# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

# Fit to match default projective camera with focal_length 50 and sensor_width 36.
default_fisheye_polynomial = [
    -1.1735143712967577e-05,
    -0.019988736953434998,
    -3.3525322965709175e-06,
    3.099275275886036e-06,
    -2.6064646454854524e-08,
]


# Utilities to generate lens polynomials to match built-in camera types, only here
# for reference at the moment, not used by the code.
def create_grid(sensor_height, sensor_width):
    import numpy as np
    if sensor_height is None:
        sensor_height = sensor_width / (16 / 9)  # Default aspect ration 16:9
    uu, vv = np.meshgrid(np.linspace(0, 1, 100), np.linspace(0, 1, 100))
    uu = (uu - 0.5) * sensor_width
    vv = (vv - 0.5) * sensor_height
    rr = np.sqrt(uu ** 2 + vv ** 2)
    return rr


def fisheye_lens_polynomial_from_projective(focal_length=50, sensor_width=36, sensor_height=None):
    import numpy as np
    rr = create_grid(sensor_height, sensor_width)
    polynomial = np.polyfit(rr.flat, (-np.arctan(rr / focal_length)).flat, 4)
    return list(reversed(polynomial))


def fisheye_lens_polynomial_from_projective_fov(fov, sensor_width=36, sensor_height=None):
    import numpy as np
    f = sensor_width / 2 / np.tan(fov / 2)
    return fisheye_lens_polynomial_from_projective(f, sensor_width, sensor_height)


def fisheye_lens_polynomial_from_equisolid(lens=10.5, sensor_width=36, sensor_height=None):
    import numpy as np
    rr = create_grid(sensor_height, sensor_width)
    x = rr.reshape(-1)
    x = np.stack([x**i for i in [1, 2, 3, 4]])
    y = (-2 * np.arcsin(rr / (2 * lens))).reshape(-1)
    polynomial = np.linalg.lstsq(x.T, y.T, rcond=None)[0]
    return [0] + list(polynomial)


def fisheye_lens_polynomial_from_equidistant(fov=180, sensor_width=36, sensor_height=None):
    import numpy as np
    return [0, -np.radians(fov) / sensor_width, 0, 0, 0]


def fisheye_lens_polynomial_from_distorted_projective_polynomial(
        k1, k2, k3, focal_length=50, sensor_width=36, sensor_height=None,
):
    import numpy as np
    rr = create_grid(sensor_height, sensor_width)
    r2 = (rr / focal_length) ** 2
    r4 = r2 * r2
    r6 = r4 * r2
    r_coeff = 1 + k1 * r2 + k2 * r4 + k3 * r6
    polynomial = np.polyfit(rr.flat, (-np.arctan(rr / focal_length * r_coeff)).flat, 4)
    return list(reversed(polynomial))


def fisheye_lens_polynomial_from_distorted_projective_divisions(
        k1, k2, focal_length=50, sensor_width=36, sensor_height=None,
):
    import numpy as np
    rr = create_grid(sensor_height, sensor_width)
    r2 = (rr / focal_length) ** 2
    r4 = r2 * r2
    r_coeff = 1 + k1 * r2 + k2 * r4
    polynomial = np.polyfit(rr.flat, (-np.arctan(rr / focal_length / r_coeff)).flat, 4)
    return list(reversed(polynomial))
