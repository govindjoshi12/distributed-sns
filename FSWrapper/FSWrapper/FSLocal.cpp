#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>

#include "filesystem_utils.h"
#include "FSLocal.h"

namespace fs = std::filesystem;
using namespace std;

/*
	Initializes FSLocal instance. If baseDir string is
	not empty, create base dir folder in current working directory.
	FSLocal will prefix each file with baseDir.
*/
FSLocal::FSLocal(const string &baseDir) {

	// Current working directory is executable location

	this->baseDir = baseDir;
	if(baseDir != "") {
		// Create folder base dir in working directory if doesn't exist
		mkdir(baseDir.c_str(), 0777);
		this->baseDir += "/";
	}
}

// Creates file
int FSLocal::create(string file, bool folder, bool createDirectories) {

	if(file == "") {
		return -1;
	}

	file = getFullPath(file);

	bool success;
	if(folder && createDirectories) {
		success = fs::create_directories(file);
		return boolToInt(success);
	} 
	
	if(createDirectories) {
		string parentFolderPath = getParentFolderPath(file);
		fs::create_directories(parentFolderPath);
	}

	if(folder) {
		success = fs::create_directory(file);
	} else {
		ofstream outfile;
		outfile.open(file);
		// outfile closes on destruction

		success = !outfile.fail();
	}

	return boolToInt(success);
}

bool FSLocal::exists(string file) {
	return fs::exists(getFullPath(file));
}

int FSLocal::read(string file, string &data, int offset) {
	file = getFullPath(file);

	ifstream infile(file);
	if(infile.fail()) {
		return -1;
	}

	string line;
	data = "";
	while (getline(infile, line)) {
		data += line + "\n";
	}

	// Return if there was an error while reading file
	// Dont check fail as getline sets fail bit when eof is reached
	int success = infile.bad() ? -1 : 0;
	return success;
}

int FSLocal::write(std::string file, std::string data, bool createDirectories, bool overwrite) {
	
	file = getFullPath(file);
	if(createDirectories) {
		string parentFolderPath = getParentFolderPath(file);
		fs::create_directories(parentFolderPath);
	}

	ofstream outfile;
	ios_base::openmode mode = overwrite ? ios_base::trunc : ios_base::app;
	outfile.open(file, mode);

	if(outfile.fail()) {
		return -1;
	}

	outfile << data;

	return 1;

}

// UNTESTED
int FSLocal::remove(string file) {

	file = getFullPath(file);
	int numDeleted = fs::remove_all(file);

	// Returns true if at least one item was deleted 
	return (numDeleted > 0) ? 0 : -1;
}

/* TODO: implement */
int FSLocal::move(string file, string dest) {
	return -1;
}

/* TODO: implement */
int FSLocal::copy(string src, string dest) {
	return -1;
}

/* TODO: implement */
string FSLocal::toString() {
	return "";
}