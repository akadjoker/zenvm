/* =========================================================
** builtin_sqlite.cpp — "sqlite" module for Zen
**
** Thin wrapper around SQLite3 amalgamation.
** Two classes:
**   DB   — database connection
**   Stmt — prepared statement
**
** Usage:
**   import sqlite
**   var db = sqlite.open("game.db")
**   db.exec("CREATE TABLE IF NOT EXISTS scores (name TEXT, score INT)")
**   var st = db.prepare("INSERT INTO scores VALUES (?, ?)")
**   st.bind(1, "player1")
**   st.bind(2, 42)
**   st.step()
**   st.reset()
**   var rows = db.query("SELECT * FROM scores")
**   for row in rows
**       print(row[0], row[1])
**   db.close()
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include "sqlite3.h"
#include <cstring>
#include <cstdio>

namespace zen {

/* Forward — set during sqlite_init */
static ObjClass *g_stmt_class = nullptr;

/* =========================================================
** Stmt class
** ========================================================= */

struct StmtData
{
    sqlite3_stmt *stmt = nullptr;
    bool done         = false;

    ~StmtData()
    {
        if (stmt) { sqlite3_finalize(stmt); stmt = nullptr; }
    }
};

static StmtData *stmt_data(Value self)
{
    return zen_instance_data<StmtData>(self);
}

static void *stmt_ctor(VM *vm, int argc, Value *args)
{
    (void)vm; (void)argc; (void)args;
    return new StmtData();
}

static void stmt_dtor(VM *vm, void *data)
{
    (void)vm;
    delete (StmtData *)data;
}

/* Stmt.bind(index, value) — 1-based index, value: int/float/string/nil */
static int stmt_bind(VM *vm, Value *args, int nargs)
{
    StmtData *sd = stmt_data(args[0]);
    if (!sd->stmt)
    {
        vm->runtime_error("Stmt.bind(): statement is closed");
        return -1;
    }
    if (nargs < 3)
    {
        vm->runtime_error("Stmt.bind() expects (index, value)");
        return -1;
    }
    int idx = (int)to_number(args[1]);
    Value &v = args[2];
    int rc;
    if (is_nil(v))
        rc = sqlite3_bind_null(sd->stmt, idx);
    else if (is_string(v))
        rc = sqlite3_bind_text(sd->stmt, idx, as_cstring(v), -1, SQLITE_TRANSIENT);
    else if (is_int(v))
        rc = sqlite3_bind_int64(sd->stmt, idx, (sqlite3_int64)v.as.integer);
    else
        rc = sqlite3_bind_double(sd->stmt, idx, to_number(v));

    args[0] = val_bool(rc == SQLITE_OK);
    return 1;
}

/* Stmt.step() → true if row available, false if done */
static int stmt_step(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    StmtData *sd = stmt_data(args[0]);
    if (!sd->stmt)
    {
        vm->runtime_error("Stmt.step(): statement is closed");
        return -1;
    }
    int rc = sqlite3_step(sd->stmt);
    if (rc == SQLITE_ROW)
    {
        args[0] = val_bool(true);
    }
    else if (rc == SQLITE_DONE)
    {
        sd->done = true;
        args[0] = val_bool(false);
    }
    else
    {
        vm->runtime_error(sqlite3_errstr(rc));
        return -1;
    }
    return 1;
}

