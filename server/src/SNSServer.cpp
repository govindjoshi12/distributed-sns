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
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "SNSServer.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

// Client-Server Communication
using SNS::SNSService;
using SNS::Message;
using SNS::ListReply;
using SNS::Request;
using SNS::Reply;
using SNS::SNSStatus;

// Server-Server Communication
using SNS::SiblingRequest;
using SNS::LogReply;

// Server-Coordinator Communication
using SNS::CoordService;
using SNS::Path;
using SNS::FileInfo;
using SNS::ServerInfo;
using SNS::ServerList;

using namespace SNS;
using namespace std;

// ---- UTILITY FUNCTIONS ----

// Split str into tokens delimited by delim
// Returns a vector of strings
vector<string> split(const string &str, char delim) {
	stringstream ss(str);
	string s;
	vector<string> res;
	while(getline(ss, s, delim)) {
		res.push_back(s);
	}
	return res;
}

// Construct string from timestamp
string timestampToStr(const Timestamp &t) {
	const time_t secs = t.seconds();
	struct tm *to = localtime(&secs);
	string timeStr;

	string yr = to_string(to->tm_year + 1900); // tm_year is years since 1900
	string mon = to_string(to->tm_mon + 1); // starts at 0
	string day = to_string(to->tm_mday);
	string hr = to_string(to->tm_hour);
	string min = to_string(to->tm_min);
	string sec = to_string(to->tm_sec);

	// timeStr: "%Y-%m-%dT%H:%M:%S"
	timeStr = yr + "-" + mon + "-" + day + "T" + hr + ":" + min + ":" + sec;
	return timeStr;
}

time_t strToSeconds(const string &timestampStr) {
	tm t{};
	istringstream ss(timestampStr);

	ss >> get_time(&t, "%Y-%m-%dT%H:%M:%S");
	if (ss.fail()) {
		throw runtime_error{"failed to parse time string"};
	}   
	time_t seconds = mktime(&t);

	return seconds;
}

Timestamp strToTimestamp(const string &timestampStr) {
	
	time_t seconds = strToSeconds(timestampStr);
	Timestamp t;
	t.set_seconds(seconds);
	t.set_nanos(0);
	
	return t;
}

// https://stackoverflow.com/questions/17681439/convert-string-time-to-unix-timestamp
Timestamp* strToAllocatedTimestamp(const string &timestampStr) {

	time_t seconds = strToSeconds(timestampStr);
	google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
	timestamp->set_seconds(seconds);
	timestamp->set_nanos(0);

	return timestamp;
}

// ---- STATIC FUNCTIONS AND MEMBERS ----

string SNSServer::TOP_LEVEL_DIR = "sns";
string SNSServer::MASTER = "master";
string SNSServer::SLAVE = "slave";
string SNSServer::POSTS = "posts";
string SNSServer::USER_INFO = "userinfo";
string SNSServer::USER = "user";
string SNSServer::LOGIN = "login";
string SNSServer::FOLLOW = "follow";
string SNSServer::UNFOLLOW = "unfollow";
string SNSServer::ADD_POST = "add_post";
string SNSServer::COUNTERPARTS = "counterparts";
string SNSServer::OTHER_MASTERS = "other_masters";

// Message format:
/*
	Message format:
	T <timestamp>
	U <user>
	W <post content>
	<newline>
*/
string SNSServer::formatMessageOutput(const Message &message) 
{
	string timeStr = timestampToStr(message.timestamp());
	string u = message.username();
	string m = message.msg();

    string output;

    output += "T " + timeStr;
    output += "\nU " + u;
    output += "\nW " + m + "\n\n";
    return output;
}


// ---- SERVER ----

SNSServer::SNSServer() 
{
	filesys = make_shared<FSLocal>(TOP_LEVEL_DIR);
}

// ---- COORDINATOR COMMUNICATION ----

/*
	INITIALIZATION METHODS
*/

