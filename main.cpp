#include "token.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/currency.hpp>
#include <array>
#include <string>
#include <vector>
#include <list>
#include <ctime>

using namespace eosio;
using std::string;
using std::vector;
using std::list;

#define ID_NOT_DEFINED 0

namespace golos {
  class workers: public contract {
    public:
      typedef uint64_t comment_id_t;   
      struct comment_t {
        comment_id_t id;
        comment_id_t parent;
        string content;
        account_name author;
        time_t created;
        time_t modified;
      };
      EOSLIB_SERIALIZE(comment_t, (id)(parent)(content)(author)(created)(modified));

      typedef uint64_t tech_task_id_t;
      struct tech_task_t {
        tech_task_id_t id;
        account_name author;
        string content;
        time_t created;
        time_t modified;
        list<comment_t> upvotes;
        list<comment_t> downvotes;
      };
      EOSLIB_SERIALIZE(tech_task_t, (id)(author)(content)(created)(modified)(upvotes)(downvotes));

      typedef uint64_t proposal_id_t;
      struct proposal_t {
        proposal_id_t id;
        account_name owner;
        string title;
        string description;
        list<account_name> upvotes;
        list<account_name> downvotes;
        list<comment_t> comments;
        time_t created;
        time_t modified;
        uint64_t primary_key() const { return id; }
      };

      //@abi table records i64
      EOSLIB_SERIALIZE(proposal_t, (id)(owner)(title)(description)(upvotes)(downvotes)(comments)(created)(modified));

      typedef multi_index<N(proposals), proposal_t> proposals_t;
    public:
        workers(account_name owner): 
          contract(owner) {}

        /// @abi action
        void add_proposal(account_name owner, string content) {
          // auto key = _proposals::primary_key(document_hash);
          // // require_auth(owner);
          // records.emplace(owner, [&] (auto& r) {
          //   r.owner = owner;
          //   r.ts = publication_time();
          //   r.id = document_hash;
          //   print("put owner=", owner, ", key=", key, ", self=", _self);
          // });
        }

        void edit_proposal(proposal_id_t proposal_id, string content) {

        }

        /// @abi action
        void del_proposal(proposal_id_t proposal_id) {
          // auto proposal_id = records.find(key);
          // eosio_assert(record_it != records.end(), "record has not been found");
          // print("remove owner=", record_it->owner, ", key=", key, ", self=", _self, ", ts=", record_it->ts);
          // require_auth(record_it->owner);
          // records.erase(record_it);
        }

        /// @abi action
        void upvote_proposal(proposal_id_t proposal_id) {

        }

        /// @abi action 
        void downvote_proposal(proposal_id_t proposal_id) {

        }

        /// @abi action
        void add_comment(proposal_id_t proposal_id, string content) {

        }

        /// @abi action
        void edit_comment(proposal_id_t proposal_id, string content) {

        }

        /// @abi action
        void del_comment(proposal_id_t proposal_id, comment_id_t comment_id) {

        }

        /// @abi action
        void add_tech_task(proposal_id_t proposal_id, string content) {

        }

        /// @abi action
        void edit_tech_task(proposal_id_t proposal_id, string content) {

        }

        /// @abi action
        void del_tech_task(proposal_id_t proposal_id, tech_task_id_t task_id) {

        }

        /// @abi action
        void upvote_tech_task(proposal_id_t proposal_id, tech_task_id_t task_id, string comment) {

        }

        /// @abi action
        void downvote_tech_task(proposal_id_t proposal_id, tech_task_id_t task_id, string comment) {

        }

        asset get_balance(scope_name app_name) {
          symbol_name symbol = app_name;
          accounts acc(N(golos.token), _self);
          auto balanceIt = acc.find(symbol);
          eosio_assert(balanceIt != acc.end(), "token symbol does not exists");
          return balanceIt->balance;
        }

        asset get_locked() {

        }

        // /// @abi action
        // // https://tbfleming.github.io/cib/eos.html#gist=d230f3ab2998e8858d3e51af7e4d9aeb
        // void transfer(account_name self, uint64_t code /* code == eosio.token */) {
        //   auto data = unpack_action_data<currency::transfer>();
        //   if(data.from == self || data.to != self)
        //       return;

        //   eosio_assert(data.quantity.is_valid(), "Quntity is invalid");
        //   eosio_assert(data.quantity.amount > 0, "Invalid transfer amount");
        // }
  };
}

EOSIO_ABI(golos::workers, (add_proposal)(edit_proposal)(del_proposal) \
  (upvote_proposal)(downvote_proposal) \
  (add_comment)(edit_comment)(del_comment) \
  (add_tech_task)(edit_tech_task)(del_tech_task) \
  (upvote_tech_task)(downvote_tech_task) )