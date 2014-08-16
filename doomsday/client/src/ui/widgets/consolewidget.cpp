/** @file consolewidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/widgets/consolewidget.h"
#include "CommandAction"
#include "ui/widgets/consolecommandwidget.h"
#include "ui/widgets/inputbindingwidget.h"
#include "ui/dialogs/logsettingsdialog.h"
#include "ui/clientwindow.h"
#include "ui/styledlogsinkformatter.h"

#include <de/App>
#include <de/ScalarRule>
#include <de/KeyEvent>
#include <de/MouseEvent>
#include <de/ui/VariableToggleItem>
#include <de/ui/SubwidgetItem>
#include <de/SignalAction>
#include <de/ButtonWidget>
#include <de/ScriptCommandWidget>
#include <de/PopupMenuWidget>
#include <de/ToggleWidget>
#include <de/LogWidget>
#include <de/PersistentState>
#include <de/NativeFile>
#include <QCursor>
#include <QClipboard>

using namespace de;

static TimeDelta const LOG_OPEN_CLOSE_SPAN = 0.2;

DENG_GUI_PIMPL(ConsoleWidget),
DENG2_OBSERVES(Variable, Change)
{
    ButtonWidget *button;
    ConsoleCommandWidget *cmdLine;
    ScriptCommandWidget *scriptCmd;
    PopupMenuWidget *menu;
    LogWidget *log;
    ScalarRule *horizShift;
    ScalarRule *height;
    ScalarRule *width;
    StyledLogSinkFormatter formatter;

    enum GrabEdge {
        NotGrabbed = 0,
        RightEdge,
        TopEdge
    };

    bool opened;
    bool scriptMode;
    int grabWidth;
    GrabEdge grabHover;
    GrabEdge grabbed;

    Instance(Public *i)
        : Base(i),
          button(0),
          cmdLine(0),
          scriptCmd(0),
          menu(0),
          log(0),
          opened(true),
          scriptMode(false),
          grabWidth(0),
          grabHover(NotGrabbed),
          grabbed(NotGrabbed)
    {
        horizShift = new ScalarRule(0);
        width      = new ScalarRule(style().rules().rule("console.width").valuei());
        height     = new ScalarRule(0);

        grabWidth  = style().rules().rule("gap").valuei();

        App::config()["console.script"].audienceForChange() += this;
    }

    ~Instance()
    {
        App::config()["console.script"].audienceForChange() -= this;

        releaseRef(horizShift);
        releaseRef(width);
        releaseRef(height);
    }

    void expandLog(int delta, bool useOffsetAnimation)
    {       
        // Cannot expand if the user is grabbing the top edge.
        if(grabbed == TopEdge) return;

        Animation::Style style = useOffsetAnimation? Animation::EaseOut : Animation::Linear;

        if(height->animation().target() == 0)
        {
            // On the first expansion make sure the margins are taken into account.
            delta += log->margins().height().valuei();
        }

        height->set(height->animation().target() + delta, .25f);
        height->setStyle(style);

        if(useOffsetAnimation && opened)
        {
            // Sync the log content with the height animation.
            log->setContentYOffset(Animation::range(style,
                                                    log->contentYOffset().value() + delta, 0,
                                                    height->animation().remainingTime()));
        }
    }

    bool handleMousePositionEvent(MouseEvent const &mouse)
    {
        if(grabbed == RightEdge)
        {
            // Adjust width.
            width->set(mouse.pos().x - self.rule().left().valuei(), .1f);
            return true;
        }
        else if(grabbed == TopEdge)
        {
            if(mouse.pos().y < self.rule().bottom().valuei())
            {
                height->set(self.rule().bottom().valuei() - mouse.pos().y, .1f);
                log->enablePageKeys(false);
            }
            return true;
        }

        // Check for grab at the right edge.
        Rectanglei pos = self.rule().recti();
        pos.topLeft.x = pos.bottomRight.x - grabWidth;
        if(pos.contains(mouse.pos()))
        {
            if(grabHover != RightEdge)
            {
                grabHover = RightEdge;
                self.root().window().canvas().setCursor(Qt::SizeHorCursor);
            }
        }
        else
        {
            // Maybe a grab at the top edge, then.
            pos = self.rule().recti();
            pos.bottomRight.y = pos.topLeft.y + grabWidth;
            if(pos.contains(mouse.pos()))
            {
                if(grabHover != TopEdge)
                {
                    grabHover = TopEdge;
                    self.root().window().canvas().setCursor(Qt::SizeVerCursor);
                }
            }
            else if(grabHover != NotGrabbed)
            {
                grabHover = NotGrabbed;
                self.root().window().canvas().setCursor(Qt::ArrowCursor);
            }
        }

        return false;
    }

    void variableValueChanged(Variable &, Value const &newValue)
    {
        // We are listening to the 'console.script' variable.
        setScriptMode(newValue.isTrue());
    }

    void setScriptMode(bool yes)
    {
        CommandWidget *current;
        CommandWidget *next;

        if(scriptMode)
        {
            current = scriptCmd;
        }
        else
        {
            current = cmdLine;
        }

        if(yes)
        {
            next = scriptCmd;
        }
        else
        {
            next = cmdLine;
        }

        // Update the prompt to reflect the mode.
        button->setText(yes? _E(b) "$" : _E(b) ">");

        // Bottom of the console must follow the active command line height.
        self.rule().setInput(Rule::Bottom, next->rule().top() - style().rules().rule("unit"));

        if(scriptMode == yes)
        {
            return; // No need to change anything else.
        }

        scriptCmd->setAttribute(AnimateOpacityWhenEnabledOrDisabled, UnsetFlags);
        cmdLine->setAttribute(AnimateOpacityWhenEnabledOrDisabled, UnsetFlags);

        scriptCmd->show(yes);        
        scriptCmd->enable(yes);
        cmdLine->show(!yes);
        cmdLine->disable(yes);

        // Transfer focus.
        if(current->hasFocus())
        {
            root().setFocus(next);
        }

        scriptMode = yes;

        emit self.commandModeChanged();
    }
};

static PopupWidget *consoleShortcutPopup()
{
    PopupWidget *pop = new PopupWidget;
    // The 'padding' widget will provide empty margins around the content.
    // Popups normally do not provide any margins.
    GuiWidget *padding = new GuiWidget;
    padding->margins().set("dialog.gap");
    InputBindingWidget *bind = InputBindingWidget::newTaskBarShortcut();
    bind->setSizePolicy(ui::Expand, ui::Expand);
    padding->add(bind);
    // Place the binding inside the padding.
    bind->rule()
            .setLeftTop(padding->rule().left() + padding->margins().left(),
                        padding->rule().top()  + padding->margins().top());
    padding->rule()
            .setInput(Rule::Width,  bind->rule().width()  + padding->margins().width())
            .setInput(Rule::Height, bind->rule().height() + padding->margins().height());
    pop->setContent(padding);
    return pop;
}

static PopupWidget *advancedFeaturesPopup()
{
    auto *menu = new PopupMenuWidget;
    menu->items()
            << new ui::ActionItem(QObject::tr("Copy Log Path to Clipboard"),
                                  new SignalAction(&ClientWindow::main().console(),
                                                   SLOT(copyLogPathToClipboard())))
            << new ui::Item(ui::Item::Annotation,
                            QObject::tr("The location of the log output file is copied to "
                                        "the OS clipboard."))
            << new ui::Item(ui::Item::Separator)
            << new ui::VariableToggleItem(QObject::tr("Doomsday Script"),
                                          App::config()["console.script"])
            << new ui::Item(ui::Item::Annotation,
                            QObject::tr("The command prompt becomes an interactive script "
                                        "process with access to all the runtime modules."));
    return menu;
}

ConsoleWidget::ConsoleWidget() : GuiWidget("console"), d(new Instance(this))
{
    d->button = new ButtonWidget;
    d->button->setAction(new SignalAction(this, SLOT(openMenu())));
    add(d->button);

    d->cmdLine = new ConsoleCommandWidget("commandline");
    d->cmdLine->setEmptyContentHint(tr("Enter commands here") /*  " _E(r)_E(l)_E(t) "SHIFT-ESC" */);
    add(d->cmdLine);

    d->scriptCmd = new ScriptCommandWidget("script");
    d->scriptCmd->setEmptyContentHint(tr("Enter scripts here"));
    add(d->scriptCmd);

    d->button->setOpacity(.75f);
    d->cmdLine->setOpacity(.75f);
    d->scriptCmd->setOpacity(.75f);

    d->log = new LogWidget("log");
    d->log->setLogFormatter(d->formatter);
    d->log->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Top,    OperatorRule::maximum(rule().top(), Const(0)));
    add(d->log);

    // Blur the log background.
    enableBlur();

    // Width of the console is defined by the style.
    rule()
        .setInput(Rule::Height, *d->height)
        .setInput(Rule::Width, OperatorRule::minimum(ClientWindow::main().root().viewWidth(),
                                                     OperatorRule::maximum(*d->width, Const(320))));

    closeLog();

    // Menu for console related functions.
    d->menu = new PopupMenuWidget;
    d->menu->setAnchor(d->button->rule().left() + d->button->rule().width() / 2,
                       d->button->rule().top());

    d->menu->items()
            << new ui::Item(ui::Item::Separator, tr("Log History"))
            << new ui::ActionItem(tr("Clear Log"), new CommandAction("clear"))
            << new ui::ActionItem(tr("Show Full Log"), new SignalAction(this, SLOT(showFullLog())))
            << new ui::ActionItem(tr("Scroll to Bottom"), new SignalAction(d->log, SLOT(scrollToBottom())))
            << new ui::VariableToggleItem(tr("Go to Bottom on Enter"), App::config()["console.snap"])
            << new ui::VariableToggleItem(tr("Show Metadata"), App::config()["log.showMetadata"])
            << new ui::Item(ui::Item::Annotation, tr("Time and subsystem of each entry is printed."))
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, tr("Behavior"))
            << new ui::SubwidgetItem(tr("Log Filter & Alerts..."), ui::Right, makePopup<LogSettingsDialog>)
            << new ui::SubwidgetItem(tr("Shortcut Key..."), ui::Right, consoleShortcutPopup)
            << new ui::Item(ui::Item::Separator)
            << new ui::SubwidgetItem(tr("Advanced..."), ui::Right, advancedFeaturesPopup);

    add(d->menu);

    d->setScriptMode(App::config().getb("console.script"));

    // Signals.
    connect(d->log, SIGNAL(contentHeightIncreased(int)), this, SLOT(logContentHeightIncreased(int)));

    connect(d->cmdLine, SIGNAL(gotFocus()), this, SLOT(commandLineFocusGained()));
    connect(d->cmdLine, SIGNAL(lostFocus()), this, SLOT(commandLineFocusLost()));
    connect(d->cmdLine, SIGNAL(commandEntered(de::String)), this, SLOT(commandWasEntered(de::String)));

    connect(d->scriptCmd, SIGNAL(gotFocus()), this, SLOT(commandLineFocusGained()));
    connect(d->scriptCmd, SIGNAL(lostFocus()), this, SLOT(commandLineFocusLost()));
    connect(d->scriptCmd, SIGNAL(commandEntered(de::String)), this, SLOT(commandWasEntered(de::String)));
}

