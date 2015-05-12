/** @file st_stuff.cpp  Heretic specific HUD and statusbar widgets.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#if defined(WIN32) && defined(_MSC_VER)
// Something in here is incompatible with MSVC 2010 optimization.
// Symptom: automap not visible.
#  pragma optimize("", off)
#  pragma warning(disable : 4748)
#endif

#include "jheretic.h"
#include "st_stuff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "am_map.h"
#include "d_net.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "hu_stuff.h"
#include "p_mapsetup.h"
#include "p_tick.h" // for Pause_IsPaused
#include "player.h"
#include "hu_lib.h"
#include "hu_automap.h"
#include "hu_chat.h"
#include "hu_log.h"
#include "hu_inventory.h"
#include "p_inventory.h"
#include "r_common.h"

using namespace de;

// Current ammo icon(sbbar).
#define ST_AMMOIMGWIDTH     (24)
#define ST_AMMOICONX        (111)
#define ST_AMMOICONY        (14)

// Inventory.
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (2)

// Current inventory item.
#define ST_INVITEMX         (179)
#define ST_INVITEMY         (2)

// Current inventory item count.
#define ST_INVITEMCWIDTH    (2) // Num digits
#define ST_INVITEMCX        (208)
#define ST_INVITEMCY        (24)

// AMMO number pos.
#define ST_AMMOWIDTH        (3)
#define ST_AMMOX            (135)
#define ST_AMMOY            (4)

// ARMOR number pos.
#define ST_ARMORWIDTH       (3)
#define ST_ARMORX           (254)
#define ST_ARMORY           (12)

// HEALTH number pos.
#define ST_HEALTHWIDTH      (3)
#define ST_HEALTHX          (85)
#define ST_HEALTHY          (12)

// Key icon positions.
#define ST_KEY0WIDTH        (10)
#define ST_KEY0HEIGHT       (6)
#define ST_KEY0X            (153)
#define ST_KEY0Y            (6)
#define ST_KEY1WIDTH        (ST_KEY0WIDTH)
#define ST_KEY1X            (153)
#define ST_KEY1Y            (14)
#define ST_KEY2WIDTH        (ST_KEY0WIDTH)
#define ST_KEY2X            (153)
#define ST_KEY2Y            (22)

// Frags pos.
#define ST_FRAGSX           (85)
#define ST_FRAGSY           (13)
#define ST_FRAGSWIDTH       (2)

enum {
    UWG_STATUSBAR,
    UWG_MAPNAME,
    UWG_TOPLEFT,
    UWG_TOPCENTER,
    UWG_TOPRIGHT,
    UWG_TOP,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMLEFT2,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOMCENTER,
    UWG_BOTTOM,
    UWG_COUNTERS,
    UWG_AUTOMAP,
    NUM_UIWIDGET_GROUPS
};

struct hudstate_t
{
    dd_bool inited;
    dd_bool stopped;
    int hideTics;
    float hideAmount;
    float alpha; // Fullscreen hud alpha value.
    float showBar; // Slide statusbar amount 1.0 is fully open.
    dd_bool statusbarActive; // Whether main statusbar is active.
    int automapCheatLevel; /// \todo Belongs in player state?
    int readyItemFlashCounter;

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];
    int automapWidgetId;
    int chatWidgetId;
    int logWidgetId;

    // Statusbar:
    guidata_health_t sbarHealth;
    guidata_armor_t sbarArmor;
    guidata_frags_t sbarFrags;
    guidata_chain_t sbarChain;
    guidata_keyslot_t sbarKeyslots[3];
    guidata_readyitem_t sbarReadyitem;
    guidata_readyammo_t sbarReadyammo;
    guidata_readyammoicon_t sbarReadyammoicon;

    // Fullscreen:
    guidata_health_t health;
    guidata_armor_t armor;
    guidata_keys_t keys;
    guidata_readyammo_t readyammo;
    guidata_readyammoicon_t readyammoicon;
    guidata_frags_t frags;
    guidata_readyitem_t readyitem;

    // Other:
    guidata_automap_t automap;
    guidata_chat_t chat;
    guidata_log_t log;
    guidata_flight_t flight;
    guidata_tomeofpower_t tome;
    guidata_secrets_t secrets;
    guidata_items_t items;
    guidata_kills_t kills;
};

int ST_ChatResponder(int player, event_t *ev);

static hudstate_t hudStates[MAXPLAYERS];

static patchid_t pStatusbar;
static patchid_t pStatusbarTopLeft;
static patchid_t pStatusbarTopRight;
static patchid_t pChain;
static patchid_t pStatBar;
static patchid_t pLifeBar;
static patchid_t pInvBar;
static patchid_t pLifeGems[4];
static patchid_t pAmmoIcons[11];
static patchid_t pInvItemFlash[5];
static patchid_t pSpinTome[16];
static patchid_t pSpinFly[16];
static patchid_t pKeys[NUM_KEY_TYPES];
static patchid_t pGodLeft;
static patchid_t pGodRight;

static int headupDisplayMode(int /*player*/)
{
    return (cfg.common.screenBlocks < 10? 0 : cfg.common.screenBlocks - 10);
}

static void shadeChain(int x, int y, float alpha)
{
    DGL_Begin(DGL_QUADS);
        // Left shadow.
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(x+20, y+ST_HEIGHT);
        DGL_Vertex2f(x+20, y+ST_HEIGHT-10);
        DGL_Color4f(0, 0, 0, 0);
        DGL_Vertex2f(x+35, y+ST_HEIGHT-10);
        DGL_Vertex2f(x+35, y+ST_HEIGHT);

        // Right shadow.
        DGL_Vertex2f(x+ST_WIDTH-43, y+ST_HEIGHT);
        DGL_Vertex2f(x+ST_WIDTH-43, y+ST_HEIGHT-10);
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(x+ST_WIDTH-27, y+ST_HEIGHT-10);
        DGL_Vertex2f(x+ST_WIDTH-27, y+ST_HEIGHT);
    DGL_End();
}

void SBarChain_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_chain_t *chain = (guidata_chain_t *)wi->typedata;
    player_t const *plr    = &players[wi->player];
    int const curHealth    = de::max(plr->plr->mo->health, 0);

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    // Health marker chain animates up to the actual health value.
    int delta;
    if(curHealth < chain->healthMarker)
    {
        delta = de::clamp(1, (chain->healthMarker - curHealth) >> 2, 4);
        chain->healthMarker -= delta;
    }
    else if(curHealth > chain->healthMarker)
    {
        delta = de::clamp(1, (curHealth - chain->healthMarker) >> 2, 4);
        chain->healthMarker += delta;
    }

    if(chain->healthMarker != curHealth && (mapTime & 1))
    {
        chain->wiggle = P_Random() & 1;
    }
    else
    {
        chain->wiggle = 0;
    }
}

void SBarChain_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (0)

    static int const theirColors[] = {
        220, // Green.
        144, // Yellow.
        150, // Red.
        197  // Blue.
    };
    guidata_chain_t *chain = (guidata_chain_t *)wi->typedata;
    int chainY;
    float healthPos, gemXOffset, gemglow;
    int x, y, w, h, gemNum;
    float /*cw,*/ rgb[3];
    hudstate_t const *hud = &hudStates[wi->player];
    int chainYOffset = ST_HEIGHT * (1 - hud->showBar);
    int fullscreen = headupDisplayMode(wi->player);
    //float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    patchinfo_t pChainInfo, pGemInfo;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pChain, &pChainInfo)) return;

    if(!IS_NETGAME)
        gemNum = 2; // Always use the red gem in single player.
    else
        gemNum = cfg.playerColor[wi->player];

    if(!R_GetPatchInfo(pLifeGems[gemNum], &pGemInfo)) return;

    chainY = -9 + chain->wiggle;
    healthPos = de::clamp(0.f, chain->healthMarker / 100.f, 1.f);
    gemglow = healthPos;

    // Draw the chain.
    x = ORIGINX + 21;
    y = ORIGINY + chainY;
    w = ST_WIDTH - 21 - 28;
    h = 8;
    //cw = (float) w / pChainInfo.geometry.size.width;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, chainYOffset, 0);

    DGL_SetPatch(pChain, DGL_REPEAT, DGL_CLAMP);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = (w - pGemInfo.geometry.size.width) * healthPos;

    if(gemXOffset > 0)
    {
        // Left chain section.
        float cw = gemXOffset / pChainInfo.geometry.size.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 1 - cw, 0);
            DGL_Vertex2f(x, y);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + gemXOffset, y);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(x + gemXOffset, y + h);

            DGL_TexCoord2f(0, 1 - cw, 1);
            DGL_Vertex2f(x, y + h);
        DGL_End();
    }

    if(gemXOffset + pGemInfo.geometry.size.width < w)
    {
        // Right chain section.
        float cw = (w - gemXOffset - pGemInfo.geometry.size.width) / pChainInfo.geometry.size.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + pGemInfo.geometry.size.width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + pGemInfo.geometry.size.width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(pGemInfo.id, Vector2i(x + gemXOffset, chainY));

    DGL_Disable(DGL_TEXTURE_2D);

    shadeChain(ORIGINX, ORIGINY-ST_HEIGHT, iconAlpha/2);

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));
    DGL_Enable(DGL_TEXTURE_2D);

    R_GetColorPaletteRGBf(0, theirColors[gemNum], rgb, false);
    DGL_DrawRectf2Color(x + gemXOffset - 11, chainY - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINX
#undef ORIGINY
}

