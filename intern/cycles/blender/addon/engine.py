#
# Copyright 2011, Blender Foundation.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

import bpy

def init():
	import libcycles_blender as lib
	import os.path
	lib.init(os.path.dirname(__file__))

def create(engine, data, scene, region = 0, v3d = 0, rv3d = 0):
	import libcycles_blender as lib

	data = data.as_pointer()
	scene = scene.as_pointer()
	if region:
		region = region.as_pointer()
	if v3d:
		v3d = v3d.as_pointer()
	if rv3d:
		rv3d = rv3d.as_pointer()

	engine.session = lib.create(engine.as_pointer(), data, scene, region, v3d, rv3d)

def free(engine):
	if "session" in dir(engine):
		if engine.session:
			import libcycles_blender as lib
			lib.free(engine.session)
		del engine.session

def render(engine):
	import libcycles_blender as lib
	lib.render(engine.session)

def update(engine, data, scene):
	import libcycles_blender as lib
	lib.sync(engine.session)

def draw(engine, region, v3d, rv3d):
	import libcycles_blender as lib
	v3d = v3d.as_pointer()
	rv3d = rv3d.as_pointer()

	# draw render image
	status, substatus = lib.draw(engine.session, v3d, rv3d)

	# draw text over image
	if status != "":
		import blf
		import bgl

		fontid = 0 # todo, find out how to set this
		dim = blf.dimensions(fontid, status)
		dim_sub = blf.dimensions(fontid, substatus)

		padding = 5

		x = (region.width - max(dim[0], dim_sub[0]))*0.5 - padding
		y = (region.height - (dim[1] + dim_sub[1] + padding))*0.5 - padding

		bgl.glColor4f(0.0, 0.0, 0.0, 0.5)
		bgl.glEnable(bgl.GL_BLEND)
		bgl.glBlendFunc(bgl.GL_SRC_ALPHA, bgl.GL_ONE_MINUS_SRC_ALPHA)
		bgl.glRectf(x, y, x+max(dim[0], dim_sub[0])+padding+padding, y+dim[1]+dim_sub[1]+padding+padding+2)
		bgl.glDisable(bgl.GL_BLEND)

		x = (region.width - dim[0])*0.5
		y = (region.height - (dim[1] + dim_sub[1] + padding))*0.5 + dim_sub[1] + padding

		bgl.glColor3f(0.8, 0.8, 0.8)
		blf.position(fontid, x, y, 0)
		blf.draw(fontid, status)

		x = (region.width - dim_sub[0])*0.5
		y = (region.height - (dim[1] + dim_sub[1] + padding))*0.5

		bgl.glColor3f(0.6, 0.6, 0.6)
		blf.position(fontid, x, y, 0)
		blf.draw(fontid, substatus)

def available_devices():
	import libcycles_blender as lib
	return lib.available_devices()

def with_osl():
	import libcycles_blender as lib
	return lib.with_osl()

