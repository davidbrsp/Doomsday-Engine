/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

#ifndef __P_V19_SAVEG__
#define __P_V19_SAVEG__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

void            SV_v19_LoadGame(char *filename);

// Persistent storage/archiving.
// These are the load / save game routines.
void            P_v19_UnArchivePlayers(void);
void            P_v19_UnArchiveWorld(void);
void            P_v19_UnArchiveThinkers(void);
void            P_v19_UnArchiveSpecials(void);

#endif