ButtonWidget &ConsoleWidget::button()
{
    return *d->button;
}

CommandWidget &ConsoleWidget::commandLine()
{
    if(d->scriptMode) return *d->scriptCmd;
    return *d->cmdLine;
}

LogWidget &ConsoleWidget::log()
{
    return *d->log;
}

Rule const &ConsoleWidget::shift()
{
    return *d->horizShift;
}

bool ConsoleWidget::isLogOpen() const
{
    return d->opened;
}

void ConsoleWidget::enableBlur(bool yes)
{
    Background logBg = d->log->background();
    if(yes)
    {
        logBg.type = Background::Blurred;
    }
    else
    {
        logBg.type = Background::None;
    }
    d->log->set(logBg);
}

void ConsoleWidget::viewResized()
{
    GuiWidget::viewResized();

    if(!d->opened)
    {
        // Make sure it stays shifted out of the view.
        d->horizShift->set(-rule().width().valuei() - 1);
    }
}

void ConsoleWidget::update()
{
    GuiWidget::update();

    if(rule().top().valuei() <= 0)
    {
        // The expansion has reached the top of the screen, so we
        // can enable PageUp/Dn keys for the log.
        d->log->enablePageKeys(true);
    }
}

bool ConsoleWidget::handleEvent(Event const &event)
{
    // Hovering over the right edge shows the <-> cursor.
    if(event.type() == Event::MousePosition)
    {
        if(d->handleMousePositionEvent(event.as<MouseEvent>()))
        {
            return true;
        }
    }

    // Dragging an edge resizes the widget.
    if(d->grabHover != Instance::NotGrabbed && event.type() == Event::MouseButton)
    {
        switch(handleMouseClick(event))
        {
        case MouseClickStarted:
            d->grabbed = d->grabHover;
            return true;

        case MouseClickAborted:
        case MouseClickFinished:
            d->grabbed = Instance::NotGrabbed;
            return true;

        default:
            break;
        }
    }

    if(event.type() == Event::MouseButton && hitTest(event))
    {
        // Prevent clicks from leaking through.
        return true;
    }

    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if(key.qtKey() == Qt::Key_PageUp ||
           key.qtKey() == Qt::Key_PageDown)
        {
            if(!isLogOpen()) openLog();
            showFullLog();
            return true;
        }

        if(key.qtKey() == Qt::Key_F5) // Clear history.
        {
            clearLog();
            return true;
        }
    }
    return false;
}

