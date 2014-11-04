/** @file b_main.cpp  Event and device state bindings system.
 *
 * @todo Pretty much everything in this file belongs in InputSystem.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h" // stricmp macro, etc...
#include "ui/b_main.h"

#include <cctype>
#include <cmath>
#include <doomsday/console/exec.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include "dd_main.h"
#include "dd_def.h"
#include "clientapp.h"
#include "ui/b_command.h"
#include "ui/bindcontext.h"
#include "ui/p_control.h"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/ui_main.h"

using namespace de;

/**
 * Binding context fallback for the "global" context.
 *
 * @param ddev  Event being processed.
 *
 * @return @c true if the event was eaten and can be processed by the rest of the
 * binding context stack.
 */
static int globalContextFallback(ddevent_t const *ddev)
{
    if(App_GameLoaded())
    {
        event_t ev;
        if(InputSystem::convertEvent(ddev, &ev))
        {
            // The game's normal responder only returns true if the bindings can't
            // be used (like when chatting). Note that if the event is eaten here,
            // the rest of the bindings contexts won't get a chance to process the
            // event.
            if(gx.Responder && gx.Responder(&ev))
            {
                return true;
            }
        }
    }

    return false;
}

/// @note Called once on init.
void B_Init()
{
    InputSystem &isys = ClientApp::inputSystem();

    // In dedicated mode we have fewer binding contexts available.

    // The contexts are defined in reverse order, with the context of lowest
    // priority defined first.

    isys.newContext(DEFAULT_BINDING_CONTEXT_NAME);

    // Game contexts.
    /// @todo Game binding context setup obviously belong to the game plugin, so shouldn't be here.
    isys.newContext("map");
    isys.newContext("map-freepan");
    isys.newContext("finale"); // uses a fallback responder to handle script events
    isys.newContext("menu")->acquireAll();
    isys.newContext("gameui");
    isys.newContext("shortcut");
    isys.newContext("chat")->acquireKeyboard();
    isys.newContext("message")->acquireAll();

    // Binding context for the console.
    BindContext *bc = isys.newContext(CONSOLE_BINDING_CONTEXT_NAME);
    bc->protect();         // Only we can (de)activate.
    bc->acquireKeyboard(); // Console takes over all keyboard events.

    // UI doesn't let anything past it.
    isys.newContext(UI_BINDING_CONTEXT_NAME)->acquireAll();

    // Top-level context that is always active and overrides every other context.
    // To be used only for system-level functionality.
    bc = isys.newContext(GLOBAL_BINDING_CONTEXT_NAME);
    bc->protect();
    bc->setDDFallbackResponder(globalContextFallback);
    bc->activate();

/*
    B_BindCommand("joy-hat-angle3", "print {angle 3}");
    B_BindCommand("joy-hat-center", "print center");

    B_BindCommand("game:key-m-press", "print hello");
    B_BindCommand("key-codex20-up", "print {space released}");
    B_BindCommand("key-up-down + key-shift + key-ctrl", "print \"shifted and controlled up\"");
    B_BindCommand("key-up-down + key-shift", "print \"shifted up\"");
    B_BindCommand("mouse-left", "print mbpress");
    B_BindCommand("mouse-right-up", "print \"right mb released\"");
    B_BindCommand("joy-x-neg1.0 + key-ctrl-up", "print \"joy x negative without ctrl\"");
    B_BindCommand("joy-x- within 0.1 + joy-y-pos1", "print \"joy x centered\"");
    B_BindCommand("joy-x-pos1.0", "print \"joy x positive\"");
    B_BindCommand("joy-x-neg1.0", "print \"joy x negative\"");
    B_BindCommand("joy-z-pos1.0", "print \"joy z positive\"");
    B_BindCommand("joy-z-neg1.0", "print \"joy z negative\"");
    B_BindCommand("joy-w-pos1.0", "print \"joy w positive\"");
    B_BindCommand("joy-w-neg1.0", "print \"joy w negative\"");
    */

    /*B_BindControl("turn", "key-left-staged-inverse");
    B_BindControl("turn", "key-right-staged");
    B_BindControl("turn", "mouse-x");
    B_BindControl("turn", "joy-x + key-shift-up + joy-hat-center + key-code123-down");
    */

    // Bind all the defaults for the engine only.
    B_BindDefaults();

    isys.initialContextActivations();
}

void B_BindDefaults()
{
    InputSystem &isys = ClientApp::inputSystem();

    // Engine's highest priority context: opening control panel, opening the console.
    isys.bindCommand("global:key-f11-down + key-alt-down", "releasemouse");
    isys.bindCommand("global:key-f11-down", "togglefullscreen");
    isys.bindCommand("global:key-tilde-down + key-shift-up", "taskbar");

    // Console bindings (when open).
    isys.bindCommand("console:key-tilde-down + key-shift-up", "taskbar"); // without this, key would be entered into command line

    // Bias editor.
}

void B_BindGameDefaults()
{
    if(!App_GameLoaded()) return;
    Con_Executef(CMDS_DDAY, false, "defaultgamebindings");
}

dbinding_t *B_GetControlBindings(int localNum, int control, BindContext **bContext)
{
    if(localNum < 0 || localNum >= DDMAXPLAYERS)
        return nullptr;

    playercontrol_t *pc = P_PlayerControlById(control);
    BindContext *bc     = ClientApp::inputSystem().contextPtr(pc->bindContextName);

    if(bContext) *bContext = bc;

    if(bc)
    {
        return &bc->getControlBinding(control)->deviceBinds[localNum];
    }

    return nullptr;
}
