
/* -------------------------------------------------------------------- */
/** \name Auto-Key (Pixel Space)
 * \{ */

/* just draw a little warning message in the top-right corner of the viewport
 * to warn that autokeying is enabled */
static void drawAutoKeyWarning(TransInfo *UNUSED(t), ARegion *ar)
{
  const char *printable = IFACE_("Auto Keying On");
  float printable_size[2];
  int xco, yco;

  const rcti *rect = ED_region_visible_rect(ar);

  const int font_id = BLF_default();
  BLF_width_and_height(
      font_id, printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);

  xco = (rect->xmax - U.widget_unit) - (int)printable_size[0];
  yco = (rect->ymax - U.widget_unit);

  /* warning text (to clarify meaning of overlays)
   * - original color was red to match the icon, but that clashes badly with a less nasty border
   */
  unsigned char color[3];
  UI_GetThemeColorShade3ubv(TH_TEXT_HI, -50, color);
  BLF_color3ubv(font_id, color);
#ifdef WITH_INTERNATIONAL
  BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#else
  BLF_draw_default_ascii(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#endif

  /* autokey recording icon... */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  xco -= U.widget_unit;
  yco -= (int)printable_size[1] / 2;

  UI_icon_draw(xco, yco, ICON_REC);

  GPU_blend(false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constraints (View Space)
 * \{ */

/* called from drawview.c, as an extra per-window draw option */
void drawPropCircle(const struct bContext *C, TransInfo *t)
{
  if (t->flag & T_PROP_EDIT) {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    float tmat[4][4], imat[4][4];
    int depth_test_enabled;

    if (t->spacetype == SPACE_VIEW3D && rv3d != NULL) {
      copy_m4_m4(tmat, rv3d->viewmat);
      invert_m4_m4(imat, tmat);
    }
    else {
      unit_m4(tmat);
      unit_m4(imat);
    }

    GPU_matrix_push();

    if (t->spacetype == SPACE_VIEW3D) {
      /* pass */
    }
    else if (t->spacetype == SPACE_IMAGE) {
      GPU_matrix_scale_2f(1.0f / t->aspect[0], 1.0f / t->aspect[1]);
    }
    else if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_ACTION)) {
      /* only scale y */
      rcti *mask = &t->ar->v2d.mask;
      rctf *datamask = &t->ar->v2d.cur;
      float xsize = BLI_rctf_size_x(datamask);
      float ysize = BLI_rctf_size_y(datamask);
      float xmask = BLI_rcti_size_x(mask);
      float ymask = BLI_rcti_size_y(mask);
      GPU_matrix_scale_2f(1.0f, (ysize / xsize) * (xmask / ymask));
    }

    depth_test_enabled = GPU_depth_test_enabled();
    if (depth_test_enabled) {
      GPU_depth_test(false);
    }

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformThemeColor(TH_GRID);

    GPU_logic_op_invert_set(true);
    imm_drawcircball(t->center_global, t->prop_size, imat, pos);
    GPU_logic_op_invert_set(false);

    immUnbindProgram();

    if (depth_test_enabled) {
      GPU_depth_test(true);
    }

    GPU_matrix_pop();
  }
}

/** \} */
