# gpl: author Folkert de Vries

bl_info = {
    "name": "Surface: Plane / Cone/ Star / Wedge",
    "description": "Create a NURBS surface plane",
    "author": "Folkert de Vries",
    "version": (1, 0, 1),
    "blender": (2, 5, 9),
    "location": "View3D > Add > Surface",
    "warning": "",
    "wiki_url": "",
    "category": "Add Mesh"
}

"""
Info:
to add a surface star, plane or cone, go to add Menu > Surface > Star, Plane or Cone
next parameters like scale and u and v resolution can be adjusted in the toolshelf

have fun using this add-on
"""

import bpy
from bpy.props import (
        FloatProperty,
        IntProperty,
        )
from bpy.types import Operator


# generic class for inheritance
class MakeSurfaceHelpers:
    # get input for size and resolution
    size = FloatProperty(
            name="Size",
            description="Size of the object",
            default=1.0,
            min=0.01,
            max=100.0,
            unit="LENGTH",
            )
    res_u = IntProperty(
            name="Resolution U",
            description="Surface resolution in u direction",
            default=1,
            min=1,
            max=500,
            )
    res_v = IntProperty(
            name="Resolution V",
            description="Surface resolution in v direction",
            default=1,
            min=1,
            max=500,
            )

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "size")

        col = layout.column(align=True)
        col.prop(self, "res_u")
        col.prop(self, "res_v")


class MakeSurfaceWedge(Operator, MakeSurfaceHelpers):
    bl_idname = "object.add_surface_wedge"
    bl_label = "Add Surface Wedge"
    bl_description = "Construct a Surface Wedge"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # variables
        size = self.size
        res_u = self.res_u
        res_v = self.res_v

        # add a surface Plane
        bpy.ops.object.add_surface_plane()
        # save some time, by getting instant acces to those values
        ao = context.active_object
        point = ao.data.splines[0].points

        # rotate 90 degrees on the z axis
        ao.rotation_euler[0] = 0.0
        ao.rotation_euler[1] = 0.0
        ao.rotation_euler[2] = 1.570796

        # go into edit mode and deselect
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.curve.select_all(action='DESELECT')

        # select points 0 and 1, and extrudde them
        # declaring ao and point again seems necesary...
        ao = context.active_object
        point = ao.data.splines[0].points
        point[0].select = True
        ao = context.active_object
        point = ao.data.splines[0].points
        point[1].select = True
        bpy.ops.curve.extrude()
        # bring extruded points up 1 bu on the z axis, and toggle
        # cyclic in V direction
        bpy.ops.transform.translate(value=(0, 0, 1), constraint_axis=(False, False, True))
        bpy.ops.curve.cyclic_toggle(direction='CYCLIC_V')

        # get points to the right coords.
        point[0].co = (1.0, 0.0, 1.0, 1.0)
        point[1].co = (-1.0, 0.0, 1.0, 1.0)
        point[2].co = (1.0, -0.5, 0.0, 1.0)
        point[3].co = (-1.0, -0.5, 0.0, 1.0)
        point[4].co = (1.0, 0.5, 0.0, 1.0)
        point[5].co = (-1.0, 0.5, 0.0, 1.0)

        # go back to object mode
        bpy.ops.object.mode_set(mode='OBJECT')
        # get origin to geometry.
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='MEDIAN')
        # change name
        context.active_object.name = 'SurfaceWedge'
        # get the wedge to the 3d cursor.
        context.active_object.location = context.scene.cursor_location
        bpy.ops.transform.resize(value=(size, size, size))

        # adjust resolution in u and v direction
        context.active_object.data.resolution_u = res_u
        context.active_object.data.resolution_v = res_v

        return{'FINISHED'}


