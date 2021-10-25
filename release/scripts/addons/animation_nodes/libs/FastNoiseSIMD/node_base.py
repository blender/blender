import bpy
from bpy.props import *
from ... utils.names import toInterfaceName
from ... events import propertyChanged

from . wrapper import (
    PyNoise,
    noiseTypesData,
    perturbTypesData,
    fractalTypesData,
    cellularReturnTypesData,
    cellularDistanceFunctionsData
)

def itemsFromEnumData(data):
    return [(d[0], d[2], "", "NONE", d[3]) for d in data]

noiseTypeItems = itemsFromEnumData(noiseTypesData)
perturbTypeItems = itemsFromEnumData(perturbTypesData)
fractalTypeItems = itemsFromEnumData(fractalTypesData)
cellularReturnTypeItems = itemsFromEnumData(cellularReturnTypesData)
distanceFunctionItems = itemsFromEnumData(cellularDistanceFunctionsData)

noisePerNode = {}

class Noise3DNodeBase:
    def noiseSettingChanged(self, context):
        self.refresh()

    noiseType = EnumProperty(name = "Noise Type", default = "SIMPLEX",
        items = noiseTypeItems, update = noiseSettingChanged)

    perturbType = EnumProperty(name = "Perturb Type", default = "NONE",
        items = perturbTypeItems, update = noiseSettingChanged)

    fractalType = EnumProperty(name = "Fractal Type", default = "FBM",
        items = fractalTypeItems, update = noiseSettingChanged)

    cellularReturnType = EnumProperty(name = "Cellular Return Type", default = "CELL_VALUE",
        items = cellularReturnTypeItems, update = noiseSettingChanged)

    cellularLookupType = EnumProperty(name = "Cellular Lookup Type", default = "SIMPLEX",
        items = noiseTypeItems, update = noiseSettingChanged)

    cellularDistanceFunction = EnumProperty(name = "Cellular Distance Function",
        default = "EUCLIDEAN", items = distanceFunctionItems,
        update = noiseSettingChanged)

    def drawNoiseSettings(self, layout):
        layout.prop(self, "noiseType", text = "")
        if self.noiseType == "CELLULAR":
            col = layout.column()
            col.prop(self, "cellularReturnType", text = "")
            if self.cellularReturnType == "NOISE_LOOKUP":
                col.prop(self, "cellularLookupType", text = "")

    def drawAdvancedNoiseSettings(self, layout):
        layout.prop(self, "fractalType")
        layout.prop(self, "perturbType")

        col = layout.column()
        col.active = self.noiseType == "CELLULAR" and "DISTANCE" in self.cellularReturnType
        col.prop(self, "cellularDistanceFunction", text = "Distance")

    def createNoiseInputs(self):
        socketData = iterNoiseInputData(*self.getNoiseTypeTuple())
        for dataType, name, identifier, extra in socketData:
            self.newInput(dataType, name, identifier, **extra)

    def calculateNoise(self, vectors, *args):
        noise = self.getNoiseObject(args)
        return noise.calculateList(vectors)

    def getNoiseObject(self, args):
        noise = PyNoise()

        noise.setNoiseType(self.noiseType)
        noise.setFractalType(self.fractalType)
        noise.setPerturbType(self.perturbType)
        noise.setCellularReturnType(self.cellularReturnType)
        noise.setCellularNoiseLookupType(self.cellularLookupType)
        noise.setCellularDistanceFunction(self.cellularDistanceFunction)

        allArgs = defaultsArgs.copy()
        noiseInputData = iterNoiseInputData(*self.getNoiseTypeTuple())
        for value, (_, _, identifier, _) in zip(args, noiseInputData):
            allArgs[identifier] = value

        noise.setSeed(allArgs["seed"])
        noise.setFrequency(allArgs["frequency"])
        noise.setAxisScales(allArgs["axisScale"])
        noise.setAmplitude(allArgs["amplitude"])
        noise.setOffset(allArgs["offset"])
        noise.setOctaves(min(max(allArgs["octaves"], 1), 10))
        noise.setCellularJitter(allArgs["jitter"])
        noise.setCellularNoiseLookupFrequency(allArgs["lookupFrequency"])

        return noise

    def getNoiseTypeTuple(self):
        return (self.noiseType,
                self.perturbType, self.fractalType,
                self.cellularReturnType, self.cellularLookupType)


defaultsArgs = {
    "seed" : 0,
    "amplitude" : 1,
    "frequency" : 0.1,
    "axisScale" : (1, 1, 1),
    "offset" : (0, 0, 0),
    "octaves" : 3,
    "jitter" : 0.45,
    "lookupFrequency" : 0.2
}

def iterNoiseInputData(noiseType, perturbType, fractalType,
                       cellularReturnType, cellularLookupType):
    yield "Integer", "Seed", "seed", {}
    yield "Float", "Amplitude", "amplitude", {"value" : 1}

    if noiseType != "WHITE_NOISE":
        yield "Float", "Frequency", "frequency", {"value" : 0.1}
        yield "Vector", "Axis Scale", "axisScale", {"value" : (1, 1, 1), "hide" : True}
        yield "Vector", "Offset", "offset", {}

    if noiseType not in ("WHITE_NOISE", "CELLULAR"):
        yield "Integer", "Octaves", "octaves", {"value" : 3, "minValue" : 1, "maxValue" : 10}

    if noiseType == "CELLULAR":
        yield "Float", "Jitter", "jitter", {"value" : 0.45}
        if cellularReturnType == "NOISE_LOOKUP":
            yield "Float", "Lookup Frequency", "lookupFrequency", {"value" : 0.2}
