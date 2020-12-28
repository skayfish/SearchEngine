#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <utility>

using namespace std::string_literals;

template<typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Value& ref_to_value;
        std::mutex* guard_mutex;
        Access(Value& value, std::mutex* guard)
            :ref_to_value(value)
            , guard_mutex(guard) {
        }
        ~Access() {
            guard_mutex->unlock();
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : storage_maps_(bucket_count)
        , storage_mutexes_(bucket_count) {

    }

    Access operator[](const Key& key) {
        uint64_t pos = static_cast<uint64_t>(key);
        size_t size = storage_maps_.size();
        auto& mutex_tmp = storage_mutexes_[pos % size];
        mutex_tmp.lock();
        auto& tmp = storage_maps_[pos % size][key];
        return Access(tmp, &mutex_tmp);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        auto size = storage_maps_.size();
        for (size_t i = 0; i < size; i++) {
            while (!storage_mutexes_[i].try_lock());
            auto tmp = storage_maps_[i];
            result.merge(move(tmp));
            storage_mutexes_[i].unlock();
        }
        return result;
    }

private:
    std::vector<std::map<Key, Value>> storage_maps_;
    std::vector<std::mutex> storage_mutexes_;
};