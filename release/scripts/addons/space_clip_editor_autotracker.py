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

bl_info = {
    "name": "Autotrack",
    "author": "Miika Puustinen, Matti Kaihola, Stephen Leger",
    "version": (0, 1, 1),
    "blender": (2, 78, 0),
    "location": "Movie clip Editor > Tools Panel > Autotrack",
    "description": "Motion Tracking with automatic feature detection.",
    "warning": "",
    "wiki_url": "https://github.com/miikapuustinen/blender_autotracker",
    "category": "Motion Tracking",
    }

import bpy
import bgl
import blf
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        WindowManager,
        )
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        EnumProperty,
        PointerProperty,
        )

# for debug purposes
import time

# set to True to enable debug prints
DEBUG = False


# pass variables just like for the regular prints
def debug_print(*args, **kwargs):
    global DEBUG
    if DEBUG:
        print(*args, **kwargs)


# http://blenderscripting.blogspot.ch/2011/07/bgl-drawing-with-opengl-onto-blender-25.html
class GlDrawOnScreen():
    black = (0.0, 0.0, 0.0, 0.7)
    white = (1.0, 1.0, 1.0, 0.5)
    progress_colour = (0.2, 0.7, 0.2, 0.5)

    def String(self, text, x, y, size, colour):
        ''' my_string : the text we want to print
            pos_x, pos_y : coordinates in integer values
            size : font height.
            colour : used for definining the colour'''
        dpi, font_id = 72, 0   # dirty fast assignment
        bgl.glColor4f(*colour)
        blf.position(font_id, x, y, 0)
        blf.size(font_id, size, dpi)
        blf.draw(font_id, text)

    def _end(self):
        bgl.glEnd()
        bgl.glPopAttrib()
        bgl.glLineWidth(1)
        bgl.glDisable(bgl.GL_BLEND)
        bgl.glColor4f(0.0, 0.0, 0.0, 1.0)

    def _start_line(self, colour, width=2, style=bgl.GL_LINE_STIPPLE):
        bgl.glPushAttrib(bgl.GL_ENABLE_BIT)
        bgl.glLineStipple(1, 0x9999)
        bgl.glEnable(style)
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glColor4f(*colour)
        bgl.glLineWidth(width)
        bgl.glBegin(bgl.GL_LINE_STRIP)

    def Rectangle(self, x0, y0, x1, y1, colour, width=2, style=bgl.GL_LINE):
        self._start_line(colour, width, style)
        bgl.glVertex2i(x0, y0)
        bgl.glVertex2i(x1, y0)
        bgl.glVertex2i(x1, y1)
        bgl.glVertex2i(x0, y1)
        bgl.glVertex2i(x0, y0)
        self._end()

    def Polygon(self, pts, colour):
        bgl.glPushAttrib(bgl.GL_ENABLE_BIT)
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glColor4f(*colour)
        bgl.glBegin(bgl.GL_POLYGON)
        for pt in pts:
            x, y = pt
            bgl.glVertex2f(x, y)
        self._end()

    def ProgressBar(self, x, y, width, height, start, percent):
        x1, y1 = x + width, y + height
        # progress from current point to either start or end
        xs = x + (x1 - x) * float(start)
        if percent > 0:
            # going forward
            xi = xs + (x1 - xs) * float(percent)
        else:
            # going backward
            xi = xs - (x - xs) * float(percent)
        self.Polygon([(xs, y), (xs, y1), (xi, y1), (xi, y)], self.progress_colour)
        self.Rectangle(x, y, x1, y1, self.white, width=1)


def draw_callback(self, context):
    self.gl.ProgressBar(10, 24, 200, 16, self.start, self.progress)
    self.gl.String(str(int(100 * abs(self.progress))) + "% ESC to Stop", 14, 28, 10, self.gl.white)


