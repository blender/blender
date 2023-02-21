This repository contains utilities to perform various editing operations as well as some utilities to integrate
Uncrustify and Meld.


This is for my own personal use, but I have tried to make the tools generic (where possible) and useful to others.


Installing
==========

All the scripts install to QtCreators ``externaltools`` path:

eg:
``~/.config/QtProject/qtcreator/externaltools/``

Currently QtCreator has no way to reference commands relative to this directory so the ``externaltools`` dir **must**
be added to the systems ``PATH``.


Tools
=====

Here are a list of the tools with some details on how they work.


Assembler Preview
-----------------

``External Tools -> Compiler -> Assembler Preview``

This tool generates the assembly for the current open document,
saving it to a file in the same path with an ".asm" extension.

This can be handy for checking if the compiler is really optimizing out code as expected.

Or if some change really doesn't change any functionality.

The way it works is to get a list of the build commands that would run, and get those commands for the current file.

Then this command runs, swapping out object creation args for arguments that create the assembly.

.. note:: It would be nice to open this file, but currently this isn't supported. It's just created along side the source.

.. note:: Currently only GCC is supported.
