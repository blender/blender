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

# support reloading sub-modules
if "bpy" in locals():
    from importlib import reload
    _modules_loaded[:] = [reload(val) for val in _modules_loaded]
    del reload

_modules = [
    "add_mesh_torus",
    "anim",
    "clip",
    "console",
    "constraint",
    "file",
    "image",
    "mesh",
    "node",
    "object",
    "object_align",
    "object_quick_effects",
    "object_randomize_transform",
    "presets",
    "rigidbody",
    "screen_play_rendered_anim",
    "sequencer",
    "simulation",
    "userpref",
    "uvcalc_follow_active",
    "uvcalc_lightmap",
    "uvcalc_smart_project",
    "vertexpaint_dirt",
    "view3d",
    "gpencil_mesh_bake",
    "wm",
]

import bpy

if bpy.app.build_options.freestyle:
    _modules.append("freestyle")

__import__(name=__name__, fromlist=_modules)
_namespace = globals()
_modules_loaded = [_namespace[name] for name in _modules]
del _namespace


def register():
    from bpy.utils import register_class
    for mod in _modules_loaded:
        for cls in mod.classes:
            register_class(cls)


def unregister():
    from bpy.utils import unregister_class
    for mod in reversed(_modules_loaded):
        for cls in reversed(mod.classes):
            if cls.is_registered:
                unregister_class(cls)
