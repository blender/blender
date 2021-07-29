cimport cython
from libc.math cimport M_PI as PI
from ... utils.clamp cimport clamp
from libc.math cimport pow, sqrt, sin, cos
from ... utils.lists cimport findListSegment_LowLevel
from ... data_structures cimport Interpolation, DoubleList

'''
Here is a good source for different interpolation functions in Java:
https://github.com/libgdx/libgdx/blob/master/gdx/src/com/badlogic/gdx/math/Interpolation.java
'''

# Linear
#####################################################

cdef class Linear(Interpolation):
    def __cinit__(self):
        self.clamped = True

    cdef double evaluate(self, double x):
        return x


# Power
#####################################################

ctypedef double (*PowFunction)(double base, double exponent)

cdef double pow1(double x, double _):
    return x

cdef double pow2(double x, double _):
    return x * x

cdef double pow3(double x, double _):
    return x * x * x

cdef double pow4(double x, double _):
    cdef double y = x * x
    return y * y

cdef double pow5(double x, double _):
    cdef double y = x * x
    return y * y * x

cdef PowFunction getPowFunction(int exponent):
    if exponent == 1: return pow1
    elif exponent == 2: return pow2
    elif exponent == 3: return pow3
    elif exponent == 4: return pow4
    elif exponent == 5: return pow5
    else: return pow

cdef class PowerIn(Interpolation):
    cdef int exponent
    cdef PowFunction pow

    def __cinit__(self, int exponent):
        self.exponent = max(exponent, 1)
        self.pow = getPowFunction(self.exponent)
        self.clamped = True

    cdef double evaluate(self, double x):
        return self.pow(x, self.exponent)

cdef class PowerOut(Interpolation):
    cdef int exponent
    cdef int factor
    cdef PowFunction pow

    def __cinit__(self, int exponent):
        self.exponent = max(exponent, 1)
        self.factor = -1 if self.exponent % 2 == 0 else 1
        self.pow = getPowFunction(self.exponent)
        self.clamped = True

    cdef double evaluate(self, double x):
        return self.pow(x - 1, self.exponent) * self.factor + 1

cdef class PowerInOut(Interpolation):
    cdef int exponent
    cdef double factor
    cdef PowFunction pow

    def __cinit__(self, int exponent):
        self.exponent = max(exponent, 1)
        self.factor = -0.5 if self.exponent % 2 == 0 else 0.5
        self.pow = getPowFunction(self.exponent)
        self.clamped = True

    cdef double evaluate(self, double x):
        if x <= 0.5:
            return self.pow(x * 2, self.exponent) / 2
        else:
            return self.pow((x - 1) * 2, self.exponent) * self.factor + 1


# Exponential
#####################################################

cdef class ExponentialInterpolationBase(Interpolation):
    cdef:
        int exponent
        double base
        double minValue
        double scale

    def __cinit__(self, double base, int exponent):
        self.exponent = min(max(0, exponent), 70)
        self.base = max(0.0001, base) if base != 1 else 1.0001
        self.minValue = pow(self.base, -self.exponent)
        self.scale = 1 / (1 - self.minValue)
        self.clamped = True

cdef class ExponentialIn(ExponentialInterpolationBase):
    cdef double evaluate(self, double x):
        return (pow(self.base, self.exponent * (x - 1)) - self.minValue) * self.scale

cdef class ExponentialOut(ExponentialInterpolationBase):
    cdef double evaluate(self, double x):
        return 1 - (pow(self.base, -self.exponent * x) - self.minValue) * self.scale

cdef class ExponentialInOut(ExponentialInterpolationBase):
    cdef double evaluate(self, double x):
        if x <= 0.5:
            return (pow(self.base, self.exponent * (x * 2 - 1)) - self.minValue) * self.scale / 2
        else:
            return (2 - (pow(self.base, -self.exponent * (x * 2 - 1)) - self.minValue) * self.scale) / 2


# Circular
#####################################################

cdef class CircularInterpolationBase(Interpolation):
    def __cinit__(self):
        self.clamped = True

cdef class CircularIn(CircularInterpolationBase):
    cdef double evaluate(self, double x):
        return 1 - sqrt(1 - x * x)

cdef class CircularOut(CircularInterpolationBase):
    cdef double evaluate(self, double x):
        x -= 1
        return sqrt(1 - x * x)

cdef class CircularInOut(CircularInterpolationBase):
    cdef double evaluate(self, double x):
        if x <= 0.5:
            x *= 2
            return (1 - sqrt(1 - x * x)) / 2
        else:
            x = (x - 1) * 2
            return (sqrt(1 - x * x) + 1) / 2

