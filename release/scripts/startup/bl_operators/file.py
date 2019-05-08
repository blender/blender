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
from bpy.types import Operator
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    StringProperty,
)

# ########## Datablock previews... ##########


class WM_OT_previews_batch_generate(Operator):
    """Generate selected .blend file's previews"""
    bl_idname = "wm.previews_batch_generate"
    bl_label = "Batch-Generate Previews"
    bl_options = {'REGISTER'}

    # -----------
    # File props.
    files: CollectionProperty(
        type=bpy.types.OperatorFileListElement,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    directory: StringProperty(
        maxlen=1024,
        subtype='FILE_PATH',
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # Show only images/videos, and directories!
    filter_blender: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )
    filter_folder: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # -----------
    # Own props.
    use_scenes: BoolProperty(
        default=True,
        name="Scenes",
        description="Generate scenes' previews",
    )
    use_collections: BoolProperty(
        default=True,
        name="Collections",
        description="Generate collections' previews",
    )
    use_objects: BoolProperty(
        default=True,
        name="Objects",
        description="Generate objects' previews",
    )
    use_intern_data: BoolProperty(
        default=True,
        name="Mat/Tex/...",
        description="Generate 'internal' previews (materials, textures, images, etc.)",
    )

    use_trusted: BoolProperty(
        default=False,
        name="Trusted Blend Files",
        description="Enable python evaluation for selected files",
    )
    use_backups: BoolProperty(
        default=True,
        name="Save Backups",
        description="Keep a backup (.blend1) version of the files when saving with generated previews",
    )

    def invoke(self, context, _event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        import os
        import subprocess
        from bl_previews_utils import bl_previews_render as preview_render

        context.window_manager.progress_begin(0, len(self.files))
        context.window_manager.progress_update(0)
        for i, fn in enumerate(self.files):
            blen_path = os.path.join(self.directory, fn.name)
            cmd = [
                bpy.app.binary_path,
                "--background",
                "--factory-startup",
                "-noaudio",
            ]
            if self.use_trusted:
                cmd.append("--enable-autoexec")
            cmd.extend([
                blen_path,
                "--python",
                os.path.join(os.path.dirname(preview_render.__file__), "bl_previews_render.py"),
                "--",
            ])
            if not self.use_scenes:
                cmd.append('--no_scenes')
            if not self.use_collections:
                cmd.append('--no_collections')
            if not self.use_objects:
                cmd.append('--no_objects')
            if not self.use_intern_data:
                cmd.append('--no_data_intern')
            if not self.use_backups:
                cmd.append("--no_backups")
            if subprocess.call(cmd):
                self.report({'ERROR'}, "Previews generation process failed for file '%s'!" % blen_path)
                context.window_manager.progress_end()
                return {'CANCELLED'}
            context.window_manager.progress_update(i + 1)
        context.window_manager.progress_end()

        return {'FINISHED'}


class WM_OT_previews_batch_clear(Operator):
    """Clear selected .blend file's previews"""
    bl_idname = "wm.previews_batch_clear"
    bl_label = "Batch-Clear Previews"
    bl_options = {'REGISTER'}

    # -----------
    # File props.
    files: CollectionProperty(
        type=bpy.types.OperatorFileListElement,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    directory: StringProperty(
        maxlen=1024,
        subtype='FILE_PATH',
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # Show only images/videos, and directories!
    filter_blender: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )
    filter_folder: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # -----------
    # Own props.
    use_scenes: BoolProperty(
        default=True,
        name="Scenes",
        description="Clear scenes' previews",
    )
    use_collections: BoolProperty(
        default=True,
        name="Collections",
        description="Clear collections' previews",
    )
    use_objects: BoolProperty(
        default=True,
        name="Objects",
        description="Clear objects' previews",
    )
    use_intern_data: BoolProperty(
        default=True,
        name="Mat/Tex/...",
        description="Clear 'internal' previews (materials, textures, images, etc.)",
    )

    use_trusted: BoolProperty(
        default=False,
        name="Trusted Blend Files",
        description="Enable python evaluation for selected files",
    )
    use_backups: BoolProperty(
        default=True,
        name="Save Backups",
        description="Keep a backup (.blend1) version of the files when saving with cleared previews",
    )

    def invoke(self, context, _event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        import os
        import subprocess
        from bl_previews_utils import bl_previews_render as preview_render

        context.window_manager.progress_begin(0, len(self.files))
        context.window_manager.progress_update(0)
        for i, fn in enumerate(self.files):
            blen_path = os.path.join(self.directory, fn.name)
            cmd = [
                bpy.app.binary_path,
                "--background",
                "--factory-startup",
                "-noaudio",
            ]
            if self.use_trusted:
                cmd.append("--enable-autoexec")
            cmd.extend([
                blen_path,
                "--python",
                os.path.join(os.path.dirname(preview_render.__file__), "bl_previews_render.py"),
                "--",
                "--clear",
            ])
            if not self.use_scenes:
                cmd.append('--no_scenes')
            if not self.use_collections:
                cmd.append('--no_collections')
            if not self.use_objects:
                cmd.append('--no_objects')
            if not self.use_intern_data:
                cmd.append('--no_data_intern')
            if not self.use_backups:
                cmd.append("--no_backups")
            if subprocess.call(cmd):
                self.report({'ERROR'}, "Previews clear process failed for file '%s'!" % blen_path)
                context.window_manager.progress_end()
                return {'CANCELLED'}
            context.window_manager.progress_update(i + 1)
        context.window_manager.progress_end()

        return {'FINISHED'}


class WM_OT_blend_strings_utf8_validate(Operator):
    """Check and fix all strings in current .blend file to be valid UTF-8 Unicode """ \
    """(needed for some old, 2.4x area files)"""
    bl_idname = "wm.blend_strings_utf8_validate"
    bl_label = "Validate .blend strings"
    bl_options = {'REGISTER'}

    def validate_strings(self, item, done_items):
        if item is None:
            return False

        if item in done_items:
            return False
        done_items.add(item)

        if getattr(item, "library", None) is not None:
            return False  # No point in checking library data, we cannot fix it anyway...

        changed = False
        for prop in item.bl_rna.properties:
            if prop.identifier in {"bl_rna", "rna_type"}:
                continue  # Or we'd recurse 'till Hell freezes.
            if prop.is_readonly:
                continue
            if prop.type == 'STRING':
                val_bytes = item.path_resolve(prop.identifier, False).as_bytes()
                val_utf8 = val_bytes.decode('utf-8', 'replace')
                val_bytes_valid = val_utf8.encode('utf-8')
                if val_bytes_valid != val_bytes:
                    print("found bad utf8 encoded string %r, fixing to %r (%r)..."
                          "" % (val_bytes, val_bytes_valid, val_utf8))
                    setattr(item, prop.identifier, val_utf8)
                    changed = True
            elif prop.type == 'POINTER':
                it = getattr(item, prop.identifier)
                changed |= self.validate_strings(it, done_items)
            elif prop.type == 'COLLECTION':
                for it in getattr(item, prop.identifier):
                    changed |= self.validate_strings(it, done_items)
        return changed

    def execute(self, _context):
        changed = False
        done_items = set()
        for prop in bpy.data.bl_rna.properties:
            if prop.type == 'COLLECTION':
                for it in getattr(bpy.data, prop.identifier):
                    changed |= self.validate_strings(it, done_items)
        if changed:
            self.report({'WARNING'},
                        "Some strings were fixed, don't forget to save the .blend file to keep those changes")
        return {'FINISHED'}


classes = (
    WM_OT_previews_batch_clear,
    WM_OT_previews_batch_generate,
    WM_OT_blend_strings_utf8_validate,
)
