from bpy.props import *
from . base import SocketTemplate
from ... utils.attributes import getattrRecursive
from ... sockets.info import toIdName as toSocketIdName
from ... sockets.info import (
    isBase, isList, toBaseDataType, toListDataType
)

converter = {
    ("LIST", "BASE") : toBaseDataType,
    ("BASE", "LIST") : toListDataType,
    ("BASE", "BASE") : lambda x: x,
    ("LIST", "LIST") : lambda x: x
}

checker = {
    "BASE" : isBase,
    "LIST" : isList
}

class ListTypeSelectorSocket(SocketTemplate):
    def __init__(self, name, identifier, socketType, propertyInfo, **kwargs):
        self.name = name
        self.identifier = identifier
        self.socketType = socketType
        self.propertyName = propertyInfo[0]
        self.propertyType = propertyInfo[1]
        self.socketSettings = kwargs

        self.toProperty = converter[(socketType, self.propertyType)]
        self.toType = converter[(self.propertyType, socketType)]
        self.check = checker[socketType]

    @classmethod
    def newProperty(cls, default):
        return StringProperty(default = default)

    def create(self, node, sockets):
        dataType = self.toType(getattrRecursive(node, self.propertyName))
        socketIdName = toSocketIdName(dataType)
        socket = sockets.new(socketIdName, self.name, self.identifier)
        socket.setAttributes(self.socketSettings)
        return socket

    def getSocketIdentifiers(self):
        return {self.identifier}

    def getRelatedPropertyNames(self):
        return {self.propertyName}

    def apply(self, node, socket):
        linkedDataTypes = tuple(sorted(socket.linkedDataTypes - {"Generic"}))
        if len(linkedDataTypes) > 0:
            linkedType = linkedDataTypes[0]
            if self.check(linkedType):
                propertyValue = self.toProperty(linkedType)
                return {self.propertyName : propertyValue}, {self.propertyName}
