
Compile the code using the provided makefile:

    make

To clear the directory (and remove .txt files):
   
    make clean

To run the server without glog messages (port number is optional): 

    ./tsd <-p port>
    
To run the server with glog messages: 

    GLOG_logtostderr=1 ./tsd <-p port>


To run the client without glog messages (port number and host address are optional): 

    ./tsc <-h host_addr -p port> -u user1
    
To run the server with glog messages: 

    GLOG_logtostderr=1 ./tsc <-h host_addr -p port> -u user1
