//
// client.cpp
// ~~~~~~~~~~

#if 1
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <ClientComm.h>
///////////////////////////////////////////////////////////////////////////////////
/////                            CAPTURE   PART                               /////
///////////////////////////////////////////////////////////////////////////////////
#include <libfreenect2/config.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/packet_processor.h>
#include <libfreenect2/frame_listener.hpp>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/async_packet_processor.h>
#include <libfreenect2/rgb_packet_stream_parser.h>
#include <libfreenect2/depth_packet_stream_parser.h>
#include <libfreenect2/threading.h>
#include <timer.h>
#include <opencv2/opencv.hpp>
using namespace libfreenect2;
//--------------------------VARIABLES------------------------------
// set protonect won't be shutdown in current configuration
bool protonect_shutdown = false;
bool bkey_capture_frame = false;			// save to imgs
bool bkey_request_frame = false;			// request newest frame
// Create freenect instances
const int MAX_KINECT_CONNECTION = 2;
const int MAX_VIEW_NAME_LENGTH = 64;

static Freenect2 freenect2[MAX_KINECT_CONNECTION];
static Freenect2Device *dev[MAX_KINECT_CONNECTION];
static HANDLE hThread[MAX_KINECT_CONNECTION];
int save_count[MAX_KINECT_CONNECTION];

class Kinect_args{
public:
	int		 id;
	// DONT QUITE SURE THIS IS GOOD
	float	 save_period;
	Timer timer;
	boost::asio::ip::tcp::socket* skt;
	int save_count;
public:
	Kinect_args(){
	}
	~Kinect_args(){
	}
};

template<typename T>
void LOGOUT(T& str){
	std::cout << "[LOG] :: " << str << std::endl;
}
template<typename T>
void DEBUG(const char* s, T& str) {
	std::cout << "[DEBUG LOG] :: " << s << "->" << str << std::endl;
}

// Pre-called functions
unsigned int __stdcall libfreenect2_loop(PVOID args_);

///////////////////////////////////////////////////////////////////////////////////
/////                            NETWORK   PART                               /////
///////////////////////////////////////////////////////////////////////////////////
using boost::asio::ip::tcp;
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
	boost::asio::read_until(socket, header_buf, "\n\n");
	std::cout << "[1]  HEADER stream buf size is " << header_buf.size() << std::endl;
	std::string file_path; size_t file_size = 0;
	std::istream request_stream(&header_buf);
	request_stream >> file_path;
	//DEBUG("file_path", file_path);
	request_stream >> file_size;
	//DEBUG("file_size", file_size);
	std::vector<char> buffer_;
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
	boost::system::error_code error;
	size_t len = socket.read_some(boost::asio::buffer(buffer_.data(), buffer_.size()), error);
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
		return;
	}
}