void SBarChain_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    /// @todo Calculate dimensions properly.
    Rect_SetWidthHeight(wi->geometry, (ST_WIDTH - 21 - 28) * cfg.common.statusbarScale,
                                       8 * cfg.common.statusbarScale);
}

/**
 * Draws the whole statusbar backgound.
 * @todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void SBarBackground_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     int(-WIDTH / 2)
#define ORIGINY     int(-HEIGHT * hud->showBar)

    hudstate_t const *hud = &hudStates[wi->player];
    int fullscreen = headupDisplayMode(wi->player);
    //float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarOpacity);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarOpacity);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    if(!(iconAlpha < 1))
    {
        // We can just render the full thing as normal.
        // Top bits.
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusbarTopLeft,  Vector2i(ORIGINX      , ORIGINY - 10));
        GL_DrawPatch(pStatusbarTopRight, Vector2i(ORIGINX + 290, ORIGINY - 10));

        // Faces.
        GL_DrawPatch(pStatusbar, Vector2i(ORIGINX, ORIGINY));

        if(P_GetPlayerCheats(&players[wi->player]) & CF_GODMODE)
        {
            GL_DrawPatch(pGodLeft,  Vector2i(ORIGINX + 16 , ORIGINY + 9));
            GL_DrawPatch(pGodRight, Vector2i(ORIGINX + 287, ORIGINY + 9));
        }

        patchid_t panel = Hu_InventoryIsOpen(wi->player)? pInvBar
                        : G_Ruleset_Deathmatch()        ? pStatBar : pLifeBar;
        GL_DrawPatch(panel, Vector2i(ORIGINX + 34, ORIGINY + 2));

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);

        // Top bits.
        GL_DrawPatch(pStatusbarTopLeft,  Vector2i(ORIGINX      , ORIGINY - 10));
        GL_DrawPatch(pStatusbarTopRight, Vector2i(ORIGINX + 290, ORIGINY - 10));

        DGL_SetPatch(pStatusbar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        // Top border.
        DGL_DrawCutRectf2Tiled(ORIGINX+34, ORIGINY, 248, 2, 320, 42, 34, 0, ORIGINX, ORIGINY, 0, 0);

        // Chain background.
        DGL_DrawCutRectf2Tiled(ORIGINX+34, ORIGINY+33, 248, 9, 320, 42, 34, 33, ORIGINX, ORIGINY+191, 16, 8);

        // Faces.
        if(P_GetPlayerCheats(&players[wi->player]) & CF_GODMODE)
        {
            // If GOD mode we need to cut windows
            DGL_DrawCutRectf2Tiled(ORIGINX, ORIGINY, 34, 42, 320, 42, 0, 0, ORIGINX+16, ORIGINY+9, 16, 8);
            DGL_DrawCutRectf2Tiled(ORIGINX+282, ORIGINY, 38, 42, 320, 42, 282, 0, ORIGINX+287, ORIGINY+9, 16, 8);

            GL_DrawPatch(pGodLeft,  Vector2i(ORIGINX + 16 , ORIGINY + 9));
            GL_DrawPatch(pGodRight, Vector2i(ORIGINX + 287, ORIGINY + 9));
        }
        else
        {
            DGL_DrawCutRectf2Tiled(ORIGINX, ORIGINY, 34, 42, 320, 42, 0, 0, ORIGINX, ORIGINY, 0, 0);
            DGL_DrawCutRectf2Tiled(ORIGINX+282, ORIGINY, 38, 42, 320, 42, 282, 0, ORIGINX, ORIGINY, 0, 0);
        }

        patchid_t panel = Hu_InventoryIsOpen(wi->player)? pInvBar
                        : G_Ruleset_Deathmatch()        ? pStatBar : pLifeBar;
        GL_DrawPatch(panel, Vector2i(ORIGINX + 34, ORIGINY + 2));

        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void SBarBackground_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(wi->geometry, ST_WIDTH  * cfg.common.statusbarScale,
                                       ST_HEIGHT * cfg.common.statusbarScale);
}

int ST_Responder(event_t *ev)
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(int result = ST_ChatResponder(i, ev))
            return result;
    }
    return false; // Not eaten.
}

void ST_Ticker(timespan_t ticLength)
{
    dd_bool const isSharpTic = DD_IsSharpTick();
    if(isSharpTic)
    {
        Hu_InventoryTicker();
    }

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr   = &players[i];
        hudstate_t *hud = &hudStates[i];

        if(!plr->plr->inGame)
            continue;

        // Either slide the status bar in or fade out the fullscreen HUD.
        if(hud->statusbarActive)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha -= 0.1f;
            }
            else if(hud->showBar < 1.0f)
            {
                hud->showBar += 0.1f;
            }
        }
        else
        {
            if(cfg.common.screenBlocks == 13)
            {
                if(hud->alpha > 0.0f)
                {
                    hud->alpha -= 0.1f;
                }
            }
            else
            {
                if(hud->showBar > 0.0f)
                {
                    hud->showBar -= 0.1f;
                }
                else if(hud->alpha < 1.0f)
                {
                    hud->alpha += 0.1f;
                }
            }
        }

        // The following is restricted to fixed 35 Hz ticks.
        if(isSharpTic && !Pause_IsPaused())
        {
            if(cfg.common.hudTimer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && cfg.common.hudTimer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            if(hud->readyItemFlashCounter > 0)
                --hud->readyItemFlashCounter;
        }

        if(hud->inited)
        {
            for(int k = 0; k < NUM_UIWIDGET_GROUPS; ++k)
            {
                UIWidget_RunTic(GUI_MustFindObjectById(hud->widgetGroupIds[k]), ticLength);
            }
        }
    }
}

void SBarInventory_Drawer(uiwidget_t *obj, Point2Raw const *offset)
{
    hudstate_t const *hud = &hudStates[obj->player];
    int const yOffset     = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen  = headupDisplayMode(obj->player);
    //float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(!Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    Hu_InventoryDraw2(obj->player, -ST_WIDTH/2 + ST_INVENTORYX, -ST_HEIGHT + yOffset + ST_INVENTORYY, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarInventory_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    // @todo calculate dimensions properly!
    Rect_SetWidthHeight(wi->geometry, (ST_WIDTH - (43 * 2)) * cfg.common.statusbarScale,
                                       41 * cfg.common.statusbarScale);
}

void Frags_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;
    player_t const *plr    = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    frags->value = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            frags->value += plr->frags[i] * (i != wi->player ? 1 : -1);
        }
    }
}

void SBarFrags_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_FRAGSX)
#define Y                   (ORIGINY + ST_FRAGSY)
#define MAXDIGITS           (ST_FRAGSWIDTH)
#define TRACKING            (1)

    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;
    hudstate_t const *hud  = &hudStates[wi->player];
    int fullscreen         = headupDisplayMode(wi->player);
    float const textAlpha  = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    int yOffset            = ST_HEIGHT * (1 - hud->showBar);

    if(!G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarFrags_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                      textSize.height * cfg.common.statusbarScale);

#undef TRACKING
}

void Health_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    player_t const *plr    = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;
    hlth->value = plr->health;
}

void SBarHealth_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_HEALTHX)
#define Y                   (ORIGINY + ST_HEALTHY)
#define MAXDIGITS           (ST_HEALTHWIDTH)
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    hudstate_t const *hud  = &hudStates[wi->player];
    int const yOffset      = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen   = headupDisplayMode(wi->player);
    float const textAlpha  = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarHealth_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                       textSize.height * cfg.common.statusbarScale);
#undef TRACKING
}

void Armor_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;
    player_t const *plr    = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;
    armor->value = plr->armorPoints;
}

void SBarArmor_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_ARMORX)
#define Y                   (ORIGINY + ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;
    hudstate_t const *hud  = &hudStates[wi->player];
    int const yOffset      = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen   = headupDisplayMode(wi->player);
    float const textAlpha  = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    //float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarArmor_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                       textSize.height * cfg.common.statusbarScale);

#undef TRACKING
}

void KeySlot_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_keyslot_t *kslt = (guidata_keyslot_t *)wi->typedata;
    player_t const *plr     = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;
    kslt->patchId = plr->keys[kslt->keytypeA] ? pKeys[kslt->keytypeA] : 0;
}

void KeySlot_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX (-ST_WIDTH / 2)
#define ORIGINY (-ST_HEIGHT)

    static Vector2i const elements[] = {
        { ORIGINX + ST_KEY0X, ORIGINY + ST_KEY0Y },
        { ORIGINX + ST_KEY1X, ORIGINY + ST_KEY1Y },
        { ORIGINX + ST_KEY2X, ORIGINY + ST_KEY2Y }
    };
    guidata_keyslot_t *kslt = (guidata_keyslot_t *)wi->typedata;
    Vector2i const *loc     = &elements[kslt->keytypeA];
    hudstate_t const *hud   = &hudStates[wi->player];
    int const yOffset       = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen    = headupDisplayMode(wi->player);
    float const iconAlpha   = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(kslt->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconAlpha);

    GL_DrawPatch(kslt->patchId, *loc);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void KeySlot_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_keyslot_t *kslt = (guidata_keyslot_t *)wi->typedata;
    patchinfo_t info;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(kslt->patchId == 0 || !R_GetPatchInfo(kslt->patchId, &info)) return;

    Rect_SetWidthHeight(wi->geometry, info.geometry.size.width  * cfg.common.statusbarScale,
                                      info.geometry.size.height * cfg.common.statusbarScale);
}

void ReadyAmmo_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_readyammo_t *ammo = (guidata_readyammo_t *)wi->typedata;
    player_t const *plr       = &players[wi->player];
    int const lvl             = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    ammo->value = 1994; // Means n/a.
    if(!(plr->readyWeapon > 0 && plr->readyWeapon < 7)) return;

    for(int i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        // Does the weapon use this type of ammo?
        if(weaponInfo[plr->readyWeapon][plr->class_].mode[lvl].ammoType[i])
        {
            ammo->value = plr->ammo[i].owned;
            break;
        }
    }
}

void SBarReadyAmmo_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_AMMOX)
#define Y                   (ORIGINY + ST_AMMOY)
#define MAXDIGITS           (ST_AMMOWIDTH)
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_readyammo_t *ammo = (guidata_readyammo_t *)wi->typedata;
    hudstate_t const *hud     = &hudStates[wi->player];
    int const yOffset         = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen      = headupDisplayMode(wi->player);
    //float const textAlpha     = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha     = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], iconAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmo_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING            (1)

    DENG2_ASSERT(wi);
    guidata_readyammo_t *ammo = (guidata_readyammo_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                      textSize.height * cfg.common.statusbarScale);

#undef TRACKING
}

void ReadyAmmoIcon_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_readyammoicon_t *icon = (guidata_readyammoicon_t *)wi->typedata;
    player_t const *plr = &players[wi->player];
    int const lvl       = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;
    icon->patchId = 0;
    if(!(plr->readyWeapon > 0 && plr->readyWeapon < 7)) return;

    for(int i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        // Does the weapon use this type of ammo?
        if(weaponInfo[plr->readyWeapon][plr->class_].mode[lvl].ammoType[i])
        {
            icon->patchId = pAmmoIcons[i];
            break;
        }
    }
}

void SBarReadyAmmoIcon_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_AMMOICONX)
#define Y                   (ORIGINY + ST_AMMOICONY)

    DENG2_ASSERT(wi);
    guidata_readyammoicon_t *icon = (guidata_readyammoicon_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT*(1-hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    //float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(icon->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconAlpha);

    GL_DrawPatch(icon->patchId, Vector2i(X, Y));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmoIcon_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_readyammoicon_t *icon = (guidata_readyammoicon_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(icon->patchId == 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(icon->patchId, &info)) return;

    Rect_SetWidthHeight(wi->geometry, info.geometry.size.width  * cfg.common.statusbarScale,
                                      info.geometry.size.height * cfg.common.statusbarScale);
}

void ReadyItem_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    int const flashCounter    = hudStates[wi->player].readyItemFlashCounter;

    if(flashCounter > 0)
    {
        item->patchId = pInvItemFlash[flashCounter % 5];
    }
    else
    {
        inventoryitemtype_t readyItem = P_InventoryReadyItem(wi->player);
        if(readyItem != IIT_NONE)
        {
            item->patchId = P_GetInvItem(readyItem-1)->patchId;
        }
        else
        {
            item->patchId = 0;
        }
    }
}

void SBarReadyItem_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX (-ST_WIDTH / 2)
#define ORIGINY (-ST_HEIGHT * hud->showBar)
#define TRACKING (2)

    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const fullscreen  = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    inventoryitemtype_t readyItem;

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(item->patchId != 0)
    {
        int x, y;
        if(hud->readyItemFlashCounter > 0)
        {
            x = ORIGINX+ST_INVITEMX + 2;
            y = ORIGINY+ST_INVITEMY + 1;
        }
        else
        {
            x = ORIGINX+ST_INVITEMX;
            y = ORIGINY+ST_INVITEMY;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(item->patchId, Vector2i(x, y));

        readyItem = P_InventoryReadyItem(wi->player);
        if(!(hud->readyItemFlashCounter > 0) && IIT_NONE != readyItem)
        {
            uint count = P_InventoryCount(wi->player, readyItem);
            if(count > 1)
            {
                char buf[20];
                dd_snprintf(buf, 20, "%i", count);

                FR_SetFont(wi->font);
                FR_SetTracking(TRACKING);
                FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
                FR_DrawTextXY3(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

#undef TRACKING
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyItem_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId == 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(item->patchId, &info)) return;

    // @todo Calculate dimensions properly!
    Rect_SetWidthHeight(wi->geometry, info.geometry.size.width  * cfg.common.statusbarScale,
                                      info.geometry.size.height * cfg.common.statusbarScale);
}

void ST_FlashCurrentItem(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    player_t *plr = &players[player];

    if(!(/*(plr->plr->flags & DDPF_LOCAL) &&*/ plr->plr->inGame)) return;

    hudstate_t *hud = &hudStates[player];
    hud->readyItemFlashCounter = 4;
}

