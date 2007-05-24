/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/*
 * r_main.h: Refresh Subsystem
 */

#ifndef __DOOMSDAY_REFRESH_MAIN_H__
#define __DOOMSDAY_REFRESH_MAIN_H__

extern fixed_t  viewx, viewy, viewz;
extern float    viewfrontvec[3], viewupvec[3], viewsidevec[3];
extern fixed_t  viewxOffset, viewyOffset, viewzOffset;
extern angle_t  viewangle;
extern float    viewpitch;
extern ddplayer_t *viewplayer;
extern angle_t  clipangle;
extern fixed_t  finetangent[FINEANGLES / 2];

extern float    frameTimePos;      // 0...1: fractional part for sharp game tics
extern boolean  resyncFrameTimePos;
extern int      loadInStartupMode;
extern int      validcount;
extern int      viewwidth, viewheight, viewwindowx, viewwindowy;
extern boolean  setsizeneeded;
extern int      framecount;
extern int      viewangleoffset;
extern int      extralight;
extern fixed_t  viewcos, viewsin;
extern int      rendInfoTris;

void            R_Register(void);
void            R_Init(void);
void            R_Update(void);
void            R_Shutdown(void);
void            R_SetupWorldFrame(void);
void            R_RenderPlayerView(ddplayer_t *player);
void            R_RenderPlayerViewBorder(void);
void            R_ResetViewer(void);
void            R_ViewWindow(int x, int y, int w, int h);
void            R_NewSharpWorld(void);

#endif
