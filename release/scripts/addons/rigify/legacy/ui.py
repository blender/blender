#====================== BEGIN GPL LICENSE BLOCK ======================
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
#======================= END GPL LICENSE BLOCK ========================

# <pep8 compliant>

import bpy
from bpy.props import StringProperty

from .utils import get_rig_type, MetarigError
from .utils import write_metarig, write_widget
from . import rig_lists
from . import generate


class DATA_PT_rigify_buttons(bpy.types.Panel):
    bl_label = "Rigify Buttons"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    #bl_options = {'DEFAULT_OPEN'}

    @classmethod
    def poll(cls, context):
        if not context.armature:
            return False
        #obj = context.object
        #if obj:
        #    return (obj.mode in {'POSE', 'OBJECT', 'EDIT'})
        #return False
        return True

    def draw(self, context):
        C = context
        layout = self.layout
        obj = context.object
        id_store = C.window_manager

        if obj.mode in {'POSE', 'OBJECT'}:
            layout.operator("pose.rigify_generate", text="Generate")
        elif obj.mode == 'EDIT':
            # Build types list
            collection_name = str(id_store.rigify_collection).replace(" ", "")

            for i in range(0, len(id_store.rigify_types)):
                id_store.rigify_types.remove(0)

            for r in rig_lists.rig_list:
                # collection = r.split('.')[0]  # UNUSED
                if collection_name == "All":
                    a = id_store.rigify_types.add()
                    a.name = r
                elif r.startswith(collection_name + '.'):
                    a = id_store.rigify_types.add()
                    a.name = r
                elif (collection_name == "None") and ("." not in r):
                    a = id_store.rigify_types.add()
                    a.name = r

            ## Rig collection field
            #row = layout.row()
            #row.prop(id_store, 'rigify_collection', text="Category")

            # Rig type list
            row = layout.row()
            row.template_list("UI_UL_list", "rigify_types", id_store, "rigify_types", id_store, 'rigify_active_type')

            props = layout.operator("armature.metarig_sample_add", text="Add sample")
            props.metarig_type = id_store.rigify_types[id_store.rigify_active_type].name


class DATA_PT_rigify_layer_names(bpy.types.Panel):
    bl_label = "Rigify Layer Names"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not context.armature:
            return False
        return True

    def draw(self, context):
        layout = self.layout
        obj = context.object
        arm = obj.data

        # Ensure that the layers exist
        if 0:
            for i in range(1 + len(arm.rigify_layers), 29):
                arm.rigify_layers.add()
        else:
            # Can't add while drawing, just use button
            if len(arm.rigify_layers) < 28:
                layout.operator("pose.rigify_layer_init")
                return

        # UI
        for i, rigify_layer in enumerate(arm.rigify_layers):
            # note: rigify_layer == arm.rigify_layers[i]
            if (i % 16) == 0:
                col = layout.column()
                if i == 0:
                    col.label(text="Top Row:")
                else:
                    col.label(text="Bottom Row:")
            if (i % 8) == 0:
                col = layout.column(align=True)
            row = col.row()
            row.prop(arm, "layers", index=i, text="", toggle=True)
            split = row.split(percentage=0.8)
            split.prop(rigify_layer, "name",  text="Layer %d" % (i + 1))
            split.prop(rigify_layer, "row",   text="")

            #split.prop(rigify_layer, "column", text="")


