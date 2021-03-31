#include "test_example_functions.h"
#include "log_duration.h"
#include "process_queries.h"

#include <cmath>

using namespace std;

template <typename T>
ostream& operator<<(ostream& os, const vector<T>& arr) {
	os << "["s;
	bool isFirst = true;
	for (const auto& elem : arr) {
		if (isFirst) {
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
	for (const auto& elem : arr) {
		if (isFirst) {
			os << elem;
			isFirst = false;
		} else {
			os << ", "s << elem;
		}
	}
	os << "}"s;

	return os;
}

template <typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& arr) {
	os << "{"s;
	bool isFirst = true;
	for (const auto& [key, value] : arr) {
		if (isFirst) {
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
void AssertEqualImpl(
	const T& t, 
	const U& u, 
	const string& t_str, 
	const string& u_str, 
	const string& file,
	const string& func, 
	unsigned line, 
	const string& hint
) {
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

void AssertImpl(
	bool value, 
	const string& expr_str, 
	const string& file, 
	const string& func,
	unsigned line, 
	const string& hint
) {
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

template <typename F>
void RunTestImpl(F func, const string& func_str) {
	func();
	cerr << func_str << " OK" << endl;
}

void TestExcludeStopWordsFromAddedDocumentContent() {
	constexpr int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
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
	const vector<int> ratings = { 1, 2, 3 };

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
	const vector<int> ratings = { 1, 2, 3 };

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
	server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
	const auto found_docs = server.FindTopDocuments("cat"s);
	ASSERT_EQUAL(found_docs.size(), 1u);
	ASSERT_EQUAL(found_docs.front().rating, (1 + 2 + 3) / 3);
}

void TestFindWithPredicat() {
	constexpr int doc_id = 1;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		vector<Document> found_docs = server.FindTopDocuments(
			"cat"s,
			[](int document_id, DocumentStatus status, int rating) {
				return document_id == 0;
			}
		);
		ASSERT_EQUAL(found_docs.size(), 0u);

		found_docs = server.FindTopDocuments(
			"cat"s,
			[](int document_id, DocumentStatus status, int rating) {
				return document_id == 1;
			}
		);
		ASSERT_EQUAL(found_docs.size(), 1u);
		ASSERT_EQUAL(found_docs[0].id, doc_id);
	}

	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		vector<Document> found_docs = server.FindTopDocuments(
			"cat"s,
			[](int document_id, DocumentStatus status, int rating) {
				return status == DocumentStatus::BANNED;
			}
		);
		ASSERT_EQUAL(found_docs.size(), 0u);

		found_docs = server.FindTopDocuments(
			"cat"s,
			[](int document_id, DocumentStatus status, int rating) {
				return status == DocumentStatus::ACTUAL;
			}
		);
		ASSERT_EQUAL(found_docs.size(), 1u);
		ASSERT_EQUAL(found_docs[0].id, doc_id);

		found_docs = server.FindTopDocuments(
			"cat"s,
			[](int document_id, DocumentStatus status, int rating) {
				return rating == 3;
			}
		);
		ASSERT_EQUAL(found_docs.size(), 0u);

		found_docs = server.FindTopDocuments(
			"cat"s,
			[](int document_id, DocumentStatus status, int rating) {
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
	const vector<int> ratings = { 1, 2, 3 };

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

		const auto found_docs = server.FindTopDocuments("cat", DocumentStatus::ACTUAL);
		ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat", doc_id))), static_cast<int>(DocumentStatus::ACTUAL));
		ASSERT_EQUAL(found_docs.size(), 1u);
		ASSERT_EQUAL(found_docs[0].id, doc_id);
	}

	{
		SearchServer server;

		server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);

		ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);
		ASSERT(server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT).empty() == true);
		ASSERT(server.FindTopDocuments("cat", DocumentStatus::REMOVED).empty() == true);

		const auto found_docs = server.FindTopDocuments("cat", DocumentStatus::BANNED);
		ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat", doc_id))), static_cast<int>(DocumentStatus::BANNED));
		ASSERT_EQUAL(found_docs.size(), 1u);
		ASSERT_EQUAL(found_docs[0].id, doc_id);
	}

	{
		SearchServer server;

		server.AddDocument(doc_id, content, DocumentStatus::IRRELEVANT, ratings);

		ASSERT(server.FindTopDocuments("cat", DocumentStatus::BANNED).empty() == true);
		ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);
		ASSERT(server.FindTopDocuments("cat", DocumentStatus::REMOVED).empty() == true);

		const auto found_docs = server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT);
		ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat", doc_id))), static_cast<int>(DocumentStatus::IRRELEVANT));
		ASSERT_EQUAL(found_docs.size(), 1u);
		ASSERT_EQUAL(found_docs[0].id, doc_id);
	}

	{
		SearchServer server;

		server.AddDocument(doc_id, content, DocumentStatus::REMOVED, ratings);

		ASSERT(server.FindTopDocuments("cat", DocumentStatus::BANNED).empty() == true);
		ASSERT(server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT).empty() == true);
		ASSERT(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).empty() == true);

		const auto found_docs = server.FindTopDocuments("cat", DocumentStatus::REMOVED);
		ASSERT_EQUAL(static_cast<int>(get<DocumentStatus>(server.MatchDocument("cat", doc_id))), static_cast<int>(DocumentStatus::REMOVED));
		ASSERT_EQUAL(found_docs.size(), 1u);
		ASSERT_EQUAL(found_docs[0].id, doc_id);
	}
}

