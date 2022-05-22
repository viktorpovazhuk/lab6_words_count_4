//
// Created by vityha on 27.03.22.
//

#ifndef INDEX_FILES_FILES_METHODS_H
#define INDEX_FILES_FILES_METHODS_H

#include "time_measurement.h"
#include "file_info_t.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <oneapi/tbb/concurrent_queue.h>

namespace fs = std::filesystem;

file_info_t read_file(const std::shared_ptr<fs::path>& path);

#endif //INDEX_FILES_FILES_METHODS_H
