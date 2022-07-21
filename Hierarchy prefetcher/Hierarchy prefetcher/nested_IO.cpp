#include "HierarchicalPrefetcher.h"
#include "nested_IO.h"

using namespace std;

nested_IO::nested_IO(HierarchicalPrefetcher* prefetcher_ptr) {
    //此处可能会有问题，prefetcher初始化问题
    this->prefetcher_ptr = prefetcher_ptr;
    this->NIO_id = prefetcher_ptr->generate_new_NIO_id();
    
    //nested_IO *child_list_ptr
    //volume variables
    //volume_info
}

nested_IO *nested_IO::init_from_IO_request(map<string, unsigned long> IO_request, bool has_been_prefetched) {
    //nested_IO;
    this->time_range[0] = IO_request["time"], this->time_range[1] = IO_request["time"];
    this->addr_range[1] = IO_request["addr"], this->addr_range[1] = IO_request["addr"] + IO_request["size"];
    this->volume_info_long = {
        {"time_span",0},
        {"addr_span",IO_request["size"]},
        {"effective_addr_span",IO_request["size"]},
        {"sNIO_number",1},
        {"prefetched_sNIO_number",has_been_prefetched}
    };
    this->volume_info_bool = {
        {"max_addr_active",true},
        {"min_addr_active",true}
    };
    return this;
}

nested_IO *nested_IO::init_from_child_NIO(nested_IO child_NIO) {
    //log_level
    //structure variables
    //vector<nested_IO>;
    this->father_ptr = child_NIO.father_ptr;
    this->child_vec.push_back(child_NIO);
    this->gap_distribution = child_NIO.gap_distribution;//拷贝构造函数问题

    //init volume variables

    this->time_range[0] = child_NIO.time_range[0];
    this->time_range[1] = child_NIO.time_range[1];
    this->addr_range[0] = child_NIO.addr_range[0];
    this->addr_range[1] = child_NIO.addr_range[1];
    //数组不允许赋值
    this->volume_info_long = child_NIO.volume_info_long;
    this->volume_info_bool = child_NIO.volume_info_bool;

    //update child and father affected
    child_NIO.father_ptr = this->father_ptr;
    if (this->father_ptr == nullptr)
        this->prefetcher_ptr->root_NIO_ptr = this;
    //else {
    //    auto iter_beg = this->father_ptr->child_vec.begin();
    //    auto iter_end = this->father_ptr->child_vec.end();
    //    int index_changed = find(iter_beg, iter_end, child_NIO) - iter_beg;
    //    this->father_ptr->child_vec[index_changed] = *this;
    //    //something
    //}
    else {
        auto iter_beg = this->father_ptr->child_vec.begin();
        auto iter_end = this->father_ptr->child_vec.end();
        auto index_changed = find(iter_beg, iter_end, child_NIO) - iter_beg;
        this->father_ptr->child_vec[index_changed] = *this;
    }
    return this;
}

void nested_IO::place_sNIO_in_NIO(nested_IO sNIO) {
    //Each time a new single-IO_request NIO (sNIO) comes, place it into the NIO-tree under root_NIO.
    //[1]work out the best placement based on locality
    //a. fast pass (consecutive sNIOs often belong to to the same father NIO)
    tuple<string, nested_IO, map<string, double>> best_placement;
    nested_IO NIO_to_host_sNIO;
    int j = 0;
    int i = 0;
    if (prefetcher_ptr->last_updated_NIO_ptr != nullptr) {
        cout << "true  or  false:" << (bool)(prefetcher_ptr->last_updated_NIO_ptr == nullptr) << endl;
        i++;
        cout << "i:" << i << endl;
        cout << "NIO_id:" << NIO_id << endl;
    }
    else
    {
        j++;
        cout << "true  or  false:" << (bool)(prefetcher_ptr->last_updated_NIO_ptr == nullptr) << endl;
        cout << "j:" << j << endl;
        cout << "NIO_id:" << NIO_id << endl;
    }
        
    if (this->prefetcher_ptr->last_updated_NIO_ptr != nullptr) {
        //cout << "id1" << NIO_id<<endl;
        //cout << "id2" << this->prefetcher_ptr->last_updated_NIO_ptr->NIO_id;
        tuple<string, nested_IO, map<string, double>> tentative_placement = get<0>(this->prefetcher_ptr->last_updated_NIO_ptr->get_placement_in_top_level(sNIO));
        if (get<0>(tentative_placement) != "append")
            best_placement = tentative_placement;
        else
            best_placement = get<0>(this->get_best_placement_in_all_levels(sNIO));
    }
    else
        best_placement = get<0>(this->get_best_placement_in_all_levels(sNIO));
    //b. regular process that searches all the candidate positions

    if (get<0>(best_placement) == "append") {
        NIO_to_host_sNIO = *(get<1>(best_placement).father_ptr);
        this->prefetcher_ptr->last_updated_NIO_ptr = get<1>(best_placement).father_ptr;
    }
    else {
        NIO_to_host_sNIO = *(nested_IO(this->prefetcher_ptr).init_from_child_NIO(get<1>(best_placement)));
        this->prefetcher_ptr->last_updated_NIO_ptr = nullptr;
    }
    //something
    NIO_to_host_sNIO.place_NIO_as_child(sNIO, get<2>(best_placement));
    //[3]trigger prefetching
    NIO_to_host_sNIO.try_continuous_prefetching(sNIO);
}

