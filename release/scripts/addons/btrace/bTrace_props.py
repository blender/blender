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

import bpy
from bpy.types import (
        Panel,
        PropertyGroup,
        )
from bpy.props import (
        FloatProperty,
        EnumProperty,
        IntProperty,
        BoolProperty,
        FloatVectorProperty,
        )


# Class to define properties
class TracerProperties(PropertyGroup):
    """Options for tools"""
    curve_spline = EnumProperty(
            name="Spline",
            items=(("POLY", "Poly", "Use Poly spline type"),
                   ("NURBS", "Nurbs", "Use Nurbs spline type"),
                   ("BEZIER", "Bezier", "Use Bezier spline type")),
            description="Choose which type of spline to use when curve is created",
            default="BEZIER"
            )
    curve_handle = EnumProperty(
            name="Handle",
            items=(("ALIGNED", "Aligned", "Use Aligned Handle Type"),
                   ("AUTOMATIC", "Automatic", "Use Auto Handle Type"),
                   ("FREE_ALIGN", "Free Align", "Use Free Handle Type"),
                   ("VECTOR", "Vector", "Use Vector Handle Type")),
            description="Choose which type of handle to use when curve is created",
            default="VECTOR"
            )
    curve_resolution = IntProperty(
            name="Bevel Resolution",
            min=1, max=32,
            default=4,
            description="Adjust the Bevel resolution"
            )
    curve_depth = FloatProperty(
            name="Bevel Depth",
            min=0.0, max=100.0,
            default=0.1,
            description="Adjust the Bevel depth"
            )
    curve_u = IntProperty(
            name="Resolution U",
            min=0, max=64,
            default=12,
            description="Adjust the Surface resolution"
            )
    curve_join = BoolProperty(
            name="Join Curves",
            default=False,
            description="Join all the curves after they have been created"
            )
    curve_smooth = BoolProperty(
            name="Smooth",
            default=True,
            description="Render curve smooth"
            )
    # Option to Duplicate Mesh
    object_duplicate = BoolProperty(
            name="Apply to Copy",
            default=False,
            description="Apply curve to a copy of object"
            )
    # Distort Mesh options
    distort_modscale = IntProperty(
            name="Modulation Scale",
            min=0, max=50,
            default=2,
            description="Add a scale to modulate the curve at random points, set to 0 to disable"
            )
    distort_noise = FloatProperty(
            name="Mesh Noise",
            min=0.0, max=50.0,
            default=0.00,
            description="Adjust noise added to mesh before adding curve"
            )
    # Particle Options
    particle_step = IntProperty(
            name="Step Size",
            min=1, max=50,
            default=5,
            description="Sample one every this number of frames"
            )
    particle_auto = BoolProperty(
            name="Auto Frame Range",
            default=True,
            description="Calculate Frame Range from particles life"
            )
    particle_f_start = IntProperty(
            name='Start Frame',
            min=1, max=5000,
            default=1,
            description='Start frame'
            )
    particle_f_end = IntProperty(
            name="End Frame",
            min=1, max=5000,
            default=250,
            description="End frame"
            )
    # F-Curve Modifier Properties
    fcnoise_rot = BoolProperty(
            name="Rotation",
            default=False,
            description="Affect Rotation"
            )
    fcnoise_loc = BoolProperty(
            name="Location",
            default=True,
            description="Affect Location"
            )
    fcnoise_scale = BoolProperty(
            name="Scale",
            default=False,
            description="Affect Scale"
            )
    fcnoise_amp = IntProperty(
            name="Amp",
            min=1, max=500,
            default=5,
            description="Adjust the amplitude"
            )
    fcnoise_timescale = FloatProperty(
            name="Time Scale",
            min=1, max=500,
            default=50,
            description="Adjust the time scale"
            )
    fcnoise_key = BoolProperty(
            name="Add Keyframe",
            default=True,
            description="Keyframe is needed for tool, this adds a LocRotScale keyframe"
            )
    show_curve_settings = BoolProperty(
            name="Curve Settings",
            default=False,
            description="Change the curve settings for the created curve"
            )
    material_settings = BoolProperty(
            name="Material Settings",
            default=False,
            description="Change the material settings for the created curve"
            )
    particle_settings = BoolProperty(
            name="Particle Settings",
            default=False,
            description="Show the settings for the created curve"
            )
    animation_settings = BoolProperty(
            name="Animation Settings",
            default=False,
            description="Show the settings for the Animations"
            )
    distort_curve = BoolProperty(
            name="Add Distortion",
            default=False,
            description="Set options to distort the final curve"
            )
    connect_noise = BoolProperty(
            name="F-Curve Noise",
            default=False,
            description="Adds F-Curve Noise Modifier to selected objects"
            )
    settings_objectTrace = BoolProperty(
            name="Object Trace Settings",
            default=False,
            description="Trace selected mesh object with a curve"
            )
    settings_objectsConnect = BoolProperty(
            name="Objects Connect Settings",
            default=False,
            description="Connect objects with a curve controlled by hooks"
            )
    settings_objectTrace = BoolProperty(
            name="Object Trace Settings",
            default=False,
            description="Trace selected mesh object with a curve"
            )
    respect_order = BoolProperty(
            name="Order",
            default=False,
            description="Remember order objects were selected"
            )
    settings_particleTrace = BoolProperty(
            name="Particle Trace Settings",
            default=False,
            description="Trace particle path with a  curve"
            )
    settings_particleConnect = BoolProperty(
            name="Particle Connect Settings",
            default=False,
            description="Connect particles with a curves and animated over particle lifetime"
            )
    settings_growCurve = BoolProperty(
            name="Grow Curve Settings",
            default=False,
            description="Animate curve bevel over time by keyframing points radius"
            )
    settings_fcurve = BoolProperty(
            name="F-Curve Settings",
            default=False,
            description="F-Curve Settings"
            )
    settings_toggle = BoolProperty(
            name="Settings",
            default=False,
            description="Toggle Settings"
            )
    # Animation Options
    anim_auto = BoolProperty(
            name="Auto Frame Range",
            default=True,
            description="Automatically calculate Frame Range"
            )
    anim_f_start = IntProperty(
            name="Start",
            min=1, max=2500,
            default=1,
            description="Start frame / Hidden object"
            )
    anim_length = IntProperty(
            name="Duration",
            min=1,
            soft_max=1000, max=2500,
            default=100,
            description="Animation Length"
            )
    anim_f_fade = IntProperty(
            name="Fade After",
            min=0,
            soft_max=250, max=2500,
            default=10,
            description="Fade after this frames / Zero means no fade"
            )
    anim_delay = IntProperty(
            name="Grow",
            min=0, max=50,
            default=5,
            description="Frames it takes a point to grow"
            )
    anim_tails = BoolProperty(
            name='Tails on endpoints',
            default=True,
            description='Set radius to zero for open splines endpoints'
            )
    anim_keepr = BoolProperty(
            name="Keep Radius",
            default=True,
            description="Try to keep radius data from original curve"
            )
    animate = BoolProperty(
            name="Animate Result",
            default=False,
            description="Animate the final curve objects"
            )
    # Convert to Curve options
    convert_conti = BoolProperty(
            name="Continuous",
            default=True,
            description="Create a continuous curve using verts from mesh"
            )
    convert_everyedge = BoolProperty(
            name="Every Edge",
            default=False,
            description="Create a curve from all verts in a mesh"
            )
    convert_edgetype = EnumProperty(
            name="Edge Type for Curves",
            items=(("CONTI", "Continuous", "Create a continuous curve using verts from mesh"),
                   ("EDGEALL", "All Edges", "Create a curve from every edge in a mesh")),
            description="Choose which type of spline to use when curve is created",
            default="CONTI"
            )
    convert_joinbefore = BoolProperty(
            name="Join objects before convert",
            default=False,
            description="Join all selected mesh to one object before converting to mesh"
            )
    # Mesh Follow Options
    fol_edge_select = BoolProperty(
            name="Edge",
            default=False,
            description="Grow from edges"
            )
    fol_vert_select = BoolProperty(
            name="Vertex",
            default=False,
            description="Grow from verts"
            )
    fol_face_select = BoolProperty(
            name="Face",
            default=True,
            description="Grow from faces"
            )
    fol_mesh_type = EnumProperty(
            name="Mesh type",
            default="VERTS",
            description="Mesh feature to draw cruves from",
            items=(("VERTS", "Verts", "Draw from Verts"),
                   ("EDGES", "Edges", "Draw from Edges"),
                   ("FACES", "Faces", "Draw from Faces"),
                   ("OBJECT", "Object", "Draw from Object origin"))
            )
    fol_start_frame = IntProperty(
            name="Start Frame",
            min=1, max=2500,
            default=1,
            description="Start frame for range to trace"
            )
    fol_end_frame = IntProperty(
            name="End Frame",
            min=1, max=2500,
            default=250,
            description="End frame for range to trace"
            )
    fol_perc_verts = FloatProperty(
            name="Reduce selection by",
            min=0.001, max=1.000,
            default=0.5,
            description="percentage of total verts to trace"
            )
    fol_sel_option = EnumProperty(
            name="Selection type",
            description="Choose which objects to follow",
            default="RANDOM",
            items=(("RANDOM", "Random", "Follow Random items"),
                   ("CUSTOM", "Custom Select", "Follow selected items"),
                   ("ALL", "All", "Follow all items"))
            )
    trace_mat_color = FloatVectorProperty(
            name="Material Color",
            description="Choose material color",
            min=0, max=1,
            default=(0.0, 0.3, 0.6),
            subtype="COLOR"
            )
    trace_mat_random = BoolProperty(
            name="Random Color",
            default=False,
            description='Make the material colors random'
            )
    # Material custom Properties properties
    mat_simple_adv_toggle = EnumProperty(
            name="Material Options",
            items=(("SIMPLE", "Simple", "Show Simple Material Options"),
                   ("ADVANCED", "Advanced", "Show Advanced Material Options")),
            description="Choose which Material Options to show",
            default="SIMPLE"
            )
    mat_run_color_blender = BoolProperty(
            name="Run Color Blender",
            default=False,
            description="Generate colors from a color scheme"
            )
    mmColors = EnumProperty(
            items=(("RANDOM", "Random", "Use random colors"),
                    ("CUSTOM", "Custom", "Use custom colors"),
                    ("BW", "Black/White", "Use Black and White"),
                    ("BRIGHT", "Bright Colors", "Use Bright colors"),
                    ("EARTH", "Earth", "Use Earth colors"),
                    ("GREENBLUE", "Green to Blue", "Use Green to Blue colors")),
            description="Choose which type of colors the materials uses",
            default="BRIGHT",
            name="Define a color palette"
            )
    # Custom property for how many keyframes to skip
    mmSkip = IntProperty(
            name="frames",
            min=1, max=500,
            default=20,
            description="Number of frames between each keyframes"
            )
    # Custom property to enable/disable random order for the
    mmBoolRandom = BoolProperty(
            name="Random Order",
            default=False,
            description="Randomize the order of the colors"
            )
    # Custom Color properties
    mmColor1 = FloatVectorProperty(
            min=0, max=1,
            default=(0.8, 0.8, 0.8),
            description="Custom Color 1", subtype="COLOR"
            )
    mmColor2 = FloatVectorProperty(
            min=0, max=1,
            default=(0.8, 0.8, 0.3),
            description="Custom Color 2",
            subtype="COLOR"
            )
    mmColor3 = FloatVectorProperty(
            min=0, max=1,
            default=(0.8, 0.5, 0.6),
            description="Custom Color 3",
            subtype="COLOR"
            )
    mmColor4 = FloatVectorProperty(
            min=0, max=1,
            default=(0.2, 0.8, 0.289),
            description="Custom Color 4",
            subtype="COLOR"
            )
    mmColor5 = FloatVectorProperty(
            min=0, max=1,
            default=(1.0, 0.348, 0.8),
            description="Custom Color 5",
            subtype="COLOR"
            )
    mmColor6 = FloatVectorProperty(
            min=0, max=1,
            default=(0.4, 0.67, 0.8),
            description="Custom Color 6",
            subtype="COLOR"
            )
    mmColor7 = FloatVectorProperty(
            min=0, max=1,
            default=(0.66, 0.88, 0.8),
            description="Custom Color 7",
            subtype="COLOR"
            )
    mmColor8 = FloatVectorProperty(
            min=0, max=1,
            default=(0.8, 0.38, 0.22),
            description="Custom Color 8",
            subtype="COLOR"
            )
    # BW Color properties
    bwColor1 = FloatVectorProperty(
            min=0, max=1,
            default=(0.0, 0.0, 0.0),
            description="Black/White Color 1",
            subtype="COLOR"
            )
    bwColor2 = FloatVectorProperty(
            min=0, max=1,
            default=(1.0, 1.0, 1.0),
            description="Black/White Color 2",
            subtype="COLOR"
            )
    # Bright Color properties
    brightColor1 = FloatVectorProperty(
            min=0, max=1,
            default=(1.0, 0.0, 0.75),
            description="Bright Color 1",
            subtype="COLOR"
            )
    brightColor2 = FloatVectorProperty(
            min=0, max=1,
            default=(0.0, 1.0, 1.0),
            description="Bright Color 2",
            subtype="COLOR"
            )
    brightColor3 = FloatVectorProperty(
            min=0, max=1,
            default=(0.0, 1.0, 0.0),
            description="Bright Color 3",
            subtype="COLOR"
            )
    brightColor4 = FloatVectorProperty(
            min=0, max=1,
            default=(1.0, 1.0, 0.0),
            description="Bright Color 4", subtype="COLOR"
            )
    # Earth Color Properties
    earthColor1 = FloatVectorProperty(
            min=0, max=1,
            default=(0.068, 0.019, 0.014),
            description="Earth Color 1",
            subtype="COLOR"
            )
    earthColor2 = FloatVectorProperty(
            min=0, max=1,
            default=(0.089, 0.060, 0.047),
            description="Earth Color 2",
            subtype="COLOR"
            )
    earthColor3 = FloatVectorProperty(
            min=0, max=1,
            default=(0.188, 0.168, 0.066),
            description="Earth Color 3",
            subtype="COLOR"
            )
    earthColor4 = FloatVectorProperty(
            min=0, max=1,
            default=(0.445, 0.296, 0.065),
            description="Earth Color 4",
            subtype="COLOR"
            )
    earthColor5 = FloatVectorProperty(
            min=0, max=1,
            default=(0.745, 0.332, 0.065),
            description="Earth Color 5",
            subtype="COLOR"
            )
    # Green to Blue Color properties
    greenblueColor1 = FloatVectorProperty(
            min=0, max=1,
            default=(0.296, 0.445, 0.074),
            description="Green/Blue Color 1",
            subtype="COLOR"
            )
    greenblueColor2 = FloatVectorProperty(
            min=0, max=1,
            default=(0.651, 1.0, 0.223),
            description="Green/Blue Color 2",
            subtype="COLOR"
            )
    greenblueColor3 = FloatVectorProperty(
            min=0, max=1,
            default=(0.037, 0.047, 0.084),
            description="Green/Blue Color 3",
            subtype="COLOR"
            )

    # Toolbar show/hide booleans for tool options
    btrace_menu_items = [
            ('tool_help', "Help",
             "Pick one of the options below", "INFO", 0),
            ('tool_objectTrace', "Object Trace",
             "Trace selected mesh object with a curve", "FORCE_MAGNETIC", 1),
            ('tool_objectsConnect', "Objects Connect",
             "Connect objects with a curve controlled by hooks", "OUTLINER_OB_EMPTY", 2),
            ('tool_meshFollow', "Mesh Follow",
             "Follow selection items on animated mesh object", "DRIVER", 3),
            ('tool_handwrite', "Handwriting",
             "Create and Animate curve using the grease pencil", "BRUSH_DATA", 4),
            ('tool_particleTrace', "Particle Trace",
             "Trace particle path with a  curve", "PARTICLES", 5),
            ('tool_particleConnect', "Particle Connect",
             "Connect particles with a curves and animated over particle lifetime", "MOD_PARTICLES", 6),
            ('tool_growCurve', "Grow Curve",
             "Animate curve bevel over time by keyframing points radius", "META_BALL", 7),
            ('tool_fcurve', "F-Curve Noise",
             "Add F-Curve noise to selected objects", "RNDCURVE", 8),
            ('tool_colorblender', "Color Blender",
             "Pick the color of the created curves", "COLOR", 9),
            ]
    btrace_toolmenu = EnumProperty(
            name="Tools",
            items=btrace_menu_items,
            description="",
            default='tool_help'
            )


