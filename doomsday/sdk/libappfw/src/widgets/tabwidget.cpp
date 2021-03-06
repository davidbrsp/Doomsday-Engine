/** @file tabwidget.cpp  Tab widget.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/TabWidget"
#include "de/ui/ListData"
#include "de/MenuWidget"

#include <de/AnimationRule>

Q_DECLARE_METATYPE(de::ui::Item const *)

namespace de {

DENG_GUI_PIMPL(TabWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DENG2_OBSERVES(ui::Data,             Addition)
, DENG2_OBSERVES(ui::Data,             OrderChange)
, DENG2_OBSERVES(ButtonWidget,         Press)
{
    ui::Data::Pos current = 0;
    MenuWidget *buttons = nullptr;
    bool needUpdate = false;
    bool invertedStyle = false;
    LabelWidget *selected = nullptr;
    AnimationRule *selLeft = nullptr;
    AnimationRule *selWidth = nullptr;

    Impl(Public *i) : Base(i)
    {
        self().add(buttons = new MenuWidget);
        buttons->enableScrolling(false);
        buttons->margins().set("");
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand, GridLayout::ColumnFirst);

        buttons->organizer().audienceForWidgetCreation() += this;
        buttons->organizer().audienceForWidgetUpdate()   += this;
        buttons->items().audienceForAddition()    += this;
        buttons->items().audienceForOrderChange() += this;

        // Center the buttons inside the widget.
        buttons->rule()
                .setInput(Rule::AnchorX, self().rule().left() + self().rule().width() / 2)
                .setInput(Rule::Top, self().rule().top())
                .setAnchorPoint(Vector2f(.5f, 0));

        // Selection highlight.
        self().add(selected = new LabelWidget);
    }

    ~Impl()
    {
        releaseRef(selLeft);
        releaseRef(selWidth);
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        // Set the font and style.
        ButtonWidget &btn = widget.as<ButtonWidget>();
        btn.setSizePolicy(ui::Expand, ui::Expand);
        btn.setFont("tab.label");
        btn.margins().set("dialog.gap");
        btn.set(Background());
        btn.setBehavior(Focusable, UnsetFlags);

        btn.audienceForPress() += this;
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        widget.as<ButtonWidget>().setShortcutKey(item.as<TabItem>().shortcutKey());
    }

    void buttonPressed(ButtonWidget &button)
    {
        self().setCurrent(buttons->items().find(*buttons->organizer().findItemForWidget(button)));
    }

    void dataItemAdded(ui::Data::Pos, ui::Item const &)
    {
        needUpdate = true;
    }

    void dataItemOrderChanged()
    {
        needUpdate = true;
    }

    void setCurrent(ui::Data::Pos pos)
    {
        if (current != pos && pos < buttons->items().size())
        {
            current = pos;
            updateSelected();
            emit self().currentTabChanged();
        }
    }

    void updateSelected()
    {
        selected->set(Background(style().colors().colorf(invertedStyle? "tab.inverted.selected"
                                                                      : "tab.selected")));
        for (ui::Data::Pos i = 0; i < buttons->items().size(); ++i)
        {
            bool const sel = (i == current);
            ButtonWidget &w = buttons->itemWidget<ButtonWidget>(buttons->items().at(i));
            w.setFont(sel? "tab.selected" : "tab.label");
            w.setOpacity(sel? 1.f : 0.7f, 0.4);
            if (!invertedStyle)
            {
                w.setTextColor(sel? "tab.selected" : "text");
                w.setHoverTextColor(sel? "tab.selected" : "text");
            }
            else
            {
                w.setTextColor(sel? "tab.inverted.selected" : "inverted.text");
                w.setHoverTextColor(sel? "tab.inverted.selected" : "inverted.text");
            }
            if (sel)
            {
                TimeSpan span = .2;
                if (!selLeft)
                {
                    // Initialize the animated rules for positioning the
                    // selection highlight.
                    selLeft  = new AnimationRule(0);
                    selWidth = new AnimationRule(0);
                    selected->rule()
                        .setInput(Rule::Width,  *selWidth)
                        .setInput(Rule::Left,   *selLeft);
                    span = 0;
                }
                // Animate to new position.
                selLeft ->set(w.rule().left(),  span);
                selWidth->set(w.rule().width(), span);

                selected->rule()
                    .setInput(Rule::Height, rule("halfunit"))
                    .setInput(Rule::Top,    w.rule().bottom());
            }
        }
    }

    bool handleShortcutKey(KeyEvent const &key)
    {
        foreach (auto *w, buttons->childWidgets())
        {
            if (ButtonWidget *but = maybeAs<ButtonWidget>(w))
            {
                if (but->handleShortcut(key))
                {
                    return true;
                }
            }
        }
        return false;
    }
};

TabWidget::TabWidget(String const &name)
    : GuiWidget(name), d(new Impl(this))
{
    rule().setInput(Rule::Height, d->buttons->rule().height());
}

void TabWidget::useInvertedStyle()
{
    d->invertedStyle = true;
}

void TabWidget::clearItems()
{
    if (d->selLeft)
    {
        // Dependent on the heading widget rules.
        d->selLeft->set(d->selLeft->value());
    }
    items().clear();
}

ui::Data &TabWidget::items()
{
    return d->buttons->items();
}

ui::Data::Pos TabWidget::current() const
{
    return d->current;
}

TabItem &TabWidget::currentItem()
{
    DENG2_ASSERT(d->current < items().size());
    return items().at(d->current).as<TabItem>();
}

void TabWidget::setCurrent(ui::Data::Pos itemPos)
{
    d->setCurrent(itemPos);
}

void TabWidget::update()
{
    GuiWidget::update();

    if (d->needUpdate)
    {
        d->updateSelected();
        d->needUpdate = false;
    }
}

bool TabWidget::handleEvent(Event const &ev)
{
    if (isEnabled())
    {
        if (ev.isKeyDown())
        {
            KeyEvent const &key = ev.as<KeyEvent>();
            if (d->handleShortcutKey(key))
            {
                return true;
            }
        }
    }
    return GuiWidget::handleEvent(ev);
}

} // namespace de
