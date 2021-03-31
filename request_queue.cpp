#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
	: num_of_null_query_(0)
	, search_server_(search_server) 
{}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
	vector<Document> request = search_server_.FindTopDocuments(raw_query, status);
	AddRequest(QueryResult{ request });

	return request;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
	vector<Document> request = search_server_.FindTopDocuments(raw_query);
	AddRequest(QueryResult{ request });

	return request;
}

int RequestQueue::GetNoResultRequests() const {
	return num_of_null_query_;
}

void RequestQueue::AddRequest(const QueryResult& request) {
	if (static_cast<int>(requests_.size()) == sec_in_day_) {
		if (requests_.front().result.empty()) {
			--num_of_null_query_;
		}
		requests_.pop_front();
	}
	requests_.push_back(request);
	if (request.result.empty()) {
		++num_of_null_query_;
	}
}