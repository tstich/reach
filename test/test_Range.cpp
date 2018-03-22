//Link to Boost
#define BOOST_TEST_DYN_LINK

//Define our Module name (prints at testing)
#define BOOST_TEST_MODULE "Range"

//VERY IMPORTANT - include this last
#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>

#include <Range.h>
#include <Config.h>

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

BOOST_AUTO_TEST_CASE( constructors )
{
	{
		Range testRange;
		BOOST_CHECK_EQUAL(testRange.intervalCount(), 0);		
	}

	{
		Range testRange(0,10);
		BOOST_CHECK_EQUAL(testRange.intervalCount(), 1);		
		BOOST_CHECK_EQUAL(testRange.elementCount(), 10);		
	}
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( iterator )
{
	{
		Range testRange;
		BOOST_CHECK(testRange.begin() == testRange.end());		
	}

	{
		Range testRange(3,10);
		auto it = testRange.begin();
		for( uint64_t i = 3; i < 10; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());	
	}
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( addNumber )
{
	Range testRange;

	testRange.add(4);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 1);		

	testRange.add(5);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 1);	

	testRange.add(7);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 2);	

	BOOST_CHECK_EQUAL(testRange.elementCount(), 3);		

}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( addInterval )
{
	Range testRange(0,3);

	testRange.add(4,8);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 2);	
	{
		auto it = testRange.begin();
		for( uint64_t i = 0; i < 3; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 4; i < 8; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());			
	}


	testRange.add(1,7);
	BOOST_CHECK_EQUAL(testRange.intervalCount(), 1);	
	{
		auto it = testRange.begin();
		for( uint64_t i = 0; i < 8; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());	
	}
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( addRange )
{
	Range testRange(0,3);
	testRange.add(4,8);

	Range testRange2(1,7);
	testRange2.add(10,13);

	testRange.add(testRange2);


	BOOST_CHECK_EQUAL(testRange.intervalCount(), 2);	
	{
		auto it = testRange.begin();
		for( uint64_t i = 0; i < 8; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 10; i < 13; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());			
	}

}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

BOOST_AUTO_TEST_CASE( subtractNumber )
{
	Range testRange(0,10);

	testRange.subtract(5);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 2);		
	{
		auto it = testRange.begin();
		for( uint64_t i = 0; i < 5; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 6; i < 10; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());			
	}
	
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( subtractInterval )
{
	Range testRange(0,10);

	testRange.subtract(3,5);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 2);		
	{
		auto it = testRange.begin();
		for( uint64_t i = 0; i < 3; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 5; i < 10; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());			
	}
	
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( subtractRange )
{
	Range testRange(0,10);

	Range testRange2(1,3);
	testRange2.add(7,8);

	testRange.subtract(testRange2);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 3);		
	{
		auto it = testRange.begin();
		for( uint64_t i = 0; i < 1; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 3; i < 7; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 8; i < 10; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange.end());			
	}
	
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

BOOST_AUTO_TEST_CASE( serizalization )
{
	// UDP Packet as Struct
	#pragma pack(push, 1)
	struct {
		uint8_t numberIntervals;
		uint64_t a0;
		uint64_t b0;
		uint64_t a1;
		uint64_t b1;
	} rangeData;
	#pragma pack(pop)

	// Create
	auto range = Range(3,5);
	range.add(8,10);

	// Data Layer
	auto rangeBuffer = range.asBuffer();
	BOOST_CHECK_EQUAL(sizeof(rangeData), boost::asio::buffer_size(rangeBuffer));

	boost::asio::buffer_copy(boost::asio::buffer(&rangeData, sizeof(rangeData)), rangeBuffer); 
	
	BOOST_CHECK_EQUAL(rangeData.numberIntervals, 2);
	BOOST_CHECK_EQUAL(rangeData.a0, 3);
	BOOST_CHECK_EQUAL(rangeData.b0, 5);
	BOOST_CHECK_EQUAL(rangeData.a1, 8);
	BOOST_CHECK_EQUAL(rangeData.b1, 10);

	// Parse
	auto parsedRange = Range::fromBuffer(reinterpret_cast<uint8_t*>(&rangeData));
	BOOST_CHECK_EQUAL(parsedRange->intervalCount(), 2);
	{
		auto it = parsedRange->begin();
		for( uint64_t i = 3; i < 5; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 8; i < 10; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == parsedRange->end());			
	}
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


BOOST_AUTO_TEST_CASE( firstN )
{
	Range testRange(0,3);
	testRange.add(7,10);

	Range testRange2 = testRange.firstN(5);

	BOOST_CHECK_EQUAL(testRange2.elementCount(), 5);
	{
		auto it = testRange2.begin();
		for( uint64_t i = 0; i < 3; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		for( uint64_t i = 7; i < 9; i++, it++) 
		{
			BOOST_CHECK_EQUAL(*it, i);	
		}
		BOOST_CHECK(it == testRange2.end());			
	}
	
}
