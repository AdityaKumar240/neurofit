/*
 * ============================================================
 *  FitForge C++ Backend
 *  Framework  : Crow (https://crowcrow.net)
 *  Auth       : Supabase JWT (RS256) via jwt-cpp
 *  DB Client  : libpqxx (PostgreSQL — Supabase's underlying DB)
 *
 *  Build deps (install via vcpkg or apt):
 *    crow        - HTTP micro-framework
 *    libpqxx     - PostgreSQL C++ client
 *    jwt-cpp     - JWT decode / verify
 *    nlohmann_json - JSON (bundled with Crow)
 *    openssl     - SSL / JWT signature
 *
 *  Build command:
 *    g++ -std=c++17 main.cpp \
 *        -I/usr/local/include \
 *        -lpqxx -lpq -lssl -lcrypto -lpthread \
 *        -o fitforge_backend
 *
 *  Run:
 *    ./fitforge_backend
 * ============================================================
 */

#include "crow.h"               // Crow single-header (download from GitHub)
#include <pqxx/pqxx>            // PostgreSQL client
#include <jwt-cpp/jwt.h>        // JWT decode/verify
#include <nlohmann/json.hpp>    // JSON (comes with Crow)
#include <iostream>
#include <string>
#include <stdexcept>

using json = nlohmann::json;

// ─── Configuration (set via environment variables) ───────────
const std::string DB_CONN_STR  = std::getenv("DATABASE_URL")     ? std::getenv("DATABASE_URL")     : "";
const std::string JWT_SECRET   = std::getenv("SUPABASE_JWT_SECRET") ? std::getenv("SUPABASE_JWT_SECRET") : "";
const int         SERVER_PORT  = 8080;

// ─── JWT Auth Helper ─────────────────────────────────────────
// Returns the user UUID from the JWT, or "" on failure.
std::string verifyJWT(const std::string& authHeader) {
    if (authHeader.empty() || authHeader.size() < 8) return "";
    try {
        // Header format: "Bearer <token>"
        std::string token = authHeader.substr(7);

        auto decoded = jwt::decode(token);

        // Verify signature using the Supabase JWT secret
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{ JWT_SECRET })
            .with_issuer("https://YOUR_PROJECT_ID.supabase.co/auth/v1");

        verifier.verify(decoded);

        return decoded.get_subject(); // This is the user UUID (sub claim)
    } catch (const std::exception& e) {
        std::cerr << "[JWT] Verification failed: " << e.what() << "\n";
        return "";
    }
}

// ─── DB Helper ───────────────────────────────────────────────
pqxx::connection* getConn() {
    static pqxx::connection* conn = nullptr;
    if (!conn) conn = new pqxx::connection(DB_CONN_STR);
    return conn;
}

// ─── Middleware: require auth ────────────────────────────────
#define REQUIRE_AUTH(req, res, userId)                              \
    std::string userId = verifyJWT(req.get_header_value("Authorization")); \
    if (userId.empty()) {                                           \
        res.code = 401;                                             \
        res.write("{\"error\":\"Unauthorized\"}");                  \
        res.end(); return;                                          \
    }

// ─── CORS Helper ─────────────────────────────────────────────
void setCORS(crow::response& res) {
    res.add_header("Access-Control-Allow-Origin",  "*");
    res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.add_header("Content-Type", "application/json");
}

