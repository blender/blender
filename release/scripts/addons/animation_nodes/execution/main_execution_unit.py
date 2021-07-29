import sys, traceback
from .. import problems
from . compile_scripts import compileScript
from .. problems import ExecutionUnitNotSetup, ExceptionDuringExecution
from . code_generator import (getInitialVariables,
                              iterSetupCodeLines,
                              linkOutputSocketsToTargets,
                              getFunction_IterNodeExecutionLines)

class MainExecutionUnit:
    def __init__(self, network, nodeByID):
        self.network = network
        self.setupScript = ""
        self.executeScript = ""
        self.setupCodeObject = None
        self.executeCodeObject = None
        self.executionData = {}

        self.generateScripts(nodeByID)
        self.compileScripts()
        self.execute = self.raiseNotSetupException


    def setup(self):
        self.executionData = {}
        exec(self.setupCodeObject, self.executionData, self.executionData)
        self.execute = self.executeUnit

    def insertSubprogramFunctions(self, data):
        self.executionData.update(data)

    def finish(self):
        self.executionData.clear()
        self.execute = self.raiseNotSetupException

    def executeUnit(self):
        try:
            exec(self.executeCodeObject, self.executionData, self.executionData)
            return True
        except:
            print("\n"*5)
            traceback.print_exc()
            ExceptionDuringExecution().report()
            return False


    def getCodes(self):
        return [self.setupScript, self.executeScript]



    def generateScripts(self, nodeByID):
        try: nodes = self.network.getSortedAnimationNodes(nodeByID)
        except: return

        variables = getInitialVariables(nodes)
        self.setupScript = "\n".join(iterSetupCodeLines(nodes, variables))
        self.executeScript = "\n".join(self.iterExecutionScriptLines(nodes, variables, nodeByID))

    def iterExecutionScriptLines(self, nodes, variables, nodeByID):
        iterNodeExecutionLines = getFunction_IterNodeExecutionLines()

        for node in nodes:
            yield from iterNodeExecutionLines(node, variables)
            yield from linkOutputSocketsToTargets(node, variables, nodeByID)

    def compileScripts(self):
        self.setupCodeObject = compileScript(self.setupScript, name = "setup: {}".format(repr(self.network.treeName)))
        self.executeCodeObject = compileScript(self.executeScript, name = "execution: {}".format(repr(self.network.treeName)))


    def raiseNotSetupException(self, *args, **kwargs):
        raise ExecutionUnitNotSetup()
