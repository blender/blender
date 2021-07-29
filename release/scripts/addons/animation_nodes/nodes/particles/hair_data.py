import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import VectorizedNode
from ... data_structures import PolySpline, BezierSpline, Vector3DList

splineTypeItems = [
    ("BEZIER", "Bezier", "Each control point has two handles", "CURVE_BEZCURVE", 0),
    ("POLY", "Poly", "Linear interpolation between the spline points", "NOCURVE", 1)
]

class ParticleSystemHairDataNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ParticleSystemHairDataNode"
    bl_label = "Hair Data"

    useParticleSystemList = VectorizedNode.newVectorizeProperty()

    splineType = EnumProperty(name = "Spline Type", default = "POLY",
        items = splineTypeItems, update = propertyChanged)

    def create(self):
        self.newVectorizedInput("Particle System", "useParticleSystemList",
            ("Particle System", "particleSystem"),
            ("Particle Systems", "particleSystems"))

        self.newInput("Boolean", "Use World Space", "useWorldSpace", value = True)

        self.newOutput("Spline List", "Hair Splines", "hairSplines")

    def draw(self, layout):
        layout.prop(self, "splineType", text = "")

    def execute(self, particleSystems, useWorldSpace):
        if not self.useParticleSystemList:
            particleSystems = [particleSystems]

        splines = []
        for particleSystem in particleSystems:
            splines.extend(self.getHairSplines(particleSystem, useWorldSpace))
        return splines

    def getHairSplines(self, particleSystem, useWorldSpace):
        if particleSystem is None:
            return []
        if particleSystem.settings.type != "HAIR":
            return []

        worldMatrix = particleSystem.id_data.matrix_world
        newSpline = self.getSplineType()

        splines = []
        for particle in particleSystem.particles:
            points = Vector3DList(len(particle.hair_keys))
            particle.hair_keys.foreach_get("co", points.asMemoryView())
            if useWorldSpace:
                points.transform(worldMatrix)
            splines.append(newSpline(points))
        return splines

    def getSplineType(self):
        if self.splineType == "POLY":
            return PolySpline
        elif self.splineType == "BEZIER":
            return BezierSpline
