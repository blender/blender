"""
.. _operator_modifying_blender_data_undo:

Modifying Blender Data & Undo
+++++++++++++++++++++++++++++

Any operator modifying Blender data should enable the ``'UNDO'`` option.
This will make Blender automatically create an undo step when the operator
finishes its ``execute`` (or ``invoke``, see below) functions, and returns
``{'FINISHED'}``.

Otherwise, no undo step will be created, which will at best corrupt the
undo stack and confuse the user (since modifications done by the operator
may either not be undoable, or be undone together with other edits done
before). In many cases, this can even lead to data corruption and crashes.

Note that when an operator returns ``{'CANCELLED'}``, no undo step will be
created. This means that if an error occurs *after* modifying some data
already, it is better to return ``{'FINISHED'}``, unless it is possible to
fully undo the changes before returning.

.. note::

   Most examples in this page do not do any edit to Blender data, which is
   why it is safe to keep the default ``bl_options`` value for these operators.

.. note::

   In some complex cases, the automatic undo step created on operator exit may
   not be enough. For example, if the operator does mode switching, or calls
   other operators that should create an extra undo step, etc.

   Such manual undo push is possible using the :class:`bpy.ops.ed.undo_push`
   function. Be careful though, this is considered an advanced feature and
   requires some understanding of the actual undo system in Blender code.

"""
import bpy


class DataEditOperator(bpy.types.Operator):
    bl_idname = "object.data_edit"
    bl_label = "Data Editing Operator"
    # The default value is only 'REGISTER', 'UNDO' is mandatory when Blender data is modified
    # (and does require 'REGISTER' as well).
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        context.object.location.x += 1.0
        return {'FINISHED'}


# Only needed if you want to add into a dynamic menu.
def menu_func(self, context):
    self.layout.operator(DataEditOperator.bl_idname, text="Blender Data Editing Operator")


# Register.
bpy.utils.register_class(DataEditOperator)
bpy.types.VIEW3D_MT_view.append(menu_func)

# Test call to the newly defined operator.
bpy.ops.object.data_edit()
