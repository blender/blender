*************
Best Practice
*************

When writing your own scripts python is great for new developers to pick up and become productive, but you can also pick up odd habits or at least write scripts that are not easy for others to understand.

For your own work this is of course fine, but if you want to collaborate with others or have your work included with blender there are practices we encourage.


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

     bpy.context.scene.render.image_settings.file_format = 'PNG'
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

List Manipulation (General Python Tips)
---------------------------------------


Searching for list items
^^^^^^^^^^^^^^^^^^^^^^^^

In Python there are some handy list functions that save you having to search through the list.

Even though you're not looping on the list data **python is**, so you need to be aware of functions that will slow down your script by searching the whole list.

.. code-block:: python

   my_list.count(list_item)
   my_list.index(list_item)
   my_list.remove(list_item)
   if list_item in my_list: ...


Modifying Lists
^^^^^^^^^^^^^^^
In python we can add and remove from a list, This is slower when the list length is modifier, especially at the start of the list, since all the data after the index of modification needs to be moved up or down 1 place.

The most simple way to add onto the end of the list is to use ``my_list.append(list_item)`` or ``my_list.extend(some_list)`` and the fastest way to remove an item is ``my_list.pop()`` or ``del my_list[-1]``.

To use an index you can use ``my_list.insert(index, list_item)`` or ``list.pop(index)`` for list removal, but these are slower.

Sometimes its faster (but more memory hungry) to just rebuild the list.


Say you want to remove all triangle faces in a list.

Rather than...

.. code-block:: python

   faces = mesh.faces[:]  # make a list copy of the meshes faces
   f_idx = len(faces)     # Loop backwards
   while f_idx:           # while the value is not 0
       f_idx -= 1

       if len(faces[f_idx].vertices) == 3:
           faces.pop(f_idx)  # remove the triangle


It's faster to build a new list with list comprehension.

.. code-block:: python

   faces = [f for f in mesh.faces if len(f.vertices) != 3]


Adding List Items
^^^^^^^^^^^^^^^^^

If you have a list that you want to add onto another list, rather then...

.. code-block:: python

   for l in some_list:
       my_list.append(l)

Use...

.. code-block:: python

   my_list.extend([a, b, c...])


Note that insert can be used when needed, but it is slower than append especially when inserting at the start of a long list.

This example shows a very sub-optimal way of making a reversed list.


.. code-block:: python

   reverse_list = []
   for list_item in some_list:
       reverse_list.insert(0, list_item)


Removing List Items
^^^^^^^^^^^^^^^^^^^

Use ``my_list.pop(index)`` rather than ``my_list.remove(list_item)``

This requires you to have the index of the list item but is faster since ``remove()`` will search the list.

Here is an example of how to remove items in 1 loop, removing the last items first, which is faster (as explained above).

.. code-block:: python

   list_index = len(my_list)

   while list_index:
       list_index -= 1
       if my_list[list_index].some_test_attribute == 1:
           my_list.pop(list_index)


This example shows a fast way of removing items, for use in cases were where you can alter the list order without breaking the scripts functionality. This works by swapping 2 list items, so the item you remove is always last.

.. code-block:: python

   pop_index = 5

   # swap so the pop_index is last.
   my_list[-1], my_list[pop_index] = my_list[pop_index], my_list[-1]

   # remove last item (pop_index)
   my_list.pop()


When removing many items in a large list this can provide a good speedup.


Avoid Copying Lists
^^^^^^^^^^^^^^^^^^^

When passing a list/dictionary to a function, it is faster to have the function modify the list rather then returning a new list so python doesn't have to duplicate the list in memory.

Functions that modify a list in-place are more efficient then functions that create new lists.


This is generally slower so only use for functions when it makes sense not to modify the list in place.

>>> my_list = some_list_func(my_list)


This is generally faster since there is no re-assignment and no list duplication.

>>> some_list_func(vec)


Also note that passing a sliced list makes a copy of the list in python memory

>>> foobar(my_list[:])

If my_list was a large array containing 10000's of items, a copy could use a lot of extra memory.


Writing Strings to a File (Python General)
------------------------------------------

Here are 3 ways of joining multiple strings into 1 string for writing

This really applies to any area of your code that involves a lot of string joining.


Pythons string addition, *don't use if you can help it, especially when writing data in a loop.*

>>> file.write(str1 + " " + str2 + " " + str3 + "\n")


String formatting. Use this when you're writing string data from floats and int's

>>> file.write("%s %s %s\n" % (str1, str2, str3))


Pythons string joining function. To join a list of strings

>>> file.write(" ".join([str1, str2, str3, "\n"]))


join is fastest on many strings, string formatting is quite fast too (better for converting data types). String arithmetic is slowest.


Parsing Strings (Import/Exporting)
----------------------------------

Since many file formats are ASCII, the way you parse/export strings can make a large difference in how fast your script runs.

When importing strings to make into blender there are a few ways to parse the string.

Parsing Numbers
^^^^^^^^^^^^^^^

Use ``float(string)`` rather than ``eval(string)``, if you know the value will be an int then ``int(string)``,  float() will work for an int too but its faster to read ints with int().

Checking String Start/End
^^^^^^^^^^^^^^^^^^^^^^^^^

If you're checking the start of a string for a keyword, rather than...

>>> if line[0:5] == "vert ": ...

Use...

>>> if line.startswith("vert "):

Using ``startswith()`` is slightly faster (approx 5%) and also avoids a possible error with the slice length not matching the string length.

my_string.endswith("foo_bar") can be used for line endings too.

if your unsure whether the text is upper or lower case use lower or upper string function.

>>> if line.lower().startswith("vert ")


Use try/except Sparingly
------------------------

The **try** statement useful to save time writing error checking code.

However **try** is significantly slower then an **if** since an exception has to be set each time, so avoid using **try** in areas of your code that execute in a loop and runs many times.

There are cases where using **try** is faster than checking weather the condition will raise an error, so it is worth experimenting.


Value Comparison
----------------

Python has two ways to compare values ``a == b`` and ``a is b``, The difference is that ``==`` may run the objects comparison function ``__cmp__()`` where as ``is`` compares identity, that both variables reference the same item in memory. 

In cases where you know you are checking for the same value which is referenced from multiple places, ``is`` is faster.


Time Your Code
--------------

While developing a script its good to time it to be aware of any changes in performance, this can be done simply.

.. code-block:: python

   import time
   time_start = time.time()

   # do something...

   print("My Script Finished: %.4f sec" % time.time() - time_start)
