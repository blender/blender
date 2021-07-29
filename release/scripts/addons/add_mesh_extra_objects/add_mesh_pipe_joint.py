# GPL # "author": "Buerbaum Martin (Pontiac)"

import bpy
from math import sin, cos, tan, pi, radians
from bpy.types import Operator
from bpy.props import (
        FloatProperty,
        IntProperty,
        )


# Create a new mesh (object) from verts/edges/faces.
# verts/edges/faces ... List of vertices/edges/faces for the
#                       new mesh (as used in from_pydata)
# name ... Name of the new mesh (& object)

def create_mesh_object(context, verts, edges, faces, name):
    # Create new mesh
    mesh = bpy.data.meshes.new(name)

    # Make a mesh from a list of verts/edges/faces.
    mesh.from_pydata(verts, edges, faces)

    # Update mesh geometry after adding stuff.
    mesh.update()

    from bpy_extras import object_utils
    return object_utils.object_data_add(context, mesh, operator=None)


# A very simple "bridge" tool.

def createFaces(vertIdx1, vertIdx2, closed=False, flipped=False):
    faces = []

    if not vertIdx1 or not vertIdx2:
        return None

    if len(vertIdx1) < 2 and len(vertIdx2) < 2:
        return None

    fan = False
    if (len(vertIdx1) != len(vertIdx2)):
        if (len(vertIdx1) == 1 and len(vertIdx2) > 1):
            fan = True
        else:
            return None

    total = len(vertIdx2)

    if closed:
        # Bridge the start with the end.
        if flipped:
            face = [
                vertIdx1[0],
                vertIdx2[0],
                vertIdx2[total - 1]]
            if not fan:
                face.append(vertIdx1[total - 1])
            faces.append(face)

        else:
            face = [vertIdx2[0], vertIdx1[0]]
            if not fan:
                face.append(vertIdx1[total - 1])
            face.append(vertIdx2[total - 1])
            faces.append(face)

    # Bridge the rest of the faces.
    for num in range(total - 1):
        if flipped:
            if fan:
                face = [vertIdx2[num], vertIdx1[0], vertIdx2[num + 1]]
            else:
                face = [vertIdx2[num], vertIdx1[num],
                    vertIdx1[num + 1], vertIdx2[num + 1]]
            faces.append(face)
        else:
            if fan:
                face = [vertIdx1[0], vertIdx2[num], vertIdx2[num + 1]]
            else:
                face = [vertIdx1[num], vertIdx2[num],
                    vertIdx2[num + 1], vertIdx1[num + 1]]
            faces.append(face)

    return faces


# Create the vertices and polygons for a simple elbow (bent pipe)

class AddElbowJoint(Operator):
    bl_idname = "mesh.primitive_elbow_joint_add"
    bl_label = "Add Pipe Elbow"
    bl_description = "Construct an elbow pipe mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    radius = FloatProperty(
        name="Radius",
        description="The radius of the pipe",
        default=1.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    div = IntProperty(
        name="Divisions",
        description="Number of vertices (divisions)",
        default=32, min=3, max=256
        )
    angle = FloatProperty(
        name="Angle",
        description="The angle of the branching pipe (i.e. the 'arm' - "
                    "Measured from the center line of the main pipe",
        default=radians(45.0),
        min=radians(-179.9),
        max=radians(179.9),
        unit="ROTATION"
        )
    startLength = FloatProperty(
        name="Length Start",
        description="Length of the beginning of the pipe",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    endLength = FloatProperty(
        name="End Length",
        description="Length of the end of the pipe",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )

    def execute(self, context):
        radius = self.radius
        div = self.div

        angle = self.angle

        startLength = self.startLength
        endLength = self.endLength

        verts = []
        faces = []

        loop1 = []        # The starting circle
        loop2 = []        # The elbow circle
        loop3 = []        # The end circle

        # Create start circle
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)
            locZ = -startLength
            loop1.append(len(verts))
            verts.append([locX * radius, locY * radius, locZ])

        # Create deformed joint circle
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)
            locZ = locX * tan(angle / 2.0)
            loop2.append(len(verts))
            verts.append([locX * radius, locY * radius, locZ * radius])

        # Create end circle
        baseEndLocX = -endLength * sin(angle)
        baseEndLocZ = endLength * cos(angle)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 - angle)
            locX = locX * sin(pi / 2.0 - angle)

            loop3.append(len(verts))
            # Translate and add circle vertices to the list.
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create faces
        faces.extend(createFaces(loop1, loop2, closed=True))
        faces.extend(createFaces(loop2, loop3, closed=True))

        base = create_mesh_object(context, verts, [], faces, "Elbow Joint")

        return {'FINISHED'}


