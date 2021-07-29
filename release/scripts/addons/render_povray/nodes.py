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

from bpy.types import Node, ShaderNodeTree, CompositorNodeTree, TextureNodeTree#, NodeSocket


from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        FloatProperty,
        FloatVectorProperty,
        EnumProperty,
        #PointerProperty,
        #CollectionProperty,
        )



############### object

class ObjectNodeTree(bpy.types.NodeTree):
    '''Povray Material Nodes'''

    bl_idname = 'ObjectNodeTree'
    bl_label = 'Povray Object Nodes'
    bl_icon = 'PLUGIN'

    @classmethod
    def poll(cls, context):
        return context.scene.render.engine == 'POVRAY_RENDER'

    @classmethod
    def get_from_context(cls, context):
        ob = context.active_object
        if ob and ob.type not in {'LAMP'}:
            ma = ob.active_material
            if ma is not None:
                nt_name = ma.node_tree
                if nt_name != '':
                    return nt_name, ma, ma
        return (None, None, None)

    def update(self):
        self.refresh = True
################### output #############################################################################################

class PovrayOutputNode(Node, ObjectNodeTree):
    '''Output'''
    bl_idname = 'PovrayOutputNode'
    bl_label = 'Output'
    bl_icon = 'SOUND'

    def init(self, context):

        self.inputs.new('PovraySocketTexture', "Texture")

    def draw_buttons(self, context, layout):

        ob=context.object
        layout.prop(ob.pov, "object_ior",slider=True)

    def draw_buttons_ext(self, context, layout):

        ob=context.object
        layout.prop(ob.pov, "object_ior",slider=True)

    def draw_label(self):
        return "Output"



################### material ###########################################################################################
class PovrayTextureNode(Node, ObjectNodeTree):
    '''Texture'''
    bl_idname = 'PovrayTextureNode'
    bl_label = 'Simple texture'
    bl_icon = 'SOUND'

    def init(self, context):

        color=self.inputs.new('PovraySocketColor', "Pigment")
        color.default_value=(1,1,1)
        normal=self.inputs.new('NodeSocketFloat', "Normal")
        normal.hide_value=True
        finish=self.inputs.new('NodeSocketVector', "Finish")
        finish.hide_value=True

        self.outputs.new('PovraySocketTexture', "Texture")

    def draw_label(self):
        return "Simple texture"

class PovrayFinishNode(Node, ObjectNodeTree):
    '''Finish'''
    bl_idname = 'PovrayFinishNode'
    bl_label = 'Finish'
    bl_icon = 'SOUND'

    def init(self, context):

        self.inputs.new('PovraySocketFloat_0_1', "Emission")
        ambient=self.inputs.new('NodeSocketVector', "Ambient")
        ambient.hide_value=True
        diffuse=self.inputs.new('NodeSocketVector', "Diffuse")
        diffuse.hide_value=True
        specular=self.inputs.new('NodeSocketVector', "Highlight")
        specular.hide_value=True
        mirror=self.inputs.new('NodeSocketVector', "Mirror")
        mirror.hide_value=True
        iridescence=self.inputs.new('NodeSocketVector', "Iridescence")
        iridescence.hide_value=True
        subsurface=self.inputs.new('NodeSocketVector', "Translucency")
        subsurface.hide_value=True
        self.outputs.new('NodeSocketVector', "Finish")

    def draw_label(self):
        return "Finish"

class PovrayDiffuseNode(Node, ObjectNodeTree):
    '''Diffuse'''
    bl_idname = 'PovrayDiffuseNode'
    bl_label = 'Diffuse'
    bl_icon = 'SOUND'

    def init(self, context):

        intensity=self.inputs.new('PovraySocketFloat_0_1', "Intensity")
        intensity.default_value=0.8
        albedo=self.inputs.new('NodeSocketBool', "Albedo")
        albedo.default_value=False
        brilliance=self.inputs.new('PovraySocketFloat_0_10', "Brilliance")
        brilliance.default_value=1.8
        self.inputs.new('PovraySocketFloat_0_1', "Crand")
        self.outputs.new('NodeSocketVector', "Diffuse")

    def draw_label(self):
        return "Diffuse"

class PovrayPhongNode(Node, ObjectNodeTree):
    '''Phong'''
    bl_idname = 'PovrayPhongNode'
    bl_label = 'Phong'
    bl_icon = 'SOUND'

    def init(self, context):

        albedo=self.inputs.new('NodeSocketBool', "Albedo")
        intensity=self.inputs.new('PovraySocketFloat_0_1', "Intensity")
        intensity.default_value=0.8
        phong_size=self.inputs.new('PovraySocketInt_0_256', "Size")
        phong_size.default_value=60
        metallic=self.inputs.new('PovraySocketFloat_0_1', "Metallic")

        self.outputs.new('NodeSocketVector', "Phong")

    def draw_label(self):
        return "Phong"

class PovraySpecularNode(Node, ObjectNodeTree):
    '''Specular'''
    bl_idname = 'PovraySpecularNode'
    bl_label = 'Specular'
    bl_icon = 'SOUND'

    def init(self, context):

        albedo=self.inputs.new('NodeSocketBool', "Albedo")
        intensity=self.inputs.new('PovraySocketFloat_0_1', "Intensity")
        intensity.default_value=0.8
        roughness=self.inputs.new('PovraySocketFloat_0_1', "Roughness")
        roughness.default_value=0.02
        metallic=self.inputs.new('PovraySocketFloat_0_1', "Metallic")

        self.outputs.new('NodeSocketVector', "Specular")

    def draw_label(self):
        return "Specular"

