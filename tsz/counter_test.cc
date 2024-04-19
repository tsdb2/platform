#include "tsz/counter.h"

#include <string>

#include "common/no_destructor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/base.h"
#include "tsz/counter.h"

namespace {

using tsdb2::common::NoDestructor;

char constexpr kLoremName[] = "lorem";
char constexpr kFooName[] = "foo";

NoDestructor<tsz::Counter<tsz::EntityLabels<tsz::Field<std::string, kLoremName>>,
                          tsz::MetricFields<tsz::Field<int, kFooName>>>>
    counter1{"/lorem/ipsum"};

NoDestructor<
    tsz::Counter<tsz::EntityLabels<tsz::Field<std::string>>, tsz::MetricFields<tsz::Field<int>>>>
    counter2{"/lorem/ipsum", "lorem", "foo"};

// TODO

}  // namespace
