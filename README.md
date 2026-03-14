# MACH

MACH (Modern Asynchronous C Hypermedia) is a web framework that combines the speed of C with a high-level, declarative API.

Declarative config, compiled and hot-reloaded automatically. No garbage collector, no interpreter, no VM, no package managers, no build scripts.

Included modules: datastar, htmx, tailwind, daisyui, mustache, markdown, sqlite, postgres, s3, clerk, stripe, session auth, pdf generation, background tasks, scheduled tasks, fetch, email.

Runs entirely in Docker.

## Why MACH

MACH is a high-level, declarative framework for building web applications in C. Every layer between your code and the response (interpreters, VMs, garbage collectors, ORMs, bundlers) adds latency, memory overhead, and complexity. In MACH the path from handler to response is direct, with no layers you did not explicitly add. The execution is bare metal.

- **No boilerplate, no build scripts** - just write C and your app compiles and reloads automatically.
- **Safe by default** - the runtime handles memory, concurrency, and async I/O, so you never call `malloc` or `free` directly.
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

After the container starts, create main.c with the Hello World example below - it will be automatically compiled and reloaded. You can also attach to the TUI with docker compose attach mach for an integrated editor and console.

Hello World

```c
#include <mach.h>
#include <sqlite.h>   // SQLite module provides sqlite_config

config mach() {
  return (config) {
    .includes = { sqlite_config },

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
              .template = "<html><body>"
                          "<p>Hello {{greeting.name}}</p>"
                          "</body></html>"
            })
          }}
        }
      }
    }
  };
}
```

Core Concepts

Modules

A module encapsulates its own routes, databases, and templates. Modules are composed in main.c via the .includes array.

Example: A simple todos module that provides its own database and routes.

In ./todos/todos.c:
```c
#include <mach.h>
#include <sqlite.h>

config todos_config() {
  return (config){
    .databases = {{
      .name = "todos_db",
      .engine = sqlite,
      .connect = "file:todos.db",
      .migrations = {
        "CREATE TABLE todos (id INTEGER PRIMARY KEY, task TEXT);"
      }
    }},
    .resources = {
      .urls = {
        {"/todos", .get = {{
          db((d){{{
            .set = "todos",
            .db = "todos_db",
            .query = "SELECT * FROM todos;"
          }}}),
          render((r){ .asset = "todos_list" })
        }}}
      }
    }
  };
}
```

In main.c you include it:

```c
#include <mach.h>
#include "todos/todos.c"

config mach() {
  return (config){
    .includes = { todos_config }
  };
}
```

Resolution

· Registrations (Top‑Down): Named registrations (databases, context keys, resources) resolve top‑down. The first definition wins, allowing main.c to override any module default.
· Middleware (Chained): Pipeline steps defined in .before and .after chain top‑down across all scopes.
· Errors (Bottom‑Up): Unhandled .errors resolve bottom‑up, starting at the URL level and bubbling up to the app level.

Example: Override a database connection string from a module.

```c
// todos/todos.c defines "todos_db" with connect = "file:todos.db"

// main.c overrides it
config mach() {
  return (config){
    .includes = { todos_config },
    .databases = {{
      .name = "todos_db",                // same name as in module
      .connect = "file:/custom/path.db"  // this wins
    }}
  };
}
```

Pipelines

A pipeline is a sequence of steps that share a context. Steps execute in order, and asynchronous steps (like database queries) run concurrently where possible.

Example: A pipeline that validates input, inserts a record, and renders a response.

```c
{"/users", .post = {{
  param((p){.name="email", .validation="^[^@]+@[^@]+$"}),
  db((d){{{
    .set = "new_user",
    .db = "app_db",
    .query = "INSERT INTO users (email) VALUES ('{{email}}') RETURNING id"
  }}}),
  render((r){ .template = "<p>Created user {{#new_user}}{{id}}{{/new_user}}</p>" })
}}}
```

Embeds & Context

Use #embed to load external files (templates, SQL) and inject values via .context. Context keys are interpolated in templates using {{key}}.

Example: Embed a template and inject a site title.

```c
.context = {
  {"site_title", "My MACH App"},
  {"home_template", (asset){ #embed "templates/home.html" }}
}

// Later in a pipeline:
render((r){ .asset = "home_template" })
// The template can use {{site_title}}
```

For one‑off templates, embed directly:

```c
render((r){ (asset){ #embed "templates/special.html" } })
```

Configuration

Global Configuration

Defined once at the application or module level.

Includes

Compose your application from built-in or custom modules. All modules listed at the top are built in and available for inclusion.

