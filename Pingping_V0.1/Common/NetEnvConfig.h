/*   --- MOCA NETWORK CONFIG FILE ---	*/
#include <string>
#include <iostream>
#ifndef CONFIG
#define CONFIG
// Global config variables...
/*---------------------ENV SETTING ---------------------*/
const int TOTAL_NODE = 4;
/*--------------------- IP ADDRESS ---------------------*/
const std::string NODE_1_IP = "192.168.0.11";
const std::string NODE_2_IP = "192.168.0.12";
const std::string NODE_3_IP = "192.168.0.13";
const std::string NODE_4_IP = "192.168.0.14";

const std::string NODE_TEST_IP = "127.0.0.1";

// For Client part
const unsigned short IMG_PORT_1 = 1234;
const unsigned short IMG_PORT_2 = 1235;
const unsigned short IMG_PORT_3 = 1236;
const unsigned short IMG_PORT_4 = 1237;

// For Server part
const unsigned short CON_PORT_1 = 1212;
const unsigned short CON_PORT_2 = 1213;
const unsigned short CON_PORT_3 = 1214;
const unsigned short CON_PORT_4 = 1215;

/*--------------------- UTILITIES ----------------------*/

const char* getIP(int i) {
	switch (i) {
	case 0:
		return NODE_1_IP.c_str();
		break;
	case 1:
		return NODE_2_IP.c_str();
		break;
	case 2:
		return NODE_3_IP.c_str();
		break;
	case 3:
		return NODE_4_IP.c_str();
		break;
	case 4:
		return NODE_TEST_IP.c_str();
		break;
	}
}

const unsigned short getCONPORT(int i) {
	switch (i) {
	case 0:
		return CON_PORT_1;
		break;
	case 1:
		return CON_PORT_2;
		break;
	case 2:
		return CON_PORT_3;
		break;
	case 3:
		return CON_PORT_4;
		break;
	}
}

#endif  /* CONFIG */
