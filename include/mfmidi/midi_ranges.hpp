#pragma once

#include <concepts>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>

namespace mfmidi {
    namespace details {
        template <class T>
            requires std::move_constructible<T> && std::is_object_v<T>
        class movable_box : public std::optional<T> {
        public:
            constexpr movable_box() noexcept(std::is_nothrow_default_constructible_v<T>)
                requires std::default_initializable<T>
                : movable_box(std::in_place)
            {}

            using std::optional<T>::optional;
            using std::optional<T>::operator=;

            ~movable_box()                                                            = default;
            movable_box(const movable_box&)                                           = default;
            movable_box(movable_box&&) noexcept(std::is_nothrow_move_assignable_v<T>) = default;

            constexpr movable_box& operator=(const movable_box& other) noexcept(!std::is_copy_assignable_v<T> && std::is_nothrow_copy_constructible_v<T>)
                requires std::copy_constructible<T>
            {
                if constexpr (std::copyable<T>) {
                    std::optional<T>::operator=(other);
                    return *this;
                }
                if (this != std::addressof(other)) {
                    if (other) {
                        std::optional<T>::emplace(*other);
                    } else {
                        std::optional<T>::reset();
                    }
                }
                return *this;
            }

            constexpr movable_box& operator=(movable_box&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
            {
                if constexpr (std::movable<T>) {
                    std::optional<T>::operator=(std::move(other));
                    return *this;
                }
                if (this != std::addressof(other)) {
                    if (other) {
                        std::optional<T>::emplace(std::move(*other));
                    } else {
                        std::optional<T>::reset();
                    }
                }
                return *this;
            }
        };
    }

    template <class E>
    class foreign_vector {
        std::variant<std::span<E>, std::basic_string<std::remove_cv_t<E>>> _base;

    public:
        using value_type      = typename std::span<E>::value_type;
        using size_type       = typename std::span<E>::size_type;
        using difference_type = typename std::span<E>::difference_type;
        using pointer         = typename std::span<E>::pointer;
        using const_pointer   = typename std::span<E>::const_pointer;
        using reference       = typename std::span<E>::reference;
        using const_reference = typename std::span<E>::const_reference;

        using iterator               = pointer;
        using const_iterator         = const_pointer;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using span_type = std::variant_alternative_t<0, decltype(_base)>;
        using own_type  = std::variant_alternative_t<1, decltype(_base)>;

        foreign_vector() = default;

        template <class... T>
            requires(!std::same_as<std::tuple_element_t<0, std::tuple<T...>>, std::in_place_t> && std::constructible_from<span_type, T && ...>)
        explicit foreign_vector(T&&... args)
            : _base(std::in_place_type<span_type>, std::forward<T>(args)...)
        {}

        template <class... T>
            requires std::constructible_from<own_type, T&&...>
        explicit foreign_vector(std::in_place_t /*unused*/, T&&... args)
            : _base(std::in_place_type<own_type>, std::forward<T>(args)...)
        {}

        explicit foreign_vector(own_type own)
            : _base(std::move(own))
        {}

        explicit foreign_vector(span_type span)
            : _base(std::move(span))
        {}

        foreign_vector& operator=(own_type own)
        {
            _base = own;
            return *this;
        }

        foreign_vector& operator=(span_type span)
        {
            _base = span;
            return *this;
        }

        [[nodiscard]] bool foreign() const noexcept
        {
            return _base.index() == 0;
        }

        [[nodiscard]] auto&& own(this auto&& self)
        {
            return std::get<1>(std::forward<decltype(self)>(self)._base);
        }

        [[nodiscard]] auto&& span(this auto&& self)
        {
            return std::get<0>(std::forward<decltype(self)>(self)._base);
        }

        [[nodiscard]] iterator begin()
        {
            if (foreign()) {
                return std::get<0>(_base).data();
            }
            return std::get<1>(_base).data();
        }

        [[nodiscard]] iterator end()
        {
            if (foreign()) {
                return std::to_address(std::get<0>(_base).end());
            }
            return std::to_address(std::get<1>(_base).end());
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            if (foreign()) {
                return std::to_address(std::get<0>(_base).cbegin());
            }
            return std::to_address(std::get<1>(_base).cbegin());
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            if (foreign()) {
                return std::to_address(std::get<0>(_base).cend());
            }
            return std::to_address(std::get<1>(_base).cend());
        }

        [[nodiscard]] reverse_iterator rbegin()
        {
            return begin();
        }

        [[nodiscard]] reverse_iterator rend()
        {
            return end();
        }

        [[nodiscard]] const_reverse_iterator crbegin() const noexcept
        {
            return cbegin();
        }

        [[nodiscard]] const_reverse_iterator crend() const noexcept
        {
            return cend();
        }

