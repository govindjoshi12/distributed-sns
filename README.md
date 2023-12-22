### Design Document

#### Coordinator
The coordinator interacts with both the client and servers.
- Client Interactions
    - It offers a GetServer() method which assigns a server to the client based on its ID. Currently, the client ID is simply its username which is assumed to be a number.
    - There is also a GetUniqueClientID() method which generates unique client ids indepent of the client's usernmame. Unused for the purposes of this assignment.
- Server Interactions
    - The coordinator offers a RegisterServer() method, which allows servers to register themselves into a particular cluster. For this assignment, we assume that each cluster will only have one server. 
    - A Heartbeat() method allows a server to communicate that it is still alive. A background thread continously checks the status of each server at a certain delay. 
    - The coordinator offers a zookeeper like API with create, exists, append, and read methods. The filesystem is map of strings stored in memory so it is only persistent while the coordinator is running.

#### Server
- Upon startup, the server registers itself with the coordinator in the given cluster with the given ID. It creates a file of the form "server_clusterID_serverID" in the coordinator using the provided coordinator zookeeper-like API.
- It initializes itself with $serverpath/userinfo.txt if it exists. Posts are not initialized at the moment. 
- The userinfo.txt file follows the following format:
    - user a
    - user b
    - follow a b timestamp
    - unfollow a b  
- A background thread continously sends heartbeats to the coordinator at a certain delay, letting it know that the server is still active. 

#### Client
- The client will first contact the coordinator and obtain
a hostname and port number of the server to connect to. Everthing
else remains the same. Done through the getServerInfo() method 
in tsd.cc which is called in connectTo() before connecting to server.


#### Notes
- If you a gRPC Message is used as a private member of a class, and as a return value
of a gRPC, its value is NOT PRESERVED. The server MUST reset the message to the correct
value, even if it technically does not need to be updated.