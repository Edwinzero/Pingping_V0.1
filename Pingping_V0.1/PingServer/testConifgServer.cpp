//
// server.cpp
// ~~~~~~~~~~

#if 1
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdlib.h>

#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <timer.h>
#include <pp_config.h>
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

	void send_image(int id)
	{
		boost::system::error_code error;
		message_.assign("gis", 4);
		LOGOUT(message_);
		boost::asio::write(socket_, boost::asio::buffer(message_),
			boost::asio::transfer_all(), error);
		// Transfer imgidx file
		// rcv imgs
		recv_buffer(socket_);
		// send rcv
		message_.assign("rcv", 4);
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
				// rcv imgs
				recv_buffer(socket_);
				// send rcv
				message_.assign("rcv", 4);
				boost::asio::write(socket_, boost::asio::buffer(message_),
					boost::asio::transfer_all(), error);
			}
			recv_buffer(socket_);
			rcv_speed.Print(">>> [TIMER] :: [RCV FILES] :: ** Total Recv time is ** ");
		}

	}

	void send_config(int id){
		boost::system::error_code error;
		message_.assign("cnf", 4);
		LOGOUT(message_);
		boost::asio::write(socket_, boost::asio::buffer(message_),
			boost::asio::transfer_all(), error);
		Timer speed;
		char filepath[128];
		sprintf(filepath, "CapturedData/config_%d.txt", id);
		speed.Start();
		send_buffer(socket_, filepath);
		speed.Print("SENDING CONFIG FILE TIME IN (ms): ");

		boost::asio::streambuf rcv_buf1;
		boost::asio::read_until(socket_, rcv_buf1, "rcv");
		std::istream rcv_stat1_stream(&rcv_buf1);
		std::string checkRcv;
		rcv_stat1_stream >> checkRcv;
		LOGOUT(checkRcv);

		if (checkRcv.compare("rcv")){
		}
		else{
			LOGOUT("ERROR happens during transefer conifg file");
			// Resend?
		}
	}

	void request_frame(int id){
		boost::system::error_code error;
		message_.assign("rqs", 4);
		LOGOUT(message_);
		boost::asio::write(socket_, boost::asio::buffer(message_),
			boost::asio::transfer_all(), error);

		// rcv imgs
		recv_buffer(socket_);
		need to overwrite this method to return a buffer or cv::mat or Frame
	}

	boost::asio::ip::tcp::socket& getSocket() {
		return socket_;
	}

private:
	tcp_connection(boost::asio::io_service& io_service)
		: socket_(io_service)
	{
	}

	void send_buffer(boost::asio::ip::tcp::socket& socket, char filepath[]){
		std::ifstream source_file(filepath, std::ios_base::binary | std::ios_base::ate);
		if (!source_file)
		{
			std::cout << "[SYSTEM] :: failed to open " << filepath << std::endl;
			return;
		}
		size_t filesize = source_file.tellg();
		source_file.seekg(0);
		boost::asio::streambuf file_request;
		boost::asio::streambuf numimg_request;
		std::ostream request_stream(&file_request);
		request_stream << filepath << "\n" << filesize << "\n\n";
		LOGOUT(filepath);
		LOGOUT(filesize);
		// write header to receiver
		boost::system::error_code error;
		boost::asio::write(socket, file_request, error);

		std::cout << "[SYSTEM] :: start sending file content of " << filepath << std::endl;
		std::vector<char> buffer;
		if (source_file.eof() == false)
		{
			buffer.resize(filesize);
			source_file.read(buffer.data(), (std::streamsize)buffer.size());
			if (source_file.gcount() <= 0)
			{
				std::cout << "[SYSTEM] :: [I/O] :: read file error " << std::endl;
				return;
			}
			// write content to receiver
			boost::asio::write(socket, boost::asio::buffer(buffer.data(),
				source_file.gcount()),
				boost::asio::transfer_all(), error);
			if (error)
			{
				std::cout << "[SYSTEM] :: [I/O] :: send error:" << error << std::endl;
				return;
			}
		}

	}

	void recv_buffer(boost::asio::ip::tcp::socket& socket){//, char filepath[]){
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
		boost::system::error_code error;
		size_t len = socket_.read_some(boost::asio::buffer(buffer_.data(), buffer_.size()), error);
		if (len > 0) {
			// Store data
			output_file.write(buffer_.data(), (std::streamsize)len);
		}
		if (output_file.tellp() == (std::fstream::pos_type)(std::streamsize)file_size) {
			std::cout << "[OFSTREAM OUTPUT] :: received " << output_file.tellp() << " bytes.\n";
		}
		if (error){
			std::cout << "[SYSTEM] :: [RCV IMG CONTENT] ERROR... " << error << std::endl;
			return;
		}
	}

	tcp::socket socket_;
	std::string message_;
	std::vector<char> buffer_;
};