class BONE_PT_rigify_buttons(bpy.types.Panel):
    bl_label = "Rigify Type"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "bone"
    #bl_options = {'DEFAULT_OPEN'}

    @classmethod
    def poll(cls, context):
        if not context.armature or not context.active_pose_bone:
            return False
        obj = context.object
        if obj:
            return obj.mode == 'POSE'
        return False

    def draw(self, context):
        C = context
        id_store = C.window_manager
        bone = context.active_pose_bone
        collection_name = str(id_store.rigify_collection).replace(" ", "")
        rig_name = str(context.active_pose_bone.rigify_type).replace(" ", "")

        layout = self.layout

        # Build types list
        for i in range(0, len(id_store.rigify_types)):
            id_store.rigify_types.remove(0)

        for r in rig_lists.rig_list:
            # collection = r.split('.')[0]  # UNUSED
            if collection_name == "All":
                a = id_store.rigify_types.add()
                a.name = r
            elif r.startswith(collection_name + '.'):
                a = id_store.rigify_types.add()
                a.name = r
            elif collection_name == "None" and len(r.split('.')) == 1:
                a = id_store.rigify_types.add()
                a.name = r

        # Rig type field
        row = layout.row()
        row.prop_search(bone, "rigify_type", id_store, "rigify_types", text="Rig type:")

        # Rig type parameters / Rig type non-exist alert
        if rig_name != "":
            try:
                rig = get_rig_type(rig_name)
                rig.Rig
            except (ImportError, AttributeError):
                row = layout.row()
                box = row.box()
                box.label(text="ALERT: type \"%s\" does not exist!" % rig_name)
            else:
                try:
                    rig.parameters_ui
                except AttributeError:
                    col = layout.column()
                    col.label(text="No options")
                else:
                    col = layout.column()
                    col.label(text="Options:")
                    box = layout.box()
                    rig.parameters_ui(box, bone.rigify_parameters)


class VIEW3D_PT_tools_rigify_dev(bpy.types.Panel):
    bl_label = "Rigify Dev Tools"
    bl_category = 'Tools'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and context.mode in {'EDIT_ARMATURE','EDIT_MESH'}

    def draw(self, context):
        obj = context.active_object
        if obj is not None:
            if context.mode == 'EDIT_ARMATURE':
                r = self.layout.row()
                r.operator("armature.rigify_encode_metarig", text="Encode Metarig to Python")
                r = self.layout.row()
                r.operator("armature.rigify_encode_metarig_sample", text="Encode Sample to Python")

            if context.mode == 'EDIT_MESH':
                r = self.layout.row()
                r.operator("mesh.rigify_encode_mesh_widget", text="Encode Mesh Widget to Python")

#~ class INFO_MT_armature_metarig_add(bpy.types.Menu):
    #~ bl_idname = "INFO_MT_armature_metarig_add"
    #~ bl_label = "Meta-Rig"

    #~ def draw(self, context):
        #~ import rigify

        #~ layout = self.layout
        #~ layout.operator_context = 'INVOKE_REGION_WIN'

        #~ for submodule_type in rigify.get_submodule_types():
            #~ text = bpy.path.display_name(submodule_type)
            #~ layout.operator("pose.metarig_sample_add", text=text, icon='OUTLINER_OB_ARMATURE').metarig_type = submodule_type


def rigify_report_exception(operator, exception):
    import traceback
    import sys
    import os
    # find the module name where the error happened
    # hint, this is the metarig type!
    exceptionType, exceptionValue, exceptionTraceback = sys.exc_info()
    fn = traceback.extract_tb(exceptionTraceback)[-1][0]
    fn = os.path.basename(fn)
    fn = os.path.splitext(fn)[0]
    message = []
    if fn.startswith("__"):
        message.append("Incorrect armature...")
    else:
        message.append("Incorrect armature for type '%s'" % fn)
    message.append(exception.message)

    message.reverse()  # XXX - stupid! menu's are upside down!

    operator.report({'INFO'}, '\n'.join(message))


class LayerInit(bpy.types.Operator):
    """Initialize armature rigify layers"""

    bl_idname = "pose.rigify_layer_init"
    bl_label = "Add Rigify Layers"
    bl_options = {'UNDO'}

    def execute(self, context):
        obj = context.object
        arm = obj.data
        for i in range(1 + len(arm.rigify_layers), 29):
            arm.rigify_layers.add()
        return {'FINISHED'}


class Generate(bpy.types.Operator):
    """Generates a rig from the active metarig armature"""

    bl_idname = "pose.rigify_generate"
    bl_label = "Rigify Generate Rig"
    bl_options = {'UNDO'}

    def execute(self, context):
        import importlib
        importlib.reload(generate)

        use_global_undo = context.user_preferences.edit.use_global_undo
        context.user_preferences.edit.use_global_undo = False
        try:
            generate.generate_rig(context, context.object)
        except MetarigError as rig_exception:
            rigify_report_exception(self, rig_exception)
        finally:
            context.user_preferences.edit.use_global_undo = use_global_undo

        return {'FINISHED'}