```c
.includes = { sqlite_config, datastar_config, session_auth_config, todos_config }
```

Databases

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

Resources

Define URLs, MIME types, and HTTP verb pipelines.

```c
.resources = {
  .urls = {
    {"/api/data", mime_json,
      .get = {{
        db((d){{{
          .db = "app_db",
          .query = "select * from data;"
        }}}),
        render((r){ .asset = "data_template" })
      }}
    }
  }
}
```

Publish & Subscribe

Publish: defines the schema of events this module emits (.publish).

Subscribe: event pipelines that run automatically when specific events fire; they are defined in the .events array.

```c
.publish = {
  {"todo_created", .with = {"id", "title"}}
},
.events = {
  {"todo_created", {
    db((d){{{
      .db = "audit_db",
      .query = "insert into logs(msg) values('Created: {{title}}');"
    }}})
  }}
}
```

Scoped Configuration

Middleware (.before & .after)

Chained pipelines for common tasks like authentication or session management that apply to all resources in a scope.

```c
.before = { session(), logged_in() },
.after = { emit((e){"request_completed"}) }
```

Errors

Declarative error handlers matching specific HTTP status codes, resolving bottom-up.

```c
.errors = {
  { http_not_found, { render((r){ .asset = "404_page" }) } }
}
```

Context

Inject assets or static strings into the request context.

```c
.context = {
  {"app_name", "MACH App"},
  {"base_layout", (asset){
    #embed "layout.html"
  }}
}
```

Pipeline API

Declarative Steps

Step Description Example
param Validates context parameter against a regex. param((p){.name="id", .validation="^\\d+$"})
db Executes concurrent queries and injects results. db((d){{{.db="db", .query="select..."}}})
render Renders a template using current context. render((r){ .asset = "home" })
sse Pushes data to a persistent SSE channel. sse((s){.channel="c", .data={"hi"}})
datastar Sends reactive DOM updates (Datastar protocol). Available in datastar module. datastar((ds){.channel="updates", .target="#stats", .elements=...})
call Invokes a custom imperative C function. call((c){my_handler})
emit Publishes internal events to trigger subscribers. emit((e){"user_signup"})
redirect Client-side 302 redirect. redirect((u){"/login"})
reroute Internal server-side reroute to another URL. reroute((u){"/dashboard"})

Datastar Module

The datastar module enables reactive DOM updates using the Datastar protocol. It integrates with your pipelines to send partial HTML updates over SSE channels. To use it, include datastar.h and add datastar_config to your .includes.

A typical Datastar step specifies an SSE channel, a target element (CSS selector), a merge mode, and the new HTML content (either as an inline template or a named asset from .context).

Example: Adding a new todo item and immediately updating the list via Datastar.

```c
#include <datastar.h>
#include <sqlite.h>
#include <session_auth.h>

config myapp() {
  return (config) {
    .includes = { sqlite_config, session_auth_config, datastar_config },

    .databases = {{
      .name = "todos_db",
      .engine = sqlite,
      .connect = "file:todos.db",
      .migrations = {
        "CREATE TABLE todos (id INTEGER PRIMARY KEY, title TEXT);"
      }
    }},

    .resources = {
      .urls = {
        {"/todos",
          .before = { session(), logged_in() },
          .context = {
            {"todo_item", (asset){ #embed "todo.mustache.html" }}
          },
          .post = {{
            param((p){ .name="title", .validation="^\\S{1,16}$" }),
            db((d){{{
              .set = "new_todo",
              .db = "todos_db",
              .query = "INSERT INTO todos (title) VALUES ('{{title}}') RETURNING id, title"
            }}}),
            datastar((ds){
              .channel = "todos/{{user_id}}",
              .mode = mode_prepend,
              .target = "#todo-list",
              .elements = (r){ .asset = "todo_item" }   // render the asset with the new todo's data
            })
          }}
        }
      }
    }
  };
}
```

The todo.mustache.html template might look like:

```html
<div id="todo_{{id}}">
  <span>{{title}}</span>
  <button data-on:click="@delete('/todos/{{id}}')">delete</button>
</div>
```

When the POST request succeeds, the new todo HTML is prepended to the #todo-list container for all clients subscribed to the todos/{{user_id}} SSE channel.

HTMX Module

The htmx module provides integration with the HTMX library. Include htmx.h and add htmx_config to your .includes. This makes HTMX attributes available in your templates and enables server‑side handling of HTMX requests (e.g., partial responses).

Example Pipelines

Here are longer examples showing how multiple steps combine in real-world scenarios.

Form Handler with Validation and Database Insert

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

