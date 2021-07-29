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
    "name": "Selection Restrictor",
    "author": "Ales Sidenko",
    "version": (0, 1, 1),
    "location": "3d viewer header",
    "warning": "",
    "description": "This addon helps to restrict the selection of objects by type. "
                   "Please email me if you find a bug (sidenkoai@gmail.com)",
    "category": "3D View"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )
from bpy.props import (
        BoolProperty,
        StringProperty,
        )

from bpy.app.handlers import persistent

mesh = 'OBJECT_DATA'
curve = 'OUTLINER_OB_CURVE'
arm = 'OUTLINER_OB_ARMATURE'
empty = 'OUTLINER_OB_EMPTY'
cam = 'OUTLINER_OB_CAMERA'
lamp = 'OUTLINER_OB_LAMP'
lat = 'OUTLINER_OB_LATTICE'
font = 'OUTLINER_OB_FONT'
meta = 'OUTLINER_OB_META'
surf = 'OUTLINER_OB_SURFACE'
speak = 'OUTLINER_OB_SPEAKER'

show = 'TRIA_RIGHT'
show_restrictor = False
hide = True


# checking properties in scene to update icons when opening file
# or switching between scenes (executing in end of script)

@persistent
def check_restrictors(dummy):
    global mesh
    global curve
    global arm
    global empty
    global cam
    global lamp
    global lat
    global font
    global meta
    global surf
    global speak
    global show

    global meshrestrictorenabled
    global curverestrictorenabled
    global armrestrictorenabled
    global emptyrestrictorenabled
    global camrestrictorenabled
    global lamprestrictorenabled
    global latrestrictorenabled
    global fontrestrictorenabled
    global metarestrictorenabled
    global surfrestrictorenabled
    global speakrestrictorenabled

    # show restrictors?
    if bpy.context.scene.get('show_restrictor') is not None:
        show_restrictor = False
        show = 'TRIA_RIGHT'
    else:
        show_restrictor = True
        show = 'TRIA_DOWN'

    # mesh
    if bpy.context.scene.get('meshrestrictor') is None:
        meshrestrictorenabled = True
        mesh = 'OBJECT_DATA'
    else:
        meshrestrictorenabled = False
        mesh = 'MESH_CUBE'
    # curve
    if bpy.context.scene.get('curverestrictor') is None:
        curverestrictorenabled = True
        curve = 'OUTLINER_OB_CURVE'
    else:
        curverestrictorenabled = False
        curve = 'CURVE_DATA'
    # armature
    if bpy.context.scene.get('armrestrictor') is None:
        armrestrictorenabled = True
        arm = 'OUTLINER_OB_ARMATURE'
    else:
        armrestrictorenabled = False
        arm = 'ARMATURE_DATA'

    # empty
    if bpy.context.scene.get('emptyrestrictor') is None:
        emptyrestrictorenabled = True
        empty = 'OUTLINER_OB_EMPTY'
    else:
        emptyrestrictorenabled = False
        empty = 'EMPTY_DATA'

    # camera
    if bpy.context.scene.get('camrestrictor') is None:
        camrestrictorenabled = True
        cam = 'OUTLINER_OB_CAMERA'
    else:
        camrestrictorenabled = False
        cam = 'CAMERA_DATA'
    # lamp
    if bpy.context.scene.get('lamprestrictor') is None:
        lamprestrictorenabled = True
        lamp = 'OUTLINER_OB_LAMP'
    else:
        lamprestrictorenabled = False
        lamp = 'LAMP_DATA'

    # lattice
    if bpy.context.scene.get('latrestrictor') is None:
        latrestrictorenabled = True
        lat = 'OUTLINER_OB_LATTICE'
    else:
        latrestrictorenabled = False
        lat = 'LATTICE_DATA'

    # text
    if bpy.context.scene.get('fontrestrictor') is None:
        fontrestrictorenabled = True
        font = 'OUTLINER_OB_FONT'
    else:
        fontrestrictorenabled = False
        font = 'FONT_DATA'

    # metaballs
    if bpy.context.scene.get('metarestrictor') is None:
        metarestrictorenabled = True
        meta = 'OUTLINER_OB_META'
    else:
        metarestrictorenabled = False
        meta = 'META_DATA'

    # surfaces
    if bpy.context.scene.get('surfrestrictor') is None:
        surfrestrictorenabled = True
        surf = 'OUTLINER_OB_SURFACE'
    else:
        surfrestrictorenabled = False
        surf = 'SURFACE_DATA'

    # sounds
    if bpy.context.scene.get('speakrestrictor') is None:
        speakrestrictorenabled = True
        speak = 'OUTLINER_OB_SPEAKER'
    else:
        speakrestrictorenabled = False
        speak = 'SPEAKER'
    return{'FINISHED'}


# Show / Hide buttons

