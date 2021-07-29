import bpy
from bpy.props import *
from . data_types import dataTypeByIdentifier

def checkIDKeyName(name):
    if "*" in name:
        raise ValueError("'*' must not be in the property name")
    if name == "":
        raise ValueError("property name must not be empty")

class IDKeyProperties(bpy.types.PropertyGroup):
    bl_idname = "an_IDKeyProperties"

    def _getIDKeyData(self, dataType, propertyName):
        return dataTypeByIdentifier[dataType].get(self.id_data, propertyName)

    def _setIDKeyData(self, dataType, propertyName, data):
        return dataTypeByIdentifier[dataType].set(self.id_data, propertyName, data)

    def _doesIDKeyExist(self, dataType, propertyName):
        return dataTypeByIdentifier[dataType].exists(self.id_data, propertyName)

    def _drawProperty(self, layout, dataType, propertyName):
        dataTypeByIdentifier[dataType].drawProperty(layout, self.id_data, propertyName)

    def _drawExtras(self, layout, dataType, propertyName):
        dataTypeByIdentifier[dataType].drawExtras(layout, self.id_data, propertyName)

    def _createIDKey(self, dataType, propertyName):
        checkIDKeyName(propertyName)
        dataTypeByIdentifier[dataType].create(self.id_data, propertyName)

    def _removeIDKey(self, dataType, propertyName):
        dataTypeByIdentifier[dataType].remove(self.id_data, propertyName)

    get = _getIDKeyData
    set = _setIDKeyData
    create = _createIDKey
    remove = _removeIDKey
    exists = _doesIDKeyExist
    drawProperty = _drawProperty
    drawExtras = _drawExtras

def register():
    bpy.types.ID.id_keys = PointerProperty(name = "ID Keys", type = IDKeyProperties)

def unregister():
    del bpy.types.ID.id_keys
