//
// Created by vityha on 27.03.22.
//

#include "files_methods.h"
#include "file_info_t.h"

#include <oneapi/tbb/concurrent_queue.h>

file_info_t read_file(const std::shared_ptr<fs::path>& path) {
    file_info_t file_info;
    if (path->extension() == ".txt") {
        std::ifstream ifs(*path);
        std::string content{(std::istreambuf_iterator<char>(ifs)),
                            (std::istreambuf_iterator<char>())};
        file_info.content = std::move(content);
        file_info.extension = path->extension();
        file_info.filename = path->filename();
    }
    else if (path->extension() == ".zip") {
        file_info.content = std::move(path->string());
        file_info.extension = path->extension();
        file_info.filename = path->filename();
    }
    return file_info;
}