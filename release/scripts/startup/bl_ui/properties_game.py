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


class PhysicsButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"


class PHYSICS_PT_game_physics(PhysicsButtonsPanel, Panel):
    bl_label = "Physics"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        rd = context.scene.render
        return ob and ob.game and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game
        soft = ob.game.soft_body

        layout.prop(game, "physics_type")
        layout.separator()

        physics_type = game.physics_type

        if physics_type in {'DYNAMIC', 'RIGID_BODY'}:
            split = layout.split()

            col = split.column()
            col.prop(game, "use_actor")
            col.prop(game, "use_ghost")
            col.prop(ob, "hide_render", text="Invisible")  # out of place but useful

            col = split.column()
            col.prop(game, "use_material_physics_fh")
            col.prop(game, "use_rotate_from_normal")
            col.prop(game, "use_sleep")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.label(text="Attributes:")
            col.prop(game, "mass")
            col.prop(game, "radius")
            col.prop(game, "form_factor")

            col = split.column()
            sub = col.column()
            sub.prop(game, "use_anisotropic_friction")
            subsub = sub.column()
            subsub.active = game.use_anisotropic_friction
            subsub.prop(game, "friction_coefficients", text="", slider=True)

            split = layout.split()

            col = split.column()
            col.label(text="Velocity:")
            sub = col.column(align=True)
            sub.prop(game, "velocity_min", text="Minimum")
            sub.prop(game, "velocity_max", text="Maximum")

            col = split.column()
            col.label(text="Damping:")
            sub = col.column(align=True)
            sub.prop(game, "damping", text="Translation", slider=True)
            sub.prop(game, "rotation_damping", text="Rotation", slider=True)

            layout.separator()

            split = layout.split()

            col = split.column()
            col.label(text="Lock Translation:")
            col.prop(game, "lock_location_x", text="X")
            col.prop(game, "lock_location_y", text="Y")
            col.prop(game, "lock_location_z", text="Z")

            col = split.column()
            col.label(text="Lock Rotation:")
            col.prop(game, "lock_rotation_x", text="X")
            col.prop(game, "lock_rotation_y", text="Y")
            col.prop(game, "lock_rotation_z", text="Z")

        elif physics_type == 'SOFT_BODY':
            col = layout.column()
            col.prop(game, "use_actor")
            col.prop(game, "use_ghost")
            col.prop(ob, "hide_render", text="Invisible")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.label(text="Attributes:")
            col.prop(game, "mass")
            # disabled in the code
            # col.prop(soft, "weld_threshold")
            col.prop(soft, "location_iterations")
            col.prop(soft, "linear_stiffness", slider=True)
            col.prop(soft, "dynamic_friction", slider=True)
            col.prop(soft, "collision_margin", slider=True)
            col.prop(soft, "use_bending_constraints", text="Bending Constraints")

            col = split.column()
            col.prop(soft, "use_shape_match")
            sub = col.column()
            sub.active = soft.use_shape_match
            sub.prop(soft, "shape_threshold", slider=True)

            col.separator()

            col.label(text="Cluster Collision:")
            col.prop(soft, "use_cluster_rigid_to_softbody")
            col.prop(soft, "use_cluster_soft_to_softbody")
            sub = col.column()
            sub.active = (soft.use_cluster_rigid_to_softbody or soft.use_cluster_soft_to_softbody)
            sub.prop(soft, "cluster_iterations", text="Iterations")

        elif physics_type == 'STATIC':
            col = layout.column()
            col.prop(game, "use_actor")
            col.prop(game, "use_ghost")
            col.prop(ob, "hide_render", text="Invisible")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.label(text="Attributes:")
            col.prop(game, "radius")

            col = split.column()
            sub = col.column()
            sub.prop(game, "use_anisotropic_friction")
            subsub = sub.column()
            subsub.active = game.use_anisotropic_friction
            subsub.prop(game, "friction_coefficients", text="", slider=True)

        elif physics_type in {'SENSOR', 'INVISIBLE', 'NO_COLLISION', 'OCCLUDE'}:
            layout.prop(ob, "hide_render", text="Invisible")

        elif physics_type == 'NAVMESH':
            layout.operator("mesh.navmesh_face_copy")
            layout.operator("mesh.navmesh_face_add")

            layout.separator()

            layout.operator("mesh.navmesh_reset")
            layout.operator("mesh.navmesh_clear")


