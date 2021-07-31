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
from bpy.props import StringProperty, BoolProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import levelsOflist, changable_sockets, multi_socket, updateNode



class SverchokViewerMK1(bpy.types.Operator):
    """Sverchok viewerMK1"""
    bl_idname = "node.sverchok_viewer_buttonmk1"
    bl_label = "Sverchok viewer.mk1"
    bl_options = {'REGISTER', 'UNDO'}

    nodename = StringProperty(name='nodename')
    treename = StringProperty(name='treename')
    lines = IntProperty(name='lines', description='lines count for operate on',default=1000)

    def execute(self, context):
        node = bpy.data.node_groups[self.treename].nodes[self.nodename]
        inputs = node.inputs
        self.prep_text(context,node,inputs)
        return {'FINISHED'}

    def prep_text(self,context,node,inputs):
        'main preparation function for text'
        # outputs
        outs  = ''
        for insert in inputs:
            if insert.is_linked:
                label = insert.other.node.label
                if label:
                    label = '; node ' + label.upper()
                name = insert.name.upper()
                # vertices socket
                if insert.other.bl_idname == 'VerticesSocket':
                    itype = '\n\nSocket ' + name + label + '; type VERTICES: \n'

                # edges/faces socket
                elif insert.other.bl_idname == 'StringsSocket':
                    itype = '\n\nSocket ' + name + label + '; type EDGES/POLYGONS/OTHERS: \n'

                # matrix socket
                elif insert.other.bl_idname == 'MatrixSocket':
                    itype = '\n\nSocket ' + name + label + '; type MATRICES: \n'

                # object socket
                elif insert.other.bl_idname == 'SvObjectSocket':
                    itype = '\n\nSocket ' + name + label + '; type OBJECTS: \n'
                # else
                else:
                    itype = '\n\nSocket ' + name + label + '; type DATA: \n'

                eva = insert.sv_get()
                deptl = levelsOflist(eva)
                if deptl and deptl > 2:
                    a = self.readFORviewer_sockets_data(eva, deptl, len(eva))
                elif deptl:
                    a = self.readFORviewer_sockets_data_small(eva, deptl, len(eva))
                else:
                    a = 'None'
                outs += itype+str(a)+'\n'
        self.do_text(outs,node)

    def makeframe(self, nTree):
        '''
        Making frame to show text to user. appears in left corner
        Todo - make more organized layout with button making
        lines in up and between Frame and nodes and text of user and layout name
        '''
        # labls = [n.label for n in nTree.nodes]
        if any('Sverchok_viewer' == n.label for n in nTree.nodes):
            return
        else:
            a = nTree.nodes.new('NodeFrame')
            a.width = 800
            a.height = 1500
            locx = [n.location[0] for n in nTree.nodes]
            locy = [n.location[1] for n in nTree.nodes]
            mx, my = min(locx), max(locy)
            a.location[0] = mx - a.width - 10
            a.location[1] = my
            a.text = bpy.data.texts['Sverchok_viewer']
            a.label = 'Sverchok_viewer'
            a.shrink = False
            a.use_custom_color = True
            # this trick allows us to negative color, so user accept it as grey!!!
            color = [1 - i for i in bpy.context.user_preferences.themes['Default'].node_editor.space.back[:]]
            a.color[:] = color

    def do_text(self,outs,node):
        nTree = bpy.data.node_groups[self.treename]
        #this part can be than removed from node text viewer:

        if not 'Sverchok_viewer' in bpy.data.texts:
            bpy.data.texts.new('Sverchok_viewer')

        footer = '\n'*2 \
                + '**************************************************' + '\n' \
                + '                     The End                      '
        for_file = 'node name: ' + self.nodename \
                    + outs \
                    + footer
        bpy.data.texts['Sverchok_viewer'].clear()
        bpy.data.texts['Sverchok_viewer'].write(for_file)
        if node.frame:
            self.makeframe(nTree)

    def readFORviewer_sockets_data(self, data, dept, le):
        cache = ''
        output = ''
        deptl = dept - 1
        if le:
            cache += ('(' + str(le) + ') object(s)')
            del(le)
        if deptl > 1:
            for i, object in enumerate(data):
                cache += ('\n' + '=' + str(i) + '=   (' + str(len(object)) + ')')
                cache += str(self.readFORviewer_sockets_data(object, deptl, False))
        else:
            for k, val in enumerate(data):
                output += ('\n' + str(val))
                if k >= self.lines-1: break
        return cache + output

    def readFORviewer_sockets_data_small(self, data, dept, le):
        cache = ''
        output = ''
        deptl = dept - 1
        if le:
            cache += ('(' + str(le) + ') object(s)')
            del(le)
        if deptl > 0:
            for i, object in enumerate(data):
                cache += ('\n' + '=' + str(i) + '=   (' + str(len(object)) + ')')
                cache += str(self.readFORviewer_sockets_data_small(object, deptl, False))
        else:
            for k, val in enumerate(data):
                output += ('\n' + str(val))
        return cache + output


class ViewerNodeTextMK3(bpy.types.Node, SverchCustomTreeNode):
    """
    Triggers: Viewer Node text MK3
    Tooltip: Inspecting data from sockets in terms 
    of levels and structure by types
    multisocket lets you insert many outputs
    """
    bl_idname = 'ViewerNodeTextMK3'
    bl_label = 'Viewer text mk3'
    bl_icon = 'OUTLINER_OB_EMPTY'

    autoupdate = BoolProperty(name='update', default=False)
    frame = BoolProperty(name='frame', default=True)
    lines = IntProperty(name='lines', description='lines count for operate on', default=1000, \
                        min=1, max=2000)

    # multi sockets veriables
    newsock = BoolProperty(name='newsock', default=False)
    base_name = 'data'
    multi_socket_type = 'StringsSocket'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'data0', 'data0')

    def draw_buttons_ext(self, context, layout):
        row = layout.row()
        row.prop(self,'lines',text='lines')

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.scale_y = 4.0
        do_text = row.operator('node.sverchok_viewer_buttonmk1', text='V I E W')
        do_text.nodename = self.name
        do_text.treename = self.id_data.name
        do_text.lines = self.lines
        
        col = layout.column(align=True)
        col.prop(self, "autoupdate", text="autoupdate")
        col.prop(self, "frame", text="frame")
        

    def update(self):
        # inputs
        multi_socket(self, min=1)

        if 'data' in self.inputs and len(self.inputs['data'].links) > 0:
            inputsocketname = 'data'
            outputsocketname = ['data']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if not self.autoupdate:
            pass
        else:
            bpy.ops.node.sverchok_viewer_buttonmk1(nodename=self.name, treename=self.id_data.name, lines=self.lines)



def register():
    bpy.utils.register_class(SverchokViewerMK1)
    bpy.utils.register_class(ViewerNodeTextMK3)


def unregister():
    bpy.utils.unregister_class(ViewerNodeTextMK3)
    bpy.utils.unregister_class(SverchokViewerMK1)

if __name__ == '__main__':
    register()