class OP_Tracking_auto_tracker(Operator):
    bl_idname = "tracking.auto_track"
    bl_label = "AutoTracking"
    bl_description = ("Start Autotracking, Press Esc to Stop \n"
                      "When stopped, the added Track Markers will be kept")

    _timer = None
    _draw_handler = None

    gl = GlDrawOnScreen()
    progress = 0
    limits = 0
    t = 0

    def find_track_start(self, track):
        for m in track.markers:
            if not m.mute:
                return m.frame
        return track.markers[0].frame

    def find_track_end(self, track):
        for m in reversed(track.markers):
            if not m.mute:
                return m.frame
        return track.markers[-1].frame - 1

    def find_track_length(self, track):
        tstart = self.find_track_start(track)
        tend = self.find_track_end(track)
        return tend - tstart

    def show_tracks(self, context):
        clip = context.area.spaces.active.clip
        tracks = clip.tracking.tracks
        for track in tracks:
            track.hide = False

    def get_vars_from_context(self, context):
        scene = context.scene
        props = context.window_manager.autotracker_props
        clip = context.area.spaces.active.clip
        tracks = clip.tracking.tracks
        current_frame = scene.frame_current
        clip_end = clip.frame_start + clip.frame_duration
        clip_start = clip.frame_start
        if props.track_backwards:
            last_frame = min(clip_end, current_frame + props.frame_separation)
        else:
            last_frame = max(clip_start, current_frame - props.frame_separation)
        return scene, props, clip, tracks, current_frame, last_frame

    def delete_tracks(self, to_delete):
        bpy.ops.clip.select_all(action='DESELECT')
        for track in to_delete:
            track.select = True
        bpy.ops.clip.delete_track()

    # DETECT FEATURES
    def auto_features(self, context):
        """
            Detect features
        """
        t = time.time()

        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)

        selected = []
        old = []
        to_delete = []
        width = clip.size[0]
        delete_threshold = float(props.delete_threshold) / 100.0

        bpy.ops.clip.select_all(action='DESELECT')

        # Detect Features
        bpy.ops.clip.detect_features(
                threshold=props.df_threshold,
                min_distance=props.df_distance / 100.0 * width,
                margin=props.df_margin / 100.0 * width,
                placement=props.placement_list
                )

        # filter new and old tracks
        for track in tracks:
            if track.hide or track.lock:
                continue
            marker = track.markers.find_frame(current_frame)
            if marker is not None:
                if (not track.select) and (not marker.mute):
                    old.append(track)
                if track.select:
                    selected.append(track)

        added_tracks = len(selected)

        # Select overlapping new markers
        for track_new in selected:
            marker0 = track_new.markers.find_frame(current_frame)
            for track_old in old:
                marker1 = track_old.markers.find_frame(current_frame)
                distance = (marker1.co - marker0.co).length
                if distance < delete_threshold:
                    to_delete.append(track_new)
                    added_tracks -= 1
                    break

        # Delete Overlapping Markers
        self.delete_tracks(to_delete)
        debug_print("auto_features %.4f seconds, add: %s tracks" % (time.time() - t, added_tracks))

    # AUTOTRACK FRAMES
    def track_frames_backward(self):
        # INVOKE_DEFAULT to show progress and take account of frame_limit
        t = time.time()
        res = bpy.ops.clip.track_markers('INVOKE_DEFAULT', backwards=True, sequence=True)
        debug_print("track_frames_backward %.2f seconds %s" % (time.time() - t, res))

    def track_frames_forward(self):
        # INVOKE_DEFAULT to show progress and take account of frame_limit
        t = time.time()
        res = bpy.ops.clip.track_markers('INVOKE_DEFAULT', backwards=False, sequence=True)
        debug_print("track_frames_forward %.2f seconds %s" % (time.time() - t, res))

    def get_active_tracks(self, context):
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)

        active_tracks = []
        for track in tracks:
            if track.hide or track.lock:
                continue
            if len(track.markers) < 2:
                active_tracks.append(track)
            else:
                marker = track.markers.find_frame(current_frame)
                if (marker is not None) and (not marker.mute):
                    active_tracks.append(track)
        return active_tracks

    def select_active_tracks(self, context):
        t = time.time()
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)
        # Select active trackers for tracking
        bpy.ops.clip.select_all(action='DESELECT')
        selected = self.get_active_tracks(context)
        for track in selected:
            track.select = True
        debug_print("select_active_tracks %.2f seconds,"
                    " selected: %s" % (time.time() - t, len(selected)))
        return selected

    def estimate_motion(self, context, last, frame):
        """
            compute mean pixel motion for current frame
            TODO: use statistic here to make filtering more efficient
            last : last frame number
            frame: current frame number
            return mean pixel distance error
        """
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)
        nbtracks = 0
        distance = 0.0
        for track in tracks:
            if track.hide or track.lock:
                continue
            marker0 = track.markers.find_frame(frame)
            marker1 = track.markers.find_frame(last)
            if marker0 is not None and marker1 is not None:
                d = (marker0.co - marker1.co).length
                # skip fixed tracks
                if d > 0:
                    distance += d
                    nbtracks += 1
        if nbtracks > 0:
            mean = distance / nbtracks
        else:
            # arbitrary set to prevent division by 0 error
            mean = 10

        return mean

    # REMOVE SMALL TRACKS
    def remove_small(self, context):
        t = time.time()
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)
        to_delete = []
        bpy.ops.clip.select_all(action='DESELECT')
        for track in tracks:
            if track.hide or track.lock:
                continue
            if len(track.markers) > 1:
                marker = track.markers.find_frame(current_frame)
                if marker is None and self.find_track_length(track) < props.small_tracks:
                    to_delete.append(track)
        deleted_tracks = len(to_delete)
        self.delete_tracks(to_delete)
        debug_print("remove_small %.4f seconds, %s tracks deleted" % (time.time() - t, deleted_tracks))

    def split_track(self, context, track, split_frame, skip=0):
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)
        if props.track_backwards:
            end = scene.frame_start
            step = -1
        else:
            end = scene.frame_end
            step = 1
        new_track = tracks.new(frame=split_frame)

        for frame in range(split_frame, end, step):
            marker = track.markers.find_frame(frame)
            if marker is None:
                return
            # add new marker on new track for frame
            if abs(frame - split_frame) >= skip:
                new_marker = new_track.markers.find_frame(frame)
                if new_marker is None:
                    new_marker = new_track.markers.insert_frame(frame)
                new_marker.co = marker.co
            # remove marker on track for frame
            if frame == split_frame:
                track.hide = True
            else:
                track.markers.delete_frame(frame)
            marker.mute = True

    # REMOVE JUMPING MARKERS
    def remove_jumping(self, context):

        t = time.time()
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)

        if props.track_backwards:
            step = -1
        else:
            step = 1

        to_split = [None for track in tracks]
        for frame in range(last_frame, current_frame, step):

            last = frame - step

            # mean motion (normalized [0-1]) distance for tracks between last and current frame
            mean = self.estimate_motion(context, last, frame)

            # how much a track is allowed to move
            allowed = mean * props.jump_cut

            for i, track in enumerate(tracks):
                if track.hide or track.lock:
                    continue
                marker0 = track.markers.find_frame(frame)
                marker1 = track.markers.find_frame(last)
                if marker0 is not None and marker1 is not None:
                    distance = (marker0.co - marker1.co).length
                    # Jump Cut threshold
                    if distance > allowed:
                        if to_split[i] is None:
                            to_split[i] = [frame, frame]
                        else:
                            to_split[i][1] = frame

        jumping = 0
        for i, split in enumerate(to_split):
            if split is not None:
                self.split_track(context, tracks[i], split[0], abs(split[0] - split[1]))
                jumping += 1

        debug_print("remove_jumping: %.4f seconds, %s tracks cut." % (time.time() - t, jumping))

    def get_frame_range(self, context):
        """
            get tracking frames range
            use clip limits when clip shorter than scene
            else use scene limits
        """
        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)
        frame_start = max(scene.frame_start, clip.frame_start)
        frame_end = min(scene.frame_end, clip.frame_start + clip.frame_duration)
        frame_duration = frame_end - frame_start
        return frame_start, frame_end, frame_duration

    def modal(self, context, event):

        if event.type in {'ESC'}:
            self.report({'INFO'},
                        "Stopping, up to now added Markers will be kept. Autotracking Finished")
            self.cancel(context)
            return {'FINISHED'}

        scene, props, clip, tracks, current_frame, last_frame = self.get_vars_from_context(context)
        frame_start, frame_end, frame_duration = self.get_frame_range(context)

        if (((not props.track_backwards) and current_frame >= frame_end) or
                 (props.track_backwards and current_frame <= frame_start)):

            self.report({'INFO'},
                        "Reached the end of the Clip. Autotracking Finished")
            self.cancel(context)
            return {'FINISHED'}

        # do not run this modal while tracking operator runs
        # Known issue, you'll have to keep ESC pressed
        if event.type not in {'TIMER'} or context.scene.frame_current != self.next_frame:
            return {'PASS_THROUGH'}

        # prevent own TIMER event while running
        self.stop_timer(context)

        if props.track_backwards:
            self.next_frame = scene.frame_current - props.frame_separation
            total = self.start_frame - frame_start
        else:
            self.next_frame = scene.frame_current + props.frame_separation
            total = frame_end - self.start_frame

        if total > 0:
            self.progress = (current_frame - self.start_frame) / total
        else:
            self.progress = 0

        debug_print("Tracking frame %s" % (scene.frame_current))

        # Remove bad tracks before adding new ones
        self.remove_small(context)
        self.remove_jumping(context)

        # add new tracks
        self.auto_features(context)

        # Select active trackers for tracking
        active_tracks = self.select_active_tracks(context)

        # finish if there is nothing to track
        if len(active_tracks) == 0:
            self.report({'INFO'},
                         "No new tracks created, nothing to track. Autotrack Finished")
            self.cancel(context)
            return {'FINISHED'}

        # setup frame_limit on tracks
        for track in active_tracks:
            track.frames_limit = 0
        active_tracks[0].frames_limit = props.frame_separation

        # Forwards or backwards tracking
        if props.track_backwards:
            self.track_frames_backward()
        else:
            self.track_frames_forward()

        # setup a timer to broadcast a TIMER event to force modal to
        # re-run as fast as possible (not waiting for any mouse or keyboard event)
        self.start_timer(context)

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        scene = context.scene
        frame_start, frame_end, frame_duration = self.get_frame_range(context)

        if scene.frame_current > frame_end:
            scene.frame_current = frame_end
        elif scene.frame_current < frame_start:
            scene.frame_current = frame_start

        self.start_frame = scene.frame_current
        self.start = (scene.frame_current - frame_start) / (frame_duration)
        self.progress = 0

        # keep track of frame at witch we should detect new features and filter tracks
        self.next_frame = scene.frame_current

        # draw progress
        args = (self, context)
        self._draw_handler = bpy.types.SpaceClipEditor.draw_handler_add(
                                                        draw_callback, args,
                                                        'WINDOW', 'POST_PIXEL'
                                                        )
        self.start_timer(context)
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def __init__(self):
        self.t = time.time()

    def __del__(self):
        debug_print("AutoTrack %.2f seconds" % (time.time() - self.t))

    def execute(self, context):
        debug_print("Autotrack execute called")
        return {'FINISHED'}

    def stop_timer(self, context):
        context.window_manager.event_timer_remove(self._timer)

    def start_timer(self, context):
        self._timer = context.window_manager.event_timer_add(time_step=0.1, window=context.window)

    def cancel(self, context):
        self.stop_timer(context)
        self.show_tracks(context)
        bpy.types.SpaceClipEditor.draw_handler_remove(self._draw_handler, 'WINDOW')

    @classmethod
    def poll(cls, context):
        return (context.area.spaces.active.clip is not None)