int SNSServer::connectToCoordinator(string hostname, string port,
                          string coordHostname, string coordPort,
                          int clusterId, int serverId) 
{

	string coordAddr = coordHostname + ":" + coordPort;
	shared_ptr<Channel> coordChannel = grpc::CreateChannel(coordAddr, grpc::InsecureChannelCredentials());
	coordStub_ = make_unique<CoordService::Stub>(coordChannel);

	ClientContext context;

	serverInfo.set_clusterid(clusterId);
	serverInfo.set_serverid(serverId);
	serverInfo.set_hostname(hostname);
	serverInfo.set_port(port);
	serverInfo.set_type("SERVER");
	serverInfo.set_registered(false);

	Status status = coordStub_->Heartbeat(&context, serverInfo, &path);
	if(!status.ok()) {
		cout << status.error_message() << "\n";
		return -1;
	}

	serverInfo.set_registered(true);

	master = path.master();
	string masterString = master ? MASTER : SLAVE;
	log(INFO, "Server registered with file lock: " + path.path() + ". Server is " + masterString);

	setFilepaths(clusterId, serverId);
	initializeData(path.sync_address());

	thread(&SNSServer::sendHeartbeat, this).detach();

	// Since each server will be on the same machine for now, 
	// give each one a unique local path in which to save data

	return 1;
}

// Set local dir and filepaths that are reused throughout
void SNSServer::setFilepaths(int clusterId, int serverId) {
	localPath = "c" + to_string(clusterId) + "s" + to_string(serverId) + "/";
	userinfoPath = localPath + USER_INFO;
	postsPath = localPath + POSTS;
}

/* 
	If this server is a slave, sync address will contain the address
	of the cluster master. If it's a master, sync address will contain
	the address of a different cluster master, or will be empty.
	If empty, try to initialize from local files. 
*/
void SNSServer::initializeData(const string &syncAddress) {

	// Fails if localpath already exists
	filesys->create(localPath, true, true);

	string userinfo;
	string posts;
	bool saveToLocal = false;
	if(syncAddress == "") {

		// Even reads fails, userinfo and posts will contain empty strings
		log(INFO, "No other cluster master available to sync with. Trying to sync with local files.");

		int readUserinfo = filesys->read(userinfoPath, userinfo);
		if(readUserinfo == 0) {
			log(INFO, "Synced with local userinfo file.");
		}

		int readPosts = filesys->read(postsPath, posts);
		if(readPosts == 0) {
			log(INFO, "Synced with local posts file.");
		}

	} else {

		// Since we are initializing from a different file, clear the contents
		// of any existing file. Create empty files if they don't exist.
		filesys->write(userinfoPath, "", true, true);
		filesys->write(postsPath, "", true, true);
		saveToLocal = true;

		shared_ptr<Channel> serverChannel = grpc::CreateChannel(syncAddress, grpc::InsecureChannelCredentials());
		unique_ptr<SNSService::Stub> stub_ = make_unique<SNSService::Stub>(serverChannel);

		ClientContext context;
		SiblingRequest request;
		LogReply logReply;

		Status status = stub_->GetLog(&context, request, &logReply);

		// Possible source of failure: master fails while just before requesting data
		// In current system, if any grpc fails on the server side, we exit.
		if(!status.ok()) {
			log(FATAL, "Initialization sync failed!");
			// fatal log exits
		}
	
		userinfo = logReply.userinfo();
		posts = logReply.posts();

		log(INFO, "Synchronized with server @ " + syncAddress);
	}

	/* 
		No need to propogate this initialization.
		
		If you are a slave, you are initializing from the current cluster master,
		and slaves do not propogate commands.
		
		If you are a initializing as a cluster master, there are no slaves 
		available to propogate to yet.
	*/
	processUserInfoFromFile(userinfo, saveToLocal);
	updatePostsFromFile(posts, saveToLocal);
}

/*
	Send a heartbeat to the coordinator every heartbeatDelay seconds
	to inform it that this server is still alive. Each heartbeat conducts 
	a leader election if the current cluster master is dead.
*/
void SNSServer::sendHeartbeat() 
{
	while(true) {

		sleep(heartbeatDelay);

		ClientContext context;
		Status status = coordStub_->Heartbeat(&context, serverInfo, &path);

		if(!status.ok()) {
			log(ERROR, "Heartbeat failed...");
			exit(1);
		}

		bool originalMasterStatus = master;
		master = path.master();
		if(master && master != originalMasterStatus) {
			string message =
			"Master file lock acquired! Server is now master.";
			log(INFO, message);
		}
	}
}

