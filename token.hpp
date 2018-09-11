#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace golos {
    using eosio::asset;

    struct account
    {
        asset balance;

        uint64_t primary_key() const { return balance.symbol.name(); }
    };

    typedef eosio::multi_index<N(accounts), account> accounts;
}