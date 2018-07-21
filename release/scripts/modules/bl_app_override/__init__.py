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
Module to manage overriding various parts of Blender.

Intended for use with 'app_templates', though it can be used from anywhere.
"""


# TODO, how to check these aren't from add-ons.
# templates might need to un-register while filtering.
def class_filter(cls_parent, **kw):
    whitelist = kw.pop("whitelist", None)
    blacklist = kw.pop("blacklist", None)
    kw_items = tuple(kw.items())
    for cls in cls_parent.__subclasses__():
        # same as is_registered()
        if "bl_rna" in cls.__dict__:
            if blacklist is not None and cls.__name__ in blacklist:
                continue
            if ((whitelist is not None and cls.__name__ is whitelist) or
                    all((getattr(cls, attr) in expect) for attr, expect in kw_items)):
                yield cls


def ui_draw_filter_register(
    *,
    ui_ignore_classes=None,
    ui_ignore_operator=None,
    ui_ignore_property=None,
    ui_ignore_menu=None,
    ui_ignore_label=None
):
    import bpy

    UILayout = bpy.types.UILayout

    if ui_ignore_classes is None:
        ui_ignore_classes = (
            bpy.types.Panel,
            bpy.types.Menu,
            bpy.types.Header,
        )

    class OperatorProperties_Fake:
        pass

    class UILayout_Fake(bpy.types.UILayout):
        __slots__ = ()

        def __getattribute__(self, attr):
            # ensure we always pass down UILayout_Fake instances
            if attr in {"row", "split", "column", "box", "column_flow"}:
                real_func = UILayout.__getattribute__(self, attr)

                def dummy_func(*args, **kw):
                    # print("wrapped", attr)
                    ret = real_func(*args, **kw)
                    return UILayout_Fake(ret)
                return dummy_func

            elif attr in {"operator", "operator_menu_enum", "operator_enum", "operator_menu_hold"}:
                if ui_ignore_operator is None:
                    return UILayout.__getattribute__(self, attr)

                real_func = UILayout.__getattribute__(self, attr)

                def dummy_func(*args, **kw):
                    # print("wrapped", attr)
                    ui_test = ui_ignore_operator(args[0])
                    if ui_test is False:
                        ret = real_func(*args, **kw)
                    else:
                        if ui_test is None:
                            UILayout.__getattribute__(self, "label")("")
                        else:
                            assert(ui_test is True)
                        # may need to be set
                        ret = OperatorProperties_Fake()
                    return ret
                return dummy_func

            elif attr in {"prop", "prop_enum"}:
                if ui_ignore_property is None:
                    return UILayout.__getattribute__(self, attr)

                real_func = UILayout.__getattribute__(self, attr)

                def dummy_func(*args, **kw):
                    # print("wrapped", attr)
                    ui_test = ui_ignore_property(args[0].__class__.__name__, args[1])
                    if ui_test is False:
                        ret = real_func(*args, **kw)
                    else:
                        if ui_test is None:
                            UILayout.__getattribute__(self, "label")("")
                        else:
                            assert(ui_test is True)
                        ret = None
                    return ret
                return dummy_func

            elif attr == "menu":
                if ui_ignore_menu is None:
                    return UILayout.__getattribute__(self, attr)

                real_func = UILayout.__getattribute__(self, attr)

                def dummy_func(*args, **kw):
                    # print("wrapped", attr)
                    ui_test = ui_ignore_menu(args[0])
                    if ui_test is False:
                        ret = real_func(*args, **kw)
                    else:
                        if ui_test is None:
                            UILayout.__getattribute__(self, "label")("")
                        else:
                            assert(ui_test is True)
                        ret = None
                    return ret
                return dummy_func

            elif attr == "label":
                if ui_ignore_label is None:
                    return UILayout.__getattribute__(self, attr)

                real_func = UILayout.__getattribute__(self, attr)

                def dummy_func(*args, **kw):
                    # print("wrapped", attr)
                    ui_test = ui_ignore_label(args[0] if args else kw.get("text", ""))
                    if ui_test is False:
                        ret = real_func(*args, **kw)
                    else:
                        if ui_test is None:
                            real_func("")
                        else:
                            assert(ui_test is True)
                        ret = None
                    return ret
                return dummy_func
            else:
                return UILayout.__getattribute__(self, attr)
            # print(self, attr)

        def operator(*args, **kw):
            return super().operator(*args, **kw)

    def draw_override(func_orig, self_real, context):
        cls_real = self_real.__class__
        if cls_real is super:
            # simple, no wrapping
            return func_orig(self_real, context)

        class Wrapper(cls_real):
            __slots__ = ()

            def __getattribute__(self, attr):
                if attr == "layout":
                    return UILayout_Fake(self_real.layout)
                else:
                    cls = super()
                    try:
                        return cls.__getattr__(self, attr)
                    except AttributeError:
                        # class variable
                        try:
                            return getattr(cls, attr)
                        except AttributeError:
                            # for preset bl_idname access
                            return getattr(UILayout(self), attr)

            @property
            def layout(self):
                # print("wrapped")
                return self_real.layout

        return func_orig(Wrapper(self_real), context)

    ui_ignore_store = []

    for cls in ui_ignore_classes:
        for subcls in list(cls.__subclasses__()):
            if "draw" in subcls.__dict__:  # don't want to get parents draw()

                def replace_draw():
                    # function also serves to hold draw_old in a local name-space
                    draw_orig = subcls.draw

                    def draw(self, context):
                        return draw_override(draw_orig, self, context)
                    subcls.draw = draw

                ui_ignore_store.append((subcls, "draw", subcls.draw))

                replace_draw()

    return ui_ignore_store


def ui_draw_filter_unregister(ui_ignore_store):
    for (obj, attr, value) in ui_ignore_store:
        setattr(obj, attr, value)
