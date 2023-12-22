#include <ctime>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>

#include <grpc++/grpc++.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/util/time_util.h>

#include "SNSCoordinator.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;

using namespace std;

std::string SNSCoordinator::POSTS = "/posts.txt";
std::string SNSCoordinator::USER_INFO = "/userinfo.txt";
std::string SNSCoordinator::CLUSTER = "/cluster";
std::string SNSCoordinator::MASTER = "/master";
std::string SNSCoordinator::SLAVE = "/slave";

// ---- UTILITIY FUNCTIONS ----

time_t getCurrentTime() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
	return t_c;
}

// ---- COORDINATOR ----

SNSCoordinator::SNSCoordinator(int numClusters) {

	for(int i = 0; i < numClusters; i++) {
		map<int, shared_ptr<zNode>> cluster;
		clusters.push_back(cluster);
	}

	thread(&SNSCoordinator::checkHeartbeats, this).detach();
}

// ---- COORDINATOR HELPERS ----

string SNSCoordinator::getMasterFilepath(int clusterId)  {
	return CLUSTER + str(clusterId) + MASTER;
}

string SNSCoordinator::getSlaveFilepath(int clusterId, int serverId) {
	return CLUSTER + str(clusterId) + SLAVE + str(serverId);
}

int SNSCoordinator::getClusterMasterKey(int clusterIdx) {
	std::map<int, shared_ptr<zNode>> &cluster = clusters[clusterIdx];

	for(auto& serverPair : cluster) {
		if(serverPair.second->master) {
			return serverPair.first;
		}
	}

	return -1;
}

// ---- COORDINATOR API ----

Status SNSCoordinator::Heartbeat(ServerContext* context, const ServerInfo* serverInfo, Path* path) 
{
	int clusterId = serverInfo->clusterid();
	int clusterIdx = idToIndex(clusterId);
	int serverId = serverInfo->serverid();

	string rawPath = "";

	if(clusterIdx < 0 || clusterIdx >= clusters.size()) {
		// TODO: LOG ERROR, somehow inform server of num clusters?

		string message = 
		"Invalid cluster ID specificied. " 
		"Specify an ID between 1 and " + str(clusters.size());

		path->set_path(rawPath);
		return Status(StatusCode::INVALID_ARGUMENT, message);
	}
	
	map<int, shared_ptr<zNode>> &cluster = clusters[clusterIdx];
	map<int, shared_ptr<zNode>>::iterator it = cluster.find(serverId);

	// Gets specified server, or creates new one if doesn't exist

	shared_ptr<zNode> server;
	if(it != cluster.end()) {

		server = it->second;

		// Existing server with given cluster and server id was found
		if(!serverInfo->registered() && server->isActive()) {
			// There is an existing, active server with the given cluster and server id
			// Reject the heartbeat

			string message = "Server registration failed. There is already"
			 				"an active server with the specified cluster and server id.";

			log(ERROR, message);
			return Status(StatusCode::ALREADY_EXISTS, message);
		}
		// TODO: authentication?
	} else {
		server = make_shared<zNode>();
		cluster[serverId] = server;
	}

	// Re-set zNode from serverInfo in case host and port have changed
	// This updates the heartbeat.
	server->setFromServerInfo(serverInfo);

	if(server->master) {
		// Only log missed heartbeats?
		// log(INFO, "Heartbeat received from " + server->to_string());
		
		path->set_path(server->path);
		path->set_master(true);
		return Status::OK;
	}

	/*
		LEADER ELECTION:
		Based on zookeeper leader election pseudo-code
	*/

	// Get address to store in acquired file
	string address = server->getAddress();
	string syncAddress = "";

	// Try to acquire master file lock
	bool master = false;
	string masterFilepath = getMasterFilepath(clusterId);
	
	if(filesys.exists(masterFilepath)) {
		// Master exists
		rawPath = getSlaveFilepath(clusterId, serverId);

		// slave file lock doesn't really matter in current scenario
		// Assuming success for now
		filesys.write(rawPath, address, false, true);

		// if you are a slave, your sync address is the cluster master's address
		filesys.read(masterFilepath, syncAddress);

	} else {

		int success = filesys.create(masterFilepath, false, true);
		if(success < 0) {
			// someone else became the leader in the meantime
			filesys.read(masterFilepath, syncAddress);

		} else {
			// Master file lock acquired
			master = true;
			rawPath = masterFilepath;

			// Write server address to file
			filesys.write(masterFilepath, address, false, true);

			// If you are a cluster master and you are unregistered,
			// your sync address is the address of any other active cluster master
			if(!serverInfo->registered()) {
				shared_ptr<zNode> otherClusterMaster = getFirstAvailableClusterMaster(clusterIdx);
				if(otherClusterMaster != NULL) {
					syncAddress = otherClusterMaster->getAddress();
				}
			}
		}

	}

	// non-empty masterAddress indicates that the server is a slave
	server->path = rawPath;
	if(master) {
		server->master = true;
	}
	path->set_path(rawPath);
	path->set_master(master);
	path->set_sync_address(syncAddress);

	// In current scenario, sync address only matters to server on initialization
	// It may be important for a server to know its sync address after init. as well.
	
	// Only log first and missed heartbeats
	if(!serverInfo->registered()) {

		string message = "First heartbeat received from " + server->to_string() + ". ";
		if(syncAddress == "") {
			message += "No other cluster master available to sync with.";
		} else {
			message += "Syncing with ";
			if(master) {
				message += "other cluster master @ ";
			} else {
				message += "its own cluster master @ ";
			}
		}
		message += syncAddress;

		log(INFO, message);
	} else if(master) {
		log(INFO, "New master elected: " + server->to_string());
	}

	return Status::OK;
}