# Create the vertices and polygons for a simple tee (T) joint
# The base arm of the T can be positioned in an angle if needed though

class AddTeeJoint(Operator):
    bl_idname = "mesh.primitive_tee_joint_add"
    bl_label = "Add Pipe Tee-Joint"
    bl_description = "Construct a tee-joint pipe mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    radius = FloatProperty(
        name="Radius",
        description="The radius of the pipe",
        default=1.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    div = IntProperty(
        name="Divisions",
        description="Number of vertices (divisions)",
        default=32,
        min=4,
        max=256
        )
    angle = FloatProperty(
        name="Angle",
        description="The angle of the branching pipe (i.e. the 'arm' - "
                    "Measured from the center line of the main pipe",
        default=radians(90.0),
        min=radians(0.1),
        max=radians(179.9),
        unit="ROTATION"
        )
    startLength = FloatProperty(
        name="Length Start",
        description="Length of the beginning of the"
                    " main pipe (the straight one)",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    endLength = FloatProperty(
        name="End Length",
        description="Length of the end of the"
                    " main pipe (the straight one)",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    branchLength = FloatProperty(
        name="Arm Length",
        description="Length of the arm pipe (the bent one)",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )

    def execute(self, context):
        radius = self.radius
        div = self.div

        angle = self.angle

        startLength = self.startLength
        endLength = self.endLength
        branchLength = self.branchLength

        if (div % 2):
            # Odd vertice number not supported (yet)
            self.report({'INFO'}, "Odd vertices number is not yet supported")
            return {'CANCELLED'}

        verts = []
        faces = []

        # List of vert indices of each cross section
        loopMainStart = []     # Vert indices for the beginning of the main pipe
        loopJoint1 = []        # Vert indices for joint that is used to connect the joint & loopMainStart
        loopJoint2 = []        # Vert indices for joint that is used to connect the joint & loopArm
        loopJoint3 = []        # Vert index for joint that is used to connect the joint & loopMainEnd
        loopArm = []           # Vert indices for the end of the arm
        loopMainEnd = []       # Vert indices for the end of the main pipe.

        # Create start circle (main pipe)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)
            locZ = -startLength
            loopMainStart.append(len(verts))
            verts.append([locX * radius, locY * radius, locZ])

        # Create deformed joint circle
        vertTemp1 = None
        vertTemp2 = None
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)

            if vertIdx == 0:
                vertTemp1 = len(verts)
            if vertIdx == div / 2:
                # @todo: This will possibly break if we
                # ever support odd divisions.
                vertTemp2 = len(verts)

            loopJoint1.append(len(verts))
            if (vertIdx < div / 2):
                # Straight side of main pipe.
                locZ = 0
                loopJoint3.append(len(verts))
            else:
                # Branching side
                locZ = locX * tan(angle / 2.0)
                loopJoint2.append(len(verts))

            verts.append([locX * radius, locY * radius, locZ * radius])

        # Create 2. deformed joint (half-)circle
        loopTemp = []
        for vertIdx in range(div):
            if (vertIdx > div / 2):
                curVertAngle = vertIdx * (2.0 * pi / div)
                locX = sin(curVertAngle)
                locY = -cos(curVertAngle)
                locZ = -(radius * locX * tan((pi - angle) / 2.0))
                loopTemp.append(len(verts))
                verts.append([locX * radius, locY * radius, locZ])

        loopTemp2 = loopTemp[:]

        # Finalise 2. loop
        loopTemp.reverse()
        loopTemp.append(vertTemp1)
        loopJoint2.reverse()
        loopJoint2.extend(loopTemp)
        loopJoint2.reverse()

        # Finalise 3. loop
        loopTemp2.append(vertTemp2)
        loopTemp2.reverse()
        loopJoint3.extend(loopTemp2)

        # Create end circle (branching pipe)
        baseEndLocX = -branchLength * sin(angle)
        baseEndLocZ = branchLength * cos(angle)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 - angle)
            locX = locX * sin(pi / 2.0 - angle)

            loopArm.append(len(verts))

            # Add translated circle.
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create end circle (main pipe)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)
            locZ = endLength
            loopMainEnd.append(len(verts))
            verts.append([locX * radius, locY * radius, locZ])

        # Create faces
        faces.extend(createFaces(loopMainStart, loopJoint1, closed=True))
        faces.extend(createFaces(loopJoint2, loopArm, closed=True))
        faces.extend(createFaces(loopJoint3, loopMainEnd, closed=True))

        base = create_mesh_object(context, verts, [], faces, "Tee Joint")

        return {'FINISHED'}