class PovrayMirrorNode(Node, ObjectNodeTree):
    '''Mirror'''
    bl_idname = 'PovrayMirrorNode'
    bl_label = 'Mirror'
    bl_icon = 'SOUND'

    def init(self, context):

        color=self.inputs.new('PovraySocketColor', "Color")
        color.default_value=(1,1,1)
        metallic=self.inputs.new('PovraySocketFloat_0_1', "Metallic")
        metallic.default_value=1.0
        exponent=self.inputs.new('PovraySocketFloat_0_1', "Exponent")
        exponent.default_value=1.0
        self.inputs.new('PovraySocketFloat_0_1', "Falloff")
        self.inputs.new('NodeSocketBool', "Fresnel")
        self.inputs.new('NodeSocketBool', "Conserve energy")
        self.outputs.new('NodeSocketVector', "Mirror")

    def draw_label(self):
        return "Mirror"

class PovrayAmbientNode(Node, ObjectNodeTree):
    '''Ambient'''
    bl_idname = 'PovrayAmbientNode'
    bl_label = 'Ambient'
    bl_icon = 'SOUND'

    def init(self, context):

        self.inputs.new('PovraySocketColor', "Ambient")

        self.outputs.new('NodeSocketVector', "Ambient")

    def draw_label(self):
        return "Ambient"

class PovrayIridescenceNode(Node, ObjectNodeTree):
    '''Iridescence'''
    bl_idname = 'PovrayIridescenceNode'
    bl_label = 'Iridescence'
    bl_icon = 'SOUND'

    def init(self, context):

        amount=self.inputs.new('NodeSocketFloat', "Amount")
        amount.default_value=0.25
        thickness=self.inputs.new('NodeSocketFloat', "Thickness")
        thickness.default_value=1
        self.inputs.new('NodeSocketFloat', "Turbulence")

        self.outputs.new('NodeSocketVector', "Iridescence")

    def draw_label(self):
        return "Iridescence"

class PovraySubsurfaceNode(Node, ObjectNodeTree):
    '''Subsurface'''
    bl_idname = 'PovraySubsurfaceNode'
    bl_label = 'Subsurface'
    bl_icon = 'SOUND'

    def init(self, context):

        translucency=self.inputs.new('NodeSocketColor', "Translucency")
        translucency.default_value=(0,0,0,1)
        energy=self.inputs.new('PovraySocketInt_0_256', "Energy")
        energy.default_value=20
        self.outputs.new('NodeSocketVector', "Translucency")

    def draw_buttons(self, context, layout):
        scene=context.scene
        layout.prop(scene.pov, "sslt_enable",text="SSLT")


    def draw_buttons_ext(self, context, layout):
        scene=context.scene
        layout.prop(scene.pov, "sslt_enable",text="SSLT")

    def draw_label(self):
        return "Subsurface"

#####################################################################################################

class PovrayMappingNode(Node, ObjectNodeTree):
    '''Mapping'''
    bl_idname = 'PovrayMappingNode'
    bl_label = 'Mapping'
    bl_icon = 'SOUND'

    warp_type = EnumProperty(
            name="Warp Types",
            description="Select the type of warp",
            items=( ('cubic', "Cubic", ""),  ('cylindrical', "Cylindrical", ""),('planar', "Planar", ""),
                    ('spherical', "Spherical", ""),('toroidal', "Toroidal", ""),
                    ('uv_mapping', "UV", ""),
                    ('NONE', "None", "No indentation")),
            default='NONE')

    warp_orientation = EnumProperty(
            name="Warp Orientation",
            description="Select the orientation of warp",
            items=(('x', "X", ""), ('y', "Y", ""), ('z', "Z", "")),
            default='y')

    warp_dist_exp = FloatProperty(
            name="Distance exponent",
            description="Distance exponent",
            min=0.0, max=100.0, default=1.0)

    warp_tor_major_radius = FloatProperty(
            name="Major radius",
            description="Torus is distance from major radius",
            min=0.0, max=5.0, default=1.0)

    def init(self, context):
        self.outputs.new('NodeSocketVector', "Mapping")

    def draw_buttons(self, context, layout):

        column=layout.column()
        column.prop(self,"warp_type",text="Warp type")
        if self.warp_type in {'toroidal','spherical','cylindrical','planar'}:
            column.prop(self,"warp_orientation",text="Orientation")
            column.prop(self,"warp_dist_exp",text="Exponent")
        if self.warp_type=='toroidal':
            column.prop(self,"warp_tor_major_radius",text="Major R")

    def draw_buttons_ext(self, context, layout):

        column=layout.column()
        column.prop(self,"warp_type",text="Warp type")
        if self.warp_type in {'toroidal','spherical','cylindrical','planar'}:
            column.prop(self,"warp_orientation",text="Orientation")
            column.prop(self,"warp_dist_exp",text="Exponent")
        if self.warp_type=='toroidal':
            column.prop(self,"warp_tor_major_radius",text="Major R")

    def draw_label(self):
        return "Mapping"

