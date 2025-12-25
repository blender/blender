/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_listbase.h"

#include "DNA_sequence_types.h"

#include "SEQ_connect.hh"

namespace blender::seq {

static void strip_connections_free(Strip *strip)
{
  if (strip == nullptr) {
    return;
  }
  ListBaseT<StripConnection> *connections = &strip->connections;
  for (StripConnection &con : connections->items_mutable()) {
    MEM_delete(&con);
  }
  BLI_listbase_clear(connections);
}

void connections_duplicate(ListBaseT<StripConnection> *connections_dst,
                           ListBaseT<StripConnection> *connections_src)
{
  for (StripConnection &con : *connections_src) {
    StripConnection *con_duplicate = MEM_new_for_free<StripConnection>(__func__, con);
    BLI_addtail(connections_dst, con_duplicate);
  }
}

bool disconnect(Strip *strip)
{
  if (strip == nullptr || BLI_listbase_is_empty(&strip->connections)) {
    return false;
  }
  /* Remove `StripConnections` from other strips' `connections` list that point to `strip`. */
  for (StripConnection &con_strip : strip->connections) {
    Strip *other = con_strip.strip_ref;
    for (StripConnection &con_other : other->connections.items_mutable()) {
      if (con_other.strip_ref == strip) {
        BLI_remlink(&other->connections, &con_other);
        MEM_delete(&con_other);
      }
    }
  }
  /* Now clear `connections` for `strip` itself. */
  strip_connections_free(strip);

  return true;
}

bool disconnect(VectorSet<Strip *> &strip_list)
{
  bool changed = false;
  for (Strip *strip : strip_list) {
    changed |= disconnect(strip);
  }

  return changed;
}

void cut_one_way_connections(Strip *strip)
{
  if (strip == nullptr) {
    return;
  }
  for (StripConnection &con_strip : strip->connections.items_mutable()) {
    Strip *other = con_strip.strip_ref;
    bool is_one_way = true;
    for (StripConnection &con_other : other->connections) {
      if (con_other.strip_ref == strip) {
        /* The `other` sequence has a bidirectional connection with `strip`. */
        is_one_way = false;
        break;
      }
    }
    if (is_one_way) {
      BLI_remlink(&strip->connections, &con_strip);
      MEM_delete(&con_strip);
    }
  }
}

void connect(Strip *strip1, Strip *strip2)
{
  if (strip1 == nullptr || strip2 == nullptr) {
    return;
  }
  VectorSet<Strip *> strip_list;
  strip_list.add(strip1);
  strip_list.add(strip2);

  connect(strip_list);
}

void connect(VectorSet<Strip *> &strip_list)
{
  strip_list.remove_if([&](Strip *strip) { return strip == nullptr; });

  for (Strip *strip1 : strip_list) {
    disconnect(strip1);
    for (Strip *strip2 : strip_list) {
      if (strip1 == strip2) {
        continue;
      }
      StripConnection *con = MEM_new_for_free<StripConnection>("stripconnection");
      con->strip_ref = strip2;
      BLI_addtail(&strip1->connections, con);
    }
  }
}

VectorSet<Strip *> connected_strips_get(const Strip *strip)
{
  VectorSet<Strip *> connections;
  if (strip != nullptr) {
    for (StripConnection &con : strip->connections) {
      connections.add(con.strip_ref);
    }
  }
  return connections;
}

bool is_strip_connected(const Strip *strip)
{
  if (strip == nullptr) {
    return false;
  }
  return !BLI_listbase_is_empty(&strip->connections);
}

bool are_strips_connected_together(VectorSet<Strip *> &strip_list)
{
  const int expected_connection_num = strip_list.size() - 1;
  for (Strip *strip1 : strip_list) {
    VectorSet<Strip *> connections = connected_strips_get(strip1);
    int found_connection_num = connections.size();
    if (found_connection_num != expected_connection_num) {
      return false;
    }
    for (Strip *strip2 : connections) {
      if (!strip_list.contains(strip2)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace blender::seq
