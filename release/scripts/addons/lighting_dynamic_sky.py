# Dynamic Sky.py (c) 2015 Pratik Solanki (Draguu)

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


bl_info = {
    "name": "Dynamic Sky",
    "author": "Pratik Solanki",
    "version": (1, 0, 6),
    "blender": (2, 78, 0),
    "location": "View3D > Tools",
    "description": "Creates Dynamic Sky for Cycles",
    "warning": "",
    "wiki_url": "http://www.dragoneex.com/downloads/dynamic-skyadd-on",
    "category": "Lighting",
    }

import bpy
from bpy.props import StringProperty
from bpy.types import (
        Operator,
        Panel,
        )


# Handle error notifications
def error_handlers(self, error, reports="ERROR"):
    if self and reports:
        self.report({'WARNING'}, reports + " (See Console for more info)")

    print("\n[Dynamic Sky]\nError: {}\n".format(error))


def check_world_name(name_id="Dynamic"):
    # check if the new name pattern is in world data
    name_list = []
    suffix = 1
    try:
        name_list = [world.name for world in bpy.data.worlds if name_id in world.name]
        new_name = "{}_{}".format(name_id, len(name_list) + suffix)
        if new_name in name_list:
            # KISS failed - numbering is not sequential
            # try harvesting numbers in world names, find the rightmost ones
            test_num = []
            from re import findall
            for words in name_list:
                test_num.append(findall("\d+", words))

            suffix += max([int(l[-1]) for l in test_num])
            new_name = "{}_{}".format(name_id, suffix)
        return new_name
    except Exception as e:
        error_handlers(False, e)
        pass
    return name_id


def check_cycles():
    return ('cycles' in bpy.context.user_preferences.addons.keys())


