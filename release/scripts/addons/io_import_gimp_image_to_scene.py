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
    "name": "Import GIMP Image to Scene (.xcf/.xjt)",
    "author": "Daniel Salazar (ZanQdo)",
    "version": (2, 0, 1),
    "blender": (2, 73, 0),
    "location": "File > Import > GIMP Image to Scene(.xcf/.xjt)",
    "description": "Imports GIMP multilayer image files as a series of multiple planes",
    "warning": "XCF import requires xcftools installed",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/GIMPImageToScene",
    "category": "Import-Export",
}

"""
This script imports GIMP layered image files into 3D Scenes (.xcf, .xjt)
"""

def main(report, File, Path, LayerViewers, MixerViewers, LayerOffset,
         LayerScale, OpacityMode, AlphaMode, ShadelessMats,
         SetCamera, SetupCompo, GroupUntagged, Ext):

    #-------------------------------------------------

    #Folder = '['+File.rstrip(Ext)+']'+'_images/'
    Folder = 'images_'+'['+File.rstrip(Ext)+']/'

    if not bpy.data.is_saved:
        PathSaveRaw = Path+Folder
        PathSave = PathSaveRaw.replace(' ', '\ ')
        try: os.mkdir(PathSaveRaw)
        except: pass
    else:
        PathSave = bpy.data.filepath
        RSlash = PathSave.rfind('/')
        PathSaveRaw = PathSave[:RSlash+1]+Folder
        PathSave = PathSaveRaw.replace(' ', '\ ')
        try: os.mkdir(PathSaveRaw)
        except: pass
        PathSaveRaw = bpy.path.relpath(PathSaveRaw)+'/'

    PathRaw = Path
    Path = Path.replace(' ', '\ ')
    if Ext == '.xjt':
        ExtSave = '.jpg'
        #-------------------------------------------------
        # EXTRACT XJT
        import tarfile

        IMG = tarfile.open ('%s%s' % (PathRaw, File))
        PRP = IMG.extractfile('PRP')

        Members = IMG.getmembers()

        for Member in Members:
            Name = Member.name
            if Name.startswith('l') and Name.endswith('.jpg'):
                IMG.extract(Name, path=PathSaveRaw)

        #-------------------------------------------------
        # INFO XJT
        IMGs = []
        for Line in PRP.readlines():
            Line = str(Line)

            if Line.startswith("b'GIMP_XJ_IMAGE"):
                for Segment in Line.split():
                    if Segment.startswith('w/h:'):
                        ResX, ResY = map (int, Segment[4:].split(','))
            if Line.startswith(("b'L", "b'l")):

                """The "nice" method to check if layer has alpha channel
                sadly GIMP sometimes decides not to export an alpha channel
                if it's pure white so we are not completly sure here yet"""
                if Line.startswith("b'L"): HasAlpha = True
                else: HasAlpha = False

                md = None
                op = 1
                ox, oy = 0,0

                for Segment in Line.split():

                    if Segment.startswith("b'"):
                        imageFile = 'l' + Segment[3:] + '.jpg'
                        imageFileAlpha ='la'+Segment[3:]+'.jpg'

                        """Phisically double checking if alpha image exists
                        now we can be sure! (damn GIMP)"""
                        if HasAlpha:
                            if not os.path.isfile(PathSaveRaw+imageFileAlpha): HasAlpha = False

                        # Get Widht and Height from images
                        data = open(PathSaveRaw+imageFile, "rb").read()

                        hexList = []
                        for ch in data:
                            byt = "%02X" % ch
                            hexList.append(byt)

                        for k in range(len(hexList)-1):
                            if hexList[k] == 'FF' and (hexList[k+1] == 'C0' or hexList[k+1] == 'C2'):
                                ow = int(hexList[k+7],16)*256 + int(hexList[k+8],16)
                                oh = int(hexList[k+5],16)*256 + int(hexList[k+6],16)

                    elif Segment.startswith('md:'): # mode
                        md = Segment[3:]

                    elif Segment.startswith('op:'): # opacity
                        op = float(Segment[3:])*.01

                    elif Segment.startswith('o:'): # origin
                        ox, oy = map(int, Segment[2:].split(','))

                    elif Segment.startswith('n:'): # name
                        n = Segment[3:-4]
                        OpenBracket = n.find ('[')
                        CloseBracket = n.find (']')

                        if OpenBracket != -1 and CloseBracket != -1:
                            RenderLayer = n[OpenBracket+1:CloseBracket]
                            NameShort = n[:OpenBracket]

                        else:
                            RenderLayer = n
                            NameShort = n

                        os.rename(PathSaveRaw+imageFile, PathSaveRaw+NameShort+'.jpg')
                        if HasAlpha: os.rename(PathSaveRaw+imageFileAlpha, PathSaveRaw+NameShort+'_A'+'.jpg')

                IMGs.append({'LayerMode':md, 'LayerOpacity':op,
                            'LayerName':n, 'LayerNameShort':NameShort,
                            'RenderLayer':RenderLayer, 'LayerCoords':[ow, oh, ox, oy], 'HasAlpha':HasAlpha})

    else: # Ext == '.xcf':
        ExtSave = '.png'
        #-------------------------------------------------
        # CONFIG
        XCFInfo = 'xcfinfo'
        XCF2PNG = 'xcf2png'
        #-------------------------------------------------
        # INFO XCF

        try:
            Info = subprocess.check_output((XCFInfo, Path+File))
        except FileNotFoundError as e:
            if XCFInfo in str(e):
                report({'ERROR'}, "Please install xcftools, xcfinfo seems to be missing (%s)" % str(e))
                return False
            else:
                raise e

        Info = Info.decode()
        IMGs = []
        for Line in Info.split('\n'):
            if Line.startswith ('+'):

                Line = Line.split(' ', 4)

                RenderLayer = Line[4]

                OpenBracket = RenderLayer.find ('[')
                CloseBracket = RenderLayer.find (']')

                if OpenBracket != -1 and CloseBracket != -1:
                    RenderLayer = RenderLayer[OpenBracket+1:CloseBracket]
                    NameShort = Line[4][:OpenBracket]
                else:
                    NameShort = Line[4].rstrip()
                    if GroupUntagged:
                        RenderLayer = '__Undefined__'
                    else:
                        RenderLayer = NameShort

                LineThree = Line[3]
                Slash = LineThree.find('/')
                if Slash == -1:
                    Mode = LineThree
                    Opacity = 1
                else:
                    Mode = LineThree[:Slash]
                    Opacity = float(LineThree[Slash+1:LineThree.find('%')])*.01

                IMGs.append ({
                    'LayerMode': Mode,
                    'LayerOpacity': Opacity,
                    'LayerName': Line[4].rstrip(),
                    'LayerNameShort': NameShort,
                    'LayerCoords': list(map(int, Line[1].replace('x', ' ').replace('+', ' +').replace('-', ' -').split())),
                    'RenderLayer': RenderLayer,
                    'HasAlpha': True,
                    })
            elif Line.startswith('Version'):
                ResX, ResY = map (int, Line.split()[2].split('x'))

        #-------------------------------------------------
        # EXTRACT XCF
        if OpacityMode == 'BAKE':
            Opacity = ()
        else:
            Opacity = ("--percent", "100")
        xcf_path = Path + File
        for Layer in IMGs:
            png_path = "%s%s.png" % (PathSave, Layer['LayerName'].replace(' ', '_'))
            subprocess.call((XCF2PNG, "-C", xcf_path, "-o", png_path, Layer['LayerName']) + Opacity)

    #-------------------------------------------------
    Scene = bpy.context.scene
    #-------------------------------------------------
    # CAMERA

    if SetCamera:
        bpy.ops.object.camera_add(location=(0, 0, 10))

        Camera = bpy.context.active_object.data

        Camera.type = 'ORTHO'
        Camera.ortho_scale = ResX * .01

    #-------------------------------------------------
    # RENDER SETTINGS

    Render = Scene.render

    if SetCamera:
        Render.resolution_x = ResX
        Render.resolution_y = ResY
        Render.resolution_percentage = 100
    Render.alpha_mode = 'TRANSPARENT'

    #-------------------------------------------------
    # 3D VIEW SETTINGS

    Scene.game_settings.material_mode = 'GLSL'

    Areas = bpy.context.screen.areas

    for Area in Areas:
        if Area.type == 'VIEW_3D':
            Area.spaces.active.viewport_shade = 'TEXTURED'
            Area.spaces.active.show_textured_solid = True
            Area.spaces.active.show_floor = False

    #-------------------------------------------------
    # 3D LAYERS

    def Make3DLayer (Name, NameShort, Z, Coords, RenderLayer, LayerMode, LayerOpacity, HasAlpha):

        # RenderLayer

        if SetupCompo:
            if not bpy.context.scene.render.layers.get(RenderLayer):

                bpy.ops.scene.render_layer_add()

                LayerActive = bpy.context.scene.render.layers.active
                LayerActive.name = RenderLayer
                LayerActive.use_pass_vector = True
                LayerActive.use_sky = False
                LayerActive.use_edge_enhance = False
                LayerActive.use_strand = False
                LayerActive.use_halo = False

                global LayerNum
                for i in range (0,20):
                    if not i == LayerNum:
                        LayerActive.layers[i] = False

                bpy.context.scene.layers[LayerNum] = True

                LayerFlags[RenderLayer] = bpy.context.scene.render.layers.active.layers

                LayerList.append([RenderLayer, LayerMode, LayerOpacity])

                LayerNum += 1

        # Object
        bpy.ops.mesh.primitive_plane_add(view_align=False,
                                         enter_editmode=False,
                                         rotation=(0, 0, 0))

        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)


        Active = bpy.context.active_object

        if SetupCompo:
            Active.layers = LayerFlags[RenderLayer]

        Active.location = (
            (float(Coords[2])-(ResX*0.5))*LayerScale,
            (-float(Coords[3])+(ResY*0.5))*LayerScale, Z)

        for Vert in Active.data.vertices:
            Vert.co[0] += 1
            Vert.co[1] += -1

        Active.dimensions = float(Coords[0])*LayerScale, float(Coords[1])*LayerScale, 0

        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='MEDIAN')

        Active.show_wire = True

        Active.name = NameShort
        bpy.ops.mesh.uv_texture_add()

        # Material

        '''if bpy.data.materials.get(NameShort):
            Mat = bpy.data.materials[NameShort]
            if not Active.material_slots:
                bpy.ops.object.material_slot_add()
            Active.material_slots[0].material = Mat
        else:'''

        Mat = bpy.data.materials.new(NameShort)
        Mat.diffuse_color = (1,1,1)
        Mat.use_raytrace = False
        Mat.use_shadows = False
        Mat.use_cast_buffer_shadows = False
        Mat.use_cast_approximate = False
        if HasAlpha:
            Mat.use_transparency = True
            if OpacityMode == 'MAT': Mat.alpha = LayerOpacity
            else: Mat.alpha = 0
        if ShadelessMats: Mat.use_shadeless = True

        if Ext == '.xcf':
            # Color & Alpha PNG
            Tex = bpy.data.textures.new(NameShort, 'IMAGE')
            Tex.extension = 'CLIP'
            Tex.use_preview_alpha = True

            Img = bpy.data.images.new(NameShort, 128, 128)
            Img.source = 'FILE'
            Img.alpha_mode = AlphaMode
            Img.filepath = '%s%s%s' % (PathSaveRaw, Name, ExtSave)

            UVFace = Active.data.uv_textures[0].data[0]
            UVFace.image = Img

            Tex.image = Img

            Mat.texture_slots.add()
            TexSlot = Mat.texture_slots[0]
            TexSlot.texture = Tex
            TexSlot.use_map_alpha = True
            TexSlot.texture_coords = 'UV'
            if OpacityMode == 'TEX': TexSlot.alpha_factor = LayerOpacity
            elif OpacityMode == 'MAT': TexSlot.blend_type = 'MULTIPLY'

        else: # Ext == '.xjt'
            # Color JPG
            Tex = bpy.data.textures.new(NameShort, 'IMAGE')
            Tex.extension = 'CLIP'

            Img = bpy.data.images.new(NameShort, 128, 128)
            Img.source = 'FILE'
            Img.filepath = '%s%s%s' % (PathSaveRaw, Name, ExtSave)

            UVFace = Active.data.uv_textures[0].data[0]
            UVFace.image = Img

            Tex.image = Img

            Mat.texture_slots.add()
            TexSlot = Mat.texture_slots[0]
            TexSlot.texture = Tex
            TexSlot.texture_coords = 'UV'

            if HasAlpha:
                # Alpha JPG
                Tex = bpy.data.textures.new(NameShort+'_A', 'IMAGE')
                Tex.extension = 'CLIP'
                Tex.use_preview_alpha = True

                Img = bpy.data.images.new(NameShort+'_A', 128, 128)
                Img.source = 'FILE'
                Img.alpha_mode = AlphaMode
                Img.filepath = '%s%s_A%s' % (PathSaveRaw, Name, ExtSave)
                Img.use_alpha = False

                Tex.image = Img

                Mat.texture_slots.add()
                TexSlot = Mat.texture_slots[1]
                TexSlot.texture = Tex
                TexSlot.use_map_alpha = True
                TexSlot.use_map_color_diffuse = False
                TexSlot.texture_coords = 'UV'
                if OpacityMode == 'TEX': TexSlot.alpha_factor = LayerOpacity
                elif OpacityMode == 'MAT': TexSlot.blend_type = 'MULTIPLY'

        if not Active.material_slots:
            bpy.ops.object.material_slot_add()

        Active.material_slots[0].material = Mat


    Z = 0
    global LayerNum
    LayerNum = 0
    LayerFlags = {}
    LayerList = []

    for Layer in IMGs:
        Make3DLayer(Layer['LayerName'].replace(' ', '_'),
                    Layer['LayerNameShort'].replace(' ', '_'),
                    Z,
                    Layer['LayerCoords'],
                    Layer['RenderLayer'],
                    Layer['LayerMode'],
                    Layer['LayerOpacity'],
                    Layer['HasAlpha'],
                    )

        Z -= LayerOffset

    if SetupCompo:
        #-------------------------------------------------
        # COMPO NODES

        Scene.use_nodes = True

        Tree = Scene.node_tree

        for i in Tree.nodes:
            Tree.nodes.remove(i)

        LayerList.reverse()

        Offset = 0
        LayerLen = len(LayerList)

        for Layer in LayerList:

            Offset += 1

            X_Offset = (500*Offset)
            Y_Offset = (-300*Offset)

            Node = Tree.nodes.new('CompositorNodeRLayers')
            Node.location = (-500+X_Offset, 300+Y_Offset)
            Node.name = 'R_'+ str(Offset)
            Node.scene = Scene
            Node.layer = Layer[0]

            if LayerViewers:
                Node_V = Tree.nodes.new('CompositorNodeViewer')
                Node_V.name = Layer[0]
                Node_V.location = (-200+X_Offset, 200+Y_Offset)

                Tree.links.new(Node.outputs[0], Node_V.inputs[0])

            if LayerLen > Offset:

                Mode = LayerList[Offset][1] # has to go one step further
                LayerOpacity = LayerList[Offset][2]

                if not Mode in {'Normal', '-1'}:

                    Node = Tree.nodes.new('CompositorNodeMixRGB')
                    if OpacityMode == 'COMPO': Node.inputs['Fac'].default_value = LayerOpacity
                    else: Node.inputs['Fac'].default_value = 1
                    Node.use_alpha = True

                    if Mode in {'Addition', '7'}: Node.blend_type = 'ADD'
                    elif Mode in {'Subtract', '8'}: Node.blend_type = 'SUBTRACT'
                    elif Mode in {'Multiply', '3'}: Node.blend_type = 'MULTIPLY'
                    elif Mode in {'DarkenOnly', '9'}: Node.blend_type = 'DARKEN'
                    elif Mode in {'Dodge', '16'}: Node.blend_type = 'DODGE'
                    elif Mode in {'LightenOnly', '10'}: Node.blend_type = 'LIGHTEN'
                    elif Mode in {'Difference', '6'}: Node.blend_type = 'DIFFERENCE'
                    elif Mode in {'Divide', '15'}: Node.blend_type = 'DIVIDE'
                    elif Mode in {'Overlay', '5'}: Node.blend_type = 'OVERLAY'
                    elif Mode in {'Screen', '4'}: Node.blend_type = 'SCREEN'
                    elif Mode in {'Burn', '17'}: Node.blend_type = 'BURN'
                    elif Mode in {'Color', '13'}: Node.blend_type = 'COLOR'
                    elif Mode in {'Value', '14'}: Node.blend_type = 'VALUE'
                    elif Mode in {'Saturation', '12'}: Node.blend_type = 'SATURATION'
                    elif Mode in {'Hue', '11'}: Node.blend_type = 'HUE'
                    elif Mode in {'Softlight', '19'}: Node.blend_type = 'SOFT_LIGHT'
                    else: pass

                else:
                    Node = Tree.nodes.new('CompositorNodeAlphaOver')
                    if OpacityMode == 'COMPO': Node.inputs['Fac'].default_value = LayerOpacity
                Node.name = 'M_' + str(Offset)
                Node.location = (300+X_Offset, 250+Y_Offset)

                if MixerViewers:
                    Node_V = Tree.nodes.new('CompositorNodeViewer')
                    Node_V.name = Layer[0]
                    Node_V.location = (500+X_Offset, 350+Y_Offset)

                    Tree.links.new(Node.outputs[0], Node_V.inputs[0])

            else:
                Node = Tree.nodes.new('CompositorNodeComposite')
                Node.name = 'Composite'
                Node.location = (400+X_Offset, 350+Y_Offset)

        Nodes = bpy.context.scene.node_tree.nodes

        if LayerLen > 1:
            for i in range (1, LayerLen + 1):
                if i == 1:
                    Tree.links.new(Nodes['R_'+str(i)].outputs[0], Nodes['M_'+str(i)].inputs[1])
                if 1 < i < LayerLen:
                    Tree.links.new(Nodes['M_'+str(i-1)].outputs[0], Nodes['M_'+str(i)].inputs[1])
                if 1 < i < LayerLen+1:
                    Tree.links.new(Nodes['R_'+str(i)].outputs[0], Nodes['M_'+str(i-1)].inputs[2])
                if i == LayerLen:
                    Tree.links.new(Nodes['M_'+str(i-1)].outputs[0], Nodes['Composite'].inputs[0])
        else:
            Tree.links.new(Nodes['R_1'].outputs[0], Nodes['Composite'].inputs[0])

        for i in Tree.nodes:
            i.location[0] += -250*Offset
            i.location[1] += 150*Offset

    return True

