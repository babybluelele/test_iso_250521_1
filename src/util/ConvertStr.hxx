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

#ifndef CONVERT_STR_HXX
#define CONVERT_STR_HXX

#include <stdio.h>
#include <string.h>
#include <iconv.h>

int convert_big5ToGbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len);

int convert_GBkToUtf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len);

int convert_GBkToGBK(char *src_str, size_t src_len, char *dst_str, size_t dst_len);
int convert_utf8Toutf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len);

int convert_isUTF8(char *str, size_t len);
int convert_convertstr(char *src_str, size_t src_len, char *utf, size_t dst_len);
void mpd_softwareVolume_set(int vol);
int mpd_softwareVolume_get(void);
void mpd_softwareVolumeStatus_set(bool status);
bool mpd_softwareVolumeStatus_get(void);

#endif
