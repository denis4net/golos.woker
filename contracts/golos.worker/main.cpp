#include <eosiolib/eosio.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/currency.hpp>
#include <array>
#include <string>
#include <vector>
#include <list>
#include <ctime>
#include <algorithm>

#include "token.hpp"
#include "app_dispatcher.hpp"

using namespace eosio;

#define ID_NOT_DEFINED 0

namespace golos
{
using std::string;

template <typename T>
bool contains(vector<T> c, const T& v)
{
  return std::find(c.begin(), c.end(), v) != c.end();
}

class workers : public contract
{
public:
  typedef symbol_name app_domain_t;

  typedef uint64_t comment_id_t;
  struct comment_t
  {
    comment_id_t id;
    comment_id_t parent;
    string content;
    account_name author;
    time_t created;
    time_t modified;
  };
  EOSLIB_SERIALIZE(comment_t, (id)(parent)(content)(author)(created)(modified));

  typedef uint64_t tspec_id_t;
  struct tech_spec_t
  {
    tspec_id_t id;
    account_name author;
    string content;
    time_t created;
    time_t modified;
    vector<comment_t> upvotes;
    vector<comment_t> downvotes;
  };
  EOSLIB_SERIALIZE(tech_spec_t, (id)(author)(content)(created)(modified)(upvotes)(downvotes));

  typedef uint64_t popos_id_t;
  struct proposal_t
  {
    popos_id_t id;
    account_name author;
    string title;
    string description;
    vector<account_name> upvotes;
    vector<account_name> downvotes;
    vector<comment_t> comments;
    time_t created;
    time_t modified;
    uint64_t primary_key() const { return id; }
  };

  //@abi table proposals i64
  EOSLIB_SERIALIZE(proposal_t, (id)(author)(title)(description)(upvotes)(downvotes)(comments)(created)(modified));
  typedef multi_index<N(proposals), proposal_t> proposals_t;
  proposals_t _proposals;

  struct state_t
  {
    asset balance;
    asset total_locked;
    popos_id_t proposals_counter;
    comment_id_t comments_counter;
    tspec_id_t task_counter;

    uint64_t primary_key() const { return 0; }
  };

  EOSLIB_SERIALIZE(state_t, (balance)(total_locked)(proposals_counter));
  typedef multi_index<N(states), state_t> states_t;
  states_t _states;

  app_domain_t _app;

protected:
  auto get_state()
  {
    return _states.find(0);
  }

  symbol_name get_token_symbol()
  {
    return get_state()->balance.symbol;
  }

  const asset& get_balance()
  {
    accounts acc(N(golos.token), _self);
    auto balanceIt = acc.find(get_token_symbol());
    eosio_assert(balanceIt != acc.end(), "token symbol does not exists");
    return balanceIt->balance;
  }

  const asset get_locked()
  {
    accounts acc(N(golos.token), _self);
    auto balanceIt = acc.find(get_token_symbol());
    eosio_assert(balanceIt != acc.end(), "token symbol does not exists");
    auto locked = get_state()->total_locked;
    return balanceIt->balance - locked;
  }

public:
  workers(account_name owner, app_domain_t app) : contract(owner), _app(app), _states(_self, app), _proposals(_self, app)
  {
  }

  /// @abi action
  void create(symbol_name token_symbol)
  {
    if (_states.find(0) != _states.end())
    {
      _states.emplace(_app, [&](auto &o) {
        o.balance = asset(0, token_symbol);
        o.total_locked = asset(0, token_symbol);
        o.proposals_counter = 1;
        o.comments_counter = 1;
        o.task_counter = 1;
      });
    }
  }

  /// @abi action
  void addpropos(account_name author, string title, string description)
  {
    auto state_itr = get_state();
    popos_id_t id = state_itr->proposals_counter;
    require_auth(author);

    _proposals.emplace(author, [&](auto &o) {
      o.id = id;
      o.author = author;
      o.title = title;
      o.description = description;
      o.created = now();
      o.modified = 0;
    });

    _states.modify(state_itr, author, [](auto &o) {
      o.proposals_counter += 1;
    });
  }

