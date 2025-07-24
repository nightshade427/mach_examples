#include <mach.h>
#define m_s_401 "401"
#define m_s_root "root"
#define m_s_login "login"
#define m_s_users "users"
#define m_s_user "user"
#define m_s_session "session"
#define m_s_logged_in "logged_in"
#define m_s_user_id "user_id"

ma_state login(const ma_ctx ctx){
  auto const u = ma_params_get(ctx, "username");
  auto const p = ma_params_get(ctx, "password");
  if(!ma_ctx_has(ctx, m_s_users)){
    auto const q =
      "select id, password, salt "
      "from users "
      "where username = ? "
      "limit 1;";
    ma_db_query(ctx, login, m_s_users, m_s_users, q, u);
    return ma_processing;
  }
  auto const t = ma_ctx_get(ctx, m_s_users);
  auto const ur = ma_table_record(t, 0);
  if(!ma_auth(ur, p)){
    ma_render(ctx, ma_not_authorized, m_s_login);
    return ma_complete;
  }
  auto const uid = ma_record_get(ur, "id");
  ma_cookies_set(ctx, m_s_user_id, uid);
  ma_redirect(ctx, m_s_root);
  return ma_complete;
}

ma_state logout(const ma_ctx ctx){
  ma_cookies_delete(ctx, m_s_user_id);
  ma_redirect(ctx, m_s_root);
  return ma_complete;
}

ma_state session(const ma_ctx ctx){
  auto const uid = ma_cookies_get(ctx, m_s_user_id);
  if(!uid){
    return ma_proceed;
  }
  if(!ma_ctx_has(ctx, m_s_user)){
    auto const q =
      "select first_name "
      "from users "
      "where id = ? "
      "limit 1;";
    ma_db_query(ctx, session, m_s_user, m_s_users, q, uid);
    return ma_processing;
  }
  return ma_proceed;
}

ma_state logged_in(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_s_user)){
    ma_redirect(ctx, m_s_login);
    return ma_complete;
  }
  return ma_proceed;
}

void create_users_table(const ma_ctx ctx, const ma_db db){
  auto const q = 
    "CREATE TABLE IF NOT EXISTS users ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "first_name TEXT NOT NULL, "
      "last_name TEXT NOT NULL, "
      "username TEXT NOT NULL UNIQUE, "
      "password TEXT NOT NULL, "
      "salt TEXT NOT NULL "
    "); "
    "CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);";
  ma_db_migrate(ctx, db, q);
}

ma_config session_config(){
  return (ma_config) {
    "session",
    .templates = {
      {m_s_login, (ma_embed){
        #embed "login.mustache.html"
      }},
      {m_s_401, (ma_embed){
        #embed "401.mustache.html"
      }}
    },
    .middleware = {
      {m_s_session, .before = session},
      {m_s_logged_in, .before = logged_in}
    },
    .databases = {
      {m_s_users, ma_sqlite, "file:users.db?mode=rwc",
        .migrations = {create_users_table}
      }
    },
    .routes = {
      {.status = ma_not_authorized, .render = m_s_401},
      {ma_get, "/login", m_s_login, .render = m_s_login},
      {ma_post, "/session", m_s_session, .call = login},
      {ma_delete, "/session", .call = logout, .use = {m_s_logged_in}}
    }
  };
}
