#if 1
#include <iostream>
#include <string>

namespace config{

	const int NODE[4] = { 0, 1, 2, 3 };
	const int PORT[4] = { 1113, 1114, 1115, 1116 };

	const unsigned short getPORTFROMNODE(int i) {
		switch (i) {
		case 0:
			return PORT[0];
			break;
		case 1:
			return PORT[1];
			break;
		case 2:
			return PORT[2];
			break;
		case 3:
			return PORT[3];
			break;
		}
	}

	const unsigned short getNODEFROMPORT(int port) {
		switch (port) {
		case 0:
			return NODE[0];
			break;
		case 1:
			return NODE[1];
			break;
		case 2:
			return NODE[2];
			break;
		case 3:
			return NODE[3];
			break;
		}
	}

	// Kinect information
	const char Serial_K0[13] = "506904442542";
	const char Serial_K1[13] = "008427750247";
	const char Serial_K2[13] = "501778742542";
	const char Serial_K7[13] = "026340651247";
	bug
	const char* SERIAL_K[8] = { "506904442542", "008427750247",
		"501778742542", "000000000000",
		"000000000000", "000000000000",
		"000000000000", "026340651247" };
	
};




// refer from https://msdn.microsoft.com/en-us/library/sh8yw82b(v=vs.100).aspx
void _makepath(
	char *path,
	const char *drive,
	const char *dir,
	const char *fname,
	const char *ext
	);
void _wmakepath(
	wchar_t *path,
	const wchar_t *drive,
	const wchar_t *dir,
	const wchar_t *fname,
	const wchar_t *ext
	);


// get current time refer from http://stackoverflow.com/questions/997946/how-to-get-current-time-and-date-in-c 
void current_time(){
	// current date/time based on current system
	//time_t now = time(0);

	// convert now to string form
	//char* dt = ctime(&now);

	//std::cout << "The local date and time is: " << dt << std::endl;
}



#endif
