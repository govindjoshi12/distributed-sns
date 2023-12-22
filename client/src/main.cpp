#include <iostream>
#include <string>

#include "client.h"

using namespace std;

int main(int argc, char** argv) {

	string hostname = "localhost";
	string username = "default";
	string port = "9000";
		
	int opt = 0;
	while ((opt = getopt(argc, argv, "h:k:u:")) != -1){
		switch(opt) {
		case 'h':
			hostname = optarg;break;
		case 'u':
			username = optarg;break;
		case 'k':
			port = optarg;break;
		default:
			cout << "Invalid Command Line Argument\n";
		}
	}
			
	cout << "Logging Initialized. Client starting...\n";
	
	Client myc(hostname, port, username);
	myc.run();
	
	return 0;
}