class AutotrackerSettings(PropertyGroup):
    """Create properties"""
    df_margin = FloatProperty(
            name="Detect Features Margin",
            description="Only consider features from pixels located outside\n"
                        "the defined margin from the clip borders",
            subtype='PERCENTAGE',
            default=5,
            min=0,
            max=100
            )
    df_threshold = FloatProperty(
            name="Detect Features Threshold",
            description="Threshold level to deem a feature being good enough for tracking",
            default=0.3,
            min=0.0,
            max=1.0
            )
    # Note: merge this one with delete_threshold
    df_distance = FloatProperty(
            name="Detect Features Distance",
            description="Minimal acceptable distance between two features",
            subtype='PERCENTAGE',
            default=8,
            min=1,
            max=100
            )
    delete_threshold = FloatProperty(
            name="New Marker Threshold",
            description="Threshold of how close a new features can appear during tracking",
            subtype='PERCENTAGE',
            default=8,
            min=1,
            max=100
            )
    small_tracks = IntProperty(
            name="Minimum track length",
            description="Delete tracks shorter than this number of frames\n"
                        "Note: set to 0 for keeping all tracks",
            default=50,
            min=1,
            max=1000
            )
    frame_separation = IntProperty(
            name="Frame Separation",
            description="How often new features are generated",
            default=5,
            min=1,
            max=100
            )
    jump_cut = FloatProperty(
            name="Jump Cut",
            description="How much distance a marker can travel before it is considered "
                        "to be a bad track and cut.\nA new track wil be added "
                        "(factor relative to mean motion)",
            default=5.0,
            min=0.0,
            max=50.0
            )
    track_backwards = BoolProperty(
            name="AutoTrack Backwards",
            description="Track from the last frame of the selected clip",
            default=False
            )
    # Dropdown menu
    list_items = [
            ("FRAME", "Whole Frame", "", 1),
            ("INSIDE_GPENCIL", "Inside Grease Pencil", "", 2),
            ("OUTSIDE_GPENCIL", "Outside Grease Pencil", "", 3),
            ]
    placement_list = EnumProperty(
            name="Placement",
            description="Feature Placement",
            items=list_items
            )


