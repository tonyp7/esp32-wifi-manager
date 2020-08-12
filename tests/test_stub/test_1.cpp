#include "gtest/gtest.h"
#include "json.h"

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestStub : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
    }

public:
    TestStub();

    ~TestStub() override;
};

TestStub::TestStub()
    : Test()
{
}

TestStub::~TestStub()
{
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestStub, test_1) // NOLINT
{
    ASSERT_EQ(false, json_print_string(nullptr, nullptr));
}