void ConsoleWidget::operator >> (PersistentState &toState) const
{
    toState.names().set("console.width", d->width->value());
}

void ConsoleWidget::operator << (PersistentState const &fromState)
{
    d->width->set(fromState.names()["console.width"]);

    if(!d->opened)
    {
        // Make sure it stays out of the view.
        d->horizShift->set(-rule().width().valuei() - 1);
    }
}

void ConsoleWidget::openLog()
{
    if(d->opened) return;

    d->opened = true;
    d->horizShift->set(0, LOG_OPEN_CLOSE_SPAN);
    d->log->unsetBehavior(DisableEventDispatch);
}

void ConsoleWidget::closeLog()
{
    if(!d->opened) return;

    d->opened = false;
    d->horizShift->set(-rule().width().valuei() - 1, LOG_OPEN_CLOSE_SPAN);
    d->log->setBehavior(DisableEventDispatch);
}

void ConsoleWidget::clearLog()
{
    d->height->set(0);
    d->log->scrollToBottom();
    d->log->enablePageKeys(false);
}

void ConsoleWidget::showFullLog()
{
    openLog();
    d->log->enablePageKeys(true);
    d->expandLog(rule().top().valuei(), false);
}

void ConsoleWidget::logContentHeightIncreased(int delta)
{
    d->expandLog(delta, true);
}

