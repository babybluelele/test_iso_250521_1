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

#include "config.h"
#include "PlayerCommands.hxx"
#include "Request.hxx"
#include "queue/Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "SingleMode.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "mixer/Volume.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "IdleFlags.hxx"
#include "util/StringBuffer.hxx"
#include "util/ScopeExit.hxx"
#include "util/Exception.hxx"
#include "util/Math.hxx"
#include "util/ConRaat.hxx"

#ifdef ENABLE_DATABASE
#include "db/update/Service.hxx"
#endif
#include "Log.hxx"
#include "util/Domain.hxx"
#include "MaMpdMute.hxx"
#include <cstring>
static constexpr Domain othercommand_domain("othercommand");

#define COMMAND_STATUS_STATE            "state"
#define COMMAND_STATUS_REPEAT           "repeat"
#define COMMAND_STATUS_SINGLE           "single"
#define COMMAND_STATUS_CONSUME          "consume"
#define COMMAND_STATUS_RANDOM           "random"
#define COMMAND_STATUS_PLAYLIST         "playlist"
#define COMMAND_STATUS_PLAYLIST_LENGTH  "playlistlength"
#define COMMAND_STATUS_SONG             "song"
#define COMMAND_STATUS_SONGID           "songid"
#define COMMAND_STATUS_NEXTSONG         "nextsong"
#define COMMAND_STATUS_NEXTSONGID       "nextsongid"
#define COMMAND_STATUS_TIME             "time"
#define COMMAND_STATUS_BITRATE          "bitrate"
#define COMMAND_STATUS_ERROR            "error"
#define COMMAND_STATUS_CROSSFADE	"xfade"
#define COMMAND_STATUS_MIXRAMPDB	"mixrampdb"
#define COMMAND_STATUS_MIXRAMPDELAY	"mixrampdelay"
#define COMMAND_STATUS_AUDIO		"audio"
#define COMMAND_STATUS_UPDATING_DB	"updating_db"

