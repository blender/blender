# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Tests for `source/blender/python/intern/bpy_operator_wrap.cc`
# Python wrapping the operator system (using subclasses).
#
# ./blender.bin --background --python tests/python/bl_operator_wrap_py_api.py -- --verbose

__all__ = (
    "main",
)

import contextlib
import unittest

import bpy


@contextlib.contextmanager
def registered_operator(cls):
    bpy.utils.register_class(cls)
    try:
        yield cls
    finally:
        bpy.utils.unregister_class(cls)


class TestOperatorRegister(unittest.TestCase):

    def test_register_and_unregister(self):
        class TESTING_OT_stub(bpy.types.Operator):
            bl_idname = "testing.stub"
            bl_label = "Stub"

            def execute(self, context):
                return {'FINISHED'}

        bpy.utils.register_class(TESTING_OT_stub)
        self.assertTrue(bpy.ops.testing.stub.poll())
        bpy.utils.unregister_class(TESTING_OT_stub)
        with self.assertRaises(AttributeError):
            bpy.ops.testing.stub.poll()

    def test_execute(self):
        class TESTING_OT_exec(bpy.types.Operator):
            bl_idname = "testing.exec"
            bl_label = "Exec"

            def execute(self, context):
                return {'FINISHED'}

        with registered_operator(TESTING_OT_exec):
            self.assertEqual(bpy.ops.testing.exec(), {'FINISHED'})

    def test_execute_cancelled(self):
        class TESTING_OT_cancel(bpy.types.Operator):
            bl_idname = "testing.cancel"
            bl_label = "Cancel"

            def execute(self, context):
                return {'CANCELLED'}

        with registered_operator(TESTING_OT_cancel):
            self.assertEqual(bpy.ops.testing.cancel(), {'CANCELLED'})

    def test_poll(self):
        class TESTING_OT_poll_pass(bpy.types.Operator):
            bl_idname = "testing.poll_pass"
            bl_label = "Poll Pass"

            @classmethod
            def poll(cls, context):
                return True

            def execute(self, context):
                return {'FINISHED'}

        with registered_operator(TESTING_OT_poll_pass):
            self.assertTrue(bpy.ops.testing.poll_pass.poll())
            self.assertEqual(bpy.ops.testing.poll_pass(), {'FINISHED'})

    def test_poll_fails(self):
        class TESTING_OT_poll_fail(bpy.types.Operator):
            bl_idname = "testing.poll_fail"
            bl_label = "Poll Fail"

            @classmethod
            def poll(cls, context):
                return False

            def execute(self, context):
                return {'FINISHED'}

        with registered_operator(TESTING_OT_poll_fail):
            self.assertFalse(bpy.ops.testing.poll_fail.poll())
            with self.assertRaises(RuntimeError):
                bpy.ops.testing.poll_fail()


