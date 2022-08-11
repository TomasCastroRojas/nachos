/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>


/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
    ASSERT(freeMap != nullptr);

    if (fileSize > INDIR_MAX_FILE_SIZE) {
        return false;
    }

    raw.numBytes = fileSize;

    unsigned dataSectorCount = DataSectorCount();
    unsigned indirectionSectorCount = 0;
    if (UsesDoubleIndirection())
        indirectionSectorCount = DivRoundUp(dataSectorCount, NUM_DIRECT);
    raw.numSectors = dataSectorCount + indirectionSectorCount;
    indirTable = std::vector<FileHeader*>(indirectionSectorCount);

    if (freeMap->CountClear() < raw.numSectors) {
        return false;  // Not enough space.
    }

    // En este caso raw.numBytes = dataSectorCount <= y no hay indirecciÃ³n.
    if(raw.numBytes <= MAX_FILE_SIZE){
        for (unsigned i = 0; i < raw.numSectors; i++)
            raw.dataSectors[i] = freeMap->Find();
    // En este caso raw.numBytes = dataSectorCount + indirectionSectorCount, por lo que hacemos indirecciÃ³n.
    }else{
        // Amount of bytes that still have to be allocated.
        unsigned remainingBytes = raw.numBytes;

        // Allocate the header tables.
        for(unsigned i = 0; i < indirectionSectorCount; i++){
            raw.dataSectors[i] = freeMap->Find();
            FileHeader *dataHeader = new FileHeader;

            // nextBlock is the amount of bytes the current FileHeader will
            // store.
            unsigned nextBlock;
            if(remainingBytes <= MAX_FILE_SIZE)
                nextBlock = remainingBytes;
            else{
                // Allocate as many bytes as possible.
                nextBlock = MAX_FILE_SIZE;
                remainingBytes -= MAX_FILE_SIZE;
            }

            dataHeader->Allocate(freeMap, nextBlock);
            // Save the new FileHeader to the indirTable
            indirTable.push_back(dataHeader);
        }
    }

    return true;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    
	for (auto &it : indirTable) {
		it -> Deallocate(freeMap);
		delete it;
	}
	indirTable.clear();

    unsigned sectorLimit;
    if(UsesDoubleIndirection())
        sectorLimit = IndirectionSectorCount();
    else
        sectorLimit = raw.numSectors;

    for (unsigned i = 0; i < sectorLimit; i++) {
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);

    unsigned indirectionSectorCount = IndirectionSectorCount();
    indirTable = std::vector<FileHeader*>(indirectionSectorCount);

    // Fetch all the RawFileHeaders of the next level of indirection
    // and store them into the indirTable.
    for(unsigned i = 0; i < indirectionSectorCount; i++){
        FileHeader* dataHeader = new FileHeader;
        dataHeader -> FetchFrom(raw.dataSectors[i]);
        indirTable[i] = dataHeader;
    }
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    synchDisk->WriteSector(sector, (char *) &raw);

    // Store all the headers of the next level of indirection.
    for(unsigned i = 0; i < indirTable.size(); i++)
        indirTable[i] -> WriteBack(raw.dataSectors[i]);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    if(UsesDoubleIndirection()){
        unsigned index = offset/MAX_FILE_SIZE;
        return indirTable[index] -> ByteToSector(offset % MAX_FILE_SIZE);
    }else
        return raw.dataSectors[offset/SECTOR_SIZE];
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr) {
        printf("File header:\n");
    } else {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    for (unsigned i = 0; i < raw.numSectors; i++) {
        printf("%u ", raw.dataSectors[i]);
    }
    printf("\n");

    for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j])) {
                printf("%c", data[j]);
            } else {
                printf("\\%X", (unsigned char) data[j]);
            }
        }
        printf("\n");
    }
    delete [] data;
}

RawFileHeader *
FileHeader::GetRaw()
{
    return &raw;
}