// ---- CLIENT HELPERS ----

// Returns pointer to the client with provided username
// If createNew is true, creates a new client with username
shared_ptr<Client> SNSServer::getClient(const string &username, bool createNew)
{
	shared_ptr<Client> client = NULL;
	bool found = false;
	for(int i = 0; i < (int)(client_db.size()); i++) {
		shared_ptr<Client> c = client_db[i];
		if(username == c->username) {
			client = client_db[i];
			found = true;
			break;
		}
	}

	// If not found, returns a new client with provided username
	// So logging in with a new username creates a new user.
	if(!found && createNew) {
		client = make_shared<Client>();
		client->username = username;
	}

	return client;
}

// Returns the index of client2 in client1's following list
int SNSServer::indexOfFollowing(shared_ptr<Client> client1, shared_ptr<Client> client2)
{
	int idx = -1;
	for(int i = 0; i < ((int)client1->client_following.size()); i++) {
		if(*client1->client_following[i] == *client2) {
			idx = i;
			break;
		}
	}
	return idx;
}

// Returns index of client2 in client1's follower list
int SNSServer::indexOfFollower(shared_ptr<Client> client1, shared_ptr<Client> client2)
{
	int idx = -1;
	for(int i = 0; i < ((int)client1->client_followers.size()); i++) {
		if(*client1->client_followers[i] == *client2) {
			idx = i;
			break;
		}
	}
	return idx;
}

// Gets the latest maxPosts number of posts made by users followed by c
// Posts are only considered if they were made after c followed the poster
vector<shared_ptr<Post>> SNSServer::getFollowingPosts(shared_ptr<Client> c, int maxPosts)
{
	vector<shared_ptr<Client>> *following = &c->client_following;
	vector<Timestamp> *timestamps = &c->follow_time;

	vector<shared_ptr<Post>> timelinePosts;
	int i = (int)(all_posts.size()) - 1;
	while(i >= 0 && maxPosts > 0) {

		shared_ptr<Post> post = all_posts[i];

		// Posts can only be made by existing users so this won't fail
		shared_ptr<Client> author = getClient(post->username);
		Timestamp postTime = post->timestamp;
		
		auto it = find(following->begin(), following->end(), author);
		if(it != following->end()) {
			
			int idx = it - following->begin();
			Timestamp followTime = (*timestamps)[idx];

			if(postTime > followTime) {
				timelinePosts.push_back(post);
				maxPosts--;
			}
		}

		i--;
	}

	return timelinePosts;
}

// WRITE DATA TO LOCAL FILE
void SNSServer::write(string &file, string &data)
{
	// Note: when using a string &reference, you cannot perform
	// string concatenation in the function call. Ex. func(a + b);

	filesys->write(file, data, false, false);
}

// ---- REPLICATION HELPERS ----

/*
	These helper methods assume that the input is valid
*/

void SNSServer::loginHelper(string user)
{
	shared_ptr<Client> client = getClient(user, true);
	client->connected = true;
	client_db.push_back(client);
}

void SNSServer::followHelper(string user1, string user2, string timestampStr)
{
	shared_ptr<Client> client = getClient(user1);
	shared_ptr<Client> toFollow = getClient(user2);
	Timestamp t = strToTimestamp(timestampStr);

	client->client_following.push_back(toFollow);
	client->follow_time.push_back(t);
	toFollow->client_followers.push_back(client);
}

void SNSServer::unfollowHelper(string user1, string user2)
{
	shared_ptr<Client> client = getClient(user1);
	shared_ptr<Client> toUnFollow = getClient(user2);

	vector<shared_ptr<Client>> *clientFollowing = &client->client_following;
	vector<Timestamp> *clientFollowTimes = &client->follow_time;
	vector<shared_ptr<Client>> *targetFollowers = &toUnFollow->client_followers;

	int index = indexOfFollowing(client, toUnFollow);
	int index2 = indexOfFollower(toUnFollow, client);

	clientFollowing->erase(clientFollowing->begin() + index);
	clientFollowTimes->erase(clientFollowTimes->begin() + index);
	targetFollowers->erase(targetFollowers->begin() + index2);
}

