#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(first){
  BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(second){
  BOOST_CHECK(false);
}
