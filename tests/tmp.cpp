#include <iostream>
#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>

class MyDate
{
private:
    struct Duration
    {
        int year;
        int month;
        int day;
        Duration( int y, int m, int d ) : 
            year( y ), month( m ), day( d )
            {}
    } mdata;
public:
    MyDate() : mdata( 2011, 11, 15 )
    {}
    MyDate( int year, int month, int day ) : mdata( year, month, day )
    {}
    int getYear() const
    { return mdata.year; }
    int getMonth() const    
    { return mdata.month; }
    int getDay() const
    { return mdata.day; }
    friend bool operator < ( MyDate const& lhs, MyDate const& rhs )
    {
        if ( lhs.mdata.year > rhs.mdata.year )
            return false;           
        else if ( lhs.mdata.year < rhs.mdata.year )
            return true;
        else if ( lhs.mdata.year == rhs.mdata.year )
        {
            if ( lhs.mdata.month > rhs.mdata.month )
                return false;           
            else if ( lhs.mdata.month < rhs.mdata.month )
                return true;
            else if ( lhs.mdata.month == rhs.mdata.month )
            {
                if ( lhs.mdata.day < rhs.mdata.day )
                    return true;            
                else 
                    return false;           
            }
        }
        return false;
    }
};

class MyDateTest : public CppUnit::TestCase
{
    MyDate mybday;
    MyDate today;
public:
    MyDateTest() : mybday( 1951, 10, 1 ) {}
    void run()
    {
        testOps();
    }
    void testOps()
    {
        CPPUNIT_ASSERT( mybday < today );
    }
};

int main()
{
    MyDateTest test;
    test.run();
    return 0;
}
