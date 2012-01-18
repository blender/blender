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
    import _cycles
    import os.path

    path = os.path.dirname(__file__)
    user_path = os.path.dirname(os.path.abspath(bpy.utils.user_resource('CONFIG', '')))

    _cycles.init(path, user_path)


def create(engine, data, scene, region=0, v3d=0, rv3d=0):
    import _cycles

    data = data.as_pointer()
    userpref = bpy.context.user_preferences.as_pointer()
    scene = scene.as_pointer()
    if region:
        region = region.as_pointer()
    if v3d:
        v3d = v3d.as_pointer()
    if rv3d:
        rv3d = rv3d.as_pointer()

    engine.session = _cycles.create(engine.as_pointer(), userpref, data, scene, region, v3d, rv3d)


def free(engine):
    if hasattr(engine, "session"):
        if engine.session:
            import _cycles
            _cycles.free(engine.session)
        del engine.session


def render(engine):
    import _cycles
    if hasattr(engine, "session"):
        _cycles.render(engine.session)


def update(engine, data, scene):
    import _cycles
    _cycles.sync(engine.session)


def draw(engine, region, v3d, rv3d):
    import _cycles
    v3d = v3d.as_pointer()
    rv3d = rv3d.as_pointer()

    # draw render image
    _cycles.draw(engine.session, v3d, rv3d)


def available_devices():
    import _cycles
    return _cycles.available_devices()


def with_osl():
    import _cycles
    return _cycles.with_osl
