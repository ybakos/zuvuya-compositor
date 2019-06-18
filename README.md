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

Now let's take a step back and think about what the client needs to do in
order to get a window to appear on the screen. When a standalone program
wants to draw graphics to the screen, it writes data to the framebuffer,
which is the part of video RAM that represents the pixels of a physical
screen. Our typical graphical environments don't consist of a single program
drawing to the screen. Rather, each program appears in a "window", whose
visual content is stored in a chunk of memory serving as a canvas of pixels.

It is the job of a compositor to combine the contents of those chunks of
memory owned by each individual program and send that composition of data to
the framebuffer. This means that each program, running in its own process,
must have a way of obtaining a chunk of memory to draw in, share that memory
with the compositor, and must be able to inform the compositor that
its "window" is ready to be composited. In addition, the client must somehow
be made aware of input events, such as a mouse click.

Let's see what it takes to create a program that can talk to a Wayland
compositor, and circle back to our server, to see what it takes to
communicate with these client programs.

### Step 1

Let's add a the file _client.c_ to our project containing a simple stub for
`main`:

```
#include <stdlib.h>

int main(int argc, char** argv) {
  return EXIT_SUCCESS;
}
```

We should also configure our build settings with a new `executable` target:

```
executable('client', 'client.c')
```

Consider our primary goal of displaying some graphics on the screen. In
order to do this, we need some memory to draw on - but that memory must also
be available to the server so that the compositor can do its job. One way of
doing this involves asking the operating system to allocate this shared memory
using the standard POSIX shared memory API.

First, let's determine the size of our client's "window," which will inform
how much memory we need.

```
// ...
static const int WIDTH = 300;
static const int HEIGHT = 300;
static const in MEMORY_SIZE = 4 * WIDTH * HEIGHT;
```

Notice that `MEMORY_SIZE` is not just `WIDTH * HEIGHT`. While `WIDTH`
and `HEIGHT` specifies our dimensions in pixels, each pixel consists of four
bytes (32 bits). Because we'll be requesting the amount of memory we need in
bytes, the total number of bytes we want is four bytes * the number of pixels.

Ok, we know the size of the memory we need. Now, let's create the
second thing we need: the memory itself. Using the POSIX shared memory API,
the handle for the memory will be represented as a file descriptor, or "fd"
for short. We're going to take a shortcut here, and use a simple abstraction
we'll borrow from someone else, letting us obtain the fd in just one
statement:

```
int fd = create_shm_file(MEMORY_SIZE);
```

