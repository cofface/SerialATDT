#pragma once

#include <string>
#include <windows.h>

using namespace std;

class SerialATDT
{
private:
	std::string mComPortIdentifier;

	bool getComPort(HANDLE *hFile);
	std::string getComPortId();

public:
	//SerialATDT(std::string ident);
	//~SerialATDT(void);

	SerialATDT();
	~SerialATDT();

	bool sendCommand(std::string command, std::string &response);
};