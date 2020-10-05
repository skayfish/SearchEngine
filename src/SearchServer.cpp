#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

using namespace std;


constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        } else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
	SearchServer() = default;

	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
	    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
	{
	    for (const auto& word: stop_words_) {
	    	if (!IsValidWord(word)) {
	        	throw invalid_argument("Stop-words contains invalid characters"s);
	        }
	    }
	}

	explicit SearchServer(const string& stop_words_text)
		: SearchServer(SplitIntoWords(stop_words_text)){
	}

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    	if (document_id < 0) {
    		throw invalid_argument("ID < 0"s);
    	}
    	if (documents_.count(document_id) > 0) {
    		throw invalid_argument("A document with this ID already exists"s);
    	}
    	const vector<string> words = SplitIntoWordsNoStop(document);

    	const double inv_word_count = 1.0 / words.size();
    	for (const string& word : words) {
    		word_to_document_freqs_[word][document_id] += inv_word_count;
    	}
    	documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    	document_ids_.push_back(document_id);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_, int rating) {
            return status_ == status;
        });
    }

    template<typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter filter) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter);

        sort(matched_documents.begin(), matched_documents.end(),
        		[](const Document& lhs, const Document& rhs) {
        			if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

	int GetDocumentId(int index) const {
		if (index >= 0 && index < GetDocumentCount()) {
			return document_ids_[index];
		}
		throw out_of_range("GetDocumentId: index out of range"s);
	}

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
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



    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
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

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
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

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(static_cast<double>(GetDocumentCount()) / word_to_document_freqs_.at(word).size());
    }

    template<typename Filter>
    vector<Document> FindAllDocuments(const Query& query, Filter filter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }

        return matched_documents;
    }
};


template<typename T>
ostream& operator<<(ostream& os, const vector<T>& arr) {
    os << "["s;
    bool isFirst = true;
    for (const auto& elem: arr) {
        if (isFirst){
            os << elem;
            isFirst = false;
        } else {
            os << ", "s << elem;
        }
    }
    os << "]"s;
    return os;
}

template<typename T>
ostream& operator<<(ostream& os, const set<T>& arr) {
    os << "{"s;
    bool isFirst = true;
    for (const auto& elem: arr) {
        if (isFirst){
            os << elem;
            isFirst = false;
        } else {
            os << ", "s << elem;
        }
    }
    os << "}"s;

    return os;
}

template<typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& arr) {
    os << "{"s;
    bool isFirst = true;
    for (const auto& [key, value]: arr) {
        if (isFirst){
            os << key << ": "s << value;
            isFirst = false;
        } else {
            os << ", "s << key << ": "s << value;
        }
    }
    os << "}"s;

    return os;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename F>
void RunTestImpl(F func, const string& func_str) {
    func();
    cerr << func_str << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl(func, #func)


void TestExcludeStopWordsFromAddedDocumentContent() {
    constexpr int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestFindAddedDocument() {
    constexpr int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};


    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT(found_docs.empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(0, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "dog", DocumentStatus::ACTUAL, ratings);
        auto found_docs = server.FindTopDocuments("cat in the city dog"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, 0);
        ASSERT_EQUAL(found_docs[1].id, 1);
    }
}

void TestMinusWords() {
    constexpr int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-cat"s);
        ASSERT(found_docs.empty() == true);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-cat in"s);
        ASSERT(found_docs.empty() == true);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -cat"s);
        ASSERT(found_docs.empty() == true);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-dog cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }
}

void TestComputeAverageRating() {
    SearchServer server;
    server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs.front().rating, (1 + 2 + 3) / 3);
}

void TestFindWithPredicat() {
    constexpr int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> found_docs =  server.FindTopDocuments(
                                        "cat"s,
                                        [](int document_id, DocumentStatus status, int rating){
                                            return document_id == 0;
                                        }
                                    );
        ASSERT_EQUAL(found_docs.size(), 0u);

        found_docs =  server.FindTopDocuments(
                                        "cat"s,
                                        [](int document_id, DocumentStatus status, int rating){
                                            return document_id == 1;
                                        }
                                    );
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> found_docs =  server.FindTopDocuments(
                                        "cat"s,
                                        [](int document_id, DocumentStatus status, int rating){
                                            return status == DocumentStatus::BANNED;
                                        }
                                    );
        ASSERT_EQUAL(found_docs.size(), 0u);

        found_docs =  server.FindTopDocuments(
                                        "cat"s,
                                        [](int document_id, DocumentStatus status, int rating){
                                            return status == DocumentStatus::ACTUAL;
                                        }
                                    );
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);

        found_docs =  server.FindTopDocuments(
                                        "cat"s,
                                        [](int document_id, DocumentStatus status, int rating){
                                            return rating == 3;
                                        }
                                    );
        ASSERT_EQUAL(found_docs.size(), 0u);

        found_docs =  server.FindTopDocuments(
                                        "cat"s,
                                        [](int document_id, DocumentStatus status, int rating){
                                            return rating == 2;
                                        }
                                    );
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }
}