Using Events

```c
// In module config
.events = {
  {"daily_report", {
    db((d){{{
      .set = "stats",
      .db = "analytics_db",
      .query = "SELECT COUNT(*) as visits FROM page_views WHERE date = CURRENT_DATE"
    }}}),
    emit((e){"report_generated"})
  }}
}
```

Custom Template Tags

Command Description
{{:error:key}} Returns true if the key has a validation error
{{:error_message:key}} Returns the error message string for the key
{{:raw:key}} Outputs unescaped HTML
{{:precision:key:n}} Formats a number to n decimal places

Handler API

When you need custom logic, you drop into the imperative API via call().

Context Management

Function Description Example
get(key) Read value from context auto id = get("id");
set(key, val) Write value to context set("name", "Alice");
has(key) Check if key exists if (has("user")) { ... }
format(fmt) Interpolate context string auto msg = format("Hi {{name}}");

Errors

Function Description Example
error_set(k, e) Set error on context error_set("age", (error){.code=400, .message="Invalid age"});
error_get(k) Get error from context auto err = error_get("age");

Memory Management

Function Description Example
allocate(size) Arena-allocate memory (automatically freed at pipeline end) char* buf = allocate(1024);
defer_free(ptr) Register an externally allocated pointer for automatic cleanup when the pipeline finishes. Use this when calling third‑party libraries that return malloced memory. char* ext = strdup("hello"); defer_free(ext);

Headers & Cookies

Function Description Example
headers_set(k,v) Set response header headers_set("Content-Type", "application/json");
cookies_set(k,v) Set response cookie cookies_set("sid", "xyz123");

Responses

Function Description Example
respond(status, r) Send response with a render step respond(http_ok, (r){ .asset = "home" });
respond_sse(s) Send an SSE message respond_sse((s){.event="chat", .data={"hello"}});
redirect_to(url) HTTP redirect (302) redirect_to("/login");
reroute_to(url) Internal server‑side reroute reroute_to("/dashboard");
emit_event(event) Fire an event (triggers subscribers) emit_event("user_signup");

Tables & Records

Function Description Example
table_new() Create empty table auto t = table_new();
record_get(r, k) Get field value auto email = record_get(row, "email");
table_add(t, r) Add record to table table_add(users, new_user);
table_count(t) Number of records int n = table_count(t);

Tooling

app_info

Deep introspection of application state via the CLI:

```
app_info                    # full topology
app_info resources          # all routes
app_info assets             # all templates
app_info pipelines          # all pipelines
app_info events             # pub/sub map
app_info databases          # all databases and schemas
```

Testing & Debugging

Integrated suites for every layer:

```
unit_tests                  # fast, criterion-based unit tests
e2e_tests                   # playwright-powered browser tests
app_debug                   # launches interactive debugger attached to dev server
```

Production

Build optimized deployment environments. The resulting image contains only your compiled application and the MACH runtime, ready to deploy to any container registry or orchestration platform.

```
app_build                   # creates a slim, optimized production Docker image
```

Observability

First-class telemetry using OpenTelemetry:

· Telemetry Server: Visualizes logs and traces on port 4000 in dev mode.
· Hot Path Tracking: Automatic profiling of pipelines to identify bottlenecks.
· Error Aggregation: Unified reporting across all reactors.

Editor

Built-in terminal-based editor optimized for the MACH workflow. Supports highlighting for C, Mustache, and SQL, and is integrated with the HMR system for instant save-to-reload. Includes full LSP support for intelligent code completion and diagnostics.

AI Assistant

Built-in AI assistant in the TUI trained for MACH/IDEAL. Uses app_info and topology to generate code that understands your specific routes, queries, and events.

How It Works

IDEAL Philosophy

· Interfaces are Deep: Simple interfaces backed by deep functionality.
· Domain Centred: Each module owns exactly one responsibility.
· Encapsulated: Internal mechanics remain private.
· Autonomous: Modules are fully self-contained.
· Local: All related code is co-located.

Data-Oriented Pipelines

config executes once at boot to construct an optimized execution graph. Requests are sequential executions of pre-warmed pipelines.

Multi-Reactor

Each CPU core runs an independent reactor event loop. Async boundaries are handled between steps, scaling across cores without threads or locks in application code.

Memory Management

Uses an arena pool allocator per reactor. Each pipeline gets its own arena, which is freed automatically when the pipeline finishes. Use allocate() for temporary allocations; they live for the duration of the pipeline. For memory obtained from external libraries (e.g., via malloc), register it with defer_free() to have it cleaned up automatically at the end of the request.
