#include<iostream>
#include<fstream>
#include<sstream>
#include "HierarchicalPrefetcher.h"
#include "nested_IO.h"
#include "fifo_map.h"

using nlohmann::fifo_map;
struct cache {
public:
	long hitsPage, missPage, ioHits, ioMiss, prefetchPage, prefetchHitPage, wastePagePoped, cacheSize, unit_size;
	fifo_map <long, map<string, long>> lru;
	cache()= default;
	cache(long size,long unit_size){
		hitsPage = 0;
		missPage = 0;
		ioHits = 0;
		ioMiss = 0;
		prefetchPage = 0;
		prefetchHitPage = 0;
		wastePagePoped = 0;
		this->cacheSize = size;
		this->unit_size = unit_size;
	}
	bool isPageExist(long key) {
		//Judge whether key is in cache
		if (lru.count(key))
			return true;
		else
			return false;
	}

	bool isPageHit(long key) {
		if (!this->isPageExist(key)) {
			this->missPage = this->missPage + 1;
			return false;
		}
		else {
			this->hitsPage = this->hitsPage + 1;
			//something
			return true;
		}
	}

	void insertPage(long key, bool isPretch) {
		if (this->isPageExist(key)) {
			//value = this->lru.erase
			fifo_map <long, map<string, long>>::iterator it = this->lru.find(key);
			//auto it = lru.find(key);
			map<string, long> value = it->second;
			this->lru.erase(it);
			this->lru[key] = value;
			//fifo_map <long, map<string, int>>
		}
		else {
			map<string, long> value;
			value["isPretch"] = isPretch;
			value["hitCnt"] = 0;
			this->lru[key] = value;
			if (isPretch = true) 
				this->prefetchPage = this->prefetchPage + 1;
		}
		while ((int)this->lru.size() > this->cacheSize) {
			fifo_map <long, map<string, long>>::iterator it = this->lru.begin();
			map<string, long> value = it->second;
			this->lru.erase(it);
			if ((value["isPretch"] == true) && (value["hitCnt"] == false))
				this->wastePagePoped = this->wastePagePoped + 1;

		}
	}

	//IO manipulation 

	vector<long> ioSplitToPages(map<string,long> lbaInfo) {
		vector<long> chunks;
		long lba = lbaInfo["lba"];
		long len = lbaInfo["len"];
		long begin = floor(lba / (this->unit_size));
		long end = floor((lba+len-1)/(this->unit_size));
		for (long chunkID = begin; chunkID <= end; chunkID++) {
			chunks.push_back(chunkID);
		}
		return chunks;
	}

	bool isIOExist(map<string, long> lbaInfo) {
		bool ioHit = true;
		vector<long> pages = this->ioSplitToPages(lbaInfo);
		for (int i = 0; i < (int)pages.size(); i++) {
			if (this->isPageExist(pages[i]) == false)
				ioHit = false;
		}
		return ioHit;
	}

	bool isIoHit(map<string, long> lbaInfo) {
		vector<long> pages = this->ioSplitToPages(lbaInfo);
		bool ioHit = true;
		for (int i = 0; i < (int)pages.size(); i++) {
			if (this->isPageHit(pages[i]) == false)
				ioHit = false;
		}
		if (ioHit)
			this->ioHits = this->ioHits + 1;
		else
			this->ioMiss = this->ioMiss + 1;
		return ioHit;
	}

	vector<long> getPrefetchedPages(vector<map<string, long>>pretchIOLbaInfos) {
		vector<long> prefetchedPages;
		for (int i = 0; i < (int)pretchIOLbaInfos.size(); i++) {
			map<string,long> pretchIOLbaInfo = pretchIOLbaInfos[i];
			if (this->isIOExist(pretchIOLbaInfo))
				continue;
			vector<long> pages = this->ioSplitToPages(pretchIOLbaInfo);
			for (int i = 0; i < (int)pages.size(); i++)
				prefetchedPages.push_back(pages[i]);
		}
		return prefetchedPages;
	}

	//Public APIs
	void updateCacheStatus(map<string, long>requestedIO, vector<map<string, long>> pretchedIOs) {
		vector<long> prefetchedPages = this->ioSplitToPages(requestedIO);
		vector<long> requestedPages = this->ioSplitToPages(requestedIO);
		for (int i = 0; i < (int)requestedPages.size(); i++) {
			long key = requestedPages[i];
			this->insertPage(key, true);
		}
		for (int i = 0; i < (int)prefetchedPages.size(); i++) {
			long key = prefetchedPages[i];
			this->insertPage(key, true);
		}
	}

