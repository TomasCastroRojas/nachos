#include "open_file_list.hh"
#include "read_write_controller.hh"
#include "threads/lock.hh"

OpenFileList::OpenFileList()
{
    first = last = nullptr;
    listLock = new Lock("OpenFileList Lock");
}

OpenFileList::~OpenFileList()
{
    FileMetaData* aux;

    while(first != nullptr)
    {
  		aux = first -> next;
  		delete first -> lock;
  		delete first;
  		first = aux;
	}
    delete listLock;
}

// Adds the file to the open file list.
// If the file is already open:
//      and is pending removal, it does nothing.
//      else it increases openInstances by 1.
ReadWriteController*
OpenFileList::AddOpenFile(int sector){

	ReadWriteController* fileRW = nullptr;

	FileMetaData* node = FindOpenFile(sector);
	if(node != nullptr){
		if(not node -> pendingRemove){
			  node -> openInstances++;
        fileRW = node -> lock;
    }
	}else{
		if(IsEmpty())
			first = last = CreateNode(sector);
		else{
			last -> next = CreateNode(sector);
			last = last -> next;
		}

    fileRW = last -> lock;
	}

	return fileRW;
}


void
OpenFileList::CloseOpenFile(int sector){

	FileMetaData* node = FindOpenFile(sector);
	if(node != nullptr){
		if(node -> openInstances > 1)
			node -> openInstances--;
		else
			DeleteNode(node);
	}

}


// Returns true if the file is currently open, in which case
// SetUpRemoval sets pendingRemove to true atomically.
// If the file is not open, it just returns false.
// Assumes the fileListLock is already taken by the file system.
bool
OpenFileList::SetUpRemoval(int sector){
    bool fileIsOpen;

    FileMetaData* node = FindOpenFile(sector);
	if(node != nullptr){
        node -> pendingRemove = true;
        fileIsOpen = true;
   }else
        fileIsOpen = false;

    return fileIsOpen;
}

// Allows the file system to acquire the list's lock.
void
OpenFileList::AcquireListLock(){
    listLock -> Acquire();
}

// Allows the file system to release the list's lock.
void
OpenFileList::ReleaseListLock(){
    listLock -> Release();
}


FileMetaData*
OpenFileList::FindOpenFile(int sector){
	FileMetaData* aux;
	for(aux = first;
	    aux != nullptr and aux->sector != sector;
	    aux = aux -> next);

	return aux;
}

bool
OpenFileList::IsEmpty(){
	return first == nullptr;
}

FileMetaData*
OpenFileList::CreateNode(int sector){
	FileMetaData* node = new FileMetaData;
    node -> sector = sector;
	node -> lock = new ReadWriteController();
	node -> openInstances = 1;
	node -> pendingRemove = false;
    node -> next = nullptr;

	return node;
}


void
OpenFileList::DeleteNode(FileMetaData* target){
	FileMetaData* aux = nullptr;
	//If the first item is to be deleted, advance the first pointer.
	if(first == target)
		first = first -> next;

	else{
		for(aux = first; aux -> next != target; aux = aux -> next);

		aux -> next = aux -> next -> next;
	}

	//If the last item is to be deleted, bring the last pointer one item back.
	if(last == target)
		last = aux;


    delete target -> lock;
    delete target;
}