/**
 * $Id$
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
 */

/*


The interface routines for reading and writing PLY polygon files.

Greg Turk, February 1994

---------------------------------------------------------------

A PLY file contains a single polygonal _object_.

An object is composed of lists of _elements_.  Typical elements are
vertices, faces, edges and materials.

Each type of element for a given object has one or more _properties_
associated with the element type.  For instance, a vertex element may
have as properties the floating-point values x,y,z and the three unsigned
chars representing red, green and blue.

---------------------------------------------------------------

Copyright (c) 1994 The Board of Trustees of The Leland Stanford
Junior University.  All rights reserved.   
  
Permission to use, copy, modify and distribute this software and its   
documentation for any purpose is hereby granted without fee, provided   
that the above copyright notice and this permission notice appear in   
all copies of this software and that you do not sell the software.   
  
THE SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,   
EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY   
WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.   

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ply.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

char *type_names[] = {
"invalid",
"char", "short", "int",
"uchar", "ushort", "uint",
"float", "double",
};

int ply_type_size[] = {
  0, 1, 2, 4, 1, 2, 4, 4, 8
};

#define NO_OTHER_PROPS  -1

#define DONT_STORE_PROP  0
#define STORE_PROP       1

#define OTHER_PROP       0
#define NAMED_PROP       1


/* returns 1 if strings are equal, 0 if not */
int equal_strings(char *, char *);

/* find an element in a plyfile's list */
PlyElement *find_element(PlyFile *, char *);

/* find a property in an element's list */
PlyProperty *find_property(PlyElement *, char *, int *);

/* write to a file the word describing a PLY file data type */
void write_scalar_type (FILE *, int);

/* read a line from a file and break it up into separate words */
char **get_words(FILE *, int *, char **);
char **old_get_words(FILE *, int *);

/* write an item to a file */
void write_binary_item(FILE *, int, unsigned int, double, int);
void write_ascii_item(FILE *, int, unsigned int, double, int);
double old_write_ascii_item(FILE *, char *, int);

/* add information to a PLY file descriptor */
void add_element(PlyFile *, char **);
void add_property(PlyFile *, char **);
void add_comment(PlyFile *, char *);
void add_obj_info(PlyFile *, char *);

/* copy a property */
void copy_property(PlyProperty *, PlyProperty *);

/* store a value into where a pointer and a type specify */
void store_item(char *, int, int, unsigned int, double);

/* return the value of a stored item */
void get_stored_item( void *, int, int *, unsigned int *, double *);

/* return the value stored in an item, given ptr to it and its type */
double get_item_value(char *, int);

/* get binary or ascii item and store it according to ptr and type */
void get_ascii_item(char *, int, int *, unsigned int *, double *);
void get_binary_item(FILE *, int, int *, unsigned int *, double *);

/* get a bunch of elements from a file */
void ascii_get_element(PlyFile *, char *);
void binary_get_element(PlyFile *, char *);

/* memory allocation */
char *my_alloc(int, int, char *);


/*************/
/*  Writing  */
/*************/


/******************************************************************************
Given a file pointer, get ready to write PLY data to the file.

Entry:
  fp         - the given file pointer
  nelems     - number of elements in object
  elem_names - list of element names
  file_type  - file type, either ascii or binary

Exit:
  returns a pointer to a PlyFile, used to refer to this file, or NULL if error
******************************************************************************/

PlyFile *ply_write(
  FILE *fp,
  int nelems,
  char **elem_names,
  int file_type
)
{
  int i;
  PlyFile *plyfile;
  PlyElement *elem;

  /* check for NULL file pointer */
  if (fp == NULL)
    return (NULL);

  /* create a record for this object */

  plyfile = (PlyFile *) myalloc (sizeof (PlyFile));
  plyfile->file_type = file_type;
  plyfile->num_comments = 0;
  plyfile->num_obj_info = 0;
  plyfile->nelems = nelems;
  plyfile->version = 1.0;
  plyfile->fp = fp;
  plyfile->other_elems = NULL;

  /* tuck aside the names of the elements */

  plyfile->elems = (PlyElement **) myalloc (sizeof (PlyElement *) * nelems);
  for (i = 0; i < nelems; i++) {
    elem = (PlyElement *) myalloc (sizeof (PlyElement));
    plyfile->elems[i] = elem;
    elem->name = strdup (elem_names[i]);
    elem->num = 0;
    elem->nprops = 0;
  }

  /* return pointer to the file descriptor */
  return (plyfile);
}


/******************************************************************************
Open a polygon file for writing.

Entry:
  filename   - name of file to read from
  nelems     - number of elements in object
  elem_names - list of element names
  file_type  - file type, either ascii or binary

Exit:
  version - version number of PLY file
  returns a file identifier, used to refer to this file, or NULL if error
******************************************************************************/

PlyFile *ply_open_for_writing(
  char *filename,
  int nelems,
  char **elem_names,
  int file_type,
  float *version
)
{
  PlyFile *plyfile;
  char *name;
  FILE *fp;

  /* tack on the extension .ply, if necessary */

  name = (char *) myalloc (sizeof (char) * (strlen (filename) + 5));
  strcpy (name, filename);
  if (strlen (name) < 4 ||
      strcmp (name + strlen (name) - 4, ".ply") != 0)
      strcat (name, ".ply");

  /* open the file for writing */

  fp = fopen (name, "w");
  if (fp == NULL) {
    return (NULL);
  }

  /* create the actual PlyFile structure */

  plyfile = ply_write (fp, nelems, elem_names, file_type);
  if (plyfile == NULL)
    return (NULL);

  /* say what PLY file version number we're writing */
  *version = plyfile->version;

  /* return pointer to the file descriptor */
  return (plyfile);
}


/******************************************************************************
Describe an element, including its properties and how many will be written
to the file.

Entry:
  plyfile   - file identifier
  elem_name - name of element that information is being specified about
  nelems    - number of elements of this type to be written
  nprops    - number of properties contained in the element
  prop_list - list of properties
******************************************************************************/

void ply_describe_element(
  PlyFile *plyfile,
  char *elem_name,
  int nelems,
  int nprops,
  PlyProperty *prop_list
)
{
  int i;
  PlyElement *elem;
  PlyProperty *prop;

  /* look for appropriate element */
  elem = find_element (plyfile, elem_name);
  if (elem == NULL) {
    fprintf(stderr,"ply_describe_element: can't find element '%s'\n",elem_name);
    exit (-1);
  }

  elem->num = nelems;

  /* copy the list of properties */

  elem->nprops = nprops;
  elem->props = (PlyProperty **) myalloc (sizeof (PlyProperty *) * nprops);
  elem->store_prop = (char *) myalloc (sizeof (char) * nprops);

  for (i = 0; i < nprops; i++) {
    prop = (PlyProperty *) myalloc (sizeof (PlyProperty));
    elem->props[i] = prop;
    elem->store_prop[i] = NAMED_PROP;
    copy_property (prop, &prop_list[i]);
  }
}


/******************************************************************************
Describe a property of an element.

Entry:
  plyfile   - file identifier
  elem_name - name of element that information is being specified about
  prop      - the new property
******************************************************************************/

void ply_describe_property(
  PlyFile *plyfile,
  char *elem_name,
  PlyProperty *prop
)
{
  PlyElement *elem;
  PlyProperty *elem_prop;

  /* look for appropriate element */
  elem = find_element (plyfile, elem_name);
  if (elem == NULL) {
    fprintf(stderr, "ply_describe_property: can't find element '%s'\n",
            elem_name);
    return;
  }

  /* create room for new property */

  if (elem->nprops == 0) {
    elem->props = (PlyProperty **) myalloc (sizeof (PlyProperty *));
    elem->store_prop = (char *) myalloc (sizeof (char));
    elem->nprops = 1;
  }
  else {
    elem->nprops++;
    elem->props = (PlyProperty **)
                  realloc (elem->props, sizeof (PlyProperty *) * elem->nprops);
    elem->store_prop = (char *)
                  realloc (elem->store_prop, sizeof (char) * elem->nprops);
  }

  /* copy the new property */

  elem_prop = (PlyProperty *) myalloc (sizeof (PlyProperty));
  elem->props[elem->nprops - 1] = elem_prop;
  elem->store_prop[elem->nprops - 1] = NAMED_PROP;
  copy_property (elem_prop, prop);
}


