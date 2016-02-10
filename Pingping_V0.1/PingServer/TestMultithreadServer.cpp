//
// server.cpp
// ~~~~~~~~~~

#if 1
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <timer.h>

using boost::asio::ip::tcp;
template<typename T>
void LOGOUT(T& str) {
	std::cout << "[LOG] :: " << str << std::endl;
}
template<typename T>
void DEBUG(const char* s, T& str) {
	std::cout << "[DEBUG LOG] :: " << s << "->" << str << std::endl;
}

std::string make_daytime_string()
{
	using namespace std; // For time_t, time and ctime;
	time_t now = time(0);
	return ctime(&now);
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

	void send_image()
	{
		boost::system::error_code error;
		message_.assign("gis", 4);
		LOGOUT(message_);
		boost::asio::write(socket_, boost::asio::buffer(message_),
			boost::asio::transfer_all(), error);
		// Transfer images...
		int total_imgs = 0;
		boost::asio::streambuf totalimg_buf;

		{
			// Recv header
			boost::asio::read_until(socket_, totalimg_buf, "\n\n");
			std::istream totalimg_stream(&totalimg_buf);
			totalimg_stream >> total_imgs;
			std::cout << "[PRE-REQUEST] :: TOTAL IMAGE NUMBER :: " << total_imgs << std::endl;
			// send rcv
			message_.assign("rcv", 4);
			boost::asio::write(socket_, boost::asio::buffer(message_),
				boost::asio::transfer_all(), error);

			Timer rcv_speed;
			rcv_speed.Start();

			// Good way to send multiple command
			for (int i = 0; i < total_imgs - 1; i++) {
				// Recv header
				boost::asio::streambuf header_buf;
				boost::asio::read_until(socket_, header_buf, "\n\n");
				std::cout << "[1]  HEADER stream buf size is " << header_buf.size() << std::endl;
				std::string file_path; size_t file_size = 0;
				std::istream request_stream(&header_buf);
				request_stream >> file_path;
				//DEBUG("file_path", file_path);
				request_stream >> file_size;
				//DEBUG("file_size", file_size);
				buffer_.resize(file_size);
				std::cout << "buffer_ size is: " << buffer_.size() << std::endl;
				request_stream.read(buffer_.data(), 2); // eat the "\n\n"
				std::cout << "[2]  File path is: " << file_path << " , file size is " << file_size << std::endl;

				//LOGOUT("3.   :: Alloc img buffer...");
				//size_t pos = file_path.find_last_of("\\");
				//if (pos != std::string::npos)
				//	file_path = file_path.substr(pos + 1);
				std::ofstream output_file(file_path.c_str(), std::ios_base::binary); //output_file.open();
				if (!output_file)
				{
					std::cout << "[OFSTREAM OUTPUT] :: failed to open " << file_path << std::endl;
					return;
				}
				do
				{
					std::cout << "buffer_ size berfore read is: " << buffer_.size() << std::endl;
					request_stream.read(buffer_.data(), (std::streamsize)buffer_.size());
					std::cout << "buffer_ size after read is: " << buffer_.size() << std::endl;
					std::cout << "[OFSTREAM OUTPUT] :: " << __FUNCTION__ << " write " << request_stream.gcount() << " bytes.\n";
					output_file.write(buffer_.data(), request_stream.gcount());
				} while (request_stream.gcount()>0);

				// Recv data
				size_t len = socket_.read_some(boost::asio::buffer(buffer_.data(), buffer_.size()), error);
				DEBUG("image data content size: ", len);
				if (len > 0) {
					// Store data
					output_file.write(buffer_.data(), (std::streamsize)len);
				}
				if (output_file.tellp() == (std::fstream::pos_type)(std::streamsize)file_size) {
					std::cout << "[OFSTREAM OUTPUT] :: received " << output_file.tellp() << " bytes.\n"; // file was received
				}
				if (error)
				{
					std::cout << "[SYSTEM] :: [RCV IMG CONTENT] ERROR... " << error << std::endl;
					break;
				}


				// send rcv
				message_.assign("rcv", 4);
				boost::asio::write(socket_, boost::asio::buffer(message_),
					boost::asio::transfer_all(), error);
			}
			//*
			// Recv header
			boost::asio::streambuf header_buf;
			boost::asio::read_until(socket_, header_buf, "\n\n");
			//std::size_t len = socket_.read_some(boost::asio::buffer(buf), error);
			// alloc buffer
			std::cout << "[1]  HEADER stream buf size is " << header_buf.size() << std::endl;

			std::istream request_stream(&header_buf);
			std::string file_path; size_t file_size = 0;
			request_stream >> file_path;
			request_stream >> file_size;
			buffer_.resize(file_size);
			request_stream.read(buffer_.data(), 2); // eat the "\n\n"
			std::cout << "[2]  File path is: " << file_path << " , file size is " << file_size << std::endl;
			//size_t pos = file_path.find_last_of("\\");
			//if (pos != std::string::npos)
			//	file_path = file_path.substr(pos + 1);
			std::ofstream output_file(file_path.c_str(), std::ios_base::binary); //output_file.open();
			if (!output_file)
			{
				std::cout << "[OFSTREAM OUTPUT] :: failed to open " << file_path << std::endl;
				return;
			}
			do
			{
				request_stream.read(buffer_.data(), (std::streamsize)buffer_.size());
				std::cout << "[OFSTREAM OUTPUT] :: " << __FUNCTION__ << " write " << request_stream.gcount() << " bytes.\n";
				output_file.write(buffer_.data(), request_stream.gcount());
			} while (request_stream.gcount()>0);

			// Recv data
			size_t len = socket_.read_some(boost::asio::buffer(buffer_.data(), buffer_.size()), error);
			if (len > 0) {
				// Store data
				output_file.write(buffer_.data(), (std::streamsize)len);
			}
			if (output_file.tellp() == (std::fstream::pos_type)(std::streamsize)file_size) {
				LOGOUT("<<<Last one>>> Store img data...");
				std::cout << "[OFSTREAM OUTPUT] :: received " << output_file.tellp() << " bytes.\n";
			}
			if (error)
			{
				std::cout << "[SYSTEM] :: [RCV IMG CONTENT] ERROR... " << error << std::endl;
				return;
			}
			//*/
			rcv_speed.Print(">>> [TIMER] :: [RCV FILES] :: ** Total Recv time is ** ");
		}

	}

	boost::asio::ip::tcp::socket& getSocket() {
		return socket_;
	}

private:
	tcp_connection(boost::asio::io_service& io_service)
		: socket_(io_service)
	{
	}

	tcp::socket socket_;
	std::string message_;
	std::vector<char> buffer_;
};

