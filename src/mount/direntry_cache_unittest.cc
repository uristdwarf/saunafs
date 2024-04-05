/*

   Copyright 2017 Skytechnology sp. z o.o.
   Copyright 2023 Leil Storage OÜ

   This file is part of SaunaFS.

   SaunaFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   SaunaFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with SaunaFS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/platform.h"
#include "mount/direntry_cache.h"

#include <gtest/gtest.h>
#include <iostream>

class DirEntryCacheIntrospect : public DirEntryCache {
public:
	DirEntryCacheIntrospect(uint64_t timeout)
		: DirEntryCache(timeout) {
	}

	LookupSet::const_iterator lookup_begin() const {
		return lookup_set_.begin();
	}

	IndexSet::const_iterator index_begin() const {
		return index_set_.begin();
	}

	InodeMultiset::const_iterator inode_begin() const {
		return inode_multiset_.begin();
	}

	LookupSet::const_iterator lookup_end() const {
		return lookup_set_.end();
	}

	InodeMultiset::const_iterator inode_end() const {
		return inode_multiset_.end();
	}
};

TEST(DirEntryCache, Basic) {
	DirEntryCacheIntrospect cache(5000000);

	Attributes dummy_attributes;
	dummy_attributes.fill(0);
	Attributes attributes_with_6 = dummy_attributes;
	Attributes attributes_with_9 = dummy_attributes;
	attributes_with_6[0] = 6;
	attributes_with_9[0] = 9;
	auto current_time = cache.updateTime();
	cache.insertSequence(
		SaunaClient::Context(0, 0, 0, 0), 9,
		std::vector<DirectoryEntry>{
			{0, 1, 7, "a1", dummy_attributes},
			{1, 2, 8, "a2", dummy_attributes},
			{2, 3, 9, "a3", dummy_attributes}
		}, current_time
	);
	cache.insertSequence(
		SaunaClient::Context(1, 2, 0, 0), 11,
		std::vector<DirectoryEntry>{
			{7, 8, 5, "a1", dummy_attributes},
			{8, 9, 4, "a2", dummy_attributes},
			{9, 10, 3, "a3", dummy_attributes}
		}, current_time
	);
	cache.insertSequence(
		SaunaClient::Context(0, 0, 0, 0), 9,
		std::vector<DirectoryEntry>{
			{1, 2, 11, "a4", dummy_attributes},
			{2, 3, 13, "a3", attributes_with_9},
			{3, 4, 12, "a2", attributes_with_6}
		}, current_time
	);

	std::vector<std::tuple<int, int, int, std::string>> index_output {
		std::make_tuple(7, 9, 0, "a1"),
		std::make_tuple(11, 9, 1, "a4"),
		std::make_tuple(13, 9, 2, "a3"),
		std::make_tuple(12, 9, 3, "a2"),
		std::make_tuple(5, 11, 7, "a1"),
		std::make_tuple(4, 11, 8, "a2"),
		std::make_tuple(3, 11, 9, "a3")
	};

	std::vector<std::tuple<int, int, int, std::string>> lookup_output {
		std::make_tuple(7, 9, 0, "a1"),
		std::make_tuple(12, 9, 3, "a2"),
		std::make_tuple(13, 9, 2, "a3"),
		std::make_tuple(11, 9, 1, "a4"),
		std::make_tuple(5, 11, 7, "a1"),
		std::make_tuple(4, 11, 8, "a2"),
		std::make_tuple(3, 11, 9, "a3")
	};

	auto index_it = cache.index_begin();
	auto index_output_it = index_output.begin();
	ASSERT_EQ(cache.size(), index_output.size());
	while (index_it != cache.index_end()) {
		ASSERT_EQ(*index_output_it, std::make_tuple(index_it->inode, index_it->parent_inode, index_it->index, index_it->name));
		index_it++;
		index_output_it++;
	}
	ASSERT_TRUE(index_output_it == index_output.end());

	auto lookup_it = cache.lookup_begin();
	auto lookup_output_it = lookup_output.begin();
	ASSERT_EQ(cache.size(), lookup_output.size());
	while (lookup_it != cache.lookup_end()) {
		ASSERT_EQ(*lookup_output_it, std::make_tuple(lookup_it->inode, lookup_it->parent_inode, lookup_it->index, lookup_it->name));
		lookup_it++;
		lookup_output_it++;
	}
	ASSERT_TRUE(lookup_output_it == lookup_output.end());

	auto by_inode_it = cache.find(SaunaClient::Context(0, 0, 0, 0), 12);
	ASSERT_NE(by_inode_it, cache.inode_end());
	ASSERT_EQ(by_inode_it->attr[0], 6);
	by_inode_it++;
	ASSERT_NE(by_inode_it, cache.inode_end());
	ASSERT_EQ(by_inode_it->attr[0], 9);
	by_inode_it++;
	ASSERT_EQ(by_inode_it, cache.inode_end());
}

TEST(DirEntryCache, Repetitions) {
	DirEntryCacheIntrospect cache(5000000);

	Attributes dummy_attributes;
	dummy_attributes.fill(0);
	auto current_time = cache.updateTime();

	cache.insertSequence(SaunaClient::Context(0, 0, 0, 0), 9, std::vector<DirectoryEntry>{{0, 1, 7, "a1", dummy_attributes}}, current_time);
	cache.insertSequence(SaunaClient::Context(0, 0, 0, 0), 9, std::vector<DirectoryEntry>{{1, 2, 7, "a1", dummy_attributes}}, current_time);
	cache.removeOldest(5);
}

TEST(DirEntryCache, RandomOrder) {
	DirEntryCacheIntrospect cache(5000000);

	Attributes dummy_attributes;
	dummy_attributes.fill(0);
	Attributes attributes_with_6 = dummy_attributes;
	Attributes attributes_with_9 = dummy_attributes;
	attributes_with_6[0] = 6;
	attributes_with_9[0] = 9;
	auto current_time = cache.updateTime();
	cache.insertSequence(
		SaunaClient::Context(0, 0, 0, 0), 9,
		std::vector<DirectoryEntry>{
			{0, 1, 7, "a1", dummy_attributes},
			{1, 2, 8, "a2", dummy_attributes},
			{2, 3, 9, "a3", dummy_attributes}
		}, current_time
	);
	cache.insertSequence(
		SaunaClient::Context(0, 0, 0, 0), 9,
		std::vector<DirectoryEntry>{
			{7, 8, 5, "a4", dummy_attributes},
			{8, 9, 4, "a5", dummy_attributes},
			{9, 10, 3, "a6", dummy_attributes}
		}, current_time
	);
	cache.insertSequence(
		SaunaClient::Context(0, 0, 0, 0), 9,
		std::vector<DirectoryEntry>{
			{7, 0, 5, "a4", dummy_attributes},
			{0, 2, 7, "a2", dummy_attributes},
			{2, 3, 8, "a1", dummy_attributes}
		}, current_time
	);
	std::vector<std::tuple<int, int, int, std::string>> index_output {
		std::make_tuple(7, 9, 0, "a2"),
		std::make_tuple(8, 9, 2, "a1"),
		std::make_tuple(5, 9, 7, "a4"),
		std::make_tuple(4, 9, 8, "a5"),
		std::make_tuple(3, 9, 9, "a6")
	};

	std::vector<std::tuple<int, int, int, std::string>> lookup_output {
		std::make_tuple(8, 9, 2, "a1"),
		std::make_tuple(7, 9, 0, "a2"),
		std::make_tuple(5, 9, 7, "a4"),
		std::make_tuple(4, 9, 8, "a5"),
		std::make_tuple(3, 9, 9, "a6")
	};

	auto index_it = cache.index_begin();
	auto index_output_it = index_output.begin();
	ASSERT_EQ(cache.size(), index_output.size());
	while (index_it != cache.index_end()) {
		ASSERT_EQ(*index_output_it, std::make_tuple(index_it->inode, index_it->parent_inode, index_it->index, index_it->name));
		index_it++;
		index_output_it++;
	}
	ASSERT_TRUE(index_output_it == index_output.end());

	auto lookup_it = cache.lookup_begin();
	auto lookup_output_it = lookup_output.begin();
	ASSERT_EQ(cache.size(), lookup_output.size());
	while (lookup_it != cache.lookup_end()) {
		ASSERT_EQ(*lookup_output_it, std::make_tuple(lookup_it->inode, lookup_it->parent_inode, lookup_it->index, lookup_it->name));
		lookup_it++;
		lookup_output_it++;
	}
	ASSERT_TRUE(lookup_output_it == lookup_output.end());
}
