#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "LOICollectionA/base/ServiceContainer.h"

struct ITestSvc {
    virtual ~ITestSvc() = default;
    [[nodiscard]] virtual int value() const = 0;
};

struct TestSvcImpl : ITestSvc {
    int m_val = 0;
    explicit TestSvcImpl(int v) : m_val(v) {}
    [[nodiscard]] int value() const override { return m_val; }
};

TEST(ServiceContainerTest, RegisterAndGet) {
    ServiceContainer container;

    auto svc = std::make_shared<TestSvcImpl>(42);

    container.registerInstance<ITestSvc>(svc);

    auto retrieved = container.getService<ITestSvc>();

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->value(), 42);
}

TEST(ServiceContainerTest, NamedInstances) {
    ServiceContainer container;
    auto svc1 = std::make_shared<TestSvcImpl>(1);
    auto svc2 = std::make_shared<TestSvcImpl>(2);
    
    container.registerInstance<ITestSvc>(svc1, "first");
    container.registerInstance<ITestSvc>(svc2, "second");

    EXPECT_EQ(container.getService<ITestSvc>("first")->value(), 1);
    EXPECT_EQ(container.getService<ITestSvc>("second")->value(), 2);
}

TEST(ServiceContainerTest, GetMissingReturnsNull) {
    ServiceContainer container;

    EXPECT_EQ(container.getService<ITestSvc>(), nullptr);
    EXPECT_EQ(container.getService<ITestSvc>("missing"), nullptr);
}

TEST(ServiceContainerTest, IsRegistered) {
    ServiceContainer container;

    EXPECT_FALSE(container.isRegistered<ITestSvc>());

    container.registerInstance<ITestSvc>(std::make_shared<TestSvcImpl>(0));

    EXPECT_TRUE(container.isRegistered<ITestSvc>());
    EXPECT_FALSE(container.isRegistered<ITestSvc>("other"));
}

TEST(ServiceContainerTest, GetServiceNames) {
    ServiceContainer container;

    container.registerInstance<ITestSvc>(std::make_shared<TestSvcImpl>(0), "a");
    container.registerInstance<ITestSvc>(std::make_shared<TestSvcImpl>(0), "b");

    auto names = container.getServiceNames<ITestSvc>();

    EXPECT_EQ(names.size(), 2);

    EXPECT_NE(std::find(names.begin(), names.end(), "a"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "b"), names.end());
}

TEST(ServiceContainerTest, RemoveService) {
    ServiceContainer container;

    container.registerInstance<ITestSvc>(std::make_shared<TestSvcImpl>(0), "x");
    container.removeService<ITestSvc>("x");

    EXPECT_FALSE(container.isRegistered<ITestSvc>("x"));
}

TEST(ServiceContainerTest, ClearAll) {
    ServiceContainer container;

    container.registerInstance<ITestSvc>(std::make_shared<TestSvcImpl>(7));
    container.clear();
    
    EXPECT_FALSE(container.isRegistered<ITestSvc>());
}