class TestOperatorRegisterInvalid(unittest.TestCase):

    def test_missing_bl_idname(self):
        class TESTING_OT_no_id(bpy.types.Operator):
            bl_label = "No ID"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(RuntimeError) as ex:
            bpy.utils.register_class(TESTING_OT_no_id)
        self.assertEqual(
            str(ex.exception),
            "Error: Registering operator class 'TESTING_OT_no_id', "
            "invalid bl_idname 'TESTING_OT_no_id', Invalid character at position 0\n",
        )

    def test_missing_bl_label(self):
        class TESTING_OT_no_label(bpy.types.Operator):
            bl_idname = "testing.no_label"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(AttributeError) as ex:
            bpy.utils.register_class(TESTING_OT_no_label)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_no_label class to have an "bl_label" attribute',
        )

    def test_double_register(self):
        class TESTING_OT_double(bpy.types.Operator):
            bl_idname = "testing.double"
            bl_label = "Double"

            def execute(self, context):
                return {'FINISHED'}

        bpy.utils.register_class(TESTING_OT_double)
        self.addCleanup(bpy.utils.unregister_class, TESTING_OT_double)
        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_double)
        self.assertEqual(
            str(ex.exception),
            "register_class(...): already registered as a subclass 'TESTING_OT_double'",
        )

    def test_unregister_not_registered(self):
        class TESTING_OT_not_reg(bpy.types.Operator):
            bl_idname = "testing.not_reg"
            bl_label = "Not Reg"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(RuntimeError) as ex:
            bpy.utils.unregister_class(TESTING_OT_not_reg)
        self.assertEqual(
            str(ex.exception),
            "unregister_class(...):, missing bl_rna attribute from '_RNAMeta' instance (may not be registered)",
        )

    def test_bl_idname_no_dot(self):
        class TESTING_OT_nodot(bpy.types.Operator):
            bl_idname = "testingnodot"
            bl_label = "No Dot"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(RuntimeError) as ex:
            bpy.utils.register_class(TESTING_OT_nodot)
        self.assertEqual(
            str(ex.exception),
            "Error: Registering operator class 'TESTING_OT_nodot', "
            "invalid bl_idname 'testingnodot', Must contain 1 '.' character\n",
        )

    def test_bl_idname_empty(self):
        class TESTING_OT_empty(bpy.types.Operator):
            bl_idname = ""
            bl_label = "Empty"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(RuntimeError) as ex:
            bpy.utils.register_class(TESTING_OT_empty)
        self.assertEqual(
            str(ex.exception),
            "Error: Registering operator class 'TESTING_OT_empty', invalid bl_idname '', Must contain 1 '.' character\n",
        )

    def test_bl_idname_uppercase(self):
        class TESTING_OT_upper(bpy.types.Operator):
            bl_idname = "testing.Upper"
            bl_label = "Upper"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(RuntimeError) as ex:
            bpy.utils.register_class(TESTING_OT_upper)
        self.assertEqual(
            str(ex.exception),
            "Error: Registering operator class 'TESTING_OT_upper', "
            "invalid bl_idname 'testing.Upper', Invalid character at position 8\n",
        )

    def test_bl_idname_spaces(self):
        class TESTING_OT_spaces(bpy.types.Operator):
            bl_idname = "testing.has spaces"
            bl_label = "Spaces"

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(RuntimeError) as ex:
            bpy.utils.register_class(TESTING_OT_spaces)
        self.assertEqual(
            str(ex.exception),
            "Error: Registering operator class 'TESTING_OT_spaces', "
            "invalid bl_idname 'testing.has spaces', Invalid character at position 11\n",
        )

    def test_bl_label_wrong_type(self):
        class TESTING_OT_intlabel(bpy.types.Operator):
            bl_idname = "testing.intlabel"
            bl_label = 123

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_intlabel)
        self.assertEqual(
            str(ex.exception),
            "validating class: Operator.bl_label expected a string type, not int",
        )


class TestOperatorRegisterCallbackSignature(unittest.TestCase):

    def test_execute_missing_arg(self):
        class TESTING_OT_exec_missing(bpy.types.Operator):
            bl_idname = "testing.exec_missing"
            bl_label = "Exec Missing"

            def execute(self):
                return {'FINISHED'}

        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_exec_missing)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_exec_missing class "execute" function to have 2 args, found 1',
        )

    def test_execute_extra_arg(self):
        class TESTING_OT_exec_extra(bpy.types.Operator):
            bl_idname = "testing.exec_extra"
            bl_label = "Exec Extra"

            def execute(self, context, extra):
                return {'FINISHED'}

        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_exec_extra)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_exec_extra class "execute" function to have 2 args, found 3',
        )

    def test_invoke_missing_arg(self):
        class TESTING_OT_invoke_missing(bpy.types.Operator):
            bl_idname = "testing.invoke_missing"
            bl_label = "Invoke Missing"

            def invoke(self, context):
                return {'FINISHED'}

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_invoke_missing)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_invoke_missing class "invoke" function to have 3 args, found 2',
        )

    def test_draw_missing_arg(self):
        class TESTING_OT_draw_missing(bpy.types.Operator):
            bl_idname = "testing.draw_missing"
            bl_label = "Draw Missing"

            def draw(self):
                pass

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_draw_missing)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_draw_missing class "draw" function to have 2 args, found 1',
        )

    def test_poll_extra_arg(self):
        class TESTING_OT_poll_extra(bpy.types.Operator):
            bl_idname = "testing.poll_extra"
            bl_label = "Poll Extra"

            @classmethod
            def poll(cls, context, extra):
                return True

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_poll_extra)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_poll_extra class "poll" function to have 2 args, found 3',
        )


