#include <thread>

#include "client.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using grpc::StatusCode;

// Server Communication
using SNS::SNSStatus;
using SNS::Message;
using SNS::Request;
using SNS::Reply;
using SNS::ListReply;

// Coordinator Communication
using SNS::ServerInfo;
using SNS::ClientRequest;

using namespace SNS;
using namespace std;

// ---- CLIENT STATIC UTILITY FUNCTIONS ----

string OUTPUT_SEP = "\n---\n";

// change all characters in str to upper case
void toUpperCase(string& str)
{
	locale loc;
	for (string::size_type i = 0; i < str.size(); i++) {
		str[i] = toupper(str[i], loc);
	}
}

// Prints the given list of strings separated by the specified separator
// Does not print separator after the last token, and prints a newline
void printWithSeparator(const vector<string> &tokens, string sep)
{
	int numTokens = (int)tokens.size();
	if(numTokens > 0) {
		for(int i = 0; i < (int)tokens.size() - 1; i++) {
			cout << tokens[i] << sep;
		}
		cout << tokens[tokens.size() - 1];
	}
	cout << "\n";
}

// Return a pointer to a timestamp containing the current time
Timestamp* Client::getCurrentTimestamp() {
	google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
	timestamp->set_seconds(time(NULL));
	timestamp->set_nanos(0);
	return timestamp;
}

// Returns a message instance constructed with the
// provided username, msg, and current time
Message Client::MakeMessage(const string& username, const string& msg) {
	Message m;
	m.set_username(username);
	m.set_msg(msg);
	m.set_allocated_timestamp(getCurrentTimestamp());
	return m;
}

// Timestamp the given request with the current time
void Client::timestampRequest(Request *request) {
	request->set_allocated_timestamp(getCurrentTimestamp());
}

// ---- CLIENT ----

// Run the client bash for processing commands
void Client::run() 
{
	int ret = connect();
	if (ret < 0) {
		cout << "Connection failed: " << ret << endl;
		exit(1);
	}

	displayTitle();
	while(true) {
		string cmd = getCommand();
		IReply reply = processCommand(cmd);
		displayCommandReply(cmd, reply);
		if (reply.grpc_status.ok() && cmd == "TIMELINE") {
			processTimeline();
		}
	}

}

// ---- COMMAND PROCESSING ----

// Display title
void Client::displayTitle() const
{
	string title = 
	"\n========= TINY SNS CLIENT =========\n"
	" Command Lists and Format:\n"
	" FOLLOW <username>\n"
	" UNFOLLOW <username>\n"
	" LIST\n"
	" TIMELINE\n"
	"=====================================\n";

	cout << title;
}

// Get input and perform input validation
string Client::getCommand() const
{
	string input;
	while (true) {
		cout << "Cmd> ";
		getline(cin, input);
		size_t index = input.find_first_of(" ");
		if (index != string::npos) {
			string cmd = input.substr(0, index);
			toUpperCase(cmd);
			if(input.length() == index+1){
				cout << "Invalid Input -- No Arguments Given\n";
				continue;
			}
			string argument = input.substr(index+1, (input.length()-index));
			input = cmd + " " + argument;
		} else {
			toUpperCase(input);
			if (input != "LIST" && input != "TIMELINE") {
				cout << "Invalid Command\n";
				continue;
			}
		}
		break;
	}
	return input;
}

// Send commands to server for processing
IReply Client::processCommand(string& input)
{
	IReply ire;

	// Assuming that string input will always be a valid command
	size_t loc = input.find(" ");
	string cmd = input.substr(0, loc);
	string arg;

	// npos essentially represents "end of the string"
	if(loc != input.npos) {
		arg = input.substr(loc + 1);
	}

	// You can't use switch statements in C++ with strings
	if(cmd == "FOLLOW") {
		ire = Follow(arg);
	} else if (cmd == "UNFOLLOW") {
		ire = UnFollow(arg);
	} else if (cmd == "LIST") {
		ire = List();
	} else if (cmd == "TIMELINE") {
		ire = IReply();
	} else {
		cout << "Invalid command\n";
	}

	return ire;
}

// Display command reply
// For list reply, prints out each list obtained from server
// For others, simply prints out the message provided by the server
void Client::displayCommandReply(const string& comm, const IReply& reply) const
{
	if (reply.grpc_status.ok()) {
		switch (reply.comm_status) {
			case SUCCESS:
				// cout << "Command completed successfully\n";
				if (comm == "LIST") {
					cout << "Me: " << getMyUsername() + "\n";
					
					cout << "All users: ";
					printWithSeparator(reply.all_users);
					
					cout << "Followers: ";
					printWithSeparator(reply.followers);
					
					cout << "Following: ";
					printWithSeparator(reply.following);
				}
				cout << reply.comm_msg << "\n";
				break;
			default:
				cout << reply.comm_msg << "\n";
				break;
		}
	} else {
		cout << "grpc failed: " << reply.grpc_status.error_message() << endl;
	}
}

void Client::displayReConnectionMessage(const string& host, const string &port)
{
	cout << "Connecting to " << host << ":" << port << "..." << endl;
}

// ---- SERVER/COORDINATOR COMMUNICATION ----

