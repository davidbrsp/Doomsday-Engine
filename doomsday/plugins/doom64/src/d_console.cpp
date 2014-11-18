/** @file d_console.cpp  Doom64 specific console settings and commands.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

#include "jdoom64.h"

#include "hu_menu.h"
#include "hu_stuff.h"

D_CMD(CheatGive);
D_CMD(CheatGod);
D_CMD(CheatMassacre);
D_CMD(CheatNoClip);
D_CMD(CheatReveal);
D_CMD(CycleSpy);
D_CMD(CheatSuicide);
D_CMD(CheatWhere);
D_CMD(MakeLocal);
D_CMD(PlayDemo);
D_CMD(PrintPlayerCoords);
D_CMD(RecordDemo);
D_CMD(SetCamera);
D_CMD(SetViewLock);
D_CMD(SetViewMode);
D_CMD(SpawnMobj);
D_CMD(StopDemo);

static void updateEyeHeight()
{
    player_t* plr = &players[CONSOLEPLAYER];
    if(!(plr->plr->flags & DDPF_CAMERA))
    {
        plr->viewHeight = (float) cfg.plrViewHeight;
    }
}

D_CMD(ScreenShot)
{
    DENG2_UNUSED3(src, argc, argv);
    G_SetGameAction(GA_SCREENSHOT);
    return true;
}

void G_ConsoleRegistration()
{
    // View/Refresh
    C_VAR_INT  ("view-size",                            &cfg.setBlocks,                     0, 3, 11);
    C_VAR_BYTE ("hud-title",                            &cfg.mapTitle,                      0, 0, 1);
    C_VAR_BYTE ("hud-title-author-noiwad",              &cfg.hideIWADAuthor,                0, 0, 1);

    C_VAR_FLOAT("view-bob-height",                      &cfg.bobView,                       0, 0, 1);
    C_VAR_FLOAT("view-bob-weapon",                      &cfg.bobWeapon,                     0, 0, 1);
    C_VAR_BYTE ("view-bob-weapon-switch-lower",         &cfg.bobWeaponLower,                0, 0, 1);
    C_VAR_FLOAT("view-filter-strength",                 &cfg.filterStrength,                0, 0, 1);

    // Player
    // Player data
    C_VAR_BYTE ("player-color",                         &cfg.netColor,                      0, 0, 4);
    C_VAR_INT2 ("player-eyeheight",                     &cfg.plrViewHeight,                 0, 41, 54, updateEyeHeight);

    // Movment
    C_VAR_FLOAT("player-move-speed",                    &cfg.playerMoveSpeed,               0, 0, 1);
    C_VAR_INT  ("player-jump",                          &cfg.jumpEnabled,                   0, 0, 1);
    C_VAR_FLOAT("player-jump-power",                    &cfg.jumpPower,                     0, 0, 100);
    C_VAR_BYTE ("player-air-movement",                  &cfg.airborneMovement,              0, 0, 32);
    C_VAR_BYTE ("player-weapon-recoil",                 &cfg.weaponRecoil,                  0, 0, 1);

    // Weapon switch preferences
    C_VAR_BYTE ("player-autoswitch",                    &cfg.weaponAutoSwitch,              0, 0, 2);
    C_VAR_BYTE ("player-autoswitch-ammo",               &cfg.ammoAutoSwitch,                0, 0, 2);
    C_VAR_BYTE ("player-autoswitch-berserk",            &cfg.berserkAutoSwitch,             0, 0, 1);
    C_VAR_BYTE ("player-autoswitch-notfiring",          &cfg.noWeaponAutoSwitchIfFiring,    0, 0, 1);

    // Weapon Order preferences
    C_VAR_INT  ("player-weapon-order0",                 &cfg.weaponOrder[0],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order1",                 &cfg.weaponOrder[1],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order2",                 &cfg.weaponOrder[2],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order3",                 &cfg.weaponOrder[3],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order4",                 &cfg.weaponOrder[4],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order5",                 &cfg.weaponOrder[5],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order6",                 &cfg.weaponOrder[6],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order7",                 &cfg.weaponOrder[7],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order8",                 &cfg.weaponOrder[8],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order9",                 &cfg.weaponOrder[9],                0, 0, NUM_WEAPON_TYPES);

    C_VAR_BYTE ("player-weapon-nextmode",               &cfg.weaponNextMode,                0, 0, 1);
    C_VAR_BYTE ("player-weapon-cycle-sequential",       &cfg.weaponCycleSequential,         0, 0, 1);

    // Misc
    C_VAR_INT  ("player-camera-noclip",                 &cfg.cameraNoClip,                  0, 0, 1);
    C_VAR_BYTE ("player-death-lookup",                  &cfg.deathLookUp,                   0, 0, 1);

    // Compatibility options
    C_VAR_BYTE ("game-anybossdeath666",                 &cfg.anyBossDeath,                  0, 0, 1);
    C_VAR_BYTE ("game-corpse-sliding",                  &cfg.slidingCorpses,                0, 0, 1);
    C_VAR_BYTE ("game-maxskulls",                       &cfg.maxSkulls,                     0, 0, 1);
    C_VAR_BYTE ("game-monsters-floatoverblocking",      &cfg.allowMonsterFloatOverBlocking, 0, 0, 1);
    C_VAR_BYTE ("game-monsters-stuckindoors",           &cfg.monstersStuckInDoors,          0, 0, 1);
    C_VAR_BYTE ("game-objects-clipping",                &cfg.moveBlock,                     0, 0, 1);
    C_VAR_BYTE ("game-objects-falloff",                 &cfg.fallOff,                       0, 0, 1);
    C_VAR_BYTE ("game-objects-gibcrushednonbleeders",   &cfg.gibCrushedNonBleeders,         0, 0, 1);
    C_VAR_BYTE ("game-objects-neverhangoverledges",     &cfg.avoidDropoffs,                 0, 0, 1);
    C_VAR_BYTE ("game-player-wallrun-northonly",        &cfg.wallRunNorthOnly,              0, 0, 1);
    C_VAR_BYTE ("game-skullsinwalls",                   &cfg.allowSkullsInWalls,            0, 0, 1);
    C_VAR_BYTE ("game-zclip",                           &cfg.moveCheckZ,                    0, 0, 1);
    C_VAR_BYTE ("game-zombiescanexit",                  &cfg.zombiesCanExit,                0, 0, 1);

    // Game state
    //C_VAR_BYTE ("game-fastmonsters",                    &fastParm,                          0, 0, 1);

    // Gameplay
    C_VAR_INT  ("game-corpse-time",                     &cfg.corpseTime,                    CVF_NO_MAX, 0, 0);

    // Misc
    C_VAR_BYTE ("msg-echo",                             &cfg.echoMsg,                       0, 0, 1);


    C_CMD("spy",         "",     CycleSpy);
    C_CMD("screenshot",  "",     ScreenShot);

    C_CMD("god",         NULL,   CheatGod);
    C_CMD("noclip",      NULL,   CheatNoClip);
    C_CMD("reveal",      "i",    CheatReveal);
    C_CMD("give",        NULL,   CheatGive);
    C_CMD("kill",        "",     CheatMassacre);
    C_CMD("suicide",     NULL,   CheatSuicide);
    C_CMD("where",       "",     CheatWhere);

    C_CMD("spawnmobj",   NULL,   SpawnMobj);
    C_CMD("coord",       "",     PrintPlayerCoords);

    C_CMD("makelocp",    "i",    MakeLocal);
    C_CMD("makecam",     "i",    SetCamera);
    C_CMD("setlock",     NULL,   SetViewLock);
    C_CMD("lockmode",    "i",    SetViewLock);
    C_CMD("viewmode",    NULL,   SetViewMode);
}
