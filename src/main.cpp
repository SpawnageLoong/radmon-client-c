/*
 * Copyright (C) 2025  Richard Loong
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/termbits.h> /* struct termios2 */
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>

#include <iostream>
#include <fstream>
#include <linux/limits.h>
#include <iomanip>

using namespace std;

// Constants
#define CANUSB_INJECT_SLEEP_GAP_DEFAULT 200 /* ms */
#define CANUSB_CAN_SPEED_DEFAULT 500000
#define CANUSB_TTY_BAUD_RATE_DEFAULT 2000000
#define CANUSB_INJECT_ID_DEFAULT "010"
#define CANUSB_RECEIVE_ID_DEFAULT "011"

// Type Definitions
typedef enum {
  CANUSB_SPEED_1000000 = 0x01,
  CANUSB_SPEED_800000  = 0x02,
  CANUSB_SPEED_500000  = 0x03,
  CANUSB_SPEED_400000  = 0x04,
  CANUSB_SPEED_250000  = 0x05,
  CANUSB_SPEED_200000  = 0x06,
  CANUSB_SPEED_125000  = 0x07,
  CANUSB_SPEED_100000  = 0x08,
  CANUSB_SPEED_50000   = 0x09,
  CANUSB_SPEED_20000   = 0x0a,
  CANUSB_SPEED_10000   = 0x0b,
  CANUSB_SPEED_5000    = 0x0c,
} CANUSB_SPEED;

typedef enum {
  CANUSB_MODE_NORMAL          = 0x00,
  CANUSB_MODE_LOOPBACK        = 0x01,
  CANUSB_MODE_SILENT          = 0x02,
  CANUSB_MODE_LOOPBACK_SILENT = 0x03,
} CANUSB_MODE;

typedef enum {
  CANUSB_FRAME_STANDARD = 0x01,
  CANUSB_FRAME_EXTENDED = 0x02,
} CANUSB_FRAME;

typedef enum {
  CANUSB_INJECT_PAYLOAD_MODE_RANDOM      = 0,
  CANUSB_INJECT_PAYLOAD_MODE_INCREMENTAL = 1,
  CANUSB_INJECT_PAYLOAD_MODE_FIXED       = 2,
} CANUSB_PAYLOAD_MODE;

typedef enum {
  INFO    = 0,
  WARN    = 1,
  ERROR   = 2,
} LOGGING_LEVEL;


// Global Variables
static int program_running = 1;
static int print_traffic = 0;

char debug_output[4095];


// Function Prototypes
static CANUSB_SPEED canusb_int_to_speed(int speed);
static int generate_checksum(const unsigned char *data, int data_len);
static int frame_is_complete(const unsigned char *frame, int frame_len);
static int frame_send(int tty_fd, const unsigned char *frame, int frame_len);
static int command_settings(int tty_fd, CANUSB_SPEED speed, CANUSB_MODE mode, CANUSB_FRAME frame);
static int hex_value(int c);
static int convert_from_hex(const char *hex_string, unsigned char *bin_string, int bin_string_len);
static int send_data_frame(int tty_fd, const string hex_id, const char *hex_data);
static void clear_buffer(int tty_fd);
static void receive_frame(int tty_fd, unsigned char (&frame_out)[32]);
static int adapter_init(const char *tty_device, int baudrate);
static void display_help(const char *progname);
static void sigterm(int signo);
static void display_logo();
static void display_menu(char* user_input);
static void send_clear_cmd(int tty_fd, string inject_id);
static void send_full_dump_cmd(int tty_fd, string inject_id);
static void send_part_dump_cmd(int tty_fd, string inject_id);
static void send_update_rtc_cmd(int tty_fd, string inject_id);
static void logprintf(ofstream &logptr, string string, LOGGING_LEVEL log_level);
static void print_frame(unsigned char *frame);
static void read_frames_to_file(int tty_fd, char *bin_path, string cmd, int frame_count);
static void save_frame(int tty_fd, ofstream& dump_file);
static void test_fram(int tty_fd, char *bin_path);