class PHYSICS_PT_game_collision_bounds(PhysicsButtonsPanel, Panel):
    bl_label = "Collision Bounds"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        game = context.object.game
        rd = context.scene.render
        return (game.physics_type in {'DYNAMIC', 'RIGID_BODY', 'SENSOR', 'SOFT_BODY', 'STATIC'}) and (rd.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        game = context.active_object.game

        self.layout.prop(game, "use_collision_bounds", text="")

    def draw(self, context):
        layout = self.layout

        game = context.active_object.game

        layout.active = game.use_collision_bounds
        layout.prop(game, "collision_bounds_type", text="Bounds")

        row = layout.row()
        row.prop(game, "collision_margin", text="Margin", slider=True)
        row.prop(game, "use_collision_compound", text="Compound")


class PHYSICS_PT_game_obstacles(PhysicsButtonsPanel, Panel):
    bl_label = "Create Obstacle"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        game = context.object.game
        rd = context.scene.render
        return (game.physics_type in {'DYNAMIC', 'RIGID_BODY', 'SENSOR', 'SOFT_BODY', 'STATIC'}) and (rd.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        game = context.active_object.game

        self.layout.prop(game, "use_obstacle_create", text="")

    def draw(self, context):
        layout = self.layout

        game = context.active_object.game

        layout.active = game.use_obstacle_create

        row = layout.row()
        row.prop(game, "obstacle_radius", text="Radius")
        row.label()


class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (rd.engine in cls.COMPAT_ENGINES)


class RENDER_PT_embedded(RenderButtonsPanel, Panel):
    bl_label = "Embedded Player"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        row = layout.row()
        row.operator("view3d.game_start", text="Start")
        row.label()
        row = layout.row()
        row.label(text="Resolution:")
        row = layout.row(align=True)
        row.prop(rd, "resolution_x", slider=False, text="X")
        row.prop(rd, "resolution_y", slider=False, text="Y")


class RENDER_PT_game_player(RenderButtonsPanel, Panel):
    bl_label = "Standalone Player"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        row = layout.row()
        row.operator("wm.blenderplayer_start", text="Start")
        row.label()

        row = layout.row()
        row.label(text="Resolution:")
        row = layout.row(align=True)
        row.prop(gs, "resolution_x", slider=False, text="X")
        row.prop(gs, "resolution_y", slider=False, text="Y")
        row = layout.row()
        col = row.column()
        col.prop(gs, "show_fullscreen")
        col = row.column()
        col.prop(gs, "use_desktop")
        col.active = gs.show_fullscreen

        col = layout.column()
        col.label(text="Quality:")
        col.prop(gs, "samples")
        col = layout.column(align=True)
        col.prop(gs, "depth", text="Bit Depth", slider=False)
        col.prop(gs, "frequency", text="Refresh Rate", slider=False)


class RENDER_PT_game_stereo(RenderButtonsPanel, Panel):
    bl_label = "Stereo"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings
        stereo_mode = gs.stereo

        # stereo options:
        layout.prop(gs, "stereo", expand=True)

        # stereo:
        if stereo_mode == 'STEREO':
            layout.prop(gs, "stereo_mode")
            layout.prop(gs, "stereo_eye_separation")

        # dome:
        elif stereo_mode == 'DOME':
            layout.prop(gs, "dome_mode", text="Dome Type")

            dome_type = gs.dome_mode

            split = layout.split()

            if dome_type in {'FISHEYE', 'TRUNCATED_REAR', 'TRUNCATED_FRONT'}:
                col = split.column()

                col.prop(gs, "dome_buffer_resolution", text="Resolution", slider=True)
                col.prop(gs, "dome_angle", slider=True)

                col = split.column()
                col.prop(gs, "dome_tessellation", text="Tessellation")
                col.prop(gs, "dome_tilt")

            elif dome_type == 'PANORAM_SPH':
                col = split.column()

                col.prop(gs, "dome_buffer_resolution", text="Resolution", slider=True)
                col = split.column()
                col.prop(gs, "dome_tessellation", text="Tessellation")

            else:  # cube map
                col = split.column()

                col.prop(gs, "dome_buffer_resolution", text="Resolution", slider=True)

                col = split.column()

            layout.prop(gs, "dome_text")


class RENDER_PT_game_shading(RenderButtonsPanel, Panel):
    bl_label = "Shading"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.prop(gs, "material_mode", expand=True)

        if gs.material_mode == 'GLSL':
            split = layout.split()

            col = split.column()
            col.prop(gs, "use_glsl_lights", text="Lights")
            col.prop(gs, "use_glsl_shaders", text="Shaders")
            col.prop(gs, "use_glsl_shadows", text="Shadows")
            col.prop(gs, "use_glsl_color_management", text="Color Management")

            col = split.column()
            col.prop(gs, "use_glsl_ramps", text="Ramps")
            col.prop(gs, "use_glsl_nodes", text="Nodes")
            col.prop(gs, "use_glsl_extra_textures", text="Extra Textures")


class RENDER_PT_game_system(RenderButtonsPanel, Panel):
    bl_label = "System"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings
        row = layout.row()
        row.prop(gs, "use_frame_rate")
        row.prop(gs, "restrict_animation_updates")

        row = layout.row()
        row.prop(gs, "use_display_lists")

        row = layout.row()
        row.label("Exit Key")
        row.prop(gs, "exit_key", text="", event=True)


class RENDER_PT_game_display(RenderButtonsPanel, Panel):
    bl_label = "Display"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        row.prop(context.scene.render, "fps", text="Animation Frame Rate", slider=False)

        gs = context.scene.game_settings
        flow = layout.column_flow()
        flow.prop(gs, "show_debug_properties", text="Debug Properties")
        flow.prop(gs, "show_framerate_profile", text="Framerate and Profile")
        flow.prop(gs, "show_physics_visualization", text="Physics Visualization")
        flow.prop(gs, "use_deprecation_warnings")
        flow.prop(gs, "show_mouse", text="Mouse Cursor")

        col = layout.column()
        col.label(text="Framing:")
        col.row().prop(gs, "frame_type", expand=True)
        if gs.frame_type == 'LETTERBOX':
            col.prop(gs, "frame_color", text="")


class SceneButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"


class SCENE_PT_game_navmesh(SceneButtonsPanel, Panel):
    bl_label = "Navigation mesh"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        rd = context.scene.game_settings.recast_data

        layout.operator("mesh.navmesh_make", text='Build navigation mesh')

        col = layout.column()
        col.label(text="Rasterization:")
        row = col.row()
        row.prop(rd, "cell_size")
        row.prop(rd, "cell_height")

        col = layout.column()
        col.label(text="Agent:")
        split = col.split()

        col = split.column()
        col.prop(rd, "agent_height", text="Height")
        col.prop(rd, "agent_radius", text="Radius")

        col = split.column()
        col.prop(rd, "slope_max")
        col.prop(rd, "climb_max")

        col = layout.column()
        col.label(text="Region:")
        row = col.row()
        row.prop(rd, "region_min_size")
        row.prop(rd, "region_merge_size")

        col = layout.column()
        col.label(text="Polygonization:")
        split = col.split()

        col = split.column()
        col.prop(rd, "edge_max_len")
        col.prop(rd, "edge_max_error")

        split.prop(rd, "verts_per_poly")

        col = layout.column()
        col.label(text="Detail Mesh:")
        row = col.row()
        row.prop(rd, "sample_dist")
        row.prop(rd, "sample_max_error")


class RENDER_PT_game_sound(RenderButtonsPanel, Panel):
    bl_label = "Sound"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.prop(scene, "audio_distance_model")

        col = layout.column(align=True)
        col.prop(scene, "audio_doppler_speed", text="Speed")
        col.prop(scene, "audio_doppler_factor")


class WorldButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"


class WORLD_PT_game_context_world(WorldButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.scene) and (rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data

        split = layout.split(percentage=0.65)
        if scene:
            split.template_ID(scene, "world", new="world.new")
        elif world:
            split.template_ID(space, "pin_id")


class WORLD_PT_game_world(WorldButtonsPanel, Panel):
    bl_label = "World"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        world = context.world

        row = layout.row()
        row.column().prop(world, "horizon_color")
        row.column().prop(world, "ambient_color")


class WORLD_PT_game_mist(WorldButtonsPanel, Panel):
    bl_label = "Mist"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.mist_settings, "use_mist", text="")

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.active = world.mist_settings.use_mist
        row = layout.row()
        row.prop(world.mist_settings, "falloff")

        row = layout.row(align=True)
        row.prop(world.mist_settings, "start")
        row.prop(world.mist_settings, "depth")
        row = layout.row()
        row.prop(world.mist_settings, "intensity", text="Minimum Intensity")


class WORLD_PT_game_physics(WorldButtonsPanel, Panel):
    bl_label = "Physics"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.prop(gs, "physics_engine")
        if gs.physics_engine != 'NONE':
            layout.prop(gs, "physics_gravity", text="Gravity")

            split = layout.split()

            col = split.column()
            col.label(text="Physics Steps:")
            sub = col.column(align=True)
            sub.prop(gs, "physics_step_max", text="Max")
            sub.prop(gs, "physics_step_sub", text="Substeps")
            col.prop(gs, "fps", text="FPS")

            col = split.column()
            col.label(text="Logic Steps:")
            col.prop(gs, "logic_step_max", text="Max")

            col = layout.column()
            col.prop(gs, "use_occlusion_culling", text="Occlusion Culling")
            sub = col.column()
            sub.active = gs.use_occlusion_culling
            sub.prop(gs, "occlusion_culling_resolution", text="Resolution")

        else:
            split = layout.split()

            col = split.column()
            col.label(text="Physics Steps:")
            col.prop(gs, "fps", text="FPS")

            col = split.column()
            col.label(text="Logic Steps:")
            col.prop(gs, "logic_step_max", text="Max")


class WORLD_PT_game_physics_obstacles(WorldButtonsPanel, Panel):
    bl_label = "Obstacle simulation"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.prop(gs, "obstacle_simulation", text="Type")
        if gs.obstacle_simulation != 'NONE':
            layout.prop(gs, "level_height")
            layout.prop(gs, "show_obstacle_simulation")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
