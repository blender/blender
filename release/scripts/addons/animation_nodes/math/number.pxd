from libc.math cimport M_PI as PI
from libc.math cimport (sin, cos, tan, asin, acos, atan, atan2, hypot,
                        pow, floor, ceil, sqrt, log)

cdef double PI_HALF, PI_QUARTER

cdef double add(double x, double y)
cdef double subtract(double x, double y)
cdef double multiply(double x, double y)
cdef double divide_Save(double x, double y)
cdef double modulo_Save(double x, double y)
cdef double floorDivision_Save(double x, double y)

cdef double asin_Save(double x)
cdef double acos_Save(double x)

cdef double power_Save(double base, double exponent)

cdef double min(double x, double y)
cdef double max(double x, double y)
cdef double abs(double x)

cdef double sqrt_Save(double x)
cdef double invert(double x)
cdef double reciprocal_Save(double x)

cdef double snap_Save(double x, double step)
cdef double copySign(double x, double y)

cdef double logarithm_Save(double a, double base)

cdef double clamp(double x, double minValue, double maxValue)
