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
	}
}


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


BOOST_AUTO_TEST_CASE( addNumber )
{
	Range testRange;

	testRange.add(4);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 1);		

	testRange.add(5);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 1);	

	testRange.add(7);

	BOOST_CHECK_EQUAL(testRange.intervalCount(), 2);	
}

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