class PovrayMultiplyNode(Node, ObjectNodeTree):
    '''Multiply'''
    bl_idname = 'PovrayMultiplyNode'
    bl_label = 'Multiply'
    bl_icon = 'SOUND'

    amount_x = FloatProperty(
            name="X",
            description="Number of repeats",
            min=1.0, max=10000.0, default=1.0)

    amount_y = FloatProperty(
            name="Y",
            description="Number of repeats",
            min=1.0, max=10000.0, default=1.0)

    amount_z = FloatProperty(
            name="Z",
            description="Number of repeats",
            min=1.0, max=10000.0, default=1.0)


    def init(self, context):
        self.outputs.new('NodeSocketVector', "Amount")

    def draw_buttons(self, context, layout):

        column=layout.column()
        column.label("Amount")
        row=column.row(align=True)
        row.prop(self,"amount_x")
        row.prop(self,"amount_y")
        row.prop(self,"amount_z")

    def draw_buttons_ext(self, context, layout):

        column=layout.column()
        column.label("Amount")
        row=column.row(align=True)
        row.prop(self,"amount_x")
        row.prop(self,"amount_y")
        row.prop(self,"amount_z")

    def draw_label(self):
        return "Multiply"

class PovrayTransformNode(Node, ObjectNodeTree):
    '''Transform'''
    bl_idname = 'PovrayTransformNode'
    bl_label = 'Transform'
    bl_icon = 'SOUND'

    def init(self, context):

        self.inputs.new('PovraySocketFloatUnlimited', "Translate x")
        self.inputs.new('PovraySocketFloatUnlimited', "Translate y")
        self.inputs.new('PovraySocketFloatUnlimited', "Translate z")
        self.inputs.new('PovraySocketFloatUnlimited', "Rotate x")
        self.inputs.new('PovraySocketFloatUnlimited', "Rotate y")
        self.inputs.new('PovraySocketFloatUnlimited', "Rotate z")
        sX = self.inputs.new('PovraySocketFloatUnlimited', "Scale x")
        sX.default_value = 1.0
        sY = self.inputs.new('PovraySocketFloatUnlimited', "Scale y")
        sY.default_value = 1.0
        sZ = self.inputs.new('PovraySocketFloatUnlimited', "Scale z")
        sZ.default_value = 1.0

        self.outputs.new('NodeSocketVector', "Transform")

    def draw_label(self):
        return "Transform"

class PovrayValueNode(Node, ObjectNodeTree):
    '''Value'''
    bl_idname = 'PovrayValueNode'
    bl_label = 'Value'
    bl_icon = 'SOUND'

    def init(self, context):

        self.outputs.new('PovraySocketUniversal', "Value")

    def draw_label(self):
        return "Value"

class PovrayModifierNode(Node, ObjectNodeTree):
    '''Modifier'''
    bl_idname = 'PovrayModifierNode'
    bl_label = 'Modifier'
    bl_icon = 'SOUND'

    def init(self, context):

        turb_x=self.inputs.new('PovraySocketFloat_0_10', "Turb X")
        turb_x.default_value=0.1
        turb_y=self.inputs.new('PovraySocketFloat_0_10', "Turb Y")
        turb_y.default_value=0.1
        turb_z=self.inputs.new('PovraySocketFloat_0_10', "Turb Z")
        turb_z.default_value=0.1
        octaves=self.inputs.new('PovraySocketInt_1_9', "Octaves")
        octaves.default_value=1
        lambat=self.inputs.new('PovraySocketFloat_0_10', "Lambda")
        lambat.default_value=2.0
        omega=self.inputs.new('PovraySocketFloat_0_10', "Omega")
        omega.default_value=0.5
        freq=self.inputs.new('PovraySocketFloat_0_10', "Frequency")
        freq.default_value=2.0
        self.inputs.new('PovraySocketFloat_0_10', "Phase")

        self.outputs.new('NodeSocketVector', "Modifier")

    def draw_label(self):
        return "Modifier"

class PovrayPigmentNode(Node, ObjectNodeTree):
    '''Pigment'''
    bl_idname = 'PovrayPigmentNode'
    bl_label = 'Color'

    def init(self, context):

        color = self.inputs.new('PovraySocketColor', "Color")
        color.default_value = (1,1,1)
        povfilter = self.inputs.new('PovraySocketFloat_0_1', "Filter")
        transmit = self.inputs.new('PovraySocketFloat_0_1', "Transmit")
        self.outputs.new('NodeSocketColor', "Pigment")

    def draw_label(self):
        return "Color"

