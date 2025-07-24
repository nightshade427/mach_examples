#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config mach(){
  return (config) {
    .resources = {{
      {"home", "/",
        .get = {{
          render((r){(asset){
            #embed "home.mustache.html"
          }})
        }}
      },

      {"about", "/about",
        .get = {{
          render((r){(asset){
            #embed "about.mustache.html"
          }})
        }}
      },

      {"contact", "/contact",
        .get = {{
          render((r){(asset){
            #embed "contact.mustache.html"
          }})
        }}
      },

      {"todos", "/todos",
        .get = {{
          query((d){
            .set = "todos",
            .db = "todos_db",
            .query = (asset){
              #embed "get_todos.sql"
            }
          }),
          render((r){(asset){
            #embed "todos.mustache.html"
          }})
        }},

        .post = {{
          validate((v){
            .name = "title",
            .validation = "^\\S{1,16}$",
            .message = "must be 1-16 characters, no spaces"
          }),
          query((d){
            .db = "todos_db",
            .query = (asset){
              #embed "create_todo.sql"
            }
          }),
          redirect((u){"todos"})
        }},

        .before = {logged_in()},

        .context = {
          {"todo", (asset){
            #embed "todo.mustache.html"
          }}
        }
      },

      {"todo", "/todos/:id",
        .patch = {{
          validate((v){
            .name = "finished",
            .optional = true,
            .validation = "1",
            .message = "must be 1"
          }),
          find((d){
            .db = "todos_db",
            .query = (asset){
              #embed "update_todo.sql"
            }
          }),
          redirect((u){"todos"})
        }},

        .delete = {{
          find((d){
            .db = "todos_db",
            .query = (asset){
              #embed "delete_todo.sql"
            }
          }),
          redirect((u){"todos"})
        }},

        .before = {
          logged_in(),
          validate((v){
            .name = "id",
            .validation = "^\\d{1,10}$",
            .message = "must be between 1-9999999999"
          })
        }
      }},

      .before = {session()},

      .context = {
        {"layout", (asset){
          #embed "layout.mustache.html"
        }}
      },

      .errors = {
        {http_error, {
          render((r){(asset){
            #embed "5xx.mustache.html"
          }})
        }},

        {http_not_found, {
          render((r){(asset){
            #embed "404.mustache.html"
          }})
        }}
      }
    },

    .databases = {{
      .engine = sqlite_db,
      .name = "todos_db",
      .connect = "file:todo.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_todos_table.sql"
      }}
    }},
      
    .modules = {sqlite, session_auth}
  };
}
