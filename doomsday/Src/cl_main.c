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
 * cl_main.c: Network Client
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void Net_ResetTimer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gotframe;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

id_t		clientID;
boolean		handshakeReceived = false;
int			gameReady = false;
int			serverTime;
boolean		netLoggedIn = false;	// Logged in to the server.
boolean		clientPaused = false;	// Set by the server.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Cl_InitID(void)
{
	FILE *file;

	// Read the client ID number file.
	srand(time(NULL));
	if((file = fopen("Client.ID", "rb")) != NULL)
	{
		fread(&clientID, 4, 1, file);
		fclose(file);
		return;
	}
	// Ah-ha, we need to generate a new ID.
	clientID = Sys_GetRealTime()*rand() + (rand()&0xfff) + 
		((rand()&0xfff)<<12) + ((rand()&0xff)<<24);
	// Write it to the file.
	if((file = fopen("Client.ID", "wb")) != NULL)
	{
		fwrite(&clientID, 4, 1, file);
		fclose(file);
	}
}

int Cl_GameReady()
{
	return (handshakeReceived && gameReady);
}

void Cl_CleanUp()
{
	Con_Printf("Cl_CleanUp.\n");

	clientPaused = false;
	handshakeReceived = false;

	Cl_DestroyClientMobjs();
	Cl_InitPlayers();
	Cl_RemoveMovers();
	GL_SetFilter(0);
}

/*
 * Sends a hello packet. 
 * pcl_hello2 includes the Game ID (16 chars).
 */
void Cl_SendHello(void)
{
	char buf[16];

	Msg_Begin(pcl_hello2);
	Msg_WriteLong(clientID);
	
	// The game mode is included in the hello packet.
	memset(buf, 0, sizeof(buf));
	strncpy(buf, gx.Get(DD_GAME_MODE), sizeof(buf));
	Msg_Write(buf, 16);

	Net_SendBuffer(0, SPF_ORDERED);
}

void Cl_AnswerHandshake(handshake_packet_t *pShake)
{
	handshake_packet_t shake;
	int	i;

	// Copy the data to a buffer of our own.
	memcpy(&shake, pShake, sizeof(shake));

	// Immediately send an acknowledgement.
	Msg_Begin(pcl_ack_shake);
	Net_SendBuffer(0, SPF_ORDERED);	

	// Check the version number.
	if(shake.version != SV_VERSION)
	{
		Con_Message("Cl_AnswerHandshake: Version conflict! (you:%i, server:%i)\n",
			SV_VERSION, shake.version);
		Con_Execute("net disconnect", false);
		Demo_StopPlayback();
		Con_Open(true);
		return;
	}

	// Update time and player ingame status.
	gametic = shake.gameTime; 
	for(i = 0; i < MAXPLAYERS; i++)
		players[i].ingame = (shake.playerMask & (1<<i)) != 0;	
	consoleplayer = displayplayer = shake.yourConsole;
	clients[consoleplayer].numTics = 0;
	clients[consoleplayer].firstTic = 0;

	isClient = true;
	isServer = false;
	netLoggedIn = false;
	clientPaused = false;

	if(handshakeReceived) return;

	// This prevents redundant re-initialization.
	handshakeReceived = true;

	// Soon after this packet will follow the game's handshake.
	gameReady = false;
	Cl_InitFrame();
	
	Con_Printf("Cl_AnswerHandshake: myConsole:%i, gameTime:%i.\n",
		shake.yourConsole, shake.gameTime);

	// Tell the game that we have arrived. The level will
	// be changed when the game's handshake arrives (handled
	// in the dll).
	gx.NetPlayerEvent(consoleplayer, DDPE_ARRIVAL, 0);

	// Prepare the client-side data.
	Cl_InitClientMobjs();
	Cl_InitMovers();

	// Get ready for ticking.
	Net_ResetTimer();
}