class MakeSurfaceCone(Operator, MakeSurfaceHelpers):
    bl_idname = "object.add_surface_cone"
    bl_label = "Add Surface Cone"
    bl_description = "Construct a Surface Cone"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        size = self.size
        res_u = self.res_u
        res_v = self.res_v

        # add basemesh, a nurbs torus
        bpy.ops.surface.primitive_nurbs_surface_torus_add(location=(0, 0, 0))
        # get active object and active object name

        ao = context.active_object

        # go to edit mode
        bpy.ops.object.mode_set(mode='EDIT')
        # deselect all
        bpy.ops.curve.select_all(action='DESELECT')
        # too shorten alot of lines
        point = ao.data.splines[0].points
        # get middle points

        for i in range(0, 63):
            if point[i].co.z == 0.0:
                point[i].select = True

        # select non-middle points and delete them
        bpy.ops.curve.select_all(action='INVERT')
        bpy.ops.curve.delete(type='VERT')
        # declaring this again seems necesary...
        point = ao.data.splines[0].points
        # list of points to be in center, and 2 bu'' s higher

        ToKeep = [1, 3, 5, 7, 9, 11, 13, 15, 17]
        for i in range(0, len(ToKeep) - 1):
            point[ToKeep[i]].select = True

        bpy.ops.transform.resize(value=(0, 0, 0))
        bpy.ops.curve.cyclic_toggle(direction='CYCLIC_U')
        bpy.ops.transform.translate(value=(0, 0, 2))

        # to make cone visible
        bpy.ops.object.editmode_toggle()
        bpy.ops.object.editmode_toggle()
        # change name
        context.active_object.name = 'SurfaceCone'
        # go back to object mode
        bpy.ops.object.editmode_toggle()
        # bring object to cursor
        bpy.ops.object.mode_set(mode='OBJECT')
        context.active_object.location = context.scene.cursor_location
        # adjust size
        bpy.ops.transform.resize(value=(size, size, size))

        # adjust resolution in u and v direction
        context.active_object.data.resolution_u = res_u
        context.active_object.data.resolution_v = res_v

        return{'FINISHED'}


class MakeSurfaceStar(Operator, MakeSurfaceHelpers):
    bl_idname = "object.add_surface_star"
    bl_label = "Add Surface Star"
    bl_description = "Contruct a Surface Star"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        size = self.size
        res_u = self.res_u
        res_v = self.res_v

        # add surface circle:
        bpy.ops.surface.primitive_nurbs_surface_circle_add(location=(0, 0, 0))
        # we got 8 points, we need 40 points.
        # get active object
        ao = context.active_object
        # enter edtimode
        bpy.ops.object.mode_set(mode='EDIT')
        # deselect all
        bpy.ops.curve.select_all(action='DESELECT')
        # select point 0 and 1, and subdivide
        point = ao.data.splines[0].points

        point[0].select = True
        point[1].select = True
        bpy.ops.curve.subdivide()
        bpy.ops.curve.select_all(action='DESELECT')

        # select point 2 and 3, and subdivide
        point[2].select = True
        point[3].select = True
        bpy.ops.curve.subdivide()
        bpy.ops.curve.select_all(action='DESELECT')

        ListOfCoords = [
                (0.5, 0.0, 0.25, 1.0),
                (0.80901700258255, 0.5877853035926819, 0.25, 1.0),
                (0.1545085906982422, 0.4755282402038574, 0.25, 1.0),
                (-0.30901703238487244, 0.9510565400123596, 0.25, 1.0),
                (-0.4045085608959198, 0.293892502784729, 0.2499999850988388, 1.0),
                (-1.0, 0.0, 0.25, 1.0),
                (-0.4045085608959198, -0.293892502784729, 0.2499999850988388, 1.0),
                (-0.30901703238487244, -0.9510565400123596, 0.25, 1.0),
                (0.1545085906982422, -0.4755282402038574, 0.25, 1.0),
                (0.8090166449546814, -0.5877856612205505, 0.2499999850988388, 1.0)
                ]
        for i in range(0, 10):
            context.active_object.data.splines[0].points[i].co = ListOfCoords[i]

        # now select all, and subdivide till 40 points is reached:
        bpy.ops.curve.select_all(action='SELECT')
        bpy.ops.curve.subdivide()
        bpy.ops.curve.subdivide()
        bpy.ops.curve.subdivide()

        # extrude the star
        bpy.ops.curve.extrude(mode='TRANSLATION')
        # bring extruded part up
        bpy.ops.transform.translate(
                value=(0, 0, 0.5),
                constraint_axis=(False, False, True)
                )
        # flip normals
        bpy.ops.curve.switch_direction()
        # go back to object mode
        bpy.ops.object.mode_set(mode='OBJECT')
        # origin to geometry
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='MEDIAN')
        # get object to 3d cursor
        context.active_object.location = context.scene.cursor_location
        # change name
        ao.name = 'SurfaceStar'
        # adjust size
        bpy.ops.transform.resize(value=(size, size, size))

        # adjust resolution in u and v direction
        context.active_object.data.resolution_u = res_u
        context.active_object.data.resolution_v = res_v

        return{'FINISHED'}


