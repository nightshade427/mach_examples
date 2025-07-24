# MACH — Complete reference

C23 web framework. App = `config mach()` returning `(config){...}` of resources, databases, modules. Each request runs a pipeline of steps over a per-request context. Memory, threads, I/O managed by the framework. Tasks and events are durable.

> **This is the combined single-file reference.** It contains both the rules (Critical Rules + C Syntax Patterns) and 16+ canonical snippets in one file. Optimized for single-file deployment (Cursor/Continue docs, system-prompt-only setups). For deployments with retrieval, the split version (`llms-full.md` + `mach-snippets.md`) is more efficient.

> **Workflow on small models:** scan the snippet picker first (next section). If a snippet matches your task, copy it and substitute names. Fall back to rules + canonical snippet only if no listed pattern fits.

---

## SNIPPET PICKER — find your shape first

| If you need... | Use snippet |
|---|---|
| Show one parent record with its children (project+tasks, blog+comments, order+lines) | **#1 Detail page with children** |
| Show one record by id, 404 if missing | **#2 Detail page with find()** |
| List of records with a count or stats | **#3 List with concurrent count** |
| Form that POSTs, validates, redirects on success, re-renders with errors on failure | **#4 POST-redirect-GET form** |
| Update one field of one record (PATCH) | **#5 Update flow** |
| Delete one record | **#6 Delete flow** |
| Multi-verb resource (GET+PATCH+DELETE) sharing the same `:id` validation | **#6b Shared validation in `.steps`** |
| Create record AND notify another module | **#7 POST + emit event** + **#7b Subscriber module** |
| Conditional rendering — HTMX fragment vs full page | **#8 HTMX-aware rendering** |
| Fetch external API, render result | **#9 External fetch** |
| Trigger background work after user action | **#10 Async task on action** |
| Recurring cron job | **#11 Cron task** |
| Page only authenticated users can see | **#12 Auth-gated resource** |
| Server-Sent Events for live updates | **#13 SSE broadcast** |
| Datastar partial DOM updates | **#14 Datastar SSE update** |
| Per-user / per-tenant database | **#15 Multi-tenant DB** |
| SQL/templates extracted into separate files | **#16 External assets** |

Snippets are at the bottom of this file. Rules and patterns follow next.

---

## CRITICAL RULES — read before writing any code

### Rule 1 — NEVER use `.` between `{{` and `}}`

`{{a.b}}` renders empty. Always use sections to enter nested scope: `{{#name}}...{{/name}}`. Template shape mirrors context shape.

**Helpers use `:` not `.` — these are fine:** `{{url:name}}`, `{{input:title}}`, `{{error:title}}`, `{{precision:total:2}}`.

#### Pattern A — Single root scalar

```c
.context = {{"site_name", "MACH App"}}
```
```html
✅ <h1>{{site_name}}</h1>
```

#### Pattern B — `query()` / `find()` result is a TABLE, even single-row

```c
find({.set_key = "todo", .db = "todos_db",
  .query = "select id, title from todos where id = {{id}};"})
```
Context: `{ todo: [{id: 5, title: "Learn MACH"}] }`. Open the section to read fields:
```html
✅ {{#todo}}<h1>{{title}}</h1>{{/todo}}
❌ {{todo.title}}             dot — renders ""
❌ <h1>{{title}}</h1>         not inside #todo — renders ""
```

#### Pattern C — After `join()`: parent + nested children (most common; most-failed)

`query()` produces sibling tables. `join()` **MOVES** children inside parents — children no longer exist at root.

```c
query(
  {.set_key = "project", .db = "projects_db",
   .query = "select id, name from projects where id = {{id}};"},
  {.set_key = "tasks",   .db = "projects_db",
   .query = "select id, project_id, title from tasks where project_id = {{id}};"}
),
join(.target_table_key="project", .target_field_key="id",
     .nested_table_key="tasks", .nested_field_key="project_id",
     .target_join_field_key="tasks"),
```
Context after join: `{ project: [{id, name, tasks: [{id, project_id, title}, ...]}] }`. Tasks are now INSIDE project.

```html
✅ {{#project}}
     <h1>{{name}}</h1>
     <ul>{{#tasks}}<li>{{title}}</li>{{/tasks}}</ul>
   {{/project}}
```

**❌ Every one of these renders empty:**
- `{{project.name}}` — dot
- `{{project.tasks.title}}` — dot
- `{{#project.tasks}}...{{/project.tasks}}` — dot in section name
- `{{#tasks}}<li>{{title}}</li>{{/tasks}}` — at root after join, tasks lives inside project, not at root
- `<h1>{{name}}</h1>` then `{{#tasks}}...{{/tasks}}` separately — name is inside project record too; no partial entry. **EVERY field that came from `set_key="X"` requires `{{#X}}` first**, including the parent's own scalars.

**Rule of thumb:** if a field came from `query().set_key = "X"`, it must be inside `{{#X}}`. No partial entry — you're inside the section or at root.

#### Pattern D — 3+ levels nested

`{ org: [{name, projects: [{title, tasks: [{label}]}]}] }`:
```html
✅ {{#org}}{{name}}{{#projects}}{{title}}{{#tasks}}{{label}}{{/tasks}}{{/projects}}{{/org}}
❌ {{org.projects.tasks.label}}    dot
❌ {{#org.projects}}...             dot in section name
```

#### Pattern E — Iteration: `{{#name}}` auto-loops when name is array

```c
{ projects: [{title:"A", tasks:[{label:"x"}]}, {title:"B", tasks:[{label:"z"}]}] }
```
```html
✅ {{#projects}}<li>{{title}}<ul>{{#tasks}}<li>{{label}}</li>{{/tasks}}</ul></li>{{/projects}}
```

#### Template Checklist — run on every `{{...}}` BEFORE emitting

0. **Well-formed:** exactly two `{` and two `}`, no whitespace inside delimiters. `{{/x}` (one `}`), `{x}}`, `{{{x}}}`, `{ {x} }` are all broken. After typing every `{{/...}}`, count the closers: `}` `}`.
1. **No dot:** any `.` between `{{` and `}}` → STOP, add a section.
2. **Field reachable:** is the bare name at the current scope? At root, only top-level keys. Inside `{{#X}}`, only fields of `X`'s record. After `join(child→parent)`, child no longer at root — must be inside `{{#parent}}`.
3. **Depth = wrappers:** N nesting levels = N section pairs.
4. **Section balance + nesting:** every `{{#x}}` has exactly one `{{/x}}` later, and vice versa. Sections nest like parens, never overlap. `{{#a}}{{#b}}{{/a}}{{/b}}` is broken even though counts match. Each named section opens **once** in a given scope.

---

### Rule 2 — ONE database per DOMAIN, not one per table

A "domain" is **what one MODULE owns** — a feature slice — not a noun in your data model.

A `projects` module owns *everything* about projects: project records, tasks attached to them, comments on those tasks, tags. **All of that lives in ONE database** (`projects_db`) with **many migrations** (one per table). Tasks aren't their own domain just because "tasks" is a noun — they're part of projects because they belong to projects.

**A new database appears only when a new module appears.** New table inside the existing module → new migration in the existing db, NOT a new db.

#### ✅ Correct
```c
.databases = {{
  .engine = sqlite_db, .name = "projects_db",
  .connect = "file:projects.db?mode=rwc",
  .migrations = {
    "CREATE TABLE projects (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL);",
    "CREATE TABLE tasks (id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "project_id INTEGER NOT NULL REFERENCES projects(id), title TEXT NOT NULL);",
    "CREATE TABLE comments (id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "task_id INTEGER NOT NULL REFERENCES tasks(id), body TEXT NOT NULL);"
  }
}}
```

