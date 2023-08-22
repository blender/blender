/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "COM_ConstantOperation.h"

namespace blender::compositor::tests {

class NonHashedOperation : public NodeOperation {
 public:
  NonHashedOperation(int id)
  {
    set_id(id);
    add_output_socket(DataType::Value);
    set_width(2);
    set_height(3);
  }
};

class NonHashedConstantOperation : public ConstantOperation {
  float constant_;

 public:
  NonHashedConstantOperation(int id)
  {
    set_id(id);
    add_output_socket(DataType::Value);
    set_width(2);
    set_height(3);
    constant_ = 1.0f;
  }

  const float *get_constant_elem() override
  {
    return &constant_;
  }

  void set_constant(float value)
  {
    constant_ = value;
  }
};

class HashedOperation : public NodeOperation {
 private:
  int param1;
  float param2;

 public:
  HashedOperation(NodeOperation &input, int width, int height)
  {
    add_input_socket(DataType::Value);
    add_output_socket(DataType::Color);
    set_width(width);
    set_height(height);
    param1 = 2;
    param2 = 7.0f;

    get_input_socket(0)->set_link(input.get_output_socket());
  }

  void set_param1(int value)
  {
    param1 = value;
  }

  void hash_output_params() override
  {
    hash_params(param1, param2);
  }
};

static void test_non_equal_hashes_compare(NodeOperationHash &h1,
                                          NodeOperationHash &h2,
                                          NodeOperationHash &h3)
{
  if (h1 < h2) {
    if (h3 < h1) {
      EXPECT_TRUE(h3 < h2);
    }
    else if (h3 < h2) {
      EXPECT_TRUE(h1 < h3);
    }
    else {
      EXPECT_TRUE(h1 < h3);
      EXPECT_TRUE(h2 < h3);
    }
  }
  else {
    EXPECT_TRUE(h2 < h1);
  }
}

TEST(NodeOperation, generate_hash)
{
  /* Constant input. */
  {
    NonHashedConstantOperation input_op1(1);
    input_op1.set_constant(1.0f);
    EXPECT_EQ(input_op1.generate_hash(), std::nullopt);

    HashedOperation op1(input_op1, 6, 4);
    std::optional<NodeOperationHash> hash1_opt = op1.generate_hash();
    EXPECT_NE(hash1_opt, std::nullopt);
    NodeOperationHash hash1 = *hash1_opt;

    NonHashedConstantOperation input_op2(1);
    input_op2.set_constant(1.0f);
    HashedOperation op2(input_op2, 6, 4);
    NodeOperationHash hash2 = *op2.generate_hash();
    EXPECT_EQ(hash1, hash2);

    input_op2.set_constant(3.0f);
    hash2 = *op2.generate_hash();
    EXPECT_NE(hash1, hash2);
  }

  /* Non constant input. */
  {
    NonHashedOperation input_op(1);
    EXPECT_EQ(input_op.generate_hash(), std::nullopt);

    HashedOperation op1(input_op, 6, 4);
    HashedOperation op2(input_op, 6, 4);
    NodeOperationHash hash1 = *op1.generate_hash();
    NodeOperationHash hash2 = *op2.generate_hash();
    EXPECT_EQ(hash1, hash2);
    op1.set_param1(-1);
    hash1 = *op1.generate_hash();
    EXPECT_NE(hash1, hash2);

    HashedOperation op3(input_op, 11, 14);
    NodeOperationHash hash3 = *op3.generate_hash();
    EXPECT_NE(hash2, hash3);
    EXPECT_NE(hash1, hash3);

    test_non_equal_hashes_compare(hash1, hash2, hash3);
    test_non_equal_hashes_compare(hash3, hash2, hash1);
    test_non_equal_hashes_compare(hash2, hash3, hash1);
    test_non_equal_hashes_compare(hash3, hash1, hash2);

    NonHashedOperation input_op2(2);
    HashedOperation op4(input_op2, 11, 14);
    NodeOperationHash hash4 = *op4.generate_hash();
    EXPECT_NE(hash3, hash4);

    input_op2.set_id(1);
    hash4 = *op4.generate_hash();
    EXPECT_EQ(hash3, hash4);
  }
}

}  // namespace blender::compositor::tests