int main(int argc, char *argv[])
{
  int c, tty_fd;
  char *tty_device = NULL, user_input;
  CANUSB_SPEED speed = canusb_int_to_speed(CANUSB_CAN_SPEED_DEFAULT);
  int baudrate = CANUSB_TTY_BAUD_RATE_DEFAULT;
  bool is_exit = false;
  bool is_test_mode = false;
  unsigned char frame[32];
  string inject_id, receive_id;

  char *bin_path(argv[0]);

  time_t ts = time(NULL);
  struct tm datetime = *localtime(&ts);
  char time_string[50];
  strftime(time_string, 50, "%F_%H%Mhrs", &datetime);

  char log_path[PATH_MAX];
  strcpy(log_path, bin_path);
  strcat(log_path, "-logs/");
  strcat(log_path, time_string);
  strcat(log_path, ".log");

  inject_id = CANUSB_INJECT_ID_DEFAULT;
  receive_id = CANUSB_RECEIVE_ID_DEFAULT;

  ofstream logptr(log_path);
  logprintf(logptr, "Program started.", INFO);

  while ((c = getopt(argc, argv, "hd:s:b:i:r:t:")) != -1) {
    switch (c) {
    case 'h':
      display_help(argv[0]);
      logptr.close();
      remove(log_path);
      return EXIT_SUCCESS;

    case 'd':
      tty_device = optarg;
      sprintf(debug_output, "TTY device set to: %s", tty_device);
      logprintf(logptr, debug_output, INFO);
      break;

    case 's':
      speed = canusb_int_to_speed(atoi(optarg));
      sprintf(debug_output, "CAN speed set to: %d", atoi(optarg));
      logprintf(logptr, debug_output, INFO);
      break;

    case 'b':
      baudrate = atoi(optarg);
      sprintf(debug_output, "Baudrate set to: %d", baudrate);
      logprintf(logptr, debug_output, INFO);
      break;

    case 'i':
      inject_id = optarg;
      sprintf(debug_output, "Inject ID set to: %s", inject_id.c_str());
      logprintf(logptr, debug_output, INFO);
      break;
    
    case 'r':
      receive_id = optarg;
      sprintf(debug_output, "Receive ID set to: %s", receive_id.c_str());
      logprintf(logptr, debug_output, INFO);
      break;
    
    case 't':
      is_test_mode = true;
      break;

    case '?':
    default:
      display_help(argv[0]);
      logptr.close();
      remove(log_path);
      return EXIT_FAILURE;
    }
  }

  signal(SIGTERM, sigterm);
  signal(SIGHUP, sigterm);
  signal(SIGINT, sigterm);

  if (tty_device == NULL) {
    fprintf(stderr, "Please specify a TTY!\n");
    display_help(argv[0]);
    sprintf(debug_output, "TTY device not specified, exiting.");
    logprintf(logptr, debug_output, ERROR);
    return EXIT_FAILURE;
  }

  if (speed == 0) {
    fprintf(stderr, "Please specify a valid speed!\n");
    display_help(argv[0]);
    sprintf(debug_output, "CAN speed not specified, exiting.");
    logprintf(logptr, debug_output, ERROR);
    return EXIT_FAILURE;
  }

  tty_fd = adapter_init(tty_device, baudrate);
  if (tty_fd == -1) {
    sprintf(debug_output, "Failed to initialize adapter, exiting.");
    logprintf(logptr, debug_output, ERROR);
    return EXIT_FAILURE;
  }

  command_settings(tty_fd, speed, CANUSB_MODE_NORMAL, CANUSB_FRAME_STANDARD);
  sprintf(debug_output, "Adapter initialized successfully.");
  logprintf(logptr, debug_output, INFO);

  if (is_test_mode) {
    sprintf(debug_output, "Test mode enabled.");
    logprintf(logptr, debug_output, INFO);
    logprintf(logptr, "Dumping FRAM (32kB) to console", INFO);
    fprintf(stderr, "Dumping FRAM (32kB) to console.\n");
    send_full_dump_cmd(tty_fd, inject_id);
    read_frames_to_file(tty_fd, bin_path, "dump-fram-32kb", 7190);
    logptr.close();
    return EXIT_SUCCESS;
  }

  display_logo();

  while (!is_exit) {
    display_menu(&user_input);
    sprintf(debug_output, "User input: %c", user_input);
    logprintf(logptr, debug_output, INFO);
    switch(user_input) {
      case '1':
        logprintf(logptr, "Dumping FRAM (32kB) to console", INFO);
        fprintf(stderr, "Dumping FRAM (32kB) to console.\n");
        send_full_dump_cmd(tty_fd, inject_id);
        usleep(100000);
        read_frames_to_file(tty_fd, bin_path, "dump-fram-32kb", 7190);
        break;
      
      case '2':
        logprintf(logptr, "Dumping FRAM (512B) to console", INFO);
        fprintf(stderr, "Dumping FRAM (512B) to console.\n");
        send_part_dump_cmd(tty_fd, inject_id);
        usleep(100000);
        read_frames_to_file(tty_fd, bin_path, "dump-fram-512b", 128);
        break;
      
      case '4':
        logprintf(logptr, "Updating RTC", INFO);
        fprintf(stderr, "Updating RTC.\n");
        send_update_rtc_cmd(tty_fd, inject_id);
        usleep(100000);
        break;

      case '6':
        logprintf(logptr, "Clearing FRAM", INFO);
        fprintf(stderr, "Clearing FRAM.\n");
        send_clear_cmd(tty_fd, inject_id);
        usleep(100000);
        receive_frame(tty_fd, frame);
        break;
      
      case '8':
        logprintf(logptr, "Clearing CANbus buffer", INFO);
        fprintf(stderr, "Clearing CANbus buffer.\n");
        clear_buffer(tty_fd);
        usleep(100000);
        break;

      case '9':
        logprintf(logptr, "Exiting program", INFO);
        fprintf(stderr, "Now exiting.\n");
        is_exit = true;
        logptr.close();
        return EXIT_SUCCESS;
      
      default:
        logprintf(logptr, "Unknown command", WARN);
        fprintf(stderr, "Unknown command received.\n");
        break;
    }
  }

  logprintf(logptr, "Unexpected exit of main loop", ERROR);
  fprintf(stderr, "Unexpected exit of main loop, now exiting.\n");
  return EXIT_FAILURE;
}



