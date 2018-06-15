
#include "Preferences.hpp"
#include <sys/time.h>

class Clock {
    private:
        static constexpr char TAG[] = "Clock";
        static constexpr char ntpServer[] = "pool.ntp.org";
        static constexpr long  gmtOffset_sec = 3600;
        static constexpr int   daylightOffset_sec = 3600;
        static constexpr uint64_t MAX_IDLE_TIME_US = 60 * 1000 * 1000; // 1 minute IDLE
        static constexpr uint64_t POST_DATA_PERIOD_SEC = 5*60;  // once in 5 minutes

        Preferences preferences;
        struct timeval timeWhenPost;
        struct timeval sleepEnterTime;
        struct timeval lastActivityTime;



    public: 
        Clock();
        void loadTimeWhenPost(void);
        void saveTimeWhenPost(void);
        bool isTimeToSendData(void);
        void updateNumberOfResets(void);
        void saveSleepEnterTime(void);
        void loadSleepEnterTime(void);
        void setLastKnownTime(void);
        void updateTimeFromNTP(void);
        void printLocalTime(void);
        void updateActivityTime(void);
        struct timeval getLastActivity(void);
        void getLastActivity(struct timeval time);
        bool isLastActivityTimeout(void);
        void setLocalTime(struct timeval* time);
}