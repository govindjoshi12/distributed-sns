#include <iostream>
#include <vector>
#include <sstream>
#include <limits.h>

#include "FSWrapper/FSWrapper.h"
#include "FSWrapper/FSMemory.h"

using namespace std;

FSWrapper* filesys;
FSMemory fsm;

bool createDirs = false;
streampos pos1 = 1;
string help = 
	"ls: print filetree. > <folder>, # <file>\n"
	"mkdir <folder>: create folder\n"
	"touch <file>: create file\n"
	"test <path>: check if file exists\n"
	"cat <path>: print file contents\n"
	"rm <path>: delete file\n"
	"write <path> <data> (data in double quotes): appends data to file (creates if doesnt exist)\n"
	"createdirs <true or false>: create dirs if they dont exist (default false)\n"
	"save <folder path>: saves the filesystem to the specified folder\n";

bool getLineQuotes(stringstream &ss, string &s, char delim = ' ') {
	
	if(!ss) {
		return false;
	}
	
	stringstream output;

	char p = ss.peek();
	while(p != EOF && p != delim) {
	    if(p == '"') {
	        ss.seekg(ss.tellg() + pos1);
	        string temp;
			getline(ss, temp, '"');
			output << temp;
	    } else {
			output.put(ss.get());
		}

		if(!ss) {
	        break;
	    }

		p = ss.peek();
	}

	ss.seekg(ss.tellg() + pos1);
	s = output.str();
    
	return true;
}

vector<string> split(string input) {
    
    stringstream ss(input);
    string s;
    vector<string> tokens;
    
    while(getLineQuotes(ss, s, ' ')) {
        tokens.push_back(s);
    }
    
    return tokens;
}

void processCommand() {

	string input;
	string failed = "Command failed\n";
	string invalid = "Invalid Command\n";
	
	cout << "$ ";
	getline(cin, input);
	vector<string> tokens = split(input);

	bool valid = true;
	int success = 0;
	if(tokens.size() == 1) {

		string command = tokens[0];
		if(command == "ls") {
			cout << filesys->toString();
		} else if(command == "help") {
			cout << help;
		} else {
			valid = false;
		}

	} else if(tokens.size() == 2) {

		string command = tokens[0];
		string arg1 = tokens[1];

		if(command == "mkdir") {

			success = filesys->create(arg1, true, createDirs);

		} else if(command == "touch") {

			success = filesys->create(arg1, false, createDirs);

		} else if(command == "test") {

			bool exists = filesys->exists(arg1);
			if(exists) {
				cout << "File exists\n";
			} else {
				cout << "File does not exist\n";
			}

		} else if(command == "cat") {

			string output; 
			success = filesys->read(arg1, output);

			if(success == 0) {
				cout << output << "\n";
			} else {
				cout << "Reading failed\n";
				
				// temp sol to prevent printing invalid string later
				success = true;
			}

		} else if(command == "rm") {
			
			success = filesys->remove(arg1);

		} else if(command == "createdirs") {

			if(arg1 == "true") {

				createDirs = true;

			} else if(arg1 == "false") {

				createDirs = false;

			} else {
				valid = false;
			}

		} else if(command == "save") {

			FSMemory* temp = &fsm;
			success = temp->saveToDisk(arg1);

		} else {
			valid = false;
		}

	} else if(tokens.size() == 3) {

		string command = tokens[0];
		string arg1 = tokens[1];
		string arg2 = tokens[2];

		if(command == "write") {
			success = filesys->write(arg1, arg2, createDirs, false);
		} else if(command == "cp") {

		} else if(command == "rename") {

		} else {
			valid = false;
		}

	} else {
		valid = false;
	}

	// invalid command
	if(!valid) {
		cout << invalid;
	} else if(success < 0) {
		cout << failed;
	}
}

int main() {

	filesys = &fsm;

	while(true) {
		processCommand();
	}

	return 0;
}