class AddWyeJoint(Operator):
    bl_idname = "mesh.primitive_wye_joint_add"
    bl_label = "Add Pipe Wye-Joint"
    bl_description = "Construct a wye-joint pipe mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    radius = FloatProperty(
        name="Radius",
        description="The radius of the pipe",
        default=1.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    div = IntProperty(
        name="Divisions",
        description="Number of vertices (divisions)",
        default=32,
        min=4,
        max=256
        )
    angle1 = FloatProperty(
        name="Angle 1",
        description="The angle of the 1. branching pipe "
                    "(measured from the center line of the main pipe)",
        default=radians(45.0),
        min=radians(-179.9),
        max=radians(179.9),
        unit="ROTATION"
        )
    angle2 = FloatProperty(
        name="Angle 2",
        description="The angle of the 2. branching pipe "
                    "(measured from the center line of the main pipe) ",
        default=radians(45.0),
        min=radians(-179.9),
        max=radians(179.9),
        unit="ROTATION"
        )
    startLength = FloatProperty(
        name="Length Start",
        description="Length of the beginning of the"
                    " main pipe (the straight one)",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    branch1Length = FloatProperty(
        name="Length Arm 1",
        description="Length of the 1. arm",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    branch2Length = FloatProperty(
        name="Length Arm 2",
        description="Length of the 2. arm",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )

    def execute(self, context):
        radius = self.radius
        div = self.div

        angle1 = self.angle1
        angle2 = self.angle2

        startLength = self.startLength
        branch1Length = self.branch1Length
        branch2Length = self.branch2Length

        if (div % 2):
            # Odd vertice number not supported (yet)
            self.report({'INFO'}, "Odd vertices number is not yet supported")
            return {'CANCELLED'}

        verts = []
        faces = []

        # List of vert indices of each cross section
        loopMainStart = []      # Vert indices for the beginning of the main pipe
        loopJoint1 = []         # Vert index for joint that is used to connect the joint & loopMainStart
        loopJoint2 = []         # Vert index for joint that is used to connect the joint & loopArm1
        loopJoint3 = []         # Vert index for joint that is used to connect the joint & loopArm2
        loopArm1 = []           # Vert idxs for end of the 1. arm
        loopArm2 = []           # Vert idxs for end of the 2. arm

        # Create start circle
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)
            locZ = -startLength
            loopMainStart.append(len(verts))
            verts.append([locX * radius, locY * radius, locZ])

        # Create deformed joint circle
        vertTemp1 = None
        vertTemp2 = None
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)

            if vertIdx == 0:
                vertTemp2 = len(verts)
            if vertIdx == div / 2:
                # @todo: This will possibly break if we
                # ever support odd divisions.
                vertTemp1 = len(verts)

            loopJoint1.append(len(verts))
            if (vertIdx > div / 2):
                locZ = locX * tan(angle1 / 2.0)
                loopJoint2.append(len(verts))
            else:
                locZ = locX * tan(-angle2 / 2.0)
                loopJoint3.append(len(verts))

            verts.append([locX * radius, locY * radius, locZ * radius])

        # Create 2. deformed joint (half-)circle
        loopTemp = []
        angleJoint = (angle2 - angle1) / 2.0
        for vertIdx in range(div):
            if (vertIdx > div / 2):
                curVertAngle = vertIdx * (2.0 * pi / div)

                locX = (-sin(curVertAngle) * sin(angleJoint) / sin(angle2 - angleJoint))
                locY = -cos(curVertAngle)
                locZ = (-(sin(curVertAngle) * cos(angleJoint) / sin(angle2 - angleJoint)))

                loopTemp.append(len(verts))
                verts.append([locX * radius, locY * radius, locZ * radius])

        loopTemp2 = loopTemp[:]

        # Finalise 2. loop
        loopTemp.append(vertTemp1)
        loopTemp.reverse()
        loopTemp.append(vertTemp2)
        loopJoint2.reverse()
        loopJoint2.extend(loopTemp)
        loopJoint2.reverse()

        # Finalise 3. loop
        loopTemp2.reverse()
        loopJoint3.extend(loopTemp2)

        # Create end circle (1. branching pipe)
        baseEndLocX = -branch1Length * sin(angle1)
        baseEndLocZ = branch1Length * cos(angle1)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 - angle1)
            locX = locX * sin(pi / 2.0 - angle1)

            loopArm1.append(len(verts))
            # Add translated circle.
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create end circle (2. branching pipe)
        baseEndLocX = branch2Length * sin(angle2)
        baseEndLocZ = branch2Length * cos(angle2)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 + angle2)
            locX = locX * sin(pi / 2.0 + angle2)

            loopArm2.append(len(verts))
            # Add translated circle
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create faces
        faces.extend(createFaces(loopMainStart, loopJoint1, closed=True))
        faces.extend(createFaces(loopJoint2, loopArm1, closed=True))
        faces.extend(createFaces(loopJoint3, loopArm2, closed=True))

        base = create_mesh_object(context, verts, [], faces, "Wye Joint")

        return {'FINISHED'}


