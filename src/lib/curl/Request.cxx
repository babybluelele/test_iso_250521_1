/*
 * Copyright 2008-2018 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Request.hxx"
#include "Global.hxx"
#include "Handler.hxx"
#include "event/Call.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringStrip.hxx"
#include "util/StringView.hxx"
#include "util/CharUtil.hxx"
#include "Version.h"
#include "command/OtherCommands.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
static constexpr Domain curl_request_domain("curlRequest");

#include <curl/curl.h>

#include <algorithm>
#include <cassert>

#include <string.h>

CurlRequest::CurlRequest(CurlGlobal &_global,
			 CurlResponseHandler &_handler)
	:global(_global), handler(_handler),
	 postpone_error_event(global.GetEventLoop(),
			      BIND_THIS_METHOD(OnPostponeError))
{
	error_buffer[0] = 0;

	easy.SetPrivate((void *)this);
	easy.SetUserAgent("Music Player Daemon " VERSION);
	easy.SetHeaderFunction(_HeaderFunction, this);
	easy.SetWriteFunction(WriteFunction, this);
#if !defined(ANDROID) && !defined(_WIN32)
	easy.SetOption(CURLOPT_NETRC, 1L);
#endif
	easy.SetErrorBuffer(error_buffer);
	easy.SetNoProgress();
	easy.SetNoSignal();
	easy.SetConnectTimeout(15);
	easy.SetOption(CURLOPT_HTTPAUTH, (long) CURLAUTH_ANY);

	de_content_length = 0;
	qqHandler = nullptr;
}

CurlRequest::~CurlRequest() noexcept
{
	qqEkey.clear();
	if(qqHandler == nullptr) DestroyDecryption(qqHandler);
	FreeEasy();
}

void
CurlRequest::Start()
{
	assert(!registered);

	global.Add(*this);
	registered = true;
}

void
CurlRequest::StartIndirect()
{
	BlockingCall(global.GetEventLoop(), [this](){
			Start();
		});
}

void
CurlRequest::Stop() noexcept
{
	if (!registered)
		return;

	global.Remove(*this);
	registered = false;
}

void
CurlRequest::StopIndirect()
{
	BlockingCall(global.GetEventLoop(), [this](){
			Stop();
		});
}

void
CurlRequest::FreeEasy() noexcept
{
	if (!easy)
		return;

	Stop();
	easy = nullptr;
}

void
CurlRequest::Resume() noexcept
{
	assert(registered);

	easy.Unpause();

	global.InvalidateSockets();
}

void
CurlRequest::FinishHeaders()
{
	if (state != State::HEADERS)
		return;

	state = State::BODY;

	long status = 0;
	easy.GetInfo(CURLINFO_RESPONSE_CODE, &status);

	handler.OnHeaders(status, std::move(headers));
}

void
CurlRequest::FinishBody()
{
	FinishHeaders();

	if (state != State::BODY)
		return;

	state = State::CLOSED;
	handler.OnEnd();
}

void
CurlRequest::Done(CURLcode result) noexcept
{
	Stop();

	try {
		if (result != CURLE_OK) {
			StripRight(error_buffer);
			const char *msg = error_buffer;
			if (*msg == 0)
				msg = curl_easy_strerror(result);
			throw FormatRuntimeError("CURL failed: %s", msg);
		}
		FormatNotice(curl_request_domain, "*** curl done");
		FinishBody();
	} catch (...) {
		FormatNotice(curl_request_domain, "*** curl err(%d)", result);
		state = State::CLOSED;
		handler.OnError(std::current_exception());
	}
}

gcc_pure
static bool
IsResponseBoundaryHeader(StringView s) noexcept
{
	return s.size > 5 && (s.StartsWith("HTTP/") ||
			      /* the proprietary "ICY 200 OK" is
				 emitted by Shoutcast */
			      s.StartsWith("ICY 2"));
}

