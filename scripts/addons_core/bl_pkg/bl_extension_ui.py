# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
GUI (WARNING) this is a hack!
Written to allow a UI without modifying Blender.
"""

__all__ = (
    "display_errors",
    "register",
    "unregister",
)

import bpy

from bpy.types import (
    Menu,
    Panel,
)

from bl_ui.space_userpref import (
    USERPREF_PT_addons,
    USERPREF_PT_extensions,
    USERPREF_MT_extensions_active_repo,
)

# We may want to change these.
USE_EXTENSIONS_CHECKBOX = False
USE_EXTENSIONS_ADDON_PREFS = False
# TODO: choose how to show this when an add-on is an extension/core/legacy.
# The information is somewhat useful as you can only remove legacy add-ons from the add-ons sections.
# Whereas extensions must be removed from the extensions section.
# So without showing a distinction - the existance of these buttons is not clear.
USE_SHOW_ADDON_TYPE_AS_TEXT = True
USE_SHOW_ADDON_TYPE_AS_ICON = True


# -----------------------------------------------------------------------------
# Generic Utilities

def size_as_fmt_string(num: float, *, precision: int = 1) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"):
        if abs(num) < 1024.0:
            return "{:3.{:d}f}{:s}".format(num, precision, unit)
        num /= 1024.0
    unit = "yb"
    return "{:.{:d}f}{:s}".format(num, precision, unit)


def sizes_as_percentage_string(size_partial: int, size_final: int) -> str:
    if size_final == 0:
        percent = 0.0
    else:
        size_partial = min(size_partial, size_final)
        percent = size_partial / size_final

    return "{:-6.2f}%".format(percent * 100)


# -----------------------------------------------------------------------------
# Shared Utilities (Extensions / Legacy Add-ons)

def pkg_repo_and_id_from_theme_path(repos_all, filepath):
    import os
    if not filepath:
        return None

    # Strip the `theme.xml` filename.
    dirpath = os.path.dirname(filepath)
    repo_directory, pkg_id = os.path.split(dirpath)
    for repo_index, repo in enumerate(repos_all):
        if not os.path.samefile(repo_directory, repo.directory):
            continue
        return repo_index, pkg_id
    return None


def pkg_repo_module_prefix(repo):
    return "bl_ext.{:s}.".format(repo.module)


def module_parent_dirname(module_filepath):
    """
    Return the name of the directory above the module (it's name only).
    """
    import os
    parts = module_filepath.rsplit(os.sep, 3)
    # Unlikely.
    if not parts:
        return ""
    if parts[-1] == "__init__.py":
        if len(parts) >= 3:
            return parts[-3]
    else:
        if len(parts) >= 2:
            return parts[-2]
    return ""


def domain_extract_from_url(url):
    from urllib.parse import urlparse
    domain = urlparse(url).netloc

    return domain


def pkg_manifest_zip_all_items(pkg_manifest_local, pkg_manifest_remote):
    if pkg_manifest_remote is None:
        if pkg_manifest_local is not None:
            for pkg_id, item_local in pkg_manifest_local.items():
                yield pkg_id, (item_local, None)
        # If both are none, there are no items, that's OK.
        return
    elif pkg_manifest_local is None:
        for pkg_id, item_remote in pkg_manifest_remote.items():
            yield pkg_id, (None, item_remote)
        return

    assert (pkg_manifest_remote is not None) and (pkg_manifest_local is not None)
    pkg_manifest_local_copy = pkg_manifest_local.copy()
    for pkg_id, item_remote in pkg_manifest_remote.items():
        yield pkg_id, (pkg_manifest_local_copy.pop(pkg_id, None), item_remote)
    # Orphan packages (if they exist).
    for pkg_id, item_local in pkg_manifest_local_copy.items():
        yield pkg_id, (item_local, None)


# -----------------------------------------------------------------------------
# Add-ons UI

# While this is not a strict definition (internally they're just add-ons from different places),
# for the purposes of the UI it makes sense to differentiate add-ons this way because these add-on
# characteristics are mutually exclusive (there is no such thing as a user-core-extension for e.g.).

# Add-On Types:

# Any add-on which is an extension.
ADDON_TYPE_EXTENSION = 0
# Any add-on bundled with Blender (in the `addons_core` directory).
# These cannot be removed.
ADDON_TYPE_LEGACY_CORE = 1
# Any add-on from a user directory (add-ons from the users home directory or additional scripts directories).
ADDON_TYPE_LEGACY_USER = 2
# Any add-on which does not match any of the above characteristics.
# This is most likely from `os.path.join(bpy.utils.resource_path('LOCAL'), "scripts", "addons")`.
# In this context, the difference between this an any other add-on is not important,
# so there is no need to go to the effort of differentiating `LOCAL` from other kinds of add-ons.
# If instances of "Legacy (Other)" add-ons exist in a default installation, this may be an error,
# otherwise, it's not a problem if these occur with customized user-configurations.
ADDON_TYPE_LEGACY_OTHER = 3

addon_type_name = (
    "Extension",  # `ADDON_TYPE_EXTENSION`.
    "Core",  # `ADDON_TYPE_LEGACY_CORE`.
    "Legacy (User)",  # `ADDON_TYPE_LEGACY_USER`.
    "Legacy (Other)",  # `ADDON_TYPE_LEGACY_OTHER`.
)

addon_type_icon = (
    'COMMUNITY',  # `ADDON_TYPE_EXTENSION`.
    'BLENDER',  # `ADDON_TYPE_LEGACY_CORE`.
    'FILE_FOLDER',  # `ADDON_TYPE_LEGACY_USER`.
    'PACKAGE',  # `ADDON_TYPE_LEGACY_OTHER`.
)


# This function generalizes over displaying extension & legacy add-ons,
# (hence the many "item_*" arguments).
# Once support for legacy add-ons is dropped, the `item_` arguments
# can be replaced by a single `PkgManifest_Normalized` value.
def addon_draw_item_expanded(
        *,
        layout,  # `bpy.types.UILayout`
        mod,  # `ModuleType`
        addon_type,  # `int`
        is_enabled,  # `bool`
        # Expanded from both legacy add-ons & extensions.
        item_name,  # `str`
        item_description,  # `str`
        item_maintainer,  # `str`
        item_version,  # `str`
        item_warning_legacy,  # `str`
        item_doc_url,  # `str`
        item_tracker_url,  # `str`
):
    from bpy.app.translations import (
        pgettext_iface as iface_,
    )

    split = layout.split(factor=0.8)
    col_a = split.column()
    col_b = split.column()

    if item_description:
        col_a.label(
            text=" {:s}.".format(item_description),
            translate=False,
        )

    rowsub = col_b.row()
    rowsub.alignment = 'RIGHT'
    if addon_type == ADDON_TYPE_LEGACY_CORE:
        rowsub.active = False
        rowsub.label(text=iface_("Built-in"))
        rowsub.separator()
    elif addon_type == ADDON_TYPE_LEGACY_USER:
        rowsub.operator("preferences.addon_remove", text="Uninstall").module = mod.__name__
    del rowsub

    layout.separator(type='LINE')

    sub = layout.column()
    sub.active = is_enabled
    split = sub.split(factor=0.15)
    col_a = split.column()
    col_b = split.column()

    col_a.alignment = 'RIGHT'

    if item_doc_url:
        col_a.label(text="Website")
        col_b.split(factor=0.5).operator(
            "wm.url_open", text=domain_extract_from_url(item_doc_url), icon='HELP',
        ).url = item_doc_url
    # Only add "Report a Bug" button if tracker_url is set
    # or the add-on is bundled (use official tracker then).
    if item_tracker_url or (addon_type == ADDON_TYPE_LEGACY_CORE):
        col_a.label(text="Feedback")
        if item_tracker_url:
            col_b.split(factor=0.5).operator(
                "wm.url_open", text="Report a Bug", icon='URL',
            ).url = item_tracker_url
        else:
            addon_info = (
                "Name: {:s} {:s}\n"
                "Author: {:s}\n"
            ).format(item_name, item_version, item_maintainer)
            props = col_b.split(factor=0.5).operator(
                "wm.url_open_preset", text="Report a Bug", icon='URL',
            )
            props.type = 'BUG_ADDON'
            props.id = addon_info

    if USE_SHOW_ADDON_TYPE_AS_TEXT:
        col_a.label(text="Type")
        col_b.label(text=addon_type_name[addon_type])
    if item_maintainer:
        col_a.label(text="Maintainer")
        col_b.label(text=item_maintainer, translate=False)
    if item_version:
        col_a.label(text="Version")
        col_b.label(text=item_version, translate=False)
    if item_warning_legacy:
        # Only for legacy add-ons.
        col_a.label(text="Warning")
        col_b.label(text="  " + item_warning_legacy, icon='ERROR', translate=False)

    if addon_type != ADDON_TYPE_LEGACY_CORE:
        col_a.label(text="File")
        col_b.label(text=mod.__file__, translate=False)


# NOTE: this can be removed once upgrading from 4.1 is no longer relevant.
def addons_panel_draw_missing_with_extension_impl(
        *,
        context,  # `bpy.types.Context`
        layout,  # `bpy.types.UILayout`
        missing_modules  # `Set[str]`
):
    layout_header, layout_panel = layout.panel("builtin_addons", default_closed=True)
    layout_header.label(text="Missing Built-in Add-ons", icon='ERROR')

    if layout_panel is None:
        return

    prefs = context.preferences
    extensions_map_from_legacy_addons_ensure()

    repo_index = -1
    repo = None
    for repo_test_index, repo_test in enumerate(prefs.extensions.repos):
        if (
                repo_test.use_remote_url and
                (repo_test.remote_url.rstrip("/") == extensions_map_from_legacy_addons_url)
        ):
            repo_index = repo_test_index
            repo = repo_test
            break

    box = layout_panel.box()
    box.label(text="Add-ons previously shipped with Blender are now available from extensions.blender.org.")

    if repo is None:
        # Most likely the user manually removed this.
        box.label(text="Blender's extension repository not found!", icon='ERROR')
    elif not repo.enabled:
        box.label(text="Blender's extension repository must be enabled to install extensions!", icon='ERROR')
        repo_index = -1
    del repo

    for addon_module_name in sorted(missing_modules):
        # The `addon_pkg_id` may be an empty string, this signifies that it's not mapped to an extension.
        # The only reason to include it at all to avoid confusion because this *was* a previously built-in
        # add-on and this panel is titled "Built-in Add-ons".
        addon_pkg_id, addon_name = extensions_map_from_legacy_addons[addon_module_name]

        boxsub = box.column().box()
        colsub = boxsub.column()
        row = colsub.row()

        row_left = row.row()
        row_left.alignment = 'LEFT'

        row_left.label(text=addon_name, translate=False)

        row_right = row.row()
        row_right.alignment = 'RIGHT'

        if repo_index != -1 and addon_pkg_id:
            # NOTE: it's possible this extension is already installed.
            # the user could have installed it manually, then opened this popup.
            # This is enough of a corner case that it's not especially worth detecting
            # and communicating this particular state of affairs to the user.
            # Worst case, they install and it will re-install an already installed extension.
            props = row_right.operator("extensions.package_install", text="Install")
            props.repo_index = repo_index
            props.pkg_id = addon_pkg_id
            props.do_legacy_replace = True
            del props

        row_right.operator("preferences.addon_disable", text="", icon="X", emboss=False).module = addon_module_name


def addons_panel_draw_missing_impl(
        *,
        layout,  # `bpy.types.UILayout`
        missing_modules,  # `Set[str]`
):
    layout_header, layout_panel = layout.panel("missing_script_files", default_closed=True)
    layout_header.label(text="Missing Add-ons", icon='ERROR')

    if layout_panel is None:
        return

    box = layout_panel.box()
    for addon_module_name in sorted(missing_modules):

        boxsub = box.column().box()
        colsub = boxsub.column()
        row = colsub.row(align=True)

        row_left = row.row()
        row_left.alignment = 'LEFT'
        row_left.label(text=addon_module_name, translate=False)

        row_right = row.row()
        row_right.alignment = 'RIGHT'
        row_right.operator("preferences.addon_disable", text="", icon="X", emboss=False).module = addon_module_name


def addons_panel_draw_items(
        layout,  # `bpy.types.UILayout`
        context,  # `bpy.types.Context`
        *,
        addon_modules,  # `Dict[str, ModuleType]`
        used_addon_module_name_map,  # `Dict[str, bpy.types.Addon]`
        search_lower,  # `str`
        addon_tags_exclude,  # `Set[str]`
        enabled_only,  # `bool`
        addon_extension_manifest_map,  # `Dict[str, PkgManifest_Normalized]`
):
    # NOTE: this duplicates logic from `USERPREF_PT_addons` eventually this logic should be used instead.
    # Don't de-duplicate the logic as this is a temporary state - as long as extensions remains experimental.
    import addon_utils
    from .bl_extension_ops import (
        pkg_info_check_exclude_filter_ex,
    )

    # Initialized on demand.
    user_addon_paths = []

    for mod in addon_modules:
        module_name = mod.__name__
        is_enabled = module_name in used_addon_module_name_map
        if enabled_only and (not is_enabled):
            continue

        is_extension = addon_utils.check_extension(module_name)
        bl_info = addon_utils.module_bl_info(mod)
        show_expanded = bl_info["show_expanded"]

        if is_extension:
            del bl_info
            item_local = addon_extension_manifest_map.get(module_name)
            item_name = item_local.name
            item_description = item_local.tagline
            item_tags = item_local.tags
            item_warning_legacy = ""
            if show_expanded:
                item_maintainer = item_local.maintainer
                item_version = item_local.version
                item_doc_url = item_local.website
                item_tracker_url = ""
            del item_local
        else:
            item_name = bl_info["name"]
            item_description = bl_info["description"]
            item_tags = (bl_info["category"],)
            item_warning_legacy = bl_info["warning"]
            if show_expanded:
                item_maintainer = value.split("<", 1)[0].rstrip() if (value := bl_info["author"]) else ""
                item_version = ".".join(str(x) for x in value) if (value := bl_info["version"]) else ""
                item_doc_url = bl_info["doc_url"]
                item_tracker_url = bl_info.get("tracker_url")
            del bl_info

        if search_lower and (
                not pkg_info_check_exclude_filter_ex(
                    item_name,
                    item_description,
                    search_lower,
                )
        ):
            continue

        if addon_tags_exclude:
            if tags_exclude_match(item_tags, addon_tags_exclude):
                continue

        if is_extension:
            addon_type = ADDON_TYPE_EXTENSION
        elif module_parent_dirname(mod.__file__) == "addons_core":
            addon_type = ADDON_TYPE_LEGACY_CORE
        elif USERPREF_PT_addons.is_user_addon(mod, user_addon_paths):
            addon_type = ADDON_TYPE_LEGACY_USER
        else:
            addon_type = ADDON_TYPE_LEGACY_OTHER

        # Draw header.
        col_box = layout.column()
        box = col_box.box()
        colsub = box.column()
        row = colsub.row(align=True)

        row.operator(
            "preferences.addon_expand",
            icon='DOWNARROW_HLT' if show_expanded else 'RIGHTARROW',
            emboss=False,
        ).module = module_name

        row.operator(
            "preferences.addon_disable" if is_enabled else "preferences.addon_enable",
            icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT', text="",
            emboss=False,
        ).module = module_name

        sub = row.row()
        sub.active = is_enabled

        sub.label(text=" " + item_name, translate=False)

        if item_warning_legacy:
            sub.label(icon='ERROR')
        elif USE_SHOW_ADDON_TYPE_AS_ICON:
            sub.label(icon=addon_type_icon[addon_type])

        if show_expanded:
            addon_draw_item_expanded(
                layout=box,
                mod=mod,
                addon_type=addon_type,
                is_enabled=is_enabled,
                # Expanded from both legacy add-ons & extensions.
                item_name=item_name,
                item_description=item_description,
                item_maintainer=item_maintainer,
                item_version=item_version,
                item_warning_legacy=item_warning_legacy,
                item_doc_url=item_doc_url,
                item_tracker_url=item_tracker_url,
            )

            if is_enabled:
                if (addon_preferences := used_addon_module_name_map[module_name].preferences) is not None:
                    box.separator(type='LINE')
                    USERPREF_PT_addons.draw_addon_preferences(box, context, addon_preferences)


def addons_panel_draw_impl(
        self,
        context,  # `bpy.types.Context`
        search_lower,  # `str`
        addon_tags_exclude,  # `Set[str]`
        enabled_only,  # `bool`
):
    """
    Show all the items... we may want to paginate at some point.
    """
    import addon_utils
    from .bl_extension_ops import (
        extension_repos_read,
        repo_cache_store_refresh_from_prefs,
    )

    from . import repo_cache_store_ensure

    repo_cache_store = repo_cache_store_ensure()

    # This isn't elegant, but the preferences aren't available on registration.
    if not repo_cache_store.is_init():
        repo_cache_store_refresh_from_prefs(repo_cache_store)

    layout = self.layout

    prefs = context.preferences

    # Define a top-most column to place warnings (if-any).
    # Needed so the warnings aren't mixed in with other content.
    layout_topmost = layout.column()

    repos_all = extension_repos_read()

    # Collect exceptions accessing repositories, and optionally show them.
    errors_on_draw = []

    local_ex = None

    def error_fn_local(ex):
        nonlocal local_ex
        local_ex = ex

    addon_extension_manifest_map = {}

    for repo_index, pkg_manifest_local in enumerate(
            repo_cache_store.pkg_manifest_from_local_ensure(error_fn=error_fn_local)
    ):
        if pkg_manifest_local is None:
            continue

        repo_module_prefix = pkg_repo_module_prefix(repos_all[repo_index])
        for pkg_id, item_local in pkg_manifest_local.items():
            if item_local.type != "add-on":
                continue
            module_name = repo_module_prefix + pkg_id
            addon_extension_manifest_map[module_name] = item_local

    addon_modules = addon_utils.modules(refresh=False)
    used_addon_module_name_map = {addon.module: addon for addon in prefs.addons}

    addons_panel_draw_items(
        layout,
        context,
        addon_modules=addon_modules,
        used_addon_module_name_map=used_addon_module_name_map,
        search_lower=search_lower,
        addon_tags_exclude=addon_tags_exclude,
        enabled_only=enabled_only,
        addon_extension_manifest_map=addon_extension_manifest_map,
    )

    # Append missing scripts
    # First collect scripts that are used but have no script file.
    module_names = {mod.__name__ for mod in addon_modules}
    missing_modules = {
        addon_module_name for addon_module_name in used_addon_module_name_map
        if addon_module_name not in module_names
    }

    # NOTE: this can be removed once upgrading from 4.1 is no longer relevant.
    if missing_modules:
        # Split the missing modules into two groups, ones which can be upgraded and ones that can't.
        extensions_map_from_legacy_addons_ensure()

        missing_modules_with_extension = set()
        missing_modules_without_extension = set()

        for addon_module_name in missing_modules:
            if addon_module_name in extensions_map_from_legacy_addons:
                missing_modules_with_extension.add(addon_module_name)
            else:
                missing_modules_without_extension.add(addon_module_name)
        if missing_modules_with_extension:
            addons_panel_draw_missing_with_extension_impl(
                context=context,
                layout=layout_topmost,
                missing_modules=missing_modules_with_extension,
            )

        # Pretend none of these shenanigans ever occurred (to simplify removal).
        missing_modules = missing_modules_without_extension
    # End code-path for 4.1x migration.

    if missing_modules:
        addons_panel_draw_missing_impl(
            layout=layout_topmost,
            missing_modules=missing_modules,
        )

    # Finally show any errors in a single panel which can be dismissed.
    display_errors.errors_curr = errors_on_draw
    if errors_on_draw:
        display_errors.draw(layout_topmost)


def addons_panel_draw(panel, context):
    prefs = context.preferences
    view = prefs.view

    wm = context.window_manager
    layout = panel.layout

    split = layout.split(factor=0.5)
    row_a = split.row()
    row_b = split.row()
    row_a.prop(wm, "addon_search", text="", icon='VIEWZOOM')
    row_b.prop(view, "show_addons_enabled_only", text="Enabled Only")
    rowsub = row_b.row(align=True)

    rowsub.popover("USERPREF_PT_addons_tags", text="", icon='TAG')

    rowsub.separator()

    rowsub.menu("USERPREF_MT_addons_settings", text="", icon='DOWNARROW_HLT')
    del split, row_a, row_b, rowsub

    # Create a set of tags marked False to simplify exclusion & avoid it altogether when all tags are enabled.
    addon_tags_exclude = {k for (k, v) in wm.get("addon_tags", {}).items() if v is False}

    addons_panel_draw_impl(
        panel,
        context,
        wm.addon_search.lower(),
        addon_tags_exclude,
        view.show_addons_enabled_only,
    )


# -----------------------------------------------------------------------------
# Extensions UI

class display_errors:
    """
    This singleton class is used to store errors which are generated while drawing,
    note that these errors are reasonably obscure, examples are:
    - Failure to parse the repository JSON file.
    - Failure to access the file-system for reading where the repository is stored.

    The current and previous state are compared, when they match no drawing is done,
    this allows the current display errors to be dismissed.
    """
    errors_prev = []
    errors_curr = []

    @staticmethod
    def clear():
        display_errors.errors_prev = display_errors.errors_curr

    @staticmethod
    def draw(layout):
        if display_errors.errors_curr == display_errors.errors_prev:
            return
        box_header = layout.box()
        # Don't clip longer names.
        row = box_header.split(factor=0.9)
        row.label(text="Repository Access Errors:", icon='ERROR')
        rowsub = row.row(align=True)
        rowsub.alignment = 'RIGHT'
        rowsub.operator("extensions.status_clear_errors", text="", icon='X', emboss=False)

        box_contents = box_header.box()
        for err in display_errors.errors_curr:
            box_contents.label(text=err)


class notify_info:
    """
    This singleton class holds notification status.
    """
    # None: not yet started.
    # False: updates running.
    # True: updates complete.
    _update_state = None

    @staticmethod
    def update_ensure(repos):
        """
        Ensure updates are triggered if the preferences display extensions
        and an online sync has not yet run.

        Return true if updates are in progress.
        """
        in_progress = False
        match notify_info._update_state:
            case True:
                pass
            case None:
                from .bl_extension_notify import update_non_blocking
                # Assume nothing to do, finished.
                notify_info._update_state = True
                if repos_notify := [(repo, True) for repo in repos if repo.remote_url]:
                    if update_non_blocking(repos_fn=lambda: repos_notify):
                        # Correct assumption, starting.
                        notify_info._update_state = False
                        in_progress = True
            case False:
                from .bl_extension_notify import update_in_progress
                if update_in_progress():
                    # Not yet finished.
                    in_progress = True
                else:
                    notify_info._update_state = True
        return in_progress


def extensions_panel_draw_online_extensions_request_impl(
        self,
        _context,
):
    layout = self.layout
    layout_header, layout_panel = layout.panel("advanced", default_closed=False)
    layout_header.label(text="Online Extensions")

    if layout_panel is None:
        return

    box = layout_panel.box()

    # Text wrapping isn't supported, manually wrap.
    for line in (
            "Internet access is required to install and update online extensions. ",
            "You can adjust this later from \"System\" preferences.",
    ):
        box.label(text=line)

    row = box.row(align=True)
    row.alignment = 'LEFT'
    row.label(text="While offline, use \"Install from Disk\" instead.")
    # TODO: the URL must be updated before release,
    # this could be constructed using a function to account for Blender version & locale.
    row.operator(
        "wm.url_open",
        text="",
        icon='URL',
        emboss=False,
    ).url = "https://docs.blender.org/manual/en/dev/editors/preferences/extensions.html#install"

    row = box.row()
    props = row.operator("wm.context_set_boolean", text="Continue Offline", icon='X')
    props.data_path = "preferences.extensions.use_online_access_handled"
    props.value = True

    # The only reason to prefer this over `screen.userpref_show`
    # is it will be disabled when `--offline-mode` is forced with a useful error for why.
    row.operator("extensions.userpref_allow_online", text="Allow Online Access", icon='CHECKMARK')


extensions_map_from_legacy_addons = None
extensions_map_from_legacy_addons_url = None


# NOTE: this can be removed once upgrading from 4.1 is no longer relevant.
def extensions_map_from_legacy_addons_ensure():
    import os
    # pylint: disable-next=global-statement
    global extensions_map_from_legacy_addons, extensions_map_from_legacy_addons_url
    if extensions_map_from_legacy_addons is not None:
        return

    filepath = os.path.join(os.path.dirname(__file__), "extensions_map_from_legacy_addons.py")
    with open(filepath, "rb") as fh:
        # pylint: disable-next=eval-used
        data = eval(compile(fh.read(), filepath, "eval"), {})
    extensions_map_from_legacy_addons = data["extensions"]
    extensions_map_from_legacy_addons_url = data["remote_url"]


def extensions_map_from_legacy_addons_reverse_lookup(pkg_id):
    # Return the old name from the package ID.
    extensions_map_from_legacy_addons_ensure()
    for key_addon_module_name, (value_pkg_id, _) in extensions_map_from_legacy_addons.items():
        if pkg_id == value_pkg_id:
            return key_addon_module_name
    return ""


def extensions_panel_draw_impl(
        self,
        context,  # `bpy.types.Context`
        search_lower,   # `str`
        filter_by_type,  # `str`
        extension_tags_exclude,  # `Set[str]`
        enabled_only,  # `bool`
        updates_only,  # `bool`
        installed_only,  # `bool`
        show_development,  # `bool`
):
    """
    Show all the items... we may want to paginate at some point.
    """
    import os
    from bpy.app.translations import (
        pgettext_iface as iface_,
    )
    from .bl_extension_ops import (
        blender_extension_mark,
        blender_extension_show,
        extension_repos_read,
        pkg_info_check_exclude_filter,
        repo_cache_store_refresh_from_prefs,
    )

    from . import repo_cache_store_ensure

    repo_cache_store = repo_cache_store_ensure()

    # This isn't elegant, but the preferences aren't available on registration.
    if not repo_cache_store.is_init():
        repo_cache_store_refresh_from_prefs(repo_cache_store)

    layout = self.layout

    prefs = context.preferences

    if updates_only:
        installed_only = True

    # Define a top-most column to place warnings (if-any).
    # Needed so the warnings aren't mixed in with other content.
    layout_topmost = layout.column()

    repos_all = extension_repos_read()

    if bpy.app.online_access:
        if notify_info.update_ensure(repos_all):
            # TODO: should be part of the status bar.
            from .bl_extension_notify import update_ui_text
            text, icon = update_ui_text()
            if text:
                layout_topmost.box().label(text=text, icon=icon)
            del text, icon

    # To access enabled add-ons.
    show_addons = filter_by_type in {"", "add-on"}
    show_themes = filter_by_type in {"", "theme"}
    if show_addons:
        used_addon_module_name_map = {addon.module: addon for addon in prefs.addons}

    if show_themes:
        active_theme_info = pkg_repo_and_id_from_theme_path(repos_all, prefs.themes[0].filepath)

    # Collect exceptions accessing repositories, and optionally show them.
    errors_on_draw = []

    local_ex = None
    remote_ex = None

    def error_fn_local(ex):
        nonlocal local_ex
        local_ex = ex

    def error_fn_remote(ex):
        nonlocal remote_ex
        remote_ex = ex

    for repo_index, (
            pkg_manifest_local,
            pkg_manifest_remote,
    ) in enumerate(zip(
        repo_cache_store.pkg_manifest_from_local_ensure(error_fn=error_fn_local),
        repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=error_fn_remote),
        strict=True,
    )):
        # Show any exceptions created while accessing the JSON,
        # if the JSON has an IO error while being read or if the directory doesn't exist.
        # In general users should _not_ see these kinds of errors however we cannot prevent
        # IO errors in general and it is better to show a warning than to ignore the error entirely
        # or cause a trace-back which breaks the UI.
        if (remote_ex is not None) or (local_ex is not None):
            repo = repos_all[repo_index]
            # NOTE: `FileNotFoundError` occurs when a repository has been added but has not update with its remote.
            # We may want a way for users to know a repository is missing from the view and they need to run update
            # to access its extensions.
            if remote_ex is not None:
                if isinstance(remote_ex, FileNotFoundError) and (remote_ex.filename == repo.directory):
                    pass
                else:
                    errors_on_draw.append("Remote of \"{:s}\": {:s}".format(repo.name, str(remote_ex)))
                remote_ex = None

            if local_ex is not None:
                if isinstance(local_ex, FileNotFoundError) and (local_ex.filename == repo.directory):
                    pass
                else:
                    errors_on_draw.append("Local of \"{:s}\": {:s}".format(repo.name, str(local_ex)))
                local_ex = None
            continue

        has_remote = repos_all[repo_index].remote_url != ""
        if pkg_manifest_remote is None:
            if has_remote:
                # NOTE: it would be nice to detect when the repository ran sync and it failed.
                # This isn't such an important distinction though, the main thing users should be aware of
                # is that a "sync" is required.
                errors_on_draw.append(
                    "Repository: \"{:s}\" must sync with the remote repository.".format(
                        repos_all[repo_index].name,
                    )
                )
                continue

        # Read-only.
        is_system_repo = repos_all[repo_index].source == 'SYSTEM'

        repo_module_prefix = pkg_repo_module_prefix(repos_all[repo_index])

        for pkg_id, (item_local, item_remote) in pkg_manifest_zip_all_items(pkg_manifest_local, pkg_manifest_remote):
            item = item_local or item_remote
            if filter_by_type and (filter_by_type != item.type):
                continue
            if search_lower and (not pkg_info_check_exclude_filter(item, search_lower)):
                continue

            is_installed = item_local is not None

            if installed_only and (is_installed == 0):
                continue

            if extension_tags_exclude:
                if tags_exclude_match(item.tags, extension_tags_exclude):
                    continue

            is_addon = False
            is_theme = False
            match item.type:
                case "add-on":
                    is_addon = True
                case "theme":
                    is_theme = True

            if is_addon:
                if is_installed:
                    # Currently we only need to know the module name once installed.
                    addon_module_name = repo_module_prefix + pkg_id
                    # pylint: disable-next=possibly-used-before-assignment
                    is_enabled = addon_module_name in used_addon_module_name_map

                else:
                    is_enabled = False
                    addon_module_name = None
            elif is_theme:
                # pylint: disable-next=possibly-used-before-assignment
                is_enabled = (repo_index, pkg_id) == active_theme_info
                addon_module_name = None
            else:
                # TODO: ability to disable.
                is_enabled = is_installed
                addon_module_name = None

            if enabled_only and (not is_enabled):
                continue

            item_version = item.version
            if item_local is None:
                item_local_version = None
                is_outdated = False
            else:
                item_local_version = item_local.version
                is_outdated = item_local_version != item_version

            if updates_only:
                if not is_outdated:
                    continue

            key = (pkg_id, repo_index)
            if show_development:
                mark = key in blender_extension_mark
            show = key in blender_extension_show
            del key

            box = layout.box()

            # Left align so the operator text isn't centered.
            colsub = box.column()
            row = colsub.row(align=True)
            # row.label
            if show:
                props = row.operator("extensions.package_show_clear", text="", icon='DOWNARROW_HLT', emboss=False)
            else:
                props = row.operator("extensions.package_show_set", text="", icon='RIGHTARROW', emboss=False)
            props.pkg_id = pkg_id
            props.repo_index = repo_index
            del props

            if USE_EXTENSIONS_CHECKBOX:
                if is_installed:
                    if is_addon:
                        row.operator(
                            "preferences.addon_disable" if is_enabled else "preferences.addon_enable",
                            icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT',
                            text="",
                            emboss=False,
                        ).module = addon_module_name
                    elif is_theme:
                        props = row.operator(
                            "extensions.package_theme_disable" if is_enabled else "extensions.package_theme_enable",
                            icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT',
                            text="",
                            emboss=False,
                        )
                        props.repo_index = repo_index
                        props.pkg_id = pkg_id
                        del props
                    else:
                        # Use a place-holder checkbox icon to avoid odd text alignment
                        # when mixing with installed add-ons.
                        # Non add-ons have no concept of "enabled" right now, use installed.
                        row.operator(
                            "extensions.package_disabled",
                            text="",
                            icon='CHECKBOX_HLT',
                            emboss=False,
                        )
                else:
                    # Not installed, always placeholder.
                    row.operator(
                        "extensions.package_enable_not_installed",
                        text="",
                        icon='CHECKBOX_DEHLT',
                        emboss=False,
                    )

            if show_development:
                if mark:
                    props = row.operator("extensions.package_mark_clear", text="", icon='RADIOBUT_ON', emboss=False)
                else:
                    props = row.operator("extensions.package_mark_set", text="", icon='RADIOBUT_OFF', emboss=False)
                props.pkg_id = pkg_id
                props.repo_index = repo_index
                del props

            sub = row.row()
            sub.active = is_enabled
            if USE_EXTENSIONS_CHECKBOX:
                sub.label(text=item.name, translate=False)
            else:
                # Without checking `is_enabled` here, there is no way for the user to know if an extension
                # is enabled or not, which is useful to show - when they may be considering removing/updating
                # extensions based on them being used or not.
                sub.label(
                    text=(
                        item.name if (is_enabled or is_installed is False) else
                        item.name + iface_(" [disabled]")
                    ),
                    translate=False,
                )
            del sub

            row_right = row.row(align=True)
            row_right.alignment = 'RIGHT'

            if has_remote and (item_remote is not None):
                if is_installed:
                    # Include uninstall below.
                    if is_outdated:
                        props = row_right.operator("extensions.package_install", text="Update")
                        props.repo_index = repo_index
                        props.pkg_id = pkg_id
                        del props
                    else:
                        # Right space for alignment with the button.
                        row_right.label(text="Installed   ")
                        row_right.active = False
                else:
                    props = row_right.operator("extensions.package_install", text="Install")
                    props.repo_index = repo_index
                    props.pkg_id = pkg_id
                    del props
            else:
                # Right space for alignment with the button.
                if has_remote and (item_remote is None):
                    # There is a local item with no remote
                    row_right.label(text="Orphan")
                    row_right.label(text="", icon='ORPHAN_DATA')
                else:
                    row_right.label(text="Installed   ")

                row_right.active = False

            if show:
                col = box.column()

                row = col.row()

                # The full tagline may be multiple lines (not yet supported by Blender's UI).
                row.label(text=" {:s}.".format(item.tagline), translate=False)

                # Note that we could allow removing extensions from non-remote extension repos
                # although this is destructive, so don't enable this right now.
                if is_installed:
                    rowsub = row.row()
                    rowsub.alignment = 'RIGHT'
                    if is_system_repo:
                        rowsub.operator("extensions.package_uninstall_system", text="Uninstall")
                    else:
                        props = rowsub.operator("extensions.package_uninstall", text="Uninstall")
                        props.repo_index = repo_index
                        props.pkg_id = pkg_id
                        del props
                    del rowsub

                col.separator(type='LINE')
                del col

                col_info = box.column()
                col_info.active = is_enabled
                split = col_info.split(factor=0.15)
                col_a = split.column()
                col_b = split.column()
                col_a.alignment = "RIGHT"

                if value := item.website:
                    col_a.label(text="Website")
                    # Use half size button the full width button looks silly.
                    col_b.split(factor=0.5).operator(
                        "wm.url_open",
                        text=domain_extract_from_url(value),
                        translate=False,
                        icon='URL',
                    ).url = value
                del value

                col_a.alignment = "RIGHT"

                if is_addon:
                    col_a.label(text="Permissions")
                    # WARNING: while this is documented to be a dict, old packages may contain a list of strings.
                    # As it happens dictionary keys & list values both iterate over string,
                    # however we will want to show the dictionary values eventually.
                    if (value := item.permissions):
                        col_b.label(text=", ".join([iface_(x).title() for x in value]), translate=False)
                    else:
                        col_b.label(text="No permissions specified")
                    del value

                col_a.label(text="Maintainer")
                col_b.label(text=item.maintainer, translate=False)

                col_a.label(text="Version")
                if is_outdated:
                    col_b.label(
                        text=iface_("{:s} ({:s} available)").format(item_local_version, item_version),
                        translate=False,
                    )
                else:
                    col_b.label(text=item_version, translate=False)

                if has_remote and (item_remote is not None):
                    col_a.label(text="Size")
                    col_b.label(text=size_as_fmt_string(item_remote.archive_size), translate=False)

                col_a.label(text="License")
                col_b.label(text=item.license, translate=False)

                if len(repos_all) > 1:
                    col_a.label(text="Repository")
                    col_b.label(text=repos_all[repo_index].name, translate=False)

                if is_installed:
                    col_a.label(text="Path")
                    col_b.label(text=os.path.join(repos_all[repo_index].directory, pkg_id), translate=False)

                # Show addon user preferences.
                if USE_EXTENSIONS_ADDON_PREFS:
                    if is_enabled and is_addon:
                        if (addon_preferences := used_addon_module_name_map[addon_module_name].preferences) is not None:
                            USERPREF_PT_addons.draw_addon_preferences(box, context, addon_preferences)

    # Finally show any errors in a single panel which can be dismissed.
    display_errors.errors_curr = errors_on_draw
    if errors_on_draw:
        display_errors.draw(layout_topmost)


class USERPREF_PT_extensions_filter(Panel):
    bl_label = "Extensions Filter"

    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout

        wm = context.window_manager

        col = layout.column(heading="Show Only")
        col.use_property_split = True
        col.prop(wm, "extension_enabled_only", text="Enabled Extensions")
        col.prop(wm, "extension_updates_only", text="Updates Available")
        sub = col.column()
        sub.active = (not wm.extension_enabled_only) and (not wm.extension_updates_only)
        sub.prop(wm, "extension_installed_only", text="Installed Extensions")


class USERPREF_PT_addons_tags(Panel):
    bl_label = "Addon Tags"

    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'
    bl_ui_units_x = 13

    _wm_tags_attr = "addon_tags"

    def draw(self, _context):
        # Extended by the `bl_pkg` add-on.
        pass


class USERPREF_PT_extensions_tags(Panel):
    bl_label = "Extensions Tags"

    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'
    bl_ui_units_x = 13

    _wm_tags_attr = "extension_tags"

    def draw(self, _context):
        # Extended by the `bl_pkg` add-on.
        pass


class USERPREF_MT_addons_settings(Menu):
    bl_label = "Add-ons Settings"

    def draw(self, _context):
        layout = self.layout

        layout.operator("extensions.repo_refresh_all", text="Refresh Local", icon='FILE_REFRESH')

        layout.separator()

        layout.operator("extensions.package_install_files", text="Install from Disk...")


class USERPREF_MT_extensions_settings(Menu):
    bl_label = "Extension Settings"

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences

        layout.operator("extensions.repo_sync_all", icon='FILE_REFRESH')
        layout.operator("extensions.repo_refresh_all")

        layout.separator()

        layout.operator("extensions.package_upgrade_all", text="Install Available Updates", icon='IMPORT')
        layout.operator("extensions.package_install_files", text="Install from Disk...")

        if prefs.experimental.use_extensions_debug:

            layout.separator()

            layout.operator("extensions.package_install_marked", text="Install Marked", icon='IMPORT')
            layout.operator("extensions.package_uninstall_marked", text="Uninstall Marked", icon='X')
            layout.operator("extensions.package_obsolete_marked")

            layout.separator()

            layout.operator("extensions.repo_lock")
            layout.operator("extensions.repo_unlock")


def extensions_panel_draw(panel, context):
    from . import (
        repo_status_text,
    )

    prefs = context.preferences

    from bpy.app.translations import pgettext_iface as iface_
    from .bl_extension_ops import (
        blender_filter_by_type_map,
    )

    show_development = prefs.experimental.use_extensions_debug
    # This could be a separate option.
    show_development_reports = show_development

    wm = context.window_manager
    layout = panel.layout

    row = layout.split(factor=0.5)
    row_a = row.row()
    row_a.prop(wm, "extension_search", text="", icon='VIEWZOOM')
    row_b = row.row(align=True)
    row_b.prop(wm, "extension_type", text="")
    row_b.popover("USERPREF_PT_extensions_filter", text="", icon='FILTER')
    row_b.popover("USERPREF_PT_extensions_tags", text="", icon='TAG')

    row_b.separator()
    row_b.popover("USERPREF_PT_extensions_repos", text="Repositories")

    row_b.separator()
    row_b.menu("USERPREF_MT_extensions_settings", text="", icon='DOWNARROW_HLT')
    del row, row_a, row_b

    if show_development_reports:
        show_status = bool(repo_status_text.log)
    else:
        # Only show if running and there is progress to display.
        show_status = bool(repo_status_text.log) and repo_status_text.running
        if show_status:
            show_status = False
            for ty, msg in repo_status_text.log:
                if ty == 'PROGRESS':
                    show_status = True

    if show_status:
        box = layout.box()
        # Don't clip longer names.
        row = box.split(factor=0.9, align=True)
        if repo_status_text.running:
            row.label(text=iface_(repo_status_text.title) + "...", icon='INFO', translate=False)
        else:
            row.label(text=repo_status_text.title, icon='INFO')
        if show_development_reports:
            rowsub = row.row(align=True)
            rowsub.alignment = 'RIGHT'
            rowsub.operator("extensions.status_clear", text="", icon='X', emboss=False)
        boxsub = box.box()
        for ty, msg in repo_status_text.log:
            if ty == 'STATUS':
                boxsub.label(text=msg)
            elif ty == 'PROGRESS':
                msg_str, progress_unit, progress, progress_range = msg
                if progress <= progress_range:
                    boxsub.progress(
                        factor=progress / progress_range,
                        text="{:s}, {:s}".format(
                            sizes_as_percentage_string(progress, progress_range),
                            iface_(msg_str),
                        ),
                        translate=False,
                    )
                elif progress_unit == 'BYTE':
                    boxsub.progress(
                        factor=0.0,
                        text="{:s}, {:s}".format(iface_(msg_str), size_as_fmt_string(progress)),
                        translate=False,
                    )
                else:
                    # We might want to support other types.
                    boxsub.progress(
                        factor=0.0,
                        text="{:s}, {:d}".format(iface_(msg_str), progress),
                        translate=False,
                    )
            else:
                boxsub.label(
                    text="{:s}: {:s}".format(iface_(ty), iface_(msg)),
                    translate=False,
                )

        # Hide when running.
        if repo_status_text.running:
            return

    # Check if the extensions "Welcome" panel should be displayed.
    # Even though it can be dismissed it's quite "in-your-face" so only show when it's needed.
    if (
            # The user didn't dismiss.
            (not prefs.extensions.use_online_access_handled) and
            # Running offline.
            (not bpy.app.online_access) and
            # There is one or more repositories that require remote access.
            any(repo for repo in prefs.extensions.repos if repo.enabled and repo.use_remote_url)
    ):
        extensions_panel_draw_online_extensions_request_impl(panel, context)

    # Create a set of tags marked False to simplify exclusion & avoid it altogether when all tags are enabled.
    extension_tags_exclude = {k for (k, v) in wm.get("extension_tags", {}).items() if v is False}

    extensions_panel_draw_impl(
        panel,
        context,
        wm.extension_search.lower(),
        blender_filter_by_type_map[wm.extension_type],
        extension_tags_exclude,
        wm.extension_enabled_only,
        wm.extension_updates_only,
        wm.extension_installed_only,
        show_development,
    )


def extensions_repo_active_draw(self, _context):
    # Draw icon buttons on the right hand side of the UI-list.
    from . import repo_active_or_none
    layout = self.layout

    # Allow the poll functions to only check against the active repository.
    if (repo := repo_active_or_none()) is not None:
        layout.context_pointer_set("extension_repo", repo)

    layout.operator("extensions.repo_sync_all", text="", icon='FILE_REFRESH').use_active_only = True

    layout.operator("extensions.package_upgrade_all", text="", icon='IMPORT').use_active_only = True


# -----------------------------------------------------------------------------
# Shared (Extension / Legacy Add-ons) Tags Logic

def tags_exclude_match(
        item_tags,  # `Tuple[str]`
        exclude_tags,  # `Set[str]`
):
    if not item_tags:
        # When an item has no tags then including it makes no sense
        # since this item logically can't match any of the enabled tags.
        # skip items with no empty tags - when tags are being filtered.
        return True

    for tag in item_tags:
        # Any tag not in `exclude_tags` is assumed true.
        # This works because `exclude_tags` is a complete list of all tags.
        if tag not in exclude_tags:
            return False
    return True


def tags_current(wm, tags_attr):
    from .bl_extension_ops import (
        blender_filter_by_type_map,
        extension_repos_read,
        repo_cache_store_refresh_from_prefs,
    )

    from . import repo_cache_store_ensure

    prefs = bpy.context.preferences

    repo_cache_store = repo_cache_store_ensure()

    # This isn't elegant, but the preferences aren't available on registration.
    if not repo_cache_store.is_init():
        repo_cache_store_refresh_from_prefs(repo_cache_store)

    if tags_attr == "addon_tags":
        filter_by_type = "add-on"
        only_enabled = prefs.view.show_addons_enabled_only
    else:
        filter_by_type = blender_filter_by_type_map[wm.extension_type]
        only_enabled = wm.extension_enabled_only

    # Currently only add-ons can make use of enabled by type (usefully) for tags.
    if only_enabled and (filter_by_type == "add-on"):
        addons_enabled = {addon.module for addon in prefs.addons}

    repos_all = extension_repos_read()

    tags = set()

    for repo_index, (
            pkg_manifest_local,
            pkg_manifest_remote,
    ) in enumerate(zip(
        repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print),
        repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print) if (tags_attr != "addon_tags") else
        # For add-ons display there is never any need for "remote" items,
        # simply expand to None here to avoid duplicating the body of this for-loop.
        ((None,) * len(repos_all)),
        strict=True,
    )):
        if only_enabled:
            if filter_by_type == "add-on":
                repo_module_prefix = pkg_repo_module_prefix(repos_all[repo_index])

        for pkg_id, (item_local, item_remote) in pkg_manifest_zip_all_items(pkg_manifest_local, pkg_manifest_remote):
            item = item_local or item_remote

            # Filter using `filter_by_type`.
            if filter_by_type != item.type:
                continue

            # Filter using `Only Enabled`.
            # NOTE: this is only supported by add-ons currently.
            # This could be made to work for themes too however there is only ever one enabled theme at a time.
            # The use case for that is weak at best. "Only Enabled" can be supported by other types as needed.
            if only_enabled:
                if filter_by_type == "add-on":
                    if item_local is not None:
                        addon_module_name = repo_module_prefix + pkg_id
                        if addon_module_name not in addons_enabled:
                            continue
            if pkg_tags := item.tags:
                tags.update(pkg_tags)

    # Only for the add-ons view (extension's doesn't show legacy add-ons).
    if tags_attr == "addon_tags":
        # Legacy add-on categories as tags.
        import addon_utils
        addon_modules = addon_utils.modules(refresh=False)
        for mod in addon_modules:
            module_name = mod.__name__
            is_extension = addon_utils.check_extension(module_name)
            if is_extension:
                continue
            if only_enabled:  # No need to check `filter_by_type` here.
                if module_name not in addons_enabled:
                    continue
            bl_info = addon_utils.module_bl_info(mod)
            if t := bl_info.get("category"):
                tags.add(t)

    return tags


def tags_refresh(wm, tags_attr):
    import idprop
    tags_idprop = wm.get(tags_attr)
    if isinstance(tags_idprop, idprop.types.IDPropertyGroup):
        pass
    else:
        wm[tags_attr] = {}
        tags_idprop = wm[tags_attr]

    tags_curr = set(tags_idprop.keys())

    # Calculate tags.
    tags_next = tags_current(wm, tags_attr)

    tags_to_add = tags_next - tags_curr
    tags_to_rem = tags_curr - tags_next

    for tag in tags_to_rem:
        del tags_idprop[tag]
    for tag in tags_to_add:
        tags_idprop[tag] = True

    return list(sorted(tags_next))


def tags_panel_draw(panel, context):
    tags_attr = panel._wm_tags_attr
    from bpy.utils import escape_identifier
    from bpy.app.translations import contexts as i18n_contexts
    layout = panel.layout
    wm = context.window_manager
    tags_sorted = tags_refresh(wm, tags_attr)
    layout.label(text="Show Tags")
    # Add one so the first row is longer in the case of an odd number.
    tags_len_half = (len(tags_sorted) + 1) // 2
    split = layout.split(factor=0.5)
    col = split.column()
    for i, t in enumerate(sorted(tags_sorted)):
        if i == tags_len_half:
            col = split.column()
        col.prop(
            getattr(wm, tags_attr),
            "[\"{:s}\"]".format(escape_identifier(t)),
            text=t,
            text_ctxt=i18n_contexts.editor_preferences,
        )


# -----------------------------------------------------------------------------
# Registration

classes = (
    # Pop-overs.
    USERPREF_PT_addons_tags,
    USERPREF_MT_addons_settings,

    USERPREF_PT_extensions_filter,
    USERPREF_PT_extensions_tags,
    USERPREF_MT_extensions_settings,
)


def register():
    USERPREF_PT_addons.append(addons_panel_draw)
    USERPREF_PT_extensions.append(extensions_panel_draw)
    USERPREF_PT_addons_tags.append(tags_panel_draw)
    USERPREF_PT_extensions_tags.append(tags_panel_draw)
    USERPREF_MT_extensions_active_repo.append(extensions_repo_active_draw)

    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    USERPREF_PT_addons.remove(addons_panel_draw)
    USERPREF_PT_extensions.remove(extensions_panel_draw)
    USERPREF_PT_extensions_tags.remove(tags_panel_draw)
    USERPREF_PT_addons_tags.remove(tags_panel_draw)
    USERPREF_MT_extensions_active_repo.remove(extensions_repo_active_draw)

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
