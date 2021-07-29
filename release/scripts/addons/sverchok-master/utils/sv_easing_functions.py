# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

'''
original c code:
https://raw.githubusercontent.com/warrenm/AHEasing/master/AHEasing/easing.c
Copyright (c) 2011, Auerhaus Development, LLC
http://sam.zoy.org/wtfpl/COPYING for more details.
'''

from math import sqrt, pow, sin, cos, floor, log
from math import pi as M_PI

# cached for performance
M_2_PI = M_PI * 2
M_PI_2 = M_PI / 2


#  Modeled after the line y = x
def LinearInterpolation(p):
    return p

# ======================= QUADRIC EASING FUNCTIONS =============================

# Modeled after the parabola y = x^2
def QuadraticEaseIn(p):
    return p * p


# Modeled after the parabola y = -x^2 + 2x
def QuadraticEaseOut(p):
    return p * (2 - p)


# Modeled after the piecewise quadratic
# y = (1/2)((2x)^2)             ; [0, 0.5)
# y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
def QuadraticEaseInOut(p):
    if (p < 0.5):
        return 2 * p * p
    else:
        f = 1 - p
        return 1 - 2 * f * f

# ======================== CUBIC EASING FUNCTIONS ==============================

# Modeled after the cubic y = x^3
def CubicEaseIn(p):
    return p * p * p


# Modeled after the cubic y = (x - 1)^3 + 1
def CubicEaseOut(p):
    f = 1 - p
    return 1 - f * f * f


# Modeled after the piecewise cubic
# y = (1/2)((2x)^3)       ; [0, 0.5)
# y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
def CubicEaseInOut(p):
    if (p < 0.5):
        return 4 * p * p * p
    else:
        f = 1 - p
        return 1 - 4 * f * f * f

# ======================= QUARTIC EASING FUNCTIONS =============================

# Modeled after the quartic x^4
def QuarticEaseIn(p):
    return p * p * p * p


# Modeled after the quartic y = 1 - (x - 1)^4
def QuarticEaseOut(p):
    f = 1 - p
    return 1 - f * f * f * f


# Modeled after the piecewise quartic
# y = (1/2)((2x)^4)        ; [0, 0.5)
# y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
def QuarticEaseInOut(p):
    if (p < 0.5):
        return 8 * p * p * p * p
    else:
        f = 1 - p
        return 1 - 8 * f * f * f * f

# ======================= QUINTIC EASING FUNCTIONS =============================

# Modeled after the quintic y = x^5
def QuinticEaseIn(p):
    return p * p * p * p * p


# Modeled after the quintic y = (x - 1)^5 + 1
def QuinticEaseOut(p):
    f = 1 - p
    return 1 - f * f * f * f * f


# Modeled after the piecewise quintic
# y = (1/2)((2x)^5)       ; [0, 0.5)
# y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
def QuinticEaseInOut(p):
    if (p < 0.5):
        return 16 * p * p * p * p * p
    else:
        f = 1 - p
        return 1 - 16 * f * f * f * f * f

# ===================== SINUSOIDAL EASING FUNCTIONS ============================

# Modeled after quarter-cycle of sine wave
def SineEaseIn(p):
    return 1 - cos(p * M_PI_2)


# Modeled after quarter-cycle of sine wave (different phase)
def SineEaseOut(p):
    return sin(p * M_PI_2)


# Modeled after half sine wave
def SineEaseInOut(p):
    return (1 - cos(p * M_PI))/2

# ====================== CIRCULAR EASING FUNCTIONS ============================

# Modeled after shifted quadrant IV of unit circle
def CircularEaseIn(p):
    return 1 - sqrt(1 - p * p)


# Modeled after shifted quadrant II of unit circle
def CircularEaseOut(p):
    return sqrt((2 - p) * p)


# Modeled after the piecewise circular function
# y = (1/2)(1 - sqrt(1 - 4x^2))           ; [0, 0.5)
# y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
def CircularEaseInOut(p):
    if(p < 0.5):
        return 0.5 * (1 - sqrt(1 - 4 * (p * p)))
    else:
        return 0.5 * (sqrt(-((2 * p) - 3) * ((2 * p) - 1)) + 1)

# ===================== EXPONENTIAL EASING FUNCTIONS ===========================

# prepare derived settings to be used in the exponential function
def prepareExponentialSettings(b=2, e=10):
    b = max(b, 0.0001)
    m = pow(b,-e)
    m = m if m != 1.0 else 1.0001
    s = 1/(1-m)
    return b, e, m, s

defaultExponentialSettings = prepareExponentialSettings()

# Modeled after the exponential function y = 2^(10(x - 1))
def ExponentialEaseIn(p, settings=defaultExponentialSettings):
    b, e, m, s = settings
    return (pow(b, e * (p - 1)) - m) * s


# Modeled after the exponential function y = -2^(-10x) + 1
def ExponentialEaseOut(p, settings=defaultExponentialSettings):
    return 1 - ExponentialEaseIn(1-p, settings)


# Modeled after the piecewise exponential
# y = (1/2)2^(10(2x - 1))         ; [0,0.5)
# y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
def ExponentialEaseInOut(p, settings=defaultExponentialSettings):
    if(p < 0.5):
        return 0.5 * ExponentialEaseIn(2*p, settings)
    else:
        return 0.5 + 0.5 * ExponentialEaseOut(2*p-1, settings)

# ======================= ELASTIC EASING FUNCTIONS =============================

# prepare derived settings to be used in the elastic function
def prepareElasticSettings(n=13, b=2, e=10):
    alpha = n * M_2_PI + M_PI_2
    return b, e, alpha

defaultElasticSettings = prepareElasticSettings()