Status SNSCoordinator::GetCounterparts(ServerContext *context, const ServerInfo *masterInfo, ServerList *slaveList)
{

	map<int, shared_ptr<zNode>> &cluster = clusters[idToIndex(masterInfo->clusterid())];

	for(auto &serverPair : cluster) {
		int key = serverPair.first;
		shared_ptr<zNode> server = serverPair.second;

		// Don't include self or inactive servers
		if((int)masterInfo->serverid() != key && server->isActive()) {
			ServerInfo* s = slaveList->add_servers();
			server->setServerInfo(*s);
		}
	}

	// TODO: log who they were requested by
	log(INFO, "Counterparts requested by " + serverInfoToString(masterInfo));

	return Status::OK;
}

/* Methods for getting other cluster masters */

// Returns pointer to first available cluster master (skips cluster
// with specified index). If no others are available, returns NULL.
shared_ptr<zNode> SNSCoordinator::getFirstAvailableClusterMaster(int clusterIdx) {
	
	shared_ptr<zNode> clusterMaster = NULL;
	bool found = false;

	for(int i = 0; i < (int)clusters.size(); i++) {

		// skip cluster that requesting master belongs to
		if(i != clusterIdx) {

			map<int, shared_ptr<zNode>> &cluster = clusters[i];

			for(auto &serverPair : cluster) {
				shared_ptr<zNode> server = serverPair.second;

				// Can only be master if active
				if(server->master) {
					clusterMaster = server;
					found = true;
					break; // don't need to continue searching for master
				}
			}

			if(found) {
				break;
			}
		}
	}

	return clusterMaster;
}

Status SNSCoordinator::GetOtherClusterMasters(ServerContext *context, const ServerInfo *masterInfo, ServerList *list)
{

	int clusterIdx = idToIndex(masterInfo->clusterid());
	for(int i = 0; i < (int)clusters.size(); i++) {

		// skip cluster that requesting master belongs to
		if(i != clusterIdx) {

			map<int, shared_ptr<zNode>> &cluster = clusters[i];

			for(auto &serverPair : cluster) {
				shared_ptr<zNode> server = serverPair.second;

				// Only include masters. Can only be master if active
				if(server->master) {
					ServerInfo* s = list->add_servers();
					server->setServerInfo(*s);

					break; // don't need to continue searching for master
				}
			}
		}
	}

	// TODO: log who they were requested by
	log(INFO, "Other cluster masters requested by " + serverInfoToString(masterInfo));

	return Status::OK;
}


// ---- CLIENT API ----

Status SNSCoordinator::GetUniqueClientID(ServerContext* context, const ClientRequest* clientRequest, ID* id)
{
	int rawId = clientAssignments.size();
	id->set_id(rawId);

	// Insert key
	clientAssignments[rawId] = NULL;

	log(INFO, "New unique client ID requested. Provided ID: " + str(rawId));

	return Status::OK;
}

// Get Server Helper
shared_ptr<zNode> SNSCoordinator::getActiveMaster(int clientId) {

	/*
		First looks for an active master in clusters[clientId % numClusters]
		If no active masters found, search in remaining clusters
	*/ 
	
	int numClusters = clusters.size();
	int clusterIdx = (clientId % numClusters);
	int remainingClusters = numClusters;

	map<int, shared_ptr<zNode>>* cluster = &clusters[clusterIdx];

	shared_ptr<zNode> server = NULL;
	bool found = false;
	while(remainingClusters > 0) {
		
	  	// Get master

		if(cluster->size() > 0) {
			for(auto& serverPair : *cluster) {
				server = serverPair.second;
				// If server is master, it has to be active
				if(server != NULL && server->master) {
					found = true;
					break;
				}
			}

			if(found) {
				break;
			}
		}

		clusterIdx = (clusterIdx + 1) % numClusters;
		cluster = &clusters[clusterIdx];
		remainingClusters--;
	}

	if(found) {
		return server;
	}

	return NULL;
} 

Status SNSCoordinator::GetServer(ServerContext* context, const ID* id, ServerInfo* serverInfo)
{
	// Check if the client has already been assigned a server
	// Every assignment starts as NULL

	int rawId = id->id();

	shared_ptr<zNode> server = clientAssignments[rawId];
	bool changed = false;

	if(server == NULL || !server->master) {

		// Server not assigned yet
		// or assigned server is no longer master
		server = getActiveMaster(rawId);
		if(server == NULL) {
			string message = 
			"Server requested by client with ID " + str(rawId) + ", "
			"but no servers available to serve the request.";

			log(ERROR, message);
			return Status(StatusCode::UNAVAILABLE, "No servers available to serve the request.");
		}

		clientAssignments[rawId] = server;
		changed = true;

		// Only log if server changes
		log(INFO, "Client with id " + str(rawId) + " assigned to " + server->to_string());
	}

	server->setServerInfo(*serverInfo);
	serverInfo->set_changed(changed);

	return Status::OK;
}

// Run in a separate thread
void SNSCoordinator::checkHeartbeats() {
	while(true) {

		for(auto& cluster : clusters) {

			for(auto& serverPair : cluster) {
				shared_ptr<zNode> server = serverPair.second;

				if(difftime(getCurrentTime(), server->last_heartbeat) > maxHeartbeatDelay) {
					server->missed_heartbeats += 1;

					if(server->missed_heartbeats == 1) {
						log(WARNING, "Heartbeat missed by " + server->to_string());
					}
					
					if(server->missed_heartbeats == 2) {
						server->master = false;
						filesys.remove(server->path);

						string message = "2 Heartbeats missed by " + server->to_string() + ". Releasing file lock."

						log(WARNING, message);
					}
					
					// Do not log >= 2 missed heartbeats
				}
			}
		}

		sleep(heartbeatCheckDelay);
	}
}
