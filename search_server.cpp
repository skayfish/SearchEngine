#include <cmath>

#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const string_view& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("ID < 0"s);
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("A document with this ID already exists"s);
    }

    map<string_view, double> word_freqs_of_new_document;
    const vector<string> words = SplitIntoWordsNoStop(execution::seq, document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view& word : words) {
        if (!words_.count(word)) {
            words_.insert(static_cast<string>(word));
        }
        word_freqs_of_new_document[*words_.find(word)] += inv_word_count;
    }

    documents_.emplace(
        document_id,
        DocumentData{ 
            ComputeAverageRating(ratings), 
            status,
            word_freqs_of_new_document
        }
    );
    document_ids_.push_back(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const {
    return FindTopDocuments(
        std::execution::seq,
        raw_query,
        [](int document_id, DocumentStatus status_, int rating) {
            return status_ == DocumentStatus::ACTUAL;
        }
    );
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        std::execution::seq,
        raw_query, 
        [status](int document_id, DocumentStatus status_, int rating) {
            return status_ == status;
        }
    );
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::GetDocumentId(int index) const {
    if (index >= 0 && index < GetDocumentCount()) {
        return document_ids_[index];
    }

    throw out_of_range("GetDocumentId: index out of range"s);
}

vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(execution::seq, document_id);
}

const map<string_view, double>& SearchServer::GetWordToFrequencies(int document_id) const {
    static const map<string_view, double> empty_result = map<string_view, double>();
    if (!documents_.count(document_id)) {
        return empty_result;
    }

    return documents_.at(document_id).word_freqs;
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }

    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }

    return rating_sum / static_cast<int>(ratings.size());
}

int SearchServer::CountDocumentsContainWord(const string_view& word) const {
    int sum = 0;
    for (const auto& [_, data] : documents_) {
        if (data.word_freqs.count(word)) {
            ++sum;
        }
    }

    return sum;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(static_cast<double>(GetDocumentCount()) / CountDocumentsContainWord(word));
}