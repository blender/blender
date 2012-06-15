#!/usr/bin/env python

import math

# IT's annoying that the 1023,4 and 4095,16 almost, but dont exactly, cancel. UGH
# The intent is clearly to have the same mapping, but it's not done very well.
# Sony engineers and/or the Academy should pick one of these mappings for both.

def SLog10_to_lin(x):
    return (math.pow(10.0,(((((x*1023.0)/4.0-16.0)/219.0)-0.616596-0.03)/0.432699))-0.037584)*0.9

def SLog12_to_lin(x):
    return (math.pow(10.0,(((((x*4095.0)/16.0-16.0)/219.0)-0.616596-0.03)/0.432699))-0.037584)*0.9


def WriteSPI1D(filename, fromMin, fromMax, data):
    f = file(filename,'w')
    f.write("Version 1\n")
    f.write("From %s %s\n" % (fromMin, fromMax))
    f.write("Length %d\n" % len(data))
    f.write("Components 1\n")
    f.write("{\n")
    for value in data:
        f.write("        %s\n" % value)
    f.write("}\n")
    f.close()

def Fit(value, fromMin, fromMax, toMin, toMax):
    if fromMin == fromMax:
        raise ValueError("fromMin == fromMax")
    return (value - fromMin) / (fromMax - fromMin) * (toMax - toMin) + toMin

#
# NOTE: The ctl matrix order is transposed compared to what OCIO expects
#const float SGAMUT_TO_ACES_MTX[3][3] = {
#	{ 0.754338638,  0.021198141, -0.009756991 },
#	#{ 0.133697046,  1.005410934,  0.004508563 },
#	{ 0.111968437, -0.026610548,  1.005253201 }

NUM_SAMPLES = 2**11
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(SLog10_to_lin(x))
WriteSPI1D('slog10.spi1d', RANGE[0], RANGE[1], data)

"""
NUM_SAMPLES = 2**13
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(SLog12_to_lin(x))
WriteSPI1D('slog12.spi1d', RANGE[0], RANGE[1], data)
"""