#### ❌ Wrong — both forms

```c
// Form A: one db per table
.databases = {{...projects_db...}, {...tasks_db...}, {...comments_db...}}

// Form B (subtler, most common): parent + child split into separate dbs
.databases = {{...projects_db (with projects only)...},
              {...tasks_db (tasks should live in projects_db)...}}
query({.db = "projects_db", ...}, {.db = "tasks_db", ...})  // wrong db
```

**Parent-child relations are ONE domain, ONE database.** project+tasks, blog+comments, order+line_items, user+sessions, todo+comments — all one domain each. If `child` has `parent_id REFERENCES parent`, they belong in the same db.

**Rationalizations to REJECT:**
- *"X and Y are different concepts so they should be different domains"* → Wrong. The foreign-key relation IS what makes them one domain.
- *"More normalized / cleaner separation"* → MACH's separation unit is the **module**, not the table.
- *"Microservices use one db per service"* → Right analogy, wrong unit. In MACH, "service" = module, not table. One module = one db.
- *"My data model has X entities so I need X dbs"* → No. Number of dbs = number of modules. A module owns 3–10 tables.

**Self-check before adding a 2nd `.databases` entry:** "Am I adding a new module?" If no, you don't need a new db — add a migration to the existing one.

---

### Rule 3 — Concurrent queries: ONE step, MANY items (works across dbs too)

`query()` and `fetch()` items run in parallel. Two separate `query()` steps run **serially**. If items don't depend on each other, put them in ONE step.

```c
✅ // Same db, concurrent
query(
  {.set_key="todos", .db="todos_db", .query="select id, title from todos;"},
  {.set_key="count", .db="todos_db", .query="select count(*) as n from todos;"}
)

✅ // Across dbs — STILL one step, still concurrent
query(
  {.set_key="user",     .db="users_db",    .query="..."},
  {.set_key="orders",   .db="commerce_db", .query="..."},
  {.set_key="activity", .db="activity_db", .query="..."}
)

❌ // Three serial round-trips for independent queries
query({.set_key="user", .db="users_db", ...}),
query({.set_key="orders", .db="commerce_db", ...}),
query({.set_key="activity", .db="activity_db", ...})
```

Use separate steps **only** when a later query depends on a value the earlier one produced.

---

### Rule 4 — SQL `{{values}}` are prepared-statement parameters

Auto-bound, never spliced. SQL injection impossible. Don't pre-quote, don't pre-escape.

```c
✅ query({.db="db", .query="insert into todos(title) values({{title}});"})
❌ query({.db="db", .query="insert into todos(title) values('{{title}}');"})  // double-quoted
```

For transactions: `BEGIN`/`COMMIT`/`ROLLBACK` directly in queries.

---

### Rule 5 — `query()`/`find()` item: asset name OR `.query`. Pick one. The asset must exist.

```c
✅ query({.set_key="todos", .db="todos_db", .query="select ..."})  // inline; works anywhere

✅ query({"get_todos", .set_key="todos", .db="todos_db"})  // asset by name
   // requires: .context = {{"get_todos", (asset){#embed "get_todos.sql"}}}
   // WITHOUT that .context entry, this is a phantom reference — fails.

❌ query({"get_todos", .set_key="todos", .db="todos_db", .query="..."})  // both → boot rejection

❌ query({"get_todos", .set_key="todos", .db="todos_db"})  // when no .context defines "get_todos"
```

**If your snippet has no `.context` block embedding SQL files, use inline `.query` only.** Don't reach for the positional name to avoid writing the SQL — that produces a phantom reference. The asset-name form is for when SQL grows large and you've extracted it; not for casual use.

Same rule applies to `find()` and `render()` (asset name in `.context` vs `.template` inline).

---

### Rule 6 — `find()` raises `http_not_found` on zero rows; `query()` does not

Otherwise identical. `find()` for "must exist" lookups (detail pages, by-id reads). `query()` for lists, counts, writes, anywhere zero rows is normal.

---

### Rule 7 — No `malloc`/`free`, no threads, no mutexes

Per-request arena handles memory. Reactors + thread pool handle concurrency. App code never calls these.

For a buffer: `char *buf = allocate(256);` (reclaimed at request end). For external-lib pointer cleanup: `defer_free(out);`.

---

### Rule 8 — Resource-based, not route-based

Resources referenced by **name**, never hard-coded path:

```c
{{url:todos}}           // → /todos
{{url:todo:5}}          // → /todos/5  (literal arg)
{{url:todo:id}}         // → /todos/{id from context}
redirect("todo:{{id}}") // 302 to /todos/{id}
reroute("todos")        // re-enter pipeline server-side
```

---

## C SYNTAX PATTERNS — required for compilation

Three patterns trip up small models even when MACH semantics are correct.

### Pattern 1 — Adjacent string literals concatenate. ONE quoted line per source line.

C joins adjacent strings at compile time. Use this for every multi-line SQL and every inline template. Never put a raw newline inside a single quoted string. Never mix quoted and bare text on one line.

```c
✅ "CREATE TABLE projects ("           // each line its own
   "id INTEGER PRIMARY KEY,"            // properly terminated string
   "name TEXT NOT NULL"                 // C concatenates them
   ");"

❌ "CREATE TABLE projects (              // raw newline = compile error
     id INTEGER PRIMARY KEY, ...

❌ "<ul>"
     {{#tasks}}<li>{{title}}</li>{{/tasks}}    // bare Mustache, not in string
   "</ul>"
```

A MACH inline template is **not a heredoc** — it's a stack of small C string literals the compiler glues. Every fragment, every section marker, every HTML scrap must be inside its own `"..."`.

### Pattern 2 — Any `"` inside a C string ends it. Use `'` for HTML, no quotes for SQL identifiers.

#### 2a — SQL identifiers: don't quote them
```c
✅ "CREATE TABLE projects (id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
❌ "CREATE TABLE projects ("id" INTEGER ..., "name" TEXT ...);"  // " ends C string
```
SQLite/Postgres/MySQL all accept unquoted identifiers for normal names. Never quote in MACH SQL unless reserved word (then escape with `\"`).

#### 2b — HTML attributes: USE SINGLE QUOTES EXCLUSIVELY. Never `\"`.

```c
✅ "<li class='task' data-id='{{id}}'>{{title}}</li>"   // single quotes — nothing to escape

⚠️ "<li class=\"task\" data-id=\"{{id}}\">{{title}}</li>"
   // works, but every \" is a slip waiting to happen.
   // Drop one backslash on one attribute → compile error.

❌ "<li class=\"task\" data-id="{{id}}">{{title}}</li>"
   //                            ↑ bare " — ends C string. The most-recurring HTML failure
   //   is mixing escaped \" and bare " in one template. Don't escape; use ' everywhere.
```

**Rule:** if your inline template contains any `\"`, you took the harder route. Switch every HTML attribute to `'...'` before emitting.

### Pattern 3 — All `.fields` go INSIDE one `(config){...}` block. Brace tracking is the most-regressed bug.

There is **no file-scope config in MACH.** `.databases`, `.modules`, `.resources` are NOT standalone declarations — they are fields of the single struct value `mach()` returns. Designated-initializer syntax (`.field = value`) is meaningful only **inside** a struct initializer.

> **Mechanical rule:** in the entire `mach()` body there must be **exactly one `};`** (closing `(config){...}` + return) and **exactly one `}`** after it (closing the function). No others.

#### Three failure modes (all close `(config){...}` at the wrong point)

