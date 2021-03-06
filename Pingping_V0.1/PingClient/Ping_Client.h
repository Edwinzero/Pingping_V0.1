#pragma once
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost\thread.hpp>

#include <tinyxml2.h>
#include <timer.h>

void handle_error(const std::string& function_name, const boost::system::error_code& err)
{
	std::cout << __FUNCTION__ << " in " << function_name << " due to "
		<< err << " " << err.message() << std::endl;
}

class Ping_Client
{
public:

public:
	Ping_Client();
	~Ping_Client();
};

