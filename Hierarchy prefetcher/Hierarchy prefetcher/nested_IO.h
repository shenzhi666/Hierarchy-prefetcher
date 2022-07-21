#ifndef NESTED_IO_H
#define NESTED_IO_H

#include <iostream>
#include <string>
#include<algorithm>
#include <map>
#include<vector>
#include<tuple>
#include "HierarchicalPrefetcher.h"
using namespace std;

static map<string, double> default_log_std = {
    {"time",2},{"addr",1}
};
static map<string, double> min_log_std = {
    {"time",1},{"addr",0.5}
};
static auto profile = make_tuple(true, double(1000), default_log_std, min_log_std, 0.1, double(10));
static const string log_path = "qaq";

class HierarchicalPrefetcher;

class nested_IO {
public:
    HierarchicalPrefetcher* prefetcher_ptr;
    long NIO_id;
    nested_IO* father_ptr = nullptr;
    vector<nested_IO> child_vec;
    map <string, int> child_addr_change_record = { {"increasing",1},{"decreasing",1} };
    map <string, double> gap_distribution = {
        {"is_time_meaningful",1},
        {"log_avg_time",-2},
        {"log_std_time",-2},
        {"avg_time",-2},
        {"log_avg_addr",-2},
        {"log_std_addr",-2},
        {"avg_addr",-2}
    };
    //volume variables
    long time_range[2] = { 0,0 };
    long addr_range[2] = { 0,0 };
    map <string, unsigned long> volume_info_long;
    map <string, bool> volume_info_bool;
    //prefetching record
    vector<vector<long>> intervals_to_prefetch;

    bool operator==(const nested_IO& other) {
        if (this->NIO_id == other.NIO_id)
            return true;
        else
            return false;
    }

    nested_IO() = default;
    nested_IO(HierarchicalPrefetcher* prefetcher_ptr);
    nested_IO *init_from_IO_request(map<string, unsigned long> IO_request, bool has_been_prefetched);
    nested_IO *init_from_child_NIO(nested_IO child_NIO);
    void place_sNIO_in_NIO(nested_IO sNIO);
    tuple<tuple<string, nested_IO, map<string, double>>, double, double> get_placement_in_top_level(nested_IO sNIO);
    tuple<tuple<string, nested_IO, map<string, double>>, double> get_best_placement_in_all_levels(nested_IO sNIO);
    tuple<map<string, double>, nested_IO, map<string, double>> get_gap_info_to_sNIO(nested_IO sNIO, map <string, double> gap_distribution);
    map<string, double> quantify_gap_locality(map <string, double> gap, map <string, double> gap_distribution);
    bool check_necessity_to_be_explored(nested_IO sNIO, double bestever_closeness, map <string, double> gap_distribution);
    void place_NIO_as_child(nested_IO sNIO, map <string, double> gap);
    void update_gap_distribution(map <string, double> gap, string key = "time");
    void update_volume_info(nested_IO sNIO);
    void try_continuous_prefetching(nested_IO sNIO);
    void prune_earliest_child();
};
#endif