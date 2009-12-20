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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180


class PoseTemplateSettings(bpy.types.IDPropertyGroup):
    pass


class PoseTemplate(bpy.types.IDPropertyGroup):
    pass

PoseTemplate.StringProperty(attr="name",
                name="Name of the slave",
                description="",
                maxlen=64,
                default="")


PoseTemplateSettings.CollectionProperty(attr="templates", type=PoseTemplate, name="Templates", description="")
PoseTemplateSettings.IntProperty(attr="active_template_index",
                name="Index of the active slave",
                description="",
                default=-1,
                min=-1,
                max=65535)

PoseTemplateSettings.BoolProperty(attr="generate_def_rig",
                name="Create Deform Rig",
                description="Create a copy of the metarig, constrainted by the generated rig",
                default=False)

bpy.types.Scene.PointerProperty(attr="pose_templates", type=PoseTemplateSettings, name="Network Render", description="Network Render Settings")


def metarig_templates():
    import rigify
    return rigify.get_submodule_types()


class DATA_PT_template(bpy.types.Panel):
    bl_label = "Meta-Rig Templates"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_default_closed = True

    templates = []

    def poll(self, context):
        if not context.armature:
            return False
        obj = context.object
        if obj:
            return (obj.mode in ('POSE', 'OBJECT'))
        return False

    def draw(self, context):
        layout = self.layout
        try:
            active_type = context.active_pose_bone["type"]
        except:
            active_type = None

        scene = context.scene
        pose_templates = scene.pose_templates

        if pose_templates.active_template_index == -1:
            pose_templates.active_template_index = 0

        if not self.templates:
            self.templates[:] = metarig_templates()

        while(len(pose_templates.templates) < len(self.templates)):
            pose_templates.templates.add()
        while(len(pose_templates.templates) > len(self.templates)):
            pose_templates.templates.remove(0)

        for i, template_name in enumerate(self.templates):
            template = pose_templates.templates[i]
            if active_type == template_name:
                template.name = "<%s>" % template_name.replace("_", " ")
            else:
                template.name = " %s " % template_name.replace("_", " ")

        row = layout.row()
        row.operator("pose.metarig_generate", text="Generate")
        row.operator("pose.metarig_validate", text="Check")
        row.operator("pose.metarig_graph", text="Graph")
        row = layout.row()
        row.prop(pose_templates, "generate_def_rig")

        row = layout.row()
        col = row.column()
        col.template_list(pose_templates, "templates", pose_templates, "active_template_index", rows=1)

        subrow = col.split(percentage=0.5, align=True)
        subsubrow = subrow.row(align=True)
        subsubrow.operator("pose.metarig_assign", text="Assign")
        subsubrow.operator("pose.metarig_clear", text="Clear")

        subsubrow = subrow.split(percentage=0.8)
        subsubrow.operator("pose.metarig_sample_add", text="Sample").metarig_type = self.templates[pose_templates.active_template_index]
        subsubrow.operator("pose.metarig_sample_add", text="All").metarig_type = "" # self.templates[pose_templates.active_template_index]

        sub = row.column(align=True)
        sub.operator("pose.metarig_reload", icon="FILE_REFRESH", text="")


# operators
from bpy.props import StringProperty


class Reload(bpy.types.Operator):
    '''Re-Scan the metarig package directory for scripts'''

    bl_idname = "pose.metarig_reload"
    bl_label = "Re-Scan the list of metarig types"

    def execute(self, context):
        DATA_PT_template.templates[:] = metarig_templates()
        return ('FINISHED',)


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

    message.reverse() # XXX - stupid! menu's are upside down!

    operator.report(set(['INFO']), '\n'.join(message))


class Generate(bpy.types.Operator):
    '''Generates a metarig from the active armature'''

    bl_idname = "pose.metarig_generate"
    bl_label = "Generate Metarig"

    def execute(self, context):
        import rigify
        reload(rigify)

        meta_def = context.scene.pose_templates.generate_def_rig

        try:
            rigify.generate_rig(context, context.object, META_DEF=meta_def)
        except rigify.RigifyError as rig_exception:
            rigify_report_exception(self, rig_exception)

        return ('FINISHED',)


class Validate(bpy.types.Operator):
    '''Validate a metarig from the active armature'''

    bl_idname = "pose.metarig_validate"
    bl_label = "Validate Metarig"

    def execute(self, context):
        import rigify
        reload(rigify)
        try:
            rigify.validate_rig(context, context.object)
        except rigify.RigifyError as rig_exception:
            rigify_report_exception(self, rig_exception)
        return ('FINISHED',)


