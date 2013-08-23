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


class PHYSICS_PT_rigidbody_constraint_panel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"


class PHYSICS_PT_rigid_body_constraint(PHYSICS_PT_rigidbody_constraint_panel, Panel):
    bl_label = "Rigid Body Constraint"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.rigid_body_constraint and (not rd.use_game_engine))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbc = ob.rigid_body_constraint

        layout.prop(rbc, "type")

        row = layout.row()
        row.prop(rbc, "enabled")
        row.prop(rbc, "disable_collisions")

        layout.prop(rbc, "object1")
        layout.prop(rbc, "object2")

        if rbc.type != 'MOTOR':
            row = layout.row()
            row.prop(rbc, "use_breaking")
            sub = row.row()
            sub.active = rbc.use_breaking
            sub.prop(rbc, "breaking_threshold", text="Threshold")

        row = layout.row()
        row.prop(rbc, "use_override_solver_iterations", text="Override Iterations")
        sub = row.row()
        sub.active = rbc.use_override_solver_iterations
        sub.prop(rbc, "solver_iterations", text="Iterations")

        if rbc.type == 'HINGE':
            col = layout.column(align=True)
            col.label("Limits:")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_ang_z", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_ang_z
            sub.prop(rbc, "limit_ang_z_lower", text="Lower")
            sub.prop(rbc, "limit_ang_z_upper", text="Upper")

        elif rbc.type == 'SLIDER':
            col = layout.column(align=True)
            col.label("Limits:")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_lin_x", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_lin_x
            sub.prop(rbc, "limit_lin_x_lower", text="Lower")
            sub.prop(rbc, "limit_lin_x_upper", text="Upper")

        elif rbc.type == 'PISTON':
            col = layout.column(align=True)
            col.label("Limits:")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_lin_x", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_lin_x
            sub.prop(rbc, "limit_lin_x_lower", text="Lower")
            sub.prop(rbc, "limit_lin_x_upper", text="Upper")

            col = layout.column(align=True)

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_ang_x", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_ang_x
            sub.prop(rbc, "limit_ang_x_lower", text="Lower")
            sub.prop(rbc, "limit_ang_x_upper", text="Upper")

        elif rbc.type == 'MOTOR':
            col = layout.column(align=True)
            col.label("Linear motor:")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_motor_lin", toggle=True, text="Enable")
            sub = row.row(align=True)
            sub.active = rbc.use_motor_lin
            sub.prop(rbc, "motor_lin_target_velocity", text="Target Velocity")
            sub.prop(rbc, "motor_lin_max_impulse", text="Max Impulse")

            col.label("Angular motor:")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_motor_ang", toggle=True, text="Enable")
            sub = row.row(align=True)
            sub.active = rbc.use_motor_ang
            sub.prop(rbc, "motor_ang_target_velocity", text="Target Velocity")
            sub.prop(rbc, "motor_ang_max_impulse", text="Max Impulse")

        elif rbc.type in {'GENERIC', 'GENERIC_SPRING'}:
            col = layout.column(align=True)
            col.label("Limits:")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_lin_x", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_lin_x
            sub.prop(rbc, "limit_lin_x_lower", text="Lower")
            sub.prop(rbc, "limit_lin_x_upper", text="Upper")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_lin_y", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_lin_y
            sub.prop(rbc, "limit_lin_y_lower", text="Lower")
            sub.prop(rbc, "limit_lin_y_upper", text="Upper")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_lin_z", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_lin_z
            sub.prop(rbc, "limit_lin_z_lower", text="Lower")
            sub.prop(rbc, "limit_lin_z_upper", text="Upper")

            col = layout.column(align=True)

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_ang_x", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_ang_x
            sub.prop(rbc, "limit_ang_x_lower", text="Lower")
            sub.prop(rbc, "limit_ang_x_upper", text="Upper")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_ang_y", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_ang_y
            sub.prop(rbc, "limit_ang_y_lower", text="Lower")
            sub.prop(rbc, "limit_ang_y_upper", text="Upper")

            row = col.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 0.5
            sub.prop(rbc, "use_limit_ang_z", toggle=True)
            sub = row.row(align=True)
            sub.active = rbc.use_limit_ang_z
            sub.prop(rbc, "limit_ang_z_lower", text="Lower")
            sub.prop(rbc, "limit_ang_z_upper", text="Upper")

            if rbc.type == 'GENERIC_SPRING':
                col = layout.column(align=True)
                col.label("Springs:")

                row = col.row(align=True)
                sub = row.row(align=True)
                sub.scale_x = 0.1
                sub.prop(rbc, "use_spring_x", toggle=True, text="X")
                sub = row.row(align=True)
                sub.active = rbc.use_spring_x
                sub.prop(rbc, "spring_stiffness_x", text="Stiffness")
                sub.prop(rbc, "spring_damping_x")

                row = col.row(align=True)
                sub = row.row(align=True)
                sub.scale_x = 0.1
                sub.prop(rbc, "use_spring_y", toggle=True, text="Y")
                sub = row.row(align=True)
                sub.active = rbc.use_spring_y
                sub.prop(rbc, "spring_stiffness_y", text="Stiffness")
                sub.prop(rbc, "spring_damping_y")

                row = col.row(align=True)
                sub = row.row(align=True)
                sub.scale_x = 0.1
                sub.prop(rbc, "use_spring_z", toggle=True, text="Z")
                sub = row.row(align=True)
                sub.active = rbc.use_spring_z
                sub.prop(rbc, "spring_stiffness_z", text="Stiffness")
                sub.prop(rbc, "spring_damping_z")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
