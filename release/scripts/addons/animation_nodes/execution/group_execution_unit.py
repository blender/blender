from . compile_scripts import compileScript
from .. problems import ExecutionUnitNotSetup
from . code_generator import (getInitialVariables,
                              iterSetupCodeLines,
                              getGlobalizeStatement,
                              linkOutputSocketsToTargets,
                              getFunction_IterNodeExecutionLines)

class GroupExecutionUnit:
    def __init__(self, network, nodeByID):
        self.network = network
        self.setupScript = ""
        self.setupCodeObject = None
        self.executionData = {}

        self.generateScript(nodeByID)
        self.compileScript()
        self.execute = self.raiseNotSetupException


    def setup(self):
        self.executionData = {}
        exec(self.setupCodeObject, self.executionData, self.executionData)
        self.execute = self.executionData["main"]

    def insertSubprogramFunctions(self, data):
        self.executionData.update(data)

    def finish(self):
        self.executionData.clear()
        self.execute = self.raiseNotSetupException


    def getCodes(self):
        return [self.setupScript]


    def generateScript(self, nodeByID):
        try: nodes = self.network.getSortedAnimationNodes(nodeByID)
        except: return

        variables = getInitialVariables(nodes)
        self.setupScript = "\n".join(self.iterSetupScriptLines(nodes, variables, nodeByID))

    def iterSetupScriptLines(self, nodes, variables, nodeByID):
        yield from iterSetupCodeLines(nodes, variables)
        yield "\n\n"
        yield from self.iterFunctionGenerationScriptLines(nodes, variables, nodeByID)

    def iterFunctionGenerationScriptLines(self, nodes, variables, nodeByID):
        inputNode = self.network.getGroupInputNode(nodeByID)
        outputNode = self.network.getGroupOutputNode(nodeByID)

        yield self.getFunctionHeader(inputNode, variables)
        yield "    " + getGlobalizeStatement(nodes, variables)
        yield from iterIndented(self.iterExecutionScriptLines(nodes, variables, inputNode, nodeByID))
        yield "\n"
        yield "    " + self.getReturnStatement(outputNode, variables)

    def getFunctionHeader(self, inputNode, variables):
        for i, socket in enumerate(inputNode.outputs):
            variables[socket] = "group_input_" + str(i)

        parameterList = ", ".join([variables[socket] for socket in inputNode.sockets[:-1]])
        header = "def main({}):".format(parameterList)
        return header

    def iterExecutionScriptLines(self, nodes, variables, inputNode, nodeByID):
        iterNodeExecutionLines = getFunction_IterNodeExecutionLines()

        yield from linkOutputSocketsToTargets(inputNode, variables, nodeByID)
        for node in nodes:
            if node.bl_idname in ("an_GroupInputNode", "an_GroupOutputNode"): continue
            yield from iterNodeExecutionLines(node, variables)
            yield from linkOutputSocketsToTargets(node, variables, nodeByID)

    def getReturnStatement(self, outputNode, variables):
        if outputNode is None: return "return"
        returnList = ", ".join([variables[socket] for socket in outputNode.inputs[:-1]])
        return "return " + returnList

    def compileScript(self):
        self.setupCodeObject = compileScript(self.setupScript, name = "group: {}".format(repr(self.network.name)))


    def raiseNotSetupException(self):
        raise ExecutionUnitNotSetup()


def iterIndented(lines):
    for line in lines:
        yield "    " + line
