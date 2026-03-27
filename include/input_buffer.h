#pragma once
#ifndef INPUT_BUFFER_H
#define INPUT_BUFFER_H

#include <barrier>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

constexpr size_t DEFAULT_CHUNK = 4'000'000;
constexpr ptrdiff_t DEFAULT_PAD = 4'000;

template <size_t CHUNK = DEFAULT_CHUNK, ptrdiff_t PAD = DEFAULT_PAD>
class InputBuffer {
  static constexpr size_t BUF_SIZE = 2 * (CHUNK + PAD);
  static constexpr size_t MIDDLE = CHUNK + PAD;

  std::vector<char> m_buf;
  size_t m_active_start{0};
  size_t m_active_end{0};
  std::ifstream m_file{};

  std::mutex m_mut{};
  std::condition_variable m_cv_reader{};
  std::condition_variable m_cv_workers{};

  bool m_workers_done{true};
  bool m_chunk_ready{false};
  bool m_at_eof{false};
  bool m_shutdown{false};

  std::barrier<std::function<void()>> m_barrier;
  std::jthread m_reader_worker;

public:
  InputBuffer(std::filesystem::path const &fpath, size_t num_threads)
      : m_buf(BUF_SIZE), m_file(fpath),
        m_barrier(num_threads, [this]() noexcept { this->flip_buffers(); }) {
    m_reader_worker = std::jthread(InputBuffer::reader_func, this);
  }

  ~InputBuffer() {
    {
      std::lock_guard lck(m_mut);
      m_shutdown = true;
    }
    m_cv_reader.notify_all();
    m_cv_workers.notify_all();
  }

  static void reader_func(InputBuffer *self) {
    bool local_eof = false;
    while (!local_eof) {
      size_t bg_start = (self->m_active_start == 0) ? MIDDLE : 0;
      self->m_file.read(&self->m_buf[bg_start], MIDDLE);
      size_t read_bytes = self->m_file.gcount();

      size_t valid_end = bg_start + read_bytes;

      if (read_bytes == MIDDLE && self->m_file.peek() != EOF) {
        while (valid_end > bg_start && self->m_buf[valid_end - 1] != '\n') {
          valid_end++;
        }
        std::streamoff backtrack = (bg_start + read_bytes) - valid_end;
        self->m_file.seekg(-backtrack, std::ios::cur);
        self->m_file.clear();
      } else {
        local_eof = true;
      }

      std::unique_lock lck(self->m_mut);
      self->m_cv_reader.wait(
          lck, [&] { return self->m_workers_done || self->m_shutdown; });
      if (self->m_shutdown)
        return;

      self->m_active_start = bg_start;
      self->m_active_end = valid_end;
      self->m_at_eof = local_eof;
      self->m_chunk_ready = true;
      self->m_workers_done = false;

      self->m_cv_workers.notify_all();
      if (local_eof)
        break;
    }
  }

  char const *wait_for_chunk(size_t &out_size, bool &out_eof) {
    std::unique_lock lck(m_mut);
    m_cv_workers.wait(lck, [this] { return m_chunk_ready || m_shutdown; });

    out_size = m_active_end - m_active_start;
    out_eof = m_at_eof;
    return &m_buf[m_active_start];
  }

  void sync_workers() { m_barrier.arrive_and_wait(); }

  size_t active_end() {return m_active_end;}

private:
  void flip_buffers() {
    std::lock_guard lck(m_mut);
    m_chunk_ready = false;
    m_workers_done = true;
    m_cv_reader.notify_one();
  }
};
#endif
