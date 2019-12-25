#pragma once

#include <string>
#include <windows.h>

using namespace std;

class SerialATDT
{
private:
	std::string mComPortIdentifier;

	bool getComPort(HANDLE *hFile, std::string customPort);
	std::string getComPortId(std::string customPort);

public:
	//SerialATDT(std::string ident);
	//~SerialATDT(void);

	SerialATDT();
	~SerialATDT();

	bool sendCommand(std::string command, std::string &response, std::string customPort);
};