/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>
#include <string>
#include <memory>

#include <grpc++/grpc++.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/util/time_util.h>

#include <glog/logging.h>
#define log(severity, msg); LOG(severity) << msg << "\n---"; google::FlushLogFiles(google::severity);

#include "FSWrapper/FSWrapper.h"
#include "FSWrapper/FSMemory.h"
#include "FSWrapper/FSLocal.h"

#include <snsproto/sns.grpc.pb.h>
#include <snsproto/coordinator.grpc.pb.h>

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::ServerBuilder;
using grpc::Status;

// Server
using SNS::SNSService;
using SNS::Message;
using SNS::ListReply;
using SNS::Request;
using SNS::Reply;

// Coordinator
using SNS::CoordService;
using SNS::Path;
using SNS::ServerInfo;

using namespace SNS;

// ---- UTILITY FUNCTIONS HEADERS ----

std::vector<std::string> split(const std::string &str, char delim = ' ');
std::string timestampToStr(const Timestamp &t);
time_t strToSeconds(const std::string &timestampStr);
Timestamp strToTimestamp(const std::string &timestampStr);
Timestamp* strToAllocatedTimestamp(const std::string &timestampStr);


// ---- HELPER STRUCTS ----

struct Client {
  std::string username;
  bool connected = false;
  int following_file_size = 0;

  std::vector<std::shared_ptr<Client>> client_followers;
  std::vector<std::shared_ptr<Client>> client_following;
  std::vector<Timestamp> follow_time;

  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
	return (username == c1.username);
  }
  bool operator>(const Client& c1) const{
	return (username > c1.username);
  }
};

struct Post {

  std::string username;
  std::string data;
  Timestamp timestamp;

  Post(const Message &mInit) {
	username = mInit.username();
	data = mInit.msg();
	timestamp = mInit.timestamp();
  };

};

// ---- SERVER CLASS ----

class SNSServer final : public SNSService::Service {
  
public:

	SNSServer();
	virtual ~SNSServer() {};

	// Initialization
	int connectToCoordinator(std::string hostname, std::string port,
						  std::string coordHostname, std::string coordPort,
						  int clusterId, int serverId);

	// Client API
	Status Login(ServerContext* context, const Request* request, Reply* reply);
	Status Follow(ServerContext* context, const Request* request, Reply* reply);
	Status UnFollow(ServerContext* context, const Request* request, Reply* reply);
	Status List(ServerContext* context, const Request* request, ListReply* list_reply);
	Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream);

	// Cross-Server Communication
	Status GetLog(ServerContext *context, const SiblingRequest* sb, LogReply* logReply);
	Status AddPost(ServerContext *context, const Request* request, Reply* reply);

private:

	std::unique_ptr<CoordService::Stub> coordStub_;
	ServerInfo serverInfo;
	Path path;
	bool master;

	std::shared_ptr<FSWrapper> filesys;
	std::string localPath;
	std::string userinfoPath;
	std::string postsPath;

	int heartbeatDelay = 3;
	int synchDelay = 10;

	std::vector<std::shared_ptr<Client>> client_db;
	std::vector<std::shared_ptr<Post>> all_posts;

	// ---- COORDINATOR COMMUNICATION ----
	void sendHeartbeat();

	// ---- CLIENT API HELPERS ----
	std::shared_ptr<Client> getClient(const std::string &username, bool createNew = false);
  	int indexOfFollowing(std::shared_ptr<Client> client1, std::shared_ptr<Client> client2);
  	int indexOfFollower(std::shared_ptr<Client> client1, std::shared_ptr<Client> client2);
	std::vector<std::shared_ptr<Post>> getFollowingPosts(std::shared_ptr<Client> c, int maxPosts);
	void write(std::string &filename, std::string &data);

	// ---- REPLICATION HELPERS ----
	void setFilepaths(int clusterId, int serverId);
	void initializeData(const std::string &syncAddress);

	void loginHelper(std::string user);
	void followHelper(std::string user1, std::string user2, std::string timestampStr);
	void unfollowHelper(std::string user1, std::string user2);
	void addPostHelper(const Message &message, std::shared_ptr<Client> client, bool writeToFile = true);

	void processUserInfoFromFile(std::string filedata, bool save);
	void updatePostsFromFile(std::string filedata, bool save);
	void propogateHelper(std::string method, const Request &request, std::string destination);
	void propogate(std::string method, const Request* request);

	// STATIC MEMBERS AND FUNCTIONS
  	static std::string formatMessageOutput(const Message &message);

	static std::string MASTER;
	static std::string SLAVE;
	static std::string POSTS;
	static std::string USER_INFO;
	static std::string USER;
	static std::string LOGIN;
	static std::string FOLLOW;
	static std::string UNFOLLOW;
	static std::string ADD_POST;
	static std::string TOP_LEVEL_DIR;
	static std::string COUNTERPARTS;
	static std::string OTHER_MASTERS;
};

/*
	Note: Usage of const string references is inconsistent and only done
	when necessary. Possible TODO: use const string references everywhere possible
*/