// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/block_structure.h"
#include "ceres/matrix_proto.h"

namespace ceres {
namespace internal {

bool CellLessThan(const Cell& lhs, const Cell& rhs) {
  return (lhs.block_id < rhs.block_id);
}

#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
void ProtoToBlockStructure(const BlockStructureProto &proto,
                           CompressedRowBlockStructure *block_structure) {
  // Decode the column blocks.
  block_structure->cols.resize(proto.cols_size());
  for (int i = 0; i < proto.cols_size(); ++i) {
    block_structure->cols[i].size = proto.cols(i).size();
    block_structure->cols[i].position =
        proto.cols(i).position();
  }
  // Decode the row structure.
  block_structure->rows.resize(proto.rows_size());
  for (int i = 0; i < proto.rows_size(); ++i) {
    const CompressedRowProto &row = proto.rows(i);
    block_structure->rows[i].block.size = row.block().size();
    block_structure->rows[i].block.position = row.block().position();

    // Copy the cells within the row.
    block_structure->rows[i].cells.resize(row.cells_size());
    for (int j = 0; j < row.cells_size(); ++j) {
      const CellProto &cell = row.cells(j);
      block_structure->rows[i].cells[j].block_id = cell.block_id();
      block_structure->rows[i].cells[j].position = cell.position();
    }
  }
}

void BlockStructureToProto(const CompressedRowBlockStructure &block_structure,
                           BlockStructureProto *proto) {
  // Encode the column blocks.
  for (int i = 0; i < block_structure.cols.size(); ++i) {
    BlockProto *block = proto->add_cols();
    block->set_size(block_structure.cols[i].size);
    block->set_position(block_structure.cols[i].position);
  }
  // Encode the row structure.
  for (int i = 0; i < block_structure.rows.size(); ++i) {
    CompressedRowProto *row = proto->add_rows();
    BlockProto *block = row->mutable_block();
    block->set_size(block_structure.rows[i].block.size);
    block->set_position(block_structure.rows[i].block.position);
    for (int j = 0; j < block_structure.rows[i].cells.size(); ++j) {
      CellProto *cell = row->add_cells();
      cell->set_block_id(block_structure.rows[i].cells[j].block_id);
      cell->set_position(block_structure.rows[i].cells[j].position);
    }
  }
}
#endif

}  // namespace internal
}  // namespace ceres
