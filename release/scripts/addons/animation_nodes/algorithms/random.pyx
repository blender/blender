import sys
random = __import__("random")
from mathutils import Vector, Euler, Quaternion, Color

from .. utils.limits cimport INT_MAX
from libc.math cimport M_PI as PI
from libc.math cimport sqrt, sin, cos

def getUniformRandom(seed, min, max):
    return uniformRandomNumber(seed, min, max)

def getRandomColor(seed = None, hue = None, saturation = None, value = None):
    if seed is None: random.seed()
    else: random.seed(seed)

    if hue is None: hue = random.random()
    if saturation is None: saturation = random.random()
    if value is None: value = random.random()

    color = Color()
    color.hsv = hue, saturation, value
    return color

def getRandom3DVector(seed, double size):
    cdef int _seed = seed % INT_MAX
    return Vector((
        uniformRandomNumber(_seed + 542655, -size, size),
        uniformRandomNumber(_seed + 765456, -size, size),
        uniformRandomNumber(_seed + 123587, -size, size)
    ))

def getRandomNormalized3DVector(seed, double size):
    cdef float vector[3]
    cdef int _seed = seed % INT_MAX
    randomNormalized3DVector(_seed, vector, size)
    return Vector((vector[0], vector[1], vector[2]))

def randomNumberTuple(seed, int size, double scale):
    cdef int _seed = seed % INT_MAX
    cdef int i
    _seed *= 23412
    return tuple(randomNumber(_seed + i) * scale for i in range(size))

def uniformRandomNumberWithTwoSeeds(seed1, seed2, double min, double max):
    return uniformRandomNumber((seed1 * 674523 + seed2 * 3465284) % 0x7fffffff, min, max)

cdef void randomNormalized3DVector(int seed, float *vector, float size):
    cdef double a = uniformRandomNumber(seed, 0, 2 * PI)
    cdef double b = uniformRandomNumber(seed + 234452, -1, 1)
    cdef double c = sqrt(1 - b * b)
    vector[0] = c * cos(a) * size
    vector[1] = c * sin(a) * size
    vector[2] = b * size

cdef int uniformRandomInteger(int x, int min, int max):
    return <int>uniformRandomNumber(x, min, <double>max + 0.9999999)

cdef double uniformRandomNumber(int x, double min, double max):
    '''Generate a random number between min and max using a seed'''
    x = (x<<13) ^ x
    return ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 2147483648.0 * (max - min) + min

cdef double randomNumber(int x):
    '''Generate a random number between -1 and 1 using a seed'''
    x = (x<<13) ^ x
    return 1.0 - ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0

cdef double randomNumber_Positive(int x):
    '''Generate a random number between 0 and 1 using a seed'''
    x = (x<<13) ^ x
    return ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 2147483648.0
