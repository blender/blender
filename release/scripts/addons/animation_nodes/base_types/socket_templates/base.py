class SocketTemplate:
    def createInput(self, node):
        return self.create(node, node.inputs)

    def createOutput(self, node):
        return self.create(node, node.outputs)

    def create(self, node, sockets):
        raise NotImplementedError()

    def getSocketIdentifiers(self):
        raise NotImplementedError()

    def getRelatedPropertyNames(self):
        raise NotImplementedError()

    def applyWithContext(self, node, socket, updatedProperties, fixedProperties):
        return self.apply(node, socket)

    def apply(self, node, socket):
        raise NotImplementedError()