class PovrayColorImageNode(Node, ObjectNodeTree):
    '''ColorImage'''
    bl_idname = 'PovrayColorImageNode'
    bl_label = 'Image map'

    map_type = bpy.props.EnumProperty(
            name="Map type",
            description="",
            items=( ('uv_mapping', "UV", ""),
                    ('0', "Planar", "Default planar mapping"),
                    ('1', "Spherical", "Spherical mapping"),
                    ('2', "Cylindrical", "Cylindrical mapping"),
                    ('5', "Torroidal", "Torus or donut shaped mapping")),
            default='0')
    image = StringProperty(maxlen=1024) # , subtype="FILE_PATH"
    interpolate = EnumProperty(
            name="Interpolate",
            description="Adding the interpolate keyword can smooth the jagged look of a bitmap",
            items=(
                ('2', "Bilinear", "Gives bilinear interpolation"),
                ('4', "Normalized", "Gives normalized distance"),
            ),
            default='2')
    premultiplied = BoolProperty(default=False)
    once = BoolProperty(description="Not to repeat", default=False)

    def init(self, context):

        gamma=self.inputs.new('PovraySocketFloat_000001_10', "Gamma")
        gamma.default_value=2.0
        transmit=self.inputs.new('PovraySocketFloat_0_1', "Transmit")
        povfilter=self.inputs.new('PovraySocketFloat_0_1', "Filter")
        mapping=self.inputs.new('NodeSocketVector', "Mapping")
        mapping.hide_value=True
        transform=self.inputs.new('NodeSocketVector', "Transform")
        transform.hide_value=True
        modifier=self.inputs.new('NodeSocketVector', "Modifier")
        modifier.hide_value=True

        self.outputs.new('NodeSocketColor', "Pigment")

    def draw_buttons(self, context, layout):

        column=layout.column()
        im=None
        for image in bpy.data.images:
            if image.name == self.image:
                im=image
        split = column.split(percentage=0.8,align=True)
        split.prop_search(self,"image",context.blend_data,"images",text="")
        split.operator("pov.imageopen",text="",icon="FILESEL")
        if im is not None:
            column.prop(im,"source",text="")
        column.prop(self,"map_type",text="")
        column.prop(self,"interpolate",text="")
        row=column.row()
        row.prop(self,"premultiplied",text="Premul")
        row.prop(self,"once",text="Once")

    def draw_buttons_ext(self, context, layout):

        column=layout.column()
        im=None
        for image in bpy.data.images:
            if image.name == self.image:
                im=image
        split = column.split(percentage=0.8,align=True)
        split.prop_search(self,"image",context.blend_data,"images",text="")
        split.operator("pov.imageopen",text="",icon="FILESEL")
        if im is not None:
            column.prop(im,"source",text="")
        column.prop(self,"map_type",text="")
        column.prop(self,"interpolate",text="")
        row=column.row()
        row.prop(self,"premultiplied",text="Premul")
        row.prop(self,"once",text="Once")

    def draw_label(self):
        return "Image map"

class PovrayBumpMapNode(Node, ObjectNodeTree):
    '''BumpMap'''
    bl_idname = 'PovrayBumpMapNode'
    bl_label = 'Bump map'
    bl_icon = 'SOUND'

    map_type = bpy.props.EnumProperty(
            name="Map type",
            description="",
            items=(
                ('uv_mapping', "UV", ""),
                ('0', "Planar", "Default planar mapping"),
                ('1', "Spherical", "Spherical mapping"),
                ('2', "Cylindrical", "Cylindrical mapping"),
                ('5', "Torroidal", "Torus or donut shaped mapping")
            ),
            default='0')
    image = StringProperty(maxlen=1024) # , subtype="FILE_PATH"
    interpolate = EnumProperty(
            name="Interpolate",
            description="Adding the interpolate keyword can smooth the jagged look of a bitmap",
            items=(
                ('2', "Bilinear", "Gives bilinear interpolation"),
                ('4', "Normalized", "Gives normalized distance"),
            ),
            default='2')
    once = BoolProperty(description="Not to repeat", default=False)

    def init(self, context):

        self.inputs.new('PovraySocketFloat_0_10', "Normal")
        mapping=self.inputs.new('NodeSocketVector', "Mapping")
        mapping.hide_value=True
        transform=self.inputs.new('NodeSocketVector', "Transform")
        transform.hide_value=True
        modifier=self.inputs.new('NodeSocketVector', "Modifier")
        modifier.hide_value=True

        normal=self.outputs.new('NodeSocketFloat', "Normal")
        normal.hide_value=True

    def draw_buttons(self, context, layout):

        column=layout.column()
        im=None
        for image in bpy.data.images:
            if image.name == self.image:
                im=image
        split = column.split(percentage=0.8,align=True)
        split.prop_search(self,"image",context.blend_data,"images",text="")
        split.operator("pov.imageopen",text="",icon="FILESEL")
        if im is not None:
            column.prop(im,"source",text="")
        column.prop(self,"map_type",text="")
        column.prop(self,"interpolate",text="")
        column.prop(self,"once",text="Once")

    def draw_buttons_ext(self, context, layout):

        column=layout.column()
        im=None
        for image in bpy.data.images:
            if image.name == self.image:
                im=image
        split = column.split(percentage=0.8,align=True)
        split.prop_search(self,"image",context.blend_data,"images",text="")
        split.operator("pov.imageopen",text="",icon="FILESEL")
        if im is not None:
            column.prop(im,"source",text="")
        column.prop(self,"map_type",text="")
        column.prop(self,"interpolate",text="")
        column.prop(self,"once",text="Once")

    def draw_label(self):
        return "Bump Map"

