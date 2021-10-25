"""
.. _operator-execution_context:

Execution Context
-----------------

When calling an operator you may want to pass the execution context.

This determines the context that is given for the operator to run in, and whether
invoke() is called or only execute().

'EXEC_DEFAULT' is used by default, running only the execute() method, but you may
want the operator to take user interaction with 'INVOKE_DEFAULT' which will also
call invoke() if existing.

The execution context is one of:
('INVOKE_DEFAULT', 'INVOKE_REGION_WIN', 'INVOKE_REGION_CHANNELS',
'INVOKE_REGION_PREVIEW', 'INVOKE_AREA', 'INVOKE_SCREEN', 'EXEC_DEFAULT',
'EXEC_REGION_WIN', 'EXEC_REGION_CHANNELS', 'EXEC_REGION_PREVIEW', 'EXEC_AREA',
'EXEC_SCREEN')
"""

# group add popup
import bpy
bpy.ops.object.group_instance_add('INVOKE_DEFAULT')
