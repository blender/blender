/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_span.hh"

#include "BKE_context.hh"

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

#include "PRF_profile.hh"

namespace blender::ed::action {

static ed::animfilter::CollectionFilters get_filters(SpaceAction &saction)
{
  using namespace ed::animfilter;
  CollectionFilters filters;
  filters.only_selected = saction.ads.filterflag & ADS_FILTER_ONLYSEL;
  filters.show_hidden = saction.ads.filterflag & ADS_FILTER_INCL_HIDDEN;
  /* TODO: implement. See `fcurve_has_errors`. */
  filters.only_errors = saction.ads.filterflag & ADS_FILTER_ONLY_ERRORS;
  filters.ignore_modifier_ids = saction.ads.filterflag & ADS_FILTER_NOMODIFIERS;

  /* This is not nice but we have to keep for backwards compatibility. In the future it would be
   * good to store the ID type bit directly on the dopesheet. */
  if (saction.ads.filterflag & ADS_FILTER_NOSHAPEKEYS) {
    filters.id_type_blocklist.add(ID_KE);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOMESH) {
    filters.id_type_blocklist.add(ID_ME);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOOBJ) {
    filters.id_type_blocklist.add(ID_OB);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOLAT) {
    filters.id_type_blocklist.add(ID_LT);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOCAM) {
    filters.id_type_blocklist.add(ID_CA);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOMAT) {
    filters.id_type_blocklist.add(ID_MA);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOLAM) {
    filters.id_type_blocklist.add(ID_LA);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOCUR) {
    filters.id_type_blocklist.add(ID_CV);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOWOR) {
    filters.id_type_blocklist.add(ID_WO);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOSCE) {
    filters.id_type_blocklist.add(ID_SCE);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOPART) {
    filters.id_type_blocklist.add(ID_PA);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOMBA) {
    filters.id_type_blocklist.add(ID_MB);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOARM) {
    filters.id_type_blocklist.add(ID_AR);
  }
  if (saction.ads.filterflag & ADS_FILTER_NONTREE) {
    filters.id_type_blocklist.add(ID_NT);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOTEX) {
    filters.id_type_blocklist.add(ID_TE);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOSPK) {
    filters.id_type_blocklist.add(ID_SPK);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOLINESTYLE) {
    filters.id_type_blocklist.add(ID_LS);
  }
  if (saction.ads.filterflag & ADS_FILTER_NOGPENCIL) {
    filters.id_type_blocklist.add(ID_GP);
  }
  /* Ignoring ADS_FILTER_NLA_NOACT since we don't support the NLA here.  */

  /* NOTE: checking `filterflag2` from here on! */
  if (saction.ads.filterflag2 & ADS_FILTER_NOCACHEFILES) {
    filters.id_type_blocklist.add(ID_CF);
  }
  if (saction.ads.filterflag2 & ADS_FILTER_NOMOVIECLIPS) {
    filters.id_type_blocklist.add(ID_MC);
  }
  if (saction.ads.filterflag2 & ADS_FILTER_NOHAIR) {
    /* TODO hair has no ID type? */
    // filters.id_type_blocklist.add();
  }
  if (saction.ads.filterflag2 & ADS_FILTER_NOPOINTCLOUD) {
    filters.id_type_blocklist.add(ID_PT);
  }
  if (saction.ads.filterflag2 & ADS_FILTER_NOVOLUME) {
    filters.id_type_blocklist.add(ID_VO);
  }
  if (saction.ads.filterflag2 & ADS_FILTER_DRIVER_FALLBACK_AS_ERROR) {
    /* TODO check in legacy system how that worked. */
    // filters.id_type_blocklist.add(ID_CF);
  }
  if (saction.ads.filterflag2 & ADS_FILTER_NOLIGHTPROBE) {
    filters.id_type_blocklist.add(ID_LP);
  }
  return filters;
}

ed::animfilter::ChannelArena get_data_channels(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  ChannelArena arena;
  collect_animation_channels(C, get_filters(*saction), arena);
  return arena;
}

ed::animfilter::ChannelArena get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  PRF_scope_with_name("ed::animfilter::get_summary_tree", ProfileCategory::Editor);

  ChannelArena arena;
  collect_animation_channels(C, get_filters(*saction), arena);
  /* TODO: don't hard-code the hierarchy. */
  group_channels(arena,
                 {
                     ChannelTagType::ID,
                     ChannelTagType::ACTION,
                     ChannelTagType::ACTION_SLOT,
                     ChannelTagType::FCURVE_GROUP,
                     ChannelTagType::BONE,
                 },
                 saction->ads.flag & ADS_FLAG_MINIMAL_GROUPING);

  {
    PRF_scope_with_name("ed::animfilter::get_summary_tree::ui_state", ProfileCategory::Editor);

    /* Record this "use" of the channel runtime, as this is the moment where new channels are first
     * seen and old channels no longer exist. */
    saction->runtime->last_used++;

    /* Update the channels with the previously-stored UI state. */
    for (SummaryChannel *summary : arena.summary_channels) {
      summary->ui_state_map_set(&saction->runtime->channel_states);
    }
  }
  return arena;
}

}  // namespace blender::ed::action
