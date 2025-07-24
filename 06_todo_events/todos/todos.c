#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config todos(){
  return (config) {
    .name = "todos",

    .resources = {
      {"todos", "/todos", {logged_in()},
        .get = {
          query({"get_todos",
            .set_key = "todos",
            .db = "todos_db"
          }),
          render("todos")
        },

        .post = {
          validate({"title",
            .validation = "^\\S{1,16}$",
            .message = "must be 1-16 characters, no spaces"
          }),
          query({"create_todo",
            .db = "todos_db"
          }),
          emit("todo_created"),
          redirect("todos")
        }
      },

      {"todo", "/todos/:id", {
        logged_in(),
        validate({"id",
          .validation = "^\\d{1,10}$",
          .message = "must be between 1-9999999999"
        })},

        .patch = {
          validate({"finished",
            .optional = true,
            .validation = "1",
            .message = "must be 1"
          }),
          find({"update_todo",
            .db = "todos_db"
          }),
          redirect("todos")
        },

        .delete = {
          find({"delete_todo",
            .db = "todos_db"
          }),
          redirect("todos")
        }
      }
    },

    .context = {
      {"todos", (asset){
        #embed "todos.mustache.html"
      }},
      {"get_todos", (asset){
        #embed "get_todos.sql"
      }},
      {"create_todos", (asset){
        #embed "create_todo.sql"
      }},
      {"update_todo", (asset){
        #embed "update_todo.sql"
      }},
      {"delete_todo", (asset){
        #embed "delete_todo.sql"
      }}
    },

    .publishes = {
      {"todo_created",
        .with = {
          "user_id",
          "title"
        }
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
