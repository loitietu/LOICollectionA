#include <gtest/gtest.h>

#include "LOICollectionA/base/Wrapper.h"

TEST(ReadOnlyWrapperTest, GetAndConversion) {
    ReadOnlyWrapper<int> wrapper(42);

    const int& ref = wrapper.get();
    
    EXPECT_EQ(ref, 42);
    
    int copy = wrapper;
    
    EXPECT_EQ(copy, 42);
}

TEST(ReadOnlyWrapperTest, ArrowAndDereference) {
    struct Point { int x, y; };
    
    ReadOnlyWrapper<Point> wrapper(Point{3, 4});
    
    EXPECT_EQ(wrapper->x, 3);
    EXPECT_EQ((*wrapper).y, 4);
}

TEST(ReadOnlyWrapperTest, SubscriptOperator) {
    ReadOnlyWrapper<std::vector<int>> wrapper(std::vector<int>{10, 20, 30});
    
    EXPECT_EQ(wrapper[0], 10);
    EXPECT_EQ(wrapper[2], 30);
}

TEST(ReadOnlyWrapperTest, CallOperator) {
    auto lambda = [](int a, int b) -> int { return a + b; };
    
    ReadOnlyWrapper<decltype(lambda)> wrapper(std::move(lambda));
    
    EXPECT_EQ(wrapper(2, 3), 5);
}

TEST(ReadOnlyWrapperTest, ComparisonOperators) {
    ReadOnlyWrapper<int> w1(10);
    ReadOnlyWrapper<int> w2(10);
    ReadOnlyWrapper<int> w3(20);

    EXPECT_TRUE(w1 == w2);
    EXPECT_FALSE(w1 == w3);
    EXPECT_TRUE(w1 <=> w2 == std::strong_ordering::equal);
}

TEST(ReadOnlyWrapperTest, MoveConstruction) {
    ReadOnlyWrapper<std::string> original(std::string("hello"));
    ReadOnlyWrapper<std::string> moved(std::move(original));
    
    EXPECT_EQ(moved.get(), "hello");
}