class dsky(Operator):
    bl_idname = "sky.dyn"
    bl_label = "Make a Procedural sky"
    bl_description = ("Make a Procedural Sky with parameters in the 3D View\n"
                      "Note: Available just for Cycles renderer\n"
                      "Only the last created Dynamic World can be accessed from this panel")

    @classmethod
    def poll(cls, context):
        return check_cycles()

    def get_node_types(self, node_tree, node_type):
        for node in node_tree.nodes:
            if node.type == node_type:
                return node
        return None

    def execute(self, context):
        try:
            get_name = check_world_name()
            context.scene.dynamic_sky_name = get_name
            bpy.context.scene.render.engine = 'CYCLES'

            world = bpy.data.worlds.new(get_name)
            world.cycles.sample_as_light = True
            world.cycles.sample_map_resolution = 2048
            world.use_nodes = True

            nt = world.node_tree
            # Note: (see T52714) to avoid string localization problems, assign the name for
            # nodes that will be exposed in the 3D view (pattern UI name with underscore)
            bg = self.get_node_types(nt, "BACKGROUND")
            bg.name = "Scene_Brightness"
            bg.inputs[0].default_value[:3] = (0.5, .1, 0.6)
            bg.inputs[1].default_value = 1
            bg.location = (6708.3, 360)

            ntl = nt.links.new
            tcor = nt.nodes.new(type="ShaderNodeTexCoord")
            tcor.location = (243.729, 1005)

            map1 = nt.nodes.new(type="ShaderNodeMapping")
            map1.vector_type = 'NORMAL'
            map1.location = (786.54, 730)

            nor = nt.nodes.new(type="ShaderNodeNormal")
            nor.name = "Sky_normal"
            nor.location = (1220.16, 685)

            cr1 = nt.nodes.new(type="ShaderNodeValToRGB")
            cr1.color_ramp.elements[0].position = 0.969
            cr1.color_ramp.interpolation = 'EASE'
            cr1.location = (1671.33, 415)
            cr2 = nt.nodes.new(type="ShaderNodeValToRGB")
            cr2.color_ramp.elements[0].position = 0.991
            cr2.color_ramp.elements[1].position = 1
            cr2.color_ramp.interpolation = 'EASE'
            cr2.location = (2196.6, 415)
            cr3 = nt.nodes.new(type="ShaderNodeValToRGB")
            cr3.color_ramp.elements[0].position = 0.779
            cr3.color_ramp.elements[1].position = 1
            cr3.color_ramp.interpolation = 'EASE'
            cr3.location = (2196.6, 415)

            mat1 = nt.nodes.new(type="ShaderNodeMath")
            mat1.operation = 'MULTIPLY'
            mat1.inputs[1].default_value = 0.2
            mat1.location = (2196.6, 685)
            mat2 = nt.nodes.new(type="ShaderNodeMath")
            mat2.operation = 'MULTIPLY'
            mat2.inputs[1].default_value = 2
            mat2.location = (3294, 685)
            mat3 = nt.nodes.new(type="ShaderNodeMath")
            mat3.operation = 'MULTIPLY'
            mat3.inputs[1].default_value = 40.9
            mat3.location = (2745.24, 415)
            mat4 = nt.nodes.new(type="ShaderNodeMath")
            mat4.operation = 'SUBTRACT'
            mat4.inputs[1].default_value = 1
            mat4.location = (3294, 415)
            ntl(mat2.inputs[0], mat1.outputs[0])
            ntl(mat4.inputs[0], mat3.outputs[0])
            ntl(mat1.inputs[0], cr3.outputs[0])
            ntl(mat3.inputs[0], cr2.outputs[0])

            soft = nt.nodes.new(type="ShaderNodeMixRGB")
            soft.name = "Soft_hard"
            soft.location = (3819.3, 550)
            soft_1 = nt.nodes.new(type="ShaderNodeMixRGB")
            soft_1.location = (3819.3, 185)
            soft.inputs[0].default_value = 1
            soft_1.inputs[0].default_value = 0.466
            ntl(soft.inputs[1], mat2.outputs[0])
            ntl(soft.inputs[2], mat4.outputs[0])
            ntl(soft_1.inputs[1], mat2.outputs[0])
            ntl(soft_1.inputs[2], cr2.outputs[0])

            mix1 = nt.nodes.new(type="ShaderNodeMixRGB")
            mix1.blend_type = 'MULTIPLY'
            mix1.inputs[0].default_value = 1
            mix1.location = (4344.3, 630)
            mix1_1 = nt.nodes.new(type="ShaderNodeMixRGB")
            mix1_1.blend_type = 'MULTIPLY'
            mix1_1.inputs[0].default_value = 1
            mix1_1.location = (4344.3, 90)

            mix2 = nt.nodes.new(type="ShaderNodeMixRGB")
            mix2.location = (4782, 610)
            mix2_1 = nt.nodes.new(type="ShaderNodeMixRGB")
            mix2_1.location = (5131.8, 270)
            mix2.inputs[1].default_value = (0, 0, 0, 1)
            mix2.inputs[2].default_value = (32, 22, 14, 200)
            mix2_1.inputs[1].default_value = (0, 0, 0, 1)
            mix2_1.inputs[2].default_value = (1, 0.820, 0.650, 1)

            ntl(mix1.inputs[1], soft.outputs[0])
            ntl(mix1_1.inputs[1], soft_1.outputs[0])
            ntl(mix2.inputs[0], mix1.outputs[0])
            ntl(mix2_1.inputs[0], mix1_1.outputs[0])

            gam = nt.nodes.new(type="ShaderNodeGamma")
            gam.inputs[1].default_value = 2.3
            gam.location = (5131.8, 610)

            gam2 = nt.nodes.new(type="ShaderNodeGamma")
            gam2.name = "Sun_value"
            gam2.inputs[1].default_value = 1
            gam2.location = (5524.5, 610)

            gam3 = nt.nodes.new(type="ShaderNodeGamma")
            gam3.name = "Shadow_color_saturation"
            gam3.inputs[1].default_value = 1
            gam3.location = (5524.5, 880)

            sunopa = nt.nodes.new(type="ShaderNodeMixRGB")
            sunopa.blend_type = 'ADD'
            sunopa.inputs[0].default_value = 1
            sunopa.location = (5940.6, 610)
            sunopa_1 = nt.nodes.new(type="ShaderNodeMixRGB")
            sunopa_1.blend_type = 'ADD'
            sunopa_1.inputs[0].default_value = 1
            sunopa_1.location = (5524.5, 340)

            combine = nt.nodes.new(type="ShaderNodeMixRGB")
            combine.location = (6313.8, 360)
            ntl(combine.inputs[1], sunopa.outputs[0])
            ntl(combine.inputs[2], sunopa_1.outputs[0])
            lp = nt.nodes.new(type="ShaderNodeLightPath")
            lp.location = (5940.6, 130)
            ntl(combine.inputs[0], lp.outputs[0])

            ntl(gam2.inputs[0], gam.outputs[0])
            ntl(gam.inputs[0], mix2.outputs[0])
            ntl(bg.inputs[0], combine.outputs[0])

            map2 = nt.nodes.new(type="ShaderNodeMapping")
            map2.scale[2] = 6.00
            map2.scale[0] = 1.5
            map2.scale[1] = 1.5
            map2.location = (2196.6, 1510)

            n1 = nt.nodes.new(type="ShaderNodeTexNoise")
            n1.inputs[1].default_value = 3.8
            n1.inputs[2].default_value = 2.4
            n1.inputs[3].default_value = 0.5
            n1.location = (2745.24, 1780)

            n2 = nt.nodes.new(type="ShaderNodeTexNoise")
            n2.inputs[1].default_value = 2.0
            n2.inputs[2].default_value = 10
            n2.inputs[3].default_value = 0.2
            n2.location = (2745.24, 1510)

            ntl(n2.inputs[0], map2.outputs[0])
            ntl(n1.inputs[0], map2.outputs[0])

            sc1 = nt.nodes.new(type="ShaderNodeValToRGB")
            sc1.location = (3294, 1780)
            sc2 = nt.nodes.new(type="ShaderNodeValToRGB")
            sc2.location = (3294, 1510)
            sc3 = nt.nodes.new(type="ShaderNodeValToRGB")
            sc3.location = (3819.3, 820)
            sc3_1 = nt.nodes.new(type="ShaderNodeValToRGB")
            sc3_1.location = (4344.3, 1360)
            sc4 = nt.nodes.new(type="ShaderNodeValToRGB")
            sc4.location = (3819.3, 1090)

            sc1.color_ramp.elements[1].position = 0.649
            sc1.color_ramp.elements[0].position = 0.408

            sc2.color_ramp.elements[1].position = 0.576
            sc2.color_ramp.elements[0].position = 0.408

            sc3.color_ramp.elements.new(0.5)
            sc3.color_ramp.elements[2].position = 0.435

            sc3.color_ramp.elements[1].position = 0.160
            sc3.color_ramp.elements[0].position = 0.027

            sc3.color_ramp.elements[1].color = (1, 1, 1, 1)
            sc3.color_ramp.elements[0].color = (0.419, 0.419, 0.419, 0.419)

            sc3.color_ramp.elements[0].position = 0.0
            sc4.color_ramp.elements[0].position = 0.0
            sc4.color_ramp.elements[1].position = 0.469
            sc4.color_ramp.elements[1].color = (0, 0, 0, 1)
            sc4.color_ramp.elements[0].color = (1, 1, 0.917412, 1)

            sc3_1.color_ramp.elements.new(0.5)
            sc3_1.color_ramp.elements[2].position = 0.435

            sc3_1.color_ramp.elements[1].position = 0.187
            sc3_1.color_ramp.elements[1].color = (1, 1, 1, 1)
            sc3_1.color_ramp.elements[0].color = (0, 0, 0, 0)
            sc3_1.color_ramp.elements[0].position = 0.0

            smix1 = nt.nodes.new(type="ShaderNodeMixRGB")
            smix1.location = (3819.3, 1550)
            smix1.name = "Cloud_color"
            smix2 = nt.nodes.new(type="ShaderNodeMixRGB")
            smix2.location = (4344.3, 1630)
            smix2.name = "Cloud_density"
            smix2_1 = nt.nodes.new(type="ShaderNodeMixRGB")
            smix2_1.location = (4782, 1360)

            smix3 = nt.nodes.new(type="ShaderNodeMixRGB")
            smix3.location = (4344.3, 1090)
            smix3.name = "Sky_and_Horizon_colors"

            smix4 = nt.nodes.new(type="ShaderNodeMixRGB")
            smix4.location = (4782, 880)

            smix5 = nt.nodes.new(type="ShaderNodeMixRGB")
            smix5.name = "Cloud_opacity"
            smix5.location = (5131.8, 880)

            smix1.inputs[1].default_value = (1, 1, 1, 1)
            smix1.inputs[2].default_value = (0, 0, 0, 1)
            smix2.inputs[0].default_value = 0.267
            smix2.blend_type = 'MULTIPLY'
            smix2_1.inputs[0].default_value = 1
            smix2_1.blend_type = 'MULTIPLY'

            smix3.inputs[1].default_value = (0.434, 0.838, 1, 1)
            smix3.inputs[2].default_value = (0.962, 0.822, 0.822, 1)
            smix4.blend_type = 'MULTIPLY'
            smix4.inputs[0].default_value = 1
            smix5.blend_type = 'SCREEN'
            smix5.inputs[0].default_value = 1

            srgb = nt.nodes.new(type="ShaderNodeSeparateRGB")
            srgb.location = (786.54, 1370)
            aniadd = nt.nodes.new(type="ShaderNodeMath")
            aniadd.location = (1220.16, 1235)
            crgb = nt.nodes.new(type="ShaderNodeCombineRGB")
            crgb.location = (1671.33, 1510)
            sunrgb = nt.nodes.new(type="ShaderNodeMixRGB")
            sunrgb.name = "Sun_color"

            sunrgb.blend_type = 'MULTIPLY'
            sunrgb.inputs[2].default_value = (32, 30, 30, 200)
            sunrgb.inputs[0].default_value = 1
            sunrgb.location = (4344.3, 360)

            ntl(mix2.inputs[2], sunrgb.outputs[0])

            ntl(smix1.inputs[0], sc2.outputs[0])
            ntl(smix2.inputs[1], smix1.outputs[0])
            ntl(smix2.inputs[2], sc1.outputs[0])
            ntl(smix2_1.inputs[2], sc3_1.outputs[0])
            ntl(smix3.inputs[0], sc4.outputs[0])
            ntl(smix4.inputs[2], smix3.outputs[0])
            ntl(smix4.inputs[1], sc3.outputs[0])
            ntl(smix5.inputs[1], smix4.outputs[0])
            ntl(smix2_1.inputs[1], smix2.outputs[0])
            ntl(smix5.inputs[2], smix2_1.outputs[0])
            ntl(sunopa.inputs[1], gam3.outputs[0])
            ntl(gam3.inputs[0], smix5.outputs[0])
            ntl(mix1.inputs[2], sc3.outputs[0])
            ntl(sunopa.inputs[2], gam2.outputs[0])

            ntl(sc1.inputs[0], n1.outputs[0])
            ntl(sc2.inputs[0], n2.outputs[0])

            skynor = nt.nodes.new(type="ShaderNodeNormal")
            skynor.location = (3294, 1070)

            ntl(sc3.inputs[0], skynor.outputs[1])
            ntl(sc4.inputs[0], skynor.outputs[1])
            ntl(sc3_1.inputs[0], skynor.outputs[1])
            ntl(map2.inputs[0], crgb.outputs[0])
            ntl(skynor.inputs[0], tcor.outputs[0])
            ntl(mix1_1.inputs[2], sc3.outputs[0])
            ntl(srgb.inputs[0], tcor.outputs[0])
            ntl(crgb.inputs[1], srgb.outputs[1])
            ntl(crgb.inputs[2], srgb.outputs[2])
            ntl(aniadd.inputs[1], srgb.outputs[0])
            ntl(crgb.inputs[0], aniadd.outputs[0])

            ntl(cr1.inputs[0], nor.outputs[1])
            ntl(cr2.inputs[0], cr1.outputs[0])
            ntl(cr3.inputs[0], nor.outputs[1])
            ntl(nor.inputs[0], map1.outputs[0])
            ntl(map1.inputs[0], tcor.outputs[0])
            ntl(sunopa_1.inputs[1], smix5.outputs[0])
            ntl(sunopa_1.inputs[2], mix2_1.outputs[0])

            world_out = self.get_node_types(nt, "OUTPUT_WORLD")
            world_out.location = (7167.3, 360)

        except Exception as e:
            error_handlers(self, e, "Make a Procedural sky has failed")

            return {"CANCELLED"}

        return {'FINISHED'}