# Create the vertices and polygons for a cross (+ or X) pipe joint

class AddCrossJoint(Operator):
    bl_idname = "mesh.primitive_cross_joint_add"
    bl_label = "Add Pipe Cross-Joint"
    bl_description = "Construct a cross-joint pipe mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    radius = FloatProperty(
        name="Radius",
        description="The radius of the pipe",
        default=1.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    div = IntProperty(
        name="Divisions",
        description="Number of vertices (divisions)",
        default=32,
        min=4,
        max=256
        )
    angle1 = FloatProperty(
        name="Angle 1",
        description="The angle of the 1. arm (from the main axis)",
        default=radians(90.0),
        min=radians(-179.9),
        max=radians(179.9),
        unit="ROTATION"
        )
    angle2 = FloatProperty(name="Angle 2",
        description="The angle of the 2. arm (from the main axis)",
        default=radians(90.0),
        min=radians(-179.9),
        max=radians(179.9),
        unit="ROTATION"
        )
    angle3 = FloatProperty(name="Angle 3 (center)",
        description="The angle of the center arm (from the main axis)",
        default=radians(0.0),
        min=radians(-179.9),
        max=radians(179.9),
        unit="ROTATION"
        )
    startLength = FloatProperty(
        name="Length Start",
        description="Length of the beginning of the "
                    "main pipe (the straight one)",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    branch1Length = FloatProperty(name="Length Arm 1",
        description="Length of the 1. arm",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    branch2Length = FloatProperty(
        name="Length Arm 2",
        description="Length of the 2. arm",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    branch3Length = FloatProperty(
        name="Length Arm 3 (center)",
        description="Length of the center arm",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )

    def execute(self, context):
        radius = self.radius
        div = self.div

        angle1 = self.angle1
        angle2 = self.angle2
        angle3 = self.angle3

        startLength = self.startLength
        branch1Length = self.branch1Length
        branch2Length = self.branch2Length
        branch3Length = self.branch3Length
        if (div % 2):
            # Odd vertice number not supported (yet)
            self.report({'INFO'}, "Odd vertices number is not yet supported")
            return {'CANCELLED'}

        verts = []
        faces = []

        # List of vert indices of each cross section
        loopMainStart = []      # Vert indices for the beginning of the main pipe
        loopJoint1 = []         # Vert index for joint that is used to connect the joint & loopMainStart
        loopJoint2 = []         # Vert index for joint that is used to connect the joint & loopArm1
        loopJoint3 = []         # Vert index for joint that is used to connect the joint & loopArm2
        loopJoint4 = []         # Vert index for joint that is used to connect the joint & loopArm3
        loopArm1 = []           # Vert idxs for the end of the 1. arm
        loopArm2 = []           # Vert idxs for the end of the 2. arm
        loopArm3 = []           # Vert idxs for the center arm end

        # Create start circle
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)
            locZ = -startLength
            loopMainStart.append(len(verts))
            verts.append([locX * radius, locY * radius, locZ])

        # Create 1. deformed joint circle
        vertTemp1 = None
        vertTemp2 = None
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            locX = sin(curVertAngle)
            locY = cos(curVertAngle)

            if vertIdx == 0:
                vertTemp2 = len(verts)
            if vertIdx == div / 2:
                # @todo: This will possibly break if we
                # ever support odd divisions.
                vertTemp1 = len(verts)

            loopJoint1.append(len(verts))
            if (vertIdx > div / 2):
                locZ = locX * tan(angle1 / 2.0)
                loopJoint2.append(len(verts))
            else:
                locZ = locX * tan(-angle2 / 2.0)
                loopJoint3.append(len(verts))

            verts.append([locX * radius, locY * radius, locZ * radius])

        # Create 2. deformed joint circle
        loopTempA = []
        loopTempB = []
        angleJoint1 = (angle1 - angle3) / 2.0
        angleJoint2 = (angle2 + angle3) / 2.0
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)

            # Skip pole vertices
            # @todo: This will possibly break if
            # we ever support odd divisions
            if not (vertIdx == 0) and not (vertIdx == div / 2):

                if (vertIdx > div / 2):
                    angleJoint = angleJoint1
                    angle = angle1
                    Z = -1.0
                    loopTempA.append(len(verts))

                else:
                    angleJoint = angleJoint2
                    angle = angle2
                    Z = 1.0
                    loopTempB.append(len(verts))

                locX = (sin(curVertAngle) * sin(angleJoint) / sin(angle - angleJoint))
                locY = -cos(curVertAngle)
                locZ = (Z * (sin(curVertAngle) * cos(angleJoint) / sin(angle - angleJoint)))

                verts.append([locX * radius, locY * radius, locZ * radius])

        loopTempA2 = loopTempA[:]
        loopTempB2 = loopTempB[:]
        loopTempB3 = loopTempB[:]

        # Finalise 2. loop
        loopTempA.append(vertTemp1)
        loopTempA.reverse()
        loopTempA.append(vertTemp2)
        loopJoint2.reverse()
        loopJoint2.extend(loopTempA)
        loopJoint2.reverse()

        # Finalise 3. loop
        loopJoint3.extend(loopTempB3)

        # Finalise 4. loop
        loopTempA2.append(vertTemp1)
        loopTempA2.reverse()
        loopTempB2.append(vertTemp2)
        loopJoint4.extend(reversed(loopTempB2))
        loopJoint4.extend(loopTempA2)

        # Create end circle (1. branching pipe)
        baseEndLocX = -branch1Length * sin(angle1)
        baseEndLocZ = branch1Length * cos(angle1)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 - angle1)
            locX = locX * sin(pi / 2.0 - angle1)

            loopArm1.append(len(verts))
            # Add translated circle.
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create end circle (2. branching pipe)
        baseEndLocX = branch2Length * sin(angle2)
        baseEndLocZ = branch2Length * cos(angle2)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 + angle2)
            locX = locX * sin(pi / 2.0 + angle2)

            loopArm2.append(len(verts))
            # Add translated circle
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create end circle (center pipe)
        baseEndLocX = branch3Length * sin(angle3)
        baseEndLocZ = branch3Length * cos(angle3)
        for vertIdx in range(div):
            curVertAngle = vertIdx * (2.0 * pi / div)
            # Create circle
            locX = sin(curVertAngle) * radius
            locY = cos(curVertAngle) * radius
            locZ = 0.0

            # Rotate circle
            locZ = locX * cos(pi / 2.0 + angle3)
            locX = locX * sin(pi / 2.0 + angle3)

            loopArm3.append(len(verts))
            # Add translated circle
            verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

        # Create faces
        faces.extend(createFaces(loopMainStart, loopJoint1, closed=True))
        faces.extend(createFaces(loopJoint2, loopArm1, closed=True))
        faces.extend(createFaces(loopJoint3, loopArm2, closed=True))
        faces.extend(createFaces(loopJoint4, loopArm3, closed=True))

        base = create_mesh_object(context, verts, [], faces, "Cross Joint")

        return {'FINISHED'}


