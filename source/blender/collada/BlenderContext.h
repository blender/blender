/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup collada
 */

#ifndef __BLENDERCONTEXT_H__
#define __BLENDERCONTEXT_H__

#ifdef __cplusplus

extern "C" {
#endif

#include "DNA_object_types.h"
#include "BLI_linklist.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DNA_layer_types.h"

typedef float(Vector)[3];
typedef float(Matrix)[4][4];
typedef double(DMatrix)[4][4];

typedef enum BC_global_forward_axis {
  BC_GLOBAL_FORWARD_X = 0,
  BC_GLOBAL_FORWARD_Y = 1,
  BC_GLOBAL_FORWARD_Z = 2,
  BC_GLOBAL_FORWARD_MINUS_X = 3,
  BC_GLOBAL_FORWARD_MINUS_Y = 4,
  BC_GLOBAL_FORWARD_MINUS_Z = 5
} BC_global_forward_axis;

typedef enum BC_global_up_axis {
  BC_GLOBAL_UP_X = 0,
  BC_GLOBAL_UP_Y = 1,
  BC_GLOBAL_UP_Z = 2,
  BC_GLOBAL_UP_MINUS_X = 3,
  BC_GLOBAL_UP_MINUS_Y = 4,
  BC_GLOBAL_UP_MINUS_Z = 5
} BC_global_up_axis;

static const BC_global_forward_axis BC_DEFAULT_FORWARD = BC_GLOBAL_FORWARD_Y;
static const BC_global_up_axis BC_DEFAULT_UP = BC_GLOBAL_UP_Z;

bool bc_is_in_Export_set(LinkNode *export_set, Object *ob, ViewLayer *view_layer);
bool bc_is_base_node(LinkNode *export_set, Object *ob, ViewLayer *view_layer);
Object *bc_get_highest_exported_ancestor_or_self(LinkNode *export_set,
                                                 Object *ob,
                                                 ViewLayer *view_layer);
int bc_is_marked(Object *ob);
void bc_remove_mark(Object *ob);
void bc_set_mark(Object *ob);

#ifdef __cplusplus
}

class BCMatrix {

 private:
  mutable float matrix[4][4];
  mutable float size[3];
  mutable float rot[3];
  mutable float loc[3];
  mutable float q[4];

  void unit();
  void copy(Matrix &r, Matrix &a);

 public:
  float (&location() const)[3];
  float (&rotation() const)[3];
  float (&scale() const)[3];
  float (&quat() const)[4];

  BCMatrix(BC_global_forward_axis global_forward_axis, BC_global_up_axis global_up_axis);
  BCMatrix(const BCMatrix &mat);
  BCMatrix(Matrix &mat);
  BCMatrix(Object *ob);
  BCMatrix();

  void get_matrix(DMatrix &matrix, const bool transposed = false, const int precision = -1) const;
  void get_matrix(Matrix &matrix,
                  const bool transposed = false,
                  const int precision = -1,
                  const bool inverted = false) const;
  void set_transform(Object *ob);
  void set_transform(Matrix &mat);
  void add_transform(Matrix &to,
                     const Matrix &transform,
                     const Matrix &from,
                     const bool inverted = false);
  void apply_transform(Matrix &to,
                     const Matrix &transform,
                     const Matrix &from,
                     const bool inverted = false);
  void add_inverted_transform(Matrix &to, const Matrix &transform, const Matrix &from);
  void add_transform(const Matrix &matrix, const bool inverted = false);
  void add_transform(const BCMatrix &matrix, const bool inverted = false);
  void apply_transform(const BCMatrix &matrix, const bool inverted = false);

  const bool in_range(const BCMatrix &other, float distance) const;
  static void sanitize(Matrix &matrix, int precision);
  static void transpose(Matrix &matrix);
};

class BlenderContext {
 private:
  bContext *context;
  Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  Main *main;

 public:
  BlenderContext(bContext *C);
  bContext *get_context();
  Depsgraph *get_depsgraph();
  Scene *get_scene();
  Scene *get_evaluated_scene();
  Object *get_evaluated_object(Object *ob);
  ViewLayer *get_view_layer();
  Main *get_main();
};
#endif

#endif
