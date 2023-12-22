#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>

#include "SNSCoordinator.h"

using grpc::Server;
using grpc::ServerBuilder;

using namespace std;

void RunServer(string host, string port, int numClusters) {

	string server_address(host + ":" + port);

	SNSCoordinator service(numClusters);

	//grpc::EnableDefaultHealthCheckService(true);
	//grpc::reflection::InitProtoReflectionServerBuilderPlugin();

	ServerBuilder builder;

	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);

	// Finally assemble the server.
	unique_ptr<Server> server(builder.BuildAndStart());

	log(INFO, "Coordinator listening on " << server_address);

	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
}

int main(int argc, char** argv) {
	
	string host = "localhost";
	string port = "9000";
	int numClusters = 3;

	int opt = 0;
	while ((opt = getopt(argc, argv, "n:h:p:")) != -1) {
		switch(opt) {
			case 'n':
				numClusters = stoi(optarg);
				break;
			case 'h':
				host = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			default:
				cerr << "Invalid Command Line Argument\n";
		}
	}

  	std::string log_file_name = std::string("coordinator-") + port;
  	google::InitGoogleLogging(log_file_name.c_str());
  	log(INFO, "Logging Initialized. Coordinator starting...");

	RunServer(host, port, numClusters);
	return 0;
}
