#!/usr/bin/env python

import math

# IT's annoying that the 1023,4 and 4095,16 almost, but dont exactly, cancel. UGH
# The intent is clearly to have the same mapping, but it's not done very well.
# Sony engineers and/or the Academy should pick one of these mappings for both.

def SLog_to_lin(x):
    return (math.pow(10.0,(((((x*1023.0)/4.0-16.0)/219.0)-0.616596-0.03)/0.432699))-0.037584)*0.9

def SLog2_to_lin(x):
    if x < 0.030001222851889303:
        return (x-0.030001222851889303 ) * 0.28258064516129
    return (219.0*(math.pow(10.0, ((x-0.616596-0.03)/0.432699)) - 0.037584) /155.0)

steps = 21
for i in xrange(steps):
    x = i/(steps-1.0)
    print x, SLog_to_lin(x), SLog2_to_lin(x)

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

"""
5500K:

ACESR = 0.8764457030 * Rraw +  0.0145411681 * Graw +  0.1090131290 * Braw;
ACESG = 0.0774075345 * Rraw +  0.9529571767 * Graw + -0.0303647111 * Braw;
ACESB = 0.0573564351 * Rraw + -0.1151066335 * Graw +  1.0577501984 * Braw;

3200K:

ACESR = 1.0110238740 * Rraw +  -0.1362526051 * Graw +  0.1252287310 * Braw;
ACESG = 0.1011994504 * Rraw +   0.9562196265 * Graw + -0.0574190769 * Braw;
ACESB = 0.0600766530 * Rraw +  -0.1010185315 * Graw +  1.0409418785 * Braw;
"""

"""
NUM_SAMPLES = 2**11
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(SLog10_to_lin(x))
WriteSPI1D('slog2.spi1d', RANGE[0], RANGE[1], data)
"""
