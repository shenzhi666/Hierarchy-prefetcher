#include "HierarchicalPrefetcher.h"
#include "nested_IO.h"

using namespace std;

HierarchicalPrefetcher::HierarchicalPrefetcher(string log_path, long max_addr_gap_to_hold_correlation)
{
    cout << "Create Hierarchical Prefetcher:" << endl;
    string prefetcher_name = "hierarchical";
    //log_level,log_path,logger
    //root_NIO  last_updated_NIO
    number_of_NIOs_appeared = -1, number_of_NIOs_maintained = -1, number_of_IO_request_handled = 0;
    this->max_addr_gap_to_hold_correlation = 8388608;
}

long HierarchicalPrefetcher::generate_new_NIO_id() {
    this->number_of_NIOs_appeared += 1;
    this->number_of_NIOs_maintained += 1;
    return this->number_of_NIOs_appeared;
}

vector<vector<long>> HierarchicalPrefetcher::prefetch_on_IO_request(map<string, unsigned long> IO_request, bool has_been_prefetched) {
    /*
    When a new IO IO_request comes, place it into exsiting NIO-tree,
        prune the resulting tree when necessary, and return the addr_interval_to_prefetch.*/
    this->number_of_IO_request_handled += 1;
    //place new IO into current NIO-tree
    //something
    nested_IO* sNIO_ptr = nested_IO(this).init_from_IO_request(IO_request, has_been_prefetched);
    if (this->root_NIO_ptr == nullptr)
        this->root_NIO_ptr = sNIO_ptr;
    else
        this->root_NIO_ptr->place_sNIO_in_NIO(*sNIO_ptr);//something bad
    //trigger pruning if the number of NIOs maintained exceeds the hard limitation

    while (this->number_of_NIOs_maintained > get<1>(profile))
        this->prune_earliest_NIO();
    return sNIO_ptr->intervals_to_prefetch;
}

void HierarchicalPrefetcher::prune_earliest_NIO() {
    //find and remove the NIO with the earliest end time
    nested_IO* earliest_NIO_at_lowest_level = this->root_NIO_ptr;
    while (earliest_NIO_at_lowest_level->child_vec.size() != 0)
        *earliest_NIO_at_lowest_level = earliest_NIO_at_lowest_level->child_vec[0];

    //[2]conduct prunding
    earliest_NIO_at_lowest_level->father_ptr->prune_earliest_child();
}