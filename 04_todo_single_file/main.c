#include <mach.h>
#include <sqlite.h>
#include <mustache.h>
#include <session_auth.h>

void list_todos(){
  await_assign_db_query("todos", (q){.db = "todos_db",
    "select id, title, finished "
    "from todos "
    "where user_id = {{user_id}};"
  });
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "todos"});
}

void create_todo(){
  db_query((q){.db = "todos_db", 
    "insert into todos(user_id, title) "
    "values({{user_id}}, {{title}});"
  });
  redirect("todos");
}

void update_todo(){
  db_find((q){.db = "todos_db",
    "update todos "
    "set finished = {{finished}} "
    "where user_id = {{user_id}} "
      "and id = {{id}};"
  });
  redirect("todos");
}

void delete_todo(){
  db_find((q){.db = "todos_db",
    "delete from todos "
    "where user_id = {{user_id}} "
      "and id = {{id}};"
  });
  redirect("todos");
}

config mach(){
  return (config) {
    .uses = {"session"},
    .includes = {
      mustache_config, 
      sqlite_config,
      session_auth_config
    },
    .databases = {{
      .engine = sqlite,
      .name = "todos_db",
      .connect = "file:todo.db?mode=rwc",
      .migrations = {
        "CREATE TABLE IF NOT EXISTS todos ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "user_id INTEGER NOT NULL, "
          "title TEXT NOT NULL, "
          "finished INTEGER CHECK(finished IN (1))"
        "); "
        "CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);"
      }
    }},
    .routes = {
      {.status = http_error, .render = "5xx"},
      {.status = http_not_found, .render = "404"},
      {"/", http_get, "home", .render = "home"},
      {"/about", http_get, "about", .render = "about"},
      {"/contact", http_get, "contact", .render = "contact"},
      {"/todos", http_post, .call = create_todo, .uses = {"logged_in"}},
      {"/todos/:id", http_patch, .call = update_todo, .uses = {"logged_in"}},
      {"/todos/:id", http_delete, .call = delete_todo, .uses = {"logged_in"}},
      {"/todos", http_get, "todos", .call = list_todos, .uses = {"logged_in"}}
    },
    .resources = {
      {"layout",
        "<html>"
          "<head>"
            "<link rel='icon' href='/favicon.png'>"
          "</head>"
          "<body>"
            "<p>"
              "{{^user}}"
                "<a href='{{:url:login}}'>sign in</a>"
              "{{/user}}"
              "{{#user}}"
                "welcome, {{first_name}}"
              "{{/user}}"
            "</p>"
            "<nav>"
              "<a href='{{:url:home}}'>home</a>"
              "<a href='{{:url:about}}'>about us</a>"
              "<a href='{{:url:contact}}'>contact us</a>"
            "</nav>"
            "{{$body}}"
            "{{/body}}"
          "</body>"
        "</html>"
      },
      {"404",
        "{{< layout}}"
          "{{$body}}"
            "<p>not found</p>"
          "{{/body}}"
        "{{/layout}}"
      },
      {"5xx",
        "{{< layout}}"
          "{{$body}}"
            "<p>error</p>"
          "{{/body}}"
        "{{/layout}}"
      },
      {"home",
        "{{< layout}}"
          "{{$body}}"
            "<p>home</p>"
          "{{/body}}"
        "{{/layout}}"
      },
      {"about",
        "{{< layout}}"
          "{{$body}}"
            "<p>about us</p>"
          "{{/body}}"
        "{{/layout}}"
      },
      {"contact",
        "{{< layout}}"
          "{{$body}}"
            "<p>contact us</p>"
          "{{/body}}"
        "{{/layout}}"
      },
      {"todos",
        "{{< layout}}"
          "{{$body}}"
            "<form action='/todos' method='post'>"
              "<input type='text' name='title' placeholder='new todo' required>"
              "<button type='submit'>add</button>"
            "</form>"
            "{{^todos}}"
              "<p>no todos</p>"
            "{{/todos}}"
            "{{#todos}}"
              "{{#.}}"
                "{{> todo}}"
              "{{/.}}"
            "{{/todos}}"
          "{{/body}}"
        "{{/layout}}"
      },
      {"todo",
        "<div>"
          "<form action='/todos/{{id}}' method='post' style='display:inline'>"
            "<input type='hidden' name='http_method' value='PATCH'>"
            "{{^finished}}"
              "<input type='checkbox' name='finished' value='1'>"
            "{{/finished}}"
            "{{#finished}}"
              "<input type='checkbox' name='finished' value='1' checked>"
            "{{/finished}}"
            "{{title}}"
            "<button type='submit'>save</button>"
          "</form>"
          "<form action='/todos/{{id}}' method='post' style='display:inline'>"
            "<input type='hidden' name='http_method' value='delete'>"
            "<button type='submit'>delete</button>"
          "</form>"
        "</div>"
      }
    }
  };
}