void ST_HUDUnHide(int player, hueevent_t ev)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    player_t *plr = &players[player];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.common.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

void Flight_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_flight_t *flht = (guidata_flight_t *)wi->typedata;
    player_t const *plr    = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    flht->patchId = 0;
    if(plr->powers[PT_FLIGHT] <= 0) return;

    if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD || !(plr->powers[PT_FLIGHT] & 16))
    {
        int frame = (mapTime / 3) & 15;
        if(plr->plr->mo->flags2 & MF2_FLY)
        {
            if(flht->hitCenterFrame && (frame != 15 && frame != 0))
                frame = 15;
            else
                flht->hitCenterFrame = false;
        }
        else
        {
            if(!flht->hitCenterFrame && (frame != 15 && frame != 0))
            {
                flht->hitCenterFrame = false;
            }
            else
            {
                frame = 15;
                flht->hitCenterFrame = true;
            }
        }
        flht->patchId = pSpinFly[frame];
    }
}

void Flight_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_flight_t *flht = (guidata_flight_t *)wi->typedata;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(flht->patchId)
    {
        float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(flht->patchId, Vector2i(16, 14));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void Flight_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    //guidata_flight_t *flht = (guidata_flight_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(plr->powers[PT_FLIGHT] <= 0) return;

    /// @todo Calculate dimensions properly!
    Rect_SetWidthHeight(wi->geometry, 32 * cfg.common.hudScale, 32 * cfg.common.hudScale);
}

