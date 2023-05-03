#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include <sys/stat.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <stream_buffer.h>
#include "log.h"

extern StreamBufferHandle_t usbTxStream;

int _isatty(int fd) {
  if (fd == STDIN_FILENO || fd == STDERR_FILENO)
    return 1;

  errno = EBADF;
  return 0;
}

int _close(int fd) {
  if (fd == STDIN_FILENO || fd == STDERR_FILENO)
    return 0;

  errno = EBADF;
  return -1;
}

int _lseek(int fd, int ptr, int dir) {
  (void)fd;
  (void)ptr;
  (void)dir;

  errno = EBADF;
  return -1;
}

int _fstat(int fd, struct stat *st) {
  if (fd == STDIN_FILENO || fd == STDERR_FILENO) {
    st->st_mode = S_IFCHR;
    return 0;
  }

  errno = EBADF;
  return 0;
}

int fputc(int ch, FILE *f) {
  char c = (char) ch;

  log_append(&c, 1);
  xStreamBufferSend(usbTxStream, &c, 1, portMAX_DELAY);
  return ch;
}

int _write(int file, char *data, int len) {
  if ((file != STDOUT_FILENO) && (file != STDERR_FILENO)) {
    errno = EBADF;
    return -1;
  }

  log_append(data, len);

  while (len) {
    // xStreamBufferSend seems to deadlock if we try and send a single datum that's larger than the stream.
    // We instead break writes into 64 byte chunks; that's how big the USB_Tx task's recv buffer is, anyway.
    size_t sent = xStreamBufferSend(usbTxStream, data, (len > 64 ? 64 : len), portMAX_DELAY);
    len -= sent;
    data += sent;
  }
  return len;
}
