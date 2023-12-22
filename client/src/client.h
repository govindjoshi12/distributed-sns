#include <iostream>
#include <string>
#include <ctime>
#include <vector>

#include <grpc++/grpc++.h>
#include <google/protobuf/timestamp.pb.h>

#include <snsproto/sns.grpc.pb.h>
#include <snsproto/coordinator.grpc.pb.h>

#define MAX_DATA 256

using google::protobuf::Timestamp;

using SNS::SNSService;
using SNS::CoordService;
using SNS::SNSStatus;
using SNS::ServerInfo;
using SNS::Message;
using SNS::Request;
using SNS::ID;

using namespace SNS;

// Utilities
void toUpperCase(std::string &str);
void printWithSeparator(const std::vector<std::string> &tokens, std::string sep = ", ");

struct IReply
{
    grpc::Status grpc_status;
    enum SNSStatus comm_status;
	std::string comm_msg;
	
	// For list reply
    std::vector<std::string> all_users;
    std::vector<std::string> followers;
    std::vector<std::string> following;
};

class Client
{

public:
    Client(std::string host, std::string portNumber, std::string user) 
        : hostname(host), port(portNumber), username(user) {};
    virtual ~Client() {};
	void run();
	
private:
    
    std::string hostname;
	std::string username;
	std::string port;
	ID id;
	ServerInfo server;
	bool inChat = false;

	std::unique_ptr<SNSService::Stub> stub_;
	std::unique_ptr<CoordService::Stub> coordStub_;
	
	// Longer delay to allow leader elections to happen after dead master
	int refreshServerDelay = 5;
	int retries = 0;

    // ---- UI METHODS ----
	void displayTitle() const;
	std::string getMyUsername() const { return username; }
	void displayCommandReply(const std::string& comm, const IReply& reply) const;
    std::string getPostMessage();
    void displayPostMessage(const std::string& sender, const std::string& message, std::time_t& time);
    void displayReConnectionMessage(const std::string& host, const std::string & port);
	
    // ---- COMMAND PROCESSING ----
	void runClientBash();
	std::string getCommand() const;
    IReply processCommand(std::string& cmd);
	void processTimeline();

    // ---- CLIENT API METHODS ----
	int connect();
	void getServerInfo();
	void refreshServer();
	IReply Login();
	IReply List();
	IReply Follow(const std::string &username);
	IReply UnFollow(const std::string &username);
	void Timeline(const std::string &username);

	// STATIC UTILITY FUNCTIONS
	static Timestamp* getCurrentTimestamp();
	static Message MakeMessage(const std::string &username, const std::string &msg);
	static void timestampRequest(Request *request);
	static const int MAX_RETRIES = 3;
};