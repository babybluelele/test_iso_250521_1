#ifndef COMRAAT_H
#define COMRAAT_H

#include <string>

class ConRaat{
	private:
	int sfd;
	std::string cmd;
	std::string path;
	int ufd;
	public:
	ConRaat(int f, std::string cm);
	ConRaat(int f, std::string cm, std::string _path);
	~ConRaat();
	
	int get_fd_raat();
	int send_ctl();
	void set_cmd(std::string &cm);
	void set_cmd(const char *cm);
	std::string& get_status(std::string &str);
	std::string& get_mpd_status(std::string &str);
	std::string& get_mpd_first(std::string &str);
	std::string& get_mads_status(std::string &rstr);
};

#endif