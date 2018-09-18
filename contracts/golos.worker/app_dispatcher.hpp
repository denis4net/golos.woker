#include <iostream>
#include <tuple>
#include <utility>

namespace golos
{

using namespace ::eosio;

template < typename T , typename... Ts >
auto tuple_head( std::tuple<T,Ts...> t )
{
   return  std::get<0>(t);
}

template < std::size_t... Ns , typename... Ts >
auto tail_impl( std::index_sequence<Ns...> , std::tuple<Ts...> t )
{
   return  std::make_tuple( std::get<Ns+1u>(t)... );
}

template < typename... Ts >
auto tuple_tail( std::tuple<Ts...> t )
{
   return  tail_impl( std::make_index_sequence<sizeof...(Ts) - 1u>() , t );
}

template <typename T, typename Q, typename... Args>
bool execute_action(void (Q::*func)(Args...))
{
    size_t size = action_data_size();
    //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void *buffer = nullptr;
    if (size > 0)
    {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data(buffer, size);
    }

    auto args = unpack<std::tuple<symbol_name /* app domain */, std::decay_t<Args>... /* function args */>>((char *)buffer, size);

    if (max_stack_buffer_size < size)
    {
        free(buffer);
    }

    T obj(current_receiver(), tuple_head(args));

    auto f2 = [&](auto... a) {
        (obj.*func)(a...);
    };

    boost::mp11::tuple_apply(f2, tuple_tail(args));
    return true;
}

#define APP_DOMAIN_API_CALL(r, TYPENAME, elem)                    \
    case ::eosio::string_to_name(BOOST_PP_STRINGIZE(elem)): \
        ::golos::execute_action<TYPENAME>(&TYPENAME::elem);        \
        break;

#define APP_DOMAIN_API(TYPENAME, MEMBERS) \
    BOOST_PP_SEQ_FOR_EACH(APP_DOMAIN_API_CALL, TYPENAME, MEMBERS)

#define APP_DOMAIN_ABI(TYPENAME, MEMBERS)                                                                                            \
    extern "C"                                                                                                                   \
    {                                                                                                                            \
        void apply(uint64_t receiver, uint64_t code, uint64_t action)                                                            \
        {                                                                                                                        \
            auto self = receiver;                                                                                                \
            if (action == N(onerror))                                                                                            \
            {                                                                                                                    \
                /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
                eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");             \
            }                                                                                                                    \
            if (code == self || action == N(onerror))                                                                            \
            {                                                                                                                    \
                switch (action)                                                                                                  \
                {                                                                                                                \
                    APP_DOMAIN_API(TYPENAME, MEMBERS)                                                                                \
                }                                                                                                                \
                /* does not allow destructor of thiscontract to run: eosio_exit(0); */                                           \
            }                                                                                                                    \
        }                                                                                                                        \
    }                                                                                                                            \
} // namespace app