#------------------------------------------------------------------------
import os, subprocess
import bpy
from bpy.props import *
from math import pi

# Operator
class GIMPImageToScene(bpy.types.Operator):
    """"""
    bl_idname = "import.gimp_image_to_scene"
    bl_label = "GIMP Image to Scene"
    bl_description = "Imports GIMP multilayer image files into 3D Scenes"
    bl_options = {'REGISTER', 'UNDO'}

    filename = StringProperty(name="File Name",
        description="Name of the file")
    directory = StringProperty(name="Directory",
        description="Directory of the file")

    LayerViewers = BoolProperty(name="Layer Viewers",
        description="Add Viewer nodes to each Render Layer node",
        default=True)

    MixerViewers = BoolProperty(name="Mixer Viewers",
        description="Add Viewer nodes to each Mix node",
        default=True)

    AlphaMode = EnumProperty(name="Alpha Mode",
        description="Representation of alpha information in the RGBA pixels",
        items=(
            ('STRAIGHT', 'Texture Alpha Factor', 'Transparent RGB and alpha pixels are unmodified'),
            ('PREMUL', 'Material Alpha Value', 'Transparent RGB pixels are multiplied by the alpha channel')),
        default='STRAIGHT')

    ShadelessMats = BoolProperty(name="Shadeless Material",
        description="Set Materials as Shadeless",
        default=True)

    OpacityMode = EnumProperty(name="Opacity Mode",
        description="Layer Opacity management",
        items=(
            ('TEX', 'Texture Alpha Factor', ''),
            ('MAT', 'Material Alpha Value', ''),
            ('COMPO', 'Mixer Node Factor', ''),
            ('BAKE', 'Baked in Image Alpha', '')),
        default='TEX')

    SetCamera = BoolProperty(name="Set Camera",
        description="Create an Ortho Camera matching image resolution",
        default=True)

    SetupCompo = BoolProperty(name="Setup Node Compositing",
        description="Create a compositing node setup (will delete existing nodes)",
        default=False)

    GroupUntagged = BoolProperty(name="Group Untagged",
        description="Layers with no tag go to a single Render Layer",
        default=False)

    LayerOffset = FloatProperty(name="Layer Separation",
        description="Distance between each 3D Layer in the Z axis",
        min=0,
        default=0.50)

    LayerScale = FloatProperty(name="Layer Scale",
        description="Scale pixel resolution by Blender units",
        min=0,
        default=0.01)

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        box.label('3D Layers:', icon='SORTSIZE')
        box.prop(self, 'SetCamera', icon='OUTLINER_DATA_CAMERA')
        box.prop(self, 'OpacityMode', icon='GHOST')
        if self.OpacityMode == 'COMPO' and self.SetupCompo == False:
            box.label('Tip: Enable Node Compositing', icon='INFO')
        box.prop(self, 'AlphaMode', icon='IMAGE_RGB_ALPHA')
        box.prop(self, 'ShadelessMats', icon='SOLID')
        box.prop(self, 'LayerOffset')
        box.prop(self, 'LayerScale')

        box = layout.box()
        box.label('Compositing:', icon='RENDERLAYERS')
        box.prop(self, 'SetupCompo', icon='NODETREE')
        if self.SetupCompo:
            box.prop(self, 'GroupUntagged', icon='IMAGE_ZDEPTH')
            box.prop(self, 'LayerViewers', icon='NODE')
            box.prop(self, 'MixerViewers', icon='NODE')

    def execute(self, context):
        # File Path
        filename = self.filename
        directory = self.directory

        # Settings
        LayerViewers = self.LayerViewers
        MixerViewers = self.MixerViewers
        OpacityMode = self.OpacityMode
        AlphaMode = self.AlphaMode
        ShadelessMats = self.ShadelessMats
        SetCamera = self.SetCamera
        SetupCompo = self.SetupCompo
        GroupUntagged = self.GroupUntagged
        LayerOffset = self.LayerOffset
        LayerScale = self.LayerScale

        Ext = None
        if filename.endswith('.xcf'): Ext = '.xcf'
        elif filename.endswith('.xjt'): Ext = '.xjt'

        # Call Main Function
        if Ext:
            ret = main(self.report, filename, directory, LayerViewers, MixerViewers, LayerOffset,
                       LayerScale, OpacityMode, AlphaMode, ShadelessMats,
                       SetCamera, SetupCompo, GroupUntagged, Ext)
            if not ret:
                return {'CANCELLED'}
        else:
            self.report({'ERROR'},"Selected file wasn't valid, try .xcf or .xjt")
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = bpy.context.window_manager
        wm.fileselect_add(self)

        return {'RUNNING_MODAL'}


# Registering / Unregister
def menu_func(self, context):
    self.layout.operator(GIMPImageToScene.bl_idname, text="GIMP Image to Scene (.xcf, .xjt)", icon='PLUGIN')


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_file_import.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_func)


if __name__ == "__main__":
    register()
