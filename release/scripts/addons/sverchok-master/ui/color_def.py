# -*- coding: utf-8 -*-
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
from bpy.props import StringProperty

from sverchok.menu import make_node_cats
from sverchok.utils.logging import debug
import sverchok

colors_cache = {}

themes = [("default_theme", "Default", "Default"),
          ("nipon_blossom", "Nipon Blossom", "Nipon Blossom")]


default_theme = {
    "Viz": (1, 0.3, 0),
    "Text": (0.5, 0.5, 1),
    "Scene": (0, 0.5, 0.2),
    "Layout": (0.674, 0.242, 0.363),
    "Generator": (0, 0.5, 0.5),
}

nipon_blossom = {
    "Viz": (0.628488, 0.931008, 1.000000),
    "Text": (1.000000, 0.899344, 0.974251),
    "Scene": (0.904933, 1.000000, 0.883421),
    "Layout": (0.602957, 0.674000, 0.564277),
    "Generator": (0.92, 0.92, 0.92),
}


#  self referes to the preferences, SverchokPreferences

def color_callback(self, context):
    theme = self.sv_theme
    theme_dict = globals().get(theme)
    sv_node_colors = [
        ("Viz", "color_viz"),
        ("Text", "color_tex"),
        ("Scene", "color_sce"),
        ("Layout", "color_lay"),
        ("Generator", "color_gen"),
    ]
    # stop theme from auto updating and do one call instead of many
    auto_apply_theme = self.auto_apply_theme
    self.auto_apply_theme = False
    for name, attr in sv_node_colors:
        setattr(self, attr, theme_dict[name])
    self.auto_apply_theme = auto_apply_theme
    if self.auto_apply_theme:
        apply_theme()

def sv_colors_definition():
    addon_name = sverchok.__name__
    addon = bpy.context.user_preferences.addons.get(addon_name)
    debug("got addon")
    if addon:
        prefs = addon.preferences
        sv_node_colors = {
            "Viz": prefs.color_viz,
            "Text": prefs.color_tex,
            "Scene": prefs.color_sce,
            "Layout": prefs.color_lay,
            "Generator": prefs.color_gen,
            }
    else:
        sv_node_colors = default_theme
    sv_node_cats = make_node_cats()
    sv_cats_node = {}
    for ca, no in sv_node_cats.items():
        for n in no:
            try:
                sv_cats_node[n[0]] = sv_node_colors[ca]
            except:
                sv_cats_node[n[0]] = False
    return sv_cats_node

def rebuild_color_cache():
    global colors_cache
    colors_cache = sv_colors_definition()

def get_color(bl_id):
    """
    Get color for bl_id
    """
    if not colors_cache:
        debug("building color cache")
        rebuild_color_cache()
    return colors_cache.get(bl_id)

def sverchok_trees():
    for ng in bpy.data.node_groups:
        if ng.bl_idname == "SverchCustomTreeType":
            yield ng

def apply_theme(ng=None):
    """
    Apply theme colors
    """
    global update_cache
    global partial_update_cache
    if not ng:
        for ng in sverchok_trees():
            apply_theme(ng)
    else:
        for n in filter(lambda n:hasattr(n, "set_color"), ng.nodes):
            n.set_color()


class SverchokApplyTheme(bpy.types.Operator):
    """
    Apply Sverchok themes
    """
    bl_idname = "node.sverchok_apply_theme"
    bl_label = "Sverchok Apply theme"
    bl_options = {'REGISTER', 'UNDO'}

    tree_name = StringProperty()

    def execute(self, context):
        if self.tree_name:
            ng = bpy.data.node_groups.get(self.tree_name)
            if ng:
                apply_theme(ng)
            else:
                return {'CANCELLED'}
        else:
            apply_theme()
        return {'FINISHED'}


def register():
    bpy.utils.register_class(SverchokApplyTheme)

def unregister():
    bpy.utils.unregister_class(SverchokApplyTheme)
