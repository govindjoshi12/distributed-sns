#include <iostream>
#include <string>
#include <ctime>
#include <vector>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"
using csce438::IStatus;

#define MAX_DATA 256


/*
 * IReply structure is designed to be used for displaying the
 * result of the command that has been sent to the server.
 * For example, in the "processCommand" function, you should
 * declare a variable of IReply structure and set based on
 * the type of command and the result.
 *
 * - FOLLOW/UNFOLLOW/TIMELINE command:
 * IReply ireply;
 * ireply.grpc_status = return value of a service method
 * ireply.comm_status = one of values in IStatus enum
 *
 * - LIST command:
 * IReply ireply;
 * ireply.grpc_status = return value of a service method
 * ireply.comm_status = one of values in IStatus enum
 * reply.users = list of all users who connected to the server at least onece
 * reply.followers = list of users who following current user;
 *
 * This structure is not for communicating between server and client.
 * You need to design your own rules for the communication.
 */
struct IReply
{
    grpc::Status grpc_status;
    enum IStatus comm_status;
    std::vector<std::string> all_users;
    std::vector<std::string> followers;
    std::vector<std::string> following;
};


std::string getPostMessage();
void displayPostMessage(const std::string& sender, const std::string& message, std::time_t& time);
  
class IClient
{
public:
  void run();
  
protected:
  /*
   * Pure virtual functions to be implemented by students
   */
  virtual int connectTo() = 0;
  virtual IReply processCommand(std::string& cmd) = 0;
  virtual void processTimeline() = 0;
  virtual std::string getMyUsername() const = 0;
  
private:
  void displayTitle() const;
  std::string getCommand() const;
  void displayCommandReply(const std::string& comm, const IReply& reply) const;
  void toUpperCase(std::string& str) const;
};


