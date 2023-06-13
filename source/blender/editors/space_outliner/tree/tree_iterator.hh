/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "BLI_function_ref.hh"

struct ListBase;
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
void all(const ListBase &subtree, VisitorFn visitor);

/**
 * Preorder (meaning depth-first) traversal of all elements not part of a collapsed sub-tree.
 * Freeing the currently visited element in \a visitor is fine (but not its tree-store element).
 */
void all_open(const SpaceOutliner &, VisitorFn visitor);
void all_open(const SpaceOutliner &, const ListBase &subtree, VisitorFn visitor);

}  // namespace tree_iterator
}  // namespace blender::ed::outliner
