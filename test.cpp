#define mr_assert(x)
#define DISALLOW_COPY_AND_ASSIGN(c_class)                    \
    c_class(const c_class &source) = delete;                                         \
    c_class &operator=(const c_class &source) = delete;

#include <stdio.h>
#include <iostream>

#include "promise.h"
#include "maybe.h"
#include "monad_impl.h"

#include "observable.h"

namespace std {
    std::string to_string(const std::string &f)
    {
        return f;
    }
}

int main()
{
    // test observable join
    if (true) {
        auto inner1 = observable<int>::create(1);
        auto inner2 = observable<int>::create(2);
        auto outer = observable<observable_ptr<int>>::create(inner1);

        auto result = monad<observable_ptr<int>>::join(outer);

        printf("original\n");
        auto handle = std::move(result->observe([](const auto &result) {
            printf("r = %d\n", result);
        }, true));
        printf("change to inner2\n");
        outer->push(inner2);
        printf("change inner2\n");
        inner2->push(3);
        printf("change inner1\n");
        inner1->push(4);
        printf("change to inner1\n");
        outer->push(inner1);
        printf("end\n");
    }

    // test sequence tree observable
    if (true) {
        auto a1 = observable<int>::create(1);
        auto a2 = observable<int>::create(1);
        auto a3 = observable<int>::create(1);
        auto a4 = observable<int>::create(1);
        auto a5 = observable<int>::create(1);
        auto a6 = observable<int>::create(1);
        auto a7 = observable<int>::create(1);
        auto a8 = observable<int>::create(1);
        std::vector<observable_ptr<int>> vec { a1, a2, a3, a4, a5, a6, a7, a8 };
        auto result = monad_sequence(vec) > [](const std::vector<int> &v) {
            int sum = 0;
            for (auto s : v) sum += s;
            return sum;
        };
        
        auto handle = std::move(result->observe([](const auto &result) {
            printf("result = %d\n", result);
        }));

        a1->push(2);
        a2->push(3);
        a3->push(4);
        a4->push(5);

        printf("=====\n");
    }

    // test sequence tree promise
    if (true) {
        using R = result<int, bool>;
        promise_ptr<R> p_11 = promise<R>::create(R(Ok, 11));
        promise_ptr<R> p_12 = promise<R>::create(R(Ok, 12));
        promise_ptr<R> p_13 = promise<R>::create(R(Ok, 13));
        promise_ptr<R> p_14 = promise<R>::create(R(Ok, 14));
        std::vector<promise_ptr<R>> vp { p_11, p_12, p_13, p_14 };
        auto promise_seq = monad_sequence(vp);
        promise_seq->then([](const auto &vec) {
            if (vec.is_ok()) {
                for (auto &e : vec.ok()) {
                    std::cout << e << ", ";
                }
                std::cout << std::endl;
            } else {
                std::cout << "is error" << std::endl;
            }
        });
    }

    // test sequence tree vector
    if (true) {
        std::vector<std::vector<int>> vec_of_vec = { {1, 2}, {3, 4, 5}, {10, 20, 30}};
        auto seq_result = monad_sequence(vec_of_vec);
        for (auto &vec : seq_result) {
            for (auto &e : vec) {
                std::cout << e << ", ";
            }
            std::cout << std::endl;
        }
    }

    /* */
    // test observable monad sequence
    if (true) {
        auto a = observable<int>::create(1);
        auto b = observable<int>::create(1);
        auto c = observable<int>::create(1);
        std::vector<observable_ptr<int>> vec {a, b, c};
        auto result = monad_sequence(vec) > [](const std::vector<int> &v) {
            int sum = 0;
            for (auto s : v) sum += s;
            return sum;
        };
        
        auto handle = std::move(result->observe([](const auto &result) {
            printf("result = %d\n", result);
        }));

        a->push(2);
        b->push(3);
        c->push(4);
        a->push(1);
    }

    // test promise result monad
    if (true) {
        printf("start test promise result\n");
        using R = result<int, std::string>;
        promise_ptr<R> a = promise<R>::create(R(Ok, 3));
        promise_ptr<R> b = promise<R>::create();
        promise_ptr<R> c = promise<R>::create();
        promise_ptr<R> d = promise<R>::create();

        auto result = std::make_tuple(a, b, c, d) > [](auto a, auto b, auto c, auto d) { return a + b + c + d; };
        result->then([](const auto &t) {
            if (t.is_ok()) {
                printf("ok %d\n", t.ok());
            } else if (t.is_error()) {
                printf("error %s\n", t.error().c_str());
            } else {
                printf("unreachable!\n");
            }
        });

        printf(">>>\n");
        c->resolve(R(Error, "error in c"));
        printf(">>>\n");
        d->resolve(R(Ok, 5));
        printf(">>>\n");
    }

    // test observable monad
    if (true) {
        auto a = observable<int>::create(4);
        auto b = observable<int>::create(5);
        auto c = observable<int>::create(6);

        auto result = std::make_tuple(a, b, c) > [](auto a, auto b, auto c) {
            return a*b+c;
        };

        auto handle = std::move(result->observe([](const auto &result) {
            printf("result = %d\n", result);
        }));
        a->push(4);
        a->push(5);
        b->push(7);
    }

    // test maybe monad
    if (true) {
        maybe<int> a = maybe<int>(4);
        maybe<int> b = maybe<int>(5);
        maybe<int> c = maybe<int>(6);
        auto result = std::make_tuple(a, b, c) >= [](auto a, auto b, auto c) {
            return maybe<int>(a * b + c);
        };
        if (result.has_data()) {
            std::cout << result.get() << std::endl;
        } else {
            std::cout << "no result" << std::endl;
        }
    }
    // test monad apply
    if (true) {
        promise_ptr<int> p1 = monad<promise_ptr<int>>::wrap(20);
        promise_ptr<std::string> p2 = monad<promise_ptr<std::string>>::wrap("3");
        promise_ptr<int> p3 = monad<promise_ptr<int>>::wrap(25);
        auto result = std::make_tuple(p1, p2, p3) > [](auto a, auto b, auto c) {
            return a * atoi(b.c_str()) + c;
        };
        result->then([](const auto &a) {
            std::cout << a << std::endl;
        });
        auto result2 = std::make_tuple(p1, p2, p3) >= [](auto a, auto b, auto c) {
            return monad<promise_ptr<int>>::wrap(a * atoi(b.c_str()) + c);
        };
        result2->then([](const int &a) {
            std::cout << a << std::endl;
        });
    }
    
    // test list monad apply
    if (true) {
        std::vector<int> va {1, 2, 3};
        std::vector<int> vb {10, 20, 30};
        std::vector<int> vc {100, 200, 300};

        auto result = std::make_tuple(va, vb, vc) > [](auto a, auto b, auto c) {
            return a + b + c;
        };

        for (auto &r : result) {
            std::cout << r << ", ";
        }
        std::cout << std::endl;
    }

    // test sequence promise
    if (true) {
        promise_ptr<int> p_11 = monad<promise_ptr<int>>::wrap(11);
        promise_ptr<int> p_12 = monad<promise_ptr<int>>::wrap(12);
        promise_ptr<int> p_13 = monad<promise_ptr<int>>::wrap(13);
        std::vector<promise_ptr<int>> vp { p_11, p_12, p_13 };
        auto promise_seq = monad_sequence(vp);
        promise_seq->then([](const auto &vec) {
            for (auto &e : vec) {
                std::cout << e << ", ";
            }
            std::cout << std::endl;
        });
    }

    // test sequence vector
    if (true) {
        std::vector<std::vector<int>> vec_of_vec = { {1, 2}, {3, 4, 5}, {10, 20, 30}};
        auto seq_result = monad_sequence(vec_of_vec);
        for (auto &vec : seq_result) {
            for (auto &e : vec) {
                std::cout << e << ", ";
            }
            std::cout << std::endl;
        }
    }

    // test base
    if (true) {
        promise_ptr<int> from = monad<promise_ptr<int>>::wrap(3);
        auto ret_promise = from >= [](const int &p) {
            promise_ptr<std::string> out = monad<promise_ptr<std::string>>::wrap(std::to_string(p) + "234");
            return out;
        };

        ret_promise->then([](const auto &p) {
            printf("ret: %s\n", std::to_string(p).c_str());
        });
    }
    //*/
    return 0;
}
