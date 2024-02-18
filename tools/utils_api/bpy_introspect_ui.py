#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This script dumps ui definitions as XML.
# useful for finding bad api usage.

"""
Example usage:

  python3 tools/utils_api/bpy_introspect_ui.py
"""

import sys
ModuleType = type(sys)


def module_add(name):
    mod = sys.modules[name] = ModuleType(name)
    return mod


class AttributeBuilder:
    """__slots__ = (
        "_attr", "_attr_list", "_item_set", "_args",
        "active", "operator_context", "enabled", "index", "data"
        )"""

    def _as_py(self):
        data = [self._attr_single, self._args, [child._as_py() for child in self._attr_list]]
        return data

    def _as_xml(self, indent="  "):

        def to_xml_str(value):
            if type(value) == str:
                # quick shoddy clean
                value = value.replace("&", " ")
                value = value.replace("<", " ")
                value = value.replace(">", " ")

                return '"' + value + '"'
            else:
                return '"' + str(value) + '"'

        def dict_to_kw(args, dict_args):
            args_str = ""
            if args:
                # args_str += " ".join([to_xml_str(a) for a in args])
                for i, a in enumerate(args):
                    args_str += "arg" + str(i + 1) + "=" + to_xml_str(a) + " "

            if dict_args:
                args_str += " ".join(["%s=%s" % (key, to_xml_str(value)) for key, value in sorted(dict_args.items())])

            if args_str:
                return " " + args_str

            return ""

        lines = []

        def py_to_xml(item, indent_ctx):
            if item._attr_list:
                lines.append("%s<%s%s>" % (indent_ctx, item._attr_single, dict_to_kw(item._args_tuple, item._args)))
                for child in item._attr_list:
                    # print(child._attr)
                    py_to_xml(child, indent_ctx + indent)
                lines.append("%s</%s>" % (indent_ctx, item._attr_single))
            else:
                lines.append("%s<%s%s/>" % (indent_ctx, item._attr_single, dict_to_kw(item._args_tuple, item._args)))

        py_to_xml(self, indent)

        return "\n".join(lines)

    def __init__(self, attr, attr_single):
        self._attr = attr
        self._attr_single = attr_single
        self._attr_list = []
        self._item_set = []
        self._args = {}
        self._args_tuple = ()

    def __call__(self, *args, **kwargs):
        # print(self._attr, args, kwargs)
        self._args_tuple = args
        self._args = kwargs
        return self

    def __getattr__(self, attr):
        attr_next = self._attr + "." + attr
        # Useful for debugging.
        # print(attr_next)
        attr_obj = _attribute_builder_overrides.get(attr_next, ...)
        if attr_obj is ...:
            attr_obj = NewAttr(attr_next, attr)
        self._attr_list.append(attr_obj)
        return attr_obj

    # def __setattr__(self, attr, value):
    #     pass

    def __getitem__(self, item):
        item_obj = NewAttr(self._attr + "[" + repr(item) + "]", item)
        self._item_set.append(item_obj)
        return item_obj

    def __setitem__(self, item, value):
        pass  # TODO?

    def __repr__(self):
        return self._attr

    def __iter__(self):
        return iter([])

    # def __len__(self):
    #     return 0

    def __int__(self):
        return 0

    def __cmp__(self, other):
        return -1

    def __lt__(self, other):
        return -1

    def __gt__(self, other):
        return -1

    def __le__(self, other):
        return -1

    def __add__(self, other):
        return self

    def __sub__(self, other):
        return self

    def __truediv__(self, other):
        return self

    def __floordiv__(self, other):
        return self

    def __round__(self, other):
        return self

    def __float__(self):
        return 0.0

    # Custom functions
    def lower(self):
        return ""

    def upper(self):
        return ""

    def keys(self):
        return []


class AttributeBuilder_Seq(AttributeBuilder):
    def __len__(self):
        return 0


_attribute_builder_overrides = {
    "context.gpencil.layers": AttributeBuilder_Seq("context.gpencil.layers", "layers"),
    "context.gpencil_data.layers": AttributeBuilder_Seq("context.gpencil_data.layers", "layers"),
    "context.object.material_slots": (),
    "context.selected_nodes": (),
    "context.selected_sequences": (),
    "context.space_data.bookmarks": (),
    "context.space_data.text.filepath": "",
    "context.preferences.filepaths.script_directory": "",
    "context.tool_settings.snap_elements": (True, ) * 3,
    "context.selected_objects": (),
    "context.tool_settings.mesh_select_mode": (True, ) * 3,
    "context.mode": 'PAINT_TEXTURE',
}


def NewAttr(attr, attr_single):
    obj = AttributeBuilder(attr, attr_single)
    return obj


def NewAttr_Seq(attr, attr_single):
    obj = AttributeBuilder_Seq(attr, attr_single)
    return obj


class BaseFakeUI:

    def __init__(self):
        self.layout = NewAttr("self.layout", "layout")


class Panel(BaseFakeUI):

    @property
    def is_popover(self):
        return False


class UIList:
    pass


class Header(BaseFakeUI):
    pass


