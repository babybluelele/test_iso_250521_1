/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_OTHER_COMMANDS_HXX
#define MPD_OTHER_COMMANDS_HXX

#include "CommandResult.hxx"
#include <string>
class Client;
class Request;
class Response;

struct setAlsa{
	std::string sndName;
	std::string DSDType;
	std::string softwareVol;
	std::string allowFormat;
};

CommandResult
handle_urlhandlers(Client &client, Request request, Response &response);

CommandResult
handle_decoders(Client &client, Request request, Response &response);

CommandResult
handle_kill(Client &client, Request request, Response &response);

CommandResult
handle_listfiles(Client &client, Request request, Response &response);

CommandResult
handle_lsinfo(Client &client, Request request, Response &response);

CommandResult
handle_update(Client &client, Request request, Response &response);

CommandResult
handle_rescan(Client &client, Request request, Response &response);

CommandResult
handle_setvol(Client &client, Request request, Response &response);

CommandResult
handle_volume(Client &client, Request request, Response &response);

CommandResult
handle_stats(Client &client, Request request, Response &response);

CommandResult
handle_config(Client &client, Request request, Response &response);

CommandResult
handle_idle(Client &client, Request request, Response &response);

void handle_set_streamKey(std::string &p, std::string &vec);
void handle_get_streamKey(std::string &p, std::string &vec);
void handle_set_qqTrackType(std::string &qqTrackType);
void handle_get_qqTrackType(std::string &qqTrackType);
void handle_get_alsa(setAlsa & sa);
void handle_set_sndName(std::string & sndName);
void handle_set_DSDType(std::string & DSDType);
void handle_set_softwareVol(std::string & software);
void handle_set_allowFormat(std::string & allowFormat);

#endif
