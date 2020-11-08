#include "search_server.h"
#include <cmath>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {

}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("ID < 0"s);
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("A document with this ID already exists"s);
    }

    map<string, double> word_freqs_of_new_document;
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_freqs_of_new_document[word] += inv_word_count;
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

vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(
        raw_query,
        [](int document_id, DocumentStatus status_, int rating) {
            return status_ == DocumentStatus::ACTUAL;
        }
    );
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
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

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;

    
    for (const string& word : query.plus_words) {
        if (documents_.count(document_id) && documents_.at(document_id).word_freqs.count(word)) {
            matched_words.push_back(word);
        }
    }

    for (const string& word : query.minus_words) {
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

const map<string, double>& SearchServer::GetWordToFrequencies(int document_id) const {
    static const map<string, double> empty_result = map<string, double>();
    if (!documents_.count(document_id)) {
        return empty_result;
    }

    return documents_.at(document_id).word_freqs;
}

void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);
    for (size_t i = 0; i < document_ids_.size(); i++) {
        if (document_ids_[i] == document_id) {
            document_ids_.erase(document_ids_.begin() + i);
            break;
        }
    }
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    return none_of(
        word.begin(), 
        word.end(), 
        [](char c) {
            return c >= '\0' && c < ' ';
        }
    );
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("SplitIntoWordsNoStop: text contains invalid characters"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }

    return words;
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
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
    } else if (!IsValidWord(text)) {
        throw invalid_argument("ParseQueryWord: text contains invalid characters"s);
    }

    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query result;
    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }

    return result;
}

int SearchServer::CountDocumentsContainWord(const string& word) const {
    int sum = 0;
    for (const auto& [_, data] : documents_) {
        if (data.word_freqs.count(word)) {
            ++sum;
        }
    }

    return sum;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(static_cast<double>(GetDocumentCount()) / CountDocumentsContainWord(word));
}