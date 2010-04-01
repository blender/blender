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
from bpy.props import StringProperty

class EditExternally(bpy.types.Operator):
    '''Edit image in an external application'''
    bl_idname = "image.external_edit"
    bl_label = "Image Edit Externally"
    bl_options = {'REGISTER'}

    path = StringProperty(name="File Path", description="Path to an image file", maxlen= 1024, default= "")

    def _editor_guess(self, context):
        import platform
        system = platform.system()

        image_editor = context.user_preferences.filepaths.image_editor

        # use image editor in the preferences when available.
        if not image_editor:
            if system == 'Windows':
                image_editor = ["start"] # not tested!
            elif system == 'Darwin':
                image_editor = ["open"]
            else:
                image_editor = ["gimp"]
        else:
            if system == 'Darwin':
                # blender file selector treats .app as a folder
                # and will include a trailing backslash, so we strip it.
                image_editor.rstrip('\\')
                image_editor = ["open", "-a", image_editor]

        return image_editor

    def execute(self, context):
        import subprocess
        path = self.properties.path
        image_editor = self._editor_guess(context)

        cmd = []
        cmd.extend(image_editor)
        cmd.append(bpy.utils.expandpath(path))

        subprocess.Popen(cmd)

        return {'FINISHED'}

    def invoke(self, context, event):
        try:
            path = context.space_data.image.filename
        except:
            self.report({'ERROR'}, "Image not found on disk")
            return {'CANCELLED'}

        self.properties.path = path
        self.execute(context)
        
        return {'FINISHED'}


