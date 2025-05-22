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

#ifndef CURL_REQUEST_HXX
#define CURL_REQUEST_HXX

#include "Easy.hxx"
#include "event/DeferEvent.hxx"

#include <map>
#include <string>
#include <exception>
#include "QMDecryption.h"
struct StringView;
class CurlGlobal;
class CurlResponseHandler;

class CurlRequest final {
	CurlGlobal &global;

	CurlResponseHandler &handler;

	/** the curl handle */
	CurlEasy easy;

	enum class State {
		HEADERS,
		BODY,
		CLOSED,
	} state = State::HEADERS;

	std::multimap<std::string, std::string> headers;
	std::string tmpUrl;

	/**
	 * An exception caught by DataReceived(), which will be
	 * forwarded into a "safe" stack frame by
	 * #postpone_error_event.  This works around the
	 * problem that libcurl crashes if you call
	 * curl_multi_remove_handle() from within the WRITEFUNCTION
	 * (i.e. DataReceived()).
	 */
	std::exception_ptr postponed_error;

	DeferEvent postpone_error_event;

	/** error message provided by libcurl */
	char error_buffer[CURL_ERROR_SIZE];

	bool registered = false;

public:
	size_t content_length;
	size_t content_range;
	size_t de_content_length;
	DecryptionHandle qqHandler;
	std::string qqEkey;
	/**
	 * To start sending the request, call Start().
	 */
	CurlRequest(CurlGlobal &_global,
		    CurlResponseHandler &_handler);

	CurlRequest(CurlGlobal &_global, const char *url,
		    CurlResponseHandler &_handler)
		:CurlRequest(_global, _handler) {
		SetUrl(url);
	}

	~CurlRequest() noexcept;

	CurlRequest(const CurlRequest &) = delete;
	CurlRequest &operator=(const CurlRequest &) = delete;

	/**
	 * Register this request via CurlGlobal::Add(), which starts
	 * the request.
	 *
	 * This method must be called in the event loop thread.
	 */
	void Start();

	/**
	 * A thread-safe version of Start().
	 */
	void StartIndirect();

	/**
	 * Unregister this request via CurlGlobal::Remove().
	 *
	 * This method must be called in the event loop thread.
	 */
	void Stop() noexcept;

	/**
	 * A thread-safe version of Stop().
	 */
	void StopIndirect();

	CURL *Get() noexcept {
		return easy.Get();
	}

	template<typename T>
	void SetOption(CURLoption option, T value) {
		easy.SetOption(option, value);
	}

	void SetUrl(const char *url) {
		tmpUrl = url;
		easy.SetURL(url);
	}

	bool checkQQMusic()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/QQPlay?id=");
		if(n != std::string::npos)
		{
			return true;
		}
		
		return false;
	}

	bool checkQobuzMusic()
	{
		//http://127.0.0.1:8899/qobuzPlay?id=
		size_t n = tmpUrl.find("http://127.0.0.1:8899/qobuzPlay?id=");
		if(n != std::string::npos)
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusic()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		if(n != std::string::npos)
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicDff()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".dff&id=");
		size_t n3 = tmpUrl.find(".dff&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicDsf()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".dsf&id=");
		size_t n3 = tmpUrl.find(".dsf&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicAiff()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".aiff&id=");
		size_t n3 = tmpUrl.find(".aiff&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicFlac()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".flac&id=");
		size_t n3 = tmpUrl.find(".flac&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicWav()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".wav&id=");
		size_t n3 = tmpUrl.find(".wav&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicOgg()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".ogg&id=");
		size_t n3 = tmpUrl.find(".ogg&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkBaiduMusicMp3()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/baiduNetDiskPlay?path=");
		size_t n2 = tmpUrl.find(".mp3&id=");
		size_t n3 = tmpUrl.find(".mp3&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	//===================
	bool checkWangyiMusicLocal()
	{
		//http://192.168.1.35:61323/mp3?filepath=%2Fstorage%2Femulated%2F0%2FMusic%2FPCM%2F03.A+ROAD+TOO+LONG.wav&decode=false
		//http://10.10.10.127:61323/mp3?filepath=%2Fstorage%2Femulated%2F0%2Fnetease%2Fcloudmusic%2FCache%2FMusic1%2F1349888444-320000-51660615a845b33a3667963fea01b210.mp3.uc%21&decode=true
		size_t n = tmpUrl.find("&decode=false");
		size_t n2 = tmpUrl.find("&decode=true");
		if(n != std::string::npos || n2 != std::string::npos)
		{
			if(tmpUrl.size() == n + 13 || tmpUrl.size() == n2 + 12)
			{
				if(std::string::npos != tmpUrl.find("61323/mp3?filepath=")) return true;
			}
		}
		
		return false;
	}

	bool checkWangyiMusic()
	{
		if(std::string::npos != tmpUrl.find("music.126.net/")) return true;
		return false;
	}

//Dropbox start
	bool checkDropboxMusic()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		if(n != std::string::npos)
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicDff()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".dff&id=");
		size_t n3 = tmpUrl.find(".dff&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicDsf()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".dsf&id=");
		size_t n3 = tmpUrl.find(".dsf&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicAiff()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".aiff&id=");
		size_t n3 = tmpUrl.find(".aiff&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicFlac()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".flac&id=");
		size_t n3 = tmpUrl.find(".flac&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicWav()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".wav&id=");
		size_t n3 = tmpUrl.find(".wav&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicOgg()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".ogg&id=");
		size_t n3 = tmpUrl.find(".ogg&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}

	bool checkDropboxMusicMp3()
	{
		size_t n = tmpUrl.find("http://127.0.0.1:8899/DropboxPlay?");
		size_t n2 = tmpUrl.find(".mp3&id=");
		size_t n3 = tmpUrl.find(".mp3&cover=");
		if(n != std::string::npos && (n2 != std::string::npos || n3 != std::string::npos))
		{
			return true;
		}
		
		return false;
	}
//Dropbox end
	void getUrlSuffix(std::string &tail)
	{
		size_t n = tmpUrl.rfind(".");
		if(n != std::string::npos)
		{
			tail = tmpUrl.substr(n, tmpUrl.size());
		}
	}
	/**
	 * CurlResponseHandler::OnData() shall throw this to pause the
	 * stream.  Call Resume() to resume the transfer.
	 */
	struct Pause {};

	void Resume() noexcept;

	/**
	 * A HTTP request is finished.  Called by #CurlGlobal.
	 */
	void Done(CURLcode result) noexcept;

private:
	/**
	 * Frees the current "libcurl easy" handle, and everything
	 * associated with it.
	 */
	void FreeEasy() noexcept;

	void FinishHeaders();
	void FinishBody();

	size_t DataReceived(const void *ptr, size_t size) noexcept;

	void HeaderFunction(StringView s) noexcept;

	void OnPostponeError() noexcept;

	/** called by curl when new data is available */
	static size_t _HeaderFunction(char *ptr, size_t size, size_t nmemb,
				      void *stream) noexcept;

	/** called by curl when new data is available */
	static size_t WriteFunction(char *ptr, size_t size, size_t nmemb,
				    void *stream) noexcept;
};

#endif
