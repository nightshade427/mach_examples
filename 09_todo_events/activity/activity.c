#include <mach.h>
#include <sqlite.h>
#include <mustache.h>
#include <session_auth.h>

void list_activity(){
  await_assign_db_query("activity", (q){.db = "activity_db",
    "select action, title, created_at "
    "from activity "
    "where user_id = {{user_id}} "
    "order by created_at desc "
    "limit 50;"
  });
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "activity"});
}

void on_todo_created(){
  db_query((q){.db = "activity_db",
    "insert into activity(user_id, action, title) "
    "values({{user_id}}, 'created', {{title}});"
  });
}

config activity_config(){
  return (config) {
    "activity",
    .uses = {
      "session",
      "logged_in"
    },
    .includes = {
      sqlite_config,
      mustache_config,
      session_auth_config
    },
    .subscribes = {
      {"todo_created", .call = on_todo_created}
    },
    .routes = {
      {"/activity", http_get, "activity", .call = list_activity}
    },
    .resources = {
      {"activity", (resource){
        #embed "activity.mustache.html"
      }},
    },
    .databases = {{
      .name = "activity_db",
      .engine = sqlite,
      .connect = "file:activity.db?mode=rwc",
      .migrations = {
        "CREATE TABLE IF NOT EXISTS activity ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "user_id INTEGER NOT NULL, "
          "action TEXT NOT NULL, "
          "title TEXT NOT NULL, "
          "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        "); "
        "CREATE INDEX IF NOT EXISTS idx_activity_user_id ON activity(user_id);"
      }
    }}
  };
}
