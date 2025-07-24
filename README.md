# MACH

## Why MACH

MACH (Modern Asynchronous C Hypermedia) is a platform for high-level, asynchronous, reactive, declarative web development in C.

* **Zero Boilerplate:** No build scripts, package managers, ORMs, or other bloat. It compiles and hot-reloads automatically.
* **Safe by Default:** Automatically handles memory, concurrency, and async I/O. Never call `malloc` or `free`. No threads, mutexes, or locks. The runtime handles this for you. All database queries use prepared statements. Tasks and events are durable by default, with automatic recovery on restart. All code automatically emits OpenTelemetry spans, logs, and errors.
* **Batteries Included:** Built-in SSE support and modules for Datastar, HTMX, Tailwind, SQLite, Postgres, MySQL, Redis/Valkey, DuckDB, and auth. Native multi-tenant database support.

---

## Table of Contents
* [Quick Start](#quick-start)
* [Philosophy](#philosophy)
* [How It Works](#how-it-works)
* [Tooling](#tooling)
* [Building with MACH](#building-with-mach)
* [Reference](#reference)
* [License](#license)

---

## Quick Start

Everything runs in Docker, no other local dependencies needed.

```bash
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml

# Dev server on :3000, telemetry on :4000
# Includes file watching, auto compilation, hot code reloading, HMR
docker compose up
```

Attach to the TUI with `docker compose attach mach`

The containerized environment includes a TUI editor with AI, LSP, and console.
You can also use your own editor. MACH watches the project directory.

Create `main.c` with the example below to see hot-reloading work.

### At A Glance

```c
#include <mach.h>
#include <sqlite.h>

// returns the app configuration, runs once at boot
config mach(){
  return (config) {
    // {{}} defines an array
    .resources = {{
      {"home", "/",
        // pipeline: query the db, then render the result
        .get = {{
          // single query uses double braces (struct > query)
          query((d){
            .set = "greeting",
            .db = "hello_db",
              "select name "
              "from greetings "
              "limit 1;"
          }),
          // mustache template using "greeting" from context
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
```

---

## Philosophy

Applications are data transformations: input from sources, business logic transforms data, output data to the client. MACH keeps each piece standard. Data comes from raw SQL, HTTP fetch, and JSON instead of ORMs. Business logic is plain C functions. Output is standard HTML, CSS, JS via Mustache templates. These pieces compose inside pipelines: ordered lists of steps that transform a request into a response.

Tooling is standard: lldb for debugging, Playwright and Criterion for testing, OpenTelemetry for observability. All built in, nothing to configure.

### IDEAL Philosophy
MACH's design rejects object-oriented complexity in favor of data-oriented composition. Where [SOLID](https://youtu.be/zHiWqnTWsn4) guides OOP abstraction, IDEAL guides data and behavior composition, drawing from [A Philosophy of Software Design](https://youtu.be/lz451zUlF-k) (deep interfaces, complexity reduction), [CUPID](https://youtu.be/cyZDLjLuQ9g) (domain-centered, composable properties), and [Data-Oriented Design](https://youtu.be/rX0ItVEVjHc) (data transforms over object hierarchies).

* **Interfaces are Deep:** Minimize API surface, maximize capability per call. `query()` handles SQL execution, prepared statements, concurrency, and error triggering in one step.
* **Domain Centered:** Each module owns exactly one domain. A `todos` module defines everything related to todos and nothing else.
* **Encapsulated:** Hide implementation, expose data. Pipeline context is always readable and inspectable; module internals are not.
* **Autonomous:** Modules are fully self-contained. Each carries its own schemas, migrations, seeds, and event contracts. Modules cannot call each other directly (enforced by compiler). They interact only through the hypermedia layer or events (pub/sub messaging).
* **Local:** Co-locate all related code. A module's SQL, templates, and handlers live together.

### Everything is a String
The web is text: HTTP, HTML, JSON, SQL. MACH leans into this. The pipeline context stores and passes all data as safe, arena-backed strings, organized into scopes: `input:` for raw request parameters, `error:` for validation/error data, and app scope (no prefix) for validated and application data. If your application needs to parse a value into a specific C type, use an `invoke()` step to isolate that conversion in your business logic.

---

## How It Works
* [Data-Oriented Pipelines](#data-oriented-pipelines)
* [Multi-Reactor Architecture](#multi-reactor-architecture)
* [Memory Management](#memory-management)
* [Safe by Default](#safe-by-default)
* [String Interpolation](#string-interpolation)

### Data-Oriented Pipelines
The `mach()` function runs once at boot. Its returned `config` is processed into an optimized execution graph with precompiled pipelines, queries, and templates. Each incoming request then executes its matching pipeline as a sequence of pre-warmed steps.

![Boot-Time Compilation](./10-boot-time-compilation.svg)

![Request Pipeline Flow](./02-request-pipeline-flow.svg)

### Multi-Reactor Architecture
MACH runs two types of reactors. Request reactors handle HTTP traffic: one per CPU core, each with its own event loop. Async boundaries are handled between pipeline steps, scaling across cores without threads or locks in application code. Each pipeline step handles its own async calls internally and guarantees all async work is complete before the next step begins. A single worker reactor handles `task()`, `task_async()`, and `.cron` schedules. It has its own event loop backed by a thread pool, so tasks can be CPU-bound and use blocking I/O without stalling request handling.

![Multi-Reactor Architecture](./01-multi-reactor-architecture.svg)

### Memory Management
Each reactor maintains a pool of arena allocators. When a request arrives, the pipeline is assigned an arena from the pool. All allocations during that pipeline draw from the arena. When the pipeline completes, the arena is instantly cleared and returned to the pool. No per-object cleanup, no garbage collection, no manual memory management.

### Safe by Default

MACH prevents common C and web vulnerabilities at the framework level.

#### Memory Safety
Application code never calls `malloc` or `free`. Each request pipeline gets a dedicated arena allocator, instantly reclaimed on completion. No leaks, double-frees, or use-after-free. All framework data structures (tables, records, strings) enforce bounds checking; out-of-bounds reads and missing context values return `nullptr` instead of faulting. Pipelines that exceed their memory limit (default 5MB, configurable via `compose.yml`) safely abort with a 500 error, preventing OOM denial-of-service.

#### SQL Injection Prevention
Interpolations like `{{user_id}}` inside `query()` or `find()` are natively bound as parameterized inputs to prepared statements. SQL injection is structurally impossible through standard pipeline execution.

#### XSS Prevention
The `render()` step auto-escapes all context values in Mustache templates. Malicious input is rendered as text. Raw HTML requires an explicit opt-in via the `{{raw:field}}` helper.

#### Thread Safety
Application code never manages threads, mutexes, or locks. The multi-reactor architecture isolates request state to the pipeline's context. Concurrent database queries execute via the event loop. No race conditions in business logic.

### String Interpolation
All strings in MACH are interpolatable. Anywhere you see a `string` field (SQL queries, `.connect`, `.channel`, `.url`, `.data`, `format()`) you can use `{{context_key}}` to reference values from the pipeline context. Scoped values are accessed with their prefix: `{{input:field}}` for raw request parameters, `{{error:field}}`, `{{error_code:field}}`, `{{error_message:field}}` for errors. App-scope values use no prefix: `{{field}}`. In `query()` and `find()` steps, interpolated values are bound as prepared statement parameters, not string-concatenated.

---

## Tooling

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
Built on lldb with pipeline-aware commands. Halt on individual pipeline steps, step through execution, and inspect the full pipeline context including nested tables and records.

### Deployment
```bash
app_build                   # outputs slim, optimized production Docker image
```
MACH deploys as a standard Docker container. It does not terminate TLS; production deployments should place MACH behind a reverse proxy or load balancer (e.g., Nginx, Caddy, AWS ALB) to handle HTTPS.

### Observability
Every pipeline step automatically emits OpenTelemetry spans. Logs, traces, errors, and auto-profiling are visualized on the telemetry server at port 4000. No manual instrumentation required.

### Development Environment
Built-in TUI editor with HMR, LSP support, and a topology-aware AI assistant.

---

## Building with MACH
* [Notation](#notation)
* [Modules & Composition](#modules--composition)
* [External Dependencies](#external-dependencies)
* [Context](#context)
* [Databases](#databases)
* [Static Files](#static-files)
* [Middleware Pipelines](#middleware-pipelines)
* [Resource Pipelines](#resource-pipelines)
* [Event Pipelines](#event-pipelines)
* [Error and Repair Pipelines](#error-and-repair-pipelines)
* [Task Pipelines](#task-pipelines)
* [Pipeline Steps](#pipeline-steps)

### Notation
Each pipeline step takes a compound literal of its step config type, passed as a positional argument.

MACH's declarative syntax uses C's designated initializer braces at different depths:

* **Single `{}`**: a single value or struct.
* **Double `{{}}`**: an array of items: pipelines, URLs, context entries, concurrent queries in `queries()`, etc.
* **`(asset){ #embed "file" }`**: the `asset` type bakes a file directly into the compiled binary using standard C23 `#embed`. Used for templates, SQL files, and other static content.

```c
// {{}} wraps an array of pipeline steps
.get = {{
  // single query: just a struct literal
  query((d){
    .set = "todos",
    .db = "todos_db",
    "select * from todos;"
  }),
  // multiple queries run concurrently via queries
  queries((da){{
    {
      .set = "projects",
      .db = "projects_db",
      "select * from projects;"
    },
    {
      .set = "tags",
      .db = "todos_db",
      "select * from tags;"
    }
  }}),
  // {} wraps a single value
  render((r){(asset){
    #embed "todos.mustache.html"
  }})
}}
```

### Modules & Composition

Every MACH app and module returns a `config` struct. The root `main.c` includes modules via `#include` and registers them in `.modules`. Configurations merge top-down: anything named resolves to the first registration, so root configs can override module defaults.

![App Composition Tree](./05-app-composition-tree.svg)

Each module is a directory containing a single `.c` config file alongside the SQL and template files it references via `#embed`:

```
├── activity/                        # activity module
│   ├── activity.c                   # config function
│   ├── activity.mustache.html       # templates
│   ├── create_activity_table.sql    # migrations
│   ├── get_activities.sql           # queries
│   └── insert_activity.sql
├── todos/                           # todos module
│   ├── todos.c
│   ├── todos.mustache.html
│   ├── todo.mustache.html
│   ├── create_todos_table.sql
│   ├── create_todo.sql
│   ├── get_todos.sql
│   ├── update_todo.sql
│   └── delete_todo.sql
├── static/                          # root-level templates (not a module)
│   ├── layout.mustache.html
│   ├── home.mustache.html
│   ├── 404.mustache.html
│   └── 5xx.mustache.html
├── public/                          # served directly
│   └── favicon.png
└── main.c                           # app entry point
```

The root config defines shared concerns (home page, layout, global middleware, error pages) and composes everything via `.modules`:

```c
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config) {
    .resources = {{
      {"home", "/", .get = {{ /* ... */ }} }},
      .before = {session()},
      .context = {{"layout", (asset){ #embed "static/layout.mustache.html" }}},
      .errors = { /* 404, 5xx handlers */ }
    },
    .modules = {todos, activity, session_auth}
  };
}
```

Each module owns its resources, database, and migrations. Modules cannot call each other directly; they communicate through pub/sub events.

### External Dependencies
MACH enforces a containerized development environment. You write standard C23 using MACH APIs, no local toolchain required. For third-party C libraries:

1. **The `/vendor` Directory:** Drop headers and libraries (`.so`, `.a`) into `/vendor` in your project root. The auto-compiler will discover, include, and link them.
2. **Custom Dockerfile:** For system-level dependencies (e.g., `libsodium`), create a `Dockerfile` that inherits from the MACH base image, run `apt-get install`, and update your `compose.yml` to build from it.

#### Memory Management with External Libraries
Third-party memory must be bridged to the arena to maintain safety:

* **`allocate(bytes)`**: Provides a buffer from the pipeline's arena, reclaimed automatically on request completion.
* **`defer_free(ptr)`**: Schedules cleanup for pointers allocated by external libraries (e.g., via `malloc`). Executed when the arena is released.

### Context
The `.context` block injects variables and assets into the pipeline context. Values like site names or HTML layouts become available to templates without passing them on every request. Use `#embed` to bake files into the binary via the `asset` type. Sensitive data and API keys are handled via standard Docker secrets. Secrets exposed to the container are automatically loaded and available for string interpolation in the pipeline context.

#### Context Scopes
The pipeline context uses scoped namespaces to separate concerns:

* **`input:xxx`**: Raw request parameters (query string, form body, URL parameters). All incoming request data lands in the `input` scope. Access in templates with `{{input:title}}`, `{{input:email}}`, etc.
* **`error:xxx`**: Validation and error data. Access in templates with `{{error:name}}`, `{{error_code:name}}`, `{{error_message:name}}`.
* **`xxx`** (app scope): Application data — query results, context variables, and validated inputs. No prefix.

The `input` and `error` scopes mirror each other: raw input arrives in `input:xxx`, and if validation fails, the error lands in `error:xxx`. The `validate()` step bridges input to app scope: when validation passes, `.name` promotes the value from `input:name` to `name` in app scope. This means downstream steps like `query()`, `render()`, and `invoke()` always work with validated app-scope values, never raw input.

![Context Scoping](./07-context-scoping.svg)

```c
config mach() {
  return (config) {
    .context = {
      {"base_layout", (asset){
        #embed "layout.mustache.html"
      }}
    }
  };
}
```

### Databases
Each module owns its schema and migrations. When `.migrations` or `.seeds` are provided, MACH tracks them in a `mach_meta` table. Migrations are forward-only and index-based: they run in array order, each applied once, with new migrations appended to the end. Seeds are idempotent and safe to re-run.

All engines follow the same configuration pattern. Set `.engine` to the appropriate constant: `sqlite_db`, `postgres_db`, `mysql_db`, `redis_db` (Redis/Valkey), or `duckdb_db`.

Multi-tenant databases use interpolation in the `.connect` string. The interpolated value can be a user, organization, or any other scope. Connections are pooled with LRU eviction. Active tenants stay warm, idle connections get reclaimed.

![Database Multi-Tenancy](./09-database-multi-tenancy.svg)

```c
config todos() {
  return (config) {
    .databases = {{
      .engine = sqlite_db,
      .name = "todos_db",
      .connect = "file:{{tenant_id}}_todo.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_todos_table.sql"
      }},
      .seeds = {(asset){
        #embed "seed_todos_table.sql"
      }}
    }}
  };
}
```

### Static Files
Create a `public` directory in your project root. Files placed there are served directly. Use it for images, fonts, pre-built CSS/JS, and other assets that don't need compilation into the binary.

Reference static files in templates with the `{{asset:filename}}` helper. It resolves to the file's URL path with a content-based checksum appended for cache-busting. The response is served with immutable, cache-forever headers, so browsers cache aggressively but always fetch the new version when the file changes.

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

### Middleware Pipelines
Middleware pipelines (`.before` and `.after`) execute at the boundaries of the request lifecycle, chaining top-down across all scopes. Define global middleware at the root config, or scope it to specific resources or events.

![Middleware Scoping](./06-middleware-scoping.svg)

```c
#include <mach.h>
#include <session_auth.h>

config mach() {
  return (config) {
    .modules = { session_auth },

    // Execute before every resource pipeline
    .before = { session() },

    // Execute after every resource pipeline
    .after = { invoke((i){log_request}) }
  };
}
```

### Resource Pipelines
MACH is resource-based, not route-based. Each entry in `.resources` defines a named resource at a URL with its supported HTTP verbs (`.get`, `.post`, `.delete`, `.sse`, etc.) and localized middleware. The first positional field is the resource `.name`, used by `redirect()`, `reroute()`, and `{{url:name}}` to reference the resource without hard-coding paths.

Clients select the verb through the request method, or by passing `http_method` as a query parameter, hidden form field, or any other mechanism that places it in context. For example, `GET /todos` hits `.get`, `POST /todos` hits `.post`, and `/todos?http_method=sse` connects to the SSE stream.

The router handles path specificity automatically. Exact matches (e.g., `/todos/active`) are always prioritized before parameterized matches (e.g., `/todos/:id`), regardless of definition order. File uploads are available in context like any other request parameter.

```c
config todos() {
  return (config) {
    .resources = {{
      { "todos", "/todos",
        .post = {{
          validate((v){
            .name = "title",
            .validation = "^\\S{1,16}$",
            .message = "1-16 chars, no spaces"
          }),
          query((d){
            .db = "todos_db",
            (asset){
              #embed "create_todo.sql"
            }
          }),
          emit((e){"todo_created"}),
          redirect((u){"todos"})
        }},

        .before = { logged_in() }
      }}
    }
  };
}
```

### Event Pipelines

Internal pub/sub for cross-module communication. The publisher doesn't know who listens; the subscriber doesn't know who emits. Adding a new subscriber is a new module with an `.events` entry. No changes to the publisher.

Events are durable by default. When `.publishes` is defined anywhere in the config, MACH automatically creates a `mach_events` database to track event delivery. Every emitted event is recorded and its delivery to each subscriber is tracked. If the process crashes, undelivered events are automatically replayed on the next boot. Event lifecycle events emit OpenTelemetry spans alongside request pipeline telemetry.

A module declares outbound events in `.publishes`. Each entry accepts `.event` (positional) and `.with` (context keys to pass to subscribers). Subscribers define `.events` pipelines. Each entry accepts `.event` (positional) and `.steps` (positional). The `emit()` step triggers the event from a pipeline.

![Event Pub/Sub](./03-event-pub-sub.svg)

```c
// todos.c: publishes "todo_created"
config todos(){
  return (config) {
    .name = "todos",
    .publishes = {
      {"todo_created", .with = {"user_id", "title"}}
    },
    // ...
  };
}

// activity.c: subscribes to "todo_created"
config activity(){
  return (config) {
    .name = "activity",
    .events = {
      {"todo_created", {
        query((d){ .db = "activity_db", "insert into activity ..." })
      }}
    },
    // ...
  };
}
```

### Error and Repair Pipelines

When a pipeline step fails, execution halts. Resolved bottom-up: verb, resource, module, root, stopping at the first match. Unhandled errors surface in the TUI console and telemetry.

`.errors` are terminal. The matching error pipeline executes, sends the response to the client, and ends the request. Each entry accepts `.error_code` (positional) and `.steps` (positional).

`.repairs` are resumable. The repair pipeline fixes the context, then resumes the original pipeline at the step following the failure. Same entry format as `.errors`. If no matching repair is found, resolution falls through to `.errors`.

![Error Resolution](./04-error-resolution.svg)

```c
config mach(){
  return (config) {
    .errors = {
      {http_not_found, {
        render((r){ (asset){ #embed "404.html" } })
      }}
    },

    .repairs = {
      {http_not_authorized, {
        invoke((i){refresh_session_token})
      }}
    }
  };
}
```

### Task Pipelines

Tasks are named pipelines that run on the worker reactor instead of request reactors. CPU-bound computation, blocking I/O, heavy work that shouldn't occupy a request reactor. Resolved bottom-up: verb, resource, module, root.

Tasks are durable by default. When `.tasks` is defined anywhere in the config, MACH automatically creates a `mach_tasks` database to track task state. Every task invocation is recorded with its status, context, and progress. If the process crashes, incomplete tasks are automatically recovered and resumed on the next boot. Task lifecycle events emit OpenTelemetry spans alongside request pipeline telemetry.

Each entry accepts `.name` (positional), `.accepts` (context keys to pull from the caller's context into the task's context), `.cron` (schedule interval), and `.steps` (positional). Tasks with `.cron` run on the worker reactor at the specified interval. Any task can also be triggered from pipelines via the `task()` and `task_async()` steps.

```c
config todos(){
  return (config) {
    .name = "todos",
    .tasks = {
      {"recount_todos", .accepts = {"user_id"}, {{
        query((d){ .db = "todos_db", "update users set todo_count = ..." })
      }}},

      {"daily_digest", .cron = "0 8 * * *", {{
        query((d){ .db = "todos_db", "insert into digest_reports ..." })
      }}}
    }
  };
}
```

### Pipeline Steps

Steps are the units of work in a pipeline. Each step receives the current request context, acts on it, and passes control to the next step.

* [validate](#validate)
* [validators](#validators)
* [find & query](#find--query)
* [queries](#queries)
* [join](#join)
* [fetch](#fetch)
* [invoke](#invoke)
* [emit](#emit)
* [task & task_async](#task--task_async)
* [sse](#sse)
* [ds_sse](#ds_sse)
* [render](#render)
* [headers & cookies](#headers--cookies)
* [redirect & reroute](#redirect--reroute)
* [nest](#nest)
* [if_context](#if_context)

#### validate

`validate()` validates a request parameter (query string, form body, or URL parameter) from the `input` scope. On success, the value is promoted from `input:name` to `name` in app scope, making it available to downstream steps like `query()`, `render()`, and `invoke()`. On failure, an error is stored in `error:name` and a `400 Bad Request` is triggered, resolved through the nearest error/repair pipeline.

Validate a required parameter with a regex pattern:

```c
.post = {{
  validate((v){
    .name = "title",
    .validation = "^\\S{1,16}$",
    .message = "must be 1-16 characters, no spaces"
  }),
  // input:title validated and promoted to app scope as "title"
  // pipeline continues with {{title}} available
}}
```

Mark a parameter as optional with `.optional` so it won't trigger an error when absent:

```c
.get = {{
  validate((v){
    .name = "filter",
    .optional = true,
    .validation = "^(active|done)$",
    .message = "must be 'active' or 'done'"
  }),
  // filter may or may not be in context
}}
```

Use `.fallback` to inject a default value when absent. The parameter is always in context after validation:

```c
.get = {{
  validate((v){
    .name = "page",
    .fallback = "1",
    .validation = "^\\d{1,4}$",
    .message = "must be a number"
  }),
  // page is always in context: either the request value or "1"
}}
```

URL parameters like `:id` in `/todos/:id` are also validated with `validate()`:

```c
{"todo", "/todos/:id",
  .before = {
    validate((v){
      .name = "id",
      .validation = "^\\d{1,10}$",
      .message = "must be a number"
    })
  }
}
```

When validation fails, the error is stored in context under `error:name`, accessible in templates via `{{error:name}}`, `{{error_code:name}}`, and `{{error_message:name}}`. The raw input value remains available in `input:name` for re-rendering the form. Validation failure halts the pipeline and triggers the error pipeline. Share a template via `.context` to re-render the form with errors:

```c
{"todos", "/todos",
  .get = {{
    render((r){.asset = "form"})
  }},
  .post = {{
    validate((v){
      .name = "email",
      .validation = validate_email,
      .message = "must be a valid email"
    }),
    validate((v){
      .name = "title",
      .validation = validate_not_empty,
      .message = "cannot be empty"
    }),
    query((d){
      .db = "todos_db",
      (asset){ #embed "create_todo.sql" }
    }),
    redirect((u){"todos"})
  }},

  .errors = {
    {http_bad_request, {
      render((r){.asset = "form"})
    }}
  },

  .context = {{
    {"form", (asset){
      #embed "todos_form.mustache.html"
    }}
  }}
}
```

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

#### validators

MACH ships common validation patterns as `#define` macros. These are regex patterns, usable anywhere `.validation` is accepted. Define your own the same way.

```c
// Strings
#define validate_not_empty   "^\\S+.*$"
#define validate_alpha       "^[a-zA-Z]+$"
#define validate_alphanumeric "^[a-zA-Z0-9]+$"
#define validate_slug        "^[a-z0-9]+(-[a-z0-9]+)*$"
#define validate_no_html     "^[^<>]*$"

// Numbers
#define validate_integer     "^-?[0-9]+$"
#define validate_positive    "^[1-9][0-9]*$"
#define validate_float       "^-?[0-9]+(\\.[0-9]+)?$"
#define validate_percentage  "^(100|[1-9]?[0-9])$"

// Identity
#define validate_email       "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
#define validate_uuid        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
#define validate_username    "^[a-zA-Z][a-zA-Z0-9_]{2,31}$"

// Dates & Times
#define validate_date        "^[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])$"
#define validate_time        "^([01][0-9]|2[0-3]):[0-5][0-9]$"
#define validate_datetime    "^[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])T([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9])?$"

// Web
#define validate_url         "^https?://[a-zA-Z0-9.-]+(:[0-9]+)?(/[^\\s]*)?$"
#define validate_ipv4        "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
#define validate_hex_color   "^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$"

// Codes & IDs
#define validate_zipcode_us  "^[0-9]{5}(-[0-9]{4})?$"
#define validate_phone_e164  "^\\+[1-9][0-9]{6,14}$"
#define validate_cron        "^([0-9*,/-]+\\s){4}[0-9*,/-]+$"

// Security
#define validate_no_sqli     "^[^;'\"\\\\]*$"
#define validate_token       "^[a-zA-Z0-9_-]{16,128}$"
#define validate_base64      "^[A-Za-z0-9+/]+=*$"

// Boolean-ish (form checkboxes, toggles)
#define validate_boolean     "^(true|false|0|1)$"
#define validate_yes_no      "^(yes|no)$"
#define validate_on_off      "^(on|off)$"
```

Use them directly in `validate()`:

```c
validate((v){
  .name = "email",
  .validation = validate_email,
  .message = "must be a valid email"
}),
validate((v){
  .name = "age",
  .validation = validate_positive,
  .message = "must be a positive number"
})
```

#### find & query

`find()` and `query()` each execute a single database query. Both specify `.db` for the database name, with the SQL string as a positional field. Use `.set` to store results in context. All queries use prepared statements. Interpolations like `{{user_id}}` are bound as parameters, not concatenated.

The difference: `find()` triggers a `404 Not Found` if no rows are returned; `query()` does not.

Use `.asset` to reference a SQL query stored in `.context` by name, instead of inlining or embedding.

Fetch a collection:

```c
.get = {{
  query((d){
    .set = "todos",
    .db = "todos_db",
    (asset){
      #embed "get_todos.sql"
    }
  }),
  render((r){(asset){
    #embed "todos.mustache.html"
  }})
}}
```

Fetch a single record, auto-triggering 404 if missing:

```c
.get = {{
  find((d){
    .set = "todo",
    .db = "todos_db",
    (asset){
      #embed "get_todo.sql"
    }
  }),
  render((r){(asset){
    #embed "todo.mustache.html"
  }})
}}
```

Execute a write without storing results:

```c
.delete = {{
  query((d){
    .db = "todos_db",
    (asset){
      #embed "delete_todo.sql"
    }
  }),
  redirect((u){"todos"})
}}
```

#### queries

`queries()` executes multiple database queries concurrently. It takes a `da` config wrapping an array of `d` query configs. Each query runs independently on the same reactor. All results are available in context after the step completes. If one query fails, the remaining queries still complete and their results remain in context, so error/repair handlers have data available for compensating logic if they resume the pipeline.

```c
.get = {{
  queries((da){{
    {
      .set = "todos",
      .db = "todos_db",
      "select * from todos where user_id = {{user_id}};"
    },
    {
      .set = "count",
      .db = "todos_db",
      "select count(*) as total from todos where user_id = {{user_id}};"
    }
  }}),
  render((r){(asset){
    #embed "todos.mustache.html"
  }})
}}
```

For transactions, use standard SQL transaction statements directly in your queries (`BEGIN`, `COMMIT`, `ROLLBACK`).

#### join

`join()` nests records from one context table into each matching record of another, like a SQL JOIN but in-memory across context tables. Useful when data comes from separate databases or queries and needs to be combined for rendering.

```c
.get = {{
  queries((da){{
    {
      .set = "projects",
      .db = "projects_db",
      "select id, name from projects where user_id = {{user_id}};"
    },
    {
      .set = "todos",
      .db = "todos_db",
      "select id, title, project_id from todos where user_id = {{user_id}};"
    }
  }}),
  join((j){
    .target_table = "projects",
    .target_field = "id",
    .nested_table = "todos",
    .nested_field = "project_id",
    .target_join_field = "todos"
  }),
  render((r){(asset){
    #embed "projects.mustache.html"
  }})
}}
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
.get = {{
  fetch((f){
    .set = "weather",
    .url = "https://api.weather.dev/forecast?city={{city}}",
  }),
  render((r){.asset = "forecast"})
}}
```

POST a JSON body built from context tables:

```c
.post = {{
  queries((da){{
    {
      .set = "order",
      .db = "orders_db",
      "select id, total from orders where id = {{order_id}};"
    },
    {
      .set = "items",
      .db = "orders_db",
      "select name, qty, price from order_items where order_id = {{order_id}};"
    }
  }}),
  join((j){
    .target_table = "order",
    .target_field = "id",
    .nested_table = "items",
    .nested_field = "order_id",
    .target_join_field = "items"
  }),
  fetch((f){
    .set = "receipt",
    .url = "https://api.payments.dev/charge",
    .method = http_post,
    .headers = {
      {"Authorization", "Bearer {{api_key}}"}
    },
    .json = "order"
  }),
  render((r){.asset = "receipt"})
}}
```

#### invoke

`invoke()` calls a C function with access to the full pipeline context via the Imperative API. It runs synchronously on the reactor. Long-running or blocking work will block the reactor.

```c
.get = {{
  query((d){
    .set = "challengers",
    .db = "pokemon_db",
      "select id, name, sprite "
      "from pokemons "
      "order by random() "
      "limit 2;"
  }),
  invoke((i){assign_opponents}),
  render((r){.asset = "home"})
}}
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

To trigger an error/repair pipeline from inside an `invoke()`, use `error_set()`:

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

`emit()` triggers an internal pub/sub event. Other modules subscribe in their `.events` array and react independently, with no direct dependency on the emitter.

```c
.post = {{
  validate((v){
    .name = "title",
    .validation = "^\\S{1,16}$",
    .message = "1-16 characters"
  }),
  query((d){
    .set = "todo",
    .db = "todos_db",
    (asset){
      #embed "create_todo.sql"
    }
  }),
  emit((e){"todo_created"}),
  redirect((u){"todos"})
}}
```

#### task & task_async

`task()` sends a named task to the worker reactor and yields the request reactor. The request reactor is free to handle other requests while the task runs. When the task completes, the pipeline resumes on the request reactor.

```c
.post = {{
  validate((v){
    .name = "title",
    .validation = "^\\S{1,16}$",
    .message = "1-16 characters"
  }),
  query((d){
    .db = "todos_db",
    (asset){ #embed "create_todo.sql" }
  }),
  task((t){"recount_todos"}),
  redirect((u){"todos"})
}}
```

`task_async()` sends a named task to the worker reactor and continues the request pipeline. Fire-and-forget.

```c
.post = {{
  query((d){
    .db = "todos_db",
    (asset){ #embed "create_todo.sql" }
  }),
  task_async((t){"recount_todos"}),
  redirect((u){"todos"})
}}
```

#### sse

`sse()` pushes a Server-Sent Event. With `.channel`, the event is broadcast to all connected clients on that channel. Without `.channel`, the event is returned directly to the requesting client.

![SSE / Datastar Flow](./08-sse-datastar-flow.svg)

Declare an SSE channel on a resource with `.sse`. This creates a persistent streaming endpoint. Clients connect using the resource URL with the SSE verb: `new EventSource("/todos?http_method=sse")`. The `.sse` block supports `.channel`, `.steps` (positional), `.before`, `.after`, `.context`, and `.errors`. The optional `.steps` pipeline runs when a client first connects.

```c
{"todos", "/todos",
  .sse = {
    .channel = "todos/{{user_id}}",
    {{
      // steps run on connect, e.g. send initial state
      query((d){
        .set = "todos",
        .db = "todos_db",
        "select * from todos where user_id = {{user_id}};"
      }),
      sse((s){
        .event = "initial",
        .data = {"{{todos}}"}
      })
    }}
  }
}
```

Push an event from another pipeline:

```c
.post = {{
  // ... validate and query steps ...
  sse((s){
    .channel = "todos/{{user_id}}",
    .event = "new_todo",
    .data = {"{{todo}}"}
  })
}}
```

The `.event` and `.data` fields map directly to the SSE protocol's `event:` and `data:` lines. `.data` is an array for multi-line SSE data. `.comment` maps to the SSE `:` comment line.

#### ds_sse

The `datastar` module provides `ds_sse`, which combines SSE with DOM updates by pushing Datastar-formatted events targeting specific elements.

```c
.post = {{
  // ... validate and query steps ...
  ds_sse((ds){
    .target = "todos",
    .mode = mode_prepend,
    .channel = "todos/{{user_id}}",
    .elements = {.asset = "todo"}
  })
}}
```

`.target` specifies the DOM element ID. `.mode` controls fragment insertion: `mode_prepend`, `mode_append`, `mode_replace`, `mode_remove`, and others. `.elements` accepts a full `r` (render) config. Like `sse()`, without `.channel` the event goes directly to the requesting client.

Use `.signals` to update client-side reactive state without touching the DOM:

```c
.post = {{
  // ... validate and query steps ...
  ds_sse((ds){
    .channel = "todos/{{user_id}}",
    .signals = "{\"count\": {{count}}}"
  })
}}
```

#### render

`render()` outputs a Mustache template using the current pipeline context.

A static page using a layout from `.context`:

```c
.resources = {{
  {"about", "/about",
    .get = {{
      render((r){(asset){
        #embed "about.mustache.html"
      }})
    }}
  }},

  .context = {
    {"layout", (asset){
      #embed "layout.mustache.html"
    }}
  }
}
```

Where `about.mustache.html` inherits from the layout:

```html
{{< layout}}
  {{$body}}
    <p>about us</p>
  {{/body}}
{{/layout}}
```

Query and render:

```c
{"todos", "/todos",
  .get = {{
    query((d){
      .set = "todos",
      .db = "todos_db",
      "select id, title, finished from todos where user_id = {{user_id}};"
    }),
    render((r){
      "{{< layout}}"
        "{{$body}}"
          "{{#todos}}"
              "<div>"
                "<span>{{title}}</span>"
                "{{#finished}}<em>done</em>{{/finished}}"
              "</div>"
          "{{/todos}}"
          "{{^todos}}"
            "<p>no todos yet</p>"
          "{{/todos}}"
        "{{/body}}"
      "{{/layout}}"
    })
  }}
}
```

Use `.asset` to reference a template by name in `.context`:

```c
render((r){.asset = "home"})
```

Use `.json` to serialize a context value as a JSON response. The content type is automatically set to `application/json`. Context tables with nested tables produce structured JSON payloads:

```c
.get = {{
  queries((da){{
    {
      .set = "order",
      .db = "orders_db",
      "select id, total from orders where id = {{order_id}};"
    },
    {
      .set = "items",
      .db = "orders_db",
      "select name, qty, price from order_items where order_id = {{order_id}};"
    }
  }}),
  join((j){
    .target_table = "order",
    .target_field = "id",
    .nested_table = "items",
    .nested_field = "order_id",
    .target_join_field = "items"
  }),
  render((r){
    .json = "order"
  })
}}
```

#### Template Engines

MACH includes two template engines: Mustache (default) and MDM (Markdown with Mustache). Use `.engine` to select a different engine. Additional engines can be added as modules.

Render a list with the default Mustache engine:

```c
render((r){
  "<h1>Hello {{name}}</h1>"
  "<p>You have <strong>{{count}}</strong> todos remaining.</p>"
  "<ul>"
    "{{#todos}}"
      "<li>{{title}}</li>"
    "{{/todos}}"
  "</ul>"
})
```

Render the same content as Markdown with Mustache interpolation:

```c
render((r){
  .engine = "mdm",
  "# Hello {{name}}\n"
  "\n"
  "You have **{{count}}** todos remaining.\n"
  "\n"
  "{{#todos}}\n"
  "- {{title}}\n"
  "{{/todos}}"
})
```

#### headers & cookies

`headers()` and `cookies()` set HTTP response headers and cookies declaratively. Both use the `p` config type with an array of name/value pairs. Values support string interpolation.

```c
.get = {{
  query((d){
    .set = "user",
    .db = "users_db",
    "select id, name from users where id = {{user_id}};"
  }),
  headers((p){{
    {"X-Request-Id", "{{request_id}}"},
    {"Cache-Control", "no-store"}
  }}),
  cookies((p){{
    {"session", "{{session_id}}"}
  }}),
  render((r){.asset = "profile"})
}}
```

#### redirect & reroute

`redirect()` sends a `302` to the client, causing the browser to navigate to a new URL. `reroute()` re-enters the router server-side, executing the target resource's pipeline within the same request. Both accept a resource name, not a URL path.

Redirect after a form submission (POST-redirect-GET):

```c
.post = {{
  validate((v){
    .name = "title",
    .validation = "^\\S{1,16}$",
    .message = "1-16 characters"
  }),
  query((d){
    .db = "todos_db",
    (asset){
      #embed "create_todo.sql"
    }
  }),
  redirect((u){"todos"})
}}
```

Reroute to serve another resource's pipeline without a round trip:

```c
{"home", "/",
  .get = {{
    reroute((u){"todos"})
  }}
}
```

#### nest

`nest()` groups multiple pipeline steps into a single composite step. Use it to organize pipeline logic or to apply a single `.if_context` guard to an entire block of steps at once.

Compose a reusable sequence:

```c
.post = {{
  validate((v){
    .name = "title",
    .validation = validate_not_empty,
    .message = "cannot be empty"
  }),
  query((d){
    .db = "todos_db",
    (asset){ #embed "create_todo.sql" }
  }),
  nest((n){{
    sse((s){
      .channel = "todos/{{user_id}}",
      .event = "new_todo",
      .data = {"{{todo}}"}
    }),
    emit((e){"todo_created"})
  }}),
  redirect((u){"todos"})
}}
```

Conditionally execute a group with `.if_context`:

```c
.post = {{
  find((d){
    .set = "todo",
    .db = "todos_db",
    "select * from todos where id = {{id}};"
  }),
  invoke((i){classify_todo}),
  nest((n){
    .if_context = "is_urgent",
    {{
      query((d){
        .db = "todos_db",
        "update todos set priority = 'high' where id = {{id}};"
      }),
      emit((e){"urgent_todo"}),
      render((r){.asset = "urgent_confirmation"})
    }}
  }),
  render((r){
    .if_context = "is_normal",
    .asset = "standard_confirmation"
  })
}}
```

#### if_context

Any step supports `.if_context`, which names a context variable. If present, the step executes. If absent, the step is silently skipped.

This works for any context value: `.if_context = "title"` runs only if a `title` form parameter was submitted; `.if_context = "todos"` runs only if a prior `query()` stored results under that name.

```c
.post = {{
  find((d){
    .set = "todo",
    .db = "todos_db",
    "select * from todos where id = {{id}};"
  }),
  invoke((i){classify_todo}),
  query((d){
    .if_context = "is_urgent",
    .db = "todos_db",
    "update todos set priority = 'high' where id = {{id}};"
  }),
  emit((e){
    .event = "urgent_todo",
    .if_context = "is_urgent"
  }),
  render((r){
    .if_context = "is_urgent",
    .asset = "urgent_confirmation"
  }),
  render((r){
    .if_context = "is_normal",
    .asset = "standard_confirmation"
  })
}}

void classify_todo() {
  auto const todos = get("todo");
  auto const todo = table_get(todos, 0);
  auto const due = record_get(todo, "due_date");

  if (is_within_24h(due))
    set("is_urgent", "1");
  else
    set("is_normal", "1");
}
```

When multiple steps share the same `.if_context` condition, use `nest()` to apply the condition once to the entire group instead of repeating it on each step.

---

## Reference
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
Fields marked **(positional)** are the first unnamed fields in the struct initializer. They are set by position, not with `.name =` syntax.

* **.name**: Application or module identifier (positional).
* **.modules**: Modules to compose.
* **.databases**: Data stores. Accepts `.name`, `.engine`, `.connect`, `.migrations`, and `.seeds`.
* **.publishes**: Outbound event contracts. Accepts `.event` (positional) and `.with`.
* **.events**: Collection of Event Pipelines.
* **.resources**: Collection of Resource Pipelines.
* **.before** / **.after**: Global middleware pipelines.
* **.context**: Global variables and `#embed` assets. Each entry accepts `.name` (positional) and `.value` (positional).
* **.errors**: Collection of global Error Pipelines (bottom-up fallback).
* **.repairs**: Collection of global Repair Pipelines (bottom-up fallback).
* **.tasks**: Collection of global Task Pipelines (bottom-up scoping).

### Resource Pipelines (`.resources`)
* **.resources** (positional): Collection of resource pipelines. Each entry accepts `.name` (positional), `.url` (positional), `.mime` (sets the default response content type for the resource; defaults to `mime_html`, can be overridden per-response by `.mime` in a `render()` step), `.get`, `.post`, `.put`, `.patch`, `.delete`, `.before`, `.after`, `.context`, `.tasks`, `.errors`, `.repairs`, and `.sse` (which accepts `.channel`, `.steps` (positional), `.before`, `.after`, `.context`, `.tasks`, `.errors`, and `.repairs`). Each verb block (`.get`, `.post`, `.put`, `.patch`, `.delete`) also accepts `.before`, `.after`, `.context`, `.tasks`, `.errors`, and `.repairs` for verb-scoped middleware and error handling.
* **.before** / **.after**: Shared middleware for all pipelines in the block.
* **.context**: Shared context for all pipelines in the block.
* **.tasks**: Collection of scoped Task Pipelines for all pipelines in the block.
* **.errors**: Collection of shared Error Pipelines for all pipelines in the block.
* **.repairs**: Collection of shared Repair Pipelines for all pipelines in the block.

### Event Pipelines (`.events`)
Reacts to internal pub/sub events triggered by `emit()`. Each entry accepts `.event` (positional), `.steps` (positional), `.before`, `.after`, `.context`, `.tasks`, `.errors`, and `.repairs`.
* **Database**: `mach_events` — created automatically when `.publishes` is defined.

### Task Pipelines (`.tasks`)
Named pipelines that run on the worker reactor. Invoked by `task()` (yield and resume) or `task_async()` (fire-and-forget), or run on a schedule via `.cron`. Resolved bottom-up: verb, resource, module, root. Each entry accepts `.name` (positional), `.cron`, `.accepts`, `.steps` (positional), `.before`, `.after`, and `.context`.
* **Database**: `mach_tasks` — created automatically when `.tasks` is defined.

### Error Pipelines (`.errors`)
Terminal error handlers. Catches HTTP errors or thrown exceptions, executes the pipeline, sends the response to the client, and ends the request. Each entry accepts `.error_code` (positional), `.steps` (positional), `.before`, `.after`, and `.context`. Errors resolve bottom-up: from the verb block, to the resource, to the module, to root, stopping at the first match.

### Repair Pipelines (`.repairs`)
Resumable error handlers. Catches the error, executes steps to fix the pipeline context, then automatically resumes the original pipeline at the step following the failure. Each entry accepts `.error_code` (positional), `.steps` (positional), `.before`, `.after`, and `.context`. Repairs resolve bottom-up using the same scoping rules as `.errors`. If no matching repair is found, resolution falls through to `.errors`.

### Pipeline API
All pipeline steps accept a single positional field unless additional named fields are listed.

* **validate(v)**: Validates request parameters from the `input` scope. On success, promotes the value from `input:name` to app scope (`name`). On failure, the error is stored in `error:name`, accessible in templates via `{{error:name}}`, `{{error_code:name}}`, and `{{error_message:name}}`. Accepts `.name`, `.optional`, `.fallback`, `.validation`, `.message`, and `.if_context`.
* **find(d)**: Executes a single DB query; triggers `404 Not Found` if no rows are returned. Accepts `.db`, `.set`, `.asset`, `.query` (positional, SQL string), and `.if_context`.
* **query(d)**: Executes a single DB query. Accepts `.db`, `.set`, `.asset`, `.query` (positional, SQL string), and `.if_context`.
* **queries(da)**: Executes concurrent DB queries. Wraps an array of `d` query configs. Accepts `.queries` (positional) and `.if_context`.
* **join(j)**: Nests records from one context table into another by matching fields. Accepts `.target_table`, `.target_field`, `.nested_table`, `.nested_field`, `.target_join_field`, and `.if_context`.
* **fetch(f)**: Makes an HTTP request and stores the response in context. JSON responses are parsed into tables and records (with nested tables for nested JSON); text responses are stored as a plain string. Accepts `.set`, `.url`, `.method`, `.headers`, `.text` (plain text body), `.json` (name of a context table to serialize as JSON), and `.if_context`.
* **render(r)**: Outputs templates or JSON responses. Accepts `.template` (positional). Optionally accepts `.status`, `.mime`, `.engine`, `.asset`, `.json`, and `.if_context`.
* **sse(s)**: Pushes SSE data. Accepts `.channel`, `.event`, `.comment`, `.data` (array of strings), and `.if_context`.
* **invoke(i)**: Invokes a custom imperative C function. Accepts `.call` (positional) and `.if_context`.
* **emit(e)**: Triggers an internal pub/sub event. Accepts `.event` (positional) and `.if_context`.
* **task(t)**: Sends a named task to the worker reactor, yields the request reactor, resumes on completion. Accepts `.name` (positional) and `.if_context`.
* **task_async(t)**: Sends a named task to the worker reactor, continues the request pipeline (fire-and-forget). Accepts `.name` (positional) and `.if_context`.
* **headers(p)**: Sets HTTP response headers. Accepts an array of name/value pairs (positional) and `.if_context`.
* **cookies(p)**: Sets HTTP response cookies. Accepts an array of name/value pairs (positional) and `.if_context`.
* **reroute(u)**: Re-enters the router server-side, executing the target resource's pipeline. Accepts `.resource` (positional) and `.if_context`.
* **redirect(u)**: Performs a client-side 302 redirect. Accepts `.resource` (positional) and `.if_context`.
* **nest(n)**: Groups multiple pipeline steps into a single composite step. Accepts `.steps` (positional, array of pipeline steps) and `.if_context`.

### Validators API
Regex macros for use with `validate()`. Define your own the same way: `#define validate_zipcode "^\\d{5}$"`.

* **`validate_not_empty`**: `"^\\S+.*$"`
* **`validate_alpha`**: `"^[a-zA-Z]+$"`
* **`validate_alphanumeric`**: `"^[a-zA-Z0-9]+$"`
* **`validate_slug`**: `"^[a-z0-9]+(-[a-z0-9]+)*$"`
* **`validate_no_html`**: `"^[^<>]*$"`
* **`validate_integer`**: `"^-?[0-9]+$"`
* **`validate_positive`**: `"^[1-9][0-9]*$"`
* **`validate_float`**: `"^-?[0-9]+(\\.[0-9]+)?$"`
* **`validate_percentage`**: `"^(100|[1-9]?[0-9])$"`
* **`validate_email`**: `"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"`
* **`validate_uuid`**: `"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"`
* **`validate_username`**: `"^[a-zA-Z][a-zA-Z0-9_]{2,31}$"`
* **`validate_date`**: `"^[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])$"`
* **`validate_time`**: `"^([01][0-9]|2[0-3]):[0-5][0-9]$"`
* **`validate_datetime`**: `"^[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])T([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9])?$"`
* **`validate_url`**: `"^https?://[a-zA-Z0-9.-]+(:[0-9]+)?(/[^\\s]*)?$"`
* **`validate_ipv4`**: `"^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"`
* **`validate_hex_color`**: `"^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$"`
* **`validate_zipcode_us`**: `"^[0-9]{5}(-[0-9]{4})?$"`
* **`validate_phone_e164`**: `"^\\+[1-9][0-9]{6,14}$"`
* **`validate_cron`**: `"^([0-9*,/-]+\\s){4}[0-9*,/-]+$"`
* **`validate_no_sqli`**: `"^[^;'\"\\\\]*$"`
* **`validate_token`**: `"^[a-zA-Z0-9_-]{16,128}$"`
* **`validate_base64`**: `"^[A-Za-z0-9+/]+=*$"`
* **`validate_boolean`**: `"^(true|false|0|1)$"`
* **`validate_yes_no`**: `"^(yes|no)$"`
* **`validate_on_off`**: `"^(on|off)$"`

### Imperative API
Available inside `invoke()` for custom logic. All types are defined in `mach.h`.

**Context**
* **`void* get(string const name)`**: Retrieves a value from the pipeline context.
* **`void set(string const name, void const *value)`**: Sets a value in the pipeline context.
* **`bool has(string const name)`**: Returns whether a value exists in the pipeline context.

**Memory & Strings**
* **`void* allocate(int const bytes)`**: Allocates memory from the pipeline's arena.
* **`void defer_free(void const *ptr)`**: Schedules a pointer for cleanup when the arena is released. Use for memory allocated by third-party libraries outside the arena.
* **`string format(string const format_string)`**: Interpolates `{{context_key}}` placeholders against the current pipeline context.

**Errors**
* **`void error_set(string const name, error const)`**: Triggers an error/repair pipeline. The `error` type has `.code` and `.message`.
* **`error error_get(string const name)`**: Retrieves a previously set error by name.
* **`bool error_has(string const name)`**: Returns whether an error exists for the given context variable name.

Errors set via `error_set()` and `validate()` share the `error` scope. Both are accessible in templates via `{{error:name}}`, `{{error_code:name}}`, and `{{error_message:name}}`.

**Data (Tables)**
* **`table table_new()`**: Creates a new empty table.
* **`int table_count(table const)`**: Returns the number of records in a table.
* **`record table_get(table const, int const index)`**: Returns the record at the given index.
* **`void table_add(table const, record const)`**: Appends a record to a table.
* **`void table_remove(table const, record const)`**: Removes a specific record from a table.
* **`void table_remove_at(table const, int const index)`**: Removes the record at the given index.
* **`void table_join(j const)`**: Joins two context tables in-memory. Accepts `.target_table`, `.target_field`, `.nested_table`, `.nested_field`, and `.target_join_field`.

**Data (Records)**
* **`record record_new()`**: Creates a new empty record.
* **`void record_set(record const, string const name, string const value)`**: Sets a field on a record.
* **`string record_get(record const, string const name)`**: Returns the value of a field on a record.
* **`void record_remove(record const, string const name)`**: Removes a field from a record.

### Constants & Enums

**MIME Types**
* **`mime_html`**: `"text/html"`
* **`mime_txt`**: `"text/plain"`
* **`mime_sse`**: `"text/event-stream"`
* **`mime_json`**: `"application/json"`
* **`mime_js`**: `"application/javascript"`

**HTTP Methods**
* **`http_get`**
* **`http_post`**
* **`http_put`**
* **`http_patch`**
* **`http_delete`**
* **`http_sse_method`**

**HTTP Statuses**
* **`http_ok`**: 200
* **`http_created`**: 201
* **`http_redirect`**: 302
* **`http_bad_request`**: 400
* **`http_not_authorized`**: 401
* **`http_not_found`**: 404
* **`http_error`**: 500

### Template Helpers
In addition to standard Mustache interpolation (`{{field}}`), MACH templates support built-in helpers:

* **`{{url:resource_name}}`**: resolves a named resource to its URL path. Pass URL parameters as additional colon-separated arguments: `{{url:todo:id}}` resolves to the URL for the `todo` resource, substituting the current value of `id` into the URL pattern (e.g. `/todos/42`).
* **`{{asset:filename}}`**: resolves to the path for a file in the `public` directory with a content-based checksum appended for cache-busting. The response is served with immutable, cache-forever headers. Use in `src`, `href`, or anywhere a static asset URL is needed: `{{asset:favicon.png}}`.
* **`{{raw:field}}`**: outputs the value of `field` without HTML escaping. Use when the context value contains trusted HTML that should be rendered as-is.
* **`{{input:field}}`**: outputs the raw request parameter value from the `input` scope. Use in form fields to repopulate submitted values on validation failure (e.g., `value="{{input:email}}"`). Input values are HTML-escaped like standard interpolation.
* **`{{csrf:cookiename}}`**: output the cookiefield and bind it to csrf middleware via cookie eg. session_id. This is then enforced by middleware.
* **`{{csrf_field:cookiename}}`**: outputs hidden form field that is bound to the given cookie field and csrf middleware via cookie, eg. session_id. This is then enforced by middleware.
* **`{{precision:field:N}}`**: rounds a float-type string to `N` decimal places. `{{precision:price:2}}` renders `"3.14159"` as `"3.14"`.
* **`{{#error:name}}...{{/error:name}}`**: boolean section, true when the context variable `name` has an error. Use to conditionally render error markup. Works with errors set by `validate()` (keyed by `.name`) or by `error_set()` in `invoke()`.
* **`{{error_code:name}}`**: outputs the error code for context variable `name`.
* **`{{error_message:name}}`**: outputs the error message for context variable `name`.

### Modules

Add a module's initializer function to `.modules` to use its API.

#### SQLite (`sqlite.h`)
* **Initializer**: `sqlite()`.
* **Engine Constant**: `sqlite_db`.

#### Postgres (`postgres.h`)
* **Initializer**: `postgres()`.
* **Engine Constant**: `postgres_db`.

#### MySQL (`mysql.h`)
* **Initializer**: `mysql()`.
* **Engine Constant**: `mysql_db`.

#### Redis / Valkey (`redis.h`)
* **Initializer**: `redis()`.
* **Engine Constant**: `redis_db`. Works with both Redis and Valkey (API compatible).

#### DuckDB (`duckdb.h`)
* **Initializer**: `duckdb()`.
* **Engine Constant**: `duckdb_db`.

#### HTMX (`htmx.h`)
* **Initializer**: `htmx()`.
* **Template Partial**: `{{> htmx_script}}`.
* **Imperative API**: `is_htmx()` returns `bool`.

#### Datastar (`datastar.h`)
* **Initializer**: `datastar()`.
* **Template Partial**: `{{> datastar_script}}`.
* **Pipeline Step**: `ds_sse(ds)`. Accepts `.channel`, `.mode`, `.target`, `.elements` (an `r` render config), `.signals`, `.js`, and `.if_context`.
* **Imperative API**: `is_ds()` returns `bool`.
* **Modes**: `mode_outer`, `mode_inner`, `mode_replace`, `mode_prepend`, `mode_append`, `mode_before`, `mode_after`, `mode_remove`.

#### Tailwind (`tailwind.h`)
* **Initializer**: `tailwind()`.
* **Template Partial**: `{{> tailwind_script}}`.

#### Session Auth (`session_auth.h`)
* **Initializer**: `session_auth()`.
* **Database**: Exposes `users` schema (`user_id`, `short_name`, `full_name`, `password`, `salt`).
* **Pipeline Steps**: `session()`, `logged_in()`, `login()`, `logout()`, `signup()`.
* **Context Assets**: Injects default `login` and `signup` templates.

---

## License

MACH is licensed under the [LGPL](./LICENSE).
