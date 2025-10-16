# SPDX-FileCopyrightText: 2013-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if "bpy" in locals():
    import importlib
    importlib.reload(settings)
    importlib.reload(utils_i18n)
    importlib.reload(utils_languages_menu)
else:
    import bpy
    from bpy.types import Operator
    from bpy.props import (
        BoolProperty,
        EnumProperty,
    )
    from . import settings
    from _bl_i18n_utils import utils as utils_i18n
    from _bl_i18n_utils import utils_languages_menu

import concurrent.futures
import io
import os
import shutil
import subprocess
import tempfile


# Operators ###################################################################

class UI_OT_i18n_updatetranslation_work_repo(Operator):
    """Update i18n working repository (po files)"""
    bl_idname = "ui.i18n_updatetranslation_work_repo"
    bl_label = "Update I18n Work Repository"

    use_skip_pot_gen: BoolProperty(
        name="Skip POT",
        description="Skip POT file generation",
        default=False,
    )

    def execute(self, context):
        if not hasattr(self, "settings"):
            self.settings = settings.settings
        i18n_sett = context.window_manager.i18n_update_settings
        self.settings.FILE_NAME_POT = i18n_sett.pot_path

        num_langs = len(i18n_sett.langs)
        context.window_manager.progress_begin(0, num_langs + 1)
        context.window_manager.progress_update(0)
        if not self.use_skip_pot_gen:
            env = os.environ.copy()
            env["ASAN_OPTIONS"] = "exitcode=0:" + os.environ.get("ASAN_OPTIONS", "")
            # Generate base pot from RNA messages (we use another blender instance here, to be able to perfectly
            # control our environment (factory startup, specific addons enabled/disabled...)).
            # However, we need to export current user settings about this addon!
            cmmd = (
                bpy.app.binary_path,
                "--background",
                "--factory-startup",
                "--python",
                os.path.join(os.path.dirname(utils_i18n.__file__), "bl_extract_messages.py"),
                "--",
                "--no_checks",
                "--settings",
                self.settings.to_json(),
            )
            # Not working (UI is not refreshed...).
            # self.report({'INFO'}, "Extracting messages, this will take some time...")
            context.window_manager.progress_update(1)
            ret = subprocess.run(cmmd, env=env)
            if ret.returncode != 0:
                self.report({'ERROR'}, "Message extraction process failed!")
                context.window_manager.progress_end()
                return {'CANCELLED'}

        # Now we should have a valid POT file, we have to merge it in all languages po's...
        pot = utils_i18n.I18nMessages(kind='PO', src=self.settings.FILE_NAME_POT, settings=self.settings)
        for progress, lng in enumerate(i18n_sett.langs):
            utils_i18n.I18nMessages.update_from_pot_callback(pot, dict(lng.items()), self.settings)
            context.window_manager.progress_update(progress + 2)
        # NOTE: While on linux sub-processes are `os.fork`ed by default,
        #       on Windows and OSX they are `spawn`ed.
        #       See https://docs.python.org/3/library/multiprocessing.html#contexts-and-start-methods
        #       This is a problem because spawned processes do not inherit the whole environment
        #       of the current (Blender-customized) python. In practice, the `bpy` module won't load e.g.
        #       So care must be taken that the callback passed to the executor does not rely on any
        #       Blender-specific modules etc. This is why it is using a class method from `_bl_i18n_utils`
        #       module, rather than a local function of this current Blender-only module.
        # FIXME: This can easily deadlock on powerful machine with lots of RAM (128GB) and cores (32)...
        # ~ with concurrent.futures.ProcessPoolExecutor() as exctr:
            # ~ pot = utils_i18n.I18nMessages(kind='PO', src=self.settings.FILE_NAME_POT, settings=self.settings)
            # ~ for progress, _ in enumerate(
            # ~ exctr.map(utils_i18n.I18nMessages.update_from_pot_callback,
            # ~ (pot,) * num_langs,
            # ~ [dict(lng.items()) for lng in i18n_sett.langs],
            # ~ (self.settings,) * num_langs,
            # ~ chunksize=4,
            # ~ timeout=60)):
            # ~ context.window_manager.progress_update(progress + 2)

        context.window_manager.progress_end()
        print("", flush=True)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


class UI_OT_i18n_cleanuptranslation_work_repo(Operator):
    """Clean up i18n working repository (po files)"""
    bl_idname = "ui.i18n_cleanuptranslation_work_repo"
    bl_label = "Clean up I18n Work Repository"

    def execute(self, context):
        if not hasattr(self, "settings"):
            self.settings = settings.settings
        i18n_sett = context.window_manager.i18n_update_settings
        # 'DEFAULT' and en_US are always valid, fully-translated "languages"!
        stats = {"DEFAULT": 1.0, "en_US": 1.0}

        num_langs = len(i18n_sett.langs)
        context.window_manager.progress_begin(0, num_langs + 1)
        context.window_manager.progress_update(0)
        for progress, lng in enumerate(i18n_sett.langs):
            utils_i18n.I18nMessages.cleanup_callback(dict(lng.items()), self.settings)
            context.window_manager.progress_update(progress + 1)
        # NOTE: See comment in #UI_OT_i18n_updatetranslation_work_repo `execute` function about usage caveats
        #       of the `ProcessPoolExecutor`.
        # FIXME: This can easily deadlock on powerful machine with lots of RAM (128GB) and cores (32)...
        # ~ with concurrent.futures.ProcessPoolExecutor() as exctr:
            # ~ for progress, _ in enumerate(
            # ~ exctr.map(utils_i18n.I18nMessages.cleanup_callback,
            # ~ [dict(lng.items()) for lng in i18n_sett.langs],
            # ~ (self.settings,) * num_langs,
            # ~ chunksize=4,
            # ~ timeout=60)):
            # ~ context.window_manager.progress_update(progress + 1)

        context.window_manager.progress_end()
        print("", flush=True)
        return {'FINISHED'}


