#include <gtest/gtest.h>

#include <string>

#include "LOICollectionA/frontend/DiagnosticEngine.h"

using namespace LOICollection::frontend;

TEST(DiagnosticTest, ErrorsAndWarnings) {
    DiagnosticEngine diag;

    EXPECT_FALSE(diag.hasErrors());
    EXPECT_FALSE(diag.hasWarnings());
    EXPECT_EQ(diag.getErrorMessage(), "");

    diag.addWarning({ 1, 5, 4 }, "be careful");
    EXPECT_TRUE(diag.hasWarnings());
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_NE(diag.getWarningMessage().find("be careful"), std::string::npos);
    EXPECT_NE(diag.getWarningMessage().find("line 1"), std::string::npos);

    diag.addError({ 2, 3, 10 }, "boom");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_NE(diag.getErrorMessage().find("boom"), std::string::npos);
    EXPECT_NE(diag.getErrorMessage().find("line 2"), std::string::npos);
    EXPECT_NE(diag.getErrorMessage().find("col 3"), std::string::npos);
}

TEST(DiagnosticTest, OriginLocationIsOmitted) {
    DiagnosticEngine diag;

    diag.addError({}, "no location");
    EXPECT_EQ(diag.getErrorMessage(), "no location");
}

TEST(DiagnosticTest, Clear) {
    DiagnosticEngine diag;

    diag.addError({ 1, 1, 0 }, "e1");
    diag.addNote({ 1, 2, 1 }, "note");

    EXPECT_TRUE(diag.hasErrors());

    diag.clear();

    EXPECT_FALSE(diag.hasErrors());
    EXPECT_FALSE(diag.hasWarnings());
    EXPECT_EQ(diag.getErrorMessage(), "");
}
