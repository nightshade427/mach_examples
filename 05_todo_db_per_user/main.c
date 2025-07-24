#include <mach.h>
#include <htmx.h>
#include <session_auth.h>
#define m_layout "layout"
#define m_404 "404"
#define m_5xx "5xx"
#define m_root "root"
#define m_about "about"
#define m_contact "contact"
#define m_todo "todo"
#define m_todos "todos"
#define m_session "session"
#define m_logged_in "logged_in"
#define m_user_id "user_id"

ma_state list_todos(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_todos)){
    auto const q =
      "select title, finished "
      "from todos;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const user_db = ma_str_printf(ctx, "file:%s_todo.db?mode=rwc", uid);
    ma_db_query_tenant(ctx, list_todos, m_todos, (ma_db){m_todos, .connect = user_db}, q);
    return ma_processing;
  }
  ma_render(ctx, ma_ok, m_todos);
  return ma_complete;
}

ma_state create_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_todo)){
    auto const q =
      "insert into todos(title, finished) "
      "values(?, false) "
      "returning id, title, finished;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const t = ma_params_get(ctx, "title");
    auto const user_db = ma_str_printf(ctx, "file:%s_todo.db?mode=rwc", uid);
    ma_db_query_tenant(ctx, create_todo, m_todo, (ma_db){m_todos, .connect = user_db}, q, t);
    return ma_processing;
  }
  if(is_htmx(ctx)){
    ma_render(ctx, ma_created, m_todo);
    return ma_complete;
  }
  ma_redirect(ctx, m_todos);
  return ma_complete;
}

ma_state update_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_todo)){
    auto const q =
      "update todos "
      "set finished = ? "
      "where id = ? "
      "returning id, title, finished;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const tid = ma_params_get(ctx, "id");
    auto const f = ma_params_get(ctx, "finished");
    auto const user_db = ma_str_printf(ctx, "file:%s_todo.db?mode=rwc", uid);
    ma_db_find_tenant(ctx, update_todo, m_todo, (ma_db){m_todos, .connect = user_db}, q, f, tid);
    return ma_processing;
  }
  if(is_htmx(ctx)){
    ma_render(ctx, ma_ok, m_todo);
    return ma_complete;
  }
  ma_redirect(ctx, m_todos);
  return ma_complete;
}

ma_state delete_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_todo)){
    auto const q =
      "delete from todos "
      "where id = ? "
      "returning id;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const tid = ma_params_get(ctx, "id");
    auto const user_db = ma_str_printf(ctx, "file:%s_todo.db?mode=rwc", uid);
    ma_db_find_tenant(ctx, delete_todo, m_todo, (ma_db){m_todos, .connect = user_db}, q, tid);
    return ma_processing;
  }
  if(is_htmx(ctx)){
    ma_send(ctx, ma_text, ma_ok, nullptr);
    return ma_complete;
  }
  ma_redirect(ctx, m_todos);
  return ma_complete;
}

void create_todos_table(const ma_ctx ctx, const ma_db db){
  auto const q =
    "CREATE TABLE IF NOT EXISTS todos ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "title TEXT NOT NULL, "
      "finished INTEGER NOT NULL DEFAULT 0 CHECK(finished IN (0, 1))"
    ");";
  ma_db_migrate(ctx, db, q);
}

ma_config mach(){
  return (ma_config) {
    .use = {m_session},
    .includes = {htmx_config, session_auth_config},
    .databases = {
      {m_todos, ma_sqlite,
        .migrations = {create_todos_table}
      }
    },
    .templates = {
      {m_layout, (ma_embed){
        #embed "layout.mustache.html"
      }},
      {m_404, (ma_embed){
        #embed "404.mustache.html"
      }},
      {m_5xx, (ma_embed){
        #embed "5xx.mustache.html"
      }},
      {m_root, (ma_embed){
        #embed "root.mustache.html"
      }},
      {m_about, (ma_embed){
        #embed "about.mustache.html"
      }},
      {m_contact, (ma_embed){
        #embed "contact.mustache.html"
      }},
      {m_todo, (ma_embed){
        #embed "todo.mustache.html"
      }},
      {m_todos, (ma_embed){
        #embed "todos.mustache.html"
      }}
    },
    .routes = {
      {.status = ma_error, .render = m_5xx},
      {.status = ma_not_found, .render = m_404},
      {ma_get, "/", m_root, .render = m_root},
      {ma_get, "/about", m_about, .render = m_about},
      {ma_get, "/contact", m_contact, .render = m_contact},
      {ma_get, "/todos", m_todos, .call = list_todos, .use = {m_logged_in}},
      {ma_post, "/todos", .call = create_todo, .use = {m_logged_in}},
      {ma_patch, "/todos/:id", .call = update_todo, .use = {m_logged_in}},
      {ma_delete, "/todos/:id", .call = delete_todo, .use = {m_logged_in}}
    }
  };
}
