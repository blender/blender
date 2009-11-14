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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180

class PhysicsButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll(self, context):
        ob = context.active_object
        rd = context.scene.render_data
        return ob and ob.game and (rd.engine == 'BLENDER_GAME')


class PHYSICS_PT_game_physics(PhysicsButtonsPanel):
    bl_label = "Physics"

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game
        soft = ob.game.soft_body
        col2 = context.region.width > narrowui

        if col2:
            layout.itemR(game, "physics_type")
        else:
            layout.itemR(game, "physics_type", text="")
        layout.itemS()

        #if game.physics_type == 'DYNAMIC':
        if game.physics_type in ('DYNAMIC', 'RIGID_BODY'):
            split = layout.split()

            col = split.column()
            col.itemR(game, "actor")
            col.itemR(game, "ghost")
            col.itemR(ob, "restrict_render", text="Invisible") # out of place but useful

            if col2:
                col = split.column()
            col.itemR(game, "material_physics")
            col.itemR(game, "rotate_from_normal")
            col.itemR(game, "no_sleeping")

            layout.itemS()

            split = layout.split()

            col = split.column()
            col.itemL(text="Attributes:")
            col.itemR(game, "mass")
            col.itemR(game, "radius")
            col.itemR(game, "form_factor")

            if col2:
                col = split.column()
            sub = col.column()
            sub.active = (game.physics_type == 'RIGID_BODY')
            sub.itemR(game, "anisotropic_friction")
            subsub = sub.column()
            subsub.active = game.anisotropic_friction
            subsub.itemR(game, "friction_coefficients", text="", slider=True)

            split = layout.split()

            col = split.column()
            col.itemL(text="Velocity:")
            sub = col.column(align=True)
            sub.itemR(game, "minimum_velocity", text="Minimum")
            sub.itemR(game, "maximum_velocity", text="Maximum")

            if col2:
                col = split.column()
            col.itemL(text="Damping:")
            sub = col.column(align=True)
            sub.itemR(game, "damping", text="Translation", slider=True)
            sub.itemR(game, "rotation_damping", text="Rotation", slider=True)

            layout.itemS()

            split = layout.split()

            col = split.column()
            col.itemL(text="Lock Translation:")
            col.itemR(game, "lock_x_axis", text="X")
            col.itemR(game, "lock_y_axis", text="Y")
            col.itemR(game, "lock_z_axis", text="Z")

            col = split.column()
            col.itemL(text="Lock Rotation:")
            col.itemR(game, "lock_x_rot_axis", text="X")
            col.itemR(game, "lock_y_rot_axis", text="Y")
            col.itemR(game, "lock_z_rot_axis", text="Z")

        elif game.physics_type == 'SOFT_BODY':
            col = layout.column()
            col.itemR(game, "actor")
            col.itemR(game, "ghost")
            col.itemR(ob, "restrict_render", text="Invisible")

            layout.itemS()

            split = layout.split()

            col = split.column()
            col.itemL(text="Attributes:")
            col.itemR(game, "mass")
            col.itemR(soft, "welding")
            col.itemR(soft, "position_iterations")
            col.itemR(soft, "linstiff", slider=True)
            col.itemR(soft, "dynamic_friction", slider=True)
            col.itemR(soft, "margin", slider=True)
            col.itemR(soft, "bending_const", text="Bending Constraints")

            if col2:
                col = split.column()
            col.itemR(soft, "shape_match")
            sub = col.column()
            sub.active = soft.shape_match
            sub.itemR(soft, "threshold", slider=True)

            col.itemS()

            col.itemL(text="Cluster Collision:")
            col.itemR(soft, "cluster_rigid_to_softbody")
            col.itemR(soft, "cluster_soft_to_softbody")
            sub = col.column()
            sub.active = (soft.cluster_rigid_to_softbody or soft.cluster_soft_to_softbody)
            sub.itemR(soft, "cluster_iterations", text="Iterations")

        elif game.physics_type == 'STATIC':
            col = layout.column()
            col.itemR(game, "actor")
            col.itemR(game, "ghost")
            col.itemR(ob, "restrict_render", text="Invisible")

        elif game.physics_type in ('SENSOR', 'INVISIBLE', 'NO_COLLISION', 'OCCLUDE'):
            layout.itemR(ob, "restrict_render", text="Invisible")