class PovrayImagePatternNode(Node, ObjectNodeTree):
    '''ImagePattern'''
    bl_idname = 'PovrayImagePatternNode'
    bl_label = 'Image pattern'
    bl_icon = 'SOUND'

    map_type = bpy.props.EnumProperty(
            name="Map type",
            description="",
            items=(
                ('uv_mapping', "UV", ""),
                ('0', "Planar", "Default planar mapping"),
                ('1', "Spherical", "Spherical mapping"),
                ('2', "Cylindrical", "Cylindrical mapping"),
                ('5', "Torroidal", "Torus or donut shaped mapping"),
            ),
            default='0')
    image = StringProperty(maxlen=1024) # , subtype="FILE_PATH"
    interpolate = EnumProperty(
            name="Interpolate",
            description="Adding the interpolate keyword can smooth the jagged look of a bitmap",
            items=(
                ('2', "Bilinear", "Gives bilinear interpolation"),
                ('4', "Normalized", "Gives normalized distance"),
            ),
            default='2')
    premultiplied = BoolProperty(default=False)
    once = BoolProperty(description="Not to repeat", default=False)
    use_alpha = BoolProperty(default=True)
    def init(self, context):

        gamma=self.inputs.new('PovraySocketFloat_000001_10', "Gamma")
        gamma.default_value=2.0

        self.outputs.new('PovraySocketPattern', "Pattern")

    def draw_buttons(self, context, layout):

        column=layout.column()
        im=None
        for image in bpy.data.images:
            if image.name == self.image:
                im=image
        split = column.split(percentage=0.8,align=True)
        split.prop_search(self,"image",context.blend_data,"images",text="")
        split.operator("pov.imageopen",text="",icon="FILESEL")
        if im is not None:
            column.prop(im,"source",text="")
        column.prop(self,"map_type",text="")
        column.prop(self,"interpolate",text="")
        row=column.row()
        row.prop(self,"premultiplied",text="Premul")
        row.prop(self,"once",text="Once")
        column.prop(self,"use_alpha",text="Use alpha")

    def draw_buttons_ext(self, context, layout):

        column=layout.column()
        im=None
        for image in bpy.data.images:
            if image.name == self.image:
                im=image
        split = column.split(percentage=0.8,align=True)
        split.prop_search(self,"image",context.blend_data,"images",text="")
        split.operator("pov.imageopen",text="",icon="FILESEL")
        if im is not None:
            column.prop(im,"source",text="")
        column.prop(self,"map_type",text="")
        column.prop(self,"interpolate",text="")
        row=column.row()
        row.prop(self,"premultiplied",text="Premul")
        row.prop(self,"once",text="Once")

    def draw_label(self):
        return "Image pattern"

class ShaderPatternNode(Node, ObjectNodeTree):
    '''Pattern'''
    bl_idname = 'ShaderPatternNode'
    bl_label = 'Other patterns'

    pattern = EnumProperty(
            name="Pattern",
            description="Agate, Crackle, Gradient, Pavement, Spiral, Tiling",
            items=(('agate', "Agate", ""),('crackle', "Crackle", ""),('gradient', "Gradient", ""),
                   ('pavement', "Pavement", ""),
                   ('spiral1', "Spiral 1", ""),
                   ('spiral2', "Spiral 2", ""),
                   ('tiling', "Tiling", "")),
            default='agate')

    agate_turb = FloatProperty(
            name="Agate turb",
            description="Agate turbulence",
            min=0.0, max=100.0, default=0.5)

    crackle_form_x = FloatProperty(
            name="X",
            description="Form vector X",
            min=-150.0, max=150.0, default=-1)

    crackle_form_y = FloatProperty(
            name="Y",
            description="Form vector Y",
            min=-150.0, max=150.0, default=1)

    crackle_form_z = FloatProperty(
            name="Z",
            description="Form vector Z",
            min=-150.0, max=150.0, default=0)

    crackle_metric = FloatProperty(
            name="Metric",
            description="Crackle metric",
            min=0.0, max=150.0, default=1)

    crackle_solid = BoolProperty(
            name="Solid",
            description="Crackle solid",
            default=False)

    spiral_arms = FloatProperty(
            name="Number",
            description="",
            min=0.0, max=256.0, default=2.0)

    tiling_number = IntProperty(
            name="Number",
            description="",
            min=1, max=27, default=1)

    gradient_orient = EnumProperty(
            name="Orient",
            description="",
            items=(('x', "X", ""),
                   ('y', "Y", ""),
                   ('z', "Z", "")),
            default='x')

    def init(self, context):

        pat = self.outputs.new('PovraySocketPattern', "Pattern")

    def draw_buttons(self, context, layout):

        layout.prop(self, "pattern",text="")
        if self.pattern=='agate':
            layout.prop(self, "agate_turb")
        if self.pattern=='crackle':
            layout.prop(self, "crackle_metric")
            layout.prop(self, "crackle_solid")
            layout.label("Form:")
            layout.prop(self, "crackle_form_x")
            layout.prop(self, "crackle_form_y")
            layout.prop(self, "crackle_form_z")
        if self.pattern in {"spiral1","spiral2"}:
            layout.prop(self, "spiral_arms")
        if self.pattern in {'tiling'}:
            layout.prop(self, "tiling_number")
        if self.pattern in {'gradient'}:
            layout.prop(self, "gradient_orient")
    def draw_buttons_ext(self, context, layout):
        pass

    def draw_label(self):
        return "Other patterns"

