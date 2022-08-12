/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"

#include "threads/lock.hh"
#include "read_write_controller.hh"
#include "threads/system.hh"

#include <stdio.h>
#include <string.h>


/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");

    openFiles = new OpenFileList();
    dirList = new DirectoryList();
    freeMapLock = new Lock ("File system free map lock");

    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory();
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);
        dir->SetInitialValue(NUM_DIR_ENTRIES);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
    delete openFiles;
    delete freeMapLock;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *name, unsigned initialSize, bool isDirectory)
{
    ASSERT(name != nullptr);
    ASSERT(initialSize < INDIR_MAX_FILE_SIZE);
    
    if (!isDirectory)
        DEBUG('f', "Creating file %s, size %u\n", name, initialSize);
    else
        DEBUG('f', "Creating directory %s, size %u\n", name, initialSize);


    FilePath path = currentThread->GetPath();
    path.Merge(name);
    std::string file = path.Split();
    dirList->LockAcquire();
    DEBUG('f', "Finding directory\n");
    DirectoryEntry entry = FindPath(&path);
    if (entry.sector == __UINT32_MAX__ || !entry.isDir) {
        dirList->LockRelease();
        return false;
    }
    Lock* dirLock = dirList->OpenDirectory(entry.sector);
    dirList->LockRelease();
    dirLock->Acquire();
    OpenFile* dirFile = new OpenFile(entry.sector);
    Directory *dir = new Directory();
    dir->FetchFrom(dirFile);

    bool success = true;

    if (dir->Find(file.c_str()) != -1) {
        DEBUG('f', "File is already in directory\n");
        success = false;
    } else {
        freeMapLock->Acquire();
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            DEBUG('f', "No free block for file header\n");
            success = false;  
        } else {
            bool shouldExtend = dir->Add(file.c_str(), sector, isDirectory);
            FileHeader *fh = dirFile->GetFileHeader();
            if (shouldExtend)
                success = fh->Extend(freeMap, sizeof(DirectoryEntry));
            if (success) {
                FileHeader *h = new FileHeader;
                success = h->Allocate(freeMap, initialSize);
                // Fails if no space on disk for data.
                if (success) {
                    DEBUG('f', "Creating file success \n");
                    // Everything worked, flush all changes back to disk.
                    fh->WriteBack(dirFile->GetSector());
                    h->WriteBack(sector);
                    dir->WriteBack(dirFile);
                    freeMap->WriteBack(freeMapFile);
                    if (isDirectory) {
                        Directory* newDir = new Directory();
                        newDir->SetInitialValue(initialSize/sizeof(DirectoryEntry));
                        OpenFile* newDirFile = new OpenFile(sector);
                        newDir->WriteBack(newDirFile);
                    }
                }
                delete h;
            }
        }
        freeMapLock->Release();
        delete freeMap;
    }
    dirList->LockAcquire();
    dirLock->Release();
    dirList->CloseDirectory(entry.sector);
    dirList->LockRelease();
    delete dir;
    delete dirFile;
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    FilePath path = currentThread->GetPath();
    path.Merge(name);
    dirList->LockAcquire();
    DirectoryEntry entry = FindPath(&path);

    OpenFile  *openFile = nullptr;
    if (entry.sector == __UINT32_MAX__ || entry.isDir) {
        dirList->LockRelease();
        return openFile;
    }
    path.Split(); 
    DirectoryEntry dirEntry = FindPath(&path);
    Lock* dirLock = dirList->OpenDirectory(dirEntry.sector);
    dirList->LockRelease();
    dirLock->Acquire();

    DEBUG('f', "Opening file %s\n", name);
    int sector = entry.sector;
    if (sector >= 0) {
        openFiles->AcquireListLock();
        ReadWriteController* fl = openFiles->AddOpenFile(sector);
        openFiles->ReleaseListLock();
        if (!fl) {
            dirList->LockAcquire();
            dirList->CloseDirectory(dirEntry.sector);
            dirList->LockRelease();
            dirLock->Release();
            return nullptr;
        }
        openFile = new OpenFile(sector, fl, path);  // `name` was found in directory.
    }
    dirList->LockAcquire();
    dirLock->Release();
    dirList->CloseDirectory(dirEntry.sector);
    dirList->LockRelease();
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *name)
{
    ASSERT(name != nullptr);

    FilePath path = currentThread->GetPath();
    path.Merge(name);

    dirList->LockAcquire();
    DirectoryEntry dirEntry = FindPath(&path);

    if (dirEntry.sector == __UINT32_MAX__) {
        dirList->LockRelease();
        return false;  // file not found
    }
    bool success = true;
    FilePath dirPath = currentThread->GetPath();
    dirPath.Merge(name);
    dirPath.Split(); 
    DirectoryEntry currentDirEntry = FindPath(&dirPath);
    Lock* dirLock = dirList->OpenDirectory(currentDirEntry.sector);
    dirList->LockRelease();
    dirLock->Acquire();
    
    if(dirEntry.isDir) {
        dirList->LockAcquire();
        Lock* dirToDeleteLock = dirList->OpenDirectory(dirEntry.sector);
        dirList->LockRelease();
        dirToDeleteLock->Acquire();

        OpenFile *toRemoveFile = new OpenFile(dirEntry.sector);
        Directory *dirToRemove = new Directory();
        dirToRemove->FetchFrom(toRemoveFile);
        const RawDirectory* raw = dirToRemove->GetRaw();
        unsigned i;
        for (i = 0; i < raw->tableSize; i++) {
            if (raw->table[i].inUse) 
                break;
        }
        dirList->LockAcquire();
        dirToDeleteLock->Release();
        dirList->CloseDirectory(dirEntry.sector);
        if (i < raw->tableSize || !dirList->CanRemove(dirEntry.sector)) {
            delete toRemoveFile;
            delete dirToRemove;
            success = false;
        }
        if (success) {
            std::string file = path.Split();
            int dirSector = FindPath(&path).sector;
            DeleteFromDisk(dirSector);
        }
        dirList->LockRelease();
    } else {
        openFiles->AcquireListLock();
        if (openFiles->SetUpRemoval(dirEntry.sector)){
            std::string file = path.Split();
            int fileSector = FindPath(&path).sector;
            DeleteFromDisk(fileSector);
        }
            
        openFiles->ReleaseListLock();
    }
    dirList->LockAcquire();
    dirLock->Release();
    dirList->CloseDirectory(currentDirEntry.sector);
    dirList->LockRelease();
    return success;
}

