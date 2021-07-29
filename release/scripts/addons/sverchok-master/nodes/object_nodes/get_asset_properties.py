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
from bpy.props import EnumProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, enum_item as e)

# class SvGenericCallbackWithParams() mixin  <- refresh


def frame_from_available(idx, layer):
    keys = {}
    for frame in layer.frames:
        keys[frame.frame_number] = frame.strokes
    return keys

def frame_from_available2(current_frame, layer):
    scene = bpy.context.scene
    rng = [scene.frame_start, scene.frame_end]
    inp = [frame.frame_number for frame in layer.frames]
    inp_to_index = {val: idx for idx, val in enumerate(inp)}

    # this can be cached if inp and rng unchanged. there are several redundancies here
    # but i don't care

    remaps = []  # you fill this
    last_valid = None
    for i in range(rng[0], rng[1]+1):
        if i < inp[0]:
            last_valid = inp[0]
        elif i in set(inp):
            last_valid = i
        elif i >= inp[-1]:
            last_valid = inp[-1]
        else:
            for j in range(len(inp)-1):
                begin, end = inp[j], inp[j+1]
                if begin < i < end:
                    last_valid = begin
                    
        remaps.append(last_valid)

    mdict = {idx: remaps[idx-1] for idx in range(1, len(remaps)+1)}
    tval = mdict.get(current_frame, 0)
    return inp_to_index.get(tval, 0)