/******************************************************************************
Describe what the "other" properties are that are to be stored, and where
they are in an element.
******************************************************************************/

void ply_describe_other_properties(
  PlyFile *plyfile,
  PlyOtherProp *other,
  int offset
)
{
  int i;
  PlyElement *elem;
  PlyProperty *prop;

  /* look for appropriate element */
  elem = find_element (plyfile, other->name);
  if (elem == NULL) {
    fprintf(stderr, "ply_describe_other_properties: can't find element '%s'\n",
            other->name);
    return;
  }

  /* create room for other properties */

  if (elem->nprops == 0) {
    elem->props = (PlyProperty **)
                  myalloc (sizeof (PlyProperty *) * other->nprops);
    elem->store_prop = (char *) myalloc (sizeof (char) * other->nprops);
    elem->nprops = 0;
  }
  else {
    int newsize;
    newsize = elem->nprops + other->nprops;
    elem->props = (PlyProperty **)
                  realloc (elem->props, sizeof (PlyProperty *) * newsize);
    elem->store_prop = (char *)
                  realloc (elem->store_prop, sizeof (char) * newsize);
  }

  /* copy the other properties */

  for (i = 0; i < other->nprops; i++) {
    prop = (PlyProperty *) myalloc (sizeof (PlyProperty));
    copy_property (prop, other->props[i]);
    elem->props[elem->nprops] = prop;
    elem->store_prop[elem->nprops] = OTHER_PROP;
    elem->nprops++;
  }

  /* save other info about other properties */
  elem->other_size = other->size;
  elem->other_offset = offset;
}


/******************************************************************************
State how many of a given element will be written.

Entry:
  plyfile   - file identifier
  elem_name - name of element that information is being specified about
  nelems    - number of elements of this type to be written
******************************************************************************/

void ply_element_count(
  PlyFile *plyfile,
  char *elem_name,
  int nelems
)
{
  PlyElement *elem;

  /* look for appropriate element */
  elem = find_element (plyfile, elem_name);
  if (elem == NULL) {
    fprintf(stderr,"ply_element_count: can't find element '%s'\n",elem_name);
    exit (-1);
  }

  elem->num = nelems;
}


/******************************************************************************
Signal that we've described everything a PLY file's header and that the
header should be written to the file.

Entry:
  plyfile - file identifier
******************************************************************************/

void ply_header_complete(PlyFile *plyfile)
{
  int i,j;
  FILE *fp = plyfile->fp;
  PlyElement *elem;
  PlyProperty *prop;

  fprintf (fp, "ply\n");

  switch (plyfile->file_type) {
    case PLY_ASCII:
      fprintf (fp, "format ascii 1.0\n");
      break;
    case PLY_BINARY_BE:
      fprintf (fp, "format binary_big_endian 1.0\n");
      break;
    case PLY_BINARY_LE:
      fprintf (fp, "format binary_little_endian 1.0\n");
      break;
    default:
      fprintf (stderr, "ply_header_complete: bad file type = %d\n",
               plyfile->file_type);
      exit (-1);
  }

  /* write out the comments */

  for (i = 0; i < plyfile->num_comments; i++)
    fprintf (fp, "comment %s\n", plyfile->comments[i]);

  /* write out object information */

  for (i = 0; i < plyfile->num_obj_info; i++)
    fprintf (fp, "obj_info %s\n", plyfile->obj_info[i]);

  /* write out information about each element */

  for (i = 0; i < plyfile->nelems; i++) {

    elem = plyfile->elems[i];
    fprintf (fp, "element %s %d\n", elem->name, elem->num);

    /* write out each property */
    for (j = 0; j < elem->nprops; j++) {
      prop = elem->props[j];
      if (prop->is_list) {
        fprintf (fp, "property list ");
        write_scalar_type (fp, prop->count_external);
        fprintf (fp, " ");
        write_scalar_type (fp, prop->external_type);
        fprintf (fp, " %s\n", prop->name);
      }
      else {
        fprintf (fp, "property ");
        write_scalar_type (fp, prop->external_type);
        fprintf (fp, " %s\n", prop->name);
      }
    }
  }

  fprintf (fp, "end_header\n");
}


/******************************************************************************
Specify which elements are going to be written.  This should be called
before a call to the routine ply_put_element().

Entry:
  plyfile   - file identifier
  elem_name - name of element we're talking about
******************************************************************************/

void ply_put_element_setup(PlyFile *plyfile, char *elem_name)
{
  PlyElement *elem;

  elem = find_element (plyfile, elem_name);
  if (elem == NULL) {
    fprintf(stderr, "ply_elements_setup: can't find element '%s'\n", elem_name);
    exit (-1);
  }

  plyfile->which_elem = elem;
}


/******************************************************************************
Write an element to the file.  This routine assumes that we're
writing the type of element specified in the last call to the routine
ply_put_element_setup().

Entry:
  plyfile  - file identifier
  elem_ptr - pointer to the element
******************************************************************************/

void ply_put_element(PlyFile *plyfile, void *elem_ptr)
{
  int j,k;
  FILE *fp = plyfile->fp;
  PlyElement *elem;
  PlyProperty *prop;
  char *elem_data,*item;
  char **item_ptr;
  int list_count;
  int item_size;
  int int_val;
  unsigned int uint_val;
  double double_val;
  char **other_ptr;

  elem = plyfile->which_elem;
  elem_data = elem_ptr;
  other_ptr = (char **) (((char *) elem_ptr) + elem->other_offset);

  /* write out either to an ascii or binary file */

  if (plyfile->file_type == PLY_ASCII) {

    /* write an ascii file */

    /* write out each property of the element */
    for (j = 0; j < elem->nprops; j++) {
      prop = elem->props[j];
      if (elem->store_prop[j] == OTHER_PROP)
        elem_data = *other_ptr;
      else
        elem_data = elem_ptr;
      if (prop->is_list) {
        item = elem_data + prop->count_offset;
        get_stored_item ((void *) item, prop->count_internal,
                         &int_val, &uint_val, &double_val);
        write_ascii_item (fp, int_val, uint_val, double_val,
                          prop->count_external);
        list_count = uint_val;
        item_ptr = (char **) (elem_data + prop->offset);
        item = item_ptr[0];
       item_size = ply_type_size[prop->internal_type];
        for (k = 0; k < list_count; k++) {
          get_stored_item ((void *) item, prop->internal_type,
                           &int_val, &uint_val, &double_val);
          write_ascii_item (fp, int_val, uint_val, double_val,
                            prop->external_type);
          item += item_size;
        }
      }
      else {
        item = elem_data + prop->offset;
        get_stored_item ((void *) item, prop->internal_type,
                         &int_val, &uint_val, &double_val);
        write_ascii_item (fp, int_val, uint_val, double_val,
                          prop->external_type);
      }
    }

    fprintf (fp, "\n");
  }
  else {

    /* write a binary file */

    /* write out each property of the element */
    for (j = 0; j < elem->nprops; j++) {
      prop = elem->props[j];
      if (elem->store_prop[j] == OTHER_PROP)
        elem_data = *other_ptr;
      else
        elem_data = elem_ptr;
      if (prop->is_list) {
        item = elem_data + prop->count_offset;
        item_size = ply_type_size[prop->count_internal];
        get_stored_item ((void *) item, prop->count_internal,
                         &int_val, &uint_val, &double_val);
        write_binary_item (fp, int_val, uint_val, double_val,
                           prop->count_external);
        list_count = uint_val;
        item_ptr = (char **) (elem_data + prop->offset);
        item = item_ptr[0];
        item_size = ply_type_size[prop->internal_type];
        for (k = 0; k < list_count; k++) {
          get_stored_item ((void *) item, prop->internal_type,
                           &int_val, &uint_val, &double_val);
          write_binary_item (fp, int_val, uint_val, double_val,
                             prop->external_type);
          item += item_size;
        }
      }
      else {
        item = elem_data + prop->offset;
        item_size = ply_type_size[prop->internal_type];
        get_stored_item ((void *) item, prop->internal_type,
                         &int_val, &uint_val, &double_val);
        write_binary_item (fp, int_val, uint_val, double_val,
                           prop->external_type);
      }
    }

  }
}