```c
✅ config mach(){
     return (config){          // depth 1 opens here
       .resources = {...},     // ,  separator
       .databases = {{...}},   // ,  separator
       .modules   = {sqlite}   // last field, no comma
     };                        // ONE };  closes (config){...} + return
   }                           // ONE }   closes function

❌ A: premature `};` mid-function
   .resources = {...}
   };                          // ← ends return early
   .databases = {...};         // ← orphan inside function body
   .modules = {sqlite};

❌ B: fields placed AFTER mach() closes (file scope)
   };
   }                           // function ends here
   .databases = {...};         // ← floating at file scope = compile error
   .modules = {sqlite}
   // Giveaway: comment near here saying "global config" or "module-level" — there's no such thing.

❌ C: `},` closes (config){...} too early mid-function (most-recurring)
   .resources = {...
     }
   },                          // ← `},` = `}` closed (config){...} + stray `,`
     .databases = {{...}},     // ← orphan in function body
     .modules = {sqlite}
   };                          // orphan close
```

#### Post-field-block boundary check

After every `.field = ...` block closes with `}`, the next non-whitespace token must be:
- `,` → another `.field = ...` follows (still inside `(config){...}`). ✅
- `};` → this was the last field, end of return. ✅
- `},` → BUG: extra `}` closed `(config){...}` early plus stray `,`. **Failure mode C.** Delete one `}`.

**Trouble signals (any of these = bug):**

| You see | What broke |
|---|---|
| Two `};` lines in `mach()` | Failure mode A |
| `};` `}` then `.field =` lines below | Failure mode B (file-scope orphans) |
| `},` mid-function with `.field =` lines below | Failure mode C |
| `.engine = sqlite_db` but no `.modules = {sqlite}` | Missing bundled module (see Modules) |
| Indentation says fields are inside `(config){...}` but braces say otherwise | Trust braces, not indent |

### Self-check before emitting any snippet

1. **Strings:** every line of every multi-line SQL and template starts with `"` and ends with `"`. No raw newlines or bare HTML/SQL between strings.
2. **Bare `"`:** no SQL identifiers in `"..."`. No HTML attributes in `attr="..."` — use `'...'` or `\"...\"`. **Shortcut: any `\"` in inline template → switch whole template to single quotes; faster than auditing.**
3. **Brace count:** exactly one `};` (closes `(config){...}` + return) and one `}` (closes function). All `.fields` inside that one initializer, comma-separated.
4. **No file-scope fields:** scan from closing `}` of `mach()` to end of file. Nothing between except other `config` functions or `#include`. If you see `.databases`/`.modules`/`.resources`/etc. floating, move them inside.
5. **Modules registered:** every `.engine = X_db` requires `X` in `.modules`. Search snippet for `.modules` — zero matches = bug.

---

## CANONICAL WORKED SNIPPET — copy this shape for parent + children pipelines

This is the right shape for: project+tasks, blog+comments, order+line_items, user+sessions, playlist+tracks, anything with a parent-child relation. **Substitute names freely; do not change shape.**

```c
#include <mach.h>
#include <sqlite.h>

config mach(){
  return (config){             // ← (config){ opens — depth 1
    .resources = {             //   .resources opens — depth 2
      {"project", "/projects/:id",
        .get = {
          validate({"id", .validation = validate_integer,
            .message = "must be an integer"}),

          // Rule 3: ONE step, TWO items, SAME db (Rule 2: parent + child = one domain)
          query(
            {.set_key="project", .db="projects_db",
             .query="select id, name from projects where id = {{id}};"},
            {.set_key="tasks",   .db="projects_db",   // ← SAME db
             .query="select id, project_id, title from tasks where project_id = {{id}};"}
          ),

          join(.target_table_key="project", .target_field_key="id",
               .nested_table_key="tasks", .nested_field_key="project_id",
               .target_join_field_key="tasks"),

          // Rule 1 Pattern C: enter {{#project}} for ALL parent fields,
          // including its own scalars (name, id) and nested children (tasks).
          render(.template =
            "{{#project}}"
              "<h1>{{name}}</h1>"
              "<p>Project ID: {{id}}</p>"
              "<ul class='tasks'>"
                "{{#tasks}}<li class='task'>{{title}}</li>{{/tasks}}"
              "</ul>"
            "{{/project}}")
        }
      }
    },                         // ← close .resources (depth 2 → 1), comma to continue
                               //   ⚠️ depth IS 1 here. NEXT line must be another
                               //   `.field =` OR `};`. Writing `},` here = Failure mode C.

    // Rule 2: ONE db, TWO migrations
    .databases = {{
      .engine     = sqlite_db,
      .name       = "projects_db",
      .connect    = "file:projects.db?mode=rwc",
      .migrations = {
        "CREATE TABLE projects (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL);",
        "CREATE TABLE tasks ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "project_id INTEGER NOT NULL REFERENCES projects(id),"
          "title TEXT NOT NULL"
        ");"
      }
    }},                        // ← close .databases (depth 2 → 1), comma to continue

    .modules = {sqlite}        // ← REQUIRED. `.engine = sqlite_db` above forces this.
                               //   Every .engine = X_db needs X in .modules.
                               //   Never omit "for brevity" — there is no elsewhere.
  };                           // ← `};` closes (config){...} + return (depth 1 → 0).
                               //   This MUST be the only `};` in the function.
}                              // ← `}` closes function body.
```

**What the shape requires (all immutable regardless of names):**
- ONE `.databases` entry containing migrations for parent AND child
- ONE `query()` step with multiple items, all SAME `.db`
- ONE `join()` between them
- Template wraps everything in `{{#parent}}...{{/parent}}`, children iterated inside
- `.modules = {<engine>}` listed once
- Everything inside ONE `(config){...}` block, ONE `};`, ONE `}`

---

## MODULES — two different things called "module"

> **1. User-defined modules** = `config foo(){...}` functions you write to split an app into features. **Skip these for small/single-file snippets** — put everything in `config mach()`. Defining one without registering under `.modules = {foo, ...}` makes it dead code.
>
> **2. Bundled modules** = engine and feature modules shipped with MACH. **MUST be registered in `.modules` whenever you use what they provide.** `.modules` is never optional.

#### Module-registration trigger rule (mechanical)

Every `.engine = X_db` line requires `X` in `.modules`:

| `.engine =` | `.modules` must include |
|---|---|
| `sqlite_db`   | `sqlite`   |
| `postgres_db` | `postgres` |
| `mysql_db`    | `mysql`    |
| `redis_db`    | `redis`    |
| `duckdb_db`   | `duckdb`   |

Feature modules:

| You use | Add to `.modules` |
|---|---|
| `ds_sse(...)` | `datastar` |
| `session()`, `logged_in()`, `login()`, `signup()` | `session_auth` |
| `is_htmx` flag, HTMX-aware rendering | `htmx` |

**Pre-emit check:** search your output for the literal string `.modules`. Zero matches = bug. If `.modules` exists but missing an entry for an engine you used = bug. Apply at point of typing — memory-level reminders don't reliably fire mid-generation.

---

## REFERENCE

### Notation

`{}` = single value/struct. `{{}}` = array of structs. `query({...}, {...})` = multiple items in one step.

### Context

Per-request key-value store. Three scopes:
- `input:xxx` — raw request params
- `error:xxx` — validation/error data
- (unprefixed) — app scope: query results, validated inputs, `.context` values

`validate()` promotes `input:` → app scope on success. `.context` seeds variables and assets at root, baked at compile time via `(asset){#embed "file"}`.

```c
.context = {
  {"site_name", "MACH App"},
  {"layout",    (asset){#embed "static/layout.mustache.html"}},
  {"get_todos", (asset){#embed "todos/get_todos.sql"}}
}
```

### Databases

Migrations forward-only, index-based, applied once each in array order, tracked in `mach_meta`. Seeds idempotent. Multi-tenant via `{{interpolation}}` in `.connect`; connections pooled with LRU.

