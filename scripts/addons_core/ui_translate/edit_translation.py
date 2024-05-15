# SPDX-FileCopyrightText: 2012-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import shutil
if "bpy" in locals():
    import importlib
    importlib.reload(settings)
    importlib.reload(utils_i18n)
else:
    import bpy
    from bpy.types import Operator
    from bpy.props import (
        BoolProperty,
        EnumProperty,
        StringProperty,
    )
    from . import settings
    from bl_i18n_utils import utils as utils_i18n


# A global cache for I18nMessages objects, as parsing po files takes a few seconds.
PO_CACHE = {}


def _get_messages(lang, fname):
    if fname not in PO_CACHE:
        PO_CACHE[fname] = utils_i18n.I18nMessages(uid=lang, kind='PO', key=fname, src=fname, settings=settings.settings)
    return PO_CACHE[fname]


class UI_OT_i18n_edittranslation_update_mo(Operator):
    """Try to "compile" given po file into relevant blender.mo file"""
    """(WARNING: it will replace the official mo file in your user dir!)"""
    bl_idname = "ui.i18n_edittranslation_update_mo"
    bl_label = "Edit Translation Update Mo"

    # Operator Arguments
    lang: StringProperty(
        description="Current (translated) language",
        options={'SKIP_SAVE'},
    )

    po_file: StringProperty(
        description="Path to the matching po file",
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )

    clean_mo: BoolProperty(
        description="Remove all local translation files, to be able to use the system ones again",
        default=False,
        options={'SKIP_SAVE'}
    )
    # /End Operator Arguments

    def execute(self, context):
        if self.clean_mo:
            root = bpy.utils.user_resource('DATAFILES', path=settings.settings.MO_PATH_ROOT_RELATIVE)
            if root:
                shutil.rmtree(root)
        elif not (self.lang and self.po_file):
            return {'CANCELLED'}
        else:
            mo_dir = bpy.utils.user_resource(
                'DATAFILES',
                path=settings.settings.MO_PATH_TEMPLATE_RELATIVE.format(self.lang),
                create=True,
            )
            mo_file = os.path.join(mo_dir, settings.settings.MO_FILE_NAME)
            _get_messages(self.lang, self.po_file).write(kind='MO', dest=mo_file)

        bpy.ops.ui.reloadtranslation()
        return {'FINISHED'}