bool
FileHeader::UsesDoubleIndirection() const
{
    return raw.numBytes > MAX_FILE_SIZE;
}

unsigned
FileHeader::DataSectorCount() const{
    unsigned const sectors = DivRoundUp(raw.numBytes, SECTOR_SIZE);
    return sectors;
}

unsigned
FileHeader::IndirectionSectorCount() const{
    if(not UsesDoubleIndirection())
        return 0;
    return DivRoundUp(sectorCount, NUM_DIRECT);
}

bool
FileHeader::Extend(Bitmap* freeMap, unsigned extendSize)
{
    if(extendSize == 0)
        return true; // Nothing to be done.

    unsigned oldNumBytes = raw.numBytes;
    unsigned oldNumSectors = raw.numSectors;
    bool oldDoubleIndirection = UsesDoubleIndirection();

    raw.numBytes += extendSize;
    unsigned dataSectorCount = DataSectorCount();
    unsigned indirectionSectorCount = IndirectionSectorCount();
    raw.numSectors = dataSectorCount + indirectionSectorCount;

    // Check that the final size is small enough to:
    //  - fit into a header with two indirection levels
    //  - fit into disk
    if (freeMap->CountClear() < raw.numSectors - oldNumSectors || INDIR_MAX_FILE_SIZE < raw.numBytes){
        // Restore original values
        raw.numBytes = oldNumBytes;
        raw.numSectors = oldNumSectors;
        return false;  // Not enough space.
    }

    DEBUG('f', "There is enough disk size to extend the file.\n");

    // The amount of bytes that still need to be allocated.
    unsigned remainingBytes = extendSize;

    if(not oldDoubleIndirection){
        if (oldNumBytes < SECTOR_SIZE) {
            unsigned lastSectorChunk = SECTOR_SIZE - (oldNumBytes % SECTOR_SIZE);
            remainingBytes -= lastSectorChunk;
        }

        // Fill the first level of indirection.
        for(unsigned i = oldNumSectors; i < NUM_DIRECT and remainingBytes > 0; i++){
            raw.dataSectors[i] = freeMap -> Find();
            remainingBytes -= SECTOR_SIZE;
        }

        if (remainingBytes > 0){
            FileHeader *fh = new FileHeader;
            RawFileHeader *rfh = fh->GetRaw();
            *rfh = raw;
            rfh -> numBytes = MAX_FILE_SIZE;
            rfh -> numSectors = NUM_DIRECT;
            raw.dataSectors[0] = freeMap -> Find();

            indirTable.push_back(fh);
        }
    }

    if(remainingBytes > 0){
        // Now the old table has more than one level of indirection.
        // We start from the last occupied sector table.
        FileHeader *fh = indirTable[indirTable.size() - 1];
        RawFileHeader *rfh = fh -> GetRaw();

        // The amount of free bytes in the last occupied sector table.
        unsigned freeLastTable = MAX_FILE_SIZE - rfh->numBytes;

        fh -> Extend(freeMap, remainingBytes < freeLastTable ? remainingBytes : freeLastTable);
        remainingBytes -= freeLastTable;

        // Allocate the necessary header tables.
        for(unsigned i = indirTable.size(); i < indirectionSectorCount; i++){
            raw.dataSectors[i] = freeMap -> Find();
            FileHeader *dataHeader = new FileHeader;

            // nextBlock is the amount of bytes the current FileHeader will
            // store.
            unsigned nextBlock;
            if(remainingBytes <= MAX_FILE_SIZE)
                nextBlock = remainingBytes;
            else{
                // Allocate as many bytes as possible.
                nextBlock = MAX_FILE_SIZE;
                remainingBytes -= MAX_FILE_SIZE;
            }

            dataHeader -> Allocate(freeMap, nextBlock);
            // Save the new FileHeader to the indirTable
            indirTable.push_back(dataHeader);
        }
    }

    return true;
}