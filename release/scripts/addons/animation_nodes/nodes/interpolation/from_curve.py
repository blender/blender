import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from .. container_provider import getHelperMaterial
from ... algorithms.interpolations import PyInterpolation, CachedInterpolation


class CurveMapPointCache(bpy.types.PropertyGroup):
    bl_idname = "an_CurveMapPointCache"
    handle_type = StringProperty()
    location = FloatVectorProperty(size = 2)

class CurveMapCache(bpy.types.PropertyGroup):
    bl_idname = "an_CurveMapCache"
    extend = StringProperty()
    points = CollectionProperty(type = CurveMapPointCache)
    dirty = BoolProperty(default = True)


class CurveInterpolationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InterpolationFromCurveNode"
    bl_label = "Curve Interpolation"
    bl_width_default = 200

    curveMapCache = PointerProperty(type = CurveMapCache)
    cacheInterpolation = BoolProperty(name = "Cache Interpolation", default = False,
        description = "Allows much faster evaluation of the interpolation",
        update = propertyChanged)

    def setup(self):
        self.createCurveNode()

    def create(self):
        self.newOutput("Interpolation", "Interpolation", "interpolation")

    def draw(self, layout):
        layout.template_curve_mapping(self.curveNode, "mapping", type = "NONE")
        self.invokeFunction(layout, "resetEndPoints", text = "Reset End Points")
        layout.prop(self, "cacheInterpolation")

    def execute(self):
        # load cached curve map if available
        # this happens when the node tree is appended to another file
        if not self.curveMapCache.dirty:
            self.loadCachedCurveMap()
            self.curveMapCache.dirty = True

        mapping = self.mapping
        curve = mapping.curves[3]
        try: curve.evaluate(0.5)
        except: mapping.initialize()

        interpolation = PyInterpolation(curve.evaluate)
        if self.cacheInterpolation:
            return CachedInterpolation(interpolation)
        else:
            return interpolation

    def createCurveNode(self):
        material = getHelperMaterial()
        node = material.node_tree.nodes.new("ShaderNodeRGBCurve")
        node.name = self.identifier
        mapping = self.mapping
        mapping.use_clip = True
        mapping.clip_min_y = -0.5
        mapping.clip_max_y = 1.5
        self.resetEndPoints()
        return node

    def removeCurveNode(self):
        material = getHelperMaterial()
        tree = material.node_tree
        curveNode = tree.nodes.get(self.identifier)
        if curveNode is not None:
            tree.nodes.remove(curveNode)

    def resetEndPoints(self):
        points = self.curve.points
        points[0].location = (0, 0)
        points[-1].location = (1, 1)
        self.mapping.update()

    def duplicate(self, sourceNode):
        self.createCurveNode()
        self.copyOtherCurve(sourceNode.curve)

    def delete(self):
        self.removeCurveNode()

    def cacheCurveMap(self):
        curve = self.curve
        self.curveMapCache.extend = curve.extend
        self.curveMapCache.points.clear()
        for point in curve.points:
            item = self.curveMapCache.points.add()
            item.handle_type = point.handle_type
            item.location = point.location
        self.curveMapCache.dirty = False

    def loadCachedCurveMap(self):
        self.copyOtherCurve(self.curveMapCache)

    def copyOtherCurve(self, otherCurve):
        curve = self.curve
        curve.extend = otherCurve.extend
        curvePoints = curve.points
        for i, point in enumerate(otherCurve.points):
            if len(curvePoints) == i:
                curvePoints.new(50, 50) # random start position
            curvePoints[i].location = point.location
            curvePoints[i].handle_type = point.handle_type
        self.mapping.update()

    def save(self):
        self.cacheCurveMap()

    @property
    def curve(self):
        return self.mapping.curves[3]

    @property
    def mapping(self):
        return self.curveNode.mapping

    @property
    def curveNode(self):
        material = getHelperMaterial()
        node = material.node_tree.nodes.get(self.identifier)
        if node is None: node = self.createCurveNode()
        return node
