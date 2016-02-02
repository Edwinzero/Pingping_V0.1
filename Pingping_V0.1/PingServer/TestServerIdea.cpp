//
// server.cpp
// ~~~~~~~~~~

#if 0
#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
template<typename T>
void LOGOUT(T& str){
	std::cout << "[LOG] :: " << str << std::endl;
}

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

std::string make_getimgs_string()
{
	using namespace std;
	char msg[256];
	sprintf(msg, "getimgs");
	string ret = msg;
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

	void start()
	{
		boost::system::error_code error;
		message_.assign("gis", 4);
		LOGOUT(message_);
		boost::asio::write(socket_, boost::asio::buffer(message_),
			boost::asio::transfer_all(), error);
		// Transfer images...
		{
			message_.assign("rcv", 4);
			// Good way to send multiple command
			int total_imgs = 5;
			boost::asio::streambuf header_buf;
			for (int i = 0; i < total_imgs-1; i++){
				// Recv header
				char buf[1024];
				
				boost::asio::read_until(socket_, header_buf, "\n\n");
				//std::size_t len = socket_.read_some(boost::asio::buffer(buf), error);
				// alloc buffer
				std::cout << "1.  HEADER stream buf is..." << header_buf.size() << std::endl;
				
				std::istream request_stream(&header_buf);
				std::string file_path; size_t file_size = 0;
				request_stream >> file_path;
				request_stream >> file_size;
				request_stream.read(buf, 2); // eat the "\n\n"
				std::cout << "2.  File label is: " <<  file_path << " , file size is " << file_size << std::endl;
				LOGOUT("3.   :: Alloc img buffer...");
				// send rcv
				//LOGOUT(message_);
				boost::asio::write(socket_, boost::asio::buffer(message_),
					boost::asio::transfer_all(), error);

				// Recv data
				
				size_t len = socket_.read_some(boost::asio::buffer(buf), error);
				// Store data
				LOGOUT("4.  ");
				std::cout.write(buf, len);
				LOGOUT("5.   :: Store img data...");
				// send rcv
				boost::asio::write(socket_, boost::asio::buffer(message_),
					boost::asio::transfer_all(), error);
			}
			// Recv header
			char buf[1024];
			//boost::asio::streambuf header_buf;
			boost::asio::read_until(socket_, header_buf, "\n\n");
			//std::size_t len = socket_.read_some(boost::asio::buffer(buf), error);
			// alloc buffer
			std::cout << "1.  HEADER stream buf is..." << header_buf.size() << std::endl;

			std::istream request_stream(&header_buf);
			std::string file_path; size_t file_size = 0;
			request_stream >> file_path;
			request_stream >> file_size;
			request_stream.read(buf, 2); // eat the "\n\n"
			std::cout << "2.  File label is: " << file_path << " , file size is " << file_size << std::endl;
			LOGOUT("3.   :: Alloc img buffer...");
			// send rcv
			boost::asio::write(socket_, boost::asio::buffer(message_),
				boost::asio::transfer_all(), error);
			// Recv data
			size_t len = socket_.read_some(boost::asio::buffer(buf), error);
			// Store data
			LOGOUT("4.  ");
			std::cout.write(buf, len);
			LOGOUT("<<<Last one>>> Store img data...");
		}
		
	}

	boost::asio::ip::tcp::socket& getSocket(){
		return socket_;
	}

	int count;

private:
	tcp_connection(boost::asio::io_service& io_service)
		: socket_(io_service)
	{
		count = 0;
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
		if (cmd_ == "get imgs"){
			std::cout << "[SYSTEM] :: SENDING 'GET IMAGES' COMMAND... " << std::endl;
			image_session_->start();
		}
		else if (cmd_ == "start cap"){
			std::cout << "[SYSTEM] :: SENDING 'GET IMAGES' COMMAND... " << std::endl;
			// client session
			boost::system::error_code error;
			std::string cmd;
			cmd.assign("scp",4);
			boost::asio::write(image_session_->socket(), boost::asio::buffer(cmd),
				boost::asio::transfer_all(), error);
			char buf[256];
			size_t len = image_session_->socket().read_some(boost::asio::buffer(buf), error);
			std::cout.write(buf, len);
		}
		else if (cmd == "stop cap"){
			std::cout << "[SYSTEM] :: SENDING 'STOP CAPTURE' COMMAND... " << std::endl;
			boost::system::error_code error;
			std::string cmd;
			cmd.assign("stp", 4);
			boost::asio::write(image_session_->socket(), boost::asio::buffer(cmd),
				boost::asio::transfer_all(), error);
			char buf[256];
			size_t len = image_session_->socket().read_some(boost::asio::buffer(buf), error);
			std::cout.write(buf, len);
		}
		else if (cmd == "set config"){
			std::cout << "[SYSTEM] :: SENDING 'SET CONFIG' COMMAND... " << std::endl;
			boost::system::error_code error;
			std::string cmd;
			cmd.assign("cnf", 4);
			boost::asio::write(image_session_->socket(), boost::asio::buffer(cmd),
				boost::asio::transfer_all(), error);
			char buf[256];
			size_t len = image_session_->socket().read_some(boost::asio::buffer(buf), error);
			std::cout.write(buf, len);
		}
		else if (cmd == "syn nodes"){
			std::cout << "[SYSTEM] :: SENDING 'SYNCHRONIZE NODES' COMMAND... " << std::endl;
			boost::system::error_code error;
			std::string cmd;
			cmd.assign("syn", 4);
			boost::asio::write(image_session_->socket(), boost::asio::buffer(cmd),
				boost::asio::transfer_all(), error);
			char buf[256];
			size_t len = image_session_->socket().read_some(boost::asio::buffer(buf), error);
			std::cout.write(buf, len);
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

	}

	void handle_accept(tcp_connection::pointer new_connection,
		const boost::system::error_code& error)
	{
		std::cout << "[SYSTEM] :: SERVER - CLIENT connection has been set up !!!!" << std::endl;
		std::cout << "[SYSTEM] :: Client connected from << " << image_session_->getSocket().remote_endpoint().address().to_string().c_str() << " >>" << std::endl;
		if (!error){
			//image_session_->teststart();
			// Maybe put time sychronization code here...

		}
		// If open comment, will cause connection failed...
		//start_accept();
	}

	// 4* acceptors + sessions
	tcp::acceptor acceptor_;
	tcp_connection::pointer image_session_;

	// 4* sockets + resolver for client

	std::string cmd_;
};

int main()
{
	try
	{
		// Maybe need 2 io_services, one for client func, one for server func
		boost::asio::io_service io_service;
		tcp_server server(io_service);
		// Service must be run before detecting cmd input
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
#endif