/* Stmt.row() → array of column values for the current row */
static int stmt_row(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    StmtData *sd = stmt_data(args[0]);
    if (!sd->stmt)
    {
        vm->runtime_error("Stmt.row(): statement is closed");
        return -1;
    }
    int ncols = sqlite3_column_count(sd->stmt);
    ObjArray *arr = new_array(&vm->get_gc());
    for (int i = 0; i < ncols; i++)
    {
        int t = sqlite3_column_type(sd->stmt, i);
        switch (t)
        {
            case SQLITE_INTEGER:
                array_push(&vm->get_gc(), arr, val_int((int64_t)sqlite3_column_int64(sd->stmt, i)));
                break;
            case SQLITE_FLOAT:
                array_push(&vm->get_gc(), arr, val_float(sqlite3_column_double(sd->stmt, i)));
                break;
            case SQLITE_TEXT:
            {
                const char *s = (const char *)sqlite3_column_text(sd->stmt, i);
                ObjString *os = vm->make_string(s ? s : "");
                array_push(&vm->get_gc(), arr, val_obj((Obj *)os));
                break;
            }
            case SQLITE_BLOB:
            {
                /* Return blob as string (raw bytes) */
                const void *blob = sqlite3_column_blob(sd->stmt, i);
                int bsz = sqlite3_column_bytes(sd->stmt, i);
                ObjString *os = vm->make_string((const char *)blob, bsz);
                array_push(&vm->get_gc(), arr, val_obj((Obj *)os));
                break;
            }
            default: /* SQLITE_NULL */
                array_push(&vm->get_gc(), arr, val_nil());
                break;
        }
    }
    args[0] = val_obj((Obj *)arr);
    return 1;
}

/* Stmt.reset() — reset for re-execution */
static int stmt_reset(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    StmtData *sd = stmt_data(args[0]);
    if (sd->stmt)
    {
        sqlite3_reset(sd->stmt);
        sqlite3_clear_bindings(sd->stmt);
        sd->done = false;
    }
    args[0] = val_bool(true);
    return 1;
}

/* Stmt.close() — finalise */
static int stmt_close(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    StmtData *sd = stmt_data(args[0]);
    if (sd->stmt) { sqlite3_finalize(sd->stmt); sd->stmt = nullptr; }
    args[0] = val_bool(true);
    return 1;
}

/* Stmt.columns() → number of result columns */
static int stmt_columns(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    StmtData *sd = stmt_data(args[0]);
    args[0] = val_int(sd->stmt ? sqlite3_column_count(sd->stmt) : 0);
    return 1;
}

/* Stmt.colName(i) → string name of column i (0-based) */
static int stmt_col_name(VM *vm, Value *args, int nargs)
{
    StmtData *sd = stmt_data(args[0]);
    int idx = (nargs >= 2) ? (int)to_number(args[1]) : 0;
    const char *n = sd->stmt ? sqlite3_column_name(sd->stmt, idx) : "";
    ObjString *os = vm->make_string(n ? n : "");
    args[0] = val_obj((Obj *)os);
    return 1;
}

static void stmt_init(VM *vm)
{
    g_stmt_class = vm->def_class("Stmt")
        .ctor(stmt_ctor)
        .dtor(stmt_dtor)
        .method("bind",    stmt_bind,     -1)
        .method("step",    stmt_step,      0)
        .method("row",     stmt_row,       0)
        .method("reset",   stmt_reset,     0)
        .method("close",   stmt_close,     0)
        .method("columns", stmt_columns,   0)
        .method("colName", stmt_col_name, -1)
        .end();
}
/* =========================================================
** DB class
** ========================================================= */

struct DBData
{
    sqlite3 *db    = nullptr;
    VM      *vm_ref = nullptr; /* for creating Stmt instances */

    ~DBData()
    {
        if (db) { sqlite3_close_v2(db); db = nullptr; }
    }
};

static DBData *db_data(Value self)
{
    return zen_instance_data<DBData>(self);
}

static void *db_ctor(VM *vm, int argc, Value *args)
{
    (void)argc; (void)args;
    DBData *d = new DBData();
    d->vm_ref = vm;
    return d;
}

static void db_dtor(VM *vm, void *data)
{
    (void)vm;
    delete (DBData *)data;
}

/* DB.exec(sql) → bool — run SQL that returns no rows */
static int db_exec(VM *vm, Value *args, int nargs)
{
    DBData *dd = db_data(args[0]);
    if (!dd->db)
    {
        vm->runtime_error("DB.exec(): database is closed");
        return -1;
    }
    if (nargs < 2 || !is_string(args[1]))
    {
        vm->runtime_error("DB.exec() expects (sql)");
        return -1;
    }
    char *errmsg = nullptr;
    int rc = sqlite3_exec(dd->db, as_cstring(args[1]), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "DB.exec(): %s", errmsg ? errmsg : "unknown error");
        sqlite3_free(errmsg);
        vm->runtime_error(buf);
        return -1;
    }
    args[0] = val_bool(true);
    return 1;
}

