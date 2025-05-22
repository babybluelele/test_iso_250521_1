#include <sys/stat.h>
#include<sys/socket.h>
#include<sys/un.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <string>

#include "ConRaat.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
static constexpr Domain con_domain("ConRaat");


ConRaat::ConRaat(int f, std::string cm):
	sfd(f), cmd(cm)
{
	ufd = -1;
	path = "/opt/raat-sock";
	get_fd_raat();
}

ConRaat::ConRaat(int f, std::string cm, std::string _path):
	sfd(f), cmd(cm), path(_path)
{
	ufd = -1;
	get_fd_raat();
}

ConRaat::~ConRaat()
{
	if(ufd > 0)
	{
		close(ufd);
	}
}
void ConRaat::set_cmd(std::string &cm)
{
	cmd = cm;
}

void ConRaat::set_cmd(const char *cm)
{
	cmd = cm;
}

int ConRaat::get_fd_raat()
{
	struct sockaddr_un un;
	//int sock_fd = -1;
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, path.c_str());
	ufd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(ufd < 0){
		FormatNotice(con_domain, "socket error:%s", cmd.c_str());
		return -1;
	}
	
	if(connect(ufd, (struct sockaddr *)&un, sizeof(un)) <  0){
		FormatNotice(con_domain, "connect error:%s,%s", cmd.c_str(), path.c_str());
		return -1;
	}

	return ufd;
}

int ConRaat::send_ctl()
{
	int n = -1;
	if(ufd > 0)
		n  = send(ufd, cmd.c_str(), cmd.size(), 0);
	//cout<<cmd<<endl;
	return  n;
}

std::string & ConRaat::get_status(std::string &rstr)
{
	if(ufd <= 0) return rstr;
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(ufd, &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = 200000;

	char buf[4096];
	while(1)
	{
		int ret = select(ufd + 1, &rfds, NULL, NULL, &tv);
		if(ret > 0)
		{
			int n = recv(ufd, buf, 4096 - 1, 0);
			if(n > 0 && n <= 4096 - 1)
			{
				buf[n] = '\0';
				rstr += buf;
				size_t len = rstr.size();
				std::string tail2 = rstr.substr(len - 2, 2);
				if(tail2.compare("\r\n") == 0)
					break;
			}
			else
			{
				rstr.clear();
				FormatNotice(con_domain, "get_status recv error");
				break;
			}
		}
		else
		{
			rstr.clear();
			FormatNotice(con_domain, "get_status recv timeout");
			break;
		}
	}

	return rstr;
}

std::string & ConRaat::get_mpd_status(std::string &rstr)
{
	if(ufd <= 0) return rstr;
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(ufd, &rfds);

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	char buf[4096];
	while(1)
	{
		int ret = select(ufd + 1, &rfds, NULL, NULL, &tv);
		if(ret > 0)
		{
			int n = recv(ufd, buf, 4096 - 1, 0);
			if(n > 0 && n <= 4096 - 1)
			{
				buf[n] = '\0';
				rstr += buf;
				size_t len = rstr.size();
				std::string tail2 = rstr.substr(len - 3, 3);
				if(rstr.compare(0,3, "ACK", 0, 3) == 0) break;
				if(tail2.compare("OK\n") == 0)
					break;
			}
			else
			{
				rstr.clear();
				FormatNotice(con_domain, "get_mpd_status recv error");
				break;
			}
		}
		else
		{
			rstr.clear();
			FormatNotice(con_domain, "get_mpd_status recv timeout");
			break;
		}
	}

	return rstr;
}

std::string & ConRaat::get_mads_status(std::string &rstr)
{
	if(ufd <= 0) return rstr;
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(ufd, &rfds);

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	char buf[4096];
	while(1)
	{
		int ret = select(ufd + 1, &rfds, NULL, NULL, &tv);
		if(ret > 0)
		{
			int n = recv(ufd, buf, 4096 - 1, 0);
			if(n > 0 && n <= 4096 - 1){
				buf[n] = '\0';
				rstr += buf;
				size_t len = rstr.size();
				if(len >= 6)
				{
					std::string tail2 = rstr.substr(len - 6, 6);
					if(tail2.compare("\r\nOK\r\n") == 0)
					{
						rstr = rstr.substr(0, len - 6);
						break;
					}
				}
			}else{
				rstr.clear();
				FormatNotice(con_domain, "get_mads_status recv errort");
				break;
			}
		}
		else
		{
			rstr.clear();
			FormatNotice(con_domain, "get_mads_status recv timeout");
			break;
		}
	}
	
	return rstr;
}

std::string & ConRaat::get_mpd_first(std::string &rstr)
{
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(ufd, &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = 200000;

	int ret = select(ufd + 1, &rfds, NULL, NULL, &tv);

	if (ret)
	{
		char buf[4096];
		int n = recv(ufd, buf, 4096 - 1, 0);
		if(n > 0 && n <= 4096 - 1)
		{
			buf[n] = '\0';
			rstr += buf;
		}
	}
	else
	{
			FormatNotice(con_domain, "get_mpd_status recv timeout");
	}

	return rstr;
}