// Function Definitions
static CANUSB_SPEED canusb_int_to_speed(int speed)
{
  switch (speed) {
  case 1000000:
    return CANUSB_SPEED_1000000;
  case 800000:
    return CANUSB_SPEED_800000;
  case 500000:
    return CANUSB_SPEED_500000;
  case 400000:
    return CANUSB_SPEED_400000;
  case 250000:
    return CANUSB_SPEED_250000;
  case 200000:
    return CANUSB_SPEED_200000;
  case 125000:
    return CANUSB_SPEED_125000;
  case 100000:
    return CANUSB_SPEED_100000;
  case 50000:
    return CANUSB_SPEED_50000;
  case 20000:
    return CANUSB_SPEED_20000;
  case 10000:
    return CANUSB_SPEED_10000;
  case 5000:
    return CANUSB_SPEED_5000;
  default:
    return CANUSB_SPEED_500000;
  }
}



static int generate_checksum(const unsigned char *data, int data_len)
{
  int i, checksum;

  checksum = 0;
  for (i = 0; i < data_len; i++) {
    checksum += data[i];
  }

  return checksum & 0xff;
}



static int frame_is_complete(const unsigned char *frame, int frame_len)
{
  if (frame_len > 0) {
    if (frame[0] != 0xaa) {
      /* Need to sync on 0xaa at start of frames, so just skip. */
      return 1;
    }
  }

  if (frame_len < 2) {
    return 0;
  }

  if (frame[1] == 0x55) { /* Command frame... */
    if (frame_len >= 20) { /* ...always 20 bytes. */
      return 1;
    } else {
      return 0;
    }
  } else if ((frame[1] >> 4) == 0xc) { /* Data frame... */
    if (frame_len >= (frame[1] & 0xf) + 5) { /* ...payload and 5 bytes. */
      return 1;
    } else {
      return 0;
    }
  }

  /* Unhandled frame type. */
  return 1;
}



