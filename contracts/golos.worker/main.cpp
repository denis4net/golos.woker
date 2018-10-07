#include <eosiolib/eosio.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/crypto.h>

#include <string>
#include <vector>
#include <algorithm>

#include "external.hpp"
#include "structs.hpp"

#include "app_dispatcher.hpp"

using namespace eosio;

#define TOKEN_ACCOUNT N(golos.token)
#define ZERO_ASSET asset(0, get_state()->token_symbol)
#define TIMESTAMP_UNDEFINED block_timestamp(0)
#define TIMESTAMP_NOW block_timestamp(now())

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

  struct comment_data_t
  {
    string text;

    EOSLIB_SERIALIZE(comment_data_t, (text));
  };

  struct comment_t
  {
    comment_id_t id;
    account_name author;
    comment_data_t data;
    block_timestamp created;
    block_timestamp modified;

    EOSLIB_SERIALIZE(comment_t, (id)(author)(data)(created)(modified));
  };

  struct comments_module_t
  {
    vector<comment_t> comments;

    void add(comment_id_t id, account_name author, const comment_data_t &data)
    {
      eosio_assert(std::find_if(comments.begin(), comments.end(), [&](const comment_t &comment) {
                     return comment.id == id;
                   }) == comments.end(),
                   "comment with the same id is already exists");

      comment_t comment{
          .id = id,
          .author = author,
          .data = data,
          .created = TIMESTAMP_NOW,
          .modified = TIMESTAMP_UNDEFINED};

      comments.push_back(comment);
    }

    auto lookup(comment_id_t id)
    {
      return std::find_if(comments.begin(), comments.end(), [&](auto &o) {
        return o.id == id;
      });
    }

    void del(comment_id_t id)
    {
      auto comment = lookup(id);
      eosio_assert(comment != comments.end(), "comment doens't exist");
      require_auth(comment->author);
      comments.erase(comment);
    }

    void edit(comment_id_t id, const comment_data_t &data)
    {
      auto comment = lookup(id);
      eosio_assert(comment != comments.end(), "comment doesn't exist");
      if (!data.text.empty())
      {
        comment->data.text = data.text;
      }
    }

    EOSLIB_SERIALIZE(comments_module_t, (comments));
  };

  typedef uint64_t tspec_app_id_t;

  struct tspec_data_t
  {
    string text;
    asset specification_cost;
    block_timestamp specification_eta;
    asset development_cost;
    block_timestamp development_eta;
    uint8_t payments_count;

    void update(const tspec_data_t &that)
    {
      if (!that.text.empty())
      {
        text = that.text;
      }
      if (that.specification_cost.amount != 0)
      {
        specification_cost = that.specification_cost;
      }

      if (that.specification_eta != TIMESTAMP_UNDEFINED)
      {
        specification_eta = that.specification_eta;
      }

      if (that.development_cost.amount != 0)
      {
        development_cost = that.development_cost;
      }

      if (that.development_eta != TIMESTAMP_UNDEFINED)
      {
        development_eta = that.development_eta;
      }

      if (that.payments_count != 0)
      {
        payments_count = that.payments_count;
      }
    }

    EOSLIB_SERIALIZE(tspec_data_t, (text)(specification_cost)(specification_eta)(development_cost)(development_eta)(payments_count));
  };

  struct voting_module_t
  {
    set_t<account_name> upvotes;
    set_t<account_name> downvotes;

    bool upvoted(account_name voter)
    {
      return upvotes.has(voter);
    }

    bool downvoted(account_name voter)
    {
      return downvotes.has(voter);
    }

    void upvote(account_name voter)
    {
      eosio_assert(!upvotes.has(voter), "already upvoted");
      eosio_assert(!downvotes.has(voter), "already downvoted");
      upvotes.set(voter);
    }

    void downvote(account_name voter)
    {
      eosio_assert(!upvotes.has(voter), "already upvoted");
      eosio_assert(!downvotes.has(voter), "already downvoted");
      downvotes.set(voter);
    }

    void delvote(account_name voter)
    {
      upvotes.unset(voter);
      downvotes.unset(voter);
    }

    EOSLIB_SERIALIZE(voting_module_t, (upvotes)(downvotes));
  };

  struct tspec_app_t
  {
    tspec_app_id_t id;
    account_name author;

    tspec_data_t data;

    voting_module_t votes;
    comments_module_t comments;

    block_timestamp created;
    block_timestamp modified;

    EOSLIB_SERIALIZE(tspec_app_t, (id)(author)(data)(votes)(created)(modified));

    tspec_app_t() : created(TIMESTAMP_NOW) {}
    tspec_app_t(tspec_app_id_t id, account_name author) : created(TIMESTAMP_NOW), id(id), author(author) {}


    void modify(const tspec_data_t &that)
    {
      data.update(that);
      modified = TIMESTAMP_NOW;
    }
  };
  typedef uint64_t proposal_id_t;

  //@abi table proposals i64
  struct proposal_t
  {

    enum state_t
    {
      STATE_TSPEC_APP,
      STATE_TSPEC_CREATE,
      STATE_TSPEC_WORK,
      STATE_TSPEC_FINISHED
    };

    proposal_id_t id;
    account_name author;
    string title;
    string description;

    voting_module_t votes;
    comments_module_t comments;

    // technical specification applications
    vector<tspec_app_t> tspec_apps;

    ///< technical specification author
    account_name tspec_author;
    ///< technical specification data
    tspec_data_t tspec;
    ///< perpetrator account name
    account_name worker;

    block_timestamp created;
    block_timestamp modified;
    state_t state;

    uint64_t primary_key() const { return id; }

    void set_state(state_t new_state) {
      state = new_state;
    }

    EOSLIB_SERIALIZE(proposal_t, (id)(author)(title)(description)(votes)(comments)(tspec_apps)(created)(modified)(state));
  };

  typedef multi_index<N(proposals), proposal_t> proposals_t;
  proposals_t *_proposals = nullptr;

  //@abi table states i64
  struct state_t
  {
    symbol_name token_symbol;

    uint64_t primary_key() const { return 0; }

    EOSLIB_SERIALIZE(state_t, (token_symbol));
  };

  typedef multi_index<N(states), state_t> states_t;
  states_t *_states = nullptr;

  //@abi table funds i64
  struct fund_t
  {
    account_name owner;
    asset quantity;
    uint64_t primary_key() const { return owner; }
    EOSLIB_SERIALIZE(fund_t, (owner)(quantity));
  };

  typedef multi_index<N(funds), fund_t> funds_t;
  funds_t *_funds = nullptr;

  app_domain_t _app = 0;

