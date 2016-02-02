//
// client.cpp
// ~~~~~~~~~~

#if 0
#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <ClientComm.h>

using boost::asio::ip::tcp;

template<typename T>
void LOGOUT(T& str){
	std::cout << "[LOG] :: " << str << std::endl;
}



// Comm between two socket pack

int main(int argc, char* argv[])
{
	try
	{
		boost::asio::io_service io_service;
		// Constructor, input: IP,PORT
		tcp::resolver resolver(io_service);
		tcp::resolver::query query("127.0.0.1", "1113");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		// Init socket and setup connection
		tcp::socket socket(io_service);
		boost::system::error_code error = boost::asio::error::host_not_found;
		while (error && endpoint_iterator != end)
		{
			socket.close();
			socket.connect(*endpoint_iterator++, error);
		}
		std::string serverip = socket.remote_endpoint().address().to_string().c_str();
		LOGOUT(serverip);
		LOGOUT("Connect to server successfully!!!");
		
		// Listen to port to receive command
		

		//Transfer data
		std::string send_data;
		int count_img = 5;
		
		std::string cmd;
		for (;;)
		{
			boost::array<char, 256> buf;
			boost::system::error_code error;

			buf.assign(0);
			size_t len = socket.read_some(boost::asio::buffer(buf), error);
			//size_t len = boost::asio::read(socket, buffer, error);
			std::cout.write(buf.data(), len);
			if (len > 0){
				cmd = buf.data();
				LOGOUT("Recv server cmd, cmd content in below: ");
				LOGOUT(cmd);
			}
		
			boost::asio::streambuf img_request;
			
			if(cmd == "scp"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive capture cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);

			}
			else if(cmd == "stp"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive stop capture cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);
			}
			else if(cmd == "cnf"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive set config file cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);
			}
			else if (cmd == "gis"){
				// CALL IMG_SOCK pack 
				LOGOUT("Execute SEND IMGS OPERATIONS");
				for (int i = 0; i < count_img; ++i){
					
					size_t filesize = 21000;
					
					std::ostream request_stream(&img_request);
					request_stream << i << "\n" << filesize << "\n\n";
					boost::asio::write(socket, img_request, error);
					
					buf.assign(0);
					len = socket.read_some(boost::asio::buffer(buf), error);
					std::string fuck = buf.data();
					LOGOUT(i);
					LOGOUT(fuck);
					if (fuck == "rcv"){
						send_data = comm::make_img_data_string(i);
						boost::asio::write(socket, boost::asio::buffer(send_data),
							boost::asio::transfer_all(), error);
					}
					else{
						LOGOUT("ERROR happens during transefer img no.");
						LOGOUT(i);
						// Resend?
						//send_data = comm::make_img_header_string(i);
						//boost::asio::write(socket, boost::asio::buffer(send_data),
						//	boost::asio::transfer_all(), error);
					}
					if (i < count_img - 1){
						buf.assign(0);
						len = socket.read_some(boost::asio::buffer(buf), error);
						fuck = buf.data();
						LOGOUT(i);
						LOGOUT(fuck);
					
						if (fuck == "rcv"){

						}
						else{
							LOGOUT("ERROR happens during transefer img no.");
							LOGOUT(i);
							// Resend?
							//send_data = comm::make_img_header_string(i);
							//boost::asio::write(socket, boost::asio::buffer(send_data),
							//	boost::asio::transfer_all(), error);
						}
					}
					

					{
						if (error == boost::asio::error::eof)
							break; // Connection closed cleanly by peer.
						else if (error)
							throw boost::system::system_error(error); // Some other error.
					}
				}// for
			}
			else if(cmd == "syn"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive synchronize nodes cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);
			}

			
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

/*
// Write to server.
boost::asio::streambuf write_buffer;
std::ostream output(&write_buffer);
output << "abc";
std::cout << "Writing: " << make_string(write_buffer) << std::endl;
auto bytes_transferred = boost::asio::write(server_socket, write_buffer);

// Read from client.
boost::asio::streambuf read_buffer;
bytes_transferred = boost::asio::read(client_socket, read_buffer,
	boost::asio::transfer_exactly(bytes_transferred));
std::cout << "Read: " << make_string(read_buffer) << std::endl;
read_buffer.consume(bytes_transferred); // Remove data that was read.
//*/

#endif
