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

import mathutils
from mathutils import Vector
import bpy
from bpy.props import BoolProperty, EnumProperty, IntProperty, FloatProperty, FloatVectorProperty

class MakeFur(bpy.types.Operator):
    bl_idname = "object.make_fur"
    bl_label = "Make Fur"
    bl_options = {'REGISTER', 'UNDO'}
    
    density = EnumProperty(items=(
                        ('LIGHT', "Light", ""),
                        ('MEDIUM', "Medium", ""),
                        ('HEAVY', "Heavy", "")),
                name="Fur Density",
                description="",
                default='MEDIUM')
    
    view_percentage = IntProperty(name="View %",
            default=10, min=1, max=100, soft_min=1, soft_max=100)
            
    length = FloatProperty(name="Length",
            default=0.1, min=0.001, max=100, soft_min=0.01, soft_max=10)
    
    def execute(self, context):
        count = 0
        for ob in context.selected_objects:
        
            if(ob == None or ob.type != 'MESH'):
                continue
                
            count += 1
            
            context.scene.objects.active = ob
            
            bpy.ops.object.particle_system_add()
            
            psys = ob.particle_systems[-1]
            psys.settings.type = 'HAIR'
            
            if self.density == 'LIGHT':
                psys.settings.count = 100
            elif self.density == 'MEDIUM':
                psys.settings.count = 1000
            elif self.density == 'HEAVY':
                psys.settings.count = 10000
                
            psys.settings.child_nbr = self.view_percentage
            psys.settings.hair_length = self.length
            psys.settings.use_strand_primitive = True
            psys.settings.use_hair_bspline = True
            psys.settings.child_type = 'INTERPOLATED'
            
        if count == 0:
            self.report({'ERROR'}, "Select at least one mesh object.")
            return {'CANCELLED'}
        
        mat = bpy.data.materials.new("Fur Material")
        mat.strand.tip_size = 0.25
        mat.strand.blend_distance = 0.5
        
        for ob in context.selected_objects:
            ob.data.materials.append(mat)
            ob.particle_systems[-1].settings.material = len(ob.material_slots)
        
        return {'FINISHED'}

class MakeSmoke(bpy.types.Operator):
    bl_idname = "object.make_smoke"
    bl_label = "Make Smoke"
    bl_options = {'REGISTER', 'UNDO'}
    
    style = EnumProperty(items=(
                        ('STREAM', "Stream", ""),
                        ('PUFF', "Puff", ""),
                        ('FIRE', "Fire", "")),
                name="Smoke Style",
                description="",
                default='STREAM')
                
    show_flows = BoolProperty(name="Render Smoke Objects",
                description="Keep the smoke objects visible during rendering.",
                default=False)
    
    def execute(self, context):
        count = 0
        min_co = Vector((0,0,0))
        max_co = Vector((0,0,0))
        for ob in context.selected_objects:
        
            if(ob == None or ob.type != 'MESH'):
                continue
            
            context.scene.objects.active = ob
            
            # make each selected object a smoke flow
            bpy.ops.object.modifier_add(type='SMOKE')
            ob.modifiers[-1].smoke_type = 'FLOW'
            
            psys = ob.particle_systems[-1]
            if self.style == 'PUFF':
                psys.settings.frame_end = psys.settings.frame_start
                psys.settings.emit_from = 'VOLUME'
                psys.settings.distribution = 'RAND'
            elif self.style == 'FIRE':
                psys.settings.effector_weights.gravity = -1
                psys.settings.lifetime = 5
                psys.settings.count = 100000
                
                ob.modifiers[-2].flow_settings.initial_velocity = True
                ob.modifiers[-2].flow_settings.temperature = 2
                
            psys.settings.use_render_emitter = self.show_flows
            if not self.show_flows:
                ob.draw_type = 'WIRE'
            
            # store bounding box min/max for the domain object
            for i in range(0, 8):
                bb_vec = Vector((ob.bound_box[i][0], ob.bound_box[i][1], ob.bound_box[i][2])) * ob.matrix_world
                
                if count == 0 and i == 0:
                    min_co += bb_vec
                    max_co += bb_vec
                else:
                    min_co[0] = min(bb_vec[0], min_co[0])
                    min_co[1] = min(bb_vec[1], min_co[1])
                    min_co[2] = min(bb_vec[2], min_co[2])
                    max_co[0] = max(bb_vec[0], max_co[0])
                    max_co[1] = max(bb_vec[1], max_co[1])
                    max_co[2] = max(bb_vec[2], max_co[2])
                
            count += 1
            
        if count == 0:
            self.report({'ERROR'}, "Select at least one mesh object.")
            return {'CANCELLED'}
        
        # add the smoke domain object
        bpy.ops.mesh.primitive_cube_add()
        ob = context.active_object
        ob.name = "Smoke Domain"
        
        # give the smoke some room above the flows
        ob.location[0] = (max_co[0] + min_co[0]) * 0.5
        ob.location[1] = (max_co[1] + min_co[1]) * 0.5
        ob.location[2] = max_co[2] - min_co[2]
        ob.scale[0] = max_co[0] - min_co[0]
        ob.scale[1] = max_co[1] - min_co[1]
        ob.scale[2] = 2*(max_co[2] - min_co[2])
        
        # setup smoke domain
        bpy.ops.object.modifier_add(type='SMOKE')
        ob.modifiers[-1].smoke_type = 'DOMAIN'
        if self.style == 'FIRE':
            ob.modifiers[-1].domain_settings.use_dissolve_smoke = True
            ob.modifiers[-1].domain_settings.dissolve_speed = 20
            ob.modifiers[-1].domain_settings.use_high_resolution = True
        
        # create a volume material with a voxel data texture for the domain
        bpy.ops.object.material_slot_add()
        
        mat = ob.material_slots[0].material
        mat.name = "Smoke Domain Material"
        mat.type = 'VOLUME'
        mat.volume.density = 0
        mat.volume.density_scale = 5
            
        mat.texture_slots.add()
        mat.texture_slots[0].texture = bpy.data.textures.new("Smoke Density", 'VOXEL_DATA')
        mat.texture_slots[0].texture.voxel_data.domain_object = ob
        mat.texture_slots[0].use_map_color_emission = False
        mat.texture_slots[0].use_map_density = True
        
        # for fire add a second texture for emission and emission color
        if self.style == 'FIRE':
            mat.volume.emission = 5
            mat.texture_slots.add()
            mat.texture_slots[1].texture = bpy.data.textures.new("Smoke Heat", 'VOXEL_DATA')
            mat.texture_slots[1].texture.voxel_data.domain_object = ob
            mat.texture_slots[1].texture.use_color_ramp = True
            
            ramp = mat.texture_slots[1].texture.color_ramp
            
            elem = ramp.elements.new(0.333)
            elem.color[0] = elem.color[3] = 1
            elem.color[1] = elem.color[2] = 0
            
            elem = ramp.elements.new(0.666)
            elem.color[0] = elem.color[1] = elem.color[3] = 1
            elem.color[2] = 0
            
            mat.texture_slots[1].use_map_emission = True
            mat.texture_slots[1].blend_type = 'MULTIPLY'
            
        return {'FINISHED'}
        
