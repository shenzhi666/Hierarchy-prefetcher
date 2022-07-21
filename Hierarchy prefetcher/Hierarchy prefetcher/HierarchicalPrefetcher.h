#ifndef HIERARCHICALPREFETCHER_H
#define HIERARCHICALPREFETCHER_H
#include"nested_IO.h"

class nested_IO;
class HierarchicalPrefetcher {
public:
    nested_IO* root_NIO_ptr = nullptr;//changed
    //nested_IO  root_NIO = *root_NIO_ptr;
    nested_IO* last_updated_NIO_ptr = nullptr;
    //nested_IO last_updated_NIO = *last_updated_NIO_ptr;
    //nested_NIO无初始化可能会有问题
    long number_of_NIOs_appeared, number_of_NIOs_maintained, number_of_IO_request_handled;
    long max_addr_gap_to_hold_correlation;
    HierarchicalPrefetcher() = default;

    HierarchicalPrefetcher(string log_path, long max_addr_gap_to_hold_correlation);
    long generate_new_NIO_id();
    vector<vector<long>> prefetch_on_IO_request(map<string, unsigned long> IO_request, bool has_been_prefetched = false);
    void prune_earliest_NIO();

};
#endif