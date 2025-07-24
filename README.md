# MACH Web Platform

## Overview
MACH (Modern - Asynchronous - C - Hypermedia) is a batteries included, fast, no ceremony, anti-complexity, web platform designed to combine the raw performance of C with the developer experience of modern frameworks. No need to worry about dependencies, packages, compilations, or builds, just launch the tui and write code and MACH handles the rest.

## Hello World

**Text Response**
```c
#include <mach.h>

config mach(){
  return (config) {
    .routes = {
      {http_get, "/", .text = "hello"}
    }
  };
}
```

**Dynamic Text Response**
```c
#include <mach.h>

void hello(){
  auto name = get("name");
  if(!name) name = "World";
  auto const msg = sfmt("Hello %s", name);
  res_async(mime_txt, http_ok, msg);
}

config mach(){
  return (config) {
    .routes = {
      {http_get, "/", .call = hello}
    }
  };
}
```

**Html Response With SQLite**
```c
#include <mach.h>
#include <mustache.h>
#include <sqlite.h>

void hello(){
  auto const q =
    "select name "
    "from greetings "
    "limit ?;";
  auto const limit = "1";
  db_query_async_await("greeting", "hello_db", q, limit);
  render_res_async(http_ok, "hello_world");
}

void create_greetings_table(){
  auto const q =
    "CREATE TABLE IF NOT EXISTS greetings ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "name TEXT NOT NULL "
    "); ";
  migrate(q);
}

void seed_greetings(){
  auto const q = 
    "INSERT INTO greetings(name) "
    "VALUES('World');";
  seed(q);
}

config mach(){
  return (config) {
    .routes = {
      {http_get, "/", .call = hello}
    },
    .includes = {
      mustache_config, 
      sqlite_config
    },
    .databases = {
      {"hello_db", .engine = "sqlite", "file:hello.db?mode=rwc",
        .migrations = {create_greetings_table},
        .seeds = {seed_greetings}
      }
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"hello_world", (string_embed){
          #embed "hello_world.mustache.html"
        }}
      }
    }}
  };
}
```

`hello_world.mustache.html`:
```html
<html>
  <body>
    {{#greeting}}
      <p>Hello {{name}}</p>
    {{/greeting}}
  </body>
</html>
```

## Key Features
* **Performance:** Native C speed, zero garbage collection overhead, and fully asynchronous execution.
* **Modern C:** Safe, simple C with automatic memory management, bounds checking, and more.
* **Docker First:** Zero host machine setup, dev/prod parity, databases, caches, and more.
* **Built-in Tooling:** AI assistance, HMR, APM, log aggregation, error reporting, debugging, and more.
* **Backend Capabilities:** Multiple simultaneous databases, database per tenant, background tasks, authentication, pdf generation, and more.
* **Ecosystem:** Included modules for datastar, htmx, tailwind, daisyui, mustache, markdown, sqlite, postgres, s3, clerk, stripe, and more.

## Quick Start
```bash
# Create development environment
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml

# Starts development server (default port 3000)
# Starts log/telemetry/apm server (default port 4000)
# Edit code using your own editor
# mach dev server does: file watching, auto compilation, hot code reloading, hmr
docker compose up

# Launches mach terminal ui
# Terminal ui includes: editor, ai, console
docker compose attach mach

# use mach console to run commands
# or from outside mach ui use:
#   docker compose exec mach {command}

# get application information
app_info
app_info routes
app_info middleware
app_info events
app_info databases
app_info module {module_name}

# run debugger attached to running dev server process
app_debug

# Build optimized production docker image
app_build

# Run criterion unit tests
unit_tests

# Run playwright e2e tests
e2e_tests
```

## Architecture Overview

### D.E.C.A.L Philosophy
* **Deep Interfaces:** Modules maximize internal complexity while minimizing surface area (e.g., complex Auth logic exposed as a single middleware).
* **Encapsulation:** Internal mechanics remain private. Modules do not leak state and communicate only through the hypermedia layer or events. This is enforced by MACH.
* **Cohesive:** Every module centers on a singular, clear domain (e.g., Billing or Tasks), keeping logic focused.
* **Autonomous:** Modules are fully self-contained units including their own routes, handlers, databases, business logic, events, ui templates, etc.
* **Locality:** All related code is co-located in a single file or folder to eliminate "file explosion" and codebase hunting.

### Why C?
Speed, simplicity, safety, ergonomics, and a great developer experience. Some of these reasons might sound counter intuitive, but you don't have to choose between performance and developer experience, you can have both.

