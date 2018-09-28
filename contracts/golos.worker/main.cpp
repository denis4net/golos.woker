#include <eosiolib/eosio.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/crypto.h>

#include <string>
#include <vector>
#include <algorithm>

#include "token.hpp"
#include "app_dispatcher.hpp"

using namespace eosio;

#define ID_NOT_DEFINED 0
#define LOG(format, ...) print_f("%(%): " format "\n", __FUNCTION__, name{_app}.to_string().c_str(), ##__VA_ARGS__);

namespace golos
{
using std::string;

template <typename T>
bool contains(vector<T> c, const T &v)
{
  return std::find(c.begin(), c.end(), v) != c.end();
}

class worker : public contract
{
public:
  typedef symbol_name app_domain_t;

  typedef uint64_t comment_id_t;
  struct comment_t
  {
    comment_id_t id;
    comment_id_t parent;
    string text;
    account_name author;
    block_timestamp created;
    block_timestamp modified;
    EOSLIB_SERIALIZE(comment_t, (id)(parent)(text)(author)(created)(modified));
  };

  typedef uint64_t tspec_id_t;

  struct tspec_vote_t
  {
    account_name author;
    string comment;
    EOSLIB_SERIALIZE(tspec_vote_t, (author)(comment));
  };

  struct tspec_t
  {
    tspec_id_t id;
    account_name author;
    string text;
    vector<tspec_vote_t> upvotes;
    vector<tspec_vote_t> downvotes;
    block_timestamp created;
    block_timestamp modified;

    EOSLIB_SERIALIZE(tspec_t, (id)(author)(text)(upvotes)(downvotes)(created)(modified));
  };
  typedef uint64_t proposal_id_t;

  //@abi table proposals i64
  struct proposal_t
  {
    proposal_id_t id;
    account_name author;
    string title;
    string description;
    vector<account_name> upvotes;
    vector<account_name> downvotes;
    vector<comment_t> comments;
    vector<tspec_t> tspecs;
    block_timestamp created;
    block_timestamp modified;
    uint64_t primary_key() const { return id; }

    EOSLIB_SERIALIZE(proposal_t, (id)(author)(title)(description)(upvotes)(downvotes)(comments)(tspecs)(created)(modified));
  };

  multi_index<N(proposals), proposal_t> _proposals;

  //@abi table states i64
  struct state_t
  {
    asset balance;
    asset total_locked;

    uint64_t primary_key() const { return 0; }

    EOSLIB_SERIALIZE(state_t, (balance)(total_locked));
  };

  multi_index<N(states), state_t> _states;

  app_domain_t _app;

protected:
  auto get_state()
  {
    auto itr = _states.find(0);
    eosio_assert(itr != _states.end(), "worker's pool has not been created for the speicifed app domain");
    return itr;
  }

  symbol_name get_token_symbol()
  {
    return get_state()->balance.symbol;
  }

  const asset &get_balance()
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

  void require_app_member(account_name account)
  {
    require_auth(account);
    //TODO: eosio_assert(golos.vest::get_balance(account, _app).amount > 0, "app domain member authority is required to do this action");
  }

  void require_app_delegate(account_name account)
  {
    require_auth(account);
    //TODO: eosio_assert(golos.ctrl::is_witness(account, _app), "app domain delegate authority is required to do this action");
  }

public:
  worker(account_name owner, app_domain_t app) : contract(owner), _app(app), _states(_self, app), _proposals(_self, app)
  {
    //TODO: assert if app is not application domain
  }

  /// @abi action
  void createpool(symbol_name token_symbol)
  {
    LOG("creating worker's pool: code=\"%\" app=\"%\"", name{_self}.to_string().c_str(), name{_app}.to_string().c_str());
    eosio_assert(_states.find(0) == _states.end(), "workers pool is already initialized for the specified app domain");
    require_auth(_app);

    _states.emplace(_app, [&](auto &o) {
      // o.balance = asset(0, token_symbol);
      // o.total_locked = asset(0, token_symbol);
    });
  }

