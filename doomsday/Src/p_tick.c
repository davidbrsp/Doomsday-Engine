/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_tick.c: Timed Playsim Events
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

#include "r_sky.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// P_MobjTicker
//===========================================================================
void P_MobjTicker(mobj_t *mo)
{
	int i = mo->halofactor & 0x7f;

	// Handle halofactor.
	if(mo->halofactor & 0x80)
	{
		// Going up.
		i += haloOccludeSpeed;
		if(i > 127) i = 127;
	}
	else
	{
		// Going down.
		i -= haloOccludeSpeed;
		if(i < 0) i = 0;
	}
	mo->halofactor &= ~0x7f;
	mo->halofactor |= i;
}

//===========================================================================
// P_Ticker
//	Doomsday's own play-ticker.
//===========================================================================
void P_Ticker(timespan_t time)
{
	thinker_t *th;
	static trigger_t fixed = { 1.0/35 };

	if(!thinkercap.next) return; // Not initialized yet.
	if(!M_CheckTrigger(&fixed, time)) return;

	// New ptcgens for planes?
	P_CheckPtcPlanes();	
	R_AnimateAnimGroups();
	R_SkyTicker();

	// Check all mobjs.
	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		// FIXME: Client mobjs!
		if(!P_IsMobjThinker(th->function)) continue;
		P_MobjTicker( (mobj_t*) th);
	}
}