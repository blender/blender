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

bl_info = {
    "name": "POVRAY-3.7",
    "author": "Campbell Barton, Silvio Falcinelli, Maurice Raybaud, "
              "Constantin Rahn, Bastien Montagne, Leonid Desyatkov",
    "version": (0, 0, 9),
    "blender": (2, 75, 0),
    "location": "Render > Engine > POV-Ray 3.7",
    "description": "Basic POV-Ray 3.7 integration for blender",
    "warning": "this script is RC",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Render/POV-Ray",
    "category": "Render",
}

if "bpy" in locals():
    import importlib
    importlib.reload(ui)
    importlib.reload(render)
    importlib.reload(shading)
    importlib.reload(update_files)

else:
    import bpy
    #import addon_utils # To use some other addons
    import nodeitems_utils #for Nodes
    from nodeitems_utils import NodeCategory, NodeItem #for Nodes
    from bpy.types import (
            AddonPreferences,
            PropertyGroup,
            #Operator,
            )
    from bpy.props import (
            StringProperty,
            BoolProperty,
            IntProperty,
            FloatProperty,
            FloatVectorProperty,
            EnumProperty,
            PointerProperty,
            CollectionProperty,
            )
    from . import (
            ui,
            render,
            update_files,
            )

def string_strip_hyphen(name):
    return name.replace("-", "")


###############################################################################
# Scene POV properties.
###############################################################################
class RenderPovSettingsScene(PropertyGroup):
    # File Options
    text_block = StringProperty(
            name="Text Scene Name",
            description="Name of POV-Ray scene to use. "
                        "Set when clicking Run to render current text only",
            maxlen=1024)
    tempfiles_enable = BoolProperty(
            name="Enable Tempfiles",
            description="Enable the OS-Tempfiles. Otherwise set the path where"
                        " to save the files",
            default=True)
    pov_editor = BoolProperty(
            name="POV-Ray editor",
            description="Don't Close POV-Ray editor after rendering (Overriden"
                        " by /EXIT command)",
            default=False)
    deletefiles_enable = BoolProperty(
            name="Delete files",
            description="Delete files after rendering. "
                        "Doesn't work with the image",
            default=True)
    scene_name = StringProperty(
            name="Scene Name",
            description="Name of POV-Ray scene to create. Empty name will use "
                        "the name of the blend file",
            maxlen=1024)
    scene_path = StringProperty(
            name="Export scene path",
            # Bug in POV-Ray RC3
            # description="Path to directory where the exported scene "
                        # "(POV and INI) is created",
            description="Path to directory where the files are created",
            maxlen=1024, subtype="DIR_PATH")
    renderimage_path = StringProperty(
            name="Rendered image path",
            description="Full path to directory where the rendered image is "
                        "saved",
            maxlen=1024, subtype="DIR_PATH")
    list_lf_enable = BoolProperty(
            name="LF in lists",
            description="Enable line breaks in lists (vectors and indices). "
                        "Disabled: lists are exported in one line",
            default=True)

    # Not a real pov option, just to know if we should write
    radio_enable = BoolProperty(
            name="Enable Radiosity",
            description="Enable POV-Rays radiosity calculation",
            default=False)

    radio_display_advanced = BoolProperty(
            name="Advanced Options",
            description="Show advanced options",
            default=False)

    media_enable = BoolProperty(
            name="Enable Media",
            description="Enable POV-Rays atmospheric media",
            default=False)
    media_samples = IntProperty(
            name="Samples",
            description="Number of samples taken from camera to first object "
                        "encountered along ray path for media calculation",
            min=1, max=100, default=35)

    media_color = FloatVectorProperty(
            name="Media Color", description="The atmospheric media color",
            precision=4, step=0.01, min=0, soft_max=1,
            default=(0.001, 0.001, 0.001),
            options={'ANIMATABLE'},
            subtype='COLOR')

    baking_enable = BoolProperty(
            name="Enable Baking",
            description="Enable POV-Rays texture baking",
            default=False)
    indentation_character = EnumProperty(
            name="Indentation",
            description="Select the indentation type",
            items=(('NONE', "None", "No indentation"),
                   ('TAB', "Tabs", "Indentation with tabs"),
                   ('SPACE', "Spaces", "Indentation with spaces")),
            default='SPACE')
    indentation_spaces = IntProperty(
            name="Quantity of spaces",
            description="The number of spaces for indentation",
            min=1, max=10, default=4)

    comments_enable = BoolProperty(
            name="Enable Comments",
            description="Add comments to pov file",
            default=True)

    # Real pov options
    command_line_switches = StringProperty(
            name="Command Line Switches",
            description="Command line switches consist of a + (plus) or - "
                        "(minus) sign, followed by one or more alphabetic "
                        "characters and possibly a numeric value",
            maxlen=500)

    antialias_enable = BoolProperty(
            name="Anti-Alias", description="Enable Anti-Aliasing",
            default=True)

    antialias_method = EnumProperty(
            name="Method",
            description="AA-sampling method. Type 1 is an adaptive, "
                        "non-recursive, super-sampling method. Type 2 is an "
                        "adaptive and recursive super-sampling method. Type 3 "
                        "is a stochastic halton based super-sampling method",
            items=(("0", "non-recursive AA", "Type 1 Sampling in POV-Ray"),
                   ("1", "recursive AA", "Type 2 Sampling in POV-Ray"),
                   ("2", "stochastic AA", "Type 3 Sampling in UberPOV")),
            default="1")

    antialias_confidence = FloatProperty(
            name="Antialias Confidence",
            description="how surely the computed color "
                        "of a given pixel is indeed"
                        "within the threshold error margin",
            min=0.0001, max=1.0000, default=0.9900, precision=4)
    antialias_depth = IntProperty(
            name="Antialias Depth", description="Depth of pixel for sampling",
            min=1, max=9, default=3)

    antialias_threshold = FloatProperty(
            name="Antialias Threshold", description="Tolerance for sub-pixels",
            min=0.0, max=1.0, soft_min=0.05, soft_max=0.5, default=0.03)

    jitter_enable = BoolProperty(
            name="Jitter",
            description="Enable Jittering. Adds noise into the sampling "
                        "process (it should be avoided to use jitter in "
                        "animation)",
            default=False)

    jitter_amount = FloatProperty(
            name="Jitter Amount", description="Amount of jittering",
            min=0.0, max=1.0, soft_min=0.01, soft_max=1.0, default=1.0)

    antialias_gamma = FloatProperty(
            name="Antialias Gamma",
            description="POV-Ray compares gamma-adjusted values for super "
                        "sampling. Antialias Gamma sets the Gamma before "
                        "comparison",
            min=0.0, max=5.0, soft_min=0.01, soft_max=2.5, default=2.5)

    max_trace_level = IntProperty(
            name="Max Trace Level",
            description="Number of reflections/refractions allowed on ray "
                        "path",
            min=1, max=256, default=5)

