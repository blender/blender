import bpy
from bpy.props import *
from collections import defaultdict
from ... base_types import AnimationNode

outputLinesByIdentifier = defaultdict(list)

class LoopViewerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LoopViewerNode"
    bl_label = "Loop Viewer"
    bl_width_default = 160

    textBlockName = StringProperty(name = "Text")

    def setup(self):
        self.newInput("Node Control", "...", "control")
        self.newInputSocket()

    def edit(self):
        controlSocket = self.inputs[-1]
        directOrigin = controlSocket.directOrigin
        if directOrigin is None: return
        socket = self.newInputSocket()
        socket.linkWith(directOrigin)
        controlSocket.removeLinks()

    def newInputSocket(self):
        socket = self.newInput("an_GenericSocket", "Data", "data")
        socket.removeable = True
        socket.moveable = True
        socket["dataWidth"] = 0
        socket.moveUp()
        return socket

    def draw(self, layout):
        if self.network.type == "Loop":
            row = layout.row(align = True)
            row.prop_search(self, "textBlockName", bpy.data, "texts", text = "")
            if self.textBlock is None:
                self.invokeFunction(row, "createNewTextBlock", icon = "ZOOMIN")
            else:
                self.invokeSelector(row, "AREA", "viewTextBlockInArea",
                    icon = "ZOOM_SELECTED")
        else:
            layout.label("Has to be in a loop", icon = "INFO")

    def drawAdvanced(self, layout):
        col = layout.column(align = True)
        for i, socket in enumerate(self.inputs[:-1]):
            col.prop(socket, '["dataWidth"]', text = "Width " + str(i + 1))

    def getInputSocketVariables(self):
        return {socket.identifier : "data_" + str(i) for i, socket in enumerate(self.inputs)}

    def getExecutionCode(self):
        if self.network.type == "Loop":
            names = ["data_" + str(i) for i in range(len(self.inputs[:-1]))]
            return "self.newOutputLine({})".format(", ".join(names))
        else:
            return ""

    def newOutputLine(self, *dataList):
        texts = []
        for data, socket in zip(dataList, self.inputs):
            if isinstance(data, float): text = str(round(data, 5))
            else: text = str(data)
            texts.append(text.rjust(socket["dataWidth"]))
        outputLinesByIdentifier[self.identifier].append(" ".join(texts))

    def updateTextBlock(self):
        textBlock = self.textBlock
        if textBlock is None: return

        textBlock.clear()
        textBlock.write("\n".join(outputLinesByIdentifier[self.identifier]))

    def clearOutputLines(self):
        try: del outputLinesByIdentifier[self.identifier]
        except: pass

    def createNewTextBlock(self):
        textBlock = bpy.data.texts.new(name = "Loop Viewer")
        self.textBlockName = textBlock.name

    def viewTextBlockInArea(self, area):
        area.type = "TEXT_EDITOR"
        area.spaces.active.text = self.textBlock

    @property
    def textBlock(self):
        return bpy.data.texts.get(self.textBlockName)

    def delete(self):
        self.clearOutputLines()
