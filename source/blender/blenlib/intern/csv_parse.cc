/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_csv_parse.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_task.hh"

#include <atomic>

namespace blender::csv_parse {

/**
 * Returns a guess for the start of the next record. Note that this could split up quoted fields.
 * This case needs to be detected at a higher level.
 */
static int64_t guess_next_record_start(const Span<char> buffer, const int64_t start)
{
  int64_t i = start;
  while (i < buffer.size()) {
    const char c = buffer[i];
    if (c == '\n') {
      return i + 1;
    }
    i++;
  }
  return buffer.size();
}

/**
 * Split the buffer into chunks of approximately the given size. The function attempts to align the
 * chunks so that records are not split. This works in the majority of cases, but can fail with
 * multi-line fields. This has to be detected at a higher level.
 */
static Vector<Span<char>> split_into_aligned_chunks(const Span<char> buffer,
                                                    int64_t approximate_chunk_size)
{
  approximate_chunk_size = std::max<int64_t>(approximate_chunk_size, 1);
  Vector<Span<char>> chunks;
  int64_t start = 0;
  while (start < buffer.size()) {
    int64_t end = std::min(start + approximate_chunk_size, buffer.size());
    end = guess_next_record_start(buffer, end);
    chunks.append(buffer.slice(IndexRange::from_begin_end(start, end)));
    start = end;
  }
  return chunks;
}

/**
 * Parses the given buffer into records and their fields.
 *
 * r_data_offsets and r_data_fields are passed into to be able to reuse their memory.
 */
static std::optional<CsvRecords> parse_records(const Span<char> buffer,
                                               const CsvParseOptions &options,
                                               Vector<int64_t> &r_data_offsets,
                                               Vector<Span<char>> &r_data_fields)
{
  using namespace detail;
  /* Clear the data that may still be in there, but do not free the memory. */
  r_data_offsets.clear();
  r_data_fields.clear();

  r_data_offsets.append(0);
  int64_t start = 0;
  while (start < buffer.size()) {
    const std::optional<int64_t> next_record_start = parse_record_fields(
        buffer,
        start,
        options.delimiter,
        options.quote,
        options.quote_escape_chars,
        r_data_fields);
    if (!next_record_start.has_value()) {
      return std::nullopt;
    }
    /* Ignore empty lines. While those are not great practice, they can occur in practice. */
    if (r_data_fields.size() > r_data_offsets.last()) {
      r_data_offsets.append(r_data_fields.size());
    }
    start = *next_record_start;
  }
  return CsvRecords(OffsetIndices<int64_t>(r_data_offsets), r_data_fields);
}

std::optional<Vector<Any<>>> parse_csv_in_chunks(
    const Span<char> buffer,
    const CsvParseOptions &options,
    FunctionRef<void(const CsvRecord &record)> process_header,
    FunctionRef<Any<>(const CsvRecords &records)> process_records)
{
  using namespace detail;

  /* First parse the first row to get the column names. */
  Vector<Span<char>> header_fields;
  const std::optional<int64_t> first_data_record_start = parse_record_fields(
      buffer, 0, options.delimiter, options.quote, options.quote_escape_chars, header_fields);
  if (!first_data_record_start.has_value()) {
    return std::nullopt;
  }
  /* Call this before starting to process the remaining data. This allows the caller to do some
   * preprocessing that is used during chunk parsing. */
  process_header(CsvRecord(header_fields));

  /* This buffer contains only the data records, without the header. */
  const Span<char> data_buffer = buffer.drop_front(*first_data_record_start);
  /* Split the buffer into chunks that can be processed in parallel. */
  const Vector<Span<char>> data_buffer_chunks = split_into_aligned_chunks(
      data_buffer, options.chunk_size_bytes);

  /* It's not common, but it can happen that .csv files contain quoted multi-line values. In the
   * unlucky case that we split the buffer in the middle of such a multi-line field, there will be
   * malformed chunks. In this case we fallback to parsing the whole buffer with a single thread.
   * If this case becomes more common, we could try to avoid splitting into malformed chunks by
   * making the splitting logic a bit smarter. */
  std::atomic<bool> found_malformed_chunk = false;
  Vector<std::optional<Any<>>> chunk_results(data_buffer_chunks.size());
  struct TLS {
    Vector<int64_t> data_offsets;
    Vector<Span<char>> data_fields;
  };
  threading::EnumerableThreadSpecific<TLS> all_tls;
  threading::parallel_for(chunk_results.index_range(), 1, [&](const IndexRange range) {
    TLS &tls = all_tls.local();
    for (const int64_t i : range) {
      if (found_malformed_chunk.load(std::memory_order_relaxed)) {
        /* All work is cancelled when there was a malformed chunk. */
        return;
      }
      const Span<char> chunk_buffer = data_buffer_chunks[i];
      const std::optional<CsvRecords> records = parse_records(
          chunk_buffer, options, tls.data_offsets, tls.data_fields);
      if (!records.has_value()) {
        found_malformed_chunk.store(true, std::memory_order_relaxed);
        return;
      }
      chunk_results[i] = process_records(*records);
    }
  });

  /* If there was a malformed chunk, process the data again in a single thread without splitting
   * the input into chunks. This should happen quite rarely but is important for overall
   * correctness. */
  if (found_malformed_chunk) {
    chunk_results.clear();
    TLS &tls = all_tls.local();
    const std::optional<CsvRecords> records = parse_records(
        data_buffer, options, tls.data_offsets, tls.data_fields);
    if (!records.has_value()) {
      return std::nullopt;
    }
    chunk_results.append(process_records(*records));
  }

  /* Prepare the return value. */
  Vector<Any<>> results;
  for (std::optional<Any<>> &result : chunk_results) {
    BLI_assert(result.has_value());
    results.append(std::move(result.value()));
  }
  return results;
}

StringRef unescape_field(const StringRef str,
                         const CsvParseOptions &options,
                         LinearAllocator<> &allocator)
{
  const StringRef escape_chars{options.quote_escape_chars};
  if (str.find_first_of(escape_chars) == StringRef::not_found) {
    return str;
  }
  /* The actual unescaped string may be shorter, but not longer. */
  MutableSpan<char> unescaped_str = allocator.allocate_array<char>(str.size());
  int64_t i = 0;
  int64_t escaped_size = 0;
  while (i < str.size()) {
    const char c = str[i];
    if (options.quote_escape_chars.contains(c)) {
      if (i + 1 < str.size() && str[i + 1] == options.quote) {
        /* Ignore the current escape character. */
        unescaped_str[escaped_size++] = options.quote;
        i += 2;
        continue;
      }
    }
    unescaped_str[escaped_size++] = c;
    i++;
  }
  return StringRef(unescaped_str.take_front(escaped_size));
}

namespace detail {

std::optional<int64_t> parse_record_fields(const Span<char> buffer,
                                           const int64_t start,
                                           const char delimiter,
                                           const char quote,
                                           const Span<char> quote_escape_chars,
                                           Vector<Span<char>> &r_fields)
{
  using namespace detail;

  const auto handle_potentially_trailing_delimiter = [&](const int64_t i) {
    if (i <= buffer.size()) {
      if (i < buffer.size()) {
        if (ELEM(buffer[i], '\n', '\r')) {
          r_fields.append({});
        }
      }
      else {
        r_fields.append({});
      }
    }
  };

  int64_t i = start;
  while (i < buffer.size()) {
    const char c = buffer[i];
    if (c == '\n') {
      return i + 1;
    }
    if (c == '\r') {
      i++;
      continue;
    }
    if (c == delimiter) {
      r_fields.append({});
      i++;
      handle_potentially_trailing_delimiter(i);
      continue;
    }
    if (c == quote) {
      i++;
      const std::optional<int64_t> end_of_field = find_end_of_quoted_field(
          buffer, i, quote, quote_escape_chars);
      if (!end_of_field.has_value()) {
        return std::nullopt;
      }
      r_fields.append(buffer.slice(IndexRange::from_begin_end(i, *end_of_field)));
      i = *end_of_field;
      while (i < buffer.size()) {
        const char inner_c = buffer[i];
        if (inner_c == quote) {
          i++;
          continue;
        }
        if (inner_c == delimiter) {
          i++;
          handle_potentially_trailing_delimiter(i);
          break;
        }
        if (ELEM(inner_c, '\n', '\r')) {
          break;
        }
        i++;
      }
      continue;
    }
    const int64_t end_of_field = find_end_of_simple_field(buffer, i, delimiter);
    r_fields.append(buffer.slice(IndexRange::from_begin_end(i, end_of_field)));
    i = end_of_field;
    while (i < buffer.size()) {
      const char inner_c = buffer[i];
      if (inner_c == delimiter) {
        i++;
        handle_potentially_trailing_delimiter(i);
        break;
      }
      if (ELEM(inner_c, '\n', '\r')) {
        break;
      }
      BLI_assert_unreachable();
    }
  }

  return buffer.size();
}

int64_t find_end_of_simple_field(const Span<char> buffer,
                                 const int64_t start,
                                 const char delimiter)
{
  int64_t i = start;
  while (i < buffer.size()) {
    const char c = buffer[i];
    if (ELEM(c, delimiter, '\n', '\r')) {
      return i;
    }
    i++;
  }
  return buffer.size();
}

std::optional<int64_t> find_end_of_quoted_field(const Span<char> buffer,
                                                const int64_t start,
                                                const char quote,
                                                const Span<char> escape_chars)
{
  int64_t i = start;
  while (i < buffer.size()) {
    const char c = buffer[i];
    if (escape_chars.contains(c)) {
      if (i + 1 < buffer.size() && buffer[i + 1] == quote) {
        i += 2;
        continue;
      }
    }
    if (c == quote) {
      return i;
    }
    i++;
  }
  return std::nullopt;
}

}  // namespace detail

}  // namespace blender::csv_parse