/******************************************************************************
Specify a comment that will be written in the header.

Entry:
  plyfile - file identifier
  comment - the comment to be written
******************************************************************************/

void ply_put_comment(PlyFile *plyfile, char *comment)
{
  /* (re)allocate space for new comment */
  if (plyfile->num_comments == 0)
    plyfile->comments = (char **) myalloc (sizeof (char *));
  else
    plyfile->comments = (char **) realloc (plyfile->comments,
                         sizeof (char *) * (plyfile->num_comments + 1));

  /* add comment to list */
  plyfile->comments[plyfile->num_comments] = strdup (comment);
  plyfile->num_comments++;
}


/******************************************************************************
Specify a piece of object information (arbitrary text) that will be written
in the header.

Entry:
  plyfile  - file identifier
  obj_info - the text information to be written
******************************************************************************/

void ply_put_obj_info(PlyFile *plyfile, char *obj_info)
{
  /* (re)allocate space for new info */
  if (plyfile->num_obj_info == 0)
    plyfile->obj_info = (char **) myalloc (sizeof (char *));
  else
    plyfile->obj_info = (char **) realloc (plyfile->obj_info,
                         sizeof (char *) * (plyfile->num_obj_info + 1));

  /* add info to list */
  plyfile->obj_info[plyfile->num_obj_info] = strdup (obj_info);
  plyfile->num_obj_info++;
}







/*************/
/*  Reading  */
/*************/



/******************************************************************************
Given a file pointer, get ready to read PLY data from the file.

Entry:
  fp - the given file pointer

Exit:
  nelems     - number of elements in object
  elem_names - list of element names
  returns a pointer to a PlyFile, used to refer to this file, or NULL if error
******************************************************************************/

PlyFile *ply_read(FILE *fp, int *nelems, char ***elem_names)
{
  int i,j;
  PlyFile *plyfile;
  int nwords;
  char **words;
  int found_format = 0;
  char **elist;
  PlyElement *elem;
  char *orig_line;

  /* check for NULL file pointer */
  if (fp == NULL)
    return (NULL);

  /* create record for this object */

  plyfile = (PlyFile *) myalloc (sizeof (PlyFile));
  plyfile->nelems = 0;
  plyfile->comments = NULL;
  plyfile->num_comments = 0;
  plyfile->obj_info = NULL;
  plyfile->num_obj_info = 0;
  plyfile->fp = fp;
  plyfile->other_elems = NULL;

  /* read and parse the file's header */

  words = get_words (plyfile->fp, &nwords, &orig_line);
  if (!words || !equal_strings (words[0], "ply"))
    return (NULL);

  while (words) {

    /* parse words */

    if (equal_strings (words[0], "format")) {
      if (nwords != 3)
        return (NULL);
      if (equal_strings (words[1], "ascii"))
        plyfile->file_type = PLY_ASCII;
      else if (equal_strings (words[1], "binary_big_endian"))
        plyfile->file_type = PLY_BINARY_BE;
      else if (equal_strings (words[1], "binary_little_endian"))
        plyfile->file_type = PLY_BINARY_LE;
      else
        return (NULL);
      plyfile->version = (float)atof (words[2]);
      found_format = 1;
    }
    else if (equal_strings (words[0], "element"))
      add_element (plyfile, words);
    else if (equal_strings (words[0], "property"))
      add_property (plyfile, words);
    else if (equal_strings (words[0], "comment"))
      add_comment (plyfile, orig_line);
    else if (equal_strings (words[0], "obj_info"))
      add_obj_info (plyfile, orig_line);
    else if (equal_strings (words[0], "end_header"))
      break;

    /* free up words space */
    free (words);

    words = get_words (plyfile->fp, &nwords, &orig_line);
  }

  /* create tags for each property of each element, to be used */
  /* later to say whether or not to store each property for the user */

  for (i = 0; i < plyfile->nelems; i++) {
    elem = plyfile->elems[i];
    elem->store_prop = (char *) myalloc (sizeof (char) * elem->nprops);
    for (j = 0; j < elem->nprops; j++)
      elem->store_prop[j] = DONT_STORE_PROP;
    elem->other_offset = NO_OTHER_PROPS; /* no "other" props by default */
  }

  /* set return values about the elements */

  elist = (char **) myalloc (sizeof (char *) * plyfile->nelems);
  for (i = 0; i < plyfile->nelems; i++)
    elist[i] = strdup (plyfile->elems[i]->name);

  *elem_names = elist;
  *nelems = plyfile->nelems;

  /* return a pointer to the file's information */

  return (plyfile);
}


/******************************************************************************
Open a polygon file for reading.

Entry:
  filename - name of file to read from

Exit:
  nelems     - number of elements in object
  elem_names - list of element names
  file_type  - file type, either ascii or binary
  version    - version number of PLY file
  returns a file identifier, used to refer to this file, or NULL if error
******************************************************************************/

PlyFile *ply_open_for_reading(
  char *filename,
  int *nelems,
  char ***elem_names,
  int *file_type,
  float *version
)
{
  FILE *fp;
  PlyFile *plyfile;
  char *name;

  /* tack on the extension .ply, if necessary */

  name = (char *) myalloc (sizeof (char) * (strlen (filename) + 5));
  strcpy (name, filename);
  if (strlen (name) < 4 ||
      strcmp (name + strlen (name) - 4, ".ply") != 0)
      strcat (name, ".ply");

  /* open the file for reading */

  fp = fopen (name, "r");
  if (fp == NULL)
    return (NULL);

  /* create the PlyFile data structure */

  plyfile = ply_read (fp, nelems, elem_names);

  /* determine the file type and version */

  *file_type = plyfile->file_type;
  *version = plyfile->version;

  /* return a pointer to the file's information */

  return (plyfile);
}


/******************************************************************************
Get information about a particular element.

Entry:
  plyfile   - file identifier
  elem_name - name of element to get information about

Exit:
  nelems   - number of elements of this type in the file
  nprops   - number of properties
  returns a list of properties, or NULL if the file doesn't contain that elem
******************************************************************************/

PlyProperty **ply_get_element_description(
  PlyFile *plyfile,
  char *elem_name,
  int *nelems,
  int *nprops
)
{
  int i;
  PlyElement *elem;
  PlyProperty *prop;
  PlyProperty **prop_list;

  /* find information about the element */
  elem = find_element (plyfile, elem_name);
  if (elem == NULL)
    return (NULL);

  *nelems = elem->num;
  *nprops = elem->nprops;

  /* make a copy of the element's property list */
  prop_list = (PlyProperty **) myalloc (sizeof (PlyProperty *) * elem->nprops);
  for (i = 0; i < elem->nprops; i++) {
    prop = (PlyProperty *) myalloc (sizeof (PlyProperty));
    copy_property (prop, elem->props[i]);
    prop_list[i] = prop;
  }

  /* return this duplicate property list */
  return (prop_list);
}


