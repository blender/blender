import bpy
from bpy.props import *
from math import radians
from bmesh.ops import dissolve_limit
from ... base_types import AnimationNode

class BMeshLimitedDissolveNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BMeshLimitedDissolveNode"
    bl_label = "Limited Dissolve BMesh"
    bl_width_default = 160

    def create(self):
        self.newInput("BMesh", "BMesh", "bm", dataIsModified = True)
        self.newInput("Float", "Angle Limit", "angleLimit", value = 30.0)
        self.newInput("Boolean", "Dissolve Boundries", "dissolveBoundries", value = False)
        self.newOutput("BMesh", "BMesh", "bm")

    def execute(self, bm, angleLimit, dissolveBoundries):
        dissolve_limit(bm, verts = bm.verts, edges = bm.edges,
                            angle_limit = radians(angleLimit),
                            use_dissolve_boundaries = dissolveBoundries)
        return bm