tuple<tuple<string, nested_IO, map<string, double>>, double, double> nested_IO::get_placement_in_top_level(nested_IO sNIO) {
    /*Determine the ultimate placement mode (parallel, merge, append) if placing sNIO under self,
        based on whether the absolute confidence level is significant enough (<0.05, 0.05-0.95, >0.95). */
    tuple<string, nested_IO, map<string, double>> placement;
    //[1]find gap_info: (predecessor, gap, locality)
    map<string,double> gap = get<0>(this->get_gap_info_to_sNIO(sNIO, this->gap_distribution));//something bad
    nested_IO predecessor = get<1>(this->get_gap_info_to_sNIO(sNIO, this->gap_distribution));
    map<string,double> locality = get<2>(this->get_gap_info_to_sNIO(sNIO, this->gap_distribution));
    map<string, double> gap_for_parallel_placement = {
        {"time",max(gap["time"],this->gap_distribution["avg_time"])},
        {"addr",max(gap["addr"],this->gap_distribution["avg_addr"])}
    };
    double min_closeness = min(locality["time_closeness"], locality["addr_closeness"]);
    double max_closeness = max(locality["time_closeness"], locality["addr_closeness"]);
    //[2]determine placement mode based on (min/max-)locality
    if (min_closeness < 0.05)
        //try refuse first since overclose require overclose in both dimensions
        //mode,predecessor,gap
        auto placement = make_tuple("parallel", *this, gap_for_parallel_placement);
    else if (max_closeness > 0.05)
        auto placement = make_tuple("merge", predecessor, gap);
    else
        auto placement = make_tuple("append", predecessor, gap);
    //[3]prepare closeness and remoteness for upper and lower levels
    double closeness = locality["closeness"];
    double remoteness = locality["remoteness"];
    if (this->father_ptr != nullptr) {
        //涉及到lambda函数
        //calculate max
        vector <size_t> vec1;
        for (size_t i = 0; i < child_vec.size(); ++i) {
            vec1.push_back(this->father_ptr->child_vec[i].gap_distribution["log_avg_addr"]);
        }
        int max_Position = max_element(vec1.begin(), vec1.end()) - vec1.begin();
        map<string, double> max_gap_distribution_among_brothers = this->father_ptr->child_vec[max_Position].gap_distribution;
        map<string, double> locality_with_brothers_referred = this->quantify_gap_locality(gap, max_gap_distribution_among_brothers);
        double closeness = locality_with_brothers_referred["closeness"];
    }
    auto result = make_tuple(placement, closeness, remoteness);
    return result;
}

