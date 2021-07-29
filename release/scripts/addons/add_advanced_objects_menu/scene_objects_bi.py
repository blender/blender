# gpl: author meta-androcto

import bpy
from bpy.types import Operator


class add_BI_scene(Operator):
    bl_idname = "bi.add_scene"
    bl_label = "Create test scene"
    bl_description = "Blender Internal renderer Scene with Objects"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            blend_data = context.blend_data
            # ob = bpy.context.active_object

            # add new scene
            bpy.ops.scene.new(type="NEW")
            scene = bpy.context.scene
            scene.name = "scene_materials"

            # render settings
            render = scene.render
            render.resolution_x = 1920
            render.resolution_y = 1080
            render.resolution_percentage = 50

            # add new world
            world = bpy.data.worlds.new("Materials_World")
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
            # add new material
            cubeMaterial = blend_data.materials.new("Cube_Material")
            bpy.ops.object.material_slot_add()
            cube.material_slots[0].material = cubeMaterial
            # Diffuse
            cubeMaterial.preview_render_type = "CUBE"
            cubeMaterial.diffuse_color = (1.000, 0.373, 0.00)
            cubeMaterial.diffuse_shader = 'OREN_NAYAR'
            cubeMaterial.diffuse_intensity = 1.0
            cubeMaterial.roughness = 0.09002
            # Specular
            cubeMaterial.specular_color = (1.000, 0.800, 0.136)
            cubeMaterial.specular_shader = "PHONG"
            cubeMaterial.specular_intensity = 1.0
            cubeMaterial.specular_hardness = 511.0
            # Shading
            cubeMaterial.ambient = 1.00
            cubeMaterial.use_cubic = False
            # Transparency
            cubeMaterial.use_transparency = False
            cubeMaterial.alpha = 0
            # Mirror
            cubeMaterial.raytrace_mirror.use = True
            cubeMaterial.mirror_color = (1.000, 0.793, 0.0)
            cubeMaterial.raytrace_mirror.reflect_factor = 0.394
            cubeMaterial.raytrace_mirror.fresnel = 2.0
            cubeMaterial.raytrace_mirror.fresnel_factor = 1.641
            cubeMaterial.raytrace_mirror.fade_to = "FADE_TO_SKY"
            cubeMaterial.raytrace_mirror.gloss_anisotropic = 1.0
            # Shadow
            cubeMaterial.use_transparent_shadows = True

            # Add a texture
            cubetex = blend_data.textures.new("CloudTex", type='CLOUDS')
            cubetex.noise_type = 'SOFT_NOISE'
            cubetex.noise_scale = 0.25
            mtex = cubeMaterial.texture_slots.add()
            mtex.texture = cubetex
            mtex.texture_coords = 'ORCO'
            mtex.scale = (0.800, 0.800, 0.800)
            mtex.use_map_mirror = True
            mtex.mirror_factor = 0.156
            mtex.use_map_color_diffuse = True
            mtex.diffuse_color_factor = 0.156
            mtex.use_map_normal = True
            mtex.normal_factor = 0.010
            mtex.blend_type = "ADD"
            mtex.use_rgb_to_intensity = True
            mtex.color = (1.000, 0.207, 0.000)

            # Add monkey
            bpy.ops.mesh.primitive_monkey_add(location=(-0.1, 0.08901, 1.505))
            bpy.ops.transform.rotate(value=(1.15019), axis=(0, 0, 1))
            bpy.ops.transform.rotate(value=(-0.673882), axis=(0, 1, 0))
            bpy.ops.transform.rotate(value=-0.055, axis=(1, 0, 0))
            bpy.ops.object.modifier_add(type='SUBSURF')
            bpy.ops.object.shade_smooth()
            monkey = bpy.context.active_object
            # add new material
            monkeyMaterial = blend_data.materials.new("Monkey_Material")
            bpy.ops.object.material_slot_add()
            monkey.material_slots[0].material = monkeyMaterial
            # Material settings
            monkeyMaterial.preview_render_type = "MONKEY"
            monkeyMaterial.diffuse_color = (0.239, 0.288, 0.288)
            monkeyMaterial.specular_color = (0.604, 0.465, 0.136)
            monkeyMaterial.diffuse_shader = 'LAMBERT'
            monkeyMaterial.diffuse_intensity = 1.0
            monkeyMaterial.specular_intensity = 0.3
            monkeyMaterial.ambient = 0
            monkeyMaterial.type = 'SURFACE'
            monkeyMaterial.use_cubic = True
            monkeyMaterial.use_transparency = False
            monkeyMaterial.alpha = 0
            monkeyMaterial.use_transparent_shadows = True
            monkeyMaterial.raytrace_mirror.use = True
            monkeyMaterial.raytrace_mirror.reflect_factor = 0.65
            monkeyMaterial.raytrace_mirror.fade_to = "FADE_TO_MATERIAL"

            # Add plane
            bpy.ops.mesh.primitive_plane_add(
                            radius=50, view_align=False, enter_editmode=False, location=(0, 0, -1)
                            )
            bpy.ops.object.editmode_toggle()
            bpy.ops.transform.rotate(
                    value=-0.8, axis=(0, 0, 1), constraint_axis=(False, False, True),
                    constraint_orientation='GLOBAL', mirror=False, proportional='DISABLED',
                    proportional_edit_falloff='SMOOTH', proportional_size=1
                    )
            bpy.ops.uv.unwrap(method='CONFORMAL', margin=0.001)
            bpy.ops.object.editmode_toggle()
            plane = bpy.context.active_object
            # add new material
            planeMaterial = blend_data.materials.new("Plane_Material")
            bpy.ops.object.material_slot_add()
            plane.material_slots[0].material = planeMaterial
            # Material settings
            planeMaterial.preview_render_type = "CUBE"
            planeMaterial.diffuse_color = (0.2, 0.2, 0.2)
            planeMaterial.specular_color = (0.604, 0.465, 0.136)
            planeMaterial.specular_intensity = 0.3
            planeMaterial.ambient = 0
            planeMaterial.use_cubic = True
            planeMaterial.use_transparency = False
            planeMaterial.alpha = 0
            planeMaterial.use_transparent_shadows = True

        except Exception as e:
            self.report({'WARNING'},
                        "Some operations could not be performed (See Console for more info)")

            print("\n[Add Advanced  Objects]\nOperator: "
                  "bi.add_scene\nError: {}".format(e))

            return {'CANCELLED'}

        return {"FINISHED"}
