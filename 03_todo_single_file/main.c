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

const ma_str ts_layout =
  "<html>"
    "<head>"
      "{{> htmx_script}} "
      "<link rel='icon' href='/favicon.png'>"
      "{{$head}}"
      "{{/head}}"
    "</head>"
    "<body>"
      "<p>"
        "{{^user}}"
          "<a href='{{:url:login}}'>sign in</a>"
        "{{/user}}"
        "{{#user}}"
          "welcome, {{first_name}}"
        "{{/user}}"
      "</p>"
      "<nav>"
        "<a href='{{:url:root}}'>home</a>"
        "<a href='{{:url:about}}'>about us</a>"
        "<a href='{{:url:contact}}'>contact us</a>"
      "</nav>"
      "{{$body}}"
      "{{/body}}"
    "</body>"
  "</html>";

const ma_str ts_404 =
  "{{< layout}}"
    "{{$body}}"
      "<p>not found</p>"
    "{{/body}}"
  "{{/layout}}";

const ma_str ts_5xx =
  "{{< layout}}"
    "{{$body}}"
      "<p>error</p>"
    "{{/body}}"
  "{{/layout}}";

const ma_str ts_root =
  "{{< layout}}"
    "{{$body}}"
      "<p>home</p>"
    "{{/body}}"
  "{{/layout}}";

const ma_str ts_about =
  "{{< layout}}"
    "{{$body}}"
      "<p>about us</p>"
    "{{/body}}"
  "{{/layout}}";

const ma_str ts_contact =
  "{{< layout}}"
    "{{$body}}"
      "<p>contact us</p>"
    "{{/body}}"
  "{{/layout}}";

const ma_str ts_todo =
  "{{title}}"
  "{{finished}}";

const ma_str ts_todos =
  "{{< layout}}"
    "{{$body}}"
      "{{^todos}}"
        "<p>no todos</p>"
      "{{/todos}}"
      "{{#todos}}"
        "<p>todos</p>"
        "{{#.}}"
          "{{> todo}}"
        "{{/.}}"
      "{{/todos}}"
    "{{/body}}"
  "{{/layout}}";

ma_state list_todos(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_todos)){
    auto const q =
      "select title, finished "
      "from todos "
      "where user_id = ?;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    ma_db_query(ctx, list_todos, m_todos, m_todos, q, uid);
    return ma_processing;
  }
  ma_render(ctx, ma_ok, m_todos);
  return ma_complete;
}

ma_state create_todo(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_todo)){
    auto const q =
      "insert into todos(user_id, title, finished) "
      "values(?, ?, false) "
      "returning id, title, finished;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const t = ma_params_get(ctx, "title");
    ma_db_query(ctx, create_todo, m_todo, m_todos, q, uid, t);
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
      "where user_id = ? "
        "and id = ? "
      "returning id, title, finished;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const tid = ma_params_get(ctx, "id");
    auto const f = ma_params_get(ctx, "finished");
    ma_db_find(ctx, update_todo, m_todo, m_todos, q, f, uid, tid);
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
      "where user_id = ? "
        "and id = ? "
      "returning id;";
    auto const uid = ma_cookies_get(ctx, m_user_id);
    auto const tid = ma_params_get(ctx, "id");
    ma_db_find(ctx, delete_todo, m_todo, m_todos, q, uid, tid);
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
      "user_id INTEGER NOT NULL, "
      "title TEXT NOT NULL, "
      "finished INTEGER NOT NULL DEFAULT 0 CHECK(finished IN (0, 1))"
    "); "
    "CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);";
  ma_db_migrate(ctx, db, q);
}

ma_config mach(){
  return (ma_config) {
    .use = {m_session},
    .includes = {htmx_config, session_auth_config},
    .databases = {
      {m_todos, ma_sqlite, "file:todo.db?mode=rwc",
        .migrations = {create_todos_table}
      }
    },
    .templates = {
      {m_404, ts_404},
      {m_5xx, ts_5xx},
      {m_root, ts_root},
      {m_about, ts_about},
      {m_contact, ts_contact},
      {m_todo, ts_todo},
      {m_todos, ts_todos},
      {m_layout, ts_layout}
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