void Tome_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_tomeofpower_t *tome = (guidata_tomeofpower_t *)wi->typedata;
    player_t const *plr  = &players[wi->player];
    int const ticsRemain = plr->powers[PT_WEAPONLEVEL2];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    tome->patchId = 0;
    tome->countdownSeconds = 0;

    if(ticsRemain <= 0 || 0 != plr->morphTics) return;

    // Time to player the countdown sound?
    if(ticsRemain && ticsRemain < cfg.tomeSound * TICSPERSEC)
    {
        int timeleft = ticsRemain / TICSPERSEC;
        if(tome->play != timeleft)
        {
            tome->play = timeleft;
            S_LocalSound(SFX_KEYUP, NULL);
        }
    }

    if(cfg.tomeCounter > 0 || ticsRemain > BLINKTHRESHOLD || !(ticsRemain & 16))
    {
        int frame = (mapTime / 3) & 15;
        tome->patchId = pSpinTome[frame];
    }

    if(cfg.tomeCounter > 0 && ticsRemain < cfg.tomeCounter * TICSPERSEC)
    {
        tome->countdownSeconds = 1 + ticsRemain / TICSPERSEC;
    }
}

void Tome_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_tomeofpower_t *tome = (guidata_tomeofpower_t*)wi->typedata;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(tome->patchId == 0 && tome->countdownSeconds == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    if(tome->patchId != 0)
    {
        float alpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;
        if(tome->countdownSeconds != 0)
            alpha *= tome->countdownSeconds / (float)cfg.tomeCounter;

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, alpha);
        GL_DrawPatch(tome->patchId, Vector2i(13, 13));

        DGL_Disable(DGL_TEXTURE_2D);
    }

    if(tome->countdownSeconds != 0)
    {
#define COUNT_X             (303)
#define COUNT_Y             (30)
#define TRACKING            (2)

        float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
        char buf[20];
        dd_snprintf(buf, 20, "%i", tome->countdownSeconds);

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(wi->font);
        FR_SetTracking(TRACKING);
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
        FR_DrawTextXY2(buf, 26, 26 - 2, ALIGN_BOTTOMRIGHT);

        DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
#undef COUNT_Y
#undef COUNT_X
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Tome_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_tomeofpower_t *tome = (guidata_tomeofpower_t *)wi->typedata;
    player_t const *plr  = &players[wi->player];
    int const ticsRemain = plr->powers[PT_WEAPONLEVEL2];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ticsRemain <= 0 || 0 != plr->morphTics) return;

    if(tome->patchId != 0)
    {
        // @todo Determine the actual center point of the animation at widget creation time.
        Rect_SetWidthHeight(wi->geometry, 26 * cfg.common.hudScale, 26 * cfg.common.hudScale);
    }
    else
    {
#define TRACKING            (2)

        Size2Raw textSize;
        char buf[20];
        dd_snprintf(buf, 20, "%i", tome->countdownSeconds);

        FR_SetFont(wi->font);
        FR_SetTracking(TRACKING);
        FR_TextSize(&textSize, buf);

        Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                          textSize.height * cfg.common.hudScale);
#undef TRACKING
    }
}

void ReadyAmmoIcon_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_readyammoicon_t *icon = (guidata_readyammoicon_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(hud->statusbarActive) return;
    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(icon->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(icon->patchId, Vector2i(0, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ReadyAmmoIcon_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_readyammoicon_t *icon = (guidata_readyammoicon_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(hud->statusbarActive) return;
    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(icon->patchId == 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(icon->patchId, &info)) return;

    Rect_SetWidthHeight(wi->geometry, info.geometry.size.width  * cfg.common.hudScale,
                                      info.geometry.size.height * cfg.common.hudScale);
}

void ReadyAmmo_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_readyammo_t *ammo = (guidata_readyammo_t *)wi->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    hudstate_t const *hud = &hudStates[wi->player];

    if(hud->statusbarActive) return;
    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY(buf, 0, -2);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void ReadyAmmo_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_readyammo_t *ammo = (guidata_readyammo_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(hud->statusbarActive) return;
    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void Health_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    int const health       = de::max(hlth->value, 0);
    float const textAlpha  = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    char buf[20]; dd_snprintf(buf, 5, "%i", health);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(0, 0, 0, textAlpha * .4f);
    FR_DrawTextXY(buf, 2, 1);

    FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, -1);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Health_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    int const health = de::max(hlth->value, 0);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    char buf[20]; dd_snprintf(buf, 5, "%i", health);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height *= cfg.common.hudScale);

#undef TRACKING
}

void Armor_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;
    float const textAlpha  = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY(buf, 0, -2); /// \todo Why is an offset needed?

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Armor_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", armor->value);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void Keys_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_keys_t *keys  = (guidata_keys_t *)wi->typedata;
    float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;
    patchinfo_t pInfo;

    if(!cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    int x = 0;
    if(keys->keyBoxes[KT_YELLOW])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pKeys[0], Vector2i(x, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        if(R_GetPatchInfo(pKeys[0], &pInfo))
            x += pInfo.geometry.size.width + 1;
    }

    if(keys->keyBoxes[KT_GREEN])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pKeys[1], Vector2i(x, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        if(R_GetPatchInfo(pKeys[1], &pInfo))
            x += pInfo.geometry.size.width + 1;
    }

    if(keys->keyBoxes[KT_BLUE])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pKeys[2], Vector2i(x, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Keys_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_keys_t *keys = (guidata_keys_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    for(int i = 0; i < 3; ++i)
    {
        keys->keyBoxes[i] = plr->keys[i] ? true : false;
    }
}

void Keys_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_keys_t *keys = (guidata_keys_t *)wi->typedata;
    int x = 0;// numVisible = 0;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(keys->keyBoxes[KT_YELLOW] && R_GetPatchInfo(pKeys[0], &pInfo))
    {
        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(wi->geometry, &pInfo.geometry);

        x += pInfo.geometry.size.width + 1;
    }

    if(keys->keyBoxes[KT_GREEN] && R_GetPatchInfo(pKeys[1], &pInfo))
    {
        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(wi->geometry, &pInfo.geometry);

        x += pInfo.geometry.size.width + 1;
    }

    if(keys->keyBoxes[KT_BLUE] && R_GetPatchInfo(pKeys[2], &pInfo))
    {
        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(wi->geometry, &pInfo.geometry);
    }

    Rect_SetWidthHeight(wi->geometry, Rect_Width(wi->geometry)  * cfg.common.hudScale,
                                      Rect_Height(wi->geometry) * cfg.common.hudScale);
}

void Frags_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;
    float const textAlpha  = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!G_Ruleset_Deathmatch()) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Frags_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!G_Ruleset_Deathmatch()) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", frags->value);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                      textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void ReadyItem_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define TRACKING                (2)

    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t boxInfo;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    if(item->patchId != 0)
    {
        int xOffset = 0, yOffset = 0;
        if(hud->readyItemFlashCounter > 0)
        {
            xOffset += 2;
            yOffset += 1;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha/2);
        GL_DrawPatch(pInvItemBox, Vector2i(0, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(item->patchId, Vector2i(xOffset, yOffset));

        inventoryitemtype_t readyItem = P_InventoryReadyItem(wi->player);
        if(!(hud->readyItemFlashCounter > 0) && IIT_NONE != readyItem)
        {
            uint count = P_InventoryCount(wi->player, readyItem);
            if(count > 1)
            {
                char buf[20];
                dd_snprintf(buf, 20, "%i", count);

                FR_SetFont(wi->font);
                FR_SetTracking(TRACKING);
                FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
                FR_DrawTextXY2(buf, boxInfo.geometry.size.width-1, boxInfo.geometry.size.height-3, ALIGN_BOTTOMRIGHT);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void ReadyItem_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t boxInfo;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    Rect_SetWidthHeight(wi->geometry, boxInfo.geometry.size.width  * cfg.common.hudScale,
                                      boxInfo.geometry.size.height * cfg.common.hudScale);
}

void Inventory_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    DENG2_ASSERT(wi);
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.common.hudScale, EXTRA_SCALE * cfg.common.hudScale, 1);

    Hu_InventoryDraw(wi->player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Inventory_UpdateGeometry(uiwidget_t *wi)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    /// @todo Calculate the visual dimensions properly!
    Rect_SetWidthHeight(wi->geometry, (31*7+16*2) * EXTRA_SCALE * cfg.common.hudScale,
                                       INVENTORY_HEIGHT * EXTRA_SCALE * cfg.common.hudScale);

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Kills_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_kills_t *kills = (guidata_kills_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    kills->value = plr->killCount;
}

void Kills_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_kills_t *kills = (guidata_kills_t *)wi->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!(cfg.common.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT))) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(wi->player)) return;
    if(kills->value == 1994) return;

    char buf[40], tmp[20];
    strcpy(buf, "Kills: ");
    if(cfg.common.hudShownCheatCounters & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", kills->value, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.common.hudShownCheatCounters & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.common.hudShownCheatCounters & CCH_KILLS ? "(" : ""),
                totalKills ? kills->value * 100 / totalKills : 100,
                (cfg.common.hudShownCheatCounters & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudCheatCounterScale, cfg.common.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Kills_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_kills_t *kills = (guidata_kills_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!(cfg.common.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT))) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(wi->player)) return;
    if(kills->value == 1994) return;

    char buf[40], tmp[20];
    strcpy(buf, "Kills: ");
    if(cfg.common.hudShownCheatCounters & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", kills->value, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.common.hudShownCheatCounters & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.common.hudShownCheatCounters & CCH_KILLS ? "(" : ""),
                totalKills ? kills->value * 100 / totalKills : 100,
                (cfg.common.hudShownCheatCounters & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(wi->font);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, .5f + textSize.width  * cfg.common.hudCheatCounterScale,
                                      .5f + textSize.height * cfg.common.hudCheatCounterScale);
}

void Items_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_items_t *items = (guidata_items_t*)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    items->value = plr->itemCount;
}

void Items_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_items_t *items = (guidata_items_t *)wi->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!(cfg.common.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT))) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(wi->player)) return;
    if(items->value == 1994) return;

    char buf[40], tmp[20];
    strcpy(buf, "Items: ");
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", items->value, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.common.hudShownCheatCounters & CCH_ITEMS ? "(" : ""),
                totalItems ? items->value * 100 / totalItems : 100,
                (cfg.common.hudShownCheatCounters & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudCheatCounterScale, cfg.common.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Items_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_items_t *items = (guidata_items_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!(cfg.common.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT))) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(wi->player)) return;
    if(items->value == 1994) return;

    char buf[40], tmp[20];
    strcpy(buf, "Items: ");
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", items->value, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.common.hudShownCheatCounters & CCH_ITEMS ? "(" : ""),
                totalItems ? items->value * 100 / totalItems : 100,
                (cfg.common.hudShownCheatCounters & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(wi->font);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, .5f + textSize.width  * cfg.common.hudCheatCounterScale,
                                      .5f + textSize.height * cfg.common.hudCheatCounterScale);
}

