#include <eosiolib/eosio.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/singleton.hpp>

#include <string>
#include <vector>
#include <algorithm>

#include "external.hpp"
#include "structs.hpp"

#include "app_dispatcher.hpp"

using namespace eosio;

#define TOKEN_ACCOUNT N(golos.token)
#define ZERO_ASSET asset(0, get_state().token_symbol)
#define TIMESTAMP_UNDEFINED block_timestamp(0)
#define TIMESTAMP_NOW block_timestamp(now())

#define LOG(format, ...) print_f("%(%): " format "\n", __FUNCTION__, name{_app}.to_string().c_str(), ##__VA_ARGS__);

namespace golos
{
using std::string;

class worker : public contract
{
public:
  static constexpr uint32_t voting_time_s = 7 * 24 * 3600;

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

    EOSLIB_SERIALIZE(comments_module_t, (comments));

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
  };

  typedef uint64_t tspec_id_t;

  struct tspec_data_t
  {
    string text;
    asset specification_cost;
    block_timestamp specification_eta;
    asset development_cost;
    block_timestamp development_eta;
    uint8_t payments_count;

    EOSLIB_SERIALIZE(tspec_data_t, (text)(specification_cost)(specification_eta)(development_cost)(development_eta)(payments_count));

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
  };

  struct voting_module_t
  {
    enum vote_value_t
    {
      VOTE_DOWN = 0,
      VOTE_UP = 1
    };

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

    void vote(account_name voter, vote_value_t vote)
    {
      switch (vote)
      {
      case VOTE_UP:
        upvote(voter);
        break;
      case VOTE_DOWN:
        downvote(voter);
        break;
      default:
        eosio_assert(false, "invalid vote argument");
      }
    }

    void delvote(account_name voter)
    {
      upvotes.unset(voter);
      downvotes.unset(voter);
    }

    EOSLIB_SERIALIZE(voting_module_t, (upvotes)(downvotes));
  };

  struct  tspec_app_t
  {
    tspec_id_t id;
    account_name author;

    tspec_data_t data;

    voting_module_t votes;
    comments_module_t comments;

    block_timestamp created;
    block_timestamp modified;

    EOSLIB_SERIALIZE(tspec_app_t, (id)(author)(data)(votes)(comments)(created)(modified));

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
      STATE_TSPEC_APP = 1,
      STATE_TSPEC_CREATE,
      STATE_WORK,
      STATE_WORK_REVIEW,
      STATE_DELEGATES_REVIEW,
      STATE_CLOSED
    };

    enum review_status_t
    {
      STATUS_REJECT = 0,
      STATUS_ACCEPT = 1
    };

    enum type_t {
        TYPE_1,
        TYPE_2
    };

    proposal_id_t id;
    account_name author;
    type_t type;
    account_name fund_name;
    asset deposit;
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
    block_timestamp work_begining_time;
    account_name worker;
    voting_module_t review_votes;

    block_timestamp created;
    block_timestamp modified;
    uint8_t state;

    EOSLIB_SERIALIZE(proposal_t, (id)(author)(fund_name)(deposit)(title)(description)\
                     (votes)(comments)(tspec_apps)(tspec_author)(tspec)\
                     (work_begining_time)(worker)(created)(modified)(state));

    uint64_t primary_key() const { return id; }
    void set_state(state_t new_state) { state = new_state; }
  };

  typedef multi_index<N(proposals), proposal_t> proposals_t;
  proposals_t _proposals;

  //@abi table states i64
  struct state_t
  {
    symbol_name token_symbol;

    EOSLIB_SERIALIZE(state_t, (token_symbol));

    uint64_t primary_key() const { return 0; }
  };

  singleton<N(states), state_t> _state;

  //@abi table funds i64
  struct fund_t
  {
    account_name owner;
    asset quantity;

    EOSLIB_SERIALIZE(fund_t, (owner)(quantity));

    uint64_t primary_key() const { return owner; }
  };

  typedef multi_index<N(funds), fund_t> funds_t;
  funds_t _funds;

  app_domain_t _app = 0;

