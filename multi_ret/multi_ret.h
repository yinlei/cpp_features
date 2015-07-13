#ifndef MULTI_RET_H
#define MULTI_RET_H

#include <tuple>
#include <utility>

namespace multi_ret {

    template <typename ... Args>
    struct mr : std::tuple<Args&...>
    {
        typedef std::tuple<Args&...> base_t;

        explicit mr(Args& ... args) : base_t(std::tie(args...)) {}

        template <int Idx, int Count>
            struct TupleSet
            {
                template <typename ... A, typename Iterator>
                void set(std::tuple<A...> & output, Iterator & input) {
                    std::get<Count - Idx>(output) = *input++;
                    TupleSet<Idx - 1, Count>().set(output, input);
                }
            };

        template <int Count>
            struct TupleSet<1, Count>
            {
                template <typename ... A, typename Iterator>
                void set(std::tuple<A...> & output, Iterator & input) {
                    std::get<Count - 1>(output) = *input++;
                }
            };

        template <typename T>
            struct is_container
            {
                template <typename U>
                    static char foo(U*, typename U::iterator* = NULL);

                template <typename U>
                    static int foo(...);

                static const bool value = (sizeof(foo<T>(NULL)) == sizeof(char));
            };

        template <typename T>
            typename std::enable_if<is_container<typename std::remove_reference<T>::type>::value>::type
        operator=(T && c)
        {
            auto it = c.begin();
            TupleSet<sizeof...(Args), sizeof...(Args)>().set(static_cast<base_t&>(*this), it);
        }

        
        template <typename T>
            typename std::enable_if<!is_container<typename std::remove_reference<T>::type>::value>::type
        operator=(T && t)
        {
            static_cast<base_t&>(*this) = std::forward<T>(t);
        }
        
        template <typename First, typename Second>
        void operator=(std::pair<First, Second>&& pr)
        {
            static_assert(sizeof...(Args) == 2, "");
            std::get<0>(static_cast<base_t&>(*this)) = pr.first;
            std::get<1>(static_cast<base_t&>(*this)) = pr.second;
        }
    };

    template <typename ... Args>
        mr<Args&...> make_mr(Args&& ... args)
        {
            return mr<Args&...>(args...);
        }
}

#ifndef MR
#define MR multi_ret::make_mr
#endif

#endif