class Menu(BaseFakeUI):

    def draw_preset(self, context):
        pass

    def path_menu(
            self, searchpaths, operator, *,
            props_default=None, prop_filepath="filepath",
            filter_ext=None, filter_path=None, display_name=None,
            add_operator=None,
    ):
        pass

    @classmethod
    def draw_collapsible(cls, context, layout):
        cls.draw(layout, context)

    @classmethod
    def is_extended(cls):
        return False


class Operator(BaseFakeUI):
    pass


class PropertyGroup:
    pass


# setup fake module
def fake_main():
    bpy = module_add("bpy")

    # Registerable Subclasses
    bpy.types = module_add("bpy.types")
    bpy.types.Panel = Panel
    bpy.types.Header = Header
    bpy.types.Menu = Menu
    bpy.types.UIList = UIList
    bpy.types.PropertyGroup = PropertyGroup
    bpy.types.Operator = Operator

    # ID Subclasses
    bpy.types.Armature = type("Armature", (), {})
    bpy.types.Brush = type("Brush", (), {})
    bpy.types.Brush.bl_rna = NewAttr("bpy.types.Brush.bl_rna", "bl_rna")
    bpy.types.Camera = type("Camera", (), {})
    bpy.types.Curve = type("Curve", (), {})
    bpy.types.GreasePencil = type("GreasePencil", (), {})
    bpy.types.Lattice = type("Lattice", (), {})
    bpy.types.Light = type("Light", (), {})
    bpy.types.Material = type("Material", (), {})
    bpy.types.Mesh = type("Mesh", (), {})
    bpy.types.MetaBall = type("MetaBall", (), {})
    bpy.types.Object = type("Object", (), {})
    bpy.types.Object.bl_rna = NewAttr("bpy.types.Object.bl_rna", "bl_rna")
    bpy.types.ParticleSettings = type("ParticleSettings", (), {})
    bpy.types.Scene = type("Scene", (), {})
    bpy.types.Sequence = type("Sequence", (), {})
    bpy.types.Speaker = type("Speaker", (), {})
    bpy.types.SurfaceCurve = type("SurfaceCurve", (), {})
    bpy.types.TextCurve = type("SurfaceCurve", (), {})
    bpy.types.Texture = type("Texture", (), {})
    bpy.types.WindowManager = type("WindowManager", (), {})
    bpy.types.WorkSpace = type("WorkSpace", (), {})
    bpy.types.World = type("World", (), {})

    # Other types
    bpy.types.Bone = type("Bone", (), {})
    bpy.types.EditBone = type("EditBone", (), {})
    bpy.types.Event = type("Event", (), {})
    bpy.types.Event.bl_rna = NewAttr("bpy.types.Event.bl_rna", "bl_rna")
    bpy.types.FreestyleLineStyle = type("FreestyleLineStyle", (), {})
    bpy.types.PoseBone = type("PoseBone", (), {})
    bpy.types.Theme = type("Theme", (), {})
    bpy.types.Theme.bl_rna = NewAttr("bpy.types.Theme.bl_rna", "bl_rna")
    bpy.types.ToolSettings = type("bpy.types.ToolSettings", (), {})
    bpy.types.ToolSettings.bl_rna = NewAttr("bpy.types.ToolSettings.bl_rna", "bl_rna")

    bpy.props = module_add("bpy.props")
    bpy.props.StringProperty = dict
    bpy.props.BoolProperty = dict
    bpy.props.BoolVectorProperty = dict
    bpy.props.IntProperty = dict
    bpy.props.EnumProperty = dict
    bpy.props.FloatProperty = dict
    bpy.props.FloatVectorProperty = dict
    bpy.props.CollectionProperty = dict

    bpy.app = module_add("bpy.app")
    bpy.app.use_userpref_skip_save_on_exit = False

    bpy.app.build_options = module_add("bpy.app.build_options")
    bpy.app.build_options.fluid = True
    bpy.app.build_options.freestyle = True
    bpy.app.build_options.mod_fluid = True
    bpy.app.build_options.collada = True
    bpy.app.build_options.international = True
    bpy.app.build_options.mod_smoke = True
    bpy.app.build_options.alembic = True
    bpy.app.build_options.bullet = True
    bpy.app.build_options.usd = True

    bpy.app.translations = module_add("bpy.app.translations")
    bpy.app.translations.pgettext_iface = lambda s, context="": s
    bpy.app.translations.pgettext_data = lambda s: s
    bpy.app.translations.pgettext_report = lambda s: s
    bpy.app.translations.pgettext_tip = lambda s: s
    # id's are chosen at random here...
    bpy.app.translations.contexts = module_add("bpy.app.translations.contexts")
    bpy.app.translations.contexts.default = "CONTEXT_DEFAULT"
    bpy.app.translations.contexts.operator_default = "CONTEXT_DEFAULT"
    bpy.app.translations.contexts.id_particlesettings = "CONTEXT_DEFAULT"
    bpy.app.translations.contexts.id_movieclip = "CONTEXT_ID_MOVIECLIP"
    bpy.app.translations.contexts.id_windowmanager = "CONTEXT_ID_WM"
    bpy.app.translations.contexts.plural = "CONTEXT_PLURAL"

    bpy.utils = module_add("bpy.utils")
    bpy.utils.register_class = lambda cls: ()
    bpy.utils.app_template_paths = lambda: ()