void SNSServer::addPostHelper(const Message &message, shared_ptr<Client> client, bool writeToFile)
{
	// Don't write to you own stream

	// Store message in list
	shared_ptr<Post> p = make_shared<Post>(message);
	all_posts.push_back(p);

	// Write message to file
	string data = formatMessageOutput(message);
	if(writeToFile) {
		write(postsPath, data);
	}

	// Write to all followers streams if they exist
	// If you are a slave, no client streams eixst
	for(shared_ptr<Client> follower : client->client_followers) {
		
		// Only write to stream if follower has joined the timeline
		if(!(follower->stream == NULL)) {
			follower->stream->Write(message);
		}
	}
}

// Uses the client API helpers to process input from file
void SNSServer::processUserInfoFromFile(string filedata, bool save)
{
	stringstream ss(filedata);
	string s;

	while(getline(ss, s, '\n')) {
		vector<string> args = split(s);
		string op = args[0];

		string methodStr;
		Request r;
		if(op == USER) {
			loginHelper(args[1]);
		} else if(op == FOLLOW) {
			followHelper(args[1], args[2], args[3]);
		} else if(op == UNFOLLOW) {
			unfollowHelper(args[1], args[2]);
		}

		// Write the line back to our own current file
		if(save) {
			s += "\n";
			write(userinfoPath, s);
		}
	}
}

void SNSServer::updatePostsFromFile(string filedata, bool save)
{
	// Posts in file will always follow the format:
	// T <timestamp>
	// U <username>
	// W <post content>
	// newline

	// post content not allowed to contain newlines

	// Method from filesystem_utils.h.
	// TODO: would be nice to have my own lib of c++ utils
	vector<string> postTokens = splitString(filedata, "\n\n");
	for(string token : postTokens) {

		vector<string> postParts = splitString(token, "\n");
		
		// May contain a dangling empty string at the end
		if(postParts.size() > 0) {
			Timestamp *timestamp = strToAllocatedTimestamp(postParts[0].substr(2));
			string username = postParts[1].substr(2);
			shared_ptr<Client> client = getClient(username);
			string content = postParts[2].substr(2);

			Message message;

			// set_allocated_* takes ownership of pointer and deletes it when done with it
			message.set_allocated_timestamp(timestamp);
			message.set_username(username);
			message.set_msg(content);

			addPostHelper(message, client, save);
		}
	}
}

void SNSServer::propogateHelper(string method, const Request &request, string destination) 
{
	ClientContext context;
	ServerList serverList;
	Status status;
	
	if(destination == COUNTERPARTS) {
		status = coordStub_->GetCounterparts(&context, serverInfo, &serverList);
	} else if(destination == OTHER_MASTERS) {
		status = coordStub_->GetOtherClusterMasters(&context, serverInfo, &serverList);
	} else {
		// sanity check
		log(FATAL, "Invalid propogation destination specified!");
	}

	if(!status.ok()) {
		log(FATAL, "Request to coordinator for " + destination + " failed!");
	}

	// Not sure if creating stubs each time will have a big performance impact
	for(const ServerInfo &s : serverList.servers()) {

		string addr = s.hostname() + ":" + s.port();
		shared_ptr<Channel> slaveChannel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
		unique_ptr<SNSService::Stub> slaveStub_ = make_unique<SNSService::Stub>(slaveChannel);

		ClientContext slaveContext;
		Reply reply;
		Status status;

		if(method == LOGIN) {
			status = slaveStub_->Login(&slaveContext, request, &reply);
		} else if(method == FOLLOW) {
			status = slaveStub_->Follow(&slaveContext, request, &reply);
		} else if(method == UNFOLLOW) {
			status = slaveStub_->UnFollow(&slaveContext, request, &reply);
		} else if(method == ADD_POST) {
			status = slaveStub_->AddPost(&slaveContext, request, &reply);
		} else {
			// sanity check: invalid option
			log(FATAL, "Invalid replication method!");
			return;
		}

		// TODO: No handling of error status right now
		// Ideally, fail if propogation fails and retry

		log(INFO, "Sent " + method + " request to " + addr);
	}
}