# Elastic
#####################################################

cdef class ElasticInterpolationBase(Interpolation):
    cdef:
        int factor
        double bounceFactor, base, exponent

    def __cinit__(self, int bounces, double base, double exponent):
        bounces = max(0, bounces)
        self.factor = -1 if bounces % 2 == 0 else 1
        self.base = max(0, base)
        self.bounceFactor = -(bounces + 0.5) * PI
        self.exponent = exponent

cdef class ElasticIn(ElasticInterpolationBase):
    cdef double evaluate(self, double x):
        return pow(self.base, self.exponent * (x - 1)) * sin(x * self.bounceFactor) * self.factor

cdef class ElasticOut(ElasticInterpolationBase):
    cdef double evaluate(self, double x):
        x = 1 - x
        return 1 - pow(self.base, self.exponent * (x - 1)) * sin(x * self.bounceFactor) * self.factor

cdef class ElasticInOut(ElasticInterpolationBase):
    cdef double evaluate(self, double x):
        if x <= 0.5:
            x *= 2
            return pow(self.base, self.exponent * (x - 1)) * sin(x * self.bounceFactor) * self.factor / 2
        else:
            x = (1 - x) * 2
            return 1 - pow(self.base, self.exponent * (x - 1)) * sin(x * self.bounceFactor) * self.factor / 2


# Bounce
#####################################################

cdef class BounceInterpolationBase(Interpolation):
    cdef:
        DoubleList widths, heights

    def __cinit__(self, int bounces, double base):
        cdef int amount = max(1, bounces + 1)

        cdef:
            double a = 2.0 ** (amount - 1.0)
            double b = 2.0 ** amount - 2.0 ** (amount - 2.0) - 1.0
            double c = a / b

        self.widths = DoubleList(length = amount)
        self.heights = DoubleList(length = amount)
        cdef int i
        for i in range(amount):
            self.widths.data[i] = c / 2.0 ** i
            self.heights.data[i] = self.widths.data[i] * base
        self.heights.data[0] = 1
        self.clamped = True

    cdef double bounceOut(self, double x):
        x += self.widths[0] / 2
        cdef int i
        cdef double width = 0, height = 0
        for i in range(self.widths.length):
            width = self.widths[i]
            if x <= width:
                x /= width
                height = self.heights[i]
                break
            x -= width
        cdef double z = 4 / width * height * x
        return 1 - (z - z * x) * width

cdef class BounceIn(BounceInterpolationBase):
    cdef double evaluate(self, double x):
        return 1 - self.bounceOut(1 - x)

cdef class BounceOut(BounceInterpolationBase):
    cdef double evaluate(self, double x):
        return self.bounceOut(x)

cdef class BounceInOut(BounceInterpolationBase):
    cdef double evaluate(self, double x):
        if x <= 0.5:
            return (1 - self.bounceOut(1 - x * 2)) / 2
        else:
            return self.bounceOut(x * 2 - 1) / 2 + 0.5


# Back
#####################################################

cdef class BackInterpolationBase(Interpolation):
    cdef double scale

    def __cinit__(self, double scale):
        self.scale = scale

cdef class BackIn(BackInterpolationBase):
    cdef double evaluate(self, double x):
        return x * x * ((self.scale + 1) * x - self.scale)

cdef class BackOut(BackInterpolationBase):
    cdef double evaluate(self, double x):
        x -= 1
        return x * x * ((self.scale + 1) * x + self.scale) + 1

cdef class BackInOut(BackInterpolationBase):
    cdef double evaluate(self, double x):
        if x <= 0.5:
            x *= 2
            return x * x * ((self.scale + 1) * x - self.scale) / 2
        else:
            x = (x - 1) * 2
            return x * x * ((self.scale + 1) * x + self.scale) / 2 + 1


# Sine
#####################################################

cdef class SinInterpolationBase(Interpolation):
    def __cinit__(self):
        self.clamped = True

cdef class SinIn(SinInterpolationBase):
    cdef double evaluate(self, double x):
        return 1.0 - cos(x * PI / 2.0)

cdef class SinOut(SinInterpolationBase):
    cdef double evaluate(self, double x):
        return sin(x * PI / 2.0)

cdef class SinInOut(SinInterpolationBase):
    cdef double evaluate(self, double x):
        return (1.0 - cos(x * PI)) / 2.0


# Specials
#####################################################

