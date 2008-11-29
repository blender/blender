/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * @mainpage DNA- Makesdna modules 
 *
 * @section about About the DNA module
 *
 * The DNA module holds all type definitions that are serialized in a
 * blender file. There is an executable that scans all files, looking
 * for struct-s to serialize (hence sdna: Struct DNA). From this
 * information, it builds a file with numbers that encode the format,
 * the names of variables, and the plce to look for them.
 *
 * @section issues Known issues with DNA
 *
 * - Function pointers:
 *
 *   Because of historical reasons, some function pointers were
 *   untyped. The parser/dna generator has been modified to explicitly
 *   handle these special cases. Most pointers have been given proper
 *   proto's by now. DNA_space_types.h::Spacefile::returnfuncmay still
 *   be badly defined. The reason for this is that is is called with
 *   different types of arguments. It takes a char* at this moment...
 *
 * - Path to the header files
 *
 *   Also because of historical reasons, there is a path prefix to the
 *   headers that need to be scanned. This is the BASE_HEADER
 *   define. If you change the file-layout for DNA, you will probably
 *   have to change this (Not very flexible, but it is hardly ever
 *   changed. Sorry.).
 *
 * @section dependencies Dependencies
 *
 * DNA has no external dependencies (except for a few system
 * includes).
 *
 **/


/* PLEASE READ INSTRUCTIONS ABOUT ADDING VARIABLES IN 'DNA' STRUCTS IN

  intern/dna_genfile.c
  (ton)

 */



/* This file has intentionally no definitions or implementation. */