void ConsoleWidget::setFullyOpaque()
{
    d->scriptCmd->setAttribute(AnimateOpacityWhenEnabledOrDisabled);
    d->cmdLine->setAttribute(AnimateOpacityWhenEnabledOrDisabled);

    d->button->setOpacity(1, .25f);
    d->cmdLine->setOpacity(1, .25f);
    d->scriptCmd->setOpacity(1, .25f);
}

void ConsoleWidget::commandLineFocusGained()
{
    setFullyOpaque();
    openLog();

    emit commandLineGotFocus();
}

void ConsoleWidget::commandLineFocusLost()
{
    d->scriptCmd->setAttribute(AnimateOpacityWhenEnabledOrDisabled);
    d->cmdLine->setAttribute(AnimateOpacityWhenEnabledOrDisabled);

    d->button->setOpacity(.75f, .25f);
    d->cmdLine->setOpacity(.75f, .25f);
    d->scriptCmd->setOpacity(.75f, .25f);
    closeLog();
}

void ConsoleWidget::focusOnCommandLine()
{
    root().setFocus(&commandLine());
}

void ConsoleWidget::openMenu()
{
    d->menu->open();
}

void ConsoleWidget::closeMenu()
{
    d->menu->close();
}

void ConsoleWidget::commandWasEntered(String const &)
{
    if(App::config().getb("console.snap") && !d->log->isAtBottom())
    {
        d->log->scrollToBottom();
    }
}

void ConsoleWidget::copyLogPathToClipboard()
{
    if(NativeFile *native = App::rootFolder().tryLocate<NativeFile>(LogBuffer::get().outputFile()))
    {
        qApp->clipboard()->setText(native->nativePath());
    }
}