class UI_OT_i18n_updatetranslation_blender_repo(Operator):
    """Update i18n data (po files) in Blender source code repository"""
    bl_idname = "ui.i18n_updatetranslation_blender_repo"
    bl_label = "Update I18n Blender Repository"

    def execute(self, context):
        if not hasattr(self, "settings"):
            self.settings = settings.settings
        i18n_sett = context.window_manager.i18n_update_settings
        # 'DEFAULT' and en_US are always valid, fully-translated "languages"!
        stats = {"DEFAULT": 1.0, "en_US": 1.0}

        num_langs = len(i18n_sett.langs)
        context.window_manager.progress_begin(0, num_langs + 1)
        context.window_manager.progress_update(0)
        for progress, lng in enumerate(i18n_sett.langs):
            lng_uid, stats_val, reports = utils_i18n.I18nMessages.update_to_blender_repo_callback(
                dict(lng.items()), self.settings)
            context.window_manager.progress_update(progress + 1)
            stats[lng_uid] = stats_val
            print("".join(reports) + "\n")
        # NOTE: See comment in #UI_OT_i18n_updatetranslation_work_repo `execute` function about usage caveats
        #       of the `ProcessPoolExecutor`.
        # FIXME: This can easily deadlock on powerful machine with lots of RAM (128GB) and cores (32)...
        # ~ with concurrent.futures.ProcessPoolExecutor() as exctr:
            # ~ for progress, (lng_uid, stats_val, reports) in enumerate(
            # ~ exctr.map(utils_i18n.I18nMessages.update_to_blender_repo_callback,
            # ~ [dict(lng.items()) for lng in i18n_sett.langs],
            # ~ (self.settings,) * num_langs,
            # ~ chunksize=4,
            # ~ timeout=60)):
            # ~ context.window_manager.progress_update(progress + 1)
            # ~ stats[lng_uid] = stats_val
            # ~ print("".join(reports) + "\n")

        print("Generating languages' menu...", flush=True)
        context.window_manager.progress_update(progress + 2)
        languages_menu_lines = utils_languages_menu.gen_menu_file(stats, self.settings)
        with open(os.path.join(self.settings.BLENDER_I18N_ROOT, self.settings.LANGUAGES_FILE), 'w', encoding="utf8") as f:
            f.write("\n".join(languages_menu_lines))
        context.window_manager.progress_end()

        return {'FINISHED'}


class UI_OT_i18n_updatetranslation_statistics(Operator):
    """Create or extend a 'i18n_info.txt' Text datablock"""
    """(it will contain statistics and checks about current working repository PO files)"""
    bl_idname = "ui.i18n_updatetranslation_statistics"
    bl_label = "Update I18n Statistics"

    report_name = "i18n_info.txt"

    def execute(self, context):
        if not hasattr(self, "settings"):
            self.settings = settings.settings
        i18n_sett = context.window_manager.i18n_update_settings

        buff = io.StringIO()
        lst = [(lng, lng.po_path) for lng in i18n_sett.langs]

        context.window_manager.progress_begin(0, len(lst))
        context.window_manager.progress_update(0)
        for progress, (lng, path) in enumerate(lst):
            context.window_manager.progress_update(progress + 1)
            if not lng.use:
                print("Skipping {} language ({}).".format(lng.name, lng.uid))
                continue
            buff.write("Processing {} language ({}, {}).\n".format(lng.name, lng.uid, path))
            po = utils_i18n.I18nMessages(uid=lng.uid, kind='PO', src=path, settings=self.settings)
            po.print_info(prefix="    ", output=buff.write)
            errs = po.check(fix=False)
            if errs:
                buff.write("    WARNING! Po contains following errors:\n")
                buff.write("        " + "\n        ".join(errs))
                buff.write("\n")
            buff.write("\n\n")

        text = None
        if self.report_name not in bpy.data.texts:
            text = bpy.data.texts.new(self.report_name)
        else:
            text = bpy.data.texts[self.report_name]
        data = text.as_string()
        data = data + "\n" + buff.getvalue()
        text.from_string(data)
        self.report({'INFO'}, "Info written to %s text datablock!" % self.report_name)

        context.window_manager.progress_end()
        print("", flush=True)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


classes = (
    UI_OT_i18n_updatetranslation_work_repo,
    UI_OT_i18n_cleanuptranslation_work_repo,
    UI_OT_i18n_updatetranslation_blender_repo,
    UI_OT_i18n_updatetranslation_statistics,
)
