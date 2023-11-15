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

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::IStatus;

using namespace csce438;

std::string LOGIN_SUCCESS = "Login Successful!";
std::string LOGIN_FAILURE = "Login Unsuccessful...";
std::string DIR = "user_data/";

struct Client {
  std::string username;
  bool connected = false;
  int following_file_size = 0;
  std::vector<ClientData*> client_followers;
  std::vector<ClientData*> client_following;

  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
  bool operator>(const Client& c1) const{
    return (username > c1.username);
  }
};

struct Post {
  Message m;
  Client* c;
};

struct ClientData {
  Client* c;
  Timestamp t;
}

//Vector that stores every client that has been created
std::vector<Client*> client_db;
std::vector<Post*> posts;

class SNSServiceImpl final : public SNSService::Service {
  
  Client* getClient(std::string username, bool createNew = false) {
    
    Client* client = NULL;
    bool found = false;
    for(int i = 0; i < (int)(client_db.size()); i++) {
      Client* c = client_db[i];
      if(username == c->username) {
        client = client_db[i];
        found = true;
        break;
      }
    }

    // If not found, returns a new client with provided username
    // So logging in with a new username creates a new user.
    if(!found && createNew) {
      client = new Client;
      client->username = username;
    }

    return client;
  }

  // returns index of client2 in client1's following list
  int indexOfFollowing(Client* client1, Client* client2) {
    int idx = -1;
    for(int i = 0; i < ((int)client1->client_following.size()); i++) {
      if(*client1->client_following[i]->c == *client2) {
        idx = i;
        break;
      }
    }
    return idx;
  }

  // Returns index of client2 in client1's follower list
  int indexOfFollower(Client* client1, Client* client2) {
    int idx = -1;
    for(int i = 0; i < ((int)client1->client_followers.size()); i++) {
      if(*client1->client_followers[i]->c == *client2) {
        idx = i;
        break;
      }
    }
    return idx;
  }

  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    /*********
    YOUR CODE HERE
    **********/

    Client* client = getClient(request->username());

    IStatus status = SUCCESS;
    std::string message;

    int numUsers = (int)client_db.size();
    for(int i = 0; i < numUsers; i++) {
      list_reply->add_all_users(client_db[i]->username);
    }

    int numFollowers = (int)client->client_followers.size();
    for(int i = 0; i < numFollowers; i++) {
      list_reply->add_followers(client->client_followers[i]->c->username);
    }