        [[nodiscard]] reference front()
        {
            if (foreign()) {
                return std::get<0>(_base).front();
            }
            return std::get<1>(_base).front();
        }

        [[nodiscard]] const_reference front() const
        {
            if (foreign()) {
                return std::get<0>(_base).front();
            }
            return std::get<1>(_base).front();
        }

        [[nodiscard]] reference back()
        {
            if (foreign()) {
                return std::get<0>(_base).back();
            }
            return std::get<1>(_base).back();
        }

        [[nodiscard]] const_reference back() const
        {
            if (foreign()) {
                return std::get<0>(_base).back();
            }
            return std::get<1>(_base).back();
        }

        [[nodiscard]] size_type size() const noexcept
        {
            if (foreign()) {
                return std::get<0>(_base).size();
            }
            return std::get<1>(_base).size();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return size() == 0;
        }

        [[nodiscard]] pointer data()
        {
            return begin();
        }

        [[nodiscard]] const_pointer data() const
        {
            return cbegin();
        }

        [[nodiscard]] auto&& operator[](this auto&& self, size_type idx)
        {
            if (self.foreign()) {
                return std::get<0>(std::forward<decltype(self)>(self)._base)[idx];
            }
            return std::get<1>(std::forward<decltype(self)>(self)._base)[idx];
        }

        // ---
    };

    template <std::ranges::input_range V, std::indirect_unary_predicate<std::ranges::iterator_t<V>> Pred, class DeltaTime = std::remove_cvref_t<decltype(std::declval<std::ranges::range_value_t<V>>().delta_time())>>
        requires std::ranges::view<V> && std::is_object_v<Pred> && std::default_initializable<DeltaTime> && std::movable<DeltaTime> && requires(std::ranges::range_value_t<V> element, DeltaTime delta, DeltaTime& lval_delta) {
            {
                element.delta_time()
            } -> std::convertible_to<DeltaTime>;
            element.set_delta_time(delta);
            {
                lval_delta += delta
            } -> std::same_as<DeltaTime&>;
            {
                delta + delta
            } -> std::same_as<DeltaTime>;
        }
    class delta_timed_filter_view : public std::ranges::view_interface<delta_timed_filter_view<V, Pred>> {
        V                          _base;
        details::movable_box<Pred> _pred;
        class iterator {
            friend delta_timed_filter_view;
            using base_type = std::ranges::iterator_t<V>;
            delta_timed_filter_view* _view{};
            base_type                _base;
            DeltaTime                _dur;
            constexpr iterator(delta_timed_filter_view* parent, base_type it, DeltaTime init)
                : _view(parent)
                , _base(it)
                , _dur(init)
            {
            }

        public:
            using difference_type  = intptr_t;
            using value_type       = std::remove_cvref_t<std::ranges::range_value_t<V>>;
            using iterator_concept = std::conditional_t<std::ranges::forward_range<V>, std::forward_iterator_tag, std::input_iterator_tag>;

            constexpr iterator() = default;

            constexpr auto&& base(this auto&& self)
            {
                return std::forward<decltype(self)>(self)._base;
            }

            constexpr auto& operator++()
            {
                auto end = std::ranges::end(_view->_base);
                assert(_base != end);
                _dur = {};
                while (true) {
                    ++_base;
                    if (_base == end || (*_view->_pred)(*_base)) {
                        break;
                    }
                    _dur += (*_base).delta_time();
                }
                return *this;
            }

            constexpr auto operator++(int)
            {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            constexpr auto operator*() const
            {
                auto result = *_base;
                result.set_delta_time(result.delta_time() + _dur);
                return result;
            }

            constexpr bool operator==(const iterator& other) const
                requires std::equality_comparable<std::ranges::iterator_t<V>>
            {
                return _base == other._base;
            }

            constexpr bool operator==(std::default_sentinel_t) const
            {
                return _base == std::ranges::end(_view->_base);
            }

            friend constexpr decltype(auto) iter_move(const iterator& iter)
            {
                return *iter;
            }
        };

    public:
        constexpr delta_timed_filter_view()
            requires std::default_initializable<V> && std::default_initializable<Pred>
        = default;
        constexpr delta_timed_filter_view(V base, Pred pred)
            : _base(std::move(base))
            , _pred(std::move(pred))
        {}

        constexpr auto&& base(this auto&& self)
        {
            return std::forward<decltype(self)>(self)._base;
        }

        constexpr const Pred& pred() const
        {
            return *_pred;
        }

        constexpr iterator begin()
        {
            auto iter = iterator{this, std::ranges::begin(_base), {}};
            if (!(*_pred)(*iter.base())) {
                ++iter;
            }
            return iter;
        }

        constexpr std::default_sentinel_t end()
        {
            return {};
        }
    };
}