def fake_helper():

    class PropertyPanel:
        pass

    rna_prop_ui = module_add("rna_prop_ui")
    rna_prop_ui.PropertyPanel = PropertyPanel
    rna_prop_ui.draw = NewAttr("rna_prop_ui.draw", "draw")

    rigify = module_add("rigify")
    rigify.get_submodule_types = lambda: []


def fake_runtime():
    """Only call this before `draw()` functions."""

    # Misc Sub-classes
    bpy.types.EffectSequence = type("EffectSequence", (), {})

    # Operator Sub-classes.
    bpy.types.WM_OT_doc_view = type("WM_OT_doc_view", (), {"_prefix": ""})

    bpy.data = module_add("bpy.data")
    bpy.data.scenes = ()
    bpy.data.speakers = ()
    bpy.data.collections = ()
    bpy.data.meshes = ()
    bpy.data.shape_keys = ()
    bpy.data.materials = ()
    bpy.data.lattices = ()
    bpy.data.lights = ()
    bpy.data.lightprobes = ()
    bpy.data.fonts = ()
    bpy.data.textures = ()
    bpy.data.cameras = ()
    bpy.data.curves = ()
    bpy.data.linestyles = ()
    bpy.data.masks = ()
    bpy.data.metaballs = ()
    bpy.data.movieclips = ()
    bpy.data.armatures = ()
    bpy.data.particles = ()
    bpy.data.grease_pencils = ()
    bpy.data.cache_files = ()
    bpy.data.workspaces = ()

    bpy.data.is_dirty = True
    bpy.data.is_saved = True
    bpy.data.use_autopack = True

    # defined in fake_main()
    bpy.utils.smpte_from_frame = lambda f: ""
    bpy.utils.script_paths = lambda f: ()
    bpy.utils.user_resource = lambda a, b: ()

    bpy.app.debug = False
    bpy.app.version = 2, 55, 1
    bpy.app.autoexec_fail = False

    bpy.path = module_add("bpy.path")
    bpy.path.display_name = lambda f, has_ext=False: ""

    bpy_extras = module_add("bpy_extras")
    bpy_extras.keyconfig_utils = module_add("bpy_extras.keyconfig_utils")
    bpy_extras.keyconfig_utils.KM_HIERARCHY = ()
    bpy_extras.keyconfig_utils.keyconfig_merge = lambda a, b: ()

    addon_utils = module_add("addon_utils")
    # addon_utils.modules = lambda f: []

    def _(refresh=False):
        return ()
    addon_utils.modules = _
    del _
    addon_utils.modules_refresh = lambda f: None
    addon_utils.module_bl_info = lambda f: None
    addon_utils.addons_fake_modules = {}
    addon_utils.error_duplicates = ()
    addon_utils.error_encoding = ()


fake_main()
fake_helper()
# fake_runtime()  # call after initial import so we can catch
#                 # bad use of modules outside of draw() functions.

import bpy


def module_classes(mod):
    classes = []
    for value in mod.__dict__.values():
        try:
            is_subclass = issubclass(value, BaseFakeUI)
        except:
            is_subclass = False

        if is_subclass:
            classes.append(value)

    return classes


def main():

    import os
    BASE_DIR = os.path.join(os.path.dirname(__file__), "..", "..")
    BASE_DIR = os.path.normpath(os.path.abspath(BASE_DIR))
    MODULE_DIR_UI = os.path.join(BASE_DIR, "scripts", "startup")
    MODULE_DIR_MOD = os.path.join(BASE_DIR, "scripts", "modules")

    print("Using base dir: %r" % BASE_DIR)
    print("Using module dir: %r" % MODULE_DIR_UI)

    sys.path.insert(0, MODULE_DIR_UI)
    sys.path.insert(0, MODULE_DIR_MOD)

    scripts_dir = os.path.join(MODULE_DIR_UI, "bl_ui")
    for f in sorted(os.listdir(scripts_dir)):
        if f.endswith(".py") and not f.startswith("__init__"):
            # print(f)
            mod = __import__("bl_ui." + f[:-3]).__dict__[f[:-3]]

            classes = module_classes(mod)

            for cls in classes:
                setattr(bpy.types, cls.__name__, cls)

    fake_runtime()

    # print("running...")
    print("<ui>")
    for f in sorted(os.listdir(scripts_dir)):
        if f.endswith(".py") and not f.startswith("__init__"):
            # print(f)
            mod = __import__("bl_ui." + f[:-3]).__dict__[f[:-3]]

            classes = module_classes(mod)

            for cls in classes:
                # want to check if the draw function is directly in the class
                # print("draw")
                if "draw" in cls.__dict__:
                    self = cls()
                    self.draw(NewAttr("context", "context"))
                    # print(self.layout._as_py())
                    self.layout._args['id'] = mod.__name__ + "." + cls.__name__
                    print(self.layout._as_xml())
    print("</ui>")


if __name__ == "__main__":
    main()
