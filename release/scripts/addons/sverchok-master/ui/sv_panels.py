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
from bpy.props import StringProperty, BoolProperty, FloatProperty


import sverchok
from sverchok.utils.sv_update_utils import version_and_sha
from sverchok.core.update_system import process_from_nodes
from sverchok.utils import profile

objects_nodes_set = {'ObjectsNode', 'ObjectsNodeMK2', 'SvObjectsNodeMK3'}



class SverchokUpdateObjectIn(bpy.types.Operator):
    """Sverchok update all object in"""
    bl_idname = "object.sverchok_update_object_in"
    bl_label = "Sverchok update all"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj_nodes = []
        for ng in bpy.data.node_groups:
            if ng.bl_idname == 'SverchCustomTreeType':
                if ng.sv_process:
                    nodes = []
                    for n in ng.nodes:
                        if n.bl_idname in objects_nodes_set:
                            nodes.append(n)
                    if nodes:
                        obj_nodes.append(nodes)

        for n in obj_nodes:
            process_from_nodes(n)
        return {'FINISHED'}



class Sv3DViewObjInUpdater(bpy.types.Operator, object):

    """Operator which runs its self from a timer"""
    bl_idname = "wm.sv_obj_modal_update"
    bl_label = "start n stop obj updating"

    _timer = None
    mode = StringProperty(default='')
    node_name = StringProperty(default='')
    node_group = StringProperty(default='')
    speed = FloatProperty(default=1 / 13)

    def modal(self, context, event):

        if not context.scene.SvShowIn3D_active:
            self.cancel(context)
            return {'FINISHED'}

        if not (event.type == 'TIMER'):
            return {'PASS_THROUGH'}

        obj_nodes = []
        for ng in bpy.data.node_groups:
            if ng.bl_idname == 'SverchCustomTreeType':
                if ng.sv_process:
                    nodes = []
                    for n in ng.nodes:
                        if n.bl_idname in objects_nodes_set:
                            nodes.append(n)
                    if nodes:
                        obj_nodes.append(nodes)

        ''' reaches here only if event is TIMER and self.active '''
        for n in obj_nodes:
            # print('calling process on:', n.name, n.id_data)
            process_from_nodes(n)

        return {'PASS_THROUGH'}

    def event_dispatcher(self, context, type_op):
        if type_op == 'start':
            context.scene.SvShowIn3D_active = True

            # rate can only be set in event_timer_add (I think...)
            # self.speed = 1 / context.node.updateRate

            wm = context.window_manager
            self._timer = wm.event_timer_add(self.speed, context.window)
            wm.modal_handler_add(self)

        if type_op == 'end':
            context.scene.SvShowIn3D_active = False

    def execute(self, context):
        # n  = context.node
        # self.node_name = context.node.name
        # self.node_group = context.node.id_data.name

        self.event_dispatcher(context, self.mode)
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        wm = context.window_manager
        wm.event_timer_remove(self._timer)