### Data Pipeline
The platform models applications as a data transformation pipeline similar to game development. Inspired by data oriented design and self contained systems.

**Game**
> Input → World → Update World → Render

**MACH**
> Request → Context → Update Context → Render → Response

* **Input:** Creates a data context that acts as the single source of truth for the request.
* **Processing:** Update the context by executing asynchronous queries, making api calls, triggering events, running business logic, etc.
* **Output:** Renders the context into html, json, or sse.

### Multi-Reactor & Coroutines
Mach runs on a multi reactor system where each cpu core operates an independent event loop. The reactors communicate via lock free messaging for sse and other events.

Application logic runs in coroutine handlers. The reactor manages all i/o (network, db, files), while the handler executes application logic. Handlers yield control during async operations, allowing the reactor to process queued tasks and other requests concurrently.

This provides a fully multi-threaded and asynchronous runtime, yet allows you to ignore all that and write code that appears single-threaded and sequential.

### Memory Management
Mach utilizes a high-performance Arena Pool allocator per reactor, which is pre-allocated at startup and fully managed by the system.

Each arena is split into two distinct regions:

1. The Stack: reserved for the coroutine, storing local variables and execution state for the handler function.
2. The Context: reserved for the context, storing all request data, including database results, fetched content, rendered templates, and queued async tasks.

### Async Patterns
Concurrency is managed through a task queue system with two primary patterns:

1. `_async` (Queue Only): methods ending in `_async` (e.g., `db_query_async`, `render_res_async`) add a task to the queue without executing it. Returns instantly, allowing multiple tasks to be batched. Calling `async_await` runs all queued tasks concurrently. Returning from a handler automatically executes all queued tasks.

2. `_async_await` (Queue & Wait): methods ending in `_async_await` (e.g., `db_query_async_await`) queue a task and immediately wait for it to finish.

## Guide

### Configuration

Everything in MACH is declared in a `config` struct. Routes, middleware, databases, events, templates, and module dependencies are all registered in one place.

```c
config mach(){
  return (config) {
    .includes = {auth_config, billing_config},
    .uses = {"session"},
    .databases = { ... },
    .middleware = { ... },
    .routes = { ... },
    .publishes = { ... },
    .subscribes = { ... },
    .rendering = { ... }
  };
}
```

This serves three purposes simultaneously.

It wires up the application. MACH reads the config at startup to register everything, run migrations, establish connection pools, and begin serving requests.

It is self-documenting. A developer working on a module can see everything that module owns in one place, its routes, middleware, databases, event contracts, and dependencies, without hunting through files or reading framework internals. `app_info module {name}` gives an instant view into any single module.

`app_info` aggregates all modules into a complete application view. Developers can use this to understand the full topology of the running application across all modules:

```bash
app_info routes
app_info middleware
app_info events
app_info databases
app_info module {module_name}
```

The built-in AI uses these same tools to gain context before writing code, generating queries, or debugging. Because the entire application is declared and introspectable, the AI understands routes, event contracts, database schemas, and module boundaries without any additional documentation.

### Registration & Resolution
Templates, databases, middleware, and routes are all registered by name. MACH resolves them top-down, meaning registrations in `main.c` take priority over those in included modules, which take priority over their own dependencies.

```
main.c → todos.c → session_auth.c
```

Modules ship with sensible defaults that work out of the box, but anything can be overridden by registering the same name higher up the chain. No forking, no subclassing, no patching. The first registration wins.

```c
// override session_auth's default login template
{"login", (string_embed){
  #embed "my_login.mustache.html"
}}

// point session_auth's SQLite user database at Postgres
{"users", .engine = "postgres", "postgresql://localhost/myapp"}

// replace a middleware implementation
{"logged_in", .before = my_custom_logged_in}
```

### SCS Modules
Modules are self-contained systems. Each module owns its routes, handlers, databases, migrations, seeds, middleware, and templates, all co-located in a single file or folder. Modules communicate only through the hypermedia layer or events.

Writing a module is identical to writing an application, a `config` function is all that's required to expose it.

```
.
├── public
│   └── favicon.png
├── static
│   ├── layout.mustache.html
│   ├── home.mustache.html
│   ├── about.mustache.html
│   ├── 404.mustache.html
│   └── 5xx.mustache.html
├── todos
│   ├── todos.c
│   ├── todos.mustache.html
│   └── todo.mustache.html
├── billing
│   ├── billing.c
│   └── billing.mustache.html
└── main.c
```

