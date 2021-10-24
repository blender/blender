#ifndef RPLY_H
#define RPLY_H
/* ----------------------------------------------------------------------
 * RPly library, read/write PLY files
 * Diego Nehab, IMPA
 * http://www.impa.br/~diego/software/rply
 *
 * This library is distributed under the MIT License. See notice
 * at the end of this file.
 * ---------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

#define RPLY_VERSION   "RPly 1.1.3"
#define RPLY_COPYRIGHT "Copyright (C) 2003-2013 Diego Nehab"
#define RPLY_AUTHORS   "Diego Nehab"

/* ----------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */
/* structures are opaque */
typedef struct t_ply_ *p_ply;
typedef struct t_ply_element_ *p_ply_element;
typedef struct t_ply_property_ *p_ply_property;
typedef struct t_ply_argument_ *p_ply_argument;

/* ply format mode type */
typedef enum e_ply_storage_mode_ {
    PLY_BIG_ENDIAN,
    PLY_LITTLE_ENDIAN,
    PLY_ASCII,   
    PLY_DEFAULT      /* has to be the last in enum */
} e_ply_storage_mode; /* order matches ply_storage_mode_list */

/* ply data type */
typedef enum e_ply_type {
    PLY_INT8, PLY_UINT8, PLY_INT16, PLY_UINT16, 
    PLY_INT32, PLY_UINT32, PLY_FLOAT32, PLY_FLOAT64,
    PLY_CHAR, PLY_UCHAR, PLY_SHORT, PLY_USHORT,
    PLY_INT, PLY_UINT, PLY_FLOAT, PLY_DOUBLE,
    PLY_LIST    /* has to be the last in enum */
} e_ply_type;   /* order matches ply_type_list */

/* ----------------------------------------------------------------------
 * Error callback prototype
 *
 * message: error message
 * ply: handle returned by ply_open or ply_create
 * ---------------------------------------------------------------------- */
typedef void (*p_ply_error_cb)(p_ply ply, const char *message);

/* ----------------------------------------------------------------------
 * Gets user data from within an error callback 
 *
 * ply: handle returned by ply_open or ply_create
 * idata,pdata: contextual information set in ply_open or ply_create
 * ---------------------------------------------------------------------- */
int ply_get_ply_user_data(p_ply ply, void **pdata, long *idata);

/* ----------------------------------------------------------------------
 * Opens a PLY file for reading (fails if file is not a PLY file)
 *
 * name: file name
 * error_cb: error callback function
 * idata,pdata: contextual information available to users
 *
 * Returns 1 if successful, 0 otherwise
 * ---------------------------------------------------------------------- */
p_ply ply_open(const char *name, p_ply_error_cb error_cb, long idata, 
        void *pdata);

/* ----------------------------------------------------------------------
 * Reads and parses the header of a PLY file returned by ply_open
 *
 * ply: handle returned by ply_open
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_read_header(p_ply ply);

/* ----------------------------------------------------------------------
 * Property reading callback prototype
 *
 * argument: parameters for property being processed when callback is called
 *
 * Returns 1 if should continue processing file, 0 if should abort.
 * ---------------------------------------------------------------------- */
typedef int (*p_ply_read_cb)(p_ply_argument argument);

/* ----------------------------------------------------------------------
 * Sets up callbacks for property reading after header was parsed
 *
 * ply: handle returned by ply_open
 * element_name: element where property is
 * property_name: property to associate element with
 * read_cb: function to be called for each property value
 * pdata/idata: user data that will be passed to callback
 *
 * Returns 0 if no element or no property in element, returns the
 * number of element instances otherwise. 
 * ---------------------------------------------------------------------- */
long ply_set_read_cb(p_ply ply, const char *element_name, 
        const char *property_name, p_ply_read_cb read_cb, 
        void *pdata, long idata);

/* ----------------------------------------------------------------------
 * Returns information about the element originating a callback
 *
 * argument: handle to argument 
 * element: receives a the element handle (if non-null)
 * instance_index: receives the index of the current element instance 
 *     (if non-null)
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_get_argument_element(p_ply_argument argument, 
        p_ply_element *element, long *instance_index);

/* ----------------------------------------------------------------------
 * Returns information about the property originating a callback
 *
 * argument: handle to argument 
 * property: receives the property handle (if non-null)
 * length: receives the number of values in this property (if non-null)
 * value_index: receives the index of current property value (if non-null)
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_get_argument_property(p_ply_argument argument, 
        p_ply_property *property, long *length, long *value_index);

/* ----------------------------------------------------------------------
 * Returns user data associated with callback 
 *
 * pdata: receives a copy of user custom data pointer (if non-null)
 * idata: receives a copy of user custom data integer (if non-null)
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_get_argument_user_data(p_ply_argument argument, void **pdata, 
        long *idata);

/* ----------------------------------------------------------------------
 * Returns the value associated with a callback
 *
 * argument: handle to argument 
 *
 * Returns the current data item
 * ---------------------------------------------------------------------- */
double ply_get_argument_value(p_ply_argument argument); 

/* ----------------------------------------------------------------------
 * Reads all elements and properties calling the callbacks defined with
 * calls to ply_set_read_cb
 *
 * ply: handle returned by ply_open
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_read(p_ply ply);

/* ----------------------------------------------------------------------
 * Iterates over all elements by returning the next element.
 * Call with NULL to return handle to first element.
 *
 * ply: handle returned by ply_open
 * last: handle of last element returned (NULL for first element)
 *
 * Returns element if successfull or NULL if no more elements
 * ---------------------------------------------------------------------- */
