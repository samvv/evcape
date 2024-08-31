
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>

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

#define EVCAPE_SUCCESS 0
#define EVCAPE_OUT_OF_MEMORY 1
#define EVCAPE_TOO_MANY_KEYBOARDS 2
#define EVCAPE_OPEN_KEYBOARD_FAILED 3

enum EvLogLevel {
  EVCAPE_NO_LOGGING,
  EVCAPE_FATAL,
  EVCAPE_ERROR,
  EVCAPE_WARNING,
  EVCAPE_INFO,
  EVCAPE_VERBOSE,
  EVCAPE_TRACE,
};

typedef struct {
  int occupied;
  int fd;
  char* path;
} EvKeyboardHandle;

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

static int log_level = EVCAPE_INFO;

static int should_loop_epoll = 1;

static int epoll = -1;

static EvKeyboardHandle* keyboards;

static size_t num_keyboards = 0;

int evcape_add_keyboard(int fd, const char* devpath) {
  if (num_keyboards >= EVCAPE_MAX_KEYBOARDS) {
    return EVCAPE_TOO_MANY_KEYBOARDS;
  }
  for (size_t i = 0; i < EVCAPE_MAX_KEYBOARDS; i++) {
    EvKeyboardHandle* kb = &keyboards[i];
    if (kb->occupied == 0) {
      size_t n = strlen(devpath);
      char* path_clone = malloc(n);
      if (path_clone == NULL) {
        return EVCAPE_OUT_OF_MEMORY;
      }
      strncpy(path_clone, devpath, n);
      kb->occupied = 1;
      kb->fd = fd;
      kb->path = path_clone;
    }
  }
  num_keyboards++;
  return EVCAPE_SUCCESS;
}

int evcape_keyboard_fd(const char* devpath) {
  for (size_t i = 0; i < EVCAPE_MAX_KEYBOARDS; i++) {
    EvKeyboardHandle* kb = &keyboards[i];
    if (kb->occupied && strcmp(kb->path, devpath) == 0) {
      return kb->fd;
    }
  }
  return -1;
}

void evcape_remove_keyboard(const char* devpath) {
  for (size_t i = 0; i < EVCAPE_MAX_KEYBOARDS; i++) {
    EvKeyboardHandle* kb = &keyboards[i];
    if (kb->occupied && strcmp(kb->path, devpath) == 0) {
      free(kb->path);
      kb->occupied = 0;
      break;
    }
  }
}

static void evcape_vlog(int level, const char* message, va_list args) {
  if (log_level >= level) {
    time_t t;
    struct tm* tmp;
    char timestamp[256];
    time(&t);
    tmp = localtime(&t);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tmp);
    const char* prefix;
    switch (level) {
      case EVCAPE_TRACE:
        prefix = "trace";
        break;
      case EVCAPE_VERBOSE:
        prefix = "verb";
        break;
      case EVCAPE_INFO:
        prefix = "info";
        break;
      case EVCAPE_WARNING:
        prefix = "warn";
        break;
      case EVCAPE_ERROR:
        prefix = "erro";
        break;
      case EVCAPE_FATAL:
        prefix = "fata";
        break;
    }
    char buffer[256];
    vsnprintf(buffer, 256, message, args);
    fprintf(stderr, "[%s] (%s) %s\n", timestamp, prefix, buffer);
  }
}

static void evcape_log_trace(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_TRACE, message, args);
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

static void evcape_log_error(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_ERROR, message, args);
  va_end(args);
}

static void evcape_log_fatal(const char* message, ...) {
  va_list args;
  va_start(args, message);
  evcape_vlog(EVCAPE_FATAL, message, args);
  va_end(args);
}

void evcape_log_status(int status) {
  switch (status) {
    case EVCAPE_OUT_OF_MEMORY:
      evcape_log_error("host machine is out of memory");
      break;
    case EVCAPE_TOO_MANY_KEYBOARDS:
      evcape_log_error("there are more keyboards connected than evcape can handle");
      break;
    case EVCAPE_OPEN_KEYBOARD_FAILED:
      evcape_log_error("failed to open a keyboard device");
      break;
  }
}

int evcape_populate_keyboards(struct udev* udev) {

  // Keeps track of evcape errors
  int status;

  // Prepare a new query for specific devices
  struct udev_enumerate* enumerator = udev_enumerate_new(udev);

  // Configure the device query
  udev_enumerate_add_match_subsystem(enumerator, "input");
  udev_enumerate_add_match_property(enumerator, "ID_INPUT_KEYBOARD", "1");

  // Perform the actual query
  udev_enumerate_scan_devices(enumerator);

  // Now iterate through them

  struct udev_list_entry* device = udev_enumerate_get_list_entry(enumerator);
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, device) {

    const char* path = udev_list_entry_get_name(entry);

    evcape_log_verbose("found keyboard device %s", path);

    struct udev_device* kb = udev_device_new_from_syspath(udev, path);

    const char* dev_path = udev_device_get_devnode(kb);

    if (dev_path) {

      evcape_log_verbose("udev.devnode = %s", dev_path);

      int fd = open(dev_path, O_RDONLY | O_NONBLOCK);

      if (fd < 0) {
        evcape_log_error("could not open device %s", dev_path);
        continue;
      }

      status = evcape_add_keyboard(fd, dev_path);
      if (status != EVCAPE_SUCCESS) {
        evcape_log_status(status);
        continue;
      }

      struct epoll_event ev = {};
      ev.events = EPOLLIN;
      ev.data.fd = fd;
      epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &ev);

    }

  }

  udev_enumerate_unref(enumerator);

  return EVCAPE_SUCCESS;
}

