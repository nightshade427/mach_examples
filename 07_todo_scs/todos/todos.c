#include <mach.h>
#include <htmx.h>
#define m_t_todo "todo"
#define m_t_todos "todos"
#define m_t_session "session"
#define m_t_logged_in "logged_in"
#define m_t_user_id "user_id"

ma_state list_todos(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_t_todos)){
    auto const q =
      "select title, finished "
      "from todos "
      "where user_id = ?;";
    auto const uid = ma_cookies_get(ctx, m_t_user_id);
    ma_db_query(ctx, list_todos, m_t_todos, m_t_todos, q, uid);
    return ma_processing;
  }
  ma_render(ctx, ma_ok, m_t_todos);
  return ma_complete;
}

ma_state create_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_t_todo)){
    auto const q =
      "insert into todos(user_id, title, finished) "
      "values(?, ?, false) "
      "returning id, title, finished;";
    auto const uid = ma_cookies_get(ctx, m_t_user_id);
    auto const t = ma_params_get(ctx, "title");
    ma_db_query(ctx, create_todo, m_t_todo, m_t_todos, q, uid, t);
    return ma_processing;
  }
  if(is_htmx(ctx)){
    ma_render(ctx, ma_created, m_t_todo);
    return ma_complete;
  }
  ma_redirect(ctx, m_t_todos);
  return ma_complete;
}

ma_state update_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_t_todo)){
    auto const q =
      "update todos "
      "set finished = ? "
      "where user_id = ? "
        "and id = ? "
      "returning id, title, finished;";
    auto const uid = ma_cookies_get(ctx, m_t_user_id);
    auto const tid = ma_params_get(ctx, "id");
    auto const f = ma_params_get(ctx, "finished");
    ma_db_find(ctx, update_todo, m_t_todo, m_t_todos, q, f, uid, tid);
    return ma_processing;
  }
  if(is_htmx(ctx)){
    ma_render(ctx, ma_ok, m_t_todo);
    return ma_complete;
  }
  ma_redirect(ctx, m_t_todos);
  return ma_complete;
}

ma_state delete_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_t_todo)){
    auto const q =
      "delete from todos "
      "where user_id = ? "
        "and id = ? "
      "returning id;";
    auto const uid = ma_cookies_get(ctx, m_t_user_id);
    auto const tid = ma_params_get(ctx, "id");
    ma_db_find(ctx, delete_todo, m_t_todo, m_t_todos, q, uid, tid);
    return ma_processing;
  }
  if(is_htmx(ctx)){
    ma_send(ctx, ma_text, ma_ok, nullptr);
    return ma_complete;
  }
  ma_redirect(ctx, m_t_todos);
  return ma_complete;
}

void create_todos_table(const ma_ctx ctx, const ma_db db){
  auto const q =
    "CREATE TABLE IF NOT EXISTS todos ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "title TEXT NOT NULL, "
      "finished INTEGER NOT NULL DEFAULT 0 CHECK(finished IN (0, 1))"
    "); "
    "CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);";
  ma_db_migrate(ctx, db, q);
}

ma_config todos_config(){
  return (ma_config) {
    "todos",
    .use = {m_t_session, m_t_logged_in},
    .includes = {htmx_config},
    .templates = {
      {m_t_todo, (ma_embed){
        #embed "todo.mustache.html"
      }},
      {m_t_todos, (ma_embed){
        #embed "todos.mustache.html"
      }}
    },
    .databases = {
      {m_t_todos, ma_sqlite, "file:todo.db?mode=rwc",
        .migrations = {create_todos_table}
      }
    },
    .routes = {
      {ma_get, "/todos", m_t_todos, .call = list_todos},
      {ma_post, "/todos", .call = create_todo},
      {ma_patch, "/todos/:id", .call = update_todo},
      {ma_delete, "/todos/:id", .call = delete_todo}
    }
  };
}
