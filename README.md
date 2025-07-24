# MACH

## Why MACH

MACH (Modern Asynchronous C Hypermedia) is a declarative framework for building asynchronous web applications in C23.

* **No build configuration.** Compilation, hot reload, and dependency wiring are handled by the framework. There are no build scripts, package managers, or ORMs to set up.
* **Memory, concurrency, and I/O managed by the framework.** Application code does not call `malloc`/`free` or manage threads, mutexes, or locks. Database queries run as prepared statements. Pipeline steps emit OpenTelemetry spans, logs, and errors automatically.
* **Durable tasks and events.** Both are persisted. If the process crashes, incomplete tasks resume at the step where they left off and undelivered events replay on the next boot.
* **Bundled modules.** SSE plus modules for Datastar, HTMX, Tailwind, SQLite, Postgres, MySQL, Redis/Valkey, DuckDB, and auth. Multi-tenant database support is built in.

---

## Table of Contents
* [Quick Start](#quick-start)
* [Philosophy](#philosophy)
* [Guide](#guide)
* [Reference](#reference)
* [Architecture](#architecture)
* [Tooling](#tooling)
* [License](#license)

---

## Quick Start

Everything runs in Docker; no other local dependencies are required.

```bash
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml

# Dev server on :3000, telemetry on :4000
# Includes file watching, auto compilation, hot code reloading, HMR
docker compose up
```

Create `main.c` with the example below. MACH watches for changes and hot-reloads on save. Use your own editor, or attach to the built-in TUI with `docker compose attach mach` for an integrated environment with editor, AI, LSP, and console.

```c
#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          render(.template = "<h1>Hello, world!</h1>")
        }
      }
    }
  };
}
```

The `mach()` function returns a `config` struct that defines the application. The `home` resource maps `/` to a GET pipeline whose only step renders inline HTML. For a step-by-step walkthrough, see the [Guide](#guide).

---

## Philosophy

Applications are data transformations: data enters from sources, flows through business logic, and exits to the client. MACH keeps each piece standard. Data comes from raw SQL, HTTP fetch, and JSON rather than ORMs. Business logic is plain C. Output is HTML, CSS, and JS via Mustache templates. These pieces compose inside pipelines: ordered lists of steps that turn a request into a response.

Tooling is also kept standard (lldb for debugging, Playwright and Criterion for testing, OpenTelemetry for observability) and built in.

### Everything is a String

The web is largely text: HTTP, HTML, JSON, SQL. MACH takes this literally. The pipeline context stores and passes data as arena-backed strings; there is no intermediate parsing or serialization layer. Request parameters are not parsed into typed structs and objects are not serialized back to JSON. Data flows through the pipeline as strings, interpolated into SQL, templates, and URLs with `{{context_key}}`.

When business logic needs a specific C type, convert explicitly inside an `exec()` step.

### CLAD

MACH is organized around four principles.

* **(C)omposable:** small, independent steps chain into feature pipelines.
* **(L)ocality of Behavior:** the behavior of a unit of code is apparent from reading it. SQL, templates, and behavior for a feature live together rather than across separate model, view, and controller trees.
* **(A)utonomous:** modules are self-contained: own schemas, migrations, seeds, routes, UI, and logic. The compiler enforces strict boundaries.
* **(D)omain Based:** each module owns one slice of the app. A `todos` module defines everything related to todos and nothing else.

CLAD is influenced by:

* [Data Oriented Design](https://youtu.be/rX0ItVEVjHc)
* [A Philosophy of Software Design](https://youtu.be/bmSAYlu0NcY)
* [CUPID](https://youtu.be/cyZDLjLuQ9g)
* [Self-Contained Systems](https://youtu.be/Jjrencq8sUQ)
* [Locality of Behavior](https://htmx.org/essays/locality-of-behaviour)

---

## Guide

A walkthrough that builds a working todo app, introducing one MACH concept at a time.

* [1. A Page](#1-a-page)
* [2. Show Data](#2-show-data)
* [3. Accept Input](#3-accept-input)
* [4. Handle Errors](#4-handle-errors)
* [5. Nested Data](#5-nested-data)
* [6. Tasks](#6-tasks)
* [7. Modules & Events](#7-modules-events)
* [8. Calling APIs](#8-calling-apis)
* [9. External Assets](#9-external-assets)
* [10. Shared Templates](#10-shared-templates)
* [11. Final State](#11-final-state)

### 1. A Page

Two resources, each with a GET pipeline. `{{url:todos}}` resolves to the target resource's URL at render time, so changing a URL pattern updates every link.

```c
#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          render(.template =
            "<html><body>"
              "<h1>Welcome</h1>"
              "<a href='{{url:todos}}'>My Todos</a>"
            "</body></html>"
          )
        }
      },

      {"todos", "/todos",
        .get = {
          render(.template =
            "<html><body>"
              "<h1>My Todos</h1>"
              "<p>Nothing yet.</p>"
            "</body></html>"
          )
        }
      }
    }
  };
}
```

Each resource names itself (`"home"`, `"todos"`) so other pages reference it by name, not by hard-coded path.

### 2. Show Data

Add a SQLite database with one migration and one seed, then read from it in the GET pipeline.

```diff
  #include <mach.h>
+ #include <sqlite.h>

  config mach(){
    return (config) {
      .resources = {
        {"home", "/",
          .get = {
            render(.template =
              "<html><body>"
                "<h1>Welcome</h1>"
                "<a href='{{url:todos}}'>My Todos</a>"
              "</body></html>"
            )
          }
        },

        {"todos", "/todos",
          .get = {
+           query({.set_key = "todos", .db = "todos_db",
+             .query = "select id, title from todos;"}),
            render(.template =
              "<html><body>"
                "<h1>My Todos</h1>"
-               "<p>Nothing yet.</p>"
+               "<ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul>"
              "</body></html>"
            )
          }
        }
-     }
+     },
+
+     .databases = {{
+       .engine     = sqlite_db,
+       .name       = "todos_db",
+       .connect    = "file:todos.db?mode=rwc",
+       .migrations = {
+         "CREATE TABLE todos ("
+           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
+           "title TEXT NOT NULL"
+         ");"
+       },
+       .seeds = {"INSERT INTO todos(title) VALUES('Learn MACH');"}
+     }},
+
+     .modules = {sqlite}
    };
  }
```

`query` runs the SELECT and stores the rows under `todos` in pipeline context. `render` walks the section with `{{#todos}}...{{/todos}}`. Migrations run on the first connection.

### 3. Accept Input

Add a POST verb that validates a `title` parameter, inserts it, and redirects back to GET (POST-redirect-GET).

```diff
  #include <mach.h>
  #include <sqlite.h>

  config mach(){
    return (config) {
      .resources = {
        {"home", "/",
          .get = {
            render(.template =
              "<html><body>"
                "<h1>Welcome</h1>"
                "<a href='{{url:todos}}'>My Todos</a>"
              "</body></html>"
            )
          }
        },

        {"todos", "/todos",
          .get = {
            query({.set_key = "todos", .db = "todos_db",
              .query = "select id, title from todos;"}),
            render(.template =
              "<html><body>"
                "<h1>My Todos</h1>"
                "<ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul>"
+               "<form method='post' action='{{url:todos}}'>"
+                 "{{csrf:input}}"
+                 "<input name='title' value='{{input:title}}'>"
+                 "<button>Add</button>"
+               "</form>"
              "</body></html>"
            )
-         }
+         },
+
+         .post = {
+           validate({"title",
+             .validation = validate_not_empty,
+             .message    = "title cannot be empty"}),
+           query({.db = "todos_db",
+             .query = "insert into todos(title) values({{title}});"}),
+           redirect("todos")
+         }
        }
      },

      .databases = {{
        .engine     = sqlite_db,
        .name       = "todos_db",
        .connect    = "file:todos.db?mode=rwc",
        .migrations = {
          "CREATE TABLE todos ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "title TEXT NOT NULL"
          ");"
        },
        .seeds = {"INSERT INTO todos(title) VALUES('Learn MACH');"}
      }},

      .modules = {sqlite}
    };
  }
```

The POST pipeline validates first; on success, the title is promoted from `input:title` to app scope. The interpolated `{{title}}` in the SQL is bound as a prepared-statement parameter, not spliced. `redirect("todos")` returns a 302 to `/todos`.

`{{csrf:input}}` emits a hidden input carrying a CSRF token; on POST, MACH automatically verifies that the submitted token matches the one set on the cookie and rejects mismatches with a 403, so every state-changing form needs it.

### 4. Handle Errors

Validation failure raises `http_bad_request`. Add a resource-scoped error handler that re-enters the GET pipeline with `reroute("todos")`, and add error markup to the form template.

```diff
  #include <mach.h>
  #include <sqlite.h>

  config mach(){
    return (config) {
      .resources = {
        {"home", "/",
          .get = {
            render(.template =
              "<html><body>"
                "<h1>Welcome</h1>"
                "<a href='{{url:todos}}'>My Todos</a>"
              "</body></html>"
            )
          }
        },

        {"todos", "/todos",
          .get = {
            query({.set_key = "todos", .db = "todos_db",
              .query = "select id, title from todos;"}),
            render(.template =
              "<html><body>"
                "<h1>My Todos</h1>"
                "<ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul>"
                "<form method='post' action='{{url:todos}}'>"
                  "{{csrf:input}}"
                  "<input name='title' value='{{input:title}}'>"
+                 "{{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}"
                  "<button>Add</button>"
                "</form>"
              "</body></html>"
            )
          },

          .post = {
            validate({"title",
              .validation = validate_not_empty,
              .message    = "title cannot be empty"}),
            query({.db = "todos_db",
              .query = "insert into todos(title) values({{title}});"}),
            redirect("todos")
-         }
+         },
+
+         .errors = {
+           {http_bad_request, { reroute("todos") }}
+         }
        }
      },

      .databases = {{
        .engine     = sqlite_db,
        .name       = "todos_db",
        .connect    = "file:todos.db?mode=rwc",
        .migrations = {
          "CREATE TABLE todos ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "title TEXT NOT NULL"
          ");"
        },
        .seeds = {"INSERT INTO todos(title) VALUES('Learn MACH');"}
      }},

      .modules = {sqlite}
    };
  }
```

`reroute("todos")` re-enters the GET pipeline in-process, which already knows how to fetch todos and render the page. The `input:` and `error:` scopes persist through the reroute, so `{{input:title}}` repopulates the field and `{{#error:title}}` renders the message. See [redirect & reroute](#redirect-reroute).

### 5. Nested Data

Add a `/todos/:id` page that fetches a todo and its comments concurrently, nests the comments inside the todo record, and renders them together. Comments belong to the same domain as todos, so the new `comments` table is added as a migration on the existing `todos_db`.

```diff
  #include <mach.h>
  #include <sqlite.h>

  config mach(){
    return (config) {
      .resources = {
        {"home", "/",
          .get = {
            render(.template =
              "<html><body>"
                "<h1>Welcome</h1>"
                "<a href='{{url:todos}}'>My Todos</a>"
              "</body></html>"
            )
          }
        },

        {"todos", "/todos",
          .get = {
            query({.set_key = "todos", .db = "todos_db",
              .query = "select id, title from todos;"}),
            render(.template =
              "<html><body>"
                "<h1>My Todos</h1>"
-               "<ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul>"
+               "<ul>{{#todos}}<li><a href='{{url:todo:id}}'>{{title}}</a></li>{{/todos}}</ul>"
                "<form method='post' action='{{url:todos}}'>"
                  "{{csrf:input}}"
                  "<input name='title' value='{{input:title}}'>"
                  "{{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}"
                  "<button>Add</button>"
                "</form>"
              "</body></html>"
            )
          },

          .post = {
            validate({"title",
              .validation = validate_not_empty,
              .message    = "title cannot be empty"}),
            query({.db = "todos_db",
              .query = "insert into todos(title) values({{title}});"}),
            redirect("todos")
          },

          .errors = {
            {http_bad_request, { reroute("todos") }}
          }
-       }
+       },
+
+       {"todo", "/todos/:id",
+         .get = {
+           validate({"id", .validation = validate_integer,
+             .message = "must be an integer"}),
+           query(
+             {.set_key = "todo",     .db = "todos_db",
+              .query = "select id, title from todos where id = {{id}};"},
+             {.set_key = "comments", .db = "todos_db",
+              .query = "select id, todo_id, body from comments where todo_id = {{id}};"}
+           ),
+           join(
+             .target_table_key      = "todo",
+             .target_field_key      = "id",
+             .nested_table_key      = "comments",
+             .nested_field_key      = "todo_id",
+             .target_join_field_key = "comments"
+           ),
+           render(.template =
+             "<html><body>"
+               "{{#todo}}"
+                 "<h1>{{title}}</h1>"
+                 "<h2>Comments</h2>"
+                 "<ul>{{#comments}}<li>{{body}}</li>{{/comments}}</ul>"
+               "{{/todo}}"
+             "</body></html>"
+           )
+         }
+       }
      },

      .databases = {{
        .engine     = sqlite_db,
        .name       = "todos_db",
        .connect    = "file:todos.db?mode=rwc",
        .migrations = {
          "CREATE TABLE todos ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "title TEXT NOT NULL"
-         ");"
+         ");",
+         "CREATE TABLE comments ("
+           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
+           "todo_id INTEGER NOT NULL REFERENCES todos(id),"
+           "body TEXT NOT NULL"
+         ");"
        },
        .seeds = {"INSERT INTO todos(title) VALUES('Learn MACH');"}
      }},

      .modules = {sqlite}
    };
  }
```

The two queries run in parallel under one `query()` call. `join()` lifts `comments` inside each todo record, so the template enters `{{#todo}}` first and reaches `{{#comments}}` from within.

### 6. Tasks

Tasks are named pipelines that run asynchronously on task reactors. Triggered on a cron schedule or enqueued from another pipeline with `task("name")`. Add a nightly task that records the current todo count into a `daily_stats` table, and re-run the same task from the POST pipeline so stats stay fresh after every write.

```diff
  #include <mach.h>
  #include <sqlite.h>

  config mach(){
    return (config) {
      .resources = {
        {"home", "/",
          .get = {
            render(.template =
              "<html><body>"
                "<h1>Welcome</h1>"
                "<a href='{{url:todos}}'>My Todos</a>"
              "</body></html>"
            )
          }
        },

        {"todos", "/todos",
          .get = {
            query({.set_key = "todos", .db = "todos_db",
              .query = "select id, title from todos;"}),
            render(.template =
              "<html><body>"
                "<h1>My Todos</h1>"
                "<ul>{{#todos}}<li><a href='{{url:todo:id}}'>{{title}}</a></li>{{/todos}}</ul>"
                "<form method='post' action='{{url:todos}}'>"
                  "{{csrf:input}}"
                  "<input name='title' value='{{input:title}}'>"
                  "{{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}"
                  "<button>Add</button>"
                "</form>"
              "</body></html>"
            )
          },

          .post = {
            validate({"title",
              .validation = validate_not_empty,
              .message    = "title cannot be empty"}),
            query({.db = "todos_db",
              .query = "insert into todos(title) values({{title}});"}),
+           task("record_daily_stats"),
            redirect("todos")
          },

          .errors = {
            {http_bad_request, { reroute("todos") }}
          }
        },

        {"todo", "/todos/:id",
          .get = {
            validate({"id", .validation = validate_integer,
              .message = "must be an integer"}),
            query(
              {.set_key = "todo",     .db = "todos_db",
               .query = "select id, title from todos where id = {{id}};"},
              {.set_key = "comments", .db = "todos_db",
               .query = "select id, todo_id, body from comments where todo_id = {{id}};"}
            ),
            join(
              .target_table_key      = "todo",
              .target_field_key      = "id",
              .nested_table_key      = "comments",
              .nested_field_key      = "todo_id",
              .target_join_field_key = "comments"
            ),
            render(.template =
              "<html><body>"
                "{{#todo}}"
                  "<h1>{{title}}</h1>"
                  "<h2>Comments</h2>"
                  "<ul>{{#comments}}<li>{{body}}</li>{{/comments}}</ul>"
                "{{/todo}}"
              "</body></html>"
            )
          }
        }
      },

+     .tasks = {
+       {"record_daily_stats", {
+         query({.db = "todos_db",
+           .query = "insert into daily_stats(todo_count) "
+                    "select count(*) from todos;"})
+       }, .cron = "0 0 * * *"}
+     },

      .databases = {{
        .engine     = sqlite_db,
        .name       = "todos_db",
        .connect    = "file:todos.db?mode=rwc",
        .migrations = {
          "CREATE TABLE todos ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "title TEXT NOT NULL"
          ");",
          "CREATE TABLE comments ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "todo_id INTEGER NOT NULL REFERENCES todos(id),"
            "body TEXT NOT NULL"
-         ");"
+         ");",
+         "CREATE TABLE daily_stats ("
+           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
+           "recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
+           "todo_count INTEGER NOT NULL"
+         ");"
        },
        .seeds = {"INSERT INTO todos(title) VALUES('Learn MACH');"}
      }},

      .modules = {sqlite}
    };
  }
```

The same task definition is reused two ways: `.cron = "0 0 * * *"` runs it at midnight, and `task("record_daily_stats")` from the POST pipeline enqueues an on-demand run after each insert. Both invocations land on a task reactor, on separate cores from the request reactors that serve HTTP, so the POST returns immediately. To pass values from the calling context (a `user_id`, a `todo_id`), list them under `.accepts` on the task definition and reference them with `{{user_id}}` interpolation.

Tasks are durable: MACH checkpoints the context after each step, so a crash mid-task resumes at the failed step on the next boot. See [Task Pipelines](#task-pipelines).

### 7. Modules & Events

Features/Domains split into modules as the app grows; modules communicate through pub/sub events instead of calling each other directly. A module is a fully self-contained system: its own resources, databases, migrations, context, error and repair handlers, tasks, and event subscribers. `main.c` composes them and handles cross-cutting concerns.

This step extracts the todos logic into its own module and adds an `activity` module that logs an entry whenever a todo is created and exposes a page to view the log.

Modules are plain C files. Each defines a function returning `config` (for example, `config todos() { ... }`, `config activity() { ... }`), and `main.c` pulls them in with `#include` and registers them under `.modules`.

Directory layout after this step:

```
.
├── activity/
│   └── activity.c
├── todos/
│   └── todos.c
└── main.c
```

**`main.c`**: thin root that composes modules and handles cross-cutting concerns.

```c
#include <mach.h>
#include <sqlite.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config){
    .resources = {
      {"home", "/",
        .get = {
          render(.template =
            "<html><body>"
              "<h1>Welcome</h1>"
              "<a href='{{url:todos}}'>My Todos</a> · "
              "<a href='{{url:activity}}'>Activity</a>"
            "</body></html>"
          )
        }
      }
    },

    .modules = {todos, activity, sqlite}
  };
}
```

**`todos/todos.c`**: the todos module, now a publisher.

```c
#include <mach.h>
#include <sqlite.h>

config todos(){
  return (config){
    .name = "todos",

    .publishes = {
      {"todo_created", .with = {"title"}}
    },

    .resources = {
      {"todos", "/todos",
        .get = {
          query({.set_key = "todos", .db = "todos_db",
            .query = "select id, title from todos;"}),
          render(.template =
            "<html><body>"
              "<h1>My Todos</h1>"
              "<ul>{{#todos}}<li><a href='{{url:todo:id}}'>{{title}}</a></li>{{/todos}}</ul>"
              "<form method='post' action='{{url:todos}}'>"
                "{{csrf:input}}"
                "<input name='title' value='{{input:title}}'>"
                "{{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}"
                "<button>Add</button>"
              "</form>"
            "</body></html>"
          )
        },

        .post = {
          validate({"title",
            .validation = validate_not_empty,
            .message    = "title cannot be empty"}),
          query({.db = "todos_db",
            .query = "insert into todos(title) values({{title}});"}),
          task("record_daily_stats"),
          emit("todo_created"),
          redirect("todos")
        },

        .errors = {
          {http_bad_request, { reroute("todos") }}
        }
      },

      {"todo", "/todos/:id",
        .get = {
          validate({"id", .validation = validate_integer,
            .message = "must be an integer"}),
          query(
            {.set_key = "todo",     .db = "todos_db",
             .query = "select id, title from todos where id = {{id}};"},
            {.set_key = "comments", .db = "todos_db",
             .query = "select id, todo_id, body from comments where todo_id = {{id}};"}
          ),
          join(
            .target_table_key      = "todo",
            .target_field_key      = "id",
            .nested_table_key      = "comments",
            .nested_field_key      = "todo_id",
            .target_join_field_key = "comments"
          ),
          render(.template =
            "<html><body>"
              "{{#todo}}"
                "<h1>{{title}}</h1>"
                "<h2>Comments</h2>"
                "<ul>{{#comments}}<li>{{body}}</li>{{/comments}}</ul>"
              "{{/todo}}"
            "</body></html>"
          )
        }
      }
    },

    .tasks = {
      {"record_daily_stats", {
        query({.db = "todos_db",
          .query = "insert into daily_stats(todo_count) "
                   "select count(*) from todos;"})
      }, .cron = "0 0 * * *"}
    },

    .databases = {{
      .engine     = sqlite_db,
      .name       = "todos_db",
      .connect    = "file:todos.db?mode=rwc",
      .migrations = {
        "CREATE TABLE todos ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "title TEXT NOT NULL"
        ");",
        "CREATE TABLE comments ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "todo_id INTEGER NOT NULL REFERENCES todos(id),"
          "body TEXT NOT NULL"
        ");",
        "CREATE TABLE daily_stats ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
          "todo_count INTEGER NOT NULL"
        ");"
      },
      .seeds = {"INSERT INTO todos(title) VALUES('Learn MACH');"}
    }},

    .modules = {sqlite}
  };
}
```

**`activity/activity.c`**: the new subscriber: its own database, its own resource, its own event handler. Nothing references the todos module.

```c
#include <mach.h>
#include <sqlite.h>

config activity(){
  return (config){
    .name = "activity",

    .resources = {
      {"activity", "/activity",
        .get = {
          query({.set_key = "activities", .db = "activity_db",
            .query = "select kind, ref, created_at from activities "
                     "order by created_at desc;"}),
          render(.template =
            "<html><body>"
              "<h1>Activity</h1>"
              "<ul>{{#activities}}"
                "<li>{{created_at}}: {{kind}}, {{ref}}</li>"
              "{{/activities}}</ul>"
            "</body></html>"
          )
        }
      }
    },

    .events = {
      {"todo_created", {
        query({.db = "activity_db",
          .query = "insert into activities(kind, ref) "
                   "values('created', {{title}});"})
      }}
    },

    .databases = {{
      .engine     = sqlite_db,
      .name       = "activity_db",
      .connect    = "file:activity.db?mode=rwc",
      .migrations = {
        "CREATE TABLE activities ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "kind TEXT NOT NULL,"
          "ref TEXT NOT NULL,"
          "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
      }
    }},

    .modules = {sqlite}
  };
}
```

Each module owns one slice of the app. `todos` owns the todos domain (resources, database, publisher contract, scheduled task); `activity` owns activity (its resource, database, subscriber). Both can also declare their own context, error and repair handlers, and nested modules. Neither references the other. They only agree on the event name and payload.

When the POST pipeline in `todos` calls `emit("todo_created")`, MACH propagates `title` from the current context to every subscriber's pipeline. The `activity` module's `.events` entry runs with `title` available, writing the row to `activity_db`. Because `.publishes` is defined, MACH tracks delivery in a `mach_events` database, so if the process crashes between emit and delivery the event replays on the next boot. Adding a third subscriber is a new file with an `.events` entry; the `todos` module doesn't change.

### 8. Calling APIs

Pipelines reach external HTTP services the same way they read from databases. `fetch()` makes one or more requests and stores responses in context; JSON is parsed into tables and records, with nested JSON becoming nested context tables. Multiple items in a single `fetch()` run **concurrently**, just like `query()`. Add a quote of the day and the current weather to the home page, pulled from two public APIs in parallel.

**`main.c`**

```diff
  #include <mach.h>
  #include <sqlite.h>
  #include "todos/todos.c"
  #include "activity/activity.c"

  config mach(){
    return (config){
      .resources = {
        {"home", "/",
          .get = {
+           fetch(
+             {"https://api.quotable.io/random",  .set_key = "quote"},
+             {"https://api.weather.dev/now",     .set_key = "weather"}
+           ),
            render(.template =
              "<html><body>"
                "<h1>Welcome</h1>"
+               "{{#weather}}"
+                 "<p>{{city}}: {{precision:temp_c:0}}°C, {{conditions}}</p>"
+               "{{/weather}}"
+               "{{#quote}}"
+                 "<blockquote>{{content}}, {{author}}</blockquote>"
+               "{{/quote}}"
                "<a href='{{url:todos}}'>My Todos</a> · "
                "<a href='{{url:activity}}'>Activity</a>"
              "</body></html>"
            )
          }
        }
      },

      .modules = {todos, activity, sqlite}
    };
  }
```

Both requests run in parallel under one `fetch()` call, then control passes to `render()` once both responses are in context. The Quotable API returns `{"author": "...", "content": "..."}`, which MACH parses into a single-row table under `quote`; the weather response lands under `weather` the same way. The template enters each section with `{{#quote}}` and `{{#weather}}` to read its fields, the same way it does for query results.

`fetch()` also supports POST/PUT/PATCH/DELETE, custom headers, JSON or text request bodies, and URLs with `{{interpolation}}`. See [fetch](#fetch).

### 9. External Assets

Once templates and SQL grow past a few lines, extract them into files and load them with `(asset){ #embed "..." }` in `.context`, then reference them by name from `render()`, `query()`, and `find()`. Each artifact lives in a file named for what it is, edited with native tooling (Mustache-aware HTML editors, SQL formatters), and syntax-highlighted on its own terms. Apply throughout: the root, the todos module, and the activity module.

Directory layout after this step:

```
.
├── activity/
│   ├── activity.c
│   ├── activity.mustache.html
│   ├── create_activities_table.sql
│   ├── get_activities.sql
│   └── insert_activity.sql
├── static/
│   └── home.mustache.html
├── todos/
│   ├── create_todos_table.sql
│   ├── create_comments_table.sql
│   ├── create_daily_stats_table.sql
│   ├── seed_todos.sql
│   ├── get_todos.sql
│   ├── create_todo.sql
│   ├── get_todo.sql
│   ├── get_comments.sql
│   ├── record_daily_stats.sql
│   ├── todos.c
│   ├── todos_list.mustache.html
│   └── todo_detail.mustache.html
└── main.c
```

`static/` is not a module (no `.c` file). It's a plain directory for root-level templates that `main.c` references.

#### Root

**`static/home.mustache.html`**

```html
<html><body>
  <h1>Welcome</h1>
  {{#weather}}
    <p>{{city}}: {{precision:temp_c:0}}°C, {{conditions}}</p>
  {{/weather}}
  {{#quote}}
    <blockquote>{{content}}, {{author}}</blockquote>
  {{/quote}}
  <a href='{{url:todos}}'>My Todos</a> · <a href='{{url:activity}}'>Activity</a>
</body></html>
```

**`main.c`**: references the home template by name; the `fetch()` step from the previous section is unchanged.

```c
#include <mach.h>
#include <sqlite.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config){
    .resources = {
      {"home", "/",
        .get = {
          fetch(
            {"https://api.quotable.io/random", .set_key = "quote"},
            {"https://api.weather.dev/now",    .set_key = "weather"}
          ),
          render("home")
        }
      }
    },

    .context = {
      {"home", (asset){
        #embed "static/home.mustache.html"
      }}
    },

    .modules = {todos, activity, sqlite}
  };
}
```

#### Todos module

**`todos/todos_list.mustache.html`**

```html
<html><body>
  <h1>My Todos</h1>
  <ul>{{#todos}}<li><a href='{{url:todo:id}}'>{{title}}</a></li>{{/todos}}</ul>
  <form method='post' action='{{url:todos}}'>
    {{csrf:input}}
    <input name='title' value='{{input:title}}'>
    {{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}
    <button>Add</button>
  </form>
</body></html>
```

**`todos/get_todos.sql`**: and similarly for each other SQL file.

```sql
select id, title from todos;
```

**`todos/create_todo.sql`**

```sql
insert into todos(title) values({{title}});
```

SQL `{{interpolation}}` works the same as inline: bound as prepared parameters, never spliced. Migration files are plain `CREATE TABLE` statements, one per file. `todos/todo_detail.mustache.html` holds the detail-page template from step 5.

**`todos/todos.c`**: pipelines now reference assets by name; `.context` lists every asset the module uses.

```c
#include <mach.h>
#include <sqlite.h>

config todos(){
  return (config){
    .name = "todos",

    .publishes = {
      {"todo_created", .with = {"title"}}
    },

    .resources = {
      {"todos", "/todos",
        .get = {
          query({"get_todos", .set_key = "todos", .db = "todos_db"}),
          render("todos_list")
        },

        .post = {
          validate({"title",
            .validation = validate_not_empty,
            .message    = "title cannot be empty"}),
          query({"create_todo", .db = "todos_db"}),
          task("record_daily_stats"),
          emit("todo_created"),
          redirect("todos")
        },

        .errors = {
          {http_bad_request, { reroute("todos") }}
        }
      },

      {"todo", "/todos/:id",
        .get = {
          validate({"id", .validation = validate_integer,
            .message = "must be an integer"}),
          query(
            {"get_todo",     .set_key = "todo",     .db = "todos_db"},
            {"get_comments", .set_key = "comments", .db = "todos_db"}
          ),
          join(
            .target_table_key      = "todo",
            .target_field_key      = "id",
            .nested_table_key      = "comments",
            .nested_field_key      = "todo_id",
            .target_join_field_key = "comments"
          ),
          render("todo_detail")
        }
      }
    },

    .tasks = {
      {"record_daily_stats", {
        query({"record_daily_stats", .db = "todos_db"})
      }, .cron = "0 0 * * *"}
    },

    .context = {
      {"todos_list", (asset){
        #embed "todos_list.mustache.html"
      }},
      {"todo_detail", (asset){
        #embed "todo_detail.mustache.html"
      }},
      {"get_todos", (asset){
        #embed "get_todos.sql"
      }},
      {"create_todo", (asset){
        #embed "create_todo.sql"
      }},
      {"get_todo", (asset){
        #embed "get_todo.sql"
      }},
      {"get_comments", (asset){
        #embed "get_comments.sql"
      }},
      {"record_daily_stats", (asset){
        #embed "record_daily_stats.sql"
      }}
    },

    .databases = {{
      .engine     = sqlite_db,
      .name       = "todos_db",
      .connect    = "file:todos.db?mode=rwc",
      .migrations = {
        (asset){
          #embed "create_todos_table.sql"
        },
        (asset){
          #embed "create_comments_table.sql"
        },
        (asset){
          #embed "create_daily_stats_table.sql"
        }
      },
      .seeds = {
        (asset){
          #embed "seed_todos.sql"
        }
      }
    }},

    .modules = {sqlite}
  };
}
```

#### Activity module

**`activity/activity.mustache.html`**

```html
<html><body>
  <h1>Activity</h1>
  <ul>{{#activities}}
    <li>{{created_at}}: {{kind}}, {{ref}}</li>
  {{/activities}}</ul>
</body></html>
```

**`activity/insert_activity.sql`**: the event handler's insert, now a named asset.

```sql
insert into activities(kind, ref) values('created', {{title}});
```

**`activity/activity.c`**: same extraction pattern.

```c
#include <mach.h>
#include <sqlite.h>

config activity(){
  return (config){
    .name = "activity",

    .resources = {
      {"activity", "/activity",
        .get = {
          query({"get_activities", .set_key = "activities", .db = "activity_db"}),
          render("activity")
        }
      }
    },

    .events = {
      {"todo_created", {
        query({"insert_activity", .db = "activity_db"})
      }}
    },

    .context = {
      {"activity", (asset){
        #embed "activity.mustache.html"
      }},
      {"get_activities", (asset){
        #embed "get_activities.sql"
      }},
      {"insert_activity", (asset){
        #embed "insert_activity.sql"
      }}
    },

    .databases = {{
      .engine     = sqlite_db,
      .name       = "activity_db",
      .connect    = "file:activity.db?mode=rwc",
      .migrations = {
        (asset){
          #embed "create_activities_table.sql"
        }
      }
    }},

    .modules = {sqlite}
  };
}
```

The pipeline shape is unchanged; only where the strings live has moved. Each `render()`, `query()`, and `find()` step takes an asset name as its positional argument; MACH resolves it against `.context` top-down from the root at boot time (root wins on name conflicts). `.migrations` and `.seeds` accept assets directly, which is why entries now read `(asset){ #embed "..." }` instead of quoted SQL. Templates are still Mustache; SQL still supports `{{interpolation}}` bound as prepared parameters.

### 10. Shared Templates

Each page template from the previous step repeats the same `<html><body>...</body></html>` chrome and its own variant of a top nav. Mustache layout inheritance pulls the shared structure into one template that the others extend.

**`static/layout.mustache.html`**: the shared chrome, with a `{{$content}}` block that child templates override.

```html
<html><body>
  <nav>
    <a href='{{url:home}}'>Home</a> ·
    <a href='{{url:todos}}'>My Todos</a> ·
    <a href='{{url:activity}}'>Activity</a>
  </nav>
  <main>{{$content}}{{/content}}</main>
</body></html>
```

Register it once at the root so every module can use it.

```c
.context = {
  {"home", (asset){
    #embed "static/home.mustache.html"
  }},
  {"layout", (asset){
    #embed "static/layout.mustache.html"
  }}
}
```

Each page now extends `layout` and supplies its own `{{$content}}`.

**`static/home.mustache.html`**

```html
{{<layout}}
  {{$content}}
    <h1>Welcome</h1>
    {{#weather}}
      <p>{{city}}: {{precision:temp_c:0}}°C, {{conditions}}</p>
    {{/weather}}
    {{#quote}}
      <blockquote>{{content}}, {{author}}</blockquote>
    {{/quote}}
  {{/content}}
{{/layout}}
```

**`todos/todos_list.mustache.html`**

```html
{{<layout}}
  {{$content}}
    <h1>My Todos</h1>
    <ul>{{#todos}}<li><a href='{{url:todo:id}}'>{{title}}</a></li>{{/todos}}</ul>
    <form method='post' action='{{url:todos}}'>
      {{csrf:input}}
      <input name='title' value='{{input:title}}'>
      {{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}
      <button>Add</button>
    </form>
  {{/content}}
{{/layout}}
```

`{{<layout}}...{{/layout}}` says "render the `layout` asset, but with these overrides." `{{$content}}override{{/content}}` defines the override for the `content` block declared in the parent. `{{url:todos}}` and the section helpers inside the child resolve against the page's pipeline context; the partial inlines the layout's surrounding markup.

For pure inclusion without overrides, use `{{>name}}` instead. It inlines the named asset rendered against the current scope. This differs from unescaped interpolation (`{{{name}}}` or `{{&name}}`): `{{>name}}` runs Mustache on the asset, so helpers and sections inside it resolve. The unescaped forms emit the value verbatim with no further processing. Use `{{>name}}` for templates and `{{&name}}` for already-rendered HTML such as a sanitized blog body.

### 11. Final State

After all ten concept steps, the project is a small but complete app: a home page with a quote and weather pulled concurrently, a todos module with list and detail pages plus a daily stats cron, and an activity module that subscribes to todo events. Three `.c` files, two databases, one shared layout.

```
.
├── todos/                              # todos module
│   ├── todos.c                         # config todos() { resources, db, events, tasks }
│   ├── todos_list.mustache.html        # extends layout, lists todos + form
│   ├── todo_detail.mustache.html       # one todo with comments
│   ├── create_todos_table.sql          # migrations
│   ├── create_comments_table.sql
│   ├── create_daily_stats_table.sql
│   ├── seed_todos.sql                  # seeds
│   ├── get_todos.sql                   # queries
│   ├── get_todo.sql
│   ├── get_comments.sql
│   ├── create_todo.sql
│   └── record_daily_stats.sql
├── activity/                           # activity module
│   ├── activity.c                      # config activity() { resource, db, events }
│   ├── activity.mustache.html
│   ├── create_activities_table.sql
│   ├── get_activities.sql
│   └── insert_activity.sql
├── static/                             # root-level templates
│   ├── layout.mustache.html            # shared chrome with {{$content}} block
│   └── home.mustache.html              # home page extending layout
├── public/                             # served as-is
│   └── favicon.png
└── main.c                              # registers modules, declares root resources and context
```

**`main.c`**: registers both modules, defines the home resource, embeds the root templates.
```c
#include <mach.h>
#include <sqlite.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config){
    .resources = {
      {"home", "/",
        .get = {
          fetch(
            {"https://api.quotable.io/random", .set_key = "quote"},
            {"https://api.weather.dev/now",    .set_key = "weather"}
          ),
          render("home")
        }
      }
    },
    .context = {
      {"home", (asset){
        #embed "static/home.mustache.html"
      }},
      {"layout", (asset){
        #embed "static/layout.mustache.html"
      }}
    },
    .modules = {todos, activity, sqlite}
  };
}
```

**`todos/todos.c`**: full CRUD on todos, comments via join, validation, error reroute, daily stats cron, publishes `todo_created`.
```c
#include <mach.h>
#include <sqlite.h>

config todos(){
  return (config){
    .name = "todos",

    .publishes = {
      {"todo_created", .with = {"title"}}
    },

    .resources = {
      {"todos", "/todos",
        .get = {
          query({"get_todos", .set_key = "todos", .db = "todos_db"}),
          render("todos_list")
        },

        .post = {
          validate({"title",
            .validation = validate_not_empty,
            .message    = "title cannot be empty"}),
          query({"create_todo", .db = "todos_db"}),
          task("record_daily_stats"),
          emit("todo_created"),
          redirect("todos")
        },

        .errors = {
          {http_bad_request, { reroute("todos") }}
        }
      },

      {"todo", "/todos/:id",
        .get = {
          validate({"id", .validation = validate_integer,
            .message = "must be an integer"}),
          query(
            {"get_todo",     .set_key = "todo",     .db = "todos_db"},
            {"get_comments", .set_key = "comments", .db = "todos_db"}
          ),
          join(
            .target_table_key      = "todo",
            .target_field_key      = "id",
            .nested_table_key      = "comments",
            .nested_field_key      = "todo_id",
            .target_join_field_key = "comments"
          ),
          render("todo_detail")
        }
      }
    },

    .tasks = {
      {"record_daily_stats", {
        query({"record_daily_stats", .db = "todos_db"})
      }, .cron = "0 0 * * *"}
    },

    .context = {
      {"todos_list", (asset){
        #embed "todos_list.mustache.html"
      }},
      {"todo_detail", (asset){
        #embed "todo_detail.mustache.html"
      }},
      {"get_todos", (asset){
        #embed "get_todos.sql"
      }},
      {"create_todo", (asset){
        #embed "create_todo.sql"
      }},
      {"get_todo", (asset){
        #embed "get_todo.sql"
      }},
      {"get_comments", (asset){
        #embed "get_comments.sql"
      }},
      {"record_daily_stats", (asset){
        #embed "record_daily_stats.sql"
      }}
    },

    .databases = {{
      .engine     = sqlite_db,
      .name       = "todos_db",
      .connect    = "file:todos.db?mode=rwc",
      .migrations = {
        (asset){
          #embed "create_todos_table.sql"
        },
        (asset){
          #embed "create_comments_table.sql"
        },
        (asset){
          #embed "create_daily_stats_table.sql"
        }
      },
      .seeds = {
        (asset){
          #embed "seed_todos.sql"
        }
      }
    }},

    .modules = {sqlite}
  };
}
```

**`activity/activity.c`**: subscribes to `todo_created`, owns its own database.
```c
#include <mach.h>
#include <sqlite.h>

config activity(){
  return (config){
    .name = "activity",

    .resources = {
      {"activity", "/activity",
        .get = {
          query({"get_activities", .set_key = "activities", .db = "activity_db"}),
          render("activity")
        }
      }
    },

    .events = {
      {"todo_created", {
        query({"insert_activity", .db = "activity_db"})
      }}
    },

    .context = {
      {"activity", (asset){
        #embed "activity.mustache.html"
      }},
      {"get_activities", (asset){
        #embed "get_activities.sql"
      }},
      {"insert_activity", (asset){
        #embed "insert_activity.sql"
      }}
    },

    .databases = {{
      .engine     = sqlite_db,
      .name       = "activity_db",
      .connect    = "file:activity.db?mode=rwc",
      .migrations = {
        (asset){
          #embed "create_activities_table.sql"
        }
      }
    }},

    .modules = {sqlite}
  };
}
```

That covers the basic shape of a MACH app: resources route requests, pipelines transform them, error handlers recover, joins compose nested views from separate queries, tasks handle background work, events decouple modules, external assets let templates and SQL live alongside the code that uses them, and `fetch()` brings in external HTTP data. See [Modules & Composition](#modules-composition) for more on module boundaries.

---

## Reference

Each subsection describes one piece of the framework and lists every option it accepts with a minimal snippet.

* [Context](#context)
* [Databases](#databases)
* [Resource Pipelines](#resource-pipelines)
* [Template Helpers](#template-helpers)
* [Pipeline Steps](#pipeline-steps)
* [Imperative API](#imperative-api)
* [Conditionals](#conditionals)
* [Error and Repair Pipelines](#error-and-repair-pipelines)
* [Event Pipelines](#event-pipelines)
* [Task Pipelines](#task-pipelines)
* [Modules & Composition](#modules-composition)
* [Module Reference](#module-reference)
* [Static Files](#static-files)
* [External Dependencies](#external-dependencies)

### Context

Pipelines read from and write to a shared, scoped key-value store that lives for the duration of one request. Every step draws inputs from context and writes outputs back to it.

`.context` seeds that store at the root with variables and assets available on every request. Templates and SQL stored here are referenced by name in `render()`, `query()`, and `find()`. Use `(asset){ #embed "file" }` to bake files into the binary at compile time. Docker secrets exposed to the container are also available in context.

The context uses three scopes: `input:xxx` for raw request parameters, `error:xxx` for validation/error data, and unprefixed names for app scope (query results, validated inputs, context variables). `validate()` bridges `input:` to app scope.

**Inline string value**: provide the value directly.
```c
.context = {{"site_name", "MACH App"}}
```

**`(asset){ #embed ... }`**: bake a file into the binary as a named asset.
```c
.context = {
  {"layout", (asset){
    #embed "layout.mustache.html"
  }},
  {"get_todos", (asset){
    #embed "get_todos.sql"
  }}
}
```

Combined:
```c
.context = {
  {"site_name", "MACH App"},
  {"version",   "1.2.0"},
  {"layout", (asset){
    #embed "static/layout.mustache.html"
  }},
  {"home", (asset){
    #embed "static/home.mustache.html"
  }},
  {"get_todos", (asset){
    #embed "todos/get_todos.sql"
  }},
  {"create_todo", (asset){
    #embed "todos/create_todo.sql"
  }}
}
```

![Context Scoping](./07-context-scoping.svg)

### Databases

Each `.databases` entry defines a data store. Migrations are forward-only and index-based: they run in array order, each applied once, with new migrations appended to the end. Seeds are idempotent and safe to re-run. Both are tracked in a `mach_meta` table.

Multi-tenant databases use `{{interpolation}}` in `.connect`. Connections are pooled with LRU eviction: active tenants stay warm, idle connections are reclaimed.

**`.engine`**: database engine constant, provided by a module.
```c
.engine = sqlite_db
```

**`.name`**: identifier referenced by `.db` in `query()` and `find()`.
```c
.name = "todos_db"
```

**`.connect`**: engine-specific connection string. Supports `{{interpolation}}` for multi-tenancy.
```c
.connect = "file:{{user_id}}_todo.db?mode=rwc"
```

**`.migrations`**: array of SQL migration strings or assets, applied once each in order.
```c
.migrations = {
  (asset){
    #embed "create_todos_table.sql"
  }
}
```

**`.seeds`**: array of idempotent seed statements, safe to re-run on every boot.
```c
.seeds = {"INSERT OR IGNORE INTO todos(id, title) VALUES(1, 'Hello');"}
```

Combined:
```c
.databases = {{
  .engine     = sqlite_db,
  .name       = "blog_db",
  .connect    = "file:{{user_id}}_blog.db?mode=rwc",
  .migrations = {
    "CREATE TABLE blogs ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "title TEXT NOT NULL,"
      "content TEXT NOT NULL"
    ");",
    "CREATE TABLE comments ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "blog_id INTEGER NOT NULL REFERENCES blogs(id),"
      "body TEXT NOT NULL"
    ");"
  },
  .seeds = {
    "INSERT OR IGNORE INTO blogs(id, title, content) VALUES(1, 'Hello', 'First post');"
  }
}}
```

**Engines:** `sqlite_db`, `postgres_db`, `mysql_db`, `redis_db`, `duckdb_db`

![Database Multi-Tenancy](./09-database-multi-tenancy.svg)

### Resource Pipelines

MACH is resource-based, not route-based. Each entry in `.resources` defines a named URL endpoint with HTTP verb pipelines. Resources are identified by name: `{{url:name}}`, `redirect()`, and `reroute()` take a `name[:arg1:arg2...]` identifier; positional args fill the `:params` of the URL pattern in order. Path specificity is automatic: exact matches (`/todos/active`) beat parameterized matches (`/todos/:id`) regardless of definition order.

> **`{{url:name}}` with URL params.** Args after the name are positional, colon-separated, literals or context keys:
> - ✅ `{{url:todo:5}}` resolves to `/todos/5`
> - ✅ `{{url:todo:id}}` reads `id` from current scope (useful inside `{{#todos}}...{{/todos}}` where each iteration has its own `id`)
> - ✅ `{{url:org_todo:acme:5}}` fills multiple `:params` in URL-pattern order (e.g. `/orgs/:org/todos/:id`)

Clients select a verb via the request method, or by passing `http_method` as a query/form parameter. This lets HTML forms (limited to GET/POST) reach any verb, and gives SSE a connection path: `/todos?http_method=sse`.

**`.name` *(pos)***: resource identifier used by `{{url:name}}`, `redirect()`, and `reroute()`.
```c
{"todos", "/todos", .get = { ... }}
```

**`.url` *(pos)***: URL pattern. Supports `:params`.
```c
{"todo", "/todos/:id", .get = { ... }}
```

**`.steps` *(pos)***: shared steps that run before every verb pipeline on the resource.
```c
{"todo", "/todos/:id", {
  validate({"id", .validation = "^\\d+$", .message = "must be a number"})
}, .get = { ... }, .delete = { ... }}
```

**`.mime`**: default response content type for the resource.
```c
{"feed", "/feed.json", .mime = mime_json, .get = { ... }}
```

**`.get` `.post` `.put` `.patch` `.delete`**: verb pipelines: ordered arrays of steps that transform a request into a response.
```c
{"todos", "/todos",
  .get  = { query({"get_todos", .set_key = "todos", .db = "db"}), render("todos") },
  .post = { validate({"title", .validation = validate_not_empty}), redirect("todos") }
}
```

**`.sse`**: persistent SSE channel. First positional is the channel name; steps run on connect.
```c
{"todos", "/todos",
  .sse = {"todos/{{user_id}}",
    query({"get_todos", .set_key = "todos", .db = "db"}),
    sse(.event = "initial", .data = {"{{todos}}"})
  }
}
```

**`.errors` / `.repairs`**: resource-scoped error and repair pipelines. See [Error and Repair Pipelines](#error-and-repair-pipelines).
```c
{"todos", "/todos",
  .post = { ... },
  .errors = {{http_bad_request, { render("form") }}}
}
```

Combined:
```c
{"todo", "/todos/:id", {
  validate({"id", .validation = "^\\d+$", .message = "must be a number"})
},
  .mime  = mime_html,
  .get   = { find({"get_todo", .set_key = "todo", .db = "todos_db"}),
             render("todo") },
  .patch = { validate({"title", .validation = validate_not_empty, .message = "required"}),
             query({.db = "todos_db",
               .query = "update todos set title = {{title}} where id = {{id}};"}),
             redirect("todo:{{id}}") },
  .delete = { query({.db = "todos_db",
                .query = "delete from todos where id = {{id}};"}),
              redirect("todos") },
  .sse    = {"todo/{{id}}", sse(.event = "ready") },
  .errors = {{http_not_found, { render("404") }}}
}
```

**MIME types (for `.mime`):** `mime_html`, `mime_txt`, `mime_sse`, `mime_json`, `mime_js`

### Template Helpers

Templates are Mustache. MACH supports the full Mustache base spec with one exception: dot notation. Use a section instead. `{{a.b}}` does not work; `{{#a}}{{b}}{{/a}}` does.

Supported base-spec features:
- **Interpolation**: `{{name}}` (HTML-escaped), `{{{name}}}` or `{{&name}}` (unescaped).
- **Sections**: `{{#name}}...{{/name}}` renders when truthy and iterates over arrays.
- **Inverted sections**: `{{^name}}...{{/name}}` renders when falsy or empty.
- **Comments**: `{{! ignored }}`.
- **Set delimiters**: `{{=<% %>=}}`.
- **Partials**: `{{>name}}` inlines the context asset named `name`, rendered against the current scope.
- **Layout inheritance**: `{{<parent}}{{$block}}override{{/block}}{{/parent}}` renders `parent` with each `{{$block}}default{{/block}}` block in the parent replaced by the override here.

No Mustache extensions are supported. There are no lambdas, no `{{#if}}` / `{{else}}`, no `{{#each}}`. Branching and iteration come from sections; counts and conditions are computed in SQL or `exec()` and rendered as plain context values.

On top of base Mustache, templates and other interpolated strings support built-in helpers with `{{helper:args}}` syntax. Arguments are positional and colon-separated; each can be a literal or a context key.

**`{{precision:field:N}}`**: format a numeric value with N decimal places.
```c
render(.template = "<p>Total: ${{precision:total:2}}</p>")
```

**`{{input:field}}`**: raw, unvalidated request parameter from the `input` scope. Typically used to repopulate form fields after a validation error.
```c
render(.template = "<input name='title' value='{{input:title}}'>")
```

**`{{error:field}}`**: truthy when `field` has an error. Used as a Mustache section to conditionally render markup.
```c
render(.template = "{{#error:title}}<span class='error'>invalid</span>{{/error:title}}")
```

**`{{error_message:field}}`**: human-readable message for a field error, from `validate()`'s `.message` or from `error_set()`.
```c
render(.template = "<span>{{error_message:title}}</span>")
```

**`{{error_code:field}}`**: HTTP status code associated with a field error (e.g. `400`, `404`).
```c
render(.template = "<p>Code: {{error_code:title}}</p>")
```

**`{{url:name}}`**: resolve a resource identifier to its URL. Takes positional args to fill `:params`. Args can be literals or context keys.
```c
render(.template =
  "<a href='{{url:todos}}'>All</a>"              // /todos
  "<a href='{{url:todo:5}}'>Item 5</a>"          // /todos/5
  "<a href='{{url:todo:id}}'>From context</a>"   // /todos/{{id}}
)
```

**`{{asset:filename}}`**: resolve a file in `public/` to a cache-busted URL (content checksum + immutable cache headers). See [Static Files](#static-files).
```c
render(.template = "<link rel='stylesheet' href='{{asset:styles.css}}'>")
```

**`{{csrf:token}}`**: emit a CSRF token, for use in URL query strings. Generates a random hash, sets it on an httponly/secure/samesite cookie, and outputs the same value inline.
```c
render(.template = "<a href='{{url:logout}}?csrf={{csrf:token}}'>Log out</a>")
```

**`{{csrf:input}}`**: emit a hidden `<input>` carrying a CSRF token, for use inside a `<form>`. Same cookie-setting behavior as `{{csrf:token}}`.
```c
render(.template = "<form>{{csrf:input}}<input name='title'><button>Add</button></form>")
```

Combined:
```c
render(.template =
  "<link rel='stylesheet' href='{{asset:styles.css}}'>"
  "<article>"
    "{{#post}}"
      "<h2>{{title}}</h2>"
      "<p>Rating: {{precision:score:1}}/5</p>"
      "<div>{{&body_html}}</div>"
    "{{/post}}"
    "<form method='post' action='{{url:comments}}'>"
      "{{csrf:input}}"
      "<input name='body' value='{{input:body}}'>"
      "{{#error:body}}<span>{{error_message:body}}</span>{{/error:body}}"
      "<button>Comment</button>"
    "</form>"
    "<a href='{{url:logout}}?csrf={{csrf:token}}'>Log out</a>"
  "</article>"
)
```

> **CSRF verification is automatic.** MACH checks that the incoming token (from the form field or query parameter) matches the value stored in the CSRF cookie and rejects mismatches with a 403. The cookie is httponly, secure, and samesite, so nothing beyond emitting `{{csrf:token}}` or `{{csrf:input}}` in the rendered response is required.

### Pipeline Steps

Steps are the units of work in a pipeline. Each receives the current context, acts on it, and passes control to the next. All steps accept `.if_context` and `.unless_context` for conditional execution.

* [validate](#validate)
* [find & query](#find-query)
* [join](#join)
* [fetch](#fetch)
* [exec](#exec)
* [emit](#emit)
* [task](#task)
* [sse](#sse)
* [render](#render)
* [headers & cookies](#headers-cookies)
* [redirect & reroute](#redirect-reroute)
* [nest](#nest)

![Request Pipeline Flow](./02-request-pipeline-flow.svg)

#### validate

Checks request parameters (query string, form body, URL params) against regex patterns. On success, each value is promoted from `input:name` to app scope. On failure, errors land in `error:name` and a `400 Bad Request` triggers the nearest [error/repair pipeline](#error-and-repair-pipelines). All validations in one call complete before the error fires, so all errors are available together for form re-rendering.

Built-in regex macros are defined in `mach.h`; define your own the same way: `#define validate_zipcode "^\\d{5}$"`.

**`.param_key` *(pos)***: name of the parameter to validate.
```c
validate({"title", .validation = "^\\S+$", .message = "required"})
```

**`.validation`**: regex pattern, or a built-in validator macro.
```c
validate({"email", .validation = validate_email, .message = "bad email"})
```

**`.message`**: human-readable error shown via `{{error_message:name}}`.
```c
validate({"age", .validation = "^\\d+$", .message = "must be a number"})
```

**`.optional`**: skip validation when the parameter is absent.
```c
validate({"filter", .optional = true, .validation = "^(active|done)$"})
```

**`.fallback`**: default value injected when the parameter is absent.
```c
validate({"page", .fallback = "1", .validation = "^\\d+$"})
```

Combined:
```c
validate(
  {"email",  .validation = validate_email,     .message = "must be a valid email"},
  {"title",  .validation = validate_not_empty, .message = "cannot be empty"},
  {"page",   .fallback   = "1",
             .validation = "^\\d+$",            .message = "must be a number"},
  {"filter", .optional   = true,
             .validation = "^(active|done)$",   .message = "must be 'active' or 'done'"}
)
```

**Built-in validators:**
- Strings: `validate_not_empty`, `validate_alpha`, `validate_alphanumeric`, `validate_slug`, `validate_no_html`
- Numbers: `validate_integer`, `validate_positive`, `validate_float`, `validate_percentage`
- Identity: `validate_email`, `validate_uuid`, `validate_username`
- Dates & times: `validate_date`, `validate_time`, `validate_datetime`
- Web: `validate_url`, `validate_ipv4`, `validate_hex_color`
- Codes: `validate_zipcode_us`, `validate_phone_e164`, `validate_cron`
- Security: `validate_no_sqli`, `validate_token`, `validate_base64`
- Boolean: `validate_boolean`, `validate_yes_no`, `validate_on_off`

#### find & query

Both run database queries. `.db` selects the database; `.set_key` stores the result in context as a table, even for single-row queries. SQL is either inlined with `.query` or referenced by name as the positional (loaded from `.context`). Multiple items in a single step run **concurrently**. Queries use prepared statements; interpolated `{{values}}` are bound, not spliced. For transactions, use `BEGIN`/`COMMIT`/`ROLLBACK` directly in your queries.

The only difference between the two: `find()` raises `404 Not Found` on zero rows; `query()` does not.

> **Positional asset name OR `.query`, not both.** Each item picks one:
> - ✅ `query({"get_todos", .set_key = "todos", .db = "todos_db"})`: SQL loaded by asset name from `.context`
> - ✅ `query({.set_key = "todos", .db = "todos_db", .query = "select id, title from todos;"})`: SQL inlined
> - ❌ `query({"get_todos", .set_key = "todos", .db = "todos_db", .query = "select ..."})`: combining the two is rejected

> **Concurrency = multiple items in one step, not multiple steps.** `query({...}, {...})` runs both queries in parallel. Two back-to-back `query({...})` steps run serially.

**`.template_key` *(pos)***: name of a SQL asset stored in `.context`. Mutually exclusive with `.query`.
```c
query({"get_todos", .set_key = "todos", .db = "todos_db"})
```

**`.query`**: inline SQL string. Supports `{{interpolation}}`, bound as parameters. Mutually exclusive with the positional asset name.
```c
query({.set_key = "todos", .db = "todos_db",
  .query = "select id, title from todos where user_id = {{user_id}};"})
```

**`.set_key`**: context key for the result table.
```c
query({.set_key = "active", .db = "db", .query = "select * from todos;"})
```

**`.db`**: name of the database, matching a `.databases` entry.
```c
query({.db = "todos_db", .query = "select 1;"})
```

**`.if_context` / `.unless_context`** *(per item)*: conditionally include or skip individual queries while running the others concurrently.
```c
query(
  {"get_todos",  .set_key = "todos",  .db = "db"},
  {"get_urgent", .if_context = "show_urgent", .set_key = "u", .db = "db"}
)
```

Combined:
```c
query(
  {"get_todos", .set_key = "todos", .db = "todos_db"},
  {.set_key = "count", .db = "todos_db",
   .query = "select count(*) as n from todos where user_id = {{user_id}};"},
  {.if_context = "show_urgent", .set_key = "urgent", .db = "todos_db",
   .query = "select id, title from todos where user_id = {{user_id}} and priority = 'high';"}
)
```

#### join

Nests records from one context table into each matching record of another, like a SQL JOIN performed in memory. Useful when records come from separate databases or queries and need to be combined. After the step, each outer record gains a new field holding its matched inner records.

**`.target_table_key`**: outer table whose records receive nested children.
```c
.target_table_key = "projects"
```

**`.target_field_key`**: field on the outer table to match against.
```c
.target_field_key = "id"
```

**`.nested_table_key`**: inner table whose records get nested.
```c
.nested_table_key = "todos"
```

**`.nested_field_key`**: field on the inner table that points at the outer.
```c
.nested_field_key = "project_id"
```

**`.target_join_field_key`**: new field on outer records holding the matched inner records.
```c
.target_join_field_key = "todos"
```

Combined:
```c
join(
  .target_table_key = "projects",
  .target_field_key = "id",
  .nested_table_key = "todos",
  .nested_field_key = "project_id",
  .target_join_field_key = "todos"
)
```

**Full context example.** A common pattern is concurrent `query()` → `join()` → `render()`: fetch parent and children from separate queries, then render them as one nested structure. Blog + comments, single database:

```c
{"blog", "/blogs/:id",
  .get = {
    validate({"id", .validation = validate_integer, .message = "must be an integer"}),

    // Fetch both concurrently: one query() call, two items
    query(
      {.set_key = "blog",     .db = "blog_db",
       .query = "select id, title, content from blogs where id = {{id}};"},
      {.set_key = "comments", .db = "blog_db",
       .query = "select id, blog_id, body from comments where blog_id = {{id}};"}
    ),

    // Nest each comment into its matching blog record
    join(
      .target_table_key      = "blog",
      .target_field_key      = "id",
      .nested_table_key      = "comments",
      .nested_field_key      = "blog_id",
      .target_join_field_key = "comments"
    ),

    // Enter {{#blog}} first; after join(), comments lives INSIDE each blog record
    render(.template =
      "<article>"
        "{{#blog}}"
          "<h1>{{title}}</h1>"
          "<div>{{content}}</div>"
          "<h2>Comments</h2>"
          "<ul>{{#comments}}<li>{{body}}</li>{{/comments}}</ul>"
        "{{/blog}}"
      "</article>"
    )
  }
}
```

Shape of the context at each step:

```
after query(): { blog: [{id, title, content}],
                 comments: [{id, blog_id, body}, ...] }       // two sibling tables

after join():  { blog: [{id, title, content,
                         comments: [{id, blog_id, body}, ...]}] }  // nested inside blog
```

#### fetch

Makes one or more HTTP requests and stores responses in context. JSON is parsed into tables and records (with nested tables for nested JSON); plain-text responses are stored as a string. Like `query()`, multiple items in a single step run **concurrently**, so a page can fan out to several services in parallel and join the results downstream.

> **Concurrency = multiple items in one step, not multiple steps.** `fetch({...}, {...})` runs both requests in parallel. Two back-to-back `fetch({...})` steps run serially.

**`.url` *(pos)***: request URL; supports `{{interpolation}}`.
```c
fetch({"https://api.weather.dev/forecast?city={{city}}", .set_key = "w"})
```

**`.set_key`**: context key for the response.
```c
fetch({"https://api.weather.dev/now", .set_key = "weather"})
```

**`.method`**: HTTP method. Defaults to `http_get`.
```c
fetch({"https://api.dev/charge", .set_key = "r", .method = http_post})
```

**`.headers`**: array of name/value pairs.
```c
fetch({"https://api.dev/me", .set_key = "r",
  .headers = {{"Authorization", "Bearer {{token}}"}}})
```

**`.json`**: context key serialized as the JSON request body.
```c
fetch({"https://api.dev/charge", .set_key = "receipt",
  .method = http_post, .json = "order"})
```

**`.text`**: context key sent as the plain-text request body.
```c
fetch({"https://api.dev/log", .set_key = "r",
  .method = http_post, .text = "raw_body"})
```

**`.if_context` / `.unless_context`** *(per item)*: conditionally include or skip individual requests while running the others concurrently.
```c
fetch(
  {"https://api.weather.dev/now",      .set_key = "weather"},
  {"https://api.quotes.dev/random",    .if_context = "show_quote", .set_key = "quote"}
)
```

Combined, single request:
```c
fetch({"https://api.payments.dev/charge",
  .set_key = "receipt",
  .method  = http_post,
  .headers = {
    {"Authorization",   "Bearer {{api_key}}"},
    {"Idempotency-Key", "{{order_id}}"}
  },
  .json    = "order"
})
```

Combined, concurrent fan-out:
```c
fetch(
  {"https://api.weather.dev/now?city={{city}}",     .set_key = "weather"},
  {"https://api.news.dev/headlines?topic={{topic}}", .set_key = "news"},
  {"https://api.quotes.dev/random",                  .set_key = "quote"}
)
```

**HTTP methods (for `.method`):** `http_get`, `http_post`, `http_put`, `http_patch`, `http_delete`, `http_sse_method`

#### exec

Calls a C function or block with access to the context via the Imperative API. `exec()` is where business logic and data shaping lives: enriching query results with computed fields, aggregating across rows, transforming data between steps, calling external C libraries, blocking I/O, CPU-heavy work. Execution is dispatched to the shared thread pool, which releases the reactor; the pipeline resumes on the original reactor when the call returns. To trigger an error/repair pipeline from inside, call `error_set()`.

**Block *(pos)***: inline block, for short logic specific to this pipeline. Here, attaching each challenger's opponent id so the template can render two voting forms with the right winner/loser pairing:
```c
exec(^(){
  auto t = get("challengers");
  record_set(table_get(t, 0), "opponent_id",
             record_get(table_get(t, 1), "id"));
  record_set(table_get(t, 1), "opponent_id",
             record_get(table_get(t, 0), "id"));
})
```

**`.call`**: reference to a named C function, for logic reuse across pipelines.
```c
exec(.call = assign_opponents)
```

Inside `exec` blocks and functions, context, memory, errors, tables, and records are manipulated through the [Imperative API](#imperative-api).

#### emit

Triggers an internal pub/sub event. Subscribers in other modules react in their `.events` pipelines, with no direct dependency on the emitter. See [Event Pipelines](#event-pipelines).

**Event name *(pos)***: name of the event to publish.
```c
emit("todo_created")
```

#### task

Adds a named job to the task database; the calling pipeline continues immediately. Task reactors pick up queued jobs and execute their pipelines. See [Task Pipelines](#task-pipelines).

**Task name *(pos)***: name of a task defined in `.tasks`.
```c
task("recount_todos")
```

#### sse

Pushes a Server-Sent Event. With `.channel`, the event is broadcast to all clients on that channel. Without `.channel`, the event is returned directly to the requesting client. See [Resource Pipelines](#resource-pipelines).

**`.channel` *(pos)***: channel to broadcast on; supports `{{interpolation}}`.
```c
sse(.channel = "todos/{{user_id}}", .event = "new_todo", .data = {"{{todo}}"})
```

**`.event`**: SSE `event:` line value.
```c
sse(.event = "ping")
```

**`.data`**: array of strings, one per SSE `data:` line (multi-line data).
```c
sse(.event = "msg", .data = {"line one", "line two"})
```

**`.comment`**: SSE `:` comment line value, useful for keep-alives.
```c
sse(.comment = "keep-alive")
```

Combined:
```c
sse(
  .channel = "todos/{{user_id}}",
  .event   = "todo_updated",
  .data    = {"id: {{todo_id}}", "title: {{title}}"},
  .comment = "broadcast at {{timestamp}}"
)
```

#### render

Outputs a template using the current context. Templates are referenced by name from `.context` or inlined.

**`.template_key` *(pos)***: asset name in `.context`. The asset is a template.
```c
render("todos")
```

**`.template`**: inline template string.
```c
render(.template = "<h1>{{site_name}}</h1>")        // {{site_name}} from .context
```

**`.status`**: HTTP response status (defaults to `http_ok`).
```c
render("not_found", .status = http_not_found)
```

**`.mime`**: override the response content type.
```c
render("plain", .mime = mime_txt)
```

**`.engine`**: template engine. Accepts `mustache` (default) or `mdm` for Markdown-with-Mustache. Additional engined can be added with modules.
```c
render(.engine = mdm, .template = "# Welcome, {{user_name}}")
```

**`.json_table_key`**: context table to serialize as the JSON response. Sets `application/json`; nested tables produce nested JSON.
```c
render(.json_table_key = "todos")
```

**HTTP statuses (for `.status`):** `http_ok` (200), `http_created` (201), `http_redirect` (302), `http_bad_request` (400), `http_not_authorized` (401), `http_not_found` (404), `http_error` (500)

**MIME types (for `.mime`):** `mime_html`, `mime_txt`, `mime_sse`, `mime_json`, `mime_js`

#### headers & cookies

Set HTTP response headers and cookies declaratively. Both accept an array of name/value pairs; values support `{{interpolation}}`.

**Pairs *(pos)***: array of `{name, value}` entries.
```c
headers({{"X-Request-Id", "{{request_id}}"}})
```
```c
cookies({{"session", "{{session_id}}"}})
```

Combined:
```c
headers({
  {"X-Request-Id",  "{{request_id}}"},
  {"Cache-Control", "no-store"}
}),
cookies({
  {"session", "{{session_id}}"},
  {"theme",   "{{theme}}"}
})
```

#### redirect & reroute

`redirect()` returns a 302 to the client, causing the browser to navigate. `reroute()` re-enters the router server-side, executing another resource's pipeline within the same request. Both take a **resource identifier** in the same `name[:arg1:arg2...]` format as `{{url:name}}`: colon-separated positional args that fill the target's `:params`. Args can be literals or context keys, and support `{{interpolation}}`.

**Resource identifier *(pos)***: target resource name, plus positional args for any `:params`.
```c
redirect("todos")                // 302 to /todos (bare resource, no args)
redirect("todo:5")               // 302 to /todos/5 (literal id)
redirect("todo:{{id}}")          // 302 to /todos/5 (id from context)
redirect("org_todo:acme:5")      // 302 to /orgs/acme/todos/5 (multi-arg)
reroute("todo:{{id}}")           // run that pipeline in-process
```

#### nest

Groups multiple steps into a single composite step. Useful when applying one `.if_context`/`.unless_context` to several steps, to avoid repeating the condition on each.

**`.steps` *(pos)***: array of steps that run as a unit.
```c
nest({query({...}), emit("urgent_todo"), render("urgent")})
```

**`.if_context` / `.unless_context`**: condition applied to the whole group.
```c
nest({query({...}), emit("urgent_todo"), render("urgent")},
  .if_context = "is_urgent")
```

### Imperative API

Functions called from `exec()` blocks and `.call` functions to read and write context, allocate memory, raise errors, and manipulate tables and records.

* [context](#context-1)
* [memory](#memory)
* [errors](#errors)
* [tables](#tables)
* [records](#records)

#### context

Read, write, and test context keys, and resolve `{{interpolation}}` against the current scope.

**`get(name)`**: returns the value stored under `name`, or `nullptr` if absent. The returned pointer is whatever was stored: a `string` for scalars, a `table` for query and fetch results.
```c
auto todos = get("todos");
```

**`set(name, value)`**: writes `value` to `name`, exposing it to downstream steps and to templates.
```c
set("is_urgent", "1");
```

**`has(name)`**: returns true when `name` exists in the current scope.
```c
if (has("user_id")) { ... }
```

**`format(fmt)`**: returns `fmt` with `{{name}}` interpolations resolved against the current context. Same scopes and helpers as templates.
```c
string greeting = format("Hello, {{user_name}}");
```

Combined:
```c
exec(^(){
  auto rows = get("todos");
  if (table_count(rows) > 5) {
    set("is_urgent", "1");
    set("banner", format("{{user_name}} has more than 5 open todos"));
  }
})
```

#### memory

Pipeline-arena allocation and deferred cleanup of foreign pointers. Both clear when the request completes.

**`allocate(bytes)`**: returns a buffer from the pipeline arena. Reclaimed automatically on request completion.
```c
char *buf = allocate(256);
```

**`defer_free(ptr)`**: schedules `free()` for a pointer returned by an external library. Runs when the arena is released.
```c
char *out = third_party_alloc(256);
defer_free(out);
```

Combined:
```c
exec(^(){
  char *url = allocate(512);
  build_signed_url(url, 512, get("path"));
  set("signed_url", url);

  char *raw = third_party_render_md(get("markdown"));
  defer_free(raw);
  set("html", raw);
})
```

#### errors

Raise field-scoped errors from inside `exec()` to trigger error/repair pipelines. The keys land in the `error:name` scope, visible to templates as `{{error:name}}`, `{{error_code:name}}`, and `{{error_message:name}}`.

**`error_set(name, err)`**: associates an error with `name` and triggers the nearest [error or repair pipeline](#error-and-repair-pipelines).
```c
error_set("token", (error){ http_bad_request, "token has expired" });
```

**`error_get(name)`**: returns the `error` previously set on `name`.
```c
error e = error_get("token");
```

**`error_has(name)`**: returns true when `name` has an error.
```c
if (error_has("token")) { ... }
```

Combined:
```c
exec(^(){
  string token = get("token");
  if (!token || strlen(token) < 16) {
    error_set("token", (error){
      http_bad_request,
      "token must be at least 16 characters"
    });
  }
})
```

#### tables

Tables are ordered collections of records, the shape `query()` produces and `fetch()` parses JSON into. Use these to build derived results.

**`table_new()`**: returns an empty table in the pipeline arena.
```c
table t = table_new();
```

**`table_count(t)`**: number of records in `t`.
```c
int n = table_count(get("todos"));
```

**`table_get(t, i)`**: record at index `i`, or `nullptr` if out of range.
```c
record first = table_get(get("todos"), 0);
```

**`table_add(t, r)`**: appends `r` to `t`.
```c
table_add(t, record_new());
```

**`table_remove(t, r)`**: removes record `r` from `t`.
```c
table_remove(t, r);
```

**`table_remove_at(t, i)`**: removes the record at index `i`.
```c
table_remove_at(t, 0);
```

Combined:
```c
exec(^(){
  table source = get("raw_users");
  table active = table_new();
  for (int i = 0; i < table_count(source); i++) {
    record u = table_get(source, i);
    string status = record_get(u, "status");
    if (status && strcmp(status, "active") == 0) {
      table_add(active, u);
    }
  }
  set("active_users", active);
})
```

#### records

Records are name-value bags, the shape of one row from `query()` or one object from `fetch()`. All values are strings, in keeping with [Everything is a String](#everything-is-a-string).

**`record_new()`**: returns an empty record in the pipeline arena.
```c
record r = record_new();
```

**`record_get(r, name)`**: string value of `name`, or `nullptr` if absent.
```c
string title = record_get(r, "title");
```

**`record_set(r, name, value)`**: writes `value` to `name` on `r`.
```c
record_set(r, "title", "New title");
```

**`record_remove(r, name)`**: removes `name` from `r`.
```c
record_remove(r, "draft");
```

Combined:
```c
exec(^(){
  table todos = get("todos");
  for (int i = 0; i < table_count(todos); i++) {
    record t = table_get(todos, i);
    string title = record_get(t, "title");
    if (title && strlen(title) > 40) {
      record_set(t, "is_long", "1");
    }
  }
})
```

### Conditionals

Every step accepts `.if_context` and `.unless_context`, which name a context variable. They work for any context value: validated inputs, query results, framework flags such as `is_htmx`, or flags set from `exec()`.

**`.if_context`**: context key. Step runs only when the value is present.
```c
render("fragment", .if_context = "is_htmx")
```

**`.unless_context`**: context key. Step runs only when the value is absent.
```c
render("full_page", .unless_context = "is_htmx")
```

For multi-state branching, set context flags from `exec()`, then key downstream steps off them:

```c
exec(.call = classify_todo),
render("urgent_confirmation",  .if_context     = "is_urgent"),
render("standard_confirmation", .unless_context = "is_urgent")
```

### Error and Repair Pipelines

When a pipeline step fails, execution halts and MACH searches for a handler bottom-up: resource, then module, then root. Errors are terminal: the handler sends a response and ends the request. Repairs are resumable: they fix the context and resume the original pipeline at the step after the failure. If no matching repair is found, resolution falls through to errors. Unhandled errors fall through to MACH's internal handler, which renders the error message as `text/plain` with the error code as the HTTP status, and surfaces in the TUI console and telemetry.

The `error` scope is shared across `validate()` failures and `error_set()` calls: `{{error:name}}`, `{{error_code:name}}`, `{{error_message:name}}`. The raw input value remains available in `input:name` for re-rendering forms.

**`.errors`**: terminal handlers keyed by error code.
```c
.errors = {
  {http_not_found,    { render("404") }},
  {http_bad_request,  { render("form") }}
}
```

**`.repairs`**: resumable handlers keyed by error code.
```c
.repairs = {
  {http_not_authorized, { exec(.call = refresh_session_token) }}
}
```

Combined:
```c
.errors = {
  {http_not_found,    { render("404") }},
  {http_bad_request,  { render("form") }},
  {http_error,        { render("500") }}
},
.repairs = {
  {http_not_authorized, { exec(.call = refresh_session_token) }}
}
```

**Built-in error codes:** `http_ok` (200), `http_created` (201), `http_redirect` (302), `http_bad_request` (400), `http_not_authorized` (401), `http_not_found` (404), `http_error` (500). Any integer works; the `http_*` constants are convenience names. Define your own for domain-specific errors, e.g. `#define err_quota_exceeded 723`.

![Error Resolution](./04-error-resolution.svg)

### Event Pipelines

Internal pub/sub for cross-module communication. The publisher does not know who listens; the subscriber does not know who emits. Adding a subscriber means adding a new module with an `.events` entry, with no changes to the publisher.

Events are durable by default. When `.publishes` is defined anywhere in the app, MACH creates a `mach_events` database to track delivery. If the process crashes, undelivered events are replayed on the next boot.

**`.publishes`**: outbound event contracts. `.event` is the name; `.with` lists context keys to pass along.
```c
.publishes = {
  {"todo_created", .with = {"user_id", "title"}}
}
```

**`.events`**: subscriber pipelines keyed by event name.
```c
.events = {
  {"todo_created", {
    query({"insert_activity", .db = "activity_db"})
  }}
}
```

**`.errors` / `.repairs`** *(per subscriber)*: each `.events` entry can declare its own handlers, resolved with the same bottom-up search as resource pipelines. See [Error and Repair Pipelines](#error-and-repair-pipelines).
```c
.events = {
  {"todo_created", {
    query({"insert_activity", .db = "activity_db"})
  }, .errors = {{http_error, { exec(.call = log_subscriber_failure) }}}}
}
```

Combined:
```c
// todos/todos.c: publisher
config todos(){
  return (config){
    .name = "todos",
    .publishes = {
      {"todo_created", .with = {"user_id", "title"}},
      {"todo_deleted", .with = {"user_id", "todo_id"}}
    },
    .resources = {
      {"todos", "/todos",
        .post = {
          validate({"title", .validation = validate_not_empty}),
          query({"insert_todo", .db = "todos_db"}),
          emit("todo_created"),
          redirect("todos")
        }
      }
    }
  };
}

// activity/activity.c: subscriber
config activity(){
  return (config){
    .name = "activity",
    .events = {
      {"todo_created", {
        query({.db = "activity_db",
          .query = "insert into activities(kind, user_id, ref) "
                   "values('created', {{user_id}}, {{title}});"})
      }},
      {"todo_deleted", {
        query({.db = "activity_db",
          .query = "insert into activities(kind, user_id, ref) "
                   "values('deleted', {{user_id}}, {{todo_id}});"})
      }}
    }
  };
}
```

![Event Pub/Sub](./03-event-pub-sub.svg)

### Task Pipelines

Tasks are named pipelines that run asynchronously on task reactors. Fire-and-forget: the calling pipeline continues immediately. Defined at the module or root level. Triggered on demand with `task("name")` or on a schedule via `.cron`. Tasks can enqueue more tasks via `task()`.

Tasks are durable: when `.tasks` is defined, MACH creates a `mach_tasks` database and checkpoints the context after each step. A crash mid-task resumes at the step where it left off on the next boot.

**`.name` *(pos)***: task identifier, called via `task("name")`.
```c
.tasks = {
  {"recount_todos", { query({.db = "db", .query = "update users set ..."}) }}
}
```

**`.accepts`**: context keys to pull from the caller into the task.
```c
{"recount_todos", {
  query({.db = "db", .query = "update users set todo_count = ... where id = {{user_id}};"})
}, .accepts = {"user_id"}}
```

**`.cron`**: standard cron schedule for recurring tasks (no caller required).
```c
{"daily_digest", {
  query({.db = "db", .query = "insert into digest_reports ..."})
}, .cron = "0 8 * * *"}
```

**Steps *(pos)***: the task's pipeline body, the second positional brace block, before any designated fields.
```c
{"name", { query({...}), emit("done"), task("followup") }, .accepts = {...}}
```

**`.errors` / `.repairs`** *(per task)*: each task can declare its own handlers, resolved with the same bottom-up search as resource pipelines. See [Error and Repair Pipelines](#error-and-repair-pipelines).
```c
{"send_invoice", {
  fetch({"https://api.billing.dev/invoices/{{invoice_id}}", .set_key = "inv"})
}, .repairs = {{http_not_authorized, { exec(.call = refresh_billing_token) }}}}
```

Combined:
```c
.tasks = {
  // on-demand: enqueued via task("recount_todos")
  {"recount_todos", {
    query({.db = "todos_db",
      .query = "update users set todo_count = "
               "(select count(*) from todos where user_id = users.id) "
               "where id = {{user_id}};"})
  }, .accepts = {"user_id"}},

  // recurring: runs on schedule, no caller
  {"daily_digest", {
    query({.db = "todos_db",
      .query = "insert into digest_reports(generated_at) values(now());"}),
    emit("digest_ready")
  }, .cron = "0 8 * * *"}
}
```

### Modules & Composition

Every MACH app and module returns a `config` struct. The root `main.c` must define a function named `mach()`; modules define their own functions with any name and register them in `.modules` by bare function reference. A module owns its own resources, databases, migrations, templates, and event contracts.

When the root and a module both define something with the same name, resolution depends on the kind. Context variables and databases resolve **top-down** from the root, so the **root wins**. Error and repair handlers resolve **bottom-up** from the resource (resource → module → root), so the **innermost handler wins**. Modules don't call each other directly; they communicate through pub/sub events.

**`.name`**: module identifier.
```c
config todos(){ return (config){ .name = "todos", /* resources, databases, ... */ }; }
```

**`.modules`**: other modules to compose into this one (root or nested).
```c
.modules = {todos, activity, sqlite, session_auth}
```

**Complete module file.** A module returns a `config` with the same shape as the root app (`.resources`, `.databases`, `.events`, etc.), plus a `.name` for identity. Resource fields like `.url`, `.mime`, `.get` are not top-level config fields; they belong inside entries of `.resources`. A `blogs/blogs.c`:

```c
#include <mach.h>
#include <sqlite.h>

config blogs(){
  return (config){
    .name = "blogs",
    .resources = {
      {"blog", "/blogs/:id",
        .get = { /* validate → query → join → render; see `join` worked example */ }
      }
    },
    .databases = {{
      .engine     = sqlite_db,
      .name       = "blog_db",
      .connect    = "file:blogs.db?mode=rwc",
      .migrations = {
        "CREATE TABLE blogs ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "title TEXT NOT NULL,"
          "content TEXT NOT NULL"
        ");",
        "CREATE TABLE comments ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "blog_id INTEGER NOT NULL REFERENCES blogs(id),"
          "body TEXT NOT NULL"
        ");"
      }
    }}
  };
}
```

Bring the module into scope by `#include`ing its `.c` file from `main.c`, then register it with `.modules = {blogs, sqlite}`. The module's resources and databases are merged into the app tree at registration.
```c
// main.c
#include <mach.h>
#include "blogs/blogs.c"

config mach(){ return (config){ .modules = {blogs, sqlite} }; }
```

A typical project layout:
```
├── todos/                         # todos module
│   ├── todos.c                    # config todos() { ... }
│   ├── todos.mustache.html
│   ├── create_todos_table.sql
│   └── get_todos.sql
├── activity/                      # activity module
│   └── activity.c
├── static/                        # root-level templates (not a module)
│   ├── layout.mustache.html
│   └── home.mustache.html
├── public/                        # static files, served directly
│   └── favicon.png
└── main.c                         # registers modules
```

**Bundled modules (add the initializer to `.modules` to use):** `sqlite`, `postgres`, `mysql`, `redis`, `duckdb`, `htmx`, `datastar`, `tailwind`, `session_auth`. See [Module Reference](#module-reference) for what each one provides.

![App Composition Tree](./05-app-composition-tree.svg)
![Middleware Scoping](./06-middleware-scoping.svg)

### Module Reference

* [htmx](#htmx)
* [datastar](#datastar)
* [tailwind](#tailwind)
* [session_auth](#session_auth)
* [Database engines](#database-engines-sqlite-postgres-mysql-redis-duckdb)

#### htmx

Brings [HTMX](https://htmx.org) support to MACH apps: server-rendered partial updates, history, request indicators, all driven by HTML attributes on the elements that need them.

**Partial:** `{{>htmx}}`, the script tag for the HTMX runtime, served through MACH. Include in `<head>`.

**Context flags:**
- `is_htmx`: true when the request has the `HX-Request` header. Pair with `.if_context = "is_htmx"` to send fragments to HTMX requests and full pages to direct loads.

**Example.** A SPA-feel app with no pipeline branching: `hx-boost='true'` on `<body>` makes every `<a>` and `<form>` inside use AJAX navigation, swapping body content instead of full page reloads. Boosted requests come through the normal pipelines.
```c
render(.template =
  "<html>"
    "<head>{{>htmx}}</head>"
    "<body hx-boost='true'>"
      "<nav><a href='{{url:home}}'>Home</a></nav>"
      "<main>...</main>"
    "</body>"
  "</html>")
```

For HTMX attributes (`hx-boost`, `hx-target`, `hx-swap`, etc.), see the [HTMX docs](https://htmx.org/docs/).

#### datastar

Brings [Datastar](https://data-star.dev) support: hypermedia-driven reactive frontend where the server pushes DOM updates and reactive signal state over SSE.

**Partial:** `{{>datastar}}`, the script tag for the Datastar runtime, served through MACH. Include in `<head>`.

**Context flags:**
- `is_ds`: true when the request originates from the Datastar client. Pair with `.if_context = "is_ds"` to send Datastar SSE events back to Datastar requests and full pages to direct loads.

**Steps:**
- `ds_sse(...)`: combines SSE with Datastar-formatted events targeting specific elements. Without a channel the event goes to the requesting client; with one it broadcasts.

`ds_sse` fields:

**`.channel` *(pos)***: broadcast channel; supports `{{interpolation}}`.
```c
ds_sse("todos/{{user_id}}", .target = "todos", .elements = {"todo"})
```

**`.target`**: DOM element id for the update.
```c
ds_sse(.target = "todos", .elements = {"todo_row"})
```

**`.mode`**: fragment insertion mode for the rendered DOM fragment.
```c
ds_sse(.target = "todos", .mode = mode_prepend, .elements = {"todo_row"})
```

**`.elements`**: a `render_config` for the DOM fragment (positional is the asset name, supports `.template`, `.engine`, etc.).
```c
ds_sse(.target = "row", .elements = {"todo_row"})
```

**`.signals`**: JSON string used to update Datastar's reactive client state without touching the DOM.
```c
ds_sse(.signals = "{\"count\": {{count}}}")
```

**`.js`**: JavaScript snippet evaluated on the client.
```c
ds_sse(.js = "console.log('updated')")
```

**Types:** `ds_mode` enum with values `mode_outer`, `mode_inner`, `mode_replace`, `mode_prepend`, `mode_append`, `mode_before`, `mode_after`, `mode_remove`.

**Example.** Append a new todo row to the live list across all clients on the channel:
```c
.post = {
  validate({"title", .validation = validate_not_empty}),
  query({.set_key = "new_todo", .db = "todos_db",
    .query = "insert into todos(title) values({{title}}) returning id, title;"}),
  ds_sse("todos",
    .target   = "#todo-list",
    .mode     = mode_append,
    .elements = {.template = "{{#new_todo}}<li id='todo-{{id}}'>{{title}}</li>{{/new_todo}}"}),
  redirect("todos")
}
```

![SSE / Datastar Flow](./08-sse-datastar-flow.svg)

For Datastar attributes, signals, and the wire-format SSE events that `ds_sse` produces, see the [Datastar reference](https://data-star.dev/reference) and specifically [SSE Events](https://data-star.dev/reference/sse_events).

#### tailwind

Brings [Tailwind CSS](https://tailwindcss.com) to MACH apps.

**Partial:** `{{>tailwind}}`, the standard Tailwind CDN runtime, served through MACH. Include in `<head>`.

**Example.**
```c
render(.template =
  "<html>"
    "<head>{{>tailwind}}</head>"
    "<body class='bg-gray-50 text-gray-900 p-8'>"
      "<h1 class='text-2xl font-bold'>Welcome</h1>"
    "</body>"
  "</html>")
```

For utility classes and configuration, see the [Tailwind docs](https://tailwindcss.com/docs).

#### session_auth

Adds session-based authentication: cookie-backed sessions, login/logout/signup actions, a guard step that redirects unauthenticated users to the login page, and a step that loads the full user record into context.

**Steps:**
- `logged_in()`: guard that redirects to the `login` resource when there's no active session. When there is one, sets `user_id` in context. Drop into a resource's shared `.steps` slot to gate the whole resource.
- `session()`: takes `user_id` from context and loads the full user record into context as `user`, including `id`, `full_name`, `short_name`. Run after `logged_in()` when verb pipelines need user details beyond just the id; skip it when only `user_id` is needed.
- `login()`, `logout()`, `signup()`: available for custom auth flows where you want to handle the action yourself. Most apps don't need these; `session_auth` provides `/login`, `/logout`, and `/signup` resources internally.

The login page's template is whatever asset is registered under the name `login` in `.context`. Override it to use your own form.

**Example.** A protected dashboard with full user info available:
```c
{"dashboard", "/dashboard", {logged_in(), session()},
  .get = {
    render(.template = "{{#user}}<h1>Welcome, {{full_name}}</h1>{{/user}}")
  }
}
```

For a resource that only needs the user id, drop `session()` and use just `logged_in()`:
```c
{"my_todos", "/me/todos", {logged_in()},
  .get = {
    query({.set_key = "todos", .db = "todos_db",
      .query = "select id, title from todos where user_id = {{user_id}};"}),
    render("todos")
  }
}
```

To customize the login page, register your own template under the name `login` in `.context`. The form posts to `/login`, where `session_auth`'s internal handler validates and authenticates.

**`static/login.mustache.html`**
```html
<html><body>
  <h1>Sign in</h1>
  <form method='post' action='{{url:login}}'>
    {{csrf:input}}
    <label>Username <input name='username' value='{{input:username}}' required></label>
    {{#error:username}}<span>{{error_message:username}}</span>{{/error:username}}
    <label>Password <input name='password' type='password' required></label>
    {{#error:password}}<span>{{error_message:password}}</span>{{/error:password}}
    <button>Sign in</button>
  </form>
</body></html>
```

```c
.context = {
  {"login", (asset){
    #embed "static/login.mustache.html"
  }}
}
```

Schema for users and sessions is created automatically on first boot.

#### Database engines (sqlite, postgres, mysql, redis, duckdb)

Each engine module enables one `<engine>_db` constant for use in `.databases.engine`, and registering the module is required for any database that uses it. MACH wraps each engine's native driver, so connection strings are passed through verbatim and accept whatever the underlying driver accepts.

| Module    | Engine constant | Docs |
|-----------|-----------------|------|
| `sqlite`  | `sqlite_db`     | [SQLite](https://sqlite.org/docs.html) |
| `postgres`| `postgres_db`   | [PostgreSQL](https://www.postgresql.org/docs/) |
| `mysql`   | `mysql_db`      | [MySQL](https://dev.mysql.com/doc/) |
| `redis`   | `redis_db`      | [Redis](https://redis.io/docs/) |
| `duckdb`  | `duckdb_db`     | [DuckDB](https://duckdb.org/docs/) |

Connection strings support `{{interpolation}}` for multi-tenant setups (see [Databases](#databases)).

**Example.** Same shape regardless of engine; only `.engine`, `.connect`, and the registered module change:
```c
.databases = {{
  .engine     = postgres_db,
  .name       = "blog_db",
  .connect    = "postgres://user:pass@localhost:5432/blog",
  .migrations = { "CREATE TABLE posts (...);" }
}},
.modules = {postgres}
```

### Static Files

Files in `public/` at the project root are served directly. Use it for images, fonts, pre-built CSS/JS, and other assets that don't need to be embedded in the binary. Reference them with `{{asset:filename}}`, which resolves to a URL with a content checksum and immutable cache headers.

**`public/` directory**: drop in static files; they are served at the root URL space.
```
public/
├── favicon.png
├── logo.png
└── styles.css
```

**`{{asset:filename}}`**: cache-busting helper for use inside templates.
```html
<link rel="icon" href="{{asset:favicon.png}}">
<link rel="stylesheet" href="{{asset:styles.css}}">
<script src="{{asset:app.js}}"></script>
```

### External Dependencies

MACH expects a containerized development environment: write standard C23 against the MACH APIs, no local toolchain required. Two ways to bring in third-party C libraries, plus two helpers for bridging foreign memory back to the arena.

**`/vendor` directory**: drop headers and libraries (`.so`, `.a`) here; the auto-compiler discovers, includes, and links them.
```
/vendor/
├── libsodium.h
└── libsodium.so
```

**Custom `Dockerfile`**: inherit from the MACH base image and `apt-get install` system dependencies; reference it from `compose.yml`.
```dockerfile
FROM mach:latest
RUN apt-get update && apt-get install -y libsodium-dev
```

**`allocate(bytes)`**: provides a buffer from the pipeline arena, reclaimed on request completion.
```c
char *buf = allocate(256);
```

**`defer_free(ptr)`**: schedules cleanup for pointers returned by external libraries (e.g. via `malloc`); runs when the arena is released.
```c
char *out = third_party_alloc(256);
defer_free(out);
```

---

## Architecture
How MACH compiles, executes, and protects an application at runtime.

* [Data-Oriented Pipelines](#data-oriented-pipelines)
* [Multi-Reactor Architecture](#multi-reactor-architecture)
* [Safe by Default](#safe-by-default)
* [String Interpolation](#string-interpolation)

### Data-Oriented Pipelines

The `mach()` function runs once at boot. The returned `config` is processed into an execution graph with precompiled pipelines, queries, and templates. Each incoming request then executes its matching pipeline as a sequence of pre-warmed steps.

![Boot-Time Compilation](./10-boot-time-compilation.svg)

### Multi-Reactor Architecture

MACH runs two types of reactors backed by a shared thread pool. The request/task/cpu ratio can be set in `compose.yml`.

- **Request reactors** handle HTTP traffic; each gets its own dedicated CPU core and event loop.
- **Task reactors** handle background work; each gets its own dedicated core, monitors the task database for pending and incomplete tasks, and processes cron schedules.
- **Shared thread pool** handles CPU-bound and blocking I/O work on the remaining cores.

When any reactor's pipeline executes an `exec()` step, the work is dispatched to the shared thread pool, which releases the reactor. When the call completes, the pipeline resumes on the original reactor. The `task()` step adds jobs to the task database, where they are picked up by task reactors. Tasks can call `task()` themselves to enqueue additional work.

Application code does not manage threads, mutexes, or locks. The architecture isolates request state to the pipeline's context.

![Multi-Reactor Architecture](./01-multi-reactor-architecture.svg)

### Safe by Default

MACH prevents common C and web vulnerabilities at the framework level.

#### Memory Safety

Each reactor maintains a pool of arena allocators. When a request arrives, the pipeline is assigned an arena, and all allocations draw from it. When the pipeline completes, the arena is cleared and returned to the pool. Application code does not call `malloc` or `free`, which avoids leaks, double-frees, and use-after-free.

All framework data structures (tables, records, strings) enforce bounds checking. Out-of-bounds reads and missing context values return `nullptr` rather than faulting. Pipelines that exceed their memory limit (default 5MB, configurable in `compose.yml`) abort with a 500, mitigating OOM denial-of-service.

#### SQL Injection Prevention

Interpolations such as `{{user_id}}` inside `query()` or `find()` are bound as parameters in prepared statements, preventing SQL injection at the framework level.

#### XSS Prevention

The `render()` step auto-escapes context values in Mustache templates, so malicious input is rendered as text. Raw HTML requires explicit opt-in via Mustache's standard unescape syntax: `{{{field}}}` or `{{&field}}`.

### String Interpolation

Any string (SQL queries, URLs, connection strings, templates) can reference values from the context with `{{context_key}}`. The same scopes described in [Context](#context) apply everywhere interpolation is used.

---

## Tooling

* [Development Environment](#development-environment)
* [Introspection](#introspection)
* [Testing](#testing)
* [Debugging](#debugging)
* [Deployment](#deployment)
* [Observability](#observability)
* [Project Management](#project-management)
* [Built With](#built-with)

### Development Environment
Built-in TUI editor with HMR, LSP support, integrated source control, and a topology-aware AI assistant. The AI uses the `app_info` command to inspect the full application topology (routes, pipelines, database schemas, event contracts, and module boundaries), so it reasons about the application's actual execution graph rather than just source text.

### Introspection
```bash
app_info                    # view topology
app_info resources          # list all resources
app_info pipelines          # inspect pipelines
app_info events             # view pub/sub map
app_info databases          # inspect schemas
```

### Testing
Built-in test runners for unit and end-to-end testing; no external framework setup required.
```bash
unit_tests                  # fast, criterion-based tests
e2e_tests                   # playwright-powered browser tests
```

### Debugging
Built-in debugging with pipeline-aware commands. Halt on individual pipeline steps, step through execution, and inspect the full pipeline context including nested tables and records.
```bash
app_debug                   # interactive debugger in the TUI
```

### Deployment
MACH deploys as a standard Docker container. It does not terminate TLS; production deployments place MACH behind a reverse proxy or load balancer (Nginx, Caddy, AWS ALB) to handle HTTPS.
```bash
app_build                   # outputs slim, optimized production Docker image
```

### Observability
Each pipeline step emits OpenTelemetry spans. Logs, traces, errors, and auto-profiling are visualized on the telemetry server at port 4000. No manual instrumentation required.

### Project Management
MACH ships with integrated project infrastructure: source control, issue tracking, wiki, forum, and a project website.

### Built With

| | |
|---|---|
| [C23](https://en.cppreference.com/w/c/23) | Language standard |
| [Docker](https://www.docker.com/) | Development environment, production images, stack orchestration |
| [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) / [libuv](https://libuv.org/) | HTTP server, event loops, async I/O, file watching, shared thread pool |
| [Mustach](https://gitlab.com/jobol/mustach) | Templating and string interpolation |
| [Jansson](https://github.com/akheron/jansson) | JSON parsing and generation |
| [curl](https://curl.se/) | HTTP client for fetch steps |
| [Fossil](https://fossil-scm.org/) | Source control, wiki, forum, issue tracker, project site |
| [Fresh](https://getfresh.dev/) | TUI editor |
| [clangd](https://clangd.llvm.org/) | Language server |
| [LLDB](https://lldb.llvm.org/) | Debugger |
| [Criterion](https://github.com/Snaipe/Criterion) | Unit testing |
| [Playwright](https://playwright.dev/) | End-to-end testing |
| [SigNoz](https://signoz.io/) + [OpenTelemetry](https://opentelemetry.io/) | APM, traces, logs, errors, dashboards |
| [Open Code](https://opencode.ai/) | AI assistant with custom agent and skill files |

---

## License

MACH is licensed under the [LGPL](./LICENSE).