static int frame_send(int tty_fd, const unsigned char *frame, int frame_len)
{
  int result, i;

  if (print_traffic) {
    printf(">>> ");
    for (i = 0; i < frame_len; i++) {
      printf("%02x ", frame[i]);
    }
    if (print_traffic > 1) {
      printf("    '");
      for (i = 4; i < frame_len - 1; i++) {
        printf("%c", isalnum(frame[i]) ? frame[i] : '.');
      }
      printf("'");
    }
    printf("\n");
  }

  result = write(tty_fd, frame, frame_len);
  if (result == -1) {
    fprintf(stderr, "write() failed: %s\n", strerror(errno));
    return -1;
  }

  return frame_len;
}



static int command_settings(int tty_fd, CANUSB_SPEED speed, CANUSB_MODE mode, CANUSB_FRAME frame)
{
  int cmd_frame_len;
  unsigned char cmd_frame[20];

  cmd_frame_len = 0;
  cmd_frame[cmd_frame_len++] = 0xaa;
  cmd_frame[cmd_frame_len++] = 0x55;
  cmd_frame[cmd_frame_len++] = 0x12;
  cmd_frame[cmd_frame_len++] = speed;
  cmd_frame[cmd_frame_len++] = frame;
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = mode;
  cmd_frame[cmd_frame_len++] = 0x01;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = generate_checksum(&cmd_frame[2], 17);

  if (frame_send(tty_fd, cmd_frame, cmd_frame_len) < 0) {
    return -1;
  }

  return 0;
}



static int hex_value(int c)
{
  if (c >= 0x30 && c <= 0x39) /* '0' - '9' */
    return c - 0x30;
  else if (c >= 0x41 && c <= 0x46) /* 'A' - 'F' */
    return (c - 0x41) + 10;
  else if (c >= 0x61 && c <= 0x66) /* 'a' - 'f' */
    return (c - 0x61) + 10;
  else
    return -1;
}



static int convert_from_hex(const char *hex_string, unsigned char *bin_string, int bin_string_len)
{
  int n1, n2, high;

  high = -1;
  n1 = n2 = 0;
  while (hex_string[n1] != '\0') {
    if (hex_value(hex_string[n1]) >= 0) {
      if (high == -1) {
        high = hex_string[n1];
      } else {
        bin_string[n2] = hex_value(high) * 16 + hex_value(hex_string[n1]);
        n2++;
        if (n2 >= bin_string_len) {
          printf("hex string truncated to %d bytes\n", n2);
          break;
        }
        high = -1;
      }
    }
    n1++;
  }

  return n2;
}



