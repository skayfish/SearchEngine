#include <algorithm>
#include <execution>
#include <utility>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
	vector<vector<Document>> result(queries.size());

	transform(
		execution::par,
		queries.begin(),
		queries.end(),
		result.begin(),
		[&search_server](const string& item) {
			return search_server.FindTopDocuments(item);
		}
	);

	return result;
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
	vector<Document> result;

	auto tmp = ProcessQueries(search_server, queries);
	size_t sz = 0;
	for (const auto& elem : tmp) {
		sz += elem.size();
	}
	result.reserve(sz);
	for (auto& vec_doc : tmp) {
		for (auto& doc : vec_doc) {
			result.push_back(move(doc));
		}
	}

	return result;
}