import bpy
from mathutils import Vector
from collections import defaultdict
from .. graphics.table import Table
from .. graphics.rectangle import Rectangle
from .. utils.nodes import getAnimationNodeTrees
from .. utils.blender_ui import getDpiFactor, getDpi
from .. graphics.drawing_2d import drawText, setTextDrawingDpi

statisticsViewIsActive = False

class StatisticsDrawer(bpy.types.Operator):
    bl_idname = "an.statistics_drawer"
    bl_label = "Statistics Drawer"

    @classmethod
    def poll(cls, context):
        return context.area.type == "NODE_EDITOR" and not statisticsViewIsActive

    def invoke(self, context, event):
        global statisticsViewIsActive
        statisticsViewIsActive = True

        args = ()
        self.drawHandler = bpy.types.SpaceNodeEditor.draw_handler_add(self.drawCallback, args, "WINDOW", "POST_PIXEL")
        context.window_manager.modal_handler_add(self)

        dpiFactor = getDpiFactor()
        self.drawOffset = Vector((20 * dpiFactor, context.region.height - 40 * dpiFactor))

        self.lastMousePosition = Vector((event.mouse_region_x, event.mouse_region_y))
        self.enableViewDrag = False

        self.updateStatistics()
        return {"RUNNING_MODAL"}

    def updateStatistics(self):
        self.statistics = NodeStatistics(getAnimationNodeTrees())
        self.nodeTreeTable = createNodeTreeTable(self.statistics)
        self.mostUsedNodesTable = createMostUsedNodesTable(self.statistics)

    def modal(self, context, event):
        if context.area is not None:
            context.area.tag_redraw()

        if event.type in {"RIGHTMOUSE", "ESC"}:
            return self.finish()

        mousePosition = Vector((event.mouse_region_x, event.mouse_region_y))
        if "CTRL" in event.type:
            if event.value == "PRESS": self.enableViewDrag = True
            if event.value == "RELEASE": self.enableViewDrag = False
        if self.enableViewDrag:
            self.drawOffset += mousePosition - self.lastMousePosition
        self.lastMousePosition = mousePosition

        if self.enableViewDrag:
            return {"RUNNING_MODAL"}

        return {"PASS_THROUGH"}

    def finish(self):
        bpy.types.SpaceNodeEditor.draw_handler_remove(self.drawHandler, "WINDOW")
        global statisticsViewIsActive
        statisticsViewIsActive = False
        return {"FINISHED"}

    def drawCallback(self):
        self.updateStatistics()
        region = bpy.context.region

        dpiFactor = getDpiFactor()
        bg = Rectangle.fromRegionDimensions(region)
        bg.color = (1, 1, 1, 0.5)
        bg.draw()

        setTextDrawingDpi(getDpi())

        text = "Hold CTRL to drag the statistics - Press ESC or RMB to exit this view"
        drawText(text, 10 * dpiFactor, region.height - 20 * dpiFactor,
            color = (0, 0, 0, 0.5), size = 11)

        offset = self.drawOffset.copy()
        self.drawNodeTreeTable(offset, dpiFactor)

        offset.x += 500 * dpiFactor
        self.drawMostUsedNodesTable(offset, dpiFactor)

    def drawNodeTreeTable(self, location, dpiFactor):
        table = self.nodeTreeTable

        table.clearColumns()
        table.newColumn("Tree", 200 * dpiFactor, "CENTER", font = 0)
        table.newColumn("Nodes", 80 * dpiFactor, "RIGHT", font = 1)
        table.newColumn("Links", 80 * dpiFactor, "RIGHT", font = 1)
        table.newColumn("Subprograms", 110 * dpiFactor, "RIGHT", font = 1)

        table.rowHeight = 22 * dpiFactor
        table.headerRowHeight = 30 * dpiFactor
        table.lineThickness = 1 * dpiFactor
        table.cellPadding = 5 * dpiFactor
        table.dataFontSize = 11
        table.headerFontSize = 14
        table.draw(location)

    def drawMostUsedNodesTable(self, location, dpiFactor):
        table = self.mostUsedNodesTable

        table.clearColumns()
        table.newColumn("#", 30 * dpiFactor, "RIGHT", font = 1)
        table.newColumn("Node", 170 * dpiFactor, "LEFT", font = 0)
        table.newColumn("Amount", 80 * dpiFactor, "RIGHT", font = 1)

        table.rowHeight = 22 * dpiFactor
        table.headerRowHeight = 30 * dpiFactor
        table.lineThickness = 1 * dpiFactor
        table.cellPadding = 5 * dpiFactor
        table.dataFontSize = 11
        table.headerFontSize = 14
        table.draw(location)



def createNodeTreeTable(statistics):
    table = Table()

    for stats in statistics.nodeTreeStats + [statistics.combinedStats]:
        table.newRow({
            "Tree" : stats.name,
            "Nodes" : "{} / {}".format(stats.functionalNodeAmount, stats.totalNodeAmount),
            "Links" : stats.totalLinkAmount,
            "Subprograms" : stats.subprogramAmount})

    return table

def createMostUsedNodesTable(statistics):
    table = Table()
    stats = statistics.combinedStats
    items = sorted(stats.amountByLabel.items(), key = lambda x: x[1], reverse = True)[:10]

    for i, (name, amount) in enumerate(items, 1):
        table.newRow({
            "#" : str(i),
            "Node" : name,
            "Amount" : amount})

    return table


class NodeStatistics:
    def __init__(self, nodeTrees):
        self.nodeTreeAmount = len(nodeTrees)
        self.nodeTreeStats = [TreeStatistics.fromTree(tree) for tree in nodeTrees]
        self.combinedStats = TreeStatistics.fromMerge(self.nodeTreeStats)
        self.combinedStats.name = "Sum:"

class TreeStatistics:
    def __init__(self):
        self.name = ""
        self.totalNodeAmount = 0
        self.totalLinkAmount = 0
        self.functionalNodeAmount = 0
        self.subprogramAmount = 0
        self.amountByLabel = defaultdict(int)

    @classmethod
    def fromTree(cls, nodeTree):
        stats = cls()

        stats.name = repr(nodeTree.name)
        stats.totalNodeAmount = len(nodeTree.nodes)
        stats.totalLinkAmount = len(nodeTree.links)
        stats.subprogramAmount = len(nodeTree.subprogramNetworks)

        for node in nodeTree.nodes:
            stats.amountByLabel[node.bl_label] += 1

        stats.functionalNodeAmount = stats.totalNodeAmount \
                     - stats.amountByLabel["Reroute"] \
                     - stats.amountByLabel["Frame"]

        return stats

    @classmethod
    def fromMerge(cls, statistics):
        stats = cls()

        stats.name = ", ".join(s.name for s in statistics)
        stats.totalNodeAmount = sum(s.totalNodeAmount for s in statistics)
        stats.totalLinkAmount = sum(s.totalLinkAmount for s in statistics)
        stats.functionalNodeAmount = sum(s.functionalNodeAmount for s in statistics)
        stats.subprogramAmount = sum(s.subprogramAmount for s in statistics)

        for s in statistics:
            for name, amount in s.amountByLabel.items():
                stats.amountByLabel[name] += amount

        return stats
