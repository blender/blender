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

# <pep8 compliant>

import bpy


def init():
    import bcycles
    import os.path

    path = os.path.dirname(__file__)
    user_path = os.path.dirname(os.path.abspath(bpy.utils.user_resource('CONFIG', '')))

    bcycles.init(path, user_path)


def create(engine, data, scene, region=0, v3d=0, rv3d=0):
    import bcycles

    data = data.as_pointer()
    scene = scene.as_pointer()
    if region:
        region = region.as_pointer()
    if v3d:
        v3d = v3d.as_pointer()
    if rv3d:
        rv3d = rv3d.as_pointer()

    engine.session = bcycles.create(engine.as_pointer(), data, scene, region, v3d, rv3d)


def free(engine):
    if hasattr(engine, "session"):
        if engine.session:
            import bcycles
            bcycles.free(engine.session)
        del engine.session


def render(engine):
    import bcycles
    if hasattr(engine, "session"):
        bcycles.render(engine.session)


def update(engine, data, scene):
    import bcycles
    if scene.render.use_border:
        engine.report({'ERROR'}, "Border rendering not supported yet")
        free(engine)
    else:
        bcycles.sync(engine.session)


def draw(engine, region, v3d, rv3d):
    import bcycles
    v3d = v3d.as_pointer()
    rv3d = rv3d.as_pointer()

    # draw render image
    bcycles.draw(engine.session, v3d, rv3d)


def available_devices():
    import bcycles
    return bcycles.available_devices()


def with_osl():
    import bcycles
    return bcycles.with_osl()
