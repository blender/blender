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

# ----------------------------------------------------------
# support routines for render measures in final image
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
import bgl
# noinspection PyUnresolvedReferences
import blf
from os import path, remove
from sys import exc_info
# noinspection PyUnresolvedReferences
import bpy_extras.image_utils as img_utils
# noinspection PyUnresolvedReferences
import bpy_extras.object_utils as object_utils
# noinspection PyUnresolvedReferences
from bpy_extras import view3d_utils
from math import ceil
from .measureit_geometry import *


# -------------------------------------------------------------
# Render image main entry point
#
# -------------------------------------------------------------
def render_main(self, context, animation=False):
    # noinspection PyBroadException,PyBroadException
    # Save old info
    settings = bpy.context.scene.render.image_settings
    depth = settings.color_depth
    settings.color_depth = '8'
    # noinspection PyBroadException
    try:
        # Get visible layers
        layers = []
        scene = context.scene
        for x in range(0, 20):
            if scene.layers[x] is True:
                layers.extend([x])

        # Get object list
        objlist = context.scene.objects
        # --------------------
        # Get resolution
        # --------------------
        scene = bpy.context.scene
        render_scale = scene.render.resolution_percentage / 100

        width = int(scene.render.resolution_x * render_scale)
        height = int(scene.render.resolution_y * render_scale)
        # ---------------------------------------
        # Get output path
        # ---------------------------------------
        temp_path = path.realpath(bpy.app.tempdir)
        if len(temp_path) > 0:
            outpath = path.join(temp_path, "measureit_tmp_render.png")
        else:
            self.report({'ERROR'},
                        "MeasureIt: Unable to save temporary render image. Define a valid temp path")
            settings.color_depth = depth
            return False

        # Get Render Image
        img = get_render_image(outpath)
        if img is None:
            self.report({'ERROR'},
                        "MeasureIt: Unable to save temporary render image. Define a valid temp path")
            settings.color_depth = depth
            return False

        # -----------------------------
        # Calculate rows and columns
        # -----------------------------
        tile_x = 240
        tile_y = 216
        row_num = ceil(height / tile_y)
        col_num = ceil(width / tile_x)
        print("MeasureIt: Image divided in " + str(row_num) + "x" + str(col_num) + " tiles")

        # pixels out of visible area
        cut4 = (col_num * tile_x * 4) - width * 4  # pixels aout of drawing area
        totpixel4 = width * height * 4  # total pixels RGBA

        viewport_info = bgl.Buffer(bgl.GL_INT, 4)
        bgl.glGetIntegerv(bgl.GL_VIEWPORT, viewport_info)

        # Load image on memory
        img.gl_load(0, bgl.GL_NEAREST, bgl.GL_NEAREST)
        tex = img.bindcode[0]

        # --------------------------------------------
        # Create output image (to apply texture)
        # --------------------------------------------
        if "measureit_output" in bpy.data.images:
            out_img = bpy.data.images["measureit_output"]
            if out_img is not None:
                bpy.data.images.remove(out_img)

        out = bpy.data.images.new("measureit_output", width, height)
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
                for myobj in objlist:
                    if myobj.hide is False:
                        if 'MeasureGenerator' in myobj:
                            # verify visible layer
                            for x in range(0, 20):
                                if myobj.layers[x] is True:
                                    if x in layers:
                                        op = myobj.MeasureGenerator[0]
                                        draw_segments(context, myobj, op, None, None)
                                    break

                # -----------------------------
                # Loop to draw all debug
                # -----------------------------
                if scene.measureit_debug is True:
                    selobj = bpy.context.selected_objects
                    for myobj in selobj:
                        if scene.measureit_debug_objects is True:
                            draw_object(context, myobj, None, None)
                        elif scene.measureit_debug_object_loc is True:
                            draw_object(context, myobj, None, None)
                        if scene.measureit_debug_vertices is True:
                            draw_vertices(context, myobj, None, None)
                        elif scene.measureit_debug_vert_loc is True:
                            draw_vertices(context, myobj, None, None)
                        if scene.measureit_debug_edges is True:
                            draw_edges(context, myobj, None, None)
                        if scene.measureit_debug_faces is True or scene.measureit_debug_normals is True:
                            draw_faces(context, myobj, None, None)

                if scene.measureit_rf is True:
                    bgl.glColor3f(1.0, 1.0, 1.0)
                    rfcolor = scene.measureit_rf_color
                    rfborder = scene.measureit_rf_border
                    rfline = scene.measureit_rf_line

                    bgl.glLineWidth(rfline)
                    bgl.glColor4f(rfcolor[0], rfcolor[1], rfcolor[2], rfcolor[3])

                    x1 = rfborder
                    x2 = width - rfborder
                    y1 = int(ceil(rfborder / (width / height)))
                    y2 = height - y1
                    draw_rectangle((x1, y1), (x2, y2))

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
        bpy.data.images.remove(img)
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
        if out is not None and (scene.measureit_render is True or animation is True):
            ren_path = bpy.context.scene.render.filepath
            filename = "mit_frame"
            if len(ren_path) > 0:
                if ren_path.endswith(path.sep):
                    initpath = path.realpath(ren_path) + path.sep
                else:
                    (initpath, filename) = path.split(ren_path)

            ftxt = "%04d" % scene.frame_current
            outpath = path.realpath(path.join(initpath, filename + ftxt + ".png"))

            save_image(self, outpath, out)

        settings.color_depth = depth
        return True

    except:
        settings.color_depth = depth
        print("Unexpected error:" + str(exc_info()))
        self.report({'ERROR'}, "MeasureIt: Unable to create render image. Be sure the output render path is correct")
        return False


# --------------------------------------------------------------------
# Get the final render image and return as image object
#
# return None if no render available
# --------------------------------------------------------------------
def get_render_image(outpath):
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
        print("MeasureIt: Image " + filepath + " saved")

        # Restore old info
        settings.file_format = myformat
        settings.color_mode = mode
        settings.color_depth = depth
    except:
        print("Unexpected error:" + str(exc_info()))
        self.report({'ERROR'}, "MeasureIt: Unable to save render image")
        return
