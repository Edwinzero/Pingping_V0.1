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
/////                             DEBUG   PART                                /////
///////////////////////////////////////////////////////////////////////////////////
template<typename T>
void LOGOUT(T& str){
	std::cout << "[LOG] :: " << str << std::endl;
}
template<typename T>
void DEBUG(const char* s, T& str) {
	std::cout << "[DEBUG LOG] :: " << s << "->" << str << std::endl;
}
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
const int MAX_KINECT_CONNECTION = 1;
const int MAX_VIEW_NAME_LENGTH = 64;

static Freenect2 freenect2[MAX_KINECT_CONNECTION];
std::shared_ptr<Freenect2Device> dev[MAX_KINECT_CONNECTION];
std::shared_ptr<SyncMultiFrameListener> listener[MAX_KINECT_CONNECTION];


class Kinect_args{
public:
	int				 sensorID;						// global id
	std::string		 serial;					// serial num
	// DONT QUITE SURE THIS IS GOOD
	float	 save_period;
	Timer timer;
	int save_count;

	// view name
	char depth_name[MAX_VIEW_NAME_LENGTH];

public:
	Kinect_args() : sensorID(-1), save_period(0), save_count(0){
	}
};

void MatToHiLoVec(cv::Mat &mat, std::vector<char> &array){
	// alloc hilo buffer
	int num = mat.size().area();
	array.clear();
	array.resize(num * 2);
	for (int i = 0; i < num; ++i){
		array[2 * i] = ((char)(mat.ptr()[i]) & 0xFF);   // even store low
		array[2 * i + 1] = ((char)mat.ptr()[i] >> 8); // odd  store high
	}

}
void captureFrame(int id, cv::Mat& f){
	FrameMap frame;
	listener[id]->waitForNewFrame(frame);
	Frame *depth = frame[Frame::Depth];
	//cv::Mat depth_mat = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data);

	f = cv::Mat(depth->height, depth->width, CV_16UC1);
	for (int i = 0; i < depth->height * depth->width; i++) {
		((unsigned short*)(f.data))[i] = ((float*)(depth->data))[i];
	}
	((unsigned short*)(f.data))[0] = 233;
	//cv::Mat(depth->height, depth->width, CV_32FC1, depth->data).convertTo(f, CV_16UC1);
	//cv::imshow("omg", f);
	//cv::waitKey(1);
	listener[id]->release(frame);
}

///////////////////////////////////////////////////////////////////////////////////
/////                            NETWORK   PART                               /////
///////////////////////////////////////////////////////////////////////////////////
using boost::asio::ip::tcp;
bool deviceInitialized = false;        // flag to check whether it is initialized
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

void send_buffer(boost::asio::ip::tcp::socket& socket, Kinect_args& k_info, cv::Mat& frame){
	// 1. 
	int bufsize = frame.total()*frame.elemSize();
	// 2. allocate streambuf to tranfer buffer size
	boost::asio::streambuf stream_request;
	std::ostream request(&stream_request);
	// NEED TO FIX --------------------------------------------------------------------------------------------------------------------------------------------------------------------
	//request.write(reinterpret_cast<char*>(&k_info.id_), 4);
	request.write(reinterpret_cast<char*>(&bufsize), 4);
	//std::cout << "[KINECT ID] :: is:  " << k_info.id_ << " :: serial :: " << k_info.serial_ << std::endl;
	std::cout << "[filesize] :: file size is: bytes " << bufsize << std::endl;
	std::cout << "[1]  HEADER stream buf size is " << stream_request.size() << std::endl;
	std::cout << "BUFFER data SIZE: (buffer.size)" << frame.total() << std::endl;
	// 3. write header to receiver
	boost::system::error_code error;
	boost::asio::write(socket, stream_request, boost::asio::transfer_exactly(4), error);
	std::cout << "[SYSTEM] :: start sending file content of latest frame, size: " << bufsize << std::endl;
	{
		//4. write content to receiver
		boost::asio::write(socket, boost::asio::buffer(frame.ptr(), (std::streamsize)bufsize),
			boost::asio::transfer_all(), error);
		if (error)
		{
			std::cout << "[SYSTEM] :: [I/O] :: send error:" << error << std::endl;
			return;
		}
	}
}