#######NEW from Lanuhum
    adc_bailout_enable = BoolProperty(
            name="Enable",
            description="",
            default=False)

    adc_bailout = FloatProperty(
            name="ADC Bailout",
            description="",
            min=0.0, max=1000.0,default=0.00392156862745, precision=3)

    ambient_light_enable = BoolProperty(
            name="Enable",
            description="",
            default=False)

    ambient_light = FloatVectorProperty(
            name="Ambient Light",
            description="Ambient light is used to simulate the effect of inter-diffuse reflection",
            precision=4, step=0.01, min=0, soft_max=1,
            default=(1, 1, 1), options={'ANIMATABLE'}, subtype='COLOR',
    )
    global_settings_advanced = BoolProperty(
            name="Advanced",
            description="",
            default=False)

    irid_wavelength_enable = BoolProperty(
            name="Enable",
            description="",
            default=False)

    irid_wavelength = FloatVectorProperty(
            name="Irid Wavelength",
            description=(
                "Iridescence calculations depend upon the dominant "
                "wavelengths of the primary colors of red, green and blue light"
            ),
            precision=4, step=0.01, min=0, soft_max=1,
            default=(0.25,0.18,0.14), options={'ANIMATABLE'}, subtype='COLOR')

    charset = EnumProperty(
            name="Charset",
            description="This allows you to specify the assumed character set of all text strings",
            items=(("ascii", "ASCII", ""),
                   ("utf8", "UTF-8", ""),
                   ("sys", "SYS", "")),
            default="utf8")

    max_intersections_enable = BoolProperty(
            name="Enable",
            description="",
            default=False)

    max_intersections = IntProperty(
            name="Max Intersections",
            description="POV-Ray uses a set of internal stacks to collect ray/object intersection points",
            min=2, max=1024, default=64)

    number_of_waves_enable = BoolProperty(
            name="Enable",
            description="",
            default=False)

    number_of_waves = IntProperty(
            name="Number Waves",
            description=(
                "The waves and ripples patterns are generated by summing a series of waves, "
                "each with a slightly different center and size"
            ),
            min=1, max=10, default=1000)

    noise_generator_enable = BoolProperty(
            name="Enable",
            description="",
            default=False)

    noise_generator = IntProperty(
            name="Noise Generator",
            description="There are three noise generators implemented",
            min=1, max=3, default=2)

    ########################### PHOTONS #######################################
    photon_enable = BoolProperty(
            name="Photons",
            description="Enable global photons",
            default=False)

    photon_enable_count = BoolProperty(
            name="Spacing / Count",
            description="Enable count photons",
            default=False)

    photon_count = IntProperty(
            name="Count",
            description="Photons count",
            min=1, max=100000000, default=20000)

    photon_spacing = FloatProperty(
            name="Spacing",
            description="Average distance between photons on surfaces. half "
                        "this get four times as many surface photons",
            min=0.001, max=1.000, default=0.005,
            soft_min=0.001, soft_max=1.000, precision=3)

    photon_max_trace_level = IntProperty(
            name="Max Trace Level",
            description="Number of reflections/refractions allowed on ray "
                        "path",
            min=1, max=256, default=5)

    photon_adc_bailout = FloatProperty(
            name="ADC Bailout",
            description="The adc_bailout for photons. Use adc_bailout = "
                        "0.01 / brightest_ambient_object for good results",
            min=0.0, max=1000.0, default=0.1,
            soft_min=0.0, soft_max=1.0, precision=3)

    photon_gather_min = IntProperty(
            name="Gather Min", description="Minimum number of photons gathered"
                                           "for each point",
            min=1, max=256, default=20)

    photon_gather_max = IntProperty(
            name="Gather Max", description="Maximum number of photons gathered for each point",
            min=1, max=256, default=100)

    photon_map_file_save_load = EnumProperty(
            name="Operation",
            description="Load or Save photon map file",
            items=(("NONE", "None", ""),
                   ("save", "Save", ""),
                   ("load", "Load", "")),
            default="NONE")

    photon_map_filename = StringProperty(
            name="Filename",
            description="",
            maxlen=1024)

    photon_map_dir = StringProperty(
            name="Directory",
            description="",
            maxlen=1024, subtype="DIR_PATH")

    photon_map_file = StringProperty(
            name="File",
            description="",
            maxlen=1024, subtype="FILE_PATH")


    radio_adc_bailout = FloatProperty(
            name="ADC Bailout",
            description="The adc_bailout for radiosity rays. Use "
                        "adc_bailout = 0.01 / brightest_ambient_object for good results",
            min=0.0, max=1000.0, soft_min=0.0, soft_max=1.0, default=0.0039, precision=4)

    radio_always_sample = BoolProperty(
            name="Always Sample",
            description="Only use the data from the pretrace step and not gather "
                        "any new samples during the final radiosity pass",
            default=False)

    radio_brightness = FloatProperty(
            name="Brightness",
            description="Amount objects are brightened before being returned "
                        "upwards to the rest of the system",
            min=0.0, max=1000.0, soft_min=0.0, soft_max=10.0, default=1.0)

    radio_count = IntProperty(
            name="Ray Count",
            description="Number of rays for each new radiosity value to be calculated "
                        "(halton sequence over 1600)",
            min=1, max=10000, soft_max=1600, default=35)

    radio_error_bound = FloatProperty(
            name="Error Bound",
            description="One of the two main speed/quality tuning values, "
                        "lower values are more accurate",
            min=0.0, max=1000.0, soft_min=0.1, soft_max=10.0, default=1.8)

    radio_gray_threshold = FloatProperty(
            name="Gray Threshold",
            description="One of the two main speed/quality tuning values, "
                        "lower values are more accurate",
            min=0.0, max=1.0, soft_min=0, soft_max=1, default=0.0)

    radio_low_error_factor = FloatProperty(
            name="Low Error Factor",
            description="Just enough samples is slightly blotchy. Low error changes error "
                        "tolerance for less critical last refining pass",
            min=0.000001, max=1.0, soft_min=0.000001, soft_max=1.0, default=0.5)

    radio_media = BoolProperty(
            name="Media", description="Radiosity estimation can be affected by media",
            default=False)

    radio_subsurface = BoolProperty(
            name="Subsurface", description="Radiosity estimation can be affected by Subsurface Light Transport",
            default=False)

    radio_minimum_reuse = FloatProperty(
            name="Minimum Reuse",
            description="Fraction of the screen width which sets the minimum radius of reuse "
                        "for each sample point (At values higher than 2% expect errors)",
            min=0.0, max=1.0, soft_min=0.1, soft_max=0.1, default=0.015, precision=3)

    radio_maximum_reuse = FloatProperty(
            name="Maximum Reuse",
            description="The maximum reuse parameter works in conjunction with, and is similar to that of minimum reuse, "
                        "the only difference being that it is an upper bound rather than a lower one",
            min=0.0, max=1.0,default=0.2, precision=3)

    radio_nearest_count = IntProperty(
            name="Nearest Count",
            description="Number of old ambient values blended together to "
                        "create a new interpolated value",
            min=1, max=20, default=5)

    radio_normal = BoolProperty(
            name="Normals", description="Radiosity estimation can be affected by normals",
            default=False)

    radio_recursion_limit = IntProperty(
            name="Recursion Limit",
            description="how many recursion levels are used to calculate "
                        "the diffuse inter-reflection",
            min=1, max=20, default=1)

    radio_pretrace_start = FloatProperty(
            name="Pretrace Start",
            description="Fraction of the screen width which sets the size of the "
                        "blocks in the mosaic preview first pass",
            min=0.01, max=1.00, soft_min=0.02, soft_max=1.0, default=0.08)

    radio_pretrace_end = FloatProperty(
            name="Pretrace End",
            description="Fraction of the screen width which sets the size of the blocks "
                        "in the mosaic preview last pass",
            min=0.000925, max=1.00, soft_min=0.01, soft_max=1.00, default=0.04, precision=3)


###############################################################################
# Material POV properties.
###############################################################################
class RenderPovSettingsMaterial(PropertyGroup):
    irid_enable = BoolProperty(
            name="Iridescence coating",
            description="Newton's thin film interference (like an oil slick on a puddle of "
                        "water or the rainbow hues of a soap bubble.)",
            default=False)

    mirror_use_IOR = BoolProperty(
            name="Correct Reflection",
            description="Use same IOR as raytrace transparency to calculate mirror reflections. "
                        "More physically correct",
            default=False)

    mirror_metallic = BoolProperty(
            name="Metallic Reflection",
            description="mirror reflections get colored as diffuse (for metallic materials)",
            default=False)

    conserve_energy = BoolProperty(
            name="Conserve Energy",
            description="Light transmitted is more correctly reduced by mirror reflections, "
                        "also the sum of diffuse and translucency gets reduced below one ",
            default=True)

    irid_amount = FloatProperty(
            name="amount",
            description="Contribution of the iridescence effect to the overall surface color. "
                        "As a rule of thumb keep to around 0.25 (25% contribution) or less, "
                        "but experiment. If the surface is coming out too white, try lowering "
                        "the diffuse and possibly the ambient values of the surface",
            min=0.0, max=1.0, soft_min=0.01, soft_max=1.0, default=0.25)

    irid_thickness = FloatProperty(
            name="thickness",
            description="A very thin film will have a high frequency of color changes while a "
                        "thick film will have large areas of color",
            min=0.0, max=1000.0, soft_min=0.1, soft_max=10.0, default=1)

    irid_turbulence = FloatProperty(
            name="turbulence", description="This parameter varies the thickness",
            min=0.0, max=10.0, soft_min=0.000, soft_max=1.0, default=0)

    interior_fade_color = FloatVectorProperty(
            name="Interior Fade Color", description="Color of filtered attenuation for transparent "
                                           "materials",
            precision=4, step=0.01, min=0.0, soft_max=1.0,
            default=(0, 0, 0), options={'ANIMATABLE'}, subtype='COLOR')

    caustics_enable = BoolProperty(
            name="Caustics",
            description="use only fake refractive caustics (default) or photon based "
                        "reflective/refractive caustics",
            default=True)

    fake_caustics = BoolProperty(
            name="Fake Caustics", description="use only (Fast) fake refractive caustics",
            default=True)

    fake_caustics_power = FloatProperty(
            name="Fake caustics power",
            description="Values typically range from 0.0 to 1.0 or higher. Zero is no caustics. "
                        "Low, non-zero values give broad hot-spots while higher values give "
                        "tighter, smaller simulated focal points",
            min=0.00, max=10.0, soft_min=0.00, soft_max=5.0, default=0.07)

    refraction_caustics = BoolProperty(
            name="Refractive Caustics", description="hotspots of light focused when going through the material",
            default=True)

    photons_dispersion = FloatProperty(
            name="Chromatic Dispersion",
            description="Light passing through will be separated according to wavelength. "
                        "This ratio of refractive indices for violet to red controls how much "
                        "the colors are spread out 1 = no dispersion, good values are 1.01 to 1.1",
            min=1.0000, max=10.000, soft_min=1.0000, soft_max=1.1000, precision=4, default=1.0000)

    photons_dispersion_samples = IntProperty(
            name="Dispersion Samples", description="Number of color-steps for dispersion",
            min=2, max=128, default=7)

    photons_reflection = BoolProperty(
            name="Reflective Photon Caustics",
            description="Use this to make your Sauron's ring ;-P",
            default=False)

    refraction_type = EnumProperty(
            items=[
                   ("1", "Fake Caustics", "use fake caustics"),
                   ("2", "Photons Caustics", "use photons for refractive caustics")],
            name="Refraction Type:",
            description="use fake caustics (fast) or true photons for refractive Caustics",
            default="1")

    ##################################CustomPOV Code############################
    replacement_text = StringProperty(
            name="Declared name:",
            description="Type the declared name in custom POV code or an external "
                        ".inc it points at. texture {} expected",
            default="")



            # NODES

    def use_material_nodes_callback(self, context):
        if hasattr(context.space_data, "tree_type"):
            context.space_data.tree_type = 'ObjectNodeTree'
        mat=context.object.active_material
        if mat.pov.material_use_nodes:
            mat.use_nodes=True
            tree = mat.node_tree
            tree.name=mat.name
            links = tree.links
            default = True
            if len(tree.nodes) == 2:
                o = 0
                m = 0
                for node in tree.nodes:
                    if node.type in {"OUTPUT","MATERIAL"}:
                        tree.nodes.remove(node)
                        default = True
                for node in tree.nodes:
                    if node.bl_idname == 'PovrayOutputNode':
                        o+=1
                    if node.bl_idname == 'PovrayTextureNode':
                        m+=1
                if o == 1 and m == 1:
                    default = False
            elif len(tree.nodes) == 0:
                default = True
            else:
                default = False
            if default:
                output = tree.nodes.new('PovrayOutputNode')
                output.location = 200,200
                tmap = tree.nodes.new('PovrayTextureNode')
                tmap.location = 0,200
                links.new(tmap.outputs[0],output.inputs[0])
                tmap.select = True
                tree.nodes.active = tmap
        else:
            mat.use_nodes=False


    def use_texture_nodes_callback(self, context):
        tex=context.object.active_material.active_texture
        if tex.pov.texture_use_nodes:
            tex.use_nodes=True
            if len(tex.node_tree.nodes)==2:
                for node in tex.node_tree.nodes:
                    if node.type in {"OUTPUT","CHECKER"}:
                        tex.node_tree.nodes.remove(node)
        else:
            tex.use_nodes=False

    def node_active_callback(self, context):
        items = []
        mat=context.material
        mat.node_tree.nodes
        for node in mat.node_tree.nodes:
            node.select=False
        for node in mat.node_tree.nodes:
            if node.name==mat.pov.material_active_node:
                node.select=True
                mat.node_tree.nodes.active=node

                return node

    def node_enum_callback(self, context):
        items = []
        mat=context.material
        nodes=mat.node_tree.nodes
        for node in nodes:
            items.append(("%s"%node.name,"%s"%node.name,""))
        return items

    def pigment_normal_callback(self, context):
        render = context.scene.pov.render
        items = [("pigment", "Pigment", ""),("normal", "Normal", "")]
        if render == 'hgpovray':
            items = [("pigment", "Pigment", ""),("normal", "Normal", ""),("modulation", "Modulation", "")]
        return items

    def glow_callback(self, context):
        scene = context.scene
        ob = context.object
        ob.pov.mesh_write_as_old = ob.pov.mesh_write_as
        if scene.pov.render == 'uberpov' and ob.pov.glow:
            ob.pov.mesh_write_as = 'NONE'
        else:
            ob.pov.mesh_write_as = ob.pov.mesh_write_as_old

    material_use_nodes = BoolProperty(name="Use nodes", description="", update=use_material_nodes_callback, default=False)
    material_active_node = EnumProperty(name="Active node", description="", items=node_enum_callback, update=node_active_callback)
    preview_settings = BoolProperty(name="Preview Settings", description="",default=False)
    object_preview_transform = BoolProperty(name="Transform object", description="",default=False)
    object_preview_scale = FloatProperty(name="XYZ", min=0.5, max=2.0, default=1.0)
    object_preview_rotate = FloatVectorProperty(name="Rotate", description="", min=-180.0, max=180.0,default=(0.0,0.0,0.0), subtype='XYZ')
    object_preview_bgcontrast = FloatProperty(name="Contrast", min=0.0, max=1.0, default=0.5)


