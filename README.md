### Distributed Social Network Service
<hr>
A highy available, fault tolerant, and scalable distributed social network service with a central coordinator written with C++ and gRPC.

![Overview Diagram](./img/diagram.jpg "Overview of Service")

#### Overview
The system consists of three parts.

<hr>

##### Coordinator
TODO

<hr>

##### Server
TODO

<hr>

##### Client
TODO

<hr>

#### Running the System
This project uses cmake to build the executables. In order to build the system, you must ensure that you have the gRPC and glog libraries installed on your machines. 

To build the project, run the following commands
```
mkdir build
cd build
cmake ..
make
```

The executables will be built and placed in `build/bin`. There are three running scripts provided in the top level directory: `client.sh`, `coord.sh`, and `server.sh`. These simply run the executables placed in `build/bin` and set the log output to stderr instead of a `tmp` directory. You can ass the arguments you'd pass to the raw executables to these scripts.

##### Run Coordinator
```
./coord.sh -n <num clusters> -h <ip> -p <port number>
```

By default, the coordinator runs on `localhost:9000` with3 clusters.

##### Run Server
```
./server.sh -c <cluster id> -s <server id> -h <coord ip> -k <coord port> -i <server ip> -p <server port>
```

The cluster id must be [1, numClusters]
Default values:
```
c: 1
s: 1
h: localhost
k: 9000
i: localhost
p: 10000  
```

##### Run Client
```
./client.sh -h <coord ip> -k <coord port> -u <username>
```
By default, connects to coordinator at `localhost:9000`