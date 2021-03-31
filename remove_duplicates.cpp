#include "remove_duplicates.h"

#include <vector>
#include <iostream>

using namespace std;

void RemoveDuplicates(SearchServer& search_server){
	vector<int> duplicates_ids;
	set<vector<string>> documents_words;
	for (const auto id : search_server) {
		const auto word_freq = search_server.GetWordToFrequencies(id);
		vector<string> words(word_freq.size());
		int count = 0;
		for (const auto [word, _] : word_freq) {
			words[count] = word;
			++count;
		}
		if (documents_words.count(words)) {
			duplicates_ids.push_back(id);
		} else {
			documents_words.insert(words);
		}
	}

	for (const auto id : duplicates_ids) {
		search_server.RemoveDocument(id);
		cout << "Found duplicate document id " << id << endl;
	}
}