void TestFindDocumentsByStatus() {
    constexpr int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;

        ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::BANNED).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::REMOVED).empty() == true);
    }

    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        ASSERT(server.FindTopDocuments("cat", DocumentStatus::BANNED).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::REMOVED).empty() == true);

        const auto found_docs =  server.FindTopDocuments("cat", DocumentStatus::ACTUAL);
        ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat",doc_id))), static_cast<int>(DocumentStatus::ACTUAL));
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);

        ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::REMOVED).empty() == true);

        const auto found_docs =  server.FindTopDocuments("cat", DocumentStatus::BANNED);
        ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat",doc_id))), static_cast<int>(DocumentStatus::BANNED));
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::IRRELEVANT, ratings);

        ASSERT(server.FindTopDocuments("cat", DocumentStatus::BANNED).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::REMOVED).empty() == true);

        const auto found_docs =  server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat",doc_id))), static_cast<int>(DocumentStatus::IRRELEVANT));
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::REMOVED, ratings);

        ASSERT(server.FindTopDocuments("cat", DocumentStatus::BANNED).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT).empty() == true);
        ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);

        const auto found_docs =  server.FindTopDocuments("cat", DocumentStatus::REMOVED);
        ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat",doc_id))), static_cast<int>(DocumentStatus::REMOVED));
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }
}

bool NearlyEquals(double a, double b) {
    return abs(a - b) < 1e-6;
}

void TestDocumentRelevanceCalculation() {
    SearchServer server;
    server.AddDocument(0, "one"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "two three"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "three four five"s, DocumentStatus::ACTUAL, {1});
    {
        const auto docs = server.FindTopDocuments("one"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 0);
        ASSERT(NearlyEquals(docs[0].relevance, (log(server.GetDocumentCount() * 1.0 / 1) * 1.0)));
    }
    {
        const auto docs = server.FindTopDocuments("four"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 2);
        ASSERT(NearlyEquals(docs[0].relevance, (log(server.GetDocumentCount() * 1.0 / 1) * (1.0 / 3))));
    }
    {
        const auto docs = server.FindTopDocuments("four five"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 2);
        ASSERT(NearlyEquals(docs[0].relevance, (log(server.GetDocumentCount() * 1.0 / 1) * (2.0 / 3))));
    }
    {
        const auto docs = server.FindTopDocuments("one three"s);
        ASSERT_EQUAL(docs.size(), 3u);
        ASSERT_EQUAL(docs[0].id, 0);
        ASSERT(NearlyEquals(docs[0].relevance, (log(server.GetDocumentCount() * 1.0 / 1) * (1.0))));
        ASSERT_EQUAL(docs[1].id, 1);
        ASSERT(NearlyEquals(docs[1].relevance, (log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 2))));
        ASSERT_EQUAL(docs[2].id, 2);
        ASSERT(NearlyEquals(docs[2].relevance, (log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 3))));
    }
}

void TestMatchingDocuments() {
    {
        SearchServer server("a the and"s);
        server.AddDocument(0, "a quick brown fox jumps over the lazy dog"s, DocumentStatus::BANNED, {1, 2, 3});
        const auto [words, status] = server.MatchDocument("a lazy cat and the brown dog"s, 0);
        set<string> matched_words;
        for (const auto& word : words) {
            matched_words.insert(word);
        }
        const set<string> expected_matched_words = {"lazy"s, "dog"s, "brown"s};
        ASSERT_EQUAL(matched_words, expected_matched_words);
        ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::BANNED));
    }

    {
        SearchServer server;

        server.AddDocument(0, "black cat is in the city"s, DocumentStatus::ACTUAL, {1});

        {
            const auto [words, status] = server.MatchDocument("black cat"s, 0);
            ASSERT_EQUAL(count(words.begin(), words.end(), "cat"s), 1);
            ASSERT_EQUAL(count(words.begin(), words.end(), "black"s), 1);
        }

        {
            const auto [words, status] = server.MatchDocument("cat -black"s, 0);
            ASSERT(words.empty());
        }
    }
}

void TestSortMatchedDocumentsByRelevanceDescending() {
    SearchServer server;
    server.AddDocument(0, "white cat with black tail"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "cat eats milk"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "dog likes milk"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "dog sees a cat near the tree"s, DocumentStatus::ACTUAL, {1});

    {
        const auto docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(docs.size(), 3u);
        ASSERT(docs.front().relevance > docs.back().relevance);
        for (size_t i = 1; i < docs.size(); ++i) {
            ASSERT(docs[i - 1].relevance >= docs[i].relevance);
        }
    }
}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestFindAddedDocument);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestFindWithPredicat);
    RUN_TEST(TestFindDocumentsByStatus);
    RUN_TEST(TestDocumentRelevanceCalculation);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortMatchedDocumentsByRelevanceDescending);
}


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);

    return 0;
}
