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
from bpy.props import (
        BoolProperty,
        PointerProperty,
        )
from bpy.types import (
        PropertyGroup,
        AddonPreferences,
        )


bl_info = {
    "name": "3D Viewport Pie Menus",
    "author": "meta-androcto, pitiwazou, chromoly, italic",
    "version": (1, 1, 8),
    "blender": (2, 7, 7),
    "description": "Individual Pie Menu Activation List",
    "location": "Addons Preferences",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/3D_interaction/viewport_pies",
    "category": "Pie Menu"
    }

sub_modules_names = (
    "pie_modes_menu",
    "pie_views_numpad_menu",
    "pie_sculpt_menu",
    "pie_origin",
    "pie_cursor",
    "pie_manipulator_menu",
    "pie_snap_menu",
    "pie_orientation_menu",
    "pie_shading_menu",
    "pie_pivot_point_menu",
    "pie_proportional_menu",
    "pie_align_menu",
    "pie_delete_menu",
    "pie_apply_transform_menu",
    "pie_select_menu",
    "pie_animation_menu",
    "pie_save_open_menu",
    "pie_editor_switch_menu",
    )


sub_modules = [__import__(__package__ + "." + submod, {}, {}, submod) for
              submod in sub_modules_names]
sub_modules.sort(key=lambda mod: (mod.bl_info['category'], mod.bl_info['name']))


def _get_pref_class(mod):
    import inspect

    for obj in vars(mod).values():
        if inspect.isclass(obj) and issubclass(obj, PropertyGroup):
            if hasattr(obj, 'bl_idname') and obj.bl_idname == mod.__name__:
                return obj


def get_addon_preferences(name=''):
    """Acquisition and registration"""
    addons = bpy.context.user_preferences.addons
    if __name__ not in addons:  # wm.read_factory_settings()
        return None
    addon_prefs = addons[__name__].preferences
    if name:
        if not hasattr(addon_prefs, name):
            for mod in sub_modules:
                if mod.__name__.split('.')[-1] == name:
                    cls = _get_pref_class(mod)
                    if cls:
                        prop = PointerProperty(type=cls)
                        setattr(PieToolsPreferences, name, prop)
                        bpy.utils.unregister_class(PieToolsPreferences)
                        bpy.utils.register_class(PieToolsPreferences)
        return getattr(addon_prefs, name, None)
    else:
        return addon_prefs


def register_submodule(mod):
    if not hasattr(mod, '__addon_enabled__'):
        mod.__addon_enabled__ = False
    if not mod.__addon_enabled__:
        mod.register()
        mod.__addon_enabled__ = True


def unregister_submodule(mod):
    if mod.__addon_enabled__:
        mod.unregister()
        mod.__addon_enabled__ = False

        prefs = get_addon_preferences()
        name = mod.__name__.split('.')[-1]
        if hasattr(PieToolsPreferences, name):
            delattr(PieToolsPreferences, name)
            if prefs:
                bpy.utils.unregister_class(PieToolsPreferences)
                bpy.utils.register_class(PieToolsPreferences)
                if name in prefs:
                    del prefs[name]


def enable_all_modules(self, context):
    for mod in sub_modules:
        mod_name = mod.__name__.split('.')[-1]
        setattr(self, 'use_' + mod_name, False)
        if not mod.__addon_enabled__:
            setattr(self, 'use_' + mod_name, True)
            mod.__addon_enabled__ = True

    return None


def disable_all_modules(self, context):
    for mod in sub_modules:
        mod_name = mod.__name__.split('.')[-1]

        if mod.__addon_enabled__:
            setattr(self, 'use_' + mod_name, False)
            mod.__addon_enabled__ = False

    return None