tuple<tuple<string, nested_IO, map<string, double>>, double> nested_IO::get_best_placement_in_all_levels(nested_IO sNIO) {
    /*
        Recursively find the best placement among all the candidates based on Empirical-Risk-Estimation (ERM).
        That is, the best position is the one that would incur the highest risk if not chosen.
        Here 'risk' is the confidence level of hypothesis testing for accepting gap_to_sNIO under self.gap_distribution.
    */
    tuple<string, nested_IO, map<string, double>> lower_best_placement;
    tuple<string, nested_IO, map<string, double>>  best_placement;
    double lower_best_closeness = 0;
    //[1]get the placement at the top level granularity
    tuple<string, nested_IO, map<string, double>> top_placement = get<0>(this->get_placement_in_top_level(sNIO));
    double top_closeness = get<1>(this->get_placement_in_top_level(sNIO));
    double top_remoteness = get<2>(this->get_placement_in_top_level(sNIO));
    //[2]get the best lower-level-placement that can yield the largest closeness
    //something
    if (top_remoteness < top_closeness) {
        for (vector<nested_IO>::reverse_iterator child = this->child_vec.rbegin(); child != child_vec.rend(); ++child) {
            map <string, double> gap_distribution;
            if (child->check_necessity_to_be_explored(sNIO, max(lower_best_closeness, top_remoteness), gap_distribution)) {
                tuple<string, nested_IO, map<string, double>> child_placement = get<0>(child->get_best_placement_in_all_levels(sNIO));
                double child_closeness = get<1>(child->get_best_placement_in_all_levels(sNIO));
                if (child_closeness > lower_best_closeness) {
                    lower_best_closeness = child_closeness;
                    lower_best_placement = child_placement;
                }
            }

        }
    }
    /*[3]determine best placement by comparing top_remoteness (i.e., risk_to_refuse_top_level_placement)
            with lower_best_closeness (i.e., risk_to_refuse_lower_level_placement)*/
    if (top_remoteness >= lower_best_closeness)
        best_placement = top_placement;
    else
        best_placement = lower_best_placement;
    tuple<tuple<string, nested_IO, map<string, double>>, double> result = make_tuple(best_placement, top_closeness);
    return result;

}

tuple<map<string, double>, nested_IO, map<string, double>> nested_IO::get_gap_info_to_sNIO(nested_IO sNIO, map <string, double> gap_distribution) {
    map<string, double> locality = { {"closeness",-1} };
    nested_IO predecessor;
    map<string, double> gap = { {"time",0},{"addr",0} };
    tuple<map<string, double>, nested_IO, map<string, double>> result;

    //[1] trivial case: corner extension where the gap can be directly determined by time/addr_range
    if (this->volume_info_bool["max_addr_active"] && sNIO.addr_range[1] >= this->addr_range[1]) {
        gap["time"] = sNIO.time_range[0] - this->time_range[1];
        gap["addr"] = max(sNIO.addr_range[0] - this->addr_range[1], long(0));
    }
    else if (this->volume_info_bool["min_addr_active"] && sNIO.addr_range[0] <= this->addr_range[0]) {
        gap = {
            {"time",sNIO.time_range[0] - this->time_range[1]},
            {"addr",max(this->addr_range[0] - sNIO.addr_range[1],long(0))}
        };
    }
    else if (this->volume_info_bool["max_addr_active"] && this->volume_info_bool["min_addr_active"]) {
        gap = {
            {"time",sNIO.time_range[0] - this->time_range[1]},
            {"addr",0}
        };
    }
    if (!gap.empty()) {
        //predecessor = none
        if (!this->child_vec.empty()) {
            predecessor = this->child_vec[-1];
            gap = {
                {"time",max(predecessor.gap_distribution["avg_time"],gap["time"])},
                {"addr",max(predecessor.gap_distribution["avg_addr"],gap["addr"])}
            };
            locality = this->quantify_gap_locality(gap, gap_distribution);
        }
        tuple<map<string, double>, nested_IO, map<string, double>> result = make_tuple(gap, predecessor, locality);
        return result;
    }
        //[2] non-trivial case: check each child to identify the predecessing NIO whose gap to sNIO is the shortest
        //something 
    for (vector<nested_IO>::reverse_iterator child = this->child_vec.rbegin(); child != child_vec.rend(); ++child) {
        if (child->check_necessity_to_be_explored(sNIO, locality["closeness"], gap_distribution)) {
            auto gap_to_child = get<0>(child->get_gap_info_to_sNIO(sNIO, gap_distribution));//sometinh need to change
            gap_to_child = {
                {"time",max(child->gap_distribution["avg_time"],gap_to_child["time"])},
                {"addr",max(child->gap_distribution["avg_addr"],gap_to_child["addr"])}
            };
            auto locality_to_child = this->quantify_gap_locality(gap_to_child, gap_distribution);
            if (locality_to_child["closeness"] >= locality["closeness"]) {
                locality = locality_to_child;
                predecessor = *child;
                gap = gap_to_child;
            }

        }
    }
    result = make_tuple(gap, predecessor, locality);
    return result;
    }