void Secrets_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_secrets_t *scrt = (guidata_secrets_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    scrt->value = plr->secretCount;
}

void Secrets_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_secrets_t *scrt = (guidata_secrets_t *)wi->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!(cfg.common.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT))) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(wi->player)) return;
    if(scrt->value == 1994) return;

    char buf[40], tmp[20];
    strcpy(buf, "Secret: ");
    if(cfg.common.hudShownCheatCounters & CCH_SECRETS)
    {
        sprintf(tmp, "%i/%i ", scrt->value, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.common.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.common.hudShownCheatCounters & CCH_SECRETS ? "(" : ""),
                totalSecret ? scrt->value * 100 / totalSecret : 100,
                (cfg.common.hudShownCheatCounters & CCH_SECRETS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudCheatCounterScale, cfg.common.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Secrets_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_secrets_t *scrt = (guidata_secrets_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!(cfg.common.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT))) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(wi->player)) return;
    if(scrt->value == 1994) return;

    char buf[40], tmp[20];
    strcpy(buf, "Secret: ");
    if(cfg.common.hudShownCheatCounters & CCH_SECRETS)
    {
        sprintf(tmp, "%i/%i ", scrt->value, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.common.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.common.hudShownCheatCounters & CCH_SECRETS ? "(" : ""),
                totalSecret ? scrt->value * 100 / totalSecret : 100,
                (cfg.common.hudShownCheatCounters & CCH_SECRETS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(wi->font);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, .5f + textSize.width  * cfg.common.hudCheatCounterScale,
                                       .5f + textSize.height * cfg.common.hudCheatCounterScale);
}

static void drawUIWidgetsForPlayer(player_t *plr)
{
/// Units in fixed 320x200 screen space.
#define DISPLAY_BORDER      (2)
#define PADDING             (2)

    DENG2_ASSERT(plr);
    int const playerNum   = plr - players;
    int const displayMode = headupDisplayMode(playerNum);
    hudstate_t *hud = hudStates + playerNum;
    float scale;

    Size2Raw size, portSize;
    R_ViewPortSize(playerNum, &portSize);

    // The automap is drawn in a viewport scaled coordinate space (of viewwindow dimensions).
    uiwidget_t *wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]);
    UIWidget_SetOpacity(wi, ST_AutomapOpacity(playerNum));
    UIWidget_SetMaximumSize(wi, &portSize);
    GUI_DrawWidgetXY(wi, 0, 0);

    // The rest of the UI is drawn in a fixed 320x200 coordinate space.
    // Determine scale factors.
    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(scale, scale, 1);

    if(hud->statusbarActive || (displayMode < 3 || hud->alpha > 0))
    {
        float opacity = /**@todo Kludge: clamp*/MIN_OF(1.0f, hud->alpha)/**kludge end*/ * (1-hud->hideAmount);
        Size2Raw drawnSize = { 0, 0 };
        RectRaw displayRegion;
        int availHeight;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Scalef(1, 1.2f/*aspect correct*/, 1);

        displayRegion.origin.x = displayRegion.origin.y = 0;
        displayRegion.size.width  = .5f + portSize.width  / scale;
        displayRegion.size.height = .5f + portSize.height / (scale * 1.2f /*aspect correct*/);

        if(hud->statusbarActive)
        {
            const float statusbarOpacity = (1 - hud->hideAmount) * hud->showBar;

            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_STATUSBAR]);
            UIWidget_SetOpacity(wi, statusbarOpacity);
            UIWidget_SetMaximumSize(wi, &displayRegion.size);

            GUI_DrawWidget(wi, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);
        }

        displayRegion.origin.x += DISPLAY_BORDER;
        displayRegion.origin.y += DISPLAY_BORDER;
        displayRegion.size.width  -= DISPLAY_BORDER*2;
        displayRegion.size.height -= DISPLAY_BORDER*2;

        if(!hud->statusbarActive)
        {
            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]);
            UIWidget_SetOpacity(wi, opacity);
            UIWidget_SetMaximumSize(wi, &displayRegion.size);

            GUI_DrawWidget(wi, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);
        }

        if(!hud->statusbarActive)
        {
            int h = drawnSize.height;
            availHeight = displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0);

            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT2]);
            UIWidget_SetOpacity(wi, opacity);
            size.width = displayRegion.size.width; size.height = availHeight;
            UIWidget_SetMaximumSize(wi, &size);

            GUI_DrawWidget(wi, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);
            drawnSize.height += h;
        }

        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_MAPNAME]);
        UIWidget_SetOpacity(wi, ST_AutomapOpacity(playerNum));
        availHeight = displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0);
        size.width = displayRegion.size.width; size.height = availHeight;
        UIWidget_SetMaximumSize(wi, &size);

        GUI_DrawWidget(wi, &displayRegion.origin);

        // The other displays are always visible except when using the "no-hud" mode.
        if(hud->statusbarActive || displayMode < 3)
            opacity = 1.0f;

        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
        UIWidget_SetOpacity(wi, opacity);
        UIWidget_SetMaximumSize(wi, &displayRegion.size);

        GUI_DrawWidget(wi, &displayRegion.origin);

        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_COUNTERS]);
        UIWidget_SetOpacity(wi, opacity);
        UIWidget_SetMaximumSize(wi, &displayRegion.size);

        GUI_DrawWidget(wi, &displayRegion.origin);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef PADDING
#undef DISPLAY_BORDER
}

void ST_Drawer(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    if(!players[player].plr->inGame) return;

    R_UpdateViewFilter(player);

    hudstate_t *hud = &hudStates[player];
    hud->statusbarActive = (headupDisplayMode(player) < 2) || (ST_AutomapIsActive(player) && (cfg.common.automapHudDisplay == 0 || cfg.common.automapHudDisplay == 2) );

    drawUIWidgetsForPlayer(players + player);
}

dd_bool ST_StatusBarIsActive(int player)
{
    DENG2_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(!players[player].plr->inGame) return false;

    return hudStates[player].statusbarActive;
}

