/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Computes the number of diagonals in the matrix of the given size, where the diagonals are
 * indexed from the upper left corner to the lower right corner such that their start is at the
 * left and bottom edges of the matrix as shown in the diagram below. The numbers in the diagram
 * denote the index of the diagonal. The number of diagonals is then intuitively the number of
 * values on the left and bottom edges, which is equal to:
 *
 *   Number Of Diagonals => width + height - 1
 *
 * Notice that the minus one is due to the shared value in the corner.
 *
 *         Width = 6
 * +---+---+---+---+---+---+
 * | 0 | 1 | 2 | 3 | 4 | 5 |
 * +---+---+---+---+---+---+
 * | 1 | 2 | 3 | 4 | 5 | 6 |  Height = 3
 * +---+---+---+---+---+---+
 * | 2 | 3 | 4 | 5 | 6 | 7 |
 * +---+---+---+---+---+---+
 */
int compute_number_of_diagonals(ivec2 size)
{
  return size.x + size.y - 1;
}

/* Computes the number of values in the diagonal of the given index in the matrix with the given
 * size, where the diagonals are indexed from the upper left corner to the lower right corner such
 * that their start is at the left and bottom edges of the matrix as shown in the diagram below.
 * The numbers in the diagram denote the index of the diagonal and its length.
 *
 *             Width = 6
 *     +---+---+---+---+---+---+
 *  1  | 0 | 1 | 2 | 3 | 4 | 5 |
 *     +---+---+---+---+---+---+
 *  2  | 1 | 2 | 3 | 4 | 5 | 6 |  Height = 3
 *     +---+---+---+---+---+---+
 *     | 2 | 3 | 4 | 5 | 6 | 7 |
 *     +---+---+---+---+---+---+
 *  3        3   3   3   2   1
 *
 * To derive the length of the diagonal from the index, we note that the lengths of the diagonals
 * start at 1 and linearly increase up to the length of the longest diagonal, then remain constant
 * until it linearly decrease to 1 at the end. The length of the longest diagonal is intuitively
 * the smaller of the width and height of the matrix. The linearly increasing and constant parts of
 * the sequence can be described using the following compact equation:
 *
 *   Length => min(Longest Length, index + 1)
 *
 * While the constant and deceasing end parts of the sequence can be described using the following
 * compact equation:
 *
 *   Length => min(Longest Length, Number Of Diagonals - index)
 *
 * All three parts of the sequence can then be combined using the minimum operation because they
 * all share the same maximum value, that is, the longest length:
 *
 *   Length => min(Longest Length, index + 1, Number Of Diagonals - index)
 */
int compute_diagonal_length(ivec2 size, int diagonal_index)
{
  int length_of_longest_diagonal = min(size.x, size.y);
  int start_sequence = diagonal_index + 1;
  int end_sequence = compute_number_of_diagonals(size) - diagonal_index;
  return min(length_of_longest_diagonal, min(start_sequence, end_sequence));
}

/* Computes the position of the start of the diagonal of the given index in the matrix with the
 * given size, where the diagonals are indexed from the upper left corner to the lower right corner
 * such that their start is at the left and bottom edges of the matrix as shown in the diagram
 * below. The numbers in the diagram denote the index of the diagonal and the position of its
 * start.
 *
 *                      Width = 6
 *         +-----+-----+-----+-----+-----+-----+
 *  (0, 2) |  0  |  1  |  2  |  3  |  4  |  5  |
 *         +-----+-----+-----+-----+-----+-----+
 *  (0, 1) |  1  |  2  |  3  |  4  |  5  |  6  |  Height = 3
 *         +-----+-----+-----+-----+-----+-----+
 *         |  2  |  3  |  4  |  5  |  6  |  7  |
 *         +-----+-----+-----+-----+-----+-----+
 *  (0, 0)        (1,0) (2,0) (3,0) (4,0) (5,0)
 *
 * To derive the start position from the index, we consider each axis separately. For the X
 * position, indices up to (height - 1) have zero x positions, while other indices linearly
 * increase from (height) to the end. Which can be described using the compact equation:
 *
 *   X => max(0, index - (height - 1))
 *
 * For the Y position, indices up to (height - 1) linearly decrease from (height - 1) to zero,
 * while other indices are zero. Which can be described using the compact equation:
 *
 *   Y => max(0, (height - 1) - index)
 */
