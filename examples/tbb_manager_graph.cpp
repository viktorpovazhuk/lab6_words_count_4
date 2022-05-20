#include "../src/files_methods.h"
#include "../src/options_parser.h"
#include "../src/errors.h"
#include "../src/thread_functions.h"
#include "../src/write_in_file.h"
#include "../src/time_measurement.h"
#include "../src/thread_safe_queue.h"
#include "../src/ReadFile.h"
#include "../src/StringHashCompare.h"

#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/concurrent_hash_map.h>
#include "oneapi/tbb/flow_graph.h"

namespace fs = std::filesystem;

using namespace oneapi::tbb::flow;
using namespace oneapi::tbb;
using namespace std;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
using MapStrInt = std::map<std::string, int>;
using StringTable = oneapi::tbb::concurrent_hash_map<string, int, StringHashCompare>;

using read_files_node_t = multifunction_node<shared_ptr<string>, tuple<shared_ptr<ReadFile>, continue_msg>>;
using index_files_node_t = multifunction_node<shared_ptr<ReadFile>, tuple<shared_ptr<MapStrInt>, continue_msg>>;
using merge_dicts_node_t = function_node<shared_ptr<MapStrInt>, continue_msg>;

ReadFile read_file(shared_ptr<string> path);

MapStrInt index_file(shared_ptr<ReadFile> file);

void merge_dicts(shared_ptr<MapStrInt> dict, StringTable &globalDict);

int main() {
    string filesDirectory = "";
    size_t max_paths = 100;
    size_t max_files = 100;
    size_t max_dicts = 100;
    size_t maxFileSize = 1e+10;

    StringTable global_dict;

    graph index_graph;

    fs::recursive_directory_iterator itr(filesDirectory);
    fs::recursive_directory_iterator endItr;

    input_node<shared_ptr<string>> enum_files_node
            (index_graph, [&](flow_control &f_control) -> shared_ptr<string> {
                if (itr == endItr) {
                    f_control.stop();
                    return make_shared<string>("");
                }
                if ((itr->path().extension() == ".zip" || itr->path().extension() == ".txt") &&
                    fs::file_size(itr->path()) <= maxFileSize) {
                    // TODO: change fs::path to string in next functions
                    return make_shared<string>(move(itr->path().string()));
                }
                ++itr;
            });

    buffer_node<shared_ptr<string>> paths_buffer_in(index_graph);
    limiter_node<shared_ptr<string>> paths_limiter(index_graph, max_paths);
    buffer_node<shared_ptr<string>> paths_buffer_out(index_graph);

    read_files_node_t read_files_node(index_graph, unlimited,
                                      [&](shared_ptr<string> path, read_files_node_t::output_ports_type &outs) {
                                          ReadFile file = read_file(move(path));

                                          get<0>(outs).try_put(make_shared<ReadFile>(move(file)));
                                          get<1>(outs).try_put(continue_msg{});
                                      });

    buffer_node<shared_ptr<ReadFile>> files_buffer_in(index_graph);
    limiter_node<shared_ptr<ReadFile>> files_limiter(index_graph, max_files);
    buffer_node<shared_ptr<ReadFile>> files_buffer_out(index_graph);

    index_files_node_t index_files_node(index_graph, unlimited,
                                        [&](shared_ptr<ReadFile> file, index_files_node_t::output_ports_type &outs) {
                                            MapStrInt dict = index_file(move(file));

                                            get<0>(outs).try_put(make_shared<MapStrInt>(move(dict)));
                                            get<1>(outs).try_put(continue_msg{});
                                        });

    buffer_node<shared_ptr<MapStrInt>> dicts_buffer_in(index_graph);
    limiter_node<shared_ptr<MapStrInt>> dicts_limiter(index_graph, max_dicts);
    buffer_node<shared_ptr<MapStrInt>> dicts_buffer_out(index_graph);

    merge_dicts_node_t merge_dicts_node(index_graph, unlimited, [&](shared_ptr<MapStrInt> dict) {
        merge_dicts(move(dict), global_dict);

        return continue_msg{};
    });

    make_edge(enum_files_node, paths_buffer_in);
    make_edge(paths_buffer_in, paths_limiter);
    make_edge(paths_limiter, paths_buffer_out);
    make_edge(paths_buffer_out, read_files_node);

    make_edge(output_port<0>(read_files_node), files_buffer_in);
    make_edge(output_port<1>(read_files_node), paths_limiter.decrementer());
    make_edge(files_buffer_in, files_limiter);
    make_edge(files_limiter, files_buffer_out);
    make_edge(files_buffer_out, index_files_node);

    make_edge(output_port<0>(index_files_node), dicts_buffer_in);
    make_edge(output_port<1>(index_files_node), files_limiter.decrementer());
    make_edge(dicts_buffer_in, dicts_limiter);
    make_edge(dicts_limiter, dicts_buffer_out);
    make_edge(dicts_buffer_out, merge_dicts_node);

    make_edge(merge_dicts_node, dicts_limiter.decrementer());

    enum_files_node.activate();

    index_graph.wait_for_all();

    return 0;
}