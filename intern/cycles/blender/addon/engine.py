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

    path = os.path.dirname(__file__)
    user_path = os.path.dirname(os.path.abspath(bpy.utils.user_resource('CONFIG', '')))

    lib.init(path, user_path)

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
    lib.draw(engine.session, v3d, rv3d)

def available_devices():
    import libcycles_blender as lib
    return lib.available_devices()

def with_osl():
    import libcycles_blender as lib
    return lib.with_osl()

