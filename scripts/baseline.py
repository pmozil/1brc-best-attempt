import sys
from collections import defaultdict


def simple_solution(input_file_path, output_file_path) -> float:
    """
    Returns execution time in milliseconds.
    """
    import time

    class StationData:
        def __init__(self):
            self.count = 0
            self.sum = 0.0
            self.min = float("inf")
            self.max = float("-inf")

        def update(self, temp: float):
            self.count += 1
            self.sum += temp

            if temp < self.min:
                self.min = temp

            if temp > self.max:
                self.max = temp

        def mean(self):
            return self.sum / self.count if self.count != 0 else 0.0

    def process_file(file_path, output_path):
        station_data = defaultdict(lambda: StationData())

        with open(file_path, "r", encoding="utf-8") as file:
            for row in file:
                station, temp = row.strip().split(";")
                temp = float(temp)
                station_data[station].update(temp)

        with open(output_path, "w", newline="", encoding="utf-8") as file:
            for station, data in sorted(station_data.items()):
                file.write(
                    f"{station}={data.min:.1f}/{data.mean():.1f}/{data.max:.1f}\n"
                )

    start_time = time.perf_counter()
    process_file(input_file_path, output_file_path)
    execution_time = time.perf_counter() - start_time

    return execution_time * 1e3


print(simple_solution(sys.argv[1], sys.argv[2]))