  // @abi action
  void editpropos(popos_id_t proposal_id, string title, string description)
  {
    auto state_itr = get_state();
    auto proposal_itr = _proposals.find(proposal_id);
    eosio_assert(proposal_itr != _proposals.end(), "proposal has not been found");
    require_auth(proposal_itr->author);

    _proposals.modify(proposal_itr, proposal_itr->author, [&](auto &o) {
      bool modified = false;
      if (!description.empty())
      {
        o.description = description;
        modified = true;
      }
      if (!title.empty())
      {
        o.title = title;
        modified = true;
      }

      if (modified)
      {
        o.modified = now();
      }
    });
  }

  /// @abi action
  void delpropos(popos_id_t proposal_id)
  {
    // auto proposal_id = records.find(key);
    // eosio_assert(record_it != records.end(), "record has not been found");
    // print("remove owner=", record_it->owner, ", key=", key, ", self=", _self, ", ts=", record_it->ts);
    // require_auth(record_it->owner);
    // records.erase(record_it);
  }

  /// @abi action
  void upvote(popos_id_t proposal_id, account_name author)
  {
    auto proposal_itr = _proposals.find(proposal_id);
    eosio_assert(proposal_itr != _proposals.end(), "proposal has not been found");
    require_auth(author);

    eosio_assert(!contains(proposal_itr->downvotes, author), "author has already downvoted");
    _proposals.modify(proposal_itr, author, [&](auto &o) {
      o.upvotes.push_back(author);
    });
  }

  /// @abi action
  void downvote(popos_id_t proposal_id, account_name author)
  {
    auto proposal_itr = _proposals.find(proposal_id);
    eosio_assert(proposal_itr != _proposals.end(), "proposal has not been found");
    require_auth(author);

    eosio_assert(!contains(proposal_itr->upvotes, author), "author has already downvoted");

    _proposals.modify(proposal_itr, author, [&](auto &o) {
      o.downvotes.push_back(author);
    });
  }

  /// @abi action
  void addcomment(popos_id_t proposal_id, account_name author, string content)
  {
    auto state_itr = get_state();
    comment_id_t id = state_itr->comments_counter;
    auto proposal_itr = _proposals.find(proposal_id);
    eosio_assert(proposal_itr != _proposals.end(), "proposal has not been found");
    require_auth(author);

    _proposals.modify(proposal_itr, author, [&](auto &o) {
      comment_t comment;
      comment.id = id;
      comment.content = content;
      comment.author = author;
      comment.created = now();
      comment.modified = 0;

      o.comments.push_back(comment);
    });

    _states.modify(state_itr, author, [](auto &o) {
      o.comments_counter += 1;
    });
  }

  /// @abi action
  void editcomment(popos_id_t proposal_id, string content)
  {
  }

  /// @abi action
  void delcomment(popos_id_t proposal_id, comment_id_t comment_id)
  {
  }

  /// @abi action
  void addtspec(popos_id_t proposal_id, string content)
  {
  }

  /// @abi action
  void edittspec(popos_id_t proposal_id, string content)
  {
  }

  /// @abi action
  void deltspec(popos_id_t proposal_id, tspec_id_t task_id)
  {
  }

  /// @abi action
  void uvotetspec(popos_id_t proposal_id, tspec_id_t task_id, string comment)
  {
  }

  /// @abi action
  void dvotetspec(popos_id_t proposal_id, tspec_id_t task_id, string comment)
  {
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
} // namespace golos

APP_DOMAIN_ABI(golos::workers, (addpropos)(editpropos)(delpropos)(upvote)(downvote)\
(addcomment)(editcomment)(delcomment)(addtspec)(edittspec)(deltspec)(uvotetspec)(dvotetspec))