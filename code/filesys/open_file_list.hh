#ifndef NACHOS_FILESYS_OPENFILE_LIST__HH
#define NACHOS_FILESYS_OPENFILE_LIST__HH


class Lock;
class ReadWriteController;


struct FileMetaData {
    // Sector where the file is allocated
    int sector;
    // Used to control concurrent access
    ReadWriteController *lock;
    // Amount of OpenFile instances that reference the file
    int openInstances;
    // True iff Remove has been called on the file
    bool pendingRemove;

    FileMetaData *next;
};

// All public methods of the OpenFileList class are atomic.
class OpenFileList {
    public:
        OpenFileList();

        ~OpenFileList();

        // Adds the file to the open file list.
        // If the file is already open:
        //      and is pending removal, it does nothing.
        //      else it increases openInstances by 1.
        ReadWriteController* AddOpenFile(int sector);

        // Decreases the openInstances by 1. If no open instances remain,
        // the file is removed from the list.
        void CloseOpenFile(int sector);

        // Returns true if the file is currently open, in which case
        // SetUpRemoval sets pendingRemove to true atomically.
        // If the file is not open, it just returns false.
        // Assumes the fileListLock is already taken by the file system.
        bool SetUpRemoval(int sector);

        // Allows the file system to acquire the list's lock.
        void AcquireListLock();

        // Allows the file system to release the list's lock.
        void ReleaseListLock();


    private:
        FileMetaData* FindOpenFile(int sector);
        bool IsEmpty();
        FileMetaData* CreateNode (int sector);
        void DeleteNode(FileMetaData *target);

        Lock *listLock;

        FileMetaData *first, *last;
};


#endif