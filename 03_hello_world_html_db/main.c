#include <mach.h>
#include <sqlite.h>

config mach(){
  return (config) {

    .includes = {sqlite_config},

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
    }},

    .resources = {
      .urls = {
        {"/",
          .get = {{
            db((d){{{
              .set = "greeting",
              .db = "hello_db",
              .query =
                "select name "
                "from greetings "
                "limit 1;"
            }}}),
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
      }
    }

};}