class PHYSICS_PT_game_collision_bounds(PhysicsButtonsPanel):
    bl_label = "Collision Bounds"

    def poll(self, context):
        game = context.object.game
        rd = context.scene.render_data
        return (game.physics_type in ('DYNAMIC', 'RIGID_BODY', 'SENSOR', 'SOFT_BODY', 'STATIC')) and (rd.engine == 'BLENDER_GAME')

    def draw_header(self, context):
        game = context.active_object.game

        self.layout.itemR(game, "use_collision_bounds", text="")

    def draw(self, context):
        layout = self.layout

        game = context.active_object.game
        col2 = context.region.width > narrowui

        layout.active = game.use_collision_bounds
        if col2:
            layout.itemR(game, "collision_bounds", text="Bounds")
        else:
            layout.itemR(game, "collision_bounds", text="")

        split = layout.split()
        
        col = split.column()
        col.itemR(game, "collision_margin", text="Margin", slider=True)
        
        if col2:
            col = split.column()
        col.itemR(game, "collision_compound", text="Compound")
        

bpy.types.register(PHYSICS_PT_game_physics)
bpy.types.register(PHYSICS_PT_game_collision_bounds)


class RenderButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"

    def poll(self, context):
        rd = context.scene.render_data
        return (rd.engine == 'BLENDER_GAME')


class RENDER_PT_game(RenderButtonsPanel):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        row.itemO("view3d.game_start", text="Start")
        row.itemL()


class RENDER_PT_game_player(RenderButtonsPanel):
    bl_label = "Standalone Player"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data
        col2 = context.region.width > narrowui

        layout.itemR(gs, "fullscreen")

        split = layout.split()

        col = split.column()
        col.itemL(text="Resolution:")
        sub = col.column(align=True)
        sub.itemR(gs, "resolution_x", slider=False, text="X")
        sub.itemR(gs, "resolution_y", slider=False, text="Y")

        if col2:
            col = split.column()
        col.itemL(text="Quality:")
        sub = col.column(align=True)
        sub.itemR(gs, "depth", text="Bit Depth", slider=False)
        sub.itemR(gs, "frequency", text="FPS", slider=False)

        # framing:
        col = layout.column()
        col.itemL(text="Framing:")
        if col2:
            col.row().itemR(gs, "framing_type", expand=True)
        else:
            col.itemR(gs, "framing_type", text="")
        if gs.framing_type == 'LETTERBOX':
            col.itemR(gs, "framing_color", text="")


class RENDER_PT_game_stereo(RenderButtonsPanel):
    bl_label = "Stereo"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data
        stereo_mode = gs.stereo
        col2 = context.region.width > narrowui

        # stereo options:
        layout.itemR(gs, "stereo", expand=True)

        # stereo:
        if stereo_mode == 'STEREO':
            layout.itemR(gs, "stereo_mode")
            layout.itemL(text="To do: Focal Length")
            layout.itemL(text="To do: Eye Separation")

        # dome:
        elif stereo_mode == 'DOME':
            if col2:
                layout.itemR(gs, "dome_mode", text="Dome Type")
            else:
                layout.itemR(gs, "dome_mode", text="")

            dome_type = gs.dome_mode

            split = layout.split()

            if dome_type == 'FISHEYE' or \
               dome_type == 'TRUNCATED_REAR' or \
               dome_type == 'TRUNCATED_FRONT':

                col = split.column()
                col.itemR(gs, "dome_buffer_resolution", text="Resolution", slider=True)
                col.itemR(gs, "dome_angle", slider=True)

                if col2:
                    col = split.column()
                col.itemR(gs, "dome_tesselation", text="Tesselation")
                col.itemR(gs, "dome_tilt")

            elif dome_type == 'PANORAM_SPH':
                col = split.column()
                
                col.itemR(gs, "dome_buffer_resolution", text="Resolution", slider=True)
                if col2:
                    col = split.column()
                col.itemR(gs, "dome_tesselation", text="Tesselation")

            else: # cube map
                col = split.column()
                col.itemR(gs, "dome_buffer_resolution", text="Resolution", slider=True)
                if col2:
                    col = split.column()

            layout.itemR(gs, "dome_text")


class RENDER_PT_game_shading(RenderButtonsPanel):
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data
        col2 = context.region.width > narrowui
        
        if col2:
            layout.itemR(gs, "material_mode", expand=True)
        else:
            layout.itemR(gs, "material_mode", text="")

        if gs.material_mode == 'GLSL':
            split = layout.split()

            col = split.column()
            col.itemR(gs, "glsl_lights", text="Lights")
            col.itemR(gs, "glsl_shaders", text="Shaders")
            col.itemR(gs, "glsl_shadows", text="Shadows")

            col = split.column()
            col.itemR(gs, "glsl_ramps", text="Ramps")
            col.itemR(gs, "glsl_nodes", text="Nodes")
            col.itemR(gs, "glsl_extra_textures", text="Extra Textures")


