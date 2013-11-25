// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    string buf;
    ec->get(ino,buf);
    buf.resize(size);
    ec->put(ino,buf);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out, uint32_t type)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    //check if the file is exist
    bool found;
    lookup(parent,name,found,ino_out);
    if(found){
        return EXIST;
    }
    
    //create a new inode for the file or dir
    ec->create(type, ino_out);
    
    //modify the parent infomation
    //get the parent inode content
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error get parent inode!\n");
        return IOERR;
    }
    
    list<dirent> fm = splitdir(buf);
    struct dirent dir;
    dir.name = name;
    dir.inum = ino_out;
    fm.push_back(dir);
    
    buf = catdir(fm);
    ec->put(parent,buf);

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    
    list<yfs_client::dirent> fm;
    readdir(parent,fm);
    list<yfs_client::dirent>::iterator lt;
    ino_out = 0;
    found = false;
    
    for ( lt = fm.begin( ); lt != fm.end( ); lt++ )
        if((*lt).name == name){
            ino_out = (*lt).inum;
            found = true;
        }
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    
    //check if the parent inode is a dir
    if (isfile(dir)) {
        printf("the parent inode is not a dir!\n");
        return NOENT;
    }
    
    //get the parent inode content
    std::string buf;
    if (ec->get(dir, buf) != extent_protocol::OK) {
        printf("error get parent inode!\n");
        return IOERR;
    }
    
    list = splitdir(buf);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    string buf;
    ec->get(ino,buf);
    data = buf.substr(off,size);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    string buf;
    
    ec->get(ino,buf);
    size_t osize = buf.size();
    if(osize < off + size)
        buf.resize(off + size,'\0');
    buf = buf.replace(off,size,data,size);
    ec->put(ino,buf);
    bytes_written = size + (off - osize > 0 ? off - osize : 0);
    return r;
}

std::list<yfs_client::dirent>
yfs_client::removenode(std::string str,std::string name)
{
    char *cstr, *p, *i;
    list<dirent> res;
    cstr = new char[str.size()+1];
    strcpy(cstr,str.c_str());
    p = strtok(cstr,DIR_SEP);
    i = strtok(NULL,DIR_SEP);
    while(p!=NULL && i!=NULL)
    {
        if(p != name)
        {
            struct dirent dir;
            dir.name = p;
            dir.inum = n2i(i);
            res.push_back(dir);
        }
        p = strtok(NULL,DIR_SEP);
        i = strtok(NULL,DIR_SEP);
    }
    return res;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    //check if the file is exist
    bool found;
    inum ino;
    lookup(parent,name,found,ino);
    
    //modify the parent infomation
    //get the parent inode content
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error get parent inode!\n");
        return IOERR;
    }
       
    stringstream ss;
    ss << name << '/' << ino << '/';
    string nodestr = ss.str();
    string::size_type pos = buf.find(nodestr);
    buf.erase(pos,nodestr.length());
    ec->put(parent,buf);

    return r;
}

std::list<yfs_client::dirent>
yfs_client::splitdir(std::string str)
{
    char *cstr, *p, *i;
    list<dirent> res;
    cstr = new char[str.size()+1];
    strcpy(cstr,str.c_str());
    p = strtok(cstr,DIR_SEP);
    i = strtok(NULL,DIR_SEP);
    while(p!=NULL && i!=NULL)
    {
        struct dirent dir;
        dir.name = p;
        dir.inum = n2i(i);
        res.push_back(dir);
        p = strtok(NULL,DIR_SEP);
        i = strtok(NULL,DIR_SEP);
    }
    return res;
}

std::string
yfs_client::catdir(std::list<yfs_client::dirent> fm)
{
    stringstream ss;
    list<yfs_client::dirent>::iterator lt;
    for ( lt = fm.begin( ); lt != fm.end( ); lt++ )
        ss << (*lt).name << '/' << (*lt).inum << '/';
    return ss.str();
}