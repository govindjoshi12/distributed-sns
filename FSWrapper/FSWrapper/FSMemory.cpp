#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

#include "filesystem_utils.h"
#include "FSMemory.h"

using namespace std;

// ---- Tree Node Operations ----

// Overload method which takes string tokens
shared_ptr<FSTreeNode> FSMemory::getTreeNode(shared_ptr<FSTreeNode> node, string file, bool getParent, bool create) {
	vector<string> tokens = splitFilepath(file);
	if(getParent) {
		tokens.pop_back();
	}

	return getTreeNode(node, tokens, create);
}

shared_ptr<FSTreeNode> FSMemory::getTreeNode(shared_ptr<FSTreeNode> node, vector<string> tokens, bool create) {

	for(string token : tokens) {
		shared_ptr<FSTreeNode> next = node->getChild(token);
		if(!next) {

			if(!create) {
				return NULL;
			}

			// Create intermediate folders
			next = node->createChild(token, true, "");
		}
		node = next;
	}

	return node;
}

// ---- API Operations ----

// Create file
int FSMemory::create(string file, bool folder, bool createDirectories) {

	if(coarseMutex) {
		fsMutex.lock();
	}

	int success = -1;
	if(file == "") {
		return success;
	}

	vector<string> tokens = splitFilepath(file);
	string actualFile = tokens.back();
	tokens.pop_back();

	shared_ptr<FSTreeNode> node = getTreeNode(filetree, tokens, createDirectories);
	if(node) {

		// Fails if there is already a file with the given name 
		if(node->getChild(actualFile) == NULL) {
			node = node->createChild(actualFile, folder, "");
		
			if(node) {
				success = 0;
			}
		}
	}

	if(coarseMutex) {
		fsMutex.unlock();
	}

	return success;
}

bool FSMemory::exists(string file) {
	shared_ptr<FSTreeNode> node = getTreeNode(filetree, file, false, false);
	return node != NULL;
}

// Reads file from given offset
// if file not found or offset >= file len, returns empty str
int FSMemory::read(string file, string &data, int offset) {
	
	int success = -1;

	shared_ptr<FSTreeNode> node = getTreeNode(filetree, file, false, false);
	
	if(node && !node->folder) {
		string &fileData = node->data;
		if(fileData.length() >= offset) {
			data = fileData.substr(offset);
			success = 0;
		}
	}

	return success;
}

// append to file or create if it doesn't exist
// if overwrite, replaces the current data
// if createDirectories, create intermediate directories.
// If not createDirs, then if the file path doesn't exist, fails.
int FSMemory::write(string file, string data, bool createDirectories, bool overwrite) {
   
	if(coarseMutex) {
		fsMutex.lock();
	}

	int success = -1;
	if(file == "") {
		return success;
	}

	vector<string> tokens = splitFilepath(file);
	string actualFile = tokens.back();
	tokens.pop_back();

	shared_ptr<FSTreeNode> node = getTreeNode(filetree, tokens, createDirectories);
	if(node) {

		shared_ptr<FSTreeNode> next = node->getChild(actualFile);
		if(next) {
			// Cannot write data to a folder
			if(!next->folder) {
				next->data = overwrite ? data : next->data + data; 
				success = 0;
			}

		} else {
			next = node->createChild(actualFile, false, data);
			if(next) {
				success = 0;
			}
		}
	}

	if(coarseMutex) {
		fsMutex.unlock();
	}

	return success;
}

// originalPath: full path to file
// newName: new name (relative)
int FSMemory::rename(string originalPath, string newName) {

	if(coarseMutex) {
		fsMutex.lock();
	}

	int success = -1;
	if(originalPath == "" && !isNameValid(newName)) {
		return success;
	}

	// get parent node of actual node
	vector<string> tokens = splitFilepath(originalPath);
	string actualFile = tokens.back();
	tokens.pop_back();

	shared_ptr<FSTreeNode> node = getTreeNode(filetree, tokens, false);

	if(node) {
		success = node->renameChild(actualFile, newName);
	}

	if(coarseMutex) {
		fsMutex.unlock();
	}

	return success;
}

int FSMemory::copy(string src, string dest) {
	return -1;
}

int FSMemory::remove(string file) {
	
	if(coarseMutex) {
		fsMutex.lock();
	}

	int success = -1;
	if(file == "") {
		return success;
	}

	// get parent node of actual node
	vector<string> tokens = splitFilepath(file);
	string actualFile = tokens.back();
	tokens.pop_back();

	shared_ptr<FSTreeNode> node = getTreeNode(filetree, tokens, false);

	if(node) {
		success = node->removeChild(actualFile);
	}

	if(coarseMutex) {
		fsMutex.unlock();
	}

	return success;
}

// ---- FILE TREE TO STRING ----
string FSMemory::TREE_PREFIX = "│   ";
string FSMemory::TREE_BLANK_PREFIX = "    ";
string FSMemory::LOCAL_LAST_PREFIX = "└── ";
string FSMemory::LOCAL_PREFIX = "├── ";
string FSMemory::FOLDER = ">";
string FSMemory::FILE = "#";

string FSMemory::toStringHelper(shared_ptr<FSTreeNode> node, string prefix) {

	string filetype = node->folder ? FOLDER : FILE;
	string res = filetype + " " + node->name + "\n";

	int i = 0;
	int numChildren = node->children.size();
	for(const pair<string, shared_ptr<FSTreeNode>> &childPair : node->children) {
		shared_ptr<FSTreeNode> childNode = childPair.second;

		if(i < numChildren - 1) {
			// Not last child
			res += prefix + LOCAL_PREFIX + toStringHelper(childNode, prefix + TREE_PREFIX);
		} else {
			// last child
			res += prefix + LOCAL_LAST_PREFIX + toStringHelper(childNode, prefix + TREE_BLANK_PREFIX);
		}

		i++;
	}
	return res;
}

string FSMemory::toString() {
	return FSMemory::toStringHelper(filetree, "");
}

// ---- SAVE TO DISK ----

void FSMemory::saveHelper(shared_ptr<FSTreeNode> node, const string &dir) {
	if(!node->folder) {
		
		ofstream file(dir + "/" + node->name);
		file << node->data;
		file.close();

		return;
	}

	string nextDir = dir + "/" + node->name;
	int status = mkdir(nextDir.c_str(), 0777);

	if(status < 0) {
		throw runtime_error("ERROR WHILE SAVING TO DISK!");
	}

	for(const pair<string, shared_ptr<FSTreeNode>> &childPair : node->children) {
		shared_ptr<FSTreeNode> childNode = childPair.second;

		saveHelper(childNode, nextDir);
	}
}

int FSMemory::saveToDisk(const string &dir) {
	int success = -1;

	struct stat sb;
	if(stat(dir.c_str(), &sb) == 0) {
		// file exists and is a directory
		if(sb.st_mode & S_IFDIR) {

			// TODO: Need to check folder write permissions
			saveHelper(filetree, dir);
			success = 0;
		}
	}

	return success;
}