```c
// todos/todos.c
config todos_config(){
  return (config) {
    "todos",
    .databases = { ... },
    .routes = { ... },
    .rendering = { ... }
  };
}
```

Including a module in your app:
```c
// main.c
#include "todos/todos.c"

config mach(){
  return (config) {
    .includes = {todos_config, ...}
  };
}
```

Modules can publish and subscribe to events, allowing them to react to activity in other modules without direct coupling. A billing module can subscribe to an `order_placed` event that a cart module just happens to publish, without either knowing anything about the other's internals.

This makes MACH suitable for large codebases where teams own individual modules, billing, inventory, shipping, cart, with clean boundaries enforced by the framework.

### Events
Modules communicate across boundaries through a publish and subscribe event system. A module declares what it publishes and what it subscribes to directly in the config, making inter-module contracts visible at a glance.

```c
.publishes = {
  {"order",
    .contract = {
      "user",
      "cart"
    }
  }
},
.subscribes = {
  {"shipped", .call = on_shipped}
}
```

`.contract` declares what keys the publishing module guarantees to set on the context when the event fires. Subscribing modules can rely on those keys being present.

A handler publishes an event with `publish_async`:

```c
void create_order(){
  set("user", user);
  set("cart", cart);
  publish_async("order");
}
```

The subscribed handler runs in the subscribing module when the event fires:

```c
void on_shipped(){
  auto const user = get("user");
  auto const cart = get("cart");
}
```

Any number of modules can subscribe to the same event independently. Adding a new subscriber requires no changes to the publishing module or any other subscriber.

### Routing
Routes are declared in the config. Each route specifies a method, path, and how to respond.

```c
.routes = {
  // inline responses
  {http_get, "/", .text = "hello"},
  {http_get, "/", .html = "<p>hello</p>"},
  {http_get, "/", .json = "{\"hello\": \"world\"}"},
  {http_get, "/", .js = "console.log('hello')"},
  {http_get, "/", .css = "body { margin: 0; }"},

  // handler function
  {http_get, "/todos", .call = list_todos},

  // render a template directly, no handler needed
  {http_get, "/about", .render = "about"},

  // named route with handler and middleware
  {http_get, "/todos", "todos", .call = list_todos, .uses = {"logged_in"}},

  // route parameter
  {http_patch, "/todos/:id", .call = update_todo},

  // sse channel
  {http_sse, "/todos_sse", "todos_sse", .client = "todos/:user_id"},

  // error handlers
  {.status = http_not_found, .render = "404"},
  {.status = http_error, .render = "5xx"}
}
```

**Named Routes**

Giving a route a name registers it for reverse URL lookup. Templates can reference named routes without hardcoding paths:

```html
<a href='{{:url:todos}}'>todos</a>
<form action='{{:url:create_session}}' method='post'>
```

If a path changes, the name stays the same and links update automatically.

**Route Parameters**

Parameters declared with `:` in the path are available in the handler via `get()`:

```c
{http_patch, "/todos/:id", .call = update_todo}

void update_todo(){
  auto const id = get("id");
  ...
}
```

**Error Routes**

Error and not found routes are declared like any other route using `.status`:

```c
{.status = http_not_found, .render = "404"},
{.status = http_error, .render = "5xx"}
```

Not found errors added to the context by `db_find_async_await` bubble up to the `http_not_found` handler automatically if not caught in the handler.

**Supported Methods**

`http_get`, `http_post`, `http_put`, `http_patch`, `http_delete`, `http_head`, `http_options`, `http_sse`

### Templates

Template engines are included as modules and must be included in the config:

```c
#include <mustache.h>
#include <mdm.h>

config mach(){
  return (config) {
    .includes = {mustache_config, mdm_config},
    ...
  };
}
```

Templates are registered by name in the rendering config. Multiple engines can be declared per module, allowing mustache and mdm templates to be mixed freely.

Templates can be embedded from external files using `#embed`, or declared as string literals inline:

```c
.rendering = {
  {
    .engine = "mustache",
    .templates = {
      {"layout", (string_embed){
        #embed "layout.mustache.html"
      }},
      {"home", (string_embed){
        #embed "home.mustache.html"
      }},
      {"todo", (string_embed){
        #embed "todo.mustache.html"
      }},
      {"flash", "<p>{{message}}</p>"},
      {"spinner", "<div class='spinner'></div>"}
    }
  },
  {
    .engine = "mdm",
    .templates = {
      {"about", (string_embed){
        #embed "about.mdm.md"
      }}
    }
  }
}
```

**Mustache**

