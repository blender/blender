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
import os
from bpy.types import Operator

from mathutils import Vector, Matrix


def CLIP_spaces_walk(context, all_screens, tarea, tspace, callback, *args):
    screens = bpy.data.screens if all_screens else [context.screen]

    for screen in screens:
        for area in screen.areas:
            if area.type == tarea:
                for space in area.spaces:
                    if space.type == tspace:
                        callback(space, *args)


def CLIP_set_viewport_background(context, all_screens, clip, clip_user):
    def set_background(space_v3d, clip, user):
        bgpic = None

        for x in space_v3d.background_images:
            if x.source == 'MOVIE_CLIP':
                bgpic = x
                break

        if not bgpic:
            bgpic = space_v3d.background_images.new()

        bgpic.source = 'MOVIE_CLIP'
        bgpic.clip = clip
        bgpic.clip_user.proxy_render_size = user.proxy_render_size
        bgpic.clip_user.use_render_undistorted = True
        bgpic.use_camera_clip = False
        bgpic.view_axis = 'CAMERA'

        space_v3d.show_background_images = True

    CLIP_spaces_walk(context, all_screens, 'VIEW_3D', 'VIEW_3D',
                     set_background, clip, clip_user)


def CLIP_camera_for_clip(context, clip):
    scene = context.scene

    camera = scene.camera

    for ob in scene.objects:
        if ob.type == 'CAMERA':
            for con in ob.constraints:
                if con.type == 'CAMERA_SOLVER':
                    cur_clip = scene.active_clip if con.use_active_clip else con.clip

                    if cur_clip == clip:
                        return ob

    return camera


def CLIP_track_view_selected(sc, track):
    if track.select_anchor:
        return True

    if sc.show_marker_pattern and track.select_pattern:
        return True

    if sc.show_marker_search and track.select_search:
        return True

    return False


def CLIP_default_settings_from_track(clip, track, framenr):
    settings = clip.tracking.settings

    width = clip.size[0]
    height = clip.size[1]

    marker = track.markers.find_frame(framenr, False)
    pattern_bb = marker.pattern_bound_box

    pattern = Vector(pattern_bb[1]) - Vector(pattern_bb[0])
    search = marker.search_max - marker.search_min

    pattern[0] = pattern[0] * width
    pattern[1] = pattern[1] * height

    search[0] = search[0] * width
    search[1] = search[1] * height

    settings.default_correlation_min = track.correlation_min
    settings.default_pattern_size = max(pattern[0], pattern[1])
    settings.default_search_size = max(search[0], search[1])
    settings.default_frames_limit = track.frames_limit
    settings.default_pattern_match = track.pattern_match
    settings.default_margin = track.margin
    settings.default_motion_model = track.motion_model
    settings.use_default_brute = track.use_brute
    settings.use_default_normalization = track.use_normalization
    settings.use_default_mask = track.use_mask
    settings.use_default_red_channel = track.use_red_channel
    settings.use_default_green_channel = track.use_green_channel
    settings.use_default_blue_channel = track.use_blue_channel
    settings.default_weight = track.weight


class CLIP_OT_track_to_empty(Operator):
    """Create an Empty object which will be copying movement of active track"""

    bl_idname = "clip.track_to_empty"
    bl_label = "Link Empty to Track"
    bl_options = {'UNDO', 'REGISTER'}

    def _link_track(self, context, clip, tracking_object, track):
        sc = context.space_data
        constraint = None
        ob = None

        ob = bpy.data.objects.new(name=track.name, object_data=None)
        ob.select = True
        context.scene.objects.link(ob)
        context.scene.objects.active = ob

        for con in ob.constraints:
            if con.type == 'FOLLOW_TRACK':
                constraint = con
                break

        if constraint is None:
            constraint = ob.constraints.new(type='FOLLOW_TRACK')

        constraint.use_active_clip = False
        constraint.clip = sc.clip
        constraint.track = track.name
        constraint.use_3d_position = False
        constraint.object = tracking_object.name
        constraint.camera = CLIP_camera_for_clip(context, clip)

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip
        tracking_object = clip.tracking.objects.active

        for track in tracking_object.tracks:
            if CLIP_track_view_selected(sc, track):
                self._link_track(context, clip, tracking_object, track)

        return {'FINISHED'}