static int send_data_frame(int tty_fd, const string hex_id, const char *hex_data)
{
  int data_len;
  unsigned char binary_data[8];
  unsigned char binary_id_lsb = 0, binary_id_msb = 0;

  data_len = convert_from_hex(hex_data, binary_data, sizeof(binary_data));
  if (data_len == 0) {
    fprintf(stderr, "Unable to convert data from hex to binary!\n");
    return -1;
  }

  switch (hex_id.length()) {
    case 1:
      binary_id_lsb = hex_value(hex_id[0]);
      break;
  
    case 2:
      binary_id_lsb = (hex_value(hex_id[0]) * 16) + hex_value(hex_id[1]);
      break;
  
    case 3:
      binary_id_msb = hex_value(hex_id[0]);
      binary_id_lsb = (hex_value(hex_id[1]) * 16) + hex_value(hex_id[2]);
      break;
  
    default:
      fprintf(stderr, "Unable to convert ID from hex to binary!\n");
      return -1;
  }

  CANUSB_FRAME frame = CANUSB_FRAME_STANDARD;

  #define MAX_FRAME_SIZE 13
  int data_frame_len = 0;
  unsigned char data_frame[MAX_FRAME_SIZE] = {0x00};

  if (data_len < 0 || data_len > 8)
  {
    fprintf(stderr, "Data length code (DLC) must be between 0 and 8!\n");
    return -1;
  }

  /* Byte 0: Packet Start */
  data_frame[data_frame_len++] = 0xaa;

  /* Byte 1: CAN Bus Data Frame Information */
  data_frame[data_frame_len] = 0x00;
  data_frame[data_frame_len] |= 0xC0; /* Bit 7 Always 1, Bit 6 Always 1 */
  if (frame == CANUSB_FRAME_STANDARD)
    data_frame[data_frame_len] &= 0xDF; /* STD frame */
  else /* CANUSB_FRAME_EXTENDED */
    data_frame[data_frame_len] |= 0x20; /* EXT frame */
  data_frame[data_frame_len] &= 0xEF; /* 0=Data */
  data_frame[data_frame_len] |= data_len; /* DLC=data_len */
  data_frame_len++;

  /* Byte 2 to 3: ID */
  data_frame[data_frame_len++] = binary_id_lsb; /* lsb */
  data_frame[data_frame_len++] = binary_id_msb; /* msb */

  /* Byte 4 to (4+data_len): Data */
  for (int i = 0; i < data_len; i++)
    data_frame[data_frame_len++] = binary_data[i];

  /* Last byte: End of frame */
  data_frame[data_frame_len++] = 0x55;

  if (frame_send(tty_fd, data_frame, data_frame_len) < 0)
  {
    fprintf(stderr, "Unable to send frame!\n");
    return -1;
  }

  return 0;
}



static void clear_buffer(int tty_fd)
{
  int is_buffer_filled = 1;
  int result;
  unsigned char byte;

  while (is_buffer_filled) {
    result = read(tty_fd, &byte, 1);
    if (result != 1) {
      is_buffer_filled = 0;
      return;
    }
    usleep(2);
  }
  return;
}



static void receive_frame(int tty_fd, unsigned char (&frame_out)[32])
{
  int frame_len = 0;
  unsigned char frame[32];

  int result, checksum;
  unsigned char byte;

  int is_frame_complete = 0;

  frame_len = 0;
  while (!is_frame_complete) {
    result = read(tty_fd, &byte, 1);
    if (result == -1) {
      fprintf(stderr, "read() failed: %s\n", strerror(errno));
      return;
      
    } else if (result > 0) {
      if (print_traffic) {
        fprintf(stderr, "%02x ", byte);
      }

      frame[frame_len++] = byte;

      if (frame_is_complete(frame, frame_len)) {
        is_frame_complete = 1;
        break;
      }
    } else {
      return;
    }
    usleep(2);
  }

  if ((frame_len == 20) && (frame[0] == 0xaa) && (frame[1] == 0x55)) {
    checksum = generate_checksum(&frame[2], 17);
    if (checksum != frame[frame_len - 1]) {
      fprintf(stderr, "receive_frame() failed: Checksum incorrect\n");
      return;
    }
  }

  if (frame_len == -1) {
    printf("Frame recieve error!\n");
  } else {
    print_frame(frame);

    for (int i=0; i<frame_len; i++) {
      frame_out[i] = frame[i];
    }
  }
}



