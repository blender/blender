from bpy.props import *
from collections import defaultdict
from . base_node import AnimationNode
from ... sockets.info import toListDataType
from ... tree_info import getLinkedOutputsDict_ChangedIdentifiers
from .. effects import AutoSelectVectorization, VectorizeCodeEffect

settingsByIdentifier = defaultdict(lambda: (VectorizeCodeEffect(), dict()))

class VectorizedNode(AnimationNode):
    autoVectorizeExecution = False

    def preCreate(self):
        super().preCreate()
        self._removeSettings()
        self.vectorization = AutoSelectVectorization()

    def postCreate(self):
        self.newSocketEffect(self.vectorization)
        super().postCreate()

    @classmethod
    def newVectorizeProperty(cls):
        return BoolProperty(default = False, update = AnimationNode.refresh)

    def newVectorizedInput(self, dataType, properties, baseData, listData):
        properties = self._formatInputProperties(properties)

        baseDataType = dataType
        listDataType = toListDataType(dataType)

        isCurrentlyList = getattr(self, properties[0]) and self._evaluateDependencies(properties[1])
        socket = self.newInputGroup(isCurrentlyList,
            [baseDataType] + list(baseData),
            [listDataType] + list(listData))

        self.vectorization.input(self, properties[0], socket, isCurrentlyList, properties[1])
        if isCurrentlyList:
            self._codeEffect.input(baseData[1], listData[1])
        return socket

    def newVectorizedOutput(self, dataType, properties, baseData, listData):
        properties = self._formatOutputProperties(properties)

        baseDataType = dataType
        listDataType = toListDataType(dataType)

        isCurrentlyList = self._evaluateDependencies(properties)
        socket = self.newOutputGroup(isCurrentlyList,
            [baseDataType] + list(baseData),
            [listDataType] + list(listData))

        self._outputBaseByListName[listData[1]] = baseData[1]
        self.vectorization.output(self, properties, socket, isCurrentlyList)
        if isCurrentlyList:
            self._codeEffect.output(baseData[1], listData[1], len(self.outputs) - 1)
        return socket

    def _formatInputProperties(self, properties):
        if isinstance(properties, str):
            return (properties, [])
        return properties

    def _formatOutputProperties(self, properties):
        if isinstance(properties, str):
            return [(properties, )]
        return properties

    def _evaluateDependencies(self, properties):
        for group in properties:
            if isinstance(group, str):
                if not getattr(self, group):
                    return False
            else:
                if not any(getattr(self, prop) for prop in group):
                    return False
        return True

    def getCodeEffects(self):
        if not self.autoVectorizeExecution:
            return []
        return [self._codeEffect]

    def getLinkedBaseOutputsDict(self):
        return getLinkedOutputsDict_ChangedIdentifiers(self, self._outputBaseByListName)

    def _removeSettings(self):
        if self.identifier in settingsByIdentifier:
            del settingsByIdentifier[self.identifier]

    @property
    def _codeEffect(self):
        return settingsByIdentifier[self.identifier][0]

    @property
    def _outputBaseByListName(self):
        return settingsByIdentifier[self.identifier][1]
