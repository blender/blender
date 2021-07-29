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
# support routines for render measures in final image
# Author: Antonio Vazquez (antonioya)
# Archipack adaptation by : Stephen Leger (s-leger)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
import bgl
from os import path, remove, listdir
from sys import exc_info
import subprocess
# noinspection PyUnresolvedReferences
import bpy_extras.image_utils as img_utils
# noinspection PyUnresolvedReferences
from math import ceil
from bpy.types import Operator


class ARCHIPACK_OT_render_thumbs(Operator):
    bl_idname = "archipack.render_thumbs"
    bl_label = "Render preset thumbs"
    bl_description = "Render all presets thumbs"
    bl_options = {'REGISTER', 'INTERNAL'}

    def background_render(self, context, cls, preset):
        generator = path.dirname(path.realpath(__file__)) + path.sep + "archipack_thumbs.py"
        addon_name = __name__.split('.')[0]
        matlib_path = context.user_preferences.addons[addon_name].preferences.matlib_path
        # Run external instance of blender like the original thumbnail generator.
        cmd = [
            bpy.app.binary_path,
            "--background",
            "--factory-startup",
            "-noaudio",
            "--python", generator,
            "--",
            "addon:" + addon_name,
            "matlib:" + matlib_path,
            "cls:" + cls,
            "preset:" + preset
            ]

        popen = subprocess.Popen(cmd, stdout=subprocess.PIPE, universal_newlines=True)
        for stdout_line in iter(popen.stdout.readline, ""):
            yield stdout_line
        popen.stdout.close()
        popen.wait()

    def scan_files(self, category):
        file_list = []
        # load default presets
        dir_path = path.dirname(path.realpath(__file__))
        sub_path = "presets" + path.sep + category
        presets_path = path.join(dir_path, sub_path)
        if path.exists(presets_path):
            file_list += [presets_path + path.sep + f[:-3]
                for f in listdir(presets_path)
                if f.endswith('.py') and
                not f.startswith('.')]
        # load user def presets
        preset_paths = bpy.utils.script_paths("presets")
        for preset in preset_paths:
            presets_path = path.join(preset, category)
            if path.exists(presets_path):
                file_list += [presets_path + path.sep + f[:-3]
                    for f in listdir(presets_path)
                    if f.endswith('.py') and
                    not f.startswith('.')]

        file_list.sort()
        return file_list

    def rebuild_thumbs(self, context):
        file_list = []
        dir_path = path.dirname(path.realpath(__file__))
        sub_path = "presets"
        presets_path = path.join(dir_path, sub_path)
        print(presets_path)
        if path.exists(presets_path):
            dirs = listdir(presets_path)
            for dir in dirs:
                abs_dir = path.join(presets_path, dir)
                if path.isdir(abs_dir):
                    files = self.scan_files(dir)
                    file_list.extend([(dir, file) for file in files])

        ttl = len(file_list)
        for i, preset in enumerate(file_list):
            dir, file = preset
            cls = dir[10:]
            context.scene.archipack_progress = (100 * i / ttl)
            for l in self.background_render(context, cls, file + ".py"):
                if "[log]" in l:
                    print(l[5:].strip())
                # elif not "Fra:1" in l:
                #    print(l.strip())

    @classmethod
    def poll(cls, context):
        return context.scene.archipack_progress < 0

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        context.scene.archipack_progress_text = 'Generating thumbs'
        context.scene.archipack_progress = 0
        self.rebuild_thumbs(context)
        context.scene.archipack_progress = -1
        return {'FINISHED'}