class RestrictorShow(Operator):
    bl_idname = "restrictor.show"
    bl_label = "Show/Hide Selection Restrictors"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Show/Hide Selection Restrictors"

    hide = StringProperty()

    def execute(self, context):
        global show

        if bpy.context.scene.get('show_restrictor') is None:
            bpy.context.scene['show_restrictor'] = 1
            show = 'TRIA_DOWN'
        else:
            if bpy.context.scene.get('show_restrictor') is not None:
                del bpy.context.scene['show_restrictor']
                show = 'TRIA_RIGHT'

        return {'FINISHED'}


# Ignore the restrictor for selected objects

class IgnoreRestrictors(Operator):
    bl_idname = "ignore.restrictors"
    bl_label = "Ignore Restrictor by Selected Objects"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Ignore or do not ignore Restrictor by selected objects"
    ignore = BoolProperty()

    def execute(self, context):
        if self.ignore is True:
            for ob in bpy.context.selected_objects:
                ob['ignore_restrictors'] = 1
        else:
            for ob in bpy.context.selected_objects:
                if ob.get('ignore_restrictors') is not None:
                    del ob["ignore_restrictors"]
            bpy.ops.refresh.restrictors()

        return{'FINISHED'}


# Enable or Disable restrictors

# Restrictor Mesh

class RestrictorMesh(Operator):
    bl_idname = "restrictor.mesh"
    bl_label = "restrictor meshes"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Meshes selection restrictor"
    mesh = StringProperty()

    def execute(self, context):
        global mesh
        global meshrestrictorenabled
        if bpy.context.scene.get('meshrestrictor') is not None:
            meshrestrictorenabled = True
            if bpy.context.scene.get('meshrestrictor') is not None:
                del bpy.context.scene['meshrestrictor']
            mesh = 'OBJECT_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'MESH':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            meshrestrictorenabled = False
            bpy.context.scene['meshrestrictor'] = 1
            mesh = 'MESH_CUBE'
            for ob in bpy.context.scene.objects:
                if ob.type == 'MESH':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Curves

class RestrictorCurve(Operator):
    bl_idname = "restrictor.curve"
    bl_label = "restrictor curves"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Curves selection restrictor"

    def execute(self, context):
        global curve
        global curverestrictorenabled

        if bpy.context.scene.get('curverestrictor') is not None:
            curverestrictorenabled = True
            if bpy.context.scene.get('curverestrictor') is not None:
                del bpy.context.scene['curverestrictor']
            curve = 'OUTLINER_OB_CURVE'
            for ob in bpy.context.scene.objects:
                if ob.type == 'CURVE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            curverestrictorenabled = False
            bpy.context.scene['curverestrictor'] = 1
            curve = 'CURVE_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'CURVE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Armatures

class RestrictorArm(Operator):
    bl_idname = "restrictor.arm"
    bl_label = "restrictor armatures"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Armatures selection restrictor"

    def execute(self, context):
        global arm
        global armrestrictorenabled

        if bpy.context.scene.get('armrestrictor') is not None:
            armrestrictorenabled = True
            if bpy.context.scene.get('armrestrictor') is not None:
                del bpy.context.scene['armrestrictor']
            arm = 'OUTLINER_OB_ARMATURE'
            for ob in bpy.context.scene.objects:
                if ob.type == 'ARMATURE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            armrestrictorenabled = False
            bpy.context.scene['armrestrictor'] = 1
            arm = 'ARMATURE_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'ARMATURE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Empties

class RestrictorEmpty(Operator):
    bl_idname = "restrictor.empty"
    bl_label = "Restrictor Empties"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Empties selection restrictor"

    def execute(self, context):
        global empty
        global emptyrestrictorenabled

        if bpy.context.scene.get('emptyrestrictor') is not None:
            emptyrestrictorenabled = True
            if bpy.context.scene.get('emptyrestrictor') is not None:
                del bpy.context.scene['emptyrestrictor']
            empty = 'OUTLINER_OB_EMPTY'
            for ob in bpy.context.scene.objects:
                if ob.type == 'EMPTY':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            emptyrestrictorenabled = False
            bpy.context.scene['emptyrestrictor'] = 1
            empty = 'EMPTY_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'EMPTY':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Cameras

class RestrictorCam(Operator):
    bl_idname = "restrictor.cam"
    bl_label = "restrictor cameras"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Cameras selection restrictor"

    def execute(self, context):
        global cam
        global camrestrictorenabled

        if bpy.context.scene.get('camrestrictor') is not None:
            camrestrictorenabled = True
            if bpy.context.scene.get('camrestrictor') is not None:
                del bpy.context.scene['camrestrictor']
            cam = 'OUTLINER_OB_CAMERA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'CAMERA':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            camrestrictorenabled = False
            bpy.context.scene['camrestrictor'] = 1
            cam = 'CAMERA_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'CAMERA':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Lamps

