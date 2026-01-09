#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <sqlite3.h>

#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

struct Task {
    int id;
    std::string title;
    std::string description;
    std::string status;
};

// Глобальные объекты
sqlite3* g_db = nullptr;
std::mutex db_mtx;

// Вспомогательные функции для работы с JSON
json::value task_to_json(const Task& t) {
    json::value j;
    j[U("id")] = json::value::number(t.id);
    j[U("title")] = json::value::string(utility::conversions::to_string_t(t.title));
    j[U("description")] = json::value::string(utility::conversions::to_string_t(t.description));
    j[U("status")] = json::value::string(utility::conversions::to_string_t(t.status));
    return j;
}

// Инициализация БД и создание таблицы
bool db_init(const std::string& filename = "tasks.db") {
    std::lock_guard<std::mutex> lk(db_mtx);
    if (sqlite3_open(filename.c_str(), &g_db) != SQLITE_OK) {
        std::cerr << "Cannot open DB: " << sqlite3_errmsg(g_db) << std::endl;
        return false;
    }
    const char* sql_create =
        "CREATE TABLE IF NOT EXISTS tasks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "title TEXT NOT NULL,"
        "description TEXT,"
        "status TEXT NOT NULL"
        ");";
    char* errmsg = nullptr;
    if (sqlite3_exec(g_db, sql_create, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::cerr << "Create table failed: " << (errmsg ? errmsg : "") << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

void db_close() {
    std::lock_guard<std::mutex> lk(db_mtx);
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

// CRUD операции
bool db_insert_task(const Task& t, Task& out) {
    std::lock_guard<std::mutex> lk(db_mtx);
    const char* sql = "INSERT INTO tasks(title,description,status) VALUES(?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, t.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.status.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int id = static_cast<int>(sqlite3_last_insert_rowid(g_db));
        out = t;
        out.id = id;
        ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Task> db_get_all_tasks() {
    std::vector<Task> res;
    std::lock_guard<std::mutex> lk(db_mtx);
    const char* sql = "SELECT id, title, description, status FROM tasks;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Task t;
        t.id = sqlite3_column_int(stmt, 0);
        const unsigned char* s1 = sqlite3_column_text(stmt, 1);
        const unsigned char* s2 = sqlite3_column_text(stmt, 2);
        const unsigned char* s3 = sqlite3_column_text(stmt, 3);
        t.title = s1 ? reinterpret_cast<const char*>(s1) : "";
        t.description = s2 ? reinterpret_cast<const char*>(s2) : "";
        t.status = s3 ? reinterpret_cast<const char*>(s3) : "";
        res.push_back(t);
    }
    sqlite3_finalize(stmt);
    return res;
}

bool db_get_task(int id, Task& out) {
    std::lock_guard<std::mutex> lk(db_mtx);
    const char* sql = "SELECT id, title, description, status FROM tasks WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.id = sqlite3_column_int(stmt, 0);
        const unsigned char* s1 = sqlite3_column_text(stmt, 1);
        const unsigned char* s2 = sqlite3_column_text(stmt, 2);
        const unsigned char* s3 = sqlite3_column_text(stmt, 3);
        out.title = s1 ? reinterpret_cast<const char*>(s1) : "";
        out.description = s2 ? reinterpret_cast<const char*>(s2) : "";
        out.status = s3 ? reinterpret_cast<const char*>(s3) : "";
        ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
}

bool db_update_task(int id, const Task& t) {
    std::lock_guard<std::mutex> lk(db_mtx);
    const char* sql = "UPDATE tasks SET title = ?, description = ?, status = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, t.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, id);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_DONE) ok = (sqlite3_changes(g_db) > 0);
    sqlite3_finalize(stmt);
    return ok;
}

bool db_delete_task(int id) {
    std::lock_guard<std::mutex> lk(db_mtx);
    const char* sql = "DELETE FROM tasks WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_DONE) ok = (sqlite3_changes(g_db) > 0);
    sqlite3_finalize(stmt);
    return ok;
}

// HTTP handlers
void handle_get_all(http_request req) {
    std::cout << "GET /tasks\n";
    auto tasks = db_get_all_tasks();
    json::value arr = json::value::array();
    for (size_t i = 0; i < tasks.size(); ++i) arr[i] = task_to_json(tasks[i]);
    req.reply(status_codes::OK, arr);
}

void handle_post(http_request req) {
    std::cout << "POST /tasks\n";
    req.extract_json().then([req](pplx::task<json::value> t) mutable {
        try {
            auto j = t.get();
            Task nt;
            if (j.has_field(U("title"))) nt.title = utility::conversions::to_utf8string(j.at(U("title")).as_string());
            if (j.has_field(U("description"))) nt.description = utility::conversions::to_utf8string(j.at(U("description")).as_string());
            if (j.has_field(U("status"))) nt.status = utility::conversions::to_utf8string(j.at(U("status")).as_string());
            Task created;
            if (db_insert_task(nt, created)) {
                req.reply(status_codes::Created, task_to_json(created));
            }
            else {
                req.reply(status_codes::InternalError);
            }
        }
        catch (...) {
            req.reply(status_codes::BadRequest);
        }
        });
}

void handle_request(http_request req) {
    std::cout << utility::conversions::to_utf8string(req.method()) << " "
        << utility::conversions::to_utf8string(req.relative_uri().path()) << std::endl;

    auto path = uri::split_path(uri::decode(req.relative_uri().path()));
    if (req.method() == methods::GET && path.empty()) return handle_get_all(req);
    if (req.method() == methods::POST && path.empty()) return handle_post(req);

    if (!path.empty()) {
        int id = 0;
        try { id = std::stoi(path[0]); }
        catch (...) { req.reply(status_codes::BadRequest); return; }

        if (req.method() == methods::GET) {
            Task t;
            if (db_get_task(id, t)) req.reply(status_codes::OK, task_to_json(t));
            else req.reply(status_codes::NotFound);
            return;
        }

        if (req.method() == methods::DEL) {
            if (db_delete_task(id)) req.reply(status_codes::OK);
            else req.reply(status_codes::NotFound);
            return;
        }

        if (req.method() == methods::PUT || req.method() == methods::PATCH) {
            req.extract_json().then([req, id](pplx::task<json::value> tj) mutable {
                try {
                    auto j = tj.get();
                    Task cur;
                    if (!db_get_task(id, cur)) { req.reply(status_codes::NotFound); return; }

                    // Обновляем только пришедшие поля (PATCH-подход)
                    if (j.has_field(U("title"))) cur.title = utility::conversions::to_utf8string(j.at(U("title")).as_string());
                    if (j.has_field(U("description"))) cur.description = utility::conversions::to_utf8string(j.at(U("description")).as_string());
                    if (j.has_field(U("status"))) cur.status = utility::conversions::to_utf8string(j.at(U("status")).as_string());

                    if (db_update_task(id, cur)) req.reply(status_codes::OK, task_to_json(cur));
                    else req.reply(status_codes::InternalError);
                }
                catch (...) {
                    req.reply(status_codes::BadRequest);
                }
                });
            return;
        }
    }

    req.reply(status_codes::NotFound);
}

int main() {
    if (!db_init("tasks.db")) {
        std::cerr << "DB init failed, exiting\n";
        return 1;
    }

    http_listener listener(U("http://localhost:8080/tasks"));
    listener.support(handle_request);

    try {
        listener.open().wait();
        std::cout << "Listening...\n";
        std::string line;
        std::getline(std::cin, line); // Enter чтобы остановить
        listener.close().wait();
    }
    catch (const std::exception& e) {
        std::cerr << "Listener error: " << e.what() << std::endl;
    }

    db_close();
    return 0;
}
