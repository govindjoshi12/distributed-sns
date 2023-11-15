#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <csignal>
#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::IStatus;

using namespace csce438;

void sig_ignore(int sig) {
  std::cout << "Signal caught " + sig;
}

Timestamp* getCurrentTimestamp() {
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    return timestamp;
}

Message MakeMessage(const std::string& username, const std::string& msg) {
  Message m;
  m.set_username(username);
  m.set_msg(msg);
  m.set_allocated_timestamp(getCurrentTimestamp());
  return m;
}

void timestampRequest(Request *request) {
  request->set_allocated_timestamp(getCurrentTimestamp());
}

class Client : public IClient
{
public:
  Client(const std::string& hname,
	 const std::string& uname,
	 const std::string& p)
    :hostname(hname), username(uname), port(p) {}

  
protected:
  virtual int connectTo();
  virtual IReply processCommand(std::string& input);
  virtual void processTimeline();
  virtual std::string getMyUsername() const { return username; }

private:
  std::string hostname;
  std::string username;
  std::string port;
  
  // You can have an instance of the client stub
  // as a member variable.
  std::unique_ptr<SNSService::Stub> stub_;
  
  IReply Login();
  IReply List();
  IReply Follow(const std::string &username);
  IReply UnFollow(const std::string &username);
  void   Timeline(const std::string &username);
};


///////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////
int Client::connectTo()
{
  // ------------------------------------------------------------
  // In this function, you are supposed to create a stub so that
  // you call service methods in the processCommand/porcessTimeline
  // functions. That is, the stub should be accessible when you want
  // to call any service methods in those functions.
  // Please refer to gRpc tutorial how to create a stub.
  // ------------------------------------------------------------
    
  ///////////////////////////////////////////////////////////
  // YOUR CODE HERE
  //////////////////////////////////////////////////////////

  std::shared_ptr<Channel> channel = grpc::CreateChannel(hostname + ":" + port, grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<SNSService::Stub>(channel);

  IReply reply = Login();

  if (!reply.grpc_status.ok() || reply.comm_status != SUCCESS) {
    return -1;
  }

  return 1;
}

IReply Client::processCommand(std::string& input)
{
  // ------------------------------------------------------------
  // GUIDE 1:
  // In this function, you are supposed to parse the given input
  // command and create your own message so that you call an 
  // appropriate service method. The input command will be one
  // of the followings:
  //
  // FOLLOW <username>
  // UNFOLLOW <username>
  // LIST
  // TIMELINE
  // ------------------------------------------------------------
  
  // ------------------------------------------------------------
  // GUIDE 2:
  // Then, you should create a variable of IReply structure
  // provided by the client.h and initialize it according to
  // the result. Finally you can finish this function by returning
  // the IReply.
  // ------------------------------------------------------------
  
  
  // ------------------------------------------------------------
  // HINT: How to set the IReply?
  // Suppose you have "FOLLOW" service method for FOLLOW command,
  // IReply can be set as follow:
  // 
  //     // some codes for creating/initializing parameters for
  //     // service method
  //     IReply ire;
  //     grpc::Status status = stub_->FOLLOW(&context, /* some parameters */);
  //     ire.grpc_status = status;
  //     if (status.ok()) {
  //         ire.comm_status = SUCCESS;
  //     } else {
  //         ire.comm_status = FAILURE_NOT_EXISTS;
  //     }
  //      
  //      return ire;
  // 
  // IMPORTANT: 
  // For the command "LIST", you should set both "all_users" and 
  // "following_users" member variable of IReply.
  // ------------------------------------------------------------

    IReply ire;
    
    /*********
    YOUR CODE HERE
    **********/

    // Assuming that string input will always be a valid command
    size_t loc = input.find(" ");
    std::string cmd = input.substr(0, loc);
    std::string arg;
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
      // client.cc handles timeline
      ire.grpc_status = Status::OK;
      ire.comm_status = SUCCESS;
    } 

    return ire;
}


void Client::processTimeline()
{
    Timeline(username);
}

// List Command
IReply Client::List() {

    IReply ire;

    /*********
    YOUR CODE HERE
    **********/

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

// Follow Command        
IReply Client::Follow(const std::string& username2) {

    IReply ire; 

    /***
    YOUR CODE HERE
    ***/

    ClientContext context;
    Request request;
    Reply reply;
    request.set_username(username);
    request.add_arguments(username2);

    timestampRequest(&request);
    Status status = stub_->Follow(&context, request, &reply);

    ire.grpc_status = status;
    ire.comm_status = SUCCESS;

    if(status.ok()) {
      ire.comm_status = reply.status();
      std::cout << reply.msg() << std::endl;
    } 

    return ire;
}

// UNFollow Command  
IReply Client::UnFollow(const std::string& username2) {

    IReply ire;
    std::string failure_msg = "You do not follow this user...";

    /***
    YOUR CODE HERE
    ***/

    ClientContext context;
    Request request;
    Reply reply;
    request.set_username(username);
    request.add_arguments(username2);

    timestampRequest(&request);
    Status status = stub_->UnFollow(&context, request, &reply);

    ire.grpc_status = status;
    ire.comm_status = SUCCESS;

    if(status.ok()) {
      ire.comm_status = reply.status();
      std::cout << reply.msg() << std::endl;
    } 

    return ire;
}

// Login Command  
IReply Client::Login() {

    IReply ire;

    /***
     YOUR CODE HERE
    ***/
    
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
      std::cout << reply.msg() << std::endl;
    } 

    return ire;
}

// Timeline Command
void Client::Timeline(const std::string& username) {

    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions 
    // in client.cc file for both getting and displaying messages 
    // in timeline mode.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
    // ------------------------------------------------------------
  
    /***
    YOUR CODE HERE
    ***/

    ClientContext context;
    std::unique_ptr<ClientReaderWriter<Message, Message>> stream(stub_->Timeline(&context));

    std::thread writer([&stream, username]() {
      
      // Ensure that the client's stream field is set
      stream->Write(MakeMessage(username, ""));
      while(1) {
        std::string msgToSend = getPostMessage();
        Message m = MakeMessage(username, msgToSend);
        stream->Write(m);
      }

    });

    Message message;
    while(stream->Read(&message)) {
      std::string u = message.username();
      std::string m = message.msg();
      std::time_t t = message.timestamp().seconds();
      displayPostMessage(u, m, t);
    }

    writer.join();
    Status status = stream->Finish();

    if(!status.ok()) {
      std::cout << "Timeline gRPC failed..." << std::endl;
    }
}



//////////////////////////////////////////////
// Main Function
/////////////////////////////////////////////
int main(int argc, char** argv) {

  std::string hostname = "localhost";
  std::string username = "default";
  std::string port = "3010";
    
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
    switch(opt) {
    case 'h':
      hostname = optarg;break;
    case 'u':
      username = optarg;break;
    case 'p':
      port = optarg;break;
    default:
      std::cout << "Invalid Command Line Argument\n";
    }
  }
      
  std::cout << "Logging Initialized. Client starting...";
  
  Client myc(hostname, username, port);
  
  myc.run();
  
  return 0;
}