CommandResult
handle_play(Client &client, Request args, [[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"play\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
	int song = args.ParseOptional(0, -1);

	client.GetPartition().PlayPosition(song);
	return CommandResult::OK;
}

CommandResult
handle_playid(Client &client, Request args, [[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"playid\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
	int id = args.ParseOptional(0, -1);

	client.GetPartition().PlayId(id);
	return CommandResult::OK;
}

CommandResult
handle_stop(Client &client, [[maybe_unused]] Request args, [[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"stop\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
	client.GetPartition().Stop();
	return CommandResult::OK;
}

CommandResult
handle_currentsong(Client &client, [[maybe_unused]] Request args, Response &r)
{
	playlist_print_current(r, client.GetPlaylist());
	return CommandResult::OK;
}

CommandResult
handle_pause(Client &client, Request args, [[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"pause\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
	auto &pc = client.GetPlayerControl();

	if (!args.empty()) {
		bool pause_flag = args.ParseBool(0);
		pc.LockSetPause(pause_flag);
	} else
		pc.LockPause();

	return CommandResult::OK;
}

CommandResult
handle_status(Client &client, [[maybe_unused]] Request args, Response &r)
{
	auto &partition = client.GetPartition();
	auto &pc = partition.pc;

	const char *state = nullptr;
	int song;

	const auto player_status = pc.LockGetStatus();

	switch (player_status.state) {
	case PlayerState::STOP:
		state = "stop";
		break;
	case PlayerState::PAUSE:
		state = "pause";
		break;
	case PlayerState::PLAY:
		state = "play";
		break;
	}

	const auto &playlist = partition.playlist;

	const auto volume = volume_level_get(partition.outputs);
	if (volume >= 0)
		r.Format("volume: %i\n", volume);

	r.Format(COMMAND_STATUS_REPEAT ": %i\n"
		 COMMAND_STATUS_RANDOM ": %i\n"
		 COMMAND_STATUS_SINGLE ": %s\n"
		 COMMAND_STATUS_CONSUME ": %i\n"
		 "partition: %s\n"
		 COMMAND_STATUS_PLAYLIST ": %li\n"
		 COMMAND_STATUS_PLAYLIST_LENGTH ": %i\n"
		 COMMAND_STATUS_MIXRAMPDB ": %f\n"
		 COMMAND_STATUS_STATE ": %s\n",
		 playlist.GetRepeat(),
		 playlist.GetRandom(),
		 SingleToString(playlist.GetSingle()),
		 playlist.GetConsume(),
		 partition.name.c_str(),
		 (unsigned long)playlist.GetVersion(),
		 playlist.GetLength(),
		 (double)pc.GetMixRampDb(),
		 state);

	if (pc.GetCrossFade() > FloatDuration::zero())
		r.Format(COMMAND_STATUS_CROSSFADE ": %lu\n",
			 lround(pc.GetCrossFade().count()));

	if (pc.GetMixRampDelay() > FloatDuration::zero())
		r.Format(COMMAND_STATUS_MIXRAMPDELAY ": %f\n",
			 pc.GetMixRampDelay().count());

	song = playlist.GetCurrentPosition();
	if (song >= 0) {
		r.Format(COMMAND_STATUS_SONG ": %i\n"
			 COMMAND_STATUS_SONGID ": %u\n",
			 song, playlist.PositionToId(song));
	}

	if (player_status.state != PlayerState::STOP) {
		r.Format(COMMAND_STATUS_TIME ": %i:%i\n"
			 "elapsed: %1.3f\n"
			 COMMAND_STATUS_BITRATE ": %u\n",
			 player_status.elapsed_time.RoundS(),
			 player_status.total_time.IsNegative()
			 ? 0U
			 : unsigned(player_status.total_time.RoundS()),
			 player_status.elapsed_time.ToDoubleS(),
			 player_status.bit_rate);

		if (!player_status.total_time.IsNegative())
			r.Format("duration: %1.3f\n",
				 player_status.total_time.ToDoubleS());

		if (player_status.audio_format.IsDefined())
			r.Format(COMMAND_STATUS_AUDIO ": %s\n",
				 ToString(player_status.audio_format).c_str());
	}
#ifdef ENABLE_DATABASE
	const UpdateService *update_service = partition.instance.update;
	unsigned updateJobId = update_service != nullptr
		? update_service->GetId()
		: 0;
	if (updateJobId != 0) {
		r.Format(COMMAND_STATUS_UPDATING_DB ": %i\n",
			 updateJobId);
	}
#endif

	try {
		pc.LockCheckRethrowError();
	} catch (...) {
		r.Format(COMMAND_STATUS_ERROR ": %s\n",
			 GetFullMessage(std::current_exception()).c_str());
	}

	song = playlist.GetNextPosition();
	if (song >= 0)
		r.Format(COMMAND_STATUS_NEXTSONG ": %i\n"
			 COMMAND_STATUS_NEXTSONGID ": %u\n",
			 song, playlist.PositionToId(song));

	
	std::string dacDelay = handle_mpd_mute_getDacDelay_str();
	std::string output = handle_mpd_mute_getOutput_str();
	std::string usbLimit = handle_mpd_mute_getUsbLimit_str();
	r.Format("zdacdelay: %s\n"
			"zoutput: %s\n"
			"zusblimit: %s\n",
			dacDelay.c_str(),
			output.c_str(),
			usbLimit.c_str()
			);
			
	return CommandResult::OK;
}

CommandResult
handle_next(Client &client, [[maybe_unused]] Request args, [[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"next\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
	playlist &playlist = client.GetPlaylist();

	/* single mode is not considered when this is user who
	 * wants to change song. */
	const SingleMode single = playlist.queue.single;
	playlist.queue.single = SingleMode::OFF;

	AtScopeExit(&playlist, single) {
		playlist.queue.single = single;
	};

	client.GetPartition().PlayNext();
	return CommandResult::OK;
}

CommandResult
handle_previous(Client &client, [[maybe_unused]] Request args,
		[[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"prev\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
	client.GetPartition().PlayPrevious();
	return CommandResult::OK;
}

CommandResult
handle_repeat(Client &client, Request args, [[maybe_unused]] Response &r)
{
	bool status = args.ParseBool(0);
	client.GetPartition().SetRepeat(status);
	return CommandResult::OK;
}

CommandResult
handle_single(Client &client, Request args, [[maybe_unused]] Response &r)
{
	auto new_mode = SingleFromString(args.front());
	client.GetPartition().SetSingle(new_mode);
	return CommandResult::OK;
}

CommandResult
handle_consume(Client &client, Request args, [[maybe_unused]] Response &r)
{
	bool status = args.ParseBool(0);
	client.GetPartition().SetConsume(status);
	return CommandResult::OK;
}

CommandResult
handle_random(Client &client, Request args, [[maybe_unused]] Response &r)
{
	bool status = args.ParseBool(0);
	auto &partition = client.GetPartition();
	partition.SetRandom(status);
	partition.UpdateEffectiveReplayGainMode();
	return CommandResult::OK;
}

CommandResult
handle_clearerror(Client &client, [[maybe_unused]] Request args,
		  [[maybe_unused]] Response &r)
{
	client.GetPlayerControl().LockClearError();
	return CommandResult::OK;
}

CommandResult
handle_seek(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned song = args.ParseUnsigned(0);
	SongTime seek_time = args.ParseSongTime(1);

	client.GetPartition().SeekSongPosition(song, seek_time);
	return CommandResult::OK;
}

CommandResult
handle_seekid(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned id = args.ParseUnsigned(0);
	SongTime seek_time = args.ParseSongTime(1);

	client.GetPartition().SeekSongId(id, seek_time);
	return CommandResult::OK;
}

CommandResult
handle_seekcur(Client &client, Request args, [[maybe_unused]] Response &r)
{
	std::string cmd("{\"cmd\":\"maPlayerAction\",\"value\":\"seek\",\"value2\":\"");
	std::string tail("\"}\r\n");
	std::string mstSocket("/opt/mads-sock");
	
	const char *p = args.front();
	FormatNotice(othercommand_domain, "seek (%s)", p);
	cmd += p;
	cmd += tail;
	
	ConRaat cr(-1, cmd, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);

	bool relative = *p == '+' || *p == '-';
	SignedSongTime seek_time = ParseCommandArgSignedSongTime(p);

	client.GetPartition().SeekCurrent(seek_time, relative);
	return CommandResult::OK;
}

CommandResult
handle_crossfade(Client &client, Request args, [[maybe_unused]] Response &r)
{
	FloatDuration duration{args.ParseUnsigned(0)};
	client.GetPlayerControl().SetCrossFade(duration);
	return CommandResult::OK;
}

CommandResult
handle_mixrampdb(Client &client, Request args, [[maybe_unused]] Response &r)
{
	float db = args.ParseFloat(0);
	client.GetPlayerControl().SetMixRampDb(db);
	return CommandResult::OK;
}

CommandResult
handle_mixrampdelay(Client &client, Request args, [[maybe_unused]] Response &r)
{
	FloatDuration delay_secs{args.ParseFloat(0)};
	client.GetPlayerControl().SetMixRampDelay(delay_secs);
	return CommandResult::OK;
}

CommandResult
handle_replay_gain_mode(Client &client, Request args, Response &)
{
	auto new_mode = FromString(args.front());
	auto &partition = client.GetPartition();
	partition.SetReplayGainMode(new_mode);
	partition.EmitIdle(IDLE_OPTIONS);
	return CommandResult::OK;
}

CommandResult
handle_replay_gain_status(Client &client, [[maybe_unused]] Request args,
			  Response &r)
{
	r.Format("replay_gain_mode: %s\n",
		 ToString(client.GetPartition().replay_gain_mode));
	return CommandResult::OK;
}

//设置vDacDelay延迟
//命令方式为  on off
CommandResult
handle_ma_set_dacDelay_status([[maybe_unused]] Client &client, Request args, [[maybe_unused]] Response &r)
{
	const char *str = args.front();
    if (strcmp(str, "on") == 0) {
       handle_mpd_mute_setDacDelay("on");
    } else {
       handle_mpd_mute_setDacDelay("off");
    }
	return CommandResult::OK;
}
CommandResult
handle_ma_set_maOutput([[maybe_unused]] Client &client, Request args, [[maybe_unused]] Response &r)
{
	const char *str = args.front();
    if (strcmp(str, "DAC") == 0) {
       handle_mpd_mute_setOutput("DAC");
    } else {
       handle_mpd_mute_setOutput("digital");
    }
	return CommandResult::OK;
}
#include <iostream> 
CommandResult
handle_ma_set_maData([[maybe_unused]] Client &client, Request args, [[maybe_unused]] Response &r)
{
    if (args.size == 1)
	{
		const char *const dacDelay = args[0];
		std::cout << "LELE dacDelay: " << dacDelay << std::endl;
        //设置延迟开关
		if(strcmp(dacDelay, "on") == 0 || strcmp(dacDelay, "off") == 0 )
		{
			handle_mpd_mute_setDacDelay(dacDelay);
		}
		else
		{
			std::cout << "error dacDelay: " << dacDelay << std::endl;
		}
	}
	else if (args.size == 2)
	{
		const char *const dacDelay = args[0];
		const char *const outPut = args[1];
		std::cout << "LELE dacDelay: " << dacDelay << std::endl;
		std::cout << "LELE outPut: " << outPut << std::endl;
        //设置延迟开关
		if(strcmp(dacDelay, "on") == 0 || strcmp(dacDelay, "off") == 0)
		{
			handle_mpd_mute_setDacDelay(dacDelay);
		}
		else
		{
			std::cout << "error dacDelay: " << dacDelay << std::endl;
		}
        //设置输出方式
		if(strcmp(outPut, "DAC") == 0 || strcmp(outPut, "digital") == 0 )
		{
			handle_mpd_mute_setOutput(outPut);
		}
		else
		{
			std::cout << "error outPut: " << outPut << std::endl;
		}
	}
	else if (args.size == 3)
	{
		const char *const dacDelay = args[0];
		const char *const outPut = args[1];
		const char *const usbDaclimit = args[2];

		std::cout << "LELE dacDelay: " << dacDelay << std::endl;
		std::cout << "LELE outPut: " << outPut << std::endl;
		std::cout << "LELE usbDaclimit: " << usbDaclimit << std::endl;

        //设置延迟开关
		if(strcmp(dacDelay, "on") == 0 || strcmp(dacDelay, "off") == 0 )
		{
			handle_mpd_mute_setDacDelay(dacDelay);
		}
		else
		{
			std::cout << "error dacDelay: " << dacDelay << std::endl;
		}
        //设置输出方式
		if(strcmp(outPut, "DAC") == 0 || strcmp(outPut, "digital") == 0 )
		{
			handle_mpd_mute_setOutput(outPut);
		}
		else
		{
			std::cout << "error outPut: " << outPut << std::endl;
		}

 
		if(strcmp(usbDaclimit, "768k") == 0 ||
		strcmp(usbDaclimit, "705.6k") == 0 ||
		strcmp(usbDaclimit, "384k") == 0 ||
		strcmp(usbDaclimit, "352.8k") == 0 ||
		strcmp(usbDaclimit, "192k") == 0 ||
		strcmp(usbDaclimit, "176.4k") == 0 || 
		strcmp(usbDaclimit, "96k") == 0 || 
		strcmp(usbDaclimit, "88.2k") == 0 || 
		strcmp(usbDaclimit, "48k") == 0 || 
		strcmp(usbDaclimit, "44.1k") == 0 )
		{
			handle_mpd_mute_setUsbLimit(usbDaclimit);
		}
		else
		{
			std::cout << "error usbDaclimit: " << usbDaclimit << std::endl;
		}
	}
	return CommandResult::OK;
}