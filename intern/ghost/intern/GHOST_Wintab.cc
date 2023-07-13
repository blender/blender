/* SPDX-FileCopyrightText: 2021-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#define _USE_MATH_DEFINES

#include "GHOST_Wintab.hh"

GHOST_Wintab *GHOST_Wintab::loadWintab(HWND hwnd)
{
  /* Load Wintab library if available. */
  auto handle = unique_hmodule(::LoadLibrary("Wintab32.dll"), &::FreeLibrary);
  if (!handle) {
    return nullptr;
  }

  /* Get Wintab functions. */

  auto info = (GHOST_WIN32_WTInfo)::GetProcAddress(handle.get(), "WTInfoA");
  if (!info) {
    return nullptr;
  }

  auto open = (GHOST_WIN32_WTOpen)::GetProcAddress(handle.get(), "WTOpenA");
  if (!open) {
    return nullptr;
  }

  auto get = (GHOST_WIN32_WTGet)::GetProcAddress(handle.get(), "WTGetA");
  if (!get) {
    return nullptr;
  }

  auto set = (GHOST_WIN32_WTSet)::GetProcAddress(handle.get(), "WTSetA");
  if (!set) {
    return nullptr;
  }

  auto close = (GHOST_WIN32_WTClose)::GetProcAddress(handle.get(), "WTClose");
  if (!close) {
    return nullptr;
  }

  auto packetsGet = (GHOST_WIN32_WTPacketsGet)::GetProcAddress(handle.get(), "WTPacketsGet");
  if (!packetsGet) {
    return nullptr;
  }

  auto queueSizeGet = (GHOST_WIN32_WTQueueSizeGet)::GetProcAddress(handle.get(), "WTQueueSizeGet");
  if (!queueSizeGet) {
    return nullptr;
  }

  auto queueSizeSet = (GHOST_WIN32_WTQueueSizeSet)::GetProcAddress(handle.get(), "WTQueueSizeSet");
  if (!queueSizeSet) {
    return nullptr;
  }

  auto enable = (GHOST_WIN32_WTEnable)::GetProcAddress(handle.get(), "WTEnable");
  if (!enable) {
    return nullptr;
  }

  auto overlap = (GHOST_WIN32_WTOverlap)::GetProcAddress(handle.get(), "WTOverlap");
  if (!overlap) {
    return nullptr;
  }

  /* Build Wintab context. */

  LOGCONTEXT lc = {0};
  if (!info(WTI_DEFSYSCTX, 0, &lc)) {
    return nullptr;
  }

  Coord tablet, system;
  extractCoordinates(lc, tablet, system);
  modifyContext(lc);

  /* The Wintab spec says we must open the context disabled if we are using cursor masks. */
  auto hctx = unique_hctx(open(hwnd, &lc, FALSE), close);
  if (!hctx) {
    return nullptr;
  }

  /* Wintab provides no way to determine the maximum queue size aside from checking if attempts
   * to change the queue size are successful. */
  const int maxQueue = 500;
  /* < 0 should realistically never happen, but given we cast to size_t later on better safe than
   * sorry. */
  int queueSize = max(0, queueSizeGet(hctx.get()));

  while (queueSize < maxQueue) {
    int testSize = min(queueSize + 16, maxQueue);
    if (queueSizeSet(hctx.get(), testSize)) {
      queueSize = testSize;
    }
    else {
      /* From Windows Wintab Documentation for WTQueueSizeSet:
       * "If the return value is zero, the context has no queue because the function deletes the
       * original queue before attempting to create a new one. The application must continue
       * calling the function with a smaller queue size until the function returns a non - zero
       * value."
       *
       * In our case we start with a known valid queue size and in the event of failure roll
       * back to the last valid queue size. The Wintab spec dates back to 16 bit Windows, thus
       * assumes memory recently deallocated may not be available, which is no longer a practical
       * concern. */
      if (!queueSizeSet(hctx.get(), queueSize)) {
        /* If a previously valid queue size is no longer valid, there is likely something wrong in
         * the Wintab implementation and we should not use it. */
        return nullptr;
      }
      break;
    }
  }

  int sanityQueueSize = queueSizeGet(hctx.get());
  WINTAB_PRINTF("HCTX %p %s queueSize: %d, queueSizeGet: %d\n",
                hctx.get(),
                __func__,
                queueSize,
                sanityQueueSize);

  WINTAB_PRINTF("Loaded Wintab context %p\n", hctx.get());

  return new GHOST_Wintab(std::move(handle),
                          info,
                          get,
                          set,
                          packetsGet,
                          enable,
                          overlap,
                          std::move(hctx),
                          tablet,
                          system,
                          size_t(queueSize));
}

