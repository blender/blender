import re
import bpy
from bpy.props import *
from ... utils.code import isCodeValid
from ... tree_info import keepNodeState
from ... base_types import AnimationNode
from ... utils.layout import splitAlignment
from ... events import executionCodeChanged
from ... execution.code_generator import iter_Imports

variableNames = list("xyzabcdefghijklmnopqrstuvw")

expressionByIdentifier = {}

class ExpressionNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ExpressionNode"
    bl_label = "Expression"
    bl_width_default = 210
    dynamicLabelType = "HIDDEN_ONLY"

    def settingChanged(self, context = None):
        self.errorMessage = ""
        self.containsSyntaxError = not isCodeValid(self.expression)
        executionCodeChanged()

    def outputDataTypeChanged(self, context):
        self.recreateOutputSocket()

    expression = StringProperty(name = "Expression", update = settingChanged)
    errorMessage = StringProperty()
    lastCorrectionType = IntProperty()
    containsSyntaxError = BoolProperty()

    debugMode = BoolProperty(name = "Debug Mode", default = True,
        description = "Show detailed error messages in the node but is slower.",
        update = executionCodeChanged)

    correctType = BoolProperty(name = "Correct Type", default = True,
        description = "Check the type of the result and correct it if necessary",
        update = executionCodeChanged)

    moduleNames = StringProperty(name = "Modules", default = "math",
        description = "Comma separated module names which can be used inside the expression",
        update = executionCodeChanged,)

    outputDataType = StringProperty(default = "Generic", update = outputDataTypeChanged)

    fixedOutputDataType = BoolProperty(name = "Fixed Data Type", default = False,
        description = "When activated the output type does not automatically change")

    inlineExpression = BoolProperty(name = "Inline Expression", default = False,
        description = ("Inlining improves performance but the modules can't be used directly"
                       " (e.g. you will have to write math.sin(x) instead of just sin(x))"),
        update = executionCodeChanged)

    def setup(self):
        self.newInput("Node Control", "New Input")
        self.newOutput("Generic", "Result", "result")

    @keepNodeState
    def recreateOutputSocket(self):
        self.clearOutputs()
        self.newOutput(self.outputDataType, "Result", "result")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "expression", text = "")
        self.invokeSelector(row, "DATA_TYPE", "changeOutputTypeManually", icon = "SCRIPTWIN")

        col = layout.column(align = True)
        if self.containsSyntaxError:
            col.label("Syntax Error", icon = "ERROR")
        else:
            if self.debugMode and self.expression != "":
                if self.errorMessage != "":
                    row = col.row()
                    row.label(self.errorMessage, icon = "ERROR")
                    self.invokeFunction(row, "clearErrorMessage", icon = "X", emboss = False)
            if self.correctType:
                if self.lastCorrectionType == 1:
                    col.label("Automatic Type Correction", icon = "INFO")
                elif self.lastCorrectionType == 2:
                    col.label("Wrong Output Type", icon = "ERROR")
                    col.label("Expected {}".format(repr(self.outputDataType)), icon = "INFO")

    def drawAdvanced(self, layout):
        layout.prop(self, "moduleNames")

        col = layout.column(align = True)
        col.prop(self, "debugMode")
        col.prop(self, "correctType")
        col.prop(self, "inlineExpression")

        layout.prop(self, "fixedOutputDataType")

    def drawLabel(self):
        return self.expression

    def drawControlSocket(self, layout, socket):
        left, right = splitAlignment(layout)
        left.label(socket.name)
        self.invokeSelector(right, "DATA_TYPE", "newInputSocket",
            icon = "ZOOMIN", emboss = False)

    def getInputSocketVariables(self):
        return {socket.identifier : socket.text for socket in self.inputs}

    def getExecutionCode(self):
        if self.expression.strip() == "" or self.containsSyntaxError:
            yield "self.errorMessage = ''"
            yield "result = self.outputs[0].getDefaultValue()"
            return

        settings = self.getExpressionFunctionSettings()
        if self.identifier not in expressionByIdentifier:
            self.updateExpressionFunction(settings)
        elif expressionByIdentifier[self.identifier][0] != settings:
            self.updateExpressionFunction(settings)

        if self.debugMode:
            yield "try:"
            yield "    result = " + self.getExpressionCode()
            yield "    self.errorMessage = ''"
            yield "except Exception as e:"
            yield "    result = None"
            yield "    self.errorMessage = str(e)"
        else:
            yield "result = " + self.getExpressionCode()

        if self.correctType:
            yield "result, self.lastCorrectionType = self.outputs[0].correctValue(result)"

    def getExpressionCode(self):
        if self.inlineExpression:
            return self.expression
        else:
            if len(self.inputs) == 1:
                return "self.expressionFunction()"
            else:
                parameterList = ", ".join(socket.text for socket in self.inputs[:-1])
                return "self.expressionFunction({})".format(parameterList)

    def getUsedModules(self):
        moduleNames = re.split("\W+", self.moduleNames)
        modules = [module for module in moduleNames if module != ""]
        return modules

    def clearErrorMessage(self):
        self.errorMessage = ""

    def updateExpressionFunction(self, settings):
        function = createExpressionFunction(*settings)
        expressionByIdentifier[self.identifier] = (settings, function)

    def getExpressionFunctionSettings(self):
        parameters = [socket.text for socket in self.inputs[:-1]]
        modules = self.getUsedModules()
        return self.expression, parameters, modules

    @property
    def expressionFunction(self):
        return expressionByIdentifier[self.identifier][1]

    def edit(self):
        self.edit_Inputs()
        self.edit_Output()

    def edit_Inputs(self):
        emptySocket = self.inputs["New Input"]
        directOrigin = emptySocket.directOrigin
        if directOrigin is None: return
        dataOrigin = emptySocket.dataOrigin
        if dataOrigin.dataType == "Node Control": return
        socket = self.newInputSocket(dataOrigin.dataType)
        emptySocket.removeLinks()
        socket.linkWith(directOrigin)

    def edit_Output(self):
        if self.fixedOutputDataType: return
        dataTargets = self.outputs[0].dataTargets
        if len(dataTargets) == 1:
            dataType = dataTargets[0].dataType
            if dataType not in ("Node Control", "Generic", "Generic List"):
                self.changeOutputType(dataType)

    def changeOutputTypeManually(self, dataType):
        self.fixedOutputDataType = True
        self.changeOutputType(dataType)

    def changeOutputType(self, dataType):
        if self.outputDataType != dataType:
            self.outputDataType = dataType

    def newInputSocket(self, dataType):
        name = self.getNewSocketName()
        socket = self.newInput(dataType, name, "input")
        socket.dataIsModified = True
        socket.textProps.editable = True
        socket.textProps.variable = True
        socket.textProps.unique = True
        socket.display.text = True
        socket.display.textInput = True
        socket.display.removeOperator = True
        socket.text = name
        socket.moveable = True
        socket.removeable = True
        socket.moveUp()
        if len(self.inputs) > 2:
            socket.copyDisplaySettingsFrom(self.inputs[0])
        return socket

    def getNewSocketName(self):
        inputNames = {socket.text for socket in self.inputs}
        for name in variableNames:
            if name not in inputNames: return name
        return "x"

    def socketChanged(self):
        self.settingChanged()
        executionCodeChanged()

def createExpressionFunction(expression, variables, modules):
    code = "\n".join(iterExpressionFunctionLines(expression, variables, modules))
    globalsDict = {}
    exec(code, globalsDict, globalsDict)
    return globalsDict["main"]

def iterExpressionFunctionLines(expression, variables, modules):
    yield from iter_Imports()
    for name in modules:
        yield "import " + name
        yield "from {} import *".format(name)

    yield "def main({}):".format(", ".join(variables))
    yield "    __result__ = " + expression
    yield "    return __result__"
