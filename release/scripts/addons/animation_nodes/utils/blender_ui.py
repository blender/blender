import bpy
import functools
from mathutils import Vector

def iterActiveSpacesByType(type):
    for space in iterActiveSpaces():
        if space.type == type:
            yield space

def iterActiveSpaces():
    for area in iterAreas():
        yield area.spaces.active


def getAreaWithType(type):
    for area in iterAreasByType(type):
        return area

def iterAreasByType(type):
    for area in iterAreas():
        if area.type == type:
            yield area

def iterAreas():
    for screen in iterActiveScreens():
        for area in screen.areas:
            yield area

def iterActiveScreens():
    for windowManager in bpy.data.window_managers:
        for window in windowManager.windows:
            yield window.screen


def splitAreaVertical(area, factor):
    newArea = splitArea(area, "VERTICAL", factor)
    if factor < 0.5:
        return newArea, area
    return area, newArea

def splitAreaHorizontal(area, factor):
    newArea = splitArea(area, "HORIZONTAL", factor)
    if factor < 0.5:
        return newArea, area
    return area, newArea

def splitArea(area, direction, factor = 0.5):
    areasWithSameType = set(iterAreasByType(area.type))
    overwrite = {"area" : area,
                 "region" : area.regions[0],
                 "window" : bpy.context.window,
                 "screen" : bpy.context.screen}
    bpy.ops.screen.area_split(overwrite, direction = direction, factor = factor)
    newArea = (set(iterAreasByType(area.type)) - areasWithSameType).pop()
    return newArea


def redrawAll():
    for area in iterAreas():
        area.tag_redraw()

def redrawAreaType(areaType):
    for area in iterAreasByType(areaType):
        area.tag_redraw()

def isViewportRendering():
    return any([space.viewport_shade == "RENDERED" for space in iterActiveSpacesByType("VIEW_3D")])

def getDpiFactor():
    return getDpi() / 72

def getDpi():
    systemPreferences = bpy.context.user_preferences.system
    retinaFactor = getattr(systemPreferences, "pixel_size", 1)
    return systemPreferences.dpi * retinaFactor

def executeInAreaType(areaType):
    def changeAreaTypeDecorator(function):
        @functools.wraps(function)
        def wrapper(*args, **kwargs):
            area = bpy.context.area
            oldType = area.type
            area.type = areaType
            output = function(*args, **kwargs)
            area.type = oldType
            return output
        return wrapper
    return changeAreaTypeDecorator


def iterNodeCornerLocations(nodes, region, horizontal):
    if horizontal == "LEFT":
        yield from iterNodeBottomLeftCornerLocations(nodes, region)
    elif horizontal == "RIGHT":
        yield from iterNodeBottomRightCornerLocations(nodes, region)

def iterNodeBottomLeftCornerLocations(nodes, region):
    factor = getDpiFactor()
    viewToRegion = region.view2d.view_to_region
    for node in nodes:
        location = node.viewLocation * factor
        dimensions = node.dimensions
        yield Vector(viewToRegion(location.x, location.y - dimensions.y, clip = False))

def iterNodeBottomRightCornerLocations(nodes, region):
    factor = getDpiFactor()
    viewToRegion = region.view2d.view_to_region
    for node in nodes:
        location = node.viewLocation * factor
        dimensions = node.dimensions
        yield Vector(viewToRegion(location.x + dimensions.x, location.y - dimensions.y, clip = False))


class PieMenuHelper:
    def draw(self, context):
        pie = self.layout.menu_pie()
        self.drawLeft(pie)
        self.drawRight(pie)
        self.drawBottom(pie)
        self.drawTop(pie)
        self.drawTopLeft(pie)
        self.drawTopRight(pie)
        self.drawBottomLeft(pie)
        self.drawBottomRight(pie)

    def drawLeft(self, layout):
        self.empty(layout)

    def drawRight(self, layout):
        self.empty(layout)

    def drawBottom(self, layout):
        self.empty(layout)

    def drawTop(self, layout):
        self.empty(layout)

    def drawTopLeft(self, layout):
        self.empty(layout)

    def drawTopRight(self, layout):
        self.empty(layout)

    def drawBottomLeft(self, layout):
        self.empty(layout)

    def drawBottomRight(self, layout):
        self.empty(layout)

    def empty(self, layout, text = ""):
        layout.row().label(text)
