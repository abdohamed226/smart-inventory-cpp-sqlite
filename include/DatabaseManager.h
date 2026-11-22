#pragma once

#include <sqlite3.h>

#include <functional>
#include <string>

class DatabaseManager {
public:
    class TransactionGuard {
    public:
        explicit TransactionGuard(DatabaseManager& db);
        ~TransactionGuard();

        TransactionGuard(const TransactionGuard&) = delete;
        TransactionGuard& operator=(const TransactionGuard&) = delete;

        void commit();

    private:
        DatabaseManager& db_;
        bool active_{true};
    };

    using StatementBinder = std::function<void(sqlite3_stmt*)>;
    using RowReader = std::function<void(sqlite3_stmt*)>;

    explicit DatabaseManager(std::string dbPath);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    DatabaseManager(DatabaseManager&& other) noexcept;
    DatabaseManager& operator=(DatabaseManager&& other) noexcept;

    bool open();
    void close() noexcept;
    bool isOpen() const noexcept;
    const std::string& getDbPath() const noexcept;

    void initializeSchema();

    void execute(const std::string& sql);
    int executeUpdate(const std::string& sql, const StatementBinder& binder = nullptr);
    long long getLastInsertRowId() const noexcept;

    void query(const std::string& sql,
               const RowReader& reader,
               const StatementBinder& binder = nullptr);

    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction() noexcept;

private:
    std::string dbPath_;
    sqlite3* db_{nullptr};

    void ensureOpen() const;
    sqlite3_stmt* prepareStatement(const std::string& sql) const;
};