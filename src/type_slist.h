#pragma once

namespace mcl
{

    template<class... types>
    struct type_slist;

    template<>
    struct type_slist<>
    {
        template<class type, class next>
        struct node
        {
            typedef type deref;
            typedef next next;
        };
        struct nil_type
        {
        };
        typedef nil_type begin, end;
    };

    template<class first_type, class... other_types>
    struct type_slist<first_type, other_types...> : public type_slist<>
    {
        typedef node<first_type, typename type_slist<other_types...>::begin> begin;
    };

}