void ST_loadGraphics()
{
    pStatusbar = R_DeclarePatch("BARBACK");
    pInvBar    = R_DeclarePatch("INVBAR");
    pChain     = R_DeclarePatch("CHAIN");

    pStatBar   = R_DeclarePatch("STATBAR");
    pLifeBar   = R_DeclarePatch("LIFEBAR");

    pLifeGems[0] = R_DeclarePatch("LIFEGEM0");
    pLifeGems[1] = R_DeclarePatch("LIFEGEM1");
    pLifeGems[2] = R_DeclarePatch("LIFEGEM2");
    pLifeGems[3] = R_DeclarePatch("LIFEGEM3");

    pGodLeft  = R_DeclarePatch("GOD1");
    pGodRight = R_DeclarePatch("GOD2");
    pStatusbarTopLeft  = R_DeclarePatch("LTFCTOP");
    pStatusbarTopRight = R_DeclarePatch("RTFCTOP");

    char nameBuf[9];
    for(int i = 0; i < 16; ++i)
    {
        sprintf(nameBuf, "SPINBK%d", i);
        pSpinTome[i] = R_DeclarePatch(nameBuf);

        sprintf(nameBuf, "SPFLY%d", i);
        pSpinFly[i] = R_DeclarePatch(nameBuf);
    }

    // Inventory item flash anim.
    char const invItemFlashAnim[5][9] = {
        { "USEARTIA" },
        { "USEARTIB" },
        { "USEARTIC" },
        { "USEARTID" },
        { "USEARTIE" }
    };

    for(int i = 0; i < 5; ++i)
    {
        pInvItemFlash[i] = R_DeclarePatch(invItemFlashAnim[i]);
    }

    // Ammo icons.
    std::memset(pAmmoIcons, 0, sizeof(pAmmoIcons));
    for(int i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        AmmoDef const *def = P_AmmoDef((ammotype_t)i);
        // Available in the current game mode?
        if(def->gameModeBits & gameModeBits)
        {
            pAmmoIcons[i] = R_DeclarePatch(def->hudIcon);
        }
    }

    // Key cards.
    pKeys[0] = R_DeclarePatch("YKEYICON");
    pKeys[1] = R_DeclarePatch("GKEYICON");
    pKeys[2] = R_DeclarePatch("BKEYICON");
}

void ST_loadData()
{
    ST_loadGraphics();
}

static void initData(hudstate_t *hud)
{
    DENG2_ASSERT(hud);
    int player = hud - hudStates;

    hud->statusbarActive = true;
    hud->stopped = true;
    hud->showBar = 1;
    hud->readyItemFlashCounter = 0;

    // Fullscreen:
    hud->health.value = 1994;
    hud->armor.value = 1994;
    hud->readyammo.value = 1994;
    hud->readyammoicon.patchId = 0;
    hud->frags.value = 1994;
    hud->readyitem.patchId = 0;
    for(int i = 0; i < 3; ++i)
    {
        hud->keys.keyBoxes[i] = false;
    }

    // Statusbar:
    hud->sbarHealth.value = 1994;
    hud->sbarFrags.value = 1994;
    hud->sbarArmor.value = 1994;
    hud->sbarReadyammo.value = 1994;
    hud->sbarReadyammoicon.patchId = 0;
    hud->sbarReadyitem.patchId = 0;
    hud->sbarChain.wiggle = 0;
    hud->sbarChain.healthMarker = 0;
    for(int i = 0; i < 3; ++i)
    {
        hud->sbarKeyslots[i].slot     = i;
        hud->sbarKeyslots[i].keytypeA = (keytype_t)i;
        hud->sbarKeyslots[i].patchId  = 0;
    }

    // Other:
    hud->flight.patchId = 0;
    hud->flight.hitCenterFrame = false;
    hud->tome.patchId = 0;
    hud->tome.play = 0;
    hud->secrets.value = 1994;
    hud->items.value = 1994;
    hud->kills.value = 1994;

    hud->log._msgCount     = 0;
    hud->log._nextUsedMsg  = 0;
    hud->log._pvisMsgCount = 0;
    std::memset(hud->log._msgs, 0, sizeof(hud->log._msgs));

    ST_HUDUnHide(player, HUE_FORCE);
}

static void setAutomapCheatLevel(uiwidget_t *wi, int level)
{
    DENG2_ASSERT(wi);
    hudstate_t *hud = &hudStates[UIWidget_Player(wi)];
    hud->automapCheatLevel = level;

    int flags = UIAutomap_Flags(wi) & ~(AMF_REND_ALLLINES|AMF_REND_THINGS|AMF_REND_SPECIALLINES|AMF_REND_VERTEXES|AMF_REND_LINE_NORMALS);
    if(hud->automapCheatLevel >= 1)
        flags |= AMF_REND_ALLLINES;
    if(hud->automapCheatLevel == 2)
        flags |= AMF_REND_THINGS | AMF_REND_SPECIALLINES;
    if(hud->automapCheatLevel > 2)
        flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    UIAutomap_SetFlags(wi, flags);
}

