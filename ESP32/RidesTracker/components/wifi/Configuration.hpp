#ifndef __CONFIGURATION_H
#define __CONFIGURATION_H

#include <string>

class Configuration {
    private:
        std::string _ssid;
        std::string _pswd;
    public:
        Configuration(const char * ssid, const char * pswd ) {
            _ssid=std::string(ssid); 
            _pswd=std::string(pswd); 
        }
        const char * getSSID(void) { return _ssid.c_str(); }
        const char * getPassword(void) { return _pswd.c_str(); }
};

#endif //__CONFIGURATION_H