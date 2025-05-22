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

#include "CuePlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "../cue/CueParser.hxx"
#include "input/TextInputStream.hxx"
#include "util/StringView.hxx"
#include "util/ConvertStr.hxx"

#include <sstream>

struct CueFileBuffer{
	std::string data;
	int totalLine;
	int readLine;
	int isutf8;
};
static CueFileBuffer cfbuf;
class CuePlaylist final : public SongEnumerator {
	TextInputStream tis;
	CueParser parser;

 public:
	explicit CuePlaylist(InputStreamPtr &&is)
		:tis(std::move(is)) {
	}

	std::unique_ptr<DetachedSong> NextSong() override;
};

static std::unique_ptr<SongEnumerator>
cue_playlist_open_stream(InputStreamPtr &&is)
{
	cfbuf.totalLine = 0;
	cfbuf.readLine = 0;
	cfbuf.isutf8 = 0;
	cfbuf.data.clear();
	return std::make_unique<CuePlaylist>(std::move(is));
}

std::unique_ptr<DetachedSong>
CuePlaylist::NextSong()
{
	auto song = parser.Get();
	if (song != nullptr)
		return song;

	const char *line0;
	std::string str;
	while ((line0 = tis.ReadLine()) != nullptr) {
		str += line0;
		str += "\n";
		cfbuf.totalLine++;
		cfbuf.readLine = 0;
		cfbuf.isutf8 = 0;
		cfbuf.data = str;
	}

	//printf("text size: %ld\n", str.size());
	char *buf = new char[str.size() + 1];
	char *utf = new char[str.size() * 2];
	if(buf && utf)
	{
		strncpy(buf, str.c_str(), str.size());
		buf[str.size()] = '\0';

		if(0 == convert_isUTF8(buf, str.size()))
		{
			if(0 == convert_convertstr(buf, str.size(), utf, str.size() * 2))
			{
				//cfbuf.isutf8 = 1;
				cfbuf.data = utf;
				printf("is utf8\n");
			}
		}
	}

	if(buf != nullptr) delete [] buf;
	if(utf != nullptr) delete [] utf;

		std::istringstream input2;
		input2.str(cfbuf.data);
		std::string line;
		if(cfbuf.readLine == 0)
		{
			while(std::getline(input2, line, '\n')) 
			{
				cfbuf.readLine++;
				printf("line: %s ,%ld\n", line.c_str(), line.size());
				parser.Feed(line.c_str());
				song = parser.Get();
				if (song != nullptr)
					return song;
			}
		}
		else if(cfbuf.readLine < cfbuf.totalLine)
		{
			int i = 0;
			while(std::getline(input2, line, '\n')) 
			{
				i++;
				if(i > cfbuf.readLine && i <= cfbuf.totalLine)
				{
					printf("line: %s ,%ld\n", line.c_str(), line.size());
					parser.Feed(line.c_str());
					song = parser.Get();
					if (song != nullptr)
					{
						cfbuf.readLine = i;
						return song;
					}
				}
			}
			if(i > cfbuf.readLine) cfbuf.readLine = i;
		}

	parser.Finish();
	return parser.Get();
}

static const char *const cue_playlist_suffixes[] = {
	"cue",
	nullptr
};

static const char *const cue_playlist_mime_types[] = {
	"application/x-cue",
	nullptr
};

const PlaylistPlugin cue_playlist_plugin =
	PlaylistPlugin("cue", cue_playlist_open_stream)
	.WithAsFolder()
	.WithSuffixes(cue_playlist_suffixes)
	.WithMimeTypes(cue_playlist_mime_types);