  /// @abi action
  void addpropos(proposal_id_t proposal_id, account_name author, string title, string description)
  {
    require_app_member(author);

    LOG("adding propos % \"%\" by %", proposal_id, title.c_str(), name{author}.to_string().c_str());

    _proposals.emplace(author, [&](auto &o) {
      o.id = proposal_id;
      o.author = author;
      o.title = title;
      o.description = description;
      o.created = block_timestamp(now());
      o.modified = block_timestamp(0);
    });
  }

  // @abi action
  void editpropos(proposal_id_t proposal_id, string title, string description)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    require_app_member(proposal->author);

    _proposals.modify(proposal, proposal->author, [&](auto &o) {
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
        o.modified = block_timestamp(now());
      }
    });
  }

  /// @abi action
  void delpropos(proposal_id_t proposal_id)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    eosio_assert(proposal->upvotes.size() == 0, "proposal has been approved by one member");
    require_app_member(proposal->author);
    _proposals.erase(proposal);
  }

  /// @abi action
  void upvote(proposal_id_t proposal_id, account_name author)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    require_app_delegate(author);
    // TODO: assert(author is delegate)
    eosio_assert(!contains(proposal->downvotes, author), "author has already downvoted");
    _proposals.modify(proposal, author, [&](auto &o) {
      o.upvotes.push_back(author);
    });
  }

  /// @abi action
  void downvote(proposal_id_t proposal_id, account_name author)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    require_app_delegate(author);
    // TODO: assert(author is delegate)
    eosio_assert(!contains(proposal->upvotes, author), "author has already downvoted");

    _proposals.modify(proposal, author, [&](auto &o) {
      o.downvotes.push_back(author);
    });
  }

  const auto getcomment(const proposal_t &proposal, comment_id_t comment_id)
  {
    return std::find_if(proposal.comments.begin(), proposal.comments.end(), [&](const auto &o) {
      return o.id == comment_id;
    });
  }

  auto getcomment(proposal_t &proposal, comment_id_t comment_id)
  {
    return std::find_if(proposal.comments.begin(), proposal.comments.end(), [&](auto &o) {
      return o.id == comment_id;
    });
  }

  /// @abi action
  void addcomment(proposal_id_t proposal_id, comment_id_t comment_id, account_name author, string text)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    require_app_member(author);

    _proposals.modify(proposal, author, [&](auto &o) {
      comment_t comment;
      comment.id = comment_id;
      comment.text = text;
      comment.author = author;
      comment.created = block_timestamp(now());
      comment.modified = block_timestamp(0);

      o.comments.push_back(comment);
    });
  }

  /// @abi action
  void editcomment(proposal_id_t proposal_id, comment_id_t comment_id, string text)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    const auto comment = getcomment(*proposal, comment_id);
    eosio_assert(comment != proposal->comments.end(), "comment doesn't exist");
    require_app_member(comment->author);

    _proposals.modify(proposal, proposal->author, [&](auto &o) {
      auto comment = getcomment(o, comment_id);
      comment->text = text;
      comment->modified = block_timestamp(now());
    });
  }

  /// @abi action
  void delcomment(proposal_id_t proposal_id, comment_id_t comment_id)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");

    const auto comment = getcomment(*proposal, comment_id);
    eosio_assert(comment != proposal->comments.end(), "comment doesn't exist");
    require_app_member(comment->author);

    _proposals.modify(proposal, proposal->author, [&](auto &o) {
      o.comments.erase(std::remove_if(o.comments.begin(), o.comments.end(), [&](const comment_t &comment) {
                         return comment.id == comment_id;
                       }),
                       o.comments.end());
    });
  }

  inline auto gettspec(proposal_t &proposal, tspec_id_t tspec_id)
  {
    return std::find_if(proposal.tspecs.begin(), proposal.tspecs.end(), [&](const auto &o) {
      return o.id == tspec_id;
    });
  }

  inline const auto gettspec(const proposal_t &proposal, tspec_id_t tspec_id)
  {
    return std::find_if(proposal.tspecs.begin(), proposal.tspecs.end(), [&](const auto &o) {
      return o.id == tspec_id;
    });
  }

  /// @abi action
  void
  addtspec(proposal_id_t proposal_id, tspec_id_t tspec_id, account_name author, string text)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");

    eosio_assert(gettspec(*proposal, tspec_id) == proposal->tspecs.end(),
                 "technical specification is already exists with the same id");

    _proposals.modify(proposal, author, [&](auto &o) {
      tspec_t spec;
      spec.id = tspec_id;
      spec.author = author;
      spec.text = text;
      spec.created = block_timestamp(now());
      spec.modified = block_timestamp(0);
      o.tspecs.push_back(spec);
    });
  }

  /// @abi action
  void edittspec(proposal_id_t proposal_id, tspec_id_t tspec_id, string text)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    const auto tspec = gettspec(*proposal, tspec_id);
    eosio_assert(tspec != proposal->tspecs.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);

    _proposals.modify(proposal, tspec->author, [&](auto &o) {
      auto tspec = gettspec(o, tspec_id);
      tspec->text = text;
    });
  }

  /// @abi action
  void deltspec(proposal_id_t proposal_id, tspec_id_t tspec_id)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    auto tspec = gettspec(*proposal, tspec_id);
    eosio_assert(tspec != proposal->tspecs.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);

    _proposals.modify(proposal, tspec->author, [&](auto &o) {
      o.tspecs.erase(gettspec(o, tspec_id));
    });
  }

  /// @abi action
  void uvotetspec(proposal_id_t proposal_id, tspec_id_t tspec_id, account_name author, string comment)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    const auto tspec = gettspec(*proposal, tspec_id);
    eosio_assert(tspec != proposal->tspecs.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);

    eosio_assert(std::find_if(tspec->upvotes.begin(), tspec->upvotes.end(), [&](auto &o) {
                   return o.author == author;
                 }) == tspec->upvotes.end(),
                 "user has been already voted");

    eosio_assert(std::find_if(tspec->downvotes.begin(), tspec->downvotes.end(), [&](auto &o) {
                   return o.author == author;
                 }) == tspec->downvotes.end(),
                 "user has been already voted");
    _proposals.modify(proposal, tspec->author, [&](auto &o) {
      auto tspec = gettspec(o, tspec_id);
      tspec->upvotes.push_back(tspec_vote_t{author, comment});
    });
  }

  /// @abi action
  void dvotetspec(proposal_id_t proposal_id, tspec_id_t tspec_id, account_name author, string comment)
  {
    auto proposal = _proposals.find(proposal_id);
    eosio_assert(proposal != _proposals.end(), "proposal has not been found");
    const auto tspec = gettspec(*proposal, tspec_id);
    eosio_assert(tspec != proposal->tspecs.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);

    eosio_assert(std::find_if(tspec->upvotes.begin(), tspec->upvotes.end(), [&](auto &o) {
                   return o.author == author;
                 }) == tspec->upvotes.end(),
                 "user has been already voted");

    eosio_assert(std::find_if(tspec->downvotes.begin(), tspec->downvotes.end(), [&](auto &o) {
                   return o.author == author;
                 }) == tspec->downvotes.end(),
                 "user has been already voted");

    _proposals.modify(proposal, tspec->author, [&](auto &o) {
      auto tspec = gettspec(o, tspec_id);
      tspec->downvotes.push_back(tspec_vote_t{author, comment});
    });
  }


  // https://tbfleming.github.io/cib/eos.html#gist=d230f3ab2998e8858d3e51af7e4d9aeb
  /// @abi action
  void transfer(uint64_t code /* code == golos.token */)
  {
    auto data = unpack_action_data<currency::transfer>();
    if (data.from == _self || data.to != _self)
      return;

  /**
   struct transfer
     {
        account_name from;
        account_name to;
        asset        quantity;
        string       memo;

        EOSLIB_SERIALIZE( transfer, (from)(to)(quantity)(memo) )
      };
    */
    eosio_assert(data.quantity.is_valid(), "Quntity is invalid");
    eosio_assert(data.quantity.amount > 0, "Invalid transfer amount");
  }
};
} // namespace golos

APP_DOMAIN_ABI(golos::worker, (createpool)(addpropos)(editpropos)(delpropos)(upvote)(downvote)(addcomment)(editcomment)(delcomment)(addtspec)(edittspec)(deltspec)(uvotetspec)(dvotetspec))