`mustache` renders `.mustache.html` templates. MACH supports the full Mustache spec with the exception of lambdas and dot notation. All template lookups including layouts and partials resolve by name from the registered templates config. See the [Mustache spec](https://mustache.github.io/mustache.5.html) for full usage.

**MDM**

`mdm` renders `.mdm.md` templates. MDM is GitHub Flavored Markdown with Mustache templating. See the [GitHub Markdown spec](https://github.github.com/gfm/) for Markdown usage.

**MACH Template Commands**

MACH extends Mustache with commands that follow the format `:command:param1:param2:etc`.

`{{:url:route_name}}` outputs the path for a named route:

```html
<a href="{{:url:todos}}">todos</a>
<form action="{{:url:create_session}}" method="post">
```

`{{:raw:key}}` outputs a context value as unescaped HTML.

`{{:precision:key:decimal_places}}` outputs a number formatted to a given number of decimal places.

### Databases
MACH supports multiple simultaneous databases per application, even database per tenant. Each database is declared in the config with a name, engine, and connection string. Modules can declare their own databases, keeping data ownership aligned with module boundaries.

```c
.databases = {
  {"app_db", .engine = "postgres", "postgresql://localhost/myapp"},
  {"cache_db", .engine = "sqlite", "file:cache.db?mode=rwc"}
}
```

**Migrations & Seeds**

Migration functions run in order on startup. Each function calls `migrate()` with the SQL to execute. Seed functions run after all migrations complete.

```c
void create_users_table(){
  auto const q =
    "CREATE TABLE IF NOT EXISTS users ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "username TEXT NOT NULL UNIQUE "
    ");";
  migrate(q);
}

void seed_users(){
  auto const q =
    "INSERT INTO users(username) "
    "VALUES('admin');";
  seed(q);
}

.databases = {
  {"app_db", .engine = "sqlite", "file:app.db?mode=rwc",
    .migrations = {create_users_table},
    .seeds = {seed_users}
  }
}
```

MACH tracks migration and seed state in a `mach_schema_meta` table inside each database, storing the version, whether the database has been seeded, and more.

MACH also outputs a schema dump inside the Docker image that ai, lsp, and other tooling can read, so you can ask the built-in ai to write queries, introspect your schema, or generate migrations without leaving your editor.

**Querying**

`db_query_async_await` expects multiple rows and sets the result as a table on the context:

```c
db_query_async_await("todos", "app_db", "select * from todos where user_id = ?;", uid);
```

`db_find_async_await` expects a single row and sets the result as a record on the context. If no record is found, it adds a not found error to the context, this can be caught and handled in the handler, or left to bubble up to the `http_not_found` route handler automatically.

```c
db_find_async_await("todo", "app_db", "select * from todos where id = ?;", tid);
```

**Connection Pools**

`db_` functions use a global connection pool, one persistent connection per reactor thread. Best for shared databases that every request hits.

`tenant_db_` functions use an LRU connection pool, connections are cached for recently active tenants and evicted when idle. Best for per-tenant databases where you may have many databases but only a fraction active at any time.

```c
db user_db(){
  auto const uid = get("user_id");
  auto const d = sfmt("file:%s_todos.db?mode=rwc", uid);
  return (db){"app_db", .connect = d};
}

tenant_db_query_async_await("todos", user_db(), "select * from todos;");
```

### Middleware
Middleware is declared with a name and a `.before` handler, a `.after` handler, or both, registered in the config, and applied to routes via `.uses`.

```c
.middleware = {
  {"session", .before = session},
  {"logged_in", .before = logged_in},
  {"pretty", .after = pretty},
  {"compression", .before = deflate, .after = inflate}
}
```

Apply globally to all routes in the module:
```c
config mach(){
  return (config) {
    .uses = {"session"},
    ...
  };
}
```

Apply to a specific route:
```c
{http_delete, "/session", .call = logout, .uses = {"logged_in"}}
```

Middleware follows the same registration and resolution rules as everything else, a middleware registered in `main.c` overrides one of the same name in an included module. This lets you replace a module's default `logged_in` implementation with your own without touching the module itself.

Middleware is just a handler. Writing one requires no special api, read from the context, set values, or short-circuit the request. The same patterns you use everywhere in MACH apply.

### SSE & Realtime
MACH has first-class SSE support. A dedicated `http_sse` route type establishes a named client channel, keeping a persistent connection open to the browser.

```c
{http_sse, "/todos_sse", "todos_sse", .client = "todos/:user_id", .uses = {"logged_in"}}
```

`.client` defines the channel identifier. Each user gets their own named channel (e.g. `todos/42`). Handlers push events to a channel using the SSE response functions:

```c
auto const client = sfmt("todos/%s", uid);

sse_data_res_async(client, body);    // send a data event
sse_event_res_async(client, body);   // send a named event
sse_comment_res_async(client, body); // send a comment
```

Because SSE channels are identified by name, any handler including event handlers from other modules can push updates to a connected client. A cart module can push an update to a client connected through a billing module's SSE channel, with neither module knowing about the other.

## Handler API
Everything available inside a handler.

### Reading & Writing

`get(key)` reads a value from the context. Pulls from all sources: query params, route params, form data, request body, and session.

`set(key, value)` writes a value to the context, making it available to templates and subsequent handlers.

`has(key)` returns true if a key exists in the context.

```c
auto const uid = get("user_id");
auto const name = get("name");
if(!has("name")) set("name", "World");
```

### Strings

`sfmt(format, ...)` formats a string using printf-style format specifiers. Memory is managed by the arena allocator, no manual free needed.

```c
auto const msg = sfmt("Hello %s", name);
auto const path = sfmt("file:%s.db?mode=rwc", uid);
```

### Responses

`res_async(mime_type, http_status, body)` queues a raw response.

`render_res_async(http_status, template_key)` queues a template render as the response.

`redirect_async(route_key)` queues a redirect to a named route.

`transfer_async(handler)` transfers execution to another handler within the same request, no HTTP round trip.

`async_await()` executes all queued async tasks concurrently and resumes when all complete.

### SSE

`sse_data_res_async(client_key, body)` queues a data event to a named SSE channel.

`sse_event_res_async(client_key, body)` queues a named event to a named SSE channel.

`sse_comment_res_async(client_key, body)` queues a comment to a named SSE channel.

```c
auto const client = sfmt("todos/%s", uid);
sse_data_res_async(client, "some data");
sse_event_res_async(client, "some event");
```

### Events

`publish_async(event_key)` publishes an event to all subscribers.

### Database

`db_query_async(key, db_key, query, ...)` queues a multi-row query without waiting.

`db_query_async_await(key, db_key, query, ...)` queues and immediately executes a multi-row query, setting the result as a table on the context.

`db_find_async(key, db_key, query, ...)` queues a single-row query without waiting.

`db_find_async_await(key, db_key, query, ...)` queues and immediately executes a single-row query, setting the result as a record on the context. Adds a not found error to the context if no record is found.

`tenant_db_query_async(key, db, query, ...)` same as `db_query_async` using the LRU tenant pool.

`tenant_db_query_async_await(key, db, query, ...)` same as `db_query_async_await` using the LRU tenant pool.

`tenant_db_find_async(key, db, query, ...)` same as `db_find_async` using the LRU tenant pool.

`tenant_db_find_async_await(key, db, query, ...)` same as `db_find_async_await` using the LRU tenant pool.

```c
db_query_async_await("todos", "app_db", "select * from todos where user_id = ?;", uid);
db_find_async_await("todo", "app_db", "select * from todos where id = ?;", tid);
```

### Tables & Records

Query results are returned as tables (multiple rows) or records (single row) on the context. They can also be constructed manually.

`table_get(table, index)` gets a record from a table by index.

`table_count(table)` returns the number of records in a table.

`table_add(table, record)` adds a record to a table.

`table_remove(table, record)` removes a record from a table.

`table_remove_at(table, index)` removes a record by index.

`table_nest(base, nest, new_base_field, base_field, nest_field)` joins two tables by key, nesting the second table's records into the first.

`new_table()` creates an empty table.

`record_get(record, key)` gets a field value from a record.

`record_set(record, key, value)` sets a field value on a record.

`record_remove(record, key)` removes a field from a record.

`new_record()` creates an empty record.

```c
auto const t = get("challengers");
auto const p0 = table_get(t, 0);
auto const p1 = table_get(t, 1);
record_set(p0, "opponent_id", record_get(p1, "id"));
```

### Headers & Cookies

`headers_set(key, value)` sets a response header.

`cookies_set(key, value)` sets a response cookie.

### Errors

`error(message)` adds an error to the context. Unhandled errors bubble up to the `http_error` route handler.

### Memory

`alloc(bytes)` allocates memory from the arena. Freed automatically at the end of the request.

`defer_free(ptr)` registers a pointer for deferred cleanup at the end of the request, for memory allocated outside the arena such as from a C library.

