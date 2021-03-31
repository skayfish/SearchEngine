#pragma once

#include "search_server.h"
#include <deque>
#include <vector>
#include <string>

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer& search_server);

	template <typename DocumentPredicate>
	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
		std::vector<Document> request = search_server_.FindTopDocuments(raw_query, document_predicate);
		AddRequest(QueryResult{ request });

		return request;
	}

	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
	std::vector<Document> AddFindRequest(const std::string& raw_query);
	int GetNoResultRequests() const;

private:
	struct QueryResult {
		std::vector<Document> result;
	};
	std::deque<QueryResult> requests_;
	constexpr static int sec_in_day_ = 1440;
	int num_of_null_query_;
	const SearchServer& search_server_;

	void AddRequest(const QueryResult& request);
};