class tcp_server
{
public:
	tcp_server(boost::asio::io_service& io_service, int port, int id)
		: acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		port_ = port;
		start_accept();
		id_ = id;
	}

	void parsing_cmd(const std::string& cmd) {
		Timer checkcmd;
		checkcmd.Start();
		cmd_ = cmd;
		boost::lock_guard<boost::mutex> lock(mio);
		checkcmd.Print("Execute parsing_cmd ... ");

		std::cout << "[HOLY SHIT] LISTEN TO PORT: " << port_ << " ** SUCCESS ** !!!" << std::endl;

		if (cmd_ == "get imgs") {
			std::cout << "[SYSTEM] :: SENDING 'GET IMAGES' COMMAND... " << std::endl;
			// gis
			image_session_->send_image(id_);
		}
		else if (cmd_ == "request frame"){
			std::cout << "[SYSTEM] :: SENDING 'REQUEST LATEST FRAME' COMMAND... " << std::endl;
			// rqt
			image_session_->request_frame(id_);
		}
		else if (cmd_ == "start cap") {
			std::cout << "[SYSTEM] :: SENDING 'GET IMAGES' COMMAND... " << std::endl;
			checkcmd.Print("Start capture time is...");
			// scp
			send_cmd("scp", 4);
		}
		else if (cmd == "stop cap") {
			std::cout << "[SYSTEM] :: SENDING 'STOP CAPTURE' COMMAND... " << std::endl;
			// stp
			send_cmd("stp", 4);
		}
		else if (cmd == "set config") {
			std::cout << "[SYSTEM] :: SENDING 'SET CONFIG' COMMAND... " << std::endl;
			checkcmd.Print("Set config time is...");
			// cnf
			image_session_->send_config(id_);
		}
		else if (cmd == "syn nodes") {
			std::cout << "[SYSTEM] :: SENDING 'SYNCHRONIZE NODES' COMMAND... " << std::endl;
			checkcmd.Print("Synchronize nodes cmd time is...");
			// syn
			send_cmd("syn", 4);
		}
		else {
			std::cout << "[SYSTEM] :: [Port]" << port_ << ":: INVALID COMMAND.. Please enter command again..." << std::endl;
			checkcmd.Print("RECV INVALID CMD time is...");
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
	}

	void send_cmd(char c[], int clen = 4){
		boost::system::error_code error;
		std::string cmd;
		cmd.assign(c, clen);
		boost::asio::write(image_session_->socket(), boost::asio::buffer(cmd),
			boost::asio::transfer_all(), error);
		char buf[256];
		size_t len = image_session_->socket().read_some(boost::asio::buffer(buf), error);
		std::cout.write(buf, len);
	}

	// 4* acceptors + sessions
	tcp::acceptor acceptor_;
	tcp_connection::pointer image_session_;

	// 4* sockets + resolver for client

	std::string cmd_;
	int port_;
	int id_;
	boost::mutex mio;
};

int main()
{
	try
	{
		// Maybe need 2 io_services, one for client func, one for server func
		boost::asio::io_service io_service;
		// Idea from http://thisthread.blogspot.com/2011/04/multithreading-with-asio.html
		boost::asio::io_service::work work(io_service);
		//boost::asio::strand strand(io_service);
#define TEST
#ifdef TEST
		tcp_server server0(io_service, config::PORT_0, config::NODE_0);
#else
		tcp_server server0(io_service, config::PORT_0, config::NODE_0);
		tcp_server server1(io_service, config::PORT_1, config::NODE_1);
		//tcp_server server2(io_service, config::PORT_2, config::NODE_2);
		//tcp_server server3(io_service, config::PORT_3, config::NODE_3);
#endif
		boost::thread_group threads;
		// Service must be run before detecting cmd input
		for (int i = 0; i < 4; i++) {
			threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
		}

		char cmd_line[1024];
		while (std::cin.getline(cmd_line, 1024)) {
			io_service.post(boost::bind(&tcp_server::parsing_cmd, &server0, cmd_line));
#ifndef TEST
			io_service.post(boost::bind(&tcp_server::parsing_cmd, &server1, cmd_line));
			//io_service.post(boost::bind(&tcp_server::parsing_cmd, &server2, cmd_line));
			//io_service.post(boost::bind(&tcp_server::parsing_cmd, &server3, cmd_line));
#endif
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
