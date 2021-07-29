# ***** BEGIN GPL LICENSE BLOCK *****
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****

bl_info = {
    "name": "Edit Linked Library",
    "author": "Jason van Gumster (Fweeb), Bassam Kurdali, Pablo Vazquez",
    "version": (0, 8, 1),
    "blender": (2, 74, 0),
    "location": "View3D > Toolshelf > Edit Linked Library",
    "description": "Allows editing of objects linked from a .blend library.",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Object/Edit_Linked_Library",
    "category": "Object",
}


import bpy
from bpy.app.handlers import persistent
import os

settings = {
    "original_file": "",
    "linked_file": "",
    "linked_objects": [],
    }


@persistent
def linked_file_check(context):
    if settings["linked_file"] != "":
        if os.path.samefile(settings["linked_file"], bpy.data.filepath):
            print("Editing a linked library.")
            bpy.ops.object.select_all(action='DESELECT')
            for ob_name in settings["linked_objects"]:
                bpy.data.objects[ob_name].select = True  # XXX Assumes selected object is in the active scene
            if len(settings["linked_objects"]) == 1:
                bpy.context.scene.objects.active = bpy.data.objects[settings["linked_objects"][0]]
        else:
            # For some reason, the linked editing session ended
            # (failed to find a file or opened a different file
            # before returning to the originating .blend)
            settings["original_file"] = ""
            settings["linked_file"] = ""


class EditLinked(bpy.types.Operator):
    """Edit Linked Library"""
    bl_idname = "object.edit_linked"
    bl_label = "Edit Linked Library"

    use_autosave = bpy.props.BoolProperty(
            name="Autosave",
            description="Save the current file before opening the linked library",
            default=True)
    use_instance = bpy.props.BoolProperty(
            name="New Blender Instance",
            description="Open in a new Blender instance",
            default=False)

    @classmethod
    def poll(cls, context):
        return settings["original_file"] == "" and context.active_object is not None and (
                (context.active_object.dupli_group and
                 context.active_object.dupli_group.library is not None) or
                (context.active_object.proxy and
                 context.active_object.proxy.library is not None) or
                 context.active_object.library is not None)
        #return context.active_object is not None

    def execute(self, context):
        #print(bpy.context.active_object.library)
        target = context.active_object

        if target.dupli_group and target.dupli_group.library:
            targetpath = target.dupli_group.library.filepath
            settings["linked_objects"].extend({ob.name for ob in target.dupli_group.objects})
        elif target.library:
            targetpath = target.library.filepath
            settings["linked_objects"].append(target.name)
        elif target.proxy:
            target = target.proxy
            targetpath = target.library.filepath
            settings["linked_objects"].append(target.name)

        if targetpath:
            print(target.name + " is linked to " + targetpath)

            if self.use_autosave:
                if not bpy.data.filepath:
                    # File is not saved on disk, better to abort!
                    self.report({'ERROR'}, "Current file does not exist on disk, we cannot autosave it, aborting")
                    return {'CANCELLED'}
                bpy.ops.wm.save_mainfile()

            settings["original_file"] = bpy.data.filepath
            settings["linked_file"] = bpy.path.abspath(targetpath)

            if self.use_instance:
                import subprocess
                try:
                    subprocess.Popen([bpy.app.binary_path, settings["linked_file"]])
                except:
                    print("Error on the new Blender instance")
                    import traceback
                    traceback.print_exc()
            else:
                bpy.ops.wm.open_mainfile(filepath=settings["linked_file"])

            print("Opened linked file!")
        else:
            self.report({'WARNING'}, target.name + " is not linked")
            print(target.name + " is not linked")

        return {'FINISHED'}


class ReturnToOriginal(bpy.types.Operator):
    """Load the original file"""
    bl_idname = "wm.return_to_original"
    bl_label = "Return to Original File"

    use_autosave = bpy.props.BoolProperty(
            name="Autosave",
            description="Save the current file before opening original file",
            default=True)

    @classmethod
    def poll(cls, context):
        return (settings["original_file"] != "")

    def execute(self, context):
        if self.use_autosave:
            bpy.ops.wm.save_mainfile()

        bpy.ops.wm.open_mainfile(filepath=settings["original_file"])

        settings["original_file"] = ""
        settings["linked_objects"] = []
        print("Back to the original!")
        return {'FINISHED'}