void GHOST_Wintab::modifyContext(LOGCONTEXT &lc)
{
  lc.lcPktData = PACKETDATA;
  lc.lcPktMode = PACKETMODE;
  lc.lcMoveMask = PACKETDATA;
  lc.lcOptions |= CXO_CSRMESSAGES | CXO_MESSAGES;

  /* Tablet scaling is handled manually because some drivers don't handle HIDPI or multi-display
   * correctly; reset tablet scale factors to un-scaled tablet coordinates. */
  lc.lcOutOrgX = lc.lcInOrgX;
  lc.lcOutOrgY = lc.lcInOrgY;
  lc.lcOutExtX = lc.lcInExtX;
  lc.lcOutExtY = lc.lcInExtY;
}

void GHOST_Wintab::extractCoordinates(LOGCONTEXT &lc, Coord &tablet, Coord &system)
{
  tablet.x.org = lc.lcInOrgX;
  tablet.x.ext = lc.lcInExtX;
  tablet.y.org = lc.lcInOrgY;
  tablet.y.ext = lc.lcInExtY;

  system.x.org = lc.lcSysOrgX;
  system.x.ext = lc.lcSysExtX;
  system.y.org = lc.lcSysOrgY;
  /* Wintab maps y origin to the tablet's bottom; invert y to match Windows y origin mapping to the
   * screen top. */
  system.y.ext = -lc.lcSysExtY;
}

GHOST_Wintab::GHOST_Wintab(unique_hmodule handle,
                           GHOST_WIN32_WTInfo info,
                           GHOST_WIN32_WTGet get,
                           GHOST_WIN32_WTSet set,
                           GHOST_WIN32_WTPacketsGet packetsGet,
                           GHOST_WIN32_WTEnable enable,
                           GHOST_WIN32_WTOverlap overlap,
                           unique_hctx hctx,
                           Coord tablet,
                           Coord system,
                           size_t queueSize)
    : m_handle{std::move(handle)},
      m_fpInfo{info},
      m_fpGet{get},
      m_fpSet{set},
      m_fpPacketsGet{packetsGet},
      m_fpEnable{enable},
      m_fpOverlap{overlap},
      m_context{std::move(hctx)},
      m_tabletCoord{tablet},
      m_systemCoord{system},
      m_pkts{queueSize}
{
  m_fpInfo(WTI_INTERFACE, IFC_NDEVICES, &m_numDevices);
  WINTAB_PRINTF("Wintab Devices: %d\n", m_numDevices);

  updateCursorInfo();

  /* Debug info. */
  printContextDebugInfo();
}

GHOST_Wintab::~GHOST_Wintab()
{
  WINTAB_PRINTF("Closing Wintab context %p\n", m_context.get());
}

void GHOST_Wintab::enable()
{
  m_fpEnable(m_context.get(), true);
  m_enabled = true;
}

void GHOST_Wintab::disable()
{
  if (m_focused) {
    loseFocus();
  }
  m_fpEnable(m_context.get(), false);
  m_enabled = false;
}

void GHOST_Wintab::gainFocus()
{
  m_fpOverlap(m_context.get(), true);
  m_focused = true;
}

void GHOST_Wintab::loseFocus()
{
  if (m_lastTabletData.Active != GHOST_kTabletModeNone) {
    leaveRange();
  }

  /* Mouse mode of tablet or display layout may change when Wintab or Window is inactive. Don't
   * trust for mouse movement until re-verified. */
  m_coordTrusted = false;

  m_fpOverlap(m_context.get(), false);
  m_focused = false;
}

