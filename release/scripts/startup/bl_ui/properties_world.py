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
from bpy_extras.node_utils import find_node_input, find_output_node


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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data

        split = layout.split(percentage=0.85)
        if scene:
            split.template_ID(scene, "world", new="world.new")
        elif world:
            split.template_ID(space, "pin_id")


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

        world = context.world

        split = layout.split(align=True)
        split.prop(world.mist_settings, "start")
        split.prop(world.mist_settings, "depth")

        layout.prop(world.mist_settings, "falloff")


class WORLD_PT_custom_props(WorldButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}
    _context_path = "world"
    _property_type = bpy.types.World


class EEVEE_WORLD_PT_surface(WorldButtonsPanel, Panel):
    bl_label = "Surface"
    bl_context = "world"
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
            node = find_output_node(ntree, ('OUTPUT_WORLD',))

            if node:
                input = find_node_input(node, 'Surface')
                if input:
                    layout.template_node_view(ntree, node, input)
                else:
                    layout.label(text="Incompatible output node")
            else:
                layout.label(text="No output node")
        else:
            layout.prop(world, "horizon_color", text="Color")


classes = (
    WORLD_PT_context_world,
    WORLD_PT_custom_props,
    EEVEE_WORLD_PT_surface,
    EEVEE_WORLD_PT_mist,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
