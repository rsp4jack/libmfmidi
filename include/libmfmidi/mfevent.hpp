#pragma once

#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace libmfmidi {
    template <class T>
    concept event = std::regular<T>;

    template <class T>
    concept event_with_data = event<T> && requires(T ev) {
        ev.data();
    };
    template <class T>
    using event_data_t = decltype(std::declval<T>().data());

    template <class T, class Ev>
    concept event_handler = std::semiregular<T> && std::regular_invocable<T, const Ev&>;

    template <class T>
    concept event_emitter = requires(T obj, decltype([] {}) handler) {
        obj.remove_event_handler(obj.add_event_handler(handler));
        {
            obj.add_event_handler(handler)
        } -> std::regular;
    };

    template <class... Events>
    class event_emitter_util {
    public:
        struct token {
            std::size_t _id;
        };

        constexpr token add_event_handler(auto handler)
        {
            auto try_event_type = [this]<class Ev>(auto handler) {
                if constexpr (event_handler<decltype(handler), Ev>) {
                    return handlers.emplace(std::piecewise_construct, std::make_tuple(counter), std::make_tuple(std::in_place_type_t<std::function<void(Ev)>>{}, std::move(handler)));
                }
            };
            static_assert((event_handler<decltype(handler), Events> || ...), "handler must satisfy at least one event type's constraints");
            (try_event_type.template operator()<Events>(handler), ...);
            return token{counter++};
        }

        constexpr void remove_event_handler(token tok)
        {
            handlers.erase(tok);
        }

        template <event Ev>
        constexpr void emit(const Ev& ev)
        {
            static_assert(sizeof...(Events) > 0);
            static_assert((std::same_as<Ev, Events> || ...), "cannot emit event of type that is not in Events list");
            for (const auto& handler : handlers | std::views::elements<1>) {
                std::visit(
                    [&](auto&& func) {
                        using T = std::decay_t<decltype(func)>;
                        if constexpr (std::same_as<T, std::function<void(Ev)>>) {
                            func(ev);
                        }
                    },
                    handler
                );
            }
        }

        std::size_t                                                                        counter{};
        std::unordered_multimap<std::size_t, std::variant<std::function<void(Events)>...>> handlers;
    };
}