void SNSServer::propogate(string method, const Request *request) {
	
	// Any propogation only occurs if you are a master
	if(master) {

		Request newRequest;
		newRequest.CopyFrom(*request);
		newRequest.set_from_server(true);

		// If you are a master, you always propogate to slaves
		propogateHelper(method, newRequest, COUNTERPARTS);

		// If the request is from a client, propogate to other cluster masters
		// from_server is only set by servers
		if(!request->from_server()) {
			propogateHelper(method, newRequest, OTHER_MASTERS);
		}
	}
}

// ---- CLIENT API ----

// TODO: Use the helpers in the rpcs for consistency

/*
	If the gRPC succeeds, the API Methods always return an OK status. 
	Status about the command requested is communicated through the returned reply.
	The server fills the reply with the appropriate SNSStatus and a message
	which the client can display to its users.  
*/

Status SNSServer::Login(ServerContext* context, const Request* request, Reply* reply) 
{
	// Guaranteed to be non-null because method returns a new Client
	// with provided username if one is not found
	shared_ptr<Client> client = getClient(request->username(), true);

	SNSStatus status = SUCCESS;
	string message;

	// If client disconnects, it still exists in server memory
	// Allow reconnecting.
	if(client->connected) {
		message = "Welcome back, " + request->username();
	} else {
		client->connected = true;
		client_db.push_back(client);
		message = "User " + request->username() + " successfully logged in!";

		string data = USER + " " + request->username() + "\n";
		write(userinfoPath, data);
	}

	reply->set_status(status);
	reply->set_msg(message);

	/* Replicate */
	log(INFO, "Received login command for user " + client->username);
	
	propogate(LOGIN, request);
	
	// TODO: Log login command
	
	return Status::OK;
}

Status SNSServer::Follow(ServerContext* context, const Request* request, Reply* reply) 
{

	// Requesting user will always exist and be logged in before they can make this request
	shared_ptr<Client> client = getClient(request->username());
	shared_ptr<Client> toFollow = getClient(request->arguments()[0]);
	Timestamp t = request->timestamp();

	SNSStatus status = SUCCESS;
	string message;

	if(toFollow == NULL) {

		status = FAILURE_NOT_EXISTS;
		message = "The specified user does not exist.";

	} else if(client == toFollow) {

		// I can do a raw pointer comparision because get client
		// will return the same pointer if the username is the same
		status = FAILURE_INVALID;
		message = "You cannot follow yourself.";

	} else {

		int index = indexOfFollowing(client, toFollow);
		// You already follow this user
		if(index >= 0) {
			status = FAILURE_ALREADY_EXISTS;
			message = "You already follow this user";
		} else {
			client->client_following.push_back(toFollow);
			client->follow_time.push_back(t);
			toFollow->client_followers.push_back(client);

			string timeStr = timestampToStr(t);
			string data = FOLLOW + " " + client->username + " " + toFollow->username + " " + timeStr + "\n";
			write(userinfoPath, data);

			message = "Successfully followed " + toFollow->username;
		}
	}

	reply->set_status(status);
	reply->set_msg(message);

	// REPLICATION
	log(INFO, "Received follow request from " + client->username + " for " + toFollow->username + ": " + message);
	
	propogate(FOLLOW, request);

	return Status::OK; 
}

Status SNSServer::UnFollow(ServerContext* context, const Request* request, Reply* reply) 
{
	shared_ptr<Client> client = getClient(request->username());
	shared_ptr<Client> toUnFollow = getClient(request->arguments()[0]);

	SNSStatus status = SUCCESS;
	string message;

	if(toUnFollow == NULL) {

		status = FAILURE_NOT_EXISTS;
		message = "The specified user does not exist.";

	} else if(client == toUnFollow) {

		status = FAILURE_INVALID;
		message = "You cannot unfollow yourself.";

	} else {

		int index = indexOfFollowing(client, toUnFollow);
		if(index < 0) { 
			status = FAILURE_NOT_A_FOLLOWER;
			message = "You already do not follow this user...";
		} else {
			vector<shared_ptr<Client>> *clientFollowing = &client->client_following;
			vector<Timestamp> *clientFollowTimes = &client->follow_time;
			vector<shared_ptr<Client>> *targetFollowers = &toUnFollow->client_followers;

			int index2 = indexOfFollower(toUnFollow, client);

			clientFollowing->erase(clientFollowing->begin() + index);
			clientFollowTimes->erase(clientFollowTimes->begin() + index);
			targetFollowers->erase(targetFollowers->begin() + index2);

			string data = UNFOLLOW + " " + client->username + " " + toUnFollow->username + "\n";
			write(userinfoPath, data);

			message = "Successfully unfollowed user " + toUnFollow->username;
		}

	}

	reply->set_msg(message);
	reply->set_status(status);

	log(INFO, "Received unfollow request from " + client->username + " for " + toUnFollow->username + ": " + message);
	
	propogate(UNFOLLOW, request);

	return Status::OK; 
}

