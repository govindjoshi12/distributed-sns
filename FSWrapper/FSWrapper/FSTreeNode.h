#include <map>
#include <string>
#include <memory>
#include "filesystem_utils.h"

class FSTreeNode  {

public:
    FSTreeNode() {};
    virtual ~FSTreeNode() {};

    bool folder;
    std::string name;
    std::string data;
	std::map<std::string, std::shared_ptr<FSTreeNode>> children;

    std::shared_ptr<FSTreeNode> getChild(std::string childName);
    std::shared_ptr<FSTreeNode> createChild(std::string childName, bool childFolder, std::string childData);
    int renameChild(std::string childName, std::string newName);
    int removeChild(std::string childName);
};
