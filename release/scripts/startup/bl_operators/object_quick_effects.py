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

# <pep8-80 compliant>

from mathutils import Vector
import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        IntProperty,
        FloatProperty,
        FloatVectorProperty,
        )


def object_ensure_material(obj, mat_name):
    """ Use an existing material or add a new one.
    """
    mat = mat_slot = None
    for mat_slot in obj.material_slots:
        mat = mat_slot.material
        if mat:
            break
    if mat is None:
        mat = bpy.data.materials.new(mat_name)
        if mat_slot:
            mat_slot.material = mat
        else:
            obj.data.materials.append(mat)
    return mat


def obj_bb_minmax(obj, min_co, max_co):
    for i in range(0, 8):
        bb_vec = obj.matrix_world * Vector(obj.bound_box[i])

        min_co[0] = min(bb_vec[0], min_co[0])
        min_co[1] = min(bb_vec[1], min_co[1])
        min_co[2] = min(bb_vec[2], min_co[2])
        max_co[0] = max(bb_vec[0], max_co[0])
        max_co[1] = max(bb_vec[1], max_co[1])
        max_co[2] = max(bb_vec[2], max_co[2])


def grid_location(x, y):
    return (x * 200, y * 150)


class QuickSmoke(Operator):
    bl_idname = "object.quick_smoke"
    bl_label = "Quick Smoke"
    bl_options = {'REGISTER', 'UNDO'}

    style = EnumProperty(
            name="Smoke Style",
            items=(('SMOKE', "Smoke", ""),
                   ('FIRE', "Fire", ""),
                   ('BOTH', "Smoke + Fire", ""),
                   ),
            default='SMOKE',
            )

    show_flows = BoolProperty(
            name="Render Smoke Objects",
            description="Keep the smoke objects visible during rendering",
            default=False,
            )

    def execute(self, context):
        if not bpy.app.build_options.mod_smoke:
            self.report({'ERROR'}, "Build without Smoke modifier support")
            return {'CANCELLED'}

        fake_context = context.copy()
        mesh_objects = [obj for obj in context.selected_objects
                        if obj.type == 'MESH']
        min_co = Vector((100000.0, 100000.0, 100000.0))
        max_co = -min_co

        if not mesh_objects:
            self.report({'ERROR'}, "Select at least one mesh object")
            return {'CANCELLED'}

        for obj in mesh_objects:
            fake_context["object"] = obj
            # make each selected object a smoke flow
            bpy.ops.object.modifier_add(fake_context, type='SMOKE')
            obj.modifiers[-1].smoke_type = 'FLOW'

            # set type
            obj.modifiers[-1].flow_settings.smoke_flow_type = self.style

            if not self.show_flows:
                obj.draw_type = 'WIRE'

            # store bounding box min/max for the domain object
            obj_bb_minmax(obj, min_co, max_co)

        # add the smoke domain object
        bpy.ops.mesh.primitive_cube_add()
        obj = context.active_object
        obj.name = "Smoke Domain"

        # give the smoke some room above the flows
        obj.location = 0.5 * (max_co + min_co) + Vector((0.0, 0.0, 1.0))
        obj.scale = 0.5 * (max_co - min_co) + Vector((1.0, 1.0, 2.0))

        # setup smoke domain
        bpy.ops.object.modifier_add(type='SMOKE')
        obj.modifiers[-1].smoke_type = 'DOMAIN'
        if self.style == 'FIRE' or self.style == 'BOTH':
            obj.modifiers[-1].domain_settings.use_high_resolution = True

        # Setup material

        # Cycles
        if context.scene.render.use_shading_nodes:
            bpy.ops.object.material_slot_add()

            mat = bpy.data.materials.new("Smoke Domain Material")
            obj.material_slots[0].material = mat

            # Make sure we use nodes
            mat.use_nodes = True

            # Set node variables and clear the default nodes
            tree = mat.node_tree
            nodes = tree.nodes
            links = tree.links

            nodes.clear()

            # Create shader nodes

            # Material output
            node_out = nodes.new(type='ShaderNodeOutputMaterial')
            node_out.location = grid_location(6, 1)

            # Add shader 1
            node_add_shader_1 = nodes.new(type='ShaderNodeAddShader')
            node_add_shader_1.location = grid_location(5, 1)
            links.new(node_add_shader_1.outputs["Shader"],
                    node_out.inputs["Volume"])

            if self.style in {'SMOKE', 'FIRE', 'BOTH'}:
                # Smoke

                # Add shader 2
                node_add_shader_2 = nodes.new(type='ShaderNodeAddShader')
                node_add_shader_2.location = grid_location(4, 2)
                links.new(node_add_shader_2.outputs["Shader"],
                        node_add_shader_1.inputs[0])

                # Volume scatter
                node_scatter = nodes.new(type='ShaderNodeVolumeScatter')
                node_scatter.location = grid_location(3, 3)
                links.new(node_scatter.outputs["Volume"],
                        node_add_shader_2.inputs[0])

                # Volume absorption
                node_absorption = nodes.new(type='ShaderNodeVolumeAbsorption')
                node_absorption.location = grid_location(3, 2)
                links.new(node_absorption.outputs["Volume"],
                        node_add_shader_2.inputs[1])

                # Density Multiplier
                node_densmult = nodes.new(type='ShaderNodeMath')
                node_densmult.location = grid_location(2, 2)
                node_densmult.operation = 'MULTIPLY'
                node_densmult.inputs[1].default_value = 5.0
                links.new(node_densmult.outputs["Value"],
                        node_scatter.inputs["Density"])
                links.new(node_densmult.outputs["Value"],
                        node_absorption.inputs["Density"])

                # Attribute "density"
                node_attrib_density = nodes.new(type='ShaderNodeAttribute')
                node_attrib_density.attribute_name = "density"
                node_attrib_density.location = grid_location(1, 2)
                links.new(node_attrib_density.outputs["Fac"],
                        node_densmult.inputs[0])

                # Attribute "color"
                node_attrib_color = nodes.new(type='ShaderNodeAttribute')
                node_attrib_color.attribute_name = "color"
                node_attrib_color.location = grid_location(2, 3)
                links.new(node_attrib_color.outputs["Color"],
                        node_scatter.inputs["Color"])
                links.new(node_attrib_color.outputs["Color"],
                        node_absorption.inputs["Color"])

            if self.style in {'FIRE', 'BOTH'}:
                # Fire

                # Emission
                node_emission = nodes.new(type='ShaderNodeEmission')
                node_emission.inputs["Color"].default_value = (0.8, 0.1, 0.01, 1.0)
                node_emission.location = grid_location(4, 1)
                links.new(node_emission.outputs["Emission"],
                        node_add_shader_1.inputs[1])

                # Flame strength multiplier
                node_flame_strength_mult = nodes.new(type='ShaderNodeMath')
                node_flame_strength_mult.location = grid_location(3, 1)
                node_flame_strength_mult.operation = 'MULTIPLY'
                node_flame_strength_mult.inputs[1].default_value = 2.5
                links.new(node_flame_strength_mult.outputs["Value"],
                        node_emission.inputs["Strength"])

                # Color ramp Flame
                node_flame_ramp = nodes.new(type='ShaderNodeValToRGB')
                node_flame_ramp.location = grid_location(1, 1)
                ramp = node_flame_ramp.color_ramp
                ramp.interpolation = 'EASE'

                # orange
                elem = ramp.elements.new(0.5)
                elem.color = (1.0, 0.128, 0.0, 1.0)

                # yellow
                elem = ramp.elements.new(0.9)
                elem.color = (0.9, 0.6, 0.1, 1.0)

                links.new(node_flame_ramp.outputs["Color"],
                        node_emission.inputs["Color"])

                # Attribute "flame"
                node_attrib_flame = nodes.new(type='ShaderNodeAttribute')
                node_attrib_flame.attribute_name = "flame"
                node_attrib_flame.location = grid_location(0, 1)
                links.new(node_attrib_flame.outputs["Fac"],
                        node_flame_ramp.inputs["Fac"])
                links.new(node_attrib_flame.outputs["Fac"],
                        node_flame_strength_mult.inputs[0])

        # Blender Internal
        else:
            # create a volume material with a voxel data texture for the domain
            bpy.ops.object.material_slot_add()

            mat = bpy.data.materials.new("Smoke Domain Material")
            obj.material_slots[0].material = mat
            mat.type = 'VOLUME'
            mat.volume.density = 0
            mat.volume.density_scale = 5
            mat.volume.step_size = 0.1

            tex = bpy.data.textures.new("Smoke Density", 'VOXEL_DATA')
            tex.voxel_data.domain_object = obj
            tex.voxel_data.interpolation = 'TRICUBIC_BSPLINE'

            tex_slot = mat.texture_slots.add()
            tex_slot.texture = tex
            tex_slot.texture_coords = 'ORCO'
            tex_slot.use_map_color_emission = False
            tex_slot.use_map_density = True
            tex_slot.use_map_color_reflection = True

            # for fire add a second texture for flame emission
            mat.volume.emission_color = Vector((0.0, 0.0, 0.0))
            tex = bpy.data.textures.new("Flame", 'VOXEL_DATA')
            tex.voxel_data.domain_object = obj
            tex.voxel_data.smoke_data_type = 'SMOKEFLAME'
            tex.voxel_data.interpolation = 'TRICUBIC_BSPLINE'
            tex.use_color_ramp = True

            tex_slot = mat.texture_slots.add()
            tex_slot.texture = tex
            tex_slot.texture_coords = 'ORCO'

            # add color ramp for flame color
            ramp = tex.color_ramp
            # dark orange
            elem = ramp.elements.new(0.333)
            elem.color = (0.2, 0.03, 0.0, 1.0)

            # yellow glow
            elem = ramp.elements.new(0.666)
            elem.color = (1, 0.65, 0.25, 1.0)

            mat.texture_slots[1].use_map_density = True
            mat.texture_slots[1].use_map_emission = True
            mat.texture_slots[1].emission_factor = 5

        return {'FINISHED'}


