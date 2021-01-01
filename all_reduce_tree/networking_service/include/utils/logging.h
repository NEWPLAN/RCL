#ifndef __NEWPLAN_RCL_LOG___
#define __NEWPLAN_RCL_LOG___
#include <glog/logging.h>

class LogRegister
{
public:
    explicit LogRegister()
    {
        set_glog();
    }
    virtual ~LogRegister() {}

private:
    int set_glog(void)
    {

        if (LogRegister::registerted)
            return 1;
        LogRegister::registerted = true;

        char *LOG_LEVEL = getenv("LOG-LEVEL");
        if (LOG_LEVEL != NULL && LOG_LEVEL != nullptr)
        {
            log_level = atoi(LOG_LEVEL);
            FLAGS_logtostderr = 1;
            FLAGS_minloglevel = 0;
            FLAGS_v = log_level;
        }
        printf("LOG-LEVEL: %d\n", log_level);

        return 0;
    }

public:
    static int log_test(void)
    {
        LOG(INFO) << "Hello LOG(INFO)";
        LOG(WARNING) << "Hello LOG(WARNING)";
        LOG(ERROR) << "Hello LOG(ERROR)";
        VLOG(0) << "Hello VLOG(0)";
        VLOG(1) << "Hello VLOG(1)";
        VLOG(2) << "Hello VLOG(2)";
        VLOG(3) << "Hello VLOG(3)";
        VLOG(10) << "Hello VLOG(10)";
        VLOG(100) << "Hello VLOG(100)";
        return 0;
    }

private:
    int log_level = -1;
    static bool registerted;
};

//LogRegister f;
#define REGISTER_LOG_LEVEL(name) \
    static LogRegister mylog_##__COUNTER__##name;

REGISTER_LOG_LEVEL(glog);

#endif