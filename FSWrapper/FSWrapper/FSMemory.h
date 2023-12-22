#include <map>
#include <mutex>

#include "FSWrapper.h"
#include "FSTreeNode.h"

class FSMemory : public FSWrapper {

public:

	// Creates a new tree-based in-memory filesystem.
	// If coarseMutex is true, any writing/destructive operations will be locked.
    FSMemory(bool coarseMutex = false) {
		this->coarseMutex = coarseMutex;
		filetree = std::make_shared<FSTreeNode>();
		filetree->name = "root";
		filetree->folder = true;
	};
    virtual ~FSMemory() {};

    virtual int create(std::string file, bool folder, bool createDirectories);
	virtual bool exists(std::string file);
	virtual int read(std::string file, std::string &data, int offset = 0);
	virtual int write(std::string file, std::string data, bool createDirectories, bool overwrite);
	virtual int move(std::string file, std::string dest);
	virtual int copy(std::string src, std::string dest);
	virtual int remove(std::string file);
	std::string toString();

	int saveToDisk(const std::string &dir);

private:
	bool coarseMutex;
	std::mutex fsMutex;
	std::shared_ptr<FSTreeNode> filetree;

    std::string toStringHelper(std::shared_ptr<FSTreeNode> node, std::string prefix);
	void saveHelper(std::shared_ptr<FSTreeNode> node, const std::string &dir);

    static std::shared_ptr<FSTreeNode> getTreeNode(std::shared_ptr<FSTreeNode> node, std::string file, bool getParent, bool create);
    static std::shared_ptr<FSTreeNode> getTreeNode(std::shared_ptr<FSTreeNode> node, std::vector<std::string> tokens, bool create);

	static std::string TREE_PREFIX;
    static std::string TREE_BLANK_PREFIX;
    static std::string LOCAL_PREFIX;
    static std::string LOCAL_LAST_PREFIX;
	static std::string FOLDER;
	static std::string FILE;
};

