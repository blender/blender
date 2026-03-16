/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_function_combine_matrix(const float column1_row1,
                                  const float column1_row2,
                                  const float column1_row3,
                                  const float column1_row4,
                                  const float column2_row1,
                                  const float column2_row2,
                                  const float column2_row3,
                                  const float column2_row4,
                                  const float column3_row1,
                                  const float column3_row2,
                                  const float column3_row3,
                                  const float column3_row4,
                                  const float column4_row1,
                                  const float column4_row2,
                                  const float column4_row3,
                                  const float column4_row4,
                                  float4x4 &matrix)
{
  matrix = float4x4(column1_row1,
                    column1_row2,
                    column1_row3,
                    column1_row4,
                    column2_row1,
                    column2_row2,
                    column2_row3,
                    column2_row4,
                    column3_row1,
                    column3_row2,
                    column3_row3,
                    column3_row4,
                    column4_row1,
                    column4_row2,
                    column4_row3,
                    column4_row4);
}
