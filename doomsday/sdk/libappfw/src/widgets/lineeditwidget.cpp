/** @file widgets/lineeditwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/LineEditWidget"
#include "de/LabelWidget"
#include "de/FontLineWrapping"
#include "de/GuiRootWidget"
#include "de/GLTextComposer"
#include "de/Style"

#include <de/KeyEvent>
#include <de/MouseEvent>
#include <de/AnimationRule>
#include <de/Drawable>

#if defined (DENG_MOBILE)
#  include <QGuiApplication>
#  include <QInputMethod>
#endif

namespace de {

using namespace ui;

static TimeSpan const HEIGHT_ANIM_SPAN = .5f;
static duint const ID_BUF_TEXT   = 1;
static duint const ID_BUF_CURSOR = 2;

DENG_GUI_PIMPL(LineEditWidget)
{
    typedef GLBufferT<GuiVertex> VertexBuf;

    AnimationRule *height;
    FontLineWrapping &wraps;
    LabelWidget *hint;
    bool signalOnEnter;
    bool firstUpdateAfterCreation;

    // Style.
    ColorTheme colorTheme = Normal;
    Vector4f textColor;
    Font const *font;
    Time blinkTime;
    Animation hovering;
    float unfocusedBackgroundOpacity = 0.f;

    // GL objects.
    GLTextComposer composer;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uCursorColor;

    Impl(Public *i)
        : Base(i)
        , wraps(static_cast<FontLineWrapping &>(i->lineWraps()))
        , hint(0)
        , signalOnEnter(false)
        , firstUpdateAfterCreation(true)
        , font(0)
        , hovering(0, Animation::Linear)
        , uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4)
        , uColor      ("uColor",     GLUniform::Vec4)
        , uCursorColor("uColor",     GLUniform::Vec4)
    {
        height = new AnimationRule(0);

        self().set(Background(Vector4f(1, 1, 1, 1), Background::GradientFrame));
        self().setFont("editor.plaintext");
        updateStyle();
    }

    ~Impl()
    {
        releaseRef(height);
    }

    /**
     * Update the style used by the widget from the current UI style.
     */
    void updateStyle()
    {
        font = &self().font();
        textColor    = style().colors().colorf(colorTheme == Normal? "text" : "inverted.text");
        uCursorColor = style().colors().colorf(colorTheme == Normal? "text" : "inverted.text");

        updateBackground();

        // Update the line wrapper's font.
        wraps.setFont(*font);
        wraps.clear();
        composer.setWrapping(wraps);
        composer.forceUpdate();

        contentChanged(false);
    }

    int calculateHeight()
    {
        int const hgt = de::max(font->height().valuei(), wraps.totalHeightInPixels());
        return hgt + self().margins().height().valuei();
    }

    void updateProjection()
    {
        uMvpMatrix = root().projMatrix2D();
    }

    void updateBackground()
    {
        // If using a gradient frame, update parameters automatically.
        if (self().background().type == Background::GradientFrame)
        {
            Background bg;
            Vector3f const frameColor = style().colors().colorf(colorTheme == Normal? "text" : "inverted.text");
            if (!self().hasFocus())
            {
                bg = Background(Background::GradientFrame, Vector4f(frameColor, .15f + hovering * .2f), 6);
                if (unfocusedBackgroundOpacity > 0.f)
                {
                    bg.solidFill = Vector4f(style().colors().colorf(colorTheme == Normal? "background"
                                                                                        : "inverted.background").xyz(),
                                            unfocusedBackgroundOpacity);
                }
            }
            else
            {
                bg = Background(style().colors().colorf(colorTheme == Normal? "background"
                                                                            : "inverted.background"),
                                Background::GradientFrame,
                                Vector4f(frameColor, .25f + hovering * .3f), 6);
            }
            self().set(bg);
        }
    }

    void glInit()
    {
        composer.setAtlas(atlas());
        composer.setText(self().text());

        drawable.addBuffer(ID_BUF_TEXT, new VertexBuf);
        drawable.addBufferWithNewProgram(ID_BUF_CURSOR, new VertexBuf, "cursor");

        shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uColor
                << uAtlas();

        shaders().build(drawable.program("cursor"), "generic.color_ucolor")
                << uMvpMatrix
                << uCursorColor;

        updateProjection();
    }

    void glDeinit()
    {
        composer.release();
    }

    bool showingHint() const
    {
        return self().text().isEmpty() && !hint->text().isEmpty() && !self().hasFocus();
    }

    void updateGeometry()
    {
        updateBackground();

        if (composer.update()) self().requestGeometry();

        // Do we actually need to update geometry?
        Rectanglei pos;
        if (!self().hasChangedPlace(pos) && !self().geometryRequested())
        {
            return;
        }

        // Generate all geometry.
        self().requestGeometry(false);

        VertexBuf::Builder verts;
        self().glMakeGeometry(verts);
        drawable.buffer<VertexBuf>(ID_BUF_TEXT)
                .setVertices(gl::TriangleStrip, verts, gl::Static);

        // Cursor.
        Rectanglei const caret = self().cursorRect();

        verts.clear();
        verts.makeQuad(caret, Vector4f(1, 1, 1, 1),
                       atlas().imageRectf(self().root().solidWhitePixel()).middle());

        drawable.buffer<VertexBuf>(ID_BUF_CURSOR)
                .setVertices(gl::TriangleStrip, verts, gl::Static);
    }

    void updateHover(Vector2i const &pos)
    {
        if (/*!self().hasFocus() && */ self().hitTest(pos))
        {
            if (hovering.target() < 1)
            {
                hovering.setValue(1, .15f);
            }
        }
        else if (hovering.target() > 0)
        {
            hovering.setValue(0, .6f);
        }
    }

    void contentChanged(bool notify)
    {
        composer.setText(self().text());
        if (notify)
        {
            emit self().editorContentChanged();
        }
    }

    void atlasContentRepositioned(Atlas &)
    {
        self().requestGeometry();
    }
};

