import bpy
from . callbacks import newCallback
from . an_operator import AnimationNodeOperator
from .. execution.units import getExecutionUnitByNetwork

executionCodeTextBlockName = "Execution Code"

class PrintCurrentExecutionCode(bpy.types.Operator, AnimationNodeOperator):
    bl_idname = "an.print_current_execution_code"
    bl_label = "Print Execution Code"
    bl_description = "Print the code of the currently active node"

    def execute(self, context):
        code = getCurrentExecutionCode(lineNumbers = True)
        print("\n" * 10)
        print("#### Code for network that contains the active node ####")
        print("\n" * 2)
        print(code)
        return {"FINISHED"}

class WriteCurrentExecutionCode(bpy.types.Operator, AnimationNodeOperator):
    bl_idname = "an.write_current_execution_code"
    bl_label = "Write Execution Code"
    bl_description = "Write the code of the currently active node in a text block"

    def execute(self, context):
        code = getCurrentExecutionCode()

        textBlock = bpy.data.texts.get(executionCodeTextBlockName)
        if textBlock is None:
            textBlock = bpy.data.texts.new(executionCodeTextBlockName)

        textBlock.clear()
        textBlock.write(code)
        return {"FINISHED"}

def setupTextEditor(area):
    area.type = "TEXT_EDITOR"
    area.spaces.active.text = bpy.data.texts.get(executionCodeTextBlockName)

setupTextEditorCallback = newCallback(setupTextEditor)

def getCurrentExecutionCode(lineNumbers = False):
    network = bpy.context.active_node.network
    unit = getExecutionUnitByNetwork(network)
    codes = unit.getCodes()
    if lineNumbers:
        codes = [insertLineNumbers(code) for code in codes]
    return ("\n" * 3).join(codes)

def insertLineNumbers(code):
    lines = []
    for i, line in enumerate(code.split("\n")):
        lines.append("{:3}.  {}".format(i + 1, line))
    return "\n".join(lines)