**Engines:** `sqlite_db`, `postgres_db`, `mysql_db`, `redis_db`, `duckdb_db`

```c
.databases = {{
  .engine = sqlite_db, .name = "blog_db",
  .connect = "file:{{user_id}}_blog.db?mode=rwc",   // multi-tenant
  .migrations = {"CREATE TABLE blogs (...);", "CREATE TABLE comments (...);"},
  .seeds = {"INSERT OR IGNORE INTO blogs(...) VALUES(...);"}
}}
```

### Resource Pipelines

Each `.resources` entry is a named URL endpoint. Identified by name in `{{url:name}}`, `redirect()`, `reroute()` with colon-separated args (`name:arg1:arg2`).

URL helpers: `{{url:todos}}`, `{{url:todo:5}}` (literal), `{{url:todo:id}}` (context), `{{url:org_todo:acme:5}}` (multi-arg).

Path specificity automatic: `/todos/active` beats `/todos/:id`. Verb selection: HTTP method, or `?http_method=...` (lets HTML forms reach PATCH/DELETE/SSE).

**Fields:**
- `.name` *(pos)*, `.url` *(pos)*, `.steps` *(pos)* — middleware run before every verb
- `.mime` — default response content type
- `.get`, `.post`, `.put`, `.patch`, `.delete` — verb pipelines
- `.sse` — persistent SSE channel; first positional is channel name
- `.errors` — terminal handlers keyed by error code
- `.repairs` — resumable handlers keyed by error code

```c
{"todo", "/todos/:id", {validate({"id", .validation="^\\d+$"})},
  .mime  = mime_html,
  .get   = { find({"get_todo", .set_key="todo", .db="todos_db"}), render("todo") },
  .patch = { validate({"title", .validation=validate_not_empty}),
             query({.db="todos_db", .query="update todos set title={{title}} where id={{id}};"}),
             redirect("todo:{{id}}") },
  .delete = { query({.db="todos_db", .query="delete from todos where id={{id}};"}),
              redirect("todos") },
  .sse    = {"todo/{{id}}", sse(.event="ready")},
  .errors = {{http_not_found, {render("404")}}}
}
```

**MIME:** `mime_html`, `mime_txt`, `mime_sse`, `mime_json`, `mime_js`

### Template Helpers

`{{helper:args}}` — colon-separated, never dot.

| Helper | Purpose |
|---|---|
| `{{raw:field}}` | unescaped output (default escapes) |
| `{{precision:field:N}}` | numeric format, N decimals |
| `{{input:field}}` | raw request param (form repop) |
| `{{error:field}}` | truthy when field has error (use as section) |
| `{{error_message:field}}`, `{{error_code:field}}` | error message / status code |
| `{{url:name[:args]}}` | resource URL with positional args |
| `{{asset:filename}}` | cache-busted URL for `public/` file |
| `{{csrf:token}}`, `{{csrf:input}}` | CSRF token / hidden input (auto-verified) |

> **The table is exhaustive. Helpers not in it do not exist.** Other Mustache implementations have `{{length}}`, `{{count}}`, `{{size}}`, `{{first}}`, `{{last}}`, `{{index}}`, `{{#if}}`, `{{#each}}`, `{{else}}`, lambdas, partials. **None work in MACH.**
>
> **Supported template features:** field interpolation, `{{#name}}` (truthy/iteration), `{{^name}}` (falsy/empty), helpers above. HTML comments `<!--...-->` work; Mustache comments `{{!...}}` do NOT — use HTML.
>
> **Doing common things you might reach for missing helpers:**
> - **Counts:** compute in SQL. `{.set_key="stats", .query="select count(*) as n from tasks where project_id = {{id}};"}` then `{{#stats}}{{n}} tasks{{/stats}}`.
> - **Conditionals:** `{{#has_x}}...{{/has_x}}` where `has_x` is set by query/`exec()`. There is no `{{#if x}}`.
> - **First/last:** different SQL — `limit 1`, `order by ... desc limit 1`.
> - **Index in iteration:** `select row_number() over (...) as n` then `{{n}}`.
>
> **The template layer is a renderer, not a programming language.** If you need a feature not listed, do it in SQL or `exec()`, then render the result.

### Pipeline Steps

Every step accepts `.if_context` and `.unless_context`.

#### validate

Regex-checks request params. On success, promotes `input:name` → app scope. On failure, sets `error:name`, raises `http_bad_request`. All validations in one call complete before the error fires.

`.param_key` *(pos)*, `.validation` (regex/macro), `.message`, `.optional`, `.fallback`.

```c
validate(
  {"email",  .validation=validate_email,     .message="must be email"},
  {"title",  .validation=validate_not_empty, .message="cannot be empty"},
  {"page",   .fallback="1", .validation="^\\d+$"},
  {"filter", .optional=true, .validation="^(active|done)$"}
)
```

**Built-ins:** `validate_not_empty`, `validate_alpha`, `validate_alphanumeric`, `validate_slug`, `validate_no_html`, `validate_integer`, `validate_positive`, `validate_float`, `validate_percentage`, `validate_email`, `validate_uuid`, `validate_username`, `validate_date`, `validate_time`, `validate_datetime`, `validate_url`, `validate_ipv4`, `validate_hex_color`, `validate_zipcode_us`, `validate_phone_e164`, `validate_cron`, `validate_no_sqli`, `validate_token`, `validate_base64`, `validate_boolean`, `validate_yes_no`, `validate_on_off`. Define your own: `#define validate_zipcode "^\\d{5}$"`.

#### find & query

Both run DB queries. `find()` raises `http_not_found` on zero rows; `query()` does not. Otherwise identical.

`.set_key` stores result as TABLE in context (always). SQL inlined with `.query` OR loaded by name from `.context` as positional (Rule 5 — asset must exist). Multiple items in one step run **concurrently** (Rule 3), even across dbs. `{{values}}` parameter-bound (Rule 4). Transactions: `BEGIN`/`COMMIT`/`ROLLBACK`.

`.template_key` *(pos)*, `.query`, `.set_key`, `.db`, `.if_context`/`.unless_context` *(per item)*.

```c
query(
  {"get_todos", .set_key="todos", .db="todos_db"},
  {.set_key="count", .db="todos_db", .query="select count(*) as n from todos where user_id={{user_id}};"},
  {.if_context="show_urgent", .set_key="urgent", .db="todos_db",
   .query="select id, title from todos where priority='high';"}
)
```

#### join

Nests records from one context table into matching records of another, in memory. After `join()`, inner records live INSIDE outer records. Templates must enter outer section.

`.target_table_key`, `.target_field_key`, `.nested_table_key`, `.nested_field_key`, `.target_join_field_key` — see canonical snippet.

#### fetch

HTTP request → context. JSON parsed into tables/records; text stored as string. Multiple items run concurrently.

`.url` *(pos)*, `.set_key`, `.method` (default `http_get`), `.headers` (array of `{name, value}`), `.json`/`.text` (context key as body).

```c
fetch("https://api.x.dev/charge",
  .set_key = "receipt", .method = http_post,
  .headers = {{"Authorization", "Bearer {{api_key}}"}, {"Idempotency-Key", "{{order_id}}"}},
  .json = "order")
```

**Methods:** `http_get`, `http_post`, `http_put`, `http_patch`, `http_delete`, `http_sse_method`

#### exec

Calls C function/block with imperative context access. Dispatched to thread pool. Use for blocking I/O or CPU work. Trigger error pipeline via `error_set()`.

```c
exec(^(){ auto t = get("challengers"); record_set(table_get(t,0), "x", record_get(table_get(t,1), "id")); })
exec(.call = assign_opponents)
```

