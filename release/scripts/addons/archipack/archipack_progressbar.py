# -*- coding:utf-8 -*-

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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
# Inspired reportpanel.py by Michel Anders
# ----------------------------------------------------------
import bpy
from bpy.props import FloatProperty, StringProperty
from bpy.types import Scene
from time import time


last_update = 0
info_header_draw = None


def update(self, context):
    global last_update
    if (context.window is not None and
            context.window.screen is not None and
            context.window.screen.areas is not None):
        areas = context.window.screen.areas
        for area in areas:
            if area.type == 'INFO':
                area.tag_redraw()
        if time() - last_update > 0.1:
            bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
            last_update = time()


def register():
    Scene.archipack_progress = FloatProperty(
                                    options={'SKIP_SAVE'},
                                    default=-1,
                                    subtype='PERCENTAGE',
                                    precision=1,
                                    min=-1,
                                    soft_min=0,
                                    soft_max=100,
                                    max=101,
                                    update=update)

    Scene.archipack_progress_text = StringProperty(
                                    options={'SKIP_SAVE'},
                                    default="Progress",
                                    update=update)

    global info_header_draw
    info_header_draw = bpy.types.INFO_HT_header.draw

    def info_draw(self, context):
        global info_header_draw
        info_header_draw(self, context)
        if (context.scene.archipack_progress > -1 and
                context.scene.archipack_progress < 101):
            self.layout.separator()
            text = context.scene.archipack_progress_text
            self.layout.prop(context.scene,
                                "archipack_progress",
                                text=text,
                                slider=True)

    bpy.types.INFO_HT_header.draw = info_draw


def unregister():
    del Scene.archipack_progress
    del Scene.archipack_progress_text
    global info_header_draw
    bpy.types.INFO_HT_header.draw = info_header_draw
