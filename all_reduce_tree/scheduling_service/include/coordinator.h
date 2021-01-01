#ifndef __NEWPLAN_COORDINATOR_H__
#include <vector>
class Coordinator
{
public:
    Coordinator() {}
    virtual ~Coordinator() {}
    virtual void broadcast() = 0;
    virtual void push(int msg) = 0;
    virtual void pull(int &msg) = 0;

protected:
    std::vector<int> groups;
};
#endif