inline void
CurlRequest::HeaderFunction(StringView s) noexcept
{
	if (state > State::HEADERS)
		return;

	if (IsResponseBoundaryHeader(s)) {
		/* this is the boundary to a new response, for example
		   after a redirect */
		headers.clear();
		return;
	}

	const char *header = s.data;
	const char *end = StripRight(header, header + s.size);

	const char *value = s.Find(':');
	if (value == nullptr)
		return;

	std::string name(header, value);
	std::transform(name.begin(), name.end(), name.begin(),
		       static_cast<char(*)(char)>(ToLowerASCII));

	/* skip the colon */

	++value;

	/* strip the value */

	value = StripLeft(value, end);
	end = StripRight(value, end);

		//Content-Range: bytes 100000000-153989863/153989864
	if(strncmp("content-length", name.c_str(), 14) == 0)
	{
		content_length = std::atoi(value);
		handler.OnDataSize(content_length);
		FormatNotice(curl_request_domain, "c.content_length %zu ", content_length);
	}

	if(strncmp("content-range", name.c_str(), 13) == 0)
	{
		FormatNotice(curl_request_domain, "c.content_range %s ", value);
		std::string v = value;
		std::string v2 = v.substr(v.find("/") + 1, v.size() - v.find("/") - 1);
		content_range = std::atoi(v2.c_str());
		handler.OnDataSizetotal(content_range);
		FormatNotice(curl_request_domain, "c.content_range %zu ", content_range);

		std::string v1 = v.substr(v.find("bytes ") + 6, v.rfind("-") - v.find("bytes ") - 6);
		de_content_length = std::atoi(v1.c_str());
		handler.OnDataStartPos(de_content_length);
		FormatNotice(curl_request_domain, "c.de_content_length %zu ", de_content_length);
	}

	if("content-type" == name )
	{
		FormatNotice(curl_request_domain, " %s = %s", name.c_str(), value);
		if(checkBaiduMusic())
		{
			FormatNotice(curl_request_domain, "baiduMusic.....");
			//audio/x-dsf	audio/x-dsd
			//audio/ogg
			//mp3  audio/mpeg
			//wav, m4a,mp3, flac,aac,ogg, dsf,dff,mp4，ape，aiff
			if(checkBaiduMusicDff())
			{
				headers.emplace(std::move(name), std::string("audio/x-dsd"));
			}
			else if(checkBaiduMusicDsf())
			{
				headers.emplace(std::move(name), std::string("audio/x-dsf"));
			}
			else if(checkBaiduMusicAiff())
			{
				headers.emplace(std::move(name), std::string("audio/aiff"));
			}
			else if(checkBaiduMusicFlac())
			{
				headers.emplace(std::move(name), std::string("audio/flac"));
			}
			else if(checkBaiduMusicWav())
			{
				headers.emplace(std::move(name), std::string("audio/x-wav"));
			}
			// else if(checkBaiduMusicOgg())
			// {
			// 	headers.emplace(std::move(name), std::string("audio/ogg"));
			// }
			else if(checkBaiduMusicMp3())
			{
				headers.emplace(std::move(name), std::string("audio/mpeg"));
			}
			else
			{
				//audio/mp4   m4a
				//audio/vnd.dlna.adts aac
				//m4a aac mp4
				//headers.emplace(std::move(name), std::string(value, end));
				headers.emplace(std::move(name), std::string("audio/x-mpd-ffmpeg"));
				FormatNotice(curl_request_domain, "set audio/x-mpd-ffmpeg");
			}
		}
		else if( checkWangyiMusic() || checkWangyiMusicLocal() )	
		{
			FormatNotice(curl_request_domain, "wangyiMusic.....");
			if( strncmp("audio/mpeg", value, 10) == 0 ||
			strncmp("video/quicktime", value, 15) == 0 )
			{
				headers.emplace(std::move(name), std::string("audio/x-mpd-ffmpeg"));
				FormatNotice(curl_request_domain, "set audio/x-mpd-ffmpeg");
			}
			else
			{
				headers.emplace(std::move(name), std::string(value, end));
			}
		}
		else if(checkDropboxMusic())
		{
			FormatNotice(curl_request_domain, "Dropbox.....");
			//audio/x-dsf	audio/x-dsd
			//audio/ogg
			//mp3  audio/mpeg
			//wav, m4a,mp3, flac,aac,ogg, dsf,dff,mp4，ape，aiff
			if(checkDropboxMusicDff())
			{
				headers.emplace(std::move(name), std::string("audio/x-dsd"));
			}
			else if(checkDropboxMusicDsf())
			{
				headers.emplace(std::move(name), std::string("audio/x-dsf"));
			}
			else if(checkDropboxMusicAiff())
			{
				headers.emplace(std::move(name), std::string("audio/aiff"));
			}
			else if(checkDropboxMusicFlac())
			{
				headers.emplace(std::move(name), std::string("audio/flac"));
			}
			else if(checkDropboxMusicWav())
			{
				headers.emplace(std::move(name), std::string("audio/x-wav"));
			}
			// else if(checkDropboxMusicOgg())
			// {
			// 	headers.emplace(std::move(name), std::string("audio/ogg"));
			// }
			else if(checkDropboxMusicMp3())
			{
				headers.emplace(std::move(name), std::string("audio/mpeg"));
			}
			else
			{
				//audio/mp4   m4a
				//audio/vnd.dlna.adts aac
				//m4a aac mp4
				//headers.emplace(std::move(name), std::string(value, end));
				headers.emplace(std::move(name), std::string("audio/x-mpd-ffmpeg"));
				FormatNotice(curl_request_domain, "set audio/x-mpd-ffmpeg");
			}
		}
		//end ddropbox
		
		if(checkQQMusic())
		{
			std::string qqTrackType;
			handle_get_qqTrackType(qqTrackType);
			FormatNotice(curl_request_domain, "qq type %s", qqTrackType.c_str());
			if("flac" == qqTrackType)
			{
				headers.emplace(std::move(name), std::string("audio/flac"));
			}
			else
			{
				headers.emplace(std::move(name), std::string(value, end));
			}
		}
		else
		{
			headers.emplace(std::move(name), std::string(value, end));
		}
	}
	else
	{
		headers.emplace(std::move(name), std::string(value, end));
	}
}