class Sv3DPanel(bpy.types.Panel):
    ''' Panel to manipuplate parameters in Sverchok layouts '''

    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_label = "Sverchok " + version_and_sha
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = 'SV'

    def draw(self, context):
        layout = self.layout
        little_width = 0.12

        addon = context.user_preferences.addons.get(sverchok.__name__)
        if addon.preferences.enable_live_objin:

            # Live Update Modal trigger.
            row = layout.row()
            OP = 'wm.sv_obj_modal_update'
            if context.scene.SvShowIn3D_active:
                row.operator(OP, text='Stop live update', icon='CANCEL').mode = 'end'
            else:
                row.operator(OP, text='Start live update', icon='EDIT').mode = 'start'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.scale_y = 2.0
        row.operator('node.sv_scan_propertyes', text='Scan for props')
        row.operator("node.sverchok_update_all", text="Update all")
        row = col.row(align=True)
        row.prop(context.scene, 'sv_do_clear', text='hard clean', toggle=True)
        delley = row.operator(
            'node.sv_delete_nodelayouts',
            text='Clean layouts').do_clear = context.scene.sv_do_clear

        for tree in bpy.data.node_groups:
            if tree.bl_idname == 'SverchCustomTreeType':
                box = layout.box()
                col = box.column(align=True)
                row = col.row(align=True)

                split = row.column(align=True)
                split.scale_x = little_width
                icoco = 'DOWNARROW_HLT' if tree.SvShowIn3D else 'RIGHTARROW'
                split.prop(tree, 'SvShowIn3D', icon=icoco, emboss=False, text=' ')

                split = row.column(align=True)
                split.label(text=tree.name)

                # bakery
                split = row.column(align=True)
                split.scale_x = little_width
                baka = split.operator('node.sverchok_bake_all', text='B')
                baka.node_tree_name = tree.name

                # eye
                split = row.column(align=True)
                split.scale_x = little_width
                if tree.sv_show:
                    split.prop(tree, 'sv_show', icon='RESTRICT_VIEW_OFF', text=' ')
                else:
                    split.prop(tree, 'sv_show', icon='RESTRICT_VIEW_ON', text=' ')
                split = row.column(align=True)
                split.scale_x = little_width
                # if tree.sv_animate:
                split.prop(tree, 'sv_animate', icon='ANIM', text=' ')
                # else:
                #    split.prop(tree, 'sv_animate', icon='LOCKED', text=' ')

                split = row.column(align=True)
                split.scale_x = little_width
                split.prop(tree, "sv_process", toggle=True, text="P")

                split = row.column(align=True)
                split.scale_x = little_width
                split.prop(tree, 'use_fake_user', toggle=True, text='F')

                # variables
                if tree.SvShowIn3D:
                    for item in tree.Sv3DProps:
                        no = item.node_name
                        ver = item.prop_name
                        node = tree.nodes[no]

                        if node.label:
                            tex = node.label
                        else:
                            tex = no

                        if node.bl_idname == "ObjectsNodeMK2":
                            row = col.row(align=True)
                            row.label(text=node.label if node.label else no)
                            colo = row.row(align=True)
                            colo.scale_x = little_width * 5
                            op = colo.operator("node.sverchok_object_insertion", text="Get")
                            op.node_name = node.name
                            op.tree_name = tree.name
                            op.grup_name = node.groupname
                            op.sort = node.sort
                        elif node.bl_idname == 'SvObjectsNodeMK3':
                            node.draw_sv3dpanel_ob3(col, little_width)

                        elif node.bl_idname in {"IntegerNode", "FloatNode", "SvNumberNode"}:
                            row = col.row(align=True)
                            row.prop(node, ver, text=tex)
                            colo = row.row(align=True)
                            colo.scale_x = little_width * 2.5
                            
                            if node.bl_idname == 'SvNumberNode':
                                min_name = node.selected_mode + '_min'
                                max_name = node.selected_mode + '_max'
                            else:
                                min_name = 'minim'
                                max_name = 'maxim'
                            
                            colo.prop(node, min_name, text='', slider=True, emboss=False)
                            colo.prop(node, max_name, text='', slider=True, emboss=False)

                        elif node.bl_idname in {"SvListInputNode"}:
                            if node.mode == 'vector':
                                colum_list = col.column(align=True)
                                for i in range(self.v_int):
                                    row = colum_list.row(align=True)
                                    for j in range(3):
                                        row.prop(node, 'vector_list', index=i*3+j, text='XYZ'[j]+tex)
                            else:
                                colum_list = col.column(align=True)
                                for i in range(node.int_):
                                    row = colum_list.row(align=True)
                                    row.prop(node, node.mode, index=i, text=str(i)+tex)
                                    row.scale_x = little_width * 2.5