	float getWasteRatio() {
		long wastePageInCache = 0;
		for (pair<long,map<string,long>> it : this->lru) {
			 map<string,long> value = it.second;
			 if ((value["isPretch"] == true) && (value["hitCnt"] == 0))
				 wastePageInCache += 1;
		}
		long wastePageSoFar = wastePageInCache + this->wastePagePoped;
		if (this->prefetchPage != 0)
			return (float)wastePageSoFar / (this->prefetchPage);
		return 0;
		//something
	}

	void showResult() {
		auto wasteRatio = this->getWasteRatio();
		auto pageHitRatio = ((float)this->hitsPage) / (this->hitsPage + this->missPage);
		auto ioHitRatio = ((float)this->ioHits) / (this->ioHits + this->ioMiss);
		cout << "lruCache:" << this->cacheSize << endl;
		cout << "ioHitRatio:" << ioHitRatio << endl;
		cout << "pageHitRatio:" << pageHitRatio << endl;
		cout << "wasteRatio:" << wasteRatio << endl;
		cout << "hitPage:" << this->hitsPage << endl;
		cout << "missPage" << this->missPage << endl;
		cout << "ioHit" << this->ioHits << endl;
		cout << "ioMiss" << this->ioMiss<<endl;
		//something  wrong
	}
};

vector<map<string,long>> ReadWorkloadFromTxt(string traceFilePath) {
	vector<map<string, long>>  result_vec;
	map<string, long>  result_map;
	vector<long> result_num;
	char buffer[256];
	ifstream examplefile(traceFilePath);
	if (!examplefile.is_open())
	{
		cout << "Error opening file"; exit(1);
	}
	while (!examplefile.eof())
	{
		examplefile.getline(buffer, 100);
		//cout << buffer << endl;
		string out;
		istringstream buffer1(buffer);
		int i = 0;
		while (buffer1 >> out) {
			switch (i) {
			case 0:
				result_map["time"] = atol(out.c_str());
				//cout << 1 << out << endl;
				break;
			case 1:
				result_map["lunId"] = 0;
				//cout << 2 << endl;
				break;
			case 2:
				result_map["lba"] = atol(out.c_str());
				//cout << 3 << out << endl;
				break;
			case 3:
				result_map["len"] = atol(out.c_str());
				//cout << 4 << out << endl;
				break;
			}
			i++;
		}
		result_vec.push_back(result_map);
	}
	examplefile.close();//something bad
	result_vec.pop_back();
	return result_vec;
}

void SimWorkload(vector<map<string, long>> workload, cache cacheInst, HierarchicalPrefetcher prefetcher) {
	int number_of_items_per_log_flush = 10000;
	map<string, long> lbaInfo;
	for (int i = 0; i < workload.size(); i++) {
		lbaInfo = workload[i];
		bool isIoHit = cacheInst.isIoHit(lbaInfo);
		vector<map<string, long>> prefetchLbaInfos;
		//something 符号转换可能有误
		map<string, unsigned long> IO_request = { {"time",lbaInfo["time"]},{"addr",lbaInfo["lba"]},{"size",lbaInfo["len"]} };
		vector<vector<long>> interval_list_to_prefetch = prefetcher.prefetch_on_IO_request(IO_request, isIoHit);//some bug
		//prefetchLbaInfos = 
		//something
		cacheInst.updateCacheStatus(lbaInfo, prefetchLbaInfos);
	}
	cacheInst.showResult();
	//store_log_in_json
}


void main() {
	string traceFile = ".//Data//1.txt";
	string log_path = ".//Logs";
	vector<map<string,long>> workload = ReadWorkloadFromTxt(traceFile);
	cache cacheInst = cache(2097152,64);
	long max_addr_gap_to_hold_correlation = 4 * 1024 * 1024 * 1024;
	cout << "workload size:" << workload.size() << endl;
	HierarchicalPrefetcher prefetcher = HierarchicalPrefetcher(log_path, max_addr_gap_to_hold_correlation);
	clock_t start_time = clock();	
	//something  wrong
	SimWorkload(workload, cacheInst, prefetcher);
	cout << 3 << endl;
	clock_t end_time = clock();
	double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
	cout << duration;
}