class RestrictorLamp(Operator):
    bl_idname = "restrictor.lamp"
    bl_label = "Restrictor Lamps"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Lamps selection restrictor"

    def execute(self, context):
        global lamp
        global lamprestrictorenabled

        if bpy.context.scene.get('lamprestrictor') is not None:
            lamprestrictorenabled = True
            if bpy.context.scene.get('lamprestrictor') is not None:
                del bpy.context.scene['lamprestrictor']
            lamp = 'OUTLINER_OB_LAMP'
            for ob in bpy.context.scene.objects:
                if ob.type == 'LAMP':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            lamprestrictorenabled = False
            bpy.context.scene['lamprestrictor'] = 1
            lamp = 'LAMP_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'LAMP':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Lattice

class RestrictorLat(Operator):
    bl_idname = "restrictor.lat"
    bl_label = "Restrictor Lattices"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Lattices selection restrictor"

    def execute(self, context):
        global lat
        global latrestrictorenabled

        if bpy.context.scene.get('latrestrictor') is not None:
            latrestrictorenabled = True
            if bpy.context.scene.get('latrestrictor') is not None:
                del bpy.context.scene['latrestrictor']
            lat = 'OUTLINER_OB_LATTICE'
            for ob in bpy.context.scene.objects:
                if ob.type == 'LATTICE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False
        else:
            latrestrictorenabled = False
            bpy.context.scene['latrestrictor'] = 1
            lat = 'LATTICE_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'LATTICE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor Font

class RestrictorFont(Operator):
    bl_idname = "restrictor.font"
    bl_label = "Restrictor Font"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Text selection restrictor"

    def execute(self, context):
        global font
        global fontrestrictorenabled

        if bpy.context.scene.get('fontrestrictor') is not None:
            fontrestrictorenabled = True
            if bpy.context.scene.get('fontrestrictor') is not None:
                del bpy.context.scene['fontrestrictor']
            font = 'OUTLINER_OB_FONT'
            for ob in bpy.context.scene.objects:
                if ob.type == 'FONT':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False
        else:
            fontrestrictorenabled = False
            bpy.context.scene['fontrestrictor'] = 1
            font = 'FONT_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'FONT':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Metaballs

class RestrictorMeta(Operator):
    bl_idname = "restrictor.meta"
    bl_label = "restrictor metaballs"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Metaballs selection restrictor"

    def execute(self, context):
        global meta
        global metarestrictorenabled

        if bpy.context.scene.get('metarestrictor') is not None:
            metarestrictorenabled = True
            if bpy.context.scene.get('metarestrictor') is not None:
                del bpy.context.scene['metarestrictor']
            meta = 'OUTLINER_OB_META'
            for ob in bpy.context.scene.objects:
                if ob.type == 'META':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False
        else:
            metarestrictorenabled = False
            bpy.context.scene['metarestrictor'] = 1
            meta = 'META_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'META':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Surfaces

class RestrictorSurf(Operator):
    bl_idname = "restrictor.surf"
    bl_label = "Restrictor Surfaces"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Surfaces selection restrictor"

    def execute(self, context):
        global surf
        global surfrestrictorenabled

        if bpy.context.scene.get('surfrestrictor') is not None:
            surfrestrictorenabled = True
            if bpy.context.scene.get('surfrestrictor') is not None:
                del bpy.context.scene['surfrestrictor']
            surf = 'OUTLINER_OB_SURFACE'
            for ob in bpy.context.scene.objects:
                if ob.type == 'SURFACE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False
        else:
            surfrestrictorenabled = False
            bpy.context.scene['surfrestrictor'] = 1
            surf = 'SURFACE_DATA'
            for ob in bpy.context.scene.objects:
                if ob.type == 'SURFACE':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# Restrictor for Speakers

class RestrictorSound(Operator):
    bl_idname = "restrictor.speak"
    bl_label = "Restrictor Speakers"
    bl_description = "Sounds selection restrictor"
    bl_option = {'REGISTER', 'UNDO'}

    def execute(self, context):
        global speak
        global speakrestrictorenabled

        if bpy.context.scene.get('speakrestrictor') is not None:
            speakrestrictorenabled = True
            if bpy.context.scene.get('speakrestrictor') is not None:
                del bpy.context.scene['speakrestrictor']
            speak = 'OUTLINER_OB_SPEAKER'
            for ob in bpy.context.scene.objects:
                if ob.type == 'SPEAKER':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False

        else:
            speakrestrictorenabled = False
            bpy.context.scene['speakrestrictor'] = 1
            speak = 'SPEAKER'
            for ob in bpy.context.scene.objects:
                if ob.type == 'SPEAKER':
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = True
                        ob.select = False

        return{'FINISHED'}


# refresh restrictors for newly created objects

