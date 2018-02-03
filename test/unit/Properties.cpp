#include "gtest/gtest.h"
#include <iostream>


#include "Properties.h"

using namespace std;

TEST(Properties, Main) {
    Properties props{"config.props"};
    clog << props.getString("a", "") << endl;
    clog << props.getString("b", "") << endl;
    clog << props.getInt("b") << endl;
}
