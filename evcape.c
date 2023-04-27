
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include <libudev.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define EVCAPE_MAX_INPUT_EVENTS 32
#define EVCAPE_MAX_EPOLL_EVENTS 5
#define EVCAPE_MAX_KEYBOARDS 32
#define EVCAPE_MAX_ARG_LEN 64
#define EVCAPE_KEY_TIMEOUT 0.5

typedef struct {
  int fd;
  char* path;
} KBHandle;

static void emit(int fd, int type, int code, int val)
{
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   /* timestamp values below are ignored */
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

   write(fd, &ie, sizeof(ie));
}

static double evcape_timestamp() {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (double)tv.tv_sec + (double)tv.tv_nsec / 1000000000;
}

#define ANSI_FG_RED     "\x1b[31m"
#define ANSI_FG_BLACK   "\x1b[30m"
#define ANSI_FG_RED     "\x1b[31m"
#define ANSI_FG_GREEN   "\x1b[32m"
#define ANSI_FG_YELLOW  "\x1b[33m"
#define ANSI_FG_BLUE    "\x1b[34m"
#define ANSI_FG_MAGENTA "\x1b[35m"
#define ANSI_FG_CYAN    "\x1b[36m"
#define ANSI_FG_WHITE   "\x1b[37m"

#define ANSI_RESET "\x1b[0m"

#define ANSI_BOLD "\x1b[1m"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_REVERSED "\x1b[7m"

enum ECLogLevel {
  EVCAPE_FATAL,
  EVCAPE_ERROR,
  EVCAPE_WARNING,
  EVCAPE_INFO,
  EVCAPE_VERBOSE,
  EVCAPE_TRACE,
};

static int log_level = EVCAPE_VERBOSE;

static void evcape_vlog(int level, const char* message, va_list args) {
  if (log_level >= level) {
    char* prefix;
    switch (level) {
      case EVCAPE_VERBOSE:
        prefix = ANSI_FG_MAGENTA ANSI_BOLD "(verb)" ANSI_RESET;
        break;
      case EVCAPE_INFO:
        prefix = ANSI_FG_YELLOW ANSI_BOLD "(info)" ANSI_RESET;
        break;
      case EVCAPE_WARNING:
        prefix = ANSI_FG_RED ANSI_BOLD "(warn)" ANSI_RESET;
        break;
      case EVCAPE_ERROR:
        prefix = ANSI_FG_RED ANSI_BOLD "(erro)" ANSI_RESET;
        break;
      case EVCAPE_FATAL:
        prefix = ANSI_FG_RED ANSI_BOLD "(fata)" ANSI_RESET;
        break;
    }
    char buffer[256];
    vsprintf(buffer, message, args);
    fprintf(stderr, "%s %s\n", prefix, buffer);
  }
}

static void evcape_log_trace(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_TRACE, message, args);
  va_end(args);
}

static void evcape_log_error(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_ERROR, message, args);
  va_end(args);
}

static void evcape_log_verbose(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_VERBOSE, message, args);
  va_end(args);
}

static void evcape_log_info(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_INFO, message, args);
  va_end(args);
}

