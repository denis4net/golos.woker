#pragma once

#include <vector>
#include <algorithm>

#define EOSLIB_SERIALIZE_DERIVED2( TYPE, BASE ) \
 template<typename DataStream> \
 friend DataStream& operator << ( DataStream& ds, const TYPE& t ){ \
    return ds << static_cast<const BASE&>(t); \
 }\
 template<typename DataStream> \
 friend DataStream& operator >> ( DataStream& ds, TYPE& t ){ \
    return ds >> static_cast<BASE&>(t); \
 }

template <typename T>
class set_t : public std::vector<T>
{
  public:
    using std::vector<T>::end;
    using std::vector<T>::begin;
    using std::vector<T>::erase;
    using std::vector<T>::push_back;

    bool has(const T &v) const
    {
        return std::find(begin(), end(), v) != end();
    }

    void set(const T &v)
    {
        push_back(v);
    }

    bool unset(const T &v)
    {
        auto i = find(begin(), end(), v);
        if (i != end())
        {
            erase(i);
            return true;
        }
        return false;
    }

    EOSLIB_SERIALIZE_DERIVED2(set_t, std::vector<T>);
};
