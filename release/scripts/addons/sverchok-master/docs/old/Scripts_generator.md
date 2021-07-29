== Sverchok Script Generator Node rules ==

For this operation to work the current line must contain the text:
:   'def sv_main(**variables**):'

Where '**variables**' is something like:
:   'verts=[], petal_size=2.3, num_petals=1'

There are three types of input streams that this node can interpret:
- 'v' (vertices, 3-tuple coordinates)
- 's' (data: float, integer),
- 'm' (matrices: nested lists 4*4)

                For more information see the wiki
                see also the bundled templates for clarification


== Buttons in Node ==

* bind the found functions listed in ui_operators to the node function as attributes
    https://github.com/nortikin/sverchok/blob/master/node_Script.py#L313-L319
* if has_buttons then draw_buttons will do the extra loop over button_names
* each button name calls SvScriptUICallbackOp() with node 
    https://github.com/nortikin/sverchok/blob/master/node_Script.py#L248-L251
* each script_node allows us to access node_function,because we bind the Operator functions to the node function as attributes
* if we can access node_function then we can also access node_function.attributes. (by name)
    https://github.com/nortikin/sverchok/blob/master/node_Script.py#L107-L127

Button names must be valid attribute names ( no special characters | () , etc...)