class CLIP_OT_bundles_to_mesh(Operator):
    """Create vertex cloud using coordinates of reconstructed tracks"""

    bl_idname = "clip.bundles_to_mesh"
    bl_label = "3D Markers to Mesh"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        return (sc.type == 'CLIP_EDITOR') and sc.clip

    def execute(self, context):
        from bpy_extras.io_utils import unpack_list

        sc = context.space_data
        clip = sc.clip
        tracking_object = clip.tracking.objects.active

        new_verts = []

        mesh = bpy.data.meshes.new(name="Tracks")
        for track in tracking_object.tracks:
            if track.has_bundle:
                new_verts.append(track.bundle)

        if new_verts:
            mesh.vertices.add(len(new_verts))
            mesh.vertices.foreach_set("co", unpack_list(new_verts))

        ob = bpy.data.objects.new(name="Tracks", object_data=mesh)

        context.scene.objects.link(ob)

        return {'FINISHED'}


class CLIP_OT_delete_proxy(Operator):
    """Delete movie clip proxy files from the hard drive"""

    bl_idname = "clip.delete_proxy"
    bl_label = "Delete Proxy"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        if context.space_data.type != 'CLIP_EDITOR':
            return False

        sc = context.space_data

        return sc.clip

    def invoke(self, context, event):
        wm = context.window_manager

        return wm.invoke_confirm(self, event)

    def _rmproxy(self, abspath):
        import shutil

        if not os.path.exists(abspath):
            return

        if os.path.isdir(abspath):
            shutil.rmtree(abspath)
        else:
            os.remove(abspath)

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip
        if clip.use_proxy_custom_directory:
            proxydir = clip.proxy.directory
        else:
            clipdir = os.path.dirname(clip.filepath)
            proxydir = os.path.join(clipdir, "BL_proxy")

        clipfile = os.path.basename(clip.filepath)
        proxy = os.path.join(proxydir, clipfile)
        absproxy = bpy.path.abspath(proxy)

        # proxy_<quality>[_undostorted]
        for x in (25, 50, 75, 100):
            d = os.path.join(absproxy, "proxy_%d" % x)

            self._rmproxy(d)
            self._rmproxy(d + "_undistorted")
            self._rmproxy(os.path.join(absproxy, "proxy_%d.avi" % x))

        tc = ("free_run.blen_tc",
              "interp_free_run.blen_tc",
              "record_run.blen_tc")

        for x in tc:
            self._rmproxy(os.path.join(absproxy, x))

        # remove proxy per-clip directory
        try:
            os.rmdir(absproxy)
        except OSError:
            pass

        # remove [custom] proxy directory if empty
        try:
            absdir = bpy.path.abspath(proxydir)
            os.rmdir(absdir)
        except OSError:
            pass

        return {'FINISHED'}


class CLIP_OT_set_viewport_background(Operator):
    """Set current movie clip as a camera background in 3D view-port """ \
    """(works only when a 3D view-port is visible)"""

    bl_idname = "clip.set_viewport_background"
    bl_label = "Set as Background"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        if context.space_data.type != 'CLIP_EDITOR':
            return False

        sc = context.space_data

        return sc.clip

    def execute(self, context):
        sc = context.space_data
        CLIP_set_viewport_background(context, False, sc.clip, sc.clip_user)

        return {'FINISHED'}