**Imperative API:** `get/set/has/format`, `allocate/defer_free`, `error_set/get/has`, `table_new/count/get/add/remove/remove_at`, `record_new/set/get/remove`.

#### emit / task

```c
emit("todo_created")    // pub/sub event
task("recount_todos")   // enqueue async job
```

#### sse / ds_sse

`sse(.channel="todos/{{user_id}}", .event="updated", .data={"id: {{id}}"})` — broadcasts on channel; without channel, sends only to requester.

`ds_sse(...)` — Datastar SSE (DOM updates + reactive state). Requires `.modules = {datastar}`. Fields: `.channel`, `.target`, `.mode` (`mode_outer`/`inner`/`replace`/`prepend`/`append`/`before`/`after`/`remove`), `.elements`, `.signals` (JSON), `.js`.

#### render

Outputs Mustache template. Auto-escapes (use `{{raw:field}}` to opt out). All field access follows Rule 1 — sections, never dot. Run Template Checklist on every `{{...}}` before emitting.

`.template_key` *(pos)*, `.template`, `.status`, `.mime`, `.engine` (`mustache` default, `mdm` for Markdown-with-Mustache), `.json_table_key` (serialize table as JSON).

```c
render("todos")
render(.template = "<h1>{{site_name}}</h1>")
render("not_found", .status = http_not_found)
render(.json_table_key = "todos")
```

**Statuses:** `http_ok` (200), `http_created` (201), `http_redirect` (302), `http_bad_request` (400), `http_not_authorized` (401), `http_not_found` (404), `http_error` (500).

> Use either positional asset name (must exist in `.context`) OR inline `.template`. Don't reference phantom asset names.

#### headers / cookies / redirect / reroute / nest

```c
headers({{"X-Request-Id", "{{request_id}}"}, {"Cache-Control", "no-store"}})
cookies({{"session", "{{session_id}}"}})

redirect("todos")              // 302 /todos
redirect("todo:{{id}}")        // 302 /todos/{id}
reroute("todo:{{id}}")         // server-side, in-process

nest({query({...}), render("urgent")}, .if_context="is_urgent")  // shared conditional
```

### Conditionals

Every step accepts `.if_context` (run when key present) / `.unless_context` (when absent). Works on validated inputs, query results, framework flags (`is_htmx`), or flags set by `exec()`.

```c
render("fragment",  .if_context="is_htmx")
render("full_page", .unless_context="is_htmx")
```

### Errors and Repairs

On failure, MACH searches handlers bottom-up: resource → module → root.
- **Errors** terminal: send response, end request.
- **Repairs** resumable: fix context, resume original pipeline at step after failure.

If no matching repair, falls through to errors. Error scope shared across `validate()` and `error_set()`: `{{error:name}}`, `{{error_code:name}}`, `{{error_message:name}}`. Raw input remains in `input:name`.

```c
.errors = {
  {http_not_found,    {render("404")}},
  {http_bad_request,  {render("form")}},
  {http_error,        {render("500")}}
},
.repairs = {{http_not_authorized, {exec(.call = refresh_session_token)}}}
```

**Codes:** built-ins above; any int works. `#define err_quota_exceeded 723` for domain-specific.

### Events (cross-module pub/sub)

When `.publishes` exists anywhere, MACH creates `mach_events` to track delivery. Crashes don't drop events; replay on next boot.

```c
// publisher
.publishes = {{"todo_created", .with = {"user_id", "title"}}},
.resources = {{"todos", "/todos", .post = {validate(...), query({"insert_todo", .db="todos_db"}),
                                            emit("todo_created"), redirect("todos")}}}

// subscriber (different module, OWN db per Rule 2)
.events = {{"todo_created", {query({.db="activity_db",
  .query="insert into activities(kind, user_id) values('created', {{user_id}});"})}}}
```

### Tasks

Async pipelines on task reactors. Triggered by `task("name")` or `.cron`. Durable: `mach_tasks` checkpoints context after each step; crash mid-task resumes at failed step.

```c
.tasks = {
  {"recount_todos",
    {query({.db="todos_db", .query="update users set todo_count = ... where id = {{user_id}};"})},
    .accepts = {"user_id"}},
  {"daily_digest",
    {query({...}), emit("digest_ready")},
    .cron = "0 8 * * *"}
}
```

### Modules & Composition

Every app/module returns `config`. Root `main.c` defines `mach()`; modules define functions with any name. Module owns its resources, **its own db** (one per domain), migrations, templates, event contracts. Same-name conflicts: root wins. Modules communicate ONLY via pub/sub events.

`.name` — module identifier. `.modules` — modules to compose.

A module file:
```c
config blogs(){
  return (config){
    .name = "blogs",
    .resources = {{"blog", "/blogs/:id", .get = {/* validate→query→join→render */}}},
    .databases = {{
      .engine = sqlite_db, .name = "blog_db",
      .connect = "file:blogs.db?mode=rwc",
      .migrations = {
        "CREATE TABLE blogs (...);",
        "CREATE TABLE comments (... REFERENCES blogs(id) ...);"
      }
    }}
  };
}
```

Bring in from `main.c`:
```c
#include "blogs/blogs.c"
config mach(){ return (config){ .modules = {blogs, sqlite} }; }
```

**Bundled modules (add to `.modules`):** `sqlite`, `postgres`, `mysql`, `redis`, `duckdb`, `htmx`, `datastar`, `tailwind`, `session_auth`.

**`session_auth` provides:** `session()` (attaches session, sets `user_id`), `logged_in()` (guard, raises `http_not_authorized`), `login()`, `logout()`, `signup()`. Common as resource middleware:
```c
{"dashboard", "/dashboard", {session(), logged_in()}, .get = {render("dashboard")}}
```

### Static Files / External Deps

`public/` files served directly. Reference with `{{asset:filename}}` (content-checksummed URL, immutable cache).

External C libs: drop into `/vendor` (auto-discovered/linked), or use custom Dockerfile (`FROM mach:latest`, `apt-get install`).

Memory bridges: `allocate(N)` (per-request arena), `defer_free(ptr)` (cleanup at request end).

### Architecture (brief)

- **Boot once:** `mach()` runs at boot; config compiled into execution graph.
- **Multi-reactor:** request reactors (one/CPU), task reactors (one/CPU), shared thread pool for `exec()` and blocking I/O.
- **Memory:** per-request arena allocators; cleared at request end. Pipelines >5MB abort with 500.
- **Safety:** SQL injection prevented by parameter binding (Rule 4). XSS prevented by `render()` auto-escape. CSRF prevented by `{{csrf:token}}`/`{{csrf:input}}`.

---

## PRE-EMIT SCAN — search your output for these strings before returning

| Search for | Failure |
|---|---|
| `.` between `{{` and `}}` (e.g. `{{x.y}}`) | Rule 1: dot notation |
| Two `_db",` lines with different prefixes for ONE domain (e.g. `projects_db` + `tasks_db`) | Rule 2: parent+child split |
| Multiple `query({` calls in one pipeline that don't depend on each other | Rule 3: serial when concurrent needed |
| Positional asset name in `query()`/`find()` with no matching `.context` entry | Rule 5: phantom asset |
| `config X()` not appearing in any `.modules = {...}` | Unregistered user module (dead code) |
| `.engine = X_db` with no `X` in `.modules` | Missing bundled module (most-recurring regression) |
| `{{#child}}` at root scope after a `join(child→parent)` | Pattern C: orphan section |
| Bare `{{name}}` at root when `name` came from `set_key="X"` table | Pattern C: missing `{{#X}}` wrapper |
| `\"` in inline template | Pattern 2b: switch whole template to `'...'` |
| `{{length}}`, `{{count}}`, `{{#if}}`, `{{#each}}`, etc. | Improvised helpers — not in MACH |
| `{{#a}}` opened twice in same scope | Overlapping sections (Check 4) |
| `{{/x}` (one `}`), `{x}}`, `{{{x}}}`, `{ {x} }` | Malformed tag delimiters (Check 0) |
| Two `};` in one `mach()` | Pattern 3 Failure A |
| `};` then `}` then `.field =` lines | Pattern 3 Failure B (file-scope orphans) |
| `},` between fields where `}` closed `(config){...}` | Pattern 3 Failure C |
| `{{/x}}` with no matching `{{#x}}` earlier | Section balance (Check 4) |
| `validate()` step missing when prompt asked for one | Prompt-following: re-read prompt |

