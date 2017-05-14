// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "FileDirectoryWatcher.h"

#include <assert.h>
#include <string>
#include <vector>
#include <iostream>
#include <future>

#include <windows.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

FileDirectoryWatcher::FileDirectoryWatcher(
    const std::string& directory,
    const std::vector<std::string>& filenames,
    const bool dropExtension)
    : m_directory(directory),
    m_cropExtension(dropExtension)
{
    setupTimestamps(filenames);

    m_stopEventHandle = CreateEvent(
        NULL,   // lpEventAttributes
        true,   // bManualReset
        false,  // bInitialState
        NULL);  // lpName
    assert(m_stopEventHandle != NULL && m_stopEventHandle != INVALID_HANDLE_VALUE);

    startWatch();
}

FileDirectoryWatcher::~FileDirectoryWatcher()
{
    // send stop event
    SetEvent(m_stopEventHandle);

    bool destroyHandle = false;
    if (m_watcher.valid())
    {
        // wait if async still running
        if (m_watcher.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
        {
            m_watcher.get();
            destroyHandle = true;
        }
        assert((!m_watcher.valid()) && "Async thread not stopped. Stop event did not work!");
    }
    
    if (m_stopEventHandle != NULL && m_stopEventHandle != INVALID_HANDLE_VALUE && destroyHandle)
    {
        CloseHandle(m_stopEventHandle);
    }
}

void FileDirectoryWatcher::startWatch()
{
    auto watchFunc = [](const std::string& directory, HANDLE stopHandle)
    {
        bool retValue = false; // change notification -> true
        const LPTSTR dir = (char*)directory.c_str();
        constexpr DWORD notifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE
            | FILE_NOTIFY_CHANGE_CREATION
            | FILE_NOTIFY_CHANGE_FILE_NAME;
        HANDLE changeHandle = FindFirstChangeNotification(
            dir,            // lpPathName
            false,          // bWatchSubtree
            notifyFilter);  // dwNotifyFilter

        std::vector<HANDLE> watchHandles {changeHandle, stopHandle};

        if (changeHandle != INVALID_HANDLE_VALUE && changeHandle != NULL)
        {
            DWORD waitStatus = WaitForMultipleObjects(
                (DWORD)watchHandles.size(), // nCount
                watchHandles.data(),        // lpHandles
                false,                      // bWaitAll
                (DWORD)INFINITE);           // dwMilliseconds

            switch (waitStatus)
            {
            case WAIT_OBJECT_0:     // change notification
                retValue = true;
                break;
            case WAIT_OBJECT_0 + 1: // stop event
                retValue = false;
                break;
            case WAIT_FAILED:
                retValue = false;
                std::cout << "FileDirectoryWatcher WAIT_FAILED: " << GetLastError();
                break;
            default : break;
            }

            CloseHandle(changeHandle);
        }

        return retValue;
    };

    ResetEvent(m_stopEventHandle);
    m_watcher = std::async(std::launch::async, watchFunc, m_directory, m_stopEventHandle);
}

bool FileDirectoryWatcher::checkForChanges()
{
    if (m_watcher.valid())
    {
        if (m_watcher.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            m_changeDetected = m_watcher.get();
        }
    }
    return m_changeDetected;
}

std::vector<std::string> FileDirectoryWatcher::getChangedFilesAndReset()
{
    std::vector<std::string> changedFiles;
    if (m_changeDetected)
    {
        m_changeDetected = false;
        changedFiles = getWriteStampFiles();
        if (!m_watcher.valid())
        {
            startWatch();
        }
    }
    return changedFiles;
}

std::vector<std::string> FileDirectoryWatcher::getWriteStampFiles()
{
    std::vector<std::string> writeStampFiles;

    WIN32_FIND_DATA findData{};
    const std::string dir = m_directory + std::string("/*");
    HANDLE hFile = FindFirstFile(
        dir.c_str(),
        &findData);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        while (FindNextFile(hFile, &findData) != 0)
        {
            std::string filename = findData.cFileName;
            std::string compareFilename = filename;
            if (m_cropExtension)
            {
                compareFilename = std::move(getCroppedName(compareFilename));
            }
            auto iter = m_watchedFiles.find(compareFilename);
            if (iter != m_watchedFiles.end())
            {
                const uint64_t currTimestamp = ((uint64_t)findData.ftLastWriteTime.dwHighDateTime << 32u)
                    | ((uint64_t)findData.ftLastWriteTime.dwLowDateTime);
                if (currTimestamp != iter->second)
                {
                    iter->second = currTimestamp;
                    writeStampFiles.emplace_back(filename);
                }
            }
        }
    }

    return writeStampFiles;
}

void FileDirectoryWatcher::setupTimestamps(const std::vector<std::string>& filenames)
{
    WIN32_FIND_DATA findData{};
    const std::string dir = m_directory + std::string("/*");
    HANDLE hFile = FindFirstFile(
        dir.c_str(),
        &findData);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        while (FindNextFile(hFile, &findData) != 0)
        {
            const std::string dirFilename = m_cropExtension ?
                std::move(getCroppedName(findData.cFileName)) :
                findData.cFileName;

            for (const auto& filenameRef : filenames)
            {
                if (filenameRef == dirFilename)
                {
                    const uint64_t timestamp = ((uint64_t)findData.ftLastWriteTime.dwHighDateTime << 32u)
                        | ((uint64_t)findData.ftLastWriteTime.dwLowDateTime);
                    m_watchedFiles.emplace(dirFilename, timestamp);
                }
            }
        }
    }
}

std::string FileDirectoryWatcher::getCroppedName(const std::string& fullname)
{
    const size_t lastIndex = fullname.find_last_of(".");
    if (lastIndex != std::string::npos)
    {
        return fullname.substr(0, lastIndex);
    }
    return fullname;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