# -------------------------------------------------------------
# Defines button for render
#
# -------------------------------------------------------------
class ARCHIPACK_OT_render(Operator):
    bl_idname = "archipack.render"
    bl_label = "Render"
    bl_category = 'Archipack'
    bl_description = "Create a render image with measures. Use UV/Image editor to view image generated"
    bl_category = 'Archipack'

    # --------------------------------------------------------------------
    # Get the final render image and return as image object
    #
    # return None if no render available
    # --------------------------------------------------------------------

    def get_render_image(self, outpath):
        saved = False
        # noinspection PyBroadException
        try:
            # noinspection PyBroadException
            try:
                result = bpy.data.images['Render Result']
                if result.has_data is False:
                    # this save produce to fill data image
                    result.save_render(outpath)
                    saved = True
            except:
                print("No render image found")
                return None

            # Save and reload
            if saved is False:
                result.save_render(outpath)

            img = img_utils.load_image(outpath)

            return img
        except:
            print("Unexpected render image error")
            return None

    # -------------------------------------
    # Save image to file
    # -------------------------------------

    def save_image(self, filepath, myimage):
        # noinspection PyBroadException
        try:

            # Save old info
            settings = bpy.context.scene.render.image_settings
            myformat = settings.file_format
            mode = settings.color_mode
            depth = settings.color_depth

            # Apply new info and save
            settings.file_format = 'PNG'
            settings.color_mode = "RGBA"
            settings.color_depth = '8'
            myimage.save_render(filepath)
            print("Archipack: Image " + filepath + " saved")

            # Restore old info
            settings.file_format = myformat
            settings.color_mode = mode
            settings.color_depth = depth
        except:
            print("Unexpected error:" + str(exc_info()))
            self.report({'ERROR'}, "Archipack: Unable to save render image")
            return

    # -------------------------------------------------------------
    # Render image main entry point
    #
    # -------------------------------------------------------------

    def render_main(self, context, objlist, animation=False):
        # noinspection PyBroadException,PyBroadException
        # Save old info
        scene = context.scene
        render = scene.render
        settings = render.image_settings
        depth = settings.color_depth
        settings.color_depth = '8'
        # noinspection PyBroadException
        try:

            # Get visible layers
            layers = []
            for x in range(0, 20):
                if scene.layers[x] is True:
                    layers.extend([x])

            # --------------------
            # Get resolution
            # --------------------
            render_scale = render.resolution_percentage / 100

            width = int(render.resolution_x * render_scale)
            height = int(render.resolution_y * render_scale)
            # ---------------------------------------
            # Get output path
            # ---------------------------------------
            temp_path = path.realpath(bpy.app.tempdir)
            if len(temp_path) > 0:
                outpath = path.join(temp_path, "archipack_tmp_render.png")
            else:
                self.report({'ERROR'},
                            "Archipack: Unable to save temporary render image. Define a valid temp path")
                settings.color_depth = depth
                return False

            # Get Render Image
            img = self.get_render_image(outpath)
            if img is None:
                self.report({'ERROR'},
                            "Archipack: Unable to save temporary render image. Define a valid temp path")
                settings.color_depth = depth
                return False

            # -----------------------------
            # Calculate rows and columns
            # -----------------------------
            tile_x = 240
            tile_y = 216
            row_num = ceil(height / tile_y)
            col_num = ceil(width / tile_x)
            print("Archipack: Image divided in " + str(row_num) + "x" + str(col_num) + " tiles")

            # pixels out of visible area
            cut4 = (col_num * tile_x * 4) - width * 4  # pixels aout of drawing area
            totpixel4 = width * height * 4  # total pixels RGBA

            viewport_info = bgl.Buffer(bgl.GL_INT, 4)
            bgl.glGetIntegerv(bgl.GL_VIEWPORT, viewport_info)

            # Load image on memory
            img.gl_load(0, bgl.GL_NEAREST, bgl.GL_NEAREST)

            # 2.77 API change
            if bpy.app.version >= (2, 77, 0):
                tex = img.bindcode[0]
            else:
                tex = img.bindcode

            # --------------------------------------------
            # Create output image (to apply texture)
            # --------------------------------------------
            if "archipack_output" in bpy.data.images:
                out_img = bpy.data.images["archipack_output"]
                if out_img is not None:
                    # out_img.user_clear()
                    bpy.data.images.remove(out_img, do_unlink=True)

            out = bpy.data.images.new("archipack_output", width, height)
            tmp_pixels = [1] * totpixel4

            # --------------------------------
            # Loop for all tiles
            # --------------------------------
            for row in range(0, row_num):
                for col in range(0, col_num):
                    buffer = bgl.Buffer(bgl.GL_FLOAT, width * height * 4)
                    bgl.glDisable(bgl.GL_SCISSOR_TEST)  # if remove this line, get blender screenshot not image
                    bgl.glViewport(0, 0, tile_x, tile_y)

                    bgl.glMatrixMode(bgl.GL_PROJECTION)
                    bgl.glLoadIdentity()

                    # defines ortographic view for single tile
                    x1 = tile_x * col
                    y1 = tile_y * row
                    bgl.gluOrtho2D(x1, x1 + tile_x, y1, y1 + tile_y)

                    # Clear
                    bgl.glClearColor(0.0, 0.0, 0.0, 0.0)
                    bgl.glClear(bgl.GL_COLOR_BUFFER_BIT | bgl.GL_DEPTH_BUFFER_BIT)

                    bgl.glEnable(bgl.GL_TEXTURE_2D)
                    bgl.glBindTexture(bgl.GL_TEXTURE_2D, tex)

                    # defines drawing area
                    bgl.glBegin(bgl.GL_QUADS)

                    bgl.glColor3f(1.0, 1.0, 1.0)
                    bgl.glTexCoord2f(0.0, 0.0)
                    bgl.glVertex2f(0.0, 0.0)

                    bgl.glTexCoord2f(1.0, 0.0)
                    bgl.glVertex2f(width, 0.0)

                    bgl.glTexCoord2f(1.0, 1.0)
                    bgl.glVertex2f(width, height)

                    bgl.glTexCoord2f(0.0, 1.0)
                    bgl.glVertex2f(0.0, height)

                    bgl.glEnd()

                    # -----------------------------
                    # Loop to draw all lines
                    # -----------------------------
                    for o, d in objlist:
                        if o.hide is False:
                            # verify visible layer
                            for x in range(0, 20):
                                if o.layers[x] is True:
                                    if x in layers:
                                        context.scene.objects.active = o
                                        # print("%s: %s" % (o.name, d.manip_stack))
                                        manipulators = d.manip_stack
                                        if manipulators is not None:
                                            for m in manipulators:
                                                if m is not None:
                                                    m.draw_callback(m, context, render=True)
                                    break

                    # -----------------------------
                    # Loop to draw all debug
                    # -----------------------------
                    """
                    if scene.archipack_debug is True:
                        selobj = bpy.context.selected_objects
                        for myobj in selobj:
                            if scene.archipack_debug_vertices is True:
                                draw_vertices(context, myobj, None, None)
                            if scene.archipack_debug_faces is True or scene.archipack_debug_normals is True:
                                draw_faces(context, myobj, None, None)
                    """
                    """
                    if scene.archipack_rf is True:
                        bgl.glColor3f(1.0, 1.0, 1.0)
                        rfcolor = scene.archipack_rf_color
                        rfborder = scene.archipack_rf_border
                        rfline = scene.archipack_rf_line

                        bgl.glLineWidth(rfline)
                        bgl.glColor4f(rfcolor[0], rfcolor[1], rfcolor[2], rfcolor[3])

                        x1 = rfborder
                        x2 = width - rfborder
                        y1 = int(ceil(rfborder / (width / height)))
                        y2 = height - y1
                        draw_rectangle((x1, y1), (x2, y2))
                    """
                    # --------------------------------
                    # copy pixels to temporary area
                    # --------------------------------
                    bgl.glFinish()
                    bgl.glReadPixels(0, 0, width, height, bgl.GL_RGBA, bgl.GL_FLOAT, buffer)  # read image data
                    for y in range(0, tile_y):
                        # final image pixels position
                        p1 = (y * width * 4) + (row * tile_y * width * 4) + (col * tile_x * 4)
                        p2 = p1 + (tile_x * 4)
                        # buffer pixels position
                        b1 = y * width * 4
                        b2 = b1 + (tile_x * 4)

                        if p1 < totpixel4:  # avoid pixel row out of area
                            if col == col_num - 1:  # avoid pixel columns out of area
                                p2 -= cut4
                                b2 -= cut4

                            tmp_pixels[p1:p2] = buffer[b1:b2]

            # -----------------------
            # Copy temporary to final
            # -----------------------
            out.pixels = tmp_pixels[:]  # Assign image data
            img.gl_free()  # free opengl image memory

            # delete image
            # img.user_clear()
            bpy.data.images.remove(img, do_unlink=True)
            # remove temp file
            remove(outpath)
            # reset
            bgl.glEnable(bgl.GL_SCISSOR_TEST)
            # -----------------------
            # restore opengl defaults
            # -----------------------
            bgl.glLineWidth(1)
            bgl.glDisable(bgl.GL_BLEND)
            bgl.glColor4f(0.0, 0.0, 0.0, 1.0)
            # Saves image
            if out is not None:
                # and (scene.archipack_render is True or animation is True):
                ren_path = bpy.context.scene.render.filepath
                filename = "ap_frame"
                if len(ren_path) > 0:
                    if ren_path.endswith(path.sep):
                        initpath = path.realpath(ren_path) + path.sep
                    else:
                        (initpath, filename) = path.split(ren_path)

                ftxt = "%04d" % scene.frame_current
                outpath = path.realpath(path.join(initpath, filename + ftxt + ".png"))

                self.save_image(outpath, out)

            settings.color_depth = depth
            return True

        except:
            settings.color_depth = depth
            print("Unexpected error:" + str(exc_info()))
            self.report(
                {'ERROR'},
                "Archipack: Unable to create render image. Be sure the output render path is correct"
                )
            return False

    def get_objlist(self, context):
        """
            Get objects with gl manipulators
        """
        objlist = []
        for o in context.scene.objects:
            if o.data is not None:
                d = None
                if 'archipack_window' in o.data:
                    d = o.data.archipack_window[0]
                elif 'archipack_door' in o.data:
                    d = o.data.archipack_door[0]
                elif 'archipack_wall2' in o.data:
                    d = o.data.archipack_wall2[0]
                elif 'archipack_stair' in o.data:
                    d = o.data.archipack_stair[0]
                elif 'archipack_fence' in o.data:
                    d = o.data.archipack_fence[0]
                elif 'archipack_floor' in o.data:
                    d = o.data.archipack_floor[0]
                elif 'archipack_roof' in o.data:
                    d = o.data.archipack_roof[0]
                if d is not None:
                    objlist.append((o, d))
        return objlist

    def draw_gl(self, context):
        objlist = self.get_objlist(context)
        for o, d in objlist:
            context.scene.objects.active = o
            d.manipulable_disable(context)
            d.manipulable_invoke(context)
        return objlist

    def hide_gl(self, context, objlist):
        for o, d in objlist:
            context.scene.objects.active = o
            d.manipulable_disable(context)

    # ------------------------------
    # Execute button action
    # ------------------------------
    # noinspection PyMethodMayBeStatic,PyUnusedLocal
    def execute(self, context):
        scene = context.scene
        wm = context.window_manager
        msg = "New image created with measures. Open it in UV/image editor"
        camera_msg = "Unable to render. No camera found"

        # -----------------------------
        # Check camera
        # -----------------------------
        if scene.camera is None:
            self.report({'ERROR'}, camera_msg)
            return {'FINISHED'}

        objlist = self.draw_gl(context)

        # -----------------------------
        # Use current rendered image
        # -----------------------------
        if wm.archipack.render_type == "1":
            # noinspection PyBroadException
            try:
                result = bpy.data.images['Render Result']
                if result.has_data is False:
                    bpy.ops.render.render()
            except:
                bpy.ops.render.render()

            print("Archipack: Using current render image on buffer")
            if self.render_main(context, objlist) is True:
                self.report({'INFO'}, msg)

        # -----------------------------
        # OpenGL image
        # -----------------------------
        elif wm.archipack.render_type == "2":
            self.set_camera_view()
            self.set_only_render(True)

            print("Archipack: Rendering opengl image")
            bpy.ops.render.opengl()
            if self.render_main(context, objlist) is True:
                self.report({'INFO'}, msg)

            self.set_only_render(False)

        # -----------------------------
        # OpenGL Animation
        # -----------------------------
        elif wm.archipack.render_type == "3":
            oldframe = scene.frame_current
            self.set_camera_view()
            self.set_only_render(True)
            flag = False
            # loop frames
            for frm in range(scene.frame_start, scene.frame_end + 1):
                scene.frame_set(frm)
                print("Archipack: Rendering opengl frame %04d" % frm)
                bpy.ops.render.opengl()
                flag = self.render_main(context, objlist, True)
                if flag is False:
                    break

            self.set_only_render(False)
            scene.frame_current = oldframe
            if flag is True:
                self.report({'INFO'}, msg)

        # -----------------------------
        # Image
        # -----------------------------
        elif wm.archipack.render_type == "4":
            print("Archipack: Rendering image")
            bpy.ops.render.render()
            if self.render_main(context, objlist) is True:
                self.report({'INFO'}, msg)

        # -----------------------------
        # Animation
        # -----------------------------
        elif wm.archipack.render_type == "5":
            oldframe = scene.frame_current
            flag = False
            # loop frames
            for frm in range(scene.frame_start, scene.frame_end + 1):
                scene.frame_set(frm)
                print("Archipack: Rendering frame %04d" % frm)
                bpy.ops.render.render()
                flag = self.render_main(context, objlist, True)
                if flag is False:
                    break

            scene.frame_current = oldframe
            if flag is True:
                self.report({'INFO'}, msg)

        self.hide_gl(context, objlist)

        return {'FINISHED'}

    # ---------------------
    # Set cameraView
    # ---------------------
    # noinspection PyMethodMayBeStatic
    def set_camera_view(self):
        for area in bpy.context.screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces[0].region_3d.view_perspective = 'CAMERA'

    # -------------------------------------
    # Set only render status
    # -------------------------------------
    # noinspection PyMethodMayBeStatic
    def set_only_render(self, status):
        screen = bpy.context.screen

        v3d = False
        s = None
        # get spaceview_3d in current screen
        for a in screen.areas:
            if a.type == 'VIEW_3D':
                for s in a.spaces:
                    if s.type == 'VIEW_3D':
                        v3d = s
                        break

        if v3d is not False:
            s.show_only_render = status


def register():
    bpy.utils.register_class(ARCHIPACK_OT_render)
    bpy.utils.register_class(ARCHIPACK_OT_render_thumbs)


def unregister():
    bpy.utils.unregister_class(ARCHIPACK_OT_render)
    bpy.utils.unregister_class(ARCHIPACK_OT_render_thumbs)