/******************************************************************************
Specify which properties of an element are to be returned.  This should be
called before a call to the routine ply_get_element().

Entry:
  plyfile   - file identifier
  elem_name - which element we're talking about
  nprops    - number of properties
  prop_list - list of properties
******************************************************************************/

void ply_get_element_setup(
  PlyFile *plyfile,
  char *elem_name,
  int nprops,
  PlyProperty *prop_list
)
{
  int i;
  PlyElement *elem;
  PlyProperty *prop;
  int index;

  /* find information about the element */
  elem = find_element (plyfile, elem_name);
  plyfile->which_elem = elem;

  /* deposit the property information into the element's description */
  for (i = 0; i < nprops; i++) {

    /* look for actual property */
    prop = find_property (elem, prop_list[i].name, &index);
    if (prop == NULL) {
      fprintf (stderr, "Warning:  Can't find property '%s' in element '%s'\n",
               prop_list[i].name, elem_name);
      continue;
    }

    /* store its description */
    prop->internal_type = prop_list[i].internal_type;
    prop->offset = prop_list[i].offset;
    prop->count_internal = prop_list[i].count_internal;
    prop->count_offset = prop_list[i].count_offset;

    /* specify that the user wants this property */
    elem->store_prop[index] = STORE_PROP;
  }
}


/******************************************************************************
Specify a property of an element that is to be returned.  This should be
called (usually multiple times) before a call to the routine ply_get_element().
This routine should be used in preference to the less flexible old routine
called ply_get_element_setup().

Entry:
  plyfile   - file identifier
  elem_name - which element we're talking about
  prop      - property to add to those that will be returned
******************************************************************************/

void ply_get_property(
  PlyFile *plyfile,
  char *elem_name,
  PlyProperty *prop
)
{
  PlyElement *elem;
  PlyProperty *prop_ptr;
  int index;

  /* find information about the element */
  elem = find_element (plyfile, elem_name);
  plyfile->which_elem = elem;

  /* deposit the property information into the element's description */

  prop_ptr = find_property (elem, prop->name, &index);
  if (prop_ptr == NULL) {
    fprintf (stderr, "Warning:  Can't find property '%s' in element '%s'\n",
             prop->name, elem_name);
    return;
  }
  prop_ptr->internal_type  = prop->internal_type;
  prop_ptr->offset         = prop->offset;
  prop_ptr->count_internal = prop->count_internal;
  prop_ptr->count_offset   = prop->count_offset;

  /* specify that the user wants this property */
  elem->store_prop[index] = STORE_PROP;
}


/******************************************************************************
Read one element from the file.  This routine assumes that we're reading
the type of element specified in the last call to the routine
ply_get_element_setup().

Entry:
  plyfile  - file identifier
  elem_ptr - pointer to location where the element information should be put
******************************************************************************/

void ply_get_element(PlyFile *plyfile, void *elem_ptr)
{
  if (plyfile->file_type == PLY_ASCII)
    ascii_get_element (plyfile, (char *) elem_ptr);
  else
    binary_get_element (plyfile, (char *) elem_ptr);
}


/******************************************************************************
Extract the comments from the header information of a PLY file.

Entry:
  plyfile - file identifier

Exit:
  num_comments - number of comments returned
  returns a pointer to a list of comments
******************************************************************************/

char **ply_get_comments(PlyFile *plyfile, int *num_comments)
{
  *num_comments = plyfile->num_comments;
  return (plyfile->comments);
}


/******************************************************************************
Extract the object information (arbitrary text) from the header information
of a PLY file.

Entry:
  plyfile - file identifier

Exit:
  num_obj_info - number of lines of text information returned
  returns a pointer to a list of object info lines
******************************************************************************/

char **ply_get_obj_info(PlyFile *plyfile, int *num_obj_info)
{
  *num_obj_info = plyfile->num_obj_info;
  return (plyfile->obj_info);
}


/******************************************************************************
Make ready for "other" properties of an element-- those properties that
the user has not explicitly asked for, but that are to be stashed away
in a special structure to be carried along with the element's other
information.

Entry:
  plyfile - file identifier
  elem    - element for which we want to save away other properties
******************************************************************************/

void setup_other_props(PlyElement *elem)
{
  int i;
  PlyProperty *prop;
  int size = 0;
  int type_size;

  /* Examine each property in decreasing order of size. */
  /* We do this so that all data types will be aligned by */
  /* word, half-word, or whatever within the structure. */

  for (type_size = 8; type_size > 0; type_size /= 2) {

    /* add up the space taken by each property, and save this information */
    /* away in the property descriptor */

    for (i = 0; i < elem->nprops; i++) {

      /* don't bother with properties we've been asked to store explicitly */
      if (elem->store_prop[i])
        continue;

      prop = elem->props[i];

      /* internal types will be same as external */
      prop->internal_type = prop->external_type;
      prop->count_internal = prop->count_external;

      /* check list case */
      if (prop->is_list) {

        /* pointer to list */
        if (type_size == sizeof (void *)) {
          prop->offset = size;
          size += sizeof (void *);    /* always use size of a pointer here */
        }

        /* count of number of list elements */
        if (type_size == ply_type_size[prop->count_external]) {
          prop->count_offset = size;
          size += ply_type_size[prop->count_external];
        }
      }
      /* not list */
      else if (type_size == ply_type_size[prop->external_type]) {
        prop->offset = size;
        size += ply_type_size[prop->external_type];
      }
    }

  }

  /* save the size for the other_props structure */
  elem->other_size = size;
}


/******************************************************************************
Specify that we want the "other" properties of an element to be tucked
away within the user's structure.  The user needn't be concerned for how
these properties are stored.

Entry:
  plyfile   - file identifier
  elem_name - name of element that we want to store other_props in
  offset    - offset to where other_props will be stored inside user's structure

Exit:
  returns pointer to structure containing description of other_props
******************************************************************************/

PlyOtherProp *ply_get_other_properties(
  PlyFile *plyfile,
  char *elem_name,
  int offset
)
{
  int i;
  PlyElement *elem;
  PlyOtherProp *other;
  PlyProperty *prop;
  int nprops;

  /* find information about the element */
  elem = find_element (plyfile, elem_name);
  if (elem == NULL) {
    fprintf (stderr, "ply_get_other_properties: Can't find element '%s'\n",
             elem_name);
    return (NULL);
  }

  /* remember that this is the "current" element */
  plyfile->which_elem = elem;

  /* save the offset to where to store the other_props */
  elem->other_offset = offset;

  /* place the appropriate pointers, etc. in the element's property list */
  setup_other_props (elem);

  /* create structure for describing other_props */
  other = (PlyOtherProp *) myalloc (sizeof (PlyOtherProp));
  other->name = strdup (elem_name);
#if 0
  if (elem->other_offset == NO_OTHER_PROPS) {
    other->size = 0;
    other->props = NULL;
    other->nprops = 0;
    return (other);
  }
#endif
  other->size = elem->other_size;
  other->props = (PlyProperty **) myalloc (sizeof(PlyProperty) * elem->nprops);
  
  /* save descriptions of each "other" property */
  nprops = 0;
  for (i = 0; i < elem->nprops; i++) {
    if (elem->store_prop[i])
      continue;
    prop = (PlyProperty *) myalloc (sizeof (PlyProperty));
    copy_property (prop, elem->props[i]);
    other->props[nprops] = prop;
    nprops++;
  }
  other->nprops = nprops;

#if 1
  /* set other_offset pointer appropriately if there are NO other properties */
  if (other->nprops == 0) {
    elem->other_offset = NO_OTHER_PROPS;
  }
#endif
  
  /* return structure */
  return (other);
}




/*************************/
/*  Other Element Stuff  */
/*************************/




