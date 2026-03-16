/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_function_separate_matrix(const float4x4 matrix,
                                   float &column1_row1,
                                   float &column1_row2,
                                   float &column1_row3,
                                   float &column1_row4,
                                   float &column2_row1,
                                   float &column2_row2,
                                   float &column2_row3,
                                   float &column2_row4,
                                   float &column3_row1,
                                   float &column3_row2,
                                   float &column3_row3,
                                   float &column3_row4,
                                   float &column4_row1,
                                   float &column4_row2,
                                   float &column4_row3,
                                   float &column4_row4)
{
  column1_row1 = matrix[0][0];
  column1_row2 = matrix[0][1];
  column1_row3 = matrix[0][2];
  column1_row4 = matrix[0][3];
  column2_row1 = matrix[1][0];
  column2_row2 = matrix[1][1];
  column2_row3 = matrix[1][2];
  column2_row4 = matrix[1][3];
  column3_row1 = matrix[2][0];
  column3_row2 = matrix[2][1];
  column3_row3 = matrix[2][2];
  column3_row4 = matrix[2][3];
  column4_row1 = matrix[3][0];
  column4_row2 = matrix[3][1];
  column4_row3 = matrix[3][2];
  column4_row4 = matrix[3][3];
}
