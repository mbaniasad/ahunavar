
#include "strings.h"
#include "network.h"
#include <iostream>
int main()
{

  // Prepare our context and socket
  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_REP);
  socket.bind("tcp://*:5507");
  printf("candidate server socet binded on tcp://*:5507\n");

  zmq::message_t fail_msg;
  fail_msg.rebuild(26);
  memcpy(fail_msg.data(), "Failed to process request.", 26);

  // Handle interrupt
  s_catch_signals();

  std::vector<Bin *> requests; // Received
  std::vector<Bin *> reply;    // Sent

  /*
  * Not a multithreaded server.
  * Will definitly not work correctly
  * if multiple clients try to connect
  * during a run of the Protocol
  * TODO: Turn into multithreaded socket server
  * or multithreaded http server after getting
  * 1000 tx's through.
  */
  while (true)
  {

    try
    {
      receive(socket, requests);
      std::cout<<(requests.at(0)->data)<<"\n";
      if(memcmp(requests.at(0)->data, "decryption", 10) == 0)
      {
        printf("getting decryption for a vote\n");
        std::cout<<"W:"<<requests.at(1)<<"\n";
      }
      // free_Bins(requests);
    }
    catch (zmq::error_t &e)
    {
      printf("\nZMQ Error: %s\n", e.what());
      // free_Bins(requests);
      break;
    }
  }
  return 0;
}