int main(int argc, const char* argv[]) {

  KBHandle kbs[EVCAPE_MAX_KEYBOARDS];
  int kb_count = 0;

  // Create the udev context
  struct udev* udev = udev_new();

  // Prepare a new query for specific devices
  struct udev_enumerate* enumerator = udev_enumerate_new(udev);

  // Configure the device query
  udev_enumerate_add_match_subsystem(enumerator, "input");
  udev_enumerate_add_match_property(enumerator, "ID_INPUT_KEYBOARD", "1");

  // Perform the actual query
  udev_enumerate_scan_devices(enumerator);

  struct udev_list_entry* first_entry = udev_enumerate_get_list_entry(enumerator);
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, first_entry) {

    const char* path = udev_list_entry_get_name(entry);

    fprintf(stderr, "Found keyboard device %s\n", path);

    struct udev_device* kb = udev_device_new_from_syspath(udev, path);

    const char* dev_path = udev_device_get_devnode(kb);

    if (dev_path) {

      fprintf(stderr, "dev_path = %s\n", dev_path);

      char* new_dev_path = malloc(strlen(dev_path));

      if (kb_count >= EVCAPE_MAX_KEYBOARDS) {
        fprintf(stderr, "Error: more than %i keyboards detected\n", EVCAPE_MAX_KEYBOARDS);
        // TODO close file descriptors up till this index
        return 1;
      }

      if (!new_dev_path) {
        fprintf(stderr, "Error: out of host memory\n");
        // TODO close file descriptors up till this index
        return 1;
      }

      strcpy(new_dev_path, dev_path);

      int fd = open(dev_path, O_RDONLY | O_NONBLOCK);

      if (fd < 0) {
        fprintf(stderr, "Error: could not open file %s\n", dev_path);
        // TODO close file descriptors up till this index
        return 1;
      }

      kbs[kb_count] = (KBHandle) {
        .fd = fd,
        .path = new_dev_path,
      };

      kb_count++;

    }

  }

  udev_enumerate_unref(enumerator);

  udev_unref(udev);

  // Initialize the evdev system 
  
  int epoll = epoll_create(kb_count);

  for (int i = 0; i < kb_count; i++) {

    int fd = kbs[i].fd;

    struct epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &ev);

  }

  int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  if (uinput_fd < 0) {
    evcape_log_error("unable to open /dev/uinput");
    return 1;
  }

  // The ioctls below will enable the device that is about to be
  // created, to pass key events (in this case the escape key)

  if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY)) {
    evcape_log_error("ioctl UI_SET_EVBIT failed");
    return 1;
  }

  if (ioctl(uinput_fd, UI_SET_KEYBIT, KEY_ESC)) {
    evcape_log_error("ioctl UI_SET_KEYBIT failed");
    return 1;
  }

  struct uinput_setup usetup = {};
  usetup.id.bustype = BUS_USB,
  usetup.id.vendor = 0x1234,
  usetup.id.vendor = 0x5678,
  strcpy(usetup.name, "evcape");

  if (ioctl(uinput_fd, UI_DEV_SETUP, &usetup)) {
    evcape_log_error("ioctl UI_DEV_SETUP failed");
    return 1;
  }
  if (ioctl(uinput_fd, UI_DEV_CREATE)) {
    evcape_log_error("ioctl UI_DEV_CREATE failed")  ;
    return 1;
  }

  int should_loop = 1;
  int pressed_other_key = 0;
  double esc_pressed_timestamp;

  while (should_loop) {

    struct epoll_event events[EVCAPE_MAX_EPOLL_EVENTS];

    int count = epoll_wait(epoll, events, EVCAPE_MAX_EPOLL_EVENTS, 0);

    for (int i = 0; i < count; i++) {

      struct input_event input_events[EVCAPE_MAX_INPUT_EVENTS];

      ssize_t read_count = read(events[i].data.fd, &input_events, sizeof(struct input_event) * EVCAPE_MAX_INPUT_EVENTS);

      int input_event_count = read_count / sizeof(struct input_event);

      if (read_count % sizeof(struct input_event)) {
        evcape_log_error("inconsistent read while trying to read some keyboard events");
        return 1;
      }

      struct input_event* input_events_end = input_events + input_event_count;

      for (struct input_event* ev = input_events; ev != input_events_end; ev++) {

        evcape_log_trace("Event: time %ld.%06ld, type: %i, code: %i, value: %i", ev->time.tv_sec, ev->time.tv_usec, ev->type, ev->code, ev->value);

        if (ev->type == EV_KEY) {

          if (ev->code == KEY_CAPSLOCK) {
            if (ev->value == 1) {
              pressed_other_key = 0;
              esc_pressed_timestamp = evcape_timestamp();
            } else if (ev->value == 0) {
              double esc_released_time = evcape_timestamp();
              if (esc_released_time - esc_pressed_timestamp < EVCAPE_KEY_TIMEOUT && !pressed_other_key) {
                evcape_log_verbose("sending the Escape-key ...");
                emit(uinput_fd, EV_KEY, KEY_ESC, 1);
                emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
                emit(uinput_fd, EV_KEY, KEY_ESC, 0);
                emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
              }
            }
          } else {
            pressed_other_key = 1;
          }

        }

      }

    }

  }

  ioctl(uinput_fd, UI_DEV_DESTROY);
  close(uinput_fd);

  close(epoll);

  return 0;
}

