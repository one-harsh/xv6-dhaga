#define NPROC           64  // maximum number of processes
#define NCPU            8  // maximum number of CPUs
#define NOFILE          16  // open files per process
#define NFILE           100  // open files per system
#define NINODE          50  // maximum number of active i-nodes
#define NDEV            10  // maximum major device number
#define ROOTDEV         0  // device number of file system root disk
#define MAXARG          32  // max exec arguments
#define MAXOPBLOCKS     10  // max # of blocks any FS op writes
#define LOGSIZE         (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF            (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE          2000  // size of file system in blocks
#define MAXPATH         128   // maximum file path name
#define NDISK           2
#define NNETIF          2
#define NTHREAD         512  // 8*NPROC
#define NTHREADPERPROC  8
#define THREADSTACKSIZE 8192

#define DEBUGMODE       0   // Should log debug lines
#define NOISEMODE       0   // Enables noise level traces

#define LOG_LOCKS       0 // log lock acquisitions and releases
#define LOG_SCHED       0 // log schedulings of threads
