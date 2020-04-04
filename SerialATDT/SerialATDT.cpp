/*
Author: cofface <cofface@gmail.com>
*/

#include "SerialATDT.h"
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>

#pragma comment (lib, "setupapi.lib")

SerialATDT::SerialATDT() {
	
}

SerialATDT::~SerialATDT() {

}


std::string TCHAR2string(TCHAR* str)
{
	std::string strstr;
	try
	{
		int iLen = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);

		char* chRtn = new char[iLen * sizeof(char)];

		WideCharToMultiByte(CP_ACP, 0, str, -1, chRtn, iLen, NULL, NULL);

		strstr = chRtn;
	}
	catch (std::exception e)
	{
	}

	return strstr;
}

std::wstring string2wstring(const std::string& str)
{
	int num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	wchar_t *wide = new wchar_t[num];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide, num);
	std::wstring w_str(wide);
	delete[] wide;
	return w_str;
}

std::string SerialATDT::getComPortId(std::string customPort)
{
	std::string comPort = "";

	if (!customPort.empty())
		return customPort;

	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	int devOffset = 0;
	DWORD propertyDataType;
	HKEY devKey;
	DWORD portNameSize;
	DWORD result;
	BYTE friendlyName[4096];
	TCHAR devName[4096];
	TCHAR portName[4096];

	// Create a HDEVINFO with all present devices.
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_MODEM, 0, 0, DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		printf("\nSetupDiGetClassDevs() failed: %d\n", GetLastError());
		return "";
	}

	// Enumerate through all devices in Set.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	while (SetupDiEnumDeviceInfo(hDevInfo, devOffset++, &DeviceInfoData))
	{

		if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME,
			&propertyDataType, friendlyName, sizeof(friendlyName), NULL))
		{
			printf("\nSetupDiEnumDeviceInfo() failed: %d\n", GetLastError());
			continue;
		}

		// Look for identifying info in the name
		if (mComPortIdentifier.size() > 0) {
			const char *temp = strstr((const char*)friendlyName, mComPortIdentifier.c_str());

			if (temp == 0) {
				continue;
			}
		}

		// Get the device name.
		if (!SetupDiGetDeviceInstanceId(hDevInfo, &DeviceInfoData, devName, MAX_PATH, NULL))
		{
			printf("\nSetupDiGetDeviceInstanceId() failed: %d\n", GetLastError());
			continue;
		}

		// Open the registry key associated with the device.
		devKey = SetupDiOpenDevRegKey(hDevInfo, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if (devKey == INVALID_HANDLE_VALUE)
		{
			printf("\nSetupDiOpenDevRegKey() failed: %d\n", GetLastError());
			continue;
		}

		// Read the PortName registry key.
		portNameSize = sizeof(portName);
		result = RegQueryValueEx(devKey, TEXT("PortName"), NULL, NULL, (LPBYTE)portName, &portNameSize);
		if (result != ERROR_SUCCESS)
		{
			printf("\nRegQueryValueEx() failed: %d\n", result);
			continue;
		}

		// We are not guaranteed a null terminated string from RegQueryValueEx, so explicitly NULL-terminate it.
		portName[portNameSize / sizeof(TCHAR)] = TEXT('\0');

		// Close the registry key.
		result = RegCloseKey(devKey);
		if (result != ERROR_SUCCESS)
		{
			printf("\nRegCloseKey() failed: %d\n", result);
			continue;
		}

		// Try to open the COM port.
		comPort = TCHAR2string(portName);
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	if(!comPort.empty())
		printf("comPort:%s\n", comPort.c_str());

	return comPort;
}