# Create the vertices and polygons for a regular n-joint

class AddNJoint(Operator):
    bl_idname = "mesh.primitive_n_joint_add"
    bl_label = "Add Pipe N-Joint"
    bl_description = "Construct a n-joint pipe mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    radius = FloatProperty(
        name="Radius",
        description="The radius of the pipe",
        default=1.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )
    div = IntProperty(
        name="Divisions",
        description="Number of vertices (divisions)",
        default=32,
        min=4,
        max=256
        )
    number = IntProperty(
        name="Arms / Joints",
        description="Number of joints / arms",
        default=5,
        min=2,
        max=99999
        )
    length = FloatProperty(
        name="Length",
        description="Length of each joint / arm",
        default=3.0,
        min=0.01,
        max=100.0,
        unit="LENGTH"
        )

    def execute(self, context):
        radius = self.radius
        div = self.div
        number = self.number
        length = self.length

        if (div % 2):
            # Odd vertice number not supported (yet)
            self.report({'INFO'}, "Odd vertices number is not yet supported")
            return {'CANCELLED'}

        if (number < 2):
            return {'CANCELLED'}

        verts = []
        faces = []

        loopsEndCircles = []
        loopsJointsTemp = []
        loopsJoints = []

        vertTemp1 = None
        vertTemp2 = None

        angleDiv = (2.0 * pi / number)

        # Create vertices for the end circles
        for num in range(number):
            circle = []
            # Create start circle
            angle = num * angleDiv

            baseEndLocX = length * sin(angle)
            baseEndLocZ = length * cos(angle)
            for vertIdx in range(div):
                curVertAngle = vertIdx * (2.0 * pi / div)
                # Create circle
                locX = sin(curVertAngle) * radius
                locY = cos(curVertAngle) * radius
                locZ = 0.0

                # Rotate circle
                locZ = locX * cos(pi / 2.0 + angle)
                locX = locX * sin(pi / 2.0 + angle)

                circle.append(len(verts))
                # Add translated circle
                verts.append([baseEndLocX + locX, locY, baseEndLocZ + locZ])

            loopsEndCircles.append(circle)

            # Create vertices for the joint circles
            loopJoint = []
            for vertIdx in range(div):
                curVertAngle = vertIdx * (2.0 * pi / div)
                locX = sin(curVertAngle)
                locY = cos(curVertAngle)

                skipVert = False
                # Store pole vertices
                if vertIdx == 0:
                    if (num == 0):
                        vertTemp2 = len(verts)
                    else:
                        skipVert = True
                elif vertIdx == div / 2:
                    # @todo: This will possibly break if we
                    # ever support odd divisions
                    if (num == 0):
                        vertTemp1 = len(verts)
                    else:
                        skipVert = True

                if not skipVert:
                    if (vertIdx > div / 2):
                        locZ = -locX * tan((pi - angleDiv) / 2.0)
                        loopJoint.append(len(verts))

                        # Rotate the vert
                        cosAng = cos(-angle)
                        sinAng = sin(-angle)
                        LocXnew = locX * cosAng - locZ * sinAng
                        LocZnew = locZ * cosAng + locX * sinAng
                        locZ = LocZnew
                        locX = LocXnew

                        verts.append([
                            locX * radius,
                            locY * radius,
                            locZ * radius])
                    else:
                        # These two vertices will only be
                        # added the very first time.
                        if vertIdx == 0 or vertIdx == div / 2:
                            verts.append([locX * radius, locY * radius, locZ])

            loopsJointsTemp.append(loopJoint)

        # Create complete loops (loopsJoints) out of the
        # double number of half loops in loopsJointsTemp
        for halfLoopIdx in range(len(loopsJointsTemp)):
            if (halfLoopIdx == len(loopsJointsTemp) - 1):
                idx1 = halfLoopIdx
                idx2 = 0
            else:
                idx1 = halfLoopIdx
                idx2 = halfLoopIdx + 1

            loopJoint = []
            loopJoint.append(vertTemp2)
            loopJoint.extend(reversed(loopsJointsTemp[idx2]))
            loopJoint.append(vertTemp1)
            loopJoint.extend(loopsJointsTemp[idx1])

            loopsJoints.append(loopJoint)

        # Create faces from the two
        # loop arrays (loopsJoints -> loopsEndCircles)
        for loopIdx in range(len(loopsEndCircles)):
            faces.extend(
                createFaces(loopsJoints[loopIdx],
                loopsEndCircles[loopIdx], closed=True))

        base = create_mesh_object(context, verts, [], faces, "N Joint")

        return {'FINISHED'}
