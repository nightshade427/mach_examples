#include <mach.h>
#include <sqlite.h>

config mach(){
  return (config) {
    .resources = {{
      {"home", "/",
        .get = {{
          query((d){
            .set = "greeting",
            .db = "hello_db",
              "select name "
              "from greetings "
              "limit 1;"
          }),
          render((r){
            "<html>"
              "<body>"
                "{{#greeting}}"
                  "<p>Hello {{name}}</p>"
                "{{/greeting}}"
              "</body>"
            "</html>"
          })
        }}
      }
    }},

    .databases = {{
      .engine = sqlite_db,
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
    }},
      
    .modules = {sqlite}
  };
}