# Draw Brush panel in Toolbar
class addTracerObjectPanel(Panel):
    bl_label = "Btrace"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"
    bl_category = "Create"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        Btrace = context.window_manager.curve_tracer
        addon_prefs = context.user_preferences.addons["btrace"].preferences
        switch_expand = addon_prefs.expand_enum
        obj = context.object

        # Color Blender Panel options
        def color_blender():
            # Buttons for Color Blender
            row = box.row()
            row.label("Color palette")
            row.prop(Btrace, "mmColors", text="")

            # Show Custom Colors if selected
            if Btrace.mmColors == 'CUSTOM':
                row = box.row(align=True)
                for i in range(1, 9):
                    row.prop(Btrace, "mmColor" + str(i), text="")
            # Show Earth Colors
            elif Btrace.mmColors == 'BW':
                row = box.row(align=True)
                row.prop(Btrace, "bwColor1", text="")
                row.prop(Btrace, "bwColor2", text="")
            # Show Earth Colors
            elif Btrace.mmColors == 'BRIGHT':
                row = box.row(align=True)
                for i in range(1, 5):
                    row.prop(Btrace, "brightColor" + str(i), text="")
            # Show Earth Colors
            elif Btrace.mmColors == 'EARTH':
                row = box.row(align=True)
                for i in range(1, 6):
                    row.prop(Btrace, "earthColor" + str(i), text="")
            # Show Earth Colors
            elif Btrace.mmColors == 'GREENBLUE':
                row = box.row(align=True)
                for i in range(1, 4):
                    row.prop(Btrace, "greenblueColor" + str(i), text="")
            elif Btrace.mmColors == 'RANDOM':
                row = box.row()

        # Curve noise settings
        def curve_noise():
            row = box.row()
            row.label(text="F-Curve Noise", icon='RNDCURVE')
            row = box.row(align=True)
            row.prop(Btrace, "fcnoise_rot", toggle=True)
            row.prop(Btrace, "fcnoise_loc", toggle=True)
            row.prop(Btrace, "fcnoise_scale", toggle=True)

            col = box.column(align=True)
            col.prop(Btrace, "fcnoise_amp")
            col.prop(Btrace, "fcnoise_timescale")
            box.prop(Btrace, "fcnoise_key")

        # Curve Panel options
        def curve_settings():
            # Button for curve options
            row = self.layout.row()
            row = box.row(align=True)

            row.prop(Btrace, "show_curve_settings",
                     icon='CURVE_BEZCURVE', text="Curve Settings")
            row.prop(Btrace, "material_settings",
                     icon='MATERIAL_DATA', text="Material Settings")

            if Btrace.material_settings:
                row = box.row()
                row.label(text="Material Settings", icon='COLOR')
                row = box.row()
                row.prop(Btrace, "trace_mat_random")
                if not Btrace.trace_mat_random:
                    row = box.row()
                    row.prop(Btrace, "trace_mat_color", text="")
                else:
                    row.prop(Btrace, "mat_run_color_blender")
                    if Btrace.mat_run_color_blender:
                        row = box.row()
                        row.operator("object.colorblenderclear",
                                     text="Reset Material Keyframes",
                                     icon="KEY_DEHLT")
                        row.prop(Btrace, "mmSkip", text="Keyframe every")
                    color_blender()
                row = box.row()

            if Btrace.show_curve_settings:
                # selected curve options
                if len(context.selected_objects) > 0 and obj.type == 'CURVE':
                    col = box.column(align=True)
                    col.label(text="Edit Curves for:", icon='IPO_BEZIER')
                    col.separator()
                    col.label(text="Selected Curve Bevel Options")
                    row = col.row(align=True)
                    row.prop(obj.data, "bevel_depth", text="Depth")
                    row.prop(obj.data, "bevel_resolution", text="Resolution")
                    row = col.row(align=True)
                    row.prop(obj.data, "resolution_u")
                else:  # For new curve
                    box.label(text="New Curve Settings", icon='CURVE_BEZCURVE')
                    box.prop(Btrace, "curve_spline")
                    box.prop(Btrace, "curve_handle")
                    box.label(text="Bevel Options")
                    col = box.column(align=True)
                    row = col.row(align=True)
                    row.prop(Btrace, "curve_depth", text="Depth")
                    row.prop(Btrace, "curve_resolution", text="Resolution")
                    row = col.row(align=True)
                    row.prop(Btrace, "curve_u")

        # Grow Animation Panel options
        def add_grow():
            # Button for grow animation option
            row = box.row()
            row.label(text="Animate Final Curve", icon="SPACE2")
            row = box.row()
            row.prop(Btrace, "animate", text="Add Grow Curve Animation", icon="META_BALL")
            box.separator()
            if Btrace.animate:
                box.label(text="Frame Animation Settings:", icon="META_BALL")
                col = box.column(align=True)
                col.prop(Btrace, "anim_auto")
                if not Btrace.anim_auto:
                    row = col.row(align=True)
                    row.prop(Btrace, "anim_f_start")
                    row.prop(Btrace, "anim_length")
                row = col.row(align=True)
                row.prop(Btrace, "anim_delay")
                row.prop(Btrace, "anim_f_fade")

                box.label(text="Additional Settings")
                row = box.row()
                row.prop(Btrace, "anim_tails")
                row.prop(Btrace, "anim_keepr")

        # Start Btrace Panel
        if switch_expand == 'list':
            layout.label(text="Available Tools:", icon="COLLAPSEMENU")
            col = layout.column(align=True)
            col.prop(Btrace, "btrace_toolmenu", text="")
        elif switch_expand == 'col':
            col = layout.column(align=True)
            col.prop(Btrace, "btrace_toolmenu", expand=True)
        elif switch_expand == 'row':
            row = layout.row(align=True)
            row.alignment = 'CENTER'
            row.prop(Btrace, "btrace_toolmenu", text="", expand=True)

        # Start Object Tools
        sel = context.selected_objects

        # Default option (can be expanded into help)
        if Btrace.btrace_toolmenu == 'tool_help':
            row = layout.row()
            row.label("Pick an option", icon="HELP")

        # Object Trace
        elif Btrace.btrace_toolmenu == 'tool_objectTrace':
            row = layout.row()
            row.label(text="  Trace Tool:", icon="FORCE_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="Object Trace", icon="FORCE_MAGNETIC")
            row.operator("object.btobjecttrace", text="Run!", icon="PLAY")
            row = box.row()
            row.prop(Btrace, "settings_toggle", icon="MODIFIER", text="Settings")
            myselected = "Selected %d" % len(context.selected_objects)
            row.label(text=myselected)
            if Btrace.settings_toggle:
                box.label(text="Edge Type for Curves:", icon="IPO_CONSTANT")
                row = box.row(align=True)
                row.prop(Btrace, "convert_edgetype", text="")
                box.prop(Btrace, "object_duplicate")
                if len(sel) > 1:
                    box.prop(Btrace, "convert_joinbefore")
                else:
                    Btrace.convert_joinbefore = False
                row = box.row()
                row.prop(Btrace, "distort_curve")
                if Btrace.distort_curve:
                    col = box.column(align=True)
                    col.prop(Btrace, "distort_modscale")
                    col.prop(Btrace, "distort_noise")
                row = box.row()
                curve_settings()  # Show Curve/material settings
                add_grow()        # Grow settings here

        # Objects Connect
        elif Btrace.btrace_toolmenu == 'tool_objectsConnect':
            row = layout.row()
            row.label(text="  Trace Tool:", icon="FORCE_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="Objects Connect", icon="OUTLINER_OB_EMPTY")
            row.operator("object.btobjectsconnect", text="Run!", icon="PLAY")
            row = box.row()
            row.prop(Btrace, "settings_toggle", icon='MODIFIER', text='Settings')
            row.label(text="")
            if Btrace.settings_toggle:
                row = box.row()
                row.prop(Btrace, "respect_order", text="Selection Options")
                if Btrace.respect_order:
                    box.operator("object.select_order",
                                 text="Click to start order selection",
                                 icon='UV_SYNC_SELECT')
                row = box.row()
                row.prop(Btrace, "connect_noise", text="Add F-Curve Noise")
                if Btrace.connect_noise:
                    curve_noise()     # Show Curve Noise settings

                curve_settings()      # Show Curve/material settings
                add_grow()            # Grow settings here

        # Mesh Follow
        elif Btrace.btrace_toolmenu == 'tool_meshFollow':
            row = layout.row()
            row.label(text="  Trace Tool:", icon="FORCE_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="Mesh Follow", icon="DRIVER")
            row.operator("object.btmeshfollow", text="Run!", icon="PLAY")
            row = box.row()
            if Btrace.fol_mesh_type == 'OBJECT':
                a, b = "Trace Object", "SNAP_VOLUME"
            if Btrace.fol_mesh_type == 'VERTS':
                a, b = "Trace Verts", "SNAP_VERTEX"
            if Btrace.fol_mesh_type == 'EDGES':
                a, b = "Trace Edges", "SNAP_EDGE"
            if Btrace.fol_mesh_type == 'FACES':
                a, b = "Trace Faces", "SNAP_FACE"
            row.prop(Btrace, "settings_toggle", icon='MODIFIER', text='Settings')
            row.label(text=a, icon=b)
            if Btrace.settings_toggle:
                col = box.column(align=True)
                row = col.row(align=True)
                row.prop(Btrace, "fol_mesh_type", expand=True)
                row = col.row(align=True)
                if Btrace.fol_mesh_type != 'OBJECT':
                    row.prop(Btrace, "fol_sel_option", expand=True)
                    row = box.row()
                    if Btrace.fol_sel_option == 'RANDOM':
                        row.label("Random Select of Total")
                        row.prop(Btrace, "fol_perc_verts", text="%")
                    if Btrace.fol_sel_option == 'CUSTOM':
                        row.label("Choose selection in Edit Mode")
                    if Btrace.fol_sel_option == 'ALL':
                        row.label("Select All items")
                col = box.column(align=True)
                col.label("Time Options", icon="TIME")
                col.prop(Btrace, "particle_step")
                row = col.row(align=True)
                row.prop(Btrace, "fol_start_frame")
                row.prop(Btrace, "fol_end_frame")
                curve_settings()  # Show Curve/material settings
                add_grow()        # Grow settings here

        # Handwriting Tools
        elif Btrace.btrace_toolmenu == 'tool_handwrite':
            row = layout.row()
            row.label(text="  Trace Tool:", icon="FORCE_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text='Handwriting', icon='BRUSH_DATA')
            row.operator("curve.btwriting", text="Run!", icon='PLAY')
            row = box.row()
            row = box.row()
            row.label(text='Grease Pencil Writing Tools')
            col = box.column(align=True)
            row = col.row(align=True)
            row.operator("gpencil.draw", text="Draw", icon='BRUSH_DATA').mode = 'DRAW'
            row.operator("gpencil.draw", text="Poly", icon='VPAINT_HLT').mode = 'DRAW_POLY'
            row = col.row(align=True)
            row.operator("gpencil.draw", text="Line", icon='ZOOMOUT').mode = 'DRAW_STRAIGHT'
            row.operator("gpencil.draw", text="Erase", icon='TPAINT_HLT').mode = 'ERASER'
            row = box.row()
            row.operator("gpencil.data_unlink", text="Delete Grease Pencil Layer", icon="CANCEL")
            row = box.row()
            curve_settings()  # Show Curve/material settings
            add_grow()        # Grow settings here

        # Particle Trace
        elif Btrace.btrace_toolmenu == 'tool_particleTrace':
            row = layout.row()
            row.label(text="  Trace Tool:", icon="FORCE_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="Particle Trace", icon="PARTICLES")
            row.operator("particles.particletrace", text="Run!", icon="PLAY")
            row = box.row()
            row.prop(Btrace, "settings_toggle", icon='MODIFIER', text='Settings')
            row.label(text="")
            if Btrace.settings_toggle:
                box.prop(Btrace, "particle_step")
                row = box.row()
                row.prop(Btrace, "curve_join")
                curve_settings()  # Show Curve/material settings
                add_grow()        # Grow settings here

        # Connect Particles
        elif Btrace.btrace_toolmenu == 'tool_particleConnect':
            row = layout.row()
            row.label(text="  Trace Tool:", icon="FORCE_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text='Particle Connect', icon='MOD_PARTICLES')
            row.operator("particles.connect", icon="PLAY", text='Run!')
            row = box.row()
            row.prop(Btrace, "settings_toggle", icon='MODIFIER', text='Settings')
            row.label(text="")
            if Btrace.settings_toggle:
                box.prop(Btrace, "particle_step")
                row = box.row()
                row.prop(Btrace, 'particle_auto')
                if not Btrace.particle_auto:
                    row = box.row(align=True)
                    row.prop(Btrace, 'particle_f_start')
                    row.prop(Btrace, 'particle_f_end')
                curve_settings()  # Show Curve/material settings
                add_grow()        # Grow settings here

        # Grow Animation
        elif Btrace.btrace_toolmenu == 'tool_growCurve':
            row = layout.row()
            row.label(text="  Curve Tool:", icon="OUTLINER_OB_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="Grow Curve", icon="META_BALL")
            row.operator("curve.btgrow", text="Run!", icon="PLAY")
            row = box.row()
            row.prop(Btrace, "settings_toggle", icon="MODIFIER", text="Settings")
            row.operator("object.btreset", icon="KEY_DEHLT")
            if Btrace.settings_toggle:
                box.label(text="Frame Animation Settings:")
                col = box.column(align=True)
                col.prop(Btrace, "anim_auto")
                if not Btrace.anim_auto:
                    row = col.row(align=True)
                    row.prop(Btrace, "anim_f_start")
                    row.prop(Btrace, "anim_length")
                row = col.row(align=True)
                row.prop(Btrace, "anim_delay")
                row.prop(Btrace, "anim_f_fade")

                box.label(text="Additional Settings")
                row = box.row()
                row.prop(Btrace, "anim_tails")
                row.prop(Btrace, "anim_keepr")

        # F-Curve Noise Curve
        elif Btrace.btrace_toolmenu == 'tool_fcurve':
            row = layout.row()
            row.label(text="  Curve Tool:", icon="OUTLINER_OB_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="F-Curve Noise", icon='RNDCURVE')
            row.operator("object.btfcnoise", icon='PLAY', text="Run!")
            row = box.row()
            row.prop(Btrace, "settings_toggle", icon='MODIFIER', text='Settings')
            row.operator("object.btreset", icon='KEY_DEHLT')
            if Btrace.settings_toggle:
                curve_noise()

        # Color Blender
        elif Btrace.btrace_toolmenu == 'tool_colorblender':
            row = layout.row()
            row.label(text="  Curve/Object Tool:", icon="OUTLINER_OB_CURVE")
            box = self.layout.box()
            row = box.row()
            row.label(text="Color Blender", icon="COLOR")
            row.operator("object.colorblender", icon='PLAY', text="Run!")
            row = box.row()
            row.operator("object.colorblenderclear", text="Reset Keyframes", icon="KEY_DEHLT")
            row.prop(Btrace, "mmSkip", text="Keyframe every")
            color_blender()
