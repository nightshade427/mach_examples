# MACH

## Why MACH

MACH (Modern Asynchronous C Hypermedia) is a high-level, declarative platform for asynchronous, reactive web development in C.

* **Zero Boilerplate:** No build scripts, package managers, or ORMs to configure. It compiles and hot-reloads automatically.
* **Safe by Default:** Automatically handles memory, concurrency, and async I/O. No `malloc` or `free`. No threads, mutexes, or locks. All database queries use prepared statements. All code automatically emits OpenTelemetry spans, logs, and errors.
* **Durable by Default:** Tasks and events are automatically persisted and tracked. If the process crashes, incomplete tasks resume at the step where they left off. Undelivered events are replayed on the next boot.
* **Batteries Included:** Built-in SSE support and modules for Datastar, HTMX, Tailwind, SQLite, Postgres, MySQL, Redis/Valkey, DuckDB, and auth. Native multi-tenant database support.

---

## Table of Contents
* [Quick Start](#quick-start)
* [Philosophy](#philosophy)
* [Guide](#guide)
* [Architecture](#architecture)
* [Tooling](#tooling)
* [Reference](#reference)
* [License](#license)

---

## Quick Start

Everything runs in Docker, no other local dependencies needed. MACH uses C23.

```bash
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml

# Dev server on :3000, telemetry on :4000
# Includes file watching, auto compilation, hot code reloading, HMR
docker compose up
```

Create `main.c` in your project directory with the example below. MACH watches for changes and hot-reloads automatically. Use your own editor, or attach to the built-in TUI with `docker compose attach mach` for an integrated environment with editor, AI, LSP, and console.

### At A Glance

```c
#include <mach.h>
#include <sqlite.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          query({
            .set_key = "greeting",
            .db = "hello_db",
            .query =
              "select name "
              "from greetings "
              "limit 1;"
          }),
          render(.template =
            "<html>"
              "<body>"
                "{{#greeting}}"
                  "<p>Hello {{name}}</p>"
                "{{/greeting}}"
              "</body>"
            "</html>"
          )
        }
      }
    },

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
        "INSERT INTO greetings(name) "
        "VALUES('World');"
      }
    }},

    .modules = {sqlite}
  };
}
```

The `mach()` function returns a `config` struct that is your application. Resources define URL endpoints. Each verb (`.get`, `.post`, etc.) contains a pipeline: an ordered list of steps that transform a request into a response. Here, `.get` runs `query()` to fetch data, then `render()` to output HTML with Mustache templates. The database is declared alongside the app, complete with schema migrations and seed data.

> **A note on syntax.** MACH uses C designated initializers at different brace depths:
>
> | Braces | Meaning | Example |
> |--------|---------|---------|
> | `{}` | Single value or struct | `.get = { ... }` |
> | `{{}}` | Array of structs | `.databases = {{ ... }}` |
>
> Inside steps, each `{}` initializes one item. Steps that accept multiple items (like `query` and `validate`) use comma-separated items: `query({...}, {...})`.

---

## Philosophy

Applications are data transformations: data enters from sources, transforms through business logic, and exits to the client. MACH keeps each piece standard. Data comes from raw SQL, HTTP fetch, and JSON instead of ORMs. Business logic is plain C functions. Output is standard HTML, CSS, JS via Mustache templates.

These pieces compose inside pipelines: ordered lists of steps that transform a request into a response.

Tooling is standard: lldb for debugging, Playwright and Criterion for testing, OpenTelemetry for observability. All built in, nothing to configure.

### Everything is a String
The web is text: HTTP, HTML, JSON, SQL. MACH leans into this. The pipeline context stores and passes all data as safe, arena-backed strings.

No parsing, no serialization. You don't parse request parameters into typed structs. You don't serialize objects back to JSON. Data flows through the pipeline as strings, interpolated into SQL, templates, and URLs with `{{context_key}}`.

When you need a specific C type for business logic, you do that conversion explicitly inside your business logic in an `exec()` step.

### CLAD
MACH is designed around four principles.

* **(C)-omposable:** Applications are made of composable steps. You chain small, independent steps together into feature pipelines.
* **(L)-ocality Of Behavior:** The behavior of a unit of code is obvious just by looking at it. You do not jump between separate model, controller, and view trees to understand one feature. SQL, templates, and behavior for a feature live together.
* **(A)-utonomous:** Modules are completely self-contained. Each has its own database schemas, migrations, seeds, routes, UI, and logic. The compiler enforces strict boundaries between modules.
* **(D)-omain Based:** Each module owns exactly one distinct slice of the application. A `todos` module defines everything related to todos and absolutely nothing else.

CLAD is inspired by:

* [Data Oriented Design](https://youtu.be/rX0ItVEVjHc)
* [A Philosophy of Software Design](https://youtu.be/bmSAYlu0NcY)
* [CUPID](https://youtu.be/cyZDLjLuQ9g)
* [Self-Contained Systems](https://youtu.be/Jjrencq8sUQ)
* [Locality of Behavior](https://htmx.org/essays/locality-of-behaviour)

---

## Guide
The APIs and patterns you use to build applications with MACH.

* [Resource Pipelines](#resource-pipelines)
* [Pipeline Steps](#pipeline-steps)
* [Context](#context)
* [Conditionals](#conditionals)
* [Databases](#databases)
* [Static Files](#static-files)
* [Error and Repair Pipelines](#error-and-repair-pipelines)
* [Event Pipelines](#event-pipelines)
* [Task Pipelines](#task-pipelines)
* [Modules & Composition](#modules--composition)
* [External Dependencies](#external-dependencies)

### Resource Pipelines

MACH is resource-based, not route-based. Each entry in `.resources` defines a named resource at a URL with its supported HTTP verbs:

```c
config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          render(.template =
            "<h1>Welcome</h1>"
            "<a href='{{url:about}}'>About us</a>"
          )
        }
      },

      {"about", "/about",
        .get = {
          render(.template = "<p>about us</p>")
        }
      }
    }
  };
}
```

The resource names are used in `redirect()`, `reroute()`, and `{{url:name}}` not hard coded paths. Change the URL in one place and all references update.

Each resource supports verb pipelines: `.get`, `.post`, `.put`, `.patch`, `.delete`, and `.sse`. A verb pipeline is an ordered list of steps that transform a request into a response.

Clients select the verb through the request method, or by passing `http_method` as a query parameter, hidden form field, or any other mechanism that places it in context. This lets HTML forms (limited to GET/POST) reach any verb, and gives SSE a connection path.

For example, `GET /todos` hits `.get`, `POST /todos` hits `.post`, and `/todos?http_method=sse` connects to the SSE stream.

The router handles path specificity automatically. Exact matches (`/todos/active`) take priority over parameterized matches (`/todos/:id`), regardless of definition order. File uploads are available in context like any other request parameter.

#### Shared Steps

The optional third positional field is an array of steps that run before every verb pipeline on that resource. Use it for shared concerns like authorization or parameter validation:

```c
{"todo", "/todos/:id", {
  validate({"id",
    .validation = "^\\d{1,10}$",
    .message = "must be a number"
  })},

  .get = { /* id is validated before this runs */ },
  .patch = { /* id is validated before this runs */ },
  .delete = { /* id is validated before this runs */ }
}
```

#### SSE Endpoints

Declare an SSE channel on a resource with `.sse`. This creates a persistent streaming endpoint. Clients connect using the resource URL with the SSE verb: `new EventSource("/todos?http_method=sse")`. The `.sse` block supports a channel name as its first positional and an optional steps pipeline that runs when a client first connects.

```c
{"todos", "/todos",
  .sse = {"todos/{{user_id}}",
    // steps run on connect, e.g. send initial state
    query({"get_todos",
      .set_key = "todos",
      .db = "todos_db"
    }),
    sse(
      .event = "initial",
      .data = {"{{todos}}"}
    )
  }
}
```

![SSE / Datastar Flow](./08-sse-datastar-flow.svg)

### Pipeline Steps

Steps are the units of work in a pipeline. Each step receives the current request context, acts on it, and passes control to the next step.

* [validate](#validate)
* [find & query](#find--query)
* [join](#join)
* [fetch](#fetch)
* [exec](#exec)
* [emit](#emit)
* [task](#task)
* [sse](#sse)
* [ds_sse](#ds_sse)
* [render](#render)
* [headers & cookies](#headers--cookies)
* [redirect & reroute](#redirect--reroute)
* [nest](#nest)

#### validate

`validate()` checks request parameters (query string, form body, or URL parameters) against regex patterns. On success, each value is promoted from `input:name` to `name` in app scope, making it available to downstream steps. On failure, errors are stored in `error:name` and a `400 Bad Request` is triggered, resolved through the nearest [error/repair pipeline](#error-and-repair-pipelines).

When validating multiple parameters, all validations run to completion before triggering the error, so all errors are available at once for form re-rendering.

Validate a single required parameter with a regex pattern:

```c
.post = {
  validate({"title",
    .validation = "^\\S{1,16}$",
    .message = "must be 1-16 characters, no spaces"
  }),
  // input:title validated and promoted to app scope as "title"
  // pipeline continues with {{title}} available
}
```

Validate multiple parameters together:

```c
.post = {
  validate(
    {"email",
      .validation = validate_email,
      .message = "must be a valid email"
    },
    {"title",
      .validation = validate_not_empty,
      .message = "cannot be empty"
    }
  ),
  query({
    .db = "todos_db",
    .query =
      "insert into todos(email, title) values({{email}}, {{title}});"
  }),
  redirect("todos")
}
```

Use `.optional` to skip validation when a parameter is absent, or `.fallback` to inject a default value instead:

```c
.get = {
  validate(
    {"filter",
      .optional = true,
      .validation = "^(active|done)$",
      .message = "must be 'active' or 'done'"
    },
    {"page",
      .fallback = "1",
      .validation = "^\\d{1,4}$",
      .message = "must be a number"
    }
  ),
  // filter may or may not be in context
  // page is always in context: either the request value or "1"
}
```

URL parameters like `:id` in `/todos/:id` are also validated with `validate()`. Place shared validations in the resource's steps array so they run before any verb.

When validation fails, the error is stored in context under `error:name`, accessible in templates via `{{error:name}}`, `{{error_code:name}}`, and `{{error_message:name}}`. The raw input value remains available in `input:name` for re-rendering forms. See [Error and Repair Pipelines](#error-and-repair-pipelines) for the full pattern.

#### find & query

`find()` and `query()` execute database queries. Both specify `.db` for the database name and `.set` to store results in context. SQL can be referenced by name from `.context` as the positional field, or inlined with `.query`. Multiple queries in the same step execute concurrently. All queries use prepared statements.

> **context template or inline, not both.** Use the positional asset name `{"get_todos", ...}` **or** `.query = "..."`, never both in the same query item. The positional references a context asset by name; `.query` inlines the SQL directly.

The only difference: `find()` triggers a `404 Not Found` if no rows are returned; `query()` does not.

Fetch a collection using a context asset:

```c
.get = {
  query({"get_todos",
    .set_key = "todos",
    .db = "todos_db"
  }),
  render("todos")
}
```

Where the SQL is defined as a context asset:

```c
.context = {
  {"get_todos", (asset){
    #embed "get_todos.sql"
  }}
}
```

Fetch a single record, auto-triggering 404 if missing:

```c
.get = {
  find({"get_todo",
    .set_key = "todo",
    .db = "todos_db"
  }),
  render("todo")
}
```

Execute a write without storing results:

```c
.delete = {
  find({"delete_todo",
    .db = "todos_db"
  }),
  redirect("todos")
}
```

Run multiple queries concurrently in a single step:

```c
.get = {
  query(
    {"get_todos",
      .set_key = "todos",
      .db = "todos_db"
    },
    {"count_todos",
      .set_key = "count",
      .db = "todos_db"
    }
  ),
  render("todos")
}
```

For inline SQL without a context asset, use `.query`:

```c
query({
  .set_key = "todos",
  .db = "todos_db",
  .query =
    "select id, title, finished from todos;"
})
```

For transactions, use standard SQL transaction statements directly in your queries (`BEGIN`, `COMMIT`, `ROLLBACK`).

Individual queries within a single `query()` or `find()` step also support `.if_context` and `.unless_context`, letting you conditionally include or skip specific queries while running others concurrently:

```c
.get = {
  query(
    {"get_todos",
      .set_key = "todos",
      .db = "todos_db"
    },
    {"get_urgent_count",
      .if_context = "show_urgent",
      .set_key = "urgent_count",
      .db = "todos_db"
    }
  ),
  render("todos")
}
```

#### join

`join()` nests records from one context table into each matching record of another, like a SQL JOIN but in-memory across context tables. Useful when data comes from separate databases or queries and needs to be combined for rendering.

```c
.get = {
  query(
    {"get_projects",
      .set_key = "projects",
      .db = "projects_db"
    },
    {"get_todos",
      .set_key = "todos",
      .db = "todos_db"
    }
  ),
  join(
    .target_table_key = "projects",
    .target_field_key = "id",
    .nested_table_key = "todos",
    .nested_field_key = "project_id",
    .target_join_field_key = "todos"
  ),
  render("projects")
}
```

After the `join()` step, each project record contains a `todos` field with its matching todo records:

```html
{{#projects}}
  <h2>{{name}}</h2>
  <ul>
    {{#todos}}
      <li>{{title}}</li>
    {{/todos}}
  </ul>
{{/projects}}
```

#### fetch

`fetch()` makes an HTTP request and stores the response in context. JSON responses are automatically parsed into tables and records (with nested tables for nested JSON). Text responses are stored as a plain string.

Fetch data from an external API:

```c
.get = {
  fetch(
    .set_key = "weather",
    .url = "https://api.weather.dev/forecast?city={{city}}"
  ),
  render("forecast")
}
```

POST with a JSON body and custom headers:

```c
.post = {
  // ... query and join steps to build "order" in context ...
  fetch(
    .set_key = "receipt",
    .url = "https://api.payments.dev/charge",
    .method = http_post,
    .headers = {
      {"Authorization", "Bearer {{api_key}}"}
    },
    .json = "order"
  ),
  render("receipt")
}
```

The `.json` field serializes a context value as the request body. Nested context tables produce nested JSON. The `.text` field sends a context value as a plain text body instead.

#### exec

`exec()` calls a C function or block with access to the full pipeline context via the Imperative API. It dispatches execution to the shared thread pool, immediately releasing the reactor to handle other work. When the function completes, the pipeline resumes on the original reactor. This makes `exec()` safe for blocking I/O and CPU-heavy work without stalling any reactor.

Use an inline block for short logic that doesn't need a separate function:

```c
.get = {
  query({"get_challengers",
    .set_key = "challengers",
    .db = "pokemon_db"
  }),
  exec(^(){
    auto const t = get("challengers");
    auto const p0 = table_get(t, 0);
    auto const p1 = table_get(t, 1);
    record_set(p0, "opponent_id", record_get(p1, "id"));
    record_set(p1, "opponent_id", record_get(p0, "id"));
  }),
  render("home")
}
```

Use `.call` to reference a named function:

```c
.get = {
  query({"get_challengers",
    .set_key = "challengers",
    .db = "pokemon_db"
  }),
  exec(.call = assign_opponents),
  render("home")
}
```

```c
void assign_opponents() {
  auto const t = get("challengers");
  auto const p0 = table_get(t, 0);
  auto const p1 = table_get(t, 1);
  record_set(p0, "opponent_id", record_get(p1, "id"));
  record_set(p1, "opponent_id", record_get(p0, "id"));
}
```

All values set through the Imperative API become part of the pipeline context.

To trigger an error/repair pipeline from inside an `exec()`, use `error_set()`:

```c
void validate_ownership() {
  if (!has("is_owner"))
    error_set("auth", (error){
      .code = http_not_authorized,
      .message = "not the owner"
    });
}
```

#### emit

`emit()` triggers an internal pub/sub event. Other modules subscribe in their `.events` array and react independently, with no direct dependency on the emitter. See [Event Pipelines](#event-pipelines).

```c
.post = {
  // ... validate and query steps ...
  emit("todo_created"),
  redirect("todos")
}
```

#### task

`task()` adds a named job to the task database and continues the pipeline immediately. Fire-and-forget: the calling pipeline does not wait. Task reactors pick up queued jobs and execute their pipelines. See [Task Pipelines](#task-pipelines).

```c
.post = {
  // ... validate and query steps ...
  task("recount_todos"),
  redirect("todos")
}
```

#### sse

`sse()` pushes a Server-Sent Event. With `.channel`, the event is broadcast to all connected clients on that channel. Without `.channel`, the event is returned directly to the requesting client.

```c
.post = {
  // ... validate and query steps ...
  sse(
    .channel = "todos/{{user_id}}",
    .event = "new_todo",
    .data = {"{{todo}}"}
  )
}
```

The `.event` and `.data` fields map directly to the SSE protocol's `event:` and `data:` lines. `.data` is an array for multi-line SSE data. `.comment` maps to the SSE `:` comment line. See [SSE Endpoints](#sse-endpoints) for declaring SSE channels on resources.

#### ds_sse

[Datastar](https://data-star.dev) is a hypermedia framework where the server pushes DOM updates and reactive state to the client over SSE. The `datastar` module provides `ds_sse`, which combines SSE with DOM updates by pushing Datastar-formatted events targeting specific elements.

```c
.post = {
  // ... validate and query steps ...
  ds_sse("todos/{{user_id}}",
    .target = "todos",
    .mode = mode_prepend,
    .elements = {"todo"}
  )
}
```

The first positional field is the `.channel`. `.target` specifies the DOM element ID. `.mode` controls fragment insertion: `mode_outer`, `mode_inner`, `mode_replace`, `mode_prepend`, `mode_append`, `mode_before`, `mode_after`, and `mode_remove`. `.elements` accepts render fields (the positional is the asset name). Like `sse()`, without a channel the event goes directly to the requesting client.

Use `.signals` to update client-side reactive state without touching the DOM:

```c
.post = {
  // ... validate and query steps ...
  ds_sse("todos/{{user_id}}",
    .signals = "{\"count\": {{count}}}"
  )
}
```

#### render

`render()` outputs a Mustache template using the current pipeline context. Templates can be referenced by name from `.context` as the positional field, or inlined with `.template`.

Reference a context asset by name:

```c
render("todos")
```

Inline template with `.template`:

```c
render(.template =
  "<h1>Hello {{name}}</h1>"
  "<p>You have <strong>{{count}}</strong> todos remaining.</p>"
  "<ul>"
    "{{#todos}}"
      "<li>{{title}}</li>"
    "{{/todos}}"
  "</ul>"
)
```

> **No dot notation.** MACH uses Mustache sections, not dot paths. To access fields on a record or table, open a section and reference fields inside it. `{{blog.title}}` is **wrong**; use `{{#blog}}{{title}}{{/blog}}` instead.

Using a layout partial from `.context`:

```c
render(.template =
  "{{< layout}}"
    "{{$body}}"
      "<p>about us</p>"
    "{{/body}}"
  "{{/layout}}"
)
```

Where the layout is defined at the root level:

```c
.context = {
  {"layout",
    "<html>"
      "<body>"
        "{{$body}}{{/body}}"
      "</body>"
    "</html>"
  }
}
```

Use `.json_table_key` to serialize a context value as a JSON response. The content type is automatically set to `application/json`, and nested context tables produce nested JSON:

```c
.get = {
  query({"get_todos",
    .set_key = "todos",
    .db = "todos_db"
  }),
  render(.json_table_key = "todos")
}
```

#### Template Engines

MACH includes two template engines: Mustache (default) and MDM (Markdown with Mustache). Use `.engine` to select a different engine. Additional engines can be added as modules.

```c
render(
  .engine = "mdm",
  .template =
    "# Hello {{name}}\n"
    "\n"
    "You have **{{count}}** todos remaining.\n"
    "\n"
    "{{#todos}}\n"
    "- {{title}}\n"
    "{{/todos}}"
)
```

#### headers & cookies

`headers()` and `cookies()` set HTTP response headers and cookies declaratively. Both accept an array of name/value pairs. Values support string interpolation.

```c
.get = {
  headers({
    {"X-Request-Id", "{{request_id}}"},
    {"Cache-Control", "no-store"}
  }),
  cookies({
    {"session", "{{session_id}}"}
  }),
  render("profile")
}
```

#### redirect & reroute

`redirect()` sends a `302` to the client, causing the browser to navigate to a new URL. `reroute()` re-enters the router server-side, executing the target resource's pipeline within the same request. Both accept a resource name as the positional field, not a URL path.

Redirect after a form submission (POST-redirect-GET):

```c
.post = {
  validate({"title",
    .validation = "^\\S{1,16}$",
    .message = "1-16 characters"
  }),
  query({"create_todo",
    .db = "todos_db"
  }),
  redirect("todos")
}
```

Reroute to serve another resource's pipeline without a round trip:

```c
{"home", "/",
  .get = {
    reroute("todos")
  }
}
```

#### nest

`nest()` groups multiple pipeline steps into a single composite step. When multiple steps share the same `.if_context` or `.unless_context` condition, use `nest()` to apply the condition once instead of repeating it on each step:

```c
  // without nest: .if_context = "is_urgent" repeated 3 times
  // with nest: applied once to the entire group

  nest(
    {query({
       .db = "todos_db",
       .query = "update todos set priority = 'high' where id = {{id}};"
     }),
     emit("urgent_todo"),
     render("urgent_confirmation")
    },
    .if_context = "is_urgent"
  ),
  render("standard_confirmation",
    .unless_context = "is_urgent"
  )
```

### Context

The `.context` block injects variables and assets into the pipeline context at the root level. Values like site names or HTML layouts become available to templates without passing them on every request. Use `(asset)` with `#embed` to bake files into the binary at compile time. Both templates and SQL can be stored as context assets and referenced by name in `render()`, `query()`, and `find()` steps.

Sensitive data and API keys are handled via standard Docker secrets. Secrets exposed to the container are automatically available in the pipeline context.

```c
config mach() {
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          render("home")
        }
      }
    },

    .context = {
      {"layout", (asset){
        #embed "layout.mustache.html"
      }},
      {"home", (asset){
        #embed "home.mustache.html"
      }},
      {"get_todos", (asset){
        #embed "get_todos.sql"
      }}
    }
  };
}
```

#### Context Scopes

The pipeline context uses scoped namespaces to separate concerns:

* **`input:xxx`**: Raw request parameters (query string, form body, URL parameters). All incoming request data lands in the `input` scope. Access in templates with `{{input:title}}`, `{{input:email}}`, etc.
* **`error:xxx`**: Validation and error data. Access in templates with `{{error:name}}`, `{{error_code:name}}`, `{{error_message:name}}`.
* **`xxx`** (app scope): Application data: query results, context variables, and validated inputs. No prefix.

The `validate()` step bridges input to app scope: when validation passes, `.name` promotes the value from `input:name` to `name` in app scope. Downstream steps work with validated app-scope values, not raw input.

![Context Scoping](./07-context-scoping.svg)

### Conditionals

Any step supports `.if_context` and `.unless_context`, which name a context variable. `.if_context` executes the step only when the value is present; `.unless_context` executes only when the value is absent.

This works for any context value: `.if_context = "title"` runs only if a `title` form parameter was submitted; `.unless_context = "is_htmx"` runs only when the request is not an HTMX request.

Use `.if_context` and `.unless_context` together for clean branching:

```c
.get = {
  query({"get_todos",
    .set_key = "todos",
    .db = "todos_db"
  }),
  render("todos_fragment",
    .if_context = "is_htmx"
  ),
  render("todos_full_page",
    .unless_context = "is_htmx"
  )
}
```

For multi-state branching, combine with `exec()` to set context flags:

```c
.post = {
  find({"get_todo",
    .set_key = "todo",
    .db = "todos_db"
  }),
  exec(.call = classify_todo),
  query({
    .if_context = "is_urgent",
    .db = "todos_db",
    .query = "update todos set priority = 'high' where id = {{id}};"
  }),
  emit("urgent_todo",
    .if_context = "is_urgent"
  ),
  render("urgent_confirmation",
    .if_context = "is_urgent"
  ),
  render("standard_confirmation",
    .unless_context = "is_urgent"
  )
}

void classify_todo() {
  auto const todos = get("todo");
  auto const todo = table_get(todos, 0);
  auto const due = record_get(todo, "due_date");

  if (is_within_24h(due))
    set("is_urgent", "1");
}
```

### Databases

Each database is defined with `.engine`, `.name`, `.connect`, and optional `.migrations` and `.seeds`. When `.migrations` or `.seeds` are provided, MACH tracks them in a `mach_meta` table. Migrations run automatically when the first connection to that database is established. They are forward-only and index-based: they run in array order, each applied once, with new migrations appended to the end. Seeds are idempotent and safe to re-run.

All engines follow the same configuration pattern. Set `.engine` to the appropriate constant: `sqlite_db`, `postgres_db`, `mysql_db`, `redis_db` (Redis/Valkey), or `duckdb_db`.

```c
.databases = {{
  .engine = sqlite_db,
  .name = "todos_db",
  .connect = "file:todo.db?mode=rwc",
  .migrations = {
    "CREATE TABLE todos ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "title TEXT NOT NULL,"
      "finished INTEGER DEFAULT 0"
    ");"
  },
  .seeds = {
    "INSERT INTO todos(title) "
      "VALUES('Learn MACH'), "
      "('Build something cool');"
  }
}}
```

Multi-tenant databases use interpolation in the `.connect` string. The interpolated value can be a user, organization, or any other scope. Connections are pooled with LRU eviction. Active tenants stay warm, idle connections get reclaimed.

![Database Multi-Tenancy](./09-database-multi-tenancy.svg)

```c
.databases = {{
  .engine = sqlite_db,
  .name = "todos_db",
  .connect = "file:{{user_id}}_todo.db?mode=rwc",
  .migrations = {(asset){
    #embed "create_todos_table.sql"
  }}
}}
```

### Static Files

Create a `public` directory in your project root. Files placed there are served directly. Use it for images, fonts, pre-built CSS/JS, and other assets that don't need compilation into the binary.

Reference static files in templates with the `{{asset:filename}}` helper. It resolves to the file's URL path with a content-based checksum appended for cache-busting. Responses are served with immutable cache headers, so browsers cache indefinitely and automatically fetch the new version when content changes.

```html
<head>
  <link rel="icon" href="{{asset:favicon.png}}">
  <link rel="stylesheet" href="{{asset:styles.css}}">
</head>
<body>
  <img src="{{asset:logo.png}}">
  <script src="{{asset:app.js}}"></script>
</body>
```

### Error and Repair Pipelines

When a pipeline step fails, execution halts and MACH searches for a handler: first on the resource, then its module, then the root config. Unhandled errors surface in the TUI console and telemetry.

**Errors** are terminal: the matching error pipeline sends a response and ends the request.

**Repairs** are resumable: the repair pipeline fixes the context, then resumes the original pipeline at the step after the failure. If no matching repair is found, resolution falls through to errors.

![Error Resolution](./04-error-resolution.svg)

```c
config mach(){
  return (config) {
    .errors = {
      {http_not_found, {
        render("404")
      }}
    },

    .repairs = {
      {http_not_authorized, {
        exec(.call = refresh_session_token)
      }}
    }
  };
}
```

Errors can be scoped to individual resources. This is useful for form validation: when a POST fails validation, re-render the form with error messages and the original input values:

```c
{"todos", "/todos",
  .get = {
    render("form")
  },
  .post = {
    validate(
      {"email",
        .validation = validate_email,
        .message = "must be a valid email"
      },
      {"title",
        .validation = validate_not_empty,
        .message = "cannot be empty"
      }
    ),
    query({"create_todo",
      .db = "todos_db"
    }),
    redirect("todos")
  },

  .errors = {
    {http_bad_request, {
      render("form")
    }}
  }
}
```

The `"form"` asset is shared between the GET and the error handler. Define it in `.context` at the root level:

```c
.context = {
  {"form", (asset){
    #embed "todos_form.mustache.html"
  }}
}
```

In the template, `{{input:field}}` repopulates the form and `{{error:field}}` / `{{error_message:field}}` display errors:

```html
<input name="email" value="{{input:email}}">
{{#error:email}}
  <span>{{error_message:email}}</span>
{{/error:email}}
<input name="title" value="{{input:title}}">
{{#error:title}}
  <span>{{error_message:title}}</span>
{{/error:title}}
```

### Event Pipelines

Internal pub/sub for cross-module communication. The publisher doesn't know who listens; the subscriber doesn't know who emits. Adding a new subscriber is a new module with an `.events` entry, no changes to the publisher.

Events are durable by default. When `.publishes` is defined anywhere in the application, MACH automatically creates a `mach_events` database to track delivery. If the process crashes, undelivered events are replayed on the next boot.

A publisher declares its outbound events in `.publishes` with `.with` specifying which context keys to pass along. Subscribers react in `.events` pipelines:

![Event Pub/Sub](./03-event-pub-sub.svg)

```c
// todos/todos.c: publishes "todo_created"
config todos(){
  return (config) {
    .name = "todos",
    .publishes = {
      {"todo_created", .with = {"user_id", "title"}}
    },
    // ...
  };
}

// activity/activity.c: subscribes to "todo_created"
config activity(){
  return (config) {
    .name = "activity",
    .events = {
      {"todo_created", {
        query({"insert_activity",
          .db = "activity_db"
        })
      }}
    },
    // ...
  };
}
```

### Task Pipelines

Tasks are named pipelines that run asynchronously on task reactors. Fire-and-forget: the calling pipeline continues immediately without waiting. Tasks are defined at the module or root level.

Tasks are durable by default. When `.tasks` is defined, MACH automatically creates a `mach_tasks` database to track task state. The pipeline context is checkpointed after each step. If the process crashes mid-task, execution resumes at the exact step where it left off. A task five steps into an eight-step pipeline restarts at step six, not step one.

Use `.accepts` to pull specific context keys from the caller into the task. Use `.cron` to schedule recurring tasks. Tasks can enqueue additional tasks via `task()`.

```c
.tasks = {
  {"recount_todos", .accepts = {"user_id"}}, {
    query({
      .db = "todos_db",
      .query = "update users set todo_count = ..."
    })
  },

  {"daily_digest", .cron = "0 8 * * *"}, {
    query({
      .db = "todos_db",
      .query = "insert into digest_reports ..."
    })
  },
}
```

### Modules & Composition

Every MACH app and module returns a `config` struct. A module is simply a function that returns `config`, owning its own resources, databases, migrations, and event contracts:

```c
// todos/todos.c
#include <mach.h>
#include <sqlite.h>

config todos(){
  return (config) {
    .name = "todos",

    .resources = {
      {"todos", "/todos",
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
            .message = "1-16 chars, no spaces"
          }),
          query({"create_todo",
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
      {"create_todo", (asset){
        #embed "create_todo.sql"
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

    .modules = {sqlite}
  };
}
```

The root `main.c` includes modules via `#include` and registers them in `.modules`. Configurations merge: if the root and a module both define something with the same name (a context variable, a database, an error handler), the root's definition wins.

![App Composition Tree](./05-app-composition-tree.svg)

```c
// main.c
#include <mach.h>
#include <session_auth.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config) {
    .resources = {
      {"home", "/", {session()},
        .get = {
          render("home")
        }
      }
    },

    .context = {
      {"layout", (asset){
        #embed "static/layout.mustache.html"
      }},
      {"home", (asset){
        #embed "static/home.mustache.html"
      }}
    },

    .errors = {
      {http_not_found, {
        render("404")
      }}
    },

    .modules = {todos, activity, session_auth}
  };
}
```

Each module is a directory containing its `.c` file alongside the SQL and template files it references via `#embed`:

```
├── activity/                        # activity module
│   ├── activity.c                   # module function
│   ├── activity.mustache.html       # templates
│   ├── create_activity_table.sql    # migrations
│   ├── get_activities.sql           # queries
│   └── insert_activity.sql
├── todos/                           # todos module
│   ├── todos.c
│   ├── todos.mustache.html
│   ├── create_todos_table.sql
│   ├── create_todo.sql
│   └── get_todos.sql
├── static/                          # root-level templates (not a module)
│   ├── layout.mustache.html
│   ├── home.mustache.html
│   └── 404.mustache.html
├── public/                          # static files, served directly
│   └── favicon.png
└── main.c                           # app entry point
```

Modules don't call each other directly; they communicate through pub/sub events.

![Middleware Scoping](./06-middleware-scoping.svg)

### External Dependencies

MACH enforces a containerized development environment. You write standard C23 using MACH APIs, no local toolchain required. For third-party C libraries:

1. **The `/vendor` Directory:** Drop headers and libraries (`.so`, `.a`) into `/vendor` in your project root. The auto-compiler will discover, include, and link them.
2. **Custom Dockerfile:** For system-level dependencies (e.g., `libsodium`), create a `Dockerfile` that inherits from the MACH base image, run `apt-get install`, and update your `compose.yml` to build from it.

#### Memory Management with External Libraries
Third-party memory must be bridged to the arena to maintain safety:

* **`allocate(bytes)`**: Provides a buffer from the pipeline's arena, reclaimed automatically on request completion.
* **`defer_free(ptr)`**: Schedules cleanup for pointers allocated by external libraries (e.g., via `malloc`). Executed when the arena is released.

---

## Architecture
How MACH compiles, executes, and protects your application at runtime.

* [Data-Oriented Pipelines](#data-oriented-pipelines)
* [Multi-Reactor Architecture](#multi-reactor-architecture)
* [Safe by Default](#safe-by-default)
* [String Interpolation](#string-interpolation)

### Data-Oriented Pipelines
The `mach()` function runs once at boot. The returned `config` is processed into an optimized execution graph with precompiled pipelines, queries, and templates. Each incoming request then executes its matching pipeline as a sequence of pre-warmed steps.

![Boot-Time Compilation](./10-boot-time-compilation.svg)

![Request Pipeline Flow](./02-request-pipeline-flow.svg)

### Multi-Reactor Architecture
MACH runs two types of reactors backed by a shared thread pool.

The request/task/cpu ratio can be specified in `compose.yml`.

**Request Reactors** handle HTTP traffic: each gets its own dedicated CPU core and event loop.

**Task Reactors** handle background work: each gets its own dedicated core, monitors the task database for pending and incomplete tasks, and handles cron schedule processing.

**Shared Thread Pool** handles CPU-bound and blocking I/O work: uses the remaining cores.

When any reactor's pipeline executes an `exec()` step, the work is dispatched to the shared thread pool, immediately releasing the reactor. When the invocation completes, the pipeline resumes on the original reactor. The `task()` step adds jobs to the task database, where they are picked up by task reactors. Tasks can call `task()` themselves to enqueue additional work.

Application code doesn't need to manage threads, mutexes, or locks. The multi-reactor architecture isolates request state to the pipeline's context.

![Multi-Reactor Architecture](./01-multi-reactor-architecture.svg)

### Safe by Default

MACH prevents common C and web vulnerabilities at the framework level.

#### Memory Safety
Each reactor maintains a pool of arena allocators. When a request arrives, the pipeline is assigned an arena. All allocations draw from that arena. When the pipeline completes, the arena is instantly cleared and returned to the pool. Application code never calls `malloc` or `free`. No leaks, no double-frees, no use-after-free.

All framework data structures (tables, records, strings) enforce bounds checking. Out-of-bounds reads and missing context values return `nullptr` instead of faulting. Pipelines that exceed their memory limit (default 5MB, configurable via `compose.yml`) safely abort with a 500 error, preventing OOM denial-of-service.

#### SQL Injection Prevention
Interpolations like `{{user_id}}` inside `query()` or `find()` are automatically bound as parameters in prepared statements. SQL injection is prevented at the framework level.

#### XSS Prevention
The `render()` step auto-escapes all context values in Mustache templates. Malicious input is rendered as text. Raw HTML requires an explicit opt-in via the `{{raw:field}}` helper.

### String Interpolation
Anywhere you write a string (SQL queries, URLs, connection strings, templates) you can use `{{context_key}}` to reference values from the pipeline context. Values are scoped:

`{{field}}` for validated app-scoped values.

`{{input:field}}` for raw request parameters.

`{{error:field}}`, `{{error_code:field}}`, `{{error_message:field}}` for errors.

---

## Tooling

### Development Environment
Built-in TUI editor with HMR, LSP support, integrated source control, and a topology-aware AI assistant. The AI has access to the full application structure via `app_info`, routes, pipelines, database schemas, event contracts, and module boundaries, so it can reason about your application's actual execution graph, not just source text.

### Introspection
```bash
app_info                    # view topology
app_info resources          # list all resources
app_info pipelines          # inspect pipelines
app_info events             # view pub/sub map
app_info databases          # inspect schemas
```

### Testing
Built-in test runners for unit and end-to-end testing. No external framework setup required.
```bash
unit_tests                  # fast, criterion-based tests
e2e_tests                   # playwright-powered browser tests
```

### Debugging
```bash
app_debug                   # interactive debugger in the TUI
```
Built in debugging with pipeline-aware commands. Halt on individual pipeline steps, step through execution, and inspect the full pipeline context including nested tables and records.

### Deployment
```bash
app_build                   # outputs slim, optimized production Docker image
```
MACH deploys as a standard Docker container. It does not terminate TLS; production deployments should place MACH behind a reverse proxy or load balancer (e.g., Nginx, Caddy, AWS ALB) to handle HTTPS.

### Observability
Every pipeline step automatically emits OpenTelemetry spans. Logs, traces, errors, and auto-profiling are visualized on the telemetry server at port 4000. No manual instrumentation required.

### Project Management
MACH ships with integrated project infrastructure. Issue tracking, wiki, forum, and a project website are built in and available out of the box.

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

## Reference
Quick lookup for all configuration fields, step APIs, and module interfaces. See the [Guide](#guide) for examples and detailed explanations.
* [Global Configuration](#global-configuration)
* [Resource Pipelines](#resource-pipelines-resources)
* [Event Pipelines](#event-pipelines-events)
* [Task Pipelines](#task-pipelines-tasks)
* [Error Pipelines](#error-pipelines-errors)
* [Repair Pipelines](#repair-pipelines-repairs)
* [Pipeline API](#pipeline-api)
* [Validators API](#validators-api)
* [Imperative API](#imperative-api)
* [Constants & Enums](#constants--enums)
* [Template Helpers](#template-helpers)
* [Modules](#modules)

### Global Configuration
Fields marked *(pos)* are positional (set by position, not `.name =` syntax).

| Field | Description |
|-------|-------------|
| `.name` *(pos)* | Application or module identifier |
| `.modules` | Modules to compose |
| `.databases` | Data stores (`.name`, `.engine`, `.connect`, `.migrations`, `.seeds`) |
| `.publishes` | Outbound event contracts (`.event` *(pos)*, `.with`) |
| `.events` | Collection of Event Pipelines |
| `.resources` | Collection of Resource Pipelines |
| `.context` | Global variables and `#embed` assets (`.name` *(pos)*, `.value` *(pos)*) |
| `.errors` | Global Error Pipelines (bottom-up fallback) |
| `.repairs` | Global Repair Pipelines (bottom-up fallback) |
| `.tasks` | Task Pipelines (module and root level) |

### Resource Pipelines (`.resources`)
Each resource entry accepts:

| Field | Description |
|-------|-------------|
| `.name` *(pos)* | Resource identifier (used by `redirect()`, `reroute()`, `{{url:name}}`) |
| `.url` *(pos)* | URL pattern (e.g. `/todos/:id`) |
| `.steps` *(pos)* | Shared steps that run before each verb pipeline |
| `.mime` | Default response content type (defaults to `mime_html`) |
| `.get` `.post` `.put` `.patch` `.delete` | Verb pipelines (`.steps` only) |
| `.sse` | SSE endpoint (`.channel` *(pos)*, `.steps`) |
| `.errors` `.repairs` | Resource-scoped pipelines |

### Event Pipelines (`.events`)
Reacts to pub/sub events triggered by `emit()`. Auto-database: `mach_events` (created when `.publishes` is defined).

| Field | Description |
|-------|-------------|
| `.event` *(pos)* | Event name to subscribe to |
| `.steps` *(pos)* | Pipeline steps |
| `.errors` `.repairs` | Scoped pipelines |

### Task Pipelines (`.tasks`)
Run on task reactors. Fire-and-forget via `task()`, or scheduled via `.cron`. Defined at module or root level. Task reactors monitor the task database for pending/incomplete work and handle cron processing. Context is checkpointed after each step; incomplete tasks resume at the next step on recovery. Tasks can enqueue additional tasks via `task()`. Auto-database: `mach_tasks` (created when `.tasks` is defined).

| Field | Description |
|-------|-------------|
| `.name` *(pos)* | Task identifier |
| `.cron` | Schedule interval |
| `.accepts` | Context keys to pull from caller |
| `.steps` *(pos)* | Pipeline steps |
| `.errors` `.repairs` | Scoped pipelines |

### Error Pipelines (`.errors`)
Terminal. Sends response and ends request. Resolve bottom-up: resource → module → root.

| Field | Description |
|-------|-------------|
| `.error_code` *(pos)* | HTTP status to match |
| `.steps` *(pos)* | Pipeline steps |

### Repair Pipelines (`.repairs`)
Resumable. Fixes context, then resumes original pipeline at the next step. Same bottom-up scoping as `.errors`. Falls through to `.errors` if no match.

| Field | Description |
|-------|-------------|
| `.error_code` *(pos)* | HTTP status to match |
| `.steps` *(pos)* | Pipeline steps |

### Pipeline API
All steps accept `.if_context` and `.unless_context` for conditional execution. Fields marked *(pos)* are positional.

| Step | Description | Fields |
|------|-------------|--------|
| `validate` | Validate inputs, promote to app scope | Array of: `.param_key` *(pos)*, `.optional`, `.fallback`, `.validation`, `.message` |
| `find` | Query, 404 if no rows | Array of: `.template_key` *(pos)*, `.db`, `.set_key`, `.query`, `.if_context`, `.unless_context` |
| `query` | Query (concurrent) | Array of: `.template_key` *(pos)*, `.db`, `.set_key`, `.query`, `.if_context`, `.unless_context` |
| `join` | Nest records across context tables | `.target_table_key`, `.target_field_key`, `.nested_table_key`, `.nested_field_key`, `.target_join_field_key` |
| `fetch` | HTTP request, store response | `.set_key`, `.url`, `.method`, `.headers`, `.text`, `.json` |
| `render` | Output template or JSON | `.template_key` *(pos)*, `.template`, `.status`, `.mime`, `.engine`, `.json_table_key` |
| `sse` | Push SSE data | `.channel` *(pos)*, `.event`, `.comment`, `.data` |
| `exec` | Call C function or block on shared thread pool | Block *(pos)*, `.call` |
| `emit` | Trigger pub/sub event | `.event` *(pos)* |
| `task` | Fire-and-forget to task reactors | `.name` *(pos)* |
| `headers` | Set response headers | Array of name/value pairs *(pos)* |
| `cookies` | Set response cookies | Array of name/value pairs *(pos)* |
| `redirect` | 302 redirect | `.resource` *(pos)* |
| `reroute` | Server-side re-route | `.resource` *(pos)* |
| `nest` | Group steps into one | `.steps`, `.if_context`, `.unless_context` |
| `ds_sse` | Push Datastar SSE | `.channel` *(pos)*, `.target`, `.mode`, `.elements` (render_config), `.signals`, `.js` |

### Validators API
Regex macros for use with `validate()`. Define your own the same way: `#define validate_zipcode "^\\d{5}$"`.

| Category | Validators |
|----------|-----------|
| Strings | `validate_not_empty`, `validate_alpha`, `validate_alphanumeric`, `validate_slug`, `validate_no_html` |
| Numbers | `validate_integer`, `validate_positive`, `validate_float`, `validate_percentage` |
| Identity | `validate_email`, `validate_uuid`, `validate_username` |
| Dates & Times | `validate_date`, `validate_time`, `validate_datetime` |
| Web | `validate_url`, `validate_ipv4`, `validate_hex_color` |
| Codes & IDs | `validate_zipcode_us`, `validate_phone_e164`, `validate_cron` |
| Security | `validate_no_sqli`, `validate_token`, `validate_base64` |
| Boolean-ish | `validate_boolean`, `validate_yes_no`, `validate_on_off` |

### Imperative API
Available inside `exec()`. All types defined in `mach.h`.

**Context**

| Function | Description |
|----------|-------------|
| `void* get(string const name)` | Retrieve value from pipeline context |
| `void set(string const name, void const *value)` | Set value in pipeline context |
| `bool has(string const name)` | Check if value exists in pipeline context |

**Memory & Strings**

| Function | Description |
|----------|-------------|
| `void* allocate(int const bytes)` | Allocate from pipeline arena |
| `void defer_free(void const *ptr)` | Schedule cleanup for non-arena memory (e.g. third-party `malloc`) |
| `string format(string const format_string)` | Interpolate `{{context_key}}` placeholders |

**Errors**

| Function | Description |
|----------|-------------|
| `void error_set(string const name, error const)` | Trigger error/repair pipeline (`.code`, `.message`) |
| `error error_get(string const name)` | Retrieve error by name |
| `bool error_has(string const name)` | Check if error exists |

Errors from `error_set()` and `validate()` share the `error` scope: `{{error:name}}`, `{{error_code:name}}`, `{{error_message:name}}`.

**Data (Tables)**

| Function | Description |
|----------|-------------|
| `table table_new()` | Create empty table |
| `int table_count(table const)` | Number of records |
| `record table_get(table const, int const index)` | Record at index |
| `void table_add(table const, record const)` | Append record |
| `void table_remove(table const, record const)` | Remove specific record |
| `void table_remove_at(table const, int const index)` | Remove record at index |

**Data (Records)**

| Function | Description |
|----------|-------------|
| `record record_new()` | Create empty record |
| `void record_set(record const, string const name, string const value)` | Set field |
| `string record_get(record const, string const name)` | Get field value |
| `void record_remove(record const, string const name)` | Remove field |

### Constants & Enums

| MIME Types | Value |
|-----------|-------|
| `mime_html` | `text/html` |
| `mime_txt` | `text/plain` |
| `mime_sse` | `text/event-stream` |
| `mime_json` | `application/json` |
| `mime_js` | `application/javascript` |

| HTTP Methods | HTTP Statuses | |
|-------------|--------------|---|
| `http_get` | `http_ok` | 200 |
| `http_post` | `http_created` | 201 |
| `http_put` | `http_redirect` | 302 |
| `http_patch` | `http_bad_request` | 400 |
| `http_delete` | `http_not_authorized` | 401 |
| `http_sse_method` | `http_not_found` | 404 |
| | `http_error` | 500 |

### Template Helpers
In addition to standard Mustache interpolation (`{{field}}`):

| Helper | Description |
|--------|-------------|
| `{{url:resource_name}}` | Resolve resource to URL path. Additional args for URL params: `{{url:todo:id}}` → `/todos/42` |
| `{{asset:filename}}` | Static file path with cache-busting checksum. Served with immutable headers |
| `{{raw:field}}` | Output without HTML escaping (trusted HTML only) |
| `{{input:field}}` | Raw request parameter, HTML-escaped. For repopulating forms on validation failure |
| `{{csrf:cookiename}}` | CSRF token bound to cookie, enforced by middleware |
| `{{csrf_field:cookiename}}` | Hidden form field with CSRF token bound to cookie |
| `{{precision:field:N}}` | Round float string to N decimal places. `{{precision:price:2}}` → `"3.14"` |
| `{{error:name}}` | Boolean section, true when `name` has an error |
| `{{error_code:name}}` | Error code for context variable |
| `{{error_message:name}}` | Error message for context variable |

### Modules
Add a module's initializer function to `.modules` to use its API.

#### SQLite (`sqlite.h`)

| | |
|---|---|
| Initializer | `sqlite` |
| Engine | `sqlite_db` |

#### Postgres (`postgres.h`)

| | |
|---|---|
| Initializer | `postgres` |
| Engine | `postgres_db` |

#### MySQL (`mysql.h`)

| | |
|---|---|
| Initializer | `mysql` |
| Engine | `mysql_db` |

#### Redis / Valkey (`redis.h`)

| | |
|---|---|
| Initializer | `redis` |
| Engine | `redis_db` |

API compatible with both Redis and Valkey.

#### DuckDB (`duckdb.h`)

| | |
|---|---|
| Initializer | `duckdb` |
| Engine | `duckdb_db` |

#### HTMX (`htmx.h`)

| | |
|---|---|
| Initializer | `htmx` |
| Template Partial | `{{> htmx_script}}` |
| Context | `is_htmx` |

#### Datastar (`datastar.h`)

| | |
|---|---|
| Initializer | `datastar` |
| Template Partial | `{{> datastar_script}}` |
| Pipeline Step | `ds_sse`: `.channel` *(pos)*, `.target`, `.mode`, `.elements` (render_config), `.signals`, `.js` |
| Context | `is_ds` |
| Modes | `mode_outer`, `mode_inner`, `mode_replace`, `mode_prepend`, `mode_append`, `mode_before`, `mode_after`, `mode_remove` |

#### Tailwind (`tailwind.h`)

| | |
|---|---|
| Initializer | `tailwind` |
| Template Partial | `{{> tailwind_script}}` |

#### Session Auth (`session_auth.h`)

| | |
|---|---|
| Initializer | `session_auth` |
| Schema | `users` (`user_id`, `short_name`, `full_name`, `password`, `salt`) |
| Pipeline Steps | `session()`, `logged_in()` (includes `session()`), `login()`, `logout()`, `signup()` |
| Context Assets | Default `login` and `signup` templates |
| Context | `user` (user record with `user_id`, `short_name`, `full_name`) |

---

## License

MACH is licensed under the [LGPL](./LICENSE).