Under the hood, the `create_shm_file` function uses `shm_open` to create and
open a POSIX shared memory object. [A POSIX shared memory object is in effect a handle which can be used by unrelated processes to mmap(2) the same region of shared memory](http://man7.org/linux/man-pages/man3/shm_open.3.html).
This is exactly what we want our "unrelated" server and client processes to be
able to do.

I'm providing _shm.h_ and _shm.c_ here in this repo already, which I've
borrowed from [emersion](https://github.com/emersion/hello-wayland). To get
this to build, you'll want to include the header,

```
#include <shm.h>
```

and modify your _meson.build_ configuration:

```
rt = meson.get_compiler('c').find_library('rt') # For open_shm, etc
executable('client', ['client.c', 'shm.c'], dependencies: [rt])
```

Since we have some memory, let's write some data to it, thereby painting it
with color data. Although we have an `fd` for the shared memory, this doesn't
provide us an API to write data to the shared memory. To get this, we'll use
`mmap`.

```
#include <sys/mman.h>
// ...
void * canvas = mmap(NULL, shm_size(), PROT_READ | PROT_WRITE, MAP_SHARED,
  fd, 0);
```

You should [look up what `mmap` does](http://man7.org/linux/man-pages//man2/munmap.2.html), but in the end we now have a pointer in our
program that represents the base address of the shared memory. Let's paint!

```
#include <stdint.h>
// ...
uint32_t* pixel = canvas;
for (int i = 0; i < WIDTH * HEIGHT; ++i) {
  *pixel++ = 0x6600ff; // Purple
}
```

Our client has now obtained some shared memory, and painted to it. But how do
we get this to appear on the screen? It takes multiple steps, that will
include connecting to a Wayland _server_, obtaining a remote handle on the
_compositor_, wrapping our memory in a _buffer_, attaching the buffer to a
_surface_, and asking the compositor to incorporate the contents of the
surface.

Let's start with connecting to the server.

### Step 2

The very first thing a client needs to do is connect to a Wayland display
server ("compositor"), running in its own separate process. We can obtain a
connection to the server with the function `wl_display_connect`. It takes one
argument: the identifying name of the server. If we pass it `NULL`, it will
attempt to determine the name of the server via the environment variable
`$WAYLAND_DISPLAY`.

Our basic client might look like this:

```
// ...
#include <wayland-client.h>

int main(int argc, char** argv) {
  // ...
  struct wl_display* display = wl_display_connect(NULL);

  wl_display_disconnect(display);
  return EXIT_SUCCESS;
}
```

This program establishes a connection to the display server, and then closes
that connection.

The first goal of the client is to provide the server with access to
the shared memory the client had been drawing to, so that this "canvas" can
become part of the whole composited output driven by the server. To
accomplish this, the client needs to start talking to the server through
client-side proxies that correspond to their server-side counterparts. Wayland
is the wire between these client proxies and the server counterparts. Our
client will send requests to the server, the server will send events to the
client, and the client will use event handlers to specify what to do in
response to the events it cares about.

The initial, or "global," objects available to the client are enumerated in a
"registry."

Let's see what it takes to create a registry, so we can use it to get other
objects.

```
// ...
#include <wayland-client-protocol.h>
// ...
struct wl_registry* registry = wl_display_get_registry(display);

wl_registry_destroy(registry);
// ...
```

Notice how we're including `wayland-client-protocol.h`. The function
`wl_display_get_registry` corresponds to the Wayland protocol request
`wl_display.get_registry`. This is indeed the first request sent from the
client to the server. Now, if you read the protocol documentation for
`get_registry`, you'll see that the next thing that happens is that the
registry will automatically emit one event for each "global" object that the
client has access to. It is "advertising" the availability of global objects
that the client can obtain access to. If we bind an event handler to the
registry, our client can do something upon receiving these events, such as
obtaining a handle on the compositor object.

Let's see what the initial events are by printing them to the console. We'll
need to create a `wl_registry_listener` and add the listener to our
`wl_registry` object. A `wl_registry_listener` is just a struct that has two
function pointers. One function pointer is named `global`, and the other is
named `global_remove`. These correspond directly with the names of the
protocol events `wl_registry.global` and `wl_registry.global_remove`. In
other words, when the server emits a `wl_registry.global` event, our client's
`global` event handling function will be invoked.

```
#include <stdio.h>
// ...
static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s id %d version %d\n", interface,
    name, version);
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
    uint32_t name) {
  printf("Got a registry.remove event for id %d\n", name);
}

static const struct wl_registry_listener registry_listener = {
  .global = registry_handle_global,
  .global_remove = registry_handle_global_remove
};

int main(int argc, char** argv) {
// ...
  wl_registry_add_listener(registry, &registry_listener, NULL);
```

If we build and run this, it won't appear to do anything. It may seem that
this is because our program is exiting before it has received any events from
the server. However, in fact, the server never even receives the
`display.get_registry` request. You can prove this by putting a `sleep(5)`
call before the call to `wl_display_disconnect`. To understand why the
server never even receives the request, let's investigate the request and
response machinery of the Wayland protocol and its API.

### Step 3

Communication between a Wayland client and server is asynchronous. If this
isn't familiar, you'll need to educate yourself a little about asynchronous
communication, event loops, and message queues. When we invoke
`wl_display_get_registry`, the client does not immediately send the server
a request. Instead, `wl_display_get_registry` adds the `display.get_registry`
request to a buffer (queue) of outbound requests that are ready to be sent
to the server, and then it returns.

In order to send the request to the server, our client needs to flush its
request buffer, resulting in the requests actually being sent to the server.
We could invoke `wl_display_flush`, then wait in a loop until we see the
initial registry events, sent from the server, arrive in the client event
queue. We could then process, or "dispatch," the events in the queue, which
would fire our registry event handlers. Instead, we could invoke
`wl_display_dispatch`, which would do a few things for us:

* Flush the request buffer, sending all requests to the server
* Wait until there are events in the queue
* Dispatch each event in the event queue

However, there's a little uncertainty here. How would we know that the last
event in the event queue is the final event that the server has to send us,
in response to a particular client request? It would be great if we could
send a 'DONE?' request to the server, and it would respond with a 'YEP, DONE'
event. If only we could send such a request after our initial
`wl_display.get_registry` request. We would then know that, once we see the
'YEP, DONE' event in the event queue, that all of the events resulting from
our `wl_display.get_registry` request had been received, since
they would be enqueued before our 'YEP, DONE' event.

The `wl_display.sync` event does exactly this, and can be added to the request
buffer with the function `wl_display_sync(display)`. It even returns a
callback object, to which you can bind an event handler to specify what you
want to happen when the sync request/event roundtrip is complete.

So, to recap, we need to:

* Enqueue a `wl_display.get_registry` request
* Enqueue a `wl_display.sync` request
* Flush the request buffer, sending the two requests to the server
* Wait until there are events in the queue
* Dispatch each event in the event queue
* Listen for a `wl_callback.done` event, so we know that we received all of
  the `wl_registry.global` events

The function `wl_display_roundtrip` does this work for us. It enqueus the
`wl_display.sync` request, uses `wl_display_dispatch` to flush the buffer
and dispatch any events that arrive in the event queue, and it repeats this
until it "sees" the `wl_callback.done` event.

Let's do all this work with one statement:

```
wl_display_roundtrip(display);
```

If you run and build now, you'll see our registry `global` event handler
get invoked, printing the information about each global object. We've now
completed one request / event workflow: the client prepared a request, sent
it to the server, waited until events were in the queue, and dispatched each
event in the queue, allowing our event handlers to handle each event. To
do this, we used `wl_display_roundtrip` to help ensure that all of the
events that we care about have indeed been received.

Q: Why does wl_registry_get_version return 0, even after I've done a roundtrip?
Q: What are the two extra events from roundtrip? One is done, what is the second?

### Step 4

Recall that our goal is to somehow share the client's "canvas" of memory
with the server. The Wayland protocol provides the `wl_buffer`
abstraction that encapsulates the chunk of memory that the client can draw
on, and that the compositor can, somehow, access.

In order to create a `struct wl_buffer`, we need a `struct wl_shm_pool`.
The pool encapsulates the memory shared between the server and the client,
from which we can obtain `struct wl_buffer` objects. In order to create a
`struct wl_shm_pool`, we need to invoke `wl_shm_create_pool`. In turn, this
function expects three arguments: a `struct wl_shm` object, the file
descriptor for the shared memory, and the size of that memory. We already have
the fd and the size, so let's obtain the `wl_shm` object, then create the
pool, and use the pool to create the buffer.

The `wl_shm` object is one of the core Wayland globals provided by the server
and advertised by the registry. To get a handle on it, we need to declare a
variable for it, and enhance our `wl_registry.global` event handler.

```
#include <string.h>
// ...
static const int WL_SHM_INTERFACE_VERSION = 1;
struct wl_shm *shm;
// ...
static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, WL_SHM_INTERFACE_VERSION);
  }
}
```

Remember, our event handler will be invoked once for every
`wl_registry.global` event. But right now we only care about the one event
advertising the existence of `wl_shm`. When the handler gets invoked and the
`interface` is `wl_shm`, we'll

TODO (explain bind, interfaces, etc)

We now have all three arguments we need to obtain our `wl_shm_pool`, and can
invoke `wl_shm_create_pool`. We can then obtain our `wl_buffer`, as well.

```
// ...
static const int STRIDE = 4 * WIDTH;
static const int MEMORY_SIZE = STRIDE * HEIGHT;
// ...
struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, MEMORY_SIZE);
struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT,
  STRIDE, WL_SHM_FORMAT_ARGB8888);

wl_buffer_destroy(buffer);
wl_shm_pool_destroy(pool);
// ...
```

The _stride_ of a graphics buffer represents the number of bytes
in one _row_ of a with * height sized buffer. This is one example of how
a `wl_buffer` provides a more powerful abstraction than a raw chunk of memory. By knowing the stride, it knows that the second row of pixels in our
window starts at byte 1200, and that the third row starts at byte 2400,
and so on. You should investigate [this concept of stride in graphics](https://docs.microsoft.com/en-us/windows/desktop/medfound/image-stride).

The second argument of `wl_shm_pool_create_buffer` is an offset, indicating
our desired starting location of the buffer within the pool. There is more
memory in the pool than is in the buffer, and we can obtain multiple buffers
starting at different offsets. In this case, we'll have just one buffer, and
it begins where the pool begins, with an offset of 0. The last argument
specifies the "pixel format" of the buffer. We're just dumping bits into our
shared memory, and the pixel format indicates the meaning of groups of bits.
For example, the first byte in every four-byte clump might represent the red
value of the pixel, or three bits might represent alpha transparency. There
are [all kinds of pixel formats out there](https://github.com/torvalds/linux/blob/master/include/uapi/drm/drm_fourcc.h).

### Step 5

All right, we've got our `wl_buffer` object, and the next step is to 'attach'
our buffer to a `wl_surface`. In order to get a surface, our client will need
a handle on the server compositor.

The compositor object is another server-side "global", advertised by the
registry. To get our handle, we'll want to modify our `wl_registry.global`
event handler such that, when we see the "compositor" global advertised, we
can get our handle on the compositor.

Let's add a global variable for a `struct wl_compositor *` and modify the
`registry_handle_global` implementation to initialize this new compositor
variable.

```
// ...
static const int WL_COMPOSITOR_INTERFACE_VERSION = 4;
struct wl_compositor *compositor;

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s, id %d, version %d\n",
    interface, name, version);
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, WL_SHM_INTERFACE_VERSION);
  } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
      WL_COMPOSITOR_INTERFACE_VERSION);
  }
}

```

And in `main`, let's check to be sure that `compositor` isn't `NULL` before
continuing. We'll do a little explicit cleanup before the program exits, too.

```
  // ...
  if (compositor == NULL) {
    fprintf(stderr, "no wl_compositor obtained\n");
    return EXIT_FAILURE;
  }

  wl_compositor_destroy(compositor);
  wl_registry_destroy(registry);
  // ...
```

All right, we've got a compositor. It's important to note that, while we
describe Wayland display servers as "compositors", we're using that term at a
high level. We call Wayland display servers "compositors" because, unlike
other architectures, a Wayland display server is also responsible for doing
the compositing. In a more specific sense, the `wl_compositor` is the
component within our "compositor" that is truly responsible for combining the
contents of multiple surfaces into a single displayable output (the
framebuffer).

But then, what is a "surface?"

### Step 6

A Wayland compositor does not manage windows, it manages surfaces. A _surface_
is the primary abstraction that "wraps" a graphics buffer that clients draw
to and that the compositor composites. Visually, you can think of a surface as
a traditional "window," but this is entirely up to the display server. For
example, the server could take a surface and paint it onto a three dimensional
sphere. In addition, protocol extensions like xdg-shell may provide additional
wrappers around the core Wayland surface, similar to subclassing or object
composition.

We've got a buffer, so lets obtain a surface from the compositor and attach
the buffer to it.

```
struct wl_surface* surface = wl_compositor_create_surface(compositor);
wl_surface_attach(surface, buffer, 0, 0);

wl_surface_destroy(surface);
// ...
```

TODO: explain. Dig into the bare minimums, including server-side.
TODO: get server to render plain ol wl_surface.

Returning to client, with motivation for xdg shell.

### Step 7

If our client is to create `xdg_surface` objects, we'll need to obtain an
`xdg_wm_base` object and invoke `xdg_wm_base_get_xdg_surface`. From the documentation:

The `xdg_wm_base` interface is exposed as a global object enabling clients
to turn their wl_surfaces into windows in a desktop environment. It
defines the basic functionality needed for clients and the compositor to
create windows that can be dragged, resized, maximized, etc, as well as
creating transient windows such as popup menus.

The`xdg_wm_base` is a global advertised by the registry, so lets add a global
`xdg_wm_base` variable and enhance our registry `global` event handler to
grab a hold of the object when the corresponding event is fired.

```
// ...
static const int XDG_WM_BASE_INTERFACE_VERSION = 2;
struct xdg_wm_base *xdg_wm_base;
// ...
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface,
      XDG_WM_BASE_INTERFACE_VERSION);
  }
}
```

And down in `main`,

```
// ...
xdg_wm_base_destroy(xdg_wm_base);
// ...
```

If you build this, we'll get a failure due to some undefined types defined
in a header file for the xdg-shell client protocol. Let's include it.

```
#include <xdg-shell-client-protocol.h>
```

And if you build this, we'll get a different kind of failure, notifying us
that the header file doesn't exist. Furthermore, this header file does not
really exist anywhere, such as in a library or package installation. We must
generate this header file using two tools: _wayland-scanner_ and
_xdg-shell.xml_, the xdg-shell protocol specification xml file.

While we've been thinking of Wayland as a library, Wayland itself is only a
protocol, documented by its creators and contributors in an XML file. We have
actually been using a defacto C library that provides an API around the
Wayland protocol. When we "build and install Wayland," we are building and
installing a C library that presents us with an API for the Wayland protocol.
There are, in fact, "missing" header files in the Wayland source tree. These
header files are generated during the build step, and the build script uses
an executable utility, known as _wayland-scanner_, that parses the protocol
XML and generates the header and source files for our API.

While we get this "for free" when building and installing Wayland, we are not
as lucky with other protocols, such as xdg-shell. This and other protocols
exist in the separate _wayland-protocols_ repository. When we build and
"install" the protocols, we are merely copying the various protocol xml files
to, say, _/usr/share/wayland-protocols/_.

To do this, we could manually run two commands:

```
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml ./xdg-shell-client-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml ./xdg-shell-client-protocol.c
```

These two commands would generate the _xdg-shell-client-protocol.h_ header and
the _xdg-shell.c_ source file, storing them locally in our own source tree. We
could do this and move on, but let's automate this step by enhancing our build
configuration.

```
cmd = run_command('pkg-config', 'wayland-protocols', '--variable=pkgdatadir')
if (cmd.returncode() != 0)
  message('pkg-config could not find a path to wayland-scanner')
  error(cmd.stderr())
endif
XDG_SHELL_PROTOCOL_XML_PATH = cmd.stdout().strip() + '/stable/xdg-shell/xdg-shell.xml'

cmd = run_command('pkg-config', '--variable=wayland_scanner', 'wayland-scanner')
if (cmd.returncode() != 0)
  message('pkg-config could not find a path to wayland-scanner')
  error(cmd.stderr())
endif
WAYLAND_SCANNER = cmd.stdout().strip()

cmd = run_command(WAYLAND_SCANNER,
  'client-header',
  XDG_SHELL_PROTOCOL_XML_PATH,
  'xdg-shell-client-protocol.h'
)
if (cmd.returncode() != 0)
  message('Wayland scanner could not generate the xdg xhell header file.')
  error(cmd.stderr())
endif

cmd = run_command(WAYLAND_SCANNER, 'private-code', XDG_SHELL_PROTOCOL_XML_PATH, 'xdg-shell-client-protocol.c')
if (cmd.returncode() != 0)
  message('Wayland scanner could not generate the xdg xhell source file.')
  error(cmd.stderr())
endif

executable('client',
  ['client.c', 'shm.c', 'xdg-shell-client-protocol.c'],
  dependencies: [lib_wayland, rt]
)

```

We can now run `meson build --reconfigure` and `ninja -C build`, and our
toolchain will generate the necessary xdg-shell header and source files. We
also are including the _xdg-shell-client-protocol.c_ as one of the compilation
units in our `executable` config.

TODO: talk about interfaces, proxies, etc. How it all works.

Now that we have our `xdg_wm_base` object, and we can return to our task of
creating an `xdg_surface`.

### Step 8

Let's obtain an `xdg_surface`:

```
struct xdg_surface* xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);

xdg_surface_destroy(xdg_surface);
```

Here we are instantiating an `xdg_surface`, by passing the factory function
`xdg_wm_base_get_xdg_surface` our `wl_surface` object. The `xdg_surface`
essentially wraps the `wl_surface`, providing a base set of functionality
required to construct user interface elements requiring management by the
compositor, such as windows, menus, and toolbars. The types of functionality
are split into xdg_surface _roles_.

However, creating an `xdg_surface` does not set the role for a `wl_surface`.
In order to really have an `xdg_surface`, the client must create a
role-specific object. So we need to "wrap" our `xdg_surface` in more specific
surface, which will provide our `wl_surface` a role.

The kind of surface we'll use is an `xdg_toplevel` surface. This enables the
surface surface to set window-like properties such as maximize, fullscreen,
and minimize, and set application-specific metadata like title and id.

```
// ...
struct xdg_toplevel* toplevel = xdg_surface_get_toplevel(xdg_surface);

xdg_toplevel_destroy(toplevel);
```

We now have a toplevel surface with enough information to enable the
compositor to display it. But how do we get the compositor to display our
window?

### Step 9

The compositor is responsible for rendering the contents of all of its client
surfaces. But how does it know when to do this? If it alone continuously
rendered the buffer for each surface, it would either be wasting its time
when the contents of the buffer haven't changed, or, have no way of being
"in sync" with the client, which will be updating its buffer contents
independently in its own process.

Furthermore, the compositor uses a concept known as double-buffering. The
first buffer is the "current" one, rendered by the compositor. The second
buffer is the "pending" one, which clients can draw on and otherwise modify
its state.

To inform a compositor to make the pending buffer the current one, and then
render the contents of the new current buffer, a surface must communicate a
"commit." Let's go ahead and try this out now:

```
wl_surface_commit(surface);
wl_display_roundtrip(display);
```

When we build and run this, we'll see an error is logged, saying something
about the surface never having been configured. We should recognize that
mapping our `wl_surface` with `xdg_surface` and `xdg_toplevel` results in
the compositor sending the client a `configure` event. This is the compositor
telling the client, ok, get your surface ready for display, and then send
me an acknowledgement letting me know that you're done.

Our client needs to listen for `configure` events, and send an
acknowledgement. Let's define an event handler.

```
static void xdg_surface_handle_configure(void *data,
    struct xdg_surface *xdg_surface, uint32_t serial) {
  xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
  .configure = xdg_surface_handle_configure,
};
```

Next, let's add the event handler to the xdg_surface

```
xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
```

Here we are defining an `xdg_surface_listener`, and adding the listener to
our `xdg_surface`. Our `configure` handler will enqueue an `ack_configure`
request to be sent to the server, which will let the server know that our
surface has been configured and is ready for rendering.

But, when we run this, we still see the same error.

### Step 10

The reason we're seeing the error is due to an order of events. We should
not be attaching the buffer to the surface until after the initial configure
event. To fix this, let's move our prior `wl_surface_attach` invocation down
after the initial commit and roundtrip. The entire sequence of surface
creation and configuration should like this:

```
 struct wl_surface* surface = wl_compositor_create_surface(compositor);
 struct xdg_surface* xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base,
     surface);
 struct xdg_toplevel *xdg_toplevel= xdg_surface_get_toplevel(xdg_surface);

 xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

 wl_surface_commit(surface);
 wl_display_roundtrip(display);

 wl_surface_attach(surface, buffer, 0, 0);
```

Now that we've attached the buffer to the surface here, let's send a `commit`.
Remember, if we don't send the commit, the compositor will still be displaying
a current buffer, without our `wl_buffer` attached, while our intended
`wl_buffer` is attached to the pending buffer. (TODO: revise).

```
wl_surface_commit(surface);
```

Next, let's be sure these queued requests are finally sent to the server:

```
wl_display_roundtrip(display);
```

When you build and run now you should see, if you stay alert, our window with
its surface buffer contents appear and then disappear as our program exits.

To keep things running, we're going to need a loop. One idiomatic way of
doing this is to call `wl_display_dispatch` over and over again until it
receives a signal of `-1`. Let's remove our previous `wl_display_roundtrip`
call and replace it with this loop:

```
while (wl_display_dispatch(display) != -1) {
  // stare at the screen
}
```

This loop continuously flushes the client request queue and dispatches all
the events in the event queue. The function blocks while the event queue is
empty, too, waiting for new events to arrive.

If you build and run now, you should see a window appear with the contents
of our surface's buffer in it - a 300 x 300 purple square.

## Back to the Server

Our client runs, but connects to the Wayland compositor that we are currently
running. For example, if we're running Sway as our desktop environment, our
client connects to Sway, and displays a tiled window within our desktop
environment. What we'd like to do is run our server, and have our client connect
to _it_, instead of our main desktop compositor.

We're going to run our server, which will itself be displayed within our
desktop environment, and our client will connect to our server, which will
display the client contents. Client -> our server -> main desktop server.

TODO: Why does running our compositor automatically connect to the main desktop
compositor? How does it know to do this? Does it check for a WAYLAND_DISPLAY
first and connect to it if it exists?

How will our client know what server to connect to? In the Wayland environment,
each running compositor defines an environment variable `WAYLAND_DISPLAY`, which
stores an identifier like `wayland-0`. This is the default socket identifier for
most Wayland compositors. So then, what is the socket identifier for our server?

Well, it doesn't have a socket that it is listening to yet. So in fact, no
client can connect to it. Let's use the Wayland API to create a socket and bind
it to our display. In addition, let's log a message with the socket info and see
what it says.

```
const char *socket = wl_display_add_socket_auto(display);
wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
```

If we build and run our server, it should still appear as a window within our
main desktop compositor. When we inspect the log messages printed on the
console, we see the following message:

**[main.c: 18] Running Wayland compositor on WAYLAND_DISPLAY=wayland-1***

Aha, that is the identifying name for the socket on which our server is
listening, and we can use this information to tell our client to connect to
our server. To do so, let's redefine the `WAYLAND_DISPLAY` environment variable
when running our client. For example:

```
WAYLAND_DISPLAY=wayland-1 build/client
```

When we run this, without our server running, the client exits with a message
telling us that it failed to create a display. Now, let's run our server first,
then run the client.

This time, we see that our client logs some information per its registry
event handler, but then crashes. Let's investigate why, and see if this leads us
to understand what the server needs to support for its clients.

### Step 1

TODO: comment out client lines until we find the cause of the crash.



---

TODO going deeper:
- wl_registry_bind, and interfaces (eg wl_shm_interface), proxies, etc

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

(c) 2019 Yong Joseph Bakos. All rights reserved.
