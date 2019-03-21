/******************************************************************* 
 *  summery:        同步类任务工作池(模板)
 *  author:         hejl
 *  date:           2016-09-21
 *  description:    其实是对taskpool.h的扩展
 ******************************************************************/
#ifndef __TASKPOOL_HPP_
#define __TASKPOOL_HPP_
#include <thread>
#include <vector>
#include "public.h"
#include "queue.h"

/* usage:  // 使用参考示例
    class TaskRun{
        public:
        int run(int flag, long p2) {
            print("task is %s", flag?"exit":"running");
            if (1 == flag){
                // ... exit handle ...
            }
            return 0;
        }
    };

    TaskQueue<TaskRun, &TaskRun::run> tq;
    TaskRun* ptsk = new TaskRun;
    tq.init(2);
    tq.addTask(ptsk);
    
    // ... do other service ... 
    
    // exit handle
    tq.setExit(); // maybe call by other thread

    tq.uninit(); // wait thread exit end
*/

template< class TIRun, int (TIRun::*run_fun)(int, long), int runparam=0, int exitparam=1 >
class TaskPoolEx
{
public:
    TaskPoolEx() = default;

    int init(int threadcount)
    {
        m_threadcount = threadcount;
        return 0;
    }

    int unInit(void)
    {
        if (!m_exit)
        {
            setExit();
        }

        for (auto &t : m_threads)
            t.join();

        m_tasks.each(ClsTask, true);
        return 0;
    }

    // 设置退出标识,不再接受新任务
    void setExit(void)
    {
        m_exit = true;
        m_tasks.wakeup();
    }

    // 返回任务数量
    int size(void){ return m_tasks.size(); }

    // 添加任务进队列 // 调用addTask(d)后,下次异步进入d->run_fun();
    int addTask(TIRun* task, int delay_ms = 0)
    {
        int ret = 0;
        int taskcount;
        int threadcount; // 当前线程数

        do
        {
            ERRLOG_IF1BRK(m_exit, -1, "ADDTASK| msg=wait exiting");
            IFBREAK_N(NULL==task, -2);

            ERRLOG_IF1BRK(!m_tasks.append_delay(task, delay_ms), -3, "ADDTASK| msg=append fail| size=%d",
                m_tasks.size());

            taskcount = m_tasks.size();
            threadcount = (int)m_threads.size();
            if (taskcount > threadcount && threadcount < m_threadcount)
            {
                m_threads.emplace_back([this]() {
                    while (!m_exit)
                    {
                        TIRun* tsk = NULL;
                        m_tasks.pop_delay(tsk);
                        if (tsk && !m_exit)
                        {
	                    (tsk->*run_fun)(runparam, 1);
                        }
                    }
                    LOGDEBUG("TASKTHREAD| msg=normal thread exit|");
                });
                LOGDEBUG("TASKTHREAD| msg=create task thread| thread=%d/%d",
                    threadcount+1, m_threadcount);
            }
        }
        while (0);

        return ret;
    }

private:

    // 程序退出时清理未完成的任务
    static void ClsTask(TIRun*& task) { if (task) { (task->*run_fun)(exitparam, 0); } }

private:
    Queue<TIRun*> m_tasks;
    std::vector<std::thread> m_threads;
    int m_threadcount = 3; // 最大线程数
    bool m_exit = false;
};

#endif
