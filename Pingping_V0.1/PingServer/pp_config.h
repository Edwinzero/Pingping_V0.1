#if 1
#include <iostream>

namespace config{
	const int NODE_0 = 0;
	const int NODE_1 = 1;
	const int NODE_2 = 2;
	const int NODE_3 = 3;

	const int PORT_0 = 1113;
	const int PORT_1 = 1114;
	const int PORT_2 = 1115;
	const int PORT_3 = 1116;
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
