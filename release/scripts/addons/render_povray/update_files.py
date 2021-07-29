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
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        FloatProperty,
        FloatVectorProperty,
        EnumProperty,
        )


def update2_0_0_9():
    # Temporally register old props, so we can access their values.
    register()

    # Mapping old names -> old default values
    # XXX We could also store the new name, but as it is just the same without leading pov_ ...
    # Get default values of pov scene props.
    old_sce_props = {}
    for k in ["pov_tempfiles_enable", "pov_deletefiles_enable", "pov_scene_name", "pov_scene_path",
              "pov_renderimage_path", "pov_list_lf_enable", "pov_radio_enable",
              "pov_radio_display_advanced", "pov_media_enable", "pov_media_samples", "pov_media_color",
              "pov_baking_enable", "pov_indentation_character", "pov_indentation_spaces",
              "pov_comments_enable", "pov_command_line_switches", "pov_antialias_enable",
              "pov_antialias_method", "pov_antialias_depth", "pov_antialias_threshold",
              "pov_jitter_enable", "pov_jitter_amount", "pov_antialias_gamma", "pov_max_trace_level",
              "pov_photon_spacing", "pov_photon_max_trace_level", "pov_photon_adc_bailout",
              "pov_photon_gather_min", "pov_photon_gather_max", "pov_radio_adc_bailout",
              "pov_radio_always_sample", "pov_radio_brightness", "pov_radio_count",
              "pov_radio_error_bound", "pov_radio_gray_threshold", "pov_radio_low_error_factor",
              "pov_radio_media", "pov_radio_minimum_reuse", "pov_radio_nearest_count",
              "pov_radio_normal", "pov_radio_recursion_limit", "pov_radio_pretrace_start",
              "pov_radio_pretrace_end"]:
        old_sce_props[k] = getattr(bpy.types.Scene, k)[1].get('default', None)

    # Get default values of pov material props.
    old_mat_props = {}
    for k in ["pov_irid_enable", "pov_mirror_use_IOR", "pov_mirror_metallic", "pov_conserve_energy",
              "pov_irid_amount", "pov_irid_thickness", "pov_irid_turbulence", "pov_interior_fade_color",
              "pov_caustics_enable", "pov_fake_caustics", "pov_fake_caustics_power",
              "pov_photons_refraction", "pov_photons_dispersion", "pov_photons_reflection",
              "pov_refraction_type", "pov_replacement_text"]:
        old_mat_props[k] = getattr(bpy.types.Material, k)[1].get('default', None)

    # Get default values of pov texture props.
    old_tex_props = {}
    for k in ["pov_tex_gamma_enable", "pov_tex_gamma_value", "pov_replacement_text"]:
        old_tex_props[k] = getattr(bpy.types.Texture, k)[1].get('default', None)

    # Get default values of pov object props.
    old_obj_props = {}
    for k in ["pov_importance_value", "pov_collect_photons", "pov_replacement_text"]:
        old_obj_props[k] = getattr(bpy.types.Object, k)[1].get('default', None)

    # Get default values of pov camera props.
    old_cam_props = {}
    for k in ["pov_dof_enable", "pov_dof_aperture", "pov_dof_samples_min", "pov_dof_samples_max",
              "pov_dof_variance", "pov_dof_confidence", "pov_replacement_text"]:
        old_cam_props[k] = getattr(bpy.types.Camera, k)[1].get('default', None)

    # Get default values of pov text props.
    old_txt_props = {}
    for k in ["pov_custom_code"]:
        old_txt_props[k] = getattr(bpy.types.Text, k)[1].get('default', None)

    ################################################################################################
    # Now, update !
    # For each old pov property of each scene, if its value is not equal to the default one,
    # copy it to relevant new prop...
    for sce in bpy.data.scenes:
        for k, d in old_sce_props.items():
            val = getattr(sce, k, d)
            if val != d:
                setattr(sce.pov, k[4:], val)
    # The same goes for materials, textures, etc.
    for mat in bpy.data.materials:
        for k, d in old_mat_props.items():
            val = getattr(mat, k, d)
            if val != d:
                setattr(mat.pov, k[4:], val)
    for tex in bpy.data.textures:
        for k, d in old_tex_props.items():
            val = getattr(tex, k, d)
            if val != d:
                setattr(tex.pov, k[4:], val)
    for obj in bpy.data.objects:
        for k, d in old_obj_props.items():
            val = getattr(obj, k, d)
            if val != d:
                setattr(obj.pov, k[4:], val)
    for cam in bpy.data.cameras:
        for k, d in old_cam_props.items():
            val = getattr(cam, k, d)
            if val != d:
                setattr(cam.pov, k[4:], val)
    for txt in bpy.data.texts:
        for k, d in old_txt_props.items():
            val = getattr(txt, k, d)
            if val != d:
                setattr(txt.pov, k[4:], val)
    # Finally, unregister old props !
    unregister()