class Sample(bpy.types.Operator):
    """Create a sample metarig to be modified before generating """ \
    """the final rig"""

    bl_idname = "armature.metarig_sample_add"
    bl_label = "Add a sample metarig for a rig type"
    bl_options = {'UNDO'}

    metarig_type = StringProperty(
            name="Type",
            description="Name of the rig type to generate a sample of",
            maxlen=128,
            )

    def execute(self, context):
        if context.mode == 'EDIT_ARMATURE' and self.metarig_type != "":
            use_global_undo = context.user_preferences.edit.use_global_undo
            context.user_preferences.edit.use_global_undo = False
            try:
                rig = get_rig_type(self.metarig_type)
                create_sample = rig.create_sample
            except (ImportError, AttributeError):
                raise Exception("rig type '" + self.metarig_type + "' has no sample.")
            else:
                create_sample(context.active_object)
            finally:
                context.user_preferences.edit.use_global_undo = use_global_undo
                bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


class EncodeMetarig(bpy.types.Operator):
    """ Creates Python code that will generate the selected metarig.
    """
    bl_idname = "armature.rigify_encode_metarig"
    bl_label = "Rigify Encode Metarig"
    bl_options = {'UNDO'}

    @classmethod
    def poll(self, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        name = "metarig.py"

        if name in bpy.data.texts:
            text_block = bpy.data.texts[name]
            text_block.clear()
        else:
            text_block = bpy.data.texts.new(name)

        text = write_metarig(context.active_object, layers=True, func_name="create")
        text_block.write(text)
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


class EncodeMetarigSample(bpy.types.Operator):
    """ Creates Python code that will generate the selected metarig
        as a sample.
    """
    bl_idname = "armature.rigify_encode_metarig_sample"
    bl_label = "Rigify Encode Metarig Sample"
    bl_options = {'UNDO'}

    @classmethod
    def poll(self, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        name = "metarig_sample.py"

        if name in bpy.data.texts:
            text_block = bpy.data.texts[name]
            text_block.clear()
        else:
            text_block = bpy.data.texts.new(name)

        text = write_metarig(context.active_object, layers=False, func_name="create_sample")
        text_block.write(text)
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


class EncodeWidget(bpy.types.Operator):
    """ Creates Python code that will generate the selected metarig.
    """
    bl_idname = "mesh.rigify_encode_mesh_widget"
    bl_label = "Rigify Encode Widget"
    bl_options = {'UNDO'}

    @classmethod
    def poll(self, context):
        return context.mode == 'EDIT_MESH'

    def execute(self, context):
        name = "widget.py"

        if name in bpy.data.texts:
            text_block = bpy.data.texts[name]
            text_block.clear()
        else:
            text_block = bpy.data.texts.new(name)

        text = write_widget(context.active_object)
        text_block.write(text)
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


#menu_func = (lambda self, context: self.layout.menu("INFO_MT_armature_metarig_add", icon='OUTLINER_OB_ARMATURE'))

#from bl_ui import space_info  # ensure the menu is loaded first

def register():
    bpy.utils.register_class(DATA_PT_rigify_layer_names)
    bpy.utils.register_class(DATA_PT_rigify_buttons)
    bpy.utils.register_class(BONE_PT_rigify_buttons)
    bpy.utils.register_class(VIEW3D_PT_tools_rigify_dev)
    bpy.utils.register_class(LayerInit)
    bpy.utils.register_class(Generate)
    bpy.utils.register_class(Sample)
    bpy.utils.register_class(EncodeMetarig)
    bpy.utils.register_class(EncodeMetarigSample)
    bpy.utils.register_class(EncodeWidget)
    #space_info.INFO_MT_armature_add.append(ui.menu_func)


def unregister():
    bpy.utils.unregister_class(DATA_PT_rigify_layer_names)
    bpy.utils.unregister_class(DATA_PT_rigify_buttons)
    bpy.utils.unregister_class(BONE_PT_rigify_buttons)
    bpy.utils.unregister_class(VIEW3D_PT_tools_rigify_dev)
    bpy.utils.unregister_class(LayerInit)
    bpy.utils.unregister_class(Generate)
    bpy.utils.unregister_class(Sample)
    bpy.utils.unregister_class(EncodeMetarig)
    bpy.utils.unregister_class(EncodeMetarigSample)
    bpy.utils.unregister_class(EncodeWidget)