"""
    NOTE:
    All size properties are in percent of the clip size,
    so presets do not depend on the clip size
"""


class AutotrackerPanel(Panel):
    """Creates a Panel in the Render Layer properties window"""
    bl_label = "Autotrack"
    bl_idname = "autotrack"
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Track"

    @classmethod
    def poll(cls, context):
        return (context.area.spaces.active.clip is not None)

    def draw(self, context):
        layout = self.layout
        wm = context.window_manager

        row = layout.row()
        row.scale_y = 1.5
        row.operator("tracking.auto_track", text="Autotrack!     ", icon='PLAY')

        row = layout.row()
        row.prop(wm.autotracker_props, "track_backwards")

        row = layout.row()
        col = layout.column(align=True)
        col.prop(wm.autotracker_props, "delete_threshold")
        col.prop(wm.autotracker_props, "small_tracks")
        col.prop(wm.autotracker_props, "frame_separation", text="Frame Separation")
        col.prop(wm.autotracker_props, "jump_cut", text="Jump Threshold")

        row = layout.row()
        row.label(text="Detect Features Settings:")
        col = layout.column(align=True)
        col.prop(wm.autotracker_props, "df_margin", text="Margin:")
        col.prop(wm.autotracker_props, "df_distance", text="Distance:")
        col.prop(wm.autotracker_props, "df_threshold", text="Threshold:")

        row = layout.row()
        row.label(text="Feature Placement:")
        col = layout.column(align=True)
        col.prop(wm.autotracker_props, "placement_list", text="")


def register():
    bpy.utils.register_class(AutotrackerSettings)
    WindowManager.autotracker_props = PointerProperty(
                                            type=AutotrackerSettings
                                            )
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_class(AutotrackerSettings)
    bpy.utils.unregister_module(__name__)
    del WindowManager.autotracker_props


if __name__ == "__main__":
    register()