class RENDER_PT_game_performance(RenderButtonsPanel):
    bl_label = "Performance"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data
        col2 = context.region.width > narrowui
        
        split = layout.split()

        col = split.column()
        col.itemL(text="Show:")
        col.itemR(gs, "show_debug_properties", text="Debug Properties")
        col.itemR(gs, "show_framerate_profile", text="Framerate and Profile")
        col.itemR(gs, "show_physics_visualization", text="Physics Visualization")
        col.itemR(gs, "deprecation_warnings")

        if col2:
            col = split.column()
        col.itemL(text="Render:")
        col.itemR(gs, "all_frames")
        col.itemR(gs, "display_lists")


class RENDER_PT_game_sound(RenderButtonsPanel):
    bl_label = "Sound"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        col2 = context.region.width > narrowui

        if col2:
            layout.itemR(scene, "distance_model")
        else:
            layout.itemR(scene, "distance_model", text="")
        layout.itemR(scene, "speed_of_sound", text="Speed")
        layout.itemR(scene, "doppler_factor")

bpy.types.register(RENDER_PT_game)
bpy.types.register(RENDER_PT_game_player)
bpy.types.register(RENDER_PT_game_stereo)
bpy.types.register(RENDER_PT_game_shading)
bpy.types.register(RENDER_PT_game_performance)
bpy.types.register(RENDER_PT_game_sound)


class WorldButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"

    def poll(self, context):
        rd = context.scene.render_data
        return (rd.engine == 'BLENDER_GAME')


class WORLD_PT_game_context_world(WorldButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def poll(self, context):
        rd = context.scene.render_data
        return (context.scene) and (rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data
        col2 = context.region.width > narrowui

        if col2:
            split = layout.split(percentage=0.65)
            if scene:
                split.template_ID(scene, "world", new="world.new")
            elif world:
                split.template_ID(space, "pin_id")
        else:   
            if scene:
                layout.template_ID(scene, "world", new="world.new")
            elif world:
                layout.template_ID(space, "pin_id")

class WORLD_PT_game_world(WorldButtonsPanel):
    bl_label = "World"

    def draw(self, context):
        layout = self.layout

        world = context.world
        col2 = context.region.width > narrowui

        split = layout.split()
        
        col = split.column()
        col.itemR(world, "horizon_color")
        
        if col2:
            col = split.column()
        col.itemR(world, "ambient_color")

class WORLD_PT_game_mist(WorldButtonsPanel):
    bl_label = "Mist"
    
    def draw_header(self, context):
        world = context.world
        
        self.layout.itemR(world.mist, "enabled", text="")
    
    def draw(self, context):
        layout = self.layout

        world = context.world
        col2 = context.region.width > narrowui
        
        layout.active = world.mist.enabled
        split = layout.split()
        
        col = split.column()
        col.itemR(world.mist, "start")
        
        if col2:
            col = split.column()
        col.itemR(world.mist, "depth")

class WORLD_PT_game_physics(WorldButtonsPanel):
    bl_label = "Physics"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data
        col2 = context.region.width > narrowui

        layout.itemR(gs, "physics_engine")
        if gs.physics_engine != 'NONE':
            layout.itemR(gs, "physics_gravity", text="Gravity")

            split = layout.split()

            col = split.column()
            col.itemL(text="Physics Steps:")
            sub = col.column(align=True)
            sub.itemR(gs, "physics_step_max", text="Max")
            sub.itemR(gs, "physics_step_sub", text="Substeps")
            col.itemR(gs, "fps", text="FPS")

            if col2:
                col = split.column()
            col.itemL(text="Logic Steps:")
            col.itemR(gs, "logic_step_max", text="Max")

            col = layout.column()
            col.itemR(gs, "use_occlusion_culling", text="Occlusion Culling")
            sub = col.column()
            sub.active = gs.use_occlusion_culling
            sub.itemR(gs, "occlusion_culling_resolution", text="Resolution")

        else:
            split = layout.split()

            col = split.column()
            col.itemL(text="Physics Steps:")
            col.itemR(gs, "fps", text="FPS")

            col = split.column()
            col.itemL(text="Logic Steps:")
            col.itemR(gs, "logic_step_max", text="Max")

bpy.types.register(WORLD_PT_game_context_world)
bpy.types.register(WORLD_PT_game_world)
bpy.types.register(WORLD_PT_game_mist)
bpy.types.register(WORLD_PT_game_physics)