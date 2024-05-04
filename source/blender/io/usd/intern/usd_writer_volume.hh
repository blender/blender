/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <optional>

#include "usd_writer_abstract.hh"

struct Volume;

namespace blender::io::usd {

/* Writer for writing OpenVDB assets to UsdVolVolume. Volume data is stored in separate `.vdb`
 * files which are referenced in USD file. */
class USDVolumeWriter : public USDAbstractWriter {
 public:
  USDVolumeWriter(const USDExporterContext &ctx);

 protected:
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  virtual void do_write(HierarchyContext &context) override;

 private:
  /* Try to ensure that external `.vdb` file is available for USD to be referenced. Blender can
   * either reference external OpenVDB data or generate such data internally. Latter option will
   * mean that `resolve_vdb_file` method will try to export volume data to a new `.vdb` file.
   * If successful, this method returns absolute file path to the resolved `.vdb` file, if not,
   * returns `std::nullopt`. */
  std::optional<std::string> resolve_vdb_file(const Volume *volume) const;

  std::optional<std::string> construct_vdb_file_path(const Volume *volume) const;
  std::optional<std::string> construct_vdb_relative_file_path(
      const std::string &vdb_file_path) const;
};

}  // namespace blender::io::usd