class tcp_server
{
public:
	tcp_server(boost::asio::io_service& io_service, int port)
		: acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		port_ = port;
		start_accept();
	}

	void parsing_cmd(const std::string& cmd) {
		cmd_ = cmd;
		boost::lock_guard<boost::mutex> lock(mio);
		std::cout << "[HOLY SHIT] LISTEN TO PORT: " << port_ << " ** SUCCESS ** !!!" << std::endl;
		
		if (cmd_ == "get imgs") {
			std::cout << "[SYSTEM] :: SENDING 'GET IMAGES' COMMAND... " << std::endl;
			image_session_->send_image();
		}
		else if (cmd_ == "start cap") {
			std::cout << "[SYSTEM] :: SENDING 'GET IMAGES' COMMAND... " << std::endl;
			// client session
			boost::system::error_code error;
			std::string cmd;
			cmd.assign("scp", 4);
			boost::asio::write(image_session_->socket(), boost::asio::buffer(cmd),
				boost::asio::transfer_all(), error);
			char buf[256];
			size_t len = image_session_->socket().read_some(boost::asio::buffer(buf), error);
			std::cout.write(buf, len);
		}
		else if (cmd == "stop cap") {
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
		else if (cmd == "set config") {
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
		else if (cmd == "syn nodes") {
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
		else {
			std::cout << "[SYSTEM] :: [Port]" << port_ <<":: INVALID COMMAND.. Please enter command again..." << std::endl;
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
		if (!error) {
			//image_session_->teststart();
			// Maybe put time sychronization code here...
			boost::asio::ip::tcp::no_delay option(true);
			image_session_->getSocket().set_option(option);
		}
		// If open comment, will cause connection failed...
		//start_accept();
	}

	// 4* acceptors + sessions
	tcp::acceptor acceptor_;
	tcp_connection::pointer image_session_;

	// 4* sockets + resolver for client

	std::string cmd_;
	int port_;
	boost::mutex mio;
};

int main()
{
	try
	{
		// Maybe need 2 io_services, one for client func, one for server func
		boost::asio::io_service io_service;
		boost::asio::io_service::work work(io_service);
		tcp_server server0(io_service, 1113);
		tcp_server server1(io_service, 1114);
		tcp_server server2(io_service, 1115);
		tcp_server server3(io_service, 1116);
		boost::thread_group threads;

		boost::mutex mutex;
		// Service must be run before detecting cmd input
		for (int i = 0; i < 4; i++) {
			threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
		}

		char cmd_line[1024];
		while (std::cin.getline(cmd_line, 1024)) {
			//std::cin.getline(cmd_line, 1024);
			//server.parsing_cmd(cmd_line);
			io_service.post(boost::bind(&tcp_server::parsing_cmd, &server0, cmd_line));
			io_service.post(boost::bind(&tcp_server::parsing_cmd, &server1, cmd_line));
			io_service.post(boost::bind(&tcp_server::parsing_cmd, &server2, cmd_line));
			io_service.post(boost::bind(&tcp_server::parsing_cmd, &server3, cmd_line));
		}

		io_service.stop();
		threads.join_all();


	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
#endif
