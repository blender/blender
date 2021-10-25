import bpy
from ... base_types import AnimationNode, NodeUIExtension
from ... graphics.rectangle import Rectangle

a = None

class ActionViewerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ActionViewerNode"
    bl_label = "Action Viewer"

    def create(self):
        self.newInput("Action", "Action", "action")

    def execute(self, action):
        global a
        a = action

    def getUIExtensions(self):
        if a is None:
            return []
        return [ActionUIExtension(a)]


class ActionUIExtension(NodeUIExtension):
    def __init__(self, action):
        self.action = action

    def draw(self, node, position, width):
        box = ActionBox(self.action, position, width)
        box.draw()
        return 0 # total height

class ActionBox:
    def __init__(self, action, position, width):
        self.action = action
        self.x, self.y = position
        self.width = width

    def draw(self):
        height = 35
        evaluator = self.action.getEvaluator(self.action.getChannels())
        for i in range(5):
            rec = FrameRangeRectangle(self.x, self.y - i * height - 2,
                                      self.x + self.width, self.y - (i + 1) * height + 2,
                                      0, 250)
            rec.draw(color = (0.3, 0.3, 0.3, 1))
            evaluator.drawPreview(i, rec)

class FrameRangeRectangle(Rectangle):
    def __init__(self, x1, y1, x2, y2, startFrame, endFrame):
        assert startFrame <= endFrame
        super().__init__(x1, y1, x2, y2)
        self.startFrame = startFrame
        self.endFrame = endFrame

    def copy(self):
        rec = FrameRangeRectangle(self.x1, self.y1, self.x2, self.y2, self.startFrame, self.endFrame)
        return rec

    def getClampedSubFrameRange(self, start, end):
        return self.getFrameRange(self.clampFrame(start), self.clampFrame(end))

    def getFrameRange(self, start, end):
        return FrameRangeRectangle(
            self.getFrameX(start), self.top,
            self.getFrameX(end), self.bottom,
            start, end)

    def shiftFrameRange(self, offset):
        self.startFrame += offset
        self.endFrame += offset

    def setFrameRange(self, startFrame, endFrame):
        assert startFrame <= endFrame
        self.startFrame = startFrame
        self.endFrame = endFrame

    def clampFrame(self, frame):
        return min(max(frame, self.startFrame), self.endFrame)

    def getFrameX(self, frame):
        assert self.frameAmount > 0
        return (frame - self.startFrame) / self.frameAmount * self.width + self.left

    @property
    def frameAmount(self):
        return self.endFrame - self.startFrame
