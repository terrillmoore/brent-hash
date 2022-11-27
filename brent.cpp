/*

Module:	brent.cpp

Function:
	Test program and implemetation of Brent's variation for hashing.

Copyright and License:
	This file copyright (C) 2022 by

		MCCI Corporation
		3520 Krums Corners Road
		Ithaca, NY  14850

	See accompanying LICENSE file for copyright and license information.

Author:
	Terry Moore, MCCI Corporation	November 2022

*/

#include <algorithm>
#include <cassert>
#include <iostream>

/****************************************************************************\
|
|	Types
|
\****************************************************************************/

enum class hashMode_t : unsigned
{
	kLookup = 1,
	kAdd = 2,
	kDelete = 3,
};

/// \brief abstract type for keys in the table.
typedef int key_t;

constexpr key_t keyFree = 0;
constexpr key_t keyDeleted = -1;

/// \brief length of hash table -- this must be a prime!
constexpr int len = 127;
/// \brief secondary length, used for re-hashing. Must be len - 2.
constexpr int len2 = len - 2;

/// \brief the contents of the hash table
struct hashEntry_t
{
	key_t key = 0;
	int contents = 0;
	void markDeleted() { this->key = keyDeleted; }
	bool isFree() const { return this->key == keyFree; }
	bool isDeleted() const { return this->key == keyDeleted; }

	/// \brief does an entry contain real data (neither free nor deleted)?
	bool isOccupied() const { return !(this->isFree() || this->isDeleted()); }
};

/// \brief the hash table
hashEntry_t keytab[len];

/// \brief statistics
struct stats_t
{
	int nCall = 0;
	int nProbe = 0;
	int nRelocTry = 0;
	int nRelocProbe = 0;
	int nRelocMove = 0;
	int nDeleteTry = 0;
	int nDeleteProbe = 0;
	int nDeleteMove = 0;

	void clear()
	{
		this->nCall = 0;
		this->nProbe = 0;
		this->nRelocProbe = 0;
		this->nRelocTry = 0;
		this->nRelocMove = 0;
		this->nDeleteTry = 0;
		this->nDeleteProbe = 0;
		this->nDeleteMove = 0;
	}
	void addCall() { ++this->nCall; }
	void addProbe() { ++this->nProbe; }
	void addRelocTry() { ++this->nRelocTry; }
	void addRelocProbe() { ++this->nRelocProbe; }
	void addReloc() { ++this->nRelocMove; }
	void addDeleteTry() { ++this->nDeleteTry; }
	void addDeleteProbe() { ++this->nDeleteProbe; }
	void addDeleteMove() { ++this->nDeleteMove; }
	stats_t &add(const stats_t &b)
	{
		this->nCall += b.nCall;
		this->nProbe += b.nProbe;
		this->nRelocProbe += b.nRelocProbe;
		this->nRelocTry += b.nRelocTry;
		this->nRelocMove += b.nRelocMove;
		this->nDeleteTry += b.nDeleteTry;
		this->nDeleteProbe += b.nDeleteProbe;
		this->nDeleteMove += b.nDeleteMove;
		return *this;
	}
	void print()
	{
		std::cout << "nCall: " << this->nCall << " "
			  << "nProbe: " << this->nProbe << " "
			  << "nDeleteTry:" << this->nDeleteTry << " "
			  << "nDeleteProbe: " << this->nDeleteProbe << " "
			  << "nDeleteMove: " << this->nDeleteMove << " "
			  << "nRelocTry: " << this->nRelocTry << " "
			  << "nRelocProbe: " << this->nRelocProbe << " "
			  << "nRelocMove: " << this->nRelocMove << " "
			  << "\n";
	}
};

///
/// \brief bit-reverse a 32-bit number
///
/// \details
///	I found that bit reversing seemed to do a good job of handling
///	situations where the input key sequence was a multiple of the
///	primary and secondary key rehashing value. This is definitely
///	a personal experiment.
///
///	This is a the well known loopless bit reversal scheme.
///
constexpr std::uint32_t bitreverse(
	std::uint32_t v
	)
{
	// swap odd and even bits
	v = ((v >> 1) & UINT32_C(0x55555555)) | ((v & UINT32_C(0x55555555)) << 1);
	// swap pairs
	v = ((v >> 2) & UINT32_C(0x33333333)) | ((v & UINT32_C(0x33333333)) << 2);
	// swap nibbles
	v = ((v >> 4) & UINT32_C(0x0F0F0F0F)) | ((v & UINT32_C(0x0F0F0F0F)) << 4);
	// swap bytes
	v = ((v >> 8) & UINT32_C(0x00FF00FF)) | ((v & UINT32_C(0x00FF00FF)) << 8);
	// swap halves
	v = ((v >> 16) & UINT32_C(0x0000FFFF)) | ((v & UINT32_C(0x0000FFFF)) << 16);
	return v;
}

