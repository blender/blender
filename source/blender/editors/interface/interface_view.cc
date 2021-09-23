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
 * \ingroup edinterface
 *
 * This part of the UI-View API is mostly needed to support persistent state of items within the
 * view. Views are stored in #uiBlock's, and kept alive with it until after the next redraw. So we
 * can compare the old view items with the new view items and keep state persistent for matching
 * ones.
 */

#include <memory>
#include <variant>

#include "BLI_listbase.h"

#include "interface_intern.h"

#include "UI_interface.hh"
#include "UI_tree_view.hh"

using namespace blender;
using namespace blender::ui;

/**
 * Wrapper to store views in a #ListBase. There's no `uiView` base class, we just store views as a
 * #std::variant.
 */
struct ViewLink : public Link {
  using TreeViewPtr = std::unique_ptr<AbstractTreeView>;

  std::string idname;
  /* Note: Can't use std::get() on this until minimum macOS deployment target is 10.14. */
  std::variant<TreeViewPtr> view;
};

template<class T> T *get_view_from_link(ViewLink &link)
{
  auto *t_uptr = std::get_if<std::unique_ptr<T>>(&link.view);
  return t_uptr ? t_uptr->get() : nullptr;
}

/**
 * Override this for all available tree types.
 */
AbstractTreeView *UI_block_add_view(uiBlock &block,
                                    StringRef idname,
                                    std::unique_ptr<AbstractTreeView> tree_view)
{
  ViewLink *view_link = OBJECT_GUARDED_NEW(ViewLink);
  BLI_addtail(&block.views, view_link);

  view_link->view = std::move(tree_view);
  view_link->idname = idname;

  return get_view_from_link<AbstractTreeView>(*view_link);
}

void ui_block_free_views(uiBlock *block)
{
  LISTBASE_FOREACH_MUTABLE (ViewLink *, link, &block->views) {
    OBJECT_GUARDED_DELETE(link, ViewLink);
  }
}

static StringRef ui_block_view_find_idname(const uiBlock &block, const AbstractTreeView &view)
{
  /* First get the idname the of the view we're looking for. */
  LISTBASE_FOREACH (ViewLink *, view_link, &block.views) {
    if (get_view_from_link<AbstractTreeView>(*view_link) == &view) {
      return view_link->idname;
    }
  }

  return {};
}

uiTreeViewHandle *ui_block_view_find_matching_in_old_block(const uiBlock *new_block,
                                                           const uiTreeViewHandle *new_view_handle)
{
  const AbstractTreeView &needle_view = reinterpret_cast<const AbstractTreeView &>(
      *new_view_handle);

  uiBlock *old_block = new_block->oldblock;
  if (!old_block) {
    return nullptr;
  }

  StringRef idname = ui_block_view_find_idname(*new_block, needle_view);
  if (idname.is_empty()) {
    return nullptr;
  }

  LISTBASE_FOREACH (ViewLink *, old_view_link, &old_block->views) {
    if (old_view_link->idname == idname) {
      return reinterpret_cast<uiTreeViewHandle *>(
          get_view_from_link<AbstractTreeView>(*old_view_link));
    }
  }

  return nullptr;
}
