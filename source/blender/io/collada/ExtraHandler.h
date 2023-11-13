/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <algorithm> /* sort() */
#include <map>
#include <string>
#include <vector>

#include "COLLADASWInstanceController.h"
#include "COLLADASaxFWLFilePartLoader.h"
#include "COLLADASaxFWLIExtraDataCallbackHandler.h"

#include "AnimationImporter.h"
#include "DocumentImporter.h"

/** \brief Handler class for \<extra\> data, through which different
 * profiles can be handled
 */
class ExtraHandler : public COLLADASaxFWL::IExtraDataCallbackHandler {
 public:
  /** Constructor. */
  ExtraHandler(DocumentImporter *dimp, AnimationImporter *aimp);

  /** Handle the beginning of an element. */
  bool elementBegin(const char *elementName, const char **attributes);

  /** Handle the end of an element. */
  bool elementEnd(const char *elementName);

  /** Receive the data in text format. */
  bool textData(const char *text, size_t textLength);

  /** Method to ask, if the current callback handler want to read the data of the given extra
   * element. */
  bool parseElement(const char *profileName,
                    const unsigned long &elementHash,
                    const COLLADAFW::UniqueId &uniqueId,
                    COLLADAFW::Object *object);

  /** For backwards compatibility with older OpenCollada, new version added object parameter */
  bool parseElement(const char *profileName,
                    const unsigned long &elementHash,
                    const COLLADAFW::UniqueId &uniqueId);

 private:
  /** Disable default copy constructor. */
  ExtraHandler(const ExtraHandler &pre);
  /** Disable default assignment operator. */
  const ExtraHandler &operator=(const ExtraHandler &pre);

  /** Handle to DocumentImporter for interface to extra element data saving. */
  DocumentImporter *dimp;
  AnimationImporter *aimp;
  /** Holds Id of element for which <extra> XML elements are handled. */
  COLLADAFW::UniqueId currentUid;
  ExtraTags *currentExtraTags;
  std::string currentElement;
};
