"""
.. _operator-undo:

Undo
----

The optional second positional argument is a boolean that controls whether the
operator call is pushed onto the undo stack.

It defaults to ``False``, so operators run from a script do not add to the undo
history. Pass ``True`` when you want the call to be undoable, for instance when
a script performs an edit the user should be able to undo interactively.

If this call happens while nested inside another operator call, the argument
has no effect: the outer call already suppresses undo for everything nested
inside it, and that only lifts once the outer call returns.

Like the execution context this argument is optional, but the positional order
must be kept, so the execution context is given first.
"""

# Add a cube as an undoable operator call.
import bpy
bpy.ops.mesh.primitive_cube_add('EXEC_DEFAULT', True)