# Modeled after the damped sine wave y = sin(13pi/2*x)*pow(2, 10 * (x - 1))
def ElasticEaseIn(p, settings=defaultElasticSettings):
    b, e, alpha = settings
    return sin(alpha * p) * pow(b, e * (p - 1))


# Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*pow(2, -10x) + 1
def ElasticEaseOut(p, settings=defaultElasticSettings):
    return 1 - ElasticEaseIn(1-p, settings)


# Modeled after the piecewise exponentially-damped sine wave:
# y = (1/2)*sin(13pi/2*(2*x))*pow(2, 10 * ((2*x) - 1))      ; [0,0.5)
# y = (1/2)*(sin(-13pi/2*((2x-1)+1))*pow(2,-10(2*x-1)) + 2) ; [0.5, 1]
def ElasticEaseInOut(p, settings=defaultElasticSettings):
    if (p < 0.5):
        return 0.5 * ElasticEaseIn(2*p, settings)
    else:
        return 0.5 + 0.5 * ElasticEaseOut(2*p-1, settings)

# ========================= BACK EASING FUNCTIONS ==============================

# Modeled after the overshooting cubic y = x^3-x*sin(x*pi)
def BackEaseIn(p, s=1):
    return p * p * p - p * sin(p * M_PI) * s


# Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
def BackEaseOut(p, s=1):
    return 1 - BackEaseIn(1-p, s)


# Modeled after the piecewise overshooting cubic function:
# y = (1/2)*((2x)^3-(2x)*sin(2*x*pi))           ; [0, 0.5)
# y = (1/2)*(1-((1-x)^3-(1-x)*sin((1-x)*pi))+1) ; [0.5, 1]
def BackEaseInOut(p, s=1):
    if (p < 0.5):
        return 0.5 * BackEaseIn(2*p, s)
    else:
        return 0.5 + 0.5 * BackEaseOut(2*p-1, s)

# ======================= BOUNCE EASING FUNCTIONS ==============================

# geometric progression sum S(a,n)
def ss(a, n):
    return (1-pow(a,n))/(1-a)

# prepare derived settings to be used in the bounce function
def prepareBounceSettings(n=4, a=0.5):
    powan = pow(a,n)
    loga  = log(a)
    ssan  = ss(a,n)
    return n, a, powan, loga, ssan

defaultBounceSettings = prepareBounceSettings()

'''
    BounceEaseIn

    Formula based on geometric progression + parabola mix:

    Sum of all n bounces with attenuation a:
    S(a,n) = (1 - a^n)/(1-a) : a < 1

    Parabola function f(x):
    f(x) = 1 - (1-2*x)^2 : for x = [0,1] => f = [0,1]

    Current bounce number based on value of p, a and n:
    nn = floor(log(1-p*(1-pow(a,n)))/log(a))

    Current bounce interval based on value of p, a and n:
    x0 = S(a,nn)    : begin of bounce interval
    x1 = S(a,nn+1)  : end of bounce interval
    xx = S(a,n) * p : current x value in the [0,S(a,n)] for p

    Remap [x0,x1] interval to [0,1] to generate the f(x) parabola
    x = (xx-x0)/(x1-x0)

    Calculate the parabola for the current interval scaled to interval
    f = (1 - (1-2*x)^2) * a^n  : where x1-x0 = a^n

    Note: Code derived by Marius Giurgi for sverchok @ 2017 :)
'''
def BounceEaseIn(p, settings=defaultBounceSettings):
    n, a, powan, loga, ssan = settings

    # for ease in the progression should go from small to big bounces
    p = 1 - p # invert the progression

    # sn = ss(a, n)
    sn = ssan

    # remap p to start from the half of the first bounce
    p = (1/2 + (sn - 1/2)*p)/sn

    # the bounce number at the current p value
    nn = floor(log(1 - p*(1 - powan)) / loga)

    # find current bounce interval and current x location in interval
    x0 = ss(a, nn)
    x1 = ss(a, nn+1)
    xx = sn * p

    # Remap bounce interval to [0,1] to generate the f(x) parabola
    x = (xx - x0) / (x1 - x0) # x = [0,1]

    xt = 1 - 2*x
    f = (1 - xt * xt) * pow(a, nn)
    # print("p = %.2f  nn = %d  x0 = %.2f  x1 = %.2f  xx = %.2f  x = %.2f  f = %.2f" % (p, nn, x0, x1, xx, x, f))
    return f


def BounceEaseOut(p, settings=defaultBounceSettings):
    return 1 - BounceEaseIn(1-p, settings)


def BounceEaseInOut(p, settings=defaultBounceSettings):
    if p < 0.5:
        return 0.5 * BounceEaseIn(2*p, settings)
    else:
        return 0.5 + 0.5 * BounceEaseOut(2*p-1, settings)


easing_dict = {
    0: LinearInterpolation,
    1: QuadraticEaseIn,
    2: QuadraticEaseOut,
    3: QuadraticEaseInOut,
    4: CubicEaseIn,
    5: CubicEaseOut,
    6: CubicEaseInOut,
    7: QuarticEaseIn,
    8: QuarticEaseOut,
    9: QuarticEaseInOut,
    10: QuinticEaseIn,
    11: QuinticEaseOut,
    12: QuinticEaseInOut,
    13: SineEaseIn,
    14: SineEaseOut,
    15: SineEaseInOut,
    16: CircularEaseIn,
    17: CircularEaseOut,
    18: CircularEaseInOut,
    19: ExponentialEaseIn,
    20: ExponentialEaseOut,
    21: ExponentialEaseInOut,
    22: ElasticEaseIn,
    23: ElasticEaseOut,
    24: ElasticEaseInOut,
    25: BackEaseIn,
    26: BackEaseOut,
    27: BackEaseInOut,
    28: BounceEaseIn,
    29: BounceEaseOut,
    30: BounceEaseInOut
}
