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
from bpy.props import BoolProperty, StringProperty, FloatVectorProperty
from mathutils import Matrix

from sverchok.node_tree import (
    SverchCustomTreeNode, StringsSocket, VerticesSocket, MatrixSocket)

from sverchok.data_structure import (
    dataCorrect, node_id, Vector_generate, Matrix_generate, updateNode, SvGetSocketAnyType)

from sverchok.ui.viewer_draw import callback_disable, callback_enable


class SvColors(bpy.types.PropertyGroup):
    """ Class for colors CollectionProperty """
    color = FloatVectorProperty(
        name="svcolor", description="sverchok color",
        default=(0.055, 0.312, 0.5), min=0, max=1,
        step=1, precision=3, subtype='COLOR_GAMMA', size=3,
        update=updateNode)


class SvObjBake(bpy.types.Operator):
    """ B A K E   OBJECTS """
    bl_idname = "node.sverchok_mesh_baker"
    bl_label = "Sverchok mesh baker"
    bl_options = {'REGISTER', 'UNDO'}

    idname = StringProperty(name='idname', description='name of parent node',
                            default='')
    idtree = StringProperty(name='idtree', description='name of parent tree',
                            default='')

    def execute(self, context):
        global cache_viewer_baker
        nid = node_id(bpy.data.node_groups[self.idtree].nodes[self.idname])
        if cache_viewer_baker[nid+'m'] and not cache_viewer_baker[nid+'v']:
            return {'CANCELLED'}
        vers = dataCorrect(cache_viewer_baker[nid+'v'])
        edg_pol = dataCorrect(cache_viewer_baker[nid+'ep'])
        if cache_viewer_baker[nid+'m']:
            matrixes = dataCorrect(cache_viewer_baker[nid+'m'])
        else:
            matrixes = []
            for i in range((len(vers))):
                matrixes.append(Matrix())
        self.makeobjects(vers, edg_pol, matrixes)
        return {'FINISHED'}

    def makeobjects(self, vers, edg_pol, mats):
        # inception
        # fht = предохранитель от перебора рёбер и полигонов.
        fht = []
        if len(edg_pol[0][0]) == 2:
            pols = []
            for edgs in edg_pol:
                maxi = max(max(a) for a in edgs)
                fht.append(maxi)
                #print (maxi)
        elif len(edg_pol[0][0]) > 2:
            edgs = []
            for pols in edg_pol:
                maxi = max(max(a) for a in pols)
                fht.append(maxi)
                #print (maxi)
        #print (fht)
        vertices = Vector_generate(vers)
        matrixes = Matrix_generate(mats)
        #print('mats' + str(matrixes))
        objects = {}
        fhtagn = []
        for u, f in enumerate(fht):
            fhtagn.append(min(len(vertices[u]), fht[u]))
        #lenmesh = len(vertices) - 1
        #print ('запекание вершин ', vertices, " матрицы запекашка ", matrixes, " полиглоты ", edg_pol)
        #print (matrixes)
        for i, m in enumerate(matrixes):
            k = i
            lenver = len(vertices) - 1
            if i > lenver:
                v = vertices[-1]
                k = lenver
            else:
                v = vertices[k]
            #print (fhtagn, len(v)-1)
            if (len(v)-1) < fhtagn[k]:
                continue
            # возможно такая сложность не нужна, но пусть лежит тут. Удалять лишние точки не обязательно.
            elif fhtagn[k] < (len(v)-1):
                nonneed = (len(v)-1) - fhtagn[k]
                for q in range(nonneed):
                    v.pop((fhtagn[k]+1))
                #print (fhtagn[k], (len(v)-1))

            e = edg_pol[k] if edgs else []
            p = edg_pol[k] if pols else []

            objects[str(i)] = self.makemesh(i, v, e, p, m)
        for ite in objects.values():
            me = ite[1]
            ob = ite[0]
            calcedg = True
            if edgs:
                calcedg = False
            me.update(calc_edges=calcedg)
            bpy.context.scene.objects.link(ob)

    def makemesh(self, i, v, e, p, m):
        name = 'Sv_' + str(i)
        me = bpy.data.meshes.new(name)
        me.from_pydata(v, e, p)
        ob = bpy.data.objects.new(name, me)
        ob.matrix_world = m
        ob.show_name = False
        ob.hide_select = False
        #print ([ob,me])
        #print (ob.name + ' baked')
        return [ob, me]

cache_viewer_baker = {}