class CLIP_OT_constraint_to_fcurve(Operator):
    """Create F-Curves for object which will copy \
object's movement caused by this constraint"""

    bl_idname = "clip.constraint_to_fcurve"
    bl_label = "Constraint to F-Curve"
    bl_options = {'UNDO', 'REGISTER'}

    def _bake_object(self, scene, ob):
        con = None
        clip = None
        sfra = None
        efra = None
        frame_current = scene.frame_current
        matrices = []

        # Find constraint which would be converting
        # TODO: several camera solvers and track followers would fail,
        #       but can't think about real work-flow where it'll be useful
        for x in ob.constraints:
            if x.type in {'CAMERA_SOLVER', 'FOLLOW_TRACK', 'OBJECT_SOLVER'}:
                con = x

        if not con:
            self.report({'ERROR'},
                        "Motion Tracking constraint to be converted not found")

            return {'CANCELLED'}

        # Get clip used for parenting
        if con.use_active_clip:
            clip = scene.active_clip
        else:
            clip = con.clip

        if not clip:
            self.report({'ERROR'},
                        "Movie clip to use tracking data from isn't set")

            return {'CANCELLED'}

        if con.type == 'FOLLOW_TRACK' and con.use_3d_position:
            mat = ob.matrix_world.copy()
            ob.constraints.remove(con)
            ob.matrix_world = mat

            return {'FINISHED'}

        # Find start and end frames
        for track in clip.tracking.tracks:
            if sfra is None:
                sfra = track.markers[0].frame
            else:
                sfra = min(sfra, track.markers[0].frame)

            if efra is None:
                efra = track.markers[-1].frame
            else:
                efra = max(efra, track.markers[-1].frame)

        if sfra is None or efra is None:
            return

        # Store object matrices
        for x in range(sfra, efra + 1):
            scene.frame_set(x)
            matrices.append(ob.matrix_world.copy())

        ob.animation_data_create()

        # Apply matrices on object and insert key-frames
        i = 0
        for x in range(sfra, efra + 1):
            scene.frame_set(x)
            ob.matrix_world = matrices[i]

            ob.keyframe_insert("location")

            if ob.rotation_mode == 'QUATERNION':
                ob.keyframe_insert("rotation_quaternion")
            else:
                ob.keyframe_insert("rotation_euler")

            i += 1

        ob.constraints.remove(con)

        scene.frame_set(frame_current)

    def execute(self, context):
        scene = context.scene
        # XXX, should probably use context.selected_editable_objects
        # since selected objects can be from a lib or in hidden layer!
        for ob in scene.objects:
            if ob.select:
                self._bake_object(scene, ob)

        return {'FINISHED'}