class RenderCopySettings(bpy.types.Operator):
    """Update old POV properties to new ones"""
    bl_idname = "scene.pov_update_properties"
    bl_label = "PovRay render: Update to script v0.0.9"
    bl_option = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        update2_0_0_9()
        return {'FINISHED'}


def register():
    Scene = bpy.types.Scene
    Mat = bpy.types.Material
    Tex = bpy.types.Texture
    Obj = bpy.types.Object
    Cam = bpy.types.Camera
    Text = bpy.types.Text
    ###########################SCENE##################################

    # File Options
    Scene.pov_tempfiles_enable = BoolProperty(
            name="Enable Tempfiles",
            description="Enable the OS-Tempfiles. Otherwise set the path where to save the files",
            default=True)
    Scene.pov_deletefiles_enable = BoolProperty(
            name="Delete files",
            description="Delete files after rendering. Doesn't work with the image",
            default=True)
    Scene.pov_scene_name = StringProperty(
            name="Scene Name",
            description="Name of POV-Ray scene to create. Empty name will use the name of the blend file",
            default="", maxlen=1024)
    Scene.pov_scene_path = StringProperty(
            name="Export scene path",
            # description="Path to directory where the exported scene (POV and INI) is created",  # Bug in POV-Ray RC3
            description="Path to directory where the files are created",
            default="", maxlen=1024, subtype="DIR_PATH")
    Scene.pov_renderimage_path = StringProperty(
            name="Rendered image path",
            description="Full path to directory where the rendered image is saved",
            default="", maxlen=1024, subtype="DIR_PATH")
    Scene.pov_list_lf_enable = BoolProperty(
            name="LF in lists",
            description="Enable line breaks in lists (vectors and indices). Disabled: lists are exported in one line",
            default=True)

    # Not a real pov option, just to know if we should write
    Scene.pov_radio_enable = BoolProperty(
            name="Enable Radiosity",
            description="Enable POV-Rays radiosity calculation",
            default=False)
    Scene.pov_radio_display_advanced = BoolProperty(
            name="Advanced Options",
            description="Show advanced options",
            default=False)
    Scene.pov_media_enable = BoolProperty(
            name="Enable Media",
            description="Enable POV-Rays atmospheric media",
            default=False)
    Scene.pov_media_samples = IntProperty(
            name="Samples", description="Number of samples taken from camera to first object encountered along ray path for media calculation",
            min=1, max=100, default=35)

    Scene.pov_media_color = FloatVectorProperty(
            name="Media Color",
            description="The atmospheric media color",
            subtype='COLOR',
            precision=4,
            step=0.01,
            min=0,
            soft_max=1,
            default=(0.001, 0.001, 0.001),
            options={'ANIMATABLE'})

    Scene.pov_baking_enable = BoolProperty(
            name="Enable Baking",
            description="Enable POV-Rays texture baking",
            default=False)
    Scene.pov_indentation_character = EnumProperty(
            name="Indentation",
            description="Select the indentation type",
            items=(("0", "None", "No indentation"),
               ("1", "Tabs", "Indentation with tabs"),
               ("2", "Spaces", "Indentation with spaces")),
            default="2")
    Scene.pov_indentation_spaces = IntProperty(
            name="Quantity of spaces",
            description="The number of spaces for indentation",
            min=1, max=10, default=4)

    Scene.pov_comments_enable = BoolProperty(
            name="Enable Comments",
            description="Add comments to pov file",
            default=True)

    # Real pov options
    Scene.pov_command_line_switches = StringProperty(name="Command Line Switches",
            description="Command line switches consist of a + (plus) or - (minus) sign, followed by one or more alphabetic characters and possibly a numeric value",
            default="", maxlen=500)

    Scene.pov_antialias_enable = BoolProperty(
            name="Anti-Alias", description="Enable Anti-Aliasing",
            default=True)

    Scene.pov_antialias_method = EnumProperty(
            name="Method",
            description="AA-sampling method. Type 1 is an adaptive, non-recursive, super-sampling method. Type 2 is an adaptive and recursive super-sampling method",
            items=(("0", "non-recursive AA", "Type 1 Sampling in POV-Ray"),
               ("1", "recursive AA", "Type 2 Sampling in POV-Ray")),
            default="1")

    Scene.pov_antialias_depth = IntProperty(
            name="Antialias Depth", description="Depth of pixel for sampling",
            min=1, max=9, default=3)

    Scene.pov_antialias_threshold = FloatProperty(
            name="Antialias Threshold", description="Tolerance for sub-pixels",
            min=0.0, max=1.0, soft_min=0.05, soft_max=0.5, default=0.1)

    Scene.pov_jitter_enable = BoolProperty(
            name="Jitter", description="Enable Jittering. Adds noise into the sampling process (it should be avoided to use jitter in animation)",
            default=True)

    Scene.pov_jitter_amount = FloatProperty(
            name="Jitter Amount", description="Amount of jittering",
            min=0.0, max=1.0, soft_min=0.01, soft_max=1.0, default=1.0)

    Scene.pov_antialias_gamma = FloatProperty(
            name="Antialias Gamma", description="POV-Ray compares gamma-adjusted values for super sampling. Antialias Gamma sets the Gamma before comparison",
            min=0.0, max=5.0, soft_min=0.01, soft_max=2.5, default=2.5)

    Scene.pov_max_trace_level = IntProperty(
            name="Max Trace Level", description="Number of reflections/refractions allowed on ray path",
            min=1, max=256, default=5)

    Scene.pov_photon_spacing = FloatProperty(
            name="Spacing", description="Average distance between photons on surfaces. half this get four times as many surface photons",
            min=0.001, max=1.000, soft_min=0.001, soft_max=1.000, default=0.005, precision=3)

    Scene.pov_photon_max_trace_level = IntProperty(
            name="Max Trace Level", description="Number of reflections/refractions allowed on ray path",
            min=1, max=256, default=5)

    Scene.pov_photon_adc_bailout = FloatProperty(
            name="ADC Bailout", description="The adc_bailout for photons. Use adc_bailout = 0.01 / brightest_ambient_object for good results",
            min=0.0, max=1000.0, soft_min=0.0, soft_max=1.0, default=0.1, precision=3)

    Scene.pov_photon_gather_min = IntProperty(
            name="Gather Min", description="Minimum number of photons gathered for each point",
            min=1, max=256, default=20)

    Scene.pov_photon_gather_max = IntProperty(
            name="Gather Max", description="Maximum number of photons gathered for each point",
            min=1, max=256, default=100)

    Scene.pov_radio_adc_bailout = FloatProperty(
            name="ADC Bailout", description="The adc_bailout for radiosity rays. Use adc_bailout = 0.01 / brightest_ambient_object for good results",
            min=0.0, max=1000.0, soft_min=0.0, soft_max=1.0, default=0.01, precision=3)

    Scene.pov_radio_always_sample = BoolProperty(
            name="Always Sample", description="Only use the data from the pretrace step and not gather any new samples during the final radiosity pass",
            default=True)

    Scene.pov_radio_brightness = FloatProperty(
            name="Brightness", description="Amount objects are brightened before being returned upwards to the rest of the system",
            min=0.0, max=1000.0, soft_min=0.0, soft_max=10.0, default=1.0)

    Scene.pov_radio_count = IntProperty(
            name="Ray Count", description="Number of rays for each new radiosity value to be calculated (halton sequence over 1600)",
            min=1, max=10000, soft_max=1600, default=35)

    Scene.pov_radio_error_bound = FloatProperty(
            name="Error Bound", description="One of the two main speed/quality tuning values, lower values are more accurate",
            min=0.0, max=1000.0, soft_min=0.1, soft_max=10.0, default=1.8)

    Scene.pov_radio_gray_threshold = FloatProperty(
            name="Gray Threshold", description="One of the two main speed/quality tuning values, lower values are more accurate",
            min=0.0, max=1.0, soft_min=0, soft_max=1, default=0.0)

    Scene.pov_radio_low_error_factor = FloatProperty(
            name="Low Error Factor", description="Just enough samples is slightly blotchy. Low error changes error tolerance for less critical last refining pass",
            min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, default=0.5)

    # max_sample - not available yet
    Scene.pov_radio_media = BoolProperty(
            name="Media", description="Radiosity estimation can be affected by media",
            default=False)

    Scene.pov_radio_minimum_reuse = FloatProperty(
            name="Minimum Reuse", description="Fraction of the screen width which sets the minimum radius of reuse for each sample point (At values higher than 2% expect errors)",
            min=0.0, max=1.0, soft_min=0.1, soft_max=0.1, default=0.015, precision=3)

    Scene.pov_radio_nearest_count = IntProperty(
            name="Nearest Count", description="Number of old ambient values blended together to create a new interpolated value",
            min=1, max=20, default=5)

    Scene.pov_radio_normal = BoolProperty(
            name="Normals", description="Radiosity estimation can be affected by normals",
            default=False)

    Scene.pov_radio_recursion_limit = IntProperty(
            name="Recursion Limit", description="how many recursion levels are used to calculate the diffuse inter-reflection",
            min=1, max=20, default=3)

    Scene.pov_radio_pretrace_start = FloatProperty(
            name="Pretrace Start", description="Fraction of the screen width which sets the size of the blocks in the mosaic preview first pass",
            min=0.01, max=1.00, soft_min=0.02, soft_max=1.0, default=0.08)

    Scene.pov_radio_pretrace_end = FloatProperty(
            name="Pretrace End", description="Fraction of the screen width which sets the size of the blocks in the mosaic preview last pass",
            min=0.001, max=1.00, soft_min=0.01, soft_max=1.00, default=0.04, precision=3)

    #############################MATERIAL######################################

    Mat.pov_irid_enable = BoolProperty(
            name="Enable Iridescence",
            description="Newton's thin film interference (like an oil slick on a puddle of water or the rainbow hues of a soap bubble.)",
            default=False)

    Mat.pov_mirror_use_IOR = BoolProperty(
            name="Correct Reflection",
            description="Use same IOR as raytrace transparency to calculate mirror reflections. More physically correct",
            default=False)

    Mat.pov_mirror_metallic = BoolProperty(
            name="Metallic Reflection",
            description="mirror reflections get colored as diffuse (for metallic materials)",
            default=False)

    Mat.pov_conserve_energy = BoolProperty(
            name="Conserve Energy",
            description="Light transmitted is more correctly reduced by mirror reflections, also the sum of diffuse and translucency gets reduced below one ",
            default=True)

    Mat.pov_irid_amount = FloatProperty(
            name="amount",
            description="Contribution of the iridescence effect to the overall surface color. As a rule of thumb keep to around 0.25 (25% contribution) or less, but experiment. If the surface is coming out too white, try lowering the diffuse and possibly the ambient values of the surface",
            min=0.0, max=1.0, soft_min=0.01, soft_max=1.0, default=0.25)

    Mat.pov_irid_thickness = FloatProperty(
            name="thickness",
            description="A very thin film will have a high frequency of color changes while a thick film will have large areas of color",
            min=0.0, max=1000.0, soft_min=0.1, soft_max=10.0, default=1)

    Mat.pov_irid_turbulence = FloatProperty(
            name="turbulence",
            description="This parameter varies the thickness",
            min=0.0, max=10.0, soft_min=0.000, soft_max=1.0, default=0)

    Mat.pov_interior_fade_color = FloatVectorProperty(
            name="Fade Color",
            description="Color of filtered attenuation for transparent materials",
            subtype='COLOR',
            precision=4,
            step=0.01,
            min=0.0,
            soft_max=1.0,
            default=(0, 0, 0),
            options={'ANIMATABLE'})

    Mat.pov_caustics_enable = BoolProperty(
            name="Caustics",
            description="use only fake refractive caustics (default) or photon based reflective/refractive caustics",
            default=True)

    Mat.pov_fake_caustics = BoolProperty(
            name="Fake Caustics",
            description="use only (Fast) fake refractive caustics",
            default=True)

    Mat.pov_fake_caustics_power = FloatProperty(
            name="Fake caustics power",
            description="Values typically range from 0.0 to 1.0 or higher. Zero is no caustics. Low, non-zero values give broad hot-spots while higher values give tighter, smaller simulated focal points",
            min=0.00, max=10.0, soft_min=0.00, soft_max=1.10, default=0.1)

    Mat.pov_photons_refraction = BoolProperty(
            name="Refractive Photon Caustics",
            description="more physically correct",
            default=False)

    Mat.pov_photons_dispersion = FloatProperty(
            name="chromatic dispersion",
            description="Light passing through will be separated according to wavelength. This ratio of refractive indices for violet to red controls how much the colors are spread out 1 = no dispersion, good values are 1.01 to 1.1",
            min=1.0000, max=10.000, soft_min=1.0000, soft_max=1.1000, precision=4, default=1.0000)

    Mat.pov_photons_reflection = BoolProperty(
            name="Reflective Photon Caustics",
            description="Use this to make your Sauron's ring ;-P",
            default=False)

    Mat.pov_refraction_type = EnumProperty(
            items=[("0", "None", "use only reflective caustics"),
                   ("1", "Fake Caustics", "use fake caustics"),
                   ("2", "Photons Caustics", "use photons for refractive caustics"),
                   ],
            name="Refractive",
            description="use fake caustics (fast) or true photons for refractive Caustics",
            default="1")
    ##################################CustomPOV Code############################
    Mat.pov_replacement_text = StringProperty(
            name="Declared name:",
            description="Type the declared name in custom POV code or an external .inc it points at. texture {} expected",
            default="")

    #Only DUMMIES below for now:
    Tex.pov_replacement_text = StringProperty(
            name="Declared name:",
            description="Type the declared name in custom POV code or an external .inc it points at. pigment {} expected",
            default="")

    Obj.pov_replacement_text = StringProperty(
            name="Declared name:",
            description="Type the declared name in custom POV code or an external .inc it points at. Any POV shape expected e.g: isosurface {}",
            default="")

    Cam.pov_replacement_text = StringProperty(
            name="Texts in blend file",
            description="Type the declared name in custom POV code or an external .inc it points at. camera {} expected",
            default="")
    ##############################TEXTURE######################################

    #Custom texture gamma
    Tex.pov_tex_gamma_enable = BoolProperty(
            name="Enable custom texture gamma",
            description="Notify some custom gamma for which texture has been precorrected without the file format carrying it and only if it differs from your OS expected standard (see pov doc)",
            default=False)

    Tex.pov_tex_gamma_value = FloatProperty(
            name="Custom texture gamma",
            description="value for which the file was issued e.g. a Raw photo is gamma 1.0",
            min=0.45, max=5.00, soft_min=1.00, soft_max=2.50, default=1.00)

    #################################OBJECT####################################

    #Importance sampling
    Obj.pov_importance_value = FloatProperty(
            name="Radiosity Importance",
            description="Priority value relative to other objects for sampling radiosity rays. Increase to get more radiosity rays at comparatively small yet bright objects",
            min=0.01, max=1.00, default=1.00)

    #Collect photons
    Obj.pov_collect_photons = BoolProperty(
            name="Receive Photon Caustics",
            description="Enable object to collect photons from other objects caustics. Turn off for objects that don't really need to receive caustics (e.g. objects that generate caustics often don't need to show any on themselves) ",
            default=True)

    ##################################CAMERA###################################

    #DOF Toggle
    Cam.pov_dof_enable = BoolProperty(
            name="Depth Of Field",
            description="Enable POV-Ray Depth Of Field ",
            default=True)

    #Aperture (Intensity of the Blur)
    Cam.pov_dof_aperture = FloatProperty(
            name="Aperture",
            description="Similar to a real camera's aperture effect over focal blur (though not in physical units and independant of focal length).Increase to get more blur",
            min=0.01, max=1.00, default=0.25)

    #Aperture adaptive sampling
    Cam.pov_dof_samples_min = IntProperty(
            name="Samples Min",
            description="Minimum number of rays to use for each pixel",
            min=1, max=128, default=96)

    Cam.pov_dof_samples_max = IntProperty(
            name="Samples Max",
            description="Maximum number of rays to use for each pixel",
            min=1, max=128, default=128)

    Cam.pov_dof_variance = IntProperty(
            name="Variance",
            description="Minimum threshold (fractional value) for adaptive DOF sampling (up increases quality and render time). The value for the variance should be in the range of the smallest displayable color difference",
            min=1, max=100000, soft_max=10000, default=256)

    Cam.pov_dof_confidence = FloatProperty(
            name="Confidence",
            description="Probability to reach the real color value. Larger confidence values will lead to more samples, slower traces and better images",
            min=0.01, max=0.99, default=0.90)

    ###################################TEXT####################################

    Text.pov_custom_code = BoolProperty(
            name="Custom Code",
            description="Add this text at the top of the exported POV-Ray file",
            default=False)


