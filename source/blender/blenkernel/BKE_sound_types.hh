/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include <memory>

#if defined(WITH_AUDASPACE)

namespace aud {
class IDevice;
class IHandle;
class ISound;
class Sequence;
class SequenceEntry;
}  // namespace aud

typedef std::shared_ptr<aud::IDevice> AUD_Device;
typedef std::shared_ptr<aud::IHandle> AUD_Handle;
typedef std::shared_ptr<aud::Sequence> AUD_Sequence;
typedef std::shared_ptr<aud::ISound> AUD_Sound;
typedef std::shared_ptr<aud::SequenceEntry> AUD_SequenceEntry;

namespace aud {
struct DeviceSpecs;
}
#else
typedef std::shared_ptr<void> AUD_Device;
typedef std::shared_ptr<void> AUD_Handle;
typedef std::shared_ptr<void> AUD_Sequence;
typedef std::shared_ptr<void> AUD_Sound;
typedef std::shared_ptr<void> AUD_SequenceEntry;
#endif

namespace blender::bke {

struct NlaStripRuntime {
  AUD_SequenceEntry speaker_handle;
};

}  // namespace blender::bke
