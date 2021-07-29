# GPL # "author": Sjaak-de-Draak

bl_info = {
    "name": "Triangles",
    "description": "Create different types of triangles",
    "author": "Sjaak-de-Draak",
    "version": (1, 0, 1),
    "blender": (2, 68, 0),
    "location": "View3D > Add > Mesh",
    "warning": "First Version",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Triangles",
    "category": "Add Mesh"}

"""
This script provides a triangle mesh primitive
and a toolbar menu to further specify settings
"""

import math
import bpy
from mathutils import Vector
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        )


def checkEditMode():
    # Check if we are in edit mode
    # Returns:  1 if True
    #           0 if False
    if (bpy.context.active_object.mode == 'EDIT'):
        return 1
    return 0


def exitEditMode():
    # Check if we are in edit mode (cuz we don't want this when creating a new Mesh)
    # If we are then toggle back to object mode
    # Check if there are active objects
    if bpy.context.active_object is not None:
        # Only the active object should be in edit mode
        if (bpy.context.active_object.mode == 'EDIT'):
            bpy.ops.object.editmode_toggle()


class MakeTriangle(Operator):
    bl_idname = "mesh.make_triangle"
    bl_label = "Triangle"
    bl_description = "Construct different types of Triangle Meshes"
    bl_options = {"REGISTER", "UNDO"}

    nothing = 0
    Ya = 0.0
    Xb = 0.0
    Xc = 0.0
    Vertices = []
    Faces = []

    triangleTypeList = [
            ('ISOSCELES', "Isosceles", "Two equal sides", 0),
            ('EQUILATERAL', "Equilateral", "Three equal sides and angles (60°)", 1),
            ('ISOSCELESRIGHTANGLE', "Isosceles right angled", "90° angle and two equal sides", 2),
            ('SCALENERIGHTANGLE', "Scalene right angled", "90° angle, no equal sides", 3)
            ]
    triangleFaceList = [
            ('DEFAULT', "Normal", "1 Tri(angle) face", 0),
            ('TRIANGLES', "3 Tri faces", "4 Verticies & 3 Tri(angle) faces", 1),
            ('QUADS', "3 Quad faces", "7 Verticies & 3 Quad faces", 2),
            ('SAFEQUADS', "6 Quad faces", "12 Verticies & 6 Quad faces", 3)
            ]

    # add definitions for some manipulation buttons
    flipX = BoolProperty(
            name="Flip X sign",
            description="Draw on the other side of the X axis (Mirror on Y axis)",
            default=False
            )
    flipY = BoolProperty(
            name="Flip Y sign",
            description="Draw on the other side of the Y axis (Mirror on X axis)",
            default=False
            )
    scale = FloatProperty(
            name="Scale",
            description="Triangle scale",
            default=1.0,
            min=1.0
            )
    triangleType = EnumProperty(
            items=triangleTypeList,
            name="Type",
            description="Triangle Type"
            )
    triangleFace = EnumProperty(
            items=triangleFaceList,
            name="Face types",
            description="Triangle Face Types"
            )
    at_3Dcursor = BoolProperty(
            name="Use 3D Cursor",
            description="Draw the triangle where the 3D cursor is",
            default=False
            )

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(self, "triangleType", text="Type")
        col.prop(self, "scale")
        col.prop(self, "triangleFace", text="Face")

        col = layout.column(align=True)
        col.prop(self, "at_3Dcursor", text="3D Cursor", toggle=True)

        row = col.row(align=True)
        row.prop(self, "flipX", toggle=True)
        row.prop(self, "flipY", toggle=True)

    def drawBasicTriangleShape(self):
        # set everything to 0
        Xb = Xc = 0.0
        Ya = 0.0

        scale = self.scale
        Xsign = -1 if self.flipX else 1
        Ysign = -1 if self.flipY else 1

        # Isosceles (2 equal sides)
        if (self.triangleType == 'ISOSCELES'):
            # below a simple triangle containing 2 triangles with 1:2 side ratio
            Ya = (1 * Ysign * scale)
            A = Vector([0.0, Ya, 0.0])
            Xb = (0.5 * Xsign * scale)
            B = Vector([Xb, 0.0, 0.0])
            Xc = (-0.5 * Xsign * scale)
            C = Vector([Xc, 0.0, 0.0])

            self.Ya = Ya
            self.Xb = Xb
            self.Xc = Xc
            self.Vertices = [A, B, C, ]

            return True

        # Equilateral (all sides equal)
        if (self.triangleType == 'EQUILATERAL'):
            Ya = (math.sqrt(0.75) * Ysign * scale)
            A = Vector([0.0, Ya, 0.0])
            Xb = (0.5 * Xsign * scale)
            B = Vector([Xb, 0.0, 0.0])
            Xc = (-0.5 * Xsign * scale)
            C = Vector([Xc, 0.0, 0.0])

            self.Ya = Ya
            self.Xb = Xb
            self.Xc = Xc
            self.Vertices = [A, B, C, ]

            return True

        # Isosceles right angled (1, 1, sqrt(2))
        if (self.triangleType == 'ISOSCELESRIGHTANGLE'):
            Ya = (1 * Ysign * scale)
            A = Vector([0.0, Ya, 0.0])
            Xb = 0.0
            B = Vector([Xb, 0.0, 0.0])
            Xc = (1 * Xsign * scale)
            C = Vector([Xc, 0.0, 0.0])

            self.Ya = Ya
            self.Xb = Xb
            self.Xc = Xc
            self.Vertices = [A, B, C, ]
            return True

        # Scalene right angled (3, 4, 5)
        if (self.triangleType == 'SCALENERIGHTANGLE'):
            Ya = (1 * Ysign * scale)
            A = Vector([0.0, Ya, 0.0])
            Xb = 0
            B = Vector([Xb, 0.0, 0.0])
            Xc = (0.75 * Xsign * scale)
            C = Vector([Xc, 0.0, 0.0])

            self.Ya = Ya
            self.Xb = Xb
            self.Xc = Xc
            self.Vertices = [A, B, C, ]
            return True

        return False

    def addFaces(self, fType=None):
        Ya = self.Ya
        Xb = self.Xb
        Xc = self.Xc

        if (self.triangleFace == 'DEFAULT'):
            self.Faces = [[0, 1, 2]]
            return True

        if (self.triangleFace == 'TRIANGLES'):
            A = Vector([0.0, Ya, 0.0])
            B = Vector([Xb, 0.0, 0.0])
            C = Vector([Xc, 0.0, 0.0])
            D = Vector([((A.x + B.x + C.x) / 3), ((A.y + B.y + C.y) / 3), ((A.z + B.z + C.z) / 3)])

            self.Vertices = [A, B, C, D, ]
            self.Faces = [[0, 1, 3], [1, 2, 3], [2, 0, 3]]
            return True

        if (self.triangleFace == 'QUADS'):
            A = Vector([0.0, Ya, 0.0])
            B = Vector([Xb, 0.0, 0.0])
            C = Vector([Xc, 0.0, 0.0])
            D = Vector([((A.x + B.x + C.x) / 3), ((A.y + B.y + C.y) / 3), ((A.z + B.z + C.z) / 3)])
            AB = A.lerp(B, 0.5)
            AC = A.lerp(C, 0.5)
            BC = B.lerp(C, 0.5)

            self.Vertices = [A, AB, B, BC, C, AC, D, ]
            self.Faces = [[0, 1, 6, 5], [1, 2, 3, 6], [3, 4, 5, 6]]
            return True

        if (self.triangleFace == 'SAFEQUADS'):
            A = Vector([0.0, Ya, 0.0])
            B = Vector([Xb, 0.0, 0.0])
            C = Vector([Xc, 0.0, 0.0])
            D = Vector([((A.x + B.x + C.x) / 3), ((A.y + B.y + C.y) / 3), ((A.z + B.z + C.z) / 3)])
            E = A.lerp(D, 0.5)
            AB = A.lerp(B, 0.5)
            AC = A.lerp(C, 0.5)
            BC = B.lerp(C, 0.5)
            AAB = AB.lerp(A, 0.5)
            AAC = AC.lerp(A, 0.5)
            BBA = AB.lerp(B, 0.5)
            BBC = BC.lerp(B, 0.5)
            BCC = BC.lerp(C, 0.5)
            CCA = AC.lerp(C, 0.5)

            self.Vertices = [A, AAB, BBA, B, BBC, BC, BCC, C, CCA, AAC, D, E, ]
            self.Faces = [[0, 1, 11, 9], [1, 2, 10, 11], [2, 3, 4, 10],
                          [4, 5, 6, 10], [6, 7, 8, 10], [8, 9, 11, 10]]
            return True

        return False

    def action_common(self, context):
        # definitions:
        # a triangle consists of 3 points: A, B, C
        # a 'safer' subdividable triangle consists of 4 points: A, B, C, D
        # a subdivide friendly triangle consists of 7 points: A, B, C, D, AB, AC, BC
        # a truely subdivide friendly triangle consists of (3 x 4 = )12 points:
        # A, B, C, D, E, BC, AAB, AAC, BBA, BBC, BCC, CCA

        BasicShapeCreated = False
        ShapeFacesCreated = False
        go = 0

        #
        # call the functions for creating the triangles and test if successfull
        #
        BasicShapeCreated = self.drawBasicTriangleShape()
        if (BasicShapeCreated):
            ShapeFacesCreated = self.addFaces()
            if ShapeFacesCreated:
                go = 1

        if (go == 1):
            NewMesh = bpy.data.meshes.new("Triangle")
            NewMesh.from_pydata(self.Vertices, [], self.Faces)

            NewMesh.update()
            NewObj = bpy.data.objects.new("Triangle", NewMesh)
            context.scene.objects.link(NewObj)

            # before doing the deselect make sure edit mode isn't active
            exitEditMode()
            bpy.ops.object.select_all(action="DESELECT")
            NewObj.select = True
            context.scene.objects.active = NewObj

            if self.at_3Dcursor is True:
                # we'll need to be sure there is actually an object selected
                if NewObj.select is True:
                    # we also have to check if we're considered to be in 3D View (view3d)
                    if bpy.ops.view3d.snap_selected_to_cursor.poll() is True:
                        bpy.ops.view3d.snap_selected_to_cursor()
                    else:
                        # as we weren't considered to be in 3D View
                        # the object couldn't be moved to the 3D cursor
                        # so to avoid confusion we change the at_3Dcursor boolean to false
                        self.at_3Dcursor = False

        else:
            self.report({'WARNING'},
                        "Triangle could not be completed. (See Console for more Info)")

            print("\n[Add Mesh Extra Objects]\n\nModule: add_mesh_triangle")
            print("Triangle type: %s\n" % self.triangleType,
                  "Face type: %s\n" % self.triangleFace,
                  "Ya: %s, Xb: %s, Xc: %s\n" % (self.Ya, self.Xb, self.Xc),
                  "Vertices: %s\n" % self.Vertices,
                  "Faces: %s\n" % self.Faces)

    def execute(self, context):
        self.action_common(context)
        return {"FINISHED"}

    def invoke(self, context, event):
        self.action_common(context)
        return {"FINISHED"}