class TestOperatorRegisterCallbackWrongType(unittest.TestCase):

    def test_execute_as_classmethod(self):
        class TESTING_OT_exec_cls(bpy.types.Operator):
            bl_idname = "testing.exec_cls"
            bl_label = "Exec Cls"

            @classmethod  # Wrong: must be a regular `method`.
            def execute(cls, context):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_exec_cls)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_exec_cls class "execute" attribute to be a function, not a method',
        )

    def test_execute_as_staticmethod(self):
        class TESTING_OT_exec_static(bpy.types.Operator):
            bl_idname = "testing.exec_static"
            bl_label = "Exec Static"

            @staticmethod  # Wrong: must be a regular `method`.
            def execute(context):
                return {'FINISHED'}

        with self.assertRaises(ValueError) as ex:
            bpy.utils.register_class(TESTING_OT_exec_static)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_exec_static class "execute" function to have 2 args, found 1',
        )

    def test_execute_as_property(self):
        class TESTING_OT_exec_prop(bpy.types.Operator):
            bl_idname = "testing.exec_prop"
            bl_label = "Exec Prop"

            @property  # Wrong: must be a regular `method`.
            def execute(self):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_exec_prop)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_exec_prop class "execute" attribute to be a function, not a property',
        )

    def test_invoke_as_classmethod(self):
        class TESTING_OT_invoke_cls(bpy.types.Operator):
            bl_idname = "testing.invoke_cls"
            bl_label = "Invoke Cls"

            @classmethod  # Wrong: must be a regular `method`.
            def invoke(cls, context, event):
                return {'FINISHED'}

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_invoke_cls)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_invoke_cls class "invoke" attribute to be a function, not a method',
        )

    def test_poll_as_staticmethod(self):
        class TESTING_OT_poll_static(bpy.types.Operator):
            bl_idname = "testing.poll_static"
            bl_label = "Poll Static"

            @staticmethod  # Wrong: must be a `@classmethod`.
            def poll(context):
                return True

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_poll_static)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_poll_static class "poll" '
            'attribute to be a static/class method, not a function',
        )

    def test_poll_as_regular_method(self):
        class TESTING_OT_poll_regular(bpy.types.Operator):
            bl_idname = "testing.poll_regular"
            bl_label = "Poll Regular"

            def poll(self, context):  # Wrong: must be a `@classmethod`.
                return True

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_poll_regular)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_poll_regular class "poll" '
            'attribute to be a static/class method, not a function',
        )

    def test_draw_as_classmethod(self):
        class TESTING_OT_draw_cls(bpy.types.Operator):
            bl_idname = "testing.draw_cls"
            bl_label = "Draw Cls"

            @classmethod  # Wrong: must be a regular `method`.
            def draw(cls, context):
                pass

            def execute(self, context):
                return {'FINISHED'}

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_draw_cls)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_draw_cls class "draw" attribute to be a function, not a method',
        )

    def test_execute_as_string(self):
        class TESTING_OT_exec_str(bpy.types.Operator):
            bl_idname = "testing.exec_str"
            bl_label = "Exec Str"
            execute = "not_a_function"  # Wrong: must be a `callable`.

        with self.assertRaises(TypeError) as ex:
            bpy.utils.register_class(TESTING_OT_exec_str)
        self.assertEqual(
            str(ex.exception),
            'expected Operator, TESTING_OT_exec_str class "execute" attribute to be a function, not a str',
        )


class TestOperatorRegisterComplete(unittest.TestCase):
    """Operator using every supported flag and callback."""

    @classmethod
    def setUpClass(cls):
        class TESTING_OT_complete(bpy.types.Operator):
            bl_idname = "testing.complete"
            bl_label = "Complete"
            bl_description = "A complete operator"
            bl_translation_context = "*"
            bl_undo_group = ""
            bl_options = {item.identifier for item in bpy.types.Operator.bl_rna.properties['bl_options'].enum_items}
            bl_cursor_pending = 'DEFAULT'

            value: bpy.props.IntProperty(name="Value", default=0)
            last_value = None

            @classmethod
            def poll(cls, context):
                return True

            def invoke(self, context, event):
                return self.execute(context)

            def execute(self, context):
                type(self).last_value = self.value
                return {'FINISHED'}

            def draw(self, context):
                self.layout.prop(self, 'value')

            @classmethod
            def description(cls, context, properties):
                return "Complete operator description"

            def cancel(self, context):
                pass

            def modal(self, context, event):
                return {'FINISHED'}

            def check(self, context):
                return False

        cls._op_class = TESTING_OT_complete
        bpy.utils.register_class(TESTING_OT_complete)
        cls.op = bpy.ops.testing.complete

    @classmethod
    def tearDownClass(cls):
        bpy.utils.unregister_class(cls._op_class)

    def test_all_callbacks_implemented(self):
        """Ensure the operator implements every registered callback from RNA."""
        registered_callbacks = {
            fn_name for fn_name, fn in bpy.types.Operator.bl_rna.functions.items()
            if fn.is_registered or fn.is_registered_optional
        }
        implemented_callbacks = {
            fn_name for fn_name in registered_callbacks
            if fn_name in self._op_class.__dict__
        }
        self.assertEqual(registered_callbacks, implemented_callbacks)

    def test_all_properties_implemented(self):
        """Ensure the operator implements every registered property from RNA."""
        registered_properties = {
            prop_name for prop_name, prop in bpy.types.Operator.bl_rna.properties.items()
            if prop_name != "rna_type" and (prop.is_registered or prop.is_registered_optional)
        }
        implemented_properties = {
            prop_name for prop_name in registered_properties
            if prop_name in self._op_class.__dict__
        }
        self.assertEqual(registered_properties, implemented_properties)

    def test_poll(self):
        self.assertTrue(self.op.poll())

    def test_execute(self):
        self.assertEqual(self.op(value=99), {'FINISHED'})
        self.assertEqual(self._op_class.last_value, 99)

    def test_invoke(self):
        self.assertEqual(self.op('INVOKE_DEFAULT', value=7), {'FINISHED'})
        self.assertEqual(self._op_class.last_value, 7)

    def test_idname(self):
        self.assertEqual(self.op.idname(), 'TESTING_OT_complete')

    def test_idname_py(self):
        self.assertEqual(self.op.idname_py(), 'testing.complete')

    def test_get_rna_type(self):
        rna = self.op.get_rna_type()
        self.assertEqual(rna.identifier, 'TESTING_OT_complete')

    def test_bl_options(self):
        all_options = {item.identifier for item in bpy.types.Operator.bl_rna.properties['bl_options'].enum_items}
        self.assertEqual(self.op.bl_options, all_options)

    def test_doc(self):
        self.assertEqual(self.op.__doc__, "bpy.ops.testing.complete(value=0)\nA complete operator")

    def test_str(self):
        s = str(self.op)
        self.assertTrue(s.startswith("<function bpy.ops.testing.complete at 0x"))
        self.assertTrue(s.endswith(">"))

    def test_repr(self):
        self.assertEqual(repr(self.op), "bpy.ops.testing.complete(value=0)")