    int numFollowing = (int)client->client_following.size();
    for(int i = 0; i < numFollowing; i++) {
      list_reply->add_following(client->client_following[i]->c->username);
    }

    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {

    /*********
    YOUR CODE HERE
    **********/

    // Requesting user will always exist and be logged in before they can make this request
    Client* client = getClient(request->username());
    Client* toFollow = getClient(request->arguments()[0]);


    IStatus status = SUCCESS;
    std::string message;

    if(toFollow == NULL) {

      status = FAILURE_NOT_EXISTS;
      message = "There is no user with the provided username...";

    } else if(client == toFollow) {

      // I can do a raw pointer comparision because get client
      // will return the same pointer if the username is the same
      status = FAILURE_INVALID;
      message = "You cannot follow yourself...";

    } else {

      int index = indexOfFollowing(client, toFollow);
      // You already follow this user
      if(index >= 0) {
        status = FAILURE_ALREADY_EXISTS;
        message = "You already follow " + toFollow->username;
      } else {
        ClientData* toFollowCD = new ClientData;
        toFollowCD->c = toFollow;
        toFollowCD->t = request->timestamp();

        ClientData* clientCD = new ClientData;
        clientCD->c = client;
        clientCD->t = request->timestamp();

        client->client_following.push_back(toFollow);
        toFollow->client_followers.push_back(client);
        message = "Successfully followed " + toFollow->username;
      }

    }

    reply->set_msg(message);
    reply->set_status(status);

    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {

    /*********
    YOUR CODE HERE
    **********/

    Client* client = getClient(request->username());
    Client* toUnFollow = getClient(request->arguments()[0]);

    IStatus status = SUCCESS;
    std::string message;

    if(toUnFollow == NULL) {

      status = FAILURE_NOT_EXISTS;
      message = "There is no user with the provided username...";

    } else if(client == toUnFollow) {

      status = FAILURE_INVALID;
      message = "You cannot unfollow yourself...";

    } else {

      int index = indexOfFollowing(client, toUnFollow);
      if(index < 0) { 
        status = FAILURE_NOT_A_FOLLOWER;
        message = "You already do not follow this user...";
      } else {
        std::vector<Client*> *clientFollowing = &client->client_following;
        std::vector<Client*> *targetFollowers = &toUnFollow->client_followers;

        int index2 = indexOfFollower(toUnFollow, client);

        clientFollowing->erase(clientFollowing->begin() + index);
        targetFollowers->erase(targetFollowers->begin() + index2);

        message = "Successfully unfollowed user " + toUnFollow->username;
      }

    }

    reply->set_msg(message);
    reply->set_status(status);

    return Status::OK; 
  }

  // RPC Login
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {

    /*********
    YOUR CODE HERE
    **********/

    // Guaranteed to be non-null because method returns a new Client
    // with provided username if one is not found
    Client* client = getClient(request->username(), true);

    IStatus status = SUCCESS;
    std::string message;

    if(client->connected) {
      status = FAILURE_ALREADY_EXISTS;
      message = "Error: user " + request->username() + " already logged in...";
    } else {
      client->connected = true;
      client_db.push_back(client);
      message = "User " + request->username() + " successfully logged in!";
    }

    reply->set_status(status);
    reply->set_msg(message);
    
    return Status::OK;
  }

  std::string formatMessageOutput(Timestamp t, std::string u, std::string m) {
    std::string output;
    const time_t secs = t.seconds();
    struct tm *to = localtime(&secs);
    std::string timeStr;

    std::string yr = std::to_string(to->tm_year + 1900); // tm_year is years since 1900
    std::string mon = std::to_string(to->tm_mon + 1); // starts at 0
    std::string day = std::to_string(to->tm_mday);
    std::string hr = std::to_string(to->tm_hour);
    std::string min = std::to_string(to->tm_min);
    std::string sec = std::to_string(to->tm_sec);

    timeStr = yr + "-" + mon + "-" + day + " " + hr + ":" + min + ":" + sec;

    output += "T " + timeStr;
    output += "\nU " + u;
    output += "\nW " + m + "\n";
    return output;
  }

  std::vector<Post*> getFollowingPosts(Client* c, int maxPosts) {

    std::vector<ClientData*> following = c->client_following;
    std::set<ClientData*> followingSet(following.begin(), following.end());

    std::vector<Post*> timelinePosts;
    int i = (int)posts.size() - 1;
    while (maxPosts > 0 && i >= 0) {
      Post* p = posts[i];
      // If post p is written by someone client c follows
      if(followingSet.find(p->c) != followingSet.end()
        && p->m->timestamp() >= ) {
        // Add p to timelinePosts
        timelinePosts.push_back(p);
        maxPosts--;
      }
      i--;
    }

    return timelinePosts;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {

    /*********
    YOUR CODE HERE
    **********/
    
    // https://grpc.io/docs/languages/cpp/basics/
    Message message;

    while (stream->Read(&message)) {
      
      Timestamp t = message.timestamp();
      std::string u = message.username();
      std::string m = message.msg();

      // If you do this inside the while loop,
      // the current client doesn't start receiving 
      // messages until after it writes its first message
      Client* client = getClient(u);
      if(client->stream == 0) {
        
        // Upon joining the timeline, user sends an initial empty message
        // to indicate that this stream belongs to it. Don't write this
        // message to the timeline, and simply set the client's stream.
        client->stream = stream;
        
        // Write last min(numposts, 20) posts to stream on first arrival

        // In production, we'd use a heap/PQ which keeps messages ordered by timestamp
        // and mutex for accessing the posts vector.
        std::vector<Post*> timelinePosts = getFollowingPosts(client, 20);

        std::ofstream myFile;
        myFile.open(DIR + u + ".txt", std::ios_base::app);

        // According to testcases, these should be printed in reverse chronological order
        for(Post* p : timelinePosts) {
          stream->Write(p->m);
          myFile << formatMessageOutput(p->m.timestamp(), p->m.username(), p->m.msg());
        }

        myFile.close();

      } else {
        // Don't write to your own timeline according to test cases
        // stream->Write(message);
        // std::string output = formatMessageOutput(t, u, m);

        // Store message in list
        Post* p = new Post;
        p->m = message;
        p->c = client;
        posts.push_back(p);

        // Write message to file

        // not reading from file in this proj
        for(Client* follower : client->client_followers) {
          
          // Only write to stream if follower has joined the timeline
          // Fulfills the requirement of only showing users posts created
          // after they join the timeline.
          if(!follower->stream == 0) {
            std::ofstream file;
            file.open(DIR + follower->username + ".txt", std::ios_base::app);
            follower->stream->Write(message);
            file << formatMessageOutput(message.timestamp(), message.username(), message.msg());
            file.close();
          }
        }
      }
    }

    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  server->Wait();
}

int main(int argc, char** argv) {

  std::string port = "3010";
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  // Makes directory if it doesn't exist,
  // fails quietly if it does
  int err = mkdir(DIR.c_str(), 0777);

  std::string log_file_name = std::string("server-") + port;
  google::InitGoogleLogging(log_file_name.c_str());
  log(INFO, "Logging Initialized. Server starting...");
  RunServer(port);

  return 0;
}