class UI_OT_i18n_edittranslation(Operator):
    """Translate the label and tooltip of the given property"""
    bl_idname = "ui.edittranslation"
    bl_label = "Edit Translation"

    # Operator Arguments
    but_label: StringProperty(
        description="Label of the control",
        options={'SKIP_SAVE'},
    )

    rna_label: StringProperty(
        description="RNA-defined label of the control, if any",
        options={'SKIP_SAVE'},
    )

    enum_label: StringProperty(
        description="Label of the enum item of the control, if any",
        options={'SKIP_SAVE'},
    )

    but_tip: StringProperty(
        description="Tip of the control",
        options={'SKIP_SAVE'},
    )

    rna_tip: StringProperty(
        description="RNA-defined tip of the control, if any",
        options={'SKIP_SAVE'},
    )

    enum_tip: StringProperty(
        description="Tip of the enum item of the control, if any",
        options={'SKIP_SAVE'},
    )

    rna_struct: StringProperty(
        description="Identifier of the RNA struct, if any",
        options={'SKIP_SAVE'},
    )

    rna_prop: StringProperty(
        description="Identifier of the RNA property, if any",
        options={'SKIP_SAVE'},
    )

    rna_enum: StringProperty(
        description="Identifier of the RNA enum item, if any",
        options={'SKIP_SAVE'},
    )

    rna_ctxt: StringProperty(
        description="RNA context for label",
        options={'SKIP_SAVE'},
    )

    lang: StringProperty(
        description="Current (translated) language",
        options={'SKIP_SAVE'},
    )

    po_file: StringProperty(
        description="Path to the matching po file",
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )

    # Found in po file.
    org_but_label: StringProperty(
        description="Original label of the control",
        options={'SKIP_SAVE'},
    )

    org_rna_label: StringProperty(
        description="Original RNA-defined label of the control, if any",
        options={'SKIP_SAVE'},
    )

    org_enum_label: StringProperty(
        description="Original label of the enum item of the control, if any",
        options={'SKIP_SAVE'},
    )

    org_but_tip: StringProperty(
        description="Original tip of the control",
        options={'SKIP_SAVE'},
    )

    org_rna_tip: StringProperty(
        description="Original RNA-defined tip of the control, if any", options={'SKIP_SAVE'}
    )

    org_enum_tip: StringProperty(
        description="Original tip of the enum item of the control, if any",
        options={'SKIP_SAVE'},
    )

    flag_items = (
        ('FUZZY', "Fuzzy", "Message is marked as fuzzy in po file"),
        ('ERROR', "Error", "Some error occurred with this message"),
    )

    but_label_flags: EnumProperty(
        description="Flags about the label of the button",
        items=flag_items,
        options={'SKIP_SAVE', 'ENUM_FLAG'},
    )

    rna_label_flags: EnumProperty(
        description="Flags about the RNA-defined label of the button",
        items=flag_items,
        options={'SKIP_SAVE', 'ENUM_FLAG'},
    )

    enum_label_flags: EnumProperty(
        description="Flags about the RNA enum item label of the button",
        items=flag_items,
        options={'SKIP_SAVE', 'ENUM_FLAG'},
    )

    but_tip_flags: EnumProperty(
        description="Flags about the tip of the button",
        items=flag_items,
        options={'SKIP_SAVE', 'ENUM_FLAG'},
    )

    rna_tip_flags: EnumProperty(
        description="Flags about the RNA-defined tip of the button",
        items=flag_items,
        options={'SKIP_SAVE', 'ENUM_FLAG'},
    )

    enum_tip_flags: EnumProperty(
        description="Flags about the RNA enum item tip of the button",
        items=flag_items,
        options={'SKIP_SAVE', 'ENUM_FLAG'},
    )

    stats_str: StringProperty(
        description="Stats from opened po", options={'SKIP_SAVE'})

    update_po: BoolProperty(
        description="Update po file, try to rebuild mo file, and refresh Blender's UI",
        default=False,
        options={'SKIP_SAVE'},
    )

    update_mo: BoolProperty(
        description="Try to rebuild mo file, and refresh Blender's UI",
        default=False,
        options={'SKIP_SAVE'},
    )

    clean_mo: BoolProperty(
        description="Remove all local translation files, to be able to use the system ones again",
        default=False,
        options={'SKIP_SAVE'},
    )
    # /End Operator Arguments

    def execute(self, context):
        if not hasattr(self, "msgmap"):
            self.report('ERROR', "invoke() needs to be called before execute()")
            return {'CANCELLED'}

        msgs = _get_messages(self.lang, self.po_file)
        done_keys = set()
        for mmap in self.msgmap.values():
            if 'ERROR' in getattr(self, mmap["msg_flags"]):
                continue
            k = mmap["key"]
            if k not in done_keys and len(k) == 1:
                k = tuple(k)[0]
                msgs.msgs[k].msgstr = getattr(self, mmap["msgstr"])
                msgs.msgs[k].is_fuzzy = 'FUZZY' in getattr(self, mmap["msg_flags"])
                done_keys.add(k)

        if self.update_po:
            # Try to overwrite .po file, may fail if there are no permissions.
            try:
                msgs.write(kind='PO', dest=self.po_file)
            except Exception as e:
                self.report('ERROR', "Could not write to po file ({})".format(str(e)))
            # Always invalidate reverse messages cache afterward!
            msgs.invalidate_reverse_cache()
        if self.update_mo:
            lang = os.path.splitext(os.path.basename(self.po_file))[0]
            bpy.ops.ui.i18n_edittranslation_update_mo(po_file=self.po_file, lang=lang)
        elif self.clean_mo:
            bpy.ops.ui.i18n_edittranslation_update_mo(clean_mo=True)
        return {'FINISHED'}

    def invoke(self, context, event):
        self.msgmap = {
            "but_label": {
                "msgstr": "but_label", "msgid": "org_but_label", "msg_flags": "but_label_flags", "key": set()},
            "rna_label": {
                "msgstr": "rna_label", "msgid": "org_rna_label", "msg_flags": "rna_label_flags", "key": set()},
            "enum_label": {
                "msgstr": "enum_label", "msgid": "org_enum_label", "msg_flags": "enum_label_flags", "key": set()},
            "but_tip": {
                "msgstr": "but_tip", "msgid": "org_but_tip", "msg_flags": "but_tip_flags", "key": set()},
            "rna_tip": {
                "msgstr": "rna_tip", "msgid": "org_rna_tip", "msg_flags": "rna_tip_flags", "key": set()},
            "enum_tip": {
                "msgstr": "enum_tip", "msgid": "org_enum_tip", "msg_flags": "enum_tip_flags", "key": set()},
        }

        msgs = _get_messages(self.lang, self.po_file)
        msgs.find_best_messages_matches(self, self.msgmap, self.rna_ctxt, self.rna_struct, self.rna_prop, self.rna_enum)
        msgs.update_info()
        self.stats_str = "{}: {} messages, {} translated.".format(os.path.basename(self.po_file), msgs.nbr_msgs,
                                                                  msgs.nbr_trans_msgs)

        for mmap in self.msgmap.values():
            k = tuple(mmap["key"])
            if k:
                if len(k) == 1:
                    k = k[0]
                    ctxt, msgid = k
                    setattr(self, mmap["msgstr"], msgs.msgs[k].msgstr)
                    setattr(self, mmap["msgid"], msgid)
                    if msgs.msgs[k].is_fuzzy:
                        setattr(self, mmap["msg_flags"], {'FUZZY'})
                else:
                    setattr(self, mmap["msgid"],
                            "ERROR: Button label “{}” matches several messages in po file ({})!"
                            "".format(self.but_label, k))
                    setattr(self, mmap["msg_flags"], {'ERROR'})
            else:
                setattr(self, mmap["msgstr"], "")
                setattr(self, mmap["msgid"], "")

        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)

    def draw(self, context):
        layout = self.layout
        layout.label(text=self.stats_str)
        src, _a, _b = bpy.utils.make_rna_paths(self.rna_struct, self.rna_prop, self.rna_enum)
        if src:
            layout.label(text="    RNA Path: bpy.types." + src)
        if self.rna_ctxt:
            layout.label(text="    RNA Context: " + self.rna_ctxt)

        if self.org_but_label or self.org_rna_label or self.org_enum_label:
            # XXX Can't use box, labels are not enough readable in them :/
            box = layout.box()
            box.label(text="Labels:")
            split = box.split(factor=0.15)
            col1 = split.column()
            col2 = split.column()
            if self.org_but_label:
                col1.label(text="Button Label:")
                row = col2.row()
                row.enabled = False
                if 'ERROR' in self.but_label_flags:
                    row.alert = True
                else:
                    col1.prop_enum(self, "but_label_flags", 'FUZZY', text="Fuzzy")
                    col2.prop(self, "but_label", text="")
                row.prop(self, "org_but_label", text="")
            if self.org_rna_label:
                col1.label(text="RNA Label:")
                row = col2.row()
                row.enabled = False
                if 'ERROR' in self.rna_label_flags:
                    row.alert = True
                else:
                    col1.prop_enum(self, "rna_label_flags", 'FUZZY', text="Fuzzy")
                    col2.prop(self, "rna_label", text="")
                row.prop(self, "org_rna_label", text="")
            if self.org_enum_label:
                col1.label(text="Enum Item Label:")
                row = col2.row()
                row.enabled = False
                if 'ERROR' in self.enum_label_flags:
                    row.alert = True
                else:
                    col1.prop_enum(self, "enum_label_flags", 'FUZZY', text="Fuzzy")
                    col2.prop(self, "enum_label", text="")
                row.prop(self, "org_enum_label", text="")

        if self.org_but_tip or self.org_rna_tip or self.org_enum_tip:
            # XXX Can't use box, labels are not enough readable in them :/
            box = layout.box()
            box.label(text="Tool Tips:")
            split = box.split(factor=0.15)
            col1 = split.column()
            col2 = split.column()
            if self.org_but_tip:
                col1.label(text="Button Tip:")
                row = col2.row()
                row.enabled = False
                if 'ERROR' in self.but_tip_flags:
                    row.alert = True
                else:
                    col1.prop_enum(self, "but_tip_flags", 'FUZZY', text="Fuzzy")
                    col2.prop(self, "but_tip", text="")
                row.prop(self, "org_but_tip", text="")
            if self.org_rna_tip:
                col1.label(text="RNA Tip:")
                row = col2.row()
                row.enabled = False
                if 'ERROR' in self.rna_tip_flags:
                    row.alert = True
                else:
                    col1.prop_enum(self, "rna_tip_flags", 'FUZZY', text="Fuzzy")
                    col2.prop(self, "rna_tip", text="")
                row.prop(self, "org_rna_tip", text="")
            if self.org_enum_tip:
                col1.label(text="Enum Item Tip:")
                row = col2.row()
                row.enabled = False
                if 'ERROR' in self.enum_tip_flags:
                    row.alert = True
                else:
                    col1.prop_enum(self, "enum_tip_flags", 'FUZZY', text="Fuzzy")
                    col2.prop(self, "enum_tip", text="")
                row.prop(self, "org_enum_tip", text="")

        row = layout.row()
        row.prop(self, "update_po", text="Save to PO File", toggle=True)
        row.prop(self, "update_mo", text="Rebuild MO File", toggle=True)
        row.prop(self, "clean_mo", text="Erase Local MO files", toggle=True)


classes = (
    UI_OT_i18n_edittranslation_update_mo,
    UI_OT_i18n_edittranslation,
)