class QuickFluid(Operator):
    bl_idname = "object.quick_fluid"
    bl_label = "Quick Fluid"
    bl_options = {'REGISTER', 'UNDO'}

    style = EnumProperty(
            name="Fluid Style",
            items=(('INFLOW', "Inflow", ""),
                   ('BASIC', "Basic", "")),
            default='BASIC',
            )
    initial_velocity = FloatVectorProperty(
            name="Initial Velocity",
            description="Initial velocity of the fluid",
            min=-100.0, max=100.0,
            default=(0.0, 0.0, 0.0),
            subtype='VELOCITY',
            )
    show_flows = BoolProperty(
            name="Render Fluid Objects",
            description="Keep the fluid objects visible during rendering",
            default=False,
            )
    start_baking = BoolProperty(
            name="Start Fluid Bake",
            description=("Start baking the fluid immediately "
                         "after creating the domain object"),
            default=False,
            )

    def execute(self, context):
        if not bpy.app.build_options.mod_fluid:
            self.report({'ERROR'}, "Build without Fluid modifier support")
            return {'CANCELLED'}

        fake_context = context.copy()
        mesh_objects = [obj for obj in context.selected_objects
                        if (obj.type == 'MESH' and 0.0 not in obj.dimensions)]
        min_co = Vector((100000.0, 100000.0, 100000.0))
        max_co = -min_co

        if not mesh_objects:
            self.report({'ERROR'}, "Select at least one mesh object")
            return {'CANCELLED'}

        for obj in mesh_objects:
            fake_context["object"] = obj
            # make each selected object a fluid
            bpy.ops.object.modifier_add(fake_context, type='FLUID_SIMULATION')

            # fluid has to be before constructive modifiers,
            # so it might not be the last modifier
            for mod in obj.modifiers:
                if mod.type == 'FLUID_SIMULATION':
                    break

            if self.style == 'INFLOW':
                mod.settings.type = 'INFLOW'
                mod.settings.inflow_velocity = self.initial_velocity
            else:
                mod.settings.type = 'FLUID'
                mod.settings.initial_velocity = self.initial_velocity

            obj.hide_render = not self.show_flows
            if not self.show_flows:
                obj.draw_type = 'WIRE'

            # store bounding box min/max for the domain object
            obj_bb_minmax(obj, min_co, max_co)

        # add the fluid domain object
        bpy.ops.mesh.primitive_cube_add()
        obj = context.active_object
        obj.name = "Fluid Domain"

        # give the fluid some room below the flows
        # and scale with initial velocity
        v = 0.5 * self.initial_velocity
        obj.location = 0.5 * (max_co + min_co) + Vector((0.0, 0.0, -1.0)) + v
        obj.scale = (0.5 * (max_co - min_co) +
                     Vector((1.0, 1.0, 2.0)) +
                     Vector((abs(v[0]), abs(v[1]), abs(v[2])))
                     )

        # setup smoke domain
        bpy.ops.object.modifier_add(type='FLUID_SIMULATION')
        obj.modifiers[-1].settings.type = 'DOMAIN'

        # make the domain smooth so it renders nicely
        bpy.ops.object.shade_smooth()

        # create a ray-transparent material for the domain
        bpy.ops.object.material_slot_add()

        mat = bpy.data.materials.new("Fluid Domain Material")
        obj.material_slots[0].material = mat

        mat.specular_intensity = 1
        mat.specular_hardness = 100
        mat.use_transparency = True
        mat.alpha = 0.0
        mat.transparency_method = 'RAYTRACE'
        mat.raytrace_transparency.ior = 1.33
        mat.raytrace_transparency.depth = 4

        if self.start_baking:
            bpy.ops.fluid.bake('INVOKE_DEFAULT')

        return {'FINISHED'}