Status SNSServer::List(ServerContext* context, const Request* request, ListReply* list_reply) 
{
	shared_ptr<Client> client = getClient(request->username());

	int numUsers = (int)client_db.size();
	for(int i = 0; i < numUsers; i++) {
		list_reply->add_all_users(client_db[i]->username);
	}

	int numFollowers = (int)client->client_followers.size();
	for(int i = 0; i < numFollowers; i++) {
		list_reply->add_followers(client->client_followers[i]->username);
	}

	int numFollowing = (int)client->client_following.size();
	for(int i = 0; i < numFollowing; i++) {
		list_reply->add_following(client->client_following[i]->username);
	}

	// TODO: Log command
	log(INFO, "Received list request from " + client->username);
	return Status::OK;
}

Status SNSServer::Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) 
{
	// https://grpc.io/docs/languages/cpp/basics/
	Message message;

	while (stream->Read(&message)) {
		
		const Timestamp &t = message.timestamp();
		const string &u = message.username();
		const string &m = message.msg();

		// If you do this inside the while loop,
		// the current client doesn't start receiving 
		// messages until after it writes its first message
		shared_ptr<Client> client = getClient(u);
		if(client->stream == NULL) {
			

			log(INFO, "Entering timeline with " + client->username);
	
			// Upon joining the timeline, user sends an initial empty message
			// to indicate that this stream belongs to it. Don't write this
			// message to the timeline, and simply set the client's stream.
			client->stream = stream;
			vector<shared_ptr<Post>> timelinePosts = getFollowingPosts(client, 20);

			// According to testcases, these should be printed in reverse chronological order
			for(shared_ptr<Post> post : timelinePosts) {
				
				Message messageToSend;
				messageToSend.set_username(post->username);
				messageToSend.set_msg(post->data);

				// set_allocated_* takes ownership then deletes
				Timestamp* allocatedTimestamp = new google::protobuf::Timestamp(post->timestamp);
				messageToSend.set_allocated_timestamp(allocatedTimestamp);

				stream->Write(messageToSend);
			}

		} else {

			// Empty posts are not allowed, but there is a possibility
			// where an empty message to initialize the stream but the
			// stream already exists. Ignore this empty message
			if(m != "") {
				addPostHelper(message, client);

				// TODO: Propogate post to slaves
				if(master) {
					Request request;
					Message* toPropogate = new Message();
					toPropogate->CopyFrom(message); // Copy message

					// Transfer ownership
					request.set_allocated_message(toPropogate);
					propogate(ADD_POST, &request);
				}
			}
		}
	}

	return Status::OK;
}

// ---- CROSS-SERVER COMMUNICATION ----

Status SNSServer::GetLog(ServerContext *context, const SiblingRequest* sb, LogReply* logReply)
{
	string userinfo;
	string posts;

	filesys->read(userinfoPath, userinfo);
	filesys->read(postsPath, posts);

	logReply->set_userinfo(userinfo);
	logReply->set_posts(posts);

	return Status::OK;
}

/*
	Adds the message to the list of posts maintained by the server.
	Using a Request object rather than simply a Message object for convenience
	For each post, finds the followers of the poster and adds the post to their stream (if open)
	If the server is a slave, no streams will be open.
*/
Status SNSServer::AddPost(ServerContext *context, const Request* request, Reply* reply) 
{
	const Message &message = request->message();

	// this should always return a non-null client pointer
	// as the poster will be in the client_db before AddPost is called
	shared_ptr<Client> client = getClient(message.username(), false);

	addPostHelper(message, client);

	// TODO: More verbose message
	log(INFO, "Received add_post request.");
	return Status::OK;
}