bool SerialATDT::getComPort(HANDLE *hFile, std::string customPort)
{
	std::string comPort = getComPortId(customPort);

	*hFile = INVALID_HANDLE_VALUE;

	if (comPort.size() > 0) {
		//std::string comPortStr;
		//comPortStr.format("\\\\.\\%s", comPort.c_str());
		char buff[1024];
		snprintf(buff, sizeof(buff), "\\\\.\\%s", comPort.c_str());
		std::string comPortStr = buff;

		*hFile = ::CreateFile(string2wstring(comPortStr).c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (*hFile == INVALID_HANDLE_VALUE) {
			printf("AT file open error %ld\n", GetLastError());
		}
	}

	return *hFile != INVALID_HANDLE_VALUE;
}

bool SerialATDT::sendCommand(std::string command, std::string &response, std::string customPort)
{
	bool retFlag = true;
	HANDLE hFile = NULL;

	response = "";

	if (!getComPort(&hFile, customPort)) {
		printf("SerialATDT-Unable to get comport\n");
		return false;
	}

	::SetupComm(hFile, 2048, 2048);

	DCB dcb = { 0 };
	dcb.DCBlength = sizeof(DCB);
	::GetCommState(hFile, &dcb);

	dcb.BaudRate = 9600;

	// set additional parameters for 8-bit, no parity, one stop bit, no flow control
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;

	::SetCommState(hFile, &dcb);

	// Retrieve the timeout parameters for all read and write operations on the port. 
	COMMTIMEOUTS CommTimeouts;
	::GetCommTimeouts(hFile, &CommTimeouts);

#define BUFFER_LENGTH 256

	// Change the COMMTIMEOUTS structure settings.
	CommTimeouts.ReadIntervalTimeout = 50;
	CommTimeouts.ReadTotalTimeoutMultiplier = (2000 / BUFFER_LENGTH);  // Only wait 2 seconds (2000ms).
	CommTimeouts.ReadTotalTimeoutConstant = 50;
	CommTimeouts.WriteTotalTimeoutMultiplier = 50;
	CommTimeouts.WriteTotalTimeoutConstant = 50;

	// Set the timeout parameters for all read and write operations on the port. 
	::SetCommTimeouts(hFile, &CommTimeouts);

	char buffer[BUFFER_LENGTH + 1];
	OVERLAPPED m_ReadSync;
	unsigned error = 0;
	//EventSem readWait;
	bool finished = false;


	memset(&m_ReadSync, 0, sizeof(OVERLAPPED));

	//m_ReadSync.hEvent = readWait.GetHandle();
	m_ReadSync.Pointer = (void*)buffer;

	static int index = 0;

	// Edited here
	command += "\r";

	// First write the command out to the serial port
	UINT32 numBytesWritten = 0;
	UINT32 totalBytesWritten = 0;
	do {
		if (!::WriteFile(hFile, command.c_str(), command.length(), (LPDWORD)&numBytesWritten, NULL)) {
			printf("SerialATDT-sendCommand failed on sending.\n");
			retFlag = false;
			break;
		}

		totalBytesWritten += numBytesWritten;
	} while (totalBytesWritten < command.length());

	::FlushFileBuffers(hFile);

	// Read in the response
	if (retFlag) {
		Sleep(1000);

		while (!finished) {
			UINT32 numBytesRead = 0;

			buffer[0] = '\0';

			if (!::ReadFile(hFile, buffer, BUFFER_LENGTH, (LPDWORD)&numBytesRead, &m_ReadSync))
			{
				if ((error = ::GetLastError()) == ERROR_IO_PENDING)
				{
					error = 0;
					if (!::GetOverlappedResult(hFile, &m_ReadSync, (LPDWORD)&numBytesRead, true))
					{
						error = ::GetLastError();
					}
				}
			}

			// We read something
			if (numBytesRead > 0) {
				buffer[numBytesRead] = '\0';
				response += buffer;
			}
			else { // We timed out so just drop out
				finished = true;
			}
		}
	}

	// Remove the command from what we read in the response
	if (response.find(command) != std::string::npos) {
		response = response.substr(command.length());
	}

	if (hFile != INVALID_HANDLE_VALUE) {
		::CloseHandle(hFile);
	}

	return retFlag;
}

int main() {
	SerialATDT _at;
	std::string command = "AT^SN";
	std::string response = "";
	std::string customPort = "COM37";
	bool ret = _at.sendCommand(command, response, customPort);
	if(ret)
		printf("%s\n",response.c_str());
	system("pause");

	return 0;
}