class ShaderTextureMapNode(Node, ObjectNodeTree):
    '''Texture Map'''
    bl_idname = 'ShaderTextureMapNode'
    bl_label = 'Texture map'

    brick_size_x = FloatProperty(
            name="X",
            description="",
            min=0.0000, max=1.0000, default=0.2500)

    brick_size_y = FloatProperty(
            name="Y",
            description="",
            min=0.0000, max=1.0000, default=0.0525)

    brick_size_z = FloatProperty(
            name="Z",
            description="",
            min=0.0000, max=1.0000, default=0.1250)

    brick_mortar = FloatProperty(
            name="Mortar",
            description="Mortar",
            min=0.000, max=1.500, default=0.01)

    def init(self, context):
        mat = bpy.context.object.active_material
        self.inputs.new('PovraySocketPattern', "")
        color = self.inputs.new('NodeSocketColor', "Color ramp")
        color.hide_value = True
        for i in range(0,4):
            transform=self.inputs.new('PovraySocketTransform', "Transform")
            transform.hide_value=True
        number = mat.pov.inputs_number
        for i in range(number):
            self.inputs.new('PovraySocketTexture', "%s"%i)


        self.outputs.new('PovraySocketTexture', "Texture")

    def draw_buttons(self, context, layout):

        if self.inputs[0].default_value =='brick':
            layout.prop(self, "brick_mortar")
            layout.label("Brick size:")
            layout.prop(self, "brick_size_x")
            layout.prop(self, "brick_size_y")
            layout.prop(self, "brick_size_z")

    def draw_buttons_ext(self, context, layout):

        if self.inputs[0].default_value =='brick':
            layout.prop(self, "brick_mortar")
            layout.label("Brick size:")
            layout.prop(self, "brick_size_x")
            layout.prop(self, "brick_size_y")
            layout.prop(self, "brick_size_z")

    def draw_label(self):
        return "Texture map"


class ShaderNormalMapNode(Node, ObjectNodeTree):
    '''Normal Map'''
    bl_idname = 'ShaderNormalMapNode'
    bl_label = 'Normal map'

    brick_size_x = FloatProperty(
            name="X",
            description="",
            min=0.0000, max=1.0000, default=0.2500)

    brick_size_y = FloatProperty(
            name="Y",
            description="",
            min=0.0000, max=1.0000, default=0.0525)

    brick_size_z = FloatProperty(
            name="Z",
            description="",
            min=0.0000, max=1.0000, default=0.1250)

    brick_mortar = FloatProperty(
            name="Mortar",
            description="Mortar",
            min=0.000, max=1.500, default=0.01)

    def init(self, context):
        self.inputs.new('PovraySocketPattern', "")
        normal = self.inputs.new('PovraySocketFloat_10', "Normal")
        slope = self.inputs.new('PovraySocketMap', "Slope map")
        for i in range(0,4):
            transform=self.inputs.new('PovraySocketTransform', "Transform")
            transform.hide_value=True
        self.outputs.new('PovraySocketNormal', "Normal")

    def draw_buttons(self, context, layout):
        #for i, inp in enumerate(self.inputs):

        if self.inputs[0].default_value =='brick':
            layout.prop(self, "brick_mortar")
            layout.label("Brick size:")
            layout.prop(self, "brick_size_x")
            layout.prop(self, "brick_size_y")
            layout.prop(self, "brick_size_z")

    def draw_buttons_ext(self, context, layout):

        if self.inputs[0].default_value =='brick':
            layout.prop(self, "brick_mortar")
            layout.label("Brick size:")
            layout.prop(self, "brick_size_x")
            layout.prop(self, "brick_size_y")
            layout.prop(self, "brick_size_z")

    def draw_label(self):
        return "Normal map"

class ShaderNormalMapEntryNode(Node, ObjectNodeTree):
    '''Normal Map Entry'''
    bl_idname = 'ShaderNormalMapEntryNode'
    bl_label = 'Normal map entry'

    def init(self, context):
        self.inputs.new('PovraySocketFloat_0_1', "Stop")
        self.inputs.new('PovraySocketFloat_0_1', "Gray")
    def draw_label(self):
        return "Normal map entry"

class IsoPropsNode(Node, CompositorNodeTree):
    '''ISO Props'''
    bl_idname = 'IsoPropsNode'
    bl_label = 'Iso'
    node_label = StringProperty(maxlen=1024)
    def init(self, context):
        ob = bpy.context.object
        self.node_label = ob.name
        textName = ob.pov.function_text
        if textName:
            text = bpy.data.texts[textName]
            for line in text.lines:
                split = line.body.split()
                if split[0] == "#declare":
                    socket = self.inputs.new('NodeSocketFloat', "%s"%split[1])
                    value = split[3].split(";")
                    value = value[0]
                    socket.default_value=float(value)
    def draw_label(self):
        return self.node_label

class PovrayFogNode(Node, CompositorNodeTree):
    '''Fog settings'''
    bl_idname = 'PovrayFogNode'
    bl_label = 'Fog'
    def init(self, context):
        color=self.inputs.new('NodeSocketColor', "Color")
        color.default_value=(0.7,0.7,0.7,0.25)
        self.inputs.new('PovraySocketFloat_0_1', "Filter")
        distance = self.inputs.new('NodeSocketInt', "Distance")
        distance.default_value=150
        self.inputs.new('NodeSocketBool', "Ground")
        fog_offset=self.inputs.new('NodeSocketFloat', "Offset")
        fog_alt=self.inputs.new('NodeSocketFloat', "Altitude")
        turb = self.inputs.new('NodeSocketVector', "Turbulence")
        turb_depth=self.inputs.new('PovraySocketFloat_0_10', "Depth")
        turb_depth.default_value=0.5
        octaves=self.inputs.new('PovraySocketInt_1_9', "Octaves")
        octaves.default_value=5
        lambdat=self.inputs.new('PovraySocketFloat_0_10', "Lambda")
        lambdat.default_value=1.25
        omega=self.inputs.new('PovraySocketFloat_0_10', "Omega")
        omega.default_value=0.35
        translate = self.inputs.new('NodeSocketVector', "Translate")
        rotate = self.inputs.new('NodeSocketVector', "Rotate")
        scale = self.inputs.new('NodeSocketVector', "Scale")
        scale.default_value=(1,1,1)
    def draw_label(self):
        return "Fog"

