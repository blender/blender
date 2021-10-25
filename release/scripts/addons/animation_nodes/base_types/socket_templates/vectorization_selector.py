from bpy.props import *
from . base import SocketTemplate
from .. effects.code_effects import VectorizeCodeEffect, DefaultBaseElement
from ... utils.attributes import getattrRecursive
from ... sockets.info import toIdName as toSocketIdName
from ... sockets.info import (
    toListDataType,
    getAllowedInputDataTypes,
    getAllowedTargetDataTypes
)

class VectorizedSocket(SocketTemplate):
    def __init__(self, dataType, properties, baseData, listData, codeProperties = {}):
        self.baseName = baseData[0]
        self.baseIdentifier = baseData[1]
        self.baseSettings = baseData[2] if len(baseData) == 3 else {}

        self.listName = listData[0]
        self.listIdentifier = listData[1]
        self.listSettings = listData[2] if len(listData) == 3 else {}

        self.baseDataType = dataType
        self.listDataType = toListDataType(dataType)

        self.inputBaseTypes = getAllowedInputDataTypes(self.baseDataType)
        self.inputListTypes = getAllowedInputDataTypes(self.listDataType)

        self.outputBaseTypes = getAllowedTargetDataTypes(self.baseDataType)
        self.outputListTypes = getAllowedTargetDataTypes(self.listDataType)

        if isinstance(properties, str):
            self.properties = [properties]
        elif isinstance(properties, (list, tuple)):
            self.properties = properties

        self.allowListExtension = codeProperties.pop("allowListExtension", True)
        self.defaultElement = codeProperties.pop("default", DefaultBaseElement)
        if len(codeProperties) > 0:
            raise Exception("invalid code properties: " + str(list(codeProperties.keys())))

    @classmethod
    def newProperty(cls):
        return BoolProperty(default = False)

    def createInput(self, node):
        shouldBeList = self.inputShouldBeList(node)
        return self.createSocket(node, node.inputs, shouldBeList)

    def createOutput(self, node):
        shouldBeList = self.outputShouldBeList(node)
        return self.createSocket(node, node.outputs, shouldBeList)

    def createSocket(self, node, sockets, shouldBeList):
        if shouldBeList:
            socketIdName = toSocketIdName(self.listDataType)
            socket = sockets.new(socketIdName, self.listName, self.listIdentifier)
            socket.setAttributes(self.listSettings)
        else:
            socketIdName = toSocketIdName(self.baseDataType)
            socket = sockets.new(socketIdName, self.baseName, self.baseIdentifier)
            socket.setAttributes(self.baseSettings)
            socket.setTemporarySocketTransparency(0.80)
        return socket

    def inputShouldBeList(self, node):
        return all(getattrRecursive(node, prop) for prop in self.properties)

    def outputShouldBeList(self, node):
        return any(getattrRecursive(node, prop) for prop in self.properties)

    def getSocketIdentifiers(self):
        return {self.baseIdentifier, self.listIdentifier}

    def getRelatedPropertyNames(self):
        return set(self.properties)

    def applyWithContext(self, node, socket, updatedProperties, fixedProperties):
        linkedDataTypes = tuple(sorted(socket.linkedDataTypes - {"Generic", "Generic List"}))
        if len(linkedDataTypes) == 0:
            return {prop : False for prop in self.properties}, set()

        linkedType = linkedDataTypes[0]
        if socket.isInput:
            if linkedType in self.inputListTypes:
                if self.noneIsFixedBase(updatedProperties, fixedProperties):
                    return {prop : True for prop in self.properties}, set(self.properties)
            elif linkedType in self.inputBaseTypes:
                prop = self.getPropertyThatShouldBeBase(updatedProperties, fixedProperties)
                if prop is not None:
                    return {prop : False}, {prop}
        else:
            if linkedType in self.outputListTypes:
                prop = self.getPropertyThatShouldBeList(updatedProperties, fixedProperties)
                if prop is not None:
                    return {prop : True}, {prop}
            elif linkedType in self.outputBaseTypes:
                if self.canAllBeBase(updatedProperties, fixedProperties):
                    return {prop : False for prop in self.properties}, set(self.properties)

    def noneIsFixedBase(self, updatedProperties, fixedProperties):
        for prop in self.properties:
            if not updatedProperties.get(prop, False) and prop in fixedProperties:
                return False
        return True

    def getPropertyThatShouldBeBase(self, updatedProperties, fixedProperties):
        for prop in self.properties:
            if not (updatedProperties.get(prop, False) and prop in fixedProperties):
                return prop
        return None

    def getPropertyThatShouldBeList(self, updatedProperties, fixedProperties):
        # first check if there is already a list
        for prop in self.properties:
            if updatedProperties.get(prop, False):
                return prop

        # then check if something can be made a list
        for prop in self.properties:
            if prop not in fixedProperties:
                return prop

        return None

    def canAllBeBase(self, updatedProperties, fixedProperties):
        for prop in self.properties:
            if updatedProperties.get(prop, False) and prop in fixedProperties:
                return False
        return True

    @classmethod
    def CodeEffect(cls, node):
        effect = VectorizeCodeEffect()

        for i, (socket, template) in enumerate(node.iterInputSocketsWithTemplate()):
            if isinstance(template, VectorizedSocket):
                if template.inputShouldBeList(node):
                    effect.input(template.baseIdentifier, template.listIdentifier, i, template.allowListExtension, template.defaultElement)

        for i, (socket, template) in enumerate(node.iterOutputSocketsWithTemplate()):
            if isinstance(template, VectorizedSocket):
                if template.outputShouldBeList(node):
                    effect.output(template.baseIdentifier, template.listIdentifier, i)

        return effect
