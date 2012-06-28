"""
Execution Context
-----------------

When calling an operator you may want to pass the execution context.

This determines the context thats given to the operator to run in, and weather
invoke() is called or execute().

'EXEC_DEFAULT' is used by default but you may want the operator to take user
interaction with 'INVOKE_DEFAULT'.

The execution context is as a non keyword, string argument in:
('INVOKE_DEFAULT', 'INVOKE_REGION_WIN', 'INVOKE_REGION_CHANNELS',
'INVOKE_REGION_PREVIEW', 'INVOKE_AREA', 'INVOKE_SCREEN', 'EXEC_DEFAULT',
'EXEC_REGION_WIN', 'EXEC_REGION_CHANNELS', 'EXEC_REGION_PREVIEW', 'EXEC_AREA',
'EXEC_SCREEN')
"""

# group add popup
import bpy
bpy.ops.object.group_instance_add('INVOKE_DEFAULT')