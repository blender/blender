import bpy
import cProfile
from bpy.props import *
from io import StringIO
from contextlib import redirect_stdout

class ProfileAnimationNodes(bpy.types.Operator):
    bl_idname = "an.profile"
    bl_label = "Profile"

    function = StringProperty()
    output = StringProperty()
    sort = StringProperty()

    def execute(self, context):
        result = self.getProfilingResult()

        if self.output == "CONSOLE":
            print(result)
        elif self.output == "TEXT_BLOCK":
            textBlock = self.getOutputTextBlock()
            textBlock.clear()
            textBlock.write(result)

        return {"FINISHED"}

    def getProfilingResult(self):
        resultBuffer = StringIO()
        with redirect_stdout(resultBuffer):
            d = {"function" : self.executeFunction}
            cProfile.runctx("function()", d, d, sort = self.sort)
            self.executeFunction()
        return resultBuffer.getvalue()

    def executeFunction(self):
        if self.function == "EXECUTION":
            execute_TreeExecutiong()
        elif self.function == "TREE_ANALYSIS":
            execute_TreeAnalysis()
        elif self.function == "UPDATE_EVERYTHING":
            execute_UpdateEverything()
        elif self.function == "SCRIPT_GENERATION":
            execute_ScriptGeneration()

    def getOutputTextBlock(self):
        textBlockName = "Profiling"
        if textBlockName in bpy.data.texts:
            return bpy.data.texts[textBlockName]
        else:
            return bpy.data.texts.new(textBlockName)


def execute_TreeExecutiong():
    bpy.context.space_data.edit_tree.execute()

def execute_TreeAnalysis():
    from .. import tree_info
    tree_info.update()

def execute_UpdateEverything():
    from .. import update
    update.updateEverything()

def execute_ScriptGeneration():
    from .. execution import units
    from .. utils.nodes import createNodeByIdDict
    nodeByID = createNodeByIdDict()
    units.createExecutionUnits(nodeByID)
    nodeByID.clear()
