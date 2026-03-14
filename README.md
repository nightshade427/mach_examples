# MACH

**Modern Asynchronous C Hypermedia** - a web framework that combines the speed of C with a high-level, declarative API.

Declarative config, compiled and hot-reloaded automatically. No garbage collector, no interpreter, no VM, no package managers, no build scripts.

Included modules: datastar, htmx, tailwind, daisyui, mustache, markdown, sqlite, postgres, s3, clerk, stripe, session auth, pdf generation, background tasks, scheduled tasks, fetch, email.

Runs entirely in Docker.

## Why MACH

MACH is a high-level, declarative framework for building web applications in C. Every layer between your code and the response (interpreters, VMs, garbage collectors, ORMs, bundlers) adds latency, memory overhead, and complexity. In MACH the path from handler to response is direct, with no layers you did not explicitly add. The execution is bare metal.

- **No boilerplate, no build scripts** - just write C and your app compiles and reloads automatically.
- **Safe by default** - the runtime handles memory, concurrency, and async I/O, so you never call `malloc` or `free`.
- **Bare-metal performance** - your handlers run directly on the request path, with no interpreter or VM overhead.
- **All modules built in** - everything from auth to PDF generation is available out of the box; just include them in your config with `.includes`.

### Comparison

The same Pokemon voting app across five frameworks:

| Framework | Files | Lines of Code |
|---|---|---|
| MACH (C) | 1 | 185 |
| Rails (Ruby) | 82 | 1,012 |
| Phoenix (Elixir) | 48 | 1,542 |
| Next.js RSC (TypeScript) | 23 | 3,257 |
| Go GraphQL SPA (Go/TypeScript) | 32 | 5,534 |

### Quick Start

Get up and running immediately with zero environment configuration.

```bash
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml

# Dev server on :3000, telemetry on :4000
# Includes file watching, auto compilation, hot code reloading, HMR
docker compose up
```

After the container starts, create `main.c` with the Hello World example below - it will be automatically compiled and reloaded. You can also attach to the TUI with `docker compose attach mach` for an integrated editor and console.

### Hello World

```c
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
        "INSERT INTO greetings(name) VALUES('World');"
      }
    }},

    .resources = {
      .urls = {
        {"/",
          .get = {{
            db((d){{{
              .set = "greeting",
              .db = "hello_db",
              .query = "select name from greetings limit 1;"
            }}}),
            render((r){
              "<html><body>"
                "{{#greeting}}<p>Hello {{name}}</p>{{/greeting}}"
              "</body></html>"
            })
          }}
        }
      }
    }

};}
```

## Core Concepts

### Modules

MACH encourages building self-contained systems (the IDEAL philosophy). A module encapsulates its own routes, databases, HTML, and SQL. Modules communicate via decoupled pub/sub event contracts.

A typical multi-module directory structure:

```
.
├── main.c              # App entry point & module composition
├── activity/           # Activity module
│   ├── activity.c      # Routes and config
│   ├── activity.mustache.html
│   ├── create_activity_table.sql
│   └── get_activities.sql
├── todos/              # Todos module
│   ├── todos.c         # Routes and config
│   ├── todos.mustache.html
│   ├── todo.mustache.html
│   ├── create_todos_table.sql
│   └── get_todos.sql
├── public/             # Static public assets
│   └── favicon.png
└── static/             # Global templates and errors
    ├── layout.mustache.html
    ├── home.mustache.html
    └── 404.mustache.html
```

### Locality of Behavior (LoB)

MACH emphasizes Locality of Behavior. It prefers templates, queries, errors, and pipelines to be defined as local as possible to where they are used. The global context is reserved for cross-cutting configurations that must be universally available.

### Safe C

You do not write async/await, spawn threads, or call malloc and free. The MACH runtime handles memory management, concurrency, and async logic for you.

### Config & Scopes

Everything in MACH is declared in config structs. Definitions can be scoped to the application, module, resource, or specific URL level.

### Resolution

**Registrations (Top-Down):** Named registrations (context, resources, databases) resolve top-down. `main.c` overrides included modules. First registration wins.

**Middleware (Chained):** Pipeline steps defined in `.before` and `.after` chain top-down across all scopes.

**Errors (Bottom-Up):** Unhandled `.errors` resolve bottom-up, starting at the URL level and bubbling up to the app level.

### Pipelines

