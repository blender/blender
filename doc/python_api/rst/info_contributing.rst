.. _info_contributing:

************************
Contribute Documentation
************************

This guide covers how to contribute to Blender's Python API documentation,
including writing examples, formatting documentation, and building the docs locally.


Setting Up Your Environment
===========================

Prerequisites
-------------

Before you can build the documentation, you need:

Blender Source Code
   Clone the Blender repository following the
   `official build instructions <https://developer.blender.org/docs/handbook/building_blender/>`__.
Python Environment *(optional)*
   Set up a Python `virtual environment <https://docs.python.org/3/library/venv.html>`__.


Installing Documentation Requirements
-------------------------------------

Typically, you would set up a virtual environment and install the packages listed in
``doc/python_api/requirements.txt``. However, the only hard requirement is Sphinx,
which you can install directly:

.. code-block:: bash

   pip install -r doc/python_api/requirements.txt


Building the Documentation
--------------------------

Once you have the requirements installed, you can build the documentation:

.. code-block:: bash

   # From the Blender source root
   make doc_py

You can then open ``doc/python_api/sphinx-out/index.html`` in your browser.


Modifying API Documentation
===========================

API documentation is automatically generated from Blender's source code, meaning that
class descriptions, method signatures, etc., are defined either within C/C++ files
(via ``PyDoc_STRVAR``) or as standard doc-strings in Python files.

**To modify API class or method descriptions:**

#. Locate the relevant source file in the Blender repository.
#. Find the relevant doc-string either inside ``PyDoc_STRVAR(...)`` for the Python C/API
   or as a standard doc-string in a Python file.
#. Edit using **reStructuredText** formatting.
#. Rebuild the Python API docs with ``make doc_py`` to regenerate the pages.


Adding Example Code Snippets
============================

Code examples are a crucial part of the API documentation. They help users
understand how to use various classes, functions, and modules, appearing
above the API reference for each one.


Example File Naming Convention
------------------------------

Examples can be included as standalone script files instead of inlining
code-blocks in the doc-string. Create a file matching the naming conventions
below, and it will be included automatically.

Example files are located in ``doc/python_api/examples/`` and are matched by filename:

- ``module.N.py`` matches a module (e.g., ``gpu.1.py``).
- ``module.ClassName.N.py`` matches a class (e.g., ``bpy.types.Operator.1.py``).
- ``module.ClassName.method.N.py`` matches a method (e.g., ``bpy.types.Operator.invoke.1.py``).
- ``module.ClassName.attribute.N.py`` matches an attribute (e.g., ``bpy.types.Scene.frame_start.1.py``).
- ``module.member.N.py`` matches a module member (e.g., ``bpy.context.object.1.py``).

Multiple examples are supported, where ``N`` allows them to be ordered sequentially.


Example File Structure
----------------------

For each example, it is often useful to include a description explaining what the code demonstrates.
To support this, the doc-string at the start of the file is extracted and displayed above the code example.
The doc-string content will be formatted as reStructuredText.

Each example file should follow this structure:

.. code-block:: python

   """
   Example Title
   +++++++++++++

   A description of what this example demonstrates.
   This doc-string appears above the code in the documentation.

   You can use **reStructuredText** formatting here, for example:

   - *Italic* text with single asterisks
   - **Bold** text with double asterisks
   - ``inline code`` with double backticks
   - :class:`bpy.types.Operator` to link to API classes
   - Links to `external resources <https://www.blender.org/>`__

   .. note::

      You can use this to highlight important information.

   Everything after this doc-string is included as code.
   """
   import bpy

   # Example code goes here
   print("This is an example")


Important Notes
~~~~~~~~~~~~~~~

- The file must start with double triple-quotes ``"""`` (single-quoted doc-strings aren't recognized).
- Use section header underlines with ``+`` characters for the title.
- Everything after the doc-string is included as code in the documentation.
- To add additional code-blocks with text in between, add new files.


Best Practices for Documentation
================================


Writing Good Examples
---------------------

- **Keep it simple**: Focus on demonstrating one concept at a time.
- **Make it runnable**: Examples should work when pasted into Blender's Python console or text editor.
- **Use comments**: Comment thoroughly, assuming readers are new to the APIs and concepts being demonstrated.


Style Guidelines
----------------

For documentation, we aim for high-quality technical writing. Refer to these
style guides from the User Manual for markup and conventions:

- `Markup Guide <https://docs.blender.org/manual/en/latest/contribute/manual/guides/markup_guide.html>`__
- `Writing Guide <https://docs.blender.org/manual/en/latest/contribute/manual/guides/writing_guide.html>`__
- `reStructuredText Primer <https://www.sphinx-doc.org/en/master/usage/restructuredtext/index.html>`__


Testing Your Changes
====================

After adding or modifying documentation, rebuild the docs (see
`Building the Documentation`_) and check for any warnings about broken links,
missing references, or formatting issues. Preview the generated HTML files
in your browser to verify they look correct.


Contributing Your Changes
=========================

Once you've added or improved documentation,
follow Blender's `contribution guidelines <https://developer.blender.org/docs/handbook/contributing/>`__
to create a pull request.
