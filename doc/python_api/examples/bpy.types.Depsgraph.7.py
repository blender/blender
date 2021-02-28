"""
Dependency graph: Object.to_curve()
+++++++++++++++++++++++++++++++++++

Function to get a curve from text and curve objects. It is typically used by exporters, render
engines, and tools that need to access the curve representing the object.

The function takes the evaluated dependency graph as a required parameter and optionally a boolean
apply_modifiers which defaults to false. If apply_modifiers is true and the object is a curve object,
the spline deform modifiers are applied on the control points. Note that constructive modifiers and
modifiers that are not spline-enabled will not be applied. So modifiers like Array will not be applied
and deform modifiers that have Apply On Spline disabled will not be applied.

If the object is a text object. The text will be converted into a 3D curve and returned. Modifiers are
never applied on text objects and apply_modifiers will be ignored. If the object is neither a curve nor
a text object, an error will be reported.

.. note:: The resulting curve is owned by the object. It can be freed by calling `object.to_curve_clear()`.
.. note::
   The resulting curve must be treated as temporary, and can not be referenced from objects in the main
   database.
"""
import bpy


class OBJECT_OT_object_to_curve(bpy.types.Operator):
    """Convert selected object to curve and show number of splines"""
    bl_label = "DEG Object to Curve"
    bl_idname = "object.object_to_curve"

    def execute(self, context):
        # Access input original object.
        obj = context.object
        if obj is None:
            self.report({'INFO'}, "No active object to convert to curve")
            return {'CANCELLED'}
        if obj.type not in {'CURVE', 'FONT'}:
            self.report({'INFO'}, "Object can not be converted to curve")
            return {'CANCELLED'}
        depsgraph = context.evaluated_depsgraph_get()
        # Invoke to_curve() without applying modifiers.
        curve_without_modifiers = obj.to_curve(depsgraph)
        self.report({'INFO'}, f"{len(curve_without_modifiers.splines)} splines in a new curve without modifiers.")
        # Remove temporary curve.
        obj.to_curve_clear()
        # Invoke to_curve() with applying modifiers.
        curve_with_modifiers = obj.to_curve(depsgraph, apply_modifiers = True)
        self.report({'INFO'}, f"{len(curve_with_modifiers.splines)} splines in new curve with modifiers.")
        # Remove temporary curve.
        obj.to_curve_clear()
        return {'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_object_to_curve)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_object_to_curve)


if __name__ == "__main__":
    register()