void GHOST_Wintab::leaveRange()
{
  /* Button state can't be tracked while out of range, reset it. */
  m_buttons = 0;
  /* Set to none to indicate tablet is inactive. */
  m_lastTabletData = GHOST_TABLET_DATA_NONE;
  /* Clear the packet queue. */
  m_fpPacketsGet(m_context.get(), m_pkts.size(), m_pkts.data());
}

void GHOST_Wintab::remapCoordinates()
{
  LOGCONTEXT lc = {0};

  if (m_fpInfo(WTI_DEFSYSCTX, 0, &lc)) {
    extractCoordinates(lc, m_tabletCoord, m_systemCoord);
    modifyContext(lc);

    m_fpSet(m_context.get(), &lc);
  }
}

void GHOST_Wintab::updateCursorInfo()
{
  AXIS Pressure, Orientation[3];

  BOOL pressureSupport = m_fpInfo(WTI_DEVICES, DVC_NPRESSURE, &Pressure);
  m_maxPressure = pressureSupport ? Pressure.axMax : 0;
  WINTAB_PRINTF("HCTX %p %s maxPressure: %d\n", m_context.get(), __func__, m_maxPressure);

  BOOL tiltSupport = m_fpInfo(WTI_DEVICES, DVC_ORIENTATION, &Orientation);
  /* Check if tablet supports azimuth [0] and altitude [1], encoded in axResolution. */
  if (tiltSupport && Orientation[0].axResolution && Orientation[1].axResolution) {
    m_maxAzimuth = Orientation[0].axMax;
    m_maxAltitude = Orientation[1].axMax;
  }
  else {
    m_maxAzimuth = m_maxAltitude = 0;
  }
  WINTAB_PRINTF("HCTX %p %s maxAzimuth: %d, maxAltitude: %d\n",
                m_context.get(),
                __func__,
                m_maxAzimuth,
                m_maxAltitude);
}

void GHOST_Wintab::processInfoChange(LPARAM lParam)
{
  /* Update number of connected Wintab digitizers. */
  if (LOWORD(lParam) == WTI_INTERFACE && HIWORD(lParam) == IFC_NDEVICES) {
    m_fpInfo(WTI_INTERFACE, IFC_NDEVICES, &m_numDevices);
    WINTAB_PRINTF("HCTX %p %s numDevices: %d\n", m_context.get(), __func__, m_numDevices);
  }
}

bool GHOST_Wintab::devicesPresent()
{
  return m_numDevices > 0;
}

GHOST_TabletData GHOST_Wintab::getLastTabletData()
{
  return m_lastTabletData;
}