class TestOperatorWrapProperties(unittest.TestCase):

    def _make_operator(self, bl_idname, prop_type):
        """Create an operator class that captures `value` on execute."""

        class _OT(bpy.types.Operator):
            bl_label = "Test"
            last_value = None

            def execute(self, context):
                v = self.value
                type(self).last_value = tuple(v) if hasattr(v, "__len__") and not isinstance(v, (str, bytes)) else v
                return {'FINISHED'}

        _OT.bl_idname = bl_idname
        _OT.__name__ = "TESTING_OT_" + bl_idname.split(".")[1]
        _OT.__annotations__ = {"value": prop_type}
        return _OT

    def _run_property_test(self, *, bl_idname, prop_type, value, expected=None):
        if expected is None:
            expected = value
        op_cls = self._make_operator(bl_idname, prop_type)
        with registered_operator(op_cls):
            getattr(bpy.ops.testing, bl_idname.split(".")[1])(value=value)
            result = op_cls.last_value
            if isinstance(expected, float):
                self.assertAlmostEqual(result, expected, places=2)
            elif isinstance(expected, (list, tuple)):
                self.assertEqual(list(result), list(expected))
            else:
                self.assertEqual(result, expected)

    def test_string_property(self):
        self._run_property_test(
            bl_idname="testing.str_prop",
            prop_type=bpy.props.StringProperty(),
            value="hello",
        )

    def test_int_property(self):
        self._run_property_test(
            bl_idname="testing.int_prop",
            prop_type=bpy.props.IntProperty(),
            value=42,
        )

    def test_float_property(self):
        self._run_property_test(
            bl_idname="testing.float_prop",
            prop_type=bpy.props.FloatProperty(),
            value=3.14,
        )

    def test_bool_property(self):
        self._run_property_test(
            bl_idname="testing.bool_prop",
            prop_type=bpy.props.BoolProperty(),
            value=True,
        )

    def test_enum_property(self):
        self._run_property_test(
            bl_idname="testing.enum_prop",
            prop_type=bpy.props.EnumProperty(items=[('A', "A", ""), ('B', "B", "")]),
            value='B',
        )

    def test_byte_string_property(self):
        self._run_property_test(
            bl_idname="testing.byte_str_prop",
            prop_type=bpy.props.StringProperty(subtype='BYTE_STRING'),
            value=b"hello",
        )

    def test_int_vector_property(self):
        self._run_property_test(
            bl_idname="testing.int_vec_prop",
            prop_type=bpy.props.IntVectorProperty(size=3),
            value=(1, 2, 3),
        )

    def test_float_vector_property(self):
        self._run_property_test(
            bl_idname="testing.float_vec_prop",
            prop_type=bpy.props.FloatVectorProperty(size=3),
            value=(1.5, 2.5, 3.5),
        )

    def test_bool_vector_property(self):
        self._run_property_test(
            bl_idname="testing.bool_vec_prop",
            prop_type=bpy.props.BoolVectorProperty(size=3),
            value=(True, False, True),
        )


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == "__main__":
    main()