class RefreshRestrictors(Operator):
    bl_idname = "refresh.restrictors"
    bl_label = "Refresh Selection Restrictors"
    bl_option = {'REGISTER', 'UNDO'}
    bl_description = "Refresh restrictors"

    def execute(self, context):
        global mesh
        global curve
        global arm
        global empty
        global cam
        global lamp
        global lat
        global font
        global meta
        global surf
        global speak

        datas = {
            'meshrestrictor': ("OBJECT_DATA", "MESH_CUBE", "MESH"),
            'curverestrictor': ("OUTLINER_OB_CURVE", "CURVE_DATA", "CURVE"),
            'armrestrictor': ("OUTLINER_OB_ARMATURE", "ARMATURE_DATA", "ARMATURE"),
            'emptyrestrictor': ("OUTLINER_OB_EMPTY", "EMPTY_DATA", "EMPTY"),
            'camrestrictor': ("OUTLINER_OB_CAMERA", "CAMERA_DATA", "CAMERA"),
            'lamprestrictor': ("OUTLINER_OB_LAMP", "LAMP_DATA", "LAMP"),
            'latrestrictor': ("OUTLINER_OB_LATTICE", "LATTICE", "LATTICE"),
            'fontrestrictor': ("OUTLINER_OB_FONT", "FONT", "FONT"),
            'metarestrictor': ("OUTLINER_OB_META", "META_DATA", "META"),
            'surfrestrictor': ("SURFACE", "SURFACE_DATA", "SURFACE"),
            'speakrestrictor': ("OUTLINER_OB_SPEAKER", "SPEAKER", "SPEAKER"),
            }

        for prop, values in datas.items():
            icon_i, icon_a, types = values
            get_props = bpy.context.scene.get(prop)

            gl_icon = icon_a if get_props else icon_i

            for ob in bpy.context.scene.objects:
                if ob.type == types:
                    if ob.get('ignore_restrictors') is None:
                        ob.hide_select = False if get_props is None else True
                        if get_props is None:
                            ob.select = False

            mesh = gl_icon if types == "MESH" else mesh
            curve = gl_icon if types == "CURVE" else curve
            arm = gl_icon if types == "ARMATURE" else arm
            empty = gl_icon if types == "EMPTY" else empty
            cam = gl_icon if types == "CAMERA" else cam
            lamp = gl_icon if types == "LAMP" else lamp
            lat = gl_icon if types == "LATTICE" else lat
            font = gl_icon if types == "FONT" else font
            meta = gl_icon if types == "META" else meta
            surf = gl_icon if types == "SURFACE" else surf
            speak = gl_icon if types == "SPEAKER" else speak

        return{'FINISHED'}


class RestrictorSelection(Menu):
    """Restrict Selection"""
    bl_label = "Selection"
    bl_idname = "RestrictorSelection"

    def draw(self, context):
        global mesh
        global curve
        global arm
        global empty
        global cam
        global lamp
        global lat
        global font
        global meta
        global surf
        global speak
        global show_buttons
        global show

        layout = self.layout

        layout.operator("restrictor.mesh", icon=mesh, text="Mesh")
        layout.operator("restrictor.curve", icon=curve, text="Curve")
        layout.operator("restrictor.arm", icon=arm, text="Armature")
        layout.operator("restrictor.empty", icon=empty, text="Empty")
        layout.operator("restrictor.cam", icon=cam, text="Camera")
        layout.operator("restrictor.lamp", icon=lamp, text="Lamp")
        layout.operator("restrictor.lat", icon=lat, text="Lattice")
        layout.operator("restrictor.font", icon=font, text="Font")
        layout.operator("restrictor.meta", icon=meta, text="MetaBall")
        layout.operator("restrictor.surf", icon=surf, text="Surface")
        layout.operator("restrictor.speak", icon=speak, text="Speaker")
        layout.separator()
        layout.operator("ignore.restrictors", icon='GHOST_ENABLED', text="Enable").ignore = True
        layout.operator("ignore.restrictors", icon='GHOST_DISABLED', text="Disable").ignore = False
        layout.operator("refresh.restrictors", icon='FILE_REFRESH', text="Refresh")


def view3d_select_menu(self, context):
    self.layout.menu(RestrictorSelection.bl_idname)


def register():
    bpy.types.VIEW3D_HT_header.append(view3d_select_menu)


def unregister():
    bpy.types.VIEW3D_HT_header.remove(view3d_select_menu)

    bpy.utils.unregister_class(RefreshRestrictors)


if __name__ == "__main__":
    register()

# update icons when opening file and updating scene data
# I don't know what does "updating scene data" mean
# But I've added it here to refresh icons while switching scenes
bpy.app.handlers.load_post.append(check_restrictors)
bpy.app.handlers.scene_update_post.append(check_restrictors)
