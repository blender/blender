import os
import bpy
import collections
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

# path, encoding : last modification, content
cache = {}

class TextFileReaderNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TextFileReaderNode"
    bl_label = "Text File Reader"
    bl_width_default = 170

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Text", "Path", "path", showFileChooser = True)
        self.newInput("Text", "Encoding", "encoding", value = "ascii")
        self.newOutput("Text", "Text", "text")

    def draw(self, layout):
        if self.inputs[0].isUnlinked:
            name = os.path.basename(self.inputs[0].value)
            if name != "":
                layout.label(name, icon = "FILE_TEXT")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        self.invokeFunction(layout, "clearCache", text = "Clear Cache")

    def clearCache(self):
        cache.clear()

    def execute(self, path, encoding):
        self.errorMessage = ""

        if not os.path.exists(path):
            self.errorMessage = "Path does not exist"
            return ""

        key = (path, encoding)

        lastModification = os.stat(path).st_mtime

        loadFile = False
        if key not in cache:
            loadFile = True
        else:
            oldLastModification = cache[key][0]
            if lastModification > oldLastModification:
                loadFile = True

        if loadFile:
            try:
                with open(path, "r", encoding = encoding) as f:
                    data = f.read()
                cache[key] = (lastModification, data)
            except LookupError:
                self.errorMessage = "Invalid Encoding"
            except:
                self.errorMessage = "Encoding Error"

        return cache.get(key, (0, ""))[1]
