/*
    widgets.cpp: Additional widgets that are not part of NanoGUI

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "widgets.h"
#include "common.h"
#include <nanogui/opengl.h>

using nanogui::Button;
using nanogui::utf8;
using nanogui::nvgIsFontIcon;
using nanogui::Color;
    
ProgressButton::ProgressButton(Widget *parent, const std::string &caption, int icon)
 : Button(parent, caption, icon), mProgress(0.f) { }

void ProgressButton::draw(NVGcontext *ctx) {
    Widget::draw(ctx);

    NVGcolor gradTop = mTheme->mButtonGradientTopUnfocused;
    NVGcolor gradBot = mTheme->mButtonGradientBotUnfocused;

    if (mPushed && mProgress == 1) {
        gradTop = mTheme->mButtonGradientTopPushed;
        gradBot = mTheme->mButtonGradientBotPushed;
    } else if (mMouseFocus && mEnabled) {
        gradTop = mTheme->mButtonGradientTopFocused;
        gradBot = mTheme->mButtonGradientBotFocused;
    }

    if (mProgress != 1) {
        nvgSave(ctx);
        nvgScissor(ctx, mPos.x(), mPos.y(), (int) (mSize.x() * mProgress), mSize.y());
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, mPos.x() + 1, mPos.y() + 1.0f, mSize.x() - 2,
                   mSize.y() - 2, mTheme->mButtonCornerRadius - 1);

    if (mBackgroundColor.w() != 0) {
        nvgFillColor(ctx, Color(mBackgroundColor.head<3>(), 1.f));
        nvgFill(ctx);
        if (mPushed && mProgress == 1) {
            gradTop.a = gradBot.a = 0.8f;
        } else {
            double v = 1 - mBackgroundColor.w();
            gradTop.a = gradBot.a = mEnabled ? v : v * .5f + .5f;
        }
    }

    NVGpaint bg = nvgLinearGradient(ctx, mPos.x(), mPos.y(), mPos.x(),
                                    mPos.y() + mSize.y(), gradTop, gradBot);

    nvgFillPaint(ctx, bg);
    nvgFill(ctx);

    if (mProgress != 1) {
        nvgRestore(ctx);
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, mPos.x() + 0.5f, mPos.y() + (mPushed ? 0.5f : 1.5f), mSize.x() - 1,
                   mSize.y() - 1 - (mPushed ? 0.0f : 1.0f), mTheme->mButtonCornerRadius);
    nvgStrokeColor(ctx, mTheme->mBorderLight);
    nvgStroke(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, mPos.x() + 0.5f, mPos.y() + 0.5f, mSize.x() - 1,
                   mSize.y() - 2, mTheme->mButtonCornerRadius);
    nvgStrokeColor(ctx, mTheme->mBorderDark);
    nvgStroke(ctx);

    nvgFontSize(ctx, mFontSize == -1 ? mTheme->mButtonFontSize : mFontSize);
    nvgFontFace(ctx, "sans-bold");
    float tw = nvgTextBounds(ctx, 0,0, mCaption.c_str(), nullptr, nullptr);

    Vector2f center = mPos.cast<float>() + mSize.cast<float>() * 0.5f;
    Vector2f textPos(center.x() - tw * 0.5f, center.y() - 1);
    NVGcolor textColor =
        mTextColor.w() == 0 ? mTheme->mTextColor : mTextColor;
    if (!mEnabled)
        textColor = mTheme->mDisabledTextColor;

    if (mIcon) {
        auto icon = utf8(mIcon);

        float iw, ih = mFontSize == -1 ? mTheme->mButtonFontSize : mFontSize;
        if (nvgIsFontIcon(mIcon)) {
            ih *= 1.5f;
            nvgFontSize(ctx, ih);
            nvgFontFace(ctx, "icons");
            iw = nvgTextBounds(ctx, 0, 0, icon.data(), nullptr, nullptr);
        } else {
            int w, h;
            ih *= 0.9f;
            nvgImageSize(ctx, mIcon, &w, &h);
            iw = w * ih / h;
        }
        if (mCaption != "")
            iw += mSize.y() * 0.15f;
        nvgFillColor(ctx, textColor);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        Vector2f iconPos = center;
        iconPos.y() -= 1;

        if (mIconPosition == IconPosition::LeftCentered) {
            iconPos.x() -= (tw + iw) * 0.5f;
            textPos.x() += iw * 0.5f;
        } else if (mIconPosition == IconPosition::RightCentered) {
            textPos.x() -= iw * 0.5f;
            iconPos.x() += tw * 0.5f;
        } else if (mIconPosition == IconPosition::Left) {
            iconPos.x() = mPos.x() + 8;
        } else if (mIconPosition == IconPosition::Right) {
            iconPos.x() = mPos.x() + mSize.x() - iw - 8;
        }

        if (nvgIsFontIcon(mIcon)) {
            nvgText(ctx, iconPos.x(), iconPos.y()+1, icon.data(), nullptr);
        } else {
            NVGpaint imgPaint = nvgImagePattern(ctx,
                    iconPos.x(), iconPos.y() - ih/2, iw, ih, 0, mIcon, mEnabled ? 0.5f : 0.25f);

            nvgFillPaint(ctx, imgPaint);
            nvgFill(ctx);
        }
    }

    nvgFontSize(ctx, mFontSize == -1 ? mTheme->mButtonFontSize : mFontSize);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, mTheme->mTextColorShadow);
    nvgText(ctx, textPos.x(), textPos.y(), mCaption.c_str(), nullptr);
    nvgFillColor(ctx, textColor);
    nvgText(ctx, textPos.x(), textPos.y() + 1, mCaption.c_str(), nullptr);
}