class PovraySlopeNode(Node, TextureNodeTree):
    '''Output'''
    bl_idname = 'PovraySlopeNode'
    bl_label = 'Slope Map'

    def init(self, context):
        self.use_custom_color = True
        self.color = (0,0.2,0)
        slope = self.inputs.new('PovraySocketSlope', "0")
        slope = self.inputs.new('PovraySocketSlope', "1")
        slopemap = self.outputs.new('PovraySocketMap', "Slope map")
        output.hide_value = True
    def draw_buttons(self, context, layout):

        layout.operator("pov.nodeinputadd")
        row = layout.row()
        row.label('Value')
        row.label('Height')
        row.label('Slope')

    def draw_buttons_ext(self, context, layout):

        layout.operator("pov.nodeinputadd")
        row = layout.row()
        row.label('Value')
        row.label('Height')
        row.label('Slope')

    def draw_label(self):
        return "Slope Map"

######################################## Texture nodes ###############################
class TextureOutputNode(Node, TextureNodeTree):
    '''Output'''
    bl_idname = 'TextureOutputNode'
    bl_label = 'Color Map'

    def init(self, context):
        tex = bpy.context.object.active_material.active_texture
        num_sockets = int(tex.pov.density_lines/32)
        for i in range(num_sockets):
            color = self.inputs.new('NodeSocketColor', "%s"%i)
            color.hide_value = True

    def draw_buttons(self, context, layout):

        layout.label("Color Ramps:")

    def draw_label(self):
        return "Color Map"


##################################################################################
#################################Operators########################################
##################################################################################


class NODE_OT_iso_add(bpy.types.Operator):
    bl_idname = "pov.nodeisoadd"
    bl_label = "Create iso props"

    def execute(self, context):
        ob = bpy.context.object
        if bpy.context.scene.use_nodes == False:
            bpy.context.scene.use_nodes = True
        tree = bpy.context.scene.node_tree
        for node in tree.nodes:
            if node.bl_idname == "IsoPropsNode" and node.label == ob.name:
                tree.nodes.remove(node)
        isonode = tree.nodes.new('IsoPropsNode')
        isonode.location = (0,0)
        isonode.label = ob.name
        return {'FINISHED'}

class NODE_OT_map_create(bpy.types.Operator):
    bl_idname = "node.map_create"
    bl_label = "Create map"

    def execute(self, context):
        x = y = 0
        space = context.space_data
        tree = space.edit_tree
        for node in tree.nodes:
            if node.select == True:
                x,y = node.location
            node.select=False
        tmap = tree.nodes.new('ShaderTextureMapNode')
        tmap.location = (x - 200,y)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def draw(self, context):
        layout = self.layout
        mat = context.object.active_material
        layout.prop(mat.pov,"inputs_number")

class NODE_OT_povray_node_texture_map_add(bpy.types.Operator):
    bl_idname = "pov.nodetexmapadd"
    bl_label = "Texture map"

    def execute(self, context):
        tree=bpy.context.object.active_material.node_tree
        tmap = tree.nodes.active
        bpy.context.object.active_material.node_tree.nodes.active=tmap
        el=tmap.color_ramp.elements.new(0.5)
        for el in tmap.color_ramp.elements:
            el.color=(0,0,0,1)
        for inp in tmap.inputs:
            tmap.inputs.remove(inp)
        for outp in tmap.outputs:
            tmap.outputs.remove(outp)
        pattern=tmap.inputs.new('NodeSocketVector', "Pattern")
        pattern.hide_value=True
        for i in range(0,3):
            tmap.inputs.new('NodeSocketColor', "Shader")
        tmap.outputs.new('NodeSocketShader', "BSDF")
        tmap.label="Texture Map"
        return {'FINISHED'}


class NODE_OT_povray_node_output_add(bpy.types.Operator):
    bl_idname = "pov.nodeoutputadd"
    bl_label = "Output"

    def execute(self, context):
        tree=bpy.context.object.active_material.node_tree
        tmap = tree.nodes.new('ShaderNodeOutputMaterial')
        bpy.context.object.active_material.node_tree.nodes.active=tmap
        for inp in tmap.inputs:
            tmap.inputs.remove(inp)
        tmap.inputs.new('NodeSocketShader', "Surface")
        tmap.label="Output"
        return {'FINISHED'}

class NODE_OT_povray_node_layered_add(bpy.types.Operator):
    bl_idname = "pov.nodelayeredadd"
    bl_label = "Layered material"

    def execute(self, context):
        tree=bpy.context.object.active_material.node_tree
        tmap = tree.nodes.new('ShaderNodeAddShader')
        bpy.context.object.active_material.node_tree.nodes.active=tmap
        tmap.label="Layered material"
        return {'FINISHED'}