bool NearlyEquals(double a, double b) {
	return abs(a - b) < 1e-6;
}

void TestDocumentRelevanceCalculation() {
	SearchServer server;
	server.AddDocument(0, "one"s, DocumentStatus::ACTUAL, { 1 });
	server.AddDocument(1, "two three"s, DocumentStatus::ACTUAL, { 1 });
	server.AddDocument(2, "three four five"s, DocumentStatus::ACTUAL, { 1 });
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
		server.AddDocument(0, "a quick brown fox jumps over the lazy dog"s, DocumentStatus::BANNED, { 1, 2, 3 });
		const auto [words, status] = server.MatchDocument("a lazy cat and the brown dog"s, 0);
		set<string> matched_words;
		for (const auto& word : words) {
			matched_words.insert(static_cast<string>(word));
		}
		const set<string> expected_matched_words = { "lazy"s, "dog"s, "brown"s };
		ASSERT_EQUAL(matched_words, expected_matched_words);
		ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::BANNED));
	}

	{
		SearchServer server;
		server.AddDocument(0, "black cat is in the city"s, DocumentStatus::ACTUAL, { 1 });
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
	server.AddDocument(0, "white cat with black tail"s, DocumentStatus::ACTUAL, { 1 });
	server.AddDocument(1, "cat eats milk"s, DocumentStatus::ACTUAL, { 1 });
	server.AddDocument(2, "dog likes milk"s, DocumentStatus::ACTUAL, { 1 });
	server.AddDocument(3, "dog sees a cat near the tree"s, DocumentStatus::ACTUAL, { 1 });

	{
		const auto docs = server.FindTopDocuments("cat"s);
		ASSERT_EQUAL(docs.size(), 3u);
		ASSERT(docs.front().relevance > docs.back().relevance);
		for (size_t i = 1; i < docs.size(); ++i) {
			ASSERT(docs[i - 1].relevance >= docs[i].relevance);
		}
	}
}

void TestProcessQueries() {
	std::mt19937 generator;
	const auto dictionary = GenerateDictionary(generator, 10000, 25);
	const auto documents = GenerateQueries(generator, dictionary, 100'000, 10);

	SearchServer search_server(dictionary[0]);
	for (size_t i = 0; i < documents.size(); ++i) {
		search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
	}

	const auto queries = GenerateQueries(generator, dictionary, 10'000, 7);
	TEST_PROCESSOR(ProcessQueries);
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
	RUN_TEST(TestProcessQueries);
}

void PrintDocument(const Document& document) {
	cout << "{ "s
	     << "document_id = "s << document.id        << ", "s
	     << "relevance = "s   << document.relevance << ", "s
	     << "rating = "s      << document.rating    << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
	cout << "{ "s
	     << "document_id = "s << document_id              << ", "s
	     << "status = "s      << static_cast<int>(status) << ", "s
	     << "words ="s;
	for (const string_view& word : words) {
		cout << ' ' << static_cast<string>(word);
	}
	cout << "}"s << endl;
}

void AddDocument(
	SearchServer& search_server,
	int document_id, 
	const string& document, 
	DocumentStatus status, 
	const vector<int>& ratings
) {
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

string GenerateWord(std::mt19937& generator, int max_length) {
	const int length = uniform_int_distribution(1, max_length)(generator);
	string word;
	word.reserve(length);
	for (int i = 0; i < length; ++i) {
		word.push_back(static_cast<char>(uniform_int_distribution(int('a'), int('z'))(generator)));
	}

	return word;
}

vector<string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length) {
	vector<string> words;
	words.reserve(word_count);
	for (int i = 0; i < word_count; ++i) {
		words.push_back(GenerateWord(generator, max_length));
	}
	sort(words.begin(), words.end());
	words.erase(unique(words.begin(), words.end()), words.end());

	return words;
}

string GenerateQuery(std::mt19937& generator, const vector<string>& dictionary, int max_word_count) {
	const int word_count = uniform_int_distribution(1, max_word_count)(generator);
	string query;
	for (int i = 0; i < word_count; ++i) {
		if (!query.empty()) {
			query.push_back(' ');
		}
		query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
	}

	return query;
}

vector<string> GenerateQueries(std::mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
	vector<string> queries;
	queries.reserve(query_count);
	for (int i = 0; i < query_count; ++i) {
		queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
	}

	return queries;
}

template <typename QueriesProcessor>
void TestParallelQueries(string_view mark, QueriesProcessor processor, const SearchServer& search_server, const vector<string>& queries) {
	LOG_DURATION(mark);
	const auto documents_lists = processor(search_server, queries);
}