void GHOST_Wintab::getInput(std::vector<GHOST_WintabInfoWin32> &outWintabInfo)
{
  const int numPackets = m_fpPacketsGet(m_context.get(), m_pkts.size(), m_pkts.data());
  outWintabInfo.reserve(numPackets);

  for (int i = 0; i < numPackets; i++) {
    const PACKET pkt = m_pkts[i];
    GHOST_WintabInfoWin32 out;

    /* % 3 for multiple devices ("DualTrack"). */
    switch (pkt.pkCursor % 3) {
      case 0:
        /* Puck - processed as mouse. */
        out.tabletData.Active = GHOST_kTabletModeNone;
        break;
      case 1:
        out.tabletData.Active = GHOST_kTabletModeStylus;
        break;
      case 2:
        out.tabletData.Active = GHOST_kTabletModeEraser;
        break;
    }

    out.x = pkt.pkX;
    out.y = pkt.pkY;

    if (m_maxPressure > 0) {
      out.tabletData.Pressure = float(pkt.pkNormalPressure) / float(m_maxPressure);
    }

    if ((m_maxAzimuth > 0) && (m_maxAltitude > 0)) {
      /* From the wintab spec:
       * orAzimuth: Specifies the clockwise rotation of the cursor about the z axis through a
       * full circular range.
       * orAltitude: Specifies the angle with the x-y plane through a signed, semicircular range.
       * Positive values specify an angle upward toward the positive z axis; negative values
       * specify an angle downward toward the negative z axis.
       *
       * wintab.h defines orAltitude as a `uint` but documents orAltitude as positive for upward
       * angles and negative for downward angles. WACOM uses negative altitude values to show that
       * the pen is inverted; therefore we cast orAltitude as an (int) and then use the absolute
       * value.
       */

      ORIENTATION ort = pkt.pkOrientation;

      /* Convert raw fixed point data to radians. */
      float altRad = float((fabs(float(ort.orAltitude)) / float(m_maxAltitude)) * M_PI_2);
      float azmRad = float((float(ort.orAzimuth) / float(m_maxAzimuth)) * M_PI * 2.0);

      /* Find length of the stylus' projected vector on the XY plane. */
      float vecLen = cos(altRad);

      /* From there calculate X and Y components based on azimuth. */
      out.tabletData.Xtilt = sin(azmRad) * vecLen;
      out.tabletData.Ytilt = float(sin(M_PI_2 - azmRad) * vecLen);
    }

    out.time = pkt.pkTime;

    /* Some Wintab libraries don't handle relative button input, so we track button presses
     * manually. */
    DWORD buttonsChanged = m_buttons ^ pkt.pkButtons;
    /* We only needed the prior button state to compare to current, so we can overwrite it now. */
    m_buttons = pkt.pkButtons;

    /* Iterate over button flag indices until all flags are clear. */
    for (WORD buttonIndex = 0; buttonsChanged; buttonIndex++, buttonsChanged >>= 1) {
      if (buttonsChanged & 1) {
        GHOST_TButton button = mapWintabToGhostButton(pkt.pkCursor, buttonIndex);

        if (button != GHOST_kButtonMaskNone) {
          /* If this is not the first button found, push info for the prior Wintab button. */
          if (out.button != GHOST_kButtonMaskNone) {
            outWintabInfo.push_back(out);
          }

          out.button = button;

          DWORD buttonFlag = 1 << buttonIndex;
          out.type = pkt.pkButtons & buttonFlag ? GHOST_kEventButtonDown : GHOST_kEventButtonUp;
        }
      }
    }

    outWintabInfo.push_back(out);
  }

  if (!outWintabInfo.empty()) {
    m_lastTabletData = outWintabInfo.back().tabletData;
  }
}

GHOST_TButton GHOST_Wintab::mapWintabToGhostButton(uint cursor, WORD physicalButton)
{
  const WORD numButtons = 32;
  BYTE logicalButtons[numButtons] = {0};
  BYTE systemButtons[numButtons] = {0};

  if (!m_fpInfo(WTI_CURSORS + cursor, CSR_BUTTONMAP, &logicalButtons) ||
      !m_fpInfo(WTI_CURSORS + cursor, CSR_SYSBTNMAP, &systemButtons))
  {
    return GHOST_kButtonMaskNone;
  }

  if (physicalButton >= numButtons) {
    return GHOST_kButtonMaskNone;
  }

  BYTE lb = logicalButtons[physicalButton];

  if (lb >= numButtons) {
    return GHOST_kButtonMaskNone;
  }

  switch (systemButtons[lb]) {
    case SBN_LCLICK:
      return GHOST_kButtonMaskLeft;
    case SBN_RCLICK:
      return GHOST_kButtonMaskRight;
    case SBN_MCLICK:
      return GHOST_kButtonMaskMiddle;
    default:
      return GHOST_kButtonMaskNone;
  }
}

void GHOST_Wintab::mapWintabToSysCoordinates(int x_in, int y_in, int &x_out, int &y_out)
{
  /* Maps from range [in.org, in.org + abs(in.ext)] to [out.org, out.org + abs(out.ext)], in
   * reverse if in.ext and out.ext have differing sign. */
  auto remap = [](int inPoint, Range in, Range out) -> int {
    int absInExt = abs(in.ext);
    int absOutExt = abs(out.ext);

    /* Translate input from range [in.org, in.org + absInExt] to [0, absInExt] */
    int inMagnitude = inPoint - in.org;

    /* If signs of extents differ, reverse input over range. */
    if ((in.ext < 0) != (out.ext < 0)) {
      inMagnitude = absInExt - inMagnitude;
    }

    /* Scale from [0, absInExt] to [0, absOutExt]. */
    int outMagnitude = inMagnitude * absOutExt / absInExt;

    /* Translate from range [0, absOutExt] to [out.org, out.org + absOutExt]. */
    int outPoint = outMagnitude + out.org;

    return outPoint;
  };

  x_out = remap(x_in, m_tabletCoord.x, m_systemCoord.x);
  y_out = remap(y_in, m_tabletCoord.y, m_systemCoord.y);
}

