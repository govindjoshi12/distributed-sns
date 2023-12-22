#include "FSWrapper.h"

class FSLocal : public FSWrapper {

public:
    FSLocal(const std::string &baseDirName = "root");
    virtual ~FSLocal() {};

    virtual int create(std::string file, bool folder, bool createDirectories);
	virtual bool exists(std::string file);
	virtual int read(std::string file, std::string &data, int offset = 0);
	virtual int write(std::string file, std::string data, bool createDirectories, bool overwrite);
	virtual int move(std::string file, std::string dest);
	virtual int copy(std::string src, std::string dest);
	virtual int remove(std::string file);
	virtual std::string toString();

private:
	std::string baseDir;

	int boolToInt(bool success) { return success ? 0 : -1; }
	std::string getFullPath(std::string &file) { return baseDir + file; }
};