int main(int argc, char* argv[])
{
	try
	{
		///////////////////////////////////////////////  FREENECT2  ///////////////////////////////////////////////
		// reset the singal won't be shut down
		protonect_shutdown = false;

		// enum devices
		int numDevices = freenect2[0].enumerateDevices();
		std::cout << "numDevices : " << numDevices << std::endl;
		if (numDevices > MAX_KINECT_CONNECTION) {
			numDevices = MAX_KINECT_CONNECTION;
		}

		// open devices + configure args
		DepthPacketProcessor::Config config;
		config.MinDepth = 0.5f;
		config.MaxDepth = 4.5f;
		config.EnableBilateralFilter = true;
		config.EnableEdgeAwareFilter = true;
		Kinect_args args[MAX_KINECT_CONNECTION];
		OpenCLPacketPipeline p[MAX_KINECT_CONNECTION];
		for (int i = 0; i < numDevices; ++i) {
			args[i].id = i;
			//args[i].save_count = 0;
			p[i].getDepthPacketProcessor()->setConfiguration(config);
			dev[i] = freenect2[i].openDevice(i, &p[i]);
		}

		///////////////////////////////////////////////  NETWORK  ///////////////////////////////////////////////
		// Constructor, input: IP,PORT
		boost::asio::io_service io_service;
		tcp::resolver resolver(io_service);
		//tcp::resolver::query query("192.168.0.1", "1113");
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
		boost::asio::ip::tcp::no_delay option(true);
		socket.set_option(option);
		LOGOUT(serverip);
		LOGOUT("Connect to server successfully!!!");

		// Listen to port to receive command


		//Transfer data
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

			if (cmd == "scp"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive capture cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);
				// wait
				//std::cout << "[Capture System] :: Start using Threads" << std::endl;
				bkey_capture_frame = true;

				// begin threads
				for (int i = 0; i < numDevices; ++i) {
					hThread[i] = (HANDLE)_beginthreadex(						// return the HANDLE of thread
						NULL,													// security
						0,														// stack_size "0" = default (1MB)
						libfreenect2_loop,										// address of function
						&args[i],												// arg list
						0,														// 0 indicates manipulate immediately 
						NULL);													// return thread ID, NULL means no return
				}
			}
			else if (cmd == "stp"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive stop capture cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);
				bkey_capture_frame = false;
				protonect_shutdown = true;
			}
			else if (cmd == "cnf"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive set config file cmd \n";
				recv_buffer(socket);
				// send rcv
				std::string message;
				message.assign("rcv", 4);
				boost::asio::write(socket, boost::asio::buffer(message),
					boost::asio::transfer_all(), error);

				// PARSING CONFIG FILE
				// ...
			}
			else if (cmd == "rqs"){
				

			}
			else if (cmd == "gis"){
				Timer speed;

				// Send time index file
				char idxpath[128];
				sprintf(idxpath, "CapturedData/K%d/K%d_tIdx.txt", 0, 0);
				send_buffer(socket, idxpath);
				boost::asio::streambuf rcv_buf0;
				boost::asio::read_until(socket, rcv_buf0, "rcv");
				std::istream rcv_stat0_stream(&rcv_buf0);
				std::string checkRcv;
				rcv_stat0_stream >> checkRcv;
				LOGOUT(checkRcv);

				// send total image info
				LOGOUT("SEND TOTAL IMGS NUMBER TO SERVER...");
				std::cout << "TOTAL CAPTURED IMAGE NUMBER IS :: " << save_count[0] << std::endl;
				boost::asio::streambuf numimg_request;
				std::ostream num_request_stream(&numimg_request);
				num_request_stream << save_count[0] << "\n\n";
				boost::asio::write(socket, numimg_request, error);
				buf.assign(0);
				// read rcv
				//len = socket.read_some(boost::asio::buffer(buf), error);
				boost::asio::streambuf rcv_buf;
				boost::asio::read_until(socket, rcv_buf, "rcv");
				std::istream rcv_stat_stream(&rcv_buf);
				//std::string checkRcv;
				rcv_stat_stream >> checkRcv;
				LOGOUT(checkRcv);

				// send stored images
				LOGOUT("Execute SEND IMGS OPERATIONS");
				speed.Start();
				char filepath[128];
				for (int i = 0; i < save_count[0]; ++i){

					sprintf(filepath, "CapturedData/K0/Pose_%d.png", i);
					send_buffer(socket, filepath);
					if (i < save_count[0] - 1){
						buf.assign(0);
						// read rcv
						//len = socket.read_some(boost::asio::buffer(buf), error);
						boost::asio::streambuf rcv_buf1;
						boost::asio::read_until(socket, rcv_buf1, "rcv");
						std::istream rcv_stat1_stream(&rcv_buf1);
						rcv_stat1_stream >> checkRcv;
						LOGOUT(i);
						LOGOUT(checkRcv);

						if (checkRcv.compare("rcv")){
						}
						else{
							LOGOUT("ERROR happens during transefer img no.");
							LOGOUT(i);
							// Resend?
						}
					}

					{
						if (error == boost::asio::error::eof)
							break; // Connection closed cleanly by peer.
						else if (error)
							throw boost::system::system_error(error); // Some other error.
					}
				}// for
				speed.Print(">>> [TIMER] :: [Transmission files] :: ** Total runing time **");

			}
			else if (cmd == "syn"){
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


unsigned int __stdcall libfreenect2_loop(int id){
	// view name
	char depth_name[MAX_VIEW_NAME_LENGTH];
	sprintf(depth_name, "DEPTH_%d", args.id);

	// check device
	if (!dev[args.id]) {
		std::cout << "[Capture System] :: No device connected or failure opening the default one!!" << std::endl;
		return -1;
	}

	// listener
	SyncMultiFrameListener listener(Frame::Depth);
	dev[args.id]->setColorFrameListener(&listener);
	dev[args.id]->setIrAndDepthFrameListener(&listener);
	dev[args.id]->start();

	// capture frame
	FrameMap frame;
	char saveDepth_path[50];
	//const char cap_res[30] = "[SYSTEM] CAPTURE time is";
	//const char sav_res[30] = "[SYSTEM] SAVING time is";
	int key = -1;
	//cv::Mat rgb_mat;
	//cv::Mat ir_mat;
	cv::Mat depth_mat;
	cv::Mat sav_depth;
	std::vector<double> tstamp;
	// Timer
	args.timer.Start();
	// loop
	while (!protonect_shutdown) {
		// listen to new frame comes
		listener.waitForNewFrame(frame);
		// put correspondent data to correspondent categories
		Frame *depth = frame[Frame::Depth];
		//	DEPTH
		depth_mat = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data);
		cv::imshow(depth_name, depth_mat / 4500.0f);
		// record time
		args.timer.Print("record timestamp: ");
		tstamp.push_back(args.timer.Record());

		key = cv::waitKey(1);
		//*
		if (bkey_capture_frame){
			depth_mat.convertTo(sav_depth, CV_16UC1);
			sprintf(saveDepth_path, "CapturedData/K%d/Pose_%d.png", args.id, save_count[0]);
			// debug
			//std::cout << "[SAVING PATH IS] :::::: " << saveDepth_path << "!!!!" << std::endl;
			cv::imwrite(saveDepth_path, sav_depth);
			++save_count[0];
		}
		//*/
		// release
		listener.release(frame);
		protonect_shutdown = protonect_shutdown || (key > 0 && ((key & 0xFF) == 27)); // shutdown on escap
		// free space after display the data
	}
	// save idx
	if (tstamp.size() >= save_count[0]){
		char saveDepIdx_path[50];
		char timestr[128];
		sprintf(saveDepIdx_path, "CapturedData/K%d/K%d", args.id, args.id);
		std::string depthIdx_path = saveDepIdx_path;
		std::filebuf fb;
		fb.open(depthIdx_path + "_tIdx.txt", std::ios::out);
		for (int i = 0; i < save_count[0]; ++i){
			std::ostream os(&fb);
			sprintf(timestr, "%.2f", tstamp[i]);
			os << timestr << std::endl;
		}
		fb.close();
	}

	// stop
	std::cout << "[CAPTURE SYSTEM] :: <KINECT " << args.id << "> ***** Save *****     ***** DONE ***** !!!" << std::endl;
	dev[args.id]->stop();
	dev[args.id]->close();
	return 0;
}

#endif