// ============================================================
//  MAIN
// ============================================================
int main() {
    crow::SimpleApp app;

    // ── Health check ────────────────────────────────────────
    CROW_ROUTE(app, "/health").methods("GET"_method)([](){
        return crow::response(200, R"({"status":"ok","service":"FitForge C++ Backend"})");
    });

    // ── CORS preflight ───────────────────────────────────────
    CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)
    ([](const crow::request&, crow::response& res, std::string) {
        setCORS(res);
        res.code = 204;
        res.end();
    });

    // ============================================================
    //  WORKOUTS
    // ============================================================

    // GET /api/workouts  – list user's workouts
    CROW_ROUTE(app, "/api/workouts").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            auto rows = txn.exec_params(
                "SELECT id, name, type, duration, calories, intensity, notes, created_at "
                "FROM workouts WHERE user_id = $1 ORDER BY created_at DESC LIMIT 50",
                userId);
            json arr = json::array();
            for (auto& r : rows) {
                arr.push_back({
                    {"id",        r[0].as<int>()},
                    {"name",      r[1].as<std::string>()},
                    {"type",      r[2].as<std::string>()},
                    {"duration",  r[3].as<int>()},
                    {"calories",  r[4].as<int>()},
                    {"intensity", r[5].as<std::string>()},
                    {"notes",     r[6].is_null() ? "" : r[6].as<std::string>()},
                    {"date",      r[7].as<std::string>()}
                });
            }
            res.code = 200;
            res.write(arr.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(json{{"error", e.what()}}.dump());
        }
        res.end();
    });

    // POST /api/workouts  – log a workout
    CROW_ROUTE(app, "/api/workouts").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            auto body     = json::parse(req.body);
            pqxx::work txn(*getConn());
            auto row = txn.exec_params1(
                "INSERT INTO workouts(user_id, name, type, duration, calories, intensity, notes) "
                "VALUES($1,$2,$3,$4,$5,$6,$7) RETURNING id",
                userId,
                body.value("name",      ""),
                body.value("type",      "strength"),
                body.value("duration",  0),
                body.value("calories",  0),
                body.value("intensity", "medium"),
                body.value("notes",     ""));
            txn.commit();
            res.code = 201;
            res.write(json{{"id", row[0].as<int>()}, {"status","created"}}.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(json{{"error", e.what()}}.dump());
        }
        res.end();
    });

    // DELETE /api/workouts/:id
    CROW_ROUTE(app, "/api/workouts/<int>").methods("DELETE"_method)
    ([](const crow::request& req, crow::response& res, int id) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            txn.exec_params("DELETE FROM workouts WHERE id=$1 AND user_id=$2", id, userId);
            txn.commit();
            res.code = 200;
            res.write(R"({"status":"deleted"})");
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(json{{"error", e.what()}}.dump());
        }
        res.end();
    });

    // ============================================================
    //  MEALS
    // ============================================================

    CROW_ROUTE(app, "/api/meals").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            auto rows = txn.exec_params(
                "SELECT id, name, type, calories, protein, carbs, fat, created_at "
                "FROM meals WHERE user_id=$1 AND created_at::date = CURRENT_DATE ORDER BY created_at DESC",
                userId);
            json arr = json::array();
            for (auto& r : rows) {
                arr.push_back({
                    {"id",       r[0].as<int>()},
                    {"name",     r[1].as<std::string>()},
                    {"type",     r[2].as<std::string>()},
                    {"calories", r[3].as<int>()},
                    {"protein",  r[4].as<int>()},
                    {"carbs",    r[5].as<int>()},
                    {"fat",      r[6].as<int>()},
                    {"date",     r[7].as<std::string>()}
                });
            }
            res.code = 200;
            res.write(arr.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(json{{"error", e.what()}}.dump());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/meals").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            auto body = json::parse(req.body);
            pqxx::work txn(*getConn());
            auto row = txn.exec_params1(
                "INSERT INTO meals(user_id, name, type, calories, protein, carbs, fat) "
                "VALUES($1,$2,$3,$4,$5,$6,$7) RETURNING id",
                userId,
                body.value("name",""),
                body.value("type","lunch"),
                body.value("calories",0),
                body.value("protein",0),
                body.value("carbs",0),
                body.value("fat",0));
            txn.commit();
            res.code = 201;
            res.write(json{{"id", row[0].as<int>()}, {"status","created"}}.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(json{{"error", e.what()}}.dump());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/meals/<int>").methods("DELETE"_method)
    ([](const crow::request& req, crow::response& res, int id) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            txn.exec_params("DELETE FROM meals WHERE id=$1 AND user_id=$2", id, userId);
            txn.commit();
            res.code = 200; res.write(R"({"status":"deleted"})");
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    // ============================================================
    //  GOALS
    // ============================================================

    CROW_ROUTE(app, "/api/goals").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            auto body = json::parse(req.body);
            pqxx::work txn(*getConn());
            auto row = txn.exec_params1(
                "INSERT INTO goals(user_id, title, category, target_date, current_val, target_val, unit) "
                "VALUES($1,$2,$3,$4,$5,$6,$7) RETURNING id",
                userId,
                body.value("title",""),
                body.value("category",""),
                body.value("targetDate",""),
                body.value("current",0.0),
                body.value("target",100.0),
                body.value("unit",""));
            txn.commit();
            res.code = 201;
            res.write(json{{"id",row[0].as<int>()},{"status","created"}}.dump());
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/goals/<int>").methods("DELETE"_method)
    ([](const crow::request& req, crow::response& res, int id) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            txn.exec_params("DELETE FROM goals WHERE id=$1 AND user_id=$2", id, userId);
            txn.commit();
            res.code = 200; res.write(R"({"status":"deleted"})");
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    // ============================================================
    //  METRICS
    // ============================================================

    CROW_ROUTE(app, "/api/metrics").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            auto body = json::parse(req.body);
            pqxx::work txn(*getConn());
            // Upsert: update if row exists for today, else insert
            txn.exec_params(
                "INSERT INTO metrics(user_id, weight, height, body_fat, muscle_mass) "
                "VALUES($1,$2,$3,$4,$5) "
                "ON CONFLICT(user_id, recorded_date) DO UPDATE "
                "SET weight=$2, height=$3, body_fat=$4, muscle_mass=$5",
                userId,
                body.value("weight",  0.0),
                body.value("height",  0.0),
                body.value("fat",     0.0),
                body.value("muscle",  0.0));
            txn.commit();
            res.code = 200; res.write(R"({"status":"saved"})");
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    // ============================================================
    //  SCHEDULES
    // ============================================================

    CROW_ROUTE(app, "/api/schedules").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            auto rows = txn.exec_params(
                "SELECT id, name, day, time, duration FROM schedules WHERE user_id=$1 ORDER BY day",
                userId);
            json arr = json::array();
            for (auto& r : rows)
                arr.push_back({{"id",r[0].as<int>()},{"name",r[1].as<std::string>()},
                               {"day",r[2].as<std::string>()},{"time",r[3].as<std::string>()},
                               {"duration",r[4].as<int>()}});
            res.code = 200; res.write(arr.dump());
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/schedules").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            auto body = json::parse(req.body);
            pqxx::work txn(*getConn());
            auto row = txn.exec_params1(
                "INSERT INTO schedules(user_id, name, day, time, duration) "
                "VALUES($1,$2,$3,$4,$5) RETURNING id",
                userId,
                body.value("name",""),
                body.value("day","Monday"),
                body.value("time","07:00"),
                body.value("duration",60));
            txn.commit();
            res.code = 201;
            res.write(json{{"id",row[0].as<int>()},{"status","created"}}.dump());
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/schedules/<int>").methods("DELETE"_method)
    ([](const crow::request& req, crow::response& res, int id) {
        REQUIRE_AUTH(req, res, userId);
        setCORS(res);
        try {
            pqxx::work txn(*getConn());
            txn.exec_params("DELETE FROM schedules WHERE id=$1 AND user_id=$2", id, userId);
            txn.commit();
            res.code = 200; res.write(R"({"status":"deleted"})");
        } catch (const std::exception& e) {
            res.code = 500; res.write(json{{"error",e.what()}}.dump());
        }
        res.end();
    });

    // ── Start server ─────────────────────────────────────────
    std::cout << "[FitForge] C++ backend running on port " << SERVER_PORT << "\n";
    app.port(SERVER_PORT).multithreaded().run();
    return 0;
}
