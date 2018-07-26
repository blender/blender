# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Panel


class PHYSICS_PT_rigidbody_panel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"


class PHYSICS_PT_rigid_body(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Rigid Body"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbo = ob.rigid_body

        if rbo is not None:
            layout.prop(rbo, "type", text="Type")

            if rbo.type == 'ACTIVE':
                layout.prop(rbo, "mass")

            col = layout.column()
            if rbo.type == 'ACTIVE':
                col.prop(rbo, "enabled", text="Dynamic")
            col.prop(rbo, "kinematic", text="Animated")



class PHYSICS_PT_rigid_body_collisions(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Collisions"
    bl_parent_id = 'PHYSICS_PT_rigid_body'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body
        layout.use_property_split = True

        layout.prop(rbo, "collision_shape", text="Shape")

        if rbo.collision_shape in {'MESH', 'CONVEX_HULL'}:
            layout.prop(rbo, "mesh_source", text="Source")

        if rbo.collision_shape == 'MESH' and rbo.mesh_source == 'DEFORM':
            layout.prop(rbo, "use_deform", text="Deforming")


class PHYSICS_PT_rigid_body_collisions_surface(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Surface Response"
    bl_parent_id = 'PHYSICS_PT_rigid_body_collisions'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body
        layout.use_property_split = True

        col = layout.column()
        col.prop(rbo, "friction")
        col.prop(rbo, "restitution", text="Bounciness")

class PHYSICS_PT_rigid_body_collisions_sensitivity(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Sensitivity"
    bl_parent_id = 'PHYSICS_PT_rigid_body_collisions'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body
        layout.use_property_split = True

        col = layout.column()

        if rbo.collision_shape in {'MESH', 'CONE'}:
            col.prop(rbo, "collision_margin", text="Margin")
        else:
            col.prop(rbo, "use_margin")
            sub = col.column()
            sub.active = rbo.use_margin
            sub.prop(rbo, "collision_margin", text="Margin")

class PHYSICS_PT_rigid_body_collisions_collections(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Collision Collections"
    bl_parent_id = 'PHYSICS_PT_rigid_body_collisions'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body

        layout.prop(rbo, "collision_groups", text="")


class PHYSICS_PT_rigid_body_dynamics(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Dynamics"
    bl_parent_id = 'PHYSICS_PT_rigid_body'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                obj.rigid_body.type == 'ACTIVE' and
                (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbo = ob.rigid_body

        #col = layout.column(align=1)
        # col.label(text="Activation:")
        # XXX: settings such as activate on collison/etc.

        col = layout.column()
        col.prop(rbo, "linear_damping", text="Translation Damping")
        col.prop(rbo, "angular_damping", text="Rotation Damping")


class PHYSICS_PT_rigid_body_dynamics_deactivation(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Deactivation"
    bl_parent_id = 'PHYSICS_PT_rigid_body_dynamics'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                obj.rigid_body.type == 'ACTIVE' and
                (context.engine in cls.COMPAT_ENGINES))

    def draw_header(self, context):
        ob = context.object
        rbo = ob.rigid_body
        self.layout.prop(rbo, "use_deactivation", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbo = ob.rigid_body

        layout.active = rbo.use_deactivation

        col = layout.column()
        col.prop(rbo, "use_start_deactivated")
        col.prop(rbo, "deactivate_linear_velocity", text="Linear Velocity")
        col.prop(rbo, "deactivate_angular_velocity", text="Angular Velocity")
        # TODO: other params such as time?


classes = (
    PHYSICS_PT_rigid_body,
    PHYSICS_PT_rigid_body_collisions,
    PHYSICS_PT_rigid_body_collisions_surface,
    PHYSICS_PT_rigid_body_collisions_sensitivity,
    PHYSICS_PT_rigid_body_collisions_collections,
    PHYSICS_PT_rigid_body_dynamics,
    PHYSICS_PT_rigid_body_dynamics_deactivation,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