class SvGetAssetProperties(bpy.types.Node, SverchCustomTreeNode):
    ''' Get Asset Props '''
    bl_idname = 'SvGetAssetProperties'
    bl_label = 'Object ID Selector'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def pre_updateNode(self, context):
        ''' must rebuild for each update'''
        self.type_collection_name.clear()
        for o in bpy.data.objects:
            if o.type == self.Type:
                self.type_collection_name.add().name = o.name

        # updateNode(self, context)
        self.process()

    def frame_updateNode(self, context):
        ''' must rebuild for each update'''
        self.frame_collection_name.clear()
        gp_layer = bpy.data.grease_pencil[self.gp_name].layers[self.gp_layer]
        for idx, f in enumerate(gp_layer.frames):
            self.frame_collection_name.add().name = str(idx) + ' | ' + str(f.frame_number)

        # updateNode(self, context)
        if self.gp_selected_frame_mode == 'active frame':
            if len(self.inputs) == 0:
                self.inputs.new("StringsSocket", 'frame#')
        else:
            if len(self.inputs) > 0:
                self.inputs.remove(self.inputs[-1])

        self.process()


    type_collection_name = bpy.props.CollectionProperty(type=bpy.types.PropertyGroup)
    frame_collection_name = bpy.props.CollectionProperty(type=bpy.types.PropertyGroup)

    M = ['actions', 'brushes', 'filepath', 'grease_pencil', 'groups',
         'images', 'libraries', 'linestyles', 'masks', 'materials',
         'movieclips', 'node_groups', 'particles', 'scenes', 'screens', 'shape_keys',
         'sounds', 'speakers', 'texts', 'textures', 'worlds', 'objects']
    T = ['MESH', 'CURVE', 'SURFACE', 'META', 'FONT', 'ARMATURE',
         'LATTICE', 'EMPTY', 'CAMERA', 'LAMP', 'SPEAKER']

    Mode = EnumProperty(name="getmodes", default="objects", items=e(M), update=updateNode)
    Type = EnumProperty(name="getmodes", default="MESH", items=e(T), update=pre_updateNode)
    text_name = bpy.props.StringProperty(update=updateNode)
    object_name = bpy.props.StringProperty(update=updateNode)
    image_name = bpy.props.StringProperty(update=updateNode)
    pass_pixels = bpy.props.BoolProperty(update=updateNode)

    # GP props
    gp_name = bpy.props.StringProperty(update=updateNode)
    gp_layer = bpy.props.StringProperty(update=updateNode)
    gp_frame_current = bpy.props.BoolProperty(default=True, update=updateNode)
    gp_frame_override = bpy.props.IntProperty(default=1, update=updateNode)
    gp_stroke_idx = bpy.props.IntProperty(update=updateNode)
    gp_frame_mode_options = [(k, k, '', i) for i, k in enumerate(["pick frame", "active frame"])]
    gp_selected_frame_mode = bpy.props.EnumProperty(
        items=gp_frame_mode_options, description="offers choice between current frame or available frames",
        default="pick frame", update=frame_updateNode
    )
    gp_frame_pick = bpy.props.StringProperty(update=frame_updateNode)
    gp_pass_points = bpy.props.BoolProperty(default=True, update=updateNode)

    def draw_gp_options(self, context, layout):
        # -- points  [p.co for p in points]
        # -- color   
        #     -- color.color (stroke_color)
        #     -- color.fill_color
        #     -- color.file_alpha
        # -- line_width
        # -- draw_cyclic
        #  --- / triangles (only set is useful...)
        
        layout.prop_search(self, 'gp_name', bpy.data, 'grease_pencil', text='name')
        if not self.gp_name:
            return
        
        layout.prop_search(self, 'gp_layer', bpy.data.grease_pencil[self.gp_name], 'layers', text='layer')
        if not self.gp_layer:
            return

        layout.prop(self, 'gp_selected_frame_mode', expand=True)
        gp_layer = bpy.data.grease_pencil[self.gp_name].layers[self.gp_layer]
        frame_data = None
        if self.gp_selected_frame_mode == 'active frame':
            frame_data = gp_layer.active_frame
        else:
            # maybe display uilist with frame_index and frame_nmber.
            layout.prop_search(self, 'gp_frame_pick', self, 'frame_collection_name')
        layout.prop(self, 'gp_pass_points', text='pass points')


    def draw_buttons(self, context, layout):
        # layout.operator('node.'   ,text='refresh from scene')

        layout.row().prop(self, "Mode", text="data")

        if self.Mode == 'objects':
            layout.prop(self, "Type", "type")
            layout.prop_search(self, 'object_name', self, 'type_collection_name', text='name', icon='OBJECT_DATA')
        elif self.Mode == 'texts':
            layout.prop_search(self, 'text_name', bpy.data, 'texts', text='name')
        elif self.Mode == 'images':
            layout.prop_search(self, 'image_name', bpy.data, 'images', text='name')
            if self.image_name:
                layout.prop(self, 'pass_pixels', text='pixels')
                # size ?  new socket outputting [w/h]
        elif self.Mode == 'grease_pencil':
            self.draw_gp_options(context, layout)


    def sv_init(self, context):
        self.outputs.new('StringsSocket', "Objects")
        self.width = 210
        self.Type = 'MESH'  # helps init the custom object prop_search


    def process(self):
        output_socket = self.outputs['Objects']
        if not output_socket.is_linked:
            return

        data_list = getattr(bpy.data, self.Mode)

        if self.Mode == 'objects':
            if self.object_name:
                output_socket.sv_set([data_list[self.object_name]])
            else:
                output_socket.sv_set([i for i in data_list if i.type == self.Type])
        
        elif self.Mode == 'texts':
            if self.text_name:
                output_socket.sv_set([[data_list[self.text_name].as_string()]])
            else:
                output_socket.sv_set(data_list[:])

        elif self.Mode == 'images':
            if self.image_name:
                img = data_list[self.image_name]
                if self.pass_pixels:
                    output_socket.sv_set([[img.pixels[:]]])
                else:
                    output_socket.sv_set([img])
            else:
                output_socket.sv_set(data_list[:])

        elif self.Mode == 'grease_pencil':
            # candidate for refactor
            if self.gp_name and self.gp_layer:
                GP_and_layer = data_list[self.gp_name].layers[self.gp_layer]
                if self.gp_selected_frame_mode == 'active frame':
                    if len(self.inputs) > 0 and self.inputs[0].is_linked:

                        frame_number = self.inputs[0].sv_get()[0][0]
                        key = frame_from_available2(frame_number, GP_and_layer)
                        strokes = GP_and_layer.frames[key].strokes
                    else:
                        strokes = GP_and_layer.active_frame.strokes

                    if not strokes:
                        return

                    if self.gp_pass_points:
                        output_socket.sv_set([[p.co[:] for p in s.points] for s in strokes])
                    else:
                        output_socket.sv_set(strokes)
                else:
                    if self.gp_frame_pick:
                        idx_from_frame_pick = int(self.gp_frame_pick.split(' | ')[0])
                        frame_data = GP_and_layer.frames[idx_from_frame_pick]
                        if frame_data:
                            if self.gp_pass_points:
                                output_socket.sv_set([[p.co[:] for p in s.points] for s in frame_data.strokes])
                            else:
                                output_socket.sv_set(strokes)

        else:
            output_socket.sv_set(data_list[:])



def register():
    bpy.utils.register_class(SvGetAssetProperties)


def unregister():
    bpy.utils.unregister_class(SvGetAssetProperties)
