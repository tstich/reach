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
	for( auto it = m_intervals.begin(); it < m_intervals.end(); ++it ) {
		if( it->first <= number && number < it->second ) {
			if( it->first == number ) {
				it->first++;
			} else if ( it->second == number - 1) {
				it->second--;
			} else {
				Interval split(it->first, number);
				it->first = number + 1;
				m_intervals.insert(it, split);
			}
		}

		return;
	}
}

void Range::subtract(int64_t start, int64_t end)
{
	if( end <= start ) return;

	for( auto it = m_intervals.begin(); it < m_intervals.end(); ) {
		if( it->first <= end && start < it->second ) {
			if( start <= it->first && it->second <= end ) {
				it = m_intervals.erase(it);
			} else if (  start <= it->first && end < it->second) {
				it->first = end;
				it++;
			} else if ( start > it->first && it->second < end ) {
				it->second = start;
				it++;
			} else {
				Interval split(it->first, start);
				it->first = end;
				it = m_intervals.insert(it, split);
				it += 2;
			}
		} else {
			it++;
		}
	}

}

void Range::subtract(const Range &other)
{
	for( Interval sub: other.m_intervals) {
		subtract(sub.first, sub.second);
	}
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

std::string Range::toString() const {
	std::stringstream output;

	for( const Interval& v : m_intervals) {
		output << v.first << ":" << v.second << ",";
	}

	return output.str();
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
