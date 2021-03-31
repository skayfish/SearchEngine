#pragma once

#include "document.h"

#include <iostream>
#include <vector>
#include <iterator>
#include <algorithm>

using namespace std::string_literals;

template <typename Iterator>
class IteratorRange {
public:
	IteratorRange(Iterator range_begin, Iterator range_end, size_t range_size)
		: begin_(range_begin)
		, end_(range_end)
		, size_(range_size) 
	{}
	Iterator begin() const {
		return begin_;
	}
	Iterator end() const {
		return end_;
	}
	size_t size() const {
		return size_;
	}
private:
	Iterator begin_, end_;
	size_t size_;
};

template <typename Iterator>
class Paginator {
public:
	Paginator(Iterator range_begin, Iterator range_end, size_t page_size) {
		auto it = range_begin;
		while (it != range_end) {
			const size_t current_page_size = std::min(page_size, static_cast<size_t>(distance(it, range_end)));
			const Iterator page_end = std::next(it, current_page_size);
			auto page = IteratorRange<Iterator>(it, page_end, current_page_size);
			pages_.push_back(page);
			advance(it, current_page_size);
		}
	}
	auto begin() const {
		return pages_.begin();
	}
	auto end() const {
		return pages_.end();
	}
	size_t size() const {
		return pages_.size();
	}
private:
	std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
	return Paginator(begin(c), end(c), page_size);
}

std::ostream& operator<<(std::ostream& os, const Document& doc) {
	os << "{ "s
		<< "document_id = "s << doc.id << ", "s
		<< "relevance = "s << doc.relevance << ", "s
		<< "rating = "s << doc.rating << " }"s;

	return os;
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& it_range) {
	for (Iterator it = it_range.begin(); it != it_range.end(); ++it) {
		os << *it;
	}

	return os;
}