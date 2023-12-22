#pragma once

#include <string>

class FSWrapper {

// TODO: change string args to references

public:
	FSWrapper() {};
	virtual ~FSWrapper() {};

	virtual int create(std::string file, bool folder, bool createDirectories) = 0;
	virtual bool exists(std::string file) = 0;
	virtual int read(std::string file, std::string &data, int offset = 0) = 0;
	virtual int write(std::string file, std::string data, bool createDirectories, bool overwrite) = 0;
	virtual int move(std::string file, std::string dest) = 0;
	virtual int copy(std::string src, std::string dest) = 0;
	virtual int remove(std::string file) = 0;
	virtual std::string toString() = 0;
};

// TODO: negative return values that indicate certain errors