/******************************************************************************
Grab all the data for an element that a user does not want to explicitly
read in.

Entry:
  plyfile    - pointer to file
  elem_name  - name of element whose data is to be read in
  elem_count - number of instances of this element stored in the file

Exit:
  returns pointer to ALL the "other" element data for this PLY file
******************************************************************************/

PlyOtherElems *ply_get_other_element (
  PlyFile *plyfile,
  char *elem_name,
  int elem_count
)
{
  int i;
  PlyElement *elem;
  PlyOtherElems *other_elems;
  OtherElem *other;

  /* look for appropriate element */
  elem = find_element (plyfile, elem_name);
  if (elem == NULL) {
    fprintf (stderr,
             "ply_get_other_element: can't find element '%s'\n", elem_name);
    exit (-1);
  }

  /* create room for the new "other" element, initializing the */
  /* other data structure if necessary */

  if (plyfile->other_elems == NULL) {
    plyfile->other_elems = (PlyOtherElems *) myalloc (sizeof (PlyOtherElems));
    other_elems = plyfile->other_elems;
    other_elems->other_list = (OtherElem *) myalloc (sizeof (OtherElem));
    other = &(other_elems->other_list[0]);
    other_elems->num_elems = 1;
  }
  else {
    other_elems = plyfile->other_elems;
    other_elems->other_list = (OtherElem *) realloc (other_elems->other_list,
                              sizeof (OtherElem) * other_elems->num_elems + 1);
    other = &(other_elems->other_list[other_elems->num_elems]);
    other_elems->num_elems++;
  }

  /* count of element instances in file */
  other->elem_count = elem_count;

  /* save name of element */
  other->elem_name = strdup (elem_name);

  /* create a list to hold all the current elements */
  other->other_data = (OtherData **)
                  malloc (sizeof (OtherData *) * other->elem_count);

  /* set up for getting elements */
  other->other_props = ply_get_other_properties (plyfile, elem_name,
                         offsetof(OtherData,other_props));

  /* grab all these elements */
  for (i = 0; i < other->elem_count; i++) {
    /* grab and element from the file */
    other->other_data[i] = (OtherData *) malloc (sizeof (OtherData));
    ply_get_element (plyfile, (void *) other->other_data[i]);
  }

  /* return pointer to the other elements data */
  return (other_elems);
}


/******************************************************************************
Pass along a pointer to "other" elements that we want to save in a given
PLY file.  These other elements were presumably read from another PLY file.

Entry:
  plyfile     - file pointer in which to store this other element info
  other_elems - info about other elements that we want to store
******************************************************************************/

void ply_describe_other_elements (
  PlyFile *plyfile,
  PlyOtherElems *other_elems
)
{
  int i;
  OtherElem *other;

  /* ignore this call if there is no other element */
  if (other_elems == NULL)
    return;

  /* save pointer to this information */
  plyfile->other_elems = other_elems;

  /* describe the other properties of this element */

  for (i = 0; i < other_elems->num_elems; i++) {
    other = &(other_elems->other_list[i]);
    ply_element_count (plyfile, other->elem_name, other->elem_count);
    ply_describe_other_properties (plyfile, other->other_props,
                                   offsetof(OtherData,other_props));
  }
}


/******************************************************************************
Write out the "other" elements specified for this PLY file.

Entry:
  plyfile - pointer to PLY file to write out other elements for
******************************************************************************/

void ply_put_other_elements (PlyFile *plyfile)
{
  int i,j;
  OtherElem *other;

  /* make sure we have other elements to write */
  if (plyfile->other_elems == NULL)
    return;

  /* write out the data for each "other" element */

  for (i = 0; i < plyfile->other_elems->num_elems; i++) {

    other = &(plyfile->other_elems->other_list[i]);
    ply_put_element_setup (plyfile, other->elem_name);

    /* write out each instance of the current element */
    for (j = 0; j < other->elem_count; j++)
      ply_put_element (plyfile, (void *) other->other_data[j]);
  }
}


/******************************************************************************
Free up storage used by an "other" elements data structure.

Entry:
  other_elems - data structure to free up
******************************************************************************/




/*******************/
/*  Miscellaneous  */
/*******************/



/******************************************************************************
Close a PLY file.

Entry:
  plyfile - identifier of file to close
******************************************************************************/

void ply_close(PlyFile *plyfile)
{
  fclose (plyfile->fp);

  /* free up memory associated with the PLY file */
  free (plyfile);
}


/******************************************************************************
Get version number and file type of a PlyFile.

Entry:
  ply - pointer to PLY file

Exit:
  version - version of the file
  file_type - PLY_ASCII, PLY_BINARY_BE, or PLY_BINARY_LE
******************************************************************************/

void ply_get_info(PlyFile *ply, float *version, int *file_type)
{
  if (ply == NULL)
    return;

  *version = ply->version;
  *file_type = ply->file_type;
}


/******************************************************************************
Compare two strings.  Returns 1 if they are the same, 0 if not.
******************************************************************************/

int equal_strings(char *s1, char *s2)
{

  while (*s1 && *s2)
    if (*s1++ != *s2++)
      return (0);

  if (*s1 != *s2)
    return (0);
  else
    return (1);
}


/******************************************************************************
Find an element from the element list of a given PLY object.

Entry:
  plyfile - file id for PLY file
  element - name of element we're looking for

Exit:
  returns the element, or NULL if not found
******************************************************************************/

PlyElement *find_element(PlyFile *plyfile, char *element)
{
  int i;

  for (i = 0; i < plyfile->nelems; i++)
    if (equal_strings (element, plyfile->elems[i]->name))
      return (plyfile->elems[i]);

  return (NULL);
}


/******************************************************************************
Find a property in the list of properties of a given element.

Entry:
  elem      - pointer to element in which we want to find the property
  prop_name - name of property to find

Exit:
  index - index to position in list
  returns a pointer to the property, or NULL if not found
******************************************************************************/

PlyProperty *find_property(PlyElement *elem, char *prop_name, int *index)
{
  int i;

  for (i = 0; i < elem->nprops; i++)
    if (equal_strings (prop_name, elem->props[i]->name)) {
      *index = i;
      return (elem->props[i]);
    }

  *index = -1;
  return (NULL);
}


/******************************************************************************
Read an element from an ascii file.

Entry:
  plyfile  - file identifier
  elem_ptr - pointer to element
******************************************************************************/

