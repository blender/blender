# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""


def _string_search_property_cb(self, context, edit_text):
    return ["A", "B", "AB"]


def _test_string_prop_group_class():
    from bpy.props import (
        BoolProperty,
        StringProperty,
        CollectionProperty,
    )
    from bpy.types import (
        PropertyGroup,
        OperatorFileListElement,
    )

    class TestStringPropertyGroup(PropertyGroup):
        bool_prop: BoolProperty()

        string_prop: StringProperty()
        string_update_prop: StringProperty(options={'TEXTEDIT_UPDATE'})
        string_search_prop: StringProperty(search=_string_search_property_cb)
        string_search_prop_search: StringProperty(search=_string_search_property_cb)
        string_force_search_value_prop: StringProperty(search=_string_search_property_cb, search_options={'SORT'})
        prop_search_filter: CollectionProperty(type=OperatorFileListElement)

    return TestStringPropertyGroup


def _test_string_prop_button_panel_class():
    from bpy.types import Panel

    class UI_PT_string_property_buttons(Panel):
        bl_label = "Test String Property Buttons"
        bl_idname = "UI_PT_string_prop_button_test"
        bl_category = 'Test String Property Buttons'
        bl_space_type = 'TEXT_EDITOR'
        bl_region_type = 'UI'

        def draw(self, context):
            data = context.scene.test_property_group
            layout = self.layout

            layout.prop(data, "string_prop")

            layout.prop(data, "string_update_prop")

            # String Properties with search callback
            layout.prop(data, "string_search_prop")
            # String Properties with forced value by button
            layout.prop_search(data, "string_search_prop_search", data, "prop_search_filter")

            # String Properties with no search callback but with a collection filter
            layout.prop_search(data, "string_prop", data, "prop_search_filter")

            layout.prop(data, "string_force_search_value_prop")

    return UI_PT_string_property_buttons


def _register():
    import bpy
    classes = (_test_string_prop_group_class(), _test_string_prop_button_panel_class())
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.test_property_group = bpy.props.PointerProperty(type=classes[0])
    bpy.data.scenes['Scene'].test_property_group.prop_search_filter.add().name = "A"
    bpy.data.scenes['Scene'].test_property_group.prop_search_filter.add().name = "B"


def _test_window(windows_exclude=None):
    import bpy
    wm = bpy.data.window_managers[0]
    if windows_exclude is None:
        return wm.windows[0]
    for window in wm.windows:
        if window not in windows_exclude:
            return window
    return None


def _test_vars(window):
    import unittest
    from modules.easy_keys import EventGenerate
    return (
        EventGenerate(window),
        unittest.TestCase(),
    )


def _button_reset_value(wm, region, data, prop, t, e, expected_value, index=0):
    """Highlight button and reset its value with back_space"""
    t.assertTrue(wm.try_activate_rna_button(region, data, prop, 'HIGHLIGHT', index=index))
    yield e.back_space()
    t.assertEqual(getattr(data, prop), expected_value)


