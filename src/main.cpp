#include "files_methods.h"
#include "options_parser.h"
#include "errors.h"
#include "thread_functions.h"
#include "write_in_file.h"
#include "time_measurement.h"
#include "file_info_t.h"
#include "StringHashCompare.h"

#include <iostream>
#include <memory>
#include <tuple>
#include <filesystem>
#include <string>
#include <fstream>
#include <utility>
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/concurrent_hash_map.h>
#include "oneapi/tbb/flow_graph.h"

namespace fs = std::filesystem;

using MapStrInt = std::map<std::string, int>;
using StringTable = oneapi::tbb::concurrent_hash_map<string, int, StringHashCompare>;

using read_files_node_t = tbb::flow::multifunction_node<std::shared_ptr<fs::path>, std::tuple<std::shared_ptr<file_info_t>, tbb::flow::continue_msg>>;
using index_files_node_t = tbb::flow::multifunction_node<std::shared_ptr<file_info_t>, std::tuple<std::shared_ptr<MapStrInt>, tbb::flow::continue_msg>>;
using merge_dicts_node_t = tbb::flow::function_node<std::shared_ptr<MapStrInt>, tbb::flow::continue_msg>;

MapStrInt index_file(std::shared_ptr<file_info_t> file);

int main(int argc, char *argv[]) {
    string configFilename;
    if (argc < 2) {
        configFilename = "../configs/index.cfg";
    } else {
        std::unique_ptr<command_line_options_t> command_line_options;
        try {
            command_line_options = std::make_unique<command_line_options_t>(argc, argv);
        }
        catch (std::exception &ex) {
            std::cerr << ex.what() << std::endl;
            return Errors::OPTIONS_PARSER;
        }
        configFilename = command_line_options->config_file;
    }

    std::unique_ptr<config_file_options_t> config_file_options;
    try {
        config_file_options = std::make_unique<config_file_options_t>(configFilename);
    }
    catch (OpenConfigFileException &ex) {
        std::cerr << ex.what() << std::endl;
        return Errors::OPEN_CFG_FILE;
    } catch (std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        return Errors::READ_CFG_FILE;
    }

    std::string fn = config_file_options->out_by_n;
    std::string fa = config_file_options->out_by_a;
    std::string files_directory = config_file_options->indir;
    int max_index_threads = config_file_options->indexing_threads;
    int max_merge_threads = config_file_options->merging_threads;
    int max_file_size = config_file_options->max_file_size;
    int max_paths = config_file_options->filenames_queue_max_size;
    int max_files = config_file_options->raw_files_queue_size;
    int max_dicts = config_file_options->dictionaries_queue_size;

    FILE *file;
    file = fopen(fn.c_str(), "r");
    if (file) {
        fclose(file);
    } else {
        std::ofstream MyFile(fn);
        MyFile.close();
    }

    file = fopen(fa.c_str(), "r");
    if (file) {
        fclose(file);
    } else {
        std::ofstream MyFile(fa);
        MyFile.close();
    }

    boost::locale::generator gen;
    std::locale::global(gen("en_US.UTF-8"));

    StringTable global_dict;

    tbb::flow::graph index_graph;

    fs::recursive_directory_iterator itr(files_directory);
    fs::recursive_directory_iterator end_itr;

    tbb::flow::input_node<std::shared_ptr<fs::path>> enum_files_node
            (index_graph, [&](tbb::flow_control &f_control) -> std::shared_ptr<fs::path> {
                if (itr == end_itr) {
                    f_control.stop();
                    return std::make_shared<fs::path>("");
                }
                if ((itr->path().extension() == ".zip" || itr->path().extension() == ".txt") &&
                    fs::file_size(itr->path()) <= max_file_size) {
                    return std::make_shared<fs::path>(itr->path());
                }
                ++itr;
            });

    tbb::flow::buffer_node<std::shared_ptr<fs::path>> paths_buffer_in(index_graph);
    tbb::flow::limiter_node<std::shared_ptr<fs::path>> paths_limiter(index_graph, max_paths);
    tbb::flow::buffer_node<std::shared_ptr<fs::path>> paths_buffer_out(index_graph);

    read_files_node_t read_files_node(index_graph, 1,
                                      [&](std::shared_ptr<fs::path> path, read_files_node_t::output_ports_type &outs) {

                                          std::get<0>(outs).try_put(std::make_shared<file_info_t>(std::move(read_file(std::move(path)))));
                                          std::get<1>(outs).try_put(tbb::flow::continue_msg{});
                                      });

    tbb::flow::buffer_node<std::shared_ptr<file_info_t>> files_buffer_in(index_graph);
    tbb::flow::limiter_node<std::shared_ptr<file_info_t>> files_limiter(index_graph, max_files);
    tbb::flow::buffer_node<std::shared_ptr<file_info_t>> files_buffer_out(index_graph);

    index_files_node_t index_files_node(index_graph, max_index_threads,
                                        [&](std::shared_ptr<file_info_t> file, index_files_node_t::output_ports_type &outs) {
                                            MapStrInt dict = index_file(move(file));

                                            std::get<0>(outs).try_put(std::make_shared<MapStrInt>(move(dict)));
                                            std::get<1>(outs).try_put(tbb::flow::continue_msg{});
                                        });

    tbb::flow::buffer_node<std::shared_ptr<MapStrInt>> dicts_buffer_in(index_graph);
    tbb::flow::limiter_node<std::shared_ptr<MapStrInt>> dicts_limiter(index_graph, max_dicts);
    tbb::flow::buffer_node<std::shared_ptr<MapStrInt>> dicts_buffer_out(index_graph);

    merge_dicts_node_t merge_dicts_node(index_graph, max_merge_threads, [&](std::shared_ptr<MapStrInt> dict) {
        merge_dicts(global_dict, std::move(dict));

        return tbb::flow::continue_msg{};
    });

    make_edge(enum_files_node, paths_buffer_in);
    make_edge(paths_buffer_in, paths_limiter);
    make_edge(paths_limiter, paths_buffer_out);
    make_edge(paths_buffer_out, read_files_node);

    make_edge(tbb::flow::output_port<0>(read_files_node), files_buffer_in);
    make_edge(tbb::flow::output_port<1>(read_files_node), paths_limiter.decrementer());
    make_edge(files_buffer_in, files_limiter);
    make_edge(files_limiter, files_buffer_out);
    make_edge(files_buffer_out, index_files_node);

    make_edge(tbb::flow::output_port<0>(index_files_node), dicts_buffer_in);
    make_edge(tbb::flow::output_port<1>(index_files_node), files_limiter.decrementer());
    make_edge(dicts_buffer_in, dicts_limiter);
    make_edge(dicts_limiter, dicts_buffer_out);
    make_edge(dicts_buffer_out, merge_dicts_node);

    make_edge(merge_dicts_node, dicts_limiter.decrementer());

    enum_files_node.activate();

    index_graph.wait_for_all();

//    auto timeStart = get_current_time_fenced();
//    TimePoint timeIndexingFinish, timeMergingFinish, timeReadingFinish, timeWritingFinish;
//
//    BoundedPathQueue paths;
//    paths.set_capacity(maxFilenamesQSize);
//    BoundedRFQueue filesContents;
//    filesContents.set_capacity(maxRawFilesQSize);
//    BoundedMapQueue dicts;
//    dicts.set_capacity(maxDictionariesQSize);
//
//    std::vector<std::thread> indexingThreads;
//    indexingThreads.reserve(numberOfIndexingThreads);
//    std::vector<std::thread> mergingThreads;
//    mergingThreads.reserve(numberOfMergingThreads);
//
//    StringTable globalDict;
//
//    int numOfWorkingIndexers = numberOfIndexingThreads;
//    std::mutex numOfWorkingIndexersMutex;
//
//#ifdef PARALLEL
//    std::thread filesEnumThread(findFiles, std::ref(config_file_options->indir), std::ref(paths));
//
//    std::thread filesReadThread(readFiles, std::ref(paths), std::ref(filesContents), maxFileSize,
//                                std::ref(timeReadingFinish));
//
//    startIndexingThreads(numberOfIndexingThreads, indexingThreads, filesContents, dicts, numOfWorkingIndexers,
//                         numOfWorkingIndexersMutex, timeIndexingFinish);
//
//    startMergingThreads(numberOfMergingThreads, mergingThreads, globalDict, dicts, timeMergingFinish);
//
//    if (filesEnumThread.joinable()) {
//        filesEnumThread.join();
//    }
//
//    if (filesReadThread.joinable()) {
//        filesReadThread.join();
//    }
//
//    try {
//        for (auto &thread: indexingThreads) {
//            if (thread.joinable()) {
//                thread.join();
//            }
//        }
//        for (auto &thread: mergingThreads) {
//            if (thread.joinable()) {
//                thread.join();
//            }
//        }
//    } catch (std::error_code &e) {
//        std::cerr << "Error code " << e << ". Occurred while joining threads." << std::endl;
//    }
//
//#else
//    paths.setMaxElements(999999);
//    filesContents.setMaxElements(999999);
//
//
//    findFiles(config_file_options->indir, paths);
//
//#ifdef PRINT_CONTENT
//    auto path = paths.deque();
//    paths.enque(path);
//    while (path != fs::path("")) {
//        cout << path << '\n';
//        path = paths.deque();
//        paths.enque(path);
//    }
//    cout << "--------------------------" << '\n';
//#endif
//
//    readFiles(paths, filesContents, timeReadingFinish);
//
//#ifdef PRINT_CONTENT
//    auto content = filesContents.deque();
//    filesContents.enque(content);
//    while (!content.empty()) {
//        cout << content << '\n';
//        content = filesContents.deque();
//        filesContents.enque(content);
//    }
//    cout << "--------------------------" << '\n';
//#endif
//
//    overworkFile(filesContents, wordsDict, globalDictMutex, timeFindingFinish);
//#endif

    auto timeWritingStart = get_current_time_fenced();

    writeInFiles(fn, fa, global_dict);

    auto timeWritingFinish = get_current_time_fenced();

    auto totalTimeFinish = get_current_time_fenced();

//    auto timeReading = to_us(timeReadingFinish - timeStart);
//    auto timeIndexing = to_us(timeIndexingFinish - timeStart);
//    auto timeMerging = to_us(timeMergingFinish - timeStart);
//    auto timeWriting = to_us(timeWritingFinish - timeWritingStart);
//    auto timeTotal = to_us(totalTimeFinish - timeStart);
//
//    std::cout << "Total=" << timeTotal << "\n"
//              << "Reading=" << timeReading << "\n";
//    if (timeMerging >= timeIndexing) {
//        std::cout << "Finding=" << timeMerging << "\n";
//    }
//    else {
//        std::cout << "Finding=" << timeIndexing << "\n";
//    }
//    std::cout << "Writing=" << timeWriting << endl;

    return 0;
}