#include <Range.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>

Range::Range(int64_t start, int64_t end)
{
	if( end > start ) {
		m_intervals.push_back(Interval(start, end));
	}
	intervalCount();
}

Range::Range(int64_t number)
{
	m_intervals.push_back(Interval(number, number + 1));
	intervalCount();	
}

void Range::add(uint64_t number)
{
	Interval addition(number, number+1);
	m_intervals.insert( std::upper_bound( m_intervals.begin(), m_intervals.end(), addition), addition );

	mergeIntervals();
}

void Range::add(int64_t start, int64_t end)
{
	if( end > start ) {
		Interval addition(start, end);
		m_intervals.insert( std::upper_bound( m_intervals.begin(), m_intervals.end(), addition), addition );
		mergeIntervals();
	}
}

void Range::add(const Range &other)
{
	for( Interval addition : other.m_intervals ) {
		m_intervals.insert( std::upper_bound( m_intervals.begin(), m_intervals.end(), addition), addition );
	}

	mergeIntervals();
}

bool Range::contains(uint64_t x)
{
	for( Interval i : m_intervals ) {
		if( i.first <= x && x < i.second ) 
			return true;
	}	

	return false;
}


void Range::subtract(uint64_t number)
{
	std::deque<Interval> unrolled;
	for(auto it = begin(); it != end(); ++it) {
		unrolled.push_back(Interval(*it, *it + 1));
	}

	m_intervals.clear();
	Interval subtraction(number, number + 1);
	std::set_difference(unrolled.begin(), unrolled.end(), &subtraction, &subtraction+1, 
		std::inserter(m_intervals, m_intervals.begin()));

	mergeIntervals();
}

void Range::subtract(int64_t start, int64_t end)
{
	if( end > start ) {
		std::deque<Interval> unrolled;
		for(auto it = this->begin(); it != this->end(); ++it) {
			unrolled.push_back(Interval(*it, *it + 1));
		}

		m_intervals.clear();

		std::vector<Interval> unrolled_subtraction;
		for(uint64_t i = start; i < end; ++i) {
			unrolled_subtraction.push_back(Interval(i, i+1));
		}

		std::set_difference(unrolled.begin(), unrolled.end(), 
			unrolled_subtraction.begin(), unrolled_subtraction.end(), 
			std::inserter(m_intervals, m_intervals.begin()));

		mergeIntervals();
	}
}

void Range::subtract(const Range &other)
{
	std::deque<Interval> unrolled;
	for(auto it = this->begin(); it != this->end(); ++it) {
		unrolled.push_back(Interval(*it, *it + 1));
	}

	m_intervals.clear();

	std::vector<Interval> unrolled_subtraction;
	for(Interval v: other.m_intervals) {
		for(uint64_t i = v.first; i < v.second; ++i) {
			unrolled_subtraction.push_back(Interval(i, i+1));
		}		
	}

	std::set_difference(unrolled.begin(), unrolled.end(), 
		unrolled_subtraction.begin(), unrolled_subtraction.end(), 
		std::inserter(m_intervals, m_intervals.begin()));

	mergeIntervals();
}

void Range::mergeIntervals()
{
	if( m_intervals.size() > 1 ) {
		// Since the list of intervals is sorted, we can go greedy and consider only the
		// next interval in the list for merging
		auto it = m_intervals.begin() + 1;
		while( it != m_intervals.end() ) 
		{
			if( (it-1)->first <= it->second && (it-1)->second >= it->first ) {
				// BOOST_LOG_TRIVIAL(debug) << "Merge: " << (it-1)->first << ":" << (it-1)->second << " with " << it->first << ":" << it->second;
				// Overlap -> Merge
				(it-1)->second = std::max((it-1)->second, it->second);

				// BOOST_LOG_TRIVIAL(debug) << "-> Merged: " << (it-1)->first << ":" << (it-1)->second;
				
				// Remove the merged interval
				it = m_intervals.erase(it);
			} else {
				it++;
			}
		}
	}

	intervalCount();
}

std::vector<boost::asio::const_buffer> Range::asBuffer() const {

	std::vector<boost::asio::const_buffer> composite_buffer;

	composite_buffer.push_back(boost::asio::const_buffer(&m_intervalCount, sizeof(m_intervalCount)));
	for( const Interval& v : m_intervals) {
		composite_buffer.push_back(boost::asio::const_buffer(&v, sizeof(Interval)));
	}

	return composite_buffer;
}

std::shared_ptr<Range> Range::fromBuffer(const uint8_t* data)
{
	uint8_t intervalCount = *reinterpret_cast<const uint8_t*>(data);
	data += sizeof(uint8_t);

	std::shared_ptr<Range> range(new Range());
	BOOST_LOG_TRIVIAL(debug) << "Range::fromBuffer: " << intervalCount;

	const uint64_t* numbers = reinterpret_cast<const uint64_t*>(data);
	for( int i = 0; i < intervalCount; ++i, numbers += 2) {
		range->add(numbers[0], numbers[1]);
	}

	return range;
}


size_t Range::elementCount() const
{
	size_t elements = 0;
	for( const Interval& v : m_intervals) {
		elements += v.second - v.first;
	}

	return elements;
}

Range Range::firstN(int64_t elements)
{
	Range result;

	for( const Interval& v : m_intervals) {
		int64_t deltaElements = v.second - v.first;
		result.add(v.first, v.first + std::min<int64_t>(elements - result.elementCount(), deltaElements));
	}	

	return result;
}

Range Range::removeFirstN(int64_t elements)
{
	Range r = firstN(elements);
	subtract(r);
	return r;
}
