// THis is an "include guard"
#ifndef COORD_HEADER
#define COORD_HEADER

#include <ctime>
#include <string>
#include <memory>

#include <glog/logging.h>
#define log(severity, msg); LOG(severity) << msg << "\n---"; google::FlushLogFiles(google::severity);
#define str(num) std::to_string(num)

#include "FSWrapper/FSMemory.h"
#include <snsproto/coordinator.grpc.pb.h>

using grpc::ServerContext;
using grpc::StatusCode;
using grpc::Status;

using SNS::CoordService;
using SNS::ServerInfo;
using SNS::Confirmation;
using SNS::ID;
using SNS::ClientRequest;
using SNS::Path;
using SNS::FileData;
using SNS::FileInfo;
using SNS::ServerList;

// ---- UTILITY FUNCTION HEADERS ----
time_t getCurrentTime();

// ---- zNODE ----
// struct for storing sever/process information

struct zNode {
	int serverId = -1;
	int clusterId = -1;
	std::string hostname;
	std::string port;
	std::string type;
	std::string path;
	bool master = false;
	time_t last_heartbeat = 0;
	int missed_heartbeats = 0;

	bool isActive() {
		// Leeway of 2 missed heartbeats
		return missed_heartbeats <= 2;
	}

	void updateHeartbeat() {
		last_heartbeat = getCurrentTime();
		missed_heartbeats = 0;
	}

	void setFromServerInfo(const ServerInfo *s) {
		serverId = s->serverid();
		clusterId = s->clusterid();
		hostname = s->hostname();
		port = s->port();
		type = s->type();
		updateHeartbeat();
		// does not set path and master
	}

	// Set given serverInfo with self
	void setServerInfo(ServerInfo &s) {
		s.set_serverid(serverId);
		s.set_clusterid(clusterId);
		s.set_hostname(hostname);
		s.set_port(port);
		s.set_type(type);
	}

	std::string getAddress() {
		return hostname + ":" + port;
	}

	std::string to_string(bool verbose = true) {
		std::string str = 
		"Server " + std::to_string(serverId) + " in Cluster " + std::to_string(clusterId) + ""
		" @ " + hostname + ":" + port + " with file lock: " + path + ""; 

		return str;
	} 
};

class SNSCoordinator final : public CoordService::Service {

public:
	SNSCoordinator(int numClusters);
	virtual ~SNSCoordinator() {};

	// Server Methods
	Status Heartbeat(ServerContext* context, const ServerInfo* serverInfo, Path* path);
	Status GetCounterparts(ServerContext* context, const ServerInfo* masterInfo, ServerList* slaveList);
	Status GetOtherClusterMasters(ServerContext *context, const ServerInfo *masterInfo, ServerList *list);
	
	// Client Methods
	Status GetUniqueClientID(ServerContext* context, const ClientRequest* clientRequest, ID* id);
	Status GetServer(ServerContext* context, const ID* id, ServerInfo* serverInfo);

private:

	//potentially thread safe
	std::mutex v_mutex;
	FSMemory filesys{true};
	std::vector<std::map<int, std::shared_ptr<zNode>>> clusters;
	std::map<int, std::shared_ptr<zNode>> clientAssignments;

	int maxHeartbeatDelay = 10;
	int heartbeatCheckDelay = 3;

	void checkHeartbeats();

	// IDs are 1-based, indicies are 0-based
	int idToIndex(int id) { return id - 1; }
	int getClusterMasterKey(int clusterIdx);
	std::shared_ptr<zNode> getActiveMaster(int clientId);
	std::shared_ptr<zNode> getFirstAvailableClusterMaster(int clusterIdx);

	// STATIC VARS AND FUNCTIONS

	static std::string POSTS;
	static std::string USER_INFO;
	static std::string CLUSTER;
	static std::string MASTER;
	static std::string SLAVE;

	static std::string getMasterFilepath(int clusterId);
	static std::string getSlaveFilepath(int clusterIdx, int serverIdx);
	static std::string serverInfoToString(const ServerInfo* s) {
		std::string info = 
		"Server " + str(s->serverid()) + " in Cluster " + str(s->clusterid()) + "" 
		" @ " + s->hostname() + ":" + s->port() + "";
		return info;
	}

};

#endif