LineEditWidget::LineEditWidget(String const &name)
    : GuiWidget(name),
      AbstractLineEditor(new FontLineWrapping),
      d(new Impl(this))
{
    setBehavior(ContentClipping | Focusable);
    setAttribute(FocusHidden);

    // The widget's height is tied to the number of lines.
    rule().setInput(Rule::Height, *d->height);
}

void LineEditWidget::setText(String const &lineText)
{
    shell::AbstractLineEditor::setText(lineText);

    if (d->hint)
    {
        if (d->showingHint())
        {
            d->hint->setOpacity(1, .5);
        }
        else
        {
            d->hint->setOpacity(0);
        }
    }
}

void LineEditWidget::setEmptyContentHint(String const &hintText,
                                         String const &hintFont)
{
    if (!d->hint)
    {
        // A child widget will show the hint text.
        d->hint = new LabelWidget;
        d->hint->setTextColor("editor.hint");
        d->hint->setAlignment(ui::AlignLeft);
        d->hint->setBehavior(Unhittable | ContentClipping);
        d->hint->rule().setRect(rule());
        d->hint->setOpacity(1);
        add(d->hint);
    }
    d->hint->setFont(hintFont.isEmpty()? String("editor.hint.default") : hintFont);
    d->hint->setText(hintText);
}

void LineEditWidget::setSignalOnEnter(bool enterSignal)
{
    d->signalOnEnter = enterSignal;
}

Rectanglei LineEditWidget::cursorRect() const
{
    Vector2i const cursorPos = lineCursorPos();
    Vector2i const cp = d->wraps.charTopLeftInPixels(cursorPos.y, cursorPos.x) +
            contentRect().topLeft;

    return Rectanglei(cp + pointsToPixels(Vector2i(-1, 0)),
                      cp + Vector2i(pointsToPixels(1), d->font->height().valuei()));
}

