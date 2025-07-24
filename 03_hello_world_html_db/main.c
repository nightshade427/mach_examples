#include <mach.h>
#include <sqlite.h>
#include <mustache.h>

void hello(){
  assign("limit", "1");
  await_assign_db_query("greeting", (q){.db = "hello_db",
    "select name "
      "from greetings "
      "limit {{limit}};"
  });
  respond(http_ok, mime_html, (b){.engine = mustache,
    "<html>"
      "<body>"
        "{{#greeting}}"
          "<p>Hello {{name}}</p>"
        "{{/greeting}}"
      "</body>"
    "</html>"
  });
}

config mach(){
  return (config) {
    .routes = {
      {"/", http_get, .call = hello}
    },
    .includes = {
      sqlite_config,
      mustache_config
    },
    .databases = {{
      .engine = sqlite,
      .name = "hello_db",
      .connect = "file:hello.db?mode=rwc",
      .migrations = {
        "CREATE TABLE greetings ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "name TEXT NOT NULL"
        ");"
      },
      .seeds = {
        "INSERT INTO greetings(name)"
        "VALUES('World');"
      }
    }}
  };
}