class SaveDirty(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.save_dirty"
    bl_label = "Save Dirty"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        unique_paths = set()
        for image in bpy.data.images:
            if image.dirty:
                path = bpy.utils.expandpath(image.filename)
                if "\\" not in path and "/" not in path:
                    self.report({'WARNING'}, "Invalid path: " + path)
                elif path in unique_paths:
                    self.report({'WARNING'}, "Path used by more then one image: " + path)
                else:
                    unique_paths.add(path)
                    image.save()
        return {'FINISHED'}


class ProjectEdit(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.project_edit"
    bl_label = "Project Edit"
    bl_options = {'REGISTER'}

    _proj_hack = [""]

    def execute(self, context):
        import os
        import subprocess

        EXT = "png" # could be made an option but for now ok

        for image in bpy.data.images:
            image.tag = True

        bpy.ops.paint.image_from_view()

        image_new = None
        for image in bpy.data.images:
            if not image.tag:
                image_new = image
                break

        if not image_new:
            self.report({'ERROR'}, "Could not make new image")
            return {'CANCELLED'}

        filename = os.path.basename(bpy.data.filename)
        filename = os.path.splitext(filename)[0]
        # filename = bpy.utils.clean_name(filename) # fixes <memory> rubbish, needs checking

        if filename.startswith("."): # TODO, have a way to check if the file is saved, assuem .B25.blend
            filename = os.path.join(os.path.dirname(bpy.data.filename), filename)
        else:
            filename = "//" + filename

        obj = context.object

        if obj:
            filename += "_" + bpy.utils.clean_name(obj.name)

        filename_final = filename + "." + EXT
        i = 0

        while os.path.exists(bpy.utils.expandpath(filename_final)):
            filename_final = filename + ("%.3d.%s" % (i, EXT))
            i += 1

        image_new.name = os.path.basename(filename_final)
        ProjectEdit._proj_hack[0] = image_new.name

        image_new.filename_raw = filename_final # TODO, filename raw is crummy
        image_new.file_format = 'PNG'
        image_new.save()

        bpy.ops.image.external_edit(path=filename_final)

        return {'FINISHED'}


class ProjectApply(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.project_apply"
    bl_label = "Project Apply"
    bl_options = {'REGISTER'}

    def execute(self, context):
        image_name = ProjectEdit._proj_hack[0] # TODO, deal with this nicer

        try:
            image = bpy.data.images[image_name]
        except KeyError:
            self.report({'ERROR'}, "Could not find image '%s'" % image_name)
            return {'CANCELLED'}

        image.reload()
        bpy.ops.paint.project_image(image=image_name)

        return {'FINISHED'}

#!/usr/bin/env python
# stripped down code from youtube-dl:
# http://bitbucket.org/rg3/youtube-dl/wiki/Home
# License: Public domain code

import os
import os.path
import random
import re
import socket
import string
import tempfile
import time
import urllib
import urllib.request
import getpass

std_headers = {
	'User-Agent': 'Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.2) Gecko/20100115 Firefox/3.6',
	'Accept-Charset': 'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
	'Accept': 'text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5',
	'Accept-Language': 'en-us,en;q=0.5',
}

class DownloadError(Exception):
	pass

class FileDownloader(object):
	params = None
	_ies = []
	_download_retcode = None

	def __init__(self, params):
		self._ies = []
		self._download_retcode = 0
		self.params = params
	
	@staticmethod
	def calc_percent(byte_counter, data_len):
		if data_len is None:
			return '---.-%'
		return '%6s' % ('%3.1f%%' % (float(byte_counter) / float(data_len) * 100.0))

	@staticmethod
	def best_block_size(elapsed_time, bytes):
		new_min = max(bytes / 2.0, 1.0)
		new_max = min(max(bytes * 2.0, 1.0), 4194304) # Do not surpass 4 MB
		if elapsed_time < 0.001:
			return int(new_max)
		rate = bytes / elapsed_time
		if rate > new_max:
			return int(new_max)
		if rate < new_min:
			return int(new_min)
		return int(rate)

	@staticmethod
	def verify_url(url):
		request = urllib.request.Request(url, None, std_headers)
		data = urllib.request.urlopen(request)
		data.read(1)
		url = data.geturl()
		data.close()
		return url

	def to_stdout(self, message, skip_eol=False, ignore_encoding_errors=False):
		print(message)
	
	def to_stderr(self, message):
		print(message)
	
	def trouble(self, message=None):
		if message is not None:
			self.to_stderr(message)
		raise DownloadError(message)
		self._download_retcode = 1

	def report_progress(self, percent_str):
		print(percent_str)

	def process_info(self, url, filename):
		success = self._do_download(filename, url)

	def download(self, url, filename):
		ie = YoutubeSearchIE(YoutubeIE(self))
		ie.extract(url, filename)

		return self._download_retcode

	def _do_download(self, filename, url):
		stream = None
		open_mode = 'wb'
		request = urllib.request.Request(url, None, std_headers)

		# Establish connection
		try:
			data = urllib.request.urlopen(request)
		except:
			raise

		limit = 1024*256 # quarter of a megabyte max

		data_len = float(data.info().get('Content-length', None))
		byte_counter = 0
		block_size = 1024
		start = time.time()
		while True:
			# Download and write
			before = time.time()
			data_block = data.read(block_size)
			after = time.time()
			data_block_len = len(data_block)
			if data_block_len == 0:
				break
			byte_counter += data_block_len

			# Open file just in time
			if stream is None:
				try:
					stream = open(filename, open_mode)
				except:
					return False
			stream.write(data_block)
			block_size = self.best_block_size(after - before, data_block_len)

			# Progress message
			percent_str = self.calc_percent(byte_counter, min(data_len, limit))
			self.report_progress(percent_str)

			if byte_counter > limit:
				break

		return True

class InfoExtractor(object):
	_ready = False
	_downloader = None

	def __init__(self, downloader=None):
		self._ready = False
		self._downloader = downloader

	def initialize(self):
		if not self._ready:
			self._real_initialize()
			self._ready = True

	def extract(self, url, filename):
		self.initialize()
		return self._real_extract(url, filename)

class YoutubeIE(InfoExtractor):
	_VALID_URL = r'^((?:http://)?(?:\w+\.)?youtube\.com/(?:(?:v/)|(?:(?:watch(?:\.php)?)?[\?#](?:.+&)?v=)))?([0-9A-Za-z_-]+)(?(1).+)?$'
	_LANG_URL = r'http://uk.youtube.com/?hl=en&persist_hl=1&gl=US&persist_gl=1&opt_out_ackd=1'

	def _real_initialize(self):
		if self._downloader is None:
			return

		# Set language
		request = urllib.request.Request(self._LANG_URL, None, std_headers)
		try:
			urllib.request.urlopen(request).read()
		except:
			print("unable to set language")
			return

	def _real_extract(self, url, filename):
		# Extract video id from URL
		video_id = re.match(self._VALID_URL, url).group(2)

		# Get video info
		video_info_url = 'http://www.youtube.com/get_video_info?&video_id=%s&el=detailpage&ps=default&eurl=&gl=US&hl=en' % video_id
		request = urllib.request.Request(video_info_url, None, std_headers)
		try:
			video_info_webpage = urllib.request.urlopen(request).read()
			video_info = urllib.parse.parse_qs(str(video_info_webpage))
		except:
			print("unable to download info")
			return

		# "t" param
		if 'token' not in video_info:
			return
		token = urllib.parse.unquote_plus(video_info['token'][0])
		video_real_url = 'http://www.youtube.com/get_video?video_id=%s&t=%s&eurl=&el=detailpage&ps=default&gl=US&hl=en' % (video_id, token)

		# Process video information
		self._downloader.process_info(video_real_url, filename)

class YoutubeSearchIE(InfoExtractor):
	_TEMPLATE_URL = 'http://www.youtube.com/results?search_query=%s&page=%s&gl=US&hl=en'
	_VIDEO_INDICATOR = r'href="/watch\?v=.+?"'
	_youtube_ie = None

	def __init__(self, youtube_ie, downloader=None):
		InfoExtractor.__init__(self, downloader)
		self._youtube_ie = youtube_ie
	
	def _real_initialize(self):
		self._youtube_ie.initialize()
	
	def _real_extract(self, query, filename):
		n = 1

		result_url = self._TEMPLATE_URL % (urllib.parse.quote_plus(query), 1)
		request = urllib.request.Request(result_url, None, std_headers)
		try:
			page = str(urllib.request.urlopen(request).read())
		except:
			print("unable to download search page")
			return

		# Extract video identifiers
		for mobj in re.finditer(self._VIDEO_INDICATOR, page):
			video_id = page[mobj.span()[0]:mobj.span()[1]].split('=')[2][:-1]
			self._youtube_ie.extract('http://www.youtube.com/watch?v=%s' % video_id, filename)
			return

### MAIN PROGRAM ###
def download_movie(search_terms, filename):
	try:
		urllib.request.install_opener(urllib.request.build_opener(urllib.request.ProxyHandler()))
		urllib.request.install_opener(urllib.request.build_opener(urllib.request.HTTPCookieProcessor()))
		socket.setdefaulttimeout(10)

		fd = FileDownloader({})
		fd.download(search_terms, filename)
		return 1
	except DownloadError:
		return 0

import bpy

class MakeAMovie(bpy.types.Operator):
	'''Make a movie button'''
	bl_idname = "render.make_a_movie"
	bl_label = "Make A Movie Button"
	bl_options = {'REGISTER', 'UNDO'}

	def invoke(self, context, event):
		context.scene.thinking = "thinking .. this may take a while"
		context.manager.add_modal_handler(self)
		return {'RUNNING_MODAL'}
	
	def modal(self, context, event):
		try:
			bpy.ops.render.view_cancel()
		except:
			pass

		try:
			about = context.scene.about
			filename = os.path.join(tempfile.gettempdir(), "make_a_movie.flv")
			if not download_movie(about, filename):
				context.scene.thinking = "make a movie button is out of inspiration"
				return {'FINISHED'}

			ed = context.scene.sequence_editor
			if ed and len(ed.sequences) > 0:
				ed.sequences[0].filepath = filename
			else:
				bpy.ops.sequencer.movie_strip_add(path=filename)

			context.scene.render.file_format = 'AVI_JPEG'
			context.scene.thinking = "make a movie button is done"
			rd = context.scene.render
			rd.render_stamp = True
			rd.stamp_time = False
			rd.stamp_date = False
			rd.stamp_render_time = False
			rd.stamp_frame = False
			rd.stamp_scene = False
			rd.stamp_camera = False
			rd.stamp_filename = False
			rd.stamp_marker = False
			rd.stamp_sequencer_strip = False
			rd.stamp_background = [1, 0, 0, 1]
			rd.stamp_foreground = [0, 1, 0, 1]
			rd.stamp_font_size = 16
			rd.stamp_note = True
			rd.stamp_note_text = about.upper() + ", directed by " + getpass.getuser()

			bpy.ops.render.render('INVOKE_DEFAULT', animation=True)
		except:
			context.scene.render.render_stamp = False
			context.scene.thinking = "make a movie button is out of inspiration"

		return {'FINISHED'}

def panel_func(self, context):
	if bpy.data.filename.find(".B25.blend") != -1:
		layout = self.layout

		layout.separator()
		layout.separator()
		layout.operator("render.make_a_movie", icon='RENDER_ANIMATION')
		layout.prop(context.scene, "about")

		layout.label(text=context.scene.thinking)

subjects = ["Dreaming Elephants", "A Big Bunny", "Flying Pigs"]
about = subjects[random.randint(0, len(subjects)-1)]

bpy.types.RENDER_PT_render.append(panel_func)
bpy.types.Scene.StringProperty(name="About", default=about, attr="about")
bpy.types.Scene.StringProperty(name="Thinking", default="Status:", attr="thinking")

classes = [
    EditExternally,
    SaveDirty,
    ProjectEdit,
    ProjectApply,
	MakeAMovie]

def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