def test_string_buttons_interactions():
    _register()
    e, t = _test_vars(window := _test_window())

    area = window.screen.areas[3]
    t.assertEqual(area.type, 'VIEW_3D')
    area.type = 'TEXT_EDITOR'
    t.assertEqual(area.type, 'TEXT_EDITOR')

    area.spaces.active.show_region_ui = True
    yield

    t.assertTrue(area.spaces.active.show_region_ui)

    # Let UI to refresh so 'Test String Property Buttons' can be set as `ARegion::active_panel_category``
    yield

    region = area.regions[2]
    t.assertEqual(region.type, 'UI')
    region.active_panel_category = 'Test String Property Buttons'
    yield
    t.assertEqual(region.active_panel_category, 'Test String Property Buttons')

    import bpy
    data = bpy.data.scenes['Scene'].test_property_group
    wm = bpy.data.window_managers[0]

    # Fail to open bool property as 'TEXT_EDITING'.
    t.assertFalse(wm.try_activate_rna_button(region, data, "bool_prop", 'TEXT_EDITING'))

    # `StringProperty()` as Text button

    # Highlight button and open it with left click and type "123".
    xy = wm.try_activate_rna_button(region, data, "string_prop", 'HIGHLIGHT')
    t.assertTrue(xy)
    yield e.cursor_position_set(*xy, move=False)
    yield e.leftmouse().text("123")
    t.assertEqual(data.string_prop, "")
    yield e.ret()
    t.assertEqual(data.string_prop, "123")

    # Text edit button, type "áéíóúaeiou1234" and remove "1234".
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_prop", 'TEXT_EDITING'))
    yield e.back_space().text_unicode("áéíóúaeiou1234")
    yield e.shift.left_arrow().shift.left_arrow().shift.left_arrow().shift.left_arrow()
    yield e.back_space()
    t.assertEqual(data.string_prop, "123")
    yield e.ret()
    t.assertEqual(data.string_prop, "áéíóúaeiou")

    # Text edit button, type "a1a1a1" and paste it 3 times and undo once.
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_prop", 'TEXT_EDITING'))
    yield e.text("a1a1a1").ctrl.a().ctrl.c().left_arrow().ctrl.v().ctrl.v().ctrl.v().ctrl.z()
    t.assertEqual(data.string_prop, "áéíóúaeiou")
    yield e.ret()
    t.assertEqual(data.string_prop, "a1a1a1a1a1a1a1a1a1")

    # Highlight button and copy its "a1a1a1a1a1a1a1a1a1" content, reset its
    # value to "" and paste again "a1a1a1a1a1a1a1a1a1".
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_prop", 'HIGHLIGHT'))
    yield e.ctrl.c()
    yield from _button_reset_value(wm, region, data, "string_prop", t, e, "", 0)
    yield e.ctrl.v()
    t.assertEqual(data.string_prop, "a1a1a1a1a1a1a1a1a1")

    yield from _button_reset_value(wm, region, data, "string_prop", t, e, "", 0)

    # `StringProperty(options={'TEXTEDIT_UPDATE'})` as Text button

    # Highlight button and open it with left click and type "123", check value is updated without returning.
    xy = wm.try_activate_rna_button(region, data, "string_update_prop", 'HIGHLIGHT')
    t.assertTrue(xy)
    yield e.cursor_position_set(*xy, move=False)
    yield e.leftmouse().text("123")
    t.assertEqual(data.string_update_prop, "123")
    yield e.ret()
    t.assertEqual(data.string_update_prop, "123")

    # Text edit button, type "áéíóúaeiou1234" and remove "1234", check value is updated correctly on text change.
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_update_prop", 'TEXT_EDITING'))
    yield e.back_space().text_unicode("áéíóúaeiou1234")
    t.assertEqual(data.string_update_prop, "áéíóúaeiou1234")
    yield e.shift.left_arrow().shift.left_arrow().shift.left_arrow().shift.left_arrow().back_space()
    t.assertEqual(data.string_update_prop, "áéíóúaeiou")
    yield e.ret()
    t.assertEqual(data.string_update_prop, "áéíóúaeiou")

    # Text edit button, type "a1a1a1" and paste it 3 times and undo once,
    # check value is updated correctly on text change, but cancel this time
    # and check that value is restored correctly
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_update_prop", 'TEXT_EDITING'))
    yield e.text("a1a1a1").ctrl.a().ctrl.c().left_arrow().ctrl.v().ctrl.v().ctrl.v().ctrl.z()
    t.assertEqual(data.string_update_prop, "a1a1a1a1a1a1a1a1a1")
    yield e.esc()
    t.assertEqual(data.string_update_prop, "áéíóúaeiou")

    # Highlight button and copy its "áéíóúaeiou" content, reset its value to "" and paste again "áéíóúaeiou".
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_update_prop", 'HIGHLIGHT'))
    yield e.ctrl.c()
    yield from _button_reset_value(wm, region, data, "string_update_prop", t, e, "", 0)
    yield e.ctrl.v()
    t.assertEqual(data.string_update_prop, "áéíóúaeiou")

    yield from _button_reset_value(wm, region, data, "string_update_prop", t, e, "", 0)

    # `StringProperty(search=_string_search_property_cb)` as Search button

    # Highlight button and open it with left click and type "123", check value is updated without returning.
    xy = wm.try_activate_rna_button(region, data, "string_search_prop", 'HIGHLIGHT')
    t.assertTrue(xy)
    yield e.cursor_position_set(*xy, move=False)
    yield e.leftmouse().text("123")
    t.assertEqual(data.string_search_prop, "")
    yield e.ret()
    t.assertEqual(data.string_search_prop, "123")

    # Text edit button, type "áéíóúaeiou1234" and remove "1234", check value is updated correctly on text change.
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop", 'TEXT_EDITING'))
    yield e.back_space().text_unicode("áéíóúaeiou1234")
    t.assertEqual(data.string_search_prop, "123")
    yield e.shift.left_arrow().shift.left_arrow().shift.left_arrow().shift.left_arrow().back_space()
    t.assertEqual(data.string_search_prop, "123")
    yield e.ret()
    t.assertEqual(data.string_search_prop, "áéíóúaeiou")

    # Text edit button, type "a1a1a1" and paste it 3 times and undo once
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop", 'TEXT_EDITING'))
    yield e.text("a1a1a1").ctrl.a().ctrl.c().left_arrow().ctrl.v().ctrl.v().ctrl.v().ctrl.z()
    t.assertEqual(data.string_search_prop, "áéíóúaeiou")
    yield e.ret()
    t.assertEqual(data.string_search_prop, "a1a1a1a1a1a1a1a1a1")

    # Highlight button and copy its "a1a1a1a1a1a1a1a1a1" content, reset its
    # value to "" and paste again "a1a1a1a1a1a1a1a1a1".
    t.assertTrue(xy := wm.try_activate_rna_button(region, data, "string_search_prop", 'HIGHLIGHT'))
    yield e.ctrl.c()
    yield from _button_reset_value(wm, region, data, "string_search_prop", t, e, "", 0)
    yield e.ctrl.v()
    t.assertEqual(data.string_search_prop, "a1a1a1a1a1a1a1a1a1")

    # Open search button after pasting, search filter whill be already active,
    # Since there are no matching items, it will not select any value with arrow keys events
    yield e.cursor_position_set(*xy, move=False)
    yield e.leftmouse().down_arrow().ret()
    t.assertEqual(data.string_search_prop, "a1a1a1a1a1a1a1a1a1")

    # Open again search button and override text
    yield e.cursor_position_set(*xy, move=False)
    yield e.leftmouse().text_unicode("!äåéæœ").ret()
    t.assertEqual(data.string_search_prop, "!äåéæœ")

    # Text edit button and select the first search suggestion value
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop", 'TEXT_EDITING'))
    yield e.down_arrow().ret()
    t.assertEqual(data.string_search_prop, "A")

    # Text edit button and select the third search suggestion value ("A" is already selected in the search menu)
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop", 'TEXT_EDITING'))
    yield e.down_arrow().down_arrow().ret()
    t.assertEqual(data.string_search_prop, "AB")

    # Text edit button, type "A" and select the first search suggestion value after typing
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop", 'TEXT_EDITING'))
    yield e.text("A").down_arrow().ret()
    t.assertEqual(data.string_search_prop, "A")

    # Text edit button, type "A" and select the second search suggestion value
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop", 'TEXT_EDITING'))
    yield e.text("A").down_arrow().down_arrow().ret()
    t.assertEqual(data.string_search_prop, "AB")

    yield from _button_reset_value(wm, region, data, "string_search_prop", t, e, "", 0)

    # `StringProperty(search=_string_search_property_cb)` as `prop_search` button

    # Text edit button, type "C" and return, value will not be set as it don't matches a `prop_search` suggestion
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop_search", 'TEXT_EDITING'))
    yield e.text("C").ret()
    t.assertEqual(data.string_search_prop_search, "")

    # Text edit button, type "B" and return, value will be set since it matches a `prop_search` suggestion
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop_search", 'TEXT_EDITING'))
    yield e.text("B").ret()
    t.assertEqual(data.string_search_prop_search, "B")

    # Highlight button, pasted clipboard value which will not be set as it don't matches a `prop_search` suggestion
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_search_prop_search", 'HIGHLIGHT'))
    yield e.ctrl.v()
    t.assertEqual(data.string_search_prop_search, "B")

    yield from _button_reset_value(wm, region, data, "string_search_prop_search", t, e, "")

    # `StringProperty(search=_string_search_property_cb, search_options={'SORT'})` as Search button

    t.assertEqual(data.string_force_search_value_prop, "")
    # Text edit button, type "C" and return, value will not be set as it don't matches a suggestion
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_force_search_value_prop", 'TEXT_EDITING'))
    yield e.text("C").ret()
    t.assertEqual(data.string_force_search_value_prop, "")

    # Text edit button, type "A" and return
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_force_search_value_prop", 'TEXT_EDITING'))
    yield e.text("A").ret()
    t.assertEqual(data.string_force_search_value_prop, "A")

    # Text edit button, type "B" and return
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_force_search_value_prop", 'TEXT_EDITING'))
    yield e.text("B").ret()
    t.assertEqual(data.string_force_search_value_prop, "B")

    # Text edit button, type "A" and select second suggestion
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_force_search_value_prop", 'TEXT_EDITING'))
    yield e.text("A").down_arrow.ret()
    t.assertEqual(data.string_force_search_value_prop, "AB")

    # Highlight button, pasted clipboard value which will not be set as it don't matches a `prop_search` suggestion
    t.assertTrue(wm.try_activate_rna_button(region, data, "string_force_search_value_prop", 'HIGHLIGHT'))
    yield e.ctrl.v()
    t.assertEqual(data.string_force_search_value_prop, "AB")

    yield from _button_reset_value(wm, region, data, "string_force_search_value_prop", t, e, "")