void recv_buffer(boost::asio::ip::tcp::socket& socket, char filepath[]){
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
	std::sprintf(filepath, file_path.data());
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
	size_t len = socket.read_some(boost::asio::buffer(buffer_.data(), (std::streamsize)buffer_.size()), error);
	//for (int i = 0; i < buffer_.size(); ++i){
	//	std::cout << buffer_[i];
	//}
	std::cout << std::endl;
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

///////////////////////////////////////////////////////////////////////////////////////
/////                            MAIN FUNCTION PART                               /////
///////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	try
	{
		///////////////////////////////////////////////  FREENECT2  ///////////////////////////////////////////////
		// reset the singal won't be shut down
		protonect_shutdown = false;

		// enum devices  (numK guides device iteration)
		int numDevices = freenect2[0].enumerateDevices();
		std::cout << "numDevices : " << numDevices << std::endl;
		if (numDevices > MAX_KINECT_CONNECTION) {
			numDevices = MAX_KINECT_CONNECTION;
		}
		Kinect_args args[MAX_KINECT_CONNECTION];


		///////////////////////////////////////////////  NETWORK  ///////////////////////////////////////////////
		// Constructor, input: IP,PORT
		boost::asio::io_service io_service;
		tcp::resolver resolver(io_service);
		//tcp::resolver::query query("192.168.10.1", "1113");
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
		deviceInitialized = false;
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
				if (deviceInitialized == false){
					std::string ack = "[CLIENT MSG] :: [ACK] Receive run kinect cmd \n";
					boost::asio::write(socket, boost::asio::buffer(ack), error);
					// wait
					//std::cout << "[Capture System] :: Start using Threads" << std::endl;

					// <<< KINECT SETTING >>> //
					// open devices + configure args
					DepthPacketProcessor::Config config;
					config.MinDepth = 0.5f;
					config.MaxDepth = 4.5f;
					config.EnableBilateralFilter = true;
					config.EnableEdgeAwareFilter = true;

					OpenCLPacketPipeline p[MAX_KINECT_CONNECTION];
					for (int i = 0; i < numDevices; ++i) {
						p[i].getDepthPacketProcessor()->setConfiguration(config);
						//dev[i].reset(freenect2[i].openDevice(i, &p[i]));
						dev[i].reset(freenect2[i].openDevice(i, new OpenCLPacketPipeline(i)));
						if (!dev[i]){
							std::cout << "no device connected or failure opening the default one!" << std::endl;
							return -1;
						}
						listener[i].reset(new SyncMultiFrameListener(Frame::Color | Frame::Ir | Frame::Depth));
						dev[i]->setColorFrameListener(listener[i].get());
						dev[i]->setIrAndDepthFrameListener(listener[i].get());
						dev[i]->start();
					}
				}
				else{
					OpenCLPacketPipeline p[MAX_KINECT_CONNECTION];
					for (int i = 0; i < numDevices; ++i) {
						listener[i].reset(new SyncMultiFrameListener(Frame::Color | Frame::Ir | Frame::Depth));
						dev[i]->setColorFrameListener(listener[i].get());
						dev[i]->setIrAndDepthFrameListener(listener[i].get());
						dev[i]->start();
					}
				}

			}
			else if (cmd == "stp"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive stop capture cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);
				bkey_capture_frame = false;
				protonect_shutdown = true;
				deviceInitialized = false;
			}
			else if (cmd == "cnf"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive set config file cmd \n";
				char filepath[50];
				recv_buffer(socket, filepath);
				// send rcv
				std::string message;
				message.assign("rcv", 4);
				boost::asio::write(socket, boost::asio::buffer(message),
					boost::asio::transfer_all(), error);

				// PARSING CONFIG FILE
				// ...
				std::ifstream infile(filepath);
				std::cout << "filepath is: " << filepath << std::endl;
				for (int i = 0; i < MAX_KINECT_CONNECTION; ++i){
					infile >> args[i].serial; 
					infile >> args[i].sensorID;
					std::cout << "FOR KINECT NO." <<  i << " ==> serial :: " << args[i].serial << std::endl;
					std::cout << "               ==> global id :: " << args[i].sensorID << std::endl;
				}

			}
			else if (cmd == "rdv"){
				std::cout << "[REQUEST DEVICE] :: start enumerate devices...." << std::endl;
				// enum
				boost::asio::streambuf totalKinectInfo;
				std::ostream request(&totalKinectInfo);
				request.write(reinterpret_cast<char*>(&numDevices), 4);									// total devices
				boost::asio::write(socket, totalKinectInfo, boost::asio::transfer_exactly(4), error);

				boost::asio::streambuf stream_request;
				std::ostream request(&stream_request);
				// <<< KINECT SETTING >>> //
				// open devices + configure args
				DepthPacketProcessor::Config config;
				config.MinDepth = 0.5f;
				config.MaxDepth = 4.5f;
				config.EnableBilateralFilter = true;
				config.EnableEdgeAwareFilter = true;

				OpenCLPacketPipeline p[MAX_KINECT_CONNECTION];
				for (int i = 0; i < numDevices; ++i) {
					p[i].getDepthPacketProcessor()->setConfiguration(config);
					//dev[i].reset(freenect2[i].openDevice(i, &p[i]));
					dev[i].reset(freenect2[i].openDevice(i, new OpenCLPacketPipeline(i)));
					if (!dev[i]){
						std::cout << "no device connected or failure opening the default one!" << std::endl;
						return -1;
					}

					request.write(reinterpret_cast<char*>(&i), 4);									// local id
					std::cout << dev[i]->getSerialNumber() << std::endl;
					request.write((dev[i]->getSerialNumber().data()), 12);							// serial
				}
				size_t len = boost::asio::write(socket, stream_request, boost::asio::transfer_exactly(16*numDevices), error);

				// PARSING CONFIG FILE
				// ...
				//char filepath[10];
				//std::ifstream infile(filepath);
				//std::cout << "filepath is: " << filepath << std::endl;
				//for (int i = 0; i < MAX_KINECT_CONNECTION; ++i){
				//	infile >> args[i].serial;
				//	infile >> args[i].sensorID;
				//	std::cout << "FOR KINECT NO." << i << " ==> serial :: " << args[i].serial << std::endl;
				//	std::cout << "               ==> global id :: " << args[i].sensorID << std::endl;
				//}

			}
			else if (cmd == "rqs"){
				std::vector<cv::Mat> bufs;
				bufs.resize(numDevices);
				//cap
				for (int i = 0; i < numDevices; ++i){
					captureFrame(i, bufs[i]);
					sprintf(args[i].depth_name, "DEPTH_%d", args[i].sensorID);
					//cv::imshow(args[i].depth_name, cv::Mat(424,512,CV_16UC1, bufs[i].data()));
					//cv::imwrite("testtest.png", bufs[i]);
					//cv::waitKey(1);
				}
				//send
				for (int i = 0; i < numDevices; ++i){
					// send buffer
					send_buffer(socket, args[i], bufs[i]);
					// check rcv
					boost::asio::streambuf rcv_buf;
					boost::asio::read_until(socket, rcv_buf, "rcv");
					std::istream rcv_stat1_stream(&rcv_buf);
					std::string checkRcv;
					rcv_stat1_stream >> checkRcv;
					LOGOUT(checkRcv);
				}
			}
			else if (cmd == "rpf"){
				std::string ack = "[CLIENT MSG] :: [ACK] Receive <<request Period capture>> cmd \n";
				boost::asio::write(socket, boost::asio::buffer(ack), error);

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
				std::cout << "TOTAL CAPTURED IMAGE NUMBER IS :: " << args[0].save_count << std::endl;
				boost::asio::streambuf numimg_request;
				std::ostream num_request_stream(&numimg_request);
				num_request_stream << args[0].save_count << "\n\n";
				boost::asio::write(socket, numimg_request, error);
				buf.assign(0);
				// read rcv
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
				for (int i = 0; i < args[0].save_count; ++i){
					sprintf(filepath, "CapturedData/K0/Pose_%d.png", i);
					send_buffer(socket, filepath);
					if (i < args[0].save_count - 1){
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

#if 0
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

		if (bkey_capture_frame){
			depth_mat.convertTo(sav_depth, CV_16UC1);
			sprintf(saveDepth_path, "CapturedData/K%d/Pose_%d.png", args.id, args[0].save_count_);
			// debug
			//std::cout << "[SAVING PATH IS] :::::: " << saveDepth_path << "!!!!" << std::endl;
			cv::imwrite(saveDepth_path, sav_depth);
			++save_count[0];
		}

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
#endif
