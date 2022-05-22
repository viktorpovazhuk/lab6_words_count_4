//
// Created by petro on 27.03.2022.
//

#ifndef SERIAL_THREAD_FUNCTIONS_H
#define SERIAL_THREAD_FUNCTIONS_H

#include "time_measurement.h"
#include "StringHashCompare.h"
#include "file_info_t.h"
#include "boost/locale.hpp"

#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/concurrent_hash_map.h>
#include <oneapi/tbb/flow_graph.h>

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <locale>
#include <archive.h>
#include <archive_entry.h>

using MapStrInt = std::map<std::string, int>;
using StringTable = tbb::concurrent_hash_map<std::basic_string<char>, int, StringHashCompare>;

MapStrInt index_file(std::shared_ptr<file_info_t> file);

void merge_dicts(StringTable &globalDict, std::shared_ptr<MapStrInt> dict);

#endif //SERIAL_THREAD_FUNCTIONS_H