protected:
  auto get_state()
  {
    auto itr = _states->find(0);
    eosio_assert(itr != get_states().end(), "worker's pool has not been created for the speicifed app domain");
    return itr;
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

  void withdraw(asset quantity, string memo = "")
  {
    action(
        permission_level{_self, N(active)},
        TOKEN_ACCOUNT, N(transfer),
        std::make_tuple(_self, _self, quantity, std::string("")))
        .send();
  }

  void init(app_domain_t app)
  {
    _app = app;
    _states = new states_t(_self, app);
    _proposals = new proposals_t(_self, app);
    _funds = new funds_t(_self, app);
  }

  states_t &get_states()
  {
    eosio_assert(_states != nullptr, "contract is not initialized");
    return *_states;
  }

  proposals_t &get_proposals()
  {
    eosio_assert(_proposals != nullptr, "contract is not initialized");
    return *_proposals;
  }

  const auto get_proposal(proposal_id_t proposal_id) {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    return proposal;
  }

  funds_t &get_funds()
  {
    eosio_assert(_funds != nullptr, "contract is not initialized");
    return *_funds;
  }

public:
  worker(account_name owner, app_domain_t app) : contract(owner)
  {
    init(app);
  }

  worker(account_name owner) : contract(owner)
  {
  }

  ~worker()
  {
    delete _proposals;
    delete _states;
    delete _funds;
  }

  /// @abi action
  void createpool(symbol_name token_symbol)
  {
    LOG("creating worker's pool: code=\"%\" app=\"%\"", name{_self}.to_string().c_str(), name{_app}.to_string().c_str());
    eosio_assert(get_states().find(0) == get_states().end(), "workers pool is already initialized for the specified app domain");
    require_auth(_app);

    get_states().emplace(_app, [&](auto &o) {
      o.token_symbol = token_symbol;
    });
  }

  /// @abi action
  void addpropos(proposal_id_t proposal_id, account_name author, string title, string description)
  {
    require_app_member(author);

    LOG("adding propos % \"%\" by %", proposal_id, title.c_str(), name{author}.to_string().c_str());

    get_proposals().emplace(author, [&](auto &o) {
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
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    require_app_member(proposal->author);

    get_proposals().modify(proposal, proposal->author, [&](auto &o) {
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
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    eosio_assert(proposal->votes.upvotes.size() == 0, "proposal has been approved by one member");
    require_app_member(proposal->author);
    get_proposals().erase(proposal);
  }

  /// @abi action
  void upvote(proposal_id_t proposal_id, account_name author)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    require_app_delegate(author);

    get_proposals().modify(proposal, author, [&](auto &o) {
      o.votes.upvote(author);
    });
  }

  /// @abi action
  void downvote(proposal_id_t proposal_id, account_name author)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    require_app_delegate(author);

    get_proposals().modify(proposal, author, [&](auto &o) {
      o.votes.downvote(author);
    });
  }

  /// @abi action
  void addcomment(proposal_id_t proposal_id, comment_id_t comment_id, account_name author, const comment_data_t &data)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    require_app_member(author);

    get_proposals().modify(proposal, author, [&](auto &proposal) {
      proposal.comments.add(comment_id, author, data);
    });
  }

  /// @abi action
  void editcomment(proposal_id_t proposal_id, comment_id_t comment_id, const comment_data_t &data)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");

    get_proposals().modify(proposal, proposal->author, [&](auto &proposal) {
      proposal.comments.edit(comment_id, data);
    });
  }

  /// @abi action
  void delcomment(proposal_id_t proposal_id, comment_id_t comment_id)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");

    get_proposals().modify(proposal, proposal->author, [&](auto &proposal) {
      proposal.comments.del(comment_id);
    });
  }

  inline auto gettspec(proposal_t &proposal, tspec_app_id_t tspec_app_id)
  {
    return std::find_if(proposal.tspec_apps.begin(), proposal.tspec_apps.end(), [&](const auto &o) {
      return o.id == tspec_app_id;
    });
  }

  inline const auto gettspec(const proposal_t &proposal, tspec_app_id_t tspec_app_id)
  {
    return std::find_if(proposal.tspec_apps.begin(), proposal.tspec_apps.end(), [&](const auto &o) {
      return o.id == tspec_app_id;
    });
  }

  /// @abi action
  void addtspec(proposal_id_t proposal_id, tspec_app_id_t tspec_id, account_name author, const tspec_data_t &data)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");

    const auto tspec = gettspec(*proposal, tspec_id);
    eosio_assert(tspec == proposal->tspec_apps.end(),
                 "technical specification is already exists with the same id");

    get_proposals().modify(proposal, author, [&](auto &o) {
      tspec_app_t spec(tspec_id, author);
      spec.data.update(data);
      o.tspec_apps.push_back(spec);
    });
  }

  /// @abi action
  void edittspec(proposal_id_t proposal_id, tspec_app_id_t tspec_app_id, account_name author, const tspec_data_t &that)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    const auto tspec = gettspec(*proposal, tspec_app_id);
    eosio_assert(tspec != proposal->tspec_apps.end(), "technical specification doesn't exist");
    eosio_assert(that.specification_cost.symbol == get_state()->token_symbol, "invalid token symbol");
    eosio_assert(that.development_cost.symbol == get_state()->token_symbol, "invalid token symbol");

    require_app_member(tspec->author);

    get_proposals().modify(proposal, tspec->author, [&](auto &o) {
      auto tspec = gettspec(o, tspec_app_id);
      tspec->modify(that);
    });
  }

  /// @abi action
  void deltspec(proposal_id_t proposal_id, tspec_app_id_t tspec_app_id)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    auto tspec = gettspec(*proposal, tspec_app_id);
    eosio_assert(tspec != proposal->tspec_apps.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);
    eosio_assert(tspec->votes.upvotes.empty(), "technical specification bid can't be deleted because it already has been upvoted"); //Technical Specification 1.e

    get_proposals().modify(proposal, tspec->author, [&](auto &o) {
      o.tspec_apps.erase(gettspec(o, tspec_app_id));
    });
  }

  /// @abi action
  void uvotetspec(proposal_id_t proposal_id, tspec_app_id_t tspec_app_id, account_name author, string comment)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");

    const auto tspec = gettspec(*proposal, tspec_app_id);
    eosio_assert(tspec != proposal->tspec_apps.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);

    get_proposals().modify(proposal, tspec->author, [&](auto &o) {
      auto tspec = gettspec(o, tspec_app_id);
      tspec->votes.upvote(author);

      if (tspec->votes.upvotes.size() >= witness_count_51) {

      }
    });
  }

  /// @abi action
  void dvotetspec(proposal_id_t proposal_id, tspec_app_id_t tspec_app_id, account_name author, string comment)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    const auto tspec = gettspec(*proposal, tspec_app_id);
    eosio_assert(tspec != proposal->tspec_apps.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);

    get_proposals().modify(proposal, tspec->author, [&](auto &o) {
      auto tspec = gettspec(o, tspec_app_id);
      tspec->votes.downvote(author);
    });
  }

  /// @abi action
  void settspec(proposal_id_t proposal_id, const tspec_data_t &data) {
    auto proposalIt = get_proposal(proposal_id);
    eosio_assert(proposalIt->state == proposal_t::STATE_TSPEC_CREATE, "invalid proposal state");

    get_proposals().modify(proposalIt, proposalIt->tspec_author, [&](proposal_t &proposal) {
      proposal.tspec.update(data);
      proposal.set_state(proposal_t::STATE_TSPEC_WORK);
    });
  }

  void setworker(proposal_id_t proposal_id, account_name worker) {
    auto proposalIt = get_proposal(proposal_id);
    eosio_assert(proposalIt->state == proposal_t::STATE_TSPEC_CREATE, "invalid proposal state");

    get_proposals().modify(proposalIt, proposalIt->tspec_author, [&](proposal_t &proposal) {
      proposal.worker = worker;
      proposal.set_state(proposal_t::STATE_TSPEC_WORK);
    });
  }

  /// @abi action
  void cancel(proposal_id_t proposal_id, account_name delegate) {
    auto proposalIt = get_proposal(proposal_id);
    eosio_assert(proposalIt->state == proposal_t::STATE_TSPEC_WORK, "invalid proposal state");

  }

  /// @abi action
  void acceptwork(proposal_id_t proposal_id, account_name tspec_author) {
    auto proposalIt = get_proposal(proposal_id);
    eosio_assert(proposalIt->state == proposal_t::STATE_TSPEC_WORK, "invalid proposal state");

  }

  /// @abi action
  void approvework(proposal_id_t proposal_id, account_name delegate) {
    require_app_delegate(delegate);
    auto proposalIt = get_proposal(proposal_id);

  }

  // https://tbfleming.github.io/cib/eos.html#gist=d230f3ab2998e8858d3e51af7e4d9aeb
  /// @abi action
  void transfer(currency::transfer &t)
  {
    init(eosio::string_to_name(t.memo.c_str()));
    LOG("transfer % from \"%\" to \"%\"", t.quantity.amount, name{t.from}.to_string().c_str(), name{t.to}.to_string().c_str());

    if (t.to != _self || t.quantity.symbol == get_state()->token_symbol)
    {
      LOG("transfer from a wrong token");
      return;
    }

    eosio_assert(t.quantity.is_valid(), "Quntity is invalid");
    eosio_assert(t.quantity.amount > 0, "Invalid transfer amount");

    const account_name &payer = t.to;

    auto fund = get_funds().find(t.from);
    if (fund == get_funds().end())
    {
      get_funds().emplace(payer, [&](auto &fund) {
        fund.owner = t.from;
        fund.quantity = t.quantity;
      });
    }
    else
    {
      get_funds().modify(fund, payer, [&](auto &fund) {
        eosio_assert(fund.owner == t.from, "invalid fund owner");
        fund.quantity += t.quantity;
      });
    }
  }
};
} // namespace golos

APP_DOMAIN_ABI(golos::worker, (createpool)(addpropos)(editpropos)(delpropos)(upvote)(downvote)(addcomment)(editcomment)(delcomment)(addtspec)(edittspec)(deltspec)(uvotetspec)(dvotetspec), (transfer))