class Sample(bpy.types.Operator):
    '''Create a sample metarig to be modified before generating the final rig.'''

    bl_idname = "pose.metarig_sample_add"
    bl_label = "Re-Scan Metarig Scripts"

    metarig_type = StringProperty(name="Type", description="Name of the rig type to generate a sample of, a blank string for all", maxlen=128, default="")

    def execute(self, context):
        import rigify
        reload(rigify)
        final = (self.properties.metarig_type == "")
        objects = rigify.generate_test(context, metarig_type=self.properties.metarig_type, GENERATE_FINAL=final)

        if len(objects) > 1:
            for i, (obj_meta, obj_gen) in enumerate(objects):
                obj_meta.location.x = i * 1.0
                if obj_gen:
                    obj_gen.location.x = i * 1.0

        return ('FINISHED',)


class Graph(bpy.types.Operator):
    '''Create a graph from the active armature through graphviz'''

    bl_idname = "pose.metarig_graph"
    bl_label = "Pose Graph"

    def execute(self, context):
        import os
        import graphviz_export
        import bpy
        reload(graphviz_export)
        obj = bpy.context.object
        path = os.path.splitext(bpy.data.filename)[0] + "-" + bpy.utils.clean_name(obj.name)
        path_dot = path + ".dot"
        path_png = path + ".png"
        saved = graphviz_export.graph_armature(bpy.context.object, path_dot, CONSTRAINTS=False, DRIVERS=False)

        if saved:
            # if we seriously want this working everywhere we'll need some new approach
            os.system("dot -Tpng %s > %s; gnome-open %s &" % (path_dot, path_png, path_png))
            #os.system("python /b/xdot.py '%s' &" % path_dot)

        return ('FINISHED',)


class AsScript(bpy.types.Operator):
    '''Write the edit armature out as a python script'''

    bl_idname = "pose.metarig_to_script"
    bl_label = "Write Metarig to Script"
    bl_register = True
    bl_undo = True

    path = StringProperty(name="File Path", description="File path used for exporting the Armature file", maxlen=1024, default="")

    def execute(self, context):
        import rigify_utils
        reload(rigify_utils)
        obj = context.object
        code = rigify_utils.write_meta_rig(obj)
        path = self.properties.path
        file = open(path, "w")
        file.write(code)
        file.close()

        return ('FINISHED',)

    def invoke(self, context, event):
        import os
        obj = context.object
        self.properties.path = os.path.splitext(bpy.data.filename)[0] + "-" + bpy.utils.clean_name(obj.name) + ".py"
        wm = context.manager
        wm.add_fileselect(self)
        return ('RUNNING_MODAL',)


# operators that use the GUI
class ActiveAssign(bpy.types.Operator):
    '''Assign to the active posebone'''

    bl_idname = "pose.metarig_assign"
    bl_label = "Assign to the active posebone"

    def poll(self, context):
        bone = context.active_pose_bone
        return bool(bone and bone.id_data.mode == 'POSE')

    def execute(self, context):
        scene = context.scene
        pose_templates = scene.pose_templates
        template_name = DATA_PT_template.templates[pose_templates.active_template_index]
        context.active_pose_bone["type"] = template_name
        return ('FINISHED',)


class ActiveClear(bpy.types.Operator):
    '''Clear type from the active posebone'''

    bl_idname = "pose.metarig_clear"
    bl_label = "Metarig Clear Type"

    def poll(self, context):
        bone = context.active_pose_bone
        return bool(bone and bone.id_data.mode == 'POSE')

    def execute(self, context):
        scene = context.scene
        del context.active_pose_bone["type"]
        return ('FINISHED',)


import space_info
import dynamic_menu


class INFO_MT_armature_metarig_add(dynamic_menu.DynMenu):
    bl_idname = "INFO_MT_armature_metarig_add"
    bl_label = "Meta-Rig"

    def draw(self, context):
        import rigify

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        for submodule_type in rigify.get_submodule_types():
            text = bpy.utils.display_name(submodule_type)
            layout.operator("pose.metarig_sample_add", text=text, icon='OUTLINER_OB_ARMATURE').metarig_type = submodule_type

bpy.types.register(DATA_PT_template)

bpy.types.register(PoseTemplateSettings)
bpy.types.register(PoseTemplate)

bpy.ops.add(Reload)
bpy.ops.add(Generate)
bpy.ops.add(Validate)
bpy.ops.add(Sample)
bpy.ops.add(Graph)
bpy.ops.add(AsScript)

bpy.ops.add(ActiveAssign)
bpy.ops.add(ActiveClear)


bpy.types.register(INFO_MT_armature_metarig_add)

menu_func = (lambda self, context: self.layout.menu("INFO_MT_armature_metarig_add", icon='OUTLINER_OB_ARMATURE'))
menu_item = dynamic_menu.add(bpy.types.INFO_MT_armature_add, menu_func)