If any match: stop and fix. When in doubt about parent + child: copy snippet **#1 (Detail page with children)** in the Canonical Snippets section below, then rename.

---

# CANONICAL SNIPPETS

The 16 patterns referenced by the Snippet Picker at the top. Each snippet is self-contained: copy the closest match to your task, then substitute names freely (resources, URLs, database names, columns, template content). **Do not change shape** — the structural choices in each snippet (one `(config){...}` block, single quotes for HTML, parent section wrapping all parent fields, named verb fields, etc.) are required by the rules above.

## #1 — Detail page with children

Show one parent record (`project`) and its children (`tasks`). Parent + child = one domain = one db = two migrations.

```c
#include <mach.h>
#include <sqlite.h>

config mach(){
  return (config){
    .resources = {
      {"project", "/projects/:id",
        .get = {
          validate({"id", .validation = validate_integer,
            .message = "must be an integer"}),
          query(
            {.set_key="project", .db="projects_db",
             .query="select id, name from projects where id = {{id}};"},
            {.set_key="tasks",   .db="projects_db",
             .query="select id, project_id, title from tasks where project_id = {{id}};"}
          ),
          join(.target_table_key="project", .target_field_key="id",
               .nested_table_key="tasks",   .nested_field_key="project_id",
               .target_join_field_key="tasks"),
          render(.template =
            "{{#project}}"
              "<h1>{{name}}</h1>"
              "<ul>{{#tasks}}<li>{{title}}</li>{{/tasks}}</ul>"
            "{{/project}}")
        }
      }
    },
    .databases = {{
      .engine = sqlite_db, .name = "projects_db",
      .connect = "file:projects.db?mode=rwc",
      .migrations = {
        "CREATE TABLE projects (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL);",
        "CREATE TABLE tasks ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "project_id INTEGER NOT NULL REFERENCES projects(id),"
          "title TEXT NOT NULL"
        ");"
      }
    }},
    .modules = {sqlite}
  };
}
```

---

## #2 — Detail page with `find()` (404 if missing)

Same as #1 but for "must exist" lookups. `find()` raises `http_not_found` automatically on zero rows. Add an `.errors` handler to render the 404 page.

```c
.resources = {
  {"todo", "/todos/:id",
    .get = {
      validate({"id", .validation = validate_integer, .message = "must be an integer"}),
      find({.set_key="todo", .db="todos_db",
            .query="select id, title from todos where id = {{id}};"}),
      render(.template = "{{#todo}}<h1>{{title}}</h1>{{/todo}}")
    },
    .errors = {{http_not_found, {render(.template = "<h1>Not found</h1>", .status = http_not_found)}}}
  }
}
```

---

## #3 — List page with concurrent count

List + count fetched concurrently in ONE step.

```c
{"todos", "/todos",
  .get = {
    query(
      {.set_key="todos", .db="todos_db", .query="select id, title from todos order by id desc;"},
      {.set_key="stats", .db="todos_db", .query="select count(*) as n from todos;"}
    ),
    render(.template =
      "<h1>Todos {{#stats}}({{n}}){{/stats}}</h1>"
      "<ul>{{#todos}}<li><a href='{{url:todo:id}}'>{{title}}</a></li>{{/todos}}</ul>")
  }
}
```

Notes:
- `{{n}}` only resolves inside `{{#stats}}` because `n` is a field of the `stats` table.
- Don't try `{{stats.n}}` (dot) or `{{n}}` at root (out of scope).
- Don't use `{{length}}`, `{{count}}`, etc. — compute counts in SQL.

---

## #4 — POST-redirect-GET form with validation

Standard form pattern: GET shows the form, POST validates and either inserts then redirects, or fails and re-enters the GET pipeline with `error:` and `input:` scopes available for repopulation.

```c
{"todos", "/todos",
  .get = {
    query({.set_key="todos", .db="todos_db", .query="select id, title from todos;"}),
    render(.template =
      "<form method='post' action='{{url:todos}}'>"
        "{{csrf:input}}"
        "<input name='title' value='{{input:title}}'>"
        "{{#error:title}}<span class='err'>{{error_message:title}}</span>{{/error:title}}"
        "<button>Add</button>"
      "</form>"
      "<ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul>")
  },
  .post = {
    validate({"title", .validation = validate_not_empty, .message = "Title cannot be empty"}),
    query({.db="todos_db", .query="insert into todos(title) values({{title}});"}),
    redirect("todos")
  },
  .errors = {{http_bad_request, {reroute("todos")}}}
}
```

Notes:
- `validate_not_empty` failure raises `http_bad_request`.
- `reroute("todos")` re-enters the GET pipeline server-side, preserving `input:` and `error:` scopes.
- `{{input:title}}` repopulates the form with what the user typed.
- `{{#error:title}}` is truthy when that specific field has an error; use it as a section.
- `{{csrf:input}}` emits a hidden `<input>` with the CSRF token; verification is automatic.

---

## #5 — Update flow (PATCH)

Edit one field. The `?http_method=patch` parameter lets HTML forms reach PATCH.

```c
{"todo", "/todos/:id",
  .get = {
    validate({"id", .validation = validate_integer}),
    find({.set_key="todo", .db="todos_db", .query="select id, title from todos where id={{id}};"}),
    render(.template =
      "{{#todo}}"
        "<form method='post' action='{{url:todo:id}}?http_method=patch'>"
          "{{csrf:input}}"
          "<input name='title' value='{{input:title}}{{^input:title}}{{title}}{{/input:title}}'>"
          "<button>Save</button>"
        "</form>"
      "{{/todo}}")
  },
  .patch = {
    validate({"id",    .validation = validate_integer},
             {"title", .validation = validate_not_empty, .message = "Title required"}),
    query({.db="todos_db", .query="update todos set title={{title}} where id={{id}};"}),
    redirect("todo:{{id}}")
  },
  .errors = {{http_bad_request, {reroute("todo:{{id}}")}}}
}
```

Notes:
- `{{input:title}}{{^input:title}}{{title}}{{/input:title}}` — show the user's submitted value if present (after a failed validate), otherwise the current DB value.
- `{{^name}}...{{/name}}` is an inverted section: renders when `name` is falsy/empty.

---

## #6 — Delete flow

```c
{"todo", "/todos/:id",
  .delete = {
    validate({"id", .validation = validate_integer}),
    query({.db="todos_db", .query="delete from todos where id={{id}};"}),
    redirect("todos")
  }
}
```

Trigger from HTML: `<form method='post' action='{{url:todo:id}}?http_method=delete'>{{csrf:input}}<button>Delete</button></form>`

---

## #6b — Multi-verb resource with shared validation in `.steps`

When the same resource has GET + PATCH + DELETE (or any combination) and they all need the same `:id` validation, put the shared validation in the `.steps` middleware slot — runs once before every verb, no duplication.

