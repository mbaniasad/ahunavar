#include "bob.h"
#include "alice_client.h"
#include "network.h"
#include "scc_wrapper_interface.h"
#include "timer.h"
#include <iostream>

class Voter_client: SCC_Wrapper_Interface {

private:
  Bob bob;

  // Network
  zmq::socket_t& socket;
  zmq::socket_t& candidate_socket;

  std::vector<Bin*> requests; // Sent
  std::vector<Bin*> reply;    // Received

  std::vector<Bin*> C;
  std::vector<Bin*> Z;
  std::vector<Bin*> quotients;
  std::vector<Bin*> fake_epsilons;


  Bin* redeem_script;
  Bin* funding_tx_id;
  Bin* pub_key;

public:

  Voter_client(zmq::socket_t& s,zmq::socket_t& sc): socket(s), candidate_socket(sc){

    socket.connect(TUMBLER_SERVER_SOCKET);
    candidate_socket.connect(CANDIDATE_SERVER_SOCKET);

  }

  ~Voter_client(){

    delete_bin(redeem_script);
    delete_bin(funding_tx_id);
    delete_bin(pub_key);

    free_Bins(C);
    free_Bins(Z);
    free_Bins(fake_epsilons);
    free_Bins(quotients);

  };

  bool start(std::string candidate_name){
    std::cout<<"voting for ->>"<< candidate_name<<"\n";
    Timer timer = Timer((char *) "wrapper_protocol\0");
    timer.start();

    //=====================================
    // SCC Wrapper Protocol
    //=====================================
    if (!exchange()){
      printf("Failed in exchange\n");
      return false;
    }

    if (!commitment()){
      printf("Failed in commitment\n");
      return false;
    }

    if (!verify()){
      printf("Failed in verify\n");
      return false;
    }
    timer.end();

    if(!post()){
      printf("Failed in post\n");
      return false;
    }

    return true;
  }

  std::vector<std::string> get_candidate_list(){
    std::vector<std::string> v;
    v.push_back("abc");
    v.push_back("xyz");
    v.push_back("alice");
    v.push_back("#TODO GET FROM TUMBLER");
    return v;
  }
protected:

  bool exchange(){

    Bin* temp = new Bin(8);
    memcpy(temp->data, "exchange", 8);

    Bin* bob_pubkey = bob.get_pubkey();

    requests.push_back(temp);
    requests.push_back(bob_pubkey);
    send(socket, requests);

    delete temp;
    requests.clear();

    receive(socket, reply);

    if(reply.size() != 4){
      free_Bins(reply);
      reply.clear();
      return false;
    }

    pub_key = reply.at(0);
    redeem_script = reply.at(2);
    funding_tx_id = reply.at(3);

    bob.set_party_pubkey(pub_key);
    bob.set_rsa(reply.at(1));
    bob.set_redeem_script(redeem_script);
    bob.set_funding_tx_id(funding_tx_id);


    delete reply.at(1);

    reply.clear();

    return true;
  }

  bool commitment(){

    Bin* temp = new Bin(10);
    memcpy(temp->data, "commitment", 10);

    Bin *tx_set = new Bin();
    if (!serialize_vector(tx_set, *bob.get_tx_set(), (2 * K), HASH_256)){
      return false;
    }

    requests.push_back(temp);
    requests.push_back(tx_set);
    requests.push_back(bob.get_h_r());
    requests.push_back(bob.get_h_f());
    send(socket, requests);

    delete temp;
    delete tx_set;
    requests.clear();


    receive(socket, reply);
    if(reply.size() != 2){
      free_Bins(reply);
      reply.clear();
      return false;
    }


    if (!deserialize_vector(reply.at(0), C, 2 * K, HASH_512)){
      return false;
    }

    if (!deserialize_vector(reply.at(1), Z, 2 * K, bob.rsa_len)){
      return false;
    }

    free_Bins(reply);
    reply.clear();

    return true;
  }

  bool verify(){

    Bin *R = new Bin();
    Bin *F = new Bin();
    Bin *fake_tx = new Bin();

    std::vector<int>r = bob.get_R();
    std::vector<int>f = bob.get_F();

    Bin* temp = new Bin(6);
    memcpy(temp->data, "verify", 6);

    if (!serialize_int_vector(R, r, K)){
      delete R;
      return false;
    }

    if (!serialize_int_vector(F, f, K)){
      delete F;
      return false;
    }


    if (!serialize_vector(fake_tx, *bob.get_fake_tx(),  K, HASH_256)){
      return false;
    }

    requests.push_back(temp);
    requests.push_back(R);
    requests.push_back(F);
    requests.push_back(fake_tx);
    requests.push_back(bob.get_salt());
    send(socket, requests);

    // requests.pop_back();
    // free_Bins(requests);
    requests.clear();

    receive(socket, reply);
    if(reply.size() != 2){
      free_Bins(reply);
      reply.clear();
      return false;
    }


    if (!deserialize_vector(reply.at(0), quotients, K - 1,  bob.rsa_len)){
      return false;
    }

    if (!deserialize_vector(reply.at(1), fake_epsilons, K, bob.rsa_len)){
      return false;
    }

    if (!bob.verify_recieved_data(Z, C, fake_epsilons, quotients)){
      return false;
    }

    free_Bins(reply);
    reply.clear();

    return true;
  }

  bool post(){

    Bin* W = bob.get_W();

    Bin* epsiolon = new Bin();
    requests.clear();
    std::cout<< "Asking candidate for a ballot";
    std::cout<< "#TODO ask all candidates for ballot";
    Bin* temp = new Bin(10);
    memcpy(temp->data, "decryption", 10);
    requests.push_back(temp);
    requests.push_back(W);
    send(candidate_socket, requests);
    requests.clear();
    free_Bins(requests);

    receive(candidate_socket, reply);
    // if (!get_decryption(W, epsiolon)){
    //   return false;
    // }


    bob.set_recovered_epsilon(epsiolon);
    if(!bob.post_tx()){
      return false;
    }

    Bin* tx = bob.get_tx_fulfill();
    printf("\n\nTX fulfill is:\n");
    tx->print();
    printf("\n\n");

    // Cleanup
    delete epsiolon;

    return true;
  }

};

int main () {

  zmq::context_t context(1);
  zmq::socket_t  socket(context, ZMQ_REQ);
  zmq::socket_t  candidate_socket(context, ZMQ_REQ);


  Voter_client voter_client = Voter_client(socket, candidate_socket);
  // get the list of candidates
  std::vector<std::string> candidates_list = voter_client.get_candidate_list();
  std::cout<<"candidates are:"<<"\n";
  for(int i = 0; i < candidates_list.size(); i++)
     std::cout<<candidates_list[i]<<"\n";
  std::cout<<"enter the name of candidate:"<<"\n";   
  std::string candidate_name;
  candidate_name = "alice";// std::cin>>candidate_name;
  
  voter_client.start(candidate_name);

  return 0;
}
