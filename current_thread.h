#pragma once

/**
 *  
    $ ps -ef | grep mysqld
        mysql      2035      1  0 08:56 ?        00:00:41 /usr/sbin/mysqld
    $ top -Hp 2035
        PID  USER      PR  NI    VIRT    RES    SHR S %CPU %MEM     TIME+ COMMAND                                         
        2035 mysql     20   0 1180368    448      0 S  0.0  0.0   0:01.47 mysqld                                          
        2209 mysql     20   0 1180368    448      0 S  0.0  0.0   0:00.00 mysqld                                          
        2236 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.68 mysqld                                          
        2237 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.63 mysqld                                          
        2238 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.68 mysqld                                          
        2239 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.61 mysqld                                          
        2240 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.75 mysqld                                          
        2241 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.73 mysqld                                          
        2242 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.61 mysqld                                          
        2243 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.60 mysqld                                          
        2244 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.70 mysqld                                          
        2245 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.78 mysqld                                          
        2247 mysql     20   0 1180368    448      0 S  0.0  0.0   0:02.81 mysqld                                          
        2295 mysql     20   0 1180368    448      0 S  0.0  0.0   0:01.89 mysqld                                          
        2296 mysql     20   0 1180368    448      0 S  0.0  0.0   0:03.18 mysqld                                          
        2297 mysql     20   0 1180368    448      0 S  0.0  0.0   0:00.37 mysqld                                          
        2298 mysql     20   0 1180368    448      0 S  0.0  0.0   0:04.62 mysqld                                          
        2299 mysql     20   0 1180368    448      0 S  0.0  0.0   0:00.00 mysqld                                          
        2300 mysql     20   0 1180368    448      0 S  0.0  0.0   0:00.00 mysqld                                          
        2301 mysql     20   0 1180368    448      0 S  0.0  0.0   0:00.00 mysqld                                          
        2302 mysql     20   0 1180368    448      0 S  0.0  0.0   0:00.00 mysqld   

 * 
 */

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}

