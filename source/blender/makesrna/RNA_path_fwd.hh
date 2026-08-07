/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 *
 * Forward declarations for RNA_path.hh, for use in headers that only need to name
 * #blender::ParsedRNAPathRef (e.g. in a function signature) without needing the full RNA path
 * parsing API.
 */

#pragma once

#include <variant>

namespace blender {

template<typename T> class Span;

namespace rna_path {

struct Member;
struct LookupIndex;
struct LookupKey;

using Item = std::variant<Member, LookupIndex, LookupKey>;

}  // namespace rna_path

using ParsedRNAPathRef = Span<rna_path::Item>;

}  // namespace blender
