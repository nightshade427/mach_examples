#include <mach.h>
#include <sqlite.h>
#include <tailwind.h>

config mach() {
  return (config) {

    .resources = {
      {"home", "/",
        .get = {
          reroute("todos")
        }
      },

      {"todos", "/todos",
        .get = {
          query(
            {.set_key = "todos",
              .db = "todos_db",
              .query =
                "SELECT id, title, done, created_at "
                "FROM todos ORDER BY created_at DESC;"
            },
            {.set_key = "stats",
              .db = "todos_db",
              .query =
                "SELECT total, done FROM stats WHERE id = 1;"
            }
          ),
          render(
            "<html>"
            "<head>"
              "<title>MACH Tasks Demo</title>"
              "{{> tailwind_script}}"
            "</head>"
            "<body class='bg-gray-50 min-h-screen p-8'>"
            "<div class='max-w-lg mx-auto'>"
              "<h1 class='text-2xl font-bold mb-1'>Todos</h1>"
              "{{#stats}}"
              "<p class='text-sm text-gray-500 mb-4'>"
                "{{done}} of {{total}} done"
              "</p>"
              "{{/stats}}"
              "<form method='POST' class='flex gap-2 mb-6'>"
                "<input name='title' placeholder='What needs doing?'"
                  " class='flex-1 border rounded px-3 py-2' required>"
                "<button class='bg-blue-600 text-white px-4 py-2 rounded'>"
                  "Add"
                "</button>"
              "</form>"
              "{{#todos}}"
              "<div class='flex items-center gap-3 py-2 border-b'>"
                "<form method='POST' action='{{url:todo:id}}'>"
                  "<input type='hidden' name='http_method' value='put'>"
                  "{{#done}}"
                    "<button class='text-green-600'>&#10003;</button>"
                  "{{/done}}"
                  "{{^done}}"
                    "<button class='text-gray-300 hover:text-green-600'>&#9675;</button>"
                  "{{/done}}"
                "</form>"
                "{{#done}}"
                  "<span class='flex-1 line-through text-gray-400'>{{title}}</span>"
                "{{/done}}"
                "{{^done}}"
                  "<span class='flex-1'>{{title}}</span>"
                "{{/done}}"
                "<form method='POST' action='{{url:todo:id}}'>"
                  "<input type='hidden' name='http_method' value='delete'>"
                  "<button class='text-red-400 hover:text-red-600 text-sm'>"
                    "&#10005;"
                  "</button>"
                "</form>"
              "</div>"
              "{{/todos}}"
              "{{^todos}}"
                "<p class='text-gray-400 italic'>No todos yet.</p>"
              "{{/todos}}"
              "<a href='/activity' class='inline-block mt-6 text-sm text-blue-600 underline'>"
                "View worker activity log &#8594;"
              "</a>"
            "</div>"
            "</body>"
            "</html>"
          )
        },

        .post = {
          validate({"title",
            .validation = validate_not_empty,
            .message = "Title cannot be empty"
          }),
          query({
            .db = "todos_db",
            .query =
              "INSERT INTO todos(title) VALUES({{title}});"
          }),
          query({"recount",
            .db = "todos_db"
          }),
          task("log_created"),
          redirect("todos")
        }
      },

      {"todo", "/todos/:id", {
        validate({"id",
          .validation = validate_positive,
          .message = "Invalid id"
        })},

        .put = {
          query({
            .db = "todos_db",
            .query =
              "UPDATE todos "
              "SET done = CASE WHEN done = 0 THEN 1 ELSE 0 END "
              "WHERE id = {{id}};"
          }),
          query({"recount",
            .db = "todos_db"
          }),
          task("log_toggled"),
          redirect("todos")
        },

        .delete = {
          query({
            .db = "todos_db",
            .query =
              "DELETE FROM todos WHERE id = {{id}};"
          }),
          query({"recount",
            .db = "todos_db"
          }),
          task("log_deleted"),
          redirect("todos")
        }
      },

      {"activity", "/activity",
        .get = {
          query({
            .set_key = "logs",
            .db = "todos_db",
            .query =
              "SELECT action, detail, ran_at "
              "FROM activity_log ORDER BY ran_at DESC LIMIT 50;"
          }),
          render(
            "<html>"
            "<head>"
              "<title>Worker Activity</title>"
              "{{> tailwind_script}}"
            "</head>"
            "<body class='bg-gray-50 min-h-screen p-8'>"
            "<div class='max-w-lg mx-auto'>"
              "<h1 class='text-2xl font-bold mb-1'>Worker Activity</h1>"
              "<p class='text-sm text-gray-500 mb-4'>"
                "Task executions from the worker reactor"
              "</p>"
              "<a href='/todos'"
                " class='inline-block mb-4 text-sm text-blue-600 underline'>"
                "&#8592; Back to todos"
              "</a>"
              "{{#logs}}"
              "<div class='flex items-baseline gap-2 py-2 border-b text-sm'>"
                "<span class='font-mono text-purple-700 shrink-0'>{{action}}</span>"
                "<span class='text-gray-600 flex-1'>{{detail}}</span>"
                "<span class='text-gray-400 shrink-0'>{{ran_at}}</span>"
              "</div>"
              "{{/logs}}"
              "{{^logs}}"
                "<p class='text-gray-400 italic'>No activity yet.</p>"
              "{{/logs}}"
            "</div>"
            "</body>"
            "</html>"
          )
        }
      }
    },

    .context = {
      {"recount",
        "UPDATE stats SET "
          "total = (SELECT count(*) FROM todos),"
          "done = (SELECT count(*) FROM todos WHERE done = 1),"
          "updated_at = datetime('now') "
        "WHERE id = 1;"
      }
    },

    .errors = {
      {http_bad_request, {
        render(
          "<html>"
          "<head>"
            "<title>Error</title>"
            "{{> tailwind_script}}"
          "</head>"
          "<body class='bg-gray-50 p-8'>"
          "<div class='max-w-lg mx-auto'>"
            "<p class='text-red-600 mb-4'>{{error_message:title}}</p>"
            "<a href='/todos' class='text-blue-600 underline'>&#8592; Back</a>"
          "</div>"
          "</body>"
          "</html>"
        )
      }}
    },

    .tasks = {
      {"log_created", {
        query({
          .db = "todos_db",
          .query =
            "INSERT INTO activity_log(action, detail) "
            "VALUES('created', 'added: {{title}}');"
        })},
        .accepts = {"title"}
      },

      {"log_toggled", {
        query({
          .db = "todos_db",
          .query =
            "INSERT INTO activity_log(action, detail) "
            "VALUES('toggled', 'toggled todo #{{id}}');"
        })},
        .accepts = {"id"}
      },

      {"log_deleted", {
        query({
          .db = "todos_db",
          .query =
            "INSERT INTO activity_log(action, detail) "
            "VALUES('deleted', 'removed todo #{{id}}');"
        })},
        .accepts = {"id"}
      },

      {"cleanup_stale", {
        query(
          {.db = "todos_db",
            .query =
              "UPDATE todos SET done = 1 "
              "WHERE done = 0 "
                "AND created_at < datetime('now', '-1 hour');"
          },
          {.db = "todos_db",
            .query =
              "INSERT INTO activity_log(action, detail) "
              "VALUES('cron', 'auto-completed stale todos');"
          }
        )},
        .cron = "* * * * *"
      }
    },

    .databases = {{
      .engine  = sqlite_db,
      .name    = "todos_db",
      .connect = "file:todos.db?mode=rwc",
      .migrations = {
        "CREATE TABLE todos ("
          "id         INTEGER PRIMARY KEY AUTOINCREMENT,"
          "title      TEXT    NOT NULL,"
          "done       INTEGER NOT NULL DEFAULT 0,"
          "created_at TEXT    NOT NULL DEFAULT (datetime('now'))"
        ");",

        "CREATE TABLE stats ("
          "id         INTEGER PRIMARY KEY CHECK (id = 1),"
          "total      INTEGER NOT NULL DEFAULT 0,"
          "done       INTEGER NOT NULL DEFAULT 0,"
          "updated_at TEXT    NOT NULL DEFAULT (datetime('now'))"
        ");",

        "CREATE TABLE activity_log ("
          "id         INTEGER PRIMARY KEY AUTOINCREMENT,"
          "action     TEXT NOT NULL,"
          "detail     TEXT,"
          "ran_at     TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
      },
      .seeds = {
        "INSERT INTO todos(title) VALUES"
          "('Learn the MACH tasks API'),"
          "('Build something cool'),"
          "('Ship it');",
        "INSERT INTO stats(total, done, updated_at) VALUES"
          "(3, 0, datetime('now'));"
      }
    }},

    .modules = {sqlite, tailwind}
  };
}