/// \brief bit-reverse a signed integer.
constexpr std::int32_t bitreverse(
	std::int32_t v
	)
	{
	return std::int32_t(bitreverse(std::uint32_t(v)));
	}

///
/// \brief calculate Brent's Q (secondary hash) for a given key
///
/// \return
///	a number in [1..len-1].
///
constexpr int hash_Q(const key_t key)
{
	return bitreverse(std::uint32_t(key)) % len2 + 1;
}

///
/// \brief look up, add, or delete a key using a table managed according
///	to Brent.
///
/// \param [in] 	key	key of the entry to find or create
/// \param [in] 	mode 	specifies whether to search (only),
///				to search and add if not found, or
///				to search and delete if found.
/// \param [out] 	pEntry 	Set to point to the key if found or
///				successfully added.
/// \param [inout] 	pStat	Optionally points to a statistics object
///				that will be updated reflecting the
///				performance of the search.
///
/// \return
///	If \c true, the key was previously in the table. Otherwise,
///	it was not. For adds, if \c false, \c pEntry indicates whether
///	the value was added (nullptr if table was full, not nullptr
///	if a slot was located).
///
bool hash(const key_t key, const hashMode_t mode, hashEntry_t *&pEntry,
	  stats_t *pStat)
{
	// start up the iteration count so taht it will end
	// at len2 (which is len - 2)
	int iterationCount = 1 - 2;

	// secondary hash code. Note that this must be
	// in [1 .. len). Per [brent], this may be any
	// independent pseudo-random function of key,
	// but note that it must not be zero.
	auto secondary_Q = hash_Q(key);

	// primary hash code.
	int primary_R = std::abs(key) % len;
	auto iEntry_s = primary_R;
	pEntry = &keytab[iEntry_s];

	if (pStat)
		pStat->addCall();

	while (true)
	{
		if (pStat != nullptr)
			pStat->addProbe();
		auto const thisEntryKey = pEntry->key;
		if (thisEntryKey == keyFree)
		{
			// empty slot, end search.
			break;
		}
		else if (thisEntryKey == keyDeleted)
		{
			// 40 a deleted entry has been found
			auto iScan = iEntry_s;
			// compute address of next probe
			bool needToExit = false;
			key_t searchKey;

			if (pStat != nullptr) pStat->addDeleteTry();

			// scan forward to a free entry; if found,
			// we'll move the entry to here, to shorten
			// probes.
			do
			{
				if (pStat != nullptr)
					pStat->addDeleteProbe();
				iScan += secondary_Q;
				if (iScan >= len)
					iScan -= len;
				searchKey = keytab[iScan].key;
				// check for empty speace or complete scan of table
				if (searchKey == keyFree || iScan == primary_R)
				{
					needToExit = true;
					break;
				}
				// check for mismatch or deleted entry
			} while (searchKey != key || searchKey == keyDeleted);

			// empty space or complete scan?
			if (needToExit)
				break;

			// key found. Move it and the associated value to
			// save probes on the next search for the same key.
			// (or simply nuke the one we found if we're deleting)
			if (pStat != nullptr) pStat->addDeleteMove();

			if (mode != hashMode_t::kDelete)
				keytab[iEntry_s] = keytab[iScan];

			// where were were is now nothing.
			keytab[iScan].markDeleted();

			return true;
		}
		else if (thisEntryKey == key)
		{
			// 60: found it. delete if needed.
			if (mode == hashMode_t::kDelete)
			{
				pEntry->markDeleted();
			}
			return true;
		}
		else
		{
			// advance
			++iterationCount;
			iEntry_s += secondary_Q;
			if (iEntry_s >= len)
			{
				iEntry_s -= len;
			}
			// invariant...
			pEntry = &keytab[iEntry_s];
			if (iEntry_s == primary_R)
				break;
		}
	}
	// 30 the key is not in the table
	// the key is not in the table. return unless an
	// entry must be made. This also checks for invalid
	// key values.
	if (!(mode == hashMode_t::kAdd && iterationCount <= len2 && key != keyFree &&
	      key != keyDeleted))
	{
		pEntry = nullptr;
		return false;
	}

	// 70 add the key
	if (iterationCount <= 0)
	{
		// 120 enter the new key.
		pEntry->key = key;
		// we added it.
		return false;
	}
	else
	{
		//
		// we probed more than twice, so ... we need
		// to shuffle things around. iEntry_s.key is
		// zero.
		//
		auto const brent_s = iterationCount + 2;

		if (pStat) pStat->addRelocTry();

		// we need to iterate over Brent's h[c,d]:
		// h[0,1]..h[0,s-1], h[1,1]..h[1,s-2], h[2,1]..h[2,s-3], ...,  h[s-2,1]..h[s-2,1]
		for (int c = 0; c <= brent_s - 2; ++c)
		{
			auto const h_i = (primary_R + c * secondary_Q) % len;
			auto const q_i = hash_Q(keytab[h_i].key);

			for (int d = 1; d <= brent_s - c - 1; ++d)
			{
				if (pStat)
					pStat->addRelocProbe();
				auto const h_ij = (h_i + d * q_i) % len;
				if (!keytab[h_ij].isOccupied())
				{
					if (pStat) pStat->addReloc();
					// move key[h_i] to key[h_ij], and put new key at key[h_i].
					keytab[h_ij] = keytab[h_i];
					keytab[h_i].key = key;
					pEntry = &keytab[h_i];
					return false;
				}
			}
		}
		// no point in moving things around. Just return.
		assert(!pEntry->isOccupied());
		pEntry->key = key;
		return false;
	}

	assert(false && "Not reached");
}