###############################################################################
# Povray Nodes
###############################################################################
class PovraySocketUniversal(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketUniversal'
    bl_label = 'Povray Socket'
    value_unlimited = bpy.props.FloatProperty(default=0.0)
    value_0_1 = bpy.props.FloatProperty(min=0.0,max=1.0,default=0.0)
    value_0_10 = bpy.props.FloatProperty(min=0.0,max=10.0,default=0.0)
    value_000001_10 = bpy.props.FloatProperty(min=0.000001,max=10.0,default=0.0)
    value_1_9 = bpy.props.IntProperty(min=1,max=9,default=1)
    value_0_255 = bpy.props.IntProperty(min=0,max=255,default=0)
    percent = bpy.props.FloatProperty(min=0.0,max=100.0,default=0.0)
    def draw(self, context, layout, node, text):
        space = context.space_data
        tree = space.edit_tree
        links=tree.links
        if self.is_linked:
            value=[]
            for link in links:
                if link.from_node==node:
                    inps=link.to_node.inputs
                    for inp in inps:
                        if inp.bl_idname=="PovraySocketFloat_0_1" and inp.is_linked:
                            prop="value_0_1"
                            if prop not in value:
                                value.append(prop)
                        if inp.bl_idname=="PovraySocketFloat_000001_10" and inp.is_linked:
                            prop="value_000001_10"
                            if prop not in value:
                                value.append(prop)
                        if inp.bl_idname=="PovraySocketFloat_0_10" and inp.is_linked:
                            prop="value_0_10"
                            if prop not in value:
                                value.append(prop)
                        if inp.bl_idname=="PovraySocketInt_1_9" and inp.is_linked:
                            prop="value_1_9"
                            if prop not in value:
                                value.append(prop)
                        if inp.bl_idname=="PovraySocketInt_0_255" and inp.is_linked:
                            prop="value_0_255"
                            if prop not in value:
                                value.append(prop)
                        if inp.bl_idname=="PovraySocketFloatUnlimited" and inp.is_linked:
                            prop="value_unlimited"
                            if prop not in value:
                                value.append(prop)
            if len(value)==1:
                layout.prop(self, "%s"%value[0], text=text)
            else:
                layout.prop(self, "percent", text="Percent")
        else:
            layout.prop(self, "percent", text=text)
    def draw_color(self, context, node):
        return (1, 0, 0, 1)

class PovraySocketFloat_0_1(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloat_0_1'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(description="Input node Value_0_1",min=0,max=1,default=0)
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)

    def draw_color(self, context, node):
        return (0.5, 0.7, 0.7, 1)

class PovraySocketFloat_0_10(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloat_0_10'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(description="Input node Value_0_10",min=0,max=10,default=0)
    def draw(self, context, layout, node, text):
        if node.bl_idname == 'ShaderNormalMapNode' and node.inputs[2].is_linked:
            layout.label('')
            self.hide_value=True
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)
    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)

class PovraySocketFloat_10(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloat_10'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(description="Input node Value_10",min=-10,max=10,default=0)
    def draw(self, context, layout, node, text):
        if node.bl_idname == 'ShaderNormalMapNode' and node.inputs[2].is_linked:
            layout.label('')
            self.hide_value=True
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)
    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)

class PovraySocketFloatPositive(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloatPositive'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(description="Input Node Value Positive", min=0.0, default=0)
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)
    def draw_color(self, context, node):
        return (0.045, 0.005, 0.136, 1)

class PovraySocketFloat_000001_10(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloat_000001_10'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(min=0.000001,max=10,default=0.000001)
    def draw(self, context, layout, node, text):
        if self.is_output or self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)
    def draw_color(self, context, node):
        return (1, 0, 0, 1)

class PovraySocketFloatUnlimited(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloatUnlimited'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(default = 0.0)
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)
    def draw_color(self, context, node):
        return (0.7, 0.7, 1, 1)

class PovraySocketInt_1_9(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketInt_1_9'
    bl_label = 'Povray Socket'
    default_value = bpy.props.IntProperty(description="Input node Value_1_9",min=1,max=9,default=6)
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text)
    def draw_color(self, context, node):
        return (1, 0.7, 0.7, 1)

class PovraySocketInt_0_256(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketInt_0_256'
    bl_label = 'Povray Socket'
    default_value = bpy.props.IntProperty(min=0,max=255,default=0)
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text)
    def draw_color(self, context, node):
        return (0.5, 0.5, 0.5, 1)


