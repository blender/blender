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
from bpy.types import Panel
from rna_prop_ui import PropertyPanel
from bpy_extras.node_utils import find_node_input


class WorldButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.world and context.engine in cls.COMPAT_ENGINES)


class WORLD_PT_context_world(WorldButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data

        if scene:
            layout.template_ID(scene, "world", new="world.new")
        elif world:
            layout.template_ID(space, "pin_id")


class EEVEE_WORLD_PT_mist(WorldButtonsPanel, Panel):
    bl_label = "Mist Pass"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        if context.world and (engine in cls.COMPAT_ENGINES):
            for view_layer in context.scene.view_layers:
                if view_layer.use_pass_mist:
                    return True

        return False

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        world = context.world

        layout.prop(world.mist_settings, "start")
        layout.prop(world.mist_settings, "depth")
        layout.prop(world.mist_settings, "falloff")


class WORLD_PT_custom_props(WorldButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "world"
    _property_type = bpy.types.World


class EEVEE_WORLD_PT_surface(WorldButtonsPanel, Panel):
    bl_label = "Surface"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.world and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.prop(world, "use_nodes", icon='NODETREE')
        layout.separator()

        if world.use_nodes:
            ntree = world.node_tree
            node = ntree.get_output_node('EEVEE')

            if node:
                input = find_node_input(node, 'Surface')
                if input:
                    layout.template_node_view(ntree, node, input)
                else:
                    layout.label(text="Incompatible output node")
            else:
                layout.label(text="No output node")
        else:
            layout.prop(world, "color")


class EEVEE_WORLD_PT_volume(WorldButtonsPanel, Panel):
    bl_label = "Volume"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        world = context.world
        return world and world.use_nodes and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        world = context.world
        ntree = world.node_tree
        node = ntree.get_output_node('EEVEE')

        if node:
            input = find_node_input(node, 'Volume')
            if input:
                layout.template_node_view(ntree, node, input)
            else:
                layout.label(text="Incompatible output node")
        else:
            layout.label(text="No output node")


class WORLD_PT_viewport_display(WorldButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.world

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        world = context.world
        layout.prop(world, "color")


classes = (
    WORLD_PT_context_world,
    EEVEE_WORLD_PT_surface,
    EEVEE_WORLD_PT_volume,
    EEVEE_WORLD_PT_mist,
    WORLD_PT_viewport_display,
    WORLD_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