MACH is data-oriented. It runs sequentially through an ordered pipeline of handlers that all share the same pipeline context. Pipeline steps are executed asynchronously when needed (e.g. during database or network I/O).

### Embeds & Context

Using C23's `#embed`, you can keep templates and queries in separate files with full syntax highlighting. The `.context` field injects manual key-value pairs that support Mustache interpolation using the `{{key}}` syntax.

## Configuration

### Global Configuration

Defined once at the application or module level.

### Includes

Compose your application from built-in or custom modules. All modules listed at the top are built in and available for inclusion.

```c
.includes = { sqlite_config, datastar_config, session_auth_config, todos_config }
```

### Databases

Declare data and schemas. MACH follows the IDEAL philosophy, each module owns its own schema and migrations. Supports multi-tenant databases via context interpolation.

```c
// Database defined within the Activity module
.databases = {
  {.name = "activity_db", .engine = sqlite, .connect = "file:activity.db?mode=rwc"}
}

// Database defined within the Todos module; scoped per user
.databases = {
  {.name = "todo_db", .engine = sqlite, .connect = "file:{{user_id}}_todos.db?mode=rwc"}
}
```

### Resources

Define URLs, MIME types, and HTTP verb pipelines.

```c
.resources = {
  .urls = {
    {"/api/data", mime_json,
      .get = {{
        db((d){{{ .db = "app_db", .query = "select * from data;" }}}),
        render((r){ .asset = "data_template" })
      }}
    }
  }
}
```

### Publish & Subscribe

**Publish**: defines the schema of events this module emits.

**Subscribe**: the pipelines that run automatically when specific events fire.

```c
.publish = {
  {"todo_created", .with = {"id", "title"}}
},
.subscribe = {
  {"todo_created", {{
    db((d){{{
      .db = "audit_db",
      .query = "insert into logs(msg) values('Created: {{title}}');"
    }}})
  }}}
}
```

### Scoped Configuration

#### Middleware (`.before` & `.after`)

Chained pipelines for common tasks like authentication or session management that apply to all resources in a scope.

```c
.before = {{ session(), logged_in() }},
.after = {{ emit((e){"request_completed"}) }}
```

#### Errors

Declarative error handlers matching specific HTTP status codes, resolving bottom-up.

```c
.errors = { {http_not_found, {{ render((r){.asset = "404_page"}) }}} }
```

#### Context

Inject assets or static strings into the request context.

```c
.context = {
  {"app_name", "MACH App"},
  {"base_layout", (asset){
    #embed "layout.html"
  }}
}
```

## Pipeline API

### Declarative Steps

| Step | Description | Example |
|---|---|---|
| param | Validates context parameter against a regex. | `param((p){.name="id", .validation="^\\d+$"})` |
| db | Executes concurrent queries and injects results. The double braces allow an array of queries to run concurrently in one step. | `db((d){{{.db="db", .find="select..."}}})` |
| render | Renders a template using current context. | `render((r){ (asset){ #embed "h.html" } })` |
| sse | Pushes data to a persistent SSE channel. | `sse((s){.channel="c", .data={"hi"}})` |
| datastar | Sends reactive DOM updates (Datastar protocol). | `datastar((ds){.target="t", .elements=...})` |
| call | Invokes a custom imperative C function. | `call((c){my_handler})` |
| emit | Publishes internal events to trigger subscribers. | `emit((e){"user_signup"})` |
| redirect | Client-side 302 redirect. | `redirect((u){"/login"})` |
| reroute | Internal server-side reroute to another URL. | `reroute((u){"/dashboard"})` |

### Example Pipelines

Here are longer examples showing how multiple steps combine in real-world scenarios.

#### Form Handler with Validation and Database Insert

```c
{"/signup", .post = {{
  param((p){.name="email", .validation="^[^@]+@[^@]+$"}),
  param((p){.name="password", .validation=".{8,}"}),
  db((d){{{
    .set = "user",
    .db = "app_db",
    .query = "INSERT INTO users (email, password) VALUES ('{{email}}', '{{password}}') RETURNING id"
  }}}),
  render((r){ .asset = "welcome.mustache.html" }),
  emit((e){"user_signed_up"})
}}}
```

#### Using Events

