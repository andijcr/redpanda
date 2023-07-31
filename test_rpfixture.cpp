#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

struct Dummy{};
BOOST_FIXTURE_TEST_CASE(first, Dummy){
  BOOST_CHECK(true);
}

BOOST_FIXTURE_TEST_CASE(second, Dummy){
  BOOST_CHECK(false);
}
