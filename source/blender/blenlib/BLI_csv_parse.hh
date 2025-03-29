/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_any.hh"
#include "BLI_function_ref.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_offset_indices.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::csv_parse {

/**
 * Contains the fields of a single record of a .csv file. Usually that corresponds to a single
 * line.
 */
class CsvRecord {
 private:
  Span<Span<char>> fields_;

 public:
  CsvRecord(Span<Span<char>> fields);

  /** Number of fields in the record. */
  int64_t size() const;
  IndexRange index_range() const;

  /** Get the field at the given index. Empty data is returned if the index is too large. */
  Span<char> field(const int64_t index) const;
  StringRef field_str(const int64_t index) const;
};

/**
 * Contains the fields of multiple records.
 */
class CsvRecords {
 private:
  OffsetIndices<int64_t> offsets_;
  Span<Span<char>> fields_;

 public:
  CsvRecords(OffsetIndices<int64_t> offsets, Span<Span<char>> fields);

  /** Number of records (rows). */
  int64_t size() const;
  IndexRange index_range() const;

  /** Get the record at the given index. */
  CsvRecord record(const int64_t index) const;
};

struct CsvParseOptions {
  /** The character that separates fields within a row. */
  char delimiter = ',';
  /**
   * The character that can be used to enclose fields which contain the delimiter or span multiple
   * lines.
   */
  char quote = '"';
  /**
   * Characters that can be used to escape the quote character. By default, "" or \" both represent
   * an escaped quote.
   */
  Span<char> quote_escape_chars = Span<char>(StringRef("\"\\"));
  /** Approximate number of bytes per chunk that the input is split into. */
  int64_t chunk_size_bytes = 64 * 1024;
};

/**
 * Parses a `.csv` file. There are two important aspects to the way this interface is designed:
 * 1. It allows the file to be split into chunks that can be parsed in parallel.
 * 2. Splitting the file into individual records and fields is separated from parsing the actual
 *    content into e.g. floats. This simplifies the implementation of both parts because the
 *    logical parsing does not have to worry about e.g. the delimiter or quote characters. It also
 *    simplifies unit testing.
 *
 * \param buffer: The buffer containing the `.csv` file.
 * \param options: Options that control how the file is parsed.
 * \param process_header: A function that is called at most once and contains the fields of the
 *   first row/record.
 * \param process_records: A function that is called potentially many times in parallel and that
 *   processes a chunk of parsed records. Typically this function parses raw byte fields into e.g.
 *   ints or floats. The result of the parsing process has to be returned. Note that under specific
 *   circumstances, this function may be called twice for the same records. That can happen when
 *   the `.csv` file contains multi-line fields which were split incorrectly at first.
 * \return A vector containing the return values of the `process_records` function in the correct
 *   order. #std::nullopt is returned if the file was malformed, e.g.
 *   if it has a quoted field that is not closed.
 */
std::optional<Vector<Any<>>> parse_csv_in_chunks(
    const Span<char> buffer,
    const CsvParseOptions &options,
    FunctionRef<void(const CsvRecord &record)> process_header,
    FunctionRef<Any<>(const CsvRecords &records)> process_records);

/**
 * Same as above, but uses a templated chunk type instead of using #Any which can be more
 * convenient to use.
 */
template<typename ChunkT>
inline std::optional<Vector<ChunkT>> parse_csv_in_chunks(
    const Span<char> buffer,
    const CsvParseOptions &options,
    FunctionRef<void(const CsvRecord &record)> process_header,
    FunctionRef<ChunkT(const CsvRecords &records)> process_records)
{
  std::optional<Vector<Any<>>> result = parse_csv_in_chunks(
      buffer, options, process_header, [&](const CsvRecords &records) {
        return Any<>(process_records(records));
      });
  if (!result.has_value()) {
    return std::nullopt;
  }
  Vector<ChunkT> result_chunks;
  result_chunks.reserve(result->size());
  for (Any<> &value : *result) {
    result_chunks.append(std::move(value.get<ChunkT>()));
  }
  return result_chunks;
}

/**
 * Fields in a CSV file may contain escaped quote characters (e.g. "" or \").
 * This function replaces these with just the quote character.
 * The returned string may be reference the input string if it's the same.
 * Otherwise the returned string is allocated in the given allocator.
 */
StringRef unescape_field(const StringRef str,
                         const CsvParseOptions &options,
                         LinearAllocator<> &allocator);

/* -------------------------------------------------------------------- */
/** \name #CsvRecord inline functions.
 * \{ */

inline CsvRecord::CsvRecord(Span<Span<char>> fields) : fields_(fields) {}

inline int64_t CsvRecord::size() const
{
  return fields_.size();
}

inline IndexRange CsvRecord::index_range() const
{
  return fields_.index_range();
}

inline Span<char> CsvRecord::field(const int64_t index) const
{
  BLI_assert(index >= 0);
  if (index >= fields_.size()) {
    return {};
  }
  return fields_[index];
}

inline StringRef CsvRecord::field_str(const int64_t index) const
{
  const Span<char> value = this->field(index);
  return StringRef(value.data(), value.size());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #CsvRecords inline functions.
 * \{ */

inline CsvRecords::CsvRecords(const OffsetIndices<int64_t> offsets, const Span<Span<char>> fields)
    : offsets_(offsets), fields_(fields)
{
}

inline int64_t CsvRecords::size() const
{
  return offsets_.size();
}

inline IndexRange CsvRecords::index_range() const
{
  return offsets_.index_range();
}

inline CsvRecord CsvRecords::record(const int64_t index) const
{
  return CsvRecord(fields_.slice(offsets_[index]));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal functions exposed for testing.
 * \{ */

namespace detail {

/**
 * Find the index that ends the current field, i.e. the index of the next delimiter of newline.
 * The start index has to be the index of the first character in the field. It may also be the
 * end of the field already if it is empty.
 *
 * \param start: The index of the first character in the field. This may also be the end of the
 *   field already if it is empty.
 * \param delimiter: The character that ends the field.
 * \return Index of the next delimiter, a newline character or the end of the buffer.
 */
int64_t find_end_of_simple_field(Span<char> buffer, int64_t start, char delimiter);

/**
 * Find the index of the quote that ends the current field.
 *
 * \param start: The index after the opening quote.
 * \param quote: The quote character that ends the field.
 * \param escape_chars: The characters that may be used to escape the quote character.
 * \return Index of the quote character that ends the field, or std::nullopt if the field is
 *   malformed and does not have an end.
 */
std::optional<int64_t> find_end_of_quoted_field(Span<char> buffer,
                                                int64_t start,
                                                char quote,
                                                Span<char> escape_chars);

/**
 * Finds all fields for the record starting at the given index. Typically, the record ends with a
 * newline, but quoted multi-line records are supported as well.
 *
 * \return Index of the start of the next record or the end of the buffer. #std::nullopt is
 * returned if the buffer has a malformed record at the end,
 * i.e. a quoted field that is not closed.
 */
std::optional<int64_t> parse_record_fields(const Span<char> buffer,
                                           const int64_t start,
                                           const char delimiter,
                                           const char quote,
                                           const Span<char> quote_escape_chars,
                                           Vector<Span<char>> &r_fields);

}  // namespace detail

/** \} */

}  // namespace blender::csv_parse
