#ifndef FILES_H
#define FILES_H

#include <FS.h>

namespace kalfy
{
	namespace files
	{
		File openFileForUpload(const char * fileName);
		void uploadSucceeded(File file, const char * fileName);
		void uploadFailed(File file);
		void appendToFile(const char * fileName, const char * line);
		void deleteFile(const char * fileName);
	}
}

#endif