#pragma once

#include <map>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <set>
#include <tuple>
#include <string>
#include <execution>
#include <string_view>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
	SearchServer() = default;

	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
		: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) 
	{
		using namespace std::string_literals;

		for (const auto& word : stop_words_) {
			if (!IsValidWord(std::execution::seq, word)) {
				throw std::invalid_argument("Stop-words contains invalid characters"s);
			}
		}
	}

	explicit SearchServer(const std::string& stop_words_text);
	explicit SearchServer(const std::string_view& stop_words_text);
	void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

	std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

	template <typename Policy>
	std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view& raw_query) const {
		return FindTopDocuments(
			policy,
			raw_query,
			[](int document_id, DocumentStatus status_, int rating) {
				return status_ == DocumentStatus::ACTUAL;
			}
		);
	}

	std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

	template <typename Policy>
	std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view& raw_query, DocumentStatus status) const {
		return FindTopDocuments(
			policy,
			raw_query,
			[status](int document_id, DocumentStatus status_, int rating) {
				return status_ == status;
			}
		);
	}
	template <typename Filter>
	std::vector<Document> FindTopDocuments(const std::string_view& raw_query, Filter filter) const {
		return FindTopDocuments(
			std::execution::seq,
			raw_query,
			filter
		);
	}

	template <typename Policy, typename Filter>
	std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view& raw_query, Filter filter) const {
		const Query query = ParseQuery(policy, raw_query);
		auto matched_documents = FindAllDocuments(policy, query, filter);

		std::sort(policy, matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
					return lhs.rating > rhs.rating;
				} else {
					return lhs.relevance > rhs.relevance;
				}
			}
		);
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}

		return matched_documents;
	}

	int GetDocumentCount() const;
	int GetDocumentId(int index) const;
	std::vector<int>::const_iterator begin() const;
	std::vector<int>::const_iterator end() const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;

	template <typename Policy>
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const Policy& policy, const std::string_view& raw_query, int document_id) const {
		const Query query = ParseQuery(policy, raw_query);
		std::vector<std::string_view> matched_words;

		for (const std::string_view& word : query.plus_words) {
			if (documents_.count(document_id) && documents_.at(document_id).word_freqs.count(word)) {
				matched_words.push_back(*words_.find(word));
			}
		}

		for (const std::string_view& word : query.minus_words) {
			if (documents_.count(document_id) && documents_.at(document_id).word_freqs.count(word)) {
				matched_words.clear();
				break;
			}
		}

		return {
			matched_words,
			documents_.at(document_id).status
		};
	}

	const std::map<std::string_view, double>& GetWordToFrequencies(int document_id) const;

	void RemoveDocument(int document_id);

	template <typename Policy>
	void RemoveDocument(const Policy& policy, int document_id) {
		if (documents_.count(document_id) == 0) {
			return;
		}

		documents_.erase(document_id);
		auto it = std::find(policy, document_ids_.begin(), document_ids_.end(), document_id);
		if (it != document_ids_.end()) {
			document_ids_.erase(it);
		}
	}

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		std::map<std::string_view, double> word_freqs;
	};
	std::set<std::string, std::less<>> words_;
	std::set<std::string, std::less<>> stop_words_;
	std::map<int, DocumentData> documents_;
	std::vector<int> document_ids_;

	bool IsStopWord(const std::string_view& word) const;

	template <typename Policy>
	static bool IsValidWord(const Policy& policy, const std::string_view& word) {
		return std::none_of(
			policy,
			word.begin(),
			word.end(),
			[](char c) {
				return c >= '\0' && c < ' ';
			}
		);
	}

	template <typename Policy>
	std::vector<std::string> SplitIntoWordsNoStop(const Policy& policy, const std::string_view& text) const {
		using namespace std;

		vector<string> words;
		for (const string_view& word : SplitIntoWords(text)) {
			if (!IsValidWord(policy, word)) {
				throw invalid_argument("SplitIntoWordsNoStop: text contains invalid characters"s);
			}
			if (!IsStopWord(word)) {
				words.push_back(static_cast<string>(word));
			}
		}

		return words;
	}

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	template <typename Policy>
	QueryWord ParseQueryWord(const Policy& policy, std::string_view text) const {
		using namespace std;

		if (text.empty()) {
			throw invalid_argument("ParseQueryWord: text is empty"s);
		}

		bool is_minus = false;
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}

		if (text.empty()) {
			throw invalid_argument("ParseQueryWord: text has only \'-\'"s);
		} else if (text[0] == '-') {
			throw invalid_argument("ParseQueryWord: text has double consecutive \'-\'"s);
		} else if (!IsValidWord(policy, text)) {
			throw invalid_argument("ParseQueryWord: text contains invalid characters"s);
		}

		return {
			text,
			is_minus,
			IsStopWord(text)
		};
	}

	struct Query {
		std::set<std::string, std::less<>> plus_words;
		std::set<std::string, std::less<>> minus_words;
	};

	template <typename Policy>
	Query ParseQuery(const Policy& policy, const std::string_view& text) const {
		Query result;
		for (const std::string_view& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(policy, word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					result.minus_words.insert(static_cast<std::string>(query_word.data));
				} else {
					result.plus_words.insert(static_cast<std::string>(query_word.data));
				}
			}
		}

		return result;
	}

	int CountDocumentsContainWord(const std::string_view& word)const;
	double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

	template <typename Filter>
	std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, Filter filter) const {
		std::map<int, double> document_to_relevance;
		for (const std::string_view& word : query.plus_words) {
			if (CountDocumentsContainWord(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto& [id, data] : documents_) {
				if (filter(id, data.status, data.rating) && data.word_freqs.count(word)) {
					document_to_relevance[id] += data.word_freqs.at(word) * inverse_document_freq;
				}
			}
		}

		for (const std::string_view& word : query.minus_words) {
			if (CountDocumentsContainWord(word) == 0) {
				continue;
			}

			for (const auto& [id, data] : documents_) {
				if (data.word_freqs.count(word)) {
					document_to_relevance.erase(id);
				}
			}
		}

		std::vector<Document> matched_documents;
		for (const auto& [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back(
				{
					document_id,
					relevance,
					documents_.at(document_id).rating
				}
			);
		}

		return matched_documents;
	}

	template <typename Filter>
	std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query, Filter filter) const {
		ConcurrentMap<int, double> concurrent_map_document_to_relevance(8);
		for (const std::string_view& word : query.plus_words) {
			if (CountDocumentsContainWord(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			std::for_each(
				policy,
				documents_.begin(), documents_.end(),
				[&concurrent_map_document_to_relevance, inverse_document_freq, filter, word](const auto& elem) {
					const auto& id = elem.first;
					const auto& data = elem.second;
					if (filter(id, data.status, data.rating) && data.word_freqs.count(word)) {
						concurrent_map_document_to_relevance[id].ref_to_value += data.word_freqs.at(word) * inverse_document_freq;
					}
				}
			);
		}

		auto document_to_relevance = concurrent_map_document_to_relevance.BuildOrdinaryMap();

		for (const std::string_view& word : query.minus_words) {
			if (CountDocumentsContainWord(word) == 0) {
				continue;
			}

			for (const auto& [id, data] : documents_) {
				if (data.word_freqs.count(word)) {
					document_to_relevance.erase(id);
				}
			}
		}

		std::vector<Document> matched_documents;
		for (const auto& [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back(
				{
					document_id,
					relevance,
					documents_.at(document_id).rating
				}
			);
		}

		return matched_documents;
	}
};