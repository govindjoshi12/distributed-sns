#include <string>
#include <vector>

using namespace std;

vector<string> splitString(const string &str, string delim) {
	
	vector<string> tokens;

	size_t startIdx = 0;
	size_t endIdx = str.find(delim);
	 while(endIdx != str.npos) {
		string token = str.substr(startIdx, endIdx - startIdx);
		if(token != "") {
			tokens.push_back(token);
		}
		startIdx = endIdx + 1;
		endIdx = str.find(delim, startIdx);
	}
	 
	// Portion of string after last occurence of delimiter
	string token = str.substr(startIdx);
	if(token != "") {
		tokens.push_back(token);
	}

	return tokens;
}

vector<string> splitFilepath(const string &filepath) {
	return splitString(filepath, "/");
}

// Returns parent folder path. Includes forward-slash at end
string getParentFolderPath(const string &filepath) {
	
	// Cannot rely on finding last slash in string
	// Need to split then recombine for robustness

	vector<string> tokens = splitFilepath(filepath);
	string parentPath = "";
	
	if(tokens.size() > 1) {
		for(int i = 0; i < (int)(tokens.size()) - 1; i++) {
			parentPath += tokens[i] + "/";
		}
	}

	return parentPath;
}

// Returns relative filename
string getRelativeFilename(const string &filepath) {
	int idx = filepath.find_last_of("/");
	if(idx == filepath.npos) {
		idx = 0;
	} else {
		idx += 1;
	}

	return filepath.substr(idx);
}

// validate filename (not path, just name)
bool isNameValid(const string &filename) {
	return (filename.length() > 0) && (filename.find("/") == filename.npos);
}