class CLIP_OT_setup_tracking_scene(Operator):
    """Prepare scene for compositing 3D objects into this footage"""

    bl_idname = "clip.setup_tracking_scene"
    bl_label = "Setup Tracking Scene"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        if sc.type != 'CLIP_EDITOR':
            return False

        clip = sc.clip

        return clip and clip.tracking.reconstruction.is_valid

    @staticmethod
    def _setupScene(context):
        scene = context.scene
        scene.active_clip = context.space_data.clip

    @staticmethod
    def _setupWorld(context):
        scene = context.scene
        world = scene.world

        if not world:
            world = bpy.data.worlds.new(name="World")
            scene.world = world

        world.light_settings.use_ambient_occlusion = True
        world.light_settings.ao_blend_type = 'MULTIPLY'

        world.light_settings.use_environment_light = True
        world.light_settings.environment_energy = 0.1

        world.light_settings.distance = 1.0
        world.light_settings.sample_method = 'ADAPTIVE_QMC'
        world.light_settings.samples = 7
        world.light_settings.threshold = 0.005

    @staticmethod
    def _findOrCreateCamera(context):
        scene = context.scene

        if scene.camera:
            return scene.camera

        cam = bpy.data.cameras.new(name="Camera")
        camob = bpy.data.objects.new(name="Camera", object_data=cam)
        scene.objects.link(camob)

        scene.camera = camob

        camob.matrix_local = (Matrix.Translation((7.481, -6.508, 5.344)) *
                              Matrix.Rotation(0.815, 4, 'Z') *
                              Matrix.Rotation(0.011, 4, 'Y') *
                              Matrix.Rotation(1.109, 4, 'X'))

        return camob

    @staticmethod
    def _setupCamera(context):
        sc = context.space_data
        clip = sc.clip
        tracking = clip.tracking

        camob = CLIP_OT_setup_tracking_scene._findOrCreateCamera(context)
        cam = camob.data

        # Remove all constraints to be sure motion is fine
        camob.constraints.clear()

        # Append camera solver constraint
        con = camob.constraints.new(type='CAMERA_SOLVER')
        con.use_active_clip = True
        con.influence = 1.0

        cam.sensor_width = tracking.camera.sensor_width
        cam.lens = tracking.camera.focal_length

    @staticmethod
    def _setupViewport(context):
        sc = context.space_data
        CLIP_set_viewport_background(context, True, sc.clip, sc.clip_user)

    @staticmethod
    def _setupRenderLayers(context):
        scene = context.scene
        rlayers = scene.render.layers

        if not scene.render.layers.get("Foreground"):
            if len(rlayers) == 1:
                fg = rlayers[0]
                fg.name = 'Foreground'
            else:
                fg = scene.render.layers.new("Foreground")

            fg.use_sky = False
            fg.layers = [True] + [False] * 19
            fg.layers_zmask = [False] * 10 + [True] + [False] * 9
            fg.use_pass_vector = True

        if not scene.render.layers.get("Background"):
            bg = scene.render.layers.new("Background")
            bg.use_pass_shadow = True
            bg.use_pass_ambient_occlusion = True
            bg.layers = [False] * 10 + [True] + [False] * 9

    @staticmethod
    def _findNode(tree, type):
        for node in tree.nodes:
            if node.type == type:
                return node

        return None

    @staticmethod
    def _findOrCreateNode(tree, type):
        node = CLIP_OT_setup_tracking_scene._findNode(tree, type)

        if not node:
            node = tree.nodes.new(type=type)

        return node

    @staticmethod
    def _needSetupNodes(context):
        scene = context.scene
        tree = scene.node_tree

        if not tree:
            # No compositor node tree found, time to create it!
            return True

        for node in tree.nodes:
            if node.type in {'MOVIECLIP', 'MOVIEDISTORTION'}:
                return False

        return True

    @staticmethod
    def _offsetNodes(tree):
        for a in tree.nodes:
            for b in tree.nodes:
                if a != b and a.location == b.location:
                    b.location += Vector((40.0, 20.0))

    def _setupNodes(self, context):
        if not self._needSetupNodes(context):
            # compositor nodes were already setup or even changes already
            # do nothing to prevent nodes damage
            return

        # Enable backdrop for all compositor spaces
        def setup_space(space):
            space.show_backdrop = True

        CLIP_spaces_walk(context, True, 'NODE_EDITOR', 'NODE_EDITOR',
                         setup_space)

        sc = context.space_data
        scene = context.scene
        scene.use_nodes = True
        tree = scene.node_tree
        clip = sc.clip

        need_stabilization = False

        # create nodes
        rlayer_fg = self._findOrCreateNode(tree, 'CompositorNodeRLayers')
        rlayer_bg = tree.nodes.new(type='CompositorNodeRLayers')
        composite = self._findOrCreateNode(tree, 'CompositorNodeComposite')

        movieclip = tree.nodes.new(type='CompositorNodeMovieClip')
        distortion = tree.nodes.new(type='CompositorNodeMovieDistortion')

        if need_stabilization:
            stabilize = tree.nodes.new(type='CompositorNodeStabilize2D')

        scale = tree.nodes.new(type='CompositorNodeScale')
        invert = tree.nodes.new(type='CompositorNodeInvert')
        add_ao = tree.nodes.new(type='CompositorNodeMixRGB')
        add_shadow = tree.nodes.new(type='CompositorNodeMixRGB')
        mul_shadow = tree.nodes.new(type='CompositorNodeMixRGB')
        mul_image = tree.nodes.new(type='CompositorNodeMixRGB')
        vector_blur = tree.nodes.new(type='CompositorNodeVecBlur')
        alphaover = tree.nodes.new(type='CompositorNodeAlphaOver')
        viewer = tree.nodes.new(type='CompositorNodeViewer')

        # setup nodes
        movieclip.clip = clip

        distortion.clip = clip
        distortion.distortion_type = 'UNDISTORT'

        if need_stabilization:
            stabilize.clip = clip

        scale.space = 'RENDER_SIZE'

        rlayer_bg.scene = scene
        rlayer_bg.layer = "Background"

        rlayer_fg.scene = scene
        rlayer_fg.layer = "Foreground"

        add_ao.blend_type = 'ADD'
        add_ao.show_preview = False
        add_shadow.blend_type = 'ADD'
        add_shadow.show_preview = False

        mul_shadow.blend_type = 'MULTIPLY'
        mul_shadow.inputs["Fac"].default_value = 0.8
        mul_shadow.show_preview = False

        mul_image.blend_type = 'MULTIPLY'
        mul_image.inputs["Fac"].default_value = 0.8
        mul_image.show_preview = False

        vector_blur.factor = 0.75

        # create links
        tree.links.new(movieclip.outputs["Image"], distortion.inputs["Image"])

        if need_stabilization:
            tree.links.new(distortion.outputs["Image"],
                           stabilize.inputs["Image"])
            tree.links.new(stabilize.outputs["Image"], scale.inputs["Image"])
        else:
            tree.links.new(distortion.outputs["Image"], scale.inputs["Image"])

        tree.links.new(rlayer_bg.outputs["Alpha"], invert.inputs["Color"])

        tree.links.new(invert.outputs["Color"], add_shadow.inputs[1])
        tree.links.new(rlayer_bg.outputs["Shadow"], add_shadow.inputs[2])

        tree.links.new(invert.outputs["Color"], add_ao.inputs[1])
        tree.links.new(rlayer_bg.outputs["AO"], add_ao.inputs[2])

        tree.links.new(add_ao.outputs["Image"], mul_shadow.inputs[1])
        tree.links.new(add_shadow.outputs["Image"], mul_shadow.inputs[2])

        tree.links.new(scale.outputs["Image"], mul_image.inputs[1])
        tree.links.new(mul_shadow.outputs["Image"], mul_image.inputs[2])

        tree.links.new(rlayer_fg.outputs["Image"], vector_blur.inputs["Image"])
        tree.links.new(rlayer_fg.outputs["Z"], vector_blur.inputs["Z"])
        tree.links.new(rlayer_fg.outputs["Speed"], vector_blur.inputs["Speed"])

        tree.links.new(mul_image.outputs["Image"], alphaover.inputs[1])
        tree.links.new(vector_blur.outputs["Image"], alphaover.inputs[2])

        tree.links.new(alphaover.outputs["Image"], composite.inputs["Image"])
        tree.links.new(alphaover.outputs["Image"], viewer.inputs["Image"])

        # place nodes
        movieclip.location = Vector((-300.0, 350.0))

        distortion.location = movieclip.location
        distortion.location += Vector((200.0, 0.0))

        if need_stabilization:
            stabilize.location = distortion.location
            stabilize.location += Vector((200.0, 0.0))

            scale.location = stabilize.location
            scale.location += Vector((200.0, 0.0))
        else:
            scale.location = distortion.location
            scale.location += Vector((200.0, 0.0))

        rlayer_bg.location = movieclip.location
        rlayer_bg.location -= Vector((0.0, 350.0))

        invert.location = rlayer_bg.location
        invert.location += Vector((250.0, 50.0))

        add_ao.location = invert.location
        add_ao.location[0] += 200
        add_ao.location[1] = rlayer_bg.location[1]

        add_shadow.location = add_ao.location
        add_shadow.location -= Vector((0.0, 250.0))

        mul_shadow.location = add_ao.location
        mul_shadow.location += Vector((200.0, -50.0))

        mul_image.location = mul_shadow.location
        mul_image.location += Vector((300.0, 200.0))

        rlayer_fg.location = rlayer_bg.location
        rlayer_fg.location -= Vector((0.0, 500.0))

        vector_blur.location[0] = mul_image.location[0]
        vector_blur.location[1] = rlayer_fg.location[1]

        alphaover.location[0] = vector_blur.location[0] + 350
        alphaover.location[1] = \
            (vector_blur.location[1] + mul_image.location[1]) / 2

        composite.location = alphaover.location
        composite.location += Vector((200.0, -100.0))

        viewer.location = composite.location
        composite.location += Vector((0.0, 200.0))

        # ensure no nodes were creates on position of existing node
        self._offsetNodes(tree)

        scene.render.alpha_mode = 'TRANSPARENT'
        if scene.cycles:
            scene.cycles.film_transparent = True

    @staticmethod
    def _createMesh(scene, name, vertices, faces):
        from bpy_extras.io_utils import unpack_list

        mesh = bpy.data.meshes.new(name=name)

        mesh.vertices.add(len(vertices))
        mesh.vertices.foreach_set("co", unpack_list(vertices))

        nbr_loops = len(faces)
        nbr_polys = nbr_loops // 4
        mesh.loops.add(nbr_loops)
        mesh.polygons.add(nbr_polys)

        mesh.polygons.foreach_set("loop_start", range(0, nbr_loops, 4))
        mesh.polygons.foreach_set("loop_total", (4,) * nbr_polys)
        mesh.loops.foreach_set("vertex_index", faces)

        mesh.update()

        ob = bpy.data.objects.new(name=name, object_data=mesh)

        scene.objects.link(ob)

        return ob

    @staticmethod
    def _getPlaneVertices(half_size, z):

        return [(-half_size, -half_size, z),
                (-half_size, half_size, z),
                (half_size, half_size, z),
                (half_size, -half_size, z)]

    def _createGround(self, scene):
        vertices = self._getPlaneVertices(4.0, 0.0)
        faces = [0, 1, 2, 3]

        ob = self._createMesh(scene, "Ground", vertices, faces)
        ob["is_ground"] = True

        return ob

    @staticmethod
    def _findGround(context):
        scene = context.scene

        for ob in scene.objects:
            if ob.type == 'MESH' and "is_ground" in ob:
                return ob

        return None

    @staticmethod
    def _mergeLayers(layers_a, layers_b):

        return [(layers_a[i] | layers_b[i]) for i in range(len(layers_a))]

    @staticmethod
    def _createLamp(scene):
        lamp = bpy.data.lamps.new(name="Lamp", type='POINT')
        lampob = bpy.data.objects.new(name="Lamp", object_data=lamp)
        scene.objects.link(lampob)

        lampob.matrix_local = Matrix.Translation((4.076, 1.005, 5.904))

        lamp.distance = 30
        lamp.shadow_method = 'RAY_SHADOW'

        return lampob

    def _createSampleObject(self, scene):
        vertices = self._getPlaneVertices(1.0, -1.0) + \
            self._getPlaneVertices(1.0, 1.0)
        faces = (0, 1, 2, 3,
                 4, 7, 6, 5,
                 0, 4, 5, 1,
                 1, 5, 6, 2,
                 2, 6, 7, 3,
                 3, 7, 4, 0)

        return self._createMesh(scene, "Cube", vertices, faces)

    def _setupObjects(self, context):
        scene = context.scene

        fg = scene.render.layers.get("Foreground")
        bg = scene.render.layers.get("Background")

        all_layers = self._mergeLayers(fg.layers, bg.layers)

        # ensure all lamps are active on foreground and background
        has_lamp = False
        has_mesh = False
        for ob in scene.objects:
            if ob.type == 'LAMP':
                ob.layers = all_layers
                has_lamp = True
            elif ob.type == 'MESH' and "is_ground" not in ob:
                has_mesh = True

        # create sample lamp if there's no lamps in the scene
        if not has_lamp:
            lamp = self._createLamp(scene)
            lamp.layers = all_layers

        # create sample object if there's no meshes in the scene
        if not has_mesh:
            ob = self._createSampleObject(scene)
            ob.layers = fg.layers

        # create ground object if needed
        ground = self._findGround(context)
        if not ground:
            ground = self._createGround(scene)
            ground.layers = bg.layers
        else:
            # make sure ground is available on Background layer
            ground.layers = self._mergeLayers(ground.layers, bg.layers)

        # layers with background and foreground should be rendered
        scene.layers = self._mergeLayers(scene.layers, all_layers)

    def execute(self, context):
        self._setupScene(context)
        self._setupWorld(context)
        self._setupCamera(context)
        self._setupViewport(context)
        self._setupRenderLayers(context)
        self._setupNodes(context)
        self._setupObjects(context)

        return {'FINISHED'}


class CLIP_OT_track_settings_as_default(Operator):
    """Copy tracking settings from active track to default settings"""

    bl_idname = "clip.track_settings_as_default"
    bl_label = "Track Settings As Default"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        if sc.type != 'CLIP_EDITOR':
            return False

        clip = sc.clip

        return clip and clip.tracking.tracks.active

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip

        track = clip.tracking.tracks.active
        framenr = context.scene.frame_current - clip.frame_start + 1

        CLIP_default_settings_from_track(clip, track, framenr)

        return {'FINISHED'}