p_ply_element ply_get_next_element(p_ply ply, p_ply_element last);

/* ----------------------------------------------------------------------
 * Iterates over all comments by returning the next comment.
 * Call with NULL to return pointer to first comment.
 *
 * ply: handle returned by ply_open
 * last: pointer to last comment returned (NULL for first comment)
 *
 * Returns comment if successfull or NULL if no more comments
 * ---------------------------------------------------------------------- */
const char *ply_get_next_comment(p_ply ply, const char *last);

/* ----------------------------------------------------------------------
 * Iterates over all obj_infos by returning the next obj_info.
 * Call with NULL to return pointer to first obj_info.
 *
 * ply: handle returned by ply_open
 * last: pointer to last obj_info returned (NULL for first obj_info)
 *
 * Returns obj_info if successfull or NULL if no more obj_infos
 * ---------------------------------------------------------------------- */
const char *ply_get_next_obj_info(p_ply ply, const char *last);

/* ----------------------------------------------------------------------
 * Returns information about an element
 *
 * element: element of interest
 * name: receives a pointer to internal copy of element name (if non-null)
 * ninstances: receives the number of instances of this element (if non-null)
 *
 * Returns 1 if successfull or 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_get_element_info(p_ply_element element, const char** name,
        long *ninstances);

/* ----------------------------------------------------------------------
 * Iterates over all properties by returning the next property.
 * Call with NULL to return handle to first property.
 *
 * element: handle of element with the properties of interest
 * last: handle of last property returned (NULL for first property)
 *
 * Returns element if successfull or NULL if no more properties
 * ---------------------------------------------------------------------- */
p_ply_property ply_get_next_property(p_ply_element element, 
        p_ply_property last);

/* ----------------------------------------------------------------------
 * Returns information about a property
 *
 * property: handle to property of interest
 * name: receives a pointer to internal copy of property name (if non-null)
 * type: receives the property type (if non-null)
 * length_type: for list properties, receives the scalar type of
 *     the length field (if non-null)
 * value_type: for list properties, receives the scalar type of the value 
 *     fields  (if non-null)
 *
 * Returns 1 if successfull or 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_get_property_info(p_ply_property property, const char** name,
        e_ply_type *type, e_ply_type *length_type, e_ply_type *value_type);

/* ----------------------------------------------------------------------
 * Creates new PLY file
 *
 * name: file name
 * storage_mode: file format mode
 *
 * Returns handle to PLY file if successfull, NULL otherwise
 * ---------------------------------------------------------------------- */
p_ply ply_create(const char *name, e_ply_storage_mode storage_mode, 
        p_ply_error_cb error_cb, long idata, void *pdata);

/* ----------------------------------------------------------------------
 * Adds a new element to the PLY file created by ply_create
 *
 * ply: handle returned by ply_create
 * name: name of new element
 * ninstances: number of element of this time in file
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_add_element(p_ply ply, const char *name, long ninstances);

/* ----------------------------------------------------------------------
 * Adds a new property to the last element added by ply_add_element
 *
 * ply: handle returned by ply_create
 * name: name of new property
 * type: property type
 * length_type: scalar type of length field of a list property 
 * value_type: scalar type of value fields of a list property
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_add_property(p_ply ply, const char *name, e_ply_type type,
        e_ply_type length_type, e_ply_type value_type);

/* ----------------------------------------------------------------------
 * Adds a new list property to the last element added by ply_add_element
 *
 * ply: handle returned by ply_create
 * name: name of new property
 * length_type: scalar type of length field of a list property 
 * value_type: scalar type of value fields of a list property
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_add_list_property(p_ply ply, const char *name, 
        e_ply_type length_type, e_ply_type value_type);

/* ----------------------------------------------------------------------
 * Adds a new property to the last element added by ply_add_element
 *
 * ply: handle returned by ply_create
 * name: name of new property
 * type: property type
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_add_scalar_property(p_ply ply, const char *name, e_ply_type type);

/* ----------------------------------------------------------------------
 * Adds a new comment item 
 *
 * ply: handle returned by ply_create
 * comment: pointer to string with comment text
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_add_comment(p_ply ply, const char *comment);

/* ----------------------------------------------------------------------
 * Adds a new obj_info item 
 *
 * ply: handle returned by ply_create
 * comment: pointer to string with obj_info data
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_add_obj_info(p_ply ply, const char *obj_info);

/* ----------------------------------------------------------------------
 * Writes the PLY file header after all element and properties have been
 * defined by calls to ply_add_element and ply_add_property
 *
 * ply: handle returned by ply_create
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_write_header(p_ply ply);

/* ----------------------------------------------------------------------
 * Writes one property value, in the order they should be written to the
 * file. For each element type, write all elements of that type in order.
 * For each element, write all its properties in order. For scalar
 * properties, just write the value. For list properties, write the length 
 * and then each of the values.
 *
 * ply: handle returned by ply_create
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_write(p_ply ply, double value);

/* ----------------------------------------------------------------------
 * Closes a PLY file handle. Releases all memory used by handle
 *
 * ply: handle to be closed. 
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int ply_close(p_ply ply);

#ifdef __cplusplus
}
#endif

#endif /* RPLY_H */

/* ----------------------------------------------------------------------
 * Copyright (C) 2003-2011 Diego Nehab. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ---------------------------------------------------------------------- */
