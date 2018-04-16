#pragma once

#include <deque>
#include <boost/asio.hpp>

class Range
{
public:
	typedef std::pair<uint64_t, uint64_t> Interval;

public:
	Range() {};
	Range(int64_t number);
	Range(int64_t start, int64_t end);

	void add(uint64_t number);
	void add(int64_t start, int64_t end);
	void add(const Range &other);

	void subtract(uint64_t number);
	void subtract(int64_t start, int64_t end);
	void subtract(const Range &other);

	uint8_t intervalCount() { return m_intervalCount = static_cast<uint8_t>(m_intervals.size()); }
	size_t elementCount() const;

	Range firstN(int64_t elements);
	Range removeFirstN(int64_t elements);

	bool contains(uint64_t x);


	// Range from Buffer
	static std::shared_ptr<Range> fromBuffer(const uint8_t* data);

	// Range to Buffer
	std::vector<boost::asio::const_buffer> asBuffer() const;
    std::string toString() const;


public:
    // member typedefs provided through inheriting from std::iterator
    class iterator: public std::iterator<
                        std::input_iterator_tag,   // iterator_category
                        uint64_t,                      // value_type
                        uint64_t,                      // difference_type
                        const uint64_t*,               // pointer
                        uint64_t                       // reference
                        >
    {
    	uint64_t num;
    	std::deque<Interval>::iterator subIt;
    	std::deque<Interval>::iterator subEnd;
    public:
        explicit iterator(std::deque<Interval>::iterator _subIt, std::deque<Interval>::iterator _subEnd) : subIt(_subIt), subEnd(_subEnd), num(_subIt->first) {}
        explicit iterator(uint64_t _num) : num(_num) {}

        iterator& operator++() {num = num + 1; if (num == subIt->second) { subIt++; num = subIt != subEnd ? subIt->first : num; }; return *this;}
        iterator operator++(int) {iterator retval = *this; ++(*this); return retval;}
        bool operator==(iterator other) const {return num == other.num;}
        bool operator!=(iterator other) const {return !(*this == other);}
        reference operator*() const {return num;}
    };
    iterator begin() {return m_intervals.size() > 0 ? iterator(m_intervals.begin(), m_intervals.end()) : iterator(0);}
    iterator end() {return m_intervals.size() > 0 ? iterator(m_intervals.back().second) : iterator(0);}

private:
	void mergeIntervals();

	// Vector of sorted intervals
	std::deque< Interval > m_intervals;

	uint8_t m_intervalCount;
};