static void initAutomapForCurrentMap(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
#if __JDOOM__
    hudstate_t *hud = &hudStates[UIWidget_Player(wi)];
    automapcfg_t *mcfg;
#endif

    UIAutomap_Reset(wi);

    UIAutomap_SetMinScale(wi, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(wi, *((coord_t*) DD_GetVariable(DD_MAP_MIN_X)),
                                 *((coord_t*) DD_GetVariable(DD_MAP_MAX_X)),
                                 *((coord_t*) DD_GetVariable(DD_MAP_MIN_Y)),
                                 *((coord_t*) DD_GetVariable(DD_MAP_MAX_Y)));

#if __JDOOM__
    mcfg = UIAutomap_Config(wi);
#endif

    // Determine the obj view scale factors.
    if(UIAutomap_ZoomMax(wi))
        UIAutomap_SetScale(wi, 0);

    UIAutomap_ClearPoints(wi);

#if !__JHEXEN__
    if(G_Ruleset_Skill() == SM_BABY && cfg.common.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(wi);
        UIAutomap_SetFlags(wi, flags|AMF_REND_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    mobj_t *followMobj = UIAutomap_FollowMobj(wi);
    if(followMobj)
    {
        UIAutomap_SetCameraOrigin(wi, followMobj->origin[VX], followMobj->origin[VY]);
    }

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(wi, 0);
    }

    UIAutomap_SetReveal(wi, false);

    // Add all immediately visible lines.
    for(int i = 0; i < numlines; ++i)
    {
        xline_t *xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLineAutomapVisibility(UIWidget_Player(wi), i, true);
    }
}

void ST_Start(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    hudstate_t *hud = &hudStates[player];

    if(!hud->stopped)
    {
        ST_Stop(player);
    }

    initData(hud);

    // Initialize widgets according to player preferences.
    uiwidget_t *wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);

    int flags = UIWidget_Alignment(wi);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.common.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.common.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(wi, flags);

    wi = GUI_MustFindObjectById(hud->automapWidgetId);
    // If the automap was left open; close it.
    UIAutomap_Open(wi, false, true);
    initAutomapForCurrentMap(wi);
    UIAutomap_SetCameraRotation(wi, cfg.common.automapRotate);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    hudstate_t *hud = &hudStates[player];
    hud->stopped = true;
}

void ST_BuildWidgets(int player)
{
#define PADDING             (2) /// Units in fixed 320x200 screen space.

    if(player < 0 || player >= MAXPLAYERS) return;

struct uiwidgetgroupdef_t
{
    int group;
    int alignFlags;
    order_t order;
    int groupFlags;
    int padding; // In fixed 320x200 pixels.
};

struct uiwidgetdef_t
{
    guiwidgettype_t type;
    int alignFlags;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (uiwidget_t *wi);
    void (*drawer) (uiwidget_t *wi, Point2Raw const *offset);
    void (*ticker) (uiwidget_t *wi, timespan_t ticLength);
    void *typedata;
};

    hudstate_t *hud = hudStates + player;
    uiwidgetgroupdef_t const widgetGroupDefs[] = {
        { UWG_STATUSBAR,    ALIGN_BOTTOM },
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_TOP,          ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT },
        { UWG_TOPCENTER,    ALIGN_TOP,         ORDER_LEFTTORIGHT, UWGF_VERTICAL, PADDING },
        { UWG_TOPLEFT,      ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPRIGHT,     ALIGN_TOPRIGHT,    ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,  ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_BOTTOMLEFT2,  ALIGN_BOTTOMLEFT,  ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT, ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_BOTTOMCENTER, ALIGN_BOTTOM,      ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_BOTTOM,       ALIGN_BOTTOMLEFT,  ORDER_LEFTTORIGHT },
        { UWG_COUNTERS,     ALIGN_LEFT,        ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    int const widgetGroupDefCount = int(sizeof(widgetGroupDefs) / sizeof(widgetGroupDefs[0]));

    uiwidgetdef_t const widgetDefs[] = {
        { GUI_BOX,          ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarBackground_UpdateGeometry, SBarBackground_Drawer },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarInventory_UpdateGeometry, SBarInventory_Drawer },
        { GUI_FRAGS,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarFrags_UpdateGeometry, SBarFrags_Drawer, Frags_Ticker, &hud->sbarFrags },
        { GUI_HEALTH,       ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarHealth_UpdateGeometry, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
        { GUI_ARMOR,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarArmor_UpdateGeometry, SBarArmor_Drawer, Armor_Ticker, &hud->sbarArmor },
        { GUI_KEYSLOT,      ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      KeySlot_UpdateGeometry, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[0] },
        { GUI_KEYSLOT,      ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      KeySlot_UpdateGeometry, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[1] },
        { GUI_KEYSLOT,      ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      KeySlot_UpdateGeometry, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[2] },
        { GUI_READYAMMO,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarReadyAmmo_UpdateGeometry, SBarReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->sbarReadyammo },
        { GUI_READYAMMOICON, ALIGN_TOPLEFT,   UWG_STATUSBAR,    GF_NONE,      SBarReadyAmmoIcon_UpdateGeometry, SBarReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->sbarReadyammoicon },
        { GUI_READYITEM,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarReadyItem_UpdateGeometry, SBarReadyItem_Drawer, ReadyItem_Ticker, &hud->sbarReadyitem },
        { GUI_CHAIN,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarChain_UpdateGeometry, SBarChain_Drawer, SBarChain_Ticker, &hud->sbarChain },
        { GUI_READYAMMOICON, ALIGN_TOPLEFT,   UWG_TOPLEFT,      GF_NONE,      ReadyAmmoIcon_UpdateGeometry, ReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->readyammoicon },
        { GUI_READYAMMO,    ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_STATUS,    ReadyAmmo_UpdateGeometry, ReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->readyammo },
        { GUI_FLIGHT,       ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_NONE,      Flight_UpdateGeometry, Flight_Drawer, Flight_Ticker, &hud->flight },
        { GUI_TOME,         ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_SMALLIN,   Tome_UpdateGeometry, Tome_Drawer, Tome_Ticker, &hud->tome },
        { GUI_HEALTH,       ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_FONTB,     Health_UpdateGeometry, Health_Drawer, Health_Ticker, &hud->health },
        { GUI_KEYS,         ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_NONE,      Keys_UpdateGeometry, Keys_Drawer, Keys_Ticker, &hud->keys },
        { GUI_ARMOR,        ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_STATUS,    Armor_UpdateGeometry, Armor_Drawer, Armor_Ticker, &hud->armor },
        { GUI_FRAGS,        ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT2,  GF_STATUS,    Frags_UpdateGeometry, Frags_Drawer, Frags_Ticker, &hud->frags },
        { GUI_READYITEM,    ALIGN_BOTTOMRIGHT, UWG_BOTTOMRIGHT, GF_SMALLIN,   ReadyItem_UpdateGeometry, ReadyItem_Drawer, ReadyItem_Ticker, &hud->readyitem },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_BOTTOMCENTER, GF_SMALLIN,   Inventory_UpdateGeometry, Inventory_Drawer },
        { GUI_SECRETS,      ALIGN_TOPLEFT,    UWG_COUNTERS,     GF_FONTA,     Secrets_UpdateGeometry, Secrets_Drawer, Secrets_Ticker, &hud->secrets },
        { GUI_ITEMS,        ALIGN_TOPLEFT,    UWG_COUNTERS,     GF_FONTA,     Items_UpdateGeometry, Items_Drawer, Items_Ticker, &hud->items },
        { GUI_KILLS,        ALIGN_TOPLEFT,    UWG_COUNTERS,     GF_FONTA,     Kills_UpdateGeometry, Kills_Drawer, Kills_Ticker, &hud->kills },
        { GUI_NONE }
    };

    for(int i = 0; i < widgetGroupDefCount; ++i)
    {
        uiwidgetgroupdef_t const *def = &widgetGroupDefs[i];
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(def->groupFlags, player, def->alignFlags, def->order, def->padding);
    }

    for(int i = 0; widgetDefs[i].type != GUI_NONE; ++i)
    {
        uiwidgetdef_t const *def = &widgetDefs[i];
        uiwidgetid_t id = GUI_CreateWidget(def->type, player, def->alignFlags, FID(def->fontIdx), 1, def->updateGeometry, def->drawer, def->ticker, def->typedata);
        UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
    }

    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMCENTER]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]));

    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPRIGHT]));

    hud->logWidgetId = GUI_CreateWidget(GUI_LOG, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UILog_UpdateGeometry, UILog_Drawer, UILog_Ticker, &hud->log);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->logWidgetId));

    hud->chatWidgetId = GUI_CreateWidget(GUI_CHAT, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIChat_UpdateGeometry, UIChat_Drawer, NULL, &hud->chat);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->chatWidgetId));

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIAutomap_UpdateGeometry, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), GUI_FindObjectById(hud->automapWidgetId));

#undef PADDING
}

void ST_Init()
{
    ST_InitAutomapConfig();
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        ST_BuildWidgets(i);
        hud->inited = true;
    }
    ST_loadData();
}

void ST_Shutdown()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        hud->inited = false;
    }
}

void ST_CloseAll(int player, dd_bool fast)
{
    NetSv_DismissHUDs(player, fast);
    
    ST_AutomapOpen(player, false, fast);
    Hu_InventoryOpen(player, false);
}

uiwidget_t *ST_UIChatForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        return GUI_FindObjectById(hud->chatWidgetId);
    }
    DENG2_ASSERT(!"ST_UIChatForPlayer: Invalid player num");
    return nullptr;
}

uiwidget_t *ST_UILogForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        return GUI_FindObjectById(hud->logWidgetId);
    }
    DENG2_ASSERT(!"ST_UILogForPlayer: Invalid player num");
    return nullptr;
}

uiwidget_t* ST_UIAutomapForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        return GUI_FindObjectById(hud->automapWidgetId);
    }
    DENG2_ASSERT(!"ST_UIAutomapForPlayer: Invalid player num");
    return nullptr;
}

int ST_ChatResponder(int player, event_t *ev)
{
    if(uiwidget_t *wi = ST_UIChatForPlayer(player))
    {
        return UIChat_Responder(wi, ev);
    }
    return false;
}

dd_bool ST_ChatIsActive(int player)
{
    if(uiwidget_t *wi = ST_UIChatForPlayer(player))
    {
        return UIChat_IsActive(wi);
    }
    return false;
}

void ST_LogPost(int player, byte flags, char const *msg)
{
    if(uiwidget_t *wi = ST_UILogForPlayer(player))
    {
        UILog_Post(wi, flags, msg);
    }
}

void ST_LogRefresh(int player)
{
    if(uiwidget_t *wi = ST_UILogForPlayer(player))
    {
        UILog_Refresh(wi);
    }
}

void ST_LogEmpty(int player)
{
    if(uiwidget_t *wi = ST_UILogForPlayer(player))
    {
        UILog_Empty(wi);
    }
}

void ST_LogPostVisibilityChangeNotification()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NO_HIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void ST_LogUpdateAlignment()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        if(!hud->inited) continue;

        uiwidget_t *wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
        int flags = UIWidget_Alignment(wi);
        flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
        if(cfg.common.msgAlign == 0)
            flags |= ALIGN_LEFT;
        else if(cfg.common.msgAlign == 2)
            flags |= ALIGN_RIGHT;
        UIWidget_SetAlignment(wi, flags);
    }
}

void ST_AutomapOpen(int player, dd_bool yes, dd_bool fast)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        UIAutomap_Open(wi, yes, fast);
    }
}

dd_bool ST_AutomapIsActive(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        return UIAutomap_Active(wi);
    }
    return false;
}

