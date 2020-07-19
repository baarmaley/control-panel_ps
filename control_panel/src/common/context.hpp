//
// Created by mtsvetkov on 19.07.2020.
//

#pragma once

#include <memory>
#include <tuple>


namespace tsvetkov{
template<typename... Args>
struct MultiContextHolder {
    using StrongPtrTuple = std::tuple<std::shared_ptr<Args>...>;

    explicit MultiContextHolder(StrongPtrTuple strong_context) : strong_context(std::move(strong_context)) {}

    explicit operator bool() const {
        return impl_bool_conversion(std::make_index_sequence<sizeof...(Args)>{});
    }

    StrongPtrTuple& get() {
        return strong_context;
    }

private:
    template<size_t... I>
    bool impl_bool_conversion(std::index_sequence<I...>) const {
        return impl_bool_conversion(std::get<I>(strong_context)...);
    }

    template<typename Head, typename... Tail>
    bool impl_bool_conversion(const std::shared_ptr<Head>& head, const std::shared_ptr<Tail>&... tail) const {
        return head && impl_bool_conversion(tail...);
    }

    template<typename T>
    bool impl_bool_conversion(const std::shared_ptr<T>& ptr) const {
        return static_cast<bool>(ptr);
    }

    StrongPtrTuple strong_context;
};

template<typename... Args>
struct MultiContext {
    using WeakPtrTuple = std::tuple<std::weak_ptr<Args>...>;
    using StrongPtrTuple = std::tuple<std::shared_ptr<Args>...>;

    explicit MultiContext(WeakPtrTuple weak_context) : weak_context(std::move(weak_context)) {}

    MultiContextHolder<Args...> lock() {
        return MultiContextHolder<Args...>(lock(std::make_index_sequence<sizeof...(Args)>{}));
    }

private:
    template<size_t... I>
    StrongPtrTuple lock(std::index_sequence<I...>) {
        return std::make_tuple(std::get<I>(weak_context).lock()...);
    }

    WeakPtrTuple weak_context;
};

template<typename... Args>
MultiContext<Args...> make_multi_context(std::shared_ptr<Args>... args) {
    return MultiContext<Args...>(std::make_tuple(std::weak_ptr<Args>(args)...));
}

template<typename T>
std::weak_ptr<T> make_single_context(std::shared_ptr<T> strong_context){
    return std::weak_ptr<T>(strong_context);
}
}
