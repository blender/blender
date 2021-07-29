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
from bpy.props import StringProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import levelsOflist



class SverchokViewer(bpy.types.Operator):
    """Sverchok viewer"""
    bl_idname = "node.sverchok_viewer_button"
    bl_label = "Sverchok viewer"
    bl_options = {'REGISTER', 'UNDO'}

    nodename = StringProperty(name='nodename')
    treename = StringProperty(name='treename')

    def execute(self, context):
        inputs = bpy.data.node_groups[self.treename].nodes[self.nodename].inputs

        # vertices socket
        out_verts = 'None \n'
        if inputs['vertices'].is_linked:
            if inputs['vertices'].other.bl_idname == 'VerticesSocket':
                evaverti = inputs['vertices'].sv_get()

                deptl = levelsOflist(evaverti)
                if deptl and deptl > 2:
                    a = self.readFORviewer_sockets_data(evaverti, deptl, len(evaverti))
                elif deptl:
                    a = self.readFORviewer_sockets_data_small(evaverti, deptl, len(evaverti))
                else:
                    a = 'None \n'
                out_verts = a

        # edges/faces socket
        out_edgpol = 'None \n'
        edpotype = '\n\ndata \n'
        if inputs['edg_pol'].is_linked:
            if inputs['edg_pol'].other.bl_idname == 'StringsSocket':
                evaline_str = inputs['edg_pol'].sv_get()

                if evaline_str:
                    edpotype = self.edgDef(evaline_str)

                deptl = levelsOflist(evaline_str)
                if deptl and deptl > 2:
                    b = self.readFORviewer_sockets_data(evaline_str, deptl, len(evaline_str))
                elif deptl:
                    b = self.readFORviewer_sockets_data_small(evaline_str, deptl, len(evaline_str))
                else:
                    b = 'None \n'
                out_edgpol = str(b)

        # matrix socket
        out_matrix = 'None \n'
        if inputs['matrix'].is_linked:
            if inputs['matrix'].other.bl_idname == 'MatrixSocket':
                eva = inputs['matrix'].sv_get()

                deptl = levelsOflist(eva)
                if deptl and deptl > 2:
                    c = self.readFORviewer_sockets_data(eva, deptl, len(eva))
                elif deptl:
                    c = self.readFORviewer_sockets_data_small(eva, deptl, len(eva))
                else:
                    c = 'None \n'
                out_matrix = str(c)

        # object socket
        out_object = 'None \n'
        if inputs['object'].is_linked:
            if inputs['object'].other.bl_idname == 'SvObjectSocket':
                eva = inputs['object'].sv_get()

                deptl = levelsOflist(eva)
                if deptl and deptl > 2:
                    d = self.readFORviewer_sockets_data(eva, deptl, len(eva))
                elif deptl:
                    d = self.readFORviewer_sockets_data_small(eva, deptl, len(eva))
                else:
                    d = 'None \n'
                out_object = str(d)

        self.do_text(out_verts, out_edgpol, out_matrix, edpotype, out_object)
        return {'FINISHED'}

    def makeframe(self, nTree,):
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

    def do_text(self,vertices,edgspols,matrices,edpotype,object):
        nTree = bpy.data.node_groups[self.treename]
        #this part can be than removed from node text viewer:

        if not 'Sverchok_viewer' in bpy.data.texts:
            bpy.data.texts.new('Sverchok_viewer')

        podpis = '\n'*2 \
                + '**************************************************' + '\n' \
                + '                     The End                      '
        for_file = 'node name: ' + self.nodename \
                    + '\n\nvertices: \n' \
                    + vertices \
                    + edpotype \
                    + edgspols \
                    + '\n\nmatrixes: \n' \
                    + matrices \
                    + '\n\nobjects: \n' \
                    + object \
                    + podpis
        bpy.data.texts['Sverchok_viewer'].clear()
        bpy.data.texts['Sverchok_viewer'].write(for_file)
        self.makeframe(nTree)

    def edgDef(self, l):
        t = '\n\ndata: \n'
        if l[0]:
            # checking failed :(
            if isinstance(l[0], str) or isinstance(l[0], int) or isinstance(l[0], float):
                print('type')
                if len(l) > 2:
                    t = '\n\npolygons: \n'
                else:
                    t = '\n\nedges: \n'
            else:
                t = self.edgDef(l[0])
        return t

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

class ViewerNodeTextMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' Viewer Node text MK2 '''
    bl_idname = 'ViewerNodeTextMK2'
    bl_label = 'Viewer text mk2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    autoupdate = BoolProperty(name='update', default=False)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('StringsSocket', 'edg_pol', 'edg_pol')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')
        self.inputs.new('SvObjectSocket', 'object', 'object')

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.scale_y = 4.0
        do_text = row.operator('node.sverchok_viewer_button', text='V I E W')
        do_text.nodename = self.name
        do_text.treename = self.id_data.name
        layout.prop(self, "autoupdate", text="autoupdate")

    def process(self):
        if not self.autoupdate:
            pass
        else:
            bpy.ops.node.sverchok_viewer_button(nodename=self.name, treename=self.id_data.name)

    def update(self):
        pass



def register():
    bpy.utils.register_class(SverchokViewer)
    bpy.utils.register_class(ViewerNodeTextMK2)


def unregister():
    bpy.utils.unregister_class(ViewerNodeTextMK2)
    bpy.utils.unregister_class(SverchokViewer)

if __name__ == '__main__':
    register()
