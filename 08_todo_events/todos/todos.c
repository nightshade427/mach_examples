#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config todos(){
  return (config) {
    .name = "todos",

    .resources = {{
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
          emit((e){"todo_created"}),
          redirect((u){"todos"})
        }},

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
          validate((v){
            .name = "id",
            .validation = "^\\d{1,10}$",
            .message = "must be between 1-9999999999"
          })
        }
      }},

      .before = {
        session(),
        logged_in()
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

    .publishes = {
      {"todo_created",
        .with = {
          "user_id",
          "title"
        }
      }
    },

    .modules = {sqlite, session_auth}
  };
}