void Cl_HandlePlayerInfo(playerinfo_packet_t* info)
{
	boolean present;

	Con_Printf( "Cl_HandlePlayerInfo: console:%i name:%s\n",
		info->console, info->name);

	// Is the console number valid?
	if(info->console < 0 || info->console >= MAXPLAYERS) return;

	present = players[info->console].ingame;
	players[info->console].ingame = true;
	strcpy(clients[info->console].name, info->name);

	if(!present)
	{
		// This is a new player! Let the game know about this.
		gx.NetPlayerEvent(info->console, DDPE_ARRIVAL, 0);
	}
}

void Cl_PlayerLeaves(int number)
{
	Con_Printf("Cl_PlayerLeaves: player %i has left.\n", number);
	players[number].ingame = false;
	gx.NetPlayerEvent(number, DDPE_EXIT, 0);
}

/*
 * Client's packet handler.
 * Handles all the events the server sends.
 */
void Cl_GetPackets(void)
{
	int		i;

	// All messages come from the server.
	while(Net_GetPacket())
	{
		// First check for packets that are only valid when 
		// a game is in progress.
		if(Cl_GameReady())
		{
			boolean handled = true;
			switch(netbuffer.msg.type)
			{
			case psv_frame:
				Cl_FrameReceived();
				break;

			case psv_first_frame2:
			case psv_frame2:
				Cl_Frame2Received(netbuffer.msg.type);
				break;

			case pkt_coords:
				Cl_CoordsReceived();
				break;
				
			case psv_sound:
				Cl_Sound();
				break;

			case psv_filter:
				players[consoleplayer].filter = Msg_ReadLong();
				break;

			default:
				handled = false;
			}
			if(handled) continue; // Get the next packet.
		}
		// How about the rest?
		switch(netbuffer.msg.type)
		{
		case pkt_democam:
		case pkt_democam_resume:
			Demo_ReadLocalCamera();
			break;

		case pkt_ping:
			Net_PingResponse();
			break;

		case psv_sync:
			// The server updates our time. Latency has been taken into
			// account, so...
			gametic = Msg_ReadLong();
			Con_Printf("psv_sync: gametic=%i\n", gametic);
			Net_ResetTimer();
			break;

		case psv_handshake:
			Cl_AnswerHandshake( (handshake_packet_t*) netbuffer.msg.data);
			break;
			
		case pkt_player_info:
			Cl_HandlePlayerInfo( (playerinfo_packet_t*) netbuffer.msg.data);		
			break;
			
		case psv_player_exit:
			Cl_PlayerLeaves(Msg_ReadByte());
			break;
			
		case pkt_chat:
			Net_ShowChatMessage();
			gx.NetPlayerEvent(netbuffer.msg.data[0], DDPE_CHAT_MESSAGE,
				netbuffer.msg.data + 3);
			break;
			
		case psv_server_close:	// We should quit?
			netLoggedIn = false;
			Con_Execute("net disconnect", true);		
			break;

		case psv_console_text:
			i = Msg_ReadLong();
			Con_FPrintf(i, netbuffer.cursor);
			break;

		case pkt_login:
			// Server responds to our login request. Let's see if we 
			// were successful.
			netLoggedIn = Msg_ReadByte();
			break;

		default:
			if(netbuffer.msg.type >= pkt_game_marker)
			{
				gx.HandlePacket(netbuffer.player,
					netbuffer.msg.type, netbuffer.msg.data,
					netbuffer.length);
			}
		}
	}
}

//===========================================================================
// Cl_Ticker
//===========================================================================
void Cl_Ticker(timespan_t time)
{
	static trigger_t fixed = { 1.0/35 };

	if(!Cl_GameReady() || clientPaused) return;

	if(!M_CheckTrigger(&fixed, time)) return;

	Cl_LocalCommand();
	Cl_PredictMovement();	
	Cl_MovePsprites();
}

//========================================================================
// CCmdLogin
//	Clients use this to establish a remote connection to the server.
//========================================================================
int CCmdLogin(int argc, char **argv)
{
	// Only clients can log in.
	if(!isClient) return false;
	Msg_Begin(pkt_login);
	// Write the password.
	if(argc == 1)
		Msg_WriteByte(0);	// No password given!
	else
		Msg_Write(argv[1], strlen(argv[1]) + 1);
	Net_SendBuffer(0, SPF_ORDERED);
	return true;
}