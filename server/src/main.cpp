#include <iostream>
#include <string>

#include "SNSServer.h"

using grpc::ServerBuilder;

using namespace std;

// ------------------
// --- RUN SERVER ---
// ------------------
void RunServer(string IP, string port, 
				string coordIP, string coordPort,
				int serverId, int clusterId) {

	string server_address = IP + ":" + port;
	SNSServer service;

	// connect to coord
	int ret = service.connectToCoordinator(IP, port, coordIP, coordPort, clusterId, serverId);
	if(ret == -1) {
		log(FATAL, "Failed to connect to coordinator: " + to_string(ret))
		exit(1);
	}

	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);
	unique_ptr<Server> server(builder.BuildAndStart());
  
	log(INFO, "Server listening on " + server_address);

	server->Wait();
}

int main(int argc, char** argv) {

	int clusterId = 1;
	int serverId = 1;
	string coordIP = "localhost";
	string coordPort = "9000";
	string IP = "localhost";
	string port = "10000";

	int opt = 0;
	while ((opt = getopt(argc, argv, "c:s:h:k:i:p:")) != -1) {
		switch(opt) {
			case 'c':
				clusterId = stoi(optarg); break;
			case 's':
				serverId = stoi(optarg); break;
			case 'h':
				coordIP = optarg; break;
			case 'k':
				coordPort = optarg; break;
			case 'i':
				IP = optarg; break;
			case 'p':
				port = optarg; break;
			default:
				cerr << "Invalid Command Line Argument\n"; break;
		}
	}

  	string log_file_name = string("server-") + port;
  	google::InitGoogleLogging(log_file_name.c_str());
  	log(INFO, "Logging Initialized. Server starting...");

  	RunServer(IP, port, coordIP, coordPort, serverId, clusterId);

  	return 0;
}
