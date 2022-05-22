//
// Created by petro on 27.03.2022.
//

#include "thread_functions.h"
#include "StringHashCompare.h"

#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/concurrent_hash_map.h>

namespace fs = std::filesystem;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
using MapStrInt = std::map<std::string, int>;
using StringTable = tbb::concurrent_hash_map<std::basic_string<char>, int, StringHashCompare>;


void merge_dicts(StringTable &globalDict, std::shared_ptr<MapStrInt> dict, double& merg_time) {
    oneapi::tbb::tick_count t0 = oneapi::tbb::tick_count::now();
    try {
            StringTable::accessor a;
            for (auto &i: *dict) {
                globalDict.insert(a, i.first);
                a->second += i.second;
                a.release();
/*                dict->erase(i.first);
                if (dict->empty() == 1){
                    break;
                }*/

            }
        } catch (std::error_code &e) {
            std::cerr << "Error code " << e << ". Occurred while merging dicts" << std::endl;
        }
    oneapi::tbb::tick_count t1 = oneapi::tbb::tick_count::now();
    merg_time += (t1-t0).seconds();


}


MapStrInt index_file(std::shared_ptr<file_info_t> file, double& ind_time){
    MapStrInt localDict;
    oneapi::tbb::tick_count t0 = oneapi::tbb::tick_count::now();
    if (file->extension == ".zip") {
        struct archive *a;
        struct archive_entry *entry;
        int r;
        int64_t length;
        void *buf;

        a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        std::ifstream raw_file(file->content, std::ios::binary);
        raw_file.seekg(0, std::ios::end);
        size_t file_size = raw_file.tellg();

        r = archive_read_open_filename(a, file->content.c_str(), file_size);
        if (r != ARCHIVE_OK) {
            exit(26);
        }
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            std::string name = archive_entry_pathname(entry), ext;
            int p = name.find('.');
            ext = name.substr(p, name.size());
            name = name.substr(0, p);

            if (ext == ".txt") {
                length = archive_entry_size(entry);
                buf = malloc(length);
                archive_read_data(a, buf, length);

                std::string content{(char *) buf};

                try {
                    file->content = boost::locale::normalize(file->content);
                    file->content = boost::locale::fold_case(file->content);

                } catch (std::error_code e) {
                    printf("Indexing error");
                }

                boost::locale::boundary::ssegment_index words(boost::locale::boundary::word, file->content.begin(),
                                                              file->content.end());
                words.rule(boost::locale::boundary::word_letters);

                for (auto &word: words) {
                    if (localDict.find(word) != localDict.end()) {
                        localDict.find(word)->second += 1;
                    } else {
                        localDict.insert({word, 1});
                    }
                }

            }
        }
    } else if (file->extension == ".txt"){
        try {
            file->content = boost::locale::normalize(file->content);
            file->content = boost::locale::fold_case(file->content);

        } catch (std::error_code e) {
            printf("Indexing error");
        }

        boost::locale::boundary::ssegment_index words(boost::locale::boundary::word, file->content.begin(),
                                                      file->content.end());
        words.rule(boost::locale::boundary::word_letters);
        for (auto &word: words) {
            if (localDict.find(word) != localDict.end()) {
                localDict.find(word)->second += 1;
            } else {
                localDict.insert({word, 1});
            }
        }
    }
    oneapi::tbb::tick_count t1 = oneapi::tbb::tick_count::now();
    ind_time += (t1-t0).seconds();
    return localDict;
}