class SverchokToolsMenu(bpy.types.Panel):
    bl_idname = "Sverchok_tools_menu"
    bl_label = "SV " + version_and_sha
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = 'Sverchok'
    use_pin = True

    @classmethod
    def poll(cls, context):
        try:
            return context.space_data.edit_tree.bl_idname == 'SverchCustomTreeType'
        except:
            return False

    def draw(self, context):

        ng_name = context.space_data.node_tree.name
        layout = self.layout
        # layout.scale_y=1.1
        layout.active = True

        addon = context.user_preferences.addons.get(sverchok.__name__)
        if addon.preferences.profile_mode != "NONE":
            profile_col = layout.column(align=True)
            if profile.is_currently_enabled:
                profile_col.operator("node.sverchok_profile_toggle", text="Stop profiling", icon="CANCEL")
            else:
                profile_col.operator("node.sverchok_profile_toggle", text="Start profiling", icon="TIME")
            if profile.have_gathered_stats():
                row = profile_col.row(align=True)
                row.operator("node.sverchok_profile_dump", text="Dump data", icon="TEXT")
                row.operator("node.sverchok_profile_save", text="Save data", icon="SAVE_AS")
                profile_col.operator("node.sverchok_profile_reset", text="Reset data", icon="X")

        row = layout.row(align=True)
        col = row.column(align=True)
        col.scale_y = 3.0
        col.scale_x = 0.5
        u = "Update all"
        col.operator("node.sverchok_update_all", text=u)
        col = row.column(align=True)
        col.scale_y = 3.0
        u = "Update {0}".format(ng_name)
        op = col.operator("node.sverchok_update_current", text=u)
        op.node_group = ng_name
        box = layout.box()
        little_width = 0.12
        col = box.column(align=True)
        row = col.row(align=True)
        row.label(text='Layout')
        col0 = row.column(align=True)
        col0.scale_x = little_width
        col0.label(text='B')
        col1 = row.column(align=True)
        col1.scale_x = little_width
        col1.label(icon='RESTRICT_VIEW_OFF', text=' ')
        col2 = row.column(align=True)
        col2.scale_x = little_width
        col2.label(icon='ANIM', text=' ')

        col3 = row.column(align=True)
        col3.scale_x = little_width
        col3.label(text='P')

        col3 = row.column(align=True)
        col3.scale_x = little_width
        col3.label(text='F')

        for name, tree in bpy.data.node_groups.items():
            if tree.bl_idname == 'SverchCustomTreeType':

                row = col.row(align=True)
                # tree name
                if name == ng_name:
                    row.label(text=name)
                else:
                    row.operator('node.sv_switch_layout', text=name).layout_name = name

                # bakery
                split = row.column(align=True)
                split.scale_x = little_width
                baka = split.operator('node.sverchok_bake_all', text='B')
                baka.node_tree_name = name

                # eye
                split = row.column(align=True)
                split.scale_x = little_width
                view_icon = 'RESTRICT_VIEW_' + ('OFF' if tree.sv_show else 'ON')
                split.prop(tree, 'sv_show', icon=view_icon, text=' ')

                split = row.column(align=True)
                split.scale_x = little_width
                # animate_icon = ('UN' if tree.sv_animate else '') + 'LOCKED'
                split.prop(tree, 'sv_animate', icon='ANIM', text=' ')

                split = row.column(align=True)
                split.scale_x = little_width
                split.prop(tree, "sv_process", toggle=True, text="P")

                split = row.column(align=True)
                split.scale_x = little_width
                split.prop(tree, 'use_fake_user', toggle=True, text='F')

        if context.scene.sv_new_version:
            row = layout.row()
            row.alert = True
            row.operator(
                "node.sverchok_update_addon", text='Upgrade Sverchok addon')
        else:
            sha_update = "node.sverchok_check_for_upgrades_wsha"
            layout.row().operator(sha_update, text='Check for updates')

        layout.row().operator('node.sv_show_latest_commits')


sv_tools_classes = [
    Sv3DViewObjInUpdater,
    SverchokToolsMenu,
    Sv3DPanel,
    SverchokUpdateObjectIn
]


def register():
    bpy.types.NodeTree.SvShowIn3D = BoolProperty(
        name='show in panel',
        default=True,
        description='Show properties in 3d panel or not')

    bpy.types.Scene.SvShowIn3D_active = BoolProperty(
        name='update from 3dview',
        default=False,
        description='Allows updates directly to object-in nodes from 3d panel')

    for class_name in sv_tools_classes:
        bpy.utils.register_class(class_name)



def unregister():
    for class_name in reversed(sv_tools_classes):
        bpy.utils.unregister_class(class_name)

    del bpy.types.NodeTree.SvShowIn3D
    del bpy.types.Scene.SvShowIn3D_active

if __name__ == '__main__':
    register()