void ascii_get_element(PlyFile *plyfile, char *elem_ptr)
{
  int j,k;
  PlyElement *elem;
  PlyProperty *prop;
  char **words;
  int nwords;
  int which_word;
  char *elem_data,*item;
  char *item_ptr;
  int item_size;
  int int_val;
  unsigned int uint_val;
  double double_val;
  int list_count;
  int store_it;
  char **store_array;
  char *orig_line;
  char *other_data;
  int other_flag;

    other_flag = 0;
	other_data = NULL;
	item = NULL;
	item_size = 0;

  /* the kind of element we're reading currently */
  elem = plyfile->which_elem;

  /* do we need to setup for other_props? */

  if (elem->other_offset != NO_OTHER_PROPS) {
    char **ptr;
    other_flag = 1;
    /* make room for other_props */
    other_data = (char *) myalloc (elem->other_size);
    /* store pointer in user's structure to the other_props */
    ptr = (char **) (elem_ptr + elem->other_offset);
    *ptr = other_data;
  } else {
    other_flag = 0;
	other_data = NULL;
	item = NULL;
	item_size = 0;
  }

  /* read in the element */

  words = get_words (plyfile->fp, &nwords, &orig_line);
  if (words == NULL) {
    fprintf (stderr, "ply_get_element: unexpected end of file\n");
    exit (-1);
  }

  which_word = 0;

  for (j = 0; j < elem->nprops; j++) {

    prop = elem->props[j];
    store_it = (elem->store_prop[j] | other_flag);

    /* store either in the user's structure or in other_props */
    if (elem->store_prop[j])
      elem_data = elem_ptr;
    else
      elem_data = other_data;

    if (prop->is_list) {       /* a list */

      /* get and store the number of items in the list */
      get_ascii_item (words[which_word++], prop->count_external,
                      &int_val, &uint_val, &double_val);
      if (store_it) {
        item = elem_data + prop->count_offset;
        store_item(item, prop->count_internal, int_val, uint_val, double_val);
      }

      /* allocate space for an array of items and store a ptr to the array */
      list_count = int_val;
      item_size = ply_type_size[prop->internal_type];
      store_array = (char **) (elem_data + prop->offset);

      if (list_count == 0) {
        if (store_it)
          *store_array = NULL;
      }
      else {
        if (store_it) {
          item_ptr = (char *) myalloc (sizeof (char) * item_size * list_count);
          item = item_ptr;
          *store_array = item_ptr;
        }

        /* read items and store them into the array */
        for (k = 0; k < list_count; k++) {
          get_ascii_item (words[which_word++], prop->external_type,
                          &int_val, &uint_val, &double_val);
          if (store_it) {
            store_item (item, prop->internal_type,
                        int_val, uint_val, double_val);
            item += item_size;
          }
        }
      }

    }
    else {                     /* not a list */
      get_ascii_item (words[which_word++], prop->external_type,
                      &int_val, &uint_val, &double_val);
      if (store_it) {
        item = elem_data + prop->offset;
        store_item (item, prop->internal_type, int_val, uint_val, double_val);
      }
    }

  }

  free (words);
}


/******************************************************************************
Read an element from a binary file.

Entry:
  plyfile  - file identifier
  elem_ptr - pointer to an element
******************************************************************************/

void binary_get_element(PlyFile *plyfile, char *elem_ptr)
{
  int j,k;
  PlyElement *elem;
  PlyProperty *prop;
  FILE *fp = plyfile->fp;
  char *elem_data,*item;
  char *item_ptr;
  int item_size;
  int int_val;
  unsigned int uint_val;
  double double_val;
  int list_count;
  int store_it;
  char **store_array;
  char *other_data;
  int other_flag;


  other_flag = 0;
  other_data = NULL;
  item = NULL;
  item_size = 0;

  /* the kind of element we're reading currently */
  elem = plyfile->which_elem;

  /* do we need to setup for other_props? */

  if (elem->other_offset != NO_OTHER_PROPS) {
    char **ptr;
    other_flag = 1;
    /* make room for other_props */
    other_data = (char *) myalloc (elem->other_size);
    /* store pointer in user's structure to the other_props */
    ptr = (char **) (elem_ptr + elem->other_offset);
    *ptr = other_data;
  }
  else {
    other_flag = 0;
	other_data = NULL;
	item = NULL;
	item_size = 0;
  }
  /* read in a number of elements */

  for (j = 0; j < elem->nprops; j++) {

    prop = elem->props[j];
    store_it = (elem->store_prop[j] | other_flag);

    /* store either in the user's structure or in other_props */
    if (elem->store_prop[j])
      elem_data = elem_ptr;
    else
      elem_data = other_data;

    if (prop->is_list) {       /* a list */

      /* get and store the number of items in the list */
      get_binary_item (fp, prop->count_external,
                      &int_val, &uint_val, &double_val);
      if (store_it) {
        item = elem_data + prop->count_offset;
        store_item(item, prop->count_internal, int_val, uint_val, double_val);
      }

      /* allocate space for an array of items and store a ptr to the array */
      list_count = int_val;
      /* The "if" was added by Afra Zomorodian 8/22/95
       * so that zipper won't crash reading plies that have additional
       * properties.
       */ 
      if (store_it) {
	item_size = ply_type_size[prop->internal_type];
      }
      store_array = (char **) (elem_data + prop->offset);
      if (list_count == 0) {
        if (store_it)
          *store_array = NULL;
      }
      else {
        if (store_it) {
          item_ptr = (char *) myalloc (sizeof (char) * item_size * list_count);
          item = item_ptr;
          *store_array = item_ptr;
        }

        /* read items and store them into the array */
        for (k = 0; k < list_count; k++) {
          get_binary_item (fp, prop->external_type,
                          &int_val, &uint_val, &double_val);
          if (store_it) {
            store_item (item, prop->internal_type,
                        int_val, uint_val, double_val);
            item += item_size;
          }
        }
      }

    }
    else {                     /* not a list */
      get_binary_item (fp, prop->external_type,
                      &int_val, &uint_val, &double_val);
      if (store_it) {
        item = elem_data + prop->offset;
        store_item (item, prop->internal_type, int_val, uint_val, double_val);
      }
    }

  }
}


/******************************************************************************
Write to a file the word that represents a PLY data type.

Entry:
  fp   - file pointer
  code - code for type
******************************************************************************/

void write_scalar_type (FILE *fp, int code)
{
  /* make sure this is a valid code */

  if (code <= PLY_START_TYPE || code >= PLY_END_TYPE) {
    fprintf (stderr, "write_scalar_type: bad data code = %d\n", code);
    exit (-1);
  }

  /* write the code to a file */

  fprintf (fp, "%s", type_names[code]);
}


/******************************************************************************
Get a text line from a file and break it up into words.

IMPORTANT: The calling routine call "free" on the returned pointer once
finished with it.

Entry:
  fp - file to read from

Exit:
  nwords    - number of words returned
  orig_line - the original line of characters
  returns a list of words from the line, or NULL if end-of-file
******************************************************************************/

char **get_words(FILE *fp, int *nwords, char **orig_line)
{
#define BIG_STRING 4096
  static char str[BIG_STRING];
  static char str_copy[BIG_STRING];
  char **words;
  int max_words = 10;
  int num_words = 0;
  char *ptr,*ptr2;
  char *result;

  words = (char **) myalloc (sizeof (char *) * max_words);

  /* read in a line */
  result = fgets (str, BIG_STRING, fp);
  if (result == NULL) {
    *nwords = 0;
    *orig_line = NULL;
    return (NULL);
  }

  /* convert line-feed and tabs into spaces */
  /* (this guarentees that there will be a space before the */
  /*  null character at the end of the string) */

  str[BIG_STRING-2] = ' ';
  str[BIG_STRING-1] = '\0';

  for (ptr = str, ptr2 = str_copy; *ptr != '\0'; ptr++, ptr2++) {
    *ptr2 = *ptr;
    if (*ptr == '\t') {
      *ptr = ' ';
      *ptr2 = ' ';
    }
    else if (*ptr == '\n') {
      *ptr = ' ';
      *ptr2 = '\0';
      break;
    }
  }

  /* find the words in the line */

  ptr = str;
  while (*ptr != '\0') {

    /* jump over leading spaces */
    while (*ptr == ' ')
      ptr++;

    /* break if we reach the end */
    if (*ptr == '\0')
      break;

    /* save pointer to beginning of word */
    if (num_words >= max_words) {
      max_words += 10;
      words = (char **) realloc (words, sizeof (char *) * max_words);
    }
    words[num_words++] = ptr;

    /* jump over non-spaces */
    while (*ptr != ' ')
      ptr++;

    /* place a null character here to mark the end of the word */
    *ptr++ = '\0';
  }

  /* return the list of words */
  *nwords = num_words;
  *orig_line = str_copy;
  return (words);
}


/******************************************************************************
Return the value of an item, given a pointer to it and its type.

Entry:
  item - pointer to item
  type - data type that "item" points to

Exit:
  returns a double-precision float that contains the value of the item
******************************************************************************/

