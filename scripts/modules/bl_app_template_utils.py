# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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

# Instead of `sys.modules`
# note that we only ever have one template enabled at a time
# so it may not seem necessary to use this.
#
# However, templates may want to share between each-other,
# so any loaded modules are stored here?
#
# Note that the ID here is the app_template_id , not the modules __name__.
_modules = {}


def _enable(template_id, *, handle_error=None, ignore_not_found=False):
    from _bpy_restrict_state import RestrictBlend

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
        except Exception as ex:
            handle_error(ex)
            return None

        _modules[template_id] = mod
        if mod is None:
            return None
        mod.__template_enabled__ = False

        # 2) try run the modules register function
        try:
            mod.register()
        except Exception as ex:
            print("Exception in module register(): {!r}".format(getattr(mod, "__file__", template_id)))
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
    :type template_id: str
    :arg handle_error: Called in the case of an error,
       taking an exception argument.
    :type handle_error: Callable[[Exception], None] | None
    """

    if handle_error is None:
        def handle_error(_ex):
            import traceback
            traceback.print_exc()

    mod = _modules.get(template_id, False)

    if mod is None:
        # Loaded but has no module, remove since there is no use in keeping it.
        del _modules[template_id]
    elif getattr(mod, "__template_enabled__", False) is not False:
        mod.__template_enabled__ = False

        try:
            mod.unregister()
        except Exception as ex:
            print("Exception in module unregister(): {!r}".format(getattr(mod, "__file__", template_id)))
            handle_error(ex)
    else:
        print(
            "\tapp_template_utils.disable: {:s} not {:s}.".format(
                template_id,
                "disabled" if mod is False else "loaded",
            )
        )

    if _bpy.app.debug_python:
        print("\tapp_template_utils.disable", template_id)


def import_from_path(path, *, ignore_not_found=False):
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


def import_from_id(template_id, *, ignore_not_found=False):
    import os
    path = next(iter(_bpy.utils.app_template_paths(path=template_id)), None)
    if path is None:
        if ignore_not_found:
            return None
        else:
            raise Exception("{!r} template not found!".format(template_id))
    else:
        if ignore_not_found:
            if not os.path.exists(os.path.join(path, "__init__.py")):
                return None
        return import_from_path(path, ignore_not_found=ignore_not_found)


def activate(*, template_id=None, reload_scripts=False):
    template_id_prev = _app_template["id"]

    # not needed but may as well avoids redundant
    # disable/enable for all add-ons on "File -> New".
    if not reload_scripts and template_id_prev == template_id:
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
        print("bl_app_template_utils.reset('{:s}')".format(template_id))

    activate(template_id=template_id, reload_scripts=reload_scripts)