class MakeFluid(bpy.types.Operator):
    bl_idname = "object.make_fluid"
    bl_label = "Make Fluid"
    bl_options = {'REGISTER', 'UNDO'}
    
    style = EnumProperty(items=(
                        ('INFLOW', "Inflow", ""),
                        ('BASIC', "Basic", "")),
                name="Fluid Style",
                description="",
                default='BASIC')
                
    initial_velocity = FloatVectorProperty(name="Initial Velocity",
        description="Initial velocity of the fluid",
        default=(0.0, 0.0, 0.0), min=-100.0, max=100.0, subtype='VELOCITY')
                
    show_flows = BoolProperty(name="Render Fluid Objects",
                description="Keep the fluid objects visible during rendering.",
                default=False)
                
    start_baking = BoolProperty(name="Start Fluid Bake",
                description="Start baking the fluid immediately after creating the domain object.",
                default=False)
    
    def execute(self, context):
        count = 0
        min_co = Vector((0,0,0))
        max_co = Vector((0,0,0))
        for ob in context.selected_objects:
        
            if(ob == None or ob.type != 'MESH'):
                continue
            
            context.scene.objects.active = ob
            
            # make each selected object a fluid
            bpy.ops.object.modifier_add(type='FLUID_SIMULATION')
            if self.style == 'INFLOW':
                ob.modifiers[-1].settings.type = 'INFLOW'
                ob.modifiers[-1].settings.inflow_velocity = self.initial_velocity.copy()
            else:
                ob.modifiers[-1].settings.type = 'FLUID'
                ob.modifiers[-1].settings.initial_velocity = self.initial_velocity.copy()
            
            ob.hide_render = not self.show_flows
            if not self.show_flows:
                ob.draw_type = 'WIRE'
            
            # store bounding box min/max for the domain object
            for i in range(0, 8):
                bb_vec = Vector((ob.bound_box[i][0], ob.bound_box[i][1], ob.bound_box[i][2])) * ob.matrix_world
                
                if count == 0 and i == 0:
                    min_co += bb_vec
                    max_co += bb_vec
                else:
                    min_co[0] = min(bb_vec[0], min_co[0])
                    min_co[1] = min(bb_vec[1], min_co[1])
                    min_co[2] = min(bb_vec[2], min_co[2])
                    max_co[0] = max(bb_vec[0], max_co[0])
                    max_co[1] = max(bb_vec[1], max_co[1])
                    max_co[2] = max(bb_vec[2], max_co[2])
                
            count += 1
            
        if count == 0:
            self.report({'ERROR'}, "Select at least one mesh object.")
            return {'CANCELLED'}
        
        # add the fluid domain object
        bpy.ops.mesh.primitive_cube_add()
        ob = context.active_object
        ob.name = "Fluid Domain"
        
        # give the smoke some room above the flows
        ob.location[0] = (max_co[0] + min_co[0]) * 0.5
        ob.location[1] = (max_co[1] + min_co[1]) * 0.5
        ob.location[2] = min_co[2] - max_co[2]
        ob.scale[0] = max_co[0] - min_co[0]
        ob.scale[1] = max_co[1] - min_co[1]
        ob.scale[2] = 2*(max_co[2] - min_co[2])
        
        # setup smoke domain
        bpy.ops.object.modifier_add(type='FLUID_SIMULATION')
        ob.modifiers[-1].settings.type = 'DOMAIN'
        
        # make the domain smooth so it renders nicely
        bpy.ops.object.shade_smooth()
        
        # create a ray-transparent material for the domain
        bpy.ops.object.material_slot_add()
        
        mat = ob.material_slots[0].material
        mat.name = "Fluid Domain Material"
        mat.specular_intensity = 1
        mat.specular_hardness = 100
        mat.use_transparency = True
        mat.alpha = 0
        mat.transparency_method = 'RAYTRACE'
        mat.raytrace_transparency.ior = 1.33
        mat.raytrace_transparency.depth = 4
        
        if self.start_baking:
            bpy.ops.fluid.bake()
            
        return {'FINISHED'}