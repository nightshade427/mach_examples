#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config todos(){
  return (config) {"todos",
    .resources = {
      {"todos", "/todos", {logged_in()},
        .get = {
          query({"get_todos", .set_key = "todos", .db = "todos_db"}),
          render("todos")
        },

        .post = {
          input({"title", .matches = m_not_empty}),
          query({"create_todo", .db = "todos_db"}),
          emit("todo_created"),
          redirect("todos")
        }
      },

      {"todo", "/todos/:id", {
        logged_in(),
        input({"id", .matches = m_positive})},

        .patch = {
          input({"finished", .optional = true, .matches = "1", .message = "must be 1"}),
          query({"update_todo", .db = "todos_db"}),
          redirect("todos")
        },

        .delete = {
          query({"delete_todo", .db = "todos_db"}),
          redirect("todos")
        }
      }
    },

    .publishes = {
      {"todo_created", .with = {"user_id", "title"}}
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