bool
FileSystem::DeleteFromDisk(int sector) 
{
    FileHeader *fileH = new FileHeader;
    freeMapLock->Acquire();
    fileH->FetchFrom(sector);

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.

    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    freeMapLock->Release();
    delete fileH;
    delete freeMap;
    return true;
}

DirectoryEntry
FileSystem::FindPath(FilePath* path)
{
    DirectoryEntry entry = { true, true, DIRECTORY_SECTOR };
    
    Directory* dir = new Directory();
    
    for (auto& part : path->List()) {
        OpenFile* file = new OpenFile(entry.sector);
        dir->FetchFrom(file);
        int index = dir->FindIndex(part.c_str());
        if (index < 0) {
            DEBUG('f', "Can't find file: %s\n", part.c_str());
            entry.sector = __UINT32_MAX__;
            return entry;
        }
        entry = dir->GetRaw()->table[index];
    }

    return entry;
}

/// List all the files in the file system directory.
void
FileSystem::List()
{
    DEBUG('f', "Listing Directory\n");
    FilePath path = currentThread->GetPath();
    currentThread->currentDirLock->Acquire();
    DirectoryEntry entry = FindPath(&path);
    OpenFile* dirFile = new OpenFile(entry.sector);
    Directory dir;
    dir.FetchFrom(dirFile);
    dir.List();
    currentThread->currentDirLock->Release();
    delete dirFile;
}

void
FileSystem::firstThreadStart()
{
    FilePath path = currentThread->GetPath();
    dirList->LockAcquire();
    DirectoryEntry entry = FindPath(&path);
    
    if (entry.sector == __UINT32_MAX__ || !entry.isDir) {
        dirList->LockRelease();
        return;
    }
    currentThread->currentDirLock = dirList->OpenDirectory(entry.sector);
    dirList->LockRelease();
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory();
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory();

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}

Bitmap *
FileSystem::AcquireFreeMap()
{
    freeMapLock->Acquire();

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    return freeMap;
}

Bitmap*
FileSystem::GetCurrentFreeMap()
{
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    return freeMap;
}

/// Marks the end of the freeMap usage. The lock is released, the
/// memory is freed and the changes are saved to disk.
void
FileSystem::ReleaseFreeMap(Bitmap *freeMap_)
{
    freeMap_->WriteBack(freeMapFile);
    delete freeMap_;

    freeMapLock -> Release();
}