protected:
  auto get_state()
  {
    return _state.get();
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

  proposals_t &get_proposals()
  {
    return _proposals;
  }

  const auto get_proposal(proposal_id_t proposal_id)
  {
    auto proposal = get_proposals().find(proposal_id);
    eosio_assert(proposal != get_proposals().end(), "proposal has not been found");
    return proposal;
  }

  funds_t &get_funds()
  {
    return _funds;
  }

  auto get_fund(account_name fund_name)
  {
    auto fund_ptr = get_funds().find(fund_name);
    eosio_assert(fund_ptr != get_funds().end(), "fund doesn't exists");
    return fund_ptr;
  }

  inline auto get_tspec(proposal_t &proposal, tspec_id_t tspec_app_id)
  {
    return std::find_if(proposal.tspec_apps.begin(), proposal.tspec_apps.end(), [&](const auto &o) {
      return o.id == tspec_app_id;
    });
  }

  inline const auto get_tspec(const proposal_t &proposal, tspec_id_t tspec_app_id)
  {
    return std::find_if(proposal.tspec_apps.begin(), proposal.tspec_apps.end(), [&](const auto &o) {
      return o.id == tspec_app_id;
    });
  }

  void choose_proposal_tspec(proposal_t &proposal, tspec_app_t &tspec_app)
  {
    const asset budget = tspec_app.data.development_cost + tspec_app.data.specification_cost;
    auto fund = get_funds().find(proposal.fund_name);
    eosio_assert(fund != get_funds().end(), "fund doens't exist");
    eosio_assert(budget <= fund->quantity, "insufficient funds");

    proposal.tspec_author = tspec_app.author;
    proposal.tspec = tspec_app.data;

    if (proposal.type == proposal_t::TYPE_1) {
        proposal.set_state(proposal_t::STATE_TSPEC_CREATE);
    } else {
        proposal.set_state(proposal_t::STATE_DELEGATES_REVIEW);
        proposal.worker = proposal.tspec_author;
    }

    if (proposal.deposit == ZERO_ASSET)
    {
      proposal.deposit = budget;
      get_funds().modify(fund, proposal.fund_name, [&](auto &fund) {
        fund.quantity -= budget;
      });
    }
  }

  void pay(proposal_t &proposal, const string &memo)
  {
    withdraw(proposal.deposit, memo);
    proposal.deposit = ZERO_ASSET;
  }

  void refund(proposal_t &proposal)
  {
    eosio_assert(proposal.deposit.amount > 0, "no funds were deposited");

    auto fund_ptr = get_fund(proposal.fund_name);
    get_funds().modify(fund_ptr, _app, [&](auto &fund) {
      fund.quantity += proposal.deposit;
    });

    proposal.deposit = ZERO_ASSET;
  }

  void close(proposal_t &proposal)
  {
    proposal.set_state(proposal_t::STATE_CLOSED);
  }

public:
  worker(account_name owner, app_domain_t app) : contract(owner),
                                                 _app(app),
                                                 _state(_self, app),
                                                 _proposals(_self, app),
                                                 _funds(_self, app)
  {
  }

  /// @abi action
  void createpool(symbol_name token_symbol)
  {
    LOG("creating worker's pool: code=\"%\" app=\"%\"", name{_self}.to_string().c_str(), name{_app}.to_string().c_str());
    eosio_assert(!_state.exists(), "workers pool is already initialized for the specified app domain");
    require_auth(_app);

    _state.set(state_t{.token_symbol=token_symbol}, _app);
  }

  /// @abi action
  void addpropos(proposal_id_t proposal_id, account_name author, string title, string description)
  {
    require_app_member(author);

    LOG("adding propos % \"%\" by %", proposal_id, title.c_str(), name{author}.to_string().c_str());

    get_proposals().emplace(author, [&](auto &o) {
      o.id = proposal_id;
      o.type = proposal_t::TYPE_1;
      o.author = author;
      o.title = title;
      o.description = description;
      o.created = TIMESTAMP_NOW;
      o.modified = TIMESTAMP_UNDEFINED;
      o.state = (uint8_t)proposal_t::STATE_TSPEC_APP;
      o.fund_name = _app;
    });
    LOG("added");
  }

  void addpropos2(proposal_id_t proposal_id, account_name author,
                  const string& title, const string& description,
                  const tspec_data_t &specification, account_name worker)
  {
      require_app_member(author);

      LOG("adding propos % \"%\" by %", proposal_id, title.c_str(), name{author}.to_string().c_str());

      get_proposals().emplace(author, [&](proposal_t &o) {
        o.id = proposal_id;
        o.type = proposal_t::TYPE_2;
        o.author = author;
        o.title = title;
        o.description = description;
        o.created = TIMESTAMP_NOW;
        o.modified = TIMESTAMP_UNDEFINED;
        o.state = (uint8_t)proposal_t::STATE_TSPEC_APP;
        o.tspec = specification;
        o.fund_name = _app;

        o.tspec_apps.push_back(tspec_app_t{
                                   .id = 0,
                                   .author = author,
                                   .data = specification,
                                   .created = TIMESTAMP_NOW,
                                   .modified = TIMESTAMP_UNDEFINED
                               });

      });
  }

  void setproposfund(proposal_id_t proposal_id, account_name fund_name, asset quantity)
  {
    auto proposal_ptr = get_proposals().find(proposal_id);
    eosio_assert(proposal_ptr != get_proposals().end(), "proposal has not been found");
    require_app_member(fund_name);

    eosio_assert(proposal_ptr->deposit == ZERO_ASSET, "fund is already deposited");
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_APP, "invalid state");

    auto fund_ptr = get_fund(fund_name);
    eosio_assert(fund_ptr->quantity >= quantity, "insufficient funds");

    get_proposals().modify(proposal_ptr, fund_name, [&](auto &o) {
      o.fund_name = fund_name;
      o.deposit = quantity;
    });

    get_funds().modify(fund_ptr, fund_name, [&](auto &fund) {
      fund.quantity -= quantity;
    });
  }

  // @abi action
  void editpropos(proposal_id_t proposal_id, string title, string description)
  {
    auto proposal_ptr = get_proposal(proposal_id);
    require_app_member(proposal_ptr->author);
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_APP, "invalid state");

    get_proposals().modify(proposal_ptr, proposal_ptr->author, [&](auto &o) {
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
    auto proposal_ptr = get_proposal(proposal_id);
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_APP, "invalid state");
    eosio_assert(proposal_ptr->votes.upvotes.size() == 0, "proposal has been approved by one member");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    require_app_member(proposal_ptr->author);
    get_proposals().erase(proposal_ptr);
  }

  /// @abi action
  void votepropos(proposal_id_t proposal_id, account_name author, uint8_t vote)
  {
    auto proposal_ptr = get_proposals().find(proposal_id);
    eosio_assert(proposal_ptr != get_proposals().end(), "proposal has not been found");
    eosio_assert(voting_time_s + proposal_ptr->created.to_time_point().sec_since_epoch() <= now(), "voting time is over");
    require_app_member(author);

    get_proposals().modify(proposal_ptr, author, [&](auto &o) {
      o.votes.vote(author, static_cast<voting_module_t::vote_value_t>(vote));
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

  /// @abi action
  void addtspec(proposal_id_t proposal_id, tspec_id_t tspec_id, account_name author, const tspec_data_t &tspec)
  {
    auto proposal_ptr = get_proposals().find(proposal_id);
    eosio_assert(proposal_ptr != get_proposals().end(), "proposal has not been found");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    const auto tspec_ptr = get_tspec(*proposal_ptr, tspec_id);
    eosio_assert(tspec_ptr == proposal_ptr->tspec_apps.end(),
                 "technical specification is already exists with the same id");

    get_proposals().modify(proposal_ptr, author, [&](auto &o) {
      tspec_app_t spec = {
                          .id=tspec_id,
                          .author=author,
                          .created=TIMESTAMP_NOW,
                          .modified=TIMESTAMP_UNDEFINED,
                          .data = tspec
                         };
      o.tspec_apps.push_back(spec);
    });
  }

  /// @abi action
  void edittspec(proposal_id_t proposal_id, tspec_id_t tspec_app_id, account_name author, const tspec_data_t &that)
  {
    auto proposal_ptr = get_proposals().find(proposal_id);
    eosio_assert(proposal_ptr != get_proposals().end(), "proposal has not been found");
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_APP, "invalid state");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    const auto tspec = get_tspec(*proposal_ptr, tspec_app_id);
    eosio_assert(tspec != proposal_ptr->tspec_apps.end(), "technical specification doesn't exist");
    eosio_assert(that.specification_cost.symbol == get_state().token_symbol, "invalid token symbol");
    eosio_assert(that.development_cost.symbol == get_state().token_symbol, "invalid token symbol");

    require_app_member(tspec->author);

    get_proposals().modify(proposal_ptr, tspec->author, [&](auto &o) {
      auto tspec = get_tspec(o, tspec_app_id);
      tspec->modify(that);
    });
  }

  /// @abi action
  void deltspec(proposal_id_t proposal_id, tspec_id_t tspec_app_id)
  {
    auto proposal_ptr = get_proposals().find(proposal_id);
    eosio_assert(proposal_ptr != get_proposals().end(), "proposal has not been found");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    auto tspec = get_tspec(*proposal_ptr, tspec_app_id);
    eosio_assert(tspec != proposal_ptr->tspec_apps.end(), "technical specification doesn't exist");
    require_app_member(tspec->author);
    eosio_assert(tspec->votes.upvotes.empty(), "technical specification bid can't be deleted because it already has been upvoted"); //Technical Specification 1.e

    get_proposals().modify(proposal_ptr, tspec->author, [&](auto &o) {
      o.tspec_apps.erase(get_tspec(o, tspec_app_id));
    });
  }

  /// @abi action
  void votetspec(proposal_id_t proposal_id, tspec_id_t tspec_app_id, account_name author, uint8_t vote, comment_id_t comment_id, const comment_data_t &comment)
  {
    auto proposal_ptr = get_proposals().find(proposal_id);
    eosio_assert(proposal_ptr != get_proposals().end(), "proposal has not been found");
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_APP, "invalid state");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    const auto tspec_ptr = get_tspec(*proposal_ptr, tspec_app_id);
    eosio_assert(tspec_ptr != proposal_ptr->tspec_apps.end(), "technical specification doesn't exist");
    require_app_delegate(author);
    eosio_assert(voting_time_s + tspec_ptr->created.to_time_point().sec_since_epoch() <= now(), "voting time is over");

    get_proposals().modify(proposal_ptr, tspec_ptr->author, [&](auto &o) {
      auto tspec = get_tspec(o, tspec_app_id);
      tspec->votes.vote(author, static_cast<voting_module_t::vote_value_t>(vote));

      if (!comment.text.empty())
      {
        tspec->comments.add(comment_id, author, comment);
      }

      switch (vote)
      {
      case voting_module_t::VOTE_UP:
        if (tspec->votes.upvotes.size() >= witness_count_51)
        {
          choose_proposal_tspec(o, *tspec);
        }
        break;
      case voting_module_t::VOTE_DOWN:
        break;
      }
    });
  }

  /// @abi action
  void settspec(proposal_id_t proposal_id, const tspec_data_t &data)
  {
    auto proposal_ptr = get_proposal(proposal_id);
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_CREATE, "invalid proposal state");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    get_proposals().modify(proposal_ptr, proposal_ptr->tspec_author, [&](proposal_t &proposal) {
      proposal.tspec.update(data);
      proposal.set_state(proposal_t::STATE_WORK);
    });
  }

  void startwork(proposal_id_t proposal_id, account_name worker)
  {
    auto proposal_ptr = get_proposal(proposal_id);
    eosio_assert(proposal_ptr->state == proposal_t::STATE_TSPEC_CREATE, "invalid proposal state");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    get_proposals().modify(proposal_ptr, proposal_ptr->tspec_author, [&](proposal_t &proposal) {
      proposal.worker = worker;
      proposal.work_begining_time = TIMESTAMP_NOW;
      proposal.set_state(proposal_t::STATE_WORK);
    });
  }

  void cancelwork(proposal_id_t proposal_id, account_name initiator) {
      auto proposal_ptr = get_proposal(proposal_id);
      eosio_assert(proposal_ptr->state == proposal_t::STATE_WORK, "invalid proposal state");
      eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

      if (initiator == proposal_ptr->worker) {
        require_auth(proposal_ptr->worker);
      } else {
          require_auth(proposal_ptr->tspec_author);
      }

      get_proposals().modify(proposal_ptr, proposal_ptr->worker, [&](auto &proposal) {
          refund(proposal);
          close(proposal);
          proposal.set_state(proposal_t::STATE_CLOSED);
      });
  }

  void finishwork(proposal_id_t proposal_id)
  {
    auto proposal_ptr = get_proposal(proposal_id);
    eosio_assert(proposal_ptr->state == proposal_t::STATE_WORK, "invalid proposal state");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");
    require_auth(proposal_ptr->worker);

    get_proposals().modify(proposal_ptr, proposal_ptr->worker, [&](auto &proposal) {
      proposal.set_state(proposal_t::STATE_WORK_REVIEW);
    });
  }

  /// @abi action
  void acceptwork(proposal_id_t proposal_id, account_name tspec_author)
  {
    auto proposal_ptr = get_proposal(proposal_id);
    eosio_assert(proposal_ptr->state == proposal_t::STATE_WORK_REVIEW, "invalid proposal state");
    eosio_assert(proposal_ptr->type == proposal_t::TYPE_1, "unsupported action");

    get_proposals().modify(proposal_ptr, proposal_ptr->worker, [&](auto &proposal) {
      proposal.set_state(proposal_t::STATE_DELEGATES_REVIEW);
    });
  }

  /// @abi action
  void reviewwork(proposal_id_t proposal_id, account_name reviewer, uint8_t status, comment_id_t comment_id, const comment_data_t &comment)
  {
    require_app_delegate(reviewer);
    auto proposal_ptr = get_proposal(proposal_id);
    require_app_delegate(reviewer);

    get_proposals().modify(proposal_ptr, reviewer, [&](proposal_t &proposal) {
      switch (status)
      {
      case proposal_t::STATUS_REJECT:
        eosio_assert(proposal.state == proposal_t::STATE_DELEGATES_REVIEW ||
                         proposal.state == proposal_t::STATE_WORK ||
                         proposal.state == proposal_t::STATE_WORK_REVIEW,
                     "invalid state");

        proposal.review_votes.downvote(reviewer);
        if (proposal.review_votes.downvotes.size() >= witness_count_51)
        {
          refund(proposal);
          close(proposal);
        }
        break;

      case proposal_t::STATUS_ACCEPT:
        eosio_assert(proposal.state == proposal_t::STATE_DELEGATES_REVIEW, "invalid state");

        proposal.review_votes.downvote(reviewer);
        if (proposal.review_votes.upvotes.size() >= witness_count_51)
        {
          pay(proposal, "work payment");
          close(proposal);
        }
        break;

      default:
        eosio_assert(false, "invalid review status");
      }

      if (!comment.text.empty())
      {
        proposal.comments.add(comment_id, reviewer, comment);
      }
    });
  }

  // https://tbfleming.github.io/cib/eos.html#gist=d230f3ab2998e8858d3e51af7e4d9aeb
  /// @abi action
  static void transfer(currency::transfer &t)
  {
    print_f("%: transfer % from \"%\" to \"%\"", __FUNCTION__, t.quantity.amount, name{t.from}.to_string().c_str(), name{t.to}.to_string().c_str());

    worker self(current_receiver(), eosio::string_to_name(t.memo.c_str()));
    if (t.to != self._self || t.quantity.symbol == self.get_state().token_symbol)
    {
      print_f("%: transfer from a wrong token", __FUNCTION__);
      return;
    }

    eosio_assert(t.quantity.is_valid(), "Quntity is invalid");
    eosio_assert(t.quantity.amount > 0, "Invalid transfer amount");

    const account_name &payer = t.to;

    auto fund = self.get_funds().find(t.from);
    if (fund == self.get_funds().end())
    {
      self.get_funds().emplace(payer, [&](auto &fund) {
        fund.owner = t.from;
        fund.quantity = t.quantity;
      });
    }
    else
    {
      self.get_funds().modify(fund, payer, [&](auto &fund) {
        eosio_assert(fund.owner == t.from, "invalid fund owner");
        fund.quantity += t.quantity;
      });
    }
  }
};
} // namespace golos

APP_DOMAIN_ABI(golos::worker, (createpool)\
               (addpropos2)\
               (addpropos)(editpropos)(delpropos)(votepropos)\
               (addcomment)(editcomment)(delcomment)\
               (addtspec)(edittspec)(deltspec)(votetspec)(settspec)\
               (startwork)(finishwork)(acceptwork)(reviewwork)(cancelwork),\
               (transfer))