map<string, double> nested_IO::quantify_gap_locality(map <string, double> gap, map <string, double> gap_distribution) {
    /*
    Respectively calculate the confidence level of accepting gap in time and addr dimension.
        Returns: locality = {'closeness', 'remoteness', 'time_closeness', 'addr_closeness'}*/
        //transfer to log scale
    map<string, double> log_gap;
    map <string, double> deviations;
    map<string, double> closenesses;
    map<string, double> locality;
    log_gap = {
        {"time",(gap["time"] > 0) ? log10(gap["time"]) : -1},
        {"addr",(gap["addr"] > 0) ? log10(gap["addr"]) : -1}
    };
    if (gap_distribution["log_std_time"] == -2)//boundary case
        closenesses = {
            {"time",0},
            {"addr",0}
    };
    else {
        deviations = {
            {"time",log_gap["time"] - gap_distribution["log_avg_time"] / gap_distribution["log_std_time"]},
            {"addr",log_gap["addr"] - gap_distribution["log_avg_addr"] / gap_distribution["log_std_addr"]}
        };
        closenesses = {
             {"time",1 - (1 + erf(deviations["time"] / sqrt(2))) / 2},
             {"addr",1 - (1 + erf(deviations["addr"] / sqrt(2))) / 2}
        };
    }
    //If the addr_gap is larger than self.prefetcher.max_addr_gap_to_hold_correlation, then ignore the time influence.
    if (gap_distribution["is_time_meaningful"] == 0)
        closenesses["time"] = closenesses["addr"];
    locality = {
        {"closeness",sqrt(closenesses["time"] * closenesses["addr"])},
        {"remoteness",sqrt((1 - closenesses["time"] * closenesses["addr"]))},
        {"time_closeness",closenesses["time"]},
        {"addr_closeness",closenesses["addr"]}
    };
    return locality;

}

bool nested_IO::check_necessity_to_be_explored(nested_IO sNIO, double bestever_closeness, map <string, double> gap_distribution) {
    /*Get a min possible gap and check whether even that faked shortest gap can
        yield a closeness level better than bestever_closeness benchmark.
        If not, then return that it is no longer necessary to check self.*/
    double min_possible_addr_gap = 0;
    map<string, double> shortest_possible_gap;
    this->father_ptr->child_vec;
    //calculate max
    vector <size_t> vec1;
    for (size_t i = 0; i < child_vec.size(); ++i) {
        vec1.push_back(this->father_ptr->child_vec[i].gap_distribution["log_avg_addr"]);
    }
    int max_Position = max_element(vec1.begin(), vec1.end()) - vec1.begin();

    if (bestever_closeness == 0)
        return true;
    if (gap_distribution.empty()) {
        if (this->father_ptr != nullptr)
            gap_distribution = this->father_ptr->child_vec[max_Position].gap_distribution;
        else
            gap_distribution = this->gap_distribution;
    }
    //get shortest possible gap
    if (this->addr_range[1] < sNIO.addr_range[0])
        min_possible_addr_gap = sNIO.addr_range[0] - this->addr_range[1];
    else if (this->addr_range[0] > sNIO.addr_range[1]) {
        min_possible_addr_gap = this->addr_range[0] - sNIO.addr_range[1];
        shortest_possible_gap = {
            {"addr",min_possible_addr_gap},
            {"time",max(sNIO.time_range[0] - this->time_range[1],long(0))}
        };
    }
    map<string, double> locality = this->quantify_gap_locality(shortest_possible_gap, gap_distribution);
    if (locality["closeness"] < bestever_closeness)
        return false;
    return true;
}

void nested_IO::place_NIO_as_child(nested_IO sNIO, map <string, double> gap) {
    //update structure
    sNIO.father_ptr = this;
    this->child_vec.push_back(sNIO);
    if (sNIO.addr_range[0] > this->child_vec[-1].addr_range[1])
        this->child_addr_change_record["increasing"] += 1;
    if (sNIO.addr_range[1] < this->child_vec[-1].addr_range[0])
        this->child_addr_change_record["decreasing"] += 1;
    this->update_gap_distribution(gap);

    //update volume info
    this->update_volume_info(sNIO);
}

void nested_IO::update_gap_distribution(map <string, double> gap, string key) {
    auto m = this->child_vec.size() - 2;
    double u = this->gap_distribution["log_avg_" + key];
    double v = this->gap_distribution["log_std_" + key];
    double x = gap[key] > 0 ? log10(gap[key]) : -1;
    double new_u, new_v;
    if (m == 0) {
        new_u = x;
        new_v = get<2>(profile)[key];
        if (key == "addr" && gap[key] > this->prefetcher_ptr->max_addr_gap_to_hold_correlation)
            this->gap_distribution["is_time_meaningful"] = 0;
    }
    else {
        new_u = (m * u + x) / (m + 1);
        new_v = sqrt((m * (pow(v, 2) + pow(new_u - u, 2)) + pow(new_u - x, 2)) / (m + 1));
        if (new_v < get<3>(profile)[key])
            new_v = get<3>(profile)[key];
    }
    this->gap_distribution["log_avg" + key] = new_u;
    this->gap_distribution["log_std" + key] = new_v;
    this->gap_distribution["avg_" + key] = pow(10, new_u);
    if (key == "time")
        this->update_gap_distribution(gap, key = "addr");
}

