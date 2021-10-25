import bpy
import textwrap
from collections import defaultdict
from .. utils.timing import prettyTime
from .. draw_handler import drawHandler
from .. graphics.text_box import TextBox
from .. utils.operators import makeOperator
from .. preferences import getExecutionCodeType

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

def getMeasurementResultString(node):
    result = measurementsByNodeIdentifier[node.identifier]
    if result.calls == 0: return "Not Measured"
    else: return str(result)