int Client::connect()
{

   // Connect to coordinator to obtain a unique ID
   // and start a separate thread for obtaining and refreshing
   // host and port info for server to communicate with

	shared_ptr<Channel> coordChannel = grpc::CreateChannel(hostname + ":" + port, grpc::InsecureChannelCredentials());
	coordStub_ = make_unique<CoordService::Stub>(coordChannel);

	ClientContext context;
	ClientRequest request;

	Status status = coordStub_->GetUniqueClientID(&context, request, &id);
	if(!status.ok()) {
		return -1;
	}

	getServerInfo();
	thread(&Client::refreshServer, this).detach();
	
	IReply reply = Login();
	if (!reply.grpc_status.ok() || reply.comm_status != SUCCESS) {
		return -1;
	}
	displayCommandReply("login", reply);

	return 1;
}

void Client::getServerInfo()
{
	ClientContext context;
	Status status = coordStub_->GetServer(&context, id, &server);

	if(!status.ok()) {
		if(status.error_code() == StatusCode::UNAVAILABLE) {
			cout << "Server unavailable. Retrying...\n";
		} else {
			cout << "Unknown error. Exiting." << "\n";
			exit(1);
		}
	}

	string serverHostname = server.hostname();
	string serverPort = server.port();

	if(server.changed()) {
		displayReConnectionMessage(serverHostname, serverPort);
	}

	shared_ptr<Channel> channel = grpc::CreateChannel(serverHostname + ":" + serverPort, grpc::InsecureChannelCredentials());
	stub_ = make_unique<SNSService::Stub>(channel);
}

void Client::refreshServer()
{
	while(true) {
		sleep(refreshServerDelay);
		getServerInfo();
	}
}

IReply Client::Login()
{
	IReply ire;
	
	ClientContext context;
	Request request;
	Reply reply;
	request.set_username(username);

	timestampRequest(&request);
	Status status = stub_->Login(&context, request, &reply);

	ire.grpc_status = status;
	ire.comm_status = FAILURE_UNKNOWN;

	if(status.ok()) {
		ire.comm_status = reply.status();
		ire.comm_msg = reply.msg();
	} 

	return ire;
}

IReply Client::List()
{
	IReply ire;

	ClientContext context;
	Request request;
	ListReply listReply;
	request.set_username(username);

	timestampRequest(&request);
	Status status = stub_->List(&context, request, &listReply);

	ire.grpc_status = status;
	ire.comm_status = FAILURE_UNKNOWN;
	
	if(status.ok()) {
		
		for(auto user : listReply.all_users()) {
			ire.all_users.push_back(user);
		}

		for(auto follower : listReply.followers()) {
			ire.followers.push_back(follower);
		}

		for(auto followingClient : listReply.following()) {
			ire.following.push_back(followingClient);
		}

		ire.comm_status = SUCCESS;

	}

	return ire;
}

IReply Client::Follow(const string &username2)
{
	IReply ire; 

	ClientContext context;
	Request request;
	Reply reply;
	request.set_username(username);
	request.add_arguments(username2);

	timestampRequest(&request);
	Status status = stub_->Follow(&context, request, &reply);

	ire.grpc_status = status;
	ire.comm_status = FAILURE_UNKNOWN;

	if(status.ok()) {
		ire.comm_status = reply.status();
		ire.comm_msg = reply.msg();
	} 

	return ire;
}

IReply Client::UnFollow(const string &username2)
{
	IReply ire;
	string failure_msg = "You do not follow this user...";

	ClientContext context;
	Request request;
	Reply reply;
	request.set_username(username);
	request.add_arguments(username2);

	timestampRequest(&request);
	Status status = stub_->UnFollow(&context, request, &reply);

	ire.grpc_status = status;
	ire.comm_status = FAILURE_UNKNOWN;

	if(status.ok()) {
		ire.comm_status = reply.status();
		ire.comm_msg = reply.msg();
	} 

	return ire;
}

// ---- TIMELINE ----

void Client::processTimeline()
{
	cout << "Now you are in the timeline" << endl;
	Timeline(username);
}

string Client::getPostMessage()
{
	char buf[MAX_DATA];
	while (1) {
		fgets(buf, MAX_DATA, stdin);
		if (buf[0] != '\n')  break;
	}

	string message(buf);
	return message;
}

void Client::displayPostMessage(const string& sender, const string& message, time_t& time)
{
	string t_str(ctime(&time));
	t_str[t_str.size()-1] = '\0';
	cout << sender << " (" << t_str << ") >> " << message << endl;
}

void Client::Timeline(const string &username)
{

	// Note:
	//
	// Once a user enter to timeline mode , there is no way
	// to command mode. You can terminate the client program by pressing
	// CTRL-C and re-connect

	ClientContext context;
	unique_ptr<ClientReaderWriter<Message, Message>> stream(stub_->Timeline(&context));

	thread writer([&stream, username, this]() {
		
		// Ensure that the client's stream field is set
		stream->Write(MakeMessage(username, ""));
		while(true) {
			string msgToSend = this->getPostMessage();
			Message m = MakeMessage(username, msgToSend);
			stream->Write(m);
		}

	});

	Message message;
	while(stream->Read(&message)) {
		string u = message.username();
		string m = message.msg();
		time_t t = message.timestamp().seconds();
		displayPostMessage(u, m, t);
	}

	writer.join();
	Status status = stream->Finish();

	if(!status.ok()) {
		cout << "Timeline gRPC failed..." << endl;
	}
}