ivec2 compute_diagonal_start(ivec2 size, int index)
{
  return ivec2(max(0, index - (size.y - 1)), max(0, (size.y - 1) - index));
}

/* Computes a direction vector such that when added to the position of a value in a matrix will
 * yield the position of the next value in the same diagonal. According to the choice of the start
 * of the diagonal in compute_diagonal_start, this is (1, 1). */
ivec2 get_diagonal_direction()
{
  return ivec2(1);
}

/* Computes the number of values in the anti diagonal of the given index in the matrix with the
 * given size, where the anti diagonals are indexed from the lower left corner to the upper right
 * corner such that that their start is at the bottom and right edges of the matrix as shown in the
 * diagram below. The numbers in the diagram denote the index of the anti diagonal and its length.
 *
 *                     Width = 6
 *             +---+---+---+---+---+---+
 *             | 2 | 3 | 4 | 5 | 6 | 7 |  1
 *             +---+---+---+---+---+---+
 *  Height = 3 | 1 | 2 | 3 | 4 | 5 | 6 |  2
 *             +---+---+---+---+---+---+
 *             | 0 | 1 | 2 | 3 | 4 | 5 |
 *             +---+---+---+---+---+---+
 *               1   2   3   3   3        3
 *
 * The length of the anti diagonal is identical to the length of the diagonal of the same index, as
 * can be seen by comparing the above diagram with the one in the compute_diagonal_length function,
 * since the anti diagonals are merely flipped diagonals. */
int compute_anti_diagonal_length(ivec2 size, int diagonal_index)
{
  return compute_diagonal_length(size, diagonal_index);
}

/* Computes the position of the start of the anti diagonal of the given index in the matrix with
 * the given size, where the anti diagonals are indexed from the lower left corner to the upper
 * right corner such that their start is at the bottom and right edges of the matrix as shown in
 * the diagram below. The numbers in the diagram denote the index of the anti diagonal and the
 * position of its start.
 *
 *                           Width = 6
 *              +-----+-----+-----+-----+-----+-----+
 *              |  2  |  3  |  4  |  5  |  6  |  7  |  (5,2)
 *              +-----+-----+-----+-----+-----+-----+
 *  Height = 3  |  1  |  2  |  3  |  4  |  5  |  6  |  (5,1)
 *              +-----+-----+-----+-----+-----+-----+
 *              |  0  |  1  |  2  |  3  |  4  |  5  |
 *              +-----+-----+-----+-----+-----+-----+
 *               (0,0) (1,0) (2,0) (3,0) (4,0)         (5,0)
 *
 * To derive the start position from the index, we consider each axis separately. For the X
 * position, indices up to (width - 1) linearly increase from zero, while other indices are all
 * (width - 1). Which can be described using the compact equation:
 *
 *   X => min((width - 1), index)
 *
 * For the Y position, indices up to (width - 1) are zero, while other indices linearly increase
 * from zero to (height - 1). Which can be described using the compact equation:
 *
 *   Y => max(0, index - (width - 1))
 */
ivec2 compute_anti_diagonal_start(ivec2 size, int index)
{
  return ivec2(min(size.x - 1, index), max(0, index - (size.x - 1)));
}

/* Computes a direction vector such that when added to the position of a value in a matrix will
 * yield the position of the next value in the same anti diagonal. According to the choice of the
 * start of the anti diagonal in compute_anti_diagonal_start, this is (-1, 1). */
ivec2 get_anti_diagonal_direction()
{
  return ivec2(-1, 1);
}
