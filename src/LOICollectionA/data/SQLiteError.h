#pragma once

#include <string>

#include <sqlite3.h>

namespace SQLiteError {
    enum class SQLiteStorageErrorCode : int {
        Timeout = 1,
        TransactionCommitFailed = 2,
        TransactionRollbackFailed = 3
    };

    struct SQLiteErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "SQLiteError";
        }
        
        [[nodiscard]] std::string message(int ev) const override {
            return sqlite3_errstr(ev);
        }
    };

    struct SQLiteStorageErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "SQLiteStorageError";
        }
        
        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<SQLiteStorageErrorCode>(ev)) {
                case SQLiteStorageErrorCode::Timeout: return "get connection timeout";
                case SQLiteStorageErrorCode::TransactionCommitFailed: return "transaction commit failed";
                case SQLiteStorageErrorCode::TransactionRollbackFailed: return "transaction rollback failed";
            }

            return "Unknown";
        }
    };

    inline std::error_code makeErrorCode(int e) {
        static SQLiteErrorCategory cat;
        return std::error_code{ e, cat };
    }

    inline std::error_code makeErrorCode(SQLiteStorageErrorCode e) {
        static SQLiteStorageErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }
}
