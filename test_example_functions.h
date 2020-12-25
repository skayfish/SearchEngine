#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <random>
#include <string_view>

#include "search_server.h"
#include "document.h"

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& arr);

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& arr);

template<typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::map<K, V>& arr);

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint);

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
    unsigned line, const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename F>
void RunTestImpl(F func, const std::string& func_str);

#define RUN_TEST(func)  RunTestImpl(func, #func)


void TestExcludeStopWordsFromAddedDocumentContent();
void TestFindAddedDocument();
void TestMinusWords();
void TestComputeAverageRating();
void TestFindWithPredicat();
void TestFindDocumentsByStatus();

bool NearlyEquals(double a, double b);

void TestDocumentRelevanceCalculation();
void TestMatchingDocuments();
void TestSortMatchedDocumentsByRelevanceDescending();
void TestProcessQueries();

void TestSearchServer();


void PrintDocument(const Document& document);
void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);
void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string& query);

std::string GenerateWord(std::mt19937& generator, int max_length);
std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length);
std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int max_word_count);
std::vector<std::string> GenerateQueries(std::mt19937& generator, const std::vector<std::string>& dictionary, int query_count, int max_word_count);

template <typename QueriesProcessor>
void TestParallelQueries(std::string_view mark, QueriesProcessor processor, const SearchServer& search_server, const std::vector<std::string>& queries);

#define TEST_PROCESSOR(processor) TestParallelQueries(#processor, processor, search_server, queries)