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

#ifndef MA_MPD_MUTE_HXX
#define MA_MPD_MUTE_HXX

//#include <chrono>
#include <string>
//void handle_mpd_mute_wait(int totalWaitTimeMillis, int checkIntervalMillis);
bool  handle_mpd_mute_parseLine_fist(std::string line);
int handle_mpd_mute_parseLine_do(void);
void handle_mpd_mute_setDacDelay(std::string dacdelay);
bool handle_mpd_mute_getDacDelay();
void handle_send_maPlayerAction(std::string audio_old,std::string audio_new);
void handle_mpd_mute_setOutput(std::string output);
std::string handle_mpd_mute_getDacDelay_str();
std::string handle_mpd_mute_getOutput_str();
void handle_mpd_mute_setUsbLimit(std::string usblimit);
std::string handle_mpd_mute_getUsbLimit_str();
#endif
