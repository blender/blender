from collections import defaultdict

implicitConversions = defaultdict(dict)
conversionFunctions = []

def registerImplicitConversion(fromType, toType, converter):
    if hasattr(converter, "__call__"):
        conversionFunctions.append(converter)
        path = "AN.sockets.implicit_conversion.conversionFunctions[{}](value)"
        path = path.format(len(conversionFunctions) - 1)
        implicitConversions[toType][fromType] = path
    elif isinstance(converter, str):
        implicitConversions[toType][fromType] = converter
    elif converter is None:
        implicitConversions[toType][fromType] = None
    else:
        raise Exception("expected str, function or None")

def iterTypesThatCanConvertTo(toType):
    yield from implicitConversions[toType].keys()

def getConversionCode(fromType, toType):
    try: return implicitConversions[toType][fromType]
    except: return None
