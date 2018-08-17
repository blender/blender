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
from bpy.types import (
    Panel,
)


class PHYSICS_PT_rigidbody_constraint_panel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"


class PHYSICS_PT_rigid_body_constraint(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Rigid Body Constraint"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.rigid_body_constraint and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbc = ob.rigid_body_constraint

        layout.prop(rbc, "type")


class PHYSICS_PT_rigid_body_constraint_settings(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Settings"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.rigid_body_constraint and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        rbc = ob.rigid_body_constraint

        col = flow.column()
        col.prop(rbc, "enabled")
        col.prop(rbc, "disable_collisions")

        if rbc.type != 'MOTOR':
            col = flow.column()
            col.prop(rbc, "use_breaking")

            sub = col.column()
            sub.active = rbc.use_breaking
            sub.prop(rbc, "breaking_threshold", text="Threshold")


class PHYSICS_PT_rigid_body_constraint_objects(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Objects"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.rigid_body_constraint and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbc = ob.rigid_body_constraint

        layout.prop(rbc, "object1", text="First")
        layout.prop(rbc, "object2", text="Second")


class PHYSICS_PT_rigid_body_constraint_override_iterations(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Override Iterations"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.rigid_body_constraint and context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        ob = context.object
        rbc = ob.rigid_body_constraint
        self.layout.row().prop(rbc, "use_override_solver_iterations", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbc = ob.rigid_body_constraint

        layout.active = rbc.use_override_solver_iterations
        layout.prop(rbc, "solver_iterations", text="Iterations")


class PHYSICS_PT_rigid_body_constraint_limits(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Limits"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and rbc and (rbc.type in {'GENERIC', 'GENERIC_SPRING', 'HINGE', 'SLIDER', 'PISTON'})
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        return  # do nothing.


class PHYSICS_PT_rigid_body_constraint_limits_linear(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Linear"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint_limits'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and rbc
                and (rbc.type in {'GENERIC', 'GENERIC_SPRING', 'SLIDER', 'PISTON'})
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        rbc = ob.rigid_body_constraint

        if rbc.type in {'PISTON', 'SLIDER'}:
            col = flow.column()
            col.prop(rbc, "use_limit_lin_x")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_lin_x
            sub.prop(rbc, "limit_lin_x_lower", text="X Lower")
            sub.prop(rbc, "limit_lin_x_upper", text="Upper")

        elif rbc.type in {'GENERIC', 'GENERIC_SPRING'}:
            col = flow.column()
            col.prop(rbc, "use_limit_lin_x")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_lin_x
            sub.prop(rbc, "limit_lin_x_lower", text="X Lower")
            sub.prop(rbc, "limit_lin_x_upper", text="Upper")

            col = flow.column()
            col.prop(rbc, "use_limit_lin_y")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_lin_y
            sub.prop(rbc, "limit_lin_y_lower", text="Y Lower")
            sub.prop(rbc, "limit_lin_y_upper", text="Upper")

            col = flow.column()
            col.prop(rbc, "use_limit_lin_z")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_lin_z
            sub.prop(rbc, "limit_lin_z_lower", text="Z Lower")
            sub.prop(rbc, "limit_lin_z_upper", text="Upper")


class PHYSICS_PT_rigid_body_constraint_limits_angular(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Angular"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint_limits'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and rbc
                and (rbc.type in {'GENERIC_SPRING', 'HINGE', 'PISTON'})
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        rbc = ob.rigid_body_constraint

        if rbc.type == 'HINGE':
            col = flow.column()
            col.prop(rbc, "use_limit_ang_z")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_ang_z
            sub.prop(rbc, "limit_ang_z_lower", text="Z Lower")
            sub.prop(rbc, "limit_ang_z_upper", text="Upper")

        elif rbc.type == 'PISTON':
            col = flow.column()
            col.prop(rbc, "use_limit_ang_x")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_ang_x
            sub.prop(rbc, "limit_ang_x_lower", text="X Lower")
            sub.prop(rbc, "limit_ang_x_upper", text="Upper")

        elif rbc.type == 'GENERIC_SPRING':
            col = flow.column()
            col.prop(rbc, "use_limit_ang_x")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_ang_x
            sub.prop(rbc, "limit_ang_x_lower", text="X Lower")
            sub.prop(rbc, "limit_ang_x_upper", text="Upper")

            col = flow.column()
            col.prop(rbc, "use_limit_ang_y")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_ang_y
            sub.prop(rbc, "limit_ang_y_lower", text="Y Lower")
            sub.prop(rbc, "limit_ang_y_upper", text="Upper")

            col = flow.column()
            col.prop(rbc, "use_limit_ang_z")

            sub = col.column(align=True)
            sub.active = rbc.use_limit_ang_z
            sub.prop(rbc, "limit_ang_z_lower", text="Z Lower")
            sub.prop(rbc, "limit_ang_z_upper", text="Upper")


class PHYSICS_PT_rigid_body_constraint_motor(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Motor"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and rbc and rbc.type == 'MOTOR'
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        return  # do nothing.


class PHYSICS_PT_rigid_body_constraint_motor_angular(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Angular"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint_motor'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and rbc and rbc.type == 'MOTOR'
                and context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        self.layout.row().prop(rbc, "use_motor_ang", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        rbc = ob.rigid_body_constraint

        flow.active = rbc.use_motor_ang

        col = flow.column(align=True)
        col.prop(rbc, "motor_ang_target_velocity", text="Target Velocity")

        col = flow.column(align=True)
        col.prop(rbc, "motor_ang_max_impulse", text="Max Impulse")


class PHYSICS_PT_rigid_body_constraint_motor_linear(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Linear"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint_motor'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and rbc and rbc.type == 'MOTOR'
                and context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        self.layout.row().prop(rbc, "use_motor_lin", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        rbc = ob.rigid_body_constraint

        flow.active = rbc.use_motor_lin

        col = flow.column(align=True)
        col.prop(rbc, "motor_lin_target_velocity", text="Target Velocity")

        col = flow.column(align=True)
        col.prop(rbc, "motor_lin_max_impulse", text="Max Impulse")


class PHYSICS_PT_rigid_body_constraint_springs(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Springs"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and ob.rigid_body_constraint
                and rbc.type in {'GENERIC_SPRING'}
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbc = ob.rigid_body_constraint

        layout.prop(rbc, "spring_type", text="Type")


class PHYSICS_PT_rigid_body_constraint_springs_angular(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Angular"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint_springs'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and ob.rigid_body_constraint
                and rbc.type in {'GENERIC_SPRING'}
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbc = ob.rigid_body_constraint

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(rbc, "use_spring_ang_x", text="X Angle")

        sub = col.column(align=True)
        sub.active = rbc.use_spring_ang_x
        sub.prop(rbc, "spring_stiffness_ang_x", text="X Stiffness")
        sub.prop(rbc, "spring_damping_ang_x", text="Damping")

        col = flow.column()
        col.prop(rbc, "use_spring_ang_y", text="Y Angle")

        sub = col.column(align=True)
        sub.active = rbc.use_spring_ang_y
        sub.prop(rbc, "spring_stiffness_ang_y", text="Y Stiffness")
        sub.prop(rbc, "spring_damping_ang_y", text="Damping")

        col = flow.column()
        col.prop(rbc, "use_spring_ang_z", text="Z Angle")

        sub = col.column(align=True)
        sub.active = rbc.use_spring_ang_z
        sub.prop(rbc, "spring_stiffness_ang_z", text="Z Stiffness")
        sub.prop(rbc, "spring_damping_ang_z", text="Damping")


class PHYSICS_PT_rigid_body_constraint_springs_linear(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Linear"
    bl_parent_id = 'PHYSICS_PT_rigid_body_constraint_springs'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        rbc = ob.rigid_body_constraint

        return (ob and ob.rigid_body_constraint
                and rbc.type in {'GENERIC_SPRING'}
                and context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        rbc = ob.rigid_body_constraint

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(rbc, "use_spring_x", text="X Axis")

        sub = col.column(align=True)
        sub.active = rbc.use_spring_x
        sub.prop(rbc, "spring_stiffness_x", text="X Stiffness")
        sub.prop(rbc, "spring_damping_x", text="Damping")

        col = flow.column()
        col.prop(rbc, "use_spring_y", text="Y Axis")

        sub = col.column(align=True)
        sub.active = rbc.use_spring_y
        sub.prop(rbc, "spring_stiffness_y", text="Stiffness")
        sub.prop(rbc, "spring_damping_y", text="Damping")

        col = flow.column()
        col.prop(rbc, "use_spring_z", text="Z Axis")

        sub = col.column(align=True)
        sub.active = rbc.use_spring_z
        sub.prop(rbc, "spring_stiffness_z", text="Stiffness")
        sub.prop(rbc, "spring_damping_z", text="Damping")


classes = (
    PHYSICS_PT_rigid_body_constraint,
    PHYSICS_PT_rigid_body_constraint_settings,
    PHYSICS_PT_rigid_body_constraint_limits,
    PHYSICS_PT_rigid_body_constraint_limits_angular,
    PHYSICS_PT_rigid_body_constraint_limits_linear,
    PHYSICS_PT_rigid_body_constraint_motor,
    PHYSICS_PT_rigid_body_constraint_motor_angular,
    PHYSICS_PT_rigid_body_constraint_motor_linear,
    PHYSICS_PT_rigid_body_constraint_objects,
    PHYSICS_PT_rigid_body_constraint_override_iterations,
    PHYSICS_PT_rigid_body_constraint_springs,
    PHYSICS_PT_rigid_body_constraint_springs_angular,
    PHYSICS_PT_rigid_body_constraint_springs_linear,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
