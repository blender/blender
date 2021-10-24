/*
    gui_serializer.h: Utility class for serializing the entire user interface
    state to a file. which is useful for debugging.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/


#pragma once

#include "widgets.h"
#include "serializer.h"

class GUISerializer : public Serializer {
public:
    GUISerializer() : Serializer() { }
    GUISerializer(const std::string &filename, bool compat) : Serializer(filename, compat) { }
    GUISerializer(const std::string &filename, bool compatibilityMode = false,
               const ProgressCallback &progress = ProgressCallback()) : Serializer(filename, compatibilityMode, progress) { }
    using Serializer::get;
    using Serializer::set;

    bool get(const std::string &key, Widget *widget) const {
        pushPrefix(key);
        bool result = getRecursive(widget);
        popPrefix();
        return result;
    }

    void set(const std::string &key, const Widget *widget) {
        pushPrefix(key);
        setRecursive(widget);
        popPrefix();
    }
protected:
    void setRecursive(const Widget *widget) {
        bool serialize = widget->id().length() > 0;
        if (serialize) {
            pushPrefix(widget->id());

            if (dynamic_cast<const TextBox *>(widget))
                setTextBox((const TextBox *) widget);
            else if (dynamic_cast<const Slider *>(widget))
                setSlider((const Slider *) widget);
            else if (dynamic_cast<const ComboBox *>(widget))
                setComboBox((const ComboBox *) widget);
            else if (dynamic_cast<const CheckBox *>(widget))
                setCheckBox((const CheckBox *) widget);
            else if (dynamic_cast<const ProgressButton *>(widget))
                setProgressButton((const ProgressButton *) widget);
            else if (dynamic_cast<const Button *>(widget))
                setButton((const Button *) widget);
            else if (dynamic_cast<const Label *>(widget))
                setLabel((const Label *) widget);
            else
                setWidget(widget);
        }

        for (const Widget *child : widget->children())
            setRecursive(child);

        if (serialize)
            popPrefix();
    }

    bool getRecursive(Widget *widget) const {
        bool serialize = widget->id().length() > 0;
        bool success = true;
        if (serialize) {
            pushPrefix(widget->id());

            if (dynamic_cast<TextBox *>(widget))
                success &= getTextBox((TextBox *) widget);
            else if (dynamic_cast<Slider *>(widget))
                success &= getSlider((Slider *) widget);
            else if (dynamic_cast<ComboBox *>(widget))
                success &= getComboBox((ComboBox *) widget);
            else if (dynamic_cast<CheckBox *>(widget))
                success &= getCheckBox((CheckBox *) widget);
            else if (dynamic_cast<ProgressButton *>(widget))
                success &= getProgressButton((ProgressButton *) widget);
            else if (dynamic_cast<Button *>(widget))
                success &= getButton((Button *) widget);
            else if (dynamic_cast<Label *>(widget))
                success &= getLabel((Label *) widget);
            else
                success &= getWidget(widget);
        }

        for (Widget *child : widget->children())
            success &= getRecursive(child);

        if (serialize)
            popPrefix();
        return success;
    }

    void setWidget(const Widget *widget) {
        set("position", widget->position());
        set("size", widget->size());
        set("fixedSize", widget->fixedSize());
        set("visible", widget->visible());
        set("enabled", widget->enabled());
        set("focused", widget->focused());
        set("tooltip", widget->tooltip());
        set("fontSize", widget->hasFontSize() ? widget->fontSize() : -1);
    }

    #define try_get(name, ref) \
        if (!get(name, ref)) \
            return false;

    bool getWidget(Widget *widget) const {
        Vector2i v2i_val;
        bool bool_val;
        std::string str_val;
        int int_val;

        try_get("position", v2i_val); widget->setPosition(v2i_val);
        try_get("size", v2i_val); widget->setSize(v2i_val);
        try_get("fixedSize", v2i_val); widget->setFixedSize(v2i_val);
        try_get("visible", bool_val); widget->setVisible(bool_val);
        try_get("enabled", bool_val); widget->setEnabled(bool_val);
        try_get("focused", bool_val); widget->setFocused(bool_val);
        try_get("tooltip", str_val); widget->setTooltip(str_val);
        try_get("fontSize", int_val); widget->setFontSize(int_val);
        return true;
    }

    void setLabel(const Label *widget) {
        setWidget(widget);

        set("caption", widget->caption());
        set("font", widget->font());
        set("fontSize", widget->fontSize());
    }

    bool getLabel(Label *widget) const {
        getWidget(widget);

        std::string str_val;
        try_get("caption", str_val); widget->setCaption(str_val);
        try_get("font", str_val); widget->setFont(str_val);
        return true;
    }

    void setButton(const Button *widget) {
        setWidget(widget);

        set("caption", widget->caption());
        set("icon", widget->icon());
        set("iconPosition", (int) widget->iconPosition());
        set("pushed", widget->pushed());
        set("buttonFlags", widget->flags());
        set("backgroundColor", widget->backgroundColor());
        set("textColor", widget->textColor());
    }

    bool getButton(Button *widget) const {
        getWidget(widget);
        Color col_val;
        int int_val;
        bool bool_val;
        std::string str_val;

        try_get("caption", str_val); widget->setCaption(str_val);
        try_get("icon", int_val); widget->setIcon(int_val);
        try_get("iconPosition", int_val); widget->setIconPosition((Button::IconPosition) int_val);
        try_get("pushed", bool_val); widget->setPushed(bool_val);
        try_get("buttonFlags", int_val); widget->setFlags(int_val);
        try_get("backgroundColor", col_val); widget->setBackgroundColor(col_val);
        try_get("textColor", col_val); widget->setTextColor(col_val);
        try_get("fontSize", int_val); widget->setFontSize(int_val);
        return true;
    }

    void setProgressButton(const ProgressButton *widget) {
        setButton(widget);

        set("progress", widget->progress());
    }

    bool getProgressButton(ProgressButton *widget) const {
        getButton(widget);
        Float float_val;
        try_get("progress", float_val); widget->setProgress(float_val);
        return true;
    }

    void setCheckBox(const CheckBox *widget) {
        setWidget(widget);

        set("caption", widget->caption());
        set("pushed", widget->pushed());
        set("checked", widget->checked());
    }

    bool getCheckBox(CheckBox *widget) const {
        getWidget(widget);
        bool bool_val;
        std::string str_val;

        try_get("caption", str_val); widget->setCaption(str_val);
        try_get("pushed", bool_val); widget->setPushed(bool_val);
        try_get("checked", bool_val); widget->setChecked(bool_val);
        return true;
    }

    void setComboBox(const ComboBox *widget) {
        setButton(widget);
        set("selectedIndex", widget->selectedIndex());
    }

    bool getComboBox(ComboBox *widget) const {
        getButton(widget);
        int int_val;
        try_get("selectedIndex", int_val); widget->setSelectedIndex(int_val);
        return true;
    }

    void setSlider(const Slider *widget) {
        setWidget(widget);
        set("value", widget->value());
        set("highlightedRange.min", widget->highlightedRange().first);
        set("highlightedRange.max", widget->highlightedRange().second);
    }

    bool getSlider(Slider *widget) const {
        getWidget(widget);
        float float_val, float_val_2;
        try_get("value", float_val); widget->setValue(float_val);
        try_get("highlightedRange.min", float_val);
        try_get("highlightedRange.max", float_val_2);
        widget->setHighlightedRange(std::make_pair(float_val, float_val_2));
        return true;
    }

    void setTextBox(const TextBox *widget) {
        setWidget(widget);
        set("value", widget->value());
        set("units", widget->units());
        set("unitsImage", widget->unitsImage());
    }

    bool getTextBox(TextBox *widget) const {
        getWidget(widget);
        std::string str_value;
        int int_value;

        try_get("value", str_value); widget->setValue(str_value);
        try_get("units", str_value); widget->setUnits(str_value);
        try_get("unitsImage", int_value); widget->setUnitsImage(int_value);
        return true;
    }
    #undef try_get
};
