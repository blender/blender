from . implementations import *

def getInterpolationPreset(str name, bint easeIn, bint easeOut):
    if not (easeIn or easeOut): return Linear()
    elif name == "LINEAR": return Linear()
    elif name == "SINUSOIDAL":
        if easeIn and easeOut: return SinInOut()
        elif easeIn: return SinIn()
        return SinOut()
    elif name in exponentByName.keys():
        exponent = exponentByName[name]
        if easeIn and easeOut: return PowerInOut(exponent)
        elif easeIn: return PowerIn(exponent)
        return PowerOut(exponent)
    elif name == "EXPONENTIAL":
        base, exponent = 2, 5
        if easeIn and easeOut: return ExponentialInOut(base, exponent)
        elif easeIn: return ExponentialIn(base, exponent)
        return ExponentialOut(base, exponent)
    elif name == "CIRCULAR":
        if easeIn and easeOut: return CircularInOut()
        elif easeIn: return CircularIn()
        return CircularOut()
    elif name == "BACK":
        scale = 1.7
        if easeIn and easeOut: return BackInOut(scale)
        elif easeIn: return BackIn(scale)
        return BackOut(scale)
    elif name == "BOUNCE":
        bounces, factor = 4, 1.5
        if easeIn and easeOut: return BounceInOut(bounces, factor)
        elif easeIn: return BounceIn(bounces, factor)
        return BounceOut(bounces, factor)
    elif name == "ELASTIC":
        bounces, base, exponent = 6, 1.6, 6
        if easeIn and easeOut: return ElasticInOut(bounces, base, exponent)
        elif easeIn: return ElasticIn(bounces, base, exponent)
        return ElasticOut(bounces, base, exponent)
    raise ValueError("no preset with name '{0}'".format(name))

exponentByName = {
    "QUADRATIC" : 2,
    "CUBIC" : 3,
    "QUARTIC" : 4,
    "QUINTIC" : 5 }
