#include <algorithm>
#include <cassert>
#include <iostream>

enum class hashMode_t : unsigned
{
	kLookup = 1,
	kAdd = 2,
	kDelete = 3,
};

typedef int key_t;

constexpr int len = 127;
constexpr int len2 = len - 2;
constexpr key_t keyFree = 0;
constexpr key_t keyDeleted = -1;

struct hashEntry_t
{
	key_t key = 0;
	int contents = 0;
	void markDeleted() { this->key = keyDeleted; }
	bool isFree() const { return this->key == keyFree; }
	bool isDeleted() const { return this->key == keyDeleted; }
	bool isOccupied() const { return !(this->isFree() || this->isDeleted()); }
};

hashEntry_t keytab[len];

struct stats_t
{
	int nCall = 0;
	int nProbe = 0;
	int nRelocTry = 0;
	int nRelocProbe = 0;
	int nReloc = 0;
	int nFreeScan = 0;
	void clear()
	{
		this->nCall = 0;
		this->nProbe = 0;
		this->nRelocProbe = 0;
		this->nRelocTry = 0;
		this->nReloc = 0;
		this->nFreeScan = 0;
	}
	void addCall() { ++this->nCall; }
	void addProbe() { ++this->nProbe; }
	void addRelocTry() { ++this->nRelocTry; }
	void addRelocProbe() { ++this->nRelocProbe; }
	void addReloc() { ++this->nReloc; }
	void addFreeScan() { ++this->nFreeScan; }
	stats_t &add(const stats_t &b)
	{
		this->nCall += b.nCall;
		this->nProbe += b.nProbe;
		this->nRelocProbe += b.nRelocProbe;
		this->nRelocTry += b.nRelocTry;
		this->nReloc += b.nReloc;
		this->nFreeScan += b.nFreeScan;
		return *this;
	}
	void print()
	{
		std::cout << "nCall: " << this->nCall << " "
			  << "nProbe: " << this->nProbe << " "
			  << "nRelocTry: " << this->nRelocTry << " "
			  << "nRelocProbe: " << this->nRelocProbe << " "
			  << "nReloc: " << this->nReloc << " "
			  << "nFreeScan: " << this->nFreeScan << " "
			  << "\n";
	}
};

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
	int secondary_Q = std::abs(key) % len2 + 1;

	// primary hash code.
	int primary_R = std::abs(key) % len;
	auto iEntry = primary_R;
	pEntry = &keytab[iEntry];

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
			auto iScan = iEntry;
			// compute address of next probe
			bool needToExit = false;
			key_t searchKey;

			// scan forward to a free entry; if found,
			// we'll move the entry to here, to shorten
			// probes.
			do
			{
				if (pStat != nullptr)
					pStat->addFreeScan();
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
			if (mode != hashMode_t::kDelete)
				keytab[iEntry] = keytab[iScan];

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
			iEntry += secondary_Q;
			if (iEntry >= len)
			{
				iEntry -= len;
			}
			// invariant...
			pEntry = &keytab[iEntry];
			if (iEntry == primary_R)
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
		// to shuffle things around. The program in [brent] is
		// impenetrable, so we use the pseudocde from [kube]
		//
		auto const v = iterationCount + 2;
		int iRelocEntry;

		if (pStat) pStat->addRelocTry();

		for (int c = 0; c < v - 2; ++c)
		{
			iRelocEntry = primary_R;
			for (int d = 0; d <= c; ++d)
			{
				if (pStat)
					pStat->addRelocProbe();
				auto here = (iRelocEntry + (c - d) * secondary_Q) % len;
				if (!keytab[here].isOccupied())
				{
					if (pStat) pStat->addReloc();
					// move key[iRelocEntry] here, and put new key at iRelocEntry.
					keytab[here] = keytab[iRelocEntry];
					keytab[iRelocEntry].key = key;
					pEntry = &keytab[iRelocEntry];
					return false;
				}
			}
		}
		// no point in moving things around. Just return
		assert(!pEntry->isOccupied());
		pEntry->key = key;
		return false;
	}

	assert(false && "Not reached");
}

key_t testKey(int j) { return j * 127; }

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
