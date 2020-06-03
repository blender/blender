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

# <pep8-80 compliant>

"""
Similar to ``addon_utils``, except we can only have one active at a time.

In most cases users of this module will simply call 'activate'.
"""

__all__ = (
    "activate",
    "import_from_path",
    "import_from_id",
    "reset",
)

import bpy as _bpy

# Normally matches 'preferences.app_template_id',
# but loading new preferences will get us out of sync.
_app_template = {
    "id": "",
}

# instead of sys.modules
# note that we only ever have one template enabled at a time
# so it may not seem necessary to use this.
#
# However, templates may want to share between each-other,
# so any loaded modules are stored here?
#
# Note that the ID here is the app_template_id , not the modules __name__.
_modules = {}


def _enable(template_id, *, handle_error=None, ignore_not_found=False):
    from bpy_restrict_state import RestrictBlend

    if handle_error is None:
        def handle_error(_ex):
            import traceback
            traceback.print_exc()

    # Split registering up into 2 steps so we can undo
    # if it fails par way through.

    # disable the context, using the context at all is
    # really bad while loading an template, don't do it!
    with RestrictBlend():

        # 1) try import
        try:
            mod = import_from_id(template_id, ignore_not_found=ignore_not_found)
            if mod is None:
                return None
            mod.__template_enabled__ = False
            _modules[template_id] = mod
        except Exception as ex:
            handle_error(ex)
            return None

        # 2) try run the modules register function
        try:
            mod.register()
        except Exception as ex:
            print("Exception in module register(): %r" %
                  getattr(mod, "__file__", template_id))
            handle_error(ex)
            del _modules[template_id]
            return None

    # * OK loaded successfully! *
    mod.__template_enabled__ = True

    if _bpy.app.debug_python:
        print("\tapp_template_utils.enable", mod.__name__)

    return mod


def _disable(template_id, *, handle_error=None):
    """
    Disables a template by name.

    :arg template_id: The name of the template and module.
    :type template_id: string
    :arg handle_error: Called in the case of an error,
       taking an exception argument.
    :type handle_error: function
    """

    if handle_error is None:
        def handle_error(_ex):
            import traceback
            traceback.print_exc()

    mod = _modules.get(template_id)

    if mod and getattr(mod, "__template_enabled__", False) is not False:
        mod.__template_enabled__ = False

        try:
            mod.unregister()
        except Exception as ex:
            print("Exception in module unregister(): %r" %
                  getattr(mod, "__file__", template_id))
            handle_error(ex)
    else:
        print("\tapp_template_utils.disable: %s not %s." %
              (template_id, "disabled" if mod is None else "loaded"))

    if _bpy.app.debug_python:
        print("\tapp_template_utils.disable", template_id)


def import_from_path(path, ignore_not_found=False):
    import os
    from importlib import import_module
    base_module, template_id = path.rsplit(os.sep, 2)[-2:]
    module_name = base_module + "." + template_id

    try:
        return import_module(module_name)
    except ModuleNotFoundError as ex:
        if ignore_not_found and ex.name == module_name:
            return None
        raise ex


def import_from_id(template_id, ignore_not_found=False):
    import os
    path = next(iter(_bpy.utils.app_template_paths(template_id)), None)
    if path is None:
        if ignore_not_found:
            return None
        else:
            raise Exception("%r template not found!" % template_id)
    else:
        if ignore_not_found:
            if not os.path.exists(os.path.join(path, "__init__.py")):
                return None
        return import_from_path(path, ignore_not_found=ignore_not_found)


def activate(template_id=None):
    template_id_prev = _app_template["id"]

    # not needed but may as well avoids redundant
    # disable/enable for all add-ons on 'File -> New'
    if template_id_prev == template_id:
        return

    if template_id_prev:
        _disable(template_id_prev)

    # ignore_not_found so modules that don't contain scripts don't raise errors
    _mod = _enable(template_id, ignore_not_found=True) if template_id else None

    _app_template["id"] = template_id


def reset(*, reload_scripts=False):
    """
    Sets default state.
    """
    template_id = _bpy.context.preferences.app_template
    if _bpy.app.debug_python:
        print("bl_app_template_utils.reset('%s')" % template_id)

    # TODO reload_scripts

    activate(template_id)
