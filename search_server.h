#pragma once

#include <map>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <set>
#include <tuple>
#include <string>
#include "document.h"
#include "string_processing.h"

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
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Stop-words contains invalid characters"s);
            }
        }
    }

    explicit SearchServer(const std::string& stop_words_text);
    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    template<typename Filter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Filter filter) const 
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
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
    std::vector<int>::const_iterator end() const ;
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
    const std::map<std::string, double>& GetWordToFrequencies(int document_id) const;
    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::map<std::string, double>word_freqs;
    };

    std::set<std::string> stop_words_;
    //std::map<std::string, std::map<int, double>> word_to_document_freqs_;/////
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;

    bool IsStopWord(const std::string& word) const;
    static bool IsValidWord(const std::string& word);
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const;
    int CountDocumentsContainWord(const std::string& word)const;
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename Filter>
    std::vector<Document> FindAllDocuments(const Query& query, Filter filter) const {
        std::map<int, double> document_to_relevance;
        for (const std::string& word : query.plus_words) {
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
        /*for (const std::string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (filter(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }*/

        for (const std::string& word : query.minus_words) {
            if (CountDocumentsContainWord(word) == 0) {
                continue;
            }

            for (const auto& [id, data] : documents_) {
                if (data.word_freqs.count(word)) {
                    document_to_relevance.erase(id);
                }
            }
        }
        /*for (const std::string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }*/


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