# UI
# TODO:Add operators to the File menu?
#      Hide the entire panel for non-linked objects?
class PanelLinkedEdit(bpy.types.Panel):
    bl_label = "Edit Linked Library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Relations"
    bl_context = "objectmode"

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None) or (settings["original_file"] != "")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        icon = "OUTLINER_DATA_" + context.active_object.type

        target = None

        if context.active_object.proxy:
            target = context.active_object.proxy
        else:
            target = context.active_object.dupli_group

        if settings["original_file"] == "" and (
                (target and
                 target.library is not None) or
                context.active_object.library is not None):

            if (target is not None):
                props = layout.operator("object.edit_linked", icon="LINK_BLEND",
                                        text="Edit Library: %s" % target.name)
            else:
                props = layout.operator("object.edit_linked", icon="LINK_BLEND",
                                        text="Edit Library: %s" % context.active_object.name)
            props.use_autosave = scene.use_autosave
            props.use_instance = scene.use_instance

            layout.prop(scene, "use_autosave")
            layout.prop(scene, "use_instance")

            if (target is not None):
                layout.label(text="Path: %s" %
                             target.library.filepath)
            else:
                layout.label(text="Path: %s" %
                             context.active_object.library.filepath)

        elif settings["original_file"] != "":

            if scene.use_instance:
                layout.operator("wm.return_to_original",
                                text="Reload Current File",
                                icon="FILE_REFRESH").use_autosave = False

                layout.separator()

                #XXX - This is for nested linked assets... but it only works
                #  when launching a new Blender instance. Nested links don't
                #  currently work when using a single instance of Blender.
                props = layout.operator("object.edit_linked",
                                        text="Edit Library: %s" % context.active_object.dupli_group.name,
                                        icon="LINK_BLEND")
                props.use_autosave = scene.use_autosave
                props.use_instance = scene.use_instance
                layout.prop(scene, "use_autosave")
                layout.prop(scene, "use_instance")

                layout.label(text="Path: %s" %
                             context.active_object.dupli_group.library.filepath)

            else:
                props = layout.operator("wm.return_to_original", icon="LOOP_BACK")
                props.use_autosave = scene.use_autosave

                layout.prop(scene, "use_autosave")

        else:
            layout.label(text="%s is not linked" % context.active_object.name,
                         icon=icon)


addon_keymaps = []


def register():
    bpy.app.handlers.load_post.append(linked_file_check)
    bpy.utils.register_class(EditLinked)
    bpy.utils.register_class(ReturnToOriginal)
    bpy.utils.register_class(PanelLinkedEdit)

    # Is there a better place to store this properties?
    bpy.types.Scene.use_autosave = bpy.props.BoolProperty(
            name="Autosave",
            description="Save the current file before opening a linked file",
            default=True)
    bpy.types.Scene.use_instance = bpy.props.BoolProperty(
            name="New Blender Instance",
            description="Open in a new Blender instance",
            default=False)

    # Keymapping (deactivated by default; activated when a library object is selected)
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:  # don't register keymaps from command line
        km = kc.keymaps.new(name="3D View", space_type='VIEW_3D')
        kmi = km.keymap_items.new("object.edit_linked", 'NUMPAD_SLASH', 'PRESS', shift=True)
        kmi.active = True
        addon_keymaps.append((km, kmi))
        kmi = km.keymap_items.new("wm.return_to_original", 'NUMPAD_SLASH', 'PRESS', shift=True)
        kmi.active = True
        addon_keymaps.append((km, kmi))


def unregister():
    bpy.utils.unregister_class(EditLinked)
    bpy.utils.unregister_class(ReturnToOriginal)
    bpy.utils.unregister_class(PanelLinkedEdit)
    bpy.app.handlers.load_post.remove(linked_file_check)

    del bpy.types.Scene.use_autosave
    del bpy.types.Scene.use_instance

    # handle the keymap
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()


if __name__ == "__main__":
    register()
