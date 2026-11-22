#include "DatabaseManager.h"

#include <stdexcept>
#include <utility>

namespace {

void finalizeStatement(sqlite3_stmt* stmt) noexcept {
    if (stmt != nullptr) {
        sqlite3_finalize(stmt);
    }
}

} // namespace

DatabaseManager::TransactionGuard::TransactionGuard(DatabaseManager& db) : db_(db) {
    db_.beginTransaction();
}

DatabaseManager::TransactionGuard::~TransactionGuard() {
    if (active_) {
        db_.rollbackTransaction();
    }
}

void DatabaseManager::TransactionGuard::commit() {
    if (!active_) {
        return;
    }
    db_.commitTransaction();
    active_ = false;
}

DatabaseManager::DatabaseManager(std::string dbPath) : dbPath_(std::move(dbPath)) {}

DatabaseManager::~DatabaseManager() {
    close();
}

DatabaseManager::DatabaseManager(DatabaseManager&& other) noexcept
    : dbPath_(std::move(other.dbPath_)), db_(other.db_) {
    other.db_ = nullptr;
}

DatabaseManager& DatabaseManager::operator=(DatabaseManager&& other) noexcept {
    if (this != &other) {
        close();
        dbPath_ = std::move(other.dbPath_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

bool DatabaseManager::open() {
    if (db_ != nullptr) {
        return true;
    }

    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string errMsg = (db_ != nullptr) ? sqlite3_errmsg(db_) : "Unknown SQLite error";
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error("Failed to open SQLite database '" + dbPath_ + "': " + errMsg);
    }

    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA busy_timeout = 5000;");
    return true;
}

void DatabaseManager::close() noexcept {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool DatabaseManager::isOpen() const noexcept {
    return db_ != nullptr;
}

const std::string& DatabaseManager::getDbPath() const noexcept {
    return dbPath_;
}

void DatabaseManager::ensureOpen() const {
    if (db_ == nullptr) {
        throw std::runtime_error("Database connection is not open.");
    }
}

sqlite3_stmt* DatabaseManager::prepareStatement(const std::string& sql) const {
    ensureOpen();

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string errMsg = sqlite3_errmsg(db_);
        throw std::runtime_error("Failed to prepare SQL statement: " + errMsg);
    }
    return stmt;
}

void DatabaseManager::initializeSchema() {
    ensureOpen();

    TransactionGuard tx(*this);

    execute(R"(
        CREATE TABLE IF NOT EXISTS Categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE CHECK (length(trim(name)) > 0),
            description TEXT
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS Products (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL CHECK (length(trim(name)) > 0),
            category_id INTEGER,
            price REAL NOT NULL CHECK (price >= 0.0),
            quantity INTEGER NOT NULL DEFAULT 0 CHECK (quantity >= 0),
            min_stock_level INTEGER NOT NULL DEFAULT 5 CHECK (min_stock_level >= 0),
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (category_id) REFERENCES Categories(id)
                ON UPDATE CASCADE
                ON DELETE SET NULL
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS Sales (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_date TEXT NOT NULL DEFAULT (datetime('now')),
            total_amount REAL NOT NULL CHECK (total_amount >= 0.0)
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS SaleItems (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_id INTEGER NOT NULL,
            product_id INTEGER,
            quantity INTEGER NOT NULL CHECK (quantity > 0),
            unit_price REAL NOT NULL CHECK (unit_price >= 0.0),
            FOREIGN KEY (sale_id) REFERENCES Sales(id)
                ON UPDATE CASCADE
                ON DELETE CASCADE,
            FOREIGN KEY (product_id) REFERENCES Products(id)
                ON UPDATE CASCADE
                ON DELETE SET NULL
        );
    )");

    execute("CREATE INDEX IF NOT EXISTS idx_products_name ON Products(name);");
    execute("CREATE INDEX IF NOT EXISTS idx_products_category ON Products(category_id);");
    execute("CREATE INDEX IF NOT EXISTS idx_sale_items_sale ON SaleItems(sale_id);");
    execute("CREATE INDEX IF NOT EXISTS idx_sale_items_product ON SaleItems(product_id);");

    tx.commit();
}

void DatabaseManager::execute(const std::string& sql) {
    ensureOpen();

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string message = (errMsg != nullptr) ? errMsg : sqlite3_errmsg(db_);
        if (errMsg != nullptr) {
            sqlite3_free(errMsg);
        }
        throw std::runtime_error("SQLite execute error: " + message);
    }
}

int DatabaseManager::executeUpdate(const std::string& sql, const StatementBinder& binder) {
    sqlite3_stmt* stmt = prepareStatement(sql);

    try {
        if (binder) {
            binder(stmt);
        }

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::string errMsg = sqlite3_errmsg(db_);
            finalizeStatement(stmt);
            throw std::runtime_error("SQLite step error: " + errMsg);
        }

        finalizeStatement(stmt);
        return sqlite3_changes(db_);
    } catch (...) {
        finalizeStatement(stmt);
        throw;
    }
}

long long DatabaseManager::getLastInsertRowId() const noexcept {
    if (db_ == nullptr) {
        return -1;
    }
    return sqlite3_last_insert_rowid(db_);
}

void DatabaseManager::query(const std::string& sql,
                           const RowReader& reader,
                           const StatementBinder& binder) {
    sqlite3_stmt* stmt = prepareStatement(sql);

    try {
        if (binder) {
            binder(stmt);
        }

        while (true) {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                if (reader) {
                    reader(stmt);
                }
            } else if (rc == SQLITE_DONE) {
                break;
            } else {
                std::string errMsg = sqlite3_errmsg(db_);
                throw std::runtime_error("SQLite query step error: " + errMsg);
            }
        }

        finalizeStatement(stmt);
    } catch (...) {
        finalizeStatement(stmt);
        throw;
    }
}

void DatabaseManager::beginTransaction() {
    execute("BEGIN IMMEDIATE TRANSACTION;");
}

void DatabaseManager::commitTransaction() {
    execute("COMMIT;");
}

void DatabaseManager::rollbackTransaction() noexcept {
    if (db_ != nullptr) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}