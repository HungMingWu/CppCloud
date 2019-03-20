
#ifndef _I_TASKRUN_H_
#define _I_TASKRUN_H_

struct ITaskRun2
{
    virtual int run(int p1, long p2) = 0;
    virtual int qrun( int flag, long p2 ) = 0;
    virtual ~ITaskRun2(void){}
};



#endif
