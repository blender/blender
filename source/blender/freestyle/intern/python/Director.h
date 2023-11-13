/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

namespace Freestyle {
class UnaryPredicate0D;
class UnaryPredicate1D;
class BinaryPredicate0D;
class BinaryPredicate1D;
class ChainingIterator;
class AdjacencyIterator;
class Interface0D;
class Interface1D;
class Interface0DIterator;
class Stroke;
class StrokeShader;
}  // namespace Freestyle

// BinaryPredicate0D: __call__
int Director_BPy_BinaryPredicate0D___call__(Freestyle::BinaryPredicate0D *bp0D,
                                            Freestyle::Interface0D &i1,
                                            Freestyle::Interface0D &i2);

// BinaryPredicate1D: __call__
int Director_BPy_BinaryPredicate1D___call__(Freestyle::BinaryPredicate1D *bp1D,
                                            Freestyle::Interface1D &i1,
                                            Freestyle::Interface1D &i2);

// UnaryFunction{0D,1D}: __call__
int Director_BPy_UnaryFunction0D___call__(void *uf0D,
                                          void *py_uf0D,
                                          Freestyle::Interface0DIterator &if0D_it);
int Director_BPy_UnaryFunction1D___call__(void *uf1D, void *py_uf1D, Freestyle::Interface1D &if1D);

// UnaryPredicate0D: __call__
int Director_BPy_UnaryPredicate0D___call__(Freestyle::UnaryPredicate0D *up0D,
                                           Freestyle::Interface0DIterator &if0D_it);

// UnaryPredicate1D: __call__
int Director_BPy_UnaryPredicate1D___call__(Freestyle::UnaryPredicate1D *up1D,
                                           Freestyle::Interface1D &if1D);

// StrokeShader: shade
int Director_BPy_StrokeShader_shade(Freestyle::StrokeShader *ss, Freestyle::Stroke &s);

// ChainingIterator: init, traverse
int Director_BPy_ChainingIterator_init(Freestyle::ChainingIterator *c_it);
int Director_BPy_ChainingIterator_traverse(Freestyle::ChainingIterator *c_it,
                                           Freestyle::AdjacencyIterator &a_it);