/**
 * Runs a blocking loop that checks whether a keyboard was (dis)connected.
 */
void* evcape_monitor_udev(void* udev) {

  struct udev_monitor* mon;

  mon = udev_monitor_new_from_netlink(udev, "udev");
  if (udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL) < 0) {
    evcape_log_error("failed to call udev_monitor_filter_add_match_subsystem_devtype");
    abort();
  }
  if (udev_monitor_enable_receiving(mon) < 0){
    evcape_log_error("failed to call udev_monitor_enable_receiving");
    abort();
  }

  int fd = udev_monitor_get_fd(mon);

  for (;;) {

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int ret = select(fd+1, &fds, NULL, NULL, NULL);
    if (ret <= 0)
      break;

    if (FD_ISSET(fd, &fds)) {

      struct udev_device* dev = udev_monitor_receive_device(mon);

      if (dev) {

        const char* udev_path = udev_device_get_devnode(dev);
        if (udev_path == NULL) {
          continue;
        }

        evcape_log_verbose("detected change in device %s\n", udev_path);

        const char* value = udev_device_get_property_value(dev, "ID_INPUT_KEYBOARD");
        if (value && strcmp(value, "1") == 0) {

          evcape_log_verbose("Device %s is a keyboard!", udev_path);

          const char* udev_action = udev_device_get_action(dev);

          if (udev_action) {

            if (strcmp(udev_action, "add") == 0) {
              struct epoll_event ev = {};
              int udev_fd = open(udev_path, O_RDONLY | O_NONBLOCK);
              if (udev_fd < 0) {
                evcape_log_error("could not open keyboard %s\n", udev_path);
                continue;
              }
              evcape_add_keyboard(udev_fd, udev_path);
              ev.events = EPOLLIN;
              ev.data.fd = udev_fd;
              epoll_ctl(epoll, EPOLL_CTL_ADD, udev_fd, &ev);
            } else if (strcmp(udev_action, "remove") == 0) {
              int udev_fd = evcape_keyboard_fd(udev_path);
              assert(udev_fd != -1);
              epoll_ctl(epoll, EPOLL_CTL_DEL, udev_fd, NULL);
              evcape_remove_keyboard(udev_path);
            }
          }

        }

        udev_device_unref(dev);
      }

    }

  }

  return NULL;
}

int main(int argc, const char* argv[]) {

  int code = 0;
  char* loglevel_str = getenv("EVCAPE_LOG_LEVEL");
  if (loglevel_str != NULL) {
    int num = strtoumax(loglevel_str, NULL, 10);
    if (num != UINTMAX_MAX && errno != ERANGE && num <= EVCAPE_TRACE && num >= EVCAPE_NO_LOGGING) {
      log_level = num;
    }
  }

  // printf("Now logging with log level %i\n", log_level);

  keyboards = malloc(EVCAPE_MAX_KEYBOARDS * sizeof(EvKeyboardHandle));
  if (keyboards == NULL) {
    evcape_log_status(EVCAPE_OUT_OF_MEMORY);
    return 1;
  }
  memset(keyboards, 0, EVCAPE_MAX_KEYBOARDS * sizeof(EvKeyboardHandle));

  // Create the udev context
  struct udev* udev = udev_new();

  // Initialize the evdev system
  epoll = epoll_create(1);

  pthread_t hotplug_monitor;

  evcape_log_info("starting hotplug monitor");

  pthread_create(&hotplug_monitor, NULL, evcape_monitor_udev, udev);

  evcape_populate_keyboards(udev);

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

  int pressed_other_key = 0;
  double esc_pressed_timestamp;

  struct epoll_event events[EVCAPE_MAX_EPOLL_EVENTS];
  struct input_event input_events[EVCAPE_MAX_INPUT_EVENTS];

start: while (should_loop_epoll) {

    int count = epoll_wait(epoll, events, EVCAPE_MAX_EPOLL_EVENTS, -1);
    if (count == -1) {
      perror("failed to run epoll_wait");
      return 1;
    }

    for (int i = 0; i < count; i++) {

      ssize_t read_count = read(events[i].data.fd, &input_events, sizeof(struct input_event) * EVCAPE_MAX_INPUT_EVENTS);
      if (read_count == -1) {
        // If the device was unplugged, everything is fine
        if (errno == ENODEV) {
          goto start;
        }
        perror("failed to run read on device descriptor");
        goto exit;
      }

      int input_event_count = read_count / sizeof(struct input_event);

      if (read_count % sizeof(struct input_event)) {
        evcape_log_error("inconsistent read while trying to read some keyboard events");
        code = 1;
        goto exit;
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

exit:

  udev_unref(udev);

  ioctl(uinput_fd, UI_DEV_DESTROY);
  close(uinput_fd);

  close(epoll);

  free(keyboards);

  return code;
}

