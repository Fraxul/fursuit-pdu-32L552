#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include <FreeRTOS.h>
#include <stream_buffer.h>

extern StreamBufferHandle_t usbTxStream;

int _write(int file, char *data, int len) {
  if ((file != STDOUT_FILENO) && (file != STDERR_FILENO)) {
    errno = EBADF;
    return -1;
  }

  while (len) {
    // xStreamBufferSend seems to deadlock if we try and send a single datum that's larger than the stream.
    // We instead break writes into 64 byte chunks; that's how big the USB_Tx task's recv buffer is, anyway.
    size_t sent = xStreamBufferSend(usbTxStream, data, (len > 64 ? 64 : len), portMAX_DELAY);
    len -= sent;
    data += sent;
  }
  return len;
}
