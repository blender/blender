import bpy
import textwrap
from collections import defaultdict
from .. utils.timing import prettyTime
from .. draw_handler import drawHandler
from .. graphics.text_box import TextBox
from .. utils.operators import makeOperator
from .. preferences import getExecutionCodeType
from .. utils.blender_ui import iterNodeCornerLocations

class NodeMeasurements:
    __slots__ = ("minTime", "totalTime", "calls")

    def __init__(self):
        self.totalTime = 0
        self.calls = 0
        self.minTime = 1e10

    def registerTime(self, time):
        self.calls += 1
        self.totalTime += time
        if time < self.minTime:
            self.minTime = time

    def __repr__(self):
        return textwrap.dedent("""\
            Min: {}
            Total: {}
            Calls: {:,d}\
            """.format(prettyTime(self.minTime),
                       prettyTime(self.totalTime),
                       self.calls))

measurementsByNodeIdentifier = defaultdict(NodeMeasurements)

@makeOperator("an.reset_measurements", "Reset Measurements", redraw = True)
def resetMeasurements():
    measurementsByNodeIdentifier.clear()

def getMeasurementsDict():
    return measurementsByNodeIdentifier

def getMinExecutionTimeString(node):
    measure = measurementsByNodeIdentifier[node.identifier]
    if measure.calls > 0:
        return prettyTime(measure.minTime)
    else:
        return "Not Measured"

@drawHandler("SpaceNodeEditor", "WINDOW")
def drawMeasurementResults():
    tree = bpy.context.space_data.edit_tree
    if tree is None: return
    if tree.bl_idname != "an_AnimationNodeTree": return
    if getExecutionCodeType() != "MEASURE": return

    nodes = tree.nodes
    region = bpy.context.region
    leftCorners = iterNodeCornerLocations(nodes, region, horizontal = "LEFT")
    rightCorners = iterNodeCornerLocations(nodes, region, horizontal = "RIGHT")

    for node, leftBottom, rightBottom in zip(nodes, leftCorners, rightCorners):
        if node.isAnimationNode and not node.hide:
            if "NO_TIMING" not in node.options:
                drawMeasurementResultForNode(node, leftBottom, rightBottom)

def drawMeasurementResultForNode(node, leftBottom, rightBottom):
    result = measurementsByNodeIdentifier[node.identifier]
    if result.calls == 0: text = "Not Measured"
    else: text = str(result)

    width = rightBottom.x - leftBottom.x

    textBox = TextBox(text, leftBottom, width,
                      fontSize = width / node.dimensions.x * 11)
    textBox.padding = 3
    textBox.draw()
