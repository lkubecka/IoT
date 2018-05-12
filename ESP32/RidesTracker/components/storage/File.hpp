#ifndef FILE_H
#define FILE_H

#include <string>
#include <stdio.h>

class File
{
	const int MAX_LINE_LENGTH  = 64;
	private:
		std::string _fileName;
		FILE* _file;
	public:
		File(const char * fileName);
		File openFileForUpload(void);
		void uploadSucceeded(void);
		void uploadFailed(void);
		void appendToFile(const char * line);
		void deleteFile(void);
		void printFile(void);
};

#endif