void LineEditWidget::setColorTheme(ColorTheme theme)
{
    d->colorTheme = theme;
    d->updateStyle();
}

void LineEditWidget::setUnfocusedBackgroundOpacity(float opacity)
{
    d->unfocusedBackgroundOpacity = opacity;

    if (!hasFocus())
    {
        d->updateBackground();
    }
}

void LineEditWidget::glInit()
{
    LOG_AS("LineEditWidget");
    d->glInit();
}

void LineEditWidget::glDeinit()
{
    d->glDeinit();
}

void LineEditWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    GuiWidget::glMakeGeometry(verts);

    Rectanglei const contentRect = this->contentRect();
    Rectanglef const solidWhiteUv = d->atlas().imageRectf(root().solidWhitePixel());

    // Text lines.
    d->composer.makeVertices(verts, contentRect, AlignLeft, AlignLeft, d->textColor);

    // Underline the possible suggested completion.
    if (isSuggestingCompletion())
    {
        Rangei const   comp     = completionRange();
        Vector2i const startPos = linePos(comp.start);
        Vector2i const endPos   = linePos(comp.end);

        Vector2i const offset = contentRect.topLeft + Vector2i(0, d->font->ascent().valuei() + pointsToPixels(2));

        // It may span multiple lines.
        for (int i = startPos.y; i <= endPos.y; ++i)
        {
            Rangei const span = d->wraps.line(i).range;
            Vector2i start = d->wraps.charTopLeftInPixels(i, i == startPos.y? startPos.x : span.start) + offset;
            Vector2i end   = d->wraps.charTopLeftInPixels(i, i == endPos.y?   endPos.x   : span.end)   + offset;

            verts.makeQuad(Rectanglef(start, end + pointsToPixels(Vector2i(0, 1))),
                           Vector4f(1, 1, 1, 1), solidWhiteUv.middle());
        }
    }
}

void LineEditWidget::updateStyle()
{
    d->updateStyle();
}

void LineEditWidget::viewResized()
{
    GuiWidget::viewResized();

    updateLineWraps(RewrapNow);
    d->updateProjection();
}

void LineEditWidget::focusGained()
{
    d->contentChanged(false /* don't notify */);

    if (d->hint)
    {
        d->hint->setOpacity(0);
    }

#if defined (DENG_MOBILE)
    {
        auto &win = root().window();
        emit win.textEntryRequest();

        // Text entry happens via OS virtual keyboard.
        connect(&win, &GLWindow::userEnteredText, this, &LineEditWidget::userEnteredText);
        connect(&win, &GLWindow::userFinishedTextEntry, this, &LineEditWidget::userFinishedTextEntry);
    }
#endif
}

void LineEditWidget::focusLost()
{
#if defined (DENG_MOBILE)
    {
        auto &win = root().window();
        disconnect(&win, &GLWindow::userEnteredText, this, &LineEditWidget::userEnteredText);
        disconnect(&win, &GLWindow::userFinishedTextEntry, this, &LineEditWidget::userFinishedTextEntry);
        emit win.textEntryDismiss();
    }
#endif

    d->contentChanged(false /* don't notify */);

    if (d->hint && d->showingHint())
    {
        d->hint->setOpacity(1, 1, 0.5);
    }
}

#if defined (DENG_MOBILE)
void LineEditWidget::userEnteredText(QString text)
{
    setText(text);
}

void LineEditWidget::userFinishedTextEntry()
{
    root().popFocus();
}
#endif

void LineEditWidget::update()
{
    GuiWidget::update();

    d->updateBackground();

    // Rewrap content if necessary.
    updateLineWraps(WrapUnlessWrappedAlready);

    if (d->firstUpdateAfterCreation)
    {
        // Don't animate height immediately after creation.
        d->firstUpdateAfterCreation = false;
        d->height->finish();
    }
}

