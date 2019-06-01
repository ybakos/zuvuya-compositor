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

It is the job of a compositor to combine the contents of those chunks of
memory owned by each individual program and send that composition of data to
the framebuffer. This means that each program, running in its own process, must have a way of obtaining a chunk of memory to draw in, share that memory
with the compositor, and must be able to inform the compositor that
its "window" is ready to be composited. In addition, the client must somehow
be made aware of input events, such as a mouse click.

Let's see what it takes to create a program that can talk to a Wayland
compositor, and circle back to our compositor, to see what it takes to
communicate with these client programs.

TODO: meson.build changes (adding lib_wayland client dependency, compilation target, etc)

### Step 1

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

Notice how we're including `wayland-client-protocol.h`. The function
`wl_display_get_registry` corresponds to the Wayland protocol message
`display.get_registry`. This is indeed the first request sent from the client
to the server. Now, if you read the protocol documentation for `get_registry`,
you'll see that the next thing that happens is that the registry will
automatically emit one event for each "global" object that the client has
access to. It is "advertising" the availability of global objects that the
client can obtain access to. If we bind an event handler to the registry, our
client can do something upon receiving these events, such as obtaining a
handle on the compositor object.

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
  printf("Got a registry.global event for %s id %d\n", interface, name);
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

### Step 2

Communication between a Wayland client and server is asynchronous. If this
isn't familiar, you'll need to educate yourself a little about asynchronous
communication, event loops, and message queues. When we invoke
`wl_display_get_registry`, the client does not immediately send the server
a request. Instead, `wl_display_get_registry` adds the `display.get_registry`
request to a buffer (queue?) of outbound requests that are ready to be sent
to the server, and then it returns.

In order to send the request to the server, our client needs to flush its
request buffer, resulting in the requests actually being sent to the server.
We could invoke `wl_display_flush`, then wait in a loop until we see the
initial registry events, sent from the server, arrive in the client event
queue. We could then process, or "dispatch," the events in the queue, which
would fire our registry event handlers. Instead, we can invoke
`wl_display_dispatch`, which will do a few things for us:

* Flush the request buffer, sending all requests to the server.
* Wait until there are events in the queue
* Dispatch each event in the event queue

However, there's a little danger here. How do we know that the last event
in the event queue is the final event that the server has to send us? It
would be great if we could send a 'DONE?' request to the server, and it
would respond with a 'YEP, DONE' event. If only we could send such a request
after our initial `display.get_registry` request. We would then know that,
once we see the 'YEP, DONE' event in the event queue, that all of the events
resulting from our `display.get_registry` request had been received, since
they would be enqueued before our 'YEP, DONE' event.

The `display.sync` event does exactly this, and can be added to the request
buffer with the function `wl_display_sync(display)`. It even returns a
callback object, to which you can bind and event handler to specify what you
want to happen when the sync request/event roundtrip is complete.

So, to recap, we need to:

* Enqueue a `wl_display.get_registry` request
* Enqueue a `wl_display.sync` request
* Flush the request buffer, sending the two requests to the server
* Wait until there are events in the queue
* Dispatch each event in the event queue
* Check that our event queue has a `wl_display.done` event, so we know that
  we received all of the `wl_registry.global` events

The function `wl_display_roundtrip` does this work for us. It enqueus the
`wl_display.sync` request, uses `wl_display_dispatch` to flush the buffer
and dispatch any events that arrive in the event queue, and it repeats this
until it "sees" the `wl_display.done` event.

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

Q:
Why does wl_registry_get_version return 0, even after I've done a roundtrip?
Q: What are the two extra events from rountrip? One is done, what is the second?

### Step 3

Let's recap our original goal of displaying some graphics on the screen. In
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
```

Next, let's define a function that expresses how many bytes of memory we need.

```
int shm_size() {
  return 4 * WIDTH * HEIGHT;
}

```

Notice that the `size` of memory is not just `WIDTH * HEIGHT`. While `WIDTH`
and `HEIGHT` specifies our dimensions in pixels, each pixel consists of four
bytes (32 bits). Because `size` returns a value in bytes, the total number of
bytes we want is four bytes * the number of pixels.

Ok, we know the size of the memory we need. Now, let's create the
second thing we need: a file descriptor representing a handle on the
the memory itself using the POSIX shared memory API. The memory itself will
be represented as a file descriptor, or "fd" for short. We're going to take a
shortcut here, and use a simple abstraction we'll borrow from someone else,
letting us obtain the fd in just one statement:

```
int fd = create_shm_file(shm_size());
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
executable('client', ['client.c', 'shm.c'], dependencies: [lib_wayland, rt])
```

Since we have some memory, let's write some data to it, thereby painting it
with color data. Although we have an `fd` for the shared memory, this doesn't
provide us an API to write data to the shared memory. To get this, we'll use
`mmap`.

```
#include <sys/mman.h>
// ...
void * shm_data = mmap(NULL, shm_size(), PROT_READ | PROT_WRITE, MAP_SHARED,
  fd, 0);
