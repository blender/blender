/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_function_ref.hh"

struct SpaceOutliner;

namespace blender::ed::outliner {

struct TreeElement;

namespace tree_iterator {

using VisitorFn = FunctionRef<void(TreeElement *)>;

/**
 * Preorder (meaning depth-first) traversal of all elements (regardless of collapsed state).
 * Freeing the currently visited element in \a visitor is fine.
 */
void all(const SpaceOutliner &space_outliner, VisitorFn visitor);
void all(const ListBaseT<TreeElement> &subtree, VisitorFn visitor);

/**
 * Preorder (meaning depth-first) traversal of all elements not part of a collapsed sub-tree.
 * Freeing the currently visited element in \a visitor is fine (but not its tree-store element).
 */
void all_open(const SpaceOutliner &, VisitorFn visitor);
void all_open(const SpaceOutliner &, const ListBaseT<TreeElement> &subtree, VisitorFn visitor);

}  // namespace tree_iterator
}  // namespace blender::ed::outliner
