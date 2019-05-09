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

# -----------------------------------------------------------------------------
# AppOverrideState


class AppOverrideState:
    """
    Utility class to encapsulate overriding the application state
    so that settings can be restored afterwards.
    """
    __slots__ = (
        # setup_classes
        "_class_store",
        # setup_ui_ignore
        "_ui_ignore_store",
        # setup_addons
        "_addon_store",
    )

    # ---------
    # Callbacks
    #
    # Set as None, to make it simple to check if they're being overridden.

    # setup/teardown classes
    class_ignore = None

    # setup/teardown ui_ignore
    ui_ignore_classes = None
    ui_ignore_operator = None
    ui_ignore_property = None
    ui_ignore_menu = None
    ui_ignore_label = None

    addon_paths = None
    addons = None

    # End callbacks

    def __init__(self):
        self._class_store = None
        self._addon_store = None
        self._ui_ignore_store = None

    def _setup_classes(self):
        assert(self._class_store is None)
        self._class_store = self.class_ignore()
        from bpy.utils import unregister_class
        for cls in self._class_store:
            unregister_class(cls)

    def _teardown_classes(self):
        assert(self._class_store is not None)

        from bpy.utils import register_class
        for cls in self._class_store:
            register_class(cls)
        self._class_store = None

    def _setup_ui_ignore(self):
        import bl_app_override

        self._ui_ignore_store = bl_app_override.ui_draw_filter_register(
            ui_ignore_classes=(
                None if self.ui_ignore_classes is None
                else self.ui_ignore_classes()
            ),
            ui_ignore_operator=self.ui_ignore_operator,
            ui_ignore_property=self.ui_ignore_property,
            ui_ignore_menu=self.ui_ignore_menu,
            ui_ignore_label=self.ui_ignore_label,
        )

    def _teardown_ui_ignore(self):
        import bl_app_override
        bl_app_override.ui_draw_filter_unregister(
            self._ui_ignore_store
        )
        self._ui_ignore_store = None

    def _setup_addons(self):
        import sys

        sys_path = []
        if self.addon_paths is not None:
            for path in self.addon_paths():
                if path not in sys.path:
                    sys.path.append(path)

        import addon_utils
        addons = []
        if self.addons is not None:
            addons.extend(self.addons())
            for addon in addons:
                addon_utils.enable(addon)

        self._addon_store = {
            "sys_path": sys_path,
            "addons": addons,
        }

    def _teardown_addons(self):
        import sys

        sys_path = self._addon_store["sys_path"]
        for path in sys_path:
            # should always succeed, but if not it doesn't matter
            # (someone else was changing the sys.path), ignore!
            try:
                sys.path.remove(path)
            except:
                pass

        addons = self._addon_store["addons"]
        import addon_utils
        for addon in addons:
            addon_utils.disable(addon)

        self._addon_store.clear()
        self._addon_store = None

    def setup(self):
        if self.class_ignore is not None:
            self._setup_classes()

        if any((self.addon_paths,
                self.addons,
                )):
            self._setup_addons()

        if any((self.ui_ignore_operator,
                self.ui_ignore_property,
                self.ui_ignore_menu,
                self.ui_ignore_label,
                )):
            self._setup_ui_ignore()

    def teardown(self):
        if self._class_store is not None:
            self._teardown_classes()

        if self._addon_store is not None:
            self._teardown_addons()

        if self._ui_ignore_store is not None:
            self._teardown_ui_ignore()
