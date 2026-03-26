#include "city.h"
#include "input_buffer.h"
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>
#include <atomic>
#include <chrono>

void worker_thread(std::shared_ptr<InputBuffer<>> buf,
                   CityInfo::map_t &local_map, size_t tid,
                   size_t total_threads) {
  while (true) {
    size_t size;
    bool is_eof;
    const char *c_buf = buf->wait_for_chunk(size, is_eof);
    buf->sync_workers();

    if (size > 0) {
      size_t start = (size / total_threads) * tid;
      size_t end = (tid == total_threads - 1)
                       ? size
                       : (size / total_threads) * (tid + 1);

      if (tid > 0) {
        while (start < size && c_buf[start - 1] != '\n')
          start++;
      }
      if (tid < total_threads - 1) {
        while (end < size && c_buf[end - 1] != '\n')
          end++;
      }

      size_t curr = start;
      while (curr < end) {
        curr = CityInfo::parse_and_insert(c_buf, curr, end, local_map);
      }
    }

    if (is_eof)
      break;
  }
}

inline std::chrono::high_resolution_clock::time_point
get_current_time_fenced() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto res_time = std::chrono::high_resolution_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return res_time;
}

template <class D> inline long long to_ms(const D &d) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

int main(int argc, char **argv) {
  if (argc < 2)
    return 1;

  size_t num_threads = std::thread::hardware_concurrency() - 1;
  if (argc == 3) {
      num_threads = std::stoi(argv[2]);
  }

  auto start = get_current_time_fenced();
  auto buffer = std::make_shared<InputBuffer<>>(argv[1], num_threads);

  std::vector<CityInfo::map_t> maps(num_threads);
  std::vector<std::thread> threads;

  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker_thread, buffer, std::ref(maps[i]), i,
                         num_threads);
  }

  for (auto &t : threads)
    t.join();

  auto &final_map = maps[0];
  for (size_t i = 1; i < num_threads; ++i) {
    for (auto const &[name, stats] : maps[i]) {
      auto &target = final_map[name];
      if (target.count == 0) {
        target = stats;
      } else {
        target.min = std::min(target.min, stats.min);
        target.max = std::max(target.max, stats.max);
        target.sum += stats.sum;
        target.count += stats.count;
      }
    }
  }

  std::vector<std::pair<std::string, CityInfo>> entries(final_map.cbegin(),
                                                        final_map.cend());
  std::sort(
      entries.begin(), entries.end(),
      [](const auto &lhs, const auto &rhs) { return rhs.first >= lhs.first; });

  for (const auto &[key, val] : entries) {
    std::cout << key << "=" << std::fixed << std::setprecision(1)
              << static_cast<float>(val.min) / 10.0 << "/" << val.mean() / 10.0
              << "/" << static_cast<float>(val.max) / 10.0 << '\n';
  }
  auto end = get_current_time_fenced();

  std::cout << "TIME = " << to_ms(end - start) << std::endl;

  return 0;
}
