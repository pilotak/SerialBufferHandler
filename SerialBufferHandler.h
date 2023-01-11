/*
MIT License

Copyright (c) 2023 Pavel S

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SERIALBUFFERHANDLER_H
#define SERIALBUFFERHANDLER_H

#include <chrono>
#include <limits.h>
#include "mbed.h"
using namespace std::chrono;

class SerialBufferHandler {
  public:
    /**
     * Constructor
     *
     * @param fh    File handle used for reading responses
     * @param queue Event queue used to transfer sigio events to this thread
     */
    SerialBufferHandler(FileHandle *fh, EventQueue &queue);
    void attach(Callback<void()> cb, milliseconds timeout);
    void flush();

    /** Reads given number of bytes from receiving buffer
    *
    *  @param buf output buffer for the read
    *  @param len maximum number of bytes to read
    *  @return number of successfully read bytes or -1 in case of error
    */
    size_t read_bytes(uint8_t *buf, size_t len);

    /**
     * @brief rewind buffer until byte is found
     *
     * @param until
     * @return number of bytes available
     */
    size_t rewind_until(uint8_t until);

    /**
     * @brief rewind buffer until bytes are found
     *
     * @param find specific occurrence to find
     * @param len length of find buffer
     * @return number of bytes available
     */
    size_t rewind_until(const uint8_t *find, size_t len);

    /**
     * @brief Get number of bytes available
     *
     * @return number of bytes available
     */
    size_t available_bytes();

  private:

    void event();

    // Read from file handle
    void read();

    // Reads from serial to receiving buffer.
    // Returns true on successful read OR false on timeout
    bool fill_buffer(bool wait_for_timeout = true);

    // Reading position set to 0 and buffer's unread content moved to beginning
    void rewind_buffer();

    // Sets to 0 the reading position, reading length and the whole buffer content
    void reset_buffer();

    // Calculate remaining time for polling based on request start time and timeout
    // Returns 0 or time in ms for polling.
    int poll_timeout(bool wait_for_timeout);

    // Mutex lock
    void lock();

    // Mutex unlock
    void unlock();

    FileHandle *_fileHandle;
    EventQueue &_queue;
    Mutex _mutex;
    Callback<void()> _cb;
    uint8_t _buffer[MBED_CONF_SERIALBUFFERHANDLER_BUFFER_SIZE] = {0};

    size_t _recv_pos = 0;
    size_t _recv_len = 0;
    int _event_id = 0;
    milliseconds _timeout = 10ms; // ((8+2)*1)/1200*1000 [((8bit + 1 start+ 1 stop)*no_of_bytes) / baud * 1000ms]
    Kernel::Clock::time_point _start_time;
};

#endif // SERIALBUFFERHANDLER_H