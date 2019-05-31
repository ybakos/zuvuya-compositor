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
that the program links fine and runs, even though we have no explicit
declaration of the function prototypes nor their implementations. This means
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

### Step 5

Initializing the display with a renderer.

### Step 6

Initialize the compositor and data device manager.


## Client I

Now let's take a step back and think about what the client needs to do in order
to get a window to appear on the screen. When a standalone program wants to
draw graphics to the screen, it writes data to the framebuffer, which is the
part of video RAM that represents the pixels of a physical screen. Our typical
graphical environments don't consist of a single program drawing to the screen.
Rather, each program draws its contents in a "window", which itself is
represented as a chunk of memory.

It is the job of a compositor to provide those chunks of memory to each
individual program, then combine the contents of those chunks of memory
together and send that composition of data to the framebuffer. This means that
each program, running in its own process, must have a way of requesting a chunk
of memory from the compositor, and must be able to inform the compositor that
its "window" is ready to be composited. In addition, the client must somehow be
made aware of input events, such as a mouse click.

Let's see what it takes to create a program that can talk to a Wayland
compositor, and circle back to our compositor, to see what it takes to
communicate with these client programs.

# Step 1

The very first thing a client needs to do is connect to a Wayland compositor,
which is a server running in its own process. We can obtain a connection to
the server with the function `wl_display_connect`. It takes one argument: the
identifying name of the compositor. If we pass it `NULL`, it will attempt to
determine the name of the compositor via the environment variable
`$WAYLAND_DISPLAY`.

Our basic client might look like this:

```
#include <wayland-client.h>

int main(int argc, char** argv) {
  struct wl_display* display = wl_display_connect(NULL);

  wl_display_disconnect(display);
  return 0;
}

```

This program establishes a connection to the display server / compositor, and
then closes that connection.

Now, the goal of the client is to obtain some memory to draw to, and in order
for that memory to become part of the whole composited output, the client
needs to use memory that the compositor provides. In order to get this memory,
the client needs to start talking to the server through client-side proxies
that correspond to their server-side counterparts. The initial, and "global,"
objects available to the client are enumerated in a "registry."

Let's see what it takes to create a registry, so we can use it to get other
objects.

```
// ...
#include <wayland-client-protocol.h>
// ...

struct wl_registry* registry = wl_display_get_registry(display);
// ...
```





---

# Old drafted content

If you grew up in a graphical world, you probably think of the operating system
as "the desktop environment". After all, it's what we see when we talk about
Windows, MacOS, or Linux on the desktop for personal computing. We know, however,
that the OS is the suite of processes responsible for abstracting hardware. After
the OS finished initializing, is usually starts one more program that allows a
user to interact with the system. That program is the shell.

If you grew up prior to the age of modern desktop environment GUIs, you will
recall that the OS essentially drops you off at a text-based console. What the
OS did was it set the graphics mode of the display hardware to, say, "text mode"
and did the next most simple thing: started a text-based, command-line shell.

The shell allows you to execute programs, and not just text-based programs, but
graphical ones. What is the minimum amount of work we can do to create a program
that displays graphics on the screen?

# Step 0

The Linux operating system provides a resource known as the "frame buffer."
You can think of it as a canvas that we can paint on, or a rectangle of pixels.
The frame buffer is a part of RAM that is essentially mapped to the pixels of
your physical display. You can look up "frame buffer" for more information on
this, but know that if we set values in this memory, the screen will reflect
that as particular colors at particular pixel locations.

There are two quick ways to draw to the frame buffer. The first is by writing
to `/dev/fb0` which doesn't even require a program.

```
$ cat /dev/urandom > /dev/fb0
```

This writes a [continuous stream of random values](https://en.wikipedia.org/wiki//dev/random) to the framebuffer.

We can also write a program that uses the frame buffer API. One such program
might look like this:

```

```