def draw_world_settings(col, context):
    get_world = context.scene.world
    stored_name = context.scene.dynamic_sky_name
    get_world_keys = bpy.data.worlds.keys()

    if stored_name not in get_world_keys or len(get_world_keys) < 1:
        col.label(text="The {} World could not".format(stored_name),
                 icon="INFO")
        col.label(text="be found in the Worlds' Data", icon="BLANK1")
        return

    elif not (get_world and get_world.name == stored_name):
        col.label(text="Please select the World", icon="INFO")
        col.label(text="named {}".format(stored_name), icon="BLANK1")
        col.label(text="from the Properties > World", icon="BLANK1")
        return

    pick_world = bpy.data.worlds[stored_name]
    try:
        m = pick_world.node_tree.nodes[28]

        m = pick_world.node_tree.nodes['Sky_and_Horizon_colors'].inputs[1]
        n = pick_world.node_tree.nodes['Sky_and_Horizon_colors'].inputs[2]
        c = pick_world.node_tree.nodes['Cloud_color'].inputs[1]
        o = pick_world.node_tree.nodes['Cloud_opacity'].inputs[0]
        d = pick_world.node_tree.nodes['Cloud_density'].inputs[0]
        so = pick_world.node_tree.nodes['Sun_value'].inputs[1]
        so2 = pick_world.node_tree.nodes['Shadow_color_saturation'].inputs[1]
        no = pick_world.node_tree.nodes['Sky_normal'].outputs[0]
        sof = pick_world.node_tree.nodes['Soft_hard'].inputs[0]
        bgp = pick_world.node_tree.nodes['Scene_Brightness'].inputs[1]

        suc = pick_world.node_tree.nodes['Sun_color'].inputs[1]
    except:
        col.label(text="Please Create a new World", icon="INFO")
        col.label(text="seems that there was already", icon="BLANK1")
        col.label(text="one called {}".format(stored_name), icon="BLANK1")
        return

    col.label("World: %s" % stored_name)
    col.separator()

    col.label("Scene Control")
    col.prop(bgp, "default_value", text="Brightness")
    col.prop(so2, "default_value", text="Shadow color saturation")

    col.label("Sky Control")
    col.prop(m, "default_value", text="Sky color")
    col.prop(n, "default_value", text="Horizon Color")
    col.prop(c, "default_value", text="Cloud color")
    col.prop(o, "default_value", text="Cloud opacity")
    col.prop(d, "default_value", text="Cloud density")

    col.label("Sun Control")
    col.prop(suc, "default_value", text="")
    col.prop(so, "default_value", text="Sun value")
    col.prop(sof, "default_value", text="Soft hard")

    col.prop(no, "default_value", text="")


class Dynapanel(Panel):
    bl_label = "Dynamic sky"
    bl_idname = "SCENE_PT_layout"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_context = "objectmode"
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator("sky.dyn", text="Create", icon='MAT_SPHERE_SKY')

        col = layout.column()
        draw_world_settings(col, context)


def register():
    bpy.utils.register_class(Dynapanel)
    bpy.utils.register_class(dsky)
    bpy.types.Scene.dynamic_sky_name = StringProperty(
            name="",
            default="Dynamic"
            )


def unregister():
    bpy.utils.unregister_class(Dynapanel)
    bpy.utils.unregister_class(dsky)
    del bpy.types.Scene.dynamic_sky_name


if __name__ == "__main__":
    register()
