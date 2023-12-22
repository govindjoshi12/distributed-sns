#include "FSTreeNode.h"

using namespace std;

shared_ptr<FSTreeNode> FSTreeNode::getChild(string childName) {
	shared_ptr<FSTreeNode> node = NULL;
	map<string, shared_ptr<FSTreeNode>>::iterator it = children.find(childName);

	if(it != children.end()) {
		node = it->second;
	}

	return node;
}

// overwrites previous FSTreeNode if it exists
shared_ptr<FSTreeNode> FSTreeNode::createChild(string childName, bool childFolder, string childData) {
	
	// Invalid to create a child of a file
	// and to create a folder with data
	if(!folder || (childFolder && childData.length() > 0)) {
		return NULL;
	}

	shared_ptr<FSTreeNode> node = make_shared<FSTreeNode>();
	node->name = childName;
	node->folder = childFolder;
	node->data = childData;

	children[childName] = node;
	return node;
}

int FSTreeNode::renameChild(string childName, string newName) {
	auto nodeHandler = children.extract(childName);
	int success = -1;

	// no child with specified name
	if(!nodeHandler.empty()) {
		nodeHandler.key() = newName;
		success = 0;
	}

	// need to insert nodehandler back into map
	children.insert(std::move(nodeHandler));

	if(success == 0) {
		children[newName]->name = newName;
	}

	return success;
}

// Following file method return val convention,
// Returns 0 for success, -1 for failure
int FSTreeNode::removeChild(string childName) {

	int success = children.erase(childName);
	return success > 0 ? 0 : -1;

}