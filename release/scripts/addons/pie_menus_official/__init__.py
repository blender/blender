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
from bpy.props import (
        BoolProperty,
        PointerProperty,
        )
from bpy.types import (
        PropertyGroup,
        AddonPreferences,
        )


bl_info = {
    "name": "UI Pie Menu Official",
    "author": "Antony Riakiotakis, Sebastian Koenig",
    "version": (1, 1, 5),
    "blender": (2, 7, 7),
    "description": "Individual Pie Menu Activation List",
    "location": "Addons Preferences",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/3D_interaction/Pie_Menu",
    "category": "Pie Menu"
    }


sub_modules_names = (
    "pie_object_modes_of",
    "pie_view_of",
    "pie_shade_of",
    "pie_manipulator_of",
    "pie_pivot_of",
    "pie_snap_of",
    "pie_clip_marker_of",
    )


sub_modules = [__import__(__package__ + "." + submod, {}, {}, submod) for submod in sub_modules_names]
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
                        setattr(UIToolsPreferences, name, prop)
                        bpy.utils.unregister_class(UIToolsPreferences)
                        bpy.utils.register_class(UIToolsPreferences)
        return getattr(addon_prefs, name, None)
    else:
        return addon_prefs


def register_submodule(mod):
    mod.register()
    mod.__addon_enabled__ = True


def unregister_submodule(mod):
    if mod.__addon_enabled__:
        mod.unregister()
        mod.__addon_enabled__ = False

        prefs = get_addon_preferences()
        name = mod.__name__.split('.')[-1]
        if hasattr(UIToolsPreferences, name):
            delattr(UIToolsPreferences, name)
            if prefs:
                bpy.utils.unregister_class(UIToolsPreferences)
                bpy.utils.register_class(UIToolsPreferences)
                if name in prefs:
                    del prefs[name]


class UIToolsPreferences(AddonPreferences):
    bl_idname = __name__

    def draw(self, context):
        layout = self.layout

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
        row.label(text="End of Pie Menu Activations", icon="FILE_PARENT")


for mod in sub_modules:
    info = mod.bl_info
    mod_name = mod.__name__.split('.')[-1]

    def gen_update(mod):
        def update(self, context):
            enabled = getattr(self, 'use_' + mod.__name__.split('.')[-1])
            if enabled:
                register_submodule(mod)
            else:
                unregister_submodule(mod)
            mod.__addon_enabled__ = enabled
        return update

    prop = BoolProperty(
            name=info['name'],
            description=info.get('description', ''),
            update=gen_update(mod),
            default=True,
            )

    setattr(UIToolsPreferences, 'use_' + mod_name, prop)
    prop = BoolProperty()
    setattr(UIToolsPreferences, 'show_expanded_' + mod_name, prop)

classes = (
    UIToolsPreferences,
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