class NODE_OT_povray_input_add(bpy.types.Operator):
    bl_idname = "pov.nodeinputadd"
    bl_label = "Add entry"

    def execute(self, context):
        node=bpy.context.object.active_material.node_tree.nodes.active
        if node.type in {'VALTORGB'}:
            number=1
            for inp in node.inputs:
                if inp.type=='SHADER':
                    number+=1
            node.inputs.new('NodeSocketShader', "%s"%number)
            els=node.color_ramp.elements
            pos1=els[len(els)-1].position
            pos2=els[len(els)-2].position
            pos=(pos1-pos2)/2+pos2
            el=els.new(pos)

        if node.bl_idname == 'PovraySlopeNode':
            number=len(node.inputs)
            node.inputs.new('PovraySocketSlope', "%s"%number)


        return {'FINISHED'}

class NODE_OT_povray_input_remove(bpy.types.Operator):
    bl_idname = "pov.nodeinputremove"
    bl_label = "Remove input"

    def execute(self, context):
        node=bpy.context.object.active_material.node_tree.nodes.active
        if node.type in {'VALTORGB','ADD_SHADER'}:
            number=len(node.inputs)-1
            if number > 5:
                inp=node.inputs[number]
                node.inputs.remove(inp)
                if node.type in {'VALTORGB'}:
                    els=node.color_ramp.elements
                    number=len(els)-2
                    el=els[number]
                    els.remove(el)
        return {'FINISHED'}

class NODE_OT_povray_image_open(bpy.types.Operator):
    bl_idname = "pov.imageopen"
    bl_label = "Open"

    filepath = StringProperty(
            name="File Path",
            description="Open image",
            maxlen=1024,
            subtype='FILE_PATH',
            )

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        im=bpy.data.images.load(self.filepath)
        node=context.object.active_material.node_tree.nodes.active
        node.image = im.name
        return {'FINISHED'}


# class TEXTURE_OT_povray_open_image(bpy.types.Operator):
    # bl_idname = "pov.openimage"
    # bl_label = "Open Image"

    # filepath = StringProperty(
            # name="File Path",
            # description="Open image",
            # maxlen=1024,
            # subtype='FILE_PATH',
            # )

    # def invoke(self, context, event):
        # context.window_manager.fileselect_add(self)
        # return {'RUNNING_MODAL'}

    # def execute(self, context):
        # im=bpy.data.images.load(self.filepath)
        # tex = context.texture
        # tex.pov.image = im.name
        # scene = context.scene
        # scene.update()
        # return {'FINISHED'}

class PovrayPatternNode(bpy.types.Operator):
    bl_idname = "pov.patternnode"
    bl_label  = "Pattern"

    add=True

    def execute(self, context):
        space = context.space_data
        tree = space.edit_tree
        for node in tree.nodes:
            node.select=False
        if self.add==True:
            tmap = tree.nodes.new('ShaderNodeValToRGB')
            tmap.label="Pattern"
            for inp in tmap.inputs:
                tmap.inputs.remove(inp)
            for outp in tmap.outputs:
                tmap.outputs.remove(outp)
            pattern = tmap.inputs.new('PovraySocketPattern', "Pattern")
            pattern.hide_value = True
            mapping=tmap.inputs.new('NodeSocketVector', "Mapping")
            mapping.hide_value=True
            transform=tmap.inputs.new('NodeSocketVector', "Transform")
            transform.hide_value=True
            modifier=tmap.inputs.new('NodeSocketVector', "Modifier")
            modifier.hide_value=True
            for i in range(0,2):
                tmap.inputs.new('NodeSocketShader', "%s"%(i+1))
            tmap.outputs.new('NodeSocketShader', "Material")
            tmap.outputs.new('NodeSocketColor', "Color")
            tree.nodes.active=tmap
            self.add=False
        aNode=tree.nodes.active
        aNode.select=True
        v2d = context.region.view2d
        x, y = v2d.region_to_view(self.x, self.y)
        aNode.location = (x, y)

    def modal(self, context, event):
        if event.type == 'MOUSEMOVE':
            self.x = event.mouse_region_x
            self.y = event.mouse_region_y
            self.execute(context)
            return {'RUNNING_MODAL'}
        elif event.type == 'LEFTMOUSE':
            return {'FINISHED'}
        elif event.type in ('RIGHTMOUSE', 'ESC'):
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

class UpdatePreviewMaterial(bpy.types.Operator):
    '''Operator update preview material'''
    bl_idname = "node.updatepreview"
    bl_label = "Update preview"

    def execute(self, context):
        scene=context.scene
        ob=context.object
        for obj in scene.objects:
            if obj != ob:
                scene.objects.active=obj
                break
        scene.objects.active=ob

    def modal(self, context, event):
        if event.type == 'RIGHTMOUSE':
            self.execute(context)
            return {'FINISHED'}
        return {'PASS_THROUGH'}

    def invoke(self, context, event):
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

class UpdatePreviewKey(bpy.types.Operator):
    '''Operator update preview keymap'''
    bl_idname = "wm.updatepreviewkey"
    bl_label = "Activate RMB"
    @classmethod
    def poll(cls, context):
        conf = context.window_manager.keyconfigs.active
        mapstr = "Node Editor"
        map = conf.keymaps[mapstr]
        try:
            map.keymap_items["node.updatepreview"]
            return False
        except:
            return True

    def execute(self, context):
        conf = context.window_manager.keyconfigs.active
        mapstr = "Node Editor"
        map = conf.keymaps[mapstr]
        map.keymap_items.new("node.updatepreview",type='RIGHTMOUSE',value="PRESS")
        return {'FINISHED'}

