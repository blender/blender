# gpl: author meta-androcto

import bpy
from bpy.types import Operator


class add_cycles_scene(Operator):
    bl_idname = "objects_cycles.add_scene"
    bl_label = "Create test scene"
    bl_description = "Cycles renderer Scene with Objects"
    bl_options = {'REGISTER'}

    def execute(self, context):
        try:
            blend_data = context.blend_data

            # add new scene
            bpy.ops.scene.new(type="NEW")
            scene = bpy.context.scene
            bpy.context.scene.render.engine = 'CYCLES'
            scene.name = "scene_object_cycles"

            # render settings
            render = scene.render
            render.resolution_x = 1920
            render.resolution_y = 1080
            render.resolution_percentage = 50

            # add new world
            world = bpy.data.worlds.new("Cycles_Object_World")
            scene.world = world
            world.use_sky_blend = True
            world.use_sky_paper = True
            world.horizon_color = (0.004393, 0.02121, 0.050)
            world.zenith_color = (0.03335, 0.227, 0.359)
            world.light_settings.use_ambient_occlusion = True
            world.light_settings.ao_factor = 0.25

            # add camera
            bpy.ops.object.camera_add(
                    location=(7.48113, -6.50764, 5.34367),
                    rotation=(1.109319, 0.010817, 0.814928)
                    )
            cam = bpy.context.active_object.data
            cam.lens = 35
            cam.draw_size = 0.1
            bpy.ops.view3d.viewnumpad(type='CAMERA')

            # add point lamp
            bpy.ops.object.lamp_add(
                    type="POINT", location=(4.07625, 1.00545, 5.90386),
                    rotation=(0.650328, 0.055217, 1.866391)
                    )
            lamp1 = bpy.context.active_object.data
            lamp1.name = "Point_Right"
            lamp1.energy = 1.0
            lamp1.distance = 30.0
            lamp1.shadow_method = "RAY_SHADOW"
            lamp1.use_sphere = True

            # add point lamp2
            bpy.ops.object.lamp_add(
                    type="POINT", location=(-0.57101, -4.24586, 5.53674),
                    rotation=(1.571, 0, 0.785)
                    )
            lamp2 = bpy.context.active_object.data
            lamp2.name = "Point_Left"
            lamp2.energy = 1.0
            lamp2.distance = 30.0

            # Add cube
            bpy.ops.mesh.primitive_cube_add()
            bpy.ops.object.editmode_toggle()
            bpy.ops.mesh.subdivide(number_cuts=2)
            bpy.ops.uv.unwrap(method='CONFORMAL', margin=0.001)
            bpy.ops.object.editmode_toggle()
            cube = bpy.context.active_object

            # add cube material
            cubeMaterial = blend_data.materials.new("Cycles_Cube_Material")
            bpy.ops.object.material_slot_add()
            cube.material_slots[0].material = cubeMaterial
            # Diffuse
            cubeMaterial.preview_render_type = "CUBE"
            cubeMaterial.diffuse_color = (1.000, 0.373, 0.00)
            # Cycles
            cubeMaterial.use_nodes = True

            # Add monkey
            bpy.ops.mesh.primitive_monkey_add(location=(-0.1, 0.08901, 1.505))
            bpy.ops.transform.rotate(value=(1.15019), axis=(0, 0, 1))
            bpy.ops.transform.rotate(value=(-0.673882), axis=(0, 1, 0))
            bpy.ops.transform.rotate(value=-0.055, axis=(1, 0, 0))

            bpy.ops.object.modifier_add(type='SUBSURF')
            bpy.ops.object.shade_smooth()
            monkey = bpy.context.active_object

            # add monkey material
            monkeyMaterial = blend_data.materials.new("Cycles_Monkey_Material")
            bpy.ops.object.material_slot_add()
            monkey.material_slots[0].material = monkeyMaterial
            # Diffuse
            monkeyMaterial.preview_render_type = "MONKEY"
            monkeyMaterial.diffuse_color = (0.239, 0.288, 0.288)
            # Cycles
            monkeyMaterial.use_nodes = True

            # Add plane
            bpy.ops.mesh.primitive_plane_add(
                    radius=50, view_align=False,
                    enter_editmode=False, location=(0, 0, -1)
                    )
            bpy.ops.object.editmode_toggle()
            bpy.ops.transform.rotate(
                    value=-0.8, axis=(0, 0, 1),
                    constraint_axis=(False, False, True)
                    )
            bpy.ops.uv.unwrap(method='CONFORMAL', margin=0.001)
            bpy.ops.object.editmode_toggle()
            plane = bpy.context.active_object

            # add plane material
            planeMaterial = blend_data.materials.new("Cycles_Plane_Material")
            bpy.ops.object.material_slot_add()
            plane.material_slots[0].material = planeMaterial
            # Diffuse
            planeMaterial.preview_render_type = "FLAT"
            planeMaterial.diffuse_color = (0.2, 0.2, 0.2)
            # Cycles
            planeMaterial.use_nodes = True

        except Exception as e:
            self.report({'WARNING'},
                        "Some operations could not be performed (See Console for more info)")

            print("\n[Add Advanced  Objects]\nOperator: "
                  "objects_cycles.add_scene\nError: {}".format(e))

            return {'CANCELLED'}

        return {'FINISHED'}