```c
// In module config
.subscribe = {
  {"daily_report", {{
    db((d){{{
      .set = "stats",
      .db = "analytics_db",
      .query = "SELECT COUNT(*) as visits FROM page_views WHERE date = CURRENT_DATE"
    }}}),
    emit((e){"report_generated"})
  }}}
}
```

### Custom Template Tags

| Command | Description |
|---|---|
| `{{:error:key}}` | Returns true if the key has a validation error |
| `{{:error_message:key}}` | Returns the error message string for the key |
| `{{:raw:key}}` | Outputs unescaped HTML |
| `{{:precision:key:n}}` | Formats a number to n decimal places |

## Handler API

When you need custom logic, you drop into the imperative API via `call()`.

### Context Management

| Function | Description | Example |
|---|---|---|
| `get(key)` | Read value from context | `auto id = get("id");` |
| `set(key, val)` | Write value to context | `set("name", "Alice");` |
| `has(key)` | Check if key exists | `if (has("user")) { ... }` |
| `format(fmt)` | Interpolate context string | `auto msg = format("Hi {{name}}");` |

### Errors

| Function | Description | Example |
|---|---|---|
| `error_set(k, e)` | Set error on context | `error_set("age", (error){.code=400});` |
| `error_get(k)` | Get error from context | `auto err = error_get("age");` |

### Memory Management

| Function | Description | Example |
|---|---|---|
| `allocate(size)` | Arena-allocate memory | `char* buf = allocate(1024);` |
| `defer_free(ptr)` | Register for cleanup | `defer_free(malloc(512));` |

### Headers & Cookies

| Function | Description | Example |
|---|---|---|
| `headers_set(k,v)` | Set response header | `headers_set("Content-Type", "application/json");` |
| `cookies_set(k,v)` | Set response cookie | `cookies_set("sid", "xyz123");` |

### Responses

| Function | Description | Example |
|---|---|---|
| `respond(s, b)` | Send response struct | `respond(http_ok, (body){.asset="home"});` |
| `redirect_to(u)` | HTTP redirect | `redirect_to("/login");` |

### Tables & Records

| Function | Description | Example |
|---|---|---|
| `table_new()` | Create empty table | `auto t = table_new();` |
| `record_get(r, k)` | Get field value | `auto email = record_get(row, "email");` |

## Tooling

### app_info

Deep introspection of application state via the CLI:

```
app_info                    # full topology
app_info resources          # all routes
app_info assets             # all templates
app_info pipelines          # all pipelines
app_info events             # pub/sub map
app_info databases          # all databases and schemas
```

### Testing & Debugging

Integrated suites for every layer:

```
unit_tests                  # fast, criterion-based unit tests
e2e_tests                   # playwright-powered browser tests
app_debug                   # launches interactive debugger attached to dev server
```

### Observability

First-class telemetry using OpenTelemetry:

- **Telemetry Server:** Visualizes logs and traces on port 4000 in dev mode.
- **Hot Path Tracking:** Automatic profiling of pipelines to identify bottlenecks.
- **Error Aggregation:** Unified reporting across all reactors.

### Editor

Built-in terminal-based editor optimized for the MACH workflow. Supports highlighting for C, Mustache, and SQL, and is integrated with the HMR system for instant save-to-reload. Includes full LSP support for intelligent code completion and diagnostics.

### Production

Build optimized deployment environments. The resulting image contains only your compiled application and the MACH runtime, ready to deploy to any container registry or orchestration platform.

```
app_build                   # creates a slim, optimized production Docker image
```

### AI Assistant

Built-in AI assistant in the TUI trained for MACH/IDEAL. Uses `app_info` and topology to generate code that understands your specific routes, queries, and events.

## How It Works

### IDEAL Philosophy

- **Interfaces are Deep:** Simple interfaces backed by deep functionality.
- **Domain Centred:** Each module owns exactly one responsibility.
- **Encapsulated:** Internal mechanics remain private.
- **Autonomous:** Modules are fully self-contained.
- **Local:** All related code is co-located.

### Data-Oriented Pipelines

`config` executes once at boot to construct an optimized execution graph. Requests are sequential executions of pre-warmed pipelines.

### Multi-Reactor

Each CPU core runs an independent reactor event loop. Async boundaries are handled between steps, scaling across cores without threads or locks in application code.

### Memory Management

Uses an arena pool allocator per reactor. Each pipeline gets its own arena, which is freed automatically when the pipeline finishes.