/* DB.query(sql) → array of rows (each row is an array of values) */
static int db_query(VM *vm, Value *args, int nargs)
{
    DBData *dd = db_data(args[0]);
    if (!dd->db)
    {
        vm->runtime_error("DB.query(): database is closed");
        return -1;
    }
    if (nargs < 2 || !is_string(args[1]))
    {
        vm->runtime_error("DB.query() expects (sql)");
        return -1;
    }
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(dd->db, as_cstring(args[1]), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        vm->runtime_error(sqlite3_errmsg(dd->db));
        return -1;
    }
    int ncols = sqlite3_column_count(stmt);
    ObjArray *rows = new_array(&vm->get_gc());
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ObjArray *row = new_array(&vm->get_gc());
        for (int i = 0; i < ncols; i++)
        {
            int t = sqlite3_column_type(stmt, i);
            switch (t)
            {
                case SQLITE_INTEGER:
                    array_push(&vm->get_gc(), row, val_int((int64_t)sqlite3_column_int64(stmt, i)));
                    break;
                case SQLITE_FLOAT:
                    array_push(&vm->get_gc(), row, val_float(sqlite3_column_double(stmt, i)));
                    break;
                case SQLITE_TEXT:
                {
                    const char *s = (const char *)sqlite3_column_text(stmt, i);
                    ObjString *os = vm->make_string(s ? s : "");
                    array_push(&vm->get_gc(), row, val_obj((Obj *)os));
                    break;
                }
                default:
                    array_push(&vm->get_gc(), row, val_nil());
                    break;
            }
        }
        array_push(&vm->get_gc(), rows, val_obj((Obj *)row));
    }
    sqlite3_finalize(stmt);
    args[0] = val_obj((Obj *)rows);
    return 1;
}

/* DB.prepare(sql) → Stmt */
static int db_prepare(VM *vm, Value *args, int nargs)
{
    DBData *dd = db_data(args[0]);
    if (!dd->db)
    {
        vm->runtime_error("DB.prepare(): database is closed");
        return -1;
    }
    if (nargs < 2 || !is_string(args[1]))
    {
        vm->runtime_error("DB.prepare() expects (sql)");
        return -1;
    }
    sqlite3_stmt *raw = nullptr;
    int rc = sqlite3_prepare_v2(dd->db, as_cstring(args[1]), -1, &raw, nullptr);
    if (rc != SQLITE_OK)
    {
        vm->runtime_error(sqlite3_errmsg(dd->db));
        return -1;
    }
    /* Create a Stmt instance using the cached class pointer */
    if (!g_stmt_class)
    {
        sqlite3_finalize(raw);
        vm->runtime_error("DB.prepare(): Stmt class not registered");
        return -1;
    }
    Value stmt_val = vm->make_instance(g_stmt_class);
    if (is_nil(stmt_val))
    {
        sqlite3_finalize(raw);
        vm->runtime_error("DB.prepare(): failed to create Stmt instance");
        return -1;
    }
    StmtData *sd = zen_instance_data<StmtData>(stmt_val);
    sd->stmt = raw;
    args[0] = stmt_val;
    return 1;
}

/* DB.close() */
static int db_close(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    if (dd->db) { sqlite3_close_v2(dd->db); dd->db = nullptr; }
    args[0] = val_bool(true);
    return 1;
}

/* DB.lastInsertId() → int */
static int db_last_insert_id(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    args[0] = val_int(dd->db ? (int64_t)sqlite3_last_insert_rowid(dd->db) : 0);
    return 1;
}

/* DB.changes() → int — rows affected by last INSERT/UPDATE/DELETE */
static int db_changes(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    args[0] = val_int(dd->db ? (int64_t)sqlite3_changes(dd->db) : 0);
    return 1;
}

/* DB.error() → string — last error message */
static int db_error(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    const char *msg = dd->db ? sqlite3_errmsg(dd->db) : "not open";
    ObjString *os = vm->make_string(msg);
    args[0] = val_obj((Obj *)os);
    return 1;
}