bool GHOST_Wintab::trustCoordinates()
{
  return m_coordTrusted;
}

bool GHOST_Wintab::testCoordinates(int sysX, int sysY, int wtX, int wtY)
{
  mapWintabToSysCoordinates(wtX, wtY, wtX, wtY);

  /* Allow off by one pixel tolerance in case of rounding error. */
  if (abs(sysX - wtX) <= 1 && abs(sysY - wtY) <= 1) {
    m_coordTrusted = true;
    return true;
  }
  else {
    m_coordTrusted = false;
    return false;
  }
}

bool GHOST_Wintab::m_debug = false;

void GHOST_Wintab::setDebug(bool debug)
{
  m_debug = debug;
}

bool GHOST_Wintab::getDebug()
{
  return m_debug;
}

void GHOST_Wintab::printContextDebugInfo()
{
  if (!m_debug) {
    return;
  }

  /* Print button maps. */
  BYTE logicalButtons[32] = {0};
  BYTE systemButtons[32] = {0};
  for (int i = 0; i < 3; i++) {
    printf("initializeWintab cursor %d buttons\n", i);
    uint lbut = m_fpInfo(WTI_CURSORS + i, CSR_BUTTONMAP, &logicalButtons);
    if (lbut) {
      printf("%d", logicalButtons[0]);
      for (int j = 1; j < lbut; j++) {
        printf(", %d", logicalButtons[j]);
      }
      printf("\n");
    }
    else {
      printf("logical button error\n");
    }
    uint sbut = m_fpInfo(WTI_CURSORS + i, CSR_SYSBTNMAP, &systemButtons);
    if (sbut) {
      printf("%d", systemButtons[0]);
      for (int j = 1; j < sbut; j++) {
        printf(", %d", systemButtons[j]);
      }
      printf("\n");
    }
    else {
      printf("system button error\n");
    }
  }

  /* Print context information. */

  /* Print open context constraints. */
  uint maxcontexts, opencontexts;
  m_fpInfo(WTI_INTERFACE, IFC_NCONTEXTS, &maxcontexts);
  m_fpInfo(WTI_STATUS, STA_CONTEXTS, &opencontexts);
  printf("%u max contexts, %u open contexts\n", maxcontexts, opencontexts);

  /* Print system information. */
  printf("left: %d, top: %d, width: %d, height: %d\n",
         ::GetSystemMetrics(SM_XVIRTUALSCREEN),
         ::GetSystemMetrics(SM_YVIRTUALSCREEN),
         ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
         ::GetSystemMetrics(SM_CYVIRTUALSCREEN));

  auto printContextRanges = [](LOGCONTEXT &lc) {
    printf("lcInOrgX: %d, lcInOrgY: %d, lcInExtX: %d, lcInExtY: %d\n",
           lc.lcInOrgX,
           lc.lcInOrgY,
           lc.lcInExtX,
           lc.lcInExtY);
    printf("lcOutOrgX: %d, lcOutOrgY: %d, lcOutExtX: %d, lcOutExtY: %d\n",
           lc.lcOutOrgX,
           lc.lcOutOrgY,
           lc.lcOutExtX,
           lc.lcOutExtY);
    printf("lcSysOrgX: %d, lcSysOrgY: %d, lcSysExtX: %d, lcSysExtY: %d\n",
           lc.lcSysOrgX,
           lc.lcSysOrgY,
           lc.lcSysExtX,
           lc.lcSysExtY);
  };

  LOGCONTEXT lc;

  /* Print system context. */
  m_fpInfo(WTI_DEFSYSCTX, 0, &lc);
  printf("WTI_DEFSYSCTX\n");
  printContextRanges(lc);

  /* Print system context, manually populated. */
  m_fpInfo(WTI_DEFSYSCTX, CTX_INORGX, &lc.lcInOrgX);
  m_fpInfo(WTI_DEFSYSCTX, CTX_INORGY, &lc.lcInOrgY);
  m_fpInfo(WTI_DEFSYSCTX, CTX_INEXTX, &lc.lcInExtX);
  m_fpInfo(WTI_DEFSYSCTX, CTX_INEXTY, &lc.lcInExtY);
  m_fpInfo(WTI_DEFSYSCTX, CTX_OUTORGX, &lc.lcOutOrgX);
  m_fpInfo(WTI_DEFSYSCTX, CTX_OUTORGY, &lc.lcOutOrgY);
  m_fpInfo(WTI_DEFSYSCTX, CTX_OUTEXTX, &lc.lcOutExtX);
  m_fpInfo(WTI_DEFSYSCTX, CTX_OUTEXTY, &lc.lcOutExtY);
  m_fpInfo(WTI_DEFSYSCTX, CTX_SYSORGX, &lc.lcSysOrgX);
  m_fpInfo(WTI_DEFSYSCTX, CTX_SYSORGY, &lc.lcSysOrgY);
  m_fpInfo(WTI_DEFSYSCTX, CTX_SYSEXTX, &lc.lcSysExtX);
  m_fpInfo(WTI_DEFSYSCTX, CTX_SYSEXTY, &lc.lcSysExtY);
  printf("WTI_DEFSYSCTX CTX_*\n");
  printContextRanges(lc);

  for (uint i = 0; i < m_numDevices; i++) {
    /* Print individual device system context. */
    m_fpInfo(WTI_DSCTXS + i, 0, &lc);
    printf("WTI_DSCTXS %u\n", i);
    printContextRanges(lc);

    /* Print individual device system context, manually populated. */
    m_fpInfo(WTI_DSCTXS + i, CTX_INORGX, &lc.lcInOrgX);
    m_fpInfo(WTI_DSCTXS + i, CTX_INORGY, &lc.lcInOrgY);
    m_fpInfo(WTI_DSCTXS + i, CTX_INEXTX, &lc.lcInExtX);
    m_fpInfo(WTI_DSCTXS + i, CTX_INEXTY, &lc.lcInExtY);
    m_fpInfo(WTI_DSCTXS + i, CTX_OUTORGX, &lc.lcOutOrgX);
    m_fpInfo(WTI_DSCTXS + i, CTX_OUTORGY, &lc.lcOutOrgY);
    m_fpInfo(WTI_DSCTXS + i, CTX_OUTEXTX, &lc.lcOutExtX);
    m_fpInfo(WTI_DSCTXS + i, CTX_OUTEXTY, &lc.lcOutExtY);
    m_fpInfo(WTI_DSCTXS + i, CTX_SYSORGX, &lc.lcSysOrgX);
    m_fpInfo(WTI_DSCTXS + i, CTX_SYSORGY, &lc.lcSysOrgY);
    m_fpInfo(WTI_DSCTXS + i, CTX_SYSEXTX, &lc.lcSysExtX);
    m_fpInfo(WTI_DSCTXS + i, CTX_SYSEXTY, &lc.lcSysExtY);
    printf("WTI_DSCTX %u CTX_*\n", i);
    printContextRanges(lc);

    /* Print device axis. */
    AXIS axis_x, axis_y;
    m_fpInfo(WTI_DEVICES + i, DVC_X, &axis_x);
    m_fpInfo(WTI_DEVICES + i, DVC_Y, &axis_y);
    printf("WTI_DEVICES %u axis_x org: %d, axis_y org: %d axis_x ext: %d, axis_y ext: %d\n",
           i,
           axis_x.axMin,
           axis_y.axMin,
           axis_x.axMax - axis_x.axMin + 1,
           axis_y.axMax - axis_y.axMin + 1);
  }

  /* Other stuff while we have a log-context. */
  printf("sysmode %d\n", lc.lcSysMode);
}
