# SPDX-License-Identifier: GPL-2.0-or-later
# This code tests bug reported in #50703

import numpy

a = numpy.array([[3, 2, 0], [3, 1, 0]], dtype=numpy.int32)
a[0]