double get_item_value(char *item, int type)
{
  unsigned char *puchar;
  char *pchar;
  short int *pshort;
  unsigned short int *pushort;
  int *pint;
  unsigned int *puint;
  float *pfloat;
  double *pdouble;
  int int_value;
  unsigned int uint_value;
  double double_value;

  switch (type) {
    case PLY_CHAR:
      pchar = (char *) item;
      int_value = *pchar;
      return ((double) int_value);
    case PLY_UCHAR:
      puchar = (unsigned char *) item;
      int_value = *puchar;
      return ((double) int_value);
    case PLY_SHORT:
      pshort = (short int *) item;
      int_value = *pshort;
      return ((double) int_value);
    case PLY_USHORT:
      pushort = (unsigned short int *) item;
      int_value = *pushort;
      return ((double) int_value);
    case PLY_INT:
      pint = (int *) item;
      int_value = *pint;
      return ((double) int_value);
    case PLY_UINT:
      puint = (unsigned int *) item;
      uint_value = *puint;
      return ((double) uint_value);
    case PLY_FLOAT:
      pfloat = (float *) item;
      double_value = *pfloat;
      return (double_value);
    case PLY_DOUBLE:
      pdouble = (double *) item;
      double_value = *pdouble;
      return (double_value);
    default:
      fprintf (stderr, "get_item_value: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Write out an item to a file as raw binary bytes.

Entry:
  fp         - file to write to
  int_val    - integer version of item
  uint_val   - unsigned integer version of item
  double_val - double-precision float version of item
  type       - data type to write out
******************************************************************************/

void write_binary_item(
  FILE *fp,
  int int_val,
  unsigned int uint_val,
  double double_val,
  int type
)
{
  unsigned char uchar_val;
  char char_val;
  unsigned short ushort_val;
  short short_val;
  float float_val;

  switch (type) {
    case PLY_CHAR:
      char_val = (char)int_val;
      fwrite (&char_val, 1, 1, fp);
      break;
    case PLY_SHORT:
      short_val = (short)int_val;
      fwrite (&short_val, 2, 1, fp);
      break;
    case PLY_INT:
      fwrite (&int_val, 4, 1, fp);
      break;
    case PLY_UCHAR:
      uchar_val = (unsigned char) uint_val;
      fwrite (&uchar_val, 1, 1, fp);
      break;
    case PLY_USHORT:
      ushort_val = (unsigned short)uint_val;
      fwrite (&ushort_val, 2, 1, fp);
      break;
    case PLY_UINT:
      fwrite (&uint_val, 4, 1, fp);
      break;
    case PLY_FLOAT:
      float_val = (float) double_val;
      fwrite (&float_val, 4, 1, fp);
      break;
    case PLY_DOUBLE:
      fwrite (&double_val, 8, 1, fp);
      break;
    default:
      fprintf (stderr, "write_binary_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Write out an item to a file as ascii characters.

Entry:
  fp         - file to write to
  int_val    - integer version of item
  uint_val   - unsigned integer version of item
  double_val - double-precision float version of item
  type       - data type to write out
******************************************************************************/

void write_ascii_item(
  FILE *fp,
  int int_val,
  unsigned int uint_val,
  double double_val,
  int type
)
{
  switch (type) {
    case PLY_CHAR:
    case PLY_SHORT:
    case PLY_INT:
      fprintf (fp, "%d ", int_val);
      break;
    case PLY_UCHAR:
    case PLY_USHORT:
    case PLY_UINT:
      fprintf (fp, "%u ", uint_val);
      break;
    case PLY_FLOAT:
    case PLY_DOUBLE:
      fprintf (fp, "%g ", double_val);
      break;
    default:
      fprintf (stderr, "write_ascii_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Write out an item to a file as ascii characters.

Entry:
  fp   - file to write to
  item - pointer to item to write
  type - data type that "item" points to

Exit:
  returns a double-precision float that contains the value of the written item
******************************************************************************/

double old_write_ascii_item(FILE *fp, char *item, int type)
{
  unsigned char *puchar;
  char *pchar;
  short int *pshort;
  unsigned short int *pushort;
  int *pint;
  unsigned int *puint;
  float *pfloat;
  double *pdouble;
  int int_value;
  unsigned int uint_value;
  double double_value;

  switch (type) {
    case PLY_CHAR:
      pchar = (char *) item;
      int_value = *pchar;
      fprintf (fp, "%d ", int_value);
      return ((double) int_value);
    case PLY_UCHAR:
      puchar = (unsigned char *) item;
      int_value = *puchar;
      fprintf (fp, "%d ", int_value);
      return ((double) int_value);
    case PLY_SHORT:
      pshort = (short int *) item;
      int_value = *pshort;
      fprintf (fp, "%d ", int_value);
      return ((double) int_value);
    case PLY_USHORT:
      pushort = (unsigned short int *) item;
      int_value = *pushort;
      fprintf (fp, "%d ", int_value);
      return ((double) int_value);
    case PLY_INT:
      pint = (int *) item;
      int_value = *pint;
      fprintf (fp, "%d ", int_value);
      return ((double) int_value);
    case PLY_UINT:
      puint = (unsigned int *) item;
      uint_value = *puint;
      fprintf (fp, "%u ", uint_value);
      return ((double) uint_value);
    case PLY_FLOAT:
      pfloat = (float *) item;
      double_value = *pfloat;
      fprintf (fp, "%g ", double_value);
      return (double_value);
    case PLY_DOUBLE:
      pdouble = (double *) item;
      double_value = *pdouble;
      fprintf (fp, "%g ", double_value);
      return (double_value);
    default:
      fprintf (stderr, "old_write_ascii_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Get the value of an item that is in memory, and place the result
into an integer, an unsigned integer and a double.

Entry:
  ptr  - pointer to the item
  type - data type supposedly in the item

Exit:
  int_val    - integer value
  uint_val   - unsigned integer value
  double_val - double-precision floating point value
******************************************************************************/

void get_stored_item(
  void *ptr,
  int type,
  int *int_val,
  unsigned int *uint_val,
  double *double_val
)
{
  switch (type) {
    case PLY_CHAR:
      *int_val = *((char *) ptr);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;
    case PLY_UCHAR:
      *uint_val = *((unsigned char *) ptr);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;
    case PLY_SHORT:
      *int_val = *((short int *) ptr);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;
    case PLY_USHORT:
      *uint_val = *((unsigned short int *) ptr);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;
    case PLY_INT:
      *int_val = *((int *) ptr);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;
    case PLY_UINT:
      *uint_val = *((unsigned int *) ptr);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;
    case PLY_FLOAT:
      *double_val = *((float *) ptr);
      *int_val = (int)*double_val;
      *uint_val = (unsigned int)*double_val;
      break;
    case PLY_DOUBLE:
      *double_val = *((double *) ptr);
      *int_val = (int)*double_val;
      *uint_val =(unsigned int) *double_val;
      break;
    default:
      fprintf (stderr, "get_stored_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Get the value of an item from a binary file, and place the result
into an integer, an unsigned integer and a double.

Entry:
  fp   - file to get item from
  type - data type supposedly in the word

Exit:
  int_val    - integer value
  uint_val   - unsigned integer value
  double_val - double-precision floating point value
******************************************************************************/

void get_binary_item(
  FILE *fp,
  int type,
  int *int_val,
  unsigned int *uint_val,
  double *double_val
)
{
  char c[8];
  void *ptr;

  ptr = (void *) c;

  switch (type) {
    case PLY_CHAR:
      fread (ptr, 1, 1, fp);
      *int_val = *((char *) ptr);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;
    case PLY_UCHAR:
      fread (ptr, 1, 1, fp);
      *uint_val = *((unsigned char *) ptr);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;
    case PLY_SHORT:
      fread (ptr, 2, 1, fp);
      *int_val = *((short int *) ptr);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;
    case PLY_USHORT:
      fread (ptr, 2, 1, fp);
      *uint_val = *((unsigned short int *) ptr);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;
    case PLY_INT:
      fread (ptr, 4, 1, fp);
      *int_val = *((int *) ptr);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;
    case PLY_UINT:
      fread (ptr, 4, 1, fp);
      *uint_val = *((unsigned int *) ptr);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;
    case PLY_FLOAT:
      fread (ptr, 4, 1, fp);
      *double_val = *((float *) ptr);
      *int_val = (int)*double_val;
      *uint_val =(unsigned int) *double_val;
      break;
    case PLY_DOUBLE:
      fread (ptr, 8, 1, fp);
      *double_val = *((double *) ptr);
      *int_val = (int)*double_val;
      *uint_val = (unsigned int)*double_val;
      break;
    default:
      fprintf (stderr, "get_binary_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Extract the value of an item from an ascii word, and place the result
into an integer, an unsigned integer and a double.

Entry:
  word - word to extract value from
  type - data type supposedly in the word

Exit:
  int_val    - integer value
  uint_val   - unsigned integer value
  double_val - double-precision floating point value
******************************************************************************/

void get_ascii_item(
  char *word,
  int type,
  int *int_val,
  unsigned int *uint_val,
  double *double_val
)
{
  switch (type) {
    case PLY_CHAR:
    case PLY_UCHAR:
    case PLY_SHORT:
    case PLY_USHORT:
    case PLY_INT:
      *int_val = atoi (word);
      *uint_val = *int_val;
      *double_val = *int_val;
      break;

    case PLY_UINT:
      *uint_val = strtoul (word, (char **) NULL, 10);
      *int_val = *uint_val;
      *double_val = *uint_val;
      break;

    case PLY_FLOAT:
    case PLY_DOUBLE:
      *double_val = atof (word);
      *int_val = (int) *double_val;
      *uint_val = (unsigned int) *double_val;
      break;

    default:
      fprintf (stderr, "get_ascii_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Store a value into a place being pointed to, guided by a data type.

Entry:
  item       - place to store value
  type       - data type
  int_val    - integer version of value
  uint_val   - unsigned integer version of value
  double_val - double version of value

Exit:
  item - pointer to stored value
******************************************************************************/

void store_item (
  char *item,
  int type,
  int int_val,
  unsigned int uint_val,
  double double_val
)
{
  unsigned char *puchar;
  short int *pshort;
  unsigned short int *pushort;
  int *pint;
  unsigned int *puint;
  float *pfloat;
  double *pdouble;

  switch (type) {
    case PLY_CHAR:
      *item = (char) int_val;
      break;
    case PLY_UCHAR:
      puchar = (unsigned char *) item;
      *puchar = (unsigned char)uint_val;
      break;
    case PLY_SHORT:
      pshort = (short *) item;
      *pshort = (short)int_val;
      break;
    case PLY_USHORT:
      pushort = (unsigned short *) item;
      *pushort = (unsigned short)uint_val;
      break;
    case PLY_INT:
      pint = (int *) item;
      *pint = int_val;
      break;
    case PLY_UINT:
      puint = (unsigned int *) item;
      *puint = uint_val;
      break;
    case PLY_FLOAT:
      pfloat = (float *) item;
      *pfloat = (float)double_val;
      break;
    case PLY_DOUBLE:
      pdouble = (double *) item;
      *pdouble = double_val;
      break;
    default:
      fprintf (stderr, "store_item: bad type = %d\n", type);
      exit (-1);
  }
}


/******************************************************************************
Add an element to a PLY file descriptor.

Entry:
  plyfile - PLY file descriptor
  words   - list of words describing the element
  nwords  - number of words in the list
******************************************************************************/

void add_element (PlyFile *plyfile, char **words)
{
  PlyElement *elem;

  /* create the new element */
  elem = (PlyElement *) myalloc (sizeof (PlyElement));
  elem->name = strdup (words[1]);
  elem->num = atoi (words[2]);
  elem->nprops = 0;

  /* make room for new element in the object's list of elements */
  if (plyfile->nelems == 0)
    plyfile->elems = (PlyElement **) myalloc (sizeof (PlyElement *));
  else
    plyfile->elems = (PlyElement **) realloc (plyfile->elems,
                     sizeof (PlyElement *) * (plyfile->nelems + 1));

  /* add the new element to the object's list */
  plyfile->elems[plyfile->nelems] = elem;
  plyfile->nelems++;
}


/******************************************************************************
Return the type of a property, given the name of the property.

Entry:
  name - name of property type

Exit:
  returns integer code for property, or 0 if not found
******************************************************************************/

int get_prop_type(char *type_name)
{
  int i;

  for (i = PLY_START_TYPE + 1; i < PLY_END_TYPE; i++)
    if (equal_strings (type_name, type_names[i]))
      return (i);

  /* if we get here, we didn't find the type */
  return (0);
}


/******************************************************************************
Add a property to a PLY file descriptor.

Entry:
  plyfile - PLY file descriptor
  words   - list of words describing the property
  nwords  - number of words in the list
******************************************************************************/

void add_property (PlyFile *plyfile, char **words)
{
  PlyProperty *prop;
  PlyElement *elem;

  /* create the new property */

  prop = (PlyProperty *) myalloc (sizeof (PlyProperty));

  if (equal_strings (words[1], "list")) {       /* is a list */
    prop->count_external = get_prop_type (words[2]);
    prop->external_type = get_prop_type (words[3]);
    prop->name = strdup (words[4]);
    prop->is_list = 1;
  }
  else {                                        /* not a list */
    prop->external_type = get_prop_type (words[1]);
    prop->name = strdup (words[2]);
    prop->is_list = 0;
  }

  /* add this property to the list of properties of the current element */

  elem = plyfile->elems[plyfile->nelems - 1];

  if (elem->nprops == 0)
    elem->props = (PlyProperty **) myalloc (sizeof (PlyProperty *));
  else
    elem->props = (PlyProperty **) realloc (elem->props,
                  sizeof (PlyProperty *) * (elem->nprops + 1));

  elem->props[elem->nprops] = prop;
  elem->nprops++;
}


/******************************************************************************
Add a comment to a PLY file descriptor.

Entry:
  plyfile - PLY file descriptor
  line    - line containing comment
******************************************************************************/

void add_comment (PlyFile *plyfile, char *line)
{
  int i;

  /* skip over "comment" and leading spaces and tabs */
  i = 7;
  while (line[i] == ' ' || line[i] == '\t')
    i++;

  ply_put_comment (plyfile, &line[i]);
}


/******************************************************************************
Add a some object information to a PLY file descriptor.

Entry:
  plyfile - PLY file descriptor
  line    - line containing text info
******************************************************************************/

void add_obj_info (PlyFile *plyfile, char *line)
{
  int i;

  /* skip over "obj_info" and leading spaces and tabs */
  i = 8;
  while (line[i] == ' ' || line[i] == '\t')
    i++;

  ply_put_obj_info (plyfile, &line[i]);
}


/******************************************************************************
Copy a property.
******************************************************************************/

void copy_property(PlyProperty *dest, PlyProperty *src)
{
  dest->name = strdup (src->name);
  dest->external_type = src->external_type;
  dest->internal_type = src->internal_type;
  dest->offset = src->offset;

  dest->is_list = src->is_list;
  dest->count_external = src->count_external;
  dest->count_internal = src->count_internal;
  dest->count_offset = src->count_offset;
}


/******************************************************************************
Allocate some memory.

Entry:
  size  - amount of memory requested (in bytes)
  lnum  - line number from which memory was requested
  fname - file name from which memory was requested
******************************************************************************/

static char *my_alloc(int size, int lnum, char *fname)
{
  char *ptr;

  ptr = (char *) malloc (size);

  if (ptr == 0) {
    fprintf(stderr, "Memory allocation bombed on line %d in %s\n", lnum, fname);
  }

  return (ptr);
}

