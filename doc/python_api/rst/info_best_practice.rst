*************
Best Practice
*************


TODO: Intro text


Style Conventions
=================

For Blender 2.5 we have chosen to follow python suggested style guide to avoid mixing styles amongst our own scripts and make it easier to use python scripts from other projects.

Using our style guide for your own scripts makes it easier if you eventually want to contribute them to blender.

This style guide is known as pep8 and can be found `here <http://www.python.org/dev/peps/pep-0008>`_

A brief listing of pep8 criteria.

* camel caps for class names: MyClass

* all lower case underscore separated module names: my_module

* indentation of 4 spaces (no tabs)

* spaces around operators. ``1 + 1``, not ``1+1``

* only use explicit imports, (no importing '*')

* don't use single line: ``if val: body``, separate onto 2 lines instead.


As well as pep8 we have other conventions used for blender python scripts.

* Use single quotes for enums, and double quotes for strings.

  Both are of course strings but in our internal API enums are unique items from a limited set. eg.

  .. code-block:: python

     bpy.context.scene.render.file_format = 'PNG'
     bpy.context.scene.render.filepath = "//render_out"

* pep8 also defines that lines should not exceed 79 characters, we felt this is too restrictive so this is optional per script.

Periodically we run checks for pep8 compliance on blender scripts, for scripts to be included in this check add this line as a comment at the top of the script.

``# <pep8 compliant>``

To enable line length checks use this instead.

``# <pep8-80 compliant>``


User Interface Layout
=====================

TODO: Thomas


Script Efficiency
=================

TODO: Campbell