static int adapter_init(const char *tty_device, int baudrate)
{
  int tty_fd, result;
  struct termios2 tio;

  tty_fd = open(tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (tty_fd == -1) {
    fprintf(stderr, "open(%s) failed: %s\n", tty_device, strerror(errno));
    return -1;
  }

  result = ioctl(tty_fd, TCGETS2, &tio);
  if (result == -1) {
    fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
    close(tty_fd);
    return -1;
  }

  tio.c_cflag &= ~CBAUD;
  tio.c_cflag = BOTHER | CS8 | CSTOPB;
  tio.c_iflag = IGNPAR;
  tio.c_oflag = 0;
  tio.c_lflag = 0;
  tio.c_ispeed = baudrate;
  tio.c_ospeed = baudrate;

  result = ioctl(tty_fd, TCSETS2, &tio);
  if (result == -1) {
    fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
    close(tty_fd);
    return -1;
  }

  return tty_fd;
}



static void display_help(const char *progname)
{
  fprintf(stderr, "Usage: %s <options>\n", progname);
  fprintf(stderr, "Options:\n"
     "  -h          Display this help and exit.\n"
     "  -d DEVICE   Use TTY DEVICE.\n"
     "  -s SPEED    Set CAN SPEED in bps (default: %d).\n"
     "  -b BAUDRATE Set TTY/serial BAUDRATE (default: %d).\n"
     "  -i SEND_ID  Inject using ID (specified as hex string).\n"
     "  -r RECV_ID  Receive using ID (specified as hex string).\n"
     "\n",
     CANUSB_CAN_SPEED_DEFAULT,
     CANUSB_TTY_BAUD_RATE_DEFAULT);
}



static void sigterm(int signo)
{
  program_running = 0;
}



static void display_logo()
{
  fprintf(stderr, "\n"
    "  _____           _ __  __                _____ _ _            _   \n"
    " |  __ \\         | |  \\/  |              / ____| (_)          | |  \n"
    " | |__) |__ _  __| | \\  / | ___  _ __   | |    | |_  ___ _ __ | |_ \n"
    " |  _  // _` |/ _` | |\\/| |/ _ \\| '_ \\  | |    | | |/ _ \\ '_ \\| __|\n"
    " | | \\ \\ (_| | (_| | |  | | (_) | | | | | |____| | |  __/ | | | |_ \n"
    " |_|  \\_\\__,_|\\__,_|_|  |_|\\___/|_| |_|  \\_____|_|_|\\___|_| |_|\\__|\n");
}



static void display_menu(char* user_input)
{
  fprintf(stderr, "\n"
    "____________________________________________________________________________________\n"
    "Available Options:\n"
    "  1) Dump FRAM (32kB) to console\n"
    "  2) Dump FRAM (512B) to console\n"
    "\n"
    "  4) Update RTC\n"
    "\n"
    "  6) Clear FRAM\n"
    "\n"
    "  8) Clear CAN buffer\n"
    "  9) Exit\n"
    "\n"
    "Enter a number 1-9: ");
  
  scanf("%c", user_input);
  getchar(); // reads the /n character to ignore it.
  fprintf(stderr, "\n");
}



static void send_clear_cmd(int tty_fd, string inject_id)
{
  char data[] = { '0', '1' };
  send_data_frame(tty_fd, inject_id, data);
  return;
}



static void send_full_dump_cmd(int tty_fd, string inject_id)
{
  char data[] = { '0', '2' };
  send_data_frame(tty_fd, inject_id, data);
  return;
}



static void send_part_dump_cmd(int tty_fd, string inject_id)
{
  char data[] = { '0', '4' };
  send_data_frame(tty_fd, inject_id, data);
  return;
}



static void send_update_rtc_cmd(int tty_fd, string inject_id)
{
  time_t ts = time(NULL);
  printf("current time: %ld\n", ts);
  // separate ts into 8 bytes
  char hex_string[9];
  sprintf(hex_string, "%lx", ts);
  char data[] = { 'A', 'A', hex_string[0], hex_string[1], hex_string[2], hex_string[3], hex_string[4], hex_string[5], hex_string[6], hex_string[7] };
  //char data[] = { 'A', 'A', '6', '7', 'D', '5', '3', 'F', 'A', 'A' };
  send_data_frame(tty_fd, inject_id, data);
  return;
}



static void logprintf(ofstream &logptr, string string, LOGGING_LEVEL log_level)
{
  char time_string[50];
  time_t ts = time(NULL);
  struct tm datetime = *localtime(&ts);
  strftime(time_string, 50, "%F %H:%M:%S ", &datetime);
  std::string print_string(time_string);
  switch(log_level) {
  case 0:
    //strcat(print_string, "\033[1;37m[INFO] ");
    print_string.append("\033[1;37m[INFO] ");
    break;
  
  case 1:
    //strcat(print_string, "\033[1;33m[WARN] ");
    print_string.append("\033[1;33m[WARN] ");
    break;

  case 2:
    //strcat(print_string, "\033[1;31m[ERROR] ");
    print_string.append("\033[1;31m[ERROR] ");
    break;
  }
  //strcat(print_string, string);
  //strcat(print_string, "\033[0m");
  print_string.append(string).append("\033[0m\n");
  logptr << print_string;
}



static void print_frame(unsigned char *frame)
{
  int i;
  int frame_len = sizeof(frame);
  if ((frame_len >= 6) && (frame[0] == 0xaa) && ((frame[1] >> 4) == 0xc)) {
    printf("Frame ID: %02x%02x, Data: ", frame[3], frame[2]);
    for (i = 4; i < 12; i++) {
      printf("%02x ", frame[i]);
    }
    printf("\n");
  } else {
    printf("Unknown: ");
    for (i = 0; i <= frame_len; i++) {
      printf("%02x ", frame[i]);
    }
    printf("\n");
  }
}



static void read_frames_to_file(int tty_fd, char *bin_path, string cmd, int frame_count)
{
  time_t ts = time(NULL);
  struct tm datetime = *localtime(&ts);
  char time_string[50];
  char cmd_string[50];
  strftime(time_string, 50, "%F_%H%Mhrs%Ssec-", &datetime);

  char dump_path[PATH_MAX];
  strcpy(dump_path, bin_path);
  strcat(dump_path, "-dumps/");
  strcat(dump_path, time_string);
  sprintf(cmd_string, "%s", cmd.c_str());
  strcat(dump_path, cmd_string);
  strcat(dump_path, ".txt");

  ofstream dump_file(dump_path);

  for (int i = 0; i < frame_count; i++) {
    save_frame(tty_fd, dump_file);
  }
  dump_file.close();
  return;
}



static void save_frame(int tty_fd, ofstream& dump_file)
{
  int frame_len = 0;
  unsigned char frame[32];

  int result, checksum;
  unsigned char byte;

  int is_frame_complete = 0;

  frame_len = 0;
  while (!is_frame_complete) {
    result = read(tty_fd, &byte, 1);
    if (result == -1) {
      fprintf(stderr, "read() failed: %s\n", strerror(errno));
      return;
      
    } else if (result > 0) {
      if (print_traffic) {
        fprintf(stderr, "%02x ", byte);
      }

      frame[frame_len++] = byte;

      if (frame_is_complete(frame, frame_len)) {
        is_frame_complete = 1;
        break;
      }
    } else {
      return;
    }
    usleep(2);
  }

  if ((frame_len == 20) && (frame[0] == 0xaa) && (frame[1] == 0x55)) {
    checksum = generate_checksum(&frame[2], 17);
    if (checksum != frame[frame_len - 1]) {
      fprintf(stderr, "receive_frame() failed: Checksum incorrect\n");
      return;
    }
  }

  if (frame_len == -1) {
    printf("Frame recieve error!\n");
  } else {
    if ((frame_len >= 6) && (frame[0] == 0xaa) && ((frame[1] >> 4) == 0xc)) {
      printf("Frame ID: %02x%02x, Data: ", frame[3], frame[2]);
      dump_file << "Frame ID: " << hex << (int)frame[3] << (int)frame[2] << dec << ", Data: ";
      for (int i = 4; i < 12; i++) {
        printf("%02x ", (int)frame[i]);
        dump_file << hex << setw(2) << setfill('0') << (int)frame[i] << dec << " ";
      }
      printf("\n");
      dump_file << "\n" << ends;
    } else {
      printf("Unknown: ");
      dump_file << "Unknown: ";
      for (int i = 0; i <= frame_len; i++) {
        printf("%02x ", frame[i]);
        dump_file << hex << (int)frame[i] << dec << " ";
      }
      printf("\n");
      dump_file << "\n";
    }
  }
}