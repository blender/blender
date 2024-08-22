/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_blenlib.h"

#include "DNA_sequence_types.h"

#include "SEQ_connect.hh"
#include "SEQ_time.hh"

static void seq_connections_free(Sequence *seq)
{
  if (seq == nullptr) {
    return;
  }
  ListBase *connections = &seq->connections;
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

bool SEQ_disconnect(Sequence *seq)
{
  if (seq == nullptr || BLI_listbase_is_empty(&seq->connections)) {
    return false;
  }
  /* Remove `SeqConnections` from other strips' `connections` list that point to `seq`. */
  LISTBASE_FOREACH (SeqConnection *, con_seq, &seq->connections) {
    Sequence *other = con_seq->seq_ref;
    LISTBASE_FOREACH_MUTABLE (SeqConnection *, con_other, &other->connections) {
      if (con_other->seq_ref == seq) {
        BLI_remlink(&other->connections, con_other);
        MEM_delete(con_other);
      }
    }
  }
  /* Now clear `connections` for `seq` itself.*/
  seq_connections_free(seq);

  return true;
}

bool SEQ_disconnect(blender::VectorSet<Sequence *> &seq_list)
{
  bool changed = false;
  for (Sequence *seq : seq_list) {
    changed |= SEQ_disconnect(seq);
  }

  return changed;
}

void SEQ_cut_one_way_connections(Sequence *seq)
{
  if (seq == nullptr) {
    return;
  }
  LISTBASE_FOREACH_MUTABLE (SeqConnection *, con_seq, &seq->connections) {
    Sequence *other = con_seq->seq_ref;
    bool is_one_way = true;
    LISTBASE_FOREACH (SeqConnection *, con_other, &other->connections) {
      if (con_other->seq_ref == seq) {
        /* The `other` sequence has a bidirectional connection with `seq`. */
        is_one_way = false;
        break;
      }
    }
    if (is_one_way) {
      BLI_remlink(&seq->connections, con_seq);
      MEM_delete(con_seq);
    }
  }
}

void SEQ_connect(Sequence *seq1, Sequence *seq2)
{
  if (seq1 == nullptr || seq2 == nullptr) {
    return;
  }
  blender::VectorSet<Sequence *> seq_list;
  seq_list.add(seq1);
  seq_list.add(seq2);

  SEQ_connect(seq_list);
}

void SEQ_connect(blender::VectorSet<Sequence *> &seq_list)
{
  seq_list.remove_if([&](Sequence *seq) { return seq == nullptr; });

  for (Sequence *seq1 : seq_list) {
    SEQ_disconnect(seq1);
    for (Sequence *seq2 : seq_list) {
      if (seq1 == seq2) {
        continue;
      }
      SeqConnection *con = MEM_cnew<SeqConnection>("seqconnection");
      con->seq_ref = seq2;
      BLI_addtail(&seq1->connections, con);
    }
  }
}

blender::VectorSet<Sequence *> SEQ_get_connected_strips(const Sequence *seq)
{
  blender::VectorSet<Sequence *> connections;
  if (seq != nullptr) {
    LISTBASE_FOREACH (SeqConnection *, con, &seq->connections) {
      connections.add(con->seq_ref);
    }
  }
  return connections;
}

bool SEQ_is_strip_connected(const Sequence *seq)
{
  if (seq == nullptr) {
    return false;
  }
  return !BLI_listbase_is_empty(&seq->connections);
}

bool SEQ_are_strips_connected_together(blender::VectorSet<Sequence *> &seq_list)
{
  const int expected_connection_num = seq_list.size() - 1;
  for (Sequence *seq1 : seq_list) {
    blender::VectorSet<Sequence *> connections = SEQ_get_connected_strips(seq1);
    int found_connection_num = connections.size();
    if (found_connection_num != expected_connection_num) {
      return false;
    }
    for (Sequence *seq2 : connections) {
      if (!seq_list.contains(seq2)) {
        return false;
      }
    }
  }
  return true;
}