class PieToolsPreferences(AddonPreferences):
    bl_idname = __name__

    enable_all = BoolProperty(
            name="Enable all",
            description="Enable all Pie Modules",
            default=False,
            update=enable_all_modules
            )
    disable_all = BoolProperty(
            name="Disable all",
            description="Disable all Pie Modules",
            default=False,
            update=disable_all_modules
            )

    def draw(self, context):
        layout = self.layout
        split = layout.split(percentage=0.5, align=True)
        row = split.row()
        row.alignment = "LEFT"
        sub_box = row.box()
        sub_box.prop(self, "enable_all", emboss=False,
                    icon="VISIBLE_IPO_ON", icon_only=True)
        row.label("Enable All")

        row = split.row()
        row.alignment = "RIGHT"
        row.label("Disable All")
        sub_box = row.box()
        sub_box.prop(self, "disable_all", emboss=False,
                    icon="VISIBLE_IPO_OFF", icon_only=True)

        for mod in sub_modules:
            mod_name = mod.__name__.split('.')[-1]
            info = mod.bl_info
            column = layout.column()
            box = column.box()

            # first stage
            expand = getattr(self, 'show_expanded_' + mod_name)
            icon = 'TRIA_DOWN' if expand else 'TRIA_RIGHT'
            col = box.column()
            row = col.row()
            sub = row.row()
            sub.context_pointer_set('addon_prefs', self)
            op = sub.operator('wm.context_toggle', text='', icon=icon,
                              emboss=False)
            op.data_path = 'addon_prefs.show_expanded_' + mod_name
            sub.label('{}: {}'.format(info['category'], info['name']))
            sub = row.row()
            sub.alignment = 'RIGHT'
            if info.get('warning'):
                sub.label('', icon='ERROR')
            sub.prop(self, 'use_' + mod_name, text='')

            # The second stage
            if expand:
                if info.get('description'):
                    split = col.row().split(percentage=0.15)
                    split.label('Description:')
                    split.label(info['description'])
                if info.get('location'):
                    split = col.row().split(percentage=0.15)
                    split.label('Location:')
                    split.label(info['location'])
                """
                if info.get('author'):
                    split = col.row().split(percentage=0.15)
                    split.label('Author:')
                    split.label(info['author'])
                """
                if info.get('version'):
                    split = col.row().split(percentage=0.15)
                    split.label('Version:')
                    split.label('.'.join(str(x) for x in info['version']),
                                translate=False)
                if info.get('warning'):
                    split = col.row().split(percentage=0.15)
                    split.label('Warning:')
                    split.label('  ' + info['warning'], icon='ERROR')

                tot_row = int(bool(info.get('wiki_url')))
                if tot_row:
                    split = col.row().split(percentage=0.15)
                    split.label(text='Internet:')
                    if info.get('wiki_url'):
                        op = split.operator('wm.url_open',
                                            text='Documentation', icon='HELP')
                        op.url = info.get('wiki_url')
                    for i in range(4 - tot_row):
                        split.separator()

                # Details and settings
                if getattr(self, 'use_' + mod_name):
                    prefs = get_addon_preferences(mod_name)

                    if prefs and hasattr(prefs, 'draw'):
                        box = box.column()
                        prefs.layout = box
                        try:
                            prefs.draw(context)
                        except:
                            import traceback
                            traceback.print_exc()
                            box.label(text='Error (see console)', icon='ERROR')
                        del prefs.layout

        row = layout.row()
        row.label(text="End of 3D Viewport Pie Menus Activations",
                  icon="FILE_PARENT")


for mod in sub_modules:
    info = mod.bl_info
    mod_name = mod.__name__.split('.')[-1]

    def gen_update(mod):
        def update(self, context):
            if getattr(self, 'use_' + mod.__name__.split('.')[-1]):
                if not mod.__addon_enabled__:
                    register_submodule(mod)
            else:
                if mod.__addon_enabled__:
                    unregister_submodule(mod)
        return update

    prop = BoolProperty(
            name=info['name'],
            description=info.get('description', ''),
            update=gen_update(mod),
            )
    setattr(PieToolsPreferences, 'use_' + mod_name, prop)
    prop = BoolProperty()
    setattr(PieToolsPreferences, 'show_expanded_' + mod_name, prop)

classes = (
    PieToolsPreferences,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    prefs = get_addon_preferences()
    for mod in sub_modules:
        if not hasattr(mod, '__addon_enabled__'):
            mod.__addon_enabled__ = False
        name = mod.__name__.split('.')[-1]
        if getattr(prefs, 'use_' + name):
            register_submodule(mod)


def unregister():
    for mod in sub_modules:
        if mod.__addon_enabled__:
            unregister_submodule(mod)

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
