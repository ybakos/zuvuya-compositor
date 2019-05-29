# Zuvuya

A Wayland compositor providing an information-centric shell.

(c) 2019 Yong Joseph Bakos. All rights reserved.

---

## Journal

Notes that describe the development process & learning.

### Step 1

Let's start with a main.c that just has one function call: `wlr_log_init`.

```
int main(int argc, char** argv) {
  wlr_log_init(WLR_DEBUG, NULL);
  return 0;
}
```

This won't build, because `wlr_log_init` is a function defined in wlroots.
We must add the include directive:

```
#include <wlr/util/log.h>

int main(int argc, char** argv) {
...
```

How do we know this? Let's look at [tinywl](https://github.com/swaywm/wlroots/tree/master/tinywl), a simple compositor that relies on wlroots.
It's `main` calls `wlr_log_init`, but we can see a bunch of `#include`
directives. Digging into those header files, we eventually find `
wlr_log_init` declared in _log.h_. `wlr_log_init` initializes the wlroots
logging functions, by specifying the logging level, such as `WLR_DEBUG` or
`WLR_ERROR`, all of which are `enum` types declared in _log.h_. The
second parameter to `wlr_log_init` is a function pointer, conforming to
the type `wlr_log_func_t`, that lets you control what happens when your
code, or wlroots code, calls `wlr_log`, `wlr_vlog`, or `wlr_log_errno`.
Code should call these functions whenever it needs to log something.
The type `wlr_log_func_t` is a function that accepts three arguments: an
`enum wlr_log_importance`, like `WLR_DEBUG`, a format string, like
`"[%s:%d] "`, and a `va_list` of arguments. If you pass `NULL` as the
second parameter to `wlr_log_init`, as we're doing right now in `main`,
wlroots will use its own default behavior, which is defined in _log.c_,
as `log_std_error`.

We can witness the default behavior by logging an exit message in `main`,

```
wlr_log(WLR_DEBUG, "Foolicious");
```

If we could compile this, you'd see that `wlr_log` decorates the log message
with a date, time, source file and line number.

To compile, let's use meson, and bring in the wlroots source code as a
subproject. Just clone wlroots somewhere, make sure you can compile it,
by following the instructions in the _README_ of the wlroots repo.
Next, symlink the location of wlroots from the subprojects subdirectory.
Following the instructions on the wlroots [Getting started](https://github.com/swaywm/wlroots/wiki/Getting-started) doc leads us to
a _meson.build_ file for our project that looks like this:

```
project('zuvuya-compositor', 'c')

wlroots_project = subproject('wlroots')
lib_wlroots = wlroots_project.get_variable('wlroots')

executable('main',
  'main.c',
  dependencies: [lib_wlroots]
)
```

This tells meson about the name of your project and how to build the
executable. Notice how the dependency object, `lib_wlroots`, is used as
a configured dependency in the `executable` declaration within the build
file.

### Step 2

The first thing that the compositor needs is a `wl_display`. A `wl_display`
is defined by the Wayland protocol Server API, and you can think of it
as the core "server part" of the compositor TODO. We'll dig into the
`wl_display` API more later. For now, let's just create one and destroy it.

```
struct wl_display* display = wl_display_create();

wl_display_destroy_clients(display);
wl_display_destroy(display);
```

Notice that there are two steps to destroying a `wl_display`: destroying
client connections (TODO: or just clients?), and destroying the `wl_display`
itself. This is merely reflecting the common practice of destroying things
dependent on a resource before destroying the resource itself.

When we compile this, we should see a warning regarding the `wl_display_*`
functions we're calling, which lack explicit prototypes. This is historically
allowable in C and has changed in more recent C standards. We can configure the
compiler to stop compilation when it sees a function call that lacks any
prototype by using a compiler flag like `-Werror`. What is more interesting is
that the program links fine and runs, even though we have no explicit declaration of the function prototypes nor their implementations. This means
that, somewhere in our project, there must be an implementation of these
`wl_display_*` functions we're calling. Where? From wlroots.

The wlroots build configuration lists `wayland-server` as a dependency, which
means we have the wayland-server library available in our build. So during
linking, the linker is satisfied and, during execution, the invocation of
`wl_display_create` works just fine.

The _wayland-server.h_ header declares these function prototypes, so let's
include this header in _main.c_.

```
#include <wayland-server.h>
...
```

Now when we compile, we see no warnings. We won't add wayland-server as a
build depenency, since wlroots will bring it into our build.

Again, we can run our compositor at this point, and it configures a log,
instantiates a `wl_display` object, destroys it, logs a message, and exits.

But what the heck is _wayland-server.h_ and where does it come from? What
is Wayland, anyway?

TODO: explanation.

### Step 3

TODO: wlr_backend. What is it, why do we need it, how to get one.

Why do we need to configure our build with WLR_USE_UNSTABLE ? (We get a
compilation error if we don't.)

### Step 4

Starting the backend and running the display. A window appears!