cdef class MixedInterpolation(Interpolation):
    cdef:
        double factor
        Interpolation a, b

    def __cinit__(self, double factor, Interpolation a not None, Interpolation b not None):
        self.factor = factor
        self.a = a
        self.b = b
        self.clamped = a.clamped and b.clamped and 0 <= factor <= 1

    cdef double evaluate(self, double x):
        return self.a.evaluate(x) * (1 - self.factor) + self.b.evaluate(x) * self.factor


cdef class ChainedInterpolation(Interpolation):
    cdef:
        Interpolation a, b
        double position, endA, startB, fadeWidth
        double fadeWidthHalf, fadeStart, fadeEnd

    def __cinit__(self, Interpolation a not None, Interpolation b not None,
                        double position, double endA, double startB, double fadeWidth):
        self.a = a
        self.b = b
        self.position = clamp(position, 0.0001, 0.9999)
        self.endA = clamp(endA, 0, 1)
        self.startB = clamp(startB, 0, 1)

        self.fadeWidth = clamp(fadeWidth, 0.00001, 2 * min(self.position, 1 - self.position) - 0.0001)
        self.fadeWidthHalf = self.fadeWidth / 2.0
        self.fadeStart = self.position - self.fadeWidthHalf
        self.fadeEnd = self.position + self.fadeWidthHalf

        self.clamped = a.clamped and b.clamped

    cdef double evaluate(self, double x):
        if x < self.fadeStart:
            return self.a.evaluate(self.mapA(x))
        elif x > self.fadeEnd:
            return self.b.evaluate(self.mapB(x))
        else:
            return self.calcFade(x)

    cdef double calcFade(self, double x):
        cdef double valueA = self.a.evaluate(self.mapA(self.fadeStart))
        cdef double valueB = self.b.evaluate(self.mapB(self.fadeEnd))

        cdef double factor = (x - self.fadeStart) / self.fadeWidth
        return valueA * (1 - factor) + valueB * factor

    cdef double mapA(self, double x):
        return x / self.fadeStart * self.endA

    cdef double mapB(self, double x):
        cdef double y = (self.startB - 1) / (self.fadeEnd - 1)
        return y * x + 1 - y


cdef class PyInterpolation(Interpolation):
    cdef object function

    def __cinit__(self, object function not None):
        if not hasattr(function, "__call__"):
            raise TypeError("object is not callable")
        self.function = function

    cdef double evaluate(self, double x):
        return self.function(x)


cdef class CachedInterpolation(Interpolation):
    cdef:
        Interpolation original
        readonly DoubleList cache
        int resolution

    def __cinit__(self, Interpolation original not None, int resolution = 100):
        self.original = original
        self.resolution = max(2, resolution)
        self.updateCache()

    @cython.cdivision(True)
    cdef updateCache(self):
        self.cache = DoubleList(length = self.resolution)
        cdef int i
        for i in range(self.resolution):
            self.cache.data[i] = self.original.evaluate(i / <double>(self.resolution - 1))

    cdef double evaluate(self, double x):
        cdef long index[2]
        cdef float factor
        findListSegment_LowLevel(self.resolution, False, x, index, &factor)
        return self.cache.data[index[0]] * (1 - factor) + self.cache.data[index[1]] * factor


cdef class FCurveMapping(Interpolation):
    cdef:
        object fCurve
        double xMove, xFactor, yMove, yFactor

    def __cinit__(self, object fCurve, double xMove, double xFactor, double yMove, double yFactor):
        from bpy.types import FCurve
        if not isinstance(fCurve, FCurve):
            raise TypeError("Expected FCurve")
        self.fCurve = fCurve
        self.xMove = xMove
        self.xFactor = xFactor
        self.yMove = yMove
        self.yFactor = yFactor

    cdef double evaluate(self, double x):
        x = x * self.xFactor + self.xMove
        return (self.fCurve.evaluate(x) + self.yMove) * self.yFactor


cdef class MirroredAndChainedInterpolation(Interpolation):
    cdef Interpolation interpolation

    def __cinit__(self, Interpolation interpolation not None):
        self.interpolation = interpolation

    cdef double evaluate(self, double x):
        if x <= 0.5:
            return self.interpolation.evaluate(2 * x)
        return self.interpolation.evaluate(2 - 2 * x)

cdef class MirroredInterpolation(Interpolation):
    cdef Interpolation interpolation

    def __cinit__(self, Interpolation interpolation not None):
        self.interpolation = interpolation

    cdef double evaluate(self, double x):
        return self.interpolation.evaluate(1 - x)