void nested_IO::update_volume_info(nested_IO sNIO) {
    //this->time_range[0] = this

    this->addr_range[0] = min(sNIO.addr_range[0], this->addr_range[0]);
    this->addr_range[1] = max(sNIO.addr_range[1], this->addr_range[1]);

    this->volume_info_long["time_span"] = (this->time_range[1]) - (this->time_range[0]);
    this->volume_info_long["addr_span"] = (this->addr_range[1]) - (this->addr_range[0]);
    this->volume_info_long["effective_addr_span"] += sNIO.volume_info_long["addr_span"];
    this->volume_info_long["sNIO_number"] = this->volume_info_long["sNIO_number"] + 1;
    this->volume_info_long["prefetched_sNIO_number"] += sNIO.volume_info_bool["prefetched_sNIO_number"];

    this->volume_info_bool["max_addr_active"] = (sNIO.addr_range[1] == this->addr_range[1]) ? true : false;
    this->volume_info_bool["min_addr_active"] = (sNIO.addr_range[0] == this->addr_range[0]) ? true : false;

    if (this->father_ptr != nullptr)
        this->father_ptr->update_volume_info(sNIO);
}

void nested_IO::try_continuous_prefetching(nested_IO sNIO) {
    /* When self.addr_range is dense enough after appending sNIO as new child,
        then trigger continuous prefetching that prefetches a neighborhood region of sNIO. */

    int prefetch_size, increasing_prefetch_size, decreasing_prefetch_size;
    double density = double(this->volume_info_long["effective_addr_span"]) / double(this->volume_info_long["addr_span"]);
    if (density > get<4>(profile)) {
        prefetch_size = int(density * get<5>(profile)) * sNIO.volume_info_long["addr_span"];
        //prefetching in addr-increasing direction, with size proportional to the addr-increasing appearance
        increasing_prefetch_size = prefetch_size * (this->child_addr_change_record["increasing"])
            / ((this->child_addr_change_record["increasing"]) + (this->child_addr_change_record["decreasing"]));
        if (increasing_prefetch_size > 0)
            sNIO.intervals_to_prefetch.push_back({ sNIO.addr_range[1] ,sNIO.addr_range[1] + increasing_prefetch_size });

        // prefetching in addr - decreasing direction, with size proportional to the addr - decreasing appearance

        decreasing_prefetch_size = prefetch_size * (this->child_addr_change_record["decreasing"])
            / ((this->child_addr_change_record["increasing"]) + (this->child_addr_change_record["decreasing"]));
        if (decreasing_prefetch_size > 0)
            sNIO.intervals_to_prefetch.push_back({ sNIO.addr_range[0] - decreasing_prefetch_size, sNIO.addr_range[0] });
    }
}

void nested_IO::prune_earliest_child() {
    nested_IO NIO_updated;
    nested_IO* NIO_updated_ptr;
    size_t new_NIO_index;
    //[1]remove from tree
    this->child_vec.erase(this->child_vec.begin());
    this->prefetcher_ptr->number_of_NIOs_maintained -= 1;

    //[2] handle the removal impact
    if (this->child_vec.size() == 0)
        this->father_ptr->prune_earliest_child();
    else {
        this->time_range[0] = this->child_vec[0].time_range[0];
        NIO_updated = *this;
        NIO_updated_ptr = this;
        while (NIO_updated.father_ptr != nullptr) {
            new_NIO_index = 0;
            while ((new_NIO_index < (NIO_updated.father_ptr->child_vec.size() - 1)) &&
                NIO_updated.father_ptr->child_vec[new_NIO_index + 1].time_range[0] < NIO_updated.time_range[0])
                new_NIO_index += 1;
            if (new_NIO_index != 0) {
                NIO_updated.father_ptr->child_vec.erase(NIO_updated.father_ptr->child_vec.begin());
                NIO_updated.father_ptr->child_vec.insert(NIO_updated.father_ptr->child_vec.begin() + new_NIO_index, NIO_updated);
            }
            NIO_updated_ptr = NIO_updated.father_ptr;
            NIO_updated.time_range[0] = NIO_updated.child_vec[0].time_range[0];
        }

    }
}