/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_blenlib.h"

#include "DNA_sequence_types.h"

#include "SEQ_connect.hh"

static void strip_connections_free(Strip *strip)
{
  if (strip == nullptr) {
    return;
  }
  ListBase *connections = &strip->connections;
  LISTBASE_FOREACH_MUTABLE (SeqConnection *, con, connections) {
    MEM_delete(con);
  }
  BLI_listbase_clear(connections);
}

void SEQ_connections_duplicate(ListBase *connections_dst, ListBase *connections_src)
{
  LISTBASE_FOREACH (SeqConnection *, con, connections_src) {
    SeqConnection *con_duplicate = MEM_cnew<SeqConnection>(__func__, *con);
    BLI_addtail(connections_dst, con_duplicate);
  }
}

bool SEQ_disconnect(Strip *strip)
{
  if (strip == nullptr || BLI_listbase_is_empty(&strip->connections)) {
    return false;
  }
  /* Remove `SeqConnections` from other strips' `connections` list that point to `strip`. */
  LISTBASE_FOREACH (SeqConnection *, con_seq, &strip->connections) {
    Strip *other = con_seq->seq_ref;
    LISTBASE_FOREACH_MUTABLE (SeqConnection *, con_other, &other->connections) {
      if (con_other->seq_ref == strip) {
        BLI_remlink(&other->connections, con_other);
        MEM_delete(con_other);
      }
    }
  }
  /* Now clear `connections` for `strip` itself. */
  strip_connections_free(strip);

  return true;
}

bool SEQ_disconnect(blender::VectorSet<Strip *> &strip_list)
{
  bool changed = false;
  for (Strip *strip : strip_list) {
    changed |= SEQ_disconnect(strip);
  }

  return changed;
}

void SEQ_cut_one_way_connections(Strip *strip)
{
  if (strip == nullptr) {
    return;
  }
  LISTBASE_FOREACH_MUTABLE (SeqConnection *, con_seq, &strip->connections) {
    Strip *other = con_seq->seq_ref;
    bool is_one_way = true;
    LISTBASE_FOREACH (SeqConnection *, con_other, &other->connections) {
      if (con_other->seq_ref == strip) {
        /* The `other` sequence has a bidirectional connection with `strip`. */
        is_one_way = false;
        break;
      }
    }
    if (is_one_way) {
      BLI_remlink(&strip->connections, con_seq);
      MEM_delete(con_seq);
    }
  }
}

void SEQ_connect(Strip *seq1, Strip *seq2)
{
  if (seq1 == nullptr || seq2 == nullptr) {
    return;
  }
  blender::VectorSet<Strip *> strip_list;
  strip_list.add(seq1);
  strip_list.add(seq2);

  SEQ_connect(strip_list);
}

void SEQ_connect(blender::VectorSet<Strip *> &strip_list)
{
  strip_list.remove_if([&](Strip *strip) { return strip == nullptr; });

  for (Strip *seq1 : strip_list) {
    SEQ_disconnect(seq1);
    for (Strip *seq2 : strip_list) {
      if (seq1 == seq2) {
        continue;
      }
      SeqConnection *con = MEM_cnew<SeqConnection>("seqconnection");
      con->seq_ref = seq2;
      BLI_addtail(&seq1->connections, con);
    }
  }
}

blender::VectorSet<Strip *> SEQ_get_connected_strips(const Strip *strip)
{
  blender::VectorSet<Strip *> connections;
  if (strip != nullptr) {
    LISTBASE_FOREACH (SeqConnection *, con, &strip->connections) {
      connections.add(con->seq_ref);
    }
  }
  return connections;
}

bool SEQ_is_strip_connected(const Strip *strip)
{
  if (strip == nullptr) {
    return false;
  }
  return !BLI_listbase_is_empty(&strip->connections);
}

bool SEQ_are_strips_connected_together(blender::VectorSet<Strip *> &strip_list)
{
  const int expected_connection_num = strip_list.size() - 1;
  for (Strip *seq1 : strip_list) {
    blender::VectorSet<Strip *> connections = SEQ_get_connected_strips(seq1);
    int found_connection_num = connections.size();
    if (found_connection_num != expected_connection_num) {
      return false;
    }
    for (Strip *seq2 : connections) {
      if (!strip_list.contains(seq2)) {
        return false;
      }
    }
  }
  return true;
}