class ViewerNode(bpy.types.Node, SverchCustomTreeNode):
    ''' ViewerNode '''
    bl_idname = 'ViewerNode'
    bl_label = 'Viewer Draw'
    bl_icon = 'OUTLINER_OB_EMPTY'

    # node id
    n_id = StringProperty(default='', options={'SKIP_SAVE'})

    Vertex_show = BoolProperty(name='Vertices', description='Show or not vertices',
                    default=True,
                    update=updateNode)
    activate = BoolProperty(name='Show', description='Activate node?',
                    default=True,
                    update=updateNode)
    transparant = BoolProperty(name='Transparant', description='transparant polygons?',
                    default=False,
                    update=updateNode)
    shading = BoolProperty(name='Shading', description='shade the object or index representation?',
                    default=False,
                    update=updateNode)

    bakebuttonshow = BoolProperty(
        name='bakebuttonshow', description='show bake button on node',
        default=True,
        update=updateNode)


    color_view = SvColors.color

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('StringsSocket', 'edg_pol', 'edg_pol')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "Vertex_show", text="Verts")
        row.prop(self, "activate", text="Show")
        if self.bakebuttonshow:
            row = layout.row()
            row.scale_y = 4.0
            opera = row.operator('node.sverchok_mesh_baker', text='B A K E')
            opera.idname = self.name
            opera.idtree = self.id_data.name
        row = layout.row(align=True)
        row.prop(self, "transparant", text="Transp")
        row.prop(self, "shading", text="Shade")
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "color_view", text=" ")
        #row.template_color_picker(self, 'color_view', value_slider=True)

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, 'bakebuttonshow')

    # reset n_id on duplicate (shift-d)
    def copy(self, node):
        self.n_id = ''

    def process(self):
        global cache_viewer_baker
        # node id, used as ref
        n_id = node_id(self)
        if 'matrix' not in self.inputs:
            return

        cache_viewer_baker[n_id+'v'] = []
        cache_viewer_baker[n_id+'ep'] = []
        cache_viewer_baker[n_id+'m'] = []

        if not self.id_data.sv_show:
            callback_disable(n_id)
            return

        if self.activate and (self.inputs['vertices'].links or self.inputs['matrix'].links):
            callback_disable(n_id)

            if self.inputs['vertices'].links and \
                type(self.inputs['vertices'].links[0].from_socket) == VerticesSocket:

                propv = SvGetSocketAnyType(self, self.inputs['vertices'])
                cache_viewer_baker[n_id+'v'] = dataCorrect(propv)
            else:
                cache_viewer_baker[n_id+'v'] = []

            if self.inputs['edg_pol'].links and \
                type(self.inputs['edg_pol'].links[0].from_socket) == StringsSocket:
                prope = SvGetSocketAnyType(self, self.inputs['edg_pol'])
                cache_viewer_baker[n_id+'ep'] = dataCorrect(prope)
                #print (prope)
            else:
                cache_viewer_baker[n_id+'ep'] = []

            if self.inputs['matrix'].links and \
                type(self.inputs['matrix'].links[0].from_socket) == MatrixSocket:
                propm = SvGetSocketAnyType(self, self.inputs['matrix'])
                cache_viewer_baker[n_id+'m'] = dataCorrect(propm)
            else:
                cache_viewer_baker[n_id+'m'] = []

        else:
            callback_disable(n_id)

        if cache_viewer_baker[n_id+'v'] or cache_viewer_baker[n_id+'m']:
            callback_enable(n_id, cache_viewer_baker[n_id+'v'], cache_viewer_baker[n_id+'ep'], \
                cache_viewer_baker[n_id+'m'], self.Vertex_show, self.color_view.copy(), self.transparant, self.shading)

            self.use_custom_color = True
            self.color = (1, 0.3, 0)
        else:
            self.use_custom_color = True
            self.color = (0.1, 0.05, 0)
            #print ('отражения вершин ',len(cache_viewer_baker['v']), " рёбёры ", len(cache_viewer_baker['ep']), "матрицы",len(cache_viewer_baker['m']))
        if not self.inputs['vertices'].links and not self.inputs['matrix'].links:
            callback_disable(n_id)

    def update_socket(self, context):
        self.update()

    def free(self):
        global cache_viewer_baker
        n_id = node_id(self)
        callback_disable(n_id)
        cache_viewer_baker.pop(n_id+'v', None)
        cache_viewer_baker.pop(n_id+'ep', None)
        cache_viewer_baker.pop(n_id+'m', None)

    def bake(self):
        if self.activate and self.inputs['edg_pol'].is_linked:
            bake = bpy.ops.node.sverchok_mesh_baker
            bake(idname=self.name, idtree=self.id_data.name)

def register():
    bpy.utils.register_class(SvColors)
    bpy.utils.register_class(ViewerNode)
    bpy.utils.register_class(SvObjBake)


def unregister():
    bpy.utils.unregister_class(SvObjBake)
    bpy.utils.unregister_class(ViewerNode)
    bpy.utils.unregister_class(SvColors)

# if __name__ == '__main__':
#     register()