/****************************************************************************\
|
|	Test
|
\****************************************************************************/

key_t testKey(int j) { return uint16_t(j * 31413); }

int main()
{
	std::cout << "brent hashing test\n";
	hashEntry_t *pEntry;
	stats_t stats;

	for (auto j = 1; j < 128; ++j)
	{
		auto i = testKey(j);
		auto fResult = hash(i, hashMode_t::kAdd, pEntry, &stats);

		if (fResult)
			std::cout << "add found existing key: "
				  << "try=" << j << " "
				  << "key=" << i << " "
				  << "iEntry=" << pEntry - &keytab[0] << " "
				  << ".key=" << pEntry->key << "\n";
		if (pEntry == nullptr)
			std::cout << "add returned full table: "
				  << "try=" << j << " "
				  << "key=" << i << "\n";

		else if (pEntry->key != i)
			std::cout << "add didn't change key: "
				  << "try=" << j << " "
				  << "key=" << i << " "
				  << "iEntry=" << pEntry - &keytab[0] << " "
				  << ".key=" << pEntry->key << "\n";
	}

	stats.print();
	std::cout << "done with inserts\n";

	stats.clear();
	for (auto j = 1; j < 128; ++j)
	{
		auto i = testKey(j);
		auto fResult = hash(i, hashMode_t::kLookup, pEntry, &stats);

		if (!fResult)
		{
			if (pEntry == nullptr)
			{
				std::cout << "lookup returned false: "
					  << "try=" << j << " "
					  << "key=" << i << "\n";
			}
			else
			{
				std::cout << "lookup returned false and existing key: "
					  << "try=" << j << " "
					  << "key=" << i << " "
					  << "iEntry=" << pEntry - &keytab[0] << " "
					  << ".key=" << pEntry->key << "\n";
			}
		}
		else
		{
			if (pEntry == nullptr)
			{
				std::cout << "lookup returned true but null pointer: "
					  << "try=" << j << " "
					  << "key=" << i << "\n";
			}
			else if (pEntry->key != i)
			{
				std::cout << "lookup returned true but wrong key: "
					  << "try=" << j << " "
					  << "key=" << i << " "
					  << "iEntry=" << pEntry - &keytab[0] << " "
					  << ".key=" << pEntry->key << "\n";
			}
		}
	}

	stats.print();
	std::cout << "done with lookups\n";

	stats.clear();
	for (auto j = 1; j < 128; ++j)
	{
		auto i = testKey(j);
		auto fResult = hash(i, hashMode_t::kDelete, pEntry, &stats);

		if (!fResult)
		{
			if (pEntry == nullptr)
			{
				std::cout << "delete returned false: "
					  << "try=" << j << " "
					  << "key=" << i << "\n";
			}
			else
			{
				std::cout << "delete returned false and existing key: "
					  << "try=" << j << " "
					  << "key=" << i << " "
					  << "iEntry=" << pEntry - &keytab[0] << " "
					  << ".key=" << pEntry->key << "\n";
			}
		}
		else
		{
			if (pEntry == nullptr)
			{
				std::cout << "delete returned true but null pointer: "
					  << "try=" << j << " "
					  << "key=" << i << "\n";
			}
			else if (pEntry->key != keyDeleted)
			{
				std::cout << "delete returned true but wrong key: "
					  << "try=" << j << " "
					  << "key=" << i << " "
					  << "iEntry=" << pEntry - &keytab[0] << " "
					  << ".key=" << pEntry->key << "\n";
			}
		}
	}

	stats.print();
	std::cout << "done with deletes\n";
}