void LineEditWidget::drawContent()
{
    auto &painter = root().painter();
    painter.flush();

    GLState::push().setNormalizedScissor(painter.normalizedScissor());

    float const opac = visibleOpacity();
    d->uColor = Vector4f(1, 1, 1, opac); // Overall opacity.

    // Blink the cursor.
    Vector4f col = style().colors().colorf("editor.cursor");
    col.w *= (int(d->blinkTime.since() * 2) & 1? .25f : 1.f) * opac;
    if (!hasFocus())
    {
        col.w = 0;
    }
    d->uCursorColor = col;

    d->updateGeometry();
    d->drawable.draw();

    GLState::pop();
}

bool LineEditWidget::handleEvent(Event const &event)
{
    if (isDisabled()) return false;

    if (event.type() == Event::MousePosition)
    {
        d->updateHover(event.as<MouseEvent>().pos());
    }

    // Only handle clicks when not already focused.
    if (!hasFocus())
    {
        switch (handleMouseClick(event))
        {
        case MouseClickStarted:
            return true;

        case MouseClickFinished:
            root().setFocus(this);
            return true;

        default:
            break;
        }
    }

    if (is<KeyEvent>(event) && event.as<KeyEvent>().qtKey() == Qt::Key_Enter)
    {
        qDebug() << "LineEditWidget: Enter key" << event.type() << hasFocus();
    }

    // Only handle keys when focused.
    if (hasFocus() && event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if (key.isModifier())
        {
            // Don't eat modifier keys; the bindings system needs them.
            return false;
        }
        /*
        if (key.qtKey() == Qt::Key_Shift)
        {
            // Shift is not eaten so that Shift-Tilde can produce ~.
            // If we ate Shift, the bindings system would not realize it is down.
            return false;
        }

        if (key.qtKey() == Qt::Key_Control || key.qtKey() == Qt::Key_Alt ||
           key.qtKey() == Qt::Key_Meta)
        {
            // Modifier keys alone will be eaten when focused.
            return false;
        }*/

        if (d->signalOnEnter && (key.qtKey() == Qt::Key_Enter || key.qtKey() == Qt::Key_Return))
        {
            emit enterPressed(text());
            return true;
        }

        // Control character.
        if (handleControlKey(key.qtKey(), modifiersFromKeyEvent(key.modifiers())))
        {
            return true;
        }

        // Insert text?
        if (!key.text().isEmpty() && key.text()[0].isPrint())
        {
            // Insert some text into the editor.
            insert(key.text());
            return true;
        }
    }

    return GuiWidget::handleEvent(event);
}

shell::AbstractLineEditor::KeyModifiers LineEditWidget::modifiersFromKeyEvent(KeyEvent::Modifiers const &keyMods)
{
    KeyModifiers mods;

    if (keyMods.testFlag(KeyEvent::Shift))   mods |= Shift;
    if (keyMods.testFlag(KeyEvent::Control)) mods |= Control;
    if (keyMods.testFlag(KeyEvent::Alt))     mods |= Alt;
    if (keyMods.testFlag(KeyEvent::Meta))    mods |= Meta;

    return mods;
}

int LineEditWidget::maximumWidth() const
{
    return rule().recti().width() - margins().width().valuei();
}

void LineEditWidget::numberOfLinesChanged(int /*lineCount*/)
{
    // Changes in the widget's height are animated.
    d->height->set(d->calculateHeight(), HEIGHT_ANIM_SPAN);
}

void LineEditWidget::cursorMoved()
{
    requestGeometry();
    d->blinkTime = Time();
}

void LineEditWidget::contentChanged()
{
    d->contentChanged(true);

    if (hasRoot())
    {
        updateLineWraps(WrapUnlessWrappedAlready);
    }
}

void LineEditWidget::autoCompletionEnded(bool)
{
    // Make sure the underlining is removed.
    requestGeometry();
}

} // namespace de