def unregister():
    Scene = bpy.types.Scene
    Mat = bpy.types.Material
    Tex = bpy.types.Texture
    Obj = bpy.types.Object
    Cam = bpy.types.Camera
    Text = bpy.types.Text
    del Scene.pov_tempfiles_enable  # CR
    del Scene.pov_scene_name  # CR
    del Scene.pov_deletefiles_enable  # CR
    del Scene.pov_scene_path  # CR
    del Scene.pov_renderimage_path  # CR
    del Scene.pov_list_lf_enable  # CR
    del Scene.pov_radio_enable
    del Scene.pov_radio_display_advanced
    del Scene.pov_radio_adc_bailout
    del Scene.pov_radio_always_sample
    del Scene.pov_radio_brightness
    del Scene.pov_radio_count
    del Scene.pov_radio_error_bound
    del Scene.pov_radio_gray_threshold
    del Scene.pov_radio_low_error_factor
    del Scene.pov_radio_media
    del Scene.pov_radio_minimum_reuse
    del Scene.pov_radio_nearest_count
    del Scene.pov_radio_normal
    del Scene.pov_radio_recursion_limit
    del Scene.pov_radio_pretrace_start  # MR
    del Scene.pov_radio_pretrace_end  # MR
    del Scene.pov_media_enable  # MR
    del Scene.pov_media_samples  # MR
    del Scene.pov_media_color  # MR
    del Scene.pov_baking_enable  # MR
    del Scene.pov_max_trace_level  # MR
    del Scene.pov_photon_spacing  # MR
    del Scene.pov_photon_max_trace_level  # MR
    del Scene.pov_photon_adc_bailout  # MR
    del Scene.pov_photon_gather_min  # MR
    del Scene.pov_photon_gather_max  # MR
    del Scene.pov_antialias_enable  # CR
    del Scene.pov_antialias_method  # CR
    del Scene.pov_antialias_depth  # CR
    del Scene.pov_antialias_threshold  # CR
    del Scene.pov_antialias_gamma  # CR
    del Scene.pov_jitter_enable  # CR
    del Scene.pov_jitter_amount  # CR
    del Scene.pov_command_line_switches  # CR
    del Scene.pov_indentation_character  # CR
    del Scene.pov_indentation_spaces  # CR
    del Scene.pov_comments_enable  # CR
    del Mat.pov_irid_enable  # MR
    del Mat.pov_mirror_use_IOR  # MR
    del Mat.pov_mirror_metallic  # MR
    del Mat.pov_conserve_energy  # MR
    del Mat.pov_irid_amount  # MR
    del Mat.pov_irid_thickness  # MR
    del Mat.pov_irid_turbulence  # MR
    del Mat.pov_interior_fade_color  # MR
    del Mat.pov_caustics_enable  # MR
    del Mat.pov_fake_caustics  # MR
    del Mat.pov_fake_caustics_power  # MR
    del Mat.pov_photons_refraction  # MR
    del Mat.pov_photons_dispersion  # MR
    del Mat.pov_photons_reflection  # MR
    del Mat.pov_refraction_type  # MR
    del Mat.pov_replacement_text  # MR
    del Tex.pov_tex_gamma_enable  # MR
    del Tex.pov_tex_gamma_value  # MR
    del Tex.pov_replacement_text  # MR
    del Obj.pov_importance_value  # MR
    del Obj.pov_collect_photons  # MR
    del Obj.pov_replacement_text  # MR
    del Cam.pov_dof_enable  # MR
    del Cam.pov_dof_aperture  # MR
    del Cam.pov_dof_samples_min  # MR
    del Cam.pov_dof_samples_max  # MR
    del Cam.pov_dof_variance  # MR
    del Cam.pov_dof_confidence  # MR
    del Cam.pov_replacement_text  # MR
    del Text.pov_custom_code  # MR
