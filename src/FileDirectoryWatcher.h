#ifndef CORE_FILE_DIRECTORY_WATCHER_H
#define CORE_FILE_DIRECTORY_WATCHER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <future>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <windows.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class FileDirectoryWatcher
{
public:
    FileDirectoryWatcher(const std::string& directory,
        const std::vector<std::string>& filenames,
        const bool dropExtension);
    ~FileDirectoryWatcher();

    FileDirectoryWatcher(const FileDirectoryWatcher&) = delete;
    FileDirectoryWatcher& operator=(const FileDirectoryWatcher&) = delete;

    bool checkForChanges();

    // Returns the filenames of the files that have changed
    // and starts the watch again.
    // Returns full filenames (non cropped).
    std::vector<std::string> getChangedFilesAndReset();
private:
    void startWatch();

    std::vector<std::string> getWriteStampFiles();
    void setupTimestamps(const std::vector<std::string>& filenames);
    std::string getCroppedName(const std::string& fullname);

    std::string m_directory;
    std::unordered_map<std::string, uint64_t> m_watchedFiles;
    bool m_changeDetected = false;

    std::future<bool> m_watcher;
    HANDLE m_stopEventHandle;
    // crop extension from input files and directory files when comparing
    bool m_cropExtension = false;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_FILE_DIRECTORY_WATCHER_H