```c
{"todo", "/todos/:id",
  // .steps: shared middleware. Runs before EVERY verb on this resource.
  // Ideal for shared parameter validation, auth checks, session loading.
  {validate({"id", .validation = validate_integer,
    .message = "Project ID must be an integer"})},

  .get = {
    // No need to validate id here — already done in .steps
    find({.set_key="todo", .db="todos_db",
          .query="select id, title from todos where id = {{id}};"}),
    render(.template = "{{#todo}}<h1>{{title}}</h1>{{/todo}}")
  },

  .patch = {
    // id already validated in .steps; only validate verb-specific fields here
    validate({"title", .validation = validate_not_empty, .message = "Title required"}),
    query({.db="todos_db", .query="update todos set title={{title}} where id={{id}};"}),
    redirect("todo:{{id}}")
  },

  .delete = {
    query({.db="todos_db", .query="delete from todos where id={{id}};"}),
    redirect("todos")
  },

  .errors = {
    {http_not_found,    {render(.template="<h1>Not found</h1>", .status=http_not_found)}},
    {http_bad_request,  {reroute("todo:{{id}}")}}
  }
}
```

When to use `.steps` vs verb-pipeline validation:

| Validation type | Where it goes |
|---|---|
| Shared across multiple verbs (`:id` for GET+PATCH+DELETE) | `.steps` |
| Verb-specific (POST validating `title`, PATCH validating new fields) | Inside the verb pipeline |
| Auth/session checks needed by every verb | `.steps` |
| Single-verb resource (e.g., only `.get`) | Inside the verb pipeline (no DRY benefit from `.steps`) |

The third positional slot of a resource entry — the `{...}` after the URL — is `.steps`. It can hold any pipeline steps (validate, exec, session, logged_in, custom), not just auth middleware. It just runs them before whichever verb pipeline dispatches.

---

## #7 — POST that emits a cross-module event

Publisher module declares `.publishes` and calls `emit()`. Subscriber lives in a separate module with its own database (Rule 2).

```c
// todos/todos.c
config todos(){
  return (config){
    .name = "todos",
    .publishes = {{"todo_created", .with = {"user_id", "title"}}},
    .resources = {
      {"todos", "/todos",
        .post = {
          validate({"title", .validation = validate_not_empty},
                   {"user_id", .validation = validate_integer}),
          query({.db="todos_db",
            .query="insert into todos(user_id, title) values({{user_id}}, {{title}});"}),
          emit("todo_created"),
          redirect("todos")
        }
      }
    },
    .databases = {{
      .engine = sqlite_db, .name = "todos_db",
      .connect = "file:todos.db?mode=rwc",
      .migrations = {
        "CREATE TABLE todos (id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "user_id INTEGER NOT NULL, title TEXT NOT NULL);"
      }
    }}
  };
}
```

## #7b — Subscriber module

Different module, **its own database** (Rule 2: one db per module).

```c
// activity/activity.c
config activity(){
  return (config){
    .name = "activity",
    .events = {
      {"todo_created", {
        query({.db="activity_db",
          .query="insert into events(kind, user_id, ref) "
                 "values('todo_created', {{user_id}}, {{title}});"})
      }}
    },
    .databases = {{
      .engine = sqlite_db, .name = "activity_db",
      .connect = "file:activity.db?mode=rwc",
      .migrations = {
        "CREATE TABLE events (id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "kind TEXT NOT NULL, user_id INTEGER NOT NULL, ref TEXT, ts INTEGER DEFAULT (strftime('%s','now')));"
      }
    }}
  };
}
```

Wire it in `main.c`:
```c
#include <mach.h>
#include <sqlite.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config){
    .modules = {todos, activity, sqlite}
  };
}
```

Notes:
- Publisher doesn't know about the subscriber. Adding more subscribers = adding more modules; publisher unchanged.
- Each module owns ONE database. `todos_db` and `activity_db` are different domains.
- `.publishes.with` declares which context keys travel with the event.

---

## #8 — HTMX-aware rendering (fragment vs full page)

Use `.if_context = "is_htmx"` and `.unless_context = "is_htmx"` to send a fragment to HTMX requests and a full page otherwise.

```c
{"todos", "/todos",
  .get = {
    query({.set_key="todos", .db="todos_db", .query="select id, title from todos;"}),
    render(.template = "{{#todos}}<li>{{title}}</li>{{/todos}}",
      .if_context = "is_htmx"),
    render(.template = "<html><body><ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul></body></html>",
      .unless_context = "is_htmx")
  }
}
```

Add `htmx` to `.modules`. The framework sets `is_htmx` automatically when the request has `HX-Request` header.

---

## #9 — External fetch

`fetch()` makes an HTTP request and stores the JSON response (parsed) or text body in context.

```c
{"home", "/",
  .get = {
    fetch("https://api.quotable.io/random", .set_key = "quote"),
    render(.template = "{{#quote}}<blockquote>{{content}} — {{author}}</blockquote>{{/quote}}")
  }
}
```

Multiple `fetch()` items in one step run concurrently (same as `query()`).

POST with body:
```c
fetch("https://api.x.dev/charge",
  .set_key = "receipt",
  .method  = http_post,
  .headers = {{"Authorization", "Bearer {{api_key}}"},
              {"Idempotency-Key", "{{order_id}}"}},
  .json = "order")
```

`.json = "order"` serializes the context table named `order` as the request body.

---

## #10 — Async task triggered by user action

Pipeline calls `task("name")` which enqueues; the pipeline continues immediately. Task body runs on a task reactor.

```c
.resources = {
  {"todos", "/todos",
    .post = {
      validate({"title", .validation = validate_not_empty}),
      query({.db="todos_db", .query="insert into todos(title) values({{title}});"}),
      task("recount_todos"),         // ← enqueue, non-blocking
      redirect("todos")
    }
  }
},
.tasks = {
  {"recount_todos", {
    query({.db="todos_db",
      .query="update stats set todo_count = (select count(*) from todos);"})
  }}
}
```

If the task needs context from the caller, declare `.accepts`:
```c
.tasks = {
  {"send_welcome_email", {
    fetch("https://api.email.dev/send", .method = http_post,
      .json = "user", .headers = {...})
  }, .accepts = {"user_id", "user"}}
}
```

Tasks are durable: crash mid-task → resumes at the failed step on next boot.

---

## #11 — Cron task

Same shape as #10 but with `.cron` instead of (or in addition to) being triggered by `task()`.

```c
.tasks = {
  {"daily_digest", {
    query({.set_key="users", .db="users_db",
      .query="select id, email from users where digest_subscribed = 1;"}),
    fetch("https://api.email.dev/digest", .method = http_post,
      .headers = {{"Authorization", "Bearer {{api_key}}"}},
      .json = "users")
  }, .cron = "0 8 * * *"}     // every day at 08:00
}
```

---

## #12 — Auth-gated resource

`session_auth` provides `session()` and `logged_in()` as middleware steps.

```c
{"dashboard", "/dashboard", {session(), logged_in()},   // ← `.steps` slot — runs before every verb.
                                                         //   Use for any cross-verb middleware:
                                                         //   auth, shared validation, session loading,
                                                         //   etc. See snippet #6b for the shared-
                                                         //   validation pattern.
  .get = {
    query({.set_key="me", .db="users_db",
      .query="select id, name, email from users where id = {{user_id}};"}),
    render(.template = "{{#me}}<h1>Welcome, {{name}}</h1>{{/me}}")
  }
}
```

