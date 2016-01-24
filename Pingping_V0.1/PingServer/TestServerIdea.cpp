//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string make_daytime_string()
{
	using namespace std; // For time_t, time and ctime;
	time_t now = time(0);
	return ctime(&now);
}

std::string make_hello_string( int count)
{
	using namespace std;
	char hello[256];
	sprintf(hello, "[HELLO FROM THE OTHER SIDE] Current hello is no. %d \n", count);
	string ret = hello;
	return ret;
}

class tcp_connection
	: public boost::enable_shared_from_this<tcp_connection>
{
public:
	typedef boost::shared_ptr<tcp_connection> pointer;

	static pointer create(boost::asio::io_service& io_service)
	{
		return pointer(new tcp_connection(io_service));
	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void teststart()
	{
		message_ = make_hello_string(100);

		boost::asio::async_write(socket_, boost::asio::buffer(message_),
			boost::bind(&tcp_connection::handle_test, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}

	void start()
	{
		message_ = make_daytime_string();

		// Good way to send multiple command
		for (int i = 0; i < 20; i++){
			boost::asio::async_write(socket_, boost::asio::buffer(message_),
				boost::bind(&tcp_connection::handle_switch, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		}
	}

	int count;

private:
	tcp_connection(boost::asio::io_service& io_service)
		: socket_(io_service)
	{
		count = 0;
	}

	void handle_test(const boost::system::error_code& err, size_t bytes_transferred){
	
	}

	void handle_switch(const boost::system::error_code& err, size_t bytes_transferred){
		count++;
		if (count == 20){
			while (1){

			}
		}
		
		if (count % 2 == 0){
			message_ = make_hello_string(count);

			boost::asio::async_write(socket_, boost::asio::buffer(message_),
				boost::bind(&tcp_connection::handle_switch, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
			std::cout << "[SYSTEM] :: [Read Procedure]Current count is.. " << count << ", There is no message need to be write.." << std::endl;
		}
		else{
			message_ = make_hello_string(count);

			boost::asio::async_write(socket_, boost::asio::buffer(message_),
				boost::bind(&tcp_connection::handle_write, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
			std::cout << "[SYSTEM] :: [Write Procedure] Current count is.. " << count << ", There is no message need to be write.." << std::endl;
		}
		
	}

	void handle_write(const boost::system::error_code& /*error*/,
		size_t /*bytes_transferred*/)
	{
		//*
		message_ = make_daytime_string();

		boost::asio::async_write(socket_, boost::asio::buffer(message_),
			boost::bind(&tcp_connection::handle_switch, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
		std::cout << "[HANDLE WRITE] :: [MESSAGE]  is.. " << message_ << "..." << std::endl;
			//*/
	}

	tcp::socket socket_;
	std::string message_;
};

class tcp_server
{
public:
	tcp_server(boost::asio::io_service& io_service)
		: acceptor_(io_service, tcp::endpoint(tcp::v4(), 1113))
	{
		start_accept();
	}

	void parsing_cmd(const std::string& cmd){
		cmd_ = cmd;
		if (cmd_ == "start"){
			std::cout << "[SYSTEM] :: RECV 'START' COMMAND... " << std::endl;
			image_session_->start();
		}
		else{
			std::cout << "[SYSTEM] :: INVALID COMMAND.. Please enter command again..." << std::endl;
		}
	}
private:
	void start_accept()
	{
		image_session_ =
			tcp_connection::create(acceptor_.get_io_service());

		acceptor_.async_accept(image_session_->socket(),
			boost::bind(&tcp_server::handle_accept, this, image_session_,
			boost::asio::placeholders::error));
		/*
		tcp_connection::pointer new_connection =
			tcp_connection::create(acceptor_.get_io_service());

		acceptor_.async_accept(new_connection->socket(),
			boost::bind(&tcp_server::handle_accept, this, new_connection,
			boost::asio::placeholders::error));
			//*/
	}

	void handle_accept(tcp_connection::pointer new_connection,
		const boost::system::error_code& error)
	{
		std::cout << "[SYSTEM] :: SERVER - CLIENT connection has been set up !!!!" << std::endl;
		if (!error){
			//image_session_->teststart();
		}
		//start_accept();
	}

	tcp::acceptor acceptor_;
	std::string cmd_;
	tcp_connection::pointer image_session_;
};

int main()
{
	try
	{
		boost::asio::io_service io_service;
		tcp_server server(io_service);
		io_service.run();
		char cmd_line[1024];
		while (std::cin.getline(cmd_line, 1024)){
			//std::cin.getline(cmd_line, 1024);
			server.parsing_cmd(cmd_line);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}