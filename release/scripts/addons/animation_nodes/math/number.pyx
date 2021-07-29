cimport cython

PI_HALF = PI / 2.0
PI_QUARTER = PI / 4.0

cdef double add(double x, double y):
    return x + y

cdef double subtract(double x, double y):
    return x - y

cdef double multiply(double x, double y):
    return x * y

@cython.cdivision(True)
cdef double divide_Save(double x, double y):
    return x / y if y != 0 else 0

cdef double floorDivision_Save(double x, double y):
    if y != 0: return x // y
    else: return 0

cdef double modulo_Save(double x, double y):
    # cannot use cdivision(True) because it changes the modulo behavior
    # http://stackoverflow.com/a/3883019/4755171
    if y != 0: return x % y
    return 0

cdef double asin_Save(double x):
    if x < -1: return -PI_HALF
    if x > 1: return PI_HALF
    return asin(x)

cdef double acos_Save(double x):
    if x < -1: return PI
    if x > 1: return 0
    return acos(x)

cdef double power_Save(double base, double exponent):
    if base >= 0 or exponent % 1 == 0:
        return pow(base, exponent)
    return 0

cdef double min(double x, double y):
    if x < y: return x
    else: return y

cdef double max(double x, double y):
    if x > y: return x
    else: return y

cdef double abs(double x):
    if x < 0: return -x
    else: return x

cdef double sqrt_Save(double x):
    if x >= 0: return sqrt(x)
    else: return 0

cdef double invert(double x):
    return -x

cdef double reciprocal_Save(double x):
    if x != 0: return 1 / x
    else: return 0

cdef double snap_Save(double x, double step):
    if step != 0: return ceil(x / step - 0.5) * step
    else: return x

cdef double copySign(double x, double y):
    if y >= 0: return x if x > 0 else -x
    else: return x if x < 0 else -x

cdef double logarithm_Save(double a, double base):
    if a <= 0: return 0
    elif base <= 0 or base == 1: return log(a)
    else: return log(a) / log(base)

cdef double clamp(double x, double minValue, double maxValue):
    if x < minValue: return minValue
    if x > maxValue: return maxValue
    return x