Login resource:
```c
{"login", "/login",
  .get  = { render(.template =
    "<form method='post' action='{{url:login}}'>"
      "{{csrf:input}}"
      "<input name='email' value='{{input:email}}'>"
      "<input name='password' type='password'>"
      "<button>Sign in</button>"
    "</form>") },
  .post = {
    validate({"email", .validation = validate_email},
             {"password", .validation = validate_not_empty}),
    login(),                  // sets session cookie on success
    redirect("dashboard")
  },
  .errors = {{http_bad_request,    {reroute("login")}},
             {http_not_authorized, {reroute("login")}}}
}
```

Add `session_auth` to `.modules`.

---

## #13 — SSE broadcast channel

Persistent channel that broadcasts events to all connected clients. The `.sse` field on a resource defines the channel; client connects via EventSource.

```c
.resources = {
  // 1) Page that subscribes
  {"todos_live", "/todos/live",
    .get = { render(.template =
      "<ul id='todos'></ul>"
      "<script>"
        "const es = new EventSource('{{url:todos_stream}}');"
        "es.addEventListener('todo_added', e => {"
          "document.getElementById('todos').insertAdjacentHTML('beforeend',"
            "'<li>' + JSON.parse(e.data).title + '</li>'); });"
      "</script>") }
  },

  // 2) The SSE channel (.sse field on a resource — first positional is channel name)
  {"todos_stream", "/todos/stream",
    .sse = {"todos",
      // Optional steps run on connect, e.g. to send the initial state:
      query({.set_key="todos", .db="todos_db", .query="select id, title from todos;"}),
      sse(.event = "initial", .data = {"{{todos}}"})
    }
  },

  // 3) POST broadcasts to the channel
  {"todos", "/todos",
    .post = {
      validate({"title", .validation = validate_not_empty}),
      query({.db="todos_db", .query="insert into todos(title) values({{title}});"}),
      sse(.channel = "todos",
          .event   = "todo_added",
          .data    = {"{\"title\":\"{{title}}\"}"}),
      redirect("todos_live")
    }
  }
}
```

Notes:
- `.sse` is a **field on a resource**, not a standalone resource. The first positional inside `.sse = {...}` is the channel name; subsequent items are steps that run on connect.
- The `sse()` step with `.channel = "todos"` broadcasts to that channel from any other pipeline (e.g. the POST handler).
- An `sse()` step *without* `.channel` sends to the requesting client only — useful for the initial-state push inside `.sse`.

---

## #14 — Datastar partial DOM updates

`ds_sse()` (provided by `datastar` module) sends DOM patches and reactive state updates instead of raw events. Add `datastar` to `.modules`.

```c
.post = {
  validate({"title", .validation = validate_not_empty}),
  query({.db="todos_db", .query="insert into todos(title) values({{title}});"}),
  // After insert, fetch the new row using last_insert_rowid() (SQLite) /
  // currval (Postgres) / LAST_INSERT_ID() (MySQL):
  query({.set_key = "new_todo", .db="todos_db",
    .query = "select id, title from todos where rowid = last_insert_rowid();"}),
  ds_sse("todos",                 // broadcast channel
    .target   = "todo-list",
    .mode     = mode_append,
    .elements = {.template = "{{#new_todo}}<li id='todo-{{id}}'>{{title}}</li>{{/new_todo}}"}),
  redirect("todos")
}
```

Notes:
- The dialect-specific function for "id of last insert" varies: `last_insert_rowid()` (SQLite), `currval('seq_name')` (Postgres), `LAST_INSERT_ID()` (MySQL).
- `.elements = {.template = "..."}` uses an inline template; positional form uses an asset name from `.context`.
- Without `.channel` (just `ds_sse(...)` with no positional), the update goes only to the requesting client.

---

## #15 — Multi-tenant database

`{{interpolation}}` in `.connect` opens a per-tenant database. Connections pool with LRU eviction.

```c
.databases = {{
  .engine  = sqlite_db,
  .name    = "tenant_db",
  .connect = "file:tenants/{{tenant_id}}/data.db?mode=rwc",
  .migrations = {
    "CREATE TABLE projects (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL);"
  }
}}
```

Resource pipeline:
```c
{"projects", "/t/:tenant_id/projects",
  .get = {
    validate({"tenant_id", .validation = validate_slug}),
    query({.set_key="projects", .db="tenant_db",
      .query="select id, name from projects;"}),
    render(.template = "<ul>{{#projects}}<li>{{name}}</li>{{/projects}}</ul>")
  }
}
```

Each `tenant_id` gets its own database file, migrations applied lazily on first access.

---

## #16 — External assets

Once SQL or templates grow large, extract them. Embed at compile time with `(asset){#embed "file"}`. Reference by name from `query()`, `find()`, `render()`. The asset name is **positional**, no `.query`/`.template` field — Rule 5: pick exactly one form per item.

File layout:
```
todos/
├── todos.c
├── todos_list.mustache.html
├── get_todos.sql
└── create_todos_table.sql
```

`todos/todos.c`:
```c
config todos(){
  return (config){
    .name = "todos",
    .resources = {
      {"todos", "/todos",
        .get = {
          query({"get_todos", .set_key="todos", .db="todos_db"}),    // positional asset name
          render("todos_list")                                         // positional asset name
        }
      }
    },
    .context = {
      {"todos_list", (asset){#embed "todos_list.mustache.html"}},
      {"get_todos",  (asset){#embed "get_todos.sql"}}
    },
    .databases = {{
      .engine  = sqlite_db, .name = "todos_db",
      .connect = "file:todos.db?mode=rwc",
      .migrations = {(asset){#embed "create_todos_table.sql"}}
    }}
  };
}
```

`todos/get_todos.sql`:
```sql
select id, title from todos order by id desc;
```

`todos/todos_list.mustache.html`:
```html
<h1>Todos</h1>
<ul>{{#todos}}<li>{{title}}</li>{{/todos}}</ul>
```

If you reference an asset by name (positional) but never embed it in `.context`, the boot will fail — phantom asset. If your snippet has no `.context` block, use inline `.query` and `.template` instead.

---

## Cheat sheet — pipeline shapes by intent

| Intent | Pipeline shape |
|---|---|
| Show one thing | `validate → find/query → render` |
| Show one thing + its children | `validate → query(parent, children) → join → render` |
| Show many things | `query(list, count) → render` |
| Create | `validate → query(insert) → [emit] → [task] → redirect` |
| Update | `validate(id, fields) → query(update) → redirect` |
| Delete | `validate(id) → query(delete) → redirect` |
| External read | `fetch → render` |
| External write | `validate → fetch(POST) → redirect` |
| Auth-gated | `{session(), logged_in()} → verb pipeline` (`.steps` slot for the auth middleware) |
| Shared validation across verbs | `{validate(...)} → .get → .patch → .delete` (`.steps` slot for the shared validation) |
| Live updates | `query(insert) → sse/ds_sse → redirect` (or sse-only resource for the channel) |

---

## What to substitute, what to keep

When copying a snippet, freely substitute:
- Resource names (`project` → `blog`, `order`, `playlist`)
- URL patterns (`/projects/:id` → `/blogs/:id`, `/orders/:id`)
- Database names (`projects_db` → `blog_db`, `commerce_db`)
- Engine (`sqlite_db` → `postgres_db` etc., and update `.modules` to match)
- SQL columns and table names
- Template content

Do NOT change:
- The shape of `(config){...}` (one block, one `};`)
- The number of `.databases` entries for a single domain (one)
- The number of `query()` steps for independent items (one — bundle into items)
- The use of `.get =` / `.post =` named fields (never put pipeline in the third positional slot)
- The use of `'...'` for HTML attributes (never `\"...\"`)
- The wrapping of all parent fields in `{{#parent}}...{{/parent}}` after a join
- The presence of `.modules = {<engine>}` for whichever engine is used

If you need a shape that isn't in this file, the closest snippet is almost always the right starting point. Synthesize from one snippet, not from scratch.
