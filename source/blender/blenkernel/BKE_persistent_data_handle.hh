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

#pragma once

/** \file
 * \ingroup bke
 *
 * A PersistentDataHandle is a weak reference to some data in a Blender file. The handle itself is
 * just a number. A PersistentDataHandleMap is used to convert between handles and the actual data.
 */

#include "BLI_map.hh"

#include "DNA_ID.h"

struct Collection;
struct Object;

namespace blender::bke {

class PersistentDataHandleMap;

class PersistentDataHandle {
 private:
  /* Negative values indicate that the handle is "empty". */
  int32_t handle_;

  friend PersistentDataHandleMap;

 protected:
  PersistentDataHandle(int handle) : handle_(handle)
  {
  }

 public:
  PersistentDataHandle() : handle_(-1)
  {
  }

  friend bool operator==(const PersistentDataHandle &a, const PersistentDataHandle &b)
  {
    return a.handle_ == b.handle_;
  }

  friend bool operator!=(const PersistentDataHandle &a, const PersistentDataHandle &b)
  {
    return !(a == b);
  }

  friend std::ostream &operator<<(std::ostream &stream, const PersistentDataHandle &a)
  {
    stream << a.handle_;
    return stream;
  }

  uint64_t hash() const
  {
    return static_cast<uint64_t>(handle_);
  }
};

class PersistentIDHandle : public PersistentDataHandle {
  friend PersistentDataHandleMap;
  using PersistentDataHandle::PersistentDataHandle;
};

class PersistentObjectHandle : public PersistentIDHandle {
  friend PersistentDataHandleMap;
  using PersistentIDHandle::PersistentIDHandle;
};

class PersistentCollectionHandle : public PersistentIDHandle {
  friend PersistentDataHandleMap;
  using PersistentIDHandle::PersistentIDHandle;
};

class PersistentDataHandleMap {
 private:
  Map<int32_t, ID *> id_by_handle_;
  Map<ID *, int32_t> handle_by_id_;

 public:
  void add(int32_t handle, ID &id)
  {
    BLI_assert(handle >= 0);
    handle_by_id_.add(&id, handle);
    id_by_handle_.add(handle, &id);
  }

  PersistentIDHandle lookup(ID *id) const
  {
    const int handle = handle_by_id_.lookup_default(id, -1);
    return PersistentIDHandle(handle);
  }

  PersistentObjectHandle lookup(Object *object) const
  {
    const int handle = handle_by_id_.lookup_default((ID *)object, -1);
    return PersistentObjectHandle(handle);
  }

  PersistentCollectionHandle lookup(Collection *collection) const
  {
    const int handle = handle_by_id_.lookup_default((ID *)collection, -1);
    return PersistentCollectionHandle(handle);
  }

  ID *lookup(const PersistentIDHandle &handle) const
  {
    ID *id = id_by_handle_.lookup_default(handle.handle_, nullptr);
    return id;
  }

  Object *lookup(const PersistentObjectHandle &handle) const
  {
    ID *id = this->lookup((const PersistentIDHandle &)handle);
    if (id == nullptr) {
      return nullptr;
    }
    if (GS(id->name) != ID_OB) {
      return nullptr;
    }
    return (Object *)id;
  }

  Collection *lookup(const PersistentCollectionHandle &handle) const
  {
    ID *id = this->lookup((const PersistentIDHandle &)handle);
    if (id == nullptr) {
      return nullptr;
    }
    if (GS(id->name) != ID_GR) {
      return nullptr;
    }
    return (Collection *)id;
  }
};

}  // namespace blender::bke