```

You should [look up what `mmap` does](http://man7.org/linux/man-pages//man2/munmap.2.html), but in the end we now have a pointer in our
program that represents the base address of the shared memory. Let's paint!

```
uint32_t *pixel = shm_data;
for (int i = 0; i < WIDTH * HEIGHT; ++i) {
  *pixel++ = 0x6600ff;
}
```

Our client has now obtained some shared memory, and painted to it.



 The Wayland
protocol provides the `struct wl_buffer` abstraction that encapsulates the
chunk of memory that the client can draw on, and that the compositor can
access.

We'll need to talk to the operating system to allocate this shared memory
using the standard POSIX shared memory API, and then use that memory to
create our `struct wl_buffer`. Our current goal is to do the following,
which we can add to `main`:

```
struct wl_buffer *buffer = create_buffer();
```

And let's stub out this magic `create_buffer` function:

```
struct wl_buffer* create_buffer() {
  return NULL;
}
```

Now, in order to create a `struct wl_buffer`, we need a `struct wl_shm_pool`.
The pool encapsulates the memory shared between the server and the client,
from which we can obtain `struct wl_buffer` objects. In order to create a
`struct wl_shm_pool`, we need to use the `wl_shm` global advertised by the
registry, so we can invoke `wl_shm_create_pool`. In turn, this function
expects three arguments: the `wl_shm`, a file descriptor for the shared
memory, and the size of that memory. Let's work through creating those
arguments one at a time, and then we'll use them to create the pool, and
use the pool to create the buffer.



Your client should build and run, despite not doing much. But, our
`create_buffer` implementation now has a handle on our shared memory, and we
now have the second of the three arguments necessary for invoking
`wl_shm_create_pool`. Remember, `wl_shm_create_pool` requires a size, a file descriptor, and a `wl_shm` object.

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

Our event handler will be invoked once for every `wl_registry.global` event,
but right now we only care about the one related to `wl_shm`. When the handler
gets invoked and the `interface` is `wl_shm`, we'll ask the registry to

We now have the three arguments we need to obtain our `wl_shm_pool`, and can
invoke `wl_shm_create_pool` in our `create_buffer` function. We can then
obtain our `wl_buffer`, as well.

```
// ...
struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT,
  stride, WL_SHM_FORMAT_ARGB8888);
wl_shm_pool_destroy(pool);
return buffer;
```

The _stride_ of a graphics buffer represents the number of bytes
in one _row_ of a with * height sized buffer. This is one example of how
a `wl_buffer` provides a more powerful abstraction than a raw chunk of memory. By knowing the stride, it knows that the second row of pixels in our
window starts at byte 1200, and that the third row starts at byte 2400,
and so on. You should investigate [this concept of stride in graphics](https://docs.microsoft.com/en-us/windows/desktop/medfound/image-stride).



### Step 3b

All right, we've got our `wl_buffer` object, but there is one more step to
accomplish:




### Step N compositor

In order to get
this memory, our client will need a handle on the server compositor. The
compositor object is a server-side "global", advertised by the registry. To
get our handle, we'll want to modify our `wl_registry.global` event handler
such that, when we see the "compositor" global advertised, we can get our
handle on the compositor.

Let's add a global variable for a `struct wl_compositor *` and modify the
`registry_handle_global` implementation to initialize this new compositor
variable.

```
#include <string.h>
// ...
static const int COMPOSITOR_VERSION = 4;
struct wl_compositor *compositor;

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s, id %d, version %d\n",
    interface, name, version);
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
      COMPOSITOR_VERSION);
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
describe the server as "a compositor", we're using that term in a more
general sense. In a more specific sense, the `wl_compositor` is the component
within our "compositor" that is truly responsible for combining the contents
of multiple surfaces into a single displayable output (the
framebuffer).

But then, what is a "surface?"

### Step 4

TODO Surfaces, attaching buffer to surface, etc.


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