/* DB.begin() / commit() / rollback() — transaction helpers */
static int db_begin(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    if (!dd->db) { args[0] = val_bool(false); return 1; }
    args[0] = val_bool(sqlite3_exec(dd->db, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK);
    return 1;
}

static int db_commit(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    if (!dd->db) { args[0] = val_bool(false); return 1; }
    args[0] = val_bool(sqlite3_exec(dd->db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK);
    return 1;
}

static int db_rollback(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    DBData *dd = db_data(args[0]);
    if (!dd->db) { args[0] = val_bool(false); return 1; }
    args[0] = val_bool(sqlite3_exec(dd->db, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    return 1;
}

/* =========================================================
** Module-level: sqlite.open(path) and sqlite.memory()
** ========================================================= */

/* sqlite.open(path) → DB */
static int nat_sqlite_open(VM *vm, Value *args, int nargs)
{
    const char *path = (nargs >= 1 && is_string(args[0])) ? as_cstring(args[0]) : ":memory:";
    sqlite3 *db = nullptr;
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK)
    {
        const char *msg = sqlite3_errmsg(db);
        char buf[512];
        snprintf(buf, sizeof(buf), "sqlite.open(): %s", msg);
        sqlite3_close(db);
        vm->runtime_error(buf);
        return -1;
    }
    /* Enable WAL for better concurrent performance */
    sqlite3_exec(db, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA foreign_keys=ON",  nullptr, nullptr, nullptr);

    /* Create a DB instance */
    if (!g_stmt_class) /* g_stmt_class doubles as init sentinel */
    {
        sqlite3_close(db);
        vm->runtime_error("sqlite.open(): module not initialised");
        return -1;
    }
    /* Look up DB class from globals */
    int db_gidx = vm->find_global("DB");
    if (db_gidx < 0)
    {
        sqlite3_close(db);
        vm->runtime_error("sqlite.open(): DB class not found");
        return -1;
    }
    Value db_cls_val = vm->get_global(db_gidx);
    if (!is_class(db_cls_val))
    {
        sqlite3_close(db);
        vm->runtime_error("sqlite.open(): DB is not a class");
        return -1;
    }
    Value db_val = vm->make_instance(as_class(db_cls_val));
    if (is_nil(db_val))
    {
        sqlite3_close(db);
        vm->runtime_error("sqlite.open(): failed to create DB instance");
        return -1;
    }
    DBData *dd = zen_instance_data<DBData>(db_val);
    dd->db = db;
    dd->vm_ref = vm;
    args[0] = db_val;
    return 1;
}

/* sqlite.memory() → DB (in-memory database) */
static int nat_sqlite_memory(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    args[0] = val_obj((Obj *)vm->make_string(":memory:"));
    return nat_sqlite_open(vm, args, 1);
}

/* sqlite.version() → string */
static int nat_sqlite_version(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    ObjString *os = vm->make_string(sqlite3_libversion());
    args[0] = val_obj((Obj *)os);
    return 1;
}

/* =========================================================
** Registration
** ========================================================= */

static const NativeReg sqlite_functions[] = {
    {"open",    nat_sqlite_open,    1},
    {"memory",  nat_sqlite_memory,  0},
    {"version", nat_sqlite_version, 0},
};

static void sqlite_init(VM *vm)
{
    stmt_init(vm);

    vm->def_class("DB")
        .ctor(db_ctor)
        .dtor(db_dtor)
        .method("exec",          db_exec,           1)
        .method("query",         db_query,           1)
        .method("prepare",       db_prepare,         1)
        .method("close",         db_close,           0)
        .method("lastInsertId",  db_last_insert_id,  0)
        .method("changes",       db_changes,         0)
        .method("error",         db_error,           0)
        .method("begin",         db_begin,           0)
        .method("commit",        db_commit,          0)
        .method("rollback",      db_rollback,        0)
        .end();
}

extern const NativeLib zen_lib_sqlite = {
    "sqlite",
    sqlite_functions,
    3,
    nullptr,
    0,
    sqlite_init,
};

} /* namespace zen */