dd_bool ST_AutomapObscures2(int player, RectRaw const * /*region*/)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return false;

    if(UIAutomap_Active(wi))
    {
        if(cfg.common.automapOpacity * ST_AutomapOpacity(player) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(UIAutomap_Fullscreen(obj))
            {*/
                return true;
            /*}
            else
            {
                // We'll have to compare the dimensions.
                const int scrwidth  = Get(DD_WINDOW_WIDTH);
                const int scrheight = Get(DD_WINDOW_HEIGHT);
                const Rect* rect = UIWidget_Geometry(obj);
                float fx = FIXXTOSCREENX(region->origin.x);
                float fy = FIXYTOSCREENY(region->origin.y);
                float fw = FIXXTOSCREENX(region->size.width);
                float fh = FIXYTOSCREENY(region->size.height);

                if(dims->origin.x >= fx && dims->origin.y >= fy && dims->size.width >= fw && dims->size.height >= fh)
                    return true;
            }*/
        }
    }
    return false;
}

dd_bool ST_AutomapObscures(int player, int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    return ST_AutomapObscures2(player, &rect);
}

void ST_AutomapClearPoints(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        UIAutomap_ClearPoints(wi);
        P_SetMessage(&players[player], LMF_NO_HIDE, AMSTR_MARKSCLEARED);
    }
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return - 1;

    if(UIAutomap_PointCount(wi) == MAX_MAP_POINTS)
        return -1;

    int newPoint = UIAutomap_AddPoint(wi, x, y, z);
    char buf[20]; sprintf(buf, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], LMF_NO_HIDE, buf);

    return newPoint;
}

dd_bool ST_AutomapPointOrigin(int player, int point, coord_t *x, coord_t *y, coord_t *z)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        return UIAutomap_PointOrigin(wi, point, x, y, z);
    }
    return false;
}

void ST_ToggleAutomapMaxZoom(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        if(UIAutomap_SetZoomMax(wi, !UIAutomap_ZoomMax(wi)))
        {
            App_Log(0, "Maximum zoom %s in automap", UIAutomap_ZoomMax(wi)? "ON":"OFF");
        }
    }
}

float ST_AutomapOpacity(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        return UIAutomap_Opacity(wi);
    }
    return 0;
}

void ST_SetAutomapCameraRotation(int player, dd_bool on)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        UIAutomap_SetCameraRotation(wi, on);
    }
}

void ST_ToggleAutomapPanMode(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        if(UIAutomap_SetPanMode(wi, !UIAutomap_PanMode(wi)))
        {
            P_SetMessage(&players[player], LMF_NO_HIDE, (UIAutomap_PanMode(wi)? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON));
        }
    }
}

void ST_CycleAutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        ST_SetAutomapCheatLevel(player, (hud->automapCheatLevel + 1) % 3);
    }
}

void ST_SetAutomapCheatLevel(int player, int level)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        setAutomapCheatLevel(wi, level);
    }
}

void ST_RevealAutomap(int player, dd_bool on)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        UIAutomap_SetReveal(wi, on);
    }
}

dd_bool ST_AutomapHasReveal(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        return UIAutomap_Reveal(wi);
    }
    return false;
}

void ST_RebuildAutomap(int player)
{
    if(uiwidget_t *wi = ST_UIAutomapForPlayer(player))
    {
        UIAutomap_Rebuild(wi);
    }
}

int ST_AutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        return hudStates[player].automapCheatLevel;
    }
    return 0;
}

/**
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow()
{
    R_ResizeViewWindow(RWF_FORCE);
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_HUDUnHide(i, HUE_FORCE); // So the user can see the change.
    }
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
static void unhideHUD()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_HUDUnHide(i, HUE_FORCE);
    }
}

D_CMD(ChatOpen)
{
    DENG2_UNUSED2(src, argc);
    if(G_QuitInProgress()) return false;

    int const player = CONSOLEPLAYER;
    uiwidget_t *wi   = ST_UIChatForPlayer(player);
    if(!wi) return false;

    int destination = 0;
    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            App_Log(DE2_SCR_ERROR, "Invalid team number #%i (valid range: 0...%i)",
                    destination, NUMTEAMS);
            return false;
        }
    }
    UIChat_SetDestination(wi, destination);
    UIChat_Activate(wi, true);
    return true;
}

D_CMD(ChatAction)
{
    DENG2_UNUSED2(src, argc);
    char const *cmd = argv[0] + 4;

    if(G_QuitInProgress()) return false;

    int const player = CONSOLEPLAYER;
    uiwidget_t *wi   = ST_UIChatForPlayer(player);
    if(!wi) return false;

    if(!UIChat_IsActive(wi)) return false;

    if(!qstricmp(cmd, "complete")) // Send the message.
    {
        return UIChat_CommandResponder(wi, MCMD_SELECT);
    }
    else if(!qstricmp(cmd, "cancel")) // Close chat.
    {
        return UIChat_CommandResponder(wi, MCMD_CLOSE);
    }
    else if(!qstricmp(cmd, "delete"))
    {
        return UIChat_CommandResponder(wi, MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    DENG2_UNUSED(src);
    if(G_QuitInProgress()) return false;

    if(argc < 2 || argc > 3)
    {
        App_Log(DE2_SCR_NOTE, "Usage: %s (team) (macro number)", argv[0]);
        App_Log(DE2_SCR_MSG, "Send a chat macro to other player(s). "
                "If (team) is omitted, the message will be sent to all players.");
        return true;
    }

    int const player = CONSOLEPLAYER;
    uiwidget_t *wi   = ST_UIChatForPlayer(player);
    if(!wi) return false;

    int destination = 0;
    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            App_Log(DE2_SCR_ERROR, "Invalid team number #%i (valid range: 0...%i)",
                    destination, NUMTEAMS);
            return false;
        }
    }

    int const macroId = UIChat_ParseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        App_Log(DE2_SCR_ERROR, "Invalid macro id");
        return false;
    }

    UIChat_Activate(wi, true);
    UIChat_SetDestination(wi, destination);
    UIChat_LoadMacro(wi, macroId);
    UIChat_CommandResponder(wi, MCMD_SELECT);
    UIChat_Activate(wi, false);
    return true;
}

void ST_Register()
{
    C_VAR_FLOAT2( "hud-color-r",                    &cfg.common.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g",                    &cfg.common.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b",                    &cfg.common.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a",                    &cfg.common.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha",                 &cfg.common.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT   ( "hud-patch-replacement",          &cfg.common.hudPatchReplaceMode, 0, 0, 1 )
    C_VAR_FLOAT2( "hud-scale",                      &cfg.common.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT ( "hud-timer",                      &cfg.common.hudTimer, 0, 0, 60 )

    // Displays:
    C_VAR_BYTE2 ( "hud-ammo",                       &cfg.hudShown[HUD_AMMO], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-armor",                      &cfg.hudShown[HUD_ARMOR], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-cheat-counter",              &cfg.common.hudShownCheatCounters, 0, 0, 63, unhideHUD )
    C_VAR_FLOAT2( "hud-cheat-counter-scale",        &cfg.common.hudCheatCounterScale, 0, .1f, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-cheat-counter-show-mapopen", &cfg.common.hudCheatCounterShowWithAutomap, 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-currentitem",                &cfg.hudShown[HUD_READYITEM], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-health",                     &cfg.hudShown[HUD_HEALTH], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-keys",                       &cfg.hudShown[HUD_KEYS], 0, 0, 1, unhideHUD )
    C_VAR_INT   ( "hud-tome-timer",                 &cfg.tomeCounter, CVF_NO_MAX, 0, 0 )
    C_VAR_INT   ( "hud-tome-sound",                 &cfg.tomeSound, CVF_NO_MAX, 0, 0 )

    C_VAR_FLOAT2( "hud-status-alpha",               &cfg.common.statusbarOpacity, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-icon-a",              &cfg.common.statusbarCounterAlpha, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-size",                &cfg.common.statusbarScale, 0, 0.1f, 1, updateViewWindow )

    // Events:
    C_VAR_BYTE  ( "hud-unhide-damage",              &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-ammo",         &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-armor",        &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-health",       &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-invitem",      &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-key",          &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-powerup",      &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-weapon",       &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD("beginchat",       nullptr,   ChatOpen )
    C_CMD("chatcancel",      "",        ChatAction )
    C_CMD("chatcomplete",    "",        ChatAction )
    C_CMD("chatdelete",      "",        ChatAction )
    C_CMD("chatsendmacro",   nullptr,   ChatSendMacro )

    Hu_InventoryRegister();
}