//
// Created by petro on 27.03.2022.
//

#ifndef SERIAL_THREAD_FUNCTIONS_H
#define SERIAL_THREAD_FUNCTIONS_H

#include "time_measurement.h"
#include "StringHashCompare.h"
#include "file_info_t.h"
#include "boost/locale.hpp"

#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_hash_map.h>

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

void index_file(tbb::concurrent_bounded_queue<file_info_t> &filesContents, int &numOfWorkingIndexers, std::mutex& numOfWorkingIndexersMutex, tbb::concurrent_bounded_queue<std::map<std::basic_string<char>, int>> &dict,
                std::chrono::time_point<std::chrono::high_resolution_clock> &timeFindingFinish);

void merge_dicts(tbb::concurrent_hash_map<std::string, int, StringHashCompare> &globalDict, std::shared_ptr<std::map<std::string, int>> dict);

#endif //SERIAL_THREAD_FUNCTIONS_H