class PovraySocketPattern(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketPattern'
    bl_label = 'Povray Socket'

    default_value = bpy.props.EnumProperty(
            name="Pattern",
            description="Select the pattern",
            items=(('boxed', "Boxed", ""),('brick', "Brick", ""),('cells', "Cells", ""), ('checker', "Checker", ""),
                   ('granite', "Granite", ""),('leopard', "Leopard", ""),('marble', "Marble", ""),
                   ('onion', "Onion", ""),('planar', "Planar", ""), ('quilted', "Quilted", ""),
                   ('ripples', "Ripples", ""),  ('radial', "Radial", ""),('spherical', "Spherical", ""),
                   ('spotted', "Spotted", ""), ('waves', "Waves", ""), ('wood', "Wood", ""),
                   ('wrinkles', "Wrinkles", "")),
            default='granite')

    def draw(self, context, layout, node, text):
        if self.is_output or self.is_linked:
            layout.label("Pattern")
        else:
            layout.prop(self, "default_value", text=text)

    def draw_color(self, context, node):
        return (1, 1, 1, 1)

class PovraySocketColor(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketColor'
    bl_label = 'Povray Socket'

    default_value = bpy.props.FloatVectorProperty(
            precision=4, step=0.01, min=0, soft_max=1,
            default=(0.0, 0.0, 0.0), options={'ANIMATABLE'}, subtype='COLOR')

    def draw(self, context, layout, node, text):
        if self.is_output or self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text)

    def draw_color(self, context, node):
        return (1, 1, 0, 1)

class PovraySocketColorRGBFT(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketColorRGBFT'
    bl_label = 'Povray Socket'

    default_value = bpy.props.FloatVectorProperty(
            precision=4, step=0.01, min=0, soft_max=1,
            default=(0.0, 0.0, 0.0), options={'ANIMATABLE'}, subtype='COLOR')
    f = bpy.props.FloatProperty(default = 0.0,min=0.0,max=1.0)
    t = bpy.props.FloatProperty(default = 0.0,min=0.0,max=1.0)
    def draw(self, context, layout, node, text):
        if self.is_output or self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text)

    def draw_color(self, context, node):
        return (1, 1, 0, 1)

class PovraySocketTexture(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketTexture'
    bl_label = 'Povray Socket'
    default_value = bpy.props.IntProperty()
    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        return (0, 1, 0, 1)



class PovraySocketTransform(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketTransform'
    bl_label = 'Povray Socket'
    default_value = bpy.props.IntProperty(min=0,max=255,default=0)
    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        return (99/255, 99/255, 199/255, 1)

class PovraySocketNormal(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketNormal'
    bl_label = 'Povray Socket'
    default_value = bpy.props.IntProperty(min=0,max=255,default=0)
    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)

class PovraySocketSlope(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketSlope'
    bl_label = 'Povray Socket'
    default_value = bpy.props.FloatProperty(min = 0.0, max = 1.0)
    height = bpy.props.FloatProperty(min = 0.0, max = 10.0)
    slope = bpy.props.FloatProperty(min = -10.0, max = 10.0)
    def draw(self, context, layout, node, text):
        if self.is_output or self.is_linked:
            layout.label(text)
        else:
            layout.prop(self,'default_value',text='')
            layout.prop(self,'height',text='')
            layout.prop(self,'slope',text='')
    def draw_color(self, context, node):
        return (0, 0, 0, 1)

class PovraySocketMap(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketMap'
    bl_label = 'Povray Socket'
    default_value = bpy.props.StringProperty()
    def draw(self, context, layout, node, text):
        layout.label(text)
    def draw_color(self, context, node):
        return (0.2, 0, 0.2, 1)

class PovrayShaderNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'ObjectNodeTree'

class PovrayTextureNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'TextureNodeTree'

class PovraySceneNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'CompositorNodeTree'

node_categories = [

    PovrayShaderNodeCategory("SHADEROUTPUT", "Output", items=[
        NodeItem("PovrayOutputNode"),
        ]),

    PovrayShaderNodeCategory("SIMPLE", "Simple texture", items=[
        NodeItem("PovrayTextureNode"),
        ]),

    PovrayShaderNodeCategory("MAPS", "Maps", items=[
        NodeItem("PovrayBumpMapNode"),
        NodeItem("PovrayColorImageNode"),
        NodeItem("ShaderNormalMapNode"),
        NodeItem("PovraySlopeNode"),
        NodeItem("ShaderTextureMapNode"),
        NodeItem("ShaderNodeValToRGB"),
        ]),

    PovrayShaderNodeCategory("OTHER", "Other patterns", items=[
        NodeItem("PovrayImagePatternNode"),
        NodeItem("ShaderPatternNode"),
        ]),

    PovrayShaderNodeCategory("COLOR", "Color", items=[
        NodeItem("PovrayPigmentNode"),
        ]),

    PovrayShaderNodeCategory("TRANSFORM", "Transform", items=[
        NodeItem("PovrayMappingNode"),
        NodeItem("PovrayMultiplyNode"),
        NodeItem("PovrayModifierNode"),
        NodeItem("PovrayTransformNode"),
        NodeItem("PovrayValueNode"),
        ]),

    PovrayShaderNodeCategory("FINISH", "Finish", items=[
        NodeItem("PovrayFinishNode"),
        NodeItem("PovrayDiffuseNode"),
        NodeItem("PovraySpecularNode"),
        NodeItem("PovrayPhongNode"),
        NodeItem("PovrayAmbientNode"),
        NodeItem("PovrayMirrorNode"),
        NodeItem("PovrayIridescenceNode"),
        NodeItem("PovraySubsurfaceNode"),
        ]),

    PovrayShaderNodeCategory("CYCLES", "Cycles", items=[
        NodeItem("ShaderNodeAddShader"),
        NodeItem("ShaderNodeAmbientOcclusion"),
        NodeItem("ShaderNodeAttribute"),
        NodeItem("ShaderNodeBackground"),
        NodeItem("ShaderNodeBlackbody"),
        NodeItem("ShaderNodeBrightContrast"),
        NodeItem("ShaderNodeBsdfAnisotropic"),
        NodeItem("ShaderNodeBsdfDiffuse"),
        NodeItem("ShaderNodeBsdfGlass"),
        NodeItem("ShaderNodeBsdfGlossy"),
        NodeItem("ShaderNodeBsdfHair"),
        NodeItem("ShaderNodeBsdfRefraction"),
        NodeItem("ShaderNodeBsdfToon"),
        NodeItem("ShaderNodeBsdfTranslucent"),
        NodeItem("ShaderNodeBsdfTransparent"),
        NodeItem("ShaderNodeBsdfVelvet"),
        NodeItem("ShaderNodeBump"),
        NodeItem("ShaderNodeCameraData"),
        NodeItem("ShaderNodeCombineHSV"),
        NodeItem("ShaderNodeCombineRGB"),
        NodeItem("ShaderNodeCombineXYZ"),
        NodeItem("ShaderNodeEmission"),
        NodeItem("ShaderNodeExtendedMaterial"),
        NodeItem("ShaderNodeFresnel"),
        NodeItem("ShaderNodeGamma"),
        NodeItem("ShaderNodeGeometry"),
        NodeItem("ShaderNodeGroup"),
        NodeItem("ShaderNodeHairInfo"),
        NodeItem("ShaderNodeHoldout"),
        NodeItem("ShaderNodeHueSaturation"),
        NodeItem("ShaderNodeInvert"),
        NodeItem("ShaderNodeLampData"),
        NodeItem("ShaderNodeLayerWeight"),
        NodeItem("ShaderNodeLightFalloff"),
        NodeItem("ShaderNodeLightPath"),
        NodeItem("ShaderNodeMapping"),
        NodeItem("ShaderNodeMaterial"),
        NodeItem("ShaderNodeMath"),
        NodeItem("ShaderNodeMixRGB"),
        NodeItem("ShaderNodeMixShader"),
        NodeItem("ShaderNodeNewGeometry"),
        NodeItem("ShaderNodeNormal"),
        NodeItem("ShaderNodeNormalMap"),
        NodeItem("ShaderNodeObjectInfo"),
        NodeItem("ShaderNodeOutput"),
        NodeItem("ShaderNodeOutputLamp"),
        NodeItem("ShaderNodeOutputLineStyle"),
        NodeItem("ShaderNodeOutputMaterial"),
        NodeItem("ShaderNodeOutputWorld"),
        NodeItem("ShaderNodeParticleInfo"),
        NodeItem("ShaderNodeRGB"),
        NodeItem("ShaderNodeRGBCurve"),
        NodeItem("ShaderNodeRGBToBW"),
        NodeItem("ShaderNodeScript"),
        NodeItem("ShaderNodeSeparateHSV"),
        NodeItem("ShaderNodeSeparateRGB"),
        NodeItem("ShaderNodeSeparateXYZ"),
        NodeItem("ShaderNodeSqueeze"),
        NodeItem("ShaderNodeSubsurfaceScattering"),
        NodeItem("ShaderNodeTangent"),
        NodeItem("ShaderNodeTexBrick"),
        NodeItem("ShaderNodeTexChecker"),
        NodeItem("ShaderNodeTexCoord"),
        NodeItem("ShaderNodeTexEnvironment"),
        NodeItem("ShaderNodeTexGradient"),
        NodeItem("ShaderNodeTexImage"),
        NodeItem("ShaderNodeTexMagic"),
        NodeItem("ShaderNodeTexMusgrave"),
        NodeItem("ShaderNodeTexNoise"),
        NodeItem("ShaderNodeTexPointDensity"),
        NodeItem("ShaderNodeTexSky"),
        NodeItem("ShaderNodeTexVoronoi"),
        NodeItem("ShaderNodeTexWave"),
        NodeItem("ShaderNodeTexture"),
        NodeItem("ShaderNodeUVAlongStroke"),
        NodeItem("ShaderNodeUVMap"),
        NodeItem("ShaderNodeValToRGB"),
        NodeItem("ShaderNodeValue"),
        NodeItem("ShaderNodeVectorCurve"),
        NodeItem("ShaderNodeVectorMath"),
        NodeItem("ShaderNodeVectorTransform"),
        NodeItem("ShaderNodeVolumeAbsorption"),
        NodeItem("ShaderNodeVolumeScatter"),
        NodeItem("ShaderNodeWavelength"),
        NodeItem("ShaderNodeWireframe"),
        ]),

    PovrayTextureNodeCategory("TEXTUREOUTPUT", "Output", items=[
        NodeItem("TextureNodeValToRGB"),
        NodeItem("TextureOutputNode"),
        ]),

    PovraySceneNodeCategory("ISOSURFACE", "Isosurface", items=[
        NodeItem("IsoPropsNode"),
        ]),

    PovraySceneNodeCategory("FOG", "Fog", items=[
        NodeItem("PovrayFogNode"),

        ]),
    ]
############### end nodes

###############################################################################
# Texture POV properties.
###############################################################################
class RenderPovSettingsTexture(PropertyGroup):
    #Custom texture gamma
    tex_gamma_enable = BoolProperty(
            name="Enable custom texture gamma",
            description="Notify some custom gamma for which texture has been precorrected "
                        "without the file format carrying it and only if it differs from your "
                        "OS expected standard (see pov doc)",
            default=False)

    tex_gamma_value = FloatProperty(
            name="Custom texture gamma",
            description="value for which the file was issued e.g. a Raw photo is gamma 1.0",
            min=0.45, max=5.00, soft_min=1.00, soft_max=2.50, default=1.00)

    ##################################CustomPOV Code############################
    #commented out below if we wanted custom pov code in texture only, inside exported material:
    #replacement_text = StringProperty(
    #        name="Declared name:",
    #        description="Type the declared name in custom POV code or an external .inc "
    #                    "it points at. pigment {} expected",
    #        default="")



    tex_pattern_type = EnumProperty(
            name="Texture_Type",
            description="Choose between Blender or POV-Ray parameters to specify texture",
            items= (('agate', 'Agate', '','PLUGIN', 0),
                   ('aoi', 'Aoi', '', 'PLUGIN', 1),
                   ('average', 'Average', '', 'PLUGIN', 2),
                   ('boxed', 'Boxed', '', 'PLUGIN', 3),
                   ('bozo', 'Bozo', '', 'PLUGIN', 4),
                   ('bumps', 'Bumps', '', 'PLUGIN', 5),
                   ('cells', 'Cells', '', 'PLUGIN', 6),
                   ('crackle', 'Crackle', '', 'PLUGIN', 7),
                   ('cubic', 'Cubic', '', 'PLUGIN', 8),
                   ('cylindrical', 'Cylindrical', '', 'PLUGIN', 9),
                   ('density_file', 'Density', '(.df3)', 'PLUGIN', 10),
                   ('dents', 'Dents', '', 'PLUGIN', 11),
                   ('fractal', 'Fractal', '', 'PLUGIN', 12),
                   ('function', 'Function', '', 'PLUGIN', 13),
                   ('gradient', 'Gradient', '', 'PLUGIN', 14),
                   ('granite', 'Granite', '', 'PLUGIN', 15),
                   ('image_pattern', 'Image pattern', '', 'PLUGIN', 16),
                   ('leopard', 'Leopard', '', 'PLUGIN', 17),
                   ('marble', 'Marble', '', 'PLUGIN', 18),
                   ('onion', 'Onion', '', 'PLUGIN', 19),
                   ('pigment_pattern', 'pigment pattern', '', 'PLUGIN', 20),
                   ('planar', 'Planar', '', 'PLUGIN', 21),
                   ('quilted', 'Quilted', '', 'PLUGIN', 22),
                   ('radial', 'Radial', '', 'PLUGIN', 23),
                   ('ripples', 'Ripples', '', 'PLUGIN', 24),
                   ('slope', 'Slope', '', 'PLUGIN', 25),
                   ('spherical', 'Spherical', '', 'PLUGIN', 26),
                   ('spiral1', 'Spiral1', '', 'PLUGIN', 27),
                   ('spiral2', 'Spiral2', '', 'PLUGIN', 28),
                   ('spotted', 'Spotted', '', 'PLUGIN', 29),
                   ('waves', 'Waves', '', 'PLUGIN', 30),
                   ('wood', 'Wood', '', 'PLUGIN', 31),
                   ('wrinkles', 'Wrinkles', '', 'PLUGIN', 32),
                   ('brick', "Brick", "", 'PLUGIN', 33),
                   ('checker', "Checker", "", 'PLUGIN', 34),
                   ('hexagon', "Hexagon", "", 'PLUGIN', 35),
                   ('object', "Mesh", "", 'PLUGIN', 36),
                   ('emulator', "Internal Emulator", "", 'PLUG', 37)),
            default='emulator',
            )

    magnet_style = EnumProperty(
            name="Magnet style",
            description="magnet or julia",
            items=(('mandel', "Mandelbrot", ""),('julia', "Julia", "")),
            default='julia')

    magnet_type = IntProperty(
            name="Magnet_type",
            description="1 or 2",
            min=1, max=2, default=2)

    warp_types = EnumProperty(
            name="Warp Types",
            description="Select the type of warp",
            items=(('PLANAR', "Planar", ""), ('CUBIC', "Cubic", ""),
                   ('SPHERICAL', "Spherical", ""), ('TOROIDAL', "Toroidal", ""),
                   ('CYLINDRICAL', "Cylindrical", ""), ('NONE', "None", "No indentation")),
            default='NONE')

    warp_orientation = EnumProperty(
            name="Warp Orientation",
            description="Select the orientation of warp",
            items=(('x', "X", ""), ('y', "Y", ""), ('z', "Z", "")),
            default='y')

    wave_type = EnumProperty(
            name="Waves type",
            description="Select the type of waves",
            items=(('ramp', "Ramp", ""), ('sine', "Sine", ""), ('scallop', "Scallop", ""),
                   ('cubic', "Cubic", ""), ('poly', "Poly", ""), ('triangle', 'Triangle', "")),
            default='ramp')

    gen_noise = IntProperty(
            name="Noise Generators",
            description="Noise Generators",
            min=1, max=3, default=1)

    warp_dist_exp = FloatProperty(
            name="Distance exponent",
            description="Distance exponent",
            min=0.0, max=100.0, default=1.0)

    warp_tor_major_radius = FloatProperty(
            name="Major radius",
            description="Torus is distance from major radius",
            min=0.0, max=5.0, default=1.0)


    warp_turbulence_x = FloatProperty(
            name="Turbulence X",
            description="Turbulence X",
            min=0.0, max=5.0, default=0.0)

    warp_turbulence_y = FloatProperty(
            name="Turbulence Y",
            description="Turbulence Y",
            min=0.0, max=5.0, default=0.0)

    warp_turbulence_z = FloatProperty(
            name="Turbulence Z",
            description="Turbulence Z",
            min=0.0, max=5.0, default=0.0)

    modifier_octaves = IntProperty(
            name="Turbulence octaves",
            description="Turbulence octaves",
            min=1, max=10, default=1)

    modifier_lambda = FloatProperty(
            name="Turbulence lambda",
            description="Turbulence lambda",
            min=0.0, max=5.0, default=1.00)

    modifier_omega = FloatProperty(
            name="Turbulence omega",
            description="Turbulence omega",
            min=0.0, max=10.0, default=1.00)

    modifier_phase = FloatProperty(
            name="Phase",
            description="The phase value causes the map entries to be shifted so that the map "
                        "starts and ends at a different place",
            min=0.0, max=2.0, default=0.0)

    modifier_frequency = FloatProperty(
            name="Frequency",
            description="The frequency keyword adjusts the number of times that a color map "
                        "repeats over one cycle of a pattern",
            min=0.0, max=25.0, default=2.0)

    modifier_turbulence = FloatProperty(
            name="Turbulence",
            description="Turbulence",
            min=0.0, max=5.0, default=2.0)

    modifier_numbers = IntProperty(
            name="Numbers",
            description="Numbers",
            min=1, max=27, default=2)

    modifier_control0 = IntProperty(
            name="Control0",
            description="Control0",
            min=0, max=100, default=1)

    modifier_control1 = IntProperty(
            name="Control1",
            description="Control1",
            min=0, max=100, default=1)

    brick_size_x = FloatProperty(
            name="Brick size x",
            description="",
            min=0.0000, max=1.0000, default=0.2500)

    brick_size_y = FloatProperty(
            name="Brick size y",
            description="",
            min=0.0000, max=1.0000, default=0.0525)

    brick_size_z = FloatProperty(
            name="Brick size z",
            description="",
            min=0.0000, max=1.0000, default=0.1250)

    brick_mortar = FloatProperty(
            name="Mortar",
            description="Mortar",
            min=0.000, max=1.500, default=0.01)

    julia_complex_1 = FloatProperty(
            name="Julia Complex 1",
            description="",
            min=0.000, max=1.500, default=0.360)

    julia_complex_2 = FloatProperty(
            name="Julia Complex 2",
            description="",
            min=0.000, max=1.500, default=0.250)

    f_iter = IntProperty(
            name="Fractal Iteration",
            description="",
            min=0, max=100, default=20)

    f_exponent = IntProperty(
            name="Fractal Exponent",
            description="",
            min=2, max=33, default=2)

    f_ior = IntProperty(
            name="Fractal Interior",
            description="",
            min=1, max=6, default=1)

    f_ior_fac = FloatProperty(
            name="Fractal Interior Factor",
            description="",
            min=0.0, max=10.0, default=1.0)

    f_eor = IntProperty(
            name="Fractal Exterior",
            description="",
            min=1, max=8, default=1)

    f_eor_fac = FloatProperty(
            name="Fractal Exterior Factor",
            description="",
            min=0.0, max=10.0, default=1.0)

    grad_orient_x= IntProperty(
            name="Gradient orientation X",
            description="",
            min=0, max=1, default=0)

    grad_orient_y= IntProperty(
            name="Gradient orientation Y",
            description="",
            min=0, max=1, default=1)

    grad_orient_z= IntProperty(
            name="Gradient orientation Z",
            description="",
            min=0, max=1, default=0)

    pave_sides = EnumProperty(
            name="Pavement sides",
            description="",
            items=(('3', "3", ""), ('4', "4", ""), ('6', "6", "")),
            default='3')

    pave_pat_2= IntProperty(
            name="Pavement pattern 2",
            description="maximum: 2",
            min=1, max=2, default=2)

    pave_pat_3= IntProperty(
            name="Pavement pattern 3",
            description="maximum: 3",
            min=1, max=3, default=3)

    pave_pat_4= IntProperty(
            name="Pavement pattern 4",
            description="maximum: 4",
            min=1, max=4, default=4)

    pave_pat_5= IntProperty(
            name="Pavement pattern 5",
            description="maximum: 5",
            min=1, max=5, default=5)

    pave_pat_7= IntProperty(
            name="Pavement pattern 7",
            description="maximum: 7",
            min=1, max=7, default=7)

    pave_pat_12= IntProperty(
            name="Pavement pattern 12",
            description="maximum: 12",
            min=1, max=12, default=12)

    pave_pat_22= IntProperty(
            name="Pavement pattern 22",
            description="maximum: 22",
            min=1, max=22, default=22)

    pave_pat_35= IntProperty(
            name="Pavement pattern 35",
            description="maximum: 35",
            min=1, max=35, default=35)

    pave_tiles= IntProperty(
            name="Pavement tiles",
            description="If sides = 6, maximum tiles 5!!!",
            min=1, max=6, default=1)

    pave_form= IntProperty(
            name="Pavement form",
            description="",
            min=0, max=4, default=0)

    #########FUNCTIONS#############################################################################
    #########FUNCTIONS#############################################################################

    func_list = EnumProperty(
            name="Functions",
            description="Select the function for create pattern",
            items=(('NONE', "None", "No indentation"),
                   ("f_algbr_cyl1","Algbr cyl1",""), ("f_algbr_cyl2","Algbr cyl2",""),
                   ("f_algbr_cyl3","Algbr cyl3",""), ("f_algbr_cyl4","Algbr cyl4",""),
                   ("f_bicorn","Bicorn",""), ("f_bifolia","Bifolia",""),
                   ("f_blob","Blob",""), ("f_blob2","Blob2",""),
                   ("f_boy_surface","Boy surface",""), ("f_comma","Comma",""),
                   ("f_cross_ellipsoids","Cross ellipsoids",""),
                   ("f_crossed_trough","Crossed trough",""), ("f_cubic_saddle","Cubic saddle",""),
                   ("f_cushion","Cushion",""), ("f_devils_curve","Devils curve",""),
                   ("f_devils_curve_2d","Devils curve 2d",""),
                   ("f_dupin_cyclid","Dupin cyclid",""), ("f_ellipsoid","Ellipsoid",""),
                   ("f_enneper","Enneper",""), ("f_flange_cover","Flange cover",""),
                   ("f_folium_surface","Folium surface",""),
                   ("f_folium_surface_2d","Folium surface 2d",""), ("f_glob","Glob",""),
                   ("f_heart","Heart",""), ("f_helical_torus","Helical torus",""),
                   ("f_helix1","Helix1",""), ("f_helix2","Helix2",""), ("f_hex_x","Hex x",""),
                   ("f_hex_y","Hex y",""), ("f_hetero_mf","Hetero mf",""),
                   ("f_hunt_surface","Hunt surface",""),
                   ("f_hyperbolic_torus","Hyperbolic torus",""),
                   ("f_isect_ellipsoids","Isect ellipsoids",""),
                   ("f_kampyle_of_eudoxus","Kampyle of eudoxus",""),
                   ("f_kampyle_of_eudoxus_2d","Kampyle of eudoxus 2d",""),
                   ("f_klein_bottle","Klein bottle",""),
                   ("f_kummer_surface_v1","Kummer surface v1",""),
                   ("f_kummer_surface_v2","Kummer surface v2",""),
                   ("f_lemniscate_of_gerono","Lemniscate of gerono",""),
                   ("f_lemniscate_of_gerono_2d","Lemniscate of gerono 2d",""),
                   ("f_mesh1","Mesh1",""), ("f_mitre","Mitre",""),
                   ("f_nodal_cubic","Nodal cubic",""), ("f_noise3d","Noise3d",""),
                   ("f_noise_generator","Noise generator",""), ("f_odd","Odd",""),
                   ("f_ovals_of_cassini","Ovals of cassini",""), ("f_paraboloid","Paraboloid",""),
                   ("f_parabolic_torus","Parabolic torus",""), ("f_ph","Ph",""),
                   ("f_pillow","Pillow",""), ("f_piriform","Piriform",""),
                   ("f_piriform_2d","Piriform 2d",""), ("f_poly4","Poly4",""),
                   ("f_polytubes","Polytubes",""), ("f_quantum","Quantum",""),
                   ("f_quartic_paraboloid","Quartic paraboloid",""),
                   ("f_quartic_saddle","Quartic saddle",""),
                   ("f_quartic_cylinder","Quartic cylinder",""), ("f_r","R",""),
                   ("f_ridge","Ridge",""), ("f_ridged_mf","Ridged mf",""),
                   ("f_rounded_box","Rounded box",""), ("f_sphere","Sphere",""),
                   ("f_spikes","Spikes",""), ("f_spikes_2d","Spikes 2d",""),
                   ("f_spiral","Spiral",""), ("f_steiners_roman","Steiners roman",""),
                   ("f_strophoid","Strophoid",""), ("f_strophoid_2d","Strophoid 2d",""),
                   ("f_superellipsoid","Superellipsoid",""), ("f_th","Th",""),
                   ("f_torus","Torus",""), ("f_torus2","Torus2",""),
                   ("f_torus_gumdrop","Torus gumdrop",""), ("f_umbrella","Umbrella",""),
                   ("f_witch_of_agnesi","Witch of agnesi",""),
                   ("f_witch_of_agnesi_2d","Witch of agnesi 2d","")),

            default='NONE')

    func_x = FloatProperty(
            name="FX",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_plus_x = EnumProperty(
            name="Func plus x",
            description="",
            items=(('NONE', "None", ""), ('increase', "*", ""), ('plus', "+", "")),
            default='NONE')

    func_y = FloatProperty(
            name="FY",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_plus_y = EnumProperty(
            name="Func plus y",
            description="",
            items=(('NONE', "None", ""), ('increase', "*", ""), ('plus', "+", "")),
            default='NONE')

    func_z = FloatProperty(
            name="FZ",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_plus_z = EnumProperty(
            name="Func plus z",
            description="",
            items=(('NONE', "None", ""), ('increase', "*", ""), ('plus', "+", "")),
            default='NONE')

    func_P0 = FloatProperty(
            name="P0",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P1 = FloatProperty(
            name="P1",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P2 = FloatProperty(
            name="P2",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P3 = FloatProperty(
            name="P3",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P4 = FloatProperty(
            name="P4",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P5 = FloatProperty(
            name="P5",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P6 = FloatProperty(
            name="P6",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P7 = FloatProperty(
            name="P7",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P8 = FloatProperty(
            name="P8",
            description="",
            min=0.0, max=25.0, default=1.0)

    func_P9 = FloatProperty(
            name="P9",
            description="",
            min=0.0, max=25.0, default=1.0)

    #########################################
    tex_rot_x = FloatProperty(
            name="Rotate X",
            description="",
            min=-180.0, max=180.0, default=0.0)

    tex_rot_y = FloatProperty(
            name="Rotate Y",
            description="",
            min=-180.0, max=180.0, default=0.0)

    tex_rot_z = FloatProperty(
            name="Rotate Z",
            description="",
            min=-180.0, max=180.0, default=0.0)

    tex_mov_x = FloatProperty(
            name="Move X",
            description="",
            min=-100000.0, max=100000.0, default=0.0)

    tex_mov_y = FloatProperty(
            name="Move Y",
            description="",
            min=-100000.0, max=100000.0, default=0.0)

    tex_mov_z = FloatProperty(
            name="Move Z",
            description="",
            min=-100000.0, max=100000.0, default=0.0)

    tex_scale_x = FloatProperty(
            name="Scale X",
            description="",
            min=0.0, max=10000.0, default=1.0)

    tex_scale_y = FloatProperty(
            name="Scale Y",
            description="",
            min=0.0, max=10000.0, default=1.0)

    tex_scale_z = FloatProperty(
            name="Scale Z",
            description="",
            min=0.0, max=10000.0, default=1.0)


###############################################################################
# Object POV properties.
###############################################################################

class RenderPovSettingsObject(PropertyGroup):
    # Pov inside_vector used for CSG
    inside_vector = FloatVectorProperty(
            name="CSG Inside Vector", description="Direction to shoot CSG inside test rays at",
            precision=4, step=0.01, min=0, soft_max=1,
            default=(0.001, 0.001, 0.5),
            options={'ANIMATABLE'},
            subtype='XYZ')
            
    # Importance sampling
    importance_value = FloatProperty(
            name="Radiosity Importance",
            description="Priority value relative to other objects for sampling radiosity rays. "
                        "Increase to get more radiosity rays at comparatively small yet "
                        "bright objects",
            min=0.01, max=1.00, default=0.50)

    # Collect photons
    collect_photons = BoolProperty(
            name="Receive Photon Caustics",
            description="Enable object to collect photons from other objects caustics. Turn "
                        "off for objects that don't really need to receive caustics (e.g. objects"
                        " that generate caustics often don't need to show any on themselves)",
            default=True)

    # Photons spacing_multiplier
    spacing_multiplier = FloatProperty(
            name="Photons Spacing Multiplier",
            description="Multiplier value relative to global spacing of photons. "
                        "Decrease by half to get 4x more photons at surface of "
                        "this object (or 8x media photons than specified in the globals",
            min=0.01, max=1.00, default=1.00)

    ##################################CustomPOV Code############################
    # Only DUMMIES below for now:
    replacement_text = StringProperty(
            name="Declared name:",
            description="Type the declared name in custom POV code or an external .inc "
                        "it points at. Any POV shape expected e.g: isosurface {}",
            default="")

    #############POV-Ray specific object properties.############################
    object_as = StringProperty(maxlen=1024)

    imported_loc = FloatVectorProperty(
        name="Imported Pov location",
        precision=6,
        default=(0.0, 0.0, 0.0))

    imported_loc_cap = FloatVectorProperty(
        name="Imported Pov location",
        precision=6,
        default=(0.0, 0.0, 2.0))

    unlock_parameters = BoolProperty(name="Lock",default = False)
    
    # not in UI yet but used for sor (lathe) / prism... pov primitives
    curveshape = EnumProperty(
            name="Povray Shape Type",
            items=(("birail", "Birail", ""),
                   ("cairo", "Cairo", ""),
                   ("lathe", "Lathe", ""),
                   ("loft", "Loft", ""),
                   ("prism", "Prism", ""),
                   ("sphere_sweep", "Sphere Sweep", "")),
            default="sphere_sweep")

    mesh_write_as = EnumProperty(
            name="Mesh Write As",
            items=(("blobgrid", "Blob Grid", ""),
                   ("grid", "Grid", ""),
                   ("mesh", "Mesh", "")),
            default="mesh")

    object_ior = FloatProperty(
            name="IOR", description="IOR",
            min=1.0, max=10.0,default=1.0)

    # shape_as_light = StringProperty(name="Light",maxlen=1024)
    # fake_caustics_power = FloatProperty(
            # name="Power", description="Fake caustics power",
            # min=0.0, max=10.0,default=0.0)
    # target = BoolProperty(name="Target",description="",default=False)
    # target_value = FloatProperty(
            # name="Value", description="",
            # min=0.0, max=1.0,default=1.0)
    # refraction = BoolProperty(name="Refraction",description="",default=False)
    # dispersion = BoolProperty(name="Dispersion",description="",default=False)
    # dispersion_value = FloatProperty(
            # name="Dispersion", description="Good values are 1.01 to 1.1. ",
            # min=1.0, max=1.2,default=1.01)
    # dispersion_samples = IntProperty(name="Samples",min=2, max=100,default=7)
    # reflection = BoolProperty(name="Reflection",description="",default=False)
    # pass_through = BoolProperty(name="Pass through",description="",default=False)
    no_shadow = BoolProperty(name="No Shadow",default=False)

    no_image = BoolProperty(name="No Image",default=False)

    no_reflection = BoolProperty(name="No Reflection",default=False)

    no_radiosity = BoolProperty(name="No Radiosity",default=False)

    inverse = BoolProperty(name="Inverse",default=False)

    sturm = BoolProperty(name="Sturm",default=False)

    double_illuminate = BoolProperty(name="Double Illuminate",default=False)

    hierarchy = BoolProperty(name="Hierarchy",default=False)

    hollow = BoolProperty(name="Hollow",default=False)

    boundorclip = EnumProperty(
            name="Boundorclip",
            items=(("none", "None", ""),
                   ("bounded_by", "Bounded_by", ""),
                   ("clipped_by", "Clipped_by", "")),
            default="none")
    boundorclipob = StringProperty(maxlen=1024)

    addboundorclip = BoolProperty(description="",default=False)

    blob_threshold = FloatProperty(name="Threshold",min=0.00, max=10.0, default=0.6)

    blob_strength = FloatProperty(name="Strength",min=-10.00, max=10.0, default=1.00)

    res_u = IntProperty(name="U",min=100, max=1000, default=500)

    res_v = IntProperty(name="V",min=100, max=1000, default=500)

    contained_by = EnumProperty(
            name="Contained by",
            items=(("box", "Box", ""),
                   ("sphere", "Sphere", "")),
            default="box")

    container_scale = FloatProperty(name="Container Scale",min=0.0, max=10.0, default=1.00)

    threshold = FloatProperty(name="Threshold",min=0.0, max=10.0, default=0.00)

    accuracy = FloatProperty(name="Accuracy",min=0.0001, max=0.1, default=0.001)

    max_gradient = FloatProperty(name="Max Gradient",min=0.0, max=100.0, default=5.0)

    all_intersections = BoolProperty(name="All Intersections",default=False)

    max_trace = IntProperty(name="Max Trace",min=1, max=100,default=1)


    def prop_update_cylinder(self, context):
        if bpy.ops.pov.cylinder_update.poll():
            bpy.ops.pov.cylinder_update()
    cylinder_radius = FloatProperty(name="Cylinder R",min=0.00, max=10.0, default=0.04, update=prop_update_cylinder)
    cylinder_location_cap = FloatVectorProperty(
            name="Cylinder Cap Location", subtype='TRANSLATION',
            description="The position of the 'other' end of the cylinder (relative to object location)",
            default=(0.0, 0.0, 2.0), update=prop_update_cylinder,
    )

    imported_cyl_loc = FloatVectorProperty(
        name="Imported Pov location",
        precision=6,
        default=(0.0, 0.0, 0.0))

    imported_cyl_loc_cap = FloatVectorProperty(
        name="Imported Pov location",
        precision=6,
        default=(0.0, 0.0, 2.0))

    def prop_update_sphere(self, context):
        bpy.ops.pov.sphere_update()
    sphere_radius = FloatProperty(name="Sphere radius",min=0.00, max=10.0, default=0.5, update=prop_update_sphere)


    def prop_update_cone(self, context):
        bpy.ops.pov.cone_update()

    cone_base_radius = FloatProperty(
        name = "Base radius", description = "The first radius of the cone",
        default = 1.0, min = 0.01, max = 100.0, update=prop_update_cone)
    cone_cap_radius = FloatProperty(
        name = "Cap radius", description = "The second radius of the cone",
        default = 0.3, min = 0.0, max = 100.0, update=prop_update_cone)

    cone_segments = IntProperty(
        name = "Segments", description = "Radial segmentation of proxy mesh",
        default = 16, min = 3, max = 265, update=prop_update_cone)

    cone_height = FloatProperty(
        name = "Height", description = "Height of the cone",
        default = 2.0, min = 0.01, max = 100.0, update=prop_update_cone)

    cone_base_z = FloatProperty()
    cone_cap_z = FloatProperty()

###########Parametric
    def prop_update_parametric(self, context):
        bpy.ops.pov.parametric_update()

    u_min = FloatProperty(name = "U Min",
                    description = "",
                    default = 0.0, update=prop_update_parametric)
    v_min = FloatProperty(name = "V Min",
                    description = "",
                    default = 0.0, update=prop_update_parametric)
    u_max = FloatProperty(name = "U Max",
                    description = "",
                    default = 6.28, update=prop_update_parametric)
    v_max = FloatProperty(name = "V Max",
                    description = "",
                    default = 12.57, update=prop_update_parametric)
    x_eq = StringProperty(
                    maxlen=1024, default = "cos(v)*(1+cos(u))*sin(v/8)", update=prop_update_parametric)
    y_eq = StringProperty(
                    maxlen=1024, default = "sin(u)*sin(v/8)+cos(v/8)*1.5", update=prop_update_parametric)
    z_eq = StringProperty(
                    maxlen=1024, default = "sin(v)*(1+cos(u))*sin(v/8)", update=prop_update_parametric)

###########Torus

    def prop_update_torus(self, context):
        bpy.ops.pov.torus_update()

    torus_major_segments = IntProperty(
                    name = "Segments", description = "Radial segmentation of proxy mesh",
                    default = 48, min = 3, max = 720, update=prop_update_torus)
    torus_minor_segments = IntProperty(
                    name = "Segments", description = "Cross-section segmentation of proxy mesh",
                    default = 12, min = 3, max = 720, update=prop_update_torus)
    torus_major_radius = FloatProperty(
                    name="Major radius",
                    description="Major radius",
                    min=0.00, max=100.00, default=1.0, update=prop_update_torus)
    torus_minor_radius = FloatProperty(
                    name="Minor radius",
                    description="Minor radius",
                    min=0.00, max=100.00, default=0.25, update=prop_update_torus)


###########Rainbow
    arc_angle = FloatProperty(name = "Arc angle",
                      description = "The angle of the raynbow arc in degrees",
                      default = 360, min = 0.01, max = 360.0)
    falloff_angle = FloatProperty(name = "Falloff angle",
                      description = "The angle after which rainbow dissolves into background",
                      default = 360, min = 0.0, max = 360)

###########HeightFields

    quality = IntProperty(name = "Quality",
                      description = "",
                      default = 100, min = 1, max = 100)

    hf_filename = StringProperty(maxlen = 1024)

    hf_gamma = FloatProperty(
            name="Gamma",
            description="Gamma",
            min=0.0001, max=20.0, default=1.0)

    hf_premultiplied = BoolProperty(
            name="Premultiplied",
            description="Premultiplied",
            default=True)

    hf_smooth = BoolProperty(
            name="Smooth",
            description="Smooth",
            default=False)

    hf_water = FloatProperty(
            name="Water Level",
            description="Wather Level",
            min=0.00, max=1.00, default=0.0)

    hf_hierarchy = BoolProperty(
            name="Hierarchy",
            description="Height field hierarchy",
            default=True)

##############Superellipsoid
    def prop_update_superellipsoid(self, context):
        bpy.ops.pov.superellipsoid_update()

    se_param1 = FloatProperty(
            name="Parameter 1",
            description="",
            min=0.00, max=10.0, default=0.04)

    se_param2 = FloatProperty(
            name="Parameter 2",
            description="",
            min=0.00, max=10.0, default=0.04)

    se_u = IntProperty(name = "U-segments",
                    description = "radial segmentation",
                    default = 20, min = 4, max = 265,
                    update=prop_update_superellipsoid)
    se_v = IntProperty(name = "V-segments",
                    description = "lateral segmentation",
                    default = 20, min = 4, max = 265,
                    update=prop_update_superellipsoid)
    se_n1 = FloatProperty(name = "Ring manipulator",
                    description = "Manipulates the shape of the Ring",
                    default = 1.0, min = 0.01, max = 100.0,
                    update=prop_update_superellipsoid)
    se_n2 = FloatProperty(name = "Cross manipulator",
                    description = "Manipulates the shape of the cross-section",
                    default = 1.0, min = 0.01, max = 100.0,
                    update=prop_update_superellipsoid)
    se_edit = EnumProperty(items=[("NOTHING", "Nothing", ""),
                                ("NGONS", "N-Gons", ""),
                                ("TRIANGLES", "Triangles", "")],
                    name="Fill up and down",
                    description="",
                    default='TRIANGLES',
                    update=prop_update_superellipsoid)
#############Used for loft and Superellipsoid, etc.
    curveshape = EnumProperty(
            name="Povray Shape Type",
            items=(("birail", "Birail", ""),
                   ("cairo", "Cairo", ""),
                   ("lathe", "Lathe", ""),
                   ("loft", "Loft", ""),
                   ("prism", "Prism", ""),
                   ("sphere_sweep", "Sphere Sweep", ""),
                   ("sor", "Surface of Revolution", "")),
            default="sphere_sweep")

#############Supertorus
    def prop_update_supertorus(self, context):
        bpy.ops.pov.supertorus_update()

    st_major_radius = FloatProperty(
            name="Major radius",
            description="Major radius",
            min=0.00, max=100.00, default=1.0,
            update=prop_update_supertorus)

    st_minor_radius = FloatProperty(
            name="Minor radius",
            description="Minor radius",
            min=0.00, max=100.00, default=0.25,
            update=prop_update_supertorus)

    st_ring = FloatProperty(
            name="Ring",
            description="Ring manipulator",
            min=0.0001, max=100.00, default=1.00,
            update=prop_update_supertorus)

    st_cross = FloatProperty(
            name="Cross",
            description="Cross manipulator",
            min=0.0001, max=100.00, default=1.00,
            update=prop_update_supertorus)

    st_accuracy = FloatProperty(
            name="Accuracy",
            description="Supertorus accuracy",
            min=0.00001, max=1.00, default=0.001)

    st_max_gradient = FloatProperty(
            name="Gradient",
            description="Max gradient",
            min=0.0001, max=100.00, default=10.00,
            update=prop_update_supertorus)

    st_R = FloatProperty(name = "big radius",
                      description = "The radius inside the tube",
                      default = 1.0, min = 0.01, max = 100.0,
                      update=prop_update_supertorus)
    st_r = FloatProperty(name = "small radius",
                      description = "The radius of the tube",
                      default = 0.3, min = 0.01, max = 100.0,
                      update=prop_update_supertorus)
    st_u = IntProperty(name = "U-segments",
                    description = "radial segmentation",
                    default = 16, min = 3, max = 265,
                    update=prop_update_supertorus)
    st_v = IntProperty(name = "V-segments",
                    description = "lateral segmentation",
                    default = 8, min = 3, max = 265,
                    update=prop_update_supertorus)
    st_n1 = FloatProperty(name = "Ring manipulator",
                      description = "Manipulates the shape of the Ring",
                      default = 1.0, min = 0.01, max = 100.0,
                      update=prop_update_supertorus)
    st_n2 = FloatProperty(name = "Cross manipulator",
                      description = "Manipulates the shape of the cross-section",
                      default = 1.0, min = 0.01, max = 100.0,
                      update=prop_update_supertorus)
    st_ie = BoolProperty(name = "Use Int.+Ext. radii",
                      description = "Use internal and external radii",
                      default = False,
                      update=prop_update_supertorus)
    st_edit = BoolProperty(name="",
                        description="",
                        default=False,
                        options={'HIDDEN'},
                        update=prop_update_supertorus)

########################Loft
    loft_n = IntProperty(name = "Segments",
                    description = "Vertical segments",
                    default = 16, min = 3, max = 720)
    loft_rings_bottom = IntProperty(name = "Bottom",
                    description = "Bottom rings",
                    default = 5, min = 2, max = 100)
    loft_rings_side = IntProperty(name = "Side",
                    description = "Side rings",
                    default = 10, min = 2, max = 100)
    loft_thick = FloatProperty(name = "Thickness",
                      description = "Manipulates the shape of the Ring",
                      default = 0.3, min = 0.01, max = 1.0)
    loft_r = FloatProperty(name = "Radius",
                      description = "Radius",
                      default = 1, min = 0.01, max = 10)
    loft_height = FloatProperty(name = "Height",
                      description = "Manipulates the shape of the Ring",
                      default = 2, min = 0.01, max = 10.0)

###################Prism
    prism_n = IntProperty(name = "Sides",
                    description = "Number of sides",
                    default = 5, min = 3, max = 720)
    prism_r = FloatProperty(name = "Radius",
                    description = "Radius",
                    default = 1.0)

##################Isosurface
    iso_function_text = StringProperty(name="Function Text",maxlen=1024)#,update=iso_props_update_callback)

##################PolygonToCircle
    polytocircle_resolution = IntProperty(name = "Resolution",
                    description = "",
                    default = 3, min = 0, max = 256)
    polytocircle_ngon = IntProperty(name = "NGon",
                    description = "",
                    min = 3, max = 64,default = 5)
    polytocircle_ngonR = FloatProperty(name = "NGon Radius",
                    description = "",
                    default = 0.3)
    polytocircle_circleR = FloatProperty(name = "Circle Radius",
                    description = "",
                    default = 1.0)

                    
###############################################################################
# Modifiers POV properties.
###############################################################################
#class RenderPovSettingsModifier(PropertyGroup):
    boolean_mod = EnumProperty(
            name="Operation",
            description="Choose the type of calculation for Boolean modifier",
            items=(("BMESH", "Use the BMesh Boolean Solver", ""),
                   ("CARVE", "Use the Carve Boolean Solver", ""),
                   ("POV", "Use Pov-Ray Constructive Solid Geometry", "")),
            default="BMESH")
                    
#################Avogadro
    # filename_ext = ".png"

    # filter_glob = StringProperty(
            # default="*.exr;*.gif;*.hdr;*.iff;*.jpeg;*.jpg;*.pgm;*.png;*.pot;*.ppm;*.sys;*.tga;*.tiff;*.EXR;*.GIF;*.HDR;*.IFF;*.JPEG;*.JPG;*.PGM;*.PNG;*.POT;*.PPM;*.SYS;*.TGA;*.TIFF",
            # options={'HIDDEN'},
            # )

###############################################################################
# Camera POV properties.
###############################################################################
class RenderPovSettingsCamera(PropertyGroup):
    #DOF Toggle
    dof_enable = BoolProperty(
            name="Depth Of Field", description="EnablePOV-Ray Depth Of Field ",
            default=False)

    # Aperture (Intensity of the Blur)
    dof_aperture = FloatProperty(
            name="Aperture",
            description="Similar to a real camera's aperture effect over focal blur (though not "
                        "in physical units and independant of focal length). "
                        "Increase to get more blur",
            min=0.01, max=1.00, default=0.50)

    # Aperture adaptive sampling
    dof_samples_min = IntProperty(
            name="Samples Min", description="Minimum number of rays to use for each pixel",
            min=1, max=128, default=3)

    dof_samples_max = IntProperty(
            name="Samples Max", description="Maximum number of rays to use for each pixel",
            min=1, max=128, default=9)

    dof_variance = IntProperty(
            name="Variance",
            description="Minimum threshold (fractional value) for adaptive DOF sampling (up "
                        "increases quality and render time). The value for the variance should "
                        "be in the range of the smallest displayable color difference",
            min=1, max=100000, soft_max=10000, default=8192)

    dof_confidence = FloatProperty(
            name="Confidence",
            description="Probability to reach the real color value. Larger confidence values "
                        "will lead to more samples, slower traces and better images",
            min=0.01, max=0.99, default=0.20)

    normal_enable = BoolProperty(name="Perturbated Camera", default=False)
    cam_normal = FloatProperty(name="Normal Strenght", min=0.0, max=1.0, default=0.0)
    normal_patterns = EnumProperty(
            name="Pattern",
            description="",
            items=(('agate', "Agate", ""), ('boxed', "Boxed", ""), ('bumps', "Bumps", ""), ('cells', "Cells", ""),
                   ('crackle', "Crackle", ""),('dents', "Dents", ""),
                   ('granite', "Granite", ""),
                   ('leopard', "Leopard", ""),
                   ('marble', "Marble", ""), ('onion', "Onion", ""), ('pavement', "Pavement", ""), ('planar', "Planar", ""),
                   ('quilted', "Quilted", ""), ('ripples', "Ripples", ""),  ('radial', "Radial", ""),
                   ('spherical', "Spherical", ""),('spiral1', "Spiral1", ""), ('spiral2', "Spiral2", ""), ('spotted', "Spotted", ""),
                   ('square', "Square", ""),('tiling', "Tiling", ""),
                   ('waves', "Waves", ""), ('wood', "Wood", ""),('wrinkles', "Wrinkles", "")),
            default='agate')
    turbulence = FloatProperty(name="Turbulence", min=0.0, max=100.0, default=0.1)
    scale = FloatProperty(name="Scale", min=0.0,default=1.0)

    ##################################CustomPOV Code############################
    # Only DUMMIES below for now:
    replacement_text = StringProperty(
            name="Texts in blend file",
            description="Type the declared name in custom POV code or an external .inc "
                        "it points at. camera {} expected",
            default="")



###############################################################################
# Text POV properties.
###############################################################################
class RenderPovSettingsText(PropertyGroup):
    custom_code = EnumProperty(
            name="Custom Code",
            description="rendered source: Both adds text at the "
                        "top of the exported POV-Ray file",
            items=(("3dview", "View", ""),
                   ("text", "Text", ""),
                   ("both", "Both", "")),
            default="text")

###############################################################################
# Povray Preferences.
###############################################################################
class PovrayPreferences(AddonPreferences):
    bl_idname = __name__

    branch_feature_set_povray = EnumProperty(
                name="Feature Set",
                description="Choose between official (POV-Ray) or (UberPOV) "
                            "development branch features to write in the pov file",
                items= (('povray', 'Official POV-Ray', '','PLUGIN', 0),
                        ('uberpov', 'Unofficial UberPOV', '', 'PLUGIN', 1)),
                default='povray'
                )

    filepath_povray = StringProperty(
                name="Binary Location",
                description="Path to renderer executable",
                subtype='FILE_PATH',
                )
    docpath_povray = StringProperty(
                name="Includes Location",
                description="Path to Insert Menu files",
                subtype='FILE_PATH',
                )
    def draw(self, context):
        layout = self.layout
        layout.prop(self, "branch_feature_set_povray")
        layout.prop(self, "filepath_povray")
        layout.prop(self, "docpath_povray")












def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_add.prepend(ui.menu_func_add)
    bpy.types.INFO_MT_file_import.append(ui.menu_func_import)
    bpy.types.TEXT_MT_templates.append(ui.menu_func_templates)
    # was used for parametric objects but made the other addon unreachable on
    # unregister for other tools to use created a user action call instead
    #addon_utils.enable("add_mesh_extra_objects", default_set=False, persistent=True)

    #bpy.types.TEXTURE_PT_context_texture.prepend(TEXTURE_PT_povray_type)

    bpy.types.NODE_HT_header.append(ui.menu_func_nodes)
    nodeitems_utils.register_node_categories("POVRAYNODES", node_categories)
    bpy.types.Scene.pov = PointerProperty(type=RenderPovSettingsScene)
    #bpy.types.Modifier.pov = PointerProperty(type=RenderPovSettingsModifier)
    bpy.types.Material.pov = PointerProperty(type=RenderPovSettingsMaterial)
    bpy.types.Texture.pov = PointerProperty(type=RenderPovSettingsTexture)
    bpy.types.Object.pov = PointerProperty(type=RenderPovSettingsObject)
    bpy.types.Camera.pov = PointerProperty(type=RenderPovSettingsCamera)
    bpy.types.Text.pov = PointerProperty(type=RenderPovSettingsText)



def unregister():
    del bpy.types.Scene.pov
    del bpy.types.Material.pov
    #del bpy.types.Modifier.pov 
    del bpy.types.Texture.pov
    del bpy.types.Object.pov
    del bpy.types.Camera.pov
    del bpy.types.Text.pov
    nodeitems_utils.unregister_node_categories("POVRAYNODES")
    bpy.types.NODE_HT_header.remove(ui.menu_func_nodes)

    #bpy.types.TEXTURE_PT_context_texture.remove(TEXTURE_PT_povray_type)
    #addon_utils.disable("add_mesh_extra_objects", default_set=False)
    bpy.types.TEXT_MT_templates.remove(ui.menu_func_templates)
    bpy.types.INFO_MT_file_import.remove(ui.menu_func_import)
    bpy.types.INFO_MT_add.remove(ui.menu_func_add)
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
