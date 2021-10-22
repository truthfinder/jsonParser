#include "mmap.h"

#include <assert.h>
#include <io.h>
#include <windows.h>
#include <unordered_map>

static std::unordered_map<void*, std::pair<HANDLE, size_t>> s_mmap_handles;

size_t alloc_granularity_helper() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwAllocationGranularity;
}

size_t alloc_granularity() {
    static const size_t granularity = alloc_granularity_helper();
    return granularity;
}

void* mmap(void* start, size_t length, int prot, int flags, int fd, off_t offset) {
    if (start != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (offset & (alloc_granularity() - 1)) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (!length) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (flags & MAP_ANON) {
        if (fd != -1) {
            errno = EBADF;
            return MAP_FAILED;
        } else if (offset) {
            errno = EINVAL;
            return MAP_FAILED;
        }
    } else if (fd == -1) {
        errno = EBADF;
        return MAP_FAILED;
    }

    if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE)) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    DWORD page_protection = 0;
    DWORD desired_access = 0;

    if (prot & PROT_EXEC) {
        if (prot & PROT_WRITE) {
            page_protection = PAGE_EXECUTE_READWRITE;
            desired_access = FILE_MAP_WRITE;
        } else if (prot & PROT_READ) {
            page_protection = PAGE_EXECUTE_READ;
            desired_access = FILE_MAP_READ;
        } else {
            errno = EINVAL;
            return MAP_FAILED;
        }

        desired_access |= FILE_MAP_EXECUTE;
    } else if (prot & PROT_WRITE) {
        page_protection = PAGE_READWRITE;
        desired_access = FILE_MAP_WRITE;
    } else if (prot & PROT_READ) {
        page_protection = PAGE_READONLY;
        desired_access = FILE_MAP_READ;
    } else {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (flags & MAP_PRIVATE) {
        desired_access |= FILE_MAP_COPY;
    }

    if (!(flags & MAP_NORESERVE)) {
        flags |= SEC_RESERVE;
    }

    const off_t end = offset + static_cast<off_t>(length);
    const HANDLE fh = (fd == -1) ? INVALID_HANDLE_VALUE : (HANDLE)_get_osfhandle(fd);
    const HANDLE handle = CreateFileMappingFromApp(fh, 0, page_protection, end, 0);
    assert(handle != NULL);
    if (handle == NULL) {
        errno = ENODEV;
        return MAP_FAILED;
    }

    void* const ret = MapViewOfFileFromApp(handle, desired_access, offset, length);
    assert(ret);
    if (ret == NULL) {
        CloseHandle(handle);
        errno = ENODEV;
        return MAP_FAILED;
    }

    assert(s_mmap_handles.find(ret) == s_mmap_handles.end());
    s_mmap_handles[ret] = std::make_pair(handle, length);
    return ret;
}

int munmap(void* addr, size_t length) {
    UnmapViewOfFile(addr);
    auto it = s_mmap_handles.find(addr);

    assert(it != s_mmap_handles.end());
    assert(it->second.first);
    assert(it->second.second == length);

    if (it == s_mmap_handles.end()) {
        errno = EINVAL;
        return -1;
    }

    int res = 0;
    if (it->second.first) {
        CloseHandle(it->second.first);
    } else {
        res = EINVAL;
    }

    if (it->second.second != length) {
        res = EINVAL;
    }

    s_mmap_handles.erase(it);

    if (res) {
        errno = res;
        return -1;
    }

    return 0;
}