class MakeSurfacePlane(Operator, MakeSurfaceHelpers):
    bl_idname = "object.add_surface_plane"
    bl_label = "Add Surface Plane"
    bl_description = "Contruct a Surface Plane"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        size = self.size
        res_u = self.res_u
        res_v = self.res_v

        bpy.ops.surface.primitive_nurbs_surface_surface_add()  # add the base mesh, a NURBS Surface

        bpy.ops.transform.resize(
                value=(1, 1, 0.0001),
                constraint_axis=(False, False, True)
                )  # make it flat

        # added surface has 16 points

        # deleting points to get plane shape
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.curve.select_all(action='DESELECT')

        context.active_object.data.splines[0].points[0].select = True
        context.active_object.data.splines[0].points[1].select = True
        context.active_object.data.splines[0].points[2].select = True
        context.active_object.data.splines[0].points[3].select = True
        bpy.ops.curve.delete(type='VERT')

        context.active_object.data.splines[0].points[8].select = True
        context.active_object.data.splines[0].points[9].select = True
        context.active_object.data.splines[0].points[10].select = True
        context.active_object.data.splines[0].points[11].select = True
        bpy.ops.curve.delete(type='VERT')

        context.active_object.data.splines[0].points[0].select = True
        context.active_object.data.splines[0].points[4].select = True
        bpy.ops.curve.delete(type='VERT')
        context.active_object.data.splines[0].points[2].select = True
        context.active_object.data.splines[0].points[5].select = True
        bpy.ops.curve.delete(type='VERT')

        # assigning name
        context.active_object.name = "SurfacePlane"
        # select all
        bpy.ops.curve.select_all(action='SELECT')
        # bringing origin to center:
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='MEDIAN')
        # transform scale
        bpy.ops.object.transform_apply(scale=True)

        # bring object to 3d cursor
        bpy.ops.object.mode_set(mode='OBJECT')
        context.active_object.location = context.scene.cursor_location
        bpy.ops.transform.resize(value=(size, size, size))

        # adjust resolution in u and v direction
        context.active_object.data.resolution_u = res_u
        context.active_object.data.resolution_v = res_v

        return{'FINISHED'}


class SmoothXtimes(Operator):
    bl_idname = "curve.smooth_x_times"
    bl_label = "Smooth X Times"
    bl_space_type = "VIEW_3D"
    bl_options = {'REGISTER', 'UNDO'}

    # use of this class:
    # lets you smooth till a thousand times. this is normally difficult, because
    # you have to press w, click, press w, click etc.

    # get values
    times = IntProperty(
            name="Smooth x Times",
            min=1,
            max=1000,
            default=1,
            description="Smooth amount"
            )

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_SURFACE'

    def execute(self, context):
        # smooth times
        times = self.times
        for i in range(1, times):
            bpy.ops.curve.smooth()

        return{'FINISHED'}


def register():
    bpy.utils.register_class(MakeSurfaceHelpers)
    bpy.utils.register_class(MakeSurfacePlane)
    bpy.utils.register_class(MakeSurfaceCone)
    bpy.utils.register_class(MakeSurfaceStar)
    bpy.utils.register_class(MakeSurfaceWedge)
    bpy.utils.register_class(SmoothXtimes)


def unregister():
    bpy.utils.unregister_class(MakeSurfaceHelpers)
    bpy.utils.unregister_class(MakeSurfacePlane)
    bpy.utils.unregister_class(MakeSurfaceCone)
    bpy.utils.unregister_class(MakeSurfaceStar)
    bpy.utils.unregister_class(MakeSurfaceWedge)
    bpy.utils.unregister_class(SmoothXtimes)


if __name__ == "__main__":
    register()
