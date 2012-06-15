#!/usr/bin/env python

import math

"""

// ARRI ALEXA IDT for ALEXA logC files
//  with camera EI set to 800
// Written by v3_IDT_maker.py v0.06 on Thursday 01 March 2012 by alex

float
normalizedLogCToRelativeExposure(float x) {
	if (x > 0.149659)
		return (pow(10,(x - 0.385537) / 0.247189) - 0.052272) / 5.555556;
	else
		return (x - 0.092809) / 5.367650;
}

"""

def logCToLinear(x):
	if (x > 0.149659):
		return (math.pow(10.0,(x - 0.385537) / 0.247189) - 0.052272) / 5.555556
	else:
		return (x - 0.092809) / 5.367650

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

NUM_SAMPLES = 2**14
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(logCToLinear(x))
WriteSPI1D('logc800.spi1d', RANGE[0], RANGE[1], data)