size_t
CurlRequest::_HeaderFunction(char *ptr, size_t size, size_t nmemb,
			     void *stream) noexcept
{
	CurlRequest &c = *(CurlRequest *)stream;

	size *= nmemb;

	c.HeaderFunction({ptr, size});
	return size;
}

inline size_t
CurlRequest::DataReceived(const void *ptr, size_t received_size) noexcept
{
	assert(received_size > 0);

	try {
		FinishHeaders();
		handler.OnData({ptr, received_size});
		return received_size;
	} catch (Pause) {
		return CURL_WRITEFUNC_PAUSE;
	} catch (...) {
		state = State::CLOSED;
		/* move the CurlResponseHandler::OnError() call into a
		   "safe" stack frame */
		postponed_error = std::current_exception();
		postpone_error_event.Schedule();
		return CURL_WRITEFUNC_PAUSE;
	}

}

size_t
CurlRequest::WriteFunction(char *ptr, size_t size, size_t nmemb,
			   void *stream) noexcept
{
	CurlRequest &c = *(CurlRequest *)stream;

	size *= nmemb;
	if (size == 0)
		return 0;
	if(c.checkQQMusic())
	{
		if(c.qqHandler != nullptr)
		{
			Decrypt(c.qqHandler, c.de_content_length, ptr, size);
			c.de_content_length += size;
		}
		else
		{
			std::string iv;
			handle_get_streamKey(c.qqEkey, iv);

			if(c.qqEkey != "noEkey")
			{	c.qqHandler = CreateDecryption((const unsigned char*)c.qqEkey.c_str(), c.qqEkey.size());
				if(c.qqHandler != nullptr)
				{
					Decrypt(c.qqHandler, c.de_content_length, ptr, size);
					c.de_content_length += size;
				}
			}
		}
	}
	return c.DataReceived(ptr, size);
}

void
CurlRequest::OnPostponeError() noexcept
{
	assert(postponed_error);

	handler.OnError(postponed_error);
}
