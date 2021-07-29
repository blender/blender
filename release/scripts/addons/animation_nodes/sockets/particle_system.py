import bpy
from bpy.types import ParticleSystem
from .. base_types import AnimationNodeSocket, PythonListSocket

class ParticleSystemSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_ParticleSystemSocket"
    bl_label = "Particle System Socket"
    dataType = "Particle System"
    allowedInputTypes = ["Particle System"]
    drawColor = (1.0, 0.8, 0.6, 1)
    storable = False
    comparable = True

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def getDefaultValueCode(cls):
        return "None"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, ParticleSystem) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class ParticleSystemListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_ParticleSystemListSocket"
    bl_label = "Particle System List Socket"
    dataType = "Particle System List"
    baseDataType = "Particle System"
    allowedInputTypes = ["Particle System List"]
    drawColor = (1.0, 0.8, 0.6, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, ParticleSystem) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
