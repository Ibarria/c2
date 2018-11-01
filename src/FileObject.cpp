
#if defined(WIN32)
 #include <windows.h>
const int path_separator = '\\';
#define strdup _strdup
#else
 #error "Fix includes for realpath"
const int path_separator = '/';
#endif

#include <assert.h>

#include "FileObject.h"


FileObject::~FileObject()
{
}

FileObject::FileObject(const char * file)
{
    setFile(file);
}

FileObject::FileObject(const FileObject & other)
{
    setFile(other.full_path);
}

void FileObject::setFile(const char * file)
{
    auto res = GetFullPathNameA(file, sizeof(full_path), full_path, nullptr);
    if (res < 1) assert(!"Issue finding path");
}

char * FileObject::getFilename()
{
    return full_path;
}

void FileObject::copy(const FileObject & other)
{
    setFile(other.full_path);
}

void FileObject::setExtension(const char * extension)
{
    char *p = full_path + strlen(full_path);
    while ((*p != '.') && (p != full_path)) p--;
    p++;
    while (*extension) {
        *p = *extension;
        p++; extension++;
    }
    *p = 0;
}

char * FileObject::getRootPath()
{
    char *result = strdup(full_path);
    char *p = result + strlen(result);
    while ((*p != path_separator) && (p != result)) {
        *p = 0;
        p--;
    }
    *p = 0;
    return result;
}

char * FileObject::getName()
{
    char *p = full_path + strlen(full_path);
    while ((*p != path_separator) && (p != full_path)) p--;
    p++;
    return p;
}