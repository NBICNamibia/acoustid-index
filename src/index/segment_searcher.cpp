// Copyright (C) 2011  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

#include <algorithm>
#include "collector.h"
#include "segment_data_reader.h"
#include "segment_searcher.h"

using namespace Acoustid;

SegmentSearcher::SegmentSearcher(SegmentIndexSharedPtr index, SegmentDataReader *dataReader, uint32_t lastKey)
	: m_index(index), m_dataReader(dataReader), m_lastKey(lastKey)
{
}

SegmentSearcher::~SegmentSearcher()
{
	delete m_dataReader;
}

void SegmentSearcher::search(uint32_t *fingerprint, size_t length, Collector *collector)
{
	size_t i = 0, block = 0, lastBlock = SIZE_MAX;
	while (i < length) {
		if (block > lastBlock || lastBlock == SIZE_MAX) {
			size_t localFirstBlock, localLastBlock;
			if (fingerprint[i] > m_lastKey) {
				// All following items are larger than the last segment's key.
				return;
			}
			if (m_index->search(fingerprint[i], &localFirstBlock, &localLastBlock)) {
				if (block > localLastBlock) {
					// We already searched this block and the fingerprint item was not found.
					i++;
					continue;
				}
				// Don't search for the same block multiple times.
				block = std::max(block, localFirstBlock);
				lastBlock = localLastBlock;
			}
			else {
				// The fingerprint item is definitely not in any block.
				i++;
				continue;
			}
		}
		uint32_t firstKey = m_index->levelKey(block);
		uint32_t lastKey = block + 1 < m_index->levelKeyCount(0) ? m_index->levelKey(block + 1) : UINT32_MAX;
		ScopedPtr<BlockDataIterator> blockData(m_dataReader->readBlock(block, firstKey));
		while (blockData->next()) {
			uint32_t key = blockData->key();
			if (key >= fingerprint[i]) {
				while (key > fingerprint[i]) {
					i++;
					if (i >= length) {
						return;
					}
					else if (lastKey < fingerprint[i]) {
						// There are no longer any items in this block that we could match.
						goto nextBlock;
					}
				}
				if (key == fingerprint[i]) {
					collector->collect(blockData->value());
				}
			